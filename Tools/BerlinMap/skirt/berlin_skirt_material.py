"""Builds the Berlin horizon-skirt material (exec in an editor; the survival M_SM_HorizonSkirt is not touched).

    build_material()  ->  /Game/RTS_Survival/Materials/ground/Landscape/Berlin/M_Berlin_HorizonSkirt
                          /Game/RTS_Survival/Materials/ground/Landscape/Berlin/MI_Berlin_HorizonSkirt  (on the planes)

The plane is the landscape's BerlinRubble layer continued on world XY, node for node what M_RTS_Landscape_Berlin does
for a vertex painted pure rubble (the rim band is painted pure rubble for that reason):
  albedo   T_Berlin_BerlinRubble_D at 5 m tiles, mixed with a second sample at 0.23 x (+0.37) by T_RTSNoise_MacroVariation
           (4000 cm tile, R * 0.4 + 0.2); roughness = albedo alpha; normal = T_Berlin_BerlinRubble_N at 5 m
  colour variation   MF_RTS_ColorVariation_Landscape rebuilt on world position (the function reads landscape UVs, which
           a plane does not have): three noise samples at landscape-local metres x 0.2134 / 0.05341 / 0.002 x 4, each
           R + 0.5, multiplied; base = lerp(base, base x product, 0.5)
Past the rim (distance outside the landscape square, RimHalfSize):
  soot     one more noise sample (SootSize) darkens and lightens the ground in drifts hundreds of metres wide; it is
           faded in over the first SootFadeIn cm, so the join itself is the pure landscape look
  fade     brightness falls to FarBrightness at FadeLength (quadratic; 500 m past the rim: 800 m still left a lit
           plain against a hard horizon): the map has a black sky and no fog, so the ground goes dark toward the
           horizon instead of ending in a line
  far      roughness -> 1, normal -> flat, specular -> 0 over FlattenLength (no glints at grazing angles)
Lit, opaque, 9 texture samples on the shared wrap sampler (no sampler slots).
"""
import unreal

DIR = '/Game/RTS_Survival/Materials/ground/Landscape/Berlin'
MAT, MI = 'M_Berlin_HorizonSkirt', 'MI_Berlin_HorizonSkirt'
TEX = DIR + '/Textures/T_Berlin_BerlinRubble_'
NOISE = '/Game/RTS_Survival/Textures/Noise/T_RTSNoise_MacroVariation'
LANDSCAPE_MI = DIR + '/MI_RTS_Landscape_Berlin'
mel = unreal.MaterialEditingLibrary
ST = unreal.MaterialSamplerType


def build_material(half_size=59850.0, landscape_origin=(-59850.0, -59850.0)):
    if unreal.EditorAssetLibrary.does_asset_exist(DIR + '/' + MAT):
        mat = unreal.load_asset(DIR + '/' + MAT)
        for _ in range(12):           # one call leaves about half of the nodes behind
            if mel.get_num_material_expressions(mat) == 0:
                break
            mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT, DIR, unreal.Material, unreal.MaterialFactoryNew())
    pos = [-2600, -600]

    def N(cls, **props):
        e = mel.create_material_expression(mat, getattr(unreal, 'MaterialExpression' + cls), pos[0], pos[1])
        pos[1] += 90
        if pos[1] > 900:
            pos[0], pos[1] = pos[0] + 260, -600
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def C(a, out, b, inp):
        if not mel.connect_material_expressions(a, out, b, inp):
            raise RuntimeError('connect failed %s.%s -> %s.%s' % (a.get_name(), out, b.get_name(), inp))

    def S(name, value, group='Skirt'):
        return N('ScalarParameter', parameter_name=name, default_value=value, group=group)

    def mulc(a, out, k):
        e = N('Multiply', const_b=k)
        C(a, out, e, 'A')
        return e

    def addc(a, out, k):
        e = N('Add', const_b=k)
        C(a, out, e, 'A')
        return e

    def op(cls, a, ao, b, bo):
        e = N(cls)
        C(a, ao, e, 'A')
        C(b, bo, e, 'B')
        return e

    def lerp(a, ao, b, bo, al, alo):
        e = N('LinearInterpolate')
        C(a, ao, e, 'A')
        C(b, bo, e, 'B')
        C(al, alo, e, 'Alpha')
        return e

    def one(a, out, cls):
        e = N(cls)
        C(a, out, e, '')
        return e

    def sample(path, uv, kind):
        e = N('TextureSample', texture=unreal.load_asset(path), sampler_type=kind,
              sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        C(uv, '', e, 'UVs')
        return e

    # the landscape instance's own rubble values (defaults unless someone overrode them)
    lmi = unreal.load_asset(LANDSCAPE_MI)

    def lval(name, default):
        try:
            v = mel.get_material_instance_scalar_parameter_value(lmi, name)
            return v if v else default
        except Exception:  # noqa
            return default

    tile_m = lval('BerlinRubble_TileSize', 5.0)
    macro_scale = lval('Berlin_MacroScale', 0.23)
    blend_min, blend_max = lval('Berlin_MacroBlendMin', 0.2), lval('Berlin_MacroBlendMax', 0.6)
    noise_size = lval('Berlin_NoiseSize', 4000.0)
    print('landscape rubble values: tile %.2f m, macro scale %.3f, blend %.2f..%.2f, noise %.0f cm' % (
        tile_m, macro_scale, blend_min, blend_max, noise_size))

    world = N('WorldPosition')
    xy = N('ComponentMask', r=True, g=True, b=False, a=False)
    C(world, '', xy, '')
    uv = mulc(xy, '', 0.01 / tile_m)
    uv_macro = addc(mulc(uv, '', macro_scale), '', 0.37)
    noise = sample(NOISE, mulc(xy, '', 1.0 / noise_size), ST.SAMPLERTYPE_COLOR)
    macro_alpha = addc(mulc(noise, 'R', blend_max - blend_min), '', blend_min)
    d_tile = sample(TEX + 'D', uv, ST.SAMPLERTYPE_COLOR)
    d_macro = sample(TEX + 'D', uv_macro, ST.SAMPLERTYPE_COLOR)
    n_tile = sample(TEX + 'N', uv, ST.SAMPLERTYPE_NORMAL)
    d = lerp(d_tile, 'RGBA', d_macro, 'RGBA', macro_alpha, '')
    base = N('ComponentMask', r=True, g=True, b=True, a=False)
    C(d, '', base, '')
    rough = N('ComponentMask', r=False, g=False, b=False, a=True)
    C(d, '', rough, '')
    rough = one(rough, '', 'Saturate')

    # MF_RTS_ColorVariation_Landscape on world position (InNoiseTillling 4, InNoiseStrength 0.5)
    lc = N('Add')                                    # landscape-local metres
    C(mulc(xy, '', 0.01), '', lc, 'A')
    off = N('Constant2Vector', r=-landscape_origin[0] * 0.01, g=-landscape_origin[1] * 0.01)
    C(off, '', lc, 'B')
    var = None
    for k in (0.2134, 0.05341, 0.002):
        s = addc(sample(NOISE, mulc(lc, '', k * 4.0), ST.SAMPLERTYPE_COLOR), 'R', 0.5)
        var = s if var is None else op('Multiply', var, '', s, '')
    varied = lerp(base, '', op('Multiply', var, '', base, ''), '', N('Constant', r=0.5), '')

    # distance outside the landscape square
    half = S('RimHalfSize', half_size)
    outside = N('Max', const_b=0.0)
    C(op('Subtract', one(xy, '', 'Abs'), '', half, ''), '', outside, 'A')
    dist = one(outside, '', 'Length')
    fade_len = S('FadeLength', 50000.0)
    far_b = S('FarBrightness', 0.02)
    k = one(op('Divide', dist, '', fade_len, ''), '', 'Saturate')
    inv = one(k, '', 'OneMinus')
    inv2 = op('Multiply', inv, '', inv, '')
    fade = lerp(far_b, '', N('Constant', r=1.0), '', inv2, '')

    soot_size = S('SootSize', 70000.0)
    soot_uv = addc(op('Divide', xy, '', soot_size, ''), '', 0.31)
    soot_n = one(mulc(sample(NOISE, soot_uv, ST.SAMPLERTYPE_COLOR), 'G', 2.2), '', 'Saturate')
    soot_range = lerp(S('SootDark', 0.5), '', S('SootLight', 1.25), '', soot_n, '')
    soot_amp = op('Multiply', one(op('Divide', dist, '', S('SootFadeIn', 15000.0), ''), '', 'Saturate'), '',
                  S('SootStrength', 0.7), '')
    soot = lerp(N('Constant', r=1.0), '', soot_range, '', soot_amp, '')
    color = op('Multiply', op('Multiply', varied, '', soot, ''), '', fade, '')

    flat_k = one(op('Divide', dist, '', S('FlattenLength', 30000.0), ''), '', 'Saturate')
    rough_out = lerp(rough, '', N('Constant', r=1.0), '', flat_k, '')
    normal_out = lerp(n_tile, 'RGB', N('Constant3Vector', constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0)), '', flat_k, '')
    spec_out = lerp(N('Constant', r=0.5), '', N('Constant', r=0.0), '', flat_k, '')

    mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough_out, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(normal_out, '', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(spec_out, '', unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    if unreal.EditorAssetLibrary.does_asset_exist(DIR + '/' + MI):
        mi = unreal.load_asset(DIR + '/' + MI)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MI, DIR, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.update_material_instance(mi)
    ok2 = unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    print('material built: %d expressions, saved %s / %s' % (mel.get_num_material_expressions(mat), ok, ok2))
    return mi


def material_stats():
    mat = unreal.load_asset(DIR + '/' + MAT)
    st = mel.get_statistics(mat)
    print('M_Berlin_HorizonSkirt: pixel instructions %d, vertex %d, samplers %d, pixel texture samples %d, vt samples %d' % (
        st.num_pixel_shader_instructions, st.num_vertex_shader_instructions, st.num_samplers,
        st.num_pixel_texture_samples, st.num_virtual_texture_samples))
    land = unreal.load_asset(DIR + '/MI_RTS_Landscape_Berlin')
    st = mel.get_statistics(land)
    print('MI_RTS_Landscape_Berlin (for comparison): pixel instructions %d, samplers %d, texture samples %d' % (
        st.num_pixel_shader_instructions, st.num_samplers, st.num_pixel_texture_samples))
    surv = unreal.load_asset('/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_HorizonSkirt')
    st = mel.get_statistics(surv)
    print('M_SM_HorizonSkirt (survival, for comparison): pixel instructions %d, samplers %d, texture samples %d' % (
        st.num_pixel_shader_instructions, st.num_samplers, st.num_pixel_texture_samples))
