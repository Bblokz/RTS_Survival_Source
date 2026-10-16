"""Imports the Soviet DShK machine-gun pit built by dshk_pit_build.py (Blender) into the editor.

Run inside the editor (bridge pyfile or the editor's Python console). Blender sources live in
D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/MGPit (Scripts/dshk_pit_build.py).
  Meshes/SK_DShKPit_Gun   rigged DShK on its tripod (M_IS2), new skeleton
  Meshes/SM_DShKPit       the sandbag pit (RedLine earth, timber and sandbag materials, complex collision)
  Anims/Anim_DShKPit_*    gun base pose, recoil and box swap
  Crew/Anim_DShKPit_*     gunner and loader on the Infantry Master skeleton (root motion off: the crew meshes are
                          plain components on the gun, their animations carry their spot)
print_socket_transforms() prints the PitOrigin and Muzzle socket transforms (bone-relative) for the bridge's
set_skeleton_socket_transform; see Docs/Units/RusDShKPit.md.

The game does not render Nanite: after (re)importing a pit or fieldwork mesh, cut it with
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py and run Tools/GunPits/optimize_pit_meshes.py heavy, which
imports the cut mesh and turns Nanite off (see Docs/Units/GunPitMeshes.md).
"""
import json

import unreal

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/MGPit/Exports'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit'
MESH_DIR = ROOT + '/Meshes'
ANIM_DIR = ROOT + '/Anims'
CREW_DIR = ROOT + '/Crew'
GUN_MESH = 'SK_DShKPit_Gun'
PIT_MESH = 'SM_DShKPit'
GUN_MATERIAL = '/Game/RTS_Survival/Blueprints/Turrets/Russian/HullWeapons/Meshes/M_IS2'
PIT_MATERIALS = {
    'RL_Earth': '/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Desert/MI_RTS_DesertGround_Stones',
    'RL_Timber': '/Game/RTS_Survival/StarterContent/Materials/M_Wood_Pine',
    'RL_Sandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
}
GUN_ANIMS = ['Anim_DShKPit_Base', 'Anim_DShKPit_Fire', 'Anim_DShKPit_Reload']
CREW_ANIMS = ['Anim_DShKPit_Gunner_Idle', 'Anim_DShKPit_Gunner_Fire', 'Anim_DShKPit_Gunner_Reload',
              'Anim_DShKPit_Loader_Idle', 'Anim_DShKPit_Loader_Reload']
INFO_JSON = 'D:/UE5Projects/RTS_Survival/Source/Tools/MGPit/dshk_pit_info.json'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def _task(filename, destination, name, options):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths'))
    if not paths:
        raise RuntimeError('Import produced nothing for ' + filename)
    return paths


def _skeletal_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    data = options.skeletal_mesh_import_data
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def _anim_options(skeleton):
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
    return options


def _static_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    return options


def import_gun():
    _task(f'{SOURCE}/{GUN_MESH}.fbx', MESH_DIR, GUN_MESH, _skeletal_options())
    mesh = unreal.load_asset(f'{MESH_DIR}/{GUN_MESH}')
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        entry.set_editor_property('material_interface', unreal.load_asset(GUN_MATERIAL))
    mesh.set_editor_property('materials', materials)
    eal.save_loaded_asset(mesh)
    print('DSHKPIT_IMPORT gun slots', [str(m.get_editor_property('material_slot_name')) for m in materials])
    return mesh


def import_gun_anims(skeleton):
    for name in GUN_ANIMS:
        # A reimport cannot change the length of an existing sequence; these assets are generated only.
        if eal.does_asset_exist(f'{ANIM_DIR}/{name}'):
            eal.delete_asset(f'{ANIM_DIR}/{name}')
        _task(f'{SOURCE}/Anims/{name}.fbx', ANIM_DIR, name, _anim_options(skeleton))
        print('DSHKPIT_IMPORT', name, round(unreal.load_asset(f'{ANIM_DIR}/{name}').get_play_length(), 2))


def import_crew_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for name in CREW_ANIMS:
        if eal.does_asset_exist(f'{CREW_DIR}/{name}'):
            eal.delete_asset(f'{CREW_DIR}/{name}')
        _task(f'{SOURCE}/Crew/{name}.fbx', CREW_DIR, name, _anim_options(skeleton))
        sequence = unreal.load_asset(f'{CREW_DIR}/{name}')
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(f'{CREW_DIR}/{name}', only_if_is_dirty=False)
        print('DSHKPIT_IMPORT', name, round(sequence.get_play_length(), 2))


def import_pit():
    _task(f'{SOURCE}/{PIT_MESH}.fbx', MESH_DIR, PIT_MESH, _static_options())
    path = f'{MESH_DIR}/{PIT_MESH}'
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = PIT_MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_editor_property('lod_group', 'LargeProp')
    print('DSHKPIT_IMPORT pit', [str(s.material_slot_name) for s in mesh.static_materials], 'tris',
          mesh.get_num_triangles(0), 'saved', eal.save_asset(path, only_if_is_dirty=False))


def _component_transform(sequence, bone, parents):
    """Reference (base pose) transform of a bone in component space."""
    transform = unreal.AnimationLibrary.get_bone_pose_for_time(sequence, bone, 0.0, False)
    parent = parents.get(bone)
    while parent:
        transform = transform.multiply(unreal.AnimationLibrary.get_bone_pose_for_time(sequence, parent, 0.0, False))
        parent = parents.get(parent)
    return transform


def print_socket_transforms():
    """PitOrigin (Traverse bone, at the gun frame origin) and Muzzle (Gun bone), bone-relative."""
    # A copy of Exports/dshk_pit_info.json; the editor's sandboxed Python reads files inside the project only.
    info = json.load(open(INFO_JSON))
    sequence = unreal.load_asset(f'{ANIM_DIR}/Anim_DShKPit_Base')
    parents = {'Traverse': 'Root', 'Elevation': 'Traverse', 'Gun': 'Elevation'}
    muzzle = info['muzzle']
    wanted = {'PitOrigin': ('Traverse', unreal.Vector(0.0, 0.0, 0.0)),
              'Muzzle': ('Gun', unreal.Vector(muzzle[0], -muzzle[1], muzzle[2]))}
    result = {}
    for socket, (bone, location) in wanted.items():
        bone_component = _component_transform(sequence, bone, parents)
        desired = unreal.Transform(location, unreal.Rotator(0.0, 0.0, 0.0), unreal.Vector(1.0, 1.0, 1.0))
        relative = desired.multiply(bone_component.inverse())
        rotator = relative.rotation.rotator()
        result[socket] = {'bone': bone,
                          'relative_location': [relative.translation.x, relative.translation.y, relative.translation.z],
                          'relative_rotation': [rotator.pitch, rotator.yaw, rotator.roll],
                          'relative_scale': [relative.scale3d.x, relative.scale3d.y, relative.scale3d.z],
                          'bone_component': str(bone_component)}
    print('DSHKPIT_SOCKETS ' + json.dumps(result))
    return result


def run_all():
    mesh = import_gun()
    import_gun_anims(mesh.get_editor_property('skeleton'))
    import_crew_anims()
    import_pit()
    print_socket_transforms()


# STEPS (set before running the file) picks steps: 'gun', 'gun_anims', 'crew', 'pit', 'sockets'.
_steps = globals().get('STEPS')
if not _steps:
    run_all()
else:
    if 'gun' in _steps:
        import_gun()
    if 'gun_anims' in _steps:
        import_gun_anims(unreal.load_asset(f'{MESH_DIR}/{GUN_MESH}').get_editor_property('skeleton'))
    if 'crew' in _steps:
        import_crew_anims()
    if 'pit' in _steps:
        import_pit()
    if 'sockets' in _steps:
        print_socket_transforms()
