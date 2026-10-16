"""Imports the German Flak 37 pit's art (gerpit_build.py and gerpit_crew.py with the Flak config) into GerPits.

Run inside the editor (bridge pyfile or the editor's Python console):
  Meshes/SM_GerPit_Flak          the pit (German pit atlas, ammo box and sandbag materials, complex collision)
  Meshes/SM_GerPit_Clip_37       the 6-round clip the handler passes to the loader (ammo box material)
  Crew/Anim_GerPitFlak_Handler_Reload / _Idle, Crew/Anim_GerPitFlak_Loader_Idle   on the infantry skeleton
The Flak 37 skeleton gets PitOrigin on its root bone (37mmFlak): it cancels the root's 0.8 import scale, so the crew,
crate and clips ride on the gun at 1:1 in gun space (+X the muzzle). The socket transform is set through the bridge
(set_skeleton_socket_transform) from the values this script prints.

The game does not render Nanite: after (re)importing a pit or fieldwork mesh, cut it with
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py and run Tools/GunPits/optimize_pit_meshes.py heavy, which
imports the cut mesh and turns Nanite off (see Docs/Units/GunPitMeshes.md).
"""
import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
MESH_DIR = ROOT + '/Meshes'
CREW_DIR = ROOT + '/Crew'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
FLAK = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Flak37mm'
MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
}
ANIMS = ['Anim_GerPitFlak_Handler_Reload', 'Anim_GerPitFlak_Handler_Idle', 'Anim_GerPitFlak_Loader_Idle']
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
    if not list(task.get_editor_property('imported_object_paths')):
        raise RuntimeError('Import produced nothing for ' + filename)


def import_static(name, complex_collision, lod_group):
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
    _task(f'{EXPORT_DIR}/{name}.fbx', MESH_DIR, name, options)
    path = f'{MESH_DIR}/{name}'
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    flag = unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if complex_collision else unreal.CollisionTraceFlag.CTF_USE_DEFAULT
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', flag)
    if lod_group:
        mesh.set_editor_property('lod_group', lod_group)
    print('FLAKPIT_IMPORT', name, [str(s.material_slot_name) for s in mesh.static_materials], 'saved',
          eal.save_asset(path, only_if_is_dirty=False))


def import_crew_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for name in ANIMS:
        path = f'{CREW_DIR}/{name}'
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
        _task(f'{EXPORT_DIR}/Crew/{name}.fbx', CREW_DIR, name, options)
        sequence = unreal.load_asset(path)
        # The crew meshes are not characters; their animations move them through their root bone.
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(path, only_if_is_dirty=False)
        print('FLAKPIT_IMPORT', name, round(sequence.get_play_length(), 2))


def print_pit_origin():
    sequence = unreal.load_asset(f'{FLAK}/FireAnims/Anim_TW_Flak37_TPose')
    root = unreal.AnimationLibrary.get_bone_pose_for_time(sequence, '37mmFlak', 0.0, False)
    relative = unreal.Transform().multiply(root.inverse())
    r = relative.rotation.rotator()
    print('FLAKPIT_ORIGIN', relative.translation.x, relative.translation.y, relative.translation.z, r.pitch, r.yaw, r.roll,
          relative.scale3d.x, relative.scale3d.y, relative.scale3d.z)


_steps = globals().get('STEPS') or ['meshes', 'anims', 'origin']
if 'meshes' in _steps:
    import_static('SM_GerPit_Flak', True, 'LargeProp')
    import_static('SM_GerPit_Clip_37', False, '')
if 'anims' in _steps:
    import_crew_anims()
if 'origin' in _steps:
    print_pit_origin()
