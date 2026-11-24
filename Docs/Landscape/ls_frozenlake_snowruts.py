"""Snow ruts for M_RTS_Landscape_FrozenLake (called by ls_frozenlake_build after the Pripyat graph is built).

The shared MF_RTS_LS_TankTrails is not touched: about 20 maps call it, and rebuilding it unhooks them. This stage sits
between that function's output and the LS_TankTrails switch, and only acts where the ground is snow or frost:
  - wider ruts (a lower smoothstep on the same RT_TankTrails value), shaded much steeper (SnowRut_NormalStrength);
  - compacted, shadowed bluish snow on the rut floor (SnowRut_Tint);
  - dark peat showing where the rut is deepest (SnowRut_Soil, SnowRut_Expose);
  - a bright pushed-up berm just outside the rut edges (SnowRut_BermBrightness).
The landscape has a vertex per metre and the ruts are 30-90 cm wide, so the depth is shading only (no displacement).
It reads the same RT_TankTrails / MPC_TankTrails as the shared function, inside a [branch] on TrailEnabled and the snow
weight, through the shared clamped sampler (no extra sampler slot).
"""
import unreal

TRAILS_RT = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails"
TRAILS_MPC = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails"
GROUP = "Snow Ruts"

# R = rut (wide smoothstep), GB = rut slope across +-Reach texels, A = berm (just outside a rut edge)
SNOW_RUT_HLSL = """float4 Result = float4(0.0, 0.0, 0.0, 0.0);
[branch] if (Enabled > 0.5 && Snow > 0.02)
{
    float2 UV = (WorldXY - BoundsMin.xy) * InvSize.xy;
    if (all(UV > 0.0) && all(UV < 1.0))
    {
        float2 T = InvSize.zw * Reach;
        float C  = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV, 0).r;
        float PX = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV + float2(T.x, 0.0), 0).r;
        float NX = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV - float2(T.x, 0.0), 0).r;
        float PY = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV + float2(0.0, T.y), 0).r;
        float NY = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV - float2(0.0, T.y), 0).r;
        float Depth = smoothstep(RutLow, RutHigh, C);
        float Ring = max(max(PX, NX), max(PY, NY));
        float Berm = saturate((Ring - C) * 3.0) * (1.0 - Depth);
        Result = float4(Depth, (PX - NX) * 0.5, (PY - NY) * 0.5, Berm);
    }
}
return Result;"""


def snow_ruts(P, material):
    """Inserts the snow-rut stage between the shared trails call and the LS_TankTrails switch."""
    G = P.G
    N, C, S, V, mul, lerp, mask = G.N, G.C, G.S, G.V, G.mul, G.lerp, G.mask
    switch = next(e for e in G.live_expressions(material)
                  if isinstance(e, unreal.MaterialExpressionStaticSwitchParameter)
                  and str(e.get_editor_property("parameter_name")) == "LS_TankTrails")
    trails_call = G.mel.get_inputs_for_material_expression(material, switch)[0]
    G.begin(material, P.DESC, GROUP)
    X0, Y0 = -200, 1300
    G.comment("Snow ruts (Frozen Lake only, ls_frozenlake_snowruts.py): where the ground is snow or frost, tank ruts "
              "are wider and read deeper: steeper shading, compacted bluish snow, peat at the deepest part, a bright "
              "berm at the edges. Shading only (no displacement).", X0 - 700, Y0 - 300, 2400, 1300)
    snow_w = N("LandscapeLayerSample", X0 - 600, Y0, parameter_name="FLSnow", preview_weight=0.0)
    frost_w = N("LandscapeLayerSample", X0 - 600, Y0 + 110, parameter_name="FLFrost", preview_weight=0.0)
    snow = G.sat(G.op("Add", snow_w, "", G.mulc(frost_w, "", 0.6, X0 - 420, Y0 + 110), "", X0 - 260, Y0 + 40), "",
                 X0 - 120, Y0 + 40)
    wp = mask(N("WorldPosition", X0 - 600, Y0 + 240), "", "RG", X0 - 420, Y0 + 240)

    def coll(name, y):
        e = N("CollectionParameter", X0 - 600, Y0 + y, collection=unreal.load_asset(TRAILS_MPC))
        e.set_editor_property("parameter_name", name)
        return e

    tex = N("TextureObject", X0 - 600, Y0 + 700, texture=unreal.load_asset(TRAILS_RT),
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    pins = [("WorldXY", wp), ("BoundsMin", coll("TrailBoundsMin", 340)), ("InvSize", coll("TrailBoundsInvSize", 440)),
            ("Enabled", coll("TrailEnabled", 540)), ("Snow", snow), ("Tex", tex),
            ("RutLow", S("SnowRut_Low", 0.05, X0 - 420, Y0 + 560)),
            ("RutHigh", S("SnowRut_High", 0.35, X0 - 420, Y0 + 640)),
            ("Reach", S("SnowRut_ReachTexels", 2.5, X0 - 420, Y0 + 720))]
    sample = G.custom(SNOW_RUT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT4, "SnowRutSample",
                      [name for name, _ in pins], X0 - 140, Y0 + 400)
    for name, node in pins:
        C(node, "", sample, name)
    strength = S("TankTrailStrength", 0.0, X0, Y0 + 820)
    k = mul(snow, "", strength, "", X0 + 120, Y0 + 760)
    depth = mul(mask(sample, "", "R", X0 + 100, Y0 + 400), "", k, "", X0 + 260, Y0 + 400)
    berm = mul(mask(sample, "", "A", X0 + 100, Y0 + 500), "", k, "", X0 + 260, Y0 + 500)

    brk = N("BreakMaterialAttributes", X0 + 200, Y0 - 200)
    C(trails_call, "Attributes", brk, "Attr")
    # base colour: compacted, shadowed snow in the rut; peat at the deepest part; a brighter berm
    tint = V("SnowRut_Tint", unreal.LinearColor(0.30, 0.34, 0.44, 1.0), X0 + 300, Y0 + 60)
    packed = mul(brk, "BaseColor", tint, "", X0 + 520, Y0)
    bc1 = lerp(brk, "BaseColor", packed, "", depth, "", X0 + 700, Y0 - 40)
    deep = G.smooth(G.const(0.75, X0 + 520, Y0 + 180), G.const(1.0, X0 + 520, Y0 + 240), depth, "", X0 + 700, Y0 + 180)
    expose = mul(deep, "", S("SnowRut_Expose", 0.6, X0 + 700, Y0 + 280), "", X0 + 880, Y0 + 200)
    soil = V("SnowRut_Soil", unreal.LinearColor(0.10, 0.085, 0.07, 1.0), X0 + 880, Y0 + 60)
    bc2 = lerp(bc1, "", soil, "", expose, "", X0 + 1060, Y0 - 40)
    berm_amount = mul(berm, "", S("SnowRut_BermBrightness", 0.3, X0 + 1060, Y0 + 500), "", X0 + 1180, Y0 + 440)
    bc3 = lerp(bc2, "", G.const(1.0, X0 + 1060, Y0 + 120), "", berm_amount, "", X0 + 1240, Y0 - 40)
    rough1 = lerp(brk, "Roughness", G.const(0.55, X0 + 880, Y0 - 160), "", depth, "", X0 + 1060, Y0 - 160)
    rough = lerp(rough1, "", G.const(0.95, X0 + 1060, Y0 - 240), "", berm, "", X0 + 1240, Y0 - 200)
    # normal: the rut slope, much steeper than on soil
    slope = N("AppendVector", X0 + 300, Y0 + 300)
    C(mask(sample, "", "GB", X0 + 100, Y0 + 300), "", slope, "A")
    C(G.const(0.0, X0 + 160, Y0 + 360), "", slope, "B")
    wall = mul(S("SnowRut_NormalStrength", 5.5, X0 + 300, Y0 + 640), "", k, "", X0 + 480, Y0 + 640)
    tilt = mul(slope, "", wall, "", X0 + 520, Y0 + 300)
    tilt_ts = N("Transform", X0 + 700, Y0 + 300,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    C(tilt, "", tilt_ts, "")
    n_out = N("Normalize", X0 + 1060, Y0 + 300)
    C(G.op("Add", brk, "Normal", tilt_ts, "", X0 + 880, Y0 + 300), "", n_out, "")
    make = N("MakeMaterialAttributes", X0 + 1440, Y0 - 200)
    C(bc3, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    C(n_out, "", make, "Normal")
    for pin in ["Metallic", "Specular", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    C(make, "", switch, "True")
