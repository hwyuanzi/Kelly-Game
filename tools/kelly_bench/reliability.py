"""Exercise the final executable through the unmodified official referee."""
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from referee.runner import build, load_bots, play_game

roster = {b['name']: b for b in load_bots('tools/kelly_bench/registry.json')}
hollan = roster['Hollan']
build(hollan)
rng = random.Random(553172)
cases = [(1, 1, [0, .5, 1] * 3), (4, 4, [.5] * 2000),
         (1000, 8000, [.5] * 2000), (8000, 8000, [0, 1] * 1000),
         (1000, 8000, [.5] * 5000), (5, 1000, [1, 0, .5] * 5)]
for _ in range(24):
    n = rng.choice([10, 30, 100, 300, 2000])
    cases.append((rng.randint(1000, 8000), rng.randint(1000, 8000),
                  [round(rng.uniform(.1, .9), 5) for _ in range(n)]))
errors = []
rounds = 0
worst = 0
for i, config in enumerate(cases):
    opponent = roster[['Kelly (Python)', 'Conservative', 'Aggressive', 'Adaptive'][i % 4]]
    for role in [0, 1]:
        players = [hollan, opponent] if role == 0 else [opponent, hollan]
        result = play_game(players, config, seed=10531 + i, clock=120, game=role + 1)
        assert not any(result['errors']), result['errors']
        assert sum(result['capital']) == config[0] + config[1]
        for row in result['rows']:
            assert 'adjusted_from' not in row, row
            assert min(row['capital']) >= 0
        rounds += len(result['rows'])
        worst = max(worst, 120 - result['clocks'][role])
print(json.dumps({'games': len(cases) * 2, 'rounds': rounds, 'errors': errors,
                  'max_cumulative_seconds': worst}, indent=2))
