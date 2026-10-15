"""Imports the ML-20 meshes after ml20_fix_wheels_atlas.py: upright tyres and one atlased material per mesh.

Run inside the editor (bridge pyfile or the editor's Python console). Reimports, on the existing skeleton (no bone
changes, so every animation and anim blueprint keeps working):
  Mesh/SK_TW_ML20, Pit/Meshes/SK_ML20Pit_Gun   one slot, MI_ML20_Atlas
  Mesh/SM_ML20_Destroyed                       one slot, MI_ML20_Atlas_Burnt
and makes the atlas textures and the two material instances. Blueprints that override a mesh's materials are
reported and their overrides cleared, so the mesh's own single slot shows. The old per-set materials stay on disk
(the B-4 still uses its own); nothing references the ML-20 ones afterwards.
"""
import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/ML20_152mm'
ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM'
MATERIAL_DIR = ROOT + '/Materials'
MASTER_MATERIAL = '/Game/RTS_Survival/Meshes/Military/M_MasterPainter1'
ATLAS = 'MI_ML20_Atlas'
ATLAS_BURNT = 'MI_ML20_Atlas_Burnt'
# Burnt look of the wreck, as MI_ML20_*_Burnt.
BURNT_COLOUR = unreal.LinearColor(0.22, 0.2, 0.19, 0.0)
BURNT_ROUGHNESS_BLEND = 0.6
SKELETAL_MESHES = {'SK_TW_ML20': ROOT + '/Mesh', 'SK_ML20Pit_Gun': ROOT + '/Pit/Meshes'}
WRECK = 'SM_ML20_Destroyed'
BLUEPRINTS = [ROOT + '/BP_TW_ML20', ROOT + '/DestroyedTW_ML20', ROOT + '/Pit/RTS_BP_EmbTurret_ML20Pit',
              ROOT + '/Pit/Rus_BXP_ML20Pit']
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _import(filename, destination, name, options=None):
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
    return paths[0]


def _instance(name, parent):
    path = f'{MATERIAL_DIR}/{name}'
    instance = unreal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
        name, MATERIAL_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property('parent', parent)
    return instance


def make_materials():
    for suffix in ('D', 'ORM'):
        name = f'T_ML20_Atlas_{suffix}'
        texture = unreal.load_asset(_import(f'{EXPORT_DIR}/Textures/{name}.png', MATERIAL_DIR, name))
        # Like the per-set textures: M_MasterPainter1 samples the ORM with a Color sampler.
        texture.set_editor_property('srgb', True)
        eal.save_loaded_asset(texture)
    atlas = _instance(ATLAS, unreal.load_asset(MASTER_MATERIAL))
    mel.set_material_instance_texture_parameter_value(atlas, 'albedoTexture', unreal.load_asset(f'{MATERIAL_DIR}/T_ML20_Atlas_D'))
    mel.set_material_instance_texture_parameter_value(atlas, 'AORoughnessMetallicTexture',
                                                      unreal.load_asset(f'{MATERIAL_DIR}/T_ML20_Atlas_ORM'))
    for switch, value in (('albedoTextureEnabled', True), ('occlusionTexEnabled', True), ('RoughnessTexEnabled', True),
                          ('metallicTexEnabled', True), ('normalMapTexEnabled', False), ('detailNormalEnabled', False),
                          ('emissionEnabled', False)):
        mel.set_material_instance_static_switch_parameter_value(atlas, switch, value)
    mel.set_material_instance_vector_parameter_value(atlas, 'ColorCorrection', unreal.LinearColor(1, 1, 1, 0))
    mel.set_material_instance_scalar_parameter_value(atlas, 'Roughness Blend', 0.0)
    mel.set_material_instance_scalar_parameter_value(atlas, 'metallicValueMultiplier', 1.0)
    mel.update_material_instance(atlas)
    eal.save_loaded_asset(atlas)
    burnt = _instance(ATLAS_BURNT, atlas)
    mel.set_material_instance_vector_parameter_value(burnt, 'ColorCorrection', BURNT_COLOUR)
    mel.set_material_instance_scalar_parameter_value(burnt, 'Roughness Blend', BURNT_ROUGHNESS_BLEND)
    mel.update_material_instance(burnt)
    eal.save_loaded_asset(burnt)
    print('ML20_ATLAS materials done')


def import_skeletal_meshes():
    atlas = unreal.load_asset(f'{MATERIAL_DIR}/{ATLAS}')
    for name, folder in SKELETAL_MESHES.items():
        existing = unreal.load_asset(f'{folder}/{name}')
        skeleton = existing.get_editor_property('skeleton')
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        options.import_mesh = True
        options.import_as_skeletal = True
        options.import_animations = False
        options.import_materials = False
        options.import_textures = False
        options.create_physics_asset = False
        options.skeleton = skeleton
        data = options.skeletal_mesh_import_data
        data.set_editor_property('import_morph_targets', False)
        data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        _import(f'{EXPORT_DIR}/{name}.fbx', folder, name, options)
        mesh = unreal.load_asset(f'{folder}/{name}')
        materials = mesh.get_editor_property('materials')
        for entry in materials:
            entry.set_editor_property('material_interface', atlas)
        mesh.set_editor_property('materials', materials)
        eal.save_loaded_asset(mesh)
        print('ML20_ATLAS', name, 'slots', [str(m.get_editor_property('material_slot_name')) for m in materials],
              'skeleton', mesh.get_editor_property('skeleton').get_name())


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
    options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
    _import(f'{EXPORT_DIR}/{WRECK}.fbx', ROOT + '/Mesh', WRECK, options)
    mesh = unreal.load_asset(f'{ROOT}/Mesh/{WRECK}')
    burnt = unreal.load_asset(f'{MATERIAL_DIR}/{ATLAS_BURNT}')
    for index in range(len(mesh.static_materials)):
        mesh.set_material(index, burnt)
    eal.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
    print('ML20_ATLAS wreck slots', [(str(m.material_slot_name), m.material_interface.get_name()) for m in mesh.static_materials])


def clear_blueprint_overrides():
    """Components that override the ML-20 meshes' materials would hide the single atlas slot."""
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for path in BLUEPRINTS:
        if not eal.does_asset_exist(path):
            continue
        bp = unreal.load_asset(path)
        changed = False
        for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
            component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            if not isinstance(component, unreal.MeshComponent):
                continue
            overrides = component.get_editor_property('override_materials')
            if overrides and any(overrides):
                print('ML20_ATLAS override', path.split('/')[-1], component.get_name(), [m.get_name() if m else None for m in overrides])
                mesh = component.get_editor_property('skeletal_mesh_asset') if isinstance(component, unreal.SkeletalMeshComponent) \
                    else component.get_editor_property('static_mesh') if isinstance(component, unreal.StaticMeshComponent) else None
                if mesh and 'ML20' in mesh.get_name():
                    component.set_editor_property('override_materials', [])
                    changed = True
        if changed:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            eal.save_loaded_asset(bp)
            print('ML20_ATLAS cleared overrides in', path.split('/')[-1])
    print('ML20_ATLAS override check done')


STEP_FUNCTIONS = {'materials': make_materials, 'meshes': import_skeletal_meshes, 'wreck': import_wreck,
                  'overrides': clear_blueprint_overrides}
for _step in (globals().get('STEPS') or list(STEP_FUNCTIONS)):
    STEP_FUNCTIONS[_step]()
