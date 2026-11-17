"""Read-only: print node ids and input pin defaults of a Blueprint graph (search filter). Outside the editor.
usage: python pw_graph_find.py <bp path> <graph> <search> [raw]"""
import json
import sys

import pw_uecp as u

bp, graph, search = sys.argv[1:4]
d = u.act('get_blueprint_graph', blueprint_path=bp, graph_name=graph, search=search, detail='full',
          include_unconnected_pins=True)
if len(sys.argv) > 4:
    print(json.dumps(d, indent=1)[:12000])
    sys.exit()
nodes = d.get('nodes') or (d.get('graph') or {}).get('nodes') or []
if not nodes:
    print(json.dumps(d)[:2000])
for n in nodes:
    print(n.get('id'), '|', n.get('title') or n.get('node_title'), '|', n.get('class') or n.get('node_class') or '')
    for p in n.get('pins', []):
        dv = p.get('default_value') or p.get('default') or p.get('default_object')
        if dv not in (None, '', 'None', '0', '0.0', 'false'):
            print('     ', p.get('name'), '=', str(dv)[:160])
