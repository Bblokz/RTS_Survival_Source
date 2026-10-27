"""Adds the switched ground-breakup chain to M_RTS_Landscape_DryCastle.

Re-runnable: every node it creates carries DESC in its description and is removed first.
Inserted between LandscapeLayerBlend_0 and MF_RTS_ColorVariation_Landscape (Call_3), so the
material output and the RVT output (GetMaterialAttributes_0 reads Call_3) both follow it.
StaticSwitchParameter LS_NoiseBreakup defaults to False, which compiles the original path.
"""
import unreal

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
DESC = "LSBreakup"
GROUP = "Ground Breakup"

mel = unreal.MaterialEditingLibrary
mat = unreal.load_asset(MAT_PATH)
failures = []

USED = ["WorldPosition", "ComponentMask", "Divide", "Add", "Subtract", "Multiply", "TextureSample",
        "LinearInterpolate", "LandscapeLayerSample", "Saturate", "Max", "SmoothStep", "OneMinus",
        "BlendMaterialAttributes", "BreakMaterialAttributes", "MakeMaterialAttributes",
        "StaticSwitchParameter", "ScalarParameter", "VectorParameter", "Constant", "Constant2Vector", "AppendVector", "Comment"]


def remove_previous():
    removed = 0
    for cls in USED:
        for i in range(0, 400):
            e = unreal.find_object(mat, "MaterialExpression%s_%d" % (cls, i))
            if e and e.get_editor_property("desc") == DESC:
                mel.delete_material_expression(mat, e)
                removed += 1
    return removed


def find(name):
    e = unreal.find_object(mat, name)
    if not e:
        raise RuntimeError("missing " + name)
    return e


def find_sample(texture_name):
    """Existing TextureSample of the master by texture; reusing it adds no texture fetch."""
    for i in range(0, 200):
        e = unreal.find_object(mat, "MaterialExpressionTextureSample_%d" % i)
        if e and e.get_editor_property("desc") != DESC:
            t = e.get_editor_property("texture")
            if t and t.get_name() == texture_name:
                return e
    raise RuntimeError("no sample of " + texture_name)


def N(cls, x, y, **props):
    e = mel.create_material_expression(mat, getattr(unreal, "MaterialExpression" + cls), x, y)
    e.set_editor_property("desc", DESC)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        failures.append("%s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def S(name, value, x, y):
    return N("ScalarParameter", x, y, parameter_name=name, default_value=value, group=GROUP)


def mul(a, a_out, b, b_out, x, y):
    e = N("Multiply", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    return e


def mulc(a, a_out, k, x, y):
    e = N("Multiply", x, y, const_b=k)
    C(a, a_out, e, "A")
    return e


def addc(a, a_out, k, x, y):
    e = N("Add", x, y, const_b=k)
    C(a, a_out, e, "A")
    return e


def sat(a, a_out, x, y):
    e = N("Saturate", x, y)
    C(a, a_out, e, "")
    return e


def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    e = N("LinearInterpolate", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    C(alpha, alpha_out, e, "Alpha")
    return e


def noise_sample(wpxy, scale_param, offset, x, y):
    uv = N("Divide", x, y)
    C(wpxy, "", uv, "A")
    C(scale_param, "", uv, "B")
    off = N("Constant2Vector", x, y + 80, r=offset[0], g=offset[1])
    uvo = N("Add", x + 140, y)
    C(uv, "", uvo, "A")
    C(off, "", uvo, "B")
    t = N("TextureSample", x + 280, y, texture=unreal.load_asset(NOISE),
          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(uvo, "", t, "UVs")
    return t


def build():
    layer_blend = find("MaterialExpressionLandscapeLayerBlend_0")
    color_var = find("MaterialExpressionMaterialFunctionCall_3")
    grass_layer = find("MaterialExpressionMaterialFunctionCall_1")
    soil_bc = find_sample("T_Soil_1_BC")
    soil_n = find_sample("T_StoneGround_1_N")

    X0, Y0 = -3200, -3400
    N("Comment", X0 - 60, Y0 - 120, text="Ground breakup (LS_NoiseBreakup): noise-broken grass edges, dirt "
      "patches, dry grass, damp hollows, fine value jitter")

    # Noise at three world scales from the R channel, offset so the scales do not line up.
    wp = N("WorldPosition", X0, Y0)
    wpxy = N("ComponentMask", X0 + 200, Y0, r=True, g=True, b=False, a=False)
    C(wp, "", wpxy, "")
    s_large = S("Breakup_LargeScale", 30000.0, X0 + 200, Y0 + 120)
    s_med = S("Breakup_MediumScale", 6000.0, X0 + 200, Y0 + 320)
    s_fine = S("Breakup_FineScale", 800.0, X0 + 200, Y0 + 520)
    n_large = noise_sample(wpxy, s_large, (0.0, 0.0), X0 + 420, Y0 + 120)
    n_med = noise_sample(wpxy, s_med, (0.37, 0.61), X0 + 420, Y0 + 320)
    n_fine = noise_sample(wpxy, s_fine, (0.13, 0.83), X0 + 420, Y0 + 520)
    fine_w = S("Breakup_FineWeight", 0.35, X0 + 900, Y0 + 420)
    n_edge = lerp(n_med, "R", n_fine, "R", fine_w, "", X0 + 1100, Y0 + 320)

    # Layer weights.
    grass_w = N("LandscapeLayerSample", X0 + 900, Y0 + 700, parameter_name="GrassCastle", preview_weight=0.0)
    soft_sum = N("Add", X0 + 1100, Y0 + 900)
    w_desert = N("LandscapeLayerSample", X0 + 900, Y0 + 860, parameter_name="Desert", preview_weight=0.0)
    w_sand = N("LandscapeLayerSample", X0 + 900, Y0 + 960, parameter_name="RTSSand", preview_weight=0.0)
    w_gray = N("LandscapeLayerSample", X0 + 900, Y0 + 1060, parameter_name="GraySand", preview_weight=0.0)
    C(w_desert, "", soft_sum, "A")
    C(w_sand, "", soft_sum, "B")
    soft_sum2 = N("Add", X0 + 1240, Y0 + 960)
    C(soft_sum, "", soft_sum2, "A")
    C(w_gray, "", soft_sum2, "B")
    soft = sat(soft_sum2, "", X0 + 1380, Y0 + 960)

    # Target grassiness g' = saturate(((G - 0.5) + (n - 0.5) * EdgeNoise) * EdgeContrast + 0.5)
    edge_noise = S("Breakup_EdgeNoise", 1.6, X0 + 1100, Y0 + 460)
    edge_contrast = S("Breakup_EdgeContrast", 6.0, X0 + 1500, Y0 + 460)
    n_c = addc(n_edge, "", -0.5, X0 + 1300, Y0 + 320)
    n_s = mul(n_c, "", edge_noise, "", X0 + 1440, Y0 + 320)
    g_c = addc(grass_w, "", -0.5, X0 + 1100, Y0 + 700)
    g_sum = N("Add", X0 + 1580, Y0 + 400)
    C(g_c, "", g_sum, "A")
    C(n_s, "", g_sum, "B")
    g_con = mul(g_sum, "", edge_contrast, "", X0 + 1720, Y0 + 400)
    g_t = sat(addc(g_con, "", 0.5, X0 + 1860, Y0 + 400), "", X0 + 2000, Y0 + 400)
    delta = N("Subtract", X0 + 2140, Y0 + 400)
    C(g_t, "", delta, "A")
    C(grass_w, "", delta, "B")

    # Grass growing into soft ground: saturate(d) / (1 - G), only inside the painted falloff band.
    add_num = sat(delta, "", X0 + 2280, Y0 + 300)
    one_minus_g = N("OneMinus", X0 + 2140, Y0 + 220)
    C(grass_w, "", one_minus_g, "")
    inv_g = N("Max", X0 + 2280, Y0 + 220, const_b=0.001)
    C(one_minus_g, "", inv_g, "A")
    add_div = N("Divide", X0 + 2420, Y0 + 260)
    C(add_num, "", add_div, "A")
    C(inv_g, "", add_div, "B")
    band = sat(mulc(grass_w, "", 8.0, X0 + 2280, Y0 + 140), "", X0 + 2420, Y0 + 140)
    islands = S("Breakup_GrassIslands", 1.0, X0 + 2420, Y0 + 40)
    add_a = mul(mul(mul(add_div, "", band, "", X0 + 2560, Y0 + 200), "", soft, "", X0 + 2700, Y0 + 200),
                "", islands, "", X0 + 2840, Y0 + 200)
    add_a = sat(add_a, "", X0 + 2980, Y0 + 200)

    # Dirt bites in grass: saturate(-d) / max(G, floor). Dividing by G alone removes the grass
    # fully but lets faint grass falloff over other layers swap them for sand; the floor caps that.
    rem_num = sat(mulc(delta, "", -1.0, X0 + 2280, Y0 + 500), "", X0 + 2420, Y0 + 500)
    norm_floor = S("Breakup_DirtNormFloor", 0.5, X0 + 2280, Y0 + 640)
    g_safe = N("Max", X0 + 2420, Y0 + 600)
    C(grass_w, "", g_safe, "A")
    C(norm_floor, "", g_safe, "B")
    rem_div = N("Divide", X0 + 2560, Y0 + 520)
    C(rem_num, "", rem_div, "A")
    C(g_safe, "", rem_div, "B")
    rem_a = sat(rem_div, "", X0 + 2700, Y0 + 520)

    blend_grass = N("BlendMaterialAttributes", X0 + 3100, Y0 + 200)
    C(layer_blend, "", blend_grass, "A")
    C(grass_layer, "", blend_grass, "B")
    C(add_a, "", blend_grass, "Alpha")
    blend_dirt = N("BlendMaterialAttributes", X0 + 3300, Y0 + 400)
    C(blend_grass, "", blend_dirt, "A")
    # Bare soil: the master's T_Soil_1_BC / T_StoneGround_1_N samples (1.08 m tiling), tinted.
    dirt_tint = N("VectorParameter", X0 + 2900, Y0 + 640, parameter_name="Breakup_DirtTint",
                  default_value=unreal.LinearColor(0.95, 0.75, 0.52, 1.0), group=GROUP)
    soil_col = mul(soil_bc, "RGB", dirt_tint, "", X0 + 3100, Y0 + 640)
    soil = N("MakeMaterialAttributes", X0 + 3250, Y0 + 640)
    C(soil_col, "", soil, "BaseColor")
    C(soil_n, "RGB", soil, "Normal")
    C(S("Breakup_DirtRoughness", 0.9, X0 + 3100, Y0 + 760), "", soil, "Roughness")
    C(soil, "", blend_dirt, "B")
    C(rem_a, "", blend_dirt, "Alpha")

    # Dry grass by large/medium noise, on the final grassiness.
    dry_n = lerp(n_large, "R", n_med, "R", N("Constant", X0 + 900, Y0 + 1250, r=0.4), "", X0 + 1100, Y0 + 1200)
    dry_thr = S("Dry_Threshold", 0.55, X0 + 1100, Y0 + 1320)
    dry_soft = S("Dry_Softness", 0.2, X0 + 1100, Y0 + 1420)
    dry_max = N("Add", X0 + 1300, Y0 + 1380)
    C(dry_thr, "", dry_max, "A")
    C(dry_soft, "", dry_max, "B")
    dry_ss = N("SmoothStep", X0 + 1460, Y0 + 1240)
    C(dry_thr, "", dry_ss, "Min")
    C(dry_max, "", dry_ss, "Max")
    C(dry_n, "", dry_ss, "Value")
    dry_str = S("Dry_Strength", 0.4, X0 + 1460, Y0 + 1420)
    dry_a = mul(mul(dry_ss, "", dry_str, "", X0 + 1640, Y0 + 1260), "", g_t, "", X0 + 1780, Y0 + 1260)

    # Damp: low ground below Damp_Height, plus large-noise hollows.
    wp_z = N("ComponentMask", X0 + 200, Y0 + 1500, r=False, g=False, b=True, a=False)
    C(wp, "", wp_z, "")
    damp_h = S("Damp_Height", -200.0, X0 + 200, Y0 + 1600)
    damp_f = S("Damp_Falloff", 400.0, X0 + 200, Y0 + 1700)
    low = N("Subtract", X0 + 420, Y0 + 1560)
    C(damp_h, "", low, "A")
    C(wp_z, "", low, "B")
    low_d = N("Divide", X0 + 560, Y0 + 1560)
    C(low, "", low_d, "A")
    C(damp_f, "", low_d, "B")
    low_a = sat(low_d, "", X0 + 700, Y0 + 1560)
    damp_thr = S("Damp_NoiseThreshold", 0.72, X0 + 700, Y0 + 1660)
    damp_thr_max = addc(damp_thr, "", 0.15, X0 + 900, Y0 + 1700)
    inv_large = N("OneMinus", X0 + 900, Y0 + 1560)
    C(n_large, "R", inv_large, "")
    damp_ss = N("SmoothStep", X0 + 1060, Y0 + 1580)
    C(damp_thr, "", damp_ss, "Min")
    C(damp_thr_max, "", damp_ss, "Max")
    C(inv_large, "", damp_ss, "Value")
    damp_mx = N("Max", X0 + 1220, Y0 + 1560)
    C(low_a, "", damp_mx, "A")
    C(damp_ss, "", damp_mx, "B")
    damp_str = S("Damp_Strength", 0.5, X0 + 1220, Y0 + 1680)
    damp_a = mul(damp_mx, "", damp_str, "", X0 + 1380, Y0 + 1580)

    # Colour and roughness adjustments on the broken-up attributes.
    brk = N("BreakMaterialAttributes", X0 + 3500, Y0 + 400)
    C(blend_dirt, "", brk, "Attr")
    tint = N("VectorParameter", X0 + 1640, Y0 + 1100, parameter_name="Dry_GrassTint",
             default_value=unreal.LinearColor(1.1, 0.95, 0.5, 1.0), group=GROUP)
    bc_t = mul(brk, "BaseColor", tint, "", X0 + 3700, Y0 + 560)
    bc1 = lerp(brk, "BaseColor", bc_t, "", dry_a, "", X0 + 3850, Y0 + 480)
    darken = S("Damp_Darken", 0.55, X0 + 3700, Y0 + 700)
    bc_d = mul(bc1, "", darken, "", X0 + 4000, Y0 + 640)
    bc2 = lerp(bc1, "", bc_d, "", damp_a, "", X0 + 4150, Y0 + 560)
    fine_val = S("Breakup_FineValue", 0.2, X0 + 3700, Y0 + 800)
    jit = mul(addc(n_fine, "R", -0.5, X0 + 3850, Y0 + 860), "", fine_val, "", X0 + 4000, Y0 + 800)
    jit1 = addc(jit, "", 1.0, X0 + 4150, Y0 + 800)
    bc3 = mul(bc2, "", jit1, "", X0 + 4300, Y0 + 600)
    rough_mul = S("Damp_RoughnessMul", 0.65, X0 + 3700, Y0 + 960)
    rough_d = mul(brk, "Roughness", rough_mul, "", X0 + 4000, Y0 + 960)
    rough = lerp(brk, "Roughness", rough_d, "", damp_a, "", X0 + 4150, Y0 + 940)

    # Debug view: R = GrassCastle weight, G = target grassiness, B = soft-ground mask.
    dbg_rg = N("AppendVector", X0 + 4300, Y0 + 300)
    C(grass_w, "", dbg_rg, "A")
    C(g_t, "", dbg_rg, "B")
    dbg_rgb = N("AppendVector", X0 + 4420, Y0 + 300)
    C(dbg_rg, "", dbg_rgb, "A")
    C(soft, "", dbg_rgb, "B")
    dbg_amt = S("Breakup_DebugView", 0.0, X0 + 4300, Y0 + 200)
    bc_out = lerp(bc3, "", dbg_rgb, "", dbg_amt, "", X0 + 4420, Y0 + 500)

    make = N("MakeMaterialAttributes", X0 + 4500, Y0 + 500)
    C(bc_out, "", make, "BaseColor")
    for pin in ["Metallic", "Specular", "Normal", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    C(rough, "", make, "Roughness")

    switch = N("StaticSwitchParameter", X0 + 4750, Y0 + 600, parameter_name="LS_NoiseBreakup",
               default_value=False, group=GROUP)
    C(make, "", switch, "True")
    C(layer_blend, "", switch, "False")
    C(switch, "", color_var, "InputMaterial")


removed = remove_previous()
build()
mel.recompile_material(mat)
print("removed", removed, "failures", failures)
