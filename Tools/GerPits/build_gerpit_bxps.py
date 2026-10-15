"""Builds the German gun pit building expansions through the editor bridge (UECP direct calls, see uecp.py).

Each pit is a copy of the German leFH building expansion (player-owned, gun hidden while it is built, auto-engage
when finished or when placed in a level) with the pit mesh as building and construction mesh, its own expansion
type, the pit turret as child actor standing on the plank deck, and the selection / decal / progress bar sized for
the sandbag ring.

Run: python build_gerpit_bxps.py [Pak40,LeFH18,SFH18]
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Childs/LeFH105/RTS_BP_BXP_LeFH105mm'
# The pit deck is this high above the ground; the guns stand on it.
DECK_HEIGHT = 4.0
# The base expansion's decal is 32 x 32 units before its component scale.
DECAL_UNITS = 32.0

PITS = {
    'Pak40': {'mesh': ROOT + '/Meshes/SM_GerPit_Pak', 'type': 'BTX_GerPak40Pit', 'radius': 470.0, 'height': 70.0,
              'smoke_radius': 380.0},
    'LeFH18': {'mesh': ROOT + '/Meshes/SM_GerPit_Howitzer', 'type': 'BTX_GerLeFH18Pit', 'radius': 620.0, 'height': 110.0,
               'smoke_radius': 520.0},
    'SFH18': {'mesh': ROOT + '/Meshes/SM_GerPit_Howitzer', 'type': 'BTX_GerSFH18Pit', 'radius': 620.0, 'height': 110.0,
              'smoke_radius': 520.0},
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


def build(gun):
    pit = PITS[gun]
    bp = f'{ROOT}/Ger_BXP_GerPit_{gun}'
    turret = f'{ROOT}/Turrets/RTS_BP_EmbTurret_GerPit_{gun}'
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{bp}'):
    eal.delete_asset('{bp}')
print('DUP', eal.duplicate_asset('{BASE}', '{bp}'))
eal.save_asset('{bp}', only_if_is_dirty=False)
""", 'GerPit duplicate bxp')
    mesh_name = pit['mesh'].split('/')[-1]
    node = init_node(bp)
    for pin, value in (('NewBuildingMesh', f"{pit['mesh']}.{mesh_name}"), ('NewConstructionMesh', f"{pit['mesh']}.{mesh_name}"),
                       ('NewBuildingExpansionType', pit['type']), ('NewSmokeRadius', f"{pit['smoke_radius']:.1f}"),
                       ('NewAmountSmokes', '5')):
        act('set_pin_default', blueprint_path=bp, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    # A placed pit never runs construction and shows this template mesh.
    act('set_static_mesh_component', blueprint_path=bp, component_name='BuildingMeshComponent', mesh_path=pit['mesh'])
    act('edit_component_property', blueprint_path=bp, component_name='Howitzer', property_name='ChildActorClass',
        value=f"{turret}.RTS_BP_EmbTurret_GerPit_{gun}_C")
    act('set_component_transform', blueprint_path=bp, component_name='Howitzer', location=[0.0, 0.0, DECK_HEIGHT])
    radius, height = pit['radius'], pit['height']
    decal_scale = 2.0 * radius / DECAL_UNITS
    edits = [
        ('bpSelectionArea', 'BoxExtent', f'(X={radius * 0.85:.1f},Y={radius * 0.85:.1f},Z={height * 0.6:.1f})'),
        ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={height * 0.6:.1f})'),
        ('bpSelectionArea', 'RelativeScale3D', '(X=1.0,Y=1.0,Z=1.0)'),
        ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
        ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={height + 230.0:.1f})'),
        ('NavModifier', 'FailsafeExtent', f'(X={radius:.1f},Y={radius:.1f},Z={height:.1f})'),
        ('RTS', 'OwningPlayer', '1'),
    ]
    for component, prop, value in edits:
        act('edit_component_property', blueprint_path=bp, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=bp)
    print(gun, 'bxp compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=bp)


if __name__ == '__main__':
    for gun_name in (sys.argv[1].split(',') if len(sys.argv) > 1 else list(PITS)):
        build(gun_name)
