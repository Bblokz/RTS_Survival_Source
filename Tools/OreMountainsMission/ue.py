"""Ore Mountains mission tools: talk to our own editor on its direct UECP port (see Berlin's uecp_client.py)."""
import json, os, sys
sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'BerlinMap', 'mission')))
from uecp_client import call, action  # noqa: E402

PORT = int(os.environ.get('ORE_UECP_PORT', '30000'))
BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/'


def a(_action, **params):
    return action(PORT, _action, params)


def py(code):
    return a('execute_python', code=code, description='Ore mission stage build step')


if __name__ == '__main__':
    tool = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    out = call(PORT, tool, params) if tool in ('search_tools', 'get_tool_docs', 'call_action') else action(PORT, tool, params)
    print(json.dumps(out, indent=1)[:int(os.environ.get('ORE_MAX', '6000'))] if not isinstance(out, str) else out[:int(os.environ.get('ORE_MAX', '6000'))])
