import json,re
from pathlib import Path
here=Path(__file__).resolve().parent
root=here.parents[2]
text=(here/'results/heldout-production.txt').read_text()
def fields(line):return {k:float(v) for k,v in re.findall(r'(\w+)=([-+\d.e]+)',line)}
rows=[fields(l) for l in text.splitlines() if l.startswith('opp=') and 'variant=' in l]
assert len(rows)==58
stats={(int(r['opp']),int(r['variant'])):r for r in rows}
all_stats={int(r['variant']):r for r in [fields(l) for l in text.splitlines() if l.startswith('ALL ')]}
worst=min((stats[o,4] for o,v in stats if v==4),key=lambda r:r['delta'])
assert min(r['delta']-r['ci95'] for r in rows if r['variant']==4)>-.0025
names={0:'Official Kelly',1:'Half Kelly',4:'Favorable maximum',7:'Opponent-aware Kelly',5:'Always maximum',6:'Passive',3:'Double Kelly',10:'Bankroll-dependent',11:'12-round switching',12:'Pseudorandom stakes',13:'Threshold aggression',14:'0.6x Kelly',15:'DP response 1',16:'Frozen Hollan v1',17:'Hollan v2 self-play',18:'DP response 2',19:'DP response 3',20:'Cautious bait / maximum',21:'3-round switching',22:'Kelly bait / DP response 2',23:'Switching DP responses'}
for i in range(8):names[100+i]='Evolved adversary '+str(i+1)
order=[int(r['opp']) for r in rows if r['variant']==4]
def table(ids):
    out='| Opponent | v1 share | v2 share | Paired gain, percentage points (95% CI) |\n|---|---:|---:|---:|\n'
    for o in ids:
        b=stats[o,0];r=stats[o,4]
        out+=f"| {names[o]} | {b['score']*100:.3f}% | {r['score']*100:.3f}% | {r['delta']*100:+.3f} [{(r['delta']-r['ci95'])*100:+.3f}, {(r['delta']+r['ci95'])*100:+.3f}] |\n"
    return out
real=json.loads((here/'results/real-referee-summary.json').read_text())
reliable=json.loads((here/'results/reliability.json').read_text())
summary={'scenarios':27,'pairs_per_scenario':2500,'pairs_per_opponent':67500,'opponent_count':29,'games_per_strategy':3915000,'total_final_comparison_games':7830000,'aggregate':all_stats,'opponents':rows,'regression_tolerance_percentage_points':.25,'pointwise_95_percent_noninferiority_passed':True,'real_referee_games':real['games']+reliable['games']}
(here/'results/final-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
report=f'''# Hollan v2: measured adversarial upgrade

Hollan v2 raises the equally weighted opponent-pool score from **56.039% to 56.366%**, a paired **+0.3265 percentage points** (95% interval **+0.3098 to +0.3433**). Against the seven planning/self-play/deceptive-planning opponents, the gain is **+1.2190 points**. It reverses a disadvantage against the second DP best response: **48.908% → 51.892%**.

The additive-money metric also improves: **$9,957.82 → $10,014.19** per pairing, averaged over opponents/configurations, **+$56.37** (paired 95% interval **+$53.20 to +$59.55**). The share metric weights configurations equally; the money metric retains their different total capitals.

The largest measured regression is **−0.1027 points** against threshold aggression, with a 95% interval **−0.1583 to −0.0470**. This is a real tradeoff, accepted within the **0.25-point per-opponent tolerance** used for adoption. Every opponent's pointwise 95% lower bound clears −0.25 points. These intervals are conditional on this suite and are not simultaneous family-wise guarantees or a promise against arbitrary opponents.

## Final strategy

The production source stays inside the official template's permitted strategy section. The protocol and build script are byte-identical to the official template. It maximizes expected terminal money using the conserved total capital and all future probabilities:

`V_t(x) = max_a [ p_t V_(t+1)(x + min(a+b, T-x)) + (1-p_t) V_(t+1)(x - min(a+b, x)) ]`.

Terminal value is `x/T`; bankruptcy values are 0 and 1. Every own action and predicted opponent stake is a legal integer. The transfer is the **sum of both stakes**, capped by the loser's capital. This objective manages risk for the competition's final-capital score, rather than log growth.

The original ten elementary opponent models are retained. Four additional models recognize finite-horizon planners: the first, second and third best-response generations starting from official Kelly, plus a best response starting from opponent-aware Kelly. Their predictions use reverse-orientation value tables; dependencies strictly decrease, so construction cannot recurse cyclically. The selected model's Bellman table provides our full-horizon best response.

On every surviving transition, `abs(capital change) - previous own bet` reveals the opponent's exact previous bet. Model losses discount by **0.8** per observation (v1 used 0.985), tolerate one dollar of rounding error, scale by max($2, 1% of opponent capital), and cap outlier squared loss at 12. The original weak Kelly prior and switching hysteresis remain. A planning model is eligible only with **effective evidence ≥2.9** and **normalized mean squared loss ≤0.12**; bad fits immediately lose eligibility. This prevents unfamiliar irregular bets from casually activating a sophisticated but incorrect planner. After eight observations of an exact elementary fit, unnecessary planning predictions are skipped until a mismatch appears.

The approximately 800-point interior capital grid, exact one-dollar boundary strips, 25 coarse actions and clipping kink, 65-point live search and local exhaustive integer refinement remain empirically supported. Computation is spent on additional opponent-aware full-horizon tables, not noisy runtime rollouts. Both binary outcomes are evaluated exactly; Monte Carlo supplies the empirical evidence.

Table storage is bounded at **256 MiB**. Individual construction has an eight-second guard; every decision also has a ten-second shared deadline, with a 65-second internal cumulative cutoff. Partial tables are discarded. Memory/time/horizon failures use a legal opponent-aware Kelly fallback. Allocation exceptions clear both table orientations and reset the planner safely.

## Discovery, selection and ablations

The exact v1 source and original submission are preserved as `baseline.cpp` and `Hollan_v1.zip`. No Git push or commit was made.

1. We generated successive finite-horizon best responses automatically. The second response exploited v1 on independent validation (**48.181%** for v1); v2 recovered **51.993%** on the same games.
2. A population of 24 eight-parameter policies underwent 180 mutation/selection iterations against v1 (seed 339841), using common training tapes and separate probability sequences. Policies depend on edge, capital ratio, remaining horizon, the next probability, periodic behavior and opponent-aware forced stakes. Four distinct policies were retained. A second 120-iteration search targeted a v2 candidate on separate training scenarios (seed 339842), adding four more opponents. The search improved within its policy class; it does not prove global best-response optimality.
3. Initial discovery used seed 772615. Four planning models without a confidence gate overfit some unknown policies. A pure-action worst-case Bellman blend produced unfavorable point estimates against several familiar opponents and failed to establish a compelling broad benefit; it was rejected.
4. Fresh validation used **22,500 pairings per opponent**, seed 918374. Confidence-gated fast adaptation gained **+0.4414 points** across the 22-opponent pool, compared with +0.3665 for slow adaptation. A conditional-memory experiment gained no credible overall advantage and weakened the frozen-v1 matchup slightly; it was rejected.
5. The final algorithm was frozen before seed 941778320. Two previously untested deceptive planning policies and the second evolved population entered the held-out pool.

A separate ablation suite (seed 82910435, 15 configurations, 1,000 paired trials each, ten challenge opponents) measured:

| Variant | Mean share | Gain over v1, points (95% half-width) |
|---|---:|---:|
| Frozen v1 | 55.601% | — |
| Faster adaptation, elementary models only | 55.836% | +0.235 ±0.058 |
| Add only the first planning model | 56.087% | +0.486 ±0.066 |
| Final four planning models | 56.625% | +1.024 ±0.080 |

Against DP response 2, the final strategy beats the one-planning-model ablation by **+3.717 ±0.312 points**, and the elementary-only ablation by **+3.696 ±0.315 points**. This supports spending computation on the deeper opponent models.

## Held-out results

The final comparison contains **27 configurations** (three supplied plus 24 new randomized configurations), **2,500 paired outcome tapes per configuration**, **29 opponents**, and **both roles**: **67,500 pairings per opponent per strategy**, **3,915,000 games per strategy**, **7,830,000 games total**. Horizons span 10–2,000 rounds; capitals, bias, probability width, regime changes, alternating sequences, frozen fair sequences and a late opportunity vary independently of the training set. Baseline and candidate use identical configurations and draws. Role draws are independent. Pool confidence intervals average opponents within each shared trial before computing variance, retaining their covariance.

{table(order)}

`heldout-production.txt` uses the actual production memory limits. Tables are physically reused across simulations to amortize deterministic DP construction; beliefs and previous stakes reset every game. Financial actions are memoized by public financial state and selected model, while inference still runs independently. **2,700 cache hits were checked directly**, and an entire smoke suite matches the non-memoized implementation exactly. The production-cap run reached **256,352,112 bytes** of table storage and recorded **three unavailable-model flags**, which invoke the safe fallback. A separate larger-cache run (`heldout.txt`) measures sensitivity to table availability and benchmark amortization: its pool gain is +0.3268 points versus +0.3265 in the production-cap run, a difference of **0.0003 points**, far below Monte Carlo uncertainty. Runtime limits and machine load mean exact replay of every table-availability decision is not promised.

## Prediction accuracy on identical states

`prediction.cpp` drives both learners with the same exogenous observer bets, same financial states and same reconstructed opponent bets, using a new seed (31415926). This isolates prediction from the different capital paths caused by their betting strategies. Errors are absolute prediction error divided by the legal opponent stake limit, evaluated after round 3; per-role errors are averaged within each trial before paired uncertainty is computed. Some very short bankrupt games have no eligible observations.

| Opponent | v1 mean error / legal limit | v2 mean error / legal limit |
|---|---:|---:|
| 12-round switching | 7.574% | 4.698% |
| DP response 1 | 16.350% | 4.407% |
| Frozen Hollan v1 | 13.801% | 4.156% |
| DP response 2 | 15.226% | 3.197% |
| DP response 3 | 13.145% | 2.688% |
| Cautious bait / maximum | 27.831% | 17.050% |
| Kelly bait / DP response 2 | 11.025% | 6.325% |
| Switching DP responses | 16.900% | 13.288% |
| Pseudorandom stakes | 45.716% | 46.778% |
| Threshold aggression | 5.548% | 6.413% |

Prediction improves strongly for planners and deceptive switches, while the faster memory is worse on random and threshold policies. These tradeoffs agree with the score results; universal prediction accuracy is not claimed. Full paired intervals and observation counts are in `results/prediction.txt`.

## Reliability, official runner and portability

- All **19 official tests** pass. Official referee, samples, templates and tests are unchanged.
- Independent exhaustive reference: **383,964 Bellman states** in both orientations and **175,658 integer-action comparisons**, including nested opponent predictions. AddressSanitizer and UndefinedBehaviorSanitizer report no errors. Forced time, horizon and memory fallbacks pass.
- **60 adversarial/reliability games** through the official referee cover tiny capitals, probabilities 0/1, frozen fair games, random 10–2,000-round configurations, and the 5,000-round fallback: **25,316 rounds**, zero violations.
- Two official `start.py` round-robin tournaments: eight bots, 28 pairings each, including v1 and three nested planning opponents. Additional `start.py` matches cover sample and typical parameters. Combined replay audit: **116 games, 33,340 rounds**, no invalid or adjusted bets, forfeits, crashes or timeouts. These small real tournaments are smoke tests, not the performance estimate: Hollan had 0 wins/1 loss/6 ties in the first seven-match round robin. All outcomes and seeds are preserved.
- Total before ZIP verification: **176 real referee games**, including **92 Hollan games**, **58,656 rounds**. Longest observed Hollan cumulative clock use: **2.33 seconds**, under concurrent benchmark load, versus the 120-second limit.
- Strict `-std=c++17 -O2 -Wall -Wextra -Wconversion -Wpedantic` compilation has **zero warnings** on Apple clang 21, arm64 macOS. Only standard C++17 facilities are used. Linux/Windows execution was not available here; the unchanged official cross-platform build script must rebuild the source on the competition host.

The ZIP is extracted into a clean temporary directory, compiled with its own build.py, and played through the official referee in both roles. See `results/package.json` for that final verification and the ZIP checksum.

## Submission

Send the root **Hollan_cpp17.zip**, name **Hollan**, language **C++17**. It contains only `hollan/bot.cpp`, `hollan/build.py`, and `hollan/README.md`. Extract to `bots/hollan/` in the official repository. The existing `bots.json` entry is already correct. Build with `python3 bots/hollan/build.py`, or let the official referee build automatically. No external libraries, network access or additional setup are required.

Reproduction commands and opponent identifiers are in [README.md](README.md). Raw discovery, validation, evolution, ablation, held-out, prediction and referee outputs are retained in `results/`.
'''
(here/'REPORT.md').write_text(report)
readme=f'''# Hollan — Kelly Game submission (v2)

**Name:** Hollan<br>
**Language:** C++17<br>
**Dependencies:** C++17 compiler and Python 3.8+ for the unchanged official build script. Standard libraries only.

## Submit

Send **Hollan_cpp17.zip**, containing this complete `hollan/` source folder. Extract it as `bots/hollan/` in the official repository and register:

```json
{{"name":"Hollan","cwd":"bots/hollan","command":["./bot"],"build":["{{python}}","build.py"]}}
```

From the repository root:

```sh
python3 bots/hollan/build.py
python3 start.py match Hollan "Kelly (C++)" --config params/competition_2000.txt --no-browser
```

The referee builds automatically. Rebuild on the competition machine; the ZIP contains no platform-specific binary. The official script supports `CXX`, c++, g++, clang++ and Visual Studio. Windows uses `bot.exe` automatically. No additional setup is required.

## Strategy

Hollan maximizes expected final capital using a full-horizon Bellman DP over capital share and all known future probabilities. It evaluates both outcomes exactly, including integer bets, the sum-of-bets pot, limited liability and bankruptcy. Ten elementary opponent policies cover Kelly scaling, aggression, passivity and opponent-aware Kelly. Four additional models recognize finite-horizon planning opponents, including successive best responses.

The previous opponent stake is inferred exactly from surviving capital transitions. Discounted prediction losses adapt quickly to policy switches. A confidence gate requires several accurate observations before using a planning model. The planner uses an approximately 800-point capital grid with exact boundary strips, coarse action search plus clipping kinks, and finer integer refinement for live decisions. Opponent-aware Kelly is the bounded-time fallback.

Tables have a 256 MiB budget, eight-second construction guards, a ten-second shared decision deadline and a 65-second cumulative internal computation cutoff, leaving headroom under the official 120-second clock. The protocol and build script are unchanged.

## Validation

The final paired comparison against the previous Hollan used **27 configurations, 29 opponents, both roles, and 67,500 pairings per opponent per strategy**: **7.83 million games** total.

{table([0,15,16,18,19,20,22,23])}

Across the full pool: **+0.3265 percentage points** over v1 (paired 95% interval **+0.3098 to +0.3433**). The largest measured regression is **−0.1027 points** against threshold aggression, within the 0.25-point tolerance used for adoption. These estimates describe the test suite, not a guarantee against unknown opponents.

All **19 official tests** pass. Independent exact DP checks cover **383,964 states** and **175,658 integer-action comparisons** under address/undefined-behavior sanitizers. **176 official-referee games** (92 involving Hollan), including strong planning opponents, edge cases and real start.py tournaments/matches, have zero invalid or adjusted bets, crashes or timeouts. Longest measured Hollan cumulative clock use: **2.33 seconds** on the development machine. Strict C++17 compilation is warning-free; rebuild and test on the actual competition host.

Only the permitted strategy section was changed. Official referee, samples and templates remain unchanged.
'''
(root/'bots/hollan/README.md').write_text(readme)
print('Wrote final report, submission README and summary JSON.')
