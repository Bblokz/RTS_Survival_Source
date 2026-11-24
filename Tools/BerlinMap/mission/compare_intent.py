import json, re, sys
from uecp_client import action
from tidy_graphs import ALL, BASE, graph_names
before=json.load(open('intent_before/intent.json'))
def norm(t):
    t=re.sub(r'NSLOCTEXT\([^)]*\)','TEXT',t or '')
    return t.strip()
bad=0
for name in ALL:
    bp=BASE+name
    for g in graph_names(30001,bp):
        r=action(30001,'get_graph_intent',{'blueprint_path':bp,'graph_name':g})
        now=r.get('intent') if isinstance(r,dict) else str(r)
        key=f'{name}.{g}'
        if norm(now)!=norm(before.get(key)):
            bad+=1
            print('DIFF', key)
            if len(sys.argv)>1:
                print('--- before\n', before.get(key)); print('--- after\n', now)
print('graphs with changed logic:', bad)
