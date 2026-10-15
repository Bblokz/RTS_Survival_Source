"""Builds the ML-20 team weapon, its anim blueprint, squad, wreck and spawner entry through the editor bridge.

Copies of the B-4's assets (the ML-20 rig uses the B-4's bone names, so the B-4 graphs work on its skeleton):
  SM_ML20_Destroyed + MI_ML20_*_Burnt   the wreck mesh (ml20_destroyed.py) with burnt copies of the gun materials
  Montage_ML20_Fire / _DeployToPacked / _WheelsFWD   'Gun' slot for the recoil, DefaultSlot for the others
  AnimBp_TW_ML20     AnimBp_TW_B4 retargeted to SK_TW_ML20_Skeleton with AO_ML20 and the ML-20 montages
  DestroyedTW_ML20   the scavengeable wreck
  BP_TW_ML20         SK_TW_ML20 with AnimBp_TW_ML20, weapon ML20_152MM_TW, crew spots between the split trails,
                     yaw arc +-15 and pitch -2..60 (the aim offset's ranges), turret height at the ML-20's bore
  Rus_SquadTW_ML20   the B-4's Soviet crew with TeamWeaponClass = BP_TW_ML20
  RTS_BP_RTSAsyncSpawner   SetupRusSquads row Squad_Rus_ML20_152mm -> Rus_SquadTW_ML20

Run: python build_ml20_teamweapon.py [all|wreck_assets|montages|animbp|wreck|weapon|squad|spawner]
     (with the scratchpad uecp.py importable and the bridge on 127.0.0.1:9855)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM'
B4 = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM'
SKELETON = ROOT + '/Mesh/SK_TW_ML20_Skeleton'
ANIM_BP = ROOT + '/AnimBp_TW_ML20'
AO = ROOT + '/WeaponAnims/AO_ML20'
WRECK = ROOT + '/DestroyedTW_ML20'
TEAM_WEAPON = ROOT + '/BP_TW_ML20'
SQUAD = ROOT + '/Rus_SquadTW_ML20'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/ML20_152mm'
MATERIAL_SETS = ['Gun', 'Carriage', 'Wheels', 'Tyres']
MONTAGES = {  # name: (source animation, folder, slot)
    'Montage_ML20_Fire': (ROOT + '/WeaponAnims/Anim_ML20_Fire', 'WeaponAnims', 'Gun'),
    'Montage_ML20_DeployToPacked': (ROOT + '/PackDeploy/Anim_ML20_DeployedToPacked', 'PackDeploy', 'DefaultSlot'),
    'Montage_ML20_WheelsFWD': (ROOT + '/WheelAnims/Anim_ML20_WheelsFWD', 'WheelAnims', 'DefaultSlot'),
}
# Crew spots (Unreal gun frame): the loaders stand between the split trails behind the breech (x -104), the gunner
# at the sight beside the left wheel, as on the B-4 relative to its breech.
CREW_SPOTS = {'Loader1': ((-227.0, 70.0, 0.0), -60.0), 'Loader2': ((-227.0, -70.0, 0.0), 60.0),
              'Gunner': ((-30.0, -170.0, 0.0), 90.0)}
YAW_ARC_DEGREES = 15.0
MIN_PITCH_DEGREES = -2.0
MAX_PITCH_DEGREES = 60.0
BORE_HEIGHT = 167.0
LAUNCH_SCALE = 0.85
CASING_SCALE = 1.6
# Burnt copies of the gun materials, as MI_B4_*_Burnt.
BURNT_COLOUR = (0.22, 0.2, 0.19, 0.0)
BURNT_ROUGHNESS_BLEND = 0.6


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
    print(text[:900])
    if '"ok": false' in text or 'Traceback' in text:
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
""", 'ML20 duplicate ' + target.split('/')[-1])


def build_wreck_assets():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.set_editor_property('combine_meshes', True)
options.static_mesh_import_data.set_editor_property('auto_generate_collision', True)
options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
task = unreal.AssetImportTask()
task.filename = '{EXPORT_DIR}/SM_ML20_Destroyed.fbx'
task.destination_path = '{ROOT}/Mesh'
task.destination_name = 'SM_ML20_Destroyed'
task.automated = True
task.replace_existing = True
task.save = True
task.options = options
tools.import_asset_tasks([task])
burnt = {{}}
for material_set in {MATERIAL_SETS}:
    name = 'MI_ML20_' + material_set + '_Burnt'
    path = '{ROOT}/Materials/' + name
    instance = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, '{ROOT}/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property('parent', unreal.load_asset('{ROOT}/Materials/MI_ML20_' + material_set))
    mel.set_material_instance_vector_parameter_value(instance, 'ColorCorrection', unreal.LinearColor{BURNT_COLOUR})
    mel.set_material_instance_scalar_parameter_value(instance, 'Roughness Blend', {BURNT_ROUGHNESS_BLEND})
    mel.update_material_instance(instance)
    eal.save_loaded_asset(instance)
    burnt['MI_ML20_' + material_set] = instance
mesh = unreal.load_asset('{ROOT}/Mesh/SM_ML20_Destroyed')
for index, entry in enumerate(mesh.static_materials):
    slot = str(entry.material_slot_name)
    if slot in burnt:
        mesh.set_material(index, burnt[slot])
eal.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
print('WRECK slots', [(str(m.material_slot_name), m.material_interface.get_name()) for m in mesh.static_materials])
""", 'ML20 wreck mesh and burnt materials')


def build_montages():
    act('create_anim_slot', skeleton_path=SKELETON, slot_name='Gun')
    act('save_asset', asset_path=SKELETON)
    for name, (source, folder, slot) in MONTAGES.items():
        path = f'{ROOT}/{folder}/{name}'
        run_python(f"""
import unreal
if unreal.EditorAssetLibrary.does_asset_exist('{path}'):
    unreal.EditorAssetLibrary.delete_asset('{path}')
""", 'ML20 clear montage ' + name)
        act('create_montage', source_animation=source, name=name, save_path=f'{ROOT}/{folder}')
        if slot != 'DefaultSlot':
            act('set_montage_slot_name', montage_path=path, new_slot_name=slot)
        act('save_asset', asset_path=path)
        summary = act('get_montage_summary', montage_path=path)
        print('montage', name, summary.get('play_length'), summary.get('slot_tracks'))


def build_anim_blueprint():
    duplicate(B4 + '/AnimBp_TW_B4', ANIM_BP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{ANIM_BP}')
bp.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
unreal.EditorAssetLibrary.save_asset('{ANIM_BP}', only_if_is_dirty=False)
print('TARGET', bp.get_editor_property('target_skeleton').get_path_name())
""", 'ML20 anim blueprint skeleton')
    for node in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']:
        if node['node_class'] == 'AnimGraphNode_RotationOffsetBlendSpace':
            act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'],
                property_name='BlendSpace', property_value=AO)
    act('compile_blueprint', blueprint_path=ANIM_BP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{ANIM_BP}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
sequences = cdo.get_editor_property('Sequences')
sequences.set_editor_property('packed_sequence', unreal.load_asset('{ROOT}/PackDeploy/Anim_ML20_Packed'))
sequences.set_editor_property('deployed_sequence', unreal.load_asset('{ROOT}/PackDeploy/Anim_ML20_Deployed'))
cdo.set_editor_property('Sequences', sequences)
gun = cdo.get_editor_property('GunMontages')
fire = unreal.load_asset('{ROOT}/WeaponAnims/Montage_ML20_Fire')
gun.set_editor_property('fire_montage', fire)
gun.set_editor_property('burst_fire_montage', fire)
cdo.set_editor_property('GunMontages', gun)
moves = cdo.get_editor_property('DeployAndMvtMontages')
moves.set_editor_property('deploy_to_packed', unreal.load_asset('{ROOT}/PackDeploy/Montage_ML20_DeployToPacked'))
moves.set_editor_property('move_fwd_montage', unreal.load_asset('{ROOT}/WheelAnims/Montage_ML20_WheelsFWD'))
cdo.set_editor_property('DeployAndMvtMontages', moves)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
print('CDO', cdo.get_editor_property('Sequences'), cdo.get_editor_property('GunMontages'), cdo.get_editor_property('DeployAndMvtMontages'))
""", 'ML20 anim blueprint defaults')
    nodes = act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']
    print('assets in graph', sorted({n.get('current_asset', '').split('/')[-1] for n in nodes if n.get('current_asset')}))
    compiled = act('compile_blueprint', blueprint_path=ANIM_BP)
    print('AnimBp_TW_ML20 compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=ANIM_BP)


def build_wreck():
    duplicate(B4 + '/DestroyedTW_B4', WRECK)
    act('set_static_mesh_component', blueprint_path=WRECK, component_name='Weapon', mesh_path=ROOT + '/Mesh/SM_ML20_Destroyed')
    # The inherited Weapon component's override carries the mesh; the B-4 wreck has no material overrides to clear.
    mesh = act('get_component_property', blueprint_path=WRECK, component_name='Weapon', property_name='StaticMesh')
    overrides = act('get_component_property', blueprint_path=WRECK, component_name='Weapon', property_name='OverrideMaterials')
    print('wreck mesh', mesh.get('value'), 'overrides', repr(overrides.get('value')))
    compiled = act('compile_blueprint', blueprint_path=WRECK)
    print('wreck compile errors', compiled.get('error_count'))
    act('save_asset', asset_path=WRECK)


def struct_node(nodes, name):
    matches = [n['id'] for n in nodes if n.get('op') == 'make_struct' and n.get('detail', {}).get('struct') == name]
    if len(matches) != 1:
        print('FAILED node lookup', name, matches)
        sys.exit(1)
    return matches[0]


def build_team_weapon():
    duplicate(B4 + '/BP_TW_B4', TEAM_WEAPON)
    # The bridge's mesh + anim BP check refuses some valid pairs, so the anim class is set as a plain property.
    act('set_skeletal_mesh_component', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh',
        mesh_path=ROOT + '/Mesh/SK_TW_ML20')
    act('edit_component_property', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh', property_name='AnimClass',
        value=f'{ANIM_BP}.AnimBp_TW_ML20_C')
    for name, (location, yaw) in CREW_SPOTS.items():
        act('set_component_transform', blueprint_path=TEAM_WEAPON, component_name=name, location=list(location),
            rotation=[0.0, yaw, 0.0])
    act('set_blueprint_variable_default', blueprint_path=TEAM_WEAPON, variable_name='Weapon Name',
        default_value='ML20_152MM_TW')
    result = call_tool('call_action', {'action': 'get_graph_snapshot', 'params': {
        'blueprint_path': TEAM_WEAPON, 'graph_name': 'EventGraph', 'view': 'compact'}})
    nodes = json.loads(result['result']['content'][0]['text'], strict=False)['nodes']
    vfx = struct_node(nodes, 'WeaponVFX')
    shell = struct_node(nodes, 'WeaponShellCase')
    wreck = struct_node(nodes, 'DestroySpawnActor')
    for node, pin, value in ((vfx, 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}'),
                             (shell, 'MeshSizeMlt', f'{CASING_SCALE}'),
                             (wreck, 'ActorToSpawn', f'{WRECK}.DestroyedTW_ML20_C')):
        act('set_pin_default', blueprint_path=TEAM_WEAPON, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TEAM_WEAPON)
    print('team weapon compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TEAM_WEAPON)
    run_python(f"""
import unreal
bp = unreal.load_asset('{TEAM_WEAPON}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
config = cdo.get_editor_property('M_TeamWeaponConfig')
yaw_arc = config.get_editor_property('m_yaw_arc')
yaw_arc.set_editor_property('m_min_yaw', {-YAW_ARC_DEGREES})
yaw_arc.set_editor_property('m_max_yaw', {YAW_ARC_DEGREES})
config.set_editor_property('m_yaw_arc', yaw_arc)
pitch = config.get_editor_property('m_pitch_settings')
pitch.set_editor_property('m_min_pitch', {MIN_PITCH_DEGREES})
pitch.set_editor_property('m_max_pitch', {MAX_PITCH_DEGREES})
config.set_editor_property('m_pitch_settings', pitch)
embedded = config.get_editor_property('m_embedded_settings')
embedded.set_editor_property('turret_height', {BORE_HEIGHT})
config.set_editor_property('m_embedded_settings', embedded)
cdo.set_editor_property('M_TeamWeaponConfig', config)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
config = unreal.get_default_object(bp.generated_class()).get_editor_property('M_TeamWeaponConfig')
print('TW config', config.get_editor_property('m_yaw_arc'), config.get_editor_property('m_pitch_settings'),
      config.get_editor_property('m_embedded_settings'))
""", 'ML20 team weapon config')


def build_squad():
    duplicate(B4 + '/Rus_SquadTW_B4', SQUAD)
    run_python(f"""
import unreal
bp = unreal.load_asset('{SQUAD}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('TeamWeaponClass', unreal.load_asset('{TEAM_WEAPON}').generated_class())
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('SQUAD team weapon', unreal.get_default_object(bp.generated_class()).get_editor_property('TeamWeaponClass'))
""", 'ML20 squad team weapon class')


def register_spawner():
    run_python(f"""
import unreal
bp = unreal.load_asset('{SPAWNER}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
subtype = unreal.SquadSubtype.SQUAD_RUS_ML20_152MM
rows = [r for r in cdo.get_editor_property('SetupRusSquads') if r.get_editor_property('squad_subtype') != subtype]
row = unreal.SquadTrainingOptionSetup()
row.set_editor_property('squad_subtype', subtype)
row.set_editor_property('unit_class', unreal.load_asset('{SQUAD}').generated_class())
rows.append(row)
cdo.set_editor_property('SetupRusSquads', rows)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
print('SPAWNER rus squads', len(cdo.get_editor_property('SetupRusSquads')))
""", 'ML20 spawner row')


STEPS = {'wreck_assets': build_wreck_assets, 'montages': build_montages, 'animbp': build_anim_blueprint,
         'wreck': build_wreck, 'weapon': build_team_weapon, 'squad': build_squad, 'spawner': register_spawner}

if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    for step_name, step in STEPS.items():
        if which in ('all', step_name):
            step()
