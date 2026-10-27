"""Adds the switched tank-trail chain to a landscape master (Docs/Environment/TankTrails.md).

Masters (TARGETS): M_RTS_Landscape_DryCastle (Ger_Trains, ...) and M_RTS_Landscape_DesertPaintable (Ger_FirstMission,
RedLine, the tutorials, ...). Usage: run("DryCastle") / run("DesertPaintable").
Re-runnable: every node it creates carries DESC in its description and is removed first.
Inserted between the node feeding the main BreakMaterialAttributes (MF_RTS_ColorVariation_Landscape in both masters)
and that BreakMaterialAttributes only, so the runtime virtual texture output (GetMaterialAttributes reads the colour
variation call) never holds trails: its pages are cached and would need invalidating under every driving tank.
StaticSwitchParameter LS_TankTrails defaults to False, which compiles the original path. TankTrailStrength above
zero on the landscape material also tells UTankTrailSubsystem to draw trails on that map.
The trail texture is sampled inside a uniform [branch] on MPC_TankTrails.TrailEnabled, so with trails off (or in
the editor, where nothing sets the collection) the landscape fetches nothing.
"""
import unreal

# Per master: the material instance that turns trails on for its maps, and the layers trails dent (soft ground).
TARGETS = {
    "DryCastle": {
        "material": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle",
        "instance": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Inst",
        "soft_layers": ["GrassCastle", "Desert", "RTSSand", "GraySand"],
    },
    "DesertPaintable": {
        "material": "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/M_RTS_Landscape_DesertPaintable",
        "instance": "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/MI_RTS_Landscape_DesertPaintable",
        # DesertRockDark and ScorchConcrete are hard ground and keep no ruts.
        "soft_layers": ["Sand", "Weedy", "DesertGravel", "ScorchSandyStone"],
    },
}
RT_PATH = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails"
MPC_PATH = "/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails"
DESC = "LSTankTrails"
GROUP = "Tank Trails"

mel = unreal.MaterialEditingLibrary
mat = None
failures = []

USED = ["WorldPosition", "ComponentMask", "Multiply", "Add", "LinearInterpolate", "LandscapeLayerSample", "Saturate",
        "BreakMaterialAttributes", "MakeMaterialAttributes", "StaticSwitchParameter", "ScalarParameter",
        "VectorParameter", "Constant", "AppendVector", "Comment", "Custom", "TextureObject",
        "CollectionParameter", "Transform", "Normalize"]

# Rut, then the rut 1.5 texels further along +X and +Y; the differences are the rut slope for the normal.
# Faint values are what an 8-bit fade leaves behind, the smoothstep hides them.
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


def remove_previous():
    removed = 0
    for cls in USED:
        for i in range(0, 400):
            e = unreal.find_object(mat, "MaterialExpression%s_%d" % (cls, i))
            if e and e.get_editor_property("desc") == DESC:
                mel.delete_material_expression(mat, e)
                removed += 1
    return removed


def find_insertion():
    """The main output BreakMaterialAttributes and what fed it before the trail switch (on a re-run: the switch's
    False input)."""
    out_break = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if not out_break or out_break.get_class().get_name() != "MaterialExpressionBreakMaterialAttributes":
        raise RuntimeError("base colour is not fed by a BreakMaterialAttributes")
    source = mel.get_inputs_for_material_expression(mat, out_break)[0]
    if source and source.get_editor_property("desc") == DESC:
        source = mel.get_inputs_for_material_expression(mat, source)[1]
    if not source:
        raise RuntimeError("nothing feeds " + out_break.get_name())
    return source, out_break


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


def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    e = N("LinearInterpolate", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    C(alpha, alpha_out, e, "Alpha")
    return e


def mask(a, a_out, channels, x, y):
    r, g, b, w = [c in channels for c in "RGBA"]
    e = N("ComponentMask", x, y, r=r, g=g, b=b, a=w)
    C(a, a_out, e, "")
    return e


def collection_parameter(name, x, y):
    e = N("CollectionParameter", x, y, collection=unreal.load_asset(MPC_PATH))
    e.set_editor_property("parameter_name", name)
    return e


def soft_ground_mask(layers, x, y):
    """saturate(sum of the soft layer weights): trails only dent these layers."""
    total = None
    for index, layer in enumerate(layers):
        weight = N("LandscapeLayerSample", x, y + index * 100, parameter_name=layer, preview_weight=0.0)
        if total is None:
            total = weight
            continue
        added = N("Add", x + 220 + index * 60, y + index * 60)
        C(total, "", added, "A")
        C(weight, "", added, "B")
        total = added
    soft = N("Saturate", x + 520, y + 160)
    C(total, "", soft, "")
    return soft


def build(color_var, out_break, soft_layers):
    X0, Y0 = -2600, 900
    N("Comment", X0 - 60, Y0 - 120, text="Tank trails (LS_TankTrails): ruts drawn by UTankTrailSubsystem into "
      "RT_TankTrails, sampled only while MPC_TankTrails.TrailEnabled is on. Main pass only, never the RVT.")

    wp = N("WorldPosition", X0, Y0)
    wpxy = mask(wp, "", "RG", X0 + 180, Y0)
    bounds_min = collection_parameter("TrailBoundsMin", X0, Y0 + 120)
    inv_size = collection_parameter("TrailBoundsInvSize", X0, Y0 + 240)
    enabled = collection_parameter("TrailEnabled", X0, Y0 + 360)
    tex = N("TextureObject", X0, Y0 + 480, texture=unreal.load_asset(RT_PATH),
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    rut_low = S("TankTrail_RutLow", 0.12, X0, Y0 + 640)
    rut_high = S("TankTrail_RutHigh", 0.6, X0, Y0 + 740)

    custom = N("Custom", X0 + 420, Y0 + 200, code=TRAIL_HLSL,
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, description="TankTrailSample")
    inputs = []
    for name in ["WorldXY", "BoundsMin", "InvSize", "Enabled", "Tex", "RutLow", "RutHigh"]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    C(wpxy, "", custom, "WorldXY")
    C(bounds_min, "", custom, "BoundsMin")
    C(inv_size, "", custom, "InvSize")
    C(enabled, "", custom, "Enabled")
    C(tex, "", custom, "Tex")
    C(rut_low, "", custom, "RutLow")
    C(rut_high, "", custom, "RutHigh")

    # Trails only dent soft ground (grass, desert, sand, gravel), not rock, concrete or asphalt.
    soft = soft_ground_mask(soft_layers, X0 + 200, Y0 + 600)

    strength = S("TankTrailStrength", 0.0, X0 + 700, Y0 + 520)
    rut = mask(custom, "", "R", X0 + 700, Y0 + 200)
    amount = mul(mul(rut, "", strength, "", X0 + 900, Y0 + 300), "", soft, "", X0 + 1040, Y0 + 400)

    brk = N("BreakMaterialAttributes", X0 + 700, Y0 - 300)
    C(color_var, "", brk, "Attr")

    # Compacted, churned soil: darker and a little smoother than the ground around it.
    tint = N("VectorParameter", X0 + 900, Y0 - 120, parameter_name="TankTrail_Tint",
             default_value=unreal.LinearColor(0.72, 0.64, 0.56, 1.0), group=GROUP)
    bc_t = mul(brk, "BaseColor", tint, "", X0 + 1100, Y0 - 200)
    bc = lerp(brk, "BaseColor", bc_t, "", amount, "", X0 + 1300, Y0 - 260)
    rough_target = S("TankTrail_Roughness", 0.72, X0 + 1100, Y0 - 40)
    rough = lerp(brk, "Roughness", rough_target, "", amount, "", X0 + 1300, Y0 - 100)

    # Rut walls: the rut slope tilts the normal towards the rut centre (height = -rut).
    slope_xy = mask(custom, "", "GB", X0 + 700, Y0 + 80)
    slope = N("AppendVector", X0 + 900, Y0 + 80)
    C(slope_xy, "", slope, "A")
    C(N("Constant", X0 + 760, Y0 + 160, r=0.0), "", slope, "B")
    normal_strength = S("TankTrail_NormalStrength", 0.7, X0 + 900, Y0 + 160)
    wall_k = mul(mul(normal_strength, "", strength, "", X0 + 1040, Y0 + 160), "", soft, "", X0 + 1160, Y0 + 220)
    slope_ws = mul(slope, "", wall_k, "", X0 + 1180, Y0 + 80)
    slope_ts = N("Transform", X0 + 1320, Y0 + 80,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    C(slope_ws, "", slope_ts, "")
    n_sum = N("Add", X0 + 1500, Y0 + 20)
    C(brk, "Normal", n_sum, "A")
    C(slope_ts, "", n_sum, "B")
    n_out = N("Normalize", X0 + 1640, Y0 + 20)
    C(n_sum, "", n_out, "")

    make = N("MakeMaterialAttributes", X0 + 1800, Y0 - 200)
    C(bc, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    C(n_out, "", make, "Normal")
    for pin in ["Metallic", "Specular", "AmbientOcclusion"]:
        C(brk, pin, make, pin)

    switch = N("StaticSwitchParameter", X0 + 2000, Y0 - 200, parameter_name="LS_TankTrails",
               default_value=False, group=GROUP)
    C(make, "", switch, "True")
    C(color_var, "", switch, "False")
    C(switch, "", out_break, "Attr")


def enable_in_instance(instance_path):
    """Turns trails on for every map whose landscape uses this instance (or a child of it)."""
    mi = unreal.load_asset(instance_path)
    mel.set_material_instance_static_switch_parameter_value(mi, "LS_TankTrails", True)
    mel.set_material_instance_scalar_parameter_value(mi, "TankTrailStrength", 1.0)
    mel.update_material_instance(mi)
    return mi


def run(target="DryCastle", enable=True):
    global mat
    config = TARGETS[target]
    mat = unreal.load_asset(config["material"])
    failures.clear()
    color_var, out_break = find_insertion()
    removed = remove_previous()
    build(color_var, out_break, config["soft_layers"])
    mel.recompile_material(mat)
    print(target, "removed", removed, "failures", failures)
    if enable:
        enable_in_instance(config["instance"])
    return failures
