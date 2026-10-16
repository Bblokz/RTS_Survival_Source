"""Builds a small German pit's building expansion through the editor bridge (UECP direct calls, see uecp.py).

Like the German gun pits (Tools/GerPits/build_gerpit_bxps.py) the pit is a copy of the German leFH building expansion
(player-owned, gun hidden while it is built, auto-engage when finished or when placed in a level) with the pit mesh as
building and construction mesh, its own expansion type (needs an editor built with BTX_GerPuppchenPit /
BTX_GerPanzerschreckPit), the pit turret as child actor, and the selection / decal / progress bar sized for it.

Run: python build_gersmallpit_bxp.py Puppchen|Panzerschreck   (scratchpad uecp.py importable; UECP_PORT picks the
editor)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Childs/LeFH105/RTS_BP_BXP_LeFH105mm'
# The base expansion's decal is 32 x 32 units before its component scale.
DECAL_UNITS = 32.0
# deck: the turret stands this high (the Puppchen pit has the gun pits' 4 cm plank deck, the foxhole none);
# radius: the bags and the ammo boxes outside them.
PITS = {
    'Puppchen': {'mesh': ROOT + '/Meshes/SM_GerPuppchenPit', 'type': 'BTX_GerPuppchenPit', 'deck': 4.0,
                 'radius': 230.0, 'height': 50.0, 'smoke_radius': 220.0, 'smokes': 3},
    'Panzerschreck': {'mesh': ROOT + '/Meshes/SM_GerPanzerschreckPit', 'type': 'BTX_GerPanzerschreckPit', 'deck': 0.0,
                      'radius': 190.0, 'height': 65.0, 'smoke_radius': 180.0, 'smokes': 3},
}


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', text[:900])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    print(result.get('result', {}).get('content', [{}])[0].get('text', '')[:300])


def init_node(bp):
    nodes = act('get_graph_snapshot', blueprint_path=bp, graph_name='EventGraph', view='compact')['nodes']
    matches = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp']
    if len(matches) != 1:
        print('FAILED BP_InitBuildingExp lookup', matches)
        sys.exit(1)
    return matches[0]


def build(pit_name):
    pit = PITS[pit_name]
    bp = f'{ROOT}/Ger_BXP_GerPit_{pit_name}'
    turret = f'{ROOT}/Turrets/RTS_BP_EmbTurret_GerPit_{pit_name}'
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{bp}'):
    eal.delete_asset('{bp}')
print('DUP', eal.duplicate_asset('{BASE}', '{bp}'))
eal.save_asset('{bp}', only_if_is_dirty=False)
""", 'small pit duplicate bxp')
    mesh_ref = f"{pit['mesh']}.{pit['mesh'].split('/')[-1]}"
    node = init_node(bp)
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewBuildingExpansionType', pit['type']), ('NewSmokeRadius', f"{pit['smoke_radius']:.1f}"),
                       ('NewAmountSmokes', str(pit['smokes']))):
        act('set_pin_default', blueprint_path=bp, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    # A placed pit never runs construction and shows this template mesh.
    act('set_static_mesh_component', blueprint_path=bp, component_name='BuildingMeshComponent', mesh_path=pit['mesh'])
    act('edit_component_property', blueprint_path=bp, component_name='Howitzer', property_name='ChildActorClass',
        value=f"{turret}.RTS_BP_EmbTurret_GerPit_{pit_name}_C")
    act('set_component_transform', blueprint_path=bp, component_name='Howitzer', location=[0.0, 0.0, pit['deck']])
    radius, height = pit['radius'], pit['height']
    decal_scale = 2.0 * radius / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={radius * 0.85:.1f},Y={radius * 0.85:.1f},Z={height * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={height * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeScale3D', '(X=1.0,Y=1.0,Z=1.0)'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={height + 230.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={radius:.1f},Y={radius:.1f},Z={height:.1f})'),
            ('RTS', 'OwningPlayer', '1')):
        act('edit_component_property', blueprint_path=bp, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=bp)
    print(pit_name, 'bxp compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=bp)


if __name__ == '__main__':
    name = sys.argv[1] if len(sys.argv) > 1 else ''
    if name not in PITS:
        print('usage: build_gersmallpit_bxp.py Puppchen|Panzerschreck')
        sys.exit(1)
    build(name)
