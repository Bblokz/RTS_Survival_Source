"""Run a python file in an editor over UECP: python py_run.py <port> <file.py> [description]."""
import sys
from uecp_client import action
src = open(sys.argv[2], encoding='utf-8').read()
r = action(int(sys.argv[1]), 'execute_python', {'code': src, 'description': sys.argv[3] if len(sys.argv) > 3 else 'FrozenLake mission setup'})
print(str(r)[:1500])
