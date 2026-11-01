"""Builds the foundation pad decal material, its surfaces and the pit earth. Safe to run again: replaces what is there."""
import unreal

FOLDER = '/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Pads'
CONCRETE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/T_concrete2_bc'
GRAVEL = '/Game/RTS_Survival/Materials/ground/SmallHills/T_Desert_Western_Ground_Gravel_Slate_01_venoehv_1K_D'
ROAD_GRAVEL = '/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/T_GroundPath_Gravel_BC'
ROAD_DIRT = '/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/T_GroundPath_Dirt_BC'
EDGE_HLSL = """float2 e = abs(UV * 2.0 - 1.0);
float d = 1.0 - max(e.x, e.y);
d -= (Noise - 0.5) * Edge * 0.7;
return saturate(d / Edge) * Opacity;"""
lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def fresh(name, cls, factory):
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, FOLDER, cls, factory)


def scalar(mat, name, value):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 0)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def surface(mat):
    """World-aligned texture sample times a tint; returns (sample, tinted)."""
    pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)
    mask = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1050, 0)
    mask.set_editor_property('r', True)
    mask.set_editor_property('g', True)
    lib.connect_material_expressions(pos, '', mask, '')
    div = lib.create_material_expression(mat, unreal.MaterialExpressionDivide, -900, 0)
    lib.connect_material_expressions(mask, '', div, 'A')
    lib.connect_material_expressions(scalar(mat, 'TilingCm', 900.0), '', div, 'B')
    tex = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property('parameter_name', 'Surface')
    tex.set_editor_property('texture', unreal.load_asset(GRAVEL))
    lib.connect_material_expressions(div, '', tex, 'UVs')
    tint = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 300)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 0)
    lib.connect_material_expressions(tex, 'RGB', mul, 'A')
    lib.connect_material_expressions(tint, '', mul, 'B')
    return tex, mul


def constant(mat, value, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, y)
    node.set_editor_property('r', value)
    return node


def make_decal():
    mat = fresh('M_SovietPad', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tex, tinted = surface(mat)
    lib.connect_material_property(tinted, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(constant(mat, 0.92, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -700, 500)
    edge = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 500)
    edge.set_editor_property('code', EDGE_HLSL)
    edge.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for name in ('UV', 'Noise', 'Edge', 'Opacity'):
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    edge.set_editor_property('inputs', inputs)
    lib.connect_material_expressions(uv, '', edge, 'UV')
    lib.connect_material_expressions(tex, 'R', edge, 'Noise')
    lib.connect_material_expressions(scalar(mat, 'EdgeShare', 0.12), '', edge, 'Edge')
    lib.connect_material_expressions(scalar(mat, 'Opacity', 0.82), '', edge, 'Opacity')
    lib.connect_material_property(edge, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def make_earth():
    mat = fresh('M_SovietPitEarth', unreal.Material, unreal.MaterialFactoryNew())
    _, tinted = surface(mat)
    lib.connect_material_property(tinted, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(constant(mat, 0.95, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def make_instance(name, parent, texture, tint, tiling):
    inst = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    inst.set_editor_property('parent', parent)
    lib.set_material_instance_texture_parameter_value(inst, 'Surface', unreal.load_asset(texture))
    lib.set_material_instance_vector_parameter_value(inst, 'Tint', tint)
    lib.set_material_instance_scalar_parameter_value(inst, 'TilingCm', tiling)
    lib.update_material_instance(inst)
    unreal.EditorAssetLibrary.save_loaded_asset(inst, only_if_is_dirty=False)


pad = make_decal()
earth = make_earth()
make_instance('MI_SovietPad_Concrete', pad, CONCRETE, unreal.LinearColor(0.80, 0.79, 0.76, 1), 1400.0)
make_instance('MI_SovietPad_Gravel', pad, ROAD_GRAVEL, unreal.LinearColor(0.92, 0.90, 0.84, 1), 840.0)
make_instance('MI_SovietPad_PackedEarth', pad, ROAD_DIRT, unreal.LinearColor(0.85, 0.82, 0.76, 1), 840.0)
make_instance('MI_SovietPit_Earth', earth, ROAD_DIRT, unreal.LinearColor(0.70, 0.62, 0.52, 1), 840.0)
for name in ('M_SovietPad', 'M_SovietPitEarth', 'MI_SovietPad_Concrete', 'MI_SovietPad_Gravel', 'MI_SovietPad_PackedEarth', 'MI_SovietPit_Earth'):
    print('SOVIET_PAD', name, unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + name))
