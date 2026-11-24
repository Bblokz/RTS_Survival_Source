"""Readable pump death graph: two functions plus a one-row event graph (run after make_pumps_destructible.py)."""
import json
import sys

from uecp_client import action
from make_pumps_destructible import ON_DIES, PUMPS
from fl_graph import Graph

BODY = ON_DIES[0]['body']
EXPLOSIONS = BODY[:-2]   # location, offset, three explosions, scorch decal
COLLAPSE = BODY[-2:]     # make the swap struct, CollapseMeshWithSwapping


def event_graph():
    g = Graph()
    g.row([('evDies', 'ev.BP_OnUnitDies', None), ('boom', 'fn.Self.PlayPumpDeathExplosions', None),
           ('ruin', 'fn.Self.CollapseIntoRuin', None)])
    g.comment('The pump is destroyed: gas and medium explosions with a scorch mark, then it collapses into the ruin '
              'mesh (OnDestructibleCollapse: the DestroyPumps bonus counts it). The flood is not built yet.',
              ['evDies', 'boom', 'ruin'])
    return g


def main():
    port = int(sys.argv[1])
    for path in PUMPS:
        name = path.rsplit('/', 1)[1]
        for func, body in (('PlayPumpDeathExplosions', EXPLOSIONS), ('CollapseIntoRuin', COLLAPSE)):
            r = action(port, 'synthesize_graph', {'blueprint_path': path, 'function': func, 'body': body,
                                                  'overwrite': True})
            print(name, func, 'ok' if r.get('ok') else json.dumps(r)[:600], flush=True)
        g = event_graph()
        r = action(port, 'build_blueprint_graph', {
            'blueprint_path': path, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
            'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults, 'comments': g.comments})
        print(name, 'event graph', r.get('ok'), r.get('connections', {}).get('failed'), flush=True)
        c = action(port, 'compile_blueprint', {'blueprint_path': path})
        s = action(port, 'save_asset', {'asset_path': path})
        print(name, 'compile errors', c.get('error_count'), 'saved', s.get('ok'), flush=True)
        action(port, 'render_graph_image', {'blueprint_path': path, 'graph_name': 'EventGraph',
                                            'save_path': f'D:/UE5Projects/RTS_Survival/Saved/FrozenLake/graphs/{name}.png'})
        action(port, 'render_graph_image', {'blueprint_path': path, 'graph_name': 'PlayPumpDeathExplosions',
                                            'save_path': f'D:/UE5Projects/RTS_Survival/Saved/FrozenLake/graphs/{name}_explosions.png'})


if __name__ == '__main__':
    main()
