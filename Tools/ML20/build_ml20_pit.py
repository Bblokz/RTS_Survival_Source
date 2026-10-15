"""Builds the Soviet ML-20 gun pit through the editor bridge: the B-4 pit with the ML-20 and its smaller shells.

Copies of the B-4 pit's assets (the pit gun mesh SK_ML20Pit_Gun carries the B-4's crane and bone names, so the
B-4's anim blueprint, crew component and death graph work unchanged):
  Pit/Anims/Montage_ML20Pit_Reload / _Fire, Pit/Crew/Montage_ML20PitCrew<N>_Reload   DefaultSlot
  Pit/AnimBp_ML20Pit_Gun    AnimBp_B4Pit_Gun retargeted to SK_TW_ML20_Skeleton (yaw on B_Root, pitch on B_Mantlet,
                            faded by B4_LoadingElevation)
  Pit/RTS_BP_EmbTurret_ML20Pit   the B-4 pit turret: SK_ML20Pit_Gun, weapon ML20_152MM_Pit, the ML-20 drills, the
                            152mm shell (SM_ML20Pit_Shell) on the crane hook and in the rack, the rack moved between
                            the trails, turret height at the ML-20 bore
  Pit/Rus_BXP_ML20Pit       the B-4 pit expansion: type BTX_RusML20Pit, the turret above, leaves the B-4 pit
                            remains and the ML-20 wreck
  RTS_BP_RTSAsyncSpawner    BuildingExpansionMap / BxpPreviewMeshMap entries for BTX_RusML20Pit
  RTS_BP_EnemyController    optional (step 'enemy' only): BTX_RusML20Pit in SatteliteBuildingsForBase. Not part of
                            'all', because the B-4 pit is not in that list either.

Run: python build_ml20_pit.py [all|montages|animbp|turret|bxp|spawner|enemy]
     (with the scratchpad uecp.py importable and the bridge on 127.0.0.1:9855)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM'
B4 = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM'
PIT = ROOT + '/Pit'
SKELETON = ROOT + '/Mesh/SK_TW_ML20_Skeleton'
ANIM_BP = PIT + '/AnimBp_ML20Pit_Gun'
TURRET = PIT + '/RTS_BP_EmbTurret_ML20Pit'
BXP = PIT + '/Rus_BXP_ML20Pit'
GUN_MESH = PIT + '/Meshes/SK_ML20Pit_Gun'
SHELL_MESH = PIT + '/Meshes/SM_ML20Pit_Shell'
WRECK = ROOT + '/DestroyedTW_ML20'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
ENEMY_CONTROLLER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_EnemyController'
EXPANSION_TYPE = 'BTX_RUS_ML20_PIT'
CREW_COUNT = 4
GUN_MONTAGES = {'Montage_ML20Pit_Reload': 'Anim_ML20Pit_Reload', 'Montage_ML20Pit_Fire': 'Anim_ML20Pit_Fire'}
# The 152mm shell is the B-4 cartridge at 0.75: its pivot-to-centre offset shrinks with it (B-4: 10.6 cm).
SHELL_SCALE = 0.75
SHELL_PIVOT_OFFSET = 10.6 * SHELL_SCALE
RACK_PIVOT_OFFSET = 9.5 * SHELL_SCALE
# Unreal pit frame (y = -Blender y): the rack between the trails (ml20_pit_layout.RACK_ORIGIN), three shells on its
# cradles (top 65 cm): two side by side, one on top.
RACK_ORIGIN = (-470.0, -141.0, 0.0)
SHELL_RADIUS = 7.65
RACK_CRADLE_TOP = 65.0
RACK_SHELLS = {'RackShell1': (RACK_ORIGIN[1] + 2.0 * SHELL_RADIUS, RACK_CRADLE_TOP + SHELL_RADIUS),
               'RackShell2': (RACK_ORIGIN[1] - 2.0 * SHELL_RADIUS, RACK_CRADLE_TOP + SHELL_RADIUS),
               'RackShell3': (RACK_ORIGIN[1], RACK_CRADLE_TOP + SHELL_RADIUS * 2.73)}
# Turret: the ML-20 traverses faster than the B-4 on its turntable; same elevation as its team weapon.
ROTATION_SPEED = 9.0
MAX_PITCH = 60.0
BORE_HEIGHT = 167.0
LAUNCH_SCALE = 1.15


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', text[:800])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    print(text[:700])
    if '"ok":false' in text.replace(' ', '') or 'Traceback' in text:
        sys.exit(1)
    return text


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint asks for confirmation, which a windowless editor
    # cannot show. Every later step sets its values again.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", 'ML20 pit duplicate ' + target.split('/')[-1])


def graph_nodes(blueprint, graph='EventGraph'):
    result = call_tool('call_action', {'action': 'get_graph_snapshot', 'params': {
        'blueprint_path': blueprint, 'graph_name': graph, 'view': 'compact'}})
    return json.loads(result['result']['content'][0]['text'], strict=False)['nodes']


def one_node(nodes, predicate, label):
    matches = [n['id'] for n in nodes if predicate(n)]
    if len(matches) != 1:
        print('FAILED node lookup', label, matches)
        sys.exit(1)
    return matches[0]


def detail(node, key):
    return node.get('detail', {}).get(key)


def create_montage(source, name, folder):
    path = f'{folder}/{name}'
    run_python(f"""
import unreal
if unreal.EditorAssetLibrary.does_asset_exist('{path}'):
    unreal.EditorAssetLibrary.delete_asset('{path}')
""", 'ML20 pit clear montage ' + name)
    act('create_montage', source_animation=source, name=name, save_path=folder)
    act('save_asset', asset_path=path)
    print('montage', name, act('get_montage_summary', montage_path=path).get('play_length'))


def build_montages():
    for name, anim in GUN_MONTAGES.items():
        create_montage(f'{PIT}/Anims/{anim}', name, PIT + '/Anims')
    for index in range(1, CREW_COUNT + 1):
        create_montage(f'{PIT}/Crew/Anim_ML20PitCrew{index}_Reload', f'Montage_ML20PitCrew{index}_Reload', PIT + '/Crew')


def build_anim_blueprint():
    duplicate(B4 + '/Pit/AnimBp_B4Pit_Gun', ANIM_BP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{ANIM_BP}')
bp.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
unreal.EditorAssetLibrary.save_asset('{ANIM_BP}', only_if_is_dirty=False)
print('TARGET', bp.get_editor_property('target_skeleton').get_path_name())
""", 'ML20 pit anim blueprint skeleton')
    for node in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']:
        asset = node.get('current_asset', '')
        if node['node_class'] == 'AnimGraphNode_SequencePlayer' and 'Anim_B4Pit_' in asset:
            twin = asset.split('/')[-1].split('.')[0].replace('Anim_B4Pit_', 'Anim_ML20Pit_')
            act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'],
                property_name='Sequence', property_value=f'{PIT}/Anims/{twin}')
        elif node['node_class'] == 'AnimGraphNode_SequencePlayer' and 'Anim_B4_' in asset:
            twin = asset.split('/')[-1].split('.')[0].replace('Anim_B4_', 'Anim_ML20_')
            act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'],
                property_name='Sequence', property_value=f'{ROOT}/PackDeploy/{twin}')
    nodes = act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']
    print('assets in graph', sorted({n.get('current_asset', '').split('/')[-1] for n in nodes if n.get('current_asset')}))
    compiled = act('compile_blueprint', blueprint_path=ANIM_BP)
    print('AnimBp_ML20Pit_Gun compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=ANIM_BP)


def ml20_reload_settings(b4_settings):
    """The B-4 crew component's drill settings with every montage and idle swapped for the ML-20's."""
    swaps = (('B4_203MM/Pit/Anims/Montage_B4Pit_', 'ML20_152MM/Pit/Anims/Montage_ML20Pit_'),
             ('Montage_B4Pit_', 'Montage_ML20Pit_'),
             ('B4_203MM/Pit/Crew/Anim_B4PitCrew', 'ML20_152MM/Pit/Crew/Anim_ML20PitCrew'),
             ('Anim_B4PitCrew', 'Anim_ML20PitCrew'),
             ('B4_203MM/Pit/Crew/Montage_B4PitCrew', 'ML20_152MM/Pit/Crew/Montage_ML20PitCrew'),
             ('Montage_B4PitCrew', 'Montage_ML20PitCrew'))
    settings = b4_settings
    for old, new in swaps:
        settings = settings.replace(old, new)
    if 'B4' in settings:
        print('FAILED reload settings still name the B-4:', settings[:600])
        sys.exit(1)
    return settings


def build_turret():
    duplicate(B4 + '/Pit/RTS_BP_EmbTurret_B4Pit', TURRET)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN_MESH)
    act('edit_component_property', blueprint_path=TURRET, component_name='Turret', property_name='AnimClass',
        value=f'{ANIM_BP}.AnimBp_ML20Pit_Gun_C')
    act('set_static_mesh_component', blueprint_path=TURRET, component_name='Shell', mesh_path=SHELL_MESH)
    act('set_component_transform', blueprint_path=TURRET, component_name='Shell', location=[SHELL_PIVOT_OFFSET, 0.0, 0.0])
    act('set_component_transform', blueprint_path=TURRET, component_name='ShellRack', location=list(RACK_ORIGIN))
    for name, (y, z) in RACK_SHELLS.items():
        act('set_static_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SHELL_MESH)
        act('set_component_transform', blueprint_path=TURRET, component_name=name,
            location=[RACK_ORIGIN[0] + RACK_PIVOT_OFFSET, y, z])
    b4_settings = act('get_component_property', blueprint_path=TURRET, component_name='PitCrew',
                      property_name='ReloadSettings')['value']
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings',
        value=ml20_reload_settings(b4_settings))
    print('reload settings', act('get_component_property', blueprint_path=TURRET, component_name='PitCrew',
                                 property_name='ReloadSettings')['value'][:400])
    nodes = graph_nodes(TURRET)
    init_child = one_node(nodes, lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child')
    init_embedded = one_node(nodes, lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded')
    weapon = one_node(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'InitWeaponStateArchProjectile', 'weapon')
    vfx = one_node(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'vfx')
    for node, pin, value in ((init_child, 'NewRotationSpeed', f'{ROTATION_SPEED:.1f}'),
                             (init_child, 'NewMaxPitch', f'{MAX_PITCH:.1f}'),
                             (init_embedded, 'TurretHeight', f'{BORE_HEIGHT:.1f}'),
                             (weapon, 'WeaponName', 'ML20_152MM_Pit'),
                             (vfx, 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}')):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('ML-20 pit turret compile', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=TURRET)
    for component, prop in (('Turret', 'SkeletalMeshAsset'), ('Turret', 'AnimClass'), ('Shell', 'StaticMesh'),
                            ('RackShell3', 'RelativeLocation'), ('ShellRack', 'RelativeLocation')):
        print('check', component, prop, act('get_component_property', blueprint_path=TURRET, component_name=component,
                                            property_name=prop).get('value'))


def build_bxp():
    duplicate(B4 + '/Pit/Rus_BXP_B4HowitzerPit', BXP)
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{TURRET}.RTS_BP_EmbTurret_ML20Pit_C')
    nodes = graph_nodes(BXP, 'BP_OnUnitDies')
    gun_wreck = one_node(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'DestroySpawnActor' and any(
        'DestroyedTW_B4' in str(p.get('default')) for p in n.get('pins', [])), 'gun wreck')
    act('set_pin_default', blueprint_path=BXP, graph_name='BP_OnUnitDies', node_id=gun_wreck, pin_name='ActorToSpawn',
        value=f'{WRECK}.DestroyedTW_ML20_C')
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('ML-20 pit bxp compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=BXP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{BXP}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('PitExpansionType', unreal.BuildingExpansionType.{EXPANSION_TYPE})
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('TYPE', unreal.get_default_object(bp.generated_class()).get_editor_property('PitExpansionType'))
""", 'ML20 pit expansion type')


def register_spawner():
    run_python(f"""
import unreal
bp = unreal.load_asset('{SPAWNER}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
expansion = unreal.BuildingExpansionType.{EXPANSION_TYPE}
classes = cdo.get_editor_property('BuildingExpansionMap')
classes[expansion] = unreal.load_asset('{BXP}').generated_class()
cdo.set_editor_property('BuildingExpansionMap', classes)
previews = cdo.get_editor_property('BxpPreviewMeshMap')
previews[expansion] = previews[unreal.BuildingExpansionType.BTX_RUS_B4_HOWITZER_PIT]
cdo.set_editor_property('BxpPreviewMeshMap', previews)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
print('SPAWNER bxp', len(classes), cdo.get_editor_property('BuildingExpansionMap')[expansion],
      cdo.get_editor_property('BxpPreviewMeshMap')[expansion].get_name())
""", 'ML20 pit spawner entries')


def register_enemy_satellite():
    run_python(f"""
import unreal
bp = unreal.load_asset('{ENEMY_CONTROLLER}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
expansion = unreal.BuildingExpansionType.{EXPANSION_TYPE}
satellites = list(cdo.get_editor_property('SatteliteBuildingsForBase'))
if expansion not in satellites:
    satellites.append(expansion)
cdo.set_editor_property('SatteliteBuildingsForBase', satellites)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
print('ENEMY satellites', len(satellites), [str(s).split('.')[-1] for s in satellites][-4:])
""", 'ML20 pit enemy satellite')


STEPS = {'montages': build_montages, 'animbp': build_anim_blueprint, 'turret': build_turret, 'bxp': build_bxp,
         'spawner': register_spawner}
OPTIONAL_STEPS = {'enemy': register_enemy_satellite}

if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    for step_name, step in STEPS.items():
        if which in ('all', step_name):
            step()
    if which in OPTIONAL_STEPS:
        OPTIONAL_STEPS[which]()
