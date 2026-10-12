# Hollan — Kelly Game submission (v2)

**Name:** Hollan<br>
**Language:** C++17<br>
**Dependencies:** C++17 compiler and Python 3.8+ for the unchanged official build script. Standard libraries only.

## Submit

Send **Hollan_cpp17.zip**, containing this complete `hollan/` source folder. Extract it as `bots/hollan/` in the official repository and register:

```json
{"name":"Hollan","cwd":"bots/hollan","command":["./bot"],"build":["{python}","build.py"]}
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

| Opponent | v1 share | v2 share | Paired gain, percentage points (95% CI) |
|---|---:|---:|---:|
| Official Kelly | 56.042% | 56.042% | +0.000 [+0.000, +0.000] |
| DP response 1 | 49.988% | 50.975% | +0.987 [+0.872, +1.101] |
| Frozen Hollan v1 | 49.978% | 50.810% | +0.833 [+0.714, +0.952] |
| DP response 2 | 48.908% | 51.892% | +2.984 [+2.844, +3.123] |
| DP response 3 | 50.081% | 51.685% | +1.603 [+1.471, +1.735] |
| Cautious bait / maximum | 62.592% | 63.346% | +0.755 [+0.658, +0.851] |
| Kelly bait / DP response 2 | 51.322% | 52.389% | +1.067 [+0.958, +1.176] |
| Switching DP responses | 49.707% | 49.877% | +0.171 [+0.059, +0.282] |


Across the full pool: **+0.3265 percentage points** over v1 (paired 95% interval **+0.3098 to +0.3433**). The largest measured regression is **−0.1027 points** against threshold aggression, within the 0.25-point tolerance used for adoption. These estimates describe the test suite, not a guarantee against unknown opponents.

All **19 official tests** pass. Independent exact DP checks cover **383,964 states** and **175,658 integer-action comparisons** under address/undefined-behavior sanitizers. **176 official-referee games** (92 involving Hollan), including strong planning opponents, edge cases and real start.py tournaments/matches, have zero invalid or adjusted bets, crashes or timeouts. Longest measured Hollan cumulative clock use: **2.33 seconds** on the development machine. Strict C++17 compilation is warning-free; rebuild and test on the actual competition host.

Only the permitted strategy section was changed. Official referee, samples and templates remain unchanged.
