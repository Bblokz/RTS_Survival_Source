# Unreal (via execute_uecp_script, exec'd by ue_proto_materials.py). PROTOTYPE, delete with the folder.
# Hand-wired rebuilds of the HQ slot materials whose graphs feed material-function inputs: export_material_graph
# drops those wires and a duplicate_asset copy of them never compiled the added World Position Offset.
# Wiring read from the originals with get_material_nodes (2026-11-10).
import unreal

MEL = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty
TEAM_COLOR_MF = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/TeamColor/MF_TeamColor_CamoSwap'
ATLAS_MF = '/Game/RTS_Survival/Materials/MaterialFunctions/MF_GetAtlasUVAtIndex'


def _texture(mat, path, sampler, x, y):
    t = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
    t.set_editor_property('texture', unreal.load_asset(path))
    t.set_editor_property('sampler_type', sampler)
    return t


def _const(mat, value, x, y):
    c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property('r', value)
    return c


def _graph_textures(original):
    """Texture path and sampler type per exported node index."""
    out = {}
    for n in uecp.material.export_material_graph(material_path=original.get_path_name())['nodes']:
        tex = n.get('properties', {}).get('Texture')
        if tex:
            out[n['index']] = (tex.split("'")[1], n['properties'].get('SamplerType'))
    return out


def _sampler(name):
    return {'SAMPLERTYPE_Normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
            'SAMPLERTYPE_Masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
            'SAMPLERTYPE_LinearColor': unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}.get(
        name, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)


def build_panther(original, mat):
    """M_PantherII_Sheets / _Parts: tex0 -> TeamColor camo swap -> BaseColor, tex1/2/3 -> Metallic/Roughness/Normal."""
    tex = _graph_textures(original)
    t = {i: _texture(mat, tex[i][0], _sampler(tex[i][1]), -900, -100 + 250 * i) for i in (0, 1, 2, 3)}
    call = MEL.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -500, -100)
    call.set_material_function(unreal.load_asset(TEAM_COLOR_MF))
    MEL.connect_material_expressions(t[0], 'RGB', call, 'BaseColor')
    for name, value in (('TC_KeyColor', 'KeyColor'), ('TC_KeyColor2', 'KeyColor2')):
        p = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 300)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', MEL.get_material_default_vector_parameter_value(original, name))
        MEL.connect_material_expressions(p, '', call, value)
    for name, pin in (('TC_Strength', 'Strength'), ('TC_KeyMin', 'KeyMin'), ('TC_KeyMax', 'KeyMax')):
        p = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 500)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', MEL.get_material_default_scalar_parameter_value(original, name))
        MEL.connect_material_expressions(p, '', call, pin)
    MEL.connect_material_property(call, 'Result', P.MP_BASE_COLOR)
    MEL.connect_material_property(t[1], 'RGB', P.MP_METALLIC)
    MEL.connect_material_property(t[2], 'RGB', P.MP_ROUGHNESS)
    MEL.connect_material_property(t[3], 'RGB', P.MP_NORMAL)


def build_camo_atlas(original, mat):
    """M_GerCamoConcretePalet_Atlas: concrete cell darkened x0.411 and 0.6 metallic, camo-tent cell normal x2."""
    tex = _graph_textures(original)
    d = _texture(mat, tex[0][0], _sampler(tex[0][1]), -900, -100)
    n = _texture(mat, tex[1][0], _sampler(tex[1][1]), -900, 800)
    raom = _texture(mat, tex[2][0], _sampler(tex[2][1]), -900, 250)

    def atlas_mask(index, tiles, y):
        call = MEL.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -1300, y)
        call.set_material_function(unreal.load_asset(ATLAS_MF))
        MEL.connect_material_expressions(_const(mat, index, -1500, y), '', call, 'Index')
        MEL.connect_material_expressions(_const(mat, tiles, -1500, y + 80), '', call, 'AmountTilesPerAxis')
        return call

    concrete = atlas_mask(2.0, 2.0, -100)
    # the original's constants are 1 and 2 on this call; tiles must be 2 for the 2x2 atlas, so index 1
    tent = atlas_mask(1.0, 2.0, 150)
    grey = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, -400)
    grey.set_editor_property('constant', unreal.LinearColor(0.411458, 0.411458, 0.411458, 0))
    dark = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -450)
    MEL.connect_material_expressions(d, 'RGB', dark, 'A')
    MEL.connect_material_expressions(grey, '', dark, 'B')
    base = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -150, -250)
    MEL.connect_material_expressions(d, 'RGB', base, 'A')
    MEL.connect_material_expressions(dark, '', base, 'B')
    MEL.connect_material_expressions(concrete, 'IndexUVOnAtlas', base, 'Alpha')
    metal = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -150, 0)
    metal.set_editor_property('const_b', 0.6)
    MEL.connect_material_expressions(raom, 'B', metal, 'A')
    MEL.connect_material_expressions(concrete, 'IndexUVOnAtlas', metal, 'Alpha')
    strong = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 950)
    strong.set_editor_property('const_b', 2.0)
    MEL.connect_material_expressions(n, 'RGB', strong, 'A')
    normal = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -150, 850)
    MEL.connect_material_expressions(n, 'RGB', normal, 'A')
    MEL.connect_material_expressions(strong, '', normal, 'B')
    MEL.connect_material_expressions(tent, 'IndexUVOnAtlas', normal, 'Alpha')
    MEL.connect_material_property(base, '', P.MP_BASE_COLOR)
    MEL.connect_material_property(metal, '', P.MP_METALLIC)
    MEL.connect_material_property(raom, 'R', P.MP_ROUGHNESS)
    MEL.connect_material_property(raom, 'G', P.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(d, 'A', P.MP_OPACITY_MASK)
    MEL.connect_material_property(normal, '', P.MP_NORMAL)


SPECIAL_BUILDERS = {'M_PantherII_Sheets': build_panther, 'M_PantherII_Parts': build_panther,
                    'M_GerCamoConcretePalet_Atlas': build_camo_atlas}
