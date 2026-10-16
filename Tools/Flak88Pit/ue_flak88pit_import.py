"""Imports the German Flak 88 emplacement built in Blender (GerDefenses/Scripts/flak88_pit_build.py and
flak88_pit_crew.py) into GerPits. The gun is the game's Flak 88 model (GerDefenses/FlakSandbagged/S_FLak88.FBX),
imported into its own folder with its own skeleton, so the pit's sockets leave the embedded 88's skeleton alone.

Run inside the editor (bridge pyfile or the editor's Python console). STEPS (set before running) picks steps:
  'gun'      Meshes/Flak88/SK_GerFlak88Pit_Gun (+ its skeleton) with the 88's materials; then as 'sockets'
  'sockets'  prints the sockets build_flak88pit.py puts on the gun's skeleton: the embedded 88's fire sockets
             (BulletLaunch0, BulletOut0), PitOrigin on Flak88_Dock (the turning carriage; loader, handler and their
             rounds ride on it) and PitStatic on the static root (the ammo runner stands on it). Both sit at the deck
             top (the gun stands GUN_SINK cm below the pit's ground) with +X along the muzzle: the FBX's root bone
             carries a 180 degree turn, so the muzzle points along +X in the imported mesh's component space.
  'pit'      Meshes/SM_GerFlak88Pit: walls, niche, crate ring, deck (German pit atlas, ammo box, sandbag and the 88's
             ammo materials; complex collision; no Nanite: the sandbag material is not flagged for it)
  'round'    Meshes/SM_GerFlak88Pit_Round: the 88 mm round the crew carry (M_Flak88_Ammo)
  'crew'     Crew/Anim_GerFlak88Pit_*: loader, handler and runner on the Infantry Master skeleton (root motion off)
"""
import json

import unreal

BLENDER_EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Flak88Pit'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
MESH_DIR = ROOT + '/Meshes'
CREW_DIR = ROOT + '/Crew'
FLAK88 = '/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/Flak88'
PIT_MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
    'Flak88Pit_Shell': FLAK88 + '/Materials/M_Flak88_Ammo',
}
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
CREW_CLIPS = ('Loader_Idle', 'Loader_Reload', 'Handler_Idle', 'Handler_Reload', 'Runner_Loop')
PREFIX = 'Anim_GerFlak88Pit'
GUN_SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/FlakSandbagged/S_FLak88.FBX'
GUN_DIR = MESH_DIR + '/Flak88'
GUN_NAME = 'SK_GerFlak88Pit_Gun'
GUN_MATERIALS = {'M_Flak88_01': FLAK88 + '/Materials/M_Flak88_01', 'M_Flak88_02': FLAK88 + '/Materials/M_Flak88_02'}
# The embedded 88's fire sockets (on its skeleton): bone, location, rotation (pitch, yaw, roll).
FIRE_SOCKETS = {'BulletLaunch0': ('Flak88_GunBarrel', (-71.342887, 0.0, 1.0), (-0.122317, -179.999992, -0.001029)),
                'BulletOut0': ('Flak88_GunBarrel', (423.073179, 0.0, 0.0), (0.000108, 54.999998, -0.000076))}
# The gun mesh stands this far below the pit's ground and the deck is this high (flak88_pit_layout.json; copied here
# because the editor's sandboxed Python cannot read the Blender export folder).
GUN_SINK = 52.0
DECK_HEIGHT = 4.0
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
    path = f'{GUN_DIR}/{GUN_NAME}'
    # Interchange gives a new skeletal mesh any skeleton already in its folder: the folder holds only this gun.
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    _task(GUN_SOURCE, GUN_DIR, GUN_NAME, _skeletal_options())
    mesh = unreal.load_asset(path)
    skeleton = mesh.get_editor_property('skeleton')
    if not skeleton.get_path_name().startswith(GUN_DIR):
        raise RuntimeError(f'{GUN_NAME} was given the skeleton {skeleton.get_path_name()}; it needs its own')
    materials = mesh.get_editor_property('materials')
    for index in range(len(materials)):
        # The list holds struct copies: change one and put it back.
        entry = materials[index]
        target = GUN_MATERIALS.get(str(entry.get_editor_property('material_slot_name')))
        if target:
            entry.set_editor_property('material_interface', unreal.load_asset(target))
            materials[index] = entry
    mesh.set_editor_property('materials', materials)
    materials = mesh.get_editor_property('materials')
    print('FLAK88PIT_IMPORT gun', skeleton.get_path_name(),
          [(str(m.get_editor_property('material_slot_name')),
            m.get_editor_property('material_interface').get_name() if m.get_editor_property('material_interface') else None)
           for m in materials],
          'saved', eal.save_loaded_asset(mesh, only_if_is_dirty=False))
    return skeleton


def socket_transforms(skeleton):
    """Bone-relative socket transforms on the gun's own skeleton, from its reference pose (component space)."""
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    deck = unreal.Vector(0.0, 0.0, GUN_SINK + DECK_HEIGHT)
    result = {}
    desired = unreal.Transform(deck, unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), unreal.Vector(1.0, 1.0, 1.0))
    for socket, bone in (('PitOrigin', 'Flak88_Dock'), ('PitStatic', 'Flak88_Base')):
        bone_component = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        relative = desired.multiply(bone_component.inverse())
        rotator = relative.rotation.rotator()
        result[socket] = {'bone': bone,
                          'relative_location': [relative.translation.x, relative.translation.y, relative.translation.z],
                          'relative_rotation': [rotator.pitch, rotator.yaw, rotator.roll]}
    for socket, (bone, location, rotation) in FIRE_SOCKETS.items():
        result[socket] = {'bone': bone, 'relative_location': list(location), 'relative_rotation': list(rotation)}
    for bone in ('Flak88_Base', 'Flak88_Dock', 'Flak88_cannon', 'Flak88_GunBarrel'):
        transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        print('FLAK88PIT_BONE', bone, transform.translation, transform.rotation.rotator())
    print('FLAK88PIT_SOCKETS ' + json.dumps({'skeleton': skeleton.get_path_name(), 'sockets': result}))
    return result


def import_static(name, complex_collision):
    _task(f'{BLENDER_EXPORTS}/{name}.fbx', MESH_DIR, name, _static_options())
    path = f'{MESH_DIR}/{name}'
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = PIT_MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    if complex_collision:
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property('lod_group', 'LargeProp')
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    print('FLAK88PIT_IMPORT', name, [str(s.material_slot_name) for s in mesh.static_materials],
          [m.material_interface.get_name() if m.material_interface else None for m in mesh.static_materials],
          'tris', mesh.get_num_triangles(0), 'saved', eal.save_asset(path, only_if_is_dirty=False))


def import_crew_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for clip in CREW_CLIPS:
        name = f'{PREFIX}_{clip}'
        # A reimport cannot change the length of an existing sequence; these assets are generated only.
        if eal.does_asset_exist(f'{CREW_DIR}/{name}'):
            eal.delete_asset(f'{CREW_DIR}/{name}')
        _task(f'{BLENDER_EXPORTS}/Crew/{name}.fbx', CREW_DIR, name, _anim_options(skeleton))
        sequence = unreal.load_asset(f'{CREW_DIR}/{name}')
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(f'{CREW_DIR}/{name}', only_if_is_dirty=False)
        print('FLAK88PIT_IMPORT', name, round(sequence.get_play_length(), 2))


_steps = globals().get('STEPS') or ['gun', 'pit', 'round', 'crew']
if 'gun' in _steps:
    socket_transforms(import_gun())
if 'sockets' in _steps:
    socket_transforms(unreal.load_asset(f'{GUN_DIR}/{GUN_NAME}').get_editor_property('skeleton'))
if 'pit' in _steps:
    import_static('SM_GerFlak88Pit', True)
if 'round' in _steps:
    import_static('SM_GerFlak88Pit_Round', False)
if 'crew' in _steps:
    import_crew_anims()
