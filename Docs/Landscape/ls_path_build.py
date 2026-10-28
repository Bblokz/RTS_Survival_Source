"""Adds surface-aware detail to M_GroundPath_Dirt (ribbon material of BP_GroundPathSpline).

Re-runnable: every node it creates carries DESC in its description and is removed first.
Behind StaticSwitchParameter PathSurfaceAware (default False -> original BaseColor/Roughness/Opacity).
The ribbon samples RVT_Environment (written by the landscape) under itself and classifies the ground
by green chromaticity into green / dry / sand masks:
  green: grass strip down the centre, grass tufts eating the edges, darker moist dirt, wet darker ruts
  dry:   weaker straw centre strip
  sand:  path takes the sand's hue, wind-aligned sand drifts partly bury it
Ribbon UVs: u across the path (0.5 = centre line), v along it.
Decals (M_GroundPathDecal) get the colour/roughness part only: run(DECAL, cuts=False).
Usage: import ls_path_build as P; P.run(P.RIBBON, True); P.run(P.DECAL, False)
"""
import unreal

RIBBON = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/M_GroundPath_Dirt"
DECAL = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/M_GroundPathDecal"
RVT_PATH = "/Game/RTS_Survival/Meshes/RVTs/RVT_Environment"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
DESC = "PathSurface"
GROUP = "Surface Aware"

mel = unreal.MaterialEditingLibrary
mat = None
failures = []

USED = ["WorldPosition", "ComponentMask", "Divide", "Add", "Subtract", "Multiply", "TextureSample",
        "LinearInterpolate", "Saturate", "Max", "Min", "SmoothStep", "OneMinus", "Abs", "DotProduct",
        "RuntimeVirtualTextureSample", "TextureCoordinate", "StaticSwitchParameter", "ScalarParameter",
        "VectorParameter", "Constant", "Constant2Vector", "Constant3Vector", "AppendVector", "Exponential",
        "Comment"]


def remove_previous():
    removed = 0
    for cls in USED:
        for i in range(0, 400):
            e = unreal.find_object(mat, "MaterialExpression%s_%d" % (cls, i))
            if e and e.get_editor_property("desc") == DESC:
                mel.delete_material_expression(mat, e)
                removed += 1
    return removed


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


def op(cls, a, a_out, b, b_out, x, y):
    e = N(cls, x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    return e


def opc(cls, a, a_out, k, x, y):
    e = N(cls, x, y, const_b=k)
    C(a, a_out, e, "A")
    return e


def one(cls, a, a_out, x, y):
    e = N(cls, x, y)
    C(a, a_out, e, "")
    return e


def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    e = N("LinearInterpolate", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    C(alpha, alpha_out, e, "Alpha")
    return e


def smooth(lo, lo_out, hi, hi_out, v, v_out, x, y):
    e = N("SmoothStep", x, y)
    C(lo, lo_out, e, "Min")
    C(hi, hi_out, e, "Max")
    C(v, v_out, e, "Value")
    return e


def smoothc(v, v_out, lo, hi, x, y):
    e = N("SmoothStep", x, y, const_min=lo, const_max=hi)
    C(v, v_out, e, "Value")
    return e


def noise(wpxy, scale, x, y, offset=(0.0, 0.0)):
    """R channel of the packed noise; scale is a scalar param node or a 2-vector node (tile size in uu)."""
    uv = op("Divide", wpxy, "", scale, "", x, y)
    off = N("Constant2Vector", x, y + 70, r=offset[0], g=offset[1])
    uvo = op("Add", uv, "", off, "", x + 130, y)
    t = N("TextureSample", x + 260, y, texture=unreal.load_asset(NOISE),
          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(uvo, "", t, "UVs")
    return t


def output_node(prop):
    e = mel.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, prop))
    if not e:
        raise RuntimeError("nothing wired to " + prop)
    return e


def build(cuts):
    """cuts=False (decals): colour/roughness only; decal UVs are not laid out across the path."""
    base_color = output_node("MP_BASE_COLOR")
    roughness = output_node("MP_ROUGHNESS")
    opacity = output_node("MP_OPACITY") if cuts else None

    X0, Y0 = -4200, 900
    N("Comment", X0 - 60, Y0 - 120, text="Surface aware (PathSurfaceAware): RVT ground colour -> green/dry/sand "
      "masks -> grass strip + tufts, wet ruts, sand hue + drifts")

    # --- Ground under the ribbon --------------------------------------------------------------
    rvt = N("RuntimeVirtualTextureSample", X0, Y0, virtual_texture=unreal.load_asset(RVT_PATH),
            material_type=unreal.RuntimeVirtualTextureMaterialType.BASE_COLOR_NORMAL_SPECULAR,
            mip_value_mode=unreal.RuntimeVirtualTextureMipValueMode.RVTMVM_MIP_BIAS)
    # Mip bias blurs the ground colour so the masks follow areas, not individual grass blades.
    C(S("Surface_GroundBlur", 4.0, X0 - 200, Y0 + 80), "", rvt, "Mip Level")
    land = rvt  # output "BaseColor"
    lsum = N("DotProduct", X0 + 220, Y0)
    C(land, "BaseColor", lsum, "A")
    ones = N("Constant3Vector", X0 + 60, Y0 + 120, constant=unreal.LinearColor(1, 1, 1, 1))
    C(ones, "", lsum, "B")
    lsum_safe = opc("Max", lsum, "", 0.001, X0 + 360, Y0)
    land_g = N("ComponentMask", X0 + 220, Y0 + 200, r=False, g=True, b=False, a=False)
    C(land, "BaseColor", land_g, "")
    chrom = op("Divide", land_g, "", lsum_safe, "", X0 + 500, Y0 + 100)

    green_lo = S("Surface_GreenLo", 0.35, X0 + 500, Y0 + 220)
    green_hi = S("Surface_GreenHi", 0.41, X0 + 500, Y0 + 300)
    green = smooth(green_lo, "", green_hi, "", chrom, "", X0 + 720, Y0 + 100)
    sand_lo = S("Surface_SandLo", 0.27, X0 + 500, Y0 + 400)
    sand_hi = S("Surface_SandHi", 0.32, X0 + 500, Y0 + 480)
    not_sand = smooth(sand_lo, "", sand_hi, "", chrom, "", X0 + 720, Y0 + 400)
    sand = one("OneMinus", not_sand, "", X0 + 900, Y0 + 400)
    dry = one("Saturate", op("Subtract", not_sand, "", green, "", X0 + 900, Y0 + 260), "", X0 + 1040, Y0 + 260)

    wp = N("WorldPosition", X0, Y0 + 1000)
    wpxy = N("ComponentMask", X0 + 160, Y0 + 1000, r=True, g=True, b=False, a=False)
    C(wp, "", wpxy, "")
    if cuts:
        # --- Path-space coordinates ----------------------------------------------------------------
        uv = N("TextureCoordinate", X0, Y0 + 700)
        u = N("ComponentMask", X0 + 160, Y0 + 700, r=True, g=False, b=False, a=False)
        C(uv, "", u, "")
        xc = one("Abs", opc("Add", opc("Multiply", u, "", 2.0, X0 + 300, Y0 + 700), "", -1.0, X0 + 430, Y0 + 700),
                 "", X0 + 560, Y0 + 700)  # 0 centre line, 1 ribbon edge

        n_tuft = noise(wpxy, S("Surface_TuftScale", 1600.0, X0 + 160, Y0 + 1100), X0 + 320, Y0 + 1000, (0.21, 0.47))
        n_strip = noise(wpxy, S("Surface_StripScale", 1400.0, X0 + 160, Y0 + 1250), X0 + 320, Y0 + 1250, (0.63, 0.11))
        drift_scale = N("VectorParameter", X0 + 160, Y0 + 1400, parameter_name="Surface_DriftScale",
                        default_value=unreal.LinearColor(6000.0, 1500.0, 0, 0), group=GROUP)
        drift_xy = N("ComponentMask", X0 + 300, Y0 + 1400, r=True, g=True, b=False, a=False)
        C(drift_scale, "", drift_xy, "")
        n_drift = noise(wpxy, drift_xy, X0 + 440, Y0 + 1400, (0.37, 0.89))

        # --- Opacity cuts -------------------------------------------------------------------------
        # Centre strip: grass/straw mohawk between the wheel tracks.
        strip_w = S("Surface_StripWidth", 0.17, X0 + 900, Y0 + 900)
        strip_band = one("OneMinus", smooth(N("Constant", X0 + 900, Y0 + 980, r=0.0), "", strip_w, "", xc, "",
                                            X0 + 1060, Y0 + 900), "", X0 + 1200, Y0 + 900)
        strip_n = smoothc(n_strip, "R", 0.35, 0.55, X0 + 1060, Y0 + 1250)
        strip = op("Multiply", strip_band, "", strip_n, "", X0 + 1340, Y0 + 950)
        dry_strip = S("Surface_DryStripStrength", 0.55, X0 + 1340, Y0 + 1050)
        strip_surface = op("Add", green, "", op("Multiply", dry, "", dry_strip, "", X0 + 1340, Y0 + 1120), "",
                           X0 + 1480, Y0 + 1080)
        strip_amt = op("Multiply", op("Multiply", strip, "", strip_surface, "", X0 + 1620, Y0 + 980), "",
                       S("Surface_StripStrength", 0.9, X0 + 1620, Y0 + 1080), "", X0 + 1760, Y0 + 980)

        # Edge tufts on green: grass bites into the outer part of the ribbon.
        edge_band = smoothc(xc, "", 0.15, 0.7, X0 + 900, Y0 + 760)
        tuft_n = smoothc(n_tuft, "R", 0.45, 0.58, X0 + 1060, Y0 + 1000)
        # Dry ground also grows straw tufts into the edges, a bit less than green.
        tuft_surface = op("Add", green, "", op("Multiply", dry, "", S("Surface_DryTuftStrength", 0.7, X0 + 1060, Y0 + 860),
                                               "", X0 + 1200, Y0 + 880), "", X0 + 1340, Y0 + 880)
        tufts = op("Multiply", op("Multiply", edge_band, "", tuft_n, "", X0 + 1200, Y0 + 800), "", tuft_surface, "",
                   X0 + 1340, Y0 + 800)
        tuft_amt = op("Multiply", tufts, "", S("Surface_TuftStrength", 0.85, X0 + 1340, Y0 + 700), "",
                      X0 + 1480, Y0 + 800)

        # Sand drifts: elongated world-space noise partly buries the path on sand.
        drift_n = smoothc(n_drift, "R", 0.45, 0.62, X0 + 900, Y0 + 1400)
        drift_amt = op("Multiply", op("Multiply", drift_n, "", sand, "", X0 + 1060, Y0 + 1450), "",
                       S("Surface_DriftStrength", 0.8, X0 + 1060, Y0 + 1550), "", X0 + 1200, Y0 + 1450)

        cut = one("Saturate", op("Add", op("Add", strip_amt, "", tuft_amt, "", X0 + 1900, Y0 + 900), "",
                                  drift_amt, "", X0 + 2040, Y0 + 1000), "", X0 + 2180, Y0 + 1000)
        new_opacity = op("Multiply", opacity, "", one("OneMinus", cut, "", X0 + 2320, Y0 + 1000), "",
                         X0 + 2460, Y0 + 1000)

    # --- Colour -----------------------------------------------------------------------------
    # Sand/dry: carry the ground hue (chroma = colour / mean) into the path, keeping path luminance.
    path_sum = N("DotProduct", X0 + 1500, Y0 + 200)
    C(base_color, "", path_sum, "A")
    C(ones, "", path_sum, "B")
    land_chroma = op("Divide", land, "BaseColor", opc("Multiply", lsum_safe, "", 1.0 / 3.0, X0 + 1500, Y0 + 60),
                     "", X0 + 1650, Y0 + 60)
    hue_col = op("Multiply", land_chroma, "", opc("Multiply", path_sum, "", 1.0 / 3.0, X0 + 1650, Y0 + 200), "",
                 X0 + 1800, Y0 + 120)
    hue_amt = op("Multiply", one("OneMinus", green, "", X0 + 1800, Y0 + 260), "",
                 S("Surface_HueMatch", 0.45, X0 + 1800, Y0 + 340), "", X0 + 1940, Y0 + 260)
    col1 = lerp(base_color, "", hue_col, "", hue_amt, "", X0 + 2100, Y0 + 150)

    # Green: moister darker dirt; ruts = the darker pixels of the path texture itself, so it works for
    # any path variant. Wet ruts darken, and in noise patches they hold glossy puddles.
    moist = S("Surface_GreenDarken", 0.65, X0 + 2100, Y0 + 300)
    path_mean = opc("Multiply", path_sum, "", 1.0 / 3.0, X0 + 900, Y0 + 520)
    rut = one("OneMinus", smooth(S("Surface_RutLumLo", 0.14, X0 + 900, Y0 + 600), "",
                                 S("Surface_RutLumHi", 0.26, X0 + 900, Y0 + 680), "", path_mean, "",
                                 X0 + 1060, Y0 + 600), "", X0 + 1200, Y0 + 600)
    rut_wet = op("Multiply", op("Multiply", rut, "", green, "", X0 + 1340, Y0 + 600), "",
                 S("Surface_RutWet", 0.6, X0 + 1340, Y0 + 680), "", X0 + 1480, Y0 + 600)
    n_puddle = noise(wpxy, S("Surface_PuddleScale", 2500.0, X0 + 160, Y0 + 1600), X0 + 320, Y0 + 1600, (0.71, 0.29))
    puddle = op("Multiply", op("Multiply", smoothc(n_puddle, "R", 0.56, 0.64, X0 + 900, Y0 + 1650), "", rut, "",
                               X0 + 1060, Y0 + 1650), "", green, "", X0 + 1200, Y0 + 1650)
    puddle = op("Multiply", puddle, "", S("Surface_PuddleStrength", 0.8, X0 + 1200, Y0 + 1750), "",
                X0 + 1340, Y0 + 1650)
    rut_wet = op("Max", rut_wet, "", puddle, "", X0 + 1620, Y0 + 620)
    darken = lerp(N("Constant", X0 + 2100, Y0 + 380, r=1.0), "", moist, "", green, "", X0 + 2240, Y0 + 330)
    rut_dark = lerp(N("Constant", X0 + 2240, Y0 + 480, r=1.0), "", S("Surface_RutDarken", 0.5, X0 + 2240,
                                                                      Y0 + 560), "", rut_wet, "", X0 + 2380, Y0 + 480)
    col2 = op("Multiply", op("Multiply", col1, "", darken, "", X0 + 2380, Y0 + 200), "", rut_dark, "",
              X0 + 2520, Y0 + 250)

    # Debug: R = sand, G = green, B = dry.
    dbg = N("AppendVector", X0 + 2380, Y0 + 40)
    C(sand, "", dbg, "A")
    C(green, "", dbg, "B")
    dbg3 = N("AppendVector", X0 + 2520, Y0 + 40)
    C(dbg, "", dbg3, "A")
    C(dry, "", dbg3, "B")
    col_out = lerp(col2, "", dbg3, "", S("Surface_DebugView", 0.0, X0 + 2520, Y0 - 60), "", X0 + 2660, Y0 + 150)

    rough_wet = lerp(roughness, "", opc("Multiply", roughness, "", 0.55, X0 + 2520, Y0 + 650), "", rut_wet, "",
                     X0 + 2660, Y0 + 650)
    rough_out = lerp(rough_wet, "", N("Constant", X0 + 2660, Y0 + 760, r=0.08), "", puddle, "", X0 + 2780, Y0 + 650)

    def switch(true_node, false_node, prop, y):
        sw = N("StaticSwitchParameter", X0 + 2900, Y0 + y, parameter_name="PathSurfaceAware",
               default_value=False, group=GROUP)
        C(true_node, "", sw, "True")
        C(false_node, "", sw, "False")
        mel.connect_material_property(sw, "", getattr(unreal.MaterialProperty, prop))

    switch(col_out, base_color, "MP_BASE_COLOR", 150)
    switch(rough_out, roughness, "MP_ROUGHNESS", 650)
    if cuts:
        switch(new_opacity, opacity, "MP_OPACITY", 1000)


def restore_outputs():
    """If a previous run left switches on the outputs, put the original nodes back before removing them."""
    for prop in ["MP_BASE_COLOR", "MP_ROUGHNESS", "MP_OPACITY"]:
        e = mel.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, prop))
        if e and e.get_editor_property("desc") == DESC and isinstance(e, unreal.MaterialExpressionStaticSwitchParameter):
            original = mel.get_inputs_for_material_expression(mat, e)[1]  # inputs: True, False
            mel.connect_material_property(original, "", getattr(unreal.MaterialProperty, prop))


def run(mat_path, cuts):
    global mat
    failures.clear()
    mat = unreal.load_asset(mat_path)
    restore_outputs()
    removed = remove_previous()
    build(cuts)
    mel.recompile_material(mat)
    print(mat_path.split("/")[-1], "removed", removed, "failures", failures)
