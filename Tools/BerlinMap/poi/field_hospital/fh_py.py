"""Run editor Python in the field hospital throwaway editor: python fh_py.py <port> <file.py> (prints the output)."""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'mission'))
from uecp_client import call  # noqa: E402

if __name__ == '__main__':
    port, path = int(sys.argv[1]), sys.argv[2]
    r = call(port, 'execute_python', {'code': open(path, encoding='utf-8').read(), 'description': 'field hospital'})
    if isinstance(r, dict):
        print(r.get('output') or r.get('stdout') or json.dumps(r)[:3000])
    else:
        print(str(r)[:6000])
