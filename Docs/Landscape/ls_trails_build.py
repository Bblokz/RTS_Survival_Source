"""Tank-trail ruts as a material function, MF_RTS_LS_TankTrails (Docs/Environment/TankTrails.md).

MF_RTS_LS_TankTrails(Attributes, SoftGround) -> Attributes, RutCentre
  Ruts drawn by UTankTrailSubsystem into RT_TankTrails darken, smooth and dent the incoming attributes, on soft ground
  only (SoftGround = the layers that take ruts, 0..1). RutCentre is the rut depth on that soft ground (0..1) for later
  effects such as wet ruts. TankTrailStrength above zero on the landscape material also tells UTankTrailSubsystem to
  draw trails on that map.
The trail texture is sampled inside a uniform [branch] on MPC_TankTrails.TrailEnabled, so with trails off (or in the
editor, where nothing sets the collection) the landscape fetches nothing.
Masters call it behind StaticSwitchParameter LS_TankTrails, after MF_RTS_ColorVariation_Landscape and before the main
BreakMaterialAttributes only, so the runtime virtual texture output never holds trails: its pages are cached and
would need invalidating under every driving tank.
  M_RTS_Landscape_DryCastle (Ger_Trains, ...): wired by ls_master_build.run().
  M_RTS_Landscape_DesertPaintable (Ger_FirstMission, RedLine, the tutorials, ...): wired by run_desert_paintable().
Both masters call the one saved function. Rebuilding it (build_function) gives its inputs new ids and unhooks every
call, so rerun both wirings after a rebuild.
"""
import unreal

import ls_graph as G
from ls_graph import N, C, S, V, mul, lerp, mask

FUNCTION_NAME = "MF_RTS_LS_TankTrails"
RT_PATH = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails"
MPC_PATH = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails"
GROUP = "Tank Trails"
DESC = "LSTankTrails"
DESERT_PAINTABLE = {
    "material": "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/M_RTS_Landscape_DesertPaintable",
    "instance": "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/MI_RTS_Landscape_DesertPaintable",
    # DesertRockDark and ScorchConcrete are hard ground and keep no ruts.
    "soft_layers": ["Sand", "Weedy", "DesertGravel", "ScorchSandyStone"],
}

# Rut, then the rut 1.5 texels further along +X and +Y; the differences are the rut slope for the normal.
# Faint values are what an 8-bit fade leaves behind, the smoothstep hides them. A is the raw rut depth.
TRAIL_HLSL = """float4 Result = float4(0.0, 0.0, 0.0, 0.0);
[branch] if (Enabled > 0.5)
{
    float2 UV = (WorldXY - BoundsMin.xy) * InvSize.xy;
    if (all(UV > 0.0) && all(UV < 1.0))
    {
        float Centre = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV, 0).r;
        float AlongX = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV + float2(InvSize.z * 1.5, 0.0), 0).r;
        float AlongY = Tex.SampleLevel(View.SharedBilinearClampedSampler, UV + float2(0.0, InvSize.w * 1.5), 0).r;
        Result = float4(smoothstep(RutLow, RutHigh, Centre), AlongX - Centre, AlongY - Centre, Centre);
    }
}
return Result;"""


def collection_parameter(name, x, y):
    e = N("CollectionParameter", x, y, collection=unreal.load_asset(MPC_PATH))
    e.set_editor_property("parameter_name", name)
    return e


def build_function():
    function = G.function_asset(FUNCTION_NAME, "Tank-trail ruts from RT_TankTrails on soft ground: darker, smoother, "
                                "dented. Returns the attributes and RutCentre (rut depth on soft ground, 0..1).")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    attributes = G.function_input("Attributes", G.ATTRIBUTES, 0, X0 - 400, Y0 - 300, "Ground before the ruts.")
    soft = G.function_input("SoftGround", G.SCALAR, 1, X0 - 400, Y0 + 900,
                            "0..1: ground that takes ruts (sand, grass, gravel), not rock or concrete.")

    G.comment("Trail sample: RT_TankTrails over the area MPC_TankTrails names; nothing is fetched while trails are "
              "off. R = rut (smoothstepped), GB = rut slope, A = raw rut depth.",
              X0 - 60, Y0 - 120, 1200, 900, G.INPUT_COLOR)
    wp = N("WorldPosition", X0, Y0)
    wpxy = mask(wp, "", "RG", X0 + 180, Y0)
    bounds_min = collection_parameter("TrailBoundsMin", X0, Y0 + 120)
    inv_size = collection_parameter("TrailBoundsInvSize", X0, Y0 + 240)
    enabled = collection_parameter("TrailEnabled", X0, Y0 + 360)
    tex = N("TextureObject", X0, Y0 + 480, texture=unreal.load_asset(RT_PATH),
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    rut_low = S("TankTrail_RutLow", 0.12, X0, Y0 + 640)
    rut_high = S("TankTrail_RutHigh", 0.6, X0, Y0 + 740)
    trail = G.custom(TRAIL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT4, "TankTrailSample",
                     ["WorldXY", "BoundsMin", "InvSize", "Enabled", "Tex", "RutLow", "RutHigh"], X0 + 420, Y0 + 200)
    for pin, node in [("WorldXY", wpxy), ("BoundsMin", bounds_min), ("InvSize", inv_size), ("Enabled", enabled),
                      ("Tex", tex), ("RutLow", rut_low), ("RutHigh", rut_high)]:
        C(node, "", trail, pin)

    strength = S("TankTrailStrength", 0.0, X0 + 700, Y0 + 520)
    rut = mask(trail, "", "R", X0 + 700, Y0 + 200)
    amount = mul(mul(rut, "", strength, "", X0 + 900, Y0 + 300), "", soft, "", X0 + 1040, Y0 + 400)

    G.comment("Compacted, churned soil: darker and a little smoother than the ground around it. The rut slope tilts "
              "the normal towards the rut centre (height = -rut).", X0 + 640, Y0 - 420, 1240, 720)
    brk = N("BreakMaterialAttributes", X0 + 700, Y0 - 300)
    C(attributes, "", brk, "Attr")
    tint = V("TankTrail_Tint", unreal.LinearColor(0.72, 0.64, 0.56, 1.0), X0 + 900, Y0 - 120)
    bc_t = mul(brk, "BaseColor", tint, "", X0 + 1100, Y0 - 200)
    bc = lerp(brk, "BaseColor", bc_t, "", amount, "", X0 + 1300, Y0 - 260)
    rough_target = S("TankTrail_Roughness", 0.72, X0 + 1100, Y0 - 40)
    rough = lerp(brk, "Roughness", rough_target, "", amount, "", X0 + 1300, Y0 - 100)

    slope_xy = mask(trail, "", "GB", X0 + 700, Y0 + 80)
    slope = N("AppendVector", X0 + 900, Y0 + 80)
    C(slope_xy, "", slope, "A")
    C(G.const(0.0, X0 + 760, Y0 + 160), "", slope, "B")
    normal_strength = S("TankTrail_NormalStrength", 0.7, X0 + 900, Y0 + 160)
    wall_k = mul(mul(normal_strength, "", strength, "", X0 + 1040, Y0 + 160), "", soft, "", X0 + 1160, Y0 + 220)
    slope_ws = mul(slope, "", wall_k, "", X0 + 1180, Y0 + 80)
    slope_ts = N("Transform", X0 + 1320, Y0 + 80,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    C(slope_ws, "", slope_ts, "")
    n_sum = G.op("Add", brk, "Normal", slope_ts, "", X0 + 1500, Y0 + 20)
    n_out = N("Normalize", X0 + 1640, Y0 + 20)
    C(n_sum, "", n_out, "")

    make = N("MakeMaterialAttributes", X0 + 1800, Y0 - 200)
    C(bc, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    C(n_out, "", make, "Normal")
    for pin in ["Metallic", "Specular", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    G.function_output("Attributes", make, "", 0, X0 + 2050, Y0 - 200, "Ground with the ruts.")
    # Raw rut depth on soft ground, scaled like the ruts themselves: wet-rut mud fills from the centre line out.
    centre = mul(mul(mask(trail, "", "A", X0 + 700, Y0 + 640), "", soft, "", X0 + 860, Y0 + 640), "",
                 strength, "", X0 + 1000, Y0 + 640)
    G.function_output("RutCentre", centre, "", 1, X0 + 2050, Y0 + 640, "Rut depth on soft ground, 0..1.")
    return G.finish_function(function)


def soft_ground_mask(layers, x, y):
    """saturate(sum of the soft layer weights): trails only dent these layers."""
    total = None
    for index, layer in enumerate(layers):
        weight = N("LandscapeLayerSample", x, y + index * 100, parameter_name=layer, preview_weight=0.0)
        if total is None:
            total = weight
            continue
        total = G.op("Add", total, "", weight, "", x + 220 + index * 60, y + index * 60)
    return G.sat(total, "", x + 520, y + 160)


def enable_in_instance(instance_path):
    """Turns trails on for every map whose landscape uses this instance (or a child of it)."""
    mi = unreal.load_asset(instance_path)
    G.mel.set_material_instance_static_switch_parameter_value(mi, "LS_TankTrails", True)
    G.mel.set_material_instance_scalar_parameter_value(mi, "TankTrailStrength", 1.0)
    G.mel.update_material_instance(mi)
    return mi


def run_desert_paintable():
    """Swaps DesertPaintable's flat trail chain for the function: colour variation -> function -> main break."""
    material = unreal.load_asset(DESERT_PAINTABLE["material"])
    out_break = G.mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    source = G.mel.get_inputs_for_material_expression(material, out_break)[0]
    if source and source.get_editor_property("desc") == DESC:
        source = G.mel.get_inputs_for_material_expression(material, source)[1]
    G.failures.clear()
    # Reuse the function DryCastle already calls: rebuilding it gives its inputs new ids, which unhooks that call.
    function_path = G.FUNCTION_DIR + "/" + FUNCTION_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(function_path):
        function = unreal.load_asset(function_path)
    else:
        function = build_function()
    removed = G.remove_tagged(material, DESC)
    G.begin(material, DESC, GROUP)
    X0, Y0 = -2600, 900
    soft = soft_ground_mask(DESERT_PAINTABLE["soft_layers"], X0 + 200, Y0 + 600)
    call = G.function_call(function, X0 + 1000, Y0)
    C(source, "", call, "Attributes")
    C(soft, "", call, "SoftGround")
    switch = N("StaticSwitchParameter", X0 + 1400, Y0, parameter_name="LS_TankTrails", default_value=False,
               group=GROUP)
    C(call, "Attributes", switch, "True")
    C(source, "", switch, "False")
    C(switch, "", out_break, "Attr")
    G.mel.recompile_material(material)
    enable_in_instance(DESERT_PAINTABLE["instance"])
    print("DesertPaintable removed", removed, "failures", G.failures)
    return G.failures
