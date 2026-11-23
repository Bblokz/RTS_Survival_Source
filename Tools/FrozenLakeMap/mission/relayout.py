"""Re-lay the stage event graphs only (layout, not logic): python relayout.py <port> [StageName ...]"""
import re
import sys

import fl_graph
from uecp_client import action
from fl_stages import STAGES, path_of


def main():
    port = int(sys.argv[1])
    wanted = set(sys.argv[2:])
    for stage in STAGES:
        if wanted and stage['name'] not in wanted:
            continue
        fl_graph.SELF_SUBTITLE = 'Target is ' + re.sub(r'_', ' ', re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', stage['name']))
        g = stage['graph']()
        built = action(port, 'build_blueprint_graph', {
            'blueprint_path': path_of(stage), 'graph_name': 'EventGraph', 'clear_before_build': True,
            'clear_force': True, 'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults,
            'comments': g.comments})
        compiled = action(port, 'compile_blueprint', {'blueprint_path': path_of(stage)})
        saved = action(port, 'save_asset', {'asset_path': path_of(stage)})
        print(stage['name'], 'built', built.get('ok'), 'failed links', built.get('connections', {}).get('failed'),
              'errors', compiled.get('error_count'), 'saved', saved.get('ok'), flush=True)
        action(port, 'render_graph_image', {'blueprint_path': path_of(stage), 'graph_name': 'EventGraph',
                                            'save_path': 'D:/UE5Projects/RTS_Survival/Saved/FrozenLake/graphs/'
                                            + stage['name'] + '.png', 'max_dimension': 2400})


if __name__ == '__main__':
    main()
