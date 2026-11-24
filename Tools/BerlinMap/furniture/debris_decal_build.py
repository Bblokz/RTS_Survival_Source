"""Furniture debris decals for Ger_Berlin: imports the lookup map and builds the decal material (2026-11-13).

Headless:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
Idempotent (the material is rebuilt each run). Doc: Source/Docs/Environment/BerlinFurniture.md.

The decal shows shards of the furniture atlas itself: colour, normal and roughness come from T_BerlinFurniture_D / _N /
_ORM (the textures of the 16 furniture pieces); T_BerlinFurniture_DebrisMap (debris_mask_gen.py) only says which atlas
texel a decal texel shows (R, G) and where a shard is (B). 16 variants (4 x 4 cells), one per furniture piece.

One material instance for all: the variant is picked per decal through the DecalComponent's DecalColor,
R = (variant + 0.5) / 16, A = opacity.
"""
import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture'
TEXTURES = ROOT + '/Textures'
MATERIALS = ROOT + '/Materials'
MAP_FILE = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/T_BerlinFurniture_DebrisMap.png'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

CELL_UV = '''
float cell = clamp(floor(CellR * 16.0), 0.0, 15.0);
float2 origin = float2(fmod(cell, 4.0), floor(cell / 4.0));
return (origin + saturate(UV)) * 0.25;
'''


def log(message):
    unreal.log_warning('BERLIN_FURN_DECAL ' + message)


def import_map():
    task = unreal.AssetImportTask()
    task.filename = MAP_FILE
    task.destination_path = TEXTURES
    task.destination_name = 'T_BerlinFurniture_DebrisMap'
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(TEXTURES + '/T_BerlinFurniture_DebrisMap')
    if tex is None:
        raise RuntimeError('lookup map was not imported from ' + MAP_FILE)
    # the map holds texture coordinates: 16-bit, linear, never compressed to 8 bit, never mipped (averaged UVs are wrong)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('virtual_texture_streaming', False)
    tex.set_editor_property('never_stream', True)
    EAL.save_loaded_asset(tex, False)
    log('map %d x %d' % (tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError('connect failed %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def param(mat, cls, name, value, x, y):
    n = MEL.create_material_expression(mat, cls, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    n.set_editor_property('group', 'FurnitureDebris')
    return n


def sample(mat, texture, sampler, x, y, mip=None):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
    n.set_editor_property('texture', texture)
    n.set_editor_property('sampler_type', sampler)
    if mip is not None:
        # the looked-up coordinates jump from shard to shard, so screen-space derivatives are useless: fixed mip
        n.set_editor_property('mip_value_mode', unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
        n.set_editor_property('const_mip_value', mip)
    return n


def build_material(lookup):
    name = 'M_BerlinFurnitureDebris_Decal'
    path = MATERIALS + '/' + name
    mat = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1700, 0)
    dc = MEL.create_material_expression(mat, unreal.MaterialExpressionDecalColor, -1700, 250)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -1400, 0)
    uv.set_editor_property('description', 'DebrisVariantUV')
    uv.set_editor_property('code', CELL_UV)
    uv.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    inputs = []
    for input_name in ('UV', 'CellR'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', input_name)
        inputs.append(ci)
    uv.set_editor_property('inputs', inputs)
    link(tc, '', uv, 'UV')
    link(dc, 'R', uv, 'CellR')
    look = sample(mat, lookup, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -1100, 0, mip=0)
    link(uv, '', look, 'UVs')
    atlas_uv = MEL.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -800, 0)
    link(look, 'R', atlas_uv, 'A')
    link(look, 'G', atlas_uv, 'B')
    colour = sample(mat, unreal.load_asset(TEXTURES + '/T_BerlinFurniture_D'), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -500, -300, mip=1)
    normal = sample(mat, unreal.load_asset(TEXTURES + '/T_BerlinFurniture_N'), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -500, 0, mip=1)
    orm = sample(mat, unreal.load_asset(TEXTURES + '/T_BerlinFurniture_ORM'), unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -500, 300, mip=1)
    for node in (colour, normal, orm):
        link(atlas_uv, '', node, 'UVs')
    tint = param(mat, unreal.MaterialExpressionVectorParameter, 'DebrisTint', unreal.LinearColor(0.8, 0.8, 0.8, 1.0), -500, -520)
    base = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, -350)
    link(colour, 'RGB', base, 'A')
    link(tint, '', base, 'B')
    opacity_scale = param(mat, unreal.MaterialExpressionScalarParameter, 'DebrisOpacity', 1.0, -500, 560)
    op1 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 450)
    link(look, 'B', op1, 'A')
    link(dc, 'A', op1, 'B')
    op2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 450)
    link(op1, '', op2, 'A')
    link(opacity_scale, '', op2, 'B')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(orm, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(op2, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    mi_path = MATERIALS + '/MI_BerlinFurnitureDebris_Decal'
    mi = unreal.load_asset(mi_path) if EAL.does_asset_exist(mi_path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MI_BerlinFurnitureDebris_Decal', MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    stats = MEL.get_statistics(mat)
    log('material built: %d expressions, PS instructions %s' % (MEL.get_num_material_expressions(mat),
                                                                 stats.get_editor_property('num_pixel_shader_instructions')))
    return mi


def run():
    for name in ('T_BerlinFurniture_D', 'T_BerlinFurniture_N', 'T_BerlinFurniture_ORM'):
        if not EAL.does_asset_exist(TEXTURES + '/' + name):
            raise RuntimeError('furniture atlas texture missing: ' + name)
    mi = build_material(import_map())
    log('done ' + mi.get_path_name())


if __name__ == '__main__':
    run()
