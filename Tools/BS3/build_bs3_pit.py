"""Builds the Soviet BS-3 gun pit through the editor bridge (UECP direct calls, see uecp.py).

  Crew/Anim_BS3Pit_Handler_*    the shell handler's drill and idle (bs3_crew.py), Montage_BS3Pit_Handler_Reload
  Meshes/SM_BS3Pit              the square sandbag wall open at the rear (bs3_pit_build.py), MI_Rus_SandBag_01,
                                complex collision
  PitOriginBS3 socket           on the shared ZiS-3 skeleton's root bone (37mmFlak), at the gun frame origin; the crew,
                                the ready crate and the rounds ride on it
  Anims/AnimBp_BS3Pit           UGunPitAnimInstance: deployed pose, DefaultSlot, B_Mantlet pitch, whole-gun yaw on the
                                root bone (the gun turns a full circle with its trails and crew)
  Anims/Montage_BS3Pit_Fire     the recoil in DefaultSlot
  Turrets/RTS_BP_EmbTurret_RusBS3Pit
                                a copy of the 53-K pit turret (the Pak 40 pit's drill on Soviet crew) with the BS-3, its
                                weapon BS3_100MM_TW from LaunchBS3, the BS-3 crew layout and drill (bs3_crew.py: the Pak
                                layout turned 20 degrees about the breech so it fits between the trails) and 100 mm rounds
  Rus_BXP_BS3Pit                a copy of the 53-K pit expansion (enemy owned, engages when placed), type BTX_RusBS3Pit

Run: python build_bs3_pit.py [crew,mesh,socket,animbp,turret,crate,bxp]   (scratchpad uecp.py importable; UECP_PORT
picks an editor other than the 9855 broker's; crate only re-places the ReadyCrate on the existing turret)

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

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit'
GUN = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM'
SKELETON = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM/Mesh/SK_TW_Zis3_Skeleton'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/BS3_100mmAT/Exports'
PIT_53K_TURRET = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Turrets/RTS_BP_EmbTurret_Rus53KPit'
PIT_53K_BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_RusBS3Pit'
BXP = ROOT + '/Rus_BXP_BS3Pit'
ANIM_BP = ROOT + '/Anims/AnimBp_BS3Pit'
SANDBAG_MATERIAL = '/Game/SandBagPack/Materials/MI_Rus_SandBag_01'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/DT100mm/S_FireWeapon_LB_1_100mm.S_FireWeapon_LB_1_100mm'
PIT_ORIGIN = 'PitOriginBS3'
ROTATION_SPEED = 18.0
MAX_PITCH = 30.0
MIN_PITCH = -5.0
BORE_HEIGHT = 107.0
LAUNCH_SCALE = 1.3
MIN_RELOAD_FOR_DRILL = 3.0
# Pit sizes for the expansion: half the outer square and the wall height.
PIT_HALF = 620.0
PIT_HEIGHT = 70.0
SMOKE_RADIUS = 520.0
SMOKES = 5
DECAL_UNITS = 32.0
EXPANSION_TYPE = 'BTX_RUS_BS3_PIT'


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


def load_json(name):
    with open(f'{SOURCE}/Crew/{name}') as handle:
        return json.load(handle)


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint opens a confirmation a windowless editor cannot show.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", 'BS3 pit duplicate ' + target.split('/')[-1])


def import_crew():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
skeleton = unreal.load_asset('{INFANTRY_SKELETON}')
for name in ('Anim_BS3Pit_Handler_Reload', 'Anim_BS3Pit_Handler_Idle'):
    path = '{ROOT}/Crew/' + name
    # Reimporting over a sequence of another length fails; these assets are generated only, so start clean.
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    data = options.anim_sequence_import_data
    data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property('remove_redundant_keys', False)
    data.set_editor_property('import_bone_tracks', True)
    task = unreal.AssetImportTask()
    task.filename = '{SOURCE}/Crew/' + name + '.fbx'
    task.destination_path = '{ROOT}/Crew'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sequence = unreal.load_asset(path)
    # The handler walks through the pit on his root bone; the crew meshes are not characters.
    sequence.set_editor_property('enable_root_motion', False)
    eal.save_asset(path, only_if_is_dirty=False)
    print('BS3PIT_CREW', name, round(sequence.get_play_length(), 2))
""", 'BS3 pit handler animations')
    act('create_montage', source_animation=ROOT + '/Crew/Anim_BS3Pit_Handler_Reload', name='Montage_BS3Pit_Handler_Reload',
        save_path=ROOT + '/Crew')
    act('save_asset', asset_path=ROOT + '/Crew/Montage_BS3Pit_Handler_Reload')


def import_mesh():
    run_python(f"""
import unreal
task = unreal.AssetImportTask()
task.filename = '{SOURCE}/SM_BS3Pit.fbx'
task.destination_path = '{ROOT}/Meshes'
task.destination_name = 'SM_BS3Pit'
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
path = '{ROOT}/Meshes/SM_BS3Pit'
mesh = unreal.load_asset(path)
mesh.set_material(0, unreal.load_asset('{SANDBAG_MATERIAL}'))
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property('lod_group', 'LargeProp')
print('PIT', mesh.get_num_triangles(0), unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
""", 'BS3 pit mesh import')


def add_pit_origin():
    """PitOriginBS3: the gun frame origin, relative to the root bone's reference transform."""
    text = run_python(f"""
import unreal
seq = unreal.load_asset('{GUN}/DeployPacked/Anim_BS3_Deployed')
root = unreal.AnimationLibrary.get_bone_pose_for_time(seq, '37mmFlak', 0.0, False)
relative = unreal.Transform().multiply(root.inverse())
r = relative.rotation.rotator()
print('ORIGIN', relative.translation.x, relative.translation.y, relative.translation.z, r.pitch, r.yaw, r.roll)
""", 'BS3 pit origin')
    values = [float(v) for v in text.split('ORIGIN ')[1].split('\\')[0].split()[:6]]
    act('add_skeleton_socket', skeleton_path=SKELETON, socket_name=PIT_ORIGIN, bone_name='37mmFlak')
    act('set_skeleton_socket_transform', skeleton_path=SKELETON, socket_name=PIT_ORIGIN,
        relative_location=values[:3], relative_rotation=values[3:6], relative_scale=[1, 1, 1])
    act('save_asset', asset_path=SKELETON)
    print(PIT_ORIGIN, values)


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
    unreal.AssetToolsHelpers.get_asset_tools().create_asset('AnimBp_BS3Pit', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
""", 'BS3 pit anim blueprint')
    abp = ANIM_BP
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] != 'AnimGraphNode_Root':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence', property_value=GUN + '/DeployPacked/Anim_BS3_Deployed')
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
    print('AnimBp_BS3Pit compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)
    act('create_montage', source_animation=GUN + '/WeaponAnims/Anim_BS3_Fire', name='Montage_BS3Pit_Fire', save_path=ROOT + '/Anims')
    act('save_asset', asset_path=ROOT + '/Anims/Montage_BS3Pit_Fire')


def ue_text_vector(v):
    return f'(X={v[0]:.3f},Y={v[1]:.3f},Z={v[2]:.3f})'


def asset_text(path, kind):
    return f"/Script/Engine.{kind}'{path}.{path.split('/')[-1]}'"


def place_crate(crate):
    """Puts the ready crate under the round the handler picks up (the drill's crate spot)."""
    location, rotation, scale = pit_crate.crate_transform(pit_crate.CRATE_MESH, crate)
    act('set_static_mesh_component', blueprint_path=TURRET, component_name='ReadyCrate', mesh_path=pit_crate.CRATE_MESH)
    act('set_component_transform', blueprint_path=TURRET, component_name='ReadyCrate', location=location, rotation=rotation,
        scale=[scale] * 3)
    print('ReadyCrate', location, rotation, scale)


def crew_settings(layout, carry):
    members = []
    for name in ('Crew1', 'Crew2', 'Crew3', 'Crew4'):
        member = layout[name]
        entry = f'IdleAnimation="{asset_text(member["idle"], "AnimSequence")}"'
        if member['reload']:
            entry += f',ReloadMontage="{asset_text(member["reload"], "AnimMontage")}"'
        members.append(f'({entry})')
    fire = asset_text(ROOT + '/Anims/Montage_BS3Pit_Fire', 'AnimMontage')
    reload_value = (f'(GunFireMontage="{fire}",CrewAnimations=({",".join(members)}),'
                    f'MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings',
        value=reload_value)

    def pose(entry):
        return f'(Location={ue_text_vector(entry["location"])},Axis={ue_text_vector(entry["axis"])})'
    carry_value = (f'(RoundTag="PitRound",HandlerCrewIndex=1,LoaderCrewIndex=0,HandBone="hand_r",'
                   f'HandoverFraction={carry["handover_fraction"]:.4f},RamFraction={carry["ram_fraction"]:.4f},'
                   f'PickFraction={carry["pick_fraction"]:.4f},CrateRound={pose(carry["crate_round"])},'
                   f'IdleRound={pose(carry["idle_round"])},FirstRoundDelaySeconds=0.300000)')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ShellCarry',
        value=carry_value)
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='CrewAttachSocket',
        value=PIT_ORIGIN)


def build_turret():
    layout = load_json('bs3_crew_layout.json')
    carry = load_json('bs3pit_carry.json')
    duplicate(PIT_53K_TURRET, TURRET)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN + '/Mesh/SK_TW_BS3')
    act('edit_component_property', blueprint_path=TURRET, component_name='Turret', property_name='AnimClass',
        value=f'{ANIM_BP}.AnimBp_BS3Pit_C')
    for name in ('Crew1', 'Crew2', 'Crew3', 'Crew4'):
        x, y = layout[name]['location']
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=[x, y, 0.0],
            rotation=[0.0, layout[name]['yaw'], 0.0])
    place_crate(layout['crate'])
    # The BS-3's origin socket has its own name on the shared ZiS-3 skeleton; the ready crate follows it.
    act('edit_component_property', blueprint_path=TURRET, component_name='ReadyCrate', property_name='ComponentTags',
        value=f'("PitSocket:{PIT_ORIGIN}")')
    for name in ('Round1', 'Round2'):
        act('set_component_transform', blueprint_path=TURRET, component_name=name, scale=[layout['round_scale']] * 3)
    crew_settings(layout, carry)
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
                             (weapon, 'WeaponName', 'BS3_100MM_TW'), (weapon, 'FireSocketName', 'LaunchBS3'),
                             (vfx, 'LaunchSound', LAUNCH_SOUND),
                             (vfx, 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}')):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('BS3 pit turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


def build_bxp():
    duplicate(PIT_53K_BXP, BXP)
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    init = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp'][0]
    mesh_ref = f'{ROOT}/Meshes/SM_BS3Pit.SM_BS3Pit'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'), ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=init, pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=ROOT + '/Meshes/SM_BS3Pit')
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{TURRET}.RTS_BP_EmbTurret_RusBS3Pit_C')
    decal_scale = 2.0 * PIT_HALF / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={PIT_HALF:.1f},Y={PIT_HALF:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT + 220.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={PIT_HALF:.1f},Y={PIT_HALF:.1f},Z={PIT_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('BS3 pit bxp compile', compiled.get('error_count'), compiled.get('errors'))
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
""", 'BS3 pit expansion type')


def replace_crate():
    place_crate(load_json('bs3_crew_layout.json')['crate'])
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('BS3 pit turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['crew', 'mesh', 'socket', 'animbp', 'turret', 'bxp']
    for step in steps:
        {'crew': import_crew, 'mesh': import_mesh, 'socket': add_pit_origin, 'animbp': build_anim_blueprint,
         'turret': build_turret, 'crate': replace_crate, 'bxp': build_bxp}[step]()
