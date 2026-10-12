# Hollan v2 research and reproduction

Run from the repository root. The production bot is `bots/hollan/bot.cpp`; `baseline.cpp` is the exact pre-upgrade Hollan v1, SHA256 `da83ddb7ea59a8a47ff16c46b6d587bdf79fd38e5fdd9bb0f3c1d54748394c18`. Official files are never modified.

```sh
c++ -std=c++17 -O2 -Wno-return-type tools/kelly_bench/v2/arena.cpp -o /tmp/hollan-arena

# Frozen final held-out comparison: v1 versus v2, original production memory caps.
/tmp/hollan-arena 2500 24 941778320 4 0,1,4,7,5,6,3,10,11,12,13,14,15,16,17,18,19,20,21,22,23,100,101,102,103,104,105,106,107

# Independent ablation suite: full v2, one planning model, no planning models.
/tmp/hollan-arena 1000 12 82910435 4,7,8 0,7,11,12,15,16,18,19,20,21

# Automatic parametric adversary discovery, targeting frozen v1.
c++ -std=c++17 -O2 -Wno-return-type tools/kelly_bench/v2/evolve.cpp -o /tmp/hollan-evolve
/tmp/hollan-evolve 339841 180 0 > /tmp/evolved-opponents.txt

# Independent exact checks and sanitizers.
c++ -std=c++17 -O1 -g -Wno-return-type -fsanitize=address,undefined -fno-omit-frame-pointer tools/kelly_bench/v2/validate.cpp -o /tmp/hollan-validate
/tmp/hollan-validate
python3 -m unittest discover -s tests -v
python3 tools/kelly_bench/reliability.py

# Real official runner, with three nested planning opponents and frozen v1.
python3 tools/kelly_bench/v2/build_live.py
python3 start.py tournament --registry tools/kelly_bench/v2/registry.json --config params/competition_2000.txt --output results/hollan-v2-tournament
python3 tools/kelly_bench/v2/check_replays.py
```

Arena arguments: paired draw trials per configuration, randomized configurations in addition to the three supplied ones, seed, comma-separated candidate variants, optional comma-separated opponent IDs. Every comparison uses identical probability sequences, initial capitals and outcome draws, with independent draws for A/B games. Variant 0 (v1) is always included. Final variant 4 is also the production default. Variants 1/2 omit confidence gating; 3 keeps the old slow memory; 7 retains only the first planning model; 8 retains only elementary policies. Historical rejected worst-case blends and conditional memory are captured in `candidate_stage1.cpp` and `candidate_memory.cpp`.

The arena memoizes *financial decisions*, keyed by role, round, capital and selected model. It still runs inference and updates every game's history independently. The first 100 cache hits per configuration are checked against direct strategy calls. `-DARENA_NO_MEMO` disables this optimization; `results/direct-smoke.txt` and `results/cache-fast-smoke.txt` have identical scores and prediction errors. All table memory caps match production in the final arena. The preliminary `heldout.txt` used larger simulation caches; `heldout-production.txt` is authoritative.

Opponent IDs: 0–9 are the original elementary policies; 10 bankroll-dependent Kelly, 11 twelve-round switching, 12 deterministic pseudorandom stake sizes, 13 probability-threshold aggression, 14 0.6x Kelly, 15 first DP best response, 16 frozen adaptive Hollan v1, 17 v2 self-play, 18 second DP response, 19 third DP response, 20 cautious bait followed by maximum betting, 21 three-round passive/maximum switching, 22 Kelly bait followed by the second DP response, 23 five-round switching between the first and third DP responses. 100–107 use the eight evolved policy vectors in `genes.txt`; the second four were evolved against a v2 candidate on separate training scenarios.

`REPORT.md` explains selection criteria, the final results and limitations. Raw logs and verification outputs are in `results/`. `Hollan_v1.zip` preserves the complete previous submission. The final source-only submission ZIP is at the repository root.
