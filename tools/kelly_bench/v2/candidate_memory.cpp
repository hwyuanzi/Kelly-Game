// Kelly game bot (C++).
// Edit ONLY the section marked below. Everything outside it talks to the referee.
// Debug output goes to std::cerr, never std::cout: stdout is reserved for your bets.
// Build: python3 build.py   (the referee does this for you before the tournament)
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

// The game state, updated every round.
struct State {
    char role = 'A';            // 'A' or 'B' (you play A in one game, B in the other)
    int round = 0;              // this round, starting at 0
    vector<double> probs;       // probs[k] = probability that A wins round k (all rounds)
    long long my_capital = 0;   // your money now
    long long opp_capital = 0;  // opponent's money now
    double time_used = 0;       // seconds spent in choose_bet so far this game (timed by your bot)
};

// Do not edit above this line.
// ================== EDIT ONLY THIS SECTION ==================
// Write your strategy in choose_bet. You may add #includes and helper functions here.
// Return a whole number. At most 20% of my_capital (rounded down); a bigger bet is lowered to that.
// You have 120 seconds per game.

// Maximize expected final money in this zero-sum, finite-horizon game.
#include <cmath>
#include <exception>

namespace hollan {
using Money = long long;
constexpr int MODEL_COUNT = 14;

// Ten elementary models span cautious, aggressive, passive and opponent-aware
// play. Four additional models in Planner predict finite-horizon planning bots.
// Every policy uses public information only.
Money model_bet(int model, double p, Money x, Money y) {
    if (x <= 0) return 0;
    const Money limit = x / 5;
    const double edge = 2 * p - 1;
    double target = edge * static_cast<double>(x);
    switch (model) {
        case 1: target *= .5; break;
        case 2: target *= 1.5; break;
        case 3: target *= 2; break;
        case 4: target = p > .5 ? static_cast<double>(limit) : 0; break;
        case 5: target = static_cast<double>(limit); break;
        case 6: target = 0; break;
        case 7: {
            double forced = max(0.0, min(static_cast<double>(y / 5), -edge * static_cast<double>(y)));
            target = min(static_cast<double>(y), target) - forced;
            break;
        }
        case 8: target *= .25; break;
        case 9: target *= .75; break;
    }
    if (!(target > 0)) return 0;
    return target >= static_cast<double>(limit) ? limit : static_cast<Money>(target);
}

struct Planner {
    Money total = 0;
    vector<Money> grid;
    vector<int> lower;
    vector<double> fraction;
    vector<double> probability;
    vector<vector<double>> value, reverse_value;
    // Modes other than 4 are used only by the reproducible ablation harness.
    int variant = 4;
    chrono::steady_clock::time_point deadline = chrono::steady_clock::time_point::max();
    int observations = 0;
    vector<double> loss, evidence;
    vector<bool> unavailable;
    Money previous_x = 0, previous_y = 0, previous_bet = 0;
    int previous_round = -1;
    int resolution = 800, actions = 24;
    int active = 0;
    size_t cache_limit = 256u * 1024u * 1024u;

    void reset(const State& s) {
        total = s.my_capital + s.opp_capital;
        probability = s.probs;
        if (s.role == 'B') for (double& p : probability) p = 1 - p;
        grid.clear();
        Money step = max<Money>(1, total / resolution);
        const Money tail = min<Money>(64, total);
        for (Money x = 0; x <= total; ) {
            grid.push_back(x);
            if (x == total) break;
            Money next = (x < tail || x >= total - tail) ? x + 1 : min(total - tail, x + step);
            x = min(total, max(x + 1, next));
        }
        lower.clear(); fraction.clear();
        // Fast exact lookup for the official capital range, with a portable
        // binary-search fallback for larger custom parameter files.
        if (total <= 100000) {
            lower.resize(static_cast<size_t>(total) + 1);
            fraction.resize(static_cast<size_t>(total) + 1);
            size_t j = 0;
            for (Money x = 0; x <= total; ++x) {
                while (j + 1 < grid.size() - 1 && grid[j + 1] <= x) ++j;
                lower[static_cast<size_t>(x)] = static_cast<int>(j);
                fraction[static_cast<size_t>(x)] = static_cast<double>(x - grid[j]) / static_cast<double>(grid[j + 1] - grid[j]);
            }
        }
        value.assign(MODEL_COUNT, {});
        reverse_value.assign(MODEL_COUNT, {});
        observations = 0;
        loss.assign(MODEL_COUNT, 0);
        evidence.assign(MODEL_COUNT, 0);
        unavailable.assign(MODEL_COUNT, false);
        loss[0] = -.4;  // a weak sample-Kelly prior; observations dominate it
        previous_round = -1;
        active = 0;
    }

    double interpolate(const double* v, Money x) const {
        x = max<Money>(0, min(total, x));
        int i;
        double f;
        if (!lower.empty()) {
            i = lower[static_cast<size_t>(x)];
            f = fraction[static_cast<size_t>(x)];
        } else {
            auto pos = upper_bound(grid.begin(), grid.end(), x);
            i = static_cast<int>(pos - grid.begin()) - 1;
            i = max(0, min(static_cast<int>(grid.size()) - 2, i));
            f = static_cast<double>(x - grid[static_cast<size_t>(i)]) / static_cast<double>(grid[static_cast<size_t>(i) + 1] - grid[static_cast<size_t>(i)]);
        }
        return v[i] + f * (v[i + 1] - v[i]);
    }

    double action_value(const double* next, Money x, Money y, Money a, Money b, double p) const {
        Money pot = a + b;
        return p * interpolate(next, x + min(pot, y)) +
               (1 - p) * interpolate(next, x - min(pot, x));
    }

    Money policy(const double* next, Money x, double p, Money b, int count = 24) const {
        const Money y = total - x, limit = x / 5;
        Money bet = 0, last = -1;
        double best = -1;
        auto consider = [&](Money a) {
            a = max<Money>(0, min(limit, a));
            double v = action_value(next, x, y, a, b, p);
            if (v > best + 1e-14) {best = v; bet = a;}
        };
        for (int k = 0; k <= count; ++k) {
            Money a = limit * k / count;
            if (a != last) consider(a);
            last = a;
        }
        consider(y - b);
        return bet;
    }

    int dependency(int model) const {
        return model == 10 ? 0 : model == 11 ? 7 : model == 12 ? 10 : 12;
    }

    Money predict(int model, size_t t, Money x, double p, bool reversed = false) const {
        if (model < 10) return model_bet(model, p, x, total - x);
        int base = dependency(model);
        const auto& tables = reversed ? reverse_value : value;
        const double* next = tables[static_cast<size_t>(base)].data() + (t + 1) * grid.size();
        Money other = predict(base, t, total - x, 1 - p, !reversed);
        return policy(next, x, p, other);
    }

    bool prepare(int model, double budget = 8.0, bool reversed = false) {
        auto& tables = reversed ? reverse_value : value;
        if (!tables[static_cast<size_t>(model)].empty()) return true;
        if (unavailable[static_cast<size_t>(model)] || chrono::steady_clock::now() >= deadline) return false;
        // Planning opponents are themselves finite-horizon best responses.
        // Dependencies strictly decrease, so this recursion always terminates.
        if (model >= 10 && !prepare(dependency(model), budget, !reversed)) return false;
        auto started = chrono::steady_clock::now();
        const size_t width = grid.size(), n = probability.size();
        size_t used = 0;
        for (const auto& v : value) used += v.size() * sizeof(double);
        for (const auto& v : reverse_value) used += v.size() * sizeof(double);
        const size_t need = (n + 1) * width * sizeof(double);
        // Never evict a dependency while another table is being computed.
        if (used + need > cache_limit) {
            unavailable[static_cast<size_t>(model)] = true;
            return false;
        }
        auto& v = tables[static_cast<size_t>(model)];
        v.resize((n + 1) * width);
        for (size_t i = 0; i < width; ++i)
            v[n * width + i] = static_cast<double>(grid[i]) / static_cast<double>(total);
        for (size_t t = n; t-- > 0;) {
            if (t % 16 == 0 && (chrono::steady_clock::now() >= deadline ||
                chrono::duration<double>(chrono::steady_clock::now() - started).count() > budget)) {
                vector<double>().swap(v);
                unavailable[static_cast<size_t>(model)] = true;
                return false;
            }
            double p = reversed ? 1 - probability[t] : probability[t];
            double* now = v.data() + t * width;
            const double* next = now + width;
            now[0] = 0; now[width - 1] = 1;
            for (size_t i = 1; i + 1 < width; ++i) {
                Money x = grid[i], y = total - x, limit = x / 5;
                Money b = predict(model, t, y, 1 - p, !reversed);
                double best = -1;
                Money last = -1;
                for (int k = 0; k <= actions; ++k) {
                    Money a = limit * k / actions;
                    if (a == last) continue;
                    last = a;
                    best = max(best, action_value(next, x, y, a, b, p));
                }
                Money a = max<Money>(0, min(limit, y - b));
                now[i] = max(best, action_value(next, x, y, a, b, p));
            }
        }
        return true;
    }

    void observe(const State& s) {
        if (previous_round + 1 != s.round || previous_round < 0) return;
        Money delta = s.my_capital - previous_x;
        Money pot = delta < 0 ? -delta : delta;
        // Bankruptcy terminates official games, so every observed live
        // transition reveals the uncapped pot and the opponent's integer bet.
        if (pot < previous_bet || s.my_capital <= 0 || s.opp_capital <= 0) return;
        Money observed = pot - previous_bet;
        double p = probability[static_cast<size_t>(previous_round)];
        double scale = max(2.0, .01 * static_cast<double>(previous_y));
        bool static_exact = false;
        bool allow_deep = true;
        // Preserve longer memory when irregular bets fit none of the simple
        // families. Exact recognizable policies adapt rapidly after a switch.
        bool recognizable = active >= 10;
        bool adaptive_memory = variant == 7 || variant == 8 || variant == 9;
        if (adaptive_memory) for (int m = 0; m < 10; ++m)
            if (abs(model_bet(m, 1 - p, previous_y, previous_x) - observed) <= 1) recognizable = true;
        double retention = (variant == 1 || variant == 3 || (adaptive_memory && !recognizable)) ? .985 : .8;
        for (int m = 0; m < MODEL_COUNT; ++m) {
            if (m == 10) {
                allow_deep = variant != 8 && variant != 18 && !(static_exact && active < 10 && observations >= 8);
            }
            if (m >= 10 && (!allow_deep || ((variant == 7 || variant == 17) && m != 10))) continue;
            if (m >= 10 && (s.time_used > 50 || probability.size() > 4000 || !prepare(dependency(m), 8.0, true))) {
                loss[static_cast<size_t>(m)] = 1e6;
                continue;
            }
            double prediction = static_cast<double>(predict(m, static_cast<size_t>(previous_round), previous_y, 1 - p, true));
            double err = max(0.0, abs(prediction - static_cast<double>(observed)) - 1) / scale;
            loss[static_cast<size_t>(m)] = retention * loss[static_cast<size_t>(m)] + min(12.0, err * err);
            evidence[static_cast<size_t>(m)] = retention * evidence[static_cast<size_t>(m)] + 1;
            if (m < 10 && err == 0 && loss[static_cast<size_t>(m)] < 1e-10) static_exact = true;
        }
        ++observations;
        int best = 0;
        bool gated = variant >= 3;
        for (int m = 1; m < 14; ++m) {
            if (m >= 10 && (!allow_deep || ((variant == 7 || variant == 17) && m != 10))) continue;
            if (gated && m >= 10 &&
                (evidence[static_cast<size_t>(m)] < 2.9 || loss[static_cast<size_t>(m)] / max(1.0, evidence[static_cast<size_t>(m)]) > .12)) continue;
            if (loss[static_cast<size_t>(m)] < loss[static_cast<size_t>(best)]) best = m;
        }
        if (gated && active >= 10 && loss[static_cast<size_t>(active)] / max(1.0, evidence[static_cast<size_t>(active)]) > .12) active = 0;
        if (loss[static_cast<size_t>(best)] + .05 < loss[static_cast<size_t>(active)]) active = best;
    }

    Money choose(const State& s) {
        if (s.round < 0 || static_cast<size_t>(s.round) >= s.probs.size() ||
            s.my_capital <= 0 || s.opp_capital <= 0) return 0;
        if (total != s.my_capital + s.opp_capital || probability.empty() || previous_round >= s.round) reset(s);
        // Ten seconds for this call and 65 cumulative internal seconds leave
        // substantial headroom for the referee's I/O-inclusive 120-second clock.
        deadline = chrono::steady_clock::now() + chrono::duration_cast<chrono::steady_clock::duration>(
            chrono::duration<double>(min(10.0, max(0.0, 65 - s.time_used))));
        observe(s);
        const Money x = s.my_capital, y = s.opp_capital, limit = x / 5;
        const double p = probability[static_cast<size_t>(s.round)];
        Money result = 0;
        // Bounded safe fallback also handles custom games beyond the official
        // 2000-round range and machines with little remaining thinking time.
        if (s.time_used > 65 || probability.size() > 4000 || !prepare(active)) {
            Money b = active < 10 ? model_bet(active, 1 - p, y, x) : (prepare(dependency(active), 8.0, true) ? predict(active, static_cast<size_t>(s.round), y, 1 - p, true) : model_bet(0, 1 - p, y, x));
            double target = min(static_cast<double>(y), (2 * p - 1) * static_cast<double>(x)) - static_cast<double>(b);
            result = target > 0 ? min(limit, static_cast<Money>(target)) : 0;
        } else {
            const double* next = value[static_cast<size_t>(active)].data() + (static_cast<size_t>(s.round) + 1) * grid.size();
            Money b = active < 10 ? model_bet(active, 1 - p, y, x) : (prepare(dependency(active), 8.0, true) ? predict(active, static_cast<size_t>(s.round), y, 1 - p, true) : model_bet(0, 1 - p, y, x));
            double best = -1;
            auto consider = [&](Money a) {
                a = max<Money>(0, min(limit, a));
                double score = action_value(next, x, y, a, b, p);
                if (score > best + 1e-14) { best = score; result = a; }
            };
            int count = static_cast<int>(min<Money>(limit, 64));
            consider(0);
            for (int k = 1; k <= count; ++k) consider(limit * k / count);
            consider(y - b);
            consider(model_bet(7, p, x, y));
            Money center = result, radius = max<Money>(1, limit / 64);
            for (Money a = max<Money>(0, center - radius); a <= min(limit, center + radius); ++a) consider(a);
        }
        result = max<Money>(0, min(limit, result));
        previous_x = x; previous_y = y; previous_bet = result; previous_round = s.round;
        return result;
    }
};
}  // namespace hollan

long long choose_bet(const State& s) {
    static hollan::Planner planner;
    try {
        return planner.choose(s);
    } catch (const std::exception&) {
        // Allocation failures on constrained hosts must not forfeit a game.
        for (auto& v : planner.value) vector<double>().swap(v);
        for (auto& v : planner.reverse_value) vector<double>().swap(v);
        planner.probability.clear(); planner.total = 0;
        double p = (s.round >= 0 && static_cast<size_t>(s.round) < s.probs.size()) ? s.probs[static_cast<size_t>(s.round)] : .5;
        if (s.role == 'B') p = 1 - p;
        return hollan::model_bet(7, p, s.my_capital, s.opp_capital);
    }
}

// ============================================================
// Do not edit below this line.

// Minimal readers for the referee's flat JSON messages ("key":value, no spaces).
static size_t at(const string& s, const string& key) {
    size_t p = s.find("\"" + key + "\":");
    return p == string::npos ? p : p + key.size() + 3;
}
static double num(const string& s, const string& key) {
    size_t p = at(s, key);
    return p == string::npos ? 0 : strtod(s.c_str() + p, nullptr);
}
static string str(const string& s, const string& key) {
    size_t p = at(s, key);
    string out;
    if (p == string::npos || s[p] != '"') return out;
    for (size_t i = p + 1; i < s.size() && s[i] != '"'; ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) ++i;
        out += s[i];
    }
    return out;
}
static vector<double> arr(const string& s, const string& key) {
    vector<double> out;
    size_t p = at(s, key);
    if (p == string::npos || s[p] != '[') return out;
    const char* c = s.c_str() + p + 1;
    while (*c && *c != ']') {
        char* end;
        out.push_back(strtod(c, &end));
        c = end;
        while (*c == ',' || *c == ' ') ++c;
    }
    return out;
}

int main() {
    ios::sync_with_stdio(false);
    State st;
    string line;
    while (getline(cin, line)) {
        string type = str(line, "type");
        if (type == "start") {
            st = State();
            st.role = str(line, "role")[0];
            st.probs = arr(line, "probs");
            st.my_capital = (long long)num(line, "my_capital");
            st.opp_capital = (long long)num(line, "opp_capital");
            cout << "{\"ready\":true}" << endl;              // endl flushes
        } else if (type == "bet") {
            st.round = (int)num(line, "round");
            st.my_capital = (long long)num(line, "my_capital");
            st.opp_capital = (long long)num(line, "opp_capital");
            auto started = chrono::steady_clock::now();
            long long bet = choose_bet(st);
            st.time_used += chrono::duration<double>(chrono::steady_clock::now() - started).count();
            cout << "{\"bet\":" << bet << "}" << endl;
        }
    }
}
