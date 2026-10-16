"""Builds the Soviet 53-K gun pit through the editor bridge (UECP direct calls, see uecp.py).

  Meshes/SM_53KPit              the sandbag ring (at53k_pit_build.py), MI_Rus_SandBag_01, complex collision
  PitOrigin socket              on the 53-K's root bone (37mmFlak), at the gun frame origin; the crew ride on it
  Anims/AnimBp_53KPit           UGunPitAnimInstance: deployed pose, DefaultSlot, B_Mantlet pitch, whole-gun yaw on the
                                root bone (the gun turns a full circle with its trails and crew)
  Anims/Montage_53KPit_Fire     the recoil in DefaultSlot
  Turrets/RTS_BP_EmbTurret_Rus53KPit
                                a copy of the German Pak 40 pit turret: the same loading drill (the PaK loader's
                                own animation, the shell handler's handover and the rounds changing hands), because
                                the 53-K's breech sits where the Pak 40's does (x -72; 20 cm lower). Soviet crew,
                                the 53-K, its weapon M53K_45MM_TW from Launch53K, 45 mm rounds (the 75 mm round at
                                0.7 scale)
  Rus_BXP_53KPit                a copy of the DShK pit expansion (enemy owned, engages when placed), type
                                BTX_Rus53KPit

Run: python build_53k_pit.py [mesh,socket,animbp,turret,crate,bxp]
  crate: only re-places the ReadyCrate on the existing turret (the turret step copies the Pak 40 pit's crate)

The game does not render Nanite: after (re)importing a pit or fieldwork mesh, cut it with
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py and run Tools/GunPits/optimize_pit_meshes.py heavy, which
imports the cut mesh and turns Nanite off (see Docs/Units/GunPitMeshes.md).
"""
import json
import os
import sys
from uecp import call_tool

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GunPits'))
import pit_crate

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit'
GUN = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/53K_45MM'
SKELETON = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM/Mesh/SK_TW_Zis3_Skeleton'
SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/45mmAT_53K/Exports'
PAK_PIT_TURRET = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Pak40'
DSHK_PIT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Rus_BXP_DShKPit'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_Rus53KPit'
BXP = ROOT + '/Rus_BXP_53KPit'
ANIM_BP = ROOT + '/Anims/AnimBp_53KPit'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
SANDBAG_MATERIAL = '/Game/SandBagPack/Materials/MI_Rus_SandBag_01'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/45mmT26/S_FireWeapon_45mmT26.S_FireWeapon_45mmT26'
ROUND_SCALE = 0.7
# The Pak 40 pit's drill, whose bake holds the crate spot.
PAK_CARRY = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Crew/gerpit_pak_carry.json'
ROTATION_SPEED = 26.0
MAX_PITCH = 25.0
MIN_PITCH = -8.0
BORE_HEIGHT = 76.0
# Pit sizes for the expansion: outer edge of the bags and the wall height.
PIT_RADIUS = 570.0
PIT_HEIGHT = 70.0
SMOKE_RADIUS = 480.0
SMOKES = 4
DECAL_UNITS = 32.0
EXPANSION_TYPE = 'BTX_RUS53K_PIT'


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
    print(text[:500])
    return text


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint opens a confirmation a windowless editor cannot show.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", '53K pit duplicate ' + target.split('/')[-1])


def import_mesh():
    run_python(f"""
import unreal
task = unreal.AssetImportTask()
task.filename = '{SOURCE}/SM_53KPit.fbx'
task.destination_path = '{ROOT}/Meshes'
task.destination_name = 'SM_53KPit'
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.set_editor_property('combine_meshes', True)
options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
path = '{ROOT}/Meshes/SM_53KPit'
mesh = unreal.load_asset(path)
mesh.set_material(0, unreal.load_asset('{SANDBAG_MATERIAL}'))
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property('lod_group', 'LargeProp')
print('PIT', mesh.get_num_triangles(0), unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
""", '53K pit mesh import')


def add_pit_origin():
    """PitOrigin: the gun frame origin, relative to the root bone's reference transform."""
    text = run_python(f"""
import unreal
seq = unreal.load_asset('{GUN}/DeployPacked/Anim_53K_Deployed')
root = unreal.AnimationLibrary.get_bone_pose_for_time(seq, '37mmFlak', 0.0, False)
relative = unreal.Transform().multiply(root.inverse())
r = relative.rotation.rotator()
print('ORIGIN', relative.translation.x, relative.translation.y, relative.translation.z, r.pitch, r.yaw, r.roll)
""", '53K pit origin')
    values = [float(v) for v in text.split('ORIGIN ')[1].split('\\')[0].split()[:6]]
    act('add_skeleton_socket', skeleton_path=SKELETON, socket_name='PitOrigin53K', bone_name='37mmFlak')
    act('set_skeleton_socket_transform', skeleton_path=SKELETON, socket_name='PitOrigin53K',
        relative_location=values[:3], relative_rotation=values[3:6], relative_scale=[1, 1, 1])
    act('save_asset', asset_path=SKELETON)
    print('PitOrigin53K', values)


def modify_bone(abp, bone, x, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=0)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def build_anim_blueprint():
    run_python(f"""
import unreal
if not unreal.EditorAssetLibrary.does_asset_exist('{ANIM_BP}'):
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
    factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
    unreal.AssetToolsHelpers.get_asset_tools().create_asset('AnimBp_53KPit', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
""", '53K pit anim blueprint')
    abp = ANIM_BP
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] != 'AnimGraphNode_Root':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence', property_value=GUN + '/DeployPacked/Anim_53K_Deployed')
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='bLoopAnimation', property_value='true')
    slot = act('add_slot_node', anim_blueprint_path=abp, slot_name='DefaultSlot', position_x=-1150, position_y=0)['node_guid']
    to_cs = act('add_local_to_component_space', anim_blueprint_path=abp, position_x=-950, position_y=0)['node_guid']
    pitch = modify_bone(abp, 'B_Mantlet', -700, 'CradlePitchRotation', 'AimPitchAlpha')
    yaw = modify_bone(abp, '37mmFlak', -400, 'TurntableYawRotation')
    act('add_component_to_local_space', anim_blueprint_path=abp, position_x=-150, position_y=0)
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=seq, target_node_guid=slot, target_pin='Source')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=slot, target_node_guid=to_cs)
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=to_cs, target_node_guid=pitch, target_pin='ComponentPose')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=pitch, target_node_guid=yaw, target_pin='ComponentPose')
    to_ls = [n for n in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']
             if n['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=yaw, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=abp, node_name='Component To Local')
    compiled = act('compile_blueprint', blueprint_path=abp)
    print('AnimBp_53KPit compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)
    act('create_montage', source_animation=GUN + '/WeaponAnims/Anim_53K_Fire', name='Montage_53KPit_Fire', save_path=ROOT + '/Anims')
    act('save_asset', asset_path=ROOT + '/Anims/Montage_53KPit_Fire')


def build_turret():
    duplicate(PAK_PIT_TURRET, TURRET)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN + '/Mesh/SK_TW_53K')
    act('edit_component_property', blueprint_path=TURRET, component_name='Turret', property_name='AnimClass',
        value=f'{ANIM_BP}.AnimBp_53KPit_C')
    for name in ('Crew1', 'Crew2', 'Crew3', 'Crew4'):
        act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SOLDIER, anim_bp_path=CREW_ANIM_BP)
    for name in ('Round1', 'Round2'):
        act('set_component_transform', blueprint_path=TURRET, component_name=name, scale=[ROUND_SCALE] * 3)
    reload_settings = act('get_component_property', blueprint_path=TURRET, component_name='PitCrew',
                          property_name='ReloadSettings')['value']
    old_fire = reload_settings.split('GunFireMontage="')[1].split('"')[0]
    new_fire = f"/Script/Engine.AnimMontage'{ROOT}/Anims/Montage_53KPit_Fire.Montage_53KPit_Fire'"
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings',
        value=reload_settings.replace(old_fire, new_fire))
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='CrewAttachSocket',
        value='PitOrigin53K')
    # The 53-K's origin socket has its own name on the shared ZiS-3 skeleton; the ready crate follows it.
    act('edit_component_property', blueprint_path=TURRET, component_name='ReadyCrate', property_name='ComponentTags',
        value='("PitSocket:PitOrigin53K")')
    nodes = act('get_graph_snapshot', blueprint_path=TURRET, graph_name='EventGraph', view='compact')['nodes']

    def one(predicate, label):
        matches = [n['id'] for n in nodes if predicate(n)]
        if len(matches) != 1:
            print('FAILED node lookup', label, matches)
            sys.exit(1)
        return matches[0]

    def detail(n, key):
        return n.get('detail', {}).get(key)
    init_child = one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child')
    init_embedded = one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded')
    weapon = one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'InitWeaponStateProjectile', 'weapon')
    vfx = one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'vfx')
    for node, pin, value in ((init_child, 'NewRotationSpeed', f'{ROTATION_SPEED:.1f}'), (init_child, 'NewMaxPitch', f'{MAX_PITCH:.1f}'),
                             (init_child, 'NewMinPitch', f'{MIN_PITCH:.1f}'), (init_embedded, 'TurretHeight', f'{BORE_HEIGHT:.1f}'),
                             (weapon, 'WeaponName', 'M53K_45MM_TW'), (weapon, 'FireSocketName', 'Launch53K'),
                             (vfx, 'LaunchSound', LAUNCH_SOUND)):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('53K pit turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


def replace_crate():
    """Puts the ready crate under the round the handler picks up, without copying the Pak pit turret again."""
    with open(PAK_CARRY) as handle:
        crate = json.load(handle)['crate']
    location, rotation, scale = pit_crate.crate_transform(pit_crate.CRATE_MESH, crate)
    act('set_static_mesh_component', blueprint_path=TURRET, component_name='ReadyCrate', mesh_path=pit_crate.CRATE_MESH)
    act('set_component_transform', blueprint_path=TURRET, component_name='ReadyCrate', location=location, rotation=rotation,
        scale=[scale] * 3)
    print('ReadyCrate', location, rotation, scale)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('53K pit turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


def build_bxp():
    duplicate(DSHK_PIT, BXP)
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    init = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp'][0]
    mesh_ref = f'{ROOT}/Meshes/SM_53KPit.SM_53KPit'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'), ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=init, pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=ROOT + '/Meshes/SM_53KPit')
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{TURRET}.RTS_BP_EmbTurret_Rus53KPit_C')
    decal_scale = 2.0 * PIT_RADIUS / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT + 220.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('53K pit bxp compile', compiled.get('error_count'), compiled.get('errors'))
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
""", '53K pit expansion type')


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['mesh', 'socket', 'animbp', 'turret', 'bxp']
    for step in steps:
        {'mesh': import_mesh, 'socket': add_pit_origin, 'animbp': build_anim_blueprint, 'turret': build_turret,
         'crate': replace_crate, 'bxp': build_bxp}[step]()
