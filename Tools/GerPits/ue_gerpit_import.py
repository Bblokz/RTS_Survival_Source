"""Imports the German gun pits: pit meshes, rounds and the shell handler animations.

Run inside the editor (bridge pyfile or the editor's Python console). Blender sources live in
D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Scripts (gerpit_build.py, gerpit_crew.py).
The pits use two materials only: the German camo/concrete/plank atlas and the ammo box material.
"""
import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
MESH_DIR = ROOT + '/Meshes'
CREW_DIR = ROOT + '/Crew'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
}
PITS = ['SM_GerPit_Pak', 'SM_GerPit_Howitzer']
ROUNDS = ['SM_GerPit_Shell_75', 'SM_GerPit_Shell_105', 'SM_GerPit_Shell_150']
HANDLER_ANIMS = ['Anim_GerPitPak_Handler_Reload', 'Anim_GerPitPak_Handler_Idle',
                 'Anim_GerPitHowitzer_Handler_Reload', 'Anim_GerPitHowitzer_Handler_Idle']
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
    return options


def import_static(name, complex_collision, lod_group):
    _task(f'{EXPORT_DIR}/{name}.fbx', MESH_DIR, name, _static_options())
    path = f'{MESH_DIR}/{name}'
    mesh = unreal.load_asset(path)
    # Editing a copy of static_materials does not persist; set_material does.
    for index, slot in enumerate(mesh.static_materials):
        target = MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    flag = unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if complex_collision else unreal.CollisionTraceFlag.CTF_USE_DEFAULT
    body.set_editor_property('collision_trace_flag', flag)
    if lod_group:
        mesh.set_editor_property('lod_group', lod_group)
    saved = eal.save_asset(path, only_if_is_dirty=False)
    print('GERPIT_IMPORT', name, 'materials', [str(s.material_slot_name) for s in mesh.static_materials],
          'tris', mesh.get_num_triangles(0), 'saved', saved)


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


def import_handler_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for name in HANDLER_ANIMS:
        path = f'{CREW_DIR}/{name}'
        # Reimporting over a sequence of another length fails; these assets are generated only, so start clean.
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        _task(f'{EXPORT_DIR}/Crew/{name}.fbx', CREW_DIR, name, _anim_options(skeleton))
        sequence = unreal.load_asset(path)
        # The handler walks through the pit on his root bone; the crew meshes are not characters.
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(path, only_if_is_dirty=False)
        print('GERPIT_IMPORT', name, 'length', round(sequence.get_play_length(), 2))


def run_all():
    for name in PITS:
        import_static(name, True, 'LargeProp')
    for name in ROUNDS:
        import_static(name, False, '')
    import_handler_anims()


run_all()
