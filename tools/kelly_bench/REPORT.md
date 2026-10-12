**Historical Hollan v1 report.** The current upgrade report is [v2/REPORT.md](v2/REPORT.md).

# Hollan validation report

## Objective and rules

The repository README, all language templates and sample policies, `referee/engine.py`, `runner.py`, match/tournament entry points, parameter generator, supplied parameter files, and official tests were read before implementation. No additional linked rule document exists in the repository. The enforced rules are authoritative for this implementation.

The score is terminal money added over two independent role-swapped games. Capital is conserved. Both submitted bets contribute to the transfer, which is capped by the loser's capital. This differs from a conventional independent Kelly bet. Maximizing log growth, or simply wagering the most whenever the current edge is positive, therefore misses both clipping and future opportunities.

For a fixed inferred opponent policy and conserved total `T`, the planner approximates:

```text
V_N(x) = x / T
V_t(x) = max over integer 0 <= a <= floor(x/5) of
    q_t       * V_(t+1)(x + min(a + opponent_bet(t,x), T-x))
  + (1-q_t)   * V_(t+1)(x - min(a + opponent_bet(t,x), x))
V_t(0) = 0; V_t(T) = 1
```

This is a model-based best response in a zero-sum stochastic game, not a claim of a solved minimax equilibrium. Opponent actions depend on the current public state, not on the simultaneous bet that they cannot see. The original protocol provides no opponent identity or bets. A surviving round nevertheless reveals its uncapped transfer, so the prior opposing bet is `abs(new_capital-old_capital)-our_previous_bet`.

Ten opponent families are scored by exponentially discounted squared prediction error, with capped outlier influence, a weak official-Kelly prior, a one-dollar tolerance, and switching hysteresis. The selected family's entire future capital-dependent behavior is represented in the Bellman table. The terminal-money objective can favor an unfavorable immediate bet when it improves access to better future states.

## Empirical iteration

1. Official Kelly, opponent-aware Kelly, aggressive favorable-round maximum, and a finite-horizon risk-adjusted Kelly heuristic were compared on common random draws.
2. A backward dynamic program with inferred opponents improved money share by roughly 6–7 points on the initial six-configuration suite, versus about 1–2 points from the simple Kelly changes.
3. The suite expanded to 19 configurations and 19,000 pairings per opponent. The gain persisted against seven baseline families.
4. Resolution 400/actions 16 and resolution 1600/actions 48 were compared on the same 11 configurations and 11,000 pairings per opponent (seed 77221). Against Kelly the money shares were 57.07% and 57.10%; other changes were small and mixed. Resolution 800/actions 24 was retained to control runtime and memory.
5. A two-model weighted continuation experiment was tested against the baseline and challenge suites. Changes were mixed and too small to justify additional tables and decision complexity. It was discarded. Historical experiment logs remain in `results/`; the final harness implements the selected policy, not the discarded mixture variant.
6. A new final seed was reserved for the final held-out outcome evaluation. No strategy parameters were changed after examining it.

Runtime Monte Carlo action rollouts were unnecessary: there are only two possible round outcomes, and direct Bellman expectations remove sampling noise. Monte Carlo draws were used extensively to choose the strategy empirically.

## Final held-out benchmark

Command:

```sh
/tmp/kelly-benchmark 3000 16 800 24 42611039
```

There are three supplied configurations and 16 randomized probability sequences. Randomized horizons cycle through 10, 30, 100, 300, 2000, 100, 50 and 500 rounds. Capitals vary between $1000–4000 for A and $4000–8000 for B. Sequences include A-biased, B-biased, nearly fair, wide-range and changing-regime distributions. Both roles are always evaluated, with independent draws in each role.

Each configuration receives 3,000 draw pairs, giving **57,000 role-swapped pairings per opponent**. Seven opponents give **399,000 Hollan pairings / 798,000 Hollan simulated games**. Six tested candidate strategies bring the final run to **4,788,000 simulated games** in total. The harness directly includes `bots/hollan/bot.cpp`, verifies each submitted Hollan bet, and simulates the referee's transfer and bankruptcy rules. Tables are reused between trials for speed; inference state resets for every game. Real-process checks separately cover clocks and protocol behavior.

The score is `(our capital as A + our capital as B) / (2 * starting total)`. Each configuration receives equal weight, avoiding larger starting amounts dominating the comparison. Gains compare Hollan and the unmodified official Kelly formula against the **same opponent on the same draw pair**. Confidence intervals are approximate normal intervals for paired money-share differences on these fixed test scenarios; they do not quantify uncertainty about unknown competition opponents or a wider population of probability sequences.

| Opponent | Kelly baseline share | Hollan share | Paired gain (95% half-width) | Strict wins / ties |
|---|---:|---:|---:|---:|
| Official Kelly | 49.93% | 56.99% | +7.06 ± 0.17 pp | 31.06% / 52.15% |
| Conservative half-Kelly | 53.29% | 59.51% | +6.22 ± 0.16 pp | 36.05% / 48.71% |
| Aggressive maximum on favorable rounds | 49.50% | 56.40% | +6.90 ± 0.17 pp | 29.12% / 53.71% |
| Adaptive opponent-aware Kelly | 48.95% | 55.15% | +6.20 ± 0.17 pp | 27.93% / 54.31% |
| Always maximum | 57.06% | 63.21% | +6.15 ± 0.16 pp | 37.64% / 51.68% |
| Passive | 78.37% | 80.21% | +1.84 ± 0.09 pp | 91.42% / 0.00% |
| Double Kelly | 49.05% | 56.10% | +7.04 ± 0.18 pp | 29.03% / 53.52% |
Raw results: `results/final-heldout.txt` and `results/final-summary.json`.

Against Kelly, the 7.06-point share gain means approximately 14.1% more expected final money than the Kelly baseline on this suite. Strict pairing wins were 31.06%, ties 52.15%, and losses 16.79%; the many ties arise when each bot wins one entire game. Score share is the primary metric specified by the rules.

## Additional opponents outside the exact model list

The independent challenge run used 11 configurations, 11,000 pairings per opponent and seed 98172. It tested bankroll-dependent Kelly scaling, a policy alternating every 12 rounds, varying pseudo-random bet sizes, a favorable-probability threshold, 0.6x Kelly, and a separate finite-horizon optimizer that expects official Kelly.

| Opponent | Hollan share | Gain versus official Kelly on same opponent/draws |
|---|---:|---:|
| Bankroll-dependent scaling | 56.79% | +5.84 pp |
| Switching half-Kelly / aggressive | 56.51% | +4.65 pp |
| Varying pseudo-random bet size | 61.62% | +3.29 pp |
| Threshold maximum bettor | 56.00% | +5.41 pp |
| 0.6x Kelly | 57.68% | +5.43 pp |
| Finite-horizon optimizer | 49.90% | +6.50 pp |

The separate optimizer is a useful strong-opponent control: its matchup against Hollan is approximately even. Hollan's model family is finite, so adversarial or substantially different policies can still reduce performance. The implementation does not inspect files, processes, the opponent's source/name, hidden seeds, or future random outcomes.

## Official matches and reliability

- All **19 official unit tests passed**, including the installed Julia sample check. Original tests were not edited.
- AddressSanitizer/UndefinedBehaviorSanitizer validated **170,380 Bellman states** against an independent, fully enumerated integer recurrence. Every live integer action is also exhaustively checked on these small games.
- **60 additional games / 25,316 rounds** through `referee.runner.play_game` produced zero errors, adjusted bets, crashes or timeouts. Cases included $1/$1, $4/$4, $5/$1000, probabilities 0/0.5/1, frozen games, 2000 rounds, maximum official capitals and a 5000-round fallback. Maximum measured cumulative Hollan clock usage was **0.292 seconds** in this run.
- A `start.py tournament` run completed **15 pairings / 30 games**, including official C++ and Python Kelly and the conservative/aggressive/adaptive baselines, with zero forfeits. Maximum cumulative Hollan clock usage in the tournament was **0.40 seconds**. The randomized tournament had one Hollan win, one loss, and three ties; this small sample is a smoke test, not the performance estimate.
- Final-source `start.py match` runs against C++ Kelly completed on all three supplied files. Illustrative totals were sample **4155–845**, typical **11000–0**, and competition **9000–9000**. Their secret seeds and full replays are stored by the unchanged runner under `results/hollan-final-*`; CLI outputs are copied to this report's `results/` folder.
- Strict C++17 compilation passed with `-Wall -Wextra -Wconversion -Wpedantic` and zero warnings using Apple clang 21.0.0. Only standard C++17 headers/features are used. Linux and Windows execution were not available on this host; rebuild from source on the target host using the unchanged cross-platform official `build.py`.
- A byte comparison confirms both communication sections match the official template, and `build.py` is byte-identical. `git diff` confirms all official referee, sample bot, template and test files remain unchanged.

The production table cache is bounded to approximately 160 MiB. Each new table has an 8-second construction guard; an incomplete table is discarded and not retried. After 65 seconds of internal computation, or for horizons beyond 4000, Hollan uses the legal opponent-aware Kelly fallback. Allocation failures also fall back to a legal wager. These guards preserve headroom for referee I/O within the cumulative 120-second limit. Timing observations are machine-specific, not a promise of the same speed on every host.

## Reproduction and submission

The adjacent README lists exact compile, simulation, sanitizer, official-test and tournament commands. `baseline_bot.py` and the separate `registry.json` provide real-process baseline opponents without changing samples. Simulation IDs: strategies 0=official Kelly, 7=opponent-aware Kelly, 4=aggressive, 12=horizon heuristic, 10=Hollan, 11=oracle opponent-model DP; strategy 13 in the challenge suite is DP fixed to the Kelly opponent model. Baseline opponent IDs use the strategy model numbers; challenge IDs 10–15 follow the challenge table above. The oracle comparator is an analysis control and is not used by the submitted bot. Older experiment logs report tie-half-credit as `wins`; the final held-out log explicitly separates strict wins and ties.

Submit the root-level `Hollan_cpp17.zip` with the name **Hollan** and language **C++17**. It contains only the self-contained source folder and its instructions; compiled binaries and benchmarking tools are excluded. `bots/hollan/README.md` contains organizer-ready setup instructions. No push or commit was made.
