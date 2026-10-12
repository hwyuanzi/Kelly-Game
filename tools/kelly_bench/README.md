**Current submission: Hollan v2.** See [v2/README.md](v2/README.md) for the paired upgrade benchmark, adversarial search, ablations and strong-opponent referee tournament. The commands below reproduce the historical v1 study; its C++ benchmark and exact validator now include the frozen v1 source. `reliability.py` always checks the current production executable.

# Hollan benchmark and validation tools

Run commands from the repository root. These tools require only the official Python/C++ prerequisites. They do not modify the referee or sample bots.

```sh
c++ -std=c++17 -O2 -Wno-return-type tools/kelly_bench/benchmark.cpp -o /tmp/kelly-benchmark
/tmp/kelly-benchmark 3000 16 800 24 42611039 > /tmp/hollan-final-heldout.txt
/tmp/kelly-benchmark 1000 8 800 24 98172 1 > /tmp/hollan-challenge.txt

c++ -std=c++17 -O1 -g -Wno-return-type -fsanitize=address,undefined -fno-omit-frame-pointer tools/kelly_bench/validate.cpp -o /tmp/kelly-validate
/tmp/kelly-validate
python3 tools/kelly_bench/reliability.py
python3 -m unittest discover -s tests -v

python3 start.py tournament --registry tools/kelly_bench/registry.json --config params/competition_2000.txt --output results/hollan-real-tournament
```

`benchmark.cpp` includes the actual submission source. Arguments are: random draw pairs per configuration, number of randomized configurations in addition to the three supplied ones, interior grid resolution, coarse action subdivisions, PRNG seed, and optional challenge-suite flag. The renamed protocol `main` is never called; `-Wno-return-type` suppresses the warning caused solely by renaming the official template's main function.

`baseline_bot.py` makes the conservative, aggressive and adaptive policies available through the real official runner, using the unmodified Python template's communication section. `registry.json` is a separate benchmark roster; production `bots.json` adds only Hollan.

`validate.cpp` checks every integer action against an independent exhaustive backward recurrence for small games, and exercises computation-budget fallbacks. `reliability.py` runs the real executable against the unmodified referee and asserts legality, no adjustment, no forfeits, and conservation.

See `REPORT.md` for definitions, results, limitations and the optimization decisions. Raw outputs are in `results/`.
