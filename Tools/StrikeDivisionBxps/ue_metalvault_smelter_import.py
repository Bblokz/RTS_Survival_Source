"""Imports the Strike Division Metal Vault smelter (A1 Turmkern) meshes and its two new materials (run with the UECP
execute_python action).

Blender source, slots and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/StrikeDivisionBXPs/
MetalVault_Smelter/Model/README.md. The four meshes follow the Barracks fuel cell's pattern: the finished mesh, its
50 % health swap, the placement preview (MI_ConstructionPrev) and a box mesh that is the complex collision of the other
three (complex-as-simple), Nanite off. The FBX is authored in the vault mesh's own frame, so the pivot is the building
origin.

Safe to rerun after a Blender rebuild: meshes reimport over the same assets, textures and materials are only made once.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/StrikeDivisionBXPs/MetalVault_Smelter/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/MetalVault/Smelter'
MATS = HOME + '/Materials'
MAIN = 'SM_GerMetalVault_Smelter'
DAMAGED = MAIN + '_At50'
PREVIEW = MAIN + '_PreviewMesh'
COLLISION = MAIN + '_Collision'
SOCKETS = ('StackSmoke', 'TapGlow')
VAULT_MATERIALS = {
    'M_TowerFrame_MetalVault': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/'
                               'Opel_MetalVault/Materials/M_TowerFrame_MetalVault',
    'M_PantherII_Sheets': '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherII/Textures/'
                          'M_PantherII_Sheets',
    'M_GerSandBagScaffold_Atlas': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
    'M_RTS_MetalResource': '/Game/RTS_Survival/Blueprints/Resources/Metal/Meshes/Materials/M_RTS_MetalResource',
}
# Blender slot order (sm_kit.SLOTS); the last two are made here.
SLOT_ORDER = ('M_TowerFrame_MetalVault', 'M_PantherII_Sheets', 'M_GerSandBagScaffold_Atlas', 'M_RTS_MetalResource',
              'M_SD_Firebrick', 'M_SD_SmelterMelt')
PREVIEW_MATERIAL = '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev'
TEXTURES = (('T_SD_Firebrick_D.png', 'color'), ('T_SD_Firebrick_N.png', 'normal'))
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def save(path):
    if not ASSETS.save_asset(path, only_if_is_dirty=False):
        print('SAVE FAILED', path)


def import_textures():
    tasks = []
    for name, _ in TEXTURES:
        if ASSETS.does_asset_exist(f'{MATS}/{name[:-4]}'):
            continue
        task = unreal.AssetImportTask()
        task.filename = MODEL + 'tex/' + name
        task.destination_path = MATS
        task.automated = True
        task.save = False  # AssetImportTask(save=True) silently misses the disk here
        task.replace_existing = True
        tasks.append((name, task))
    if tasks:
        ASSET_TOOLS.import_asset_tasks([t for _, t in tasks])
    kinds = dict(TEXTURES)
    for name, task in tasks:
        path = list(task.imported_object_paths)[0]
        texture = unreal.load_asset(path)
        if kinds[name] == 'normal':
            texture.set_editor_property('srgb', False)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        save(path.split('.')[0])


def import_mesh(name):
    """Legacy FBX (Interchange ignores FbxImportUI): normals and vertex colours (the melt glow mask) as authored."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
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


def new_material(name):
    path = f'{MATS}/{name}'
    if ASSETS.does_asset_exist(path):
        return None, unreal.load_asset(path)
    return path, ASSET_TOOLS.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())


def scalar(material, name, value, x, y):
    node = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def vector(material, name, value, x, y):
    node = EDIT.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*value))
    return node


def firebrick_material():
    """Cupola firebrick: T_SD_Firebrick_D / _N (soot toward the top is in the texture: v runs up the whole shaft)."""
    path, material = new_material('M_SD_Firebrick')
    if path is None:
        return material
    base = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, -200)
    base.set_editor_property('parameter_name', 'BaseColor')
    base.set_editor_property('texture', unreal.load_asset(f'{MATS}/T_SD_Firebrick_D'))
    normal = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 200)
    normal.set_editor_property('parameter_name', 'Normal')
    normal.set_editor_property('texture', unreal.load_asset(f'{MATS}/T_SD_Firebrick_N'))
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = scalar(material, 'Roughness', 0.85, -300, 50)
    spec = scalar(material, 'Specular', 0.3, -300, 120)
    EDIT.connect_material_property(base, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    EDIT.recompile_material(material)
    save(path)
    return material


def melt_material():
    """Molten metal: dark crust base colour; emissive MeltColor x EmissiveStrength x vertex colour R (the mesh sets R per
    part: tap 1.0, charging door 0.7, flasks 0.12-0.35, stack mouth 0.35) x a slow flicker."""
    path, material = new_material('M_SD_SmelterMelt')
    if path is None:
        return material
    crust = vector(material, 'CrustColor', (0.25, 0.06, 0.01, 1.0), -400, -300)
    melt = vector(material, 'MeltColor', (1.0, 0.30, 0.03, 1.0), -900, 0)
    strength = scalar(material, 'EmissiveStrength', 300.0, -900, 150)
    vcol = EDIT.create_material_expression(material, unreal.MaterialExpressionVertexColor, -900, 250)
    time = EDIT.create_material_expression(material, unreal.MaterialExpressionTime, -1300, 450)
    speed = scalar(material, 'FlickerSpeed', 5.0, -1300, 550)
    depth = scalar(material, 'FlickerDepth', 0.12, -900, 550)
    t = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -1100, 450)
    EDIT.connect_material_expressions(time, '', t, 'A')
    EDIT.connect_material_expressions(speed, '', t, 'B')
    sine = EDIT.create_material_expression(material, unreal.MaterialExpressionSine, -950, 450)
    EDIT.connect_material_expressions(t, '', sine, '')
    lerp = EDIT.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -750, 450)
    one = EDIT.create_material_expression(material, unreal.MaterialExpressionConstant, -900, 380)
    one.set_editor_property('r', 1.0)
    low = EDIT.create_material_expression(material, unreal.MaterialExpressionOneMinus, -750, 550)
    EDIT.connect_material_expressions(depth, '', low, '')
    # flicker = lerp(1 - depth, 1, sine * 0.5 + 0.5)
    half = EDIT.create_material_expression(material, unreal.MaterialExpressionConstantBiasScale, -800, 450)
    half.set_editor_property('bias', 1.0)
    half.set_editor_property('scale', 0.5)
    EDIT.connect_material_expressions(sine, '', half, '')
    EDIT.connect_material_expressions(low, '', lerp, 'A')
    EDIT.connect_material_expressions(one, '', lerp, 'B')
    EDIT.connect_material_expressions(half, '', lerp, 'Alpha')
    a = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 0)
    EDIT.connect_material_expressions(melt, '', a, 'A')
    EDIT.connect_material_expressions(strength, '', a, 'B')
    b = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 100)
    EDIT.connect_material_expressions(a, '', b, 'A')
    EDIT.connect_material_expressions(vcol, 'R', b, 'B')
    c = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 200)
    EDIT.connect_material_expressions(b, '', c, 'A')
    EDIT.connect_material_expressions(lerp, '', c, 'B')
    rough = scalar(material, 'Roughness', 0.45, -300, -100)
    EDIT.connect_material_property(crust, '', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(material)
    save(path)
    return material


def finish_mesh(name, materials, collision):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    for index, material in enumerate(materials):
        mesh.set_material(index, material)
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    for socket_name in SOCKETS:
        socket = mesh.find_socket(socket_name)
        if socket:      # FBX empties arrive tilted; both sockets are plain positions
            socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
    if collision is not None:
        mesh.set_editor_property('complex_collision_mesh', collision)
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(path)
    found = [s for s in SOCKETS if mesh.find_socket(s)]
    bounds = mesh.get_bounding_box()
    print(name, 'triangles', mesh.get_num_triangles(0), 'sockets', found, 'bounds', bounds.min, bounds.max)


def main():
    import_textures()
    firebrick, melt = firebrick_material(), melt_material()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in (COLLISION, MAIN, DAMAGED, PREVIEW):
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    slots = [unreal.load_asset(VAULT_MATERIALS[s]) for s in SLOT_ORDER[:4]] + [firebrick, melt]
    collision = unreal.load_asset(f'{HOME}/{COLLISION}')
    finish_mesh(COLLISION, [unreal.load_asset('/Engine/EngineMaterials/WorldGridMaterial')], None)
    finish_mesh(MAIN, slots, collision)
    finish_mesh(DAMAGED, slots, collision)
    finish_mesh(PREVIEW, [unreal.load_asset(PREVIEW_MATERIAL)], collision)


main()
