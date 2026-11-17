"""Imports the Berlin rubble atlas and builds M_BerlinRubbleDecal + the 16 MI_BerlinRubbleDecal_<NN>_<Name> decals.

Run inside the editor (bridge execute_python, or uecp_call.py):
    ATLAS_DIR = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/final'; REBUILD_MATERIAL = False
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/rubbledecals/rubble_ue_build.py').read())

Material contract (graph made after Materials/Decals/LandscapeImpacts/M_LandscapeImpactDecal, which is reviewed and
in use; that master is not touched):
  Deferred decal, translucent, BaseColor + Normal + Roughness + Opacity (DBuffer translucent colour, normal, roughness).
  Scalar 'Cell' (0..15) picks the atlas cell; each MI sets it. Decal Color per placed decal (default white = neutral):
    G = brightness (1 = as drawn, 0 = 15 % darker), B < 0.5 mirrors the cell left-right (normal X flipped too),
    A = opacity multiplier.
  BedTint / PieceTint split the colour by the NRS alpha (dust bed vs rubble pieces), BedOpacity thins only the bed,
  slope fade keeps the decal off walls (receiver normal from screen derivatives of the world position).
Placed decal: rotation pitch -90 (projects down), DecalSize = (depth half, 200 * scale, 200 * scale) cm. The atlas top
edge (image up) is the source side of the fan/corner cells; see rubble_table.py.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals'
MAT_NAME = 'M_BerlinRubbleDecal'
TEX_BCO = 'T_BerlinRubbleDecal_BCO'
TEX_NRS = 'T_BerlinRubbleDecal_NRS'
NAMES = ["BrickSpillFan", "BrickHeapSpread", "MasonryStrip", "CornerPile", "PlasterDust", "ConcreteRebar", "RoofTiles",
         "SlateShards", "CharredTimber", "AshSoot", "GlassShards", "SettsTornUp", "PavingSlabs", "SteelScrap",
         "SandstoneBlocks", "GritSpill"]

try:
    ATLAS_DIR
except NameError:
    ATLAS_DIR = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/final'
try:
    REBUILD_MATERIAL
except NameError:
    REBUILD_MATERIAL = False

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ATL = unreal.AssetToolsHelpers.get_asset_tools()


def save(path, obj=None):
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    print('saved' if ok else 'SAVE FAILED', path)
    return ok


def import_texture(name, srgb, compression):
    task = unreal.AssetImportTask()
    task.filename = ATLAS_DIR + '/' + name + '.png'
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ATL.import_asset_tasks([task])
    tex = unreal.load_asset(FOLDER + '/' + name)
    if tex is None:
        raise RuntimeError('import failed ' + name)
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', compression)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('virtual_texture_streaming', False)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    save(FOLDER + '/' + name)
    print('imported', name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    return tex


def custom(mat, x, y, desc, code, out_type, inputs):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property('description', desc)
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', out_type)
    ins = []
    for nm in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', nm)
        ins.append(ci)
    n.set_editor_property('inputs', ins)
    return n


def scalar(mat, name, val, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', val)
    n.set_editor_property('group', 'Rubble')
    return n


def vector(mat, name, val, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', unreal.LinearColor(*val))
    n.set_editor_property('group', 'Rubble')
    return n


def tex_param(mat, name, tex, sampler, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('texture', tex)
    n.set_editor_property('sampler_type', sampler)
    n.set_editor_property('group', 'Rubble')
    return n


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError('connect failed %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


ATLAS_UV = """float C = clamp(floor(Cell + 0.5), 0.0, 15.0);
float Row = floor(C * 0.25);
float2 CellXY = float2(C - 4.0 * Row, Row);
float2 U = UV;
if (Mirror < 0.5) U.x = 1.0 - U.x;
return (CellXY + clamp(U, 0.004, 0.996)) * 0.25;"""

BASE_COLOR = """float3 C = Bco.rgb * lerp(BedTint, PieceTint, Nrs.a) * lerp(0.85, 1.0, Var);
float L = dot(C, float3(0.3, 0.59, 0.11));
return lerp(C, L.xxx, Desaturate);"""

NORMAL = """float2 XY = (Nrs.rg * 2.0 - 1.0) * NormalStrength;
if (Mirror < 0.5) XY.x = -XY.x;
return float3(XY, sqrt(saturate(1.0 - dot(XY, XY))));"""

OPACITY = """float3 N = cross(ddx(Wp), ddy(Wp));
float Facing = abs(N.z) * rsqrt(max(dot(N, N), 1e-12));
float A = BcoA * lerp(BedOpacity, 1.0, saturate(Nrs.a * 2.0));
return saturate(A * DecalA * Opacity) * smoothstep(SlopeFadeStart, SlopeFadeEnd, Facing);"""

ROUGH = """return saturate(Nrs.b * RoughnessMul);"""


def build_material(t_bco, t_nrs):
    path = FOLDER + '/' + MAT_NAME
    if EAL.does_asset_exist(path):
        if not REBUILD_MATERIAL:
            print('material exists, kept', path)
            return unreal.load_asset(path)
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    tc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
    dc = MEL.create_material_expression(mat, unreal.MaterialExpressionDecalColor, -1500, 200)
    cell = scalar(mat, 'Cell', 0.0, -1500, -150)
    uv = custom(mat, -1100, 60, 'AtlasCellUV', ATLAS_UV, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                ['UV', 'Cell', 'Mirror'])
    link(tc, '', uv, 'UV')
    link(cell, '', uv, 'Cell')
    link(dc, 'B', uv, 'Mirror')

    s_bco = tex_param(mat, 'RubbleBCO', t_bco, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -850, -150)
    s_nrs = tex_param(mat, 'RubbleNRS', t_nrs, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -850, 200)
    link(uv, '', s_bco, 'UVs')
    link(uv, '', s_nrs, 'UVs')

    bed_t = vector(mat, 'BedTint', (1.0, 1.0, 1.0, 1.0), -850, -650)
    pc_t = vector(mat, 'PieceTint', (1.0, 1.0, 1.0, 1.0), -850, -450)
    desat = scalar(mat, 'Desaturate', 0.0, -600, -650)
    opac = scalar(mat, 'Opacity', 1.0, -600, 600)
    bed_o = scalar(mat, 'BedOpacity', 1.0, -600, 500)
    nstr = scalar(mat, 'NormalStrength', 1.0, -600, 420)
    rmul = scalar(mat, 'RoughnessMul', 1.0, -600, 330)
    f0 = scalar(mat, 'SlopeFadeStart', 0.45, -600, 700)
    f1 = scalar(mat, 'SlopeFadeEnd', 0.8, -600, 800)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -850, 700)
    wp.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)

    bc = custom(mat, -300, -300, 'RubbleBaseColor', BASE_COLOR, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                ['Bco', 'Nrs', 'Var', 'BedTint', 'PieceTint', 'Desaturate'])
    link(s_bco, 'RGBA', bc, 'Bco')
    link(s_nrs, 'RGBA', bc, 'Nrs')
    link(dc, 'G', bc, 'Var')
    link(bed_t, '', bc, 'BedTint')
    link(pc_t, '', bc, 'PieceTint')
    link(desat, '', bc, 'Desaturate')

    nm = custom(mat, -300, 150, 'RubbleNormal', NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                ['Nrs', 'NormalStrength', 'Mirror'])
    link(s_nrs, 'RGBA', nm, 'Nrs')
    link(nstr, '', nm, 'NormalStrength')
    link(dc, 'B', nm, 'Mirror')

    rg = custom(mat, -300, 350, 'RubbleRoughness', ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                ['Nrs', 'RoughnessMul'])
    link(s_nrs, 'RGBA', rg, 'Nrs')
    link(rmul, '', rg, 'RoughnessMul')

    op = custom(mat, -300, 600, 'RubbleOpacity', OPACITY, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                ['BcoA', 'Nrs', 'DecalA', 'Opacity', 'BedOpacity', 'Wp', 'SlopeFadeStart', 'SlopeFadeEnd'])
    link(s_bco, 'A', op, 'BcoA')
    link(s_nrs, 'RGBA', op, 'Nrs')
    link(dc, 'A', op, 'DecalA')
    link(opac, '', op, 'Opacity')
    link(bed_o, '', op, 'BedOpacity')
    link(wp, '', op, 'Wp')
    link(f0, '', op, 'SlopeFadeStart')
    link(f1, '', op, 'SlopeFadeEnd')

    MEL.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nm, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rg, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    save(path)
    return mat


def build_mis(mat):
    out = []
    for i, nm in enumerate(NAMES):
        name = 'MI_BerlinRubbleDecal_%02d_%s' % (i, nm)
        path = FOLDER + '/' + name
        if EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = ATL.create_asset(name, FOLDER, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mat)
        MEL.set_material_instance_scalar_parameter_value(mi, 'Cell', float(i))
        MEL.update_material_instance(mi)
        save(path)
        out.append(path)
    return out


t_bco = import_texture(TEX_BCO, True, unreal.TextureCompressionSettings.TC_DEFAULT)
t_nrs = import_texture(TEX_NRS, False, unreal.TextureCompressionSettings.TC_MASKS)
m = build_material(t_bco, t_nrs)
paths = build_mis(m)
print('RUBBLE_BUILD_DONE', len(paths))
