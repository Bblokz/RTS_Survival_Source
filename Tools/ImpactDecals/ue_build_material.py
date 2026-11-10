"""Imports the impact-decal atlas and (re)builds M_LandscapeImpactDecal + MI_LandscapeImpactDecal in the live editor.

Run inside the editor (UECP execute_python):
    ATLAS_DIR = 'D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/r0'; BUILD_MATERIAL = True
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/ImpactDecals/ue_build_material.py').read())

Set BUILD_MATERIAL = False to only reimport the textures (review rounds). The material is only built when it does not
exist yet (an existing master is never wiped, see ue-python-material-gotchas).

Bomb craters (2026-11-09): the master samples two TextureSampleParameter2D nodes, ImpactBCO / ImpactNRS (defaults =
the shell atlas, so MI_LandscapeImpactDecal is unchanged), and MI_BombImpactDecal overrides both with T_BombImpact_*.
    ATLAS_DIR = '<shell atlas dir or None>'; BOMB_ATLAS_DIR = '<bomb atlas dir or None>'; BUILD_MATERIAL = True
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/ImpactDecals/ue_build_material.py').read())
An existing master that still has plain TextureSample nodes is migrated in place (parameterise_master): two parameter
nodes are added, the seven links are moved to them and the two old samples are deleted; nothing else is touched.

Material contract (C++ side, UDecalComponent::DecalColor):
  R = (cell + 0.5) / 16 -> cell = floor(R * 16), column = cell % 4, row = cell / 4 (row 0 top)
  G = per-decal random 0..1 -> albedo brightness and soot amount variation
  A = opacity multiplier
Blend: Deferred Decal + Translucent with BaseColor, Normal, Roughness, Opacity connected
       = "DBuffer Translucent Color, Normal, Roughness" (r.DBuffer = 1).
"""
import unreal

FOLDER = '/Game/RTS_Survival/Materials/Decals/LandscapeImpacts'
MAT_NAME = 'M_LandscapeImpactDecal'
MI_NAME = 'MI_LandscapeImpactDecal'
TEX_BCO = 'T_LandscapeImpact_BCO'
TEX_NRS = 'T_LandscapeImpact_NRS'
BOMB_MI_NAME = 'MI_BombImpactDecal'
BOMB_BCO = 'T_BombImpact_BCO'
BOMB_NRS = 'T_BombImpact_NRS'
PARAM_BCO = 'ImpactBCO'
PARAM_NRS = 'ImpactNRS'

try:
    ATLAS_DIR
except NameError:
    ATLAS_DIR = 'D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/r0'
try:
    BUILD_MATERIAL
except NameError:
    BUILD_MATERIAL = True
try:
    BOMB_ATLAS_DIR
except NameError:
    BOMB_ATLAS_DIR = None

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def import_texture(name, srgb, compression, atlas_dir=None):
    task = unreal.AssetImportTask()
    task.filename = (atlas_dir or ATLAS_DIR) + '/' + name + '.png'
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
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
    if not EAL.save_asset(FOLDER + '/' + name, only_if_is_dirty=False):
        EAL.save_loaded_asset(tex, False)
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


def scalar(mat, name, val, x, y, group='Impact'):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', val)
    n.set_editor_property('group', group)
    return n


def vector(mat, name, val, x, y, group='Impact'):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', unreal.LinearColor(*val))
    n.set_editor_property('group', group)
    return n


def mask(mat, src, r, g, b, a, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
    n.set_editor_property('r', r)
    n.set_editor_property('g', g)
    n.set_editor_property('b', b)
    n.set_editor_property('a', a)
    MEL.connect_material_expressions(src, '', n, '')
    return n


def link(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        raise RuntimeError('connect failed %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


ATLAS_UV = """float Cell = min(floor(saturate(CellR) * 16.0), 15.0);
float Row = floor(Cell * 0.25);
float2 CellXY = float2(Cell - 4.0 * Row, Row);
return (CellXY + clamp(UV, 0.004, 0.996)) * 0.25;"""

BASE_COLOR = """float3 C = Bco.rgb * Tint * lerp(0.82, 1.18, Var);
float S = saturate(Nrs.a * ScorchStrength * lerp(0.8, 1.2, Var));
return lerp(C, ScorchColor, S);"""

NORMAL = """float2 XY = (Nrs.rg * 2.0 - 1.0) * NormalStrength;
return float3(XY, sqrt(saturate(1.0 - dot(XY, XY))));"""

# Receiver normal from screen derivatives of the (camera-relative) world position; DBuffer decals can't read the GBuffer
# normal. abs() makes the cross-product sign irrelevant. Decals are projected straight down, so world Z is the axis.
OPACITY = """float3 N = cross(ddx(Wp), ddy(Wp));
float Facing = abs(N.z) * rsqrt(max(dot(N, N), 1e-12));
return saturate(BcoA * DecalA * Opacity) * smoothstep(SlopeFadeStart, SlopeFadeEnd, Facing);"""

ROUGH = """return saturate(Nrs.b * RoughnessMul);"""


def texture_param(mat, name, tex, sampler, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('texture', tex)
    n.set_editor_property('sampler_type', sampler)
    n.set_editor_property('group', 'Impact')
    return n


def _expr_of(mat, cls):
    return [e for e in unreal.ObjectIterator(cls) if e.get_outer() == mat]


def parameterise_master(mat, tex_bco, tex_nrs):
    """Migrates an existing master in place: plain TextureSample nodes -> ImpactBCO / ImpactNRS parameters.
    Never wipes the graph (the master is in use, see ue-python-material-gotchas)."""
    params = [e for e in _expr_of(mat, unreal.MaterialExpressionTextureSampleParameter2D)
              if str(e.get_editor_property('parameter_name')) in (PARAM_BCO, PARAM_NRS)]
    if len(params) == 2:
        print('master already parameterised')
        return False
    customs = {str(e.get_editor_property('description')): e for e in _expr_of(mat, unreal.MaterialExpressionCustom)}
    uv, bc, nm, rg, op = (customs['AtlasCellUV'], customs['ImpactBaseColor'], customs['ImpactNormal'],
                          customs['ImpactRoughness'], customs['ImpactOpacity'])
    old = [e for e in _expr_of(mat, unreal.MaterialExpressionTextureSample)
           if type(e) is unreal.MaterialExpressionTextureSample]
    p_bco = texture_param(mat, PARAM_BCO, tex_bco, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -850, -150)
    p_nrs = texture_param(mat, PARAM_NRS, tex_nrs, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -850, 200)
    link(uv, '', p_bco, 'UVs')
    link(uv, '', p_nrs, 'UVs')
    link(p_bco, 'RGBA', bc, 'Bco')
    link(p_nrs, 'RGBA', bc, 'Nrs')
    link(p_nrs, 'RGBA', nm, 'Nrs')
    link(p_nrs, 'RGBA', rg, 'Nrs')
    link(p_bco, 'A', op, 'BcoA')
    for e in old:
        MEL.delete_material_expression(mat, e)
    MEL.recompile_material(mat)
    print('parameterised master, removed', len(old), 'plain samples')
    return True


def save_checked(path, obj):
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    if not ok:
        ok = EAL.save_loaded_asset(obj, False)
    print('saved' if ok else 'SAVE FAILED', path)
    return ok


def build_bomb_mi(mat, tex_bco, tex_nrs):
    path = FOLDER + '/' + BOMB_MI_NAME
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BOMB_MI_NAME, FOLDER, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    MEL.set_material_instance_texture_parameter_value(mi, PARAM_BCO, tex_bco)
    MEL.set_material_instance_texture_parameter_value(mi, PARAM_NRS, tex_nrs)
    MEL.update_material_instance(mi)
    save_checked(path, mi)
    return mi


def build_material(tex_bco, tex_nrs):
    path = FOLDER + '/' + MAT_NAME
    if EAL.does_asset_exist(path):
        print('material exists, not rebuilding', path)
        return unreal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    tc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
    dc = MEL.create_material_expression(mat, unreal.MaterialExpressionDecalColor, -1500, 200)
    # DecalColor's default output is RGB only; use its per-channel outputs (R, G, A).

    uv = custom(mat, -1100, 60, 'AtlasCellUV', ATLAS_UV, unreal.CustomMaterialOutputType.CMOT_FLOAT2, ['UV', 'CellR'])
    link(tc, '', uv, 'UV')
    link(dc, 'R', uv, 'CellR')

    s_bco = texture_param(mat, PARAM_BCO, tex_bco, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -850, -150)
    link(uv, '', s_bco, 'UVs')
    s_nrs = texture_param(mat, PARAM_NRS, tex_nrs, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -850, 200)
    link(uv, '', s_nrs, 'UVs')

    tint = vector(mat, 'Tint', (1.0, 1.0, 1.0, 1.0), -850, -450)
    sc_col = vector(mat, 'ScorchColor', (0.010, 0.0085, 0.0075, 1.0), -850, -650)
    sc_str = scalar(mat, 'ScorchStrength', 0.75, -600, -650)
    opac = scalar(mat, 'Opacity', 1.0, -600, 600)
    nstr = scalar(mat, 'NormalStrength', 1.0, -600, 420)
    rmul = scalar(mat, 'RoughnessMul', 1.0, -600, 520)
    f0 = scalar(mat, 'SlopeFadeStart', 0.45, -600, 700)
    f1 = scalar(mat, 'SlopeFadeEnd', 0.8, -600, 800)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -850, 700)
    wp.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)

    bc = custom(mat, -300, -300, 'ImpactBaseColor', BASE_COLOR, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                ['Bco', 'Nrs', 'Var', 'Tint', 'ScorchColor', 'ScorchStrength'])
    link(s_bco, 'RGBA', bc, 'Bco')
    link(s_nrs, 'RGBA', bc, 'Nrs')
    link(dc, 'G', bc, 'Var')
    link(tint, '', bc, 'Tint')
    link(sc_col, '', bc, 'ScorchColor')
    link(sc_str, '', bc, 'ScorchStrength')

    nm = custom(mat, -300, 150, 'ImpactNormal', NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ['Nrs', 'NormalStrength'])
    link(s_nrs, 'RGBA', nm, 'Nrs')
    link(nstr, '', nm, 'NormalStrength')

    rg = custom(mat, -300, 350, 'ImpactRoughness', ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ['Nrs', 'RoughnessMul'])
    link(s_nrs, 'RGBA', rg, 'Nrs')
    link(rmul, '', rg, 'RoughnessMul')

    op = custom(mat, -300, 600, 'ImpactOpacity', OPACITY, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                ['BcoA', 'DecalA', 'Opacity', 'Wp', 'SlopeFadeStart', 'SlopeFadeEnd'])
    link(s_bco, 'A', op, 'BcoA')
    link(dc, 'A', op, 'DecalA')
    link(opac, '', op, 'Opacity')
    link(wp, '', op, 'Wp')
    link(f0, '', op, 'SlopeFadeStart')
    link(f1, '', op, 'SlopeFadeEnd')

    MEL.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nm, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rg, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_asset(path, only_if_is_dirty=False)
    print('built', path)
    return mat


def build_mi(mat):
    path = FOLDER + '/' + MI_NAME
    if EAL.does_asset_exist(path):
        print('MI exists', path)
        return unreal.load_asset(path)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MI_NAME, FOLDER, unreal.MaterialInstanceConstant,
                                                                 unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    EAL.save_asset(path, only_if_is_dirty=False)
    print('built', path)
    return mi


if ATLAS_DIR:
    t_bco = import_texture(TEX_BCO, True, unreal.TextureCompressionSettings.TC_DEFAULT)
    t_nrs = import_texture(TEX_NRS, False, unreal.TextureCompressionSettings.TC_MASKS)
else:
    t_bco, t_nrs = unreal.load_asset(FOLDER + '/' + TEX_BCO), unreal.load_asset(FOLDER + '/' + TEX_NRS)
if BOMB_ATLAS_DIR:
    b_bco = import_texture(BOMB_BCO, True, unreal.TextureCompressionSettings.TC_DEFAULT, BOMB_ATLAS_DIR)
    b_nrs = import_texture(BOMB_NRS, False, unreal.TextureCompressionSettings.TC_MASKS, BOMB_ATLAS_DIR)
if BUILD_MATERIAL:
    m = build_material(t_bco, t_nrs)
    if parameterise_master(m, t_bco, t_nrs):
        save_checked(FOLDER + '/' + MAT_NAME, m)
    build_mi(m)
    if BOMB_ATLAS_DIR:
        build_bomb_mi(m, b_bco, b_nrs)
