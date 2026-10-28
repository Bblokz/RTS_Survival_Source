"""Ground breakup as a material function, MF_RTS_LS_GroundBreakup (Docs/Landscape/LandscapeMaster.md).

MF_RTS_LS_GroundBreakup(LayerBlend, GrassLayer, GrassWeight, SandWeight, SoilColor, SoilNormal)
    -> Attributes, Grassiness, Damp, DirtColor
  Breaks up the painted grass edges with noise at three world scales: grass grows into the soft sand layers, bare soil
  bites into the grass, dry grass patches and damp hollows tint the result, and a fine value jitter hides tiling.
  Grassiness is the final, noise-broken grass amount (landscape grass follows it). Damp is the damp mask before
  Damp_Strength (low ground below Damp_Height plus large-noise hollows). DirtColor is the tinted bare-soil colour
  (one soil palette for every effect after it).
M_RTS_Landscape_DryCastle calls it between LandscapeLayerBlend_0 and MF_RTS_ColorVariation_Landscape behind
StaticSwitchParameter LS_NoiseBreakup (ls_master_build.run()), so the RVT output follows it too.
"""
import unreal

import ls_graph as G
from ls_graph import N, C, S, V, mul, mulc, addc, sat, lerp

FUNCTION_NAME = "MF_RTS_LS_GroundBreakup"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
GROUP = "Ground Breakup"


def noise_sample(wpxy, scale_param, offset, x, y):
    uv = N("Divide", x, y)
    C(wpxy, "", uv, "A")
    C(scale_param, "", uv, "B")
    off = N("Constant2Vector", x, y + 80, r=offset[0], g=offset[1])
    uvo = G.op("Add", uv, "", off, "", x + 140, y)
    t = N("TextureSample", x + 280, y, texture=unreal.load_asset(NOISE),
          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(uvo, "", t, "UVs")
    return t


def build_function():
    function = G.function_asset(FUNCTION_NAME, "Noise breakup of painted landscape layers: grass edges, dirt "
                                "patches, dry grass, damp hollows, value jitter. Also returns grassiness, damp and "
                                "the dirt colour for later effects.")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    layer_blend = G.function_input("LayerBlend", G.ATTRIBUTES, 0, X0 + 2800, Y0 - 100,
                                   "The painted layers (LandscapeLayerBlend).")
    grass_layer = G.function_input("GrassLayer", G.ATTRIBUTES, 1, X0 + 2800, Y0 + 20,
                                   "The grass layer's attributes, grown into the soft sand.")
    grass_w = G.function_input("GrassWeight", G.SCALAR, 2, X0 + 700, Y0 + 700, "Painted grass layer weight.")
    soft = G.function_input("SandWeight", G.SCALAR, 3, X0 + 1180, Y0 + 960,
                            "0..1: soft sand layers grass may grow into.")
    soil_bc = G.function_input("SoilColor", G.VECTOR3, 4, X0 + 2700, Y0 + 640,
                               "Bare soil base colour (reuse the master's sample: no extra fetch).")
    soil_n = G.function_input("SoilNormal", G.VECTOR3, 5, X0 + 2700, Y0 + 760, "Bare soil normal.")

    G.comment("Noise: the packed noise's R channel at three world scales (Breakup_*Scale), offset so the scales do "
              "not line up. Medium + fine make the grass edge noise.", X0 - 60, Y0 - 120, 1400, 760, G.INPUT_COLOR)
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

    G.comment("Grass edge: target grassiness g' = saturate(((G - 0.5) + (n - 0.5) * EdgeNoise) * EdgeContrast + "
              "0.5). Where g' > G grass grows into the sand; where g' < G bare soil bites into the grass.",
              X0 + 1040, Y0 - 20, 1800, 1100)
    edge_noise = S("Breakup_EdgeNoise", 1.6, X0 + 1100, Y0 + 460)
    edge_contrast = S("Breakup_EdgeContrast", 6.0, X0 + 1500, Y0 + 460)
    n_c = addc(n_edge, "", -0.5, X0 + 1300, Y0 + 320)
    n_s = mul(n_c, "", edge_noise, "", X0 + 1440, Y0 + 320)
    g_c = addc(grass_w, "", -0.5, X0 + 1100, Y0 + 700)
    g_sum = G.op("Add", g_c, "", n_s, "", X0 + 1580, Y0 + 400)
    g_con = mul(g_sum, "", edge_contrast, "", X0 + 1720, Y0 + 400)
    g_t = sat(addc(g_con, "", 0.5, X0 + 1860, Y0 + 400), "", X0 + 2000, Y0 + 400)
    delta = G.op("Subtract", g_t, "", grass_w, "", X0 + 2140, Y0 + 400)

    # Grass growing into soft ground: saturate(d) / (1 - G), only inside the painted falloff band.
    add_num = sat(delta, "", X0 + 2280, Y0 + 300)
    one_minus_g = N("OneMinus", X0 + 2140, Y0 + 220)
    C(grass_w, "", one_minus_g, "")
    inv_g = N("Max", X0 + 2280, Y0 + 220, const_b=0.001)
    C(one_minus_g, "", inv_g, "A")
    add_div = G.op("Divide", add_num, "", inv_g, "", X0 + 2420, Y0 + 260)
    band = sat(mulc(grass_w, "", 8.0, X0 + 2280, Y0 + 140), "", X0 + 2420, Y0 + 140)
    islands = S("Breakup_GrassIslands", 1.0, X0 + 2420, Y0 + 40)
    add_a = mul(mul(mul(add_div, "", band, "", X0 + 2560, Y0 + 200), "", soft, "", X0 + 2700, Y0 + 200),
                "", islands, "", X0 + 2840, Y0 + 200)
    add_a = sat(add_a, "", X0 + 2980, Y0 + 200)

    # Dirt bites in grass: saturate(-d) / max(G, floor). Dividing by G alone removes the grass
    # fully but lets faint grass falloff over other layers swap them for sand; the floor caps that.
    rem_num = sat(mulc(delta, "", -1.0, X0 + 2280, Y0 + 500), "", X0 + 2420, Y0 + 500)
    norm_floor = S("Breakup_DirtNormFloor", 0.5, X0 + 2280, Y0 + 640)
    g_safe = G.op("Max", grass_w, "", norm_floor, "", X0 + 2420, Y0 + 600)
    rem_div = G.op("Divide", rem_num, "", g_safe, "", X0 + 2560, Y0 + 520)
    rem_a = sat(rem_div, "", X0 + 2700, Y0 + 520)

    G.comment("Blend: grass layer over the sand where it grows in, then tinted bare soil (Breakup_Dirt*) where the "
              "dirt bites in.", X0 + 2640, Y0 - 160, 900, 1060, G.LAYER_COLOR)
    blend_grass = N("BlendMaterialAttributes", X0 + 3100, Y0 + 200)
    C(layer_blend, "", blend_grass, "A")
    C(grass_layer, "", blend_grass, "B")
    C(add_a, "", blend_grass, "Alpha")
    blend_dirt = N("BlendMaterialAttributes", X0 + 3300, Y0 + 400)
    C(blend_grass, "", blend_dirt, "A")
    dirt_tint = V("Breakup_DirtTint", unreal.LinearColor(0.95, 0.75, 0.52, 1.0), X0 + 2900, Y0 + 640)
    soil_col = mul(soil_bc, "", dirt_tint, "", X0 + 3100, Y0 + 640)
    soil = N("MakeMaterialAttributes", X0 + 3250, Y0 + 640)
    C(soil_col, "", soil, "BaseColor")
    C(soil_n, "", soil, "Normal")
    C(S("Breakup_DirtRoughness", 0.9, X0 + 3100, Y0 + 760), "", soil, "Roughness")
    C(soil, "", blend_dirt, "B")
    C(rem_a, "", blend_dirt, "Alpha")

    G.comment("Dry grass: large/medium noise above Dry_Threshold, on the final grassiness (tinted by Dry_GrassTint).",
              X0 + 840, Y0 + 1140, 1100, 340)
    dry_n = lerp(n_large, "R", n_med, "R", G.const(0.4, X0 + 900, Y0 + 1250), "", X0 + 1100, Y0 + 1200)
    dry_thr = S("Dry_Threshold", 0.55, X0 + 1100, Y0 + 1320)
    dry_soft = S("Dry_Softness", 0.2, X0 + 1100, Y0 + 1420)
    dry_max = G.op("Add", dry_thr, "", dry_soft, "", X0 + 1300, Y0 + 1380)
    dry_ss = G.smooth(dry_thr, dry_max, dry_n, "", X0 + 1460, Y0 + 1240)
    dry_str = S("Dry_Strength", 0.4, X0 + 1460, Y0 + 1420)
    dry_a = mul(mul(dry_ss, "", dry_str, "", X0 + 1640, Y0 + 1260), "", g_t, "", X0 + 1780, Y0 + 1260)

    G.comment("Damp: low ground below Damp_Height (fading over Damp_Falloff) plus large-noise hollows. Darkens and "
              "smooths the ground by Damp_Strength.", X0 + 140, Y0 + 1500, 1400, 300)
    wp_z = N("ComponentMask", X0 + 200, Y0 + 1500, r=False, g=False, b=True, a=False)
    C(wp, "", wp_z, "")
    damp_h = S("Damp_Height", -200.0, X0 + 200, Y0 + 1600)
    damp_f = S("Damp_Falloff", 400.0, X0 + 200, Y0 + 1700)
    low = G.op("Subtract", damp_h, "", wp_z, "", X0 + 420, Y0 + 1560)
    low_d = G.op("Divide", low, "", damp_f, "", X0 + 560, Y0 + 1560)
    low_a = sat(low_d, "", X0 + 700, Y0 + 1560)
    damp_thr = S("Damp_NoiseThreshold", 0.72, X0 + 700, Y0 + 1660)
    damp_thr_max = addc(damp_thr, "", 0.15, X0 + 900, Y0 + 1700)
    inv_large = N("OneMinus", X0 + 900, Y0 + 1560)
    C(n_large, "R", inv_large, "")
    damp_ss = G.smooth(damp_thr, damp_thr_max, inv_large, "", X0 + 1060, Y0 + 1580)
    damp_mx = G.op("Max", low_a, "", damp_ss, "", X0 + 1220, Y0 + 1560)
    damp_str = S("Damp_Strength", 0.5, X0 + 1220, Y0 + 1680)
    damp_a = mul(damp_mx, "", damp_str, "", X0 + 1380, Y0 + 1580)

    G.comment("Final colour and roughness: dry tint, damp darkening, fine value jitter (Breakup_FineValue). "
              "Breakup_DebugView 1 shows R = grass layer, G = target grassiness, B = soft sand.",
              X0 + 3640, Y0 + 160, 1100, 900, G.OUTPUT_COLOR)
    brk = N("BreakMaterialAttributes", X0 + 3500, Y0 + 400)
    C(blend_dirt, "", brk, "Attr")
    tint = V("Dry_GrassTint", unreal.LinearColor(1.1, 0.95, 0.5, 1.0), X0 + 1640, Y0 + 1100)
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

    dbg_rg = G.op("AppendVector", grass_w, "", g_t, "", X0 + 4300, Y0 + 300)
    dbg_rgb = G.op("AppendVector", dbg_rg, "", soft, "", X0 + 4420, Y0 + 300)
    dbg_amt = S("Breakup_DebugView", 0.0, X0 + 4300, Y0 + 200)
    bc_out = lerp(bc3, "", dbg_rgb, "", dbg_amt, "", X0 + 4420, Y0 + 500)

    make = N("MakeMaterialAttributes", X0 + 4500, Y0 + 500)
    C(bc_out, "", make, "BaseColor")
    for pin in ["Metallic", "Specular", "Normal", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    C(rough, "", make, "Roughness")

    G.function_output("Attributes", make, "", 0, X0 + 4800, Y0 + 500, "Broken-up ground.")
    G.function_output("Grassiness", g_t, "", 1, X0 + 4800, Y0 + 700, "Final grass amount after the edge noise.")
    G.function_output("Damp", damp_mx, "", 2, X0 + 4800, Y0 + 820, "Damp mask before Damp_Strength, 0..1.")
    G.function_output("DirtColor", soil_col, "", 3, X0 + 4800, Y0 + 940, "Tinted bare-soil colour.")
    return G.finish_function(function)
