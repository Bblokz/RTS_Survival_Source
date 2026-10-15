"""Builds the Soviet Ampulomet pit building expansion through the editor bridge (UECP direct calls, see uecp.py).

The pit is a copy of the DShK pit expansion (Soviet owned, engages at once when placed in a level, gun hidden while
it is built, collapses without a wreck) with:
  * SM_AmpulometPit as building and construction mesh,
  * the Ampulomet pit turret as the child actor on the pit floor,
  * selection, decal, progress bar and nav modifier sized for this pit,
  * type BTX_RusAmpulometPit (step 'type'; needs an editor built with that enum value).

Run: python build_ampulometpit_bxp.py [build,type]   (scratchpad uecp.py importable; UECP_PORT picks the editor)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusAmpulometPit'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Rus_BXP_DShKPit'
BXP = ROOT + '/Rus_BXP_AmpulometPit'
PIT_MESH = ROOT + '/Meshes/SM_AmpulometPit'
TURRET_CLASS = ROOT + '/Turrets/RTS_BP_EmbTurret_RusAmpulometPit.RTS_BP_EmbTurret_RusAmpulometPit_C'
# Python spells enum values in upper snake case (BTX_RusAmpulometPit).
EXPANSION_TYPE = 'BTX_RUS_AMPULOMET_PIT'
# Outer edge of the sandbags; the earth berm runs out another 1.4 m.
PIT_RADIUS = 255.0
PIT_HEIGHT = 80.0
SMOKE_RADIUS = 270.0
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
""", 'Ampulomet pit duplicate bxp')


def init_node():
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    matches = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp']
    if len(matches) != 1:
        print('FAILED BP_InitBuildingExp lookup', matches)
        sys.exit(1)
    return matches[0]


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
""", 'Ampulomet pit expansion type')


def build():
    duplicate()
    node = init_node()
    mesh_ref = f'{PIT_MESH}.{PIT_MESH.split("/")[-1]}'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'), ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=PIT_MESH)
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=TURRET_CLASS)
    decal_scale = 2.0 * PIT_RADIUS / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT + 220.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('Ampulomet pit bxp compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=BXP)


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['build', 'type']
    if 'build' in steps:
        build()
    if 'type' in steps:
        set_type()
