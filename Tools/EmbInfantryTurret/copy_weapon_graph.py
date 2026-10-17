"""Copies an infantry weapon Blueprint's BeginPlay weapon setup into an embedded infantry turret Blueprint.

The turret makes the same Setup...Weapon call with the same pin values: the weapon's StaticMesh becomes the turret's
WeaponMesh and GetOwningPLayerForWeaponInit the turret master's GetOwningPlayerForWeaponInit. Weapon Blueprint
variables are inlined as literals. A Make Map node (the bridge cannot add entries to one) becomes the map variable
SurfaceImpactEffects, filled with Map_Add before the setup call.
Run: python copy_weapon_graph.py [turret names...]   (uecp.py importable via PYTHONPATH; UECP_PORT picks an editor)
"""
import json
import sys

from uecp import call_tool

W = '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/Russian/'
T = '/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/RusBasic/'
JOBS = {
    'Rus_EIT_BasicSoviet_Mosin': W + 'Mosin/Rus_Weapon_Mosin1',
    'Rus_EIT_BasicSoviet_PTRS': W + 'PTRS/Rus_Weapon_PTRS',
    'Rus_EIT_Hazmat_Bazooka': W + 'Bazooka/Rus_Weapon_Bazooka',
}
HANDLE_MAP = {
    'var.get.StaticMesh': 'var.get.WeaponMesh',
    'fn.Self.GetOwningPLayerForWeaponInit': 'fn.Self.GetOwningPlayerForWeaponInit',
}
MAP_VARIABLE = 'SurfaceImpactEffects'


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    return json.loads(text, strict=False)


def variable_default(blueprint, variable):
    return act('get_blueprint_variable', blueprint_path=blueprint, variable_name=variable)['variable']['default']


def upstream(nodes, start_id):
    keep, stack = set(), [start_id]
    while stack:
        node = nodes[stack.pop()]
        if node['id'] in keep:
            continue
        keep.add(node['id'])
        for pin in node['pins']:
            if pin['direction'] == 'input' and pin['type'] != 'exec':
                stack += [link['node_id'] for link in pin.get('connected_to', [])]
    return keep


def translate(source_bp):
    snapshot = act('get_graph_snapshot', blueprint_path=source_bp, graph_name='EventGraph')
    nodes = {node['id']: node for node in snapshot['nodes']}
    setup = next(n for n in snapshot['nodes'] if (n.get('handle') or '').startswith('fn.Self.Setup'))
    begin = next(n for n in snapshot['nodes'] if n.get('handle') == 'ev.ReceiveBeginPlay')
    keep = upstream(nodes, setup['id']) | {begin['id']}

    inlined = {}
    for node_id in list(keep):
        handle = nodes[node_id].get('handle') or ''
        if handle.startswith('var.get.') and handle not in HANDLE_MAP:
            inlined[node_id] = variable_default(source_bp, handle[len('var.get.'):])
            keep.discard(node_id)
    make_maps = [node_id for node_id in keep if nodes[node_id].get('handle') == 'k2.MakeMap']
    assert len(make_maps) <= 1, 'one surface map per weapon expected'
    keep -= set(make_maps)

    ids = {node_id: 'n%d' % index for index, node_id in enumerate(sorted(keep))}
    out_nodes, connections, defaults = [], [], []
    exec_chain = [ids[begin['id']]]
    for map_id in make_maps:
        map_node = nodes[map_id]
        out_nodes.append({'id': 'mapget', 'handle': 'var.get.' + MAP_VARIABLE,
                          'x': map_node['position']['x'], 'y': map_node['position']['y']})
        keys = [pin for pin in map_node['pins'] if pin['name'].startswith('Key ')]
        for index, key_pin in enumerate(keys):
            add_id = 'mapadd%d' % index
            out_nodes.append({'id': add_id, 'handle': 'fn.BlueprintMapLibrary.Map_Add', 'x': 250 + 250 * index, 'y': -300})
            defaults.append({'node_id': add_id, 'pin_name': 'Key', 'value': key_pin['default']})
            connections.append({'from': 'mapget', 'from_pin': MAP_VARIABLE, 'to': add_id, 'to_pin': 'TargetMap'})
            value_pin = next(pin for pin in map_node['pins'] if pin['name'] == 'Value %d' % index)
            link = value_pin['connected_to'][0]
            connections.append({'from': ids[link['node_id']], 'from_pin': link['pin'], 'to': add_id, 'to_pin': 'Value'})
            exec_chain.append(add_id)

    for node_id in keep:
        node = nodes[node_id]
        out_nodes.append({'id': ids[node_id], 'handle': HANDLE_MAP.get(node['handle'], node['handle']),
                          'x': node['position']['x'], 'y': node['position']['y']})
        for pin in node['pins']:
            if pin['direction'] != 'input':
                continue
            links = [link for link in pin.get('connected_to', [])
                     if link['node_id'] in keep or link['node_id'] in inlined or link['node_id'] in make_maps]
            for link in links:
                if link['node_id'] in inlined:
                    defaults.append({'node_id': ids[node_id], 'pin_name': pin['name'], 'value': inlined[link['node_id']]})
                elif link['node_id'] in make_maps:
                    connections.append({'from': 'mapget', 'from_pin': MAP_VARIABLE, 'to': ids[node_id],
                                        'to_pin': pin['name']})
                else:
                    is_weapon_mesh = nodes[link['node_id']].get('handle') == 'var.get.StaticMesh'
                    connections.append({'from': ids[link['node_id']],
                                        'from_pin': 'WeaponMesh' if is_weapon_mesh else link['pin'],
                                        'to': ids[node_id], 'to_pin': pin['name']})
            if not links and 'default' in pin and pin['type'] != 'exec':
                defaults.append({'node_id': ids[node_id], 'pin_name': pin['name'], 'value': pin['default']})

    exec_chain.append(ids[setup['id']])
    for previous, following in zip(exec_chain, exec_chain[1:]):
        connections.append({'from': previous, 'from_pin': 'then', 'to': following, 'to_pin': 'execute'})
    return out_nodes, connections, defaults, bool(make_maps)


def main():
    for target, source in JOBS.items():
        if len(sys.argv) > 1 and target not in sys.argv[1:]:
            continue
        blueprint = T + target
        out_nodes, connections, defaults, needs_map = translate(source)
        if needs_map:
            act('add_variable', blueprint_path=blueprint, variable_name=MAP_VARIABLE,
                variable_type='TMap<ERTSSurfaceType,RTSSurfaceImpactData>')
            act('compile_blueprint', blueprint_path=blueprint)
        result = act('build_blueprint_graph', blueprint_path=blueprint, graph_name='EventGraph', nodes=out_nodes,
                     connections=connections, defaults=defaults, clear_before_build=True, clear_force=True)
        print('==', target, 'ok', result.get('ok'), 'nodes', len(out_nodes))
        for key in ('errors', 'failed_nodes', 'failed_connections', 'error'):
            if result.get(key):
                print(' ', key, json.dumps(result[key])[:1500])
        if isinstance(result.get('defaults'), dict) and result['defaults'].get('failed'):
            print('  failed defaults', [d for d in result['defaults']['details'] if not d.get('success')])
        compiled = act('compile_blueprint', blueprint_path=blueprint)
        print('  compile ok', compiled.get('ok'), 'errors', compiled.get('error_count'), compiled.get('errors'),
              compiled.get('action_required'))
        print('  saved', act('save_asset', asset_path=blueprint).get('ok'))


if __name__ == '__main__':
    main()
