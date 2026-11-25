"""Prints the camera tour shots of a Berlin stage function (MakeStruct MissionCameraTourShot pin defaults + links).
Usage: python inspect_tour.py <port> <blueprint name> <function>"""
import json
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'


def main():
    port, bp, graph = int(sys.argv[1]), BASE + sys.argv[2], sys.argv[3]
    snap = action(port, 'get_graph_snapshot', {'blueprint_path': bp, 'graph_name': graph, 'view': 'full'})
    if not isinstance(snap, dict) or 'nodes' not in snap:
        print(str(snap)[:2000])
        return
    for node in snap['nodes']:
        title = node.get('title') or node.get('node_class')
        pins = []
        for pin in node.get('pins', []):
            if pin.get('direction') != 'input' or pin.get('type') == 'exec':
                continue
            links = [f"{l['node_id']}.{l['pin']}" for l in pin.get('connected_to', [])]
            value = pin.get('default_value', pin.get('default'))
            if links or value not in (None, '', '0.0', '0', 'false'):
                pins.append(f"{pin['name']}={'<-' + ','.join(links) if links else value}")
        print(node['id'], title, '|', '; '.join(pins))


if __name__ == '__main__':
    main()
