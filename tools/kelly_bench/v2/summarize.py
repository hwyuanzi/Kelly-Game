import json,re,sys,math
from collections import defaultdict
rows=[]
for line in open(sys.argv[1]):
    if line.startswith('opp=') and 'variant=' in line:
        row={k:float(v) for k,v in re.findall(r'(\w+)=([\d.e+\-]+)',line)};rows.append(row)
by=defaultdict(list)
for r in rows:by[int(r['variant'])].append(r)
for v,rs in by.items():
    print('variant',v,'mean gain pp',round(100*sum(r['delta'] for r in rs)/len(rs),4),'score',round(100*sum(r['score'] for r in rs)/len(rs),3),'worst',[(int(r['opp']),round(r['delta']*100,3),round(r['ci95']*100,3)) for r in sorted(rs,key=lambda r:r['delta'])[:5]])
if len(sys.argv)>2:
    json.dump(rows,open(sys.argv[2],'w'),indent=2)
