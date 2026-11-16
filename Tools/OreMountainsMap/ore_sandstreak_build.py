"""Build the masked SandStreak material set (headless commandlet, never the shared editor):

  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi

1. Imports Saved/OreMountains/sandstreak/tex/T_SandStreak_{Wave,Puff,Grain}.png (ore_sandstreak_textures.py) as BC7,
   linear, Effects group, alpha-coverage mips on the noise channel (B).
2. Builds M_SandStreak_Masked (masked, two-sided, default lit with a world-up normal so the sand is lit like the
   ground under it, no decal response) and five MIs, one per NS_SandPush renderer slot.
3. Duplicates the untouched NS_SandPush to Dev/NS_SandPush_BeforeMasked for the one A/B verification run
   (delete it after the run).
The Niagara edits themselves (renderer materials, sort mode, spawn rates) go through the UECP bridge: see the doc
section "SandStreak masked rework" in Source/Docs/Campaign/OreMountainsMap.md.

Dynamic parameter channels keep the pack's meaning: R flipbook phase, G add-white, B emissive, A opacity power.

Known gap: a DynamicParameter node made from python has no named outputs until the package is reloaded, so the four
links from it (Param1 -> FlipbookUV.Phase, Param4 -> SandStreakMask.DynA, Param2 -> SandStreakColour.DynG,
Param3 -> the emissive Multiply.A) log LINK FAILED here. On 2026-11-16 they were wired afterwards with UECP
connect_material_nodes_bulk on the saved material (node 1 = the DynamicParameter), then validate_material + save.
Re-running this script rebuilds the material and needs that step again.
"""
import unreal

DST = '/Game/RTS_Survival/SpecialEffects/Dust/SandStreak'
TEX_SRC = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/tex/'
PACK_SYS = '/Game/DustAndSandStorm/SandStorm/Niagara/NS_SandPush'
BEFORE_SYS = DST + '/Dev/NS_SandPush_BeforeMasked'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
LOG = []


def log(msg):
    LOG.append(msg)
    unreal.log('SANDBUILD ' + msg)


# ------------------------------------------------------------------------------------------------ textures
def import_texture(name, coverage):
    task = unreal.AssetImportTask()
    task.filename = TEX_SRC + name + '.png'
    task.destination_path = DST + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(DST + '/Textures/' + name)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    if coverage:
        try:
            tex.set_editor_property('do_scale_mips_for_alpha_coverage', True)
            tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.5, 0.0))
            log('coverage mips on ' + name)
        except Exception as exc:
            log('COVERAGE MIPS NOT SET %s: %r' % (name, exc))
    else:
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    log('texture %s %dx%d' % (name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


# ------------------------------------------------------------------------------------------------ material
FLIP_UV = '''float n = max(N, 1.0);
float f = floor(saturate(Phase) * 0.9999 * n * n);
float col = fmod(f, n);
float row = floor(f / n);
return (UV + float2(col, row)) / n;'''

# Coverage test, see ore_sandstreak_textures.py: pack opacity c, boosted for the lower particle count, drawn iff it
# beats the grain noise in B. The material clips at 0.5.
MASK = '''float D = lerp(T.r, T.g, ChannelG);
float c = pow(saturate(PAlpha * D), max(DynA, 0.01)) * pow(saturate(NdotV), DotPower);
float boosted = 1.0 - pow(saturate(1.0 - c), Boost);
return 0.498 + boosted - T.b - Erode;'''

# The pack's base colour was ParticleColor * saturate(tex.rgb + AddWhite) with tex.rgb a grey 0.3-0.7 render of the
# sand; here that grey comes from the density (denser core = brighter), and the grain noise varies it per grain.
COLOUR = '''float D = lerp(T.r, T.g, ChannelG);
float shade = lerp(ShadeMin, ShadeMax, saturate(D * 1.5));
float grain = 1.0 + GrainContrast * (T.b - 0.5);
return PColor.rgb * saturate(shade + DynG) * grain * ColourGain;'''


class Graph:
    def __init__(self, mat):
        self.mat = mat
        self.y = 0

    def node(self, cls, x=-900, **props):
        self.y += 140
        e = MEL.create_material_expression(self.mat, cls, x, self.y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def scalar(self, name, value, group='SandStreak'):
        return self.node(unreal.MaterialExpressionScalarParameter, -1300, parameter_name=name, default_value=value, group=group)

    def custom(self, desc, code, inputs, out_type):
        pins = []
        for n in inputs:
            pin = unreal.CustomInput()
            pin.set_editor_property('input_name', n)
            pins.append(pin)
        return self.node(unreal.MaterialExpressionCustom, -500, code=code, description=desc, output_type=out_type,
                         inputs=pins)

    def link(self, a, a_out, b, b_in):
        ok = MEL.connect_material_expressions(a, a_out, b, b_in)
        if not ok:
            log('LINK FAILED %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))
        return ok


def build_material():
    path = DST + '/M_SandStreak_Masked'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SandStreak_Masked', DST, unreal.Material,
                                                                   unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('opacity_mask_clip_value', 0.5)
    mat.set_editor_property('dithered_lod_transition', False)
    mat.set_editor_property('tangent_space_normal', False)
    mat.set_editor_property('material_decal_response', unreal.MaterialDecalResponse.MDR_NONE)
    for flag in ('used_with_niagara_sprites', 'used_with_niagara_mesh_particles'):
        mat.set_editor_property(flag, True)

    g = Graph(mat)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -1300)
    dyn = g.node(unreal.MaterialExpressionDynamicParameter, -1300, default_value=unreal.LinearColor(1.0, 0.1, 1.0, 1.0))
    # A python-created node has unnamed outputs until ParamNames is written (PostEditChange renames them).
    dyn.set_editor_property('param_names', ['Param1', 'Param2', 'Param3', 'Param4'])
    pcol = g.node(unreal.MaterialExpressionParticleColor, -1300)
    rows = g.scalar('Rows/Columns', 6.0)
    channel = g.scalar('DensityChannelG', 0.0)
    dot_power = g.scalar('DotPower', 1.0)
    boost = g.scalar('Boost', 1.0)
    erode = g.scalar('Erode', 0.0)
    shade_min = g.scalar('ShadeMin', 0.35)
    shade_max = g.scalar('ShadeMax', 0.70)
    grain_contrast = g.scalar('GrainContrast', 0.25)
    colour_gain = g.scalar('ColourGain', 1.0)
    emissive_scale = g.scalar('EmissiveScale', 1.0)

    flip = g.custom('FlipbookUV', FLIP_UV, ['UV', 'Phase', 'N'], unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    g.link(uv, '', flip, 'UV')
    g.link(dyn, 'Param1', flip, 'Phase')
    g.link(rows, '', flip, 'N')

    tex = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -700, parameter_name='TX',
                 texture=unreal.load_asset(DST + '/Textures/T_SandStreak_Puff'),
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(flip, '', tex, 'UVs')

    cam = g.node(unreal.MaterialExpressionCameraVectorWS, -1100)
    nrm = g.node(unreal.MaterialExpressionVertexNormalWS, -1100)
    dot = g.node(unreal.MaterialExpressionDotProduct, -900)
    g.link(cam, '', dot, 'A')
    g.link(nrm, '', dot, 'B')
    absn = g.node(unreal.MaterialExpressionAbs, -800)
    g.link(dot, '', absn, '')

    mask = g.custom('SandStreakMask', MASK, ['T', 'ChannelG', 'PAlpha', 'DynA', 'NdotV', 'DotPower', 'Boost', 'Erode'],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    if not g.link(tex, 'RGBA', mask, 'T'):
        raise RuntimeError('texture RGBA output did not connect')
    g.link(channel, '', mask, 'ChannelG')
    g.link(pcol, 'A', mask, 'PAlpha')
    g.link(dyn, 'Param4', mask, 'DynA')
    g.link(absn, '', mask, 'NdotV')
    g.link(dot_power, '', mask, 'DotPower')
    g.link(boost, '', mask, 'Boost')
    g.link(erode, '', mask, 'Erode')

    colour = g.custom('SandStreakColour', COLOUR, ['T', 'ChannelG', 'PColor', 'DynG', 'ShadeMin', 'ShadeMax',
                                                   'GrainContrast', 'ColourGain'],
                      unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.link(tex, 'RGBA', colour, 'T')
    g.link(channel, '', colour, 'ChannelG')
    g.link(pcol, '', colour, 'PColor')
    g.link(dyn, 'Param2', colour, 'DynG')
    g.link(shade_min, '', colour, 'ShadeMin')
    g.link(shade_max, '', colour, 'ShadeMax')
    g.link(grain_contrast, '', colour, 'GrainContrast')
    g.link(colour_gain, '', colour, 'ColourGain')

    emis = g.node(unreal.MaterialExpressionMultiply, -300)
    g.link(colour, '', emis, 'A')
    emis_amount = g.node(unreal.MaterialExpressionMultiply, -500)
    g.link(dyn, 'Param3', emis_amount, 'A')
    g.link(emissive_scale, '', emis_amount, 'B')
    g.link(emis_amount, '', emis, 'B')

    up = g.node(unreal.MaterialExpressionConstant3Vector, -300, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    rough = g.node(unreal.MaterialExpressionConstant, -300, r=1.0)
    spec = g.node(unreal.MaterialExpressionConstant, -300, r=0.1)

    MP = unreal.MaterialProperty
    for expr, prop in ((colour, MP.MP_BASE_COLOR), (emis, MP.MP_EMISSIVE_COLOR), (mask, MP.MP_OPACITY_MASK),
                       (up, MP.MP_NORMAL), (rough, MP.MP_ROUGHNESS), (spec, MP.MP_SPECULAR)):
        if not MEL.connect_material_property(expr, '', prop):
            log('PROPERTY LINK FAILED ' + str(prop))
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    log('material nodes %d' % MEL.get_num_material_expressions(mat))
    return mat


# ------------------------------------------------------------------------------------------------ instances
# Boost = old live count / new live count per renderer (spawn rate x mean lifetime, pack round-2 rates against the
# rates ore_sandstreak_niagara sets): mesh 110 -> 40/s, smoke A 35 -> 10/s, smoke B 35 -> 6/s, grains 2000 -> 400/s.
INSTANCES = {
    'MI_SandStreak_WaveA': {'TX': 'T_SandStreak_Wave', 'Rows/Columns': 6, 'DensityChannelG': 0, 'DotPower': 2.0, 'Boost': 2.75},
    'MI_SandStreak_WaveB': {'TX': 'T_SandStreak_Wave', 'Rows/Columns': 6, 'DensityChannelG': 1, 'DotPower': 2.0, 'Boost': 2.75},
    'MI_SandStreak_PuffA': {'TX': 'T_SandStreak_Puff', 'Rows/Columns': 6, 'DensityChannelG': 0, 'DotPower': 1.0, 'Boost': 3.5},
    'MI_SandStreak_PuffB': {'TX': 'T_SandStreak_Puff', 'Rows/Columns': 6, 'DensityChannelG': 1, 'DotPower': 1.0, 'Boost': 5.8},
    'MI_SandStreak_Grain': {'TX': 'T_SandStreak_Grain', 'Rows/Columns': 1, 'DensityChannelG': 0, 'DotPower': 0.0, 'Boost': 1.0,
                            'GrainContrast': 0.0, 'ShadeMin': 0.7},
}


def build_instances(mat):
    factory = unreal.MaterialInstanceConstantFactoryNew()
    for name, params in INSTANCES.items():
        path = DST + '/' + name
        if EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DST, unreal.MaterialInstanceConstant, factory)
        MEL.set_material_instance_parent(mi, mat)
        for key, value in params.items():
            if key == 'TX':
                MEL.set_material_instance_texture_parameter_value(mi, 'TX', unreal.load_asset(DST + '/Textures/' + value))
            else:
                MEL.set_material_instance_scalar_parameter_value(mi, key, float(value))
        MEL.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
        log('instance ' + name)


def main():
    import_texture('T_SandStreak_Wave', True)
    import_texture('T_SandStreak_Puff', True)
    import_texture('T_SandStreak_Grain', False)
    mat = build_material()
    build_instances(mat)
    if not EAL.does_asset_exist(BEFORE_SYS):
        EAL.duplicate_asset(PACK_SYS, BEFORE_SYS)
        unreal.EditorAssetLibrary.save_asset(BEFORE_SYS, False)
    log('before copy %s' % EAL.does_asset_exist(BEFORE_SYS))
    log('done')


try:
    main()
except Exception as exc:
    log('ERROR ' + repr(exc))
    raise
