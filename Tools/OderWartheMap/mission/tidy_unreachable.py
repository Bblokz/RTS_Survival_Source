"""Deletes return nodes no exec wire reaches (left over when synthesize_graph rebuilt a function) in the given
graphs of Mission_OW_Arrival. Usage: python tidy_unreachable.py <port> <graph> [...]"""
import sys

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
from uecp_client import action  # noqa: E402

BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/Mission_OW_Arrival'


def stray_returns(snapshot):
    reached = {link['to'] for node in snapshot['nodes'] for link in node.get('exec_to', [])}
    return [node['id'] for node in snapshot['nodes']
            if node['node_class'] == 'K2Node_FunctionResult' and node['id'] not in reached]


def main():
    port = int(sys.argv[1])
    for graph in sys.argv[2:]:
        snapshot = action(port, 'get_graph_snapshot', {'blueprint_path': BP, 'graph_name': graph, 'view': 'compact'})
        stray = stray_returns(snapshot)
        # A function with no exec reaching any return keeps one (the graph needs its result node).
        if len(stray) == sum(1 for n in snapshot['nodes'] if n['node_class'] == 'K2Node_FunctionResult'):
            stray = stray[1:]
        if stray:
            result = action(port, 'delete_nodes', {'blueprint_path': BP, 'graph_name': graph, 'node_ids': stray})
            print(graph, 'deleted', len(stray), str(result)[:120])
        else:
            print(graph, 'clean')
    compiled = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile errors', compiled.get('error_count'))


if __name__ == '__main__':
    main()
