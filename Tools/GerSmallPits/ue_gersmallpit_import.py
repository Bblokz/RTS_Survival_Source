"""Imports a small German pit built in Blender (GerDefenses/Scripts/puppchen_pit_build.py or
panzerschreck_pit_build.py) into GerPits.

Run inside the editor (bridge pyfile or the editor's Python console) with PIT set first: PIT = 'Puppchen' or
'Panzerschreck'. Copy the pit's Exports/<Pit>Pit/<pit>_pit_info.json next to this file first: the editor's sandboxed
Python only reads project files.
  Meshes/M_GerSmallPit_Gun        copy of the German infantry weapon material (MM_Ger_Infantry_Weapons) that may be
                                  used on skeletal meshes; both guns use the German weapon atlas
  Meshes/<Pit>/SK_Ger<Pit>Pit_Gun the rigged weapon with its rocket crate and its own skeleton (one folder per gun:
                                  Interchange gives a new skeletal mesh any skeleton already in its folder)
  Meshes/SM_Ger<Pit>Pit           the bag ring (German pit atlas, ammo box and sandbag materials, complex collision,
                                  no Nanite: the sandbag material is not flagged for it)
  Anims/Anim_Ger<Pit>Pit_*        gun base pose, shot and loading drill; the reload carries the curve
                                  Ger<Pit>Pit_LoadingElevation that levels the weapon while it is loaded
  Crew/Anim_Ger<Pit>Pit_*         gunner and loader on the Infantry Master skeleton (root motion off)
STEPS (set before running) picks steps: 'gun', 'gun_anims', 'crew', 'pit', 'sockets'; print_socket_transforms()
prints the PitOrigin and Muzzle socket transforms (bone-relative) for build_gersmallpit_animbp.py.
"""
import json

import unreal

BLENDER_EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports'
TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/GerSmallPits'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
MESH_DIR = ROOT + '/Meshes'
ANIM_DIR = ROOT + '/Anims'
CREW_DIR = ROOT + '/Crew'
WEAPON_MATERIAL = '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/MMaterial/MM_Ger_Infantry_Weapons'
GUN_MATERIAL = MESH_DIR + '/M_GerSmallPit_Gun'
PIT_MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
}
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
PITS = {
    'Puppchen': {'folder': 'PuppchenPit', 'info': 'puppchen_pit_info.json'},
    'Panzerschreck': {'folder': 'PanzerschreckPit', 'info': 'panzerschreck_pit_info.json'},
}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def names(pit):
    stem = f'Ger{pit}Pit'
    # Each gun in its own folder: Interchange gives a new skeletal mesh any skeleton already in its folder.
    return {'gun': f'SK_{stem}_Gun', 'gun_dir': f'{MESH_DIR}/{pit}', 'pit': f'SM_{stem}', 'prefix': f'Anim_{stem}', 'curve': f'{stem}_LoadingElevation',
            'source': f"{BLENDER_EXPORTS}/{PITS[pit]['folder']}", 'info': f"{TOOLS}/{PITS[pit]['info']}"}


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


def make_gun_material():
    """The German infantry weapon material is not flagged for skeletal meshes; the pit guns use a flagged copy."""
    if not eal.does_asset_exist(GUN_MATERIAL):
        eal.duplicate_asset(WEAPON_MATERIAL, GUN_MATERIAL)
    material = unreal.load_asset(GUN_MATERIAL)
    material.set_editor_property('used_with_skeletal_mesh', True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    print('GERSMALLPIT_IMPORT material saved', eal.save_asset(GUN_MATERIAL, only_if_is_dirty=False))
    return material


def import_gun(pit):
    n = names(pit)
    material = make_gun_material()
    _task(f"{n['source']}/{n['gun']}.fbx", n['gun_dir'], n['gun'], _skeletal_options())
    mesh = unreal.load_asset(f"{n['gun_dir']}/{n['gun']}")
    skeleton = mesh.get_editor_property('skeleton')
    if skeleton.get_name() != n['gun'] + '_Skeleton':
        raise RuntimeError(f"{n['gun']} was given the skeleton {skeleton.get_name()}; it needs its own")
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        entry.set_editor_property('material_interface', material)
    mesh.set_editor_property('materials', materials)
    # A reimport over the existing mesh has silently kept the old geometry once; the save result shows it.
    print('GERSMALLPIT_IMPORT', n['gun'], 'saved', eal.save_loaded_asset(mesh, only_if_is_dirty=False))
    return mesh


def add_loading_curve(pit):
    n = names(pit)
    with open(n['info']) as handle:
        keys = json.load(handle)['loading_elevation_keys']
    reload_anim = unreal.load_asset(f"{ANIM_DIR}/{n['prefix']}_Reload")
    library = unreal.AnimationLibrary
    if library.does_curve_exist(reload_anim, n['curve'], unreal.RawCurveTrackTypes.RCT_FLOAT):
        library.remove_curve(reload_anim, n['curve'])
    library.add_curve(reload_anim, n['curve'], unreal.RawCurveTrackTypes.RCT_FLOAT)
    library.add_float_curve_keys(reload_anim, n['curve'], [k[0] for k in keys], [k[1] for k in keys])
    eal.save_loaded_asset(reload_anim)


def import_gun_anims(pit):
    n = names(pit)
    skeleton = unreal.load_asset(f"{n['gun_dir']}/{n['gun']}").get_editor_property('skeleton')
    for clip in ('Base', 'Fire', 'Reload'):
        name = f"{n['prefix']}_{clip}"
        # A reimport cannot change the length of an existing sequence; these assets are generated only.
        if eal.does_asset_exist(f'{ANIM_DIR}/{name}'):
            eal.delete_asset(f'{ANIM_DIR}/{name}')
        _task(f"{n['source']}/Anims/{name}.fbx", ANIM_DIR, name, _anim_options(skeleton))
        print('GERSMALLPIT_IMPORT', name, round(unreal.load_asset(f'{ANIM_DIR}/{name}').get_play_length(), 2))
    add_loading_curve(pit)


def import_crew_anims(pit):
    n = names(pit)
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for clip in ('Gunner_Idle', 'Gunner_Fire', 'Gunner_Reload', 'Loader_Idle', 'Loader_Reload'):
        name = f"{n['prefix']}_{clip}"
        if eal.does_asset_exist(f'{CREW_DIR}/{name}'):
            eal.delete_asset(f'{CREW_DIR}/{name}')
        _task(f"{n['source']}/Crew/{name}.fbx", CREW_DIR, name, _anim_options(skeleton))
        sequence = unreal.load_asset(f'{CREW_DIR}/{name}')
        sequence.set_editor_property('enable_root_motion', False)
        eal.save_asset(f'{CREW_DIR}/{name}', only_if_is_dirty=False)
        print('GERSMALLPIT_IMPORT', name, round(sequence.get_play_length(), 2))


def import_pit(pit):
    n = names(pit)
    _task(f"{n['source']}/{n['pit']}.fbx", MESH_DIR, n['pit'], _static_options())
    path = f"{MESH_DIR}/{n['pit']}"
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = PIT_MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_editor_property('lod_group', 'LargeProp')
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    print('GERSMALLPIT_IMPORT', n['pit'], [str(s.material_slot_name) for s in mesh.static_materials], 'tris',
          mesh.get_num_triangles(0), 'saved', eal.save_asset(path, only_if_is_dirty=False))


def _component_transform(sequence, bone, parents):
    transform = unreal.AnimationLibrary.get_bone_pose_for_time(sequence, bone, 0.0, False)
    parent = parents.get(bone)
    while parent:
        transform = transform.multiply(unreal.AnimationLibrary.get_bone_pose_for_time(sequence, parent, 0.0, False))
        parent = parents.get(parent)
    return transform


def print_socket_transforms(pit):
    """PitOrigin (Traverse bone, at the gun frame origin) and Muzzle (Gun bone), bone-relative."""
    n = names(pit)
    with open(n['info']) as handle:
        muzzle = json.load(handle)['muzzle']
    sequence = unreal.load_asset(f"{ANIM_DIR}/{n['prefix']}_Base")
    parents = {'Traverse': 'Root', 'Elevation': 'Traverse', 'Gun': 'Elevation'}
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
                          'relative_rotation': [rotator.pitch, rotator.yaw, rotator.roll]}
    print('GERSMALLPIT_SOCKETS ' + json.dumps(result))
    return result


_pit = globals().get('PIT')
if _pit not in PITS:
    raise RuntimeError(f'Set PIT to one of {list(PITS)} before running this file')
_steps = globals().get('STEPS') or ['gun', 'gun_anims', 'crew', 'pit', 'sockets']
if 'gun' in _steps:
    import_gun(_pit)
if 'gun_anims' in _steps:
    import_gun_anims(_pit)
if 'crew' in _steps:
    import_crew_anims(_pit)
if 'pit' in _steps:
    import_pit(_pit)
if 'sockets' in _steps:
    print_socket_transforms(_pit)
