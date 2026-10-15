"""Imports the ML-20 gun pit assets: the pit gun mesh, its drill, the crew drills, the 152mm shell prop and sockets.

Blender sources (AnimTeamWeapons/ML20_152mm): ml20_pit_animations.py (SK_ML20Pit_Gun, PitAnims, ml20_pit_curves.json)
and ml20_pit_crew.py (Crew/Anims). The pit gun shares the team weapon's skeleton (ue_ml20_import.py first).
Run inside the editor (bridge pyfile or the editor's Python console); set STEPS to run only some steps
(gun_mesh, pit_anims, crew_anims, shell). The curve file is a copy kept next to this script, because the bridge's
Python may only read files inside the project. The skeleton sockets (Launch, Out, PitOrigin, ShellMount, CartMount,
RammerMount) are set through the bridge by ml20_sockets.py, because Python cannot set a socket's transform.
"""
import json

import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/ML20_152mm'
B4_SOURCE_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/B4203MM'
CURVES_FILE = 'D:/UE5Projects/RTS_Survival/Source/Tools/ML20/ml20_pit_curves.json'
ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM'
MATERIAL_DIR = ROOT + '/Materials'
MESH_DIR = ROOT + '/Mesh'
PIT_DIR = ROOT + '/Pit'
PIT_MESH_DIR = PIT_DIR + '/Meshes'
PIT_ANIM_DIR = PIT_DIR + '/Anims'
CREW_DIR = PIT_DIR + '/Crew'
TEAM_WEAPON_MESH = MESH_DIR + '/SK_TW_ML20'
PIT_MESH_NAME = 'SK_ML20Pit_Gun'
SHELL_NAME = 'SM_ML20Pit_Shell'
# The 152mm shell is the B-4's 203mm cartridge scaled down.
SHELL_SCALE = 0.75
B4_MATERIAL_DIR = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Materials'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
CREW_COUNT = 4
# Read by UB4HowitzerPitAnimInstance: the ML-20 pit keeps the B-4's curve name so the B-4 pit anim instance works.
LOADING_CURVE = 'B4_LoadingElevation'
PIT_MATERIALS = {
    'MI_ML20Pit_Steel': '/Game/RTS_Survival/StarterContent/Materials/M_Metal_Burnished_Steel',
    'MI_ML20Pit_Timber': '/Game/RTS_Survival/StarterContent/Materials/M_Wood_Pine',
}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def _run_import(task):
    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths'))
    if not paths:
        raise RuntimeError('Import produced nothing for ' + task.filename)
    return paths


def _make_task(filename, destination, name, options):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    return task


def _team_weapon_skeleton():
    return unreal.load_asset(TEAM_WEAPON_MESH).get_editor_property('skeleton')


def import_gun_mesh():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.skeleton = _team_weapon_skeleton()
    data = options.skeletal_mesh_import_data
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    _run_import(_make_task(f'{EXPORT_DIR}/{PIT_MESH_NAME}.fbx', PIT_MESH_DIR, PIT_MESH_NAME, options))
    mesh = unreal.load_asset(f'{PIT_MESH_DIR}/{PIT_MESH_NAME}')
    # The crane keeps the B-4's textures; the pedestal and the loader's step use plain engine materials.
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        slot = str(entry.get_editor_property('material_slot_name'))
        if slot.startswith('MI_ML20Pit_'):
            target = PIT_MATERIALS[slot]
        elif slot.startswith('MI_B4_'):
            target = f'{B4_MATERIAL_DIR}/{slot}'
        else:
            target = f'{MATERIAL_DIR}/{slot}'
        entry.set_editor_property('material_interface', unreal.load_asset(target))
    mesh.set_editor_property('materials', materials)
    eal.save_loaded_asset(mesh)
    print('ML20PIT_IMPORT gun mesh slots', [str(m.get_editor_property('material_slot_name')) for m in materials])


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


def _import_anim(filename, folder, asset_name, skeleton):
    # Reimporting over a sequence of another length or rate fails; these assets are generated only, so start clean.
    if eal.does_asset_exist(f'{folder}/{asset_name}'):
        eal.delete_asset(f'{folder}/{asset_name}')
    _run_import(_make_task(filename, folder, asset_name, _anim_options(skeleton)))
    sequence = unreal.load_asset(f'{folder}/{asset_name}')
    print('ML20PIT_IMPORT', asset_name, round(sequence.get_play_length(), 2))
    return sequence


def import_pit_anims():
    skeleton = _team_weapon_skeleton()
    for name in ('Idle', 'Fire', 'Reload'):
        asset_name = f'Anim_ML20Pit_{name}'
        _import_anim(f'{EXPORT_DIR}/PitAnims/{asset_name}.fbx', PIT_ANIM_DIR, asset_name, skeleton)
    add_loading_curve()


def add_loading_curve():
    reload_anim = unreal.load_asset(f'{PIT_ANIM_DIR}/Anim_ML20Pit_Reload')
    with open(CURVES_FILE) as handle:
        keys = json.load(handle)[LOADING_CURVE]
    library = unreal.AnimationLibrary
    if library.does_curve_exist(reload_anim, LOADING_CURVE, unreal.RawCurveTrackTypes.RCT_FLOAT):
        library.remove_curve(reload_anim, LOADING_CURVE)
    library.add_curve(reload_anim, LOADING_CURVE, unreal.RawCurveTrackTypes.RCT_FLOAT)
    library.add_float_curve_keys(reload_anim, LOADING_CURVE, [k[0] for k in keys], [k[1] for k in keys])
    eal.save_loaded_asset(reload_anim)
    print('ML20PIT_IMPORT loading curve keys', len(keys))


def import_crew_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for index in range(1, CREW_COUNT + 1):
        for kind in ('Idle', 'Reload'):
            asset_name = f'Anim_ML20PitCrew{index}_{kind}'
            sequence = _import_anim(f'{EXPORT_DIR}/Crew/Anims/{asset_name}.fbx', CREW_DIR, asset_name, skeleton)
            # The crew walk around the pit through their root bone; the meshes are not characters.
            sequence.set_editor_property('enable_root_motion', False)
            eal.save_loaded_asset(sequence)


def import_shell():
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
    data.set_editor_property('import_uniform_scale', SHELL_SCALE)
    _run_import(_make_task(f'{B4_SOURCE_DIR}/SM_Cartridge.FBX', PIT_MESH_DIR, SHELL_NAME, options))
    mesh = unreal.load_asset(f'{PIT_MESH_DIR}/{SHELL_NAME}')
    for index, entry in enumerate(mesh.static_materials):
        slot = str(entry.material_slot_name).split('.')[0]
        mesh.set_material(index, unreal.load_asset(f'{B4_MATERIAL_DIR}/{slot}'))
    eal.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
    bounds = mesh.get_bounding_box()
    print('ML20PIT_IMPORT shell bounds', bounds.min, bounds.max)


STEP_FUNCTIONS = {'gun_mesh': import_gun_mesh, 'pit_anims': import_pit_anims, 'crew_anims': import_crew_anims,
                  'shell': import_shell}
for _step in (globals().get('STEPS') or list(STEP_FUNCTIONS)):
    STEP_FUNCTIONS[_step]()
