"""Build M_RTS_Water_Shore: translucent copy of M_RTS_Water with depth-based shoreline foam.

Method: the water does not write depth, so SceneDepth is the ground under each water pixel.
  k = SceneDepth / PixelDepth  ->  ground = Cam + (Pixel - Cam) * k
  water depth (vertical cm) = (Cam.z - Pixel.z) * (k - 1)
That depth drives rolling foam bands that travel toward the bank, a surging contact line where the
water meets the bank/cliff, and the soft opacity edge. One extra scene-depth read, one noise fetch.

Re-runnable: deletes and rebuilds the master from M_RTS_Water (only safe while nothing but our MI uses it).
Run in an editor:  exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/WaterShore/build_water_shore.py").read())
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

SRC = "/Game/RTS_Survival/Materials/water/JungleWater/Masters/M_RTS_Water"
DST = "/Game/RTS_Survival/Materials/water/JungleWater/Masters/M_RTS_Water_Shore"
MI = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
GROUP = "Shore"

SHORE_HLSL = r"""
// Vertical water depth (cm) above the ground behind this pixel.
float k = SceneD / max(PixD, 1.0);
float depth = max(CamDZ * (k - 1.0), 0.0);

// Rolling bands: phase grows with depth and time, so each crest travels toward depth 0 (the bank).
float wobble = (NoiseB - 0.5) * BandWobble;
float phase = depth / max(BandSpacing, 1.0) + Time * BandSpeed + wobble;
float f = frac(phase);
float shore = saturate(1.0 - depth / max(ShoreWidth, 1.0));
// Sharp shoreward front, foam trail seaward; bands grow as they reach the bank.
float bands = pow(1.0 - f, BandSharp) * shore * shore;

// Contact line at the bank: surges up when a crest arrives (phase at depth 0), then draws back.
float f0 = frac(Time * BandSpeed + wobble);
float surge = lerp(0.45, 1.0, pow(1.0 - f0, 3.0));
float contact = saturate(1.0 - depth / max(ContactWidth * surge, 0.1));

// Lumpy contact line (not a clean outline): noise thins it in places.
contact *= ContactStrength * lerp(0.55, 1.0, NoiseB);
float mask = max(bands, contact);
// Noise erosion breaks the bands into foam patches; strong mask survives more noise.
float foam = saturate((mask - NoiseA * FoamBreakup) / max(1.0 - FoamBreakup, 0.05));
foam = saturate(foam * FoamIntensity);

float waterOp = WaterOpacity * saturate(depth / max(OpacityDepth, 1.0));
float opacity = max(waterOp, foam * FoamOpacity);
return float3(foam, opacity, depth);
"""


def scalar(m, name, value, x, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    e.set_editor_property("group", GROUP)
    return e


def vector(m, name, value, x, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    e.set_editor_property("group", GROUP)
    return e


def mask(m, src, x, y, r=False, g=False, b=False, a=False):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    for k, v in (("r", r), ("g", g), ("b", b), ("a", a)):
        e.set_editor_property(k, v)
    MEL.connect_material_expressions(src, "", e, "")
    return e


def must(ok, what):
    if not ok:
        raise RuntimeError("connect failed: " + what)


def build():
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    m = EAL.duplicate_asset(SRC, DST)
    if not m:
        raise RuntimeError("duplicate failed")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)

    # Original output drivers, captured before rewiring.
    P = unreal.MaterialProperty
    orig = {p: MEL.get_material_property_input_node(m, getattr(P, p))
            for p in ("MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS", "MP_NORMAL")}

    X0, Y0 = -2600, 1400

    # --- depth inputs ---
    scene_d = MEL.create_material_expression(m, unreal.MaterialExpressionSceneDepth, X0, Y0)
    pix_d = MEL.create_material_expression(m, unreal.MaterialExpressionPixelDepth, X0, Y0 + 120)
    cam = MEL.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, X0, Y0 + 220)
    wpos = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X0, Y0 + 320)
    cam_minus = MEL.create_material_expression(m, unreal.MaterialExpressionSubtract, X0 + 250, Y0 + 260)
    must(MEL.connect_material_expressions(cam, "", cam_minus, "A"), "cam->sub")
    must(MEL.connect_material_expressions(wpos, "", cam_minus, "B"), "wpos->sub")
    cam_dz = mask(m, cam_minus, X0 + 450, Y0 + 260, b=True)

    # --- noise (one fetch of the shared packed-noise texture, world-space, slow pan) ---
    noise_scale = scalar(m, "FoamNoiseScale", 450.0, X0, Y0 + 460)
    wdiv = MEL.create_material_expression(m, unreal.MaterialExpressionDivide, X0 + 250, Y0 + 420)
    must(MEL.connect_material_expressions(wpos, "", wdiv, "A"), "wpos->div")
    must(MEL.connect_material_expressions(noise_scale, "", wdiv, "B"), "scale->div")
    wxy = mask(m, wdiv, X0 + 450, Y0 + 420, r=True, g=True)
    pan = MEL.create_material_expression(m, unreal.MaterialExpressionPanner, X0 + 650, Y0 + 420)
    pan.set_editor_property("speed_x", 0.012)
    pan.set_editor_property("speed_y", 0.007)
    must(MEL.connect_material_expressions(wxy, "", pan, "Coordinate"), "uv->pan")
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, X0 + 850, Y0 + 420)
    tex.set_editor_property("texture", unreal.load_asset(NOISE))
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    must(MEL.connect_material_expressions(pan, "", tex, "UVs"), "pan->tex")
    time = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X0 + 850, Y0 + 680)

    # --- shore parameters ---
    params = [
        ("ShoreWidth", 350.0), ("BandSpacing", 80.0), ("BandSpeed", 0.3), ("BandSharp", 3.5),
        ("BandWobble", 0.8), ("ContactWidth", 25.0), ("ContactStrength", 0.85), ("FoamBreakup", 0.45),
        ("FoamIntensity", 2.0), ("FoamOpacity", 1.0), ("WaterOpacity", 0.9), ("OpacityDepth", 120.0),
    ]
    pnodes = {}
    for i, (n, v) in enumerate(params):
        pnodes[n] = scalar(m, n, v, X0 + 850, Y0 + 800 + i * 90)

    # --- custom node ---
    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, X0 + 1250, Y0 + 300)
    cust.set_editor_property("description", "ShoreFoam")
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cust.set_editor_property("code", SHORE_HLSL)
    in_names = ["SceneD", "PixD", "CamDZ", "NoiseA", "NoiseB", "Time"] + [n for n, _ in params]
    ins = []
    for n in in_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cust.set_editor_property("inputs", ins)
    must(MEL.connect_material_expressions(scene_d, "", cust, "SceneD"), "sd")
    must(MEL.connect_material_expressions(pix_d, "", cust, "PixD"), "pd")
    must(MEL.connect_material_expressions(cam_dz, "", cust, "CamDZ"), "dz")
    must(MEL.connect_material_expressions(tex, "R", cust, "NoiseA"), "na")
    must(MEL.connect_material_expressions(tex, "G", cust, "NoiseB"), "nb")
    must(MEL.connect_material_expressions(time, "", cust, "Time"), "t")
    for n, _ in params:
        must(MEL.connect_material_expressions(pnodes[n], "", cust, n), n)

    foam_raw = mask(m, cust, X0 + 1550, Y0 + 250, r=True)
    opacity = mask(m, cust, X0 + 1550, Y0 + 400, g=True)

    # Low quality: no foam (opacity edge stays).
    qs = MEL.create_material_expression(m, unreal.MaterialExpressionQualitySwitch, X0 + 1750, Y0 + 250)
    zero = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, X0 + 1550, Y0 + 330)
    must(MEL.connect_material_expressions(foam_raw, "", qs, "Default"), "qs default")
    must(MEL.connect_material_expressions(zero, "", qs, "Low"), "qs low")
    foam = MEL.create_material_expression(m, unreal.MaterialExpressionNamedRerouteDeclaration, X0 + 1950, Y0 + 250)
    foam.set_editor_property("name", "ShoreFoam")
    must(MEL.connect_material_expressions(qs, "", foam, ""), "foam reroute")

    # --- blend surface toward foam ---
    XO = X0 + 2300
    foam_col = vector(m, "FoamColor", unreal.LinearColor(0.70, 0.68, 0.60, 1.0), XO - 250, Y0 - 200)
    foam_rough = scalar(m, "FoamRoughness", 0.75, XO - 250, Y0 + 0)

    def blend(src, target_node, target_out, y, prop):
        lerp = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, XO, y)
        must(MEL.connect_material_expressions(src, "", lerp, "A"), prop + " A")
        if target_node is not None:
            must(MEL.connect_material_expressions(target_node, target_out, lerp, "B"), prop + " B")
        must(MEL.connect_material_expressions(foam, "", lerp, "Alpha"), prop + " alpha")
        must(MEL.connect_material_property(lerp, "", getattr(P, prop)), prop + " out")
        return lerp

    blend(orig["MP_BASE_COLOR"], foam_col, "", Y0 - 200, "MP_BASE_COLOR")
    l = blend(orig["MP_METALLIC"], None, "", Y0 - 100, "MP_METALLIC")
    l.set_editor_property("const_b", 0.0)
    l = blend(orig["MP_SPECULAR"], None, "", Y0 - 50, "MP_SPECULAR")
    l.set_editor_property("const_b", 0.3)
    blend(orig["MP_ROUGHNESS"], foam_rough, "", Y0 + 0, "MP_ROUGHNESS")
    flat = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, XO - 250, Y0 + 100)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 0))
    blend(orig["MP_NORMAL"], flat, "", Y0 + 100, "MP_NORMAL")
    must(MEL.connect_material_property(opacity, "", P.MP_OPACITY), "opacity out")

    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, False)

    if not EAL.does_asset_exist(MI):
        at = unreal.AssetToolsHelpers.get_asset_tools()
        mi = at.create_asset("MI_Water_Shore", MI.rsplit("/", 1)[0], unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
    else:
        mi = unreal.load_asset(MI)
    MEL.set_material_instance_parent(mi, m)
    EAL.save_loaded_asset(mi, False)
    print("built", DST, MEL.get_num_material_expressions(m), "nodes; MI", MI)


build()
