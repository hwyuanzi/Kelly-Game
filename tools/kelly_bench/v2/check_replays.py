"""Audit official runner replays, whose capitals/clocks are in TEAM order."""
import json
from pathlib import Path
records=[];max_used=0;rounds=0
paths=sorted(Path('results/hollan-v2-tournament').rglob('results.json'))+list(Path('results/hollan-v2-sample').glob('results.json'))+list(Path('results/hollan-v2-typical').glob('results.json'))
for p in paths:
    x=json.loads(p.read_text())
    if not isinstance(x,dict):continue
    s=x['summary'];assert not any(g['errors'] for g in s['matches'])
    prev=None
    for row in x['history']:
        assert not any('adjusted' in k for k in row)
        if 'bet_A' in row:
            assert prev is not None
            capital=prev if row['match']==1 else prev[::-1]
            assert 0<=row['bet_A']<=capital[0]//5 and 0<=row['bet_B']<=capital[1]//5
            rounds+=1
        prev=row.get('capital',prev)
        if 'Hollan' in x['names'] and 'clocks' in row:
            index=x['names'].index('Hollan');max_used=max(max_used,120-row['clocks'][index])
    records.append(dict(s,seed=x['seed'],config=x['config']))
result={'pairings':len(records),'games':len(records)*2,'rounds':rounds,'errors':0,'invalid_or_adjusted_bets':0,'hollan_max_cumulative_seconds':round(max_used,4),'results':records}
Path('tools/kelly_bench/v2/results/real-referee-summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='results'},indent=2))
