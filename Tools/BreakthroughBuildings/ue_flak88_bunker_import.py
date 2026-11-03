"""Imports the Breakthrough Flak 88 bunker's meshes (run with the UECP execute_python action).

Blender source, socket table and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
Flak88Bunker/Model/README.md. Unit notes: Source/Docs/Units/GerFlak88Bunker.md.

Three meshes: the intact bunker, its 50 % health version (same pivot, sockets and UCX; the Blueprint swaps them) and the
88 mm rounds both share. The rounds get the faction's shared ammo material, M_Ammo_FactionShared, made here once.

Safe to rerun after a Blender rebuild: meshes reimport over the same assets, materials are only made once.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/Flak88Bunker/Model/'
AMMO_TEXTURES = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/AssetsForFactionIdentity/Ammo/'
BUNKERS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/'
SHARED = BUNKERS + 'BreakthroughShared'
HOME = BUNKERS + 'Flak88Bunker/Mesh'
MAIN = 'SM_BF_Flak88Bunker'
DAMAGED = MAIN + '_Damaged'
AMMO = MAIN + '_Ammo'
SOCKETS = ('Turret', 'Exp1', 'Exp2', 'Exp3')
TEXTURES = (('T_Ammo_Shared_BaseColor.png', 'color'), ('T_Ammo_Shared_ORM.png', 'mask'), ('T_Ammo_Shared_NormalDX.png', 'normal'))
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def save(path):
    if not ASSETS.save_asset(path, only_if_is_dirty=False):
        print('SAVE FAILED', path)


def import_textures():
    tasks = []
    for name, _ in TEXTURES:
        if ASSETS.does_asset_exist(f'{SHARED}/Textures/{name[:-4]}'):
            continue
        task = unreal.AssetImportTask()
        task.filename = AMMO_TEXTURES + name
        task.destination_path = SHARED + '/Textures'
        task.automated = True
        task.save = False  # AssetImportTask(save=True) silently misses the disk here
        task.replace_existing = True
        tasks.append((name, task))
    ASSET_TOOLS.import_asset_tasks([t for _, t in tasks])
    kinds = dict(TEXTURES)
    for name, task in tasks:
        path = list(task.imported_object_paths)[0]
        texture = unreal.load_asset(path)
        if kinds[name] != 'color':
            texture.set_editor_property('srgb', False)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS if kinds[name] == 'mask'
                                        else unreal.TextureCompressionSettings.TC_NORMALMAP)
        save(path.split('.')[0])


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
    task.filename = f'{MODEL}{name}.FBX'
    task.destination_path = HOME
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


def ammo_material():
    """The faction's shared ammo atlas (T_Ammo_Shared_*), ORM packed as AO, roughness, metallic."""
    path = f'{SHARED}/M_Ammo_FactionShared'
    if ASSETS.does_asset_exist(path):
        return unreal.load_asset(path)
    material = ASSET_TOOLS.create_asset('M_Ammo_FactionShared', SHARED, unreal.Material, unreal.MaterialFactoryNew())
    load = lambda texture: unreal.load_asset(f'{SHARED}/Textures/{texture}')
    base = texture_parameter(material, 'BaseColor', load('T_Ammo_Shared_BaseColor'), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -200)
    orm = texture_parameter(material, 'ORM', load('T_Ammo_Shared_ORM'), unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 100)
    normal = texture_parameter(material, 'Normal', load('T_Ammo_Shared_NormalDX'), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 400)
    EDIT.connect_material_property(base, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(orm, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    EDIT.connect_material_property(orm, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(orm, 'B', unreal.MaterialProperty.MP_METALLIC)
    EDIT.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    EDIT.recompile_material(material)
    save(path)
    return material


def finish_mesh(name, materials):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    for index, material in enumerate(materials):
        mesh.set_material(index, material)
    for socket_name in SOCKETS:
        socket = mesh.find_socket(socket_name)
        if socket:      # FBX empties arrive tilted; every socket here is a plain position
            socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
    save(path)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    found = [s for s in SOCKETS if mesh.find_socket(s)]
    print(name, 'convex hulls', hulls, 'sockets', found, 'triangles', mesh.get_num_triangles(0))


def main():
    import_textures()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in (MAIN, DAMAGED, AMMO):
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    concrete = unreal.load_asset(SHARED + '/MI_BF_SootedFortress_UVTile')
    trim, net = unreal.load_asset(SHARED + '/M_BF_Trim'), unreal.load_asset(SHARED + '/M_BF_CamoNet')
    finish_mesh(MAIN, [concrete, trim, net])
    finish_mesh(DAMAGED, [concrete, trim, net])
    finish_mesh(AMMO, [ammo_material()])


main()
