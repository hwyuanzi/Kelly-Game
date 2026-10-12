// Historical v1 reproduction. Use v2/ for the current submission.
#define main hollan_protocol_main
#include "v2/baseline.cpp"
#undef main
#include <cassert>
#include <random>

int main() {
    std::mt19937 rng(129731);
    size_t checked=0;
    for(int trial=0;trial<50;++trial) {
        State s;s.role=trial%2?'A':'B';s.my_capital=1+rng()%49;s.opp_capital=1+rng()%49;
        for(int t=0;t<7;++t)s.probs.push_back((rng()%11)/10.0);
        hollan::Planner dp;dp.reset(s);long long total=dp.total;
        for(int model=0;model<hollan::MODEL_COUNT;++model) {
            assert(dp.prepare(model));
            std::vector<std::vector<double>> exact(8,std::vector<double>((size_t)total+1));
            for(long long x=0;x<=total;++x)exact[7][(size_t)x]=(double)x/total;
            for(int t=6;t>=0;--t) {
                exact[t][0]=0;exact[t][(size_t)total]=1;
                for(long long x=1;x<total;++x) {
                    double p=dp.probability[t],best=-1;long long y=total-x;
                    long long b=hollan::model_bet(model,1-p,y,x);
                    for(long long a=0;a<=x/5;++a) {
                        long long pot=a+b;
                        double v=p*exact[t+1][(size_t)(x+std::min(pot,y))]+(1-p)*exact[t+1][(size_t)(x-std::min(pot,x))];
                        best=std::max(best,v);
                    }
                    exact[t][(size_t)x]=best;
                    double actual=dp.value[model][(size_t)t*dp.grid.size()+(size_t)x];
                    assert(std::abs(actual-best)<1e-12);++checked;
                }
            }
        }
        for(long long x=1;x<total;++x) {
            s.my_capital=x;s.opp_capital=total-x;s.round=0;
            dp.previous_round=-1;dp.active=0;
            auto bet=dp.choose(s);assert(bet>=0 && bet<=x/5);
            auto next=dp.value[0].data()+dp.grid.size();double p=dp.probability[0];
            auto b=hollan::model_bet(0,1-p,s.opp_capital,x);
            double chosen=dp.action_value(next,x,s.opp_capital,bet,b,p);
            for(long long a=0;a<=x/5;++a)assert(chosen+1e-12>=dp.action_value(next,x,s.opp_capital,a,b,p));
        }
    }
    // Time and horizon fallbacks remain legal and do not allocate a DP table.
    for(int horizon:{10,2000,5000})for(char role:{'A','B'})for(long long x:{1LL,4LL,5LL,1000LL,8000LL}) {
        State s;s.role=role;s.probs.assign(horizon,.63);s.my_capital=x;s.opp_capital=9000-x;s.time_used=100;
        hollan::Planner dp;auto bet=dp.choose(s);assert(bet>=0 && bet<=x/5);
        for(const auto& v:dp.value)assert(v.empty());
    }
    // A deliberately exhausted build budget must discard its partial table.
    State s;s.probs.assign(2000,.55);s.my_capital=1000;s.opp_capital=8000;
    hollan::Planner dp;dp.reset(s);assert(!dp.prepare(0,-1));assert(dp.value[0].empty());assert(!dp.prepare(0));
    std::cout<<"PASS: "<<checked<<" exact Bellman states; exhaustive integer choices; safe fallbacks.\n";
}
