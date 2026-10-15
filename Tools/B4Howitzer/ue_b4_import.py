"""Imports the B-4 203mm team weapon (built by b4_build_team_weapon.py in Blender) into the editor.

Run inside the editor:  import ue_b4_import; ue_b4_import.run_all()
Steps can also be run one at a time (import_textures, make_materials, import_mesh, import_lods, import_anims,
setup_aim_poses). Montages, the aim offset, sockets and the anim blueprint are made with the editor MCP actions;
see Docs/Units/B4Howitzer.md.
"""
import os

import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/B4_203mm'
ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM'
MESH_DIR = ROOT + '/Mesh'
MATERIAL_DIR = ROOT + '/Materials'
WEAPON_ANIM_DIR = ROOT + '/WeaponAnims'
PACK_DIR = ROOT + '/PackDeploy'
WHEEL_DIR = ROOT + '/WheelAnims'
MESH_NAME = 'SK_TW_B4_Howitzer'
MASTER_MATERIAL = '/Game/RTS_Survival/Meshes/Military/M_MasterPainter1'
MATERIAL_SETS = ['Gun', 'Body', 'Wheels', 'Trails', 'Trailer']

AIM_POSES = ['Base', 'Up55', 'Down2', 'Left8', 'Left8Up55', 'Left8Down2', 'Right8', 'Right8Up55', 'Right8Down2']
ANIM_FOLDERS = {
    'Fire': WEAPON_ANIM_DIR,
    'Deployed': PACK_DIR,
    'Packed': PACK_DIR,
    'DeployedToPacked': PACK_DIR,
    'WheelsFWD': WHEEL_DIR,
}
for pose in AIM_POSES:
    ANIM_FOLDERS[pose] = WEAPON_ANIM_DIR

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _run_import(task):
    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths'))
    if not paths:
        raise RuntimeError('Import produced nothing for ' + task.filename)
    return paths


def _make_task(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options is not None:
        task.options = options
    return task


def import_textures():
    textures = {}
    for material_set in MATERIAL_SETS:
        for suffix, file_suffix in (('D', 'D'), ('ORM', 'ORMs')):
            name = f'T_B4_{material_set}_{suffix}'
            filename = os.path.join(EXPORT_DIR, 'Textures', f'T_B4_{material_set}_{file_suffix}.png')
            path = _run_import(_make_task(filename, MATERIAL_DIR, name))[0]
            texture = unreal.load_asset(path)
            # The ORM is stored sRGB-encoded because M_MasterPainter1 samples it with a Color sampler.
            texture.set_editor_property('srgb', True)
            eal.save_loaded_asset(texture)
            textures[name] = texture
    return textures


def make_materials():
    master = unreal.load_asset(MASTER_MATERIAL)
    instances = {}
    for material_set in MATERIAL_SETS:
        name = f'MI_B4_{material_set}'
        path = f'{MATERIAL_DIR}/{name}'
        if eal.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = asset_tools.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                                                unreal.MaterialInstanceConstantFactoryNew())
        instance.set_editor_property('parent', master)
        albedo = unreal.load_asset(f'{MATERIAL_DIR}/T_B4_{material_set}_D')
        orm = unreal.load_asset(f'{MATERIAL_DIR}/T_B4_{material_set}_ORM')
        mel.set_material_instance_texture_parameter_value(instance, 'albedoTexture', albedo)
        mel.set_material_instance_texture_parameter_value(instance, 'AORoughnessMetallicTexture', orm)
        for switch, value in (('albedoTextureEnabled', True), ('occlusionTexEnabled', True),
                              ('RoughnessTexEnabled', True), ('metallicTexEnabled', True),
                              ('normalMapTexEnabled', False), ('detailNormalEnabled', False),
                              ('emissionEnabled', False)):
            mel.set_material_instance_static_switch_parameter_value(instance, switch, value)
        mel.set_material_instance_vector_parameter_value(instance, 'ColorCorrection', unreal.LinearColor(1, 1, 1, 0))
        mel.set_material_instance_scalar_parameter_value(instance, 'Roughness Blend', 0.0)
        mel.set_material_instance_scalar_parameter_value(instance, 'metallicValueMultiplier', 1.0)
        mel.update_material_instance(instance)
        eal.save_loaded_asset(instance)
        instances[name] = instance
    return instances


def _skeletal_options(skeleton=None):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = skeleton is None
    if skeleton is not None:
        options.skeleton = skeleton
    data = options.skeletal_mesh_import_data
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def import_mesh():
    filename = os.path.join(EXPORT_DIR, f'{MESH_NAME}.fbx')
    paths = _run_import(_make_task(filename, MESH_DIR, MESH_NAME, _skeletal_options()))
    mesh = unreal.load_asset(f'{MESH_DIR}/{MESH_NAME}')
    assign_materials(mesh)
    return paths


def assign_materials(mesh):
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        slot = str(entry.get_editor_property('material_slot_name'))
        set_name = slot.replace('MI_B4_', '')
        instance = unreal.load_asset(f'{MATERIAL_DIR}/MI_B4_{set_name}')
        if instance:
            entry.set_editor_property('material_interface', instance)
    mesh.set_editor_property('materials', materials)
    eal.save_loaded_asset(mesh)


def import_lods():
    mesh = unreal.load_asset(f'{MESH_DIR}/{MESH_NAME}')
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for lod_index in (1, 2):
        filename = os.path.join(EXPORT_DIR, f'{MESH_NAME}_LOD{lod_index}.fbx')
        result = subsystem.import_lod(mesh, lod_index, filename)
        unreal.log(f'B4 LOD{lod_index} import -> {result}')
    eal.save_loaded_asset(mesh)


def _anim_options(skeleton):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    anim_data = options.anim_sequence_import_data
    anim_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim_data.set_editor_property('remove_redundant_keys', False)
    anim_data.set_editor_property('import_bone_tracks', True)
    return options


def import_anims():
    mesh = unreal.load_asset(f'{MESH_DIR}/{MESH_NAME}')
    skeleton = mesh.get_editor_property('skeleton')
    imported = {}
    for anim_name, folder in ANIM_FOLDERS.items():
        asset_name = f'Anim_B4_{anim_name}'
        filename = os.path.join(EXPORT_DIR, 'Anims', f'{asset_name}.fbx')
        # A reimport cannot change the sample rate of an existing sequence; these assets are generated only.
        if eal.does_asset_exist(f'{folder}/{asset_name}'):
            eal.delete_asset(f'{folder}/{asset_name}')
        paths = _run_import(_make_task(filename, folder, asset_name, _anim_options(skeleton)))
        imported[asset_name] = paths
    return imported


def setup_aim_poses():
    """Aim offset samples are mesh-space rotation offsets on top of the deployed pose, like the M1938."""
    deployed = unreal.load_asset(f'{PACK_DIR}/Anim_B4_Deployed')
    for pose in AIM_POSES:
        sequence = unreal.load_asset(f'{WEAPON_ANIM_DIR}/Anim_B4_{pose}')
        sequence.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE)
        sequence.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
        sequence.set_editor_property('ref_pose_seq', deployed)
        sequence.set_editor_property('ref_frame_index', 0)
        eal.save_loaded_asset(sequence)
    wheels = unreal.load_asset(f'{WHEEL_DIR}/Anim_B4_WheelsFWD')
    wheels.set_editor_property('loop', True) if hasattr(wheels, 'loop') else None
    eal.save_loaded_asset(wheels)


def delete_generated_assets():
    """Removes the imported mesh, skeleton and animation assets so a rebuilt rig imports clean (materials stay)."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in (MESH_DIR, WEAPON_ANIM_DIR, PACK_DIR, WHEEL_DIR):
        for data in registry.get_assets_by_path(folder, recursive=True):
            path = str(data.package_name)
            if eal.does_asset_exist(path):
                eal.delete_asset(path)


def run_all():
    import_textures()
    make_materials()
    import_mesh()
    import_lods()
    import_anims()
    setup_aim_poses()
    unreal.log('B4 import done')
