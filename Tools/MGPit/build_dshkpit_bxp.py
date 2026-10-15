"""Builds the Soviet DShK pit building expansion through the editor bridge (UECP direct calls, see uecp.py).

The pit is a copy of the enemy B-4 pit expansion (Soviet owned, engages at once when placed in a level, gun hidden
while it is built) with:
  * SM_DShKPit as building and construction mesh, type BTX_RusDShKPit,
  * the DShK pit turret as the child actor on the pit floor,
  * no B-4 sandbag revetment and no B-4 wrecks: BP_OnUnitDies keeps DisableAllWeapons and the collapse, but its
    wreck list (the B-4 pit and gun wrecks) is removed, so DestroyAndSpawnActors spawns nothing,
  * selection, decal, progress bar and nav modifier sized for the small pit.

Run: python build_dshkpit_bxp.py
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit'
BASE = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit'
BXP = ROOT + '/Rus_BXP_DShKPit'
PIT_MESH = ROOT + '/Meshes/SM_DShKPit'
TURRET_CLASS = ROOT + '/Turrets/RTS_BP_EmbTurret_RusDShKPit.RTS_BP_EmbTurret_RusDShKPit_C'
# Python spells enum values in upper snake case (BTX_RusDShKPit).
EXPANSION_TYPE = 'BTX_RUS_D_SH_K_PIT'
# Outer edge of the sandbags; the earth berm runs out another 1.5 m.
PIT_RADIUS = 240.0
PIT_HEIGHT = 90.0
SMOKE_RADIUS = 260.0
SMOKES = 3
# The base expansion's decal is 32 x 32 units before its component scale.
DECAL_UNITS = 32.0


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
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    print(text[:400])
    return text


def duplicate():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{BXP}'):
    eal.delete_asset('{BXP}')
print('DUP', eal.duplicate_asset('{BASE}', '{BXP}'))
eal.save_asset('{BXP}', only_if_is_dirty=False)
""", 'DShK pit duplicate bxp')


def init_node():
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    matches = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp']
    if len(matches) != 1:
        print('FAILED BP_InitBuildingExp lookup', matches)
        sys.exit(1)
    return matches[0]


# Nodes of the B-4 death function that only build the wreck list (the exec chain and the collapse FX stay).
WRECK_NODE_PREFIXES = ('b4d_arra', 'b4d_pitw', 'b4d_gunw', 'b4d_pitx', 'b4d_gunx', 'b4d_gunr', 'b4d_addy', 'b4d_brea',
                       'b4d_mesh', 'auto_con')


def remove_b4_wrecks():
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='BP_OnUnitDies', view='compact')['nodes']
    doomed = [n['id'] for n in nodes if n['id'].startswith(WRECK_NODE_PREFIXES)]
    act('delete_nodes', blueprint_path=BXP, graph_name='BP_OnUnitDies', node_ids=doomed)
    print('removed wreck nodes', len(doomed))


def set_type():
    run_python(f"""
import unreal
bp = unreal.load_asset('{BXP}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('PitExpansionType', unreal.BuildingExpansionType.{EXPANSION_TYPE})
# A class default set from Python only survives through save_packages; checked again after a compile.
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('TYPE', unreal.get_default_object(bp.generated_class()).get_editor_property('PitExpansionType'))
""", 'DShK pit expansion type')


def build():
    duplicate()
    act('delete_component', blueprint_path=BXP, component_name='Sandbags')
    remove_b4_wrecks()
    node = init_node()
    mesh_ref = f'{PIT_MESH}.{PIT_MESH.split("/")[-1]}'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'), ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=PIT_MESH)
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=TURRET_CLASS)
    act('set_component_transform', blueprint_path=BXP, component_name='Howitzer', location=[0.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0])
    decal_scale = 2.0 * PIT_RADIUS / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeScale3D', '(X=1.0,Y=1.0,Z=1.0)'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT + 220.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('DShK pit bxp compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=BXP)
    set_type()


if __name__ == '__main__':
    build()
