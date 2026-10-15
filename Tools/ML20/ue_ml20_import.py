"""Imports the ML-20 152mm gun-howitzer team weapon (ml20_prepare.py and ml20_build_team_weapon.py in Blender).

Run inside the editor (bridge pyfile or the editor's Python console); set STEPS to a list of step names to run only
some of them (textures, materials, mesh, anims, aim_poses). Same layout and material setup as the B-4
(Tools/B4Howitzer/ue_b4_import.py): painted textures on M_MasterPainter1, aim poses additive on the deployed pose.
"""
import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/ML20_152mm'
ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM'
MESH_DIR = ROOT + '/Mesh'
MATERIAL_DIR = ROOT + '/Materials'
WEAPON_ANIM_DIR = ROOT + '/WeaponAnims'
PACK_DIR = ROOT + '/PackDeploy'
WHEEL_DIR = ROOT + '/WheelAnims'
MESH_NAME = 'SK_TW_ML20'
MASTER_MATERIAL = '/Game/RTS_Survival/Meshes/Military/M_MasterPainter1'
MATERIAL_SETS = ['Gun', 'Carriage', 'Wheels', 'Tyres']

AIM_POSES = ['Base', 'Up60', 'Down2', 'Left15', 'Left15Up60', 'Left15Down2', 'Right15', 'Right15Up60', 'Right15Down2']
ANIM_FOLDERS = {'Fire': WEAPON_ANIM_DIR, 'Deployed': PACK_DIR, 'Packed': PACK_DIR, 'DeployedToPacked': PACK_DIR,
                'WheelsFWD': WHEEL_DIR}
for _pose in AIM_POSES:
    ANIM_FOLDERS[_pose] = WEAPON_ANIM_DIR

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
    for material_set in MATERIAL_SETS:
        for suffix in ('D', 'ORM'):
            name = f'T_ML20_{material_set}_{suffix}'
            path = _run_import(_make_task(f'{EXPORT_DIR}/Textures/{name}.png', MATERIAL_DIR, name))[0]
            texture = unreal.load_asset(path)
            # Like the B-4: M_MasterPainter1 samples the ORM with a Color sampler, so it stays sRGB-encoded.
            texture.set_editor_property('srgb', True)
            eal.save_loaded_asset(texture)
    print('ML20_IMPORT textures done')


def make_materials():
    master = unreal.load_asset(MASTER_MATERIAL)
    for material_set in MATERIAL_SETS:
        name = f'MI_ML20_{material_set}'
        path = f'{MATERIAL_DIR}/{name}'
        if eal.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = asset_tools.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                                                unreal.MaterialInstanceConstantFactoryNew())
        instance.set_editor_property('parent', master)
        mel.set_material_instance_texture_parameter_value(instance, 'albedoTexture',
                                                          unreal.load_asset(f'{MATERIAL_DIR}/T_ML20_{material_set}_D'))
        mel.set_material_instance_texture_parameter_value(instance, 'AORoughnessMetallicTexture',
                                                          unreal.load_asset(f'{MATERIAL_DIR}/T_ML20_{material_set}_ORM'))
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
    print('ML20_IMPORT materials done')


def _skeletal_options(skeleton=None):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    # A reimport keeps the skeleton (new bones merge into it, like the B-4 rig gaining its pit bones).
    options.create_physics_asset = skeleton is None
    if skeleton is not None:
        options.skeleton = skeleton
    data = options.skeletal_mesh_import_data
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def import_mesh():
    existing = f'{MESH_DIR}/{MESH_NAME}'
    skeleton = unreal.load_asset(existing).get_editor_property('skeleton') if eal.does_asset_exist(existing) else None
    _run_import(_make_task(f'{EXPORT_DIR}/{MESH_NAME}.fbx', MESH_DIR, MESH_NAME, _skeletal_options(skeleton)))
    mesh = unreal.load_asset(f'{MESH_DIR}/{MESH_NAME}')
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        slot = str(entry.get_editor_property('material_slot_name'))
        instance = unreal.load_asset(f'{MATERIAL_DIR}/{slot}')
        if instance:
            entry.set_editor_property('material_interface', instance)
    mesh.set_editor_property('materials', materials)
    eal.save_loaded_asset(mesh)
    print('ML20_IMPORT mesh slots', [str(m.get_editor_property('material_slot_name')) for m in materials])


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


def import_anims():
    skeleton = unreal.load_asset(f'{MESH_DIR}/{MESH_NAME}').get_editor_property('skeleton')
    for anim_name, folder in ANIM_FOLDERS.items():
        asset_name = f'Anim_ML20_{anim_name}'
        # A reimport cannot change the length of an existing sequence; these assets are generated only.
        if eal.does_asset_exist(f'{folder}/{asset_name}'):
            eal.delete_asset(f'{folder}/{asset_name}')
        _run_import(_make_task(f'{EXPORT_DIR}/Anims/{asset_name}.fbx', folder, asset_name, _anim_options(skeleton)))
        print('ML20_IMPORT', asset_name, round(unreal.load_asset(f'{folder}/{asset_name}').get_play_length(), 2))


def setup_aim_poses():
    """Aim offset samples are mesh-space rotation offsets on top of the deployed pose, like the B-4 and M1938."""
    deployed = unreal.load_asset(f'{PACK_DIR}/Anim_ML20_Deployed')
    for pose in AIM_POSES:
        sequence = unreal.load_asset(f'{WEAPON_ANIM_DIR}/Anim_ML20_{pose}')
        sequence.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE)
        sequence.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
        sequence.set_editor_property('ref_pose_seq', deployed)
        sequence.set_editor_property('ref_frame_index', 0)
        eal.save_loaded_asset(sequence)
    print('ML20_IMPORT aim poses additive')


STEP_FUNCTIONS = {'textures': import_textures, 'materials': make_materials, 'mesh': import_mesh,
                  'anims': import_anims, 'aim_poses': setup_aim_poses}
for _step in (globals().get('STEPS') or list(STEP_FUNCTIONS)):
    STEP_FUNCTIONS[_step]()
