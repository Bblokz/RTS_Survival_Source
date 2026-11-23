"""Tidy the Berlin stage graphs in my editor: splice out reroute (knot) nodes, then compact auto-arrange.

Usage: python tidy_graphs.py <port> <spacing_x> <spacing_y> [BlueprintName ...]
"""
import json
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
ALL = ['Mission_Berlin_Arrival', 'Mission_Berlin_ReachLeitturm', 'Mission_Berlin_HoldRelay',
       'Mission_Berlin_StayUndetected', 'Mission_Berlin_ResponseForce', 'Mission_Berlin_StopRunners',
       'Mission_Berlin_CellarPeople', 'Mission_Berlin_RailYard', 'Mission_Berlin_RaidReichstag',
       'Mission_Berlin_DemeterPod', 'BP_BerlinDetectionZone']


def graph_names(port, bp):
    skeleton = action(port, 'get_blueprint_skeleton', {'blueprint_path': bp})
    names = ['EventGraph']
    for fn in skeleton.get('functions', []):
        name = fn.get('name') if isinstance(fn, dict) else fn
        if name and name not in names:
            names.append(name)
    for g in skeleton.get('graphs', []):
        name = g.get('name') if isinstance(g, dict) else g
        if name and name not in names:
            names.append(name)
    return names


def splice_knots(port, bp, graph):
    snap = action(port, 'get_graph_snapshot', {'blueprint_path': bp, 'graph_name': graph, 'view': 'compact'})
    if not isinstance(snap, dict) or 'nodes' not in snap:
        return 0, 'no snapshot'
    nodes = {n['id']: n for n in snap['nodes']}
    knots = {nid for nid, n in nodes.items() if n.get('node_class') == 'K2Node_Knot'}
    if not knots:
        return 0, 'clean'
    # Output edges: (src, pin) -> [(dst, pin)]
    out_edges = {}
    for nid, n in nodes.items():
        for pin in n.get('pins', []):
            if pin.get('direction') != 'output':
                continue
            for link in pin.get('connected_to', []):
                out_edges.setdefault((nid, pin['name']), []).append((link['node_id'], link['pin']))

    def final_targets(dst, pin, seen):
        if dst not in knots:
            return [(dst, pin)]
        result = []
        for (src, src_pin), targets in out_edges.items():
            if src != dst or (src, src_pin) in seen:
                continue
            seen.add((src, src_pin))
            for t_dst, t_pin in targets:
                result += final_targets(t_dst, t_pin, seen)
        return result

    connections = []
    for (src, src_pin), targets in out_edges.items():
        if src in knots:
            continue
        for dst, dst_pin in targets:
            if dst in knots:
                for f_dst, f_pin in final_targets(dst, dst_pin, set()):
                    connections.append({'from_node': src, 'from_pin': src_pin, 'to_node': f_dst, 'to_pin': f_pin})
    deleted = action(port, 'delete_nodes', {'blueprint_path': bp, 'graph_name': graph, 'node_ids': sorted(knots)})
    result = action(port, 'connect_pins', {'blueprint_path': bp, 'graph_name': graph, 'connections': connections})
    ok = isinstance(result, dict) and result.get('ok', True)
    return len(knots), f'connections {len(connections)} ok={ok} ' + (json.dumps(result)[:300] if not ok else '')


def main():
    port, sx, sy = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    targets = sys.argv[4:] or ALL
    for name in targets:
        bp = BASE + name
        for graph in graph_names(port, bp):
            if graph == 'EventGraph':
                continue  # hand laid out by layout_eventgraphs.py
            count, note = splice_knots(port, bp, graph)
            arranged = action(port, 'arrange_blueprint_nodes',
                              {'blueprint_path': bp, 'graph_name': graph, 'spacing_x': sx, 'spacing_y': sy})
            arr_ok = arranged.get('ok') if isinstance(arranged, dict) else str(arranged)[:120]
            print(f'{name}.{graph}: knots {count} ({note}) arrange={arr_ok}')
        compiled = action(port, 'compile_blueprint', {'blueprint_path': bp})
        print(f'{name}: compile errors={compiled.get("error_count") if isinstance(compiled, dict) else compiled}')


if __name__ == '__main__':
    main()
