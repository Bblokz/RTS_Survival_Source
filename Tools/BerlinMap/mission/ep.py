"""Run python code in my editor: python ep.py <port> <file>."""
import sys
from uecp_client import action
r = action(int(sys.argv[1]), 'execute_python', {'code': open(sys.argv[2], encoding='utf-8').read(), 'description': 'Berlin patrols'})
if isinstance(r, dict):
    for k in ('ok', 'output', 'stdout', 'error', 'result'):
        if k in r: print(k, ':', r[k] if not isinstance(r[k], str) else r[k][-6000:])
else:
    print(str(r)[-6000:])
