"""Imports the Flakvierling bunker's meshes, textures and materials (run with the UECP execute_python action).

Blender source and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
FlakvierlingBunker/Model/README.md. Unit notes: Source/Docs/Units/GerFlakvierlingBunker.md.

Also the 50 % health meshes, SM_BF_FlakvierlingBunker_Damaged and SM_BF_FlakvierlingBunker_Sandbags_Damaged (same pivot,
sockets and UCX; the Blueprint swaps them in at half health). The ammunition mesh is shared by both states.

Safe to rerun after a Blender rebuild: meshes reimport over the same assets, materials are only made once.
"""
import unreal

BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/'
BUNKERS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/'
SHARED = BUNKERS + 'BreakthroughShared'
HOME = BUNKERS + 'FlakvierlingBunker'
MAIN = 'SM_BF_FlakvierlingBunker'
SOCKETS = ('Turret', 'MG', 'Exp1', 'Exp2', 'Exp3')
# Trench anchors, Unreal cm and yaw, +X out of the bunker (Source/Docs/Units/GerPitTrenchAnchors.md). Passage: the
# trench ends in front of the crew passage between the Ringstand and the wedge, clear of the ring. Rear: against the
# closed back wall; there is no opening there yet.
TRENCH_PORTS = (('TrenchPort_Passage', (260.0, -80.0, 0.0), 0.0), ('TrenchPort_Rear', (-390.0, -150.0, 0.0), 180.0))
TEXTURES = (
    ('SharedParts/Textures/T_BF_Trim_BaseColor.png', 'color'),
    ('SharedParts/Textures/T_BF_Trim_ORM.png', 'mask'),
    ('SharedParts/Textures/T_BF_Trim_NormalDX.png', 'normal'),
    ('AssetsForFactionIdentity/CamoNet/Textures/T_CamoNet_B_Rotbraun_BC.png', 'color'),
    ('AssetsForFactionIdentity/CamoNet/Textures/T_CamoNet_B_Rotbraun_ORM.png', 'mask'),
    ('AssetsForFactionIdentity/CamoNet/Textures/T_CamoNet_B_Rotbraun_N_DX.png', 'normal'),
    ('AssetsForFactionIdentity/FieldSupply/T_BF_FieldSupply_BaseColor.png', 'color'),
    ('AssetsForFactionIdentity/FieldSupply/T_BF_FieldSupply_ORM.png', 'mask'),
    ('AssetsForFactionIdentity/FieldSupply/T_BF_FieldSupply_NormalDX.png', 'normal'),
)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def import_textures():
    tasks = []
    for path, _ in TEXTURES:
        task = unreal.AssetImportTask()
        task.filename = BLEND + path
        task.destination_path = SHARED + '/Textures'
        task.automated = True
        task.save = False  # AssetImportTask(save=True) silently misses the disk here
        task.replace_existing = True
        tasks.append(task)
    ASSET_TOOLS.import_asset_tasks(tasks)
    for (_, kind), task in zip(TEXTURES, tasks):
        path = list(task.imported_object_paths)[0]
        texture = unreal.load_asset(path)
        if kind != 'color':
            texture.set_editor_property('srgb', False)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS if kind == 'mask'
                                        else unreal.TextureCompressionSettings.TC_NORMALMAP)
        ASSETS.save_asset(path.split('.')[0], only_if_is_dirty=False)


def import_mesh(name):
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = f'{BLEND}FlakvierlingBunker/Model/{name}.FBX'
    task.destination_path = HOME + '/Mesh'
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])


def texture_parameter(material, name, texture, sampler, y):
    node = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('texture', texture)
    node.set_editor_property('sampler_type', sampler)
    return node


def pbr_material(name, base, orm, normal, masked=False):
    path = f'{SHARED}/{name}'
    if ASSETS.does_asset_exist(path):
        return unreal.load_asset(path)
    material = ASSET_TOOLS.create_asset(name, SHARED, unreal.Material, unreal.MaterialFactoryNew())
    load = lambda texture: unreal.load_asset(f'{SHARED}/Textures/{texture}')
    base_node = texture_parameter(material, 'BaseColor', load(base), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -200)
    orm_node = texture_parameter(material, 'ORM', load(orm), unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 100)
    normal_node = texture_parameter(material, 'Normal', load(normal), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 400)
    EDIT.connect_material_property(base_node, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(orm_node, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    EDIT.connect_material_property(orm_node, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(orm_node, 'B', unreal.MaterialProperty.MP_METALLIC)
    EDIT.connect_material_property(normal_node, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    if masked:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('opacity_mask_clip_value', 0.4)
        EDIT.connect_material_property(base_node, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    EDIT.recompile_material(material)
    ASSETS.save_asset(path, only_if_is_dirty=False)
    return material


def concrete_instance():
    """The faction casting look at tiling 1: this mesh's UVs already hold the 4 m tile."""
    path = SHARED + '/MI_BF_SootedFortress_UVTile'
    if ASSETS.does_asset_exist(path):
        return unreal.load_asset(path)
    source = unreal.load_asset(BUNKERS + 'FlakBunker/Material/MI_BF_SootedFortress_Casting')
    instance = ASSET_TOOLS.create_asset('MI_BF_SootedFortress_UVTile', SHARED, unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(instance, source.get_editor_property('parent'))
    for value in source.get_editor_property('scalar_parameter_values'):
        name = value.get_editor_property('parameter_info').get_editor_property('name')
        EDIT.set_material_instance_scalar_parameter_value(
            instance, name, 1.0 if str(name) == 'BF_Tiling' else value.get_editor_property('parameter_value'))
    EDIT.update_material_instance(instance)
    ASSETS.save_asset(path, only_if_is_dirty=False)
    return instance


def set_trench_ports(mesh):
    """Adds the trench anchors (UFieldTrenchAnchorComponent) or puts them back where they belong."""
    for socket_name, location, yaw in TRENCH_PORTS:
        socket = mesh.find_socket(socket_name)
        if not socket:
            socket = unreal.StaticMeshSocket(mesh)
            socket.set_editor_property('socket_name', socket_name)
            mesh.add_socket(socket)
        socket.set_editor_property('relative_location', unreal.Vector(*location))
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))


def finish_mesh(name, materials):
    path = f'{HOME}/Mesh/{name}'
    mesh = unreal.load_asset(path)
    for index, material in enumerate(materials):
        mesh.set_material(index, material)
    for socket_name in SOCKETS:
        socket = mesh.find_socket(socket_name)
        if socket:      # FBX empties arrive tilted; every socket here is a plain position
            socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
    if name in (MAIN, MAIN + '_Damaged'):
        set_trench_ports(mesh)
    ASSETS.save_asset(path, only_if_is_dirty=False)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(name, 'convex hulls', hulls)


def main():
    import_textures()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in (MAIN, MAIN + '_Sandbags', MAIN + '_Ammo', MAIN + '_Damaged', MAIN + '_Sandbags_Damaged'):
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    trim = pbr_material('M_BF_Trim', 'T_BF_Trim_BaseColor', 'T_BF_Trim_ORM', 'T_BF_Trim_NormalDX')
    net = pbr_material('M_BF_CamoNet', 'T_CamoNet_B_Rotbraun_BC', 'T_CamoNet_B_Rotbraun_ORM', 'T_CamoNet_B_Rotbraun_N_DX', masked=True)
    field = pbr_material('M_BF_FieldSupply', 'T_BF_FieldSupply_BaseColor', 'T_BF_FieldSupply_ORM', 'T_BF_FieldSupply_NormalDX')
    bags = unreal.load_asset('/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas')
    finish_mesh(MAIN, [concrete_instance(), trim, net])
    finish_mesh(MAIN + '_Sandbags', [bags])
    finish_mesh(MAIN + '_Ammo', [field])
    finish_mesh(MAIN + '_Damaged', [concrete_instance(), trim, net])
    finish_mesh(MAIN + '_Sandbags_Damaged', [bags])


main()
