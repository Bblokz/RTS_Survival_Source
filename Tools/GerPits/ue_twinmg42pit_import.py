"""Imports the German twin MG42 pit's assets (run inside the editor, e.g. the bridge's execute_python).

Blender sources: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/TwinMG42Pit
(generate_twinmg42_pit.py, twinmg42_pit_crew.py; see Docs/Units/GerTwinMG42Pit.md).
  'pit'     SM_GerTwinMG42Pit_Opt.fbx over Meshes/SM_GerPit_TwinMG42_Sandbags (the BXP keeps its reference): the two
            round MG nests, the corridor and the ammunition bay. German sandbag + ammo box materials, complex
            collision, LargeProp LODs, Nanite off.
  'box'     Meshes/SM_GerTwinMG42Pit_AmmoBox, the belt box a loader carries (no collision, Nanite off).
  'crew'    Crew/Anim_GerTwinMG42Pit_Loader{L,R}_{Reload,Idle} on the Infantry Master skeleton (root motion off).
  'socket'  PitStatic on the DShK gun skeleton's static Root bone, coincident with PitOrigin at rest: the loaders
            stand on it so they do not turn with the gun.
STEPS (set before running the file) picks steps; all by default.
"""
import unreal

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/TwinMG42Pit'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit'
MESH_DIR = ROOT + '/Meshes'
CREW_DIR = ROOT + '/Crew'
PIT_ASSET = 'SM_GerPit_TwinMG42_Sandbags'
BOX_ASSET = 'SM_GerTwinMG42Pit_AmmoBox'
GUN_SKELETON = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Meshes/SK_DShKPit_Gun_Skeleton'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
MATERIALS = {
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
}
CREW_ANIMS = ['Anim_GerTwinMG42Pit_LoaderL_Reload', 'Anim_GerTwinMG42Pit_LoaderL_Idle',
              'Anim_GerTwinMG42Pit_LoaderR_Reload', 'Anim_GerTwinMG42Pit_LoaderR_Idle']
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


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
    data.set_editor_property('build_nanite', False)
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


def _nanite_off(mesh):
    # Through the subsystem so the mesh rebuilds: setting the property alone leaves the Nanite fallback in place.
    settings = mesh.get_editor_property('nanite_settings')
    settings.enabled = False
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)


def _static(fbx, name, collision):
    _task(f'{SOURCE}/{fbx}.fbx', MESH_DIR, name, _static_options())
    path = f'{MESH_DIR}/{name}'
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    if collision:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property('lod_group', 'LargeProp')
    _nanite_off(mesh)
    print('TWINMG42_IMPORT', name, [str(s.material_slot_name) for s in mesh.static_materials], 'tris',
          mesh.get_num_triangles(0), 'bounds', mesh.get_bounding_box(), 'saved', eal.save_asset(path, only_if_is_dirty=False))


def import_pit():
    _static('SM_GerTwinMG42Pit_Opt', PIT_ASSET, True)


def import_box():
    _static('SM_GerTwinMG42Pit_AmmoBox', BOX_ASSET, False)


def import_crew():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for name in CREW_ANIMS:
        # A reimport cannot change an existing sequence's length; these assets are generated only.
        if eal.does_asset_exist(f'{CREW_DIR}/{name}'):
            eal.delete_asset(f'{CREW_DIR}/{name}')
        _task(f'{SOURCE}/Crew/{name}.fbx', CREW_DIR, name, _anim_options(skeleton))
        sequence = unreal.load_asset(f'{CREW_DIR}/{name}')
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(f'{CREW_DIR}/{name}', only_if_is_dirty=False)
        print('TWINMG42_IMPORT', name, round(sequence.get_play_length(), 3))


def socket_transform():
    """Checks PitStatic on the gun skeleton: it must sit where PitOrigin does in the reference pose. Both sockets live
    on the skeleton. PitStatic was added with the bridge's add_skeleton_socket (bone Root, location 0, rotation
    pitch 0 / yaw 0 / roll -90), which matches PitOrigin (Traverse, 0,84,0 / roll -90) because Root and Traverse share
    their rotation."""
    skeleton = unreal.load_asset(GUN_SKELETON)
    gun = unreal.load_asset(GUN_SKELETON.replace('_Skeleton', ''))
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    poses = {}
    for socket in ('PitOrigin', 'PitStatic'):
        # AnimPoseExtensions.get_socket_pose returns identity for a socket it cannot find, so compose bone + socket.
        mesh_socket = gun.find_socket(socket)
        if not mesh_socket:
            raise RuntimeError('Socket missing on the gun skeleton: ' + socket)
        bone = unreal.AnimPoseExtensions.get_bone_pose(pose, mesh_socket.get_editor_property('bone_name'),
                                                       unreal.AnimPoseSpaces.WORLD)
        relative = unreal.Transform(mesh_socket.get_editor_property('relative_location'),
                                    mesh_socket.get_editor_property('relative_rotation'),
                                    mesh_socket.get_editor_property('relative_scale'))
        poses[socket] = unreal.MathLibrary.compose_transforms(relative, bone)
        print('TWINMG42_SOCKET', socket, mesh_socket.get_editor_property('bone_name'), poses[socket].translation,
              poses[socket].rotation.rotator())
    offset = poses['PitStatic'].translation - poses['PitOrigin'].translation
    print('TWINMG42_SOCKET PitStatic matches PitOrigin', offset.length() < 0.01)


def run(steps):
    if 'pit' in steps:
        import_pit()
    if 'box' in steps:
        import_box()
    if 'crew' in steps:
        import_crew()
    if 'socket' in steps:
        socket_transform()


run(globals().get('STEPS') or ('pit', 'box', 'crew', 'socket'))
