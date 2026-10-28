"""Material-only ground detail for Ger_Trains in M_RTS_Landscape_DryCastle (Docs/Environment/GroundDetail.md).

Behind StaticSwitchParameter LS_GroundDetail (default False; on only in MI_RTS_Landscape_DryCastle_Trains), inserted
after the tank-trail switch and before the main BreakMaterialAttributes, so the RVT output never sees it:
  wet ruts  tank ruts on damp grass ground fill with mud from the centre line out, at a level that varies along the
            rut; their roughness never drops below Lumen's trace limit
  furrows   ridge-and-furrow soil inside each farm field's real outline, aligned with its crop rows
All soil and mud colours derive from the breakup chain's bare-soil colour (T_Soil_1_BC x Breakup_DirtTint),
so the map has one soil palette.
Fields reach the material as instance parameters (write_fields(), from the level's ACrushableCropFieldActor):
  Field<i>_A = (row origin x, y, cos yaw, sin yaw)   Field<i>_B = (2 pi / row spacing, first row offset, radius^2, 0)
  Field<i>_C = (corner0 xy, corner1 xy)              Field<i>_D = (corner2 xy, corner3 xy), counter-clockwise
  FieldCount, FieldBounds = (min x, min y, max x, max y) of all outlines: pixels outside skip the field loop.
Re-runnable: nodes carry DESC and are removed first. Usage: import ls_detail_build as D; D.run(); D.write_fields()
"""
import math

import unreal

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
MI_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Trains"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
DESC = "LSGroundDetail"
GROUP = "Ground Detail"
SWITCH = "LS_GroundDetail"
TRAIL_DESC = "LSTankTrails"
BREAKUP_DESC = "LSBreakup"
# Breakup chain nodes (ls_breakup_build.py, X0 = -3200, Y0 = -3400): the damp mask before its strength
# (X0 + 1220, Y0 + 1560) and the tinted bare-soil colour T_Soil_1_BC x Breakup_DirtTint (X0 + 3100, Y0 + 640).
DAMP_MAX_POS = (-1980, -1840)
# The breakup chain's GrassCastle layer sample (X0 + 900, Y0 + 700): only grass ground turns ruts to mud.
GRASS_LAYER_POS = (-2300, -2700)
SOIL_COLOR_POS = (-100, -2760)
MAX_FIELDS = 10
FIELD_EDGE_FADE = 40.0
# The field edge is pushed out by up to FIELD_EDGE_JITTER / 2 by noise; bounds and circles keep that margin.
FIELD_EDGE_JITTER = 80.0
FIELD_BOUNDS_MARGIN = 60.0
# Detail noise: 3.7 m on axes rotated by 37 degrees, so it does not line up with the breakup noise.
DETAIL_NOISE_SCALE = 370.0
DETAIL_NOISE_ANGLE = 37.0
# Trail chain nodes (ls_trails_build.py, X0 = -2600, Y0 = 900): the soft-ground mask and TankTrailStrength.
TRAIL_SOFT_POS = (-1880, 1660)
TRAIL_STRENGTH_POS = (-1900, 1420)

mel = unreal.MaterialEditingLibrary
mat = None
failures = []

def tagged_expressions(material, desc):
    """Every expression of the material carrying desc. Walks the material's real expressions: looking them up by
    name index misses nodes once repeated rebuilds push the numbering past any fixed range."""
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression)
            if e.get_outer() == material and e.get_editor_property("desc") == desc]


FURROW_HLSL_HEAD = """float4 A[%d] = {%s};
float4 B[%d] = {%s};
float4 Q0[%d] = {%s};
float4 Q1[%d] = {%s};
"""
# Signed distance of q to the left of edge pa -> pb (positive inside a counter-clockwise outline).
FURROW_HLSL_BODY = """#define RTS_FIELD_EDGE(pa, pb, q) ((((pb).x - (pa).x) * ((q).y - (pa).y) - ((pb).y - (pa).y) * ((q).x - (pa).x)) / max(length((pb) - (pa)), 0.001))
float4 Result = float4(0.0, 0.0, 0.0, 0.0);
if (any(WorldXY < Bounds.xy) || any(WorldXY > Bounds.zw))
{
    return Result;
}
int count = (int)FieldCount;
// Unrolled so A/B/Q0/Q1 fold into direct parameter reads instead of per-pixel local arrays.
[unroll] for (int i = 0; i < %d; i++)
{
    if (i >= count)
    {
        break;
    }
    // Circle reject first: B.z is the squared radius of the field around its origin.
    float2 fromOrigin = WorldXY - A[i].xy;
    if (dot(fromOrigin, fromOrigin) > B[i].z)
    {
        continue;
    }
    float2 p0 = Q0[i].xy;
    float2 p1 = Q0[i].zw;
    float2 p2 = Q1[i].xy;
    float2 p3 = Q1[i].zw;
    float inside = min(min(RTS_FIELD_EDGE(p0, p1, WorldXY), RTS_FIELD_EDGE(p1, p2, WorldXY)),
                       min(RTS_FIELD_EDGE(p2, p3, WorldXY), RTS_FIELD_EDGE(p3, p0, WorldXY))) + EdgeJitter;
    if (inside <= 0.0)
    {
        continue;
    }
    float4 a = A[i];
    float2 rowAxis = float2(-a.w, a.z);
    float rowCoordinate = dot(WorldXY - a.xy, rowAxis);
    float phase = (rowCoordinate - B[i].y) * B[i].x;
    // ridge = 1 on the crop rows, -1 in the furrows between them; slope is d(ridge)/d(world xy)
    float ridge = cos(phase);
    float2 slope = -sin(phase) * B[i].x * rowAxis;
    Result = float4(saturate(inside / EdgeFade), ridge, slope);
    break;
}
#undef RTS_FIELD_EDGE
return Result;"""


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


def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    e = N("LinearInterpolate", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    C(alpha, alpha_out, e, "Alpha")
    return e


def smooth(lo, hi, value, value_out, x, y):
    e = N("SmoothStep", x, y)
    C(lo, "", e, "Min")
    C(hi, "", e, "Max")
    C(value, value_out, e, "Value")
    return e


def mask(a, a_out, channels, x, y):
    r, g, b, w = [c in channels for c in "RGBA"]
    e = N("ComponentMask", x, y, r=r, g=g, b=b, a=w)
    C(a, a_out, e, "")
    return e


def remove_previous():
    removed = 0
    for e in tagged_expressions(mat, DESC):
        mel.delete_material_expression(mat, e)
        removed += 1
    return removed


def find_by_desc(cls, desc, position=None):
    for e in tagged_expressions(mat, desc):
        if e.get_class().get_name() != "MaterialExpression" + cls:
            continue
        if position is None or (e.get_editor_property("material_expression_editor_x"),
                                e.get_editor_property("material_expression_editor_y")) == position:
            return e
    raise RuntimeError("no %s node with desc %s at %s" % (cls, desc, position))


def find_insertion():
    """Main output BreakMaterialAttributes and what fed it before this chain (the tank-trail switch)."""
    out_break = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    source = mel.get_inputs_for_material_expression(mat, out_break)[0]
    if source and source.get_editor_property("desc") == DESC:
        source = mel.get_inputs_for_material_expression(mat, source)[1]
    if not source:
        # A failed earlier run removed this chain and left the break unfed: rejoin the tank-trail switch.
        source = find_by_desc("StaticSwitchParameter", TRAIL_DESC)
    return source, out_break


def field_inputs(x, y):
    """Field<i>_A/B/C/D vector parameters, appended to float4 (RGB + alpha)."""
    pins = []
    for i in range(MAX_FIELDS):
        for k, suffix in enumerate("ABCD"):
            row = 4 * i + k
            p = N("VectorParameter", x, y + row * 90, parameter_name="Field%d_%s" % (i, suffix),
                  default_value=unreal.LinearColor(0, 0, 0, 0), group=GROUP + " Fields")
            rgba = N("AppendVector", x + 220, y + row * 90)
            C(p, "", rgba, "A")
            C(p, "A", rgba, "B")
            pins.append(("F%d%s" % (i, suffix), rgba))
    return pins


def furrow_node(wpxy, edge_jitter, x, y):
    pins = field_inputs(x - 500, y)
    arrays = []
    for suffix in "ABCD":
        arrays += [MAX_FIELDS, ", ".join("F%d%s" % (i, suffix) for i in range(MAX_FIELDS))]
    code = FURROW_HLSL_HEAD % tuple(arrays) + FURROW_HLSL_BODY % MAX_FIELDS
    custom = N("Custom", x, y, code=code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
               description="FieldFurrows")
    inputs = []
    for name in ["WorldXY", "EdgeFade", "Bounds", "FieldCount", "EdgeJitter"] + [n for n, _ in pins]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    C(wpxy, "", custom, "WorldXY")
    C(N("Constant", x - 160, y - 80, r=FIELD_EDGE_FADE), "", custom, "EdgeFade")
    bounds = N("VectorParameter", x - 500, y - 260, parameter_name="FieldBounds",
               default_value=unreal.LinearColor(0, 0, 0, 0), group=GROUP + " Fields")
    bounds4 = N("AppendVector", x - 280, y - 260)
    C(bounds, "", bounds4, "A")
    C(bounds, "A", bounds4, "B")
    C(bounds4, "", custom, "Bounds")
    count = N("ScalarParameter", x - 500, y - 160, parameter_name="FieldCount", default_value=0.0,
              group=GROUP + " Fields")
    C(count, "", custom, "FieldCount")
    C(edge_jitter, "", custom, "EdgeJitter")
    for name, node in pins:
        C(node, "", custom, name)
    return custom


def build(source, out_break):
    X0, Y0 = -2600, 3000
    N("Comment", X0 - 60, Y0 - 120, text="Ground detail (LS_GroundDetail): wet "
      "tank ruts, ridge-and-furrow soil inside the farm fields (Field<i>_A-D from ls_detail_build.write_fields)")
    brk = N("BreakMaterialAttributes", X0 + 2200, Y0 - 300)
    C(source, "", brk, "Attr")
    wp = N("WorldPosition", X0, Y0)
    wpxy = mask(wp, "", "RG", X0 + 160, Y0)
    damp = find_by_desc("Max", BREAKUP_DESC, DAMP_MAX_POS)
    soil = find_by_desc("Multiply", BREAKUP_DESC, SOIL_COLOR_POS)

    # --- 3.7 m noise on rotated axes: varies the rut mud along each rut and roughens the field outlines ---
    noise_detail = N("TextureSample", X0 + 480, Y0 + 260, texture=unreal.load_asset(NOISE),
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(rotated_uv(wpxy, X0 + 320, Y0 + 260), "", noise_detail, "UVs")
    # Only damp grass ground turns ruts to mud; the damp mask alone also covers every low-lying desert.
    on_grass = smooth(const(0.3, X0 + 1080, Y0 - 160), const(0.7, X0 + 1080, Y0 - 100),
                      find_by_desc("LandscapeLayerSample", BREAKUP_DESC, GRASS_LAYER_POS), "", X0 + 1220, Y0 - 140)

    # --- tank ruts: the raw rut depth on soft ground; mud fills the rut from its centre line outwards ---
    trail = find_by_desc("Custom", TRAIL_DESC)
    trail_soft = find_by_desc("Saturate", TRAIL_DESC, TRAIL_SOFT_POS)
    trail_strength = find_by_desc("ScalarParameter", TRAIL_DESC, TRAIL_STRENGTH_POS)
    centre = op("Multiply", op("Multiply", mask(trail, "", "A", X0 + 720, Y0 + 560), "", trail_soft, "",
                               X0 + 860, Y0 + 560), "", trail_strength, "", X0 + 1000, Y0 + 560)
    # The mud level varies along the rut (3.7 m noise), so mud comes and goes in tapered streaks, not planks.
    mud_level = op("Add", const(0.25, X0 + 720, Y0 + 640), "",
                   op("Multiply", noise_detail, "B", const(0.35, X0 + 720, Y0 + 700), "", X0 + 860, Y0 + 680), "",
                   X0 + 1000, Y0 + 660)
    rut_damp = smooth(const(0.15, X0 + 1000, Y0 + 900), const(0.6, X0 + 1000, Y0 + 960), damp, "",
                      X0 + 1140, Y0 + 900)
    mud_top = op("Add", mud_level, "", const(0.25, X0 + 1000, Y0 + 760), "", X0 + 1140, Y0 + 740)
    wet_rut = op("Multiply", smooth(mud_level, mud_top, centre, "", X0 + 1280, Y0 + 680), "",
                 op("Multiply", rut_damp, "", on_grass, "", X0 + 1280, Y0 + 900), "",
                 X0 + 1420, Y0 + 760)

    # --- furrows inside the field outlines; the 3.7 m noise roughens the outline so fields are not laid-down rugs ---
    edge_jitter = op("Multiply", op("Subtract", noise_detail, "R", const(0.5, X0 + 600, Y0 + 1100), "",
                                    X0 + 720, Y0 + 1100), "",
                     const(FIELD_EDGE_JITTER, X0 + 720, Y0 + 1160), "", X0 + 860, Y0 + 1120)
    furrow = furrow_node(wpxy, edge_jitter, X0 + 600, Y0 + 1400)
    field = mask(furrow, "", "R", X0 + 900, Y0 + 1300)
    ridge = mask(furrow, "", "G", X0 + 900, Y0 + 1380)
    neg_ridge = op("Multiply", ridge, "", const(-1.0, X0 + 900, Y0 + 1460), "", X0 + 1040, Y0 + 1400)
    # Narrow dark furrow bottoms and lit ridge tops instead of soft half-and-half stripes.
    furrow_bottom = smooth(const(0.2, X0 + 1040, Y0 + 1480), const(0.85, X0 + 1040, Y0 + 1540), neg_ridge, "",
                           X0 + 1200, Y0 + 1420)
    ridge_top = smooth(const(0.55, X0 + 1040, Y0 + 1600), const(1.0, X0 + 1040, Y0 + 1660), ridge, "",
                       X0 + 1200, Y0 + 1580)
    soil_amount = op("Multiply", field, "", S("Furrow_SoilAmount", 0.8, X0 + 1040, Y0 + 1220), "",
                     X0 + 1200, Y0 + 1240)

    # --- colour: one soil palette from the breakup's bare soil ---
    field_soil = op("Multiply", soil, "", S("Furrow_SoilBrightness", 0.85, X0 + 1400, Y0 - 120), "",
                    X0 + 1560, Y0 - 160)
    bc_soil = lerp(brk, "BaseColor", field_soil, "", soil_amount, "", X0 + 1720, Y0 - 200)
    # Furrow bottoms: 30 % desaturated and darkened; the furrow normal adds the rest of the shading.
    furrow_color = op("Multiply", desaturate(bc_soil, 0.3, X0 + 1720, Y0 - 120), "",
                      S("Furrow_Darken", 0.75, X0 + 1720, Y0 - 40), "", X0 + 1880, Y0 - 100)
    bc_furrow = lerp(bc_soil, "", furrow_color, "", op("Multiply", furrow_bottom, "", field, "", X0 + 1720, Y0 + 40),
                     "", X0 + 2040, Y0 - 160)
    ridge_amount = op("Multiply", op("Multiply", ridge_top, "", field, "", X0 + 1400, Y0 + 180), "",
                      S("Furrow_RidgeLight", 0.08, X0 + 1400, Y0 + 240), "", X0 + 1560, Y0 + 200)
    ridge_light = op("Add", const(1.0, X0 + 1560, Y0 + 140), "", ridge_amount, "", X0 + 1720, Y0 + 160)
    bc1 = op("Multiply", bc_furrow, "", ridge_light, "", X0 + 2200, Y0 - 120)
    # Mud: wet ruts take darkened, desaturated soil.
    mud_color = op("Multiply", desaturate(soil, 0.4, X0 + 1720, Y0 + 300), "",
                   S("Wet_RutMudDarken", 0.45, X0 + 1720, Y0 + 380), "", X0 + 1880, Y0 + 300)
    rut_mud = op("Multiply", wet_rut, "", S("Wet_RutMudAmount", 0.7, X0 + 1720, Y0 + 540), "", X0 + 1880, Y0 + 480)
    bc = lerp(bc1, "", mud_color, "", rut_mud, "", X0 + 2360, Y0 + 300)

    # --- roughness: wet ruts slicker but never below Lumen's trace limit ---
    rough_scaled = op("Multiply", brk, "Roughness", S("Wet_RoughnessMul", 0.7, X0 + 2040, Y0 + 1180), "",
                      X0 + 2200, Y0 + 1160)
    rough_floor = op("Max", rough_scaled, "", S("Wet_RoughnessFloor", 0.45, X0 + 2040, Y0 + 1240), "",
                     X0 + 2360, Y0 + 1180)
    rough = lerp(brk, "Roughness", rough_floor, "", wet_rut, "", X0 + 2520, Y0 + 1160)

    # --- normal: furrow slope (height = ridge * depth / 2) tilts the normal ---
    slope_xy = mask(furrow, "", "BA", X0 + 900, Y0 + 1740)
    slope = N("AppendVector", X0 + 1060, Y0 + 1740)
    C(slope_xy, "", slope, "A")
    C(const(0.0, X0 + 920, Y0 + 1820), "", slope, "B")
    depth = op("Multiply", S("Furrow_Depth", 9.0, X0 + 1060, Y0 + 1840), "", field, "", X0 + 1220, Y0 + 1820)
    tilt_ws = op("Multiply", slope, "", op("Multiply", depth, "", const(-0.5, X0 + 1220, Y0 + 1900), "",
                                         X0 + 1380, Y0 + 1860), "", X0 + 1540, Y0 + 1760)
    tilt_ts = N("Transform", X0 + 1700, Y0 + 1760,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    C(tilt_ws, "", tilt_ts, "")
    n_sum = op("Add", brk, "Normal", tilt_ts, "", X0 + 2200, Y0 + 1400)
    n_norm = N("Normalize", X0 + 2360, Y0 + 1400)
    C(n_sum, "", n_norm, "")
    make = N("MakeMaterialAttributes", X0 + 3200, Y0 - 100)
    C(bc, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    C(n_norm, "", make, "Normal")
    for pin in ["Metallic", "Specular", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    switch = N("StaticSwitchParameter", X0 + 3400, Y0 - 100, parameter_name=SWITCH, default_value=False, group=GROUP)
    C(make, "", switch, "True")
    C(source, "", switch, "False")
    C(switch, "", out_break, "Attr")


def desaturate(color, fraction, x, y):
    e = N("Desaturation", x, y)
    C(color, "", e, "")
    C(const(fraction, x - 140, y + 40), "", e, "Fraction")
    return e


def const(value, x, y):
    return N("Constant", x, y, r=value)


def rotated_uv(wpxy, x, y):
    """World XY rotated by DETAIL_NOISE_ANGLE and scaled by DETAIL_NOISE_SCALE: the detail noise UVs."""
    angle = math.radians(DETAIL_NOISE_ANGLE)
    code = "return float2(W.x * %.5f - W.y * %.5f, W.x * %.5f + W.y * %.5f) / %.1f;" % (
        math.cos(angle), math.sin(angle), math.sin(angle), math.cos(angle), DETAIL_NOISE_SCALE)
    custom = N("Custom", x, y, code=code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
               description="DetailNoiseUV")
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", "W")
    custom.set_editor_property("inputs", [custom_input])
    C(wpxy, "", custom, "W")
    return custom


def write_fields():
    """Writes the level's crop fields into the Trains instance (see the module docstring). Rows run along each field's
    local X and are spaced along local Y from the outline's minimum + spacing / 2, as PlacePlants does."""
    mi = unreal.load_asset(MI_PATH)
    fields = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if "CropField" not in actor.get_class().get_name():
            continue
        spline = actor.get_component_by_class(unreal.SplineComponent)
        if not spline or spline.get_number_of_spline_points() != 4:
            continue
        fields.append(field_parameters(actor, spline))
    fields = fields[:MAX_FIELDS]
    zero = unreal.LinearColor(0, 0, 0, 0)
    for i in range(MAX_FIELDS):
        values = fields[i][1:] if i < len(fields) else (zero, zero, zero, zero)
        for suffix, value in zip("ABCD", values):
            mel.set_material_instance_vector_parameter_value(mi, "Field%d_%s" % (i, suffix), value)
    corners = [c for f in fields for c in f[5]]
    bounds = unreal.LinearColor(min(c[0] for c in corners) - FIELD_BOUNDS_MARGIN,
                                min(c[1] for c in corners) - FIELD_BOUNDS_MARGIN,
                                max(c[0] for c in corners) + FIELD_BOUNDS_MARGIN,
                                max(c[1] for c in corners) + FIELD_BOUNDS_MARGIN) if corners else zero
    mel.set_material_instance_vector_parameter_value(mi, "FieldBounds", bounds)
    mel.set_material_instance_scalar_parameter_value(mi, "FieldCount", float(len(fields)))
    mel.update_material_instance(mi)
    return [f[0] for f in fields]


def field_parameters(actor, spline):
    """(label, A, B, C, D, world corners) for one field; corners counter-clockwise for the inside test."""
    transform = actor.get_actor_transform()
    world = [spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD) for i in range(4)]
    local = [transform.inverse_transform_location(p) for p in world]
    ys = sorted(p.y for p in local)
    xs = sorted(p.x for p in local)
    spacing = actor.get_editor_property("layout").get_editor_property("row_spacing")
    centre_local = unreal.Vector((xs[0] + xs[-1]) / 2, (ys[0] + ys[-1]) / 2, 0)
    origin = transform.transform_location(centre_local)
    yaw = math.radians(actor.get_actor_rotation().yaw)
    first_row = ys[0] + spacing / 2 - centre_local.y
    corners = [(p.x, p.y) for p in world]
    radius = max(math.hypot(c[0] - origin.x, c[1] - origin.y) for c in corners) + FIELD_BOUNDS_MARGIN
    area = sum(corners[k][0] * corners[(k + 1) % 4][1] - corners[(k + 1) % 4][0] * corners[k][1] for k in range(4))
    if area < 0:
        corners.reverse()
    return (actor.get_actor_label(),
            unreal.LinearColor(origin.x, origin.y, math.cos(yaw), math.sin(yaw)),
            unreal.LinearColor(2 * math.pi / spacing, first_row, radius * radius, 0),
            unreal.LinearColor(corners[0][0], corners[0][1], corners[1][0], corners[1][1]),
            unreal.LinearColor(corners[2][0], corners[2][1], corners[3][0], corners[3][1]),
            corners)


def run(enable=True):
    global mat
    mat = unreal.load_asset(MAT_PATH)
    failures.clear()
    source, out_break = find_insertion()
    removed = remove_previous()
    build(source, out_break)
    mel.recompile_material(mat)
    print("removed", removed, "failures", failures)
    if enable:
        mi = unreal.load_asset(MI_PATH)
        mel.set_material_instance_static_switch_parameter_value(mi, SWITCH, True)
        mel.update_material_instance(mi)
    return failures
