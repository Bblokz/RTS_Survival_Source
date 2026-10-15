"""Imports the Soviet 100 mm BS-3 field gun built by bs3_build.py (Blender) into the editor.

Run inside the editor (bridge pyfile or the editor's Python console). Blender sources:
D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/BS3_100mmAT (Scripts/bs3_build.py).

The mesh and its animations go onto the ZiS-3's skeleton (SK_TW_Zis3_Skeleton: same bone names and hierarchy), so
the ZiS-3 anim blueprint, team weapon and squad can be copied with their assets swapped. Each mesh keeps its own
reference pose; the BS-3's fire and shell sockets are separate skeleton sockets (LaunchBS3, OutBS3) because the
ZiS-3's Launch and Out sit at its own, shorter barrel.
  Mesh/SK_TW_BS3, Mesh/SM_Destroyed_BS3       gun and wreck (MI_BS3, MI_BS3_Burnt on M_MasterPainter1)
  DeployPacked/, WeaponAnims/, WheelsAnims/   the animations, with the ZiS-3's additive settings for the aim poses
                                              and the recoil
STEPS (set before running the file) picks steps: 'materials', 'mesh', 'anims', 'wreck', 'sockets'.
"""
import json

import unreal

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/BS3_100mmAT/Exports'
ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM'
ZIS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM'
ZIS_SKELETON = ZIS + '/Mesh/SK_TW_Zis3_Skeleton'
MASTER_MATERIAL = '/Game/RTS_Survival/Meshes/Military/M_MasterPainter1'
# A copy of Exports/bs3_rig_info.json: the editor's sandboxed Python reads files inside the project only.
INFO_JSON = 'D:/UE5Projects/RTS_Survival/Source/Tools/BS3/bs3_rig_info.json'
# Animation -> (folder, the ZiS-3 animation whose additive settings it copies).
ANIMS = {
    'Deployed': ('DeployPacked', None), 'Packed': ('DeployPacked', None), 'DeployedToPacked': ('DeployPacked', None),
    'Base': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Base'), 'Left25': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Left25'),
    'Right25': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Right25'), 'Up30': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Up30'),
    'Down10': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Down10'),
    'Left25Up30': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Left25Up30'),
    'Left25Down10': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Left25Down10'),
    'Right25Up30': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Right25Up30'),
    'Right25Down10': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Right25Down10'),
    'Fire': ('WeaponAnims', 'WeaponAnims/Anim_Zis3_Fire'), 'WheelsFWD': ('WheelsAnims', None),
}
BURNT_COLOUR = unreal.LinearColor(0.22, 0.2, 0.19, 0.0)
BURNT_ROUGHNESS_BLEND = 0.6
PAINTED_METALLIC = 0.15
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _task(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options is not None:
        task.options = options
    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths'))
    if not paths:
        raise RuntimeError('Import produced nothing for ' + filename)
    return paths


def make_materials():
    for name in ('T_BS3_D', 'T_BS3_ORM'):
        texture = unreal.load_asset(_task(f'{SOURCE}/{name}.png', f'{ROOT}/Materials', name)[0])
        # The ORM is stored sRGB-encoded because M_MasterPainter1 samples it with a Color sampler.
        texture.set_editor_property('srgb', True)
        eal.save_loaded_asset(texture)
    instance = _instance('MI_BS3', unreal.load_asset(MASTER_MATERIAL))
    mel.set_material_instance_texture_parameter_value(instance, 'albedoTexture', unreal.load_asset(f'{ROOT}/Materials/T_BS3_D'))
    mel.set_material_instance_texture_parameter_value(instance, 'AORoughnessMetallicTexture',
                                                      unreal.load_asset(f'{ROOT}/Materials/T_BS3_ORM'))
    for switch, value in (('albedoTextureEnabled', True), ('occlusionTexEnabled', True), ('RoughnessTexEnabled', True),
                          ('metallicTexEnabled', True), ('normalMapTexEnabled', False), ('detailNormalEnabled', False),
                          ('emissionEnabled', False)):
        mel.set_material_instance_static_switch_parameter_value(instance, switch, value)
    mel.set_material_instance_vector_parameter_value(instance, 'ColorCorrection', unreal.LinearColor(1, 1, 1, 0))
    mel.set_material_instance_scalar_parameter_value(instance, 'Roughness Blend', 0.0)
    # The Unity metallic map marks the painted steel half metallic (mean 0.53), which renders it near black.
    mel.set_material_instance_scalar_parameter_value(instance, 'metallicValueMultiplier', PAINTED_METALLIC)
    mel.update_material_instance(instance)
    eal.save_loaded_asset(instance)
    burnt = _instance('MI_BS3_Burnt', instance)
    mel.set_material_instance_vector_parameter_value(burnt, 'ColorCorrection', BURNT_COLOUR)
    mel.set_material_instance_scalar_parameter_value(burnt, 'Roughness Blend', BURNT_ROUGHNESS_BLEND)
    mel.update_material_instance(burnt)
    eal.save_loaded_asset(burnt)
    print('BS3 materials done')


def _instance(name, parent):
    path = f'{ROOT}/Materials/{name}'
    if eal.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = asset_tools.create_asset(name, f'{ROOT}/Materials', unreal.MaterialInstanceConstant,
                                            unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property('parent', parent)
    return instance


def import_mesh():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.skeleton = unreal.load_asset(ZIS_SKELETON)
    data = options.skeletal_mesh_import_data
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    _task(f'{SOURCE}/SK_TW_BS3.fbx', f'{ROOT}/Mesh', 'SK_TW_BS3', options)
    mesh = unreal.load_asset(f'{ROOT}/Mesh/SK_TW_BS3')
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        entry.set_editor_property('material_interface', unreal.load_asset(f'{ROOT}/Materials/MI_BS3'))
    mesh.set_editor_property('materials', materials)
    eal.save_loaded_asset(mesh)
    print('BS3 mesh skeleton', mesh.get_editor_property('skeleton').get_name())


def import_anims():
    skeleton = unreal.load_asset(ZIS_SKELETON)
    deployed = None
    for name, (folder, zis_twin) in ANIMS.items():
        asset = f'Anim_BS3_{name}'
        path = f'{ROOT}/{folder}/{asset}'
        # A reimport cannot change the length of an existing sequence; these assets are generated only.
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
        _task(f'{SOURCE}/Anims/{asset}.fbx', f'{ROOT}/{folder}', asset, options)
        sequence = unreal.load_asset(path)
        if name == 'Deployed':
            deployed = sequence
        if zis_twin:
            _copy_additive_settings(sequence, unreal.load_asset(f'{ZIS}/{zis_twin}'))
        eal.save_loaded_asset(sequence)
        print('BS3 anim', asset, round(sequence.get_play_length(), 2), sequence.get_editor_property('additive_anim_type'))


def _copy_additive_settings(sequence, zis_sequence):
    """Same additive type and base as the ZiS-3's twin; a ZiS-3 base sequence is swapped for the BS-3's own."""
    sequence.set_editor_property('additive_anim_type', zis_sequence.get_editor_property('additive_anim_type'))
    sequence.set_editor_property('ref_pose_type', zis_sequence.get_editor_property('ref_pose_type'))
    sequence.set_editor_property('ref_frame_index', zis_sequence.get_editor_property('ref_frame_index'))
    zis_ref = zis_sequence.get_editor_property('ref_pose_seq')
    if zis_ref:
        twin = zis_ref.get_name().replace('Anim_Zis3_', 'Anim_BS3_')
        folder = ANIMS[twin.replace('Anim_BS3_', '')][0]
        sequence.set_editor_property('ref_pose_seq', unreal.load_asset(f'{ROOT}/{folder}/{twin}'))


def import_wreck():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property('combine_meshes', True)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', True)
    _task(f'{SOURCE}/SM_Destroyed_BS3.fbx', f'{ROOT}/Mesh', 'SM_Destroyed_BS3', options)
    path = f'{ROOT}/Mesh/SM_Destroyed_BS3'
    mesh = unreal.load_asset(path)
    for index in range(len(mesh.static_materials)):
        mesh.set_material(index, unreal.load_asset(f'{ROOT}/Materials/MI_BS3_Burnt'))
    print('BS3 wreck saved', eal.save_asset(path, only_if_is_dirty=False))


def _component_transform(sequence, bone, parents):
    transform = unreal.AnimationLibrary.get_bone_pose_for_time(sequence, bone, 0.0, False)
    parent = parents.get(bone)
    while parent:
        transform = transform.multiply(unreal.AnimationLibrary.get_bone_pose_for_time(sequence, parent, 0.0, False))
        parent = parents.get(parent)
    return transform


def print_socket_transforms():
    """LaunchBS3 (muzzle) and OutBS3 (breech) on B_Gun, bone-relative, for the bridge's set_skeleton_socket_transform."""
    info = json.load(open(INFO_JSON))
    sequence = unreal.load_asset(f'{ROOT}/DeployPacked/Anim_BS3_Deployed')
    parents = {'Wheels': '37mmFlak', 'B_Root': 'Wheels', 'B_Turret': 'B_Root', 'B_Mantlet': 'B_Turret', 'B_Gun': 'B_Mantlet'}
    result = {}
    for socket, key in (('LaunchBS3', 'muzzle'), ('OutBS3', 'breech')):
        point = info[key]
        bone_component = _component_transform(sequence, 'B_Gun', parents)
        desired = unreal.Transform(unreal.Vector(point[0], -point[1], point[2]), unreal.Rotator(0.0, 0.0, 0.0),
                                   unreal.Vector(1.0, 1.0, 1.0))
        relative = desired.multiply(bone_component.inverse())
        rotator = relative.rotation.rotator()
        result[socket] = {'relative_location': [relative.translation.x, relative.translation.y, relative.translation.z],
                          'relative_rotation': [rotator.pitch, rotator.yaw, rotator.roll]}
    print('BS3_SOCKETS ' + json.dumps(result))
    return result


_steps = globals().get('STEPS') or ['materials', 'mesh', 'anims', 'wreck', 'sockets']
if 'materials' in _steps:
    make_materials()
if 'mesh' in _steps:
    import_mesh()
if 'anims' in _steps:
    import_anims()
if 'wreck' in _steps:
    import_wreck()
if 'sockets' in _steps:
    print_socket_transforms()
