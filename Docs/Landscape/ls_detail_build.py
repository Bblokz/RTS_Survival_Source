"""Ground detail as two material functions (Docs/Environment/GroundDetail.md).

MF_RTS_LS_FieldFurrows(Attributes, SoilColor) -> Attributes, FieldMask
  Ridge-and-furrow soil inside each farm field's real outline, aligned with its crop rows. FieldMask is 1 inside a
  field (landscape grass stays out of it).
MF_RTS_LS_FieldFurrows also returns DetailNoise, its 3.7 m noise sample.
MF_RTS_LS_WetRuts(Attributes, RutCentre, Damp, GrassWeight, SoilColor, DetailNoise optional) -> Attributes
  Tank ruts on damp grass ground fill with mud from the centre line out, at a level that varies along the rut; their
  roughness never drops below Lumen's trace limit. The damp mask alone also covers every low-lying desert, so only
  grass ground (GrassWeight) turns ruts to mud.
Both take SoilColor (the breakup's tinted bare soil), so the map has one soil palette.
M_RTS_Landscape_DryCastle calls them, furrows then wet ruts, behind StaticSwitchParameter LS_GroundDetail after the
tank trails and before the main BreakMaterialAttributes, so the RVT output never sees them (ls_master_build.run()).
Fields reach the material as instance parameters (write_fields(), from the level's ACrushableCropFieldActor):
  Field<i>_A = (row origin x, y, cos yaw, sin yaw)   Field<i>_B = (2 pi / row spacing, first row offset, 0, 0)
  Field<i>_C = (edge 0 inward unit normal xy, edge 1 normal xy)
  Field<i>_D = (edge 2 normal xy, edge 3 normal xy)  Field<i>_E = (edge 0..3 offsets): inside distance to edge k is
  dot(normal k, world xy) - offset k, so the outline test costs two vector multiply-adds and three mins per field.
  FieldCount, FieldBounds = (min x, min y, max x, max y) of all outlines: pixels outside skip the field loop.
"""
import math

import unreal

import ls_graph as G
from ls_graph import N, C, S, op, lerp, smooth, mask, const

MI_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Trains"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
FURROWS_NAME = "MF_RTS_LS_FieldFurrows"
WET_RUTS_NAME = "MF_RTS_LS_WetRuts"
GROUP = "Ground Detail"
MAX_FIELDS = 10
FIELD_EDGE_FADE = 40.0
# The field edge is pushed out by up to FIELD_EDGE_JITTER / 2 by noise; bounds and circles keep that margin.
FIELD_EDGE_JITTER = 80.0
FIELD_BOUNDS_MARGIN = 60.0
# Detail noise: 3.7 m on axes rotated by 37 degrees, so it does not line up with the breakup noise.
DETAIL_NOISE_SCALE = 370.0
DETAIL_NOISE_ANGLE = 37.0

mel = unreal.MaterialEditingLibrary


FIELD_SUFFIXES = "ABCDE"
FURROW_HLSL_HEAD = """float4 A[%d] = {%s};
float4 B[%d] = {%s};
float4 N01[%d] = {%s};
float4 N23[%d] = {%s};
float4 Offsets[%d] = {%s};
"""
# The edge planes are precomputed by write_fields(): the shader's instruction count is what the editor's shader
# complexity view shows, and this loop is unrolled once per field.
FURROW_HLSL_BODY = """float4 Result = float4(0.0, 0.0, 0.0, 0.0);
if (any(WorldXY < Bounds.xy) || any(WorldXY > Bounds.zw))
{
    return Result;
}
int count = (int)FieldCount;
// Unrolled so the arrays fold into direct parameter reads instead of per-pixel local arrays. The compiler already
// shares the row maths after the break between the unrolled fields (hoisting it by hand cost 4 instructions more).
[unroll] for (int i = 0; i < %d; i++)
{
    if (i >= count)
    {
        break;
    }
    // Distance inside each of the four fence edges (positive inside), all four at once.
    float4 edges = float4(N01[i].xz, N23[i].xz) * WorldXY.x + float4(N01[i].yw, N23[i].yw) * WorldXY.y - Offsets[i];
    float inside = min(min(edges.x, edges.y), min(edges.z, edges.w)) + EdgeJitter;
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
return Result;"""


def rotated_uv(wpxy, x, y):
    """World XY rotated by DETAIL_NOISE_ANGLE and scaled by DETAIL_NOISE_SCALE: the detail noise UVs. Plain math
    nodes, not a custom node: the compiler shares identical math (and the texture fetch after it) between the
    furrow and wet-rut functions, while every custom node compiles to a function of its own."""
    angle = math.radians(DETAIL_NOISE_ANGLE)
    cos_k, sin_k = math.cos(angle) / DETAIL_NOISE_SCALE, math.sin(angle) / DETAIL_NOISE_SCALE
    u = op("DotProduct", wpxy, "", N("Constant2Vector", x, y + 80, r=cos_k, g=-sin_k), "", x + 160, y)
    v = op("DotProduct", wpxy, "", N("Constant2Vector", x, y + 200, r=sin_k, g=cos_k), "", x + 160, y + 120)
    return op("AppendVector", u, "", v, "", x + 300, y)


def detail_noise(x, y):
    """The 3.7 m detail noise sample."""
    wpxy = mask(N("WorldPosition", x, y), "", "RG", x + 160, y)
    sample = N("TextureSample", x + 640, y, texture=unreal.load_asset(NOISE),
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(rotated_uv(wpxy, x + 320, y), "", sample, "UVs")
    return wpxy, sample


def field_inputs(x, y):
    """Field<i>_A..E vector parameters, appended to float4 (RGB + alpha)."""
    pins = []
    for i in range(MAX_FIELDS):
        for k, suffix in enumerate(FIELD_SUFFIXES):
            row = len(FIELD_SUFFIXES) * i + k
            p = N("VectorParameter", x, y + row * 90, parameter_name="Field%d_%s" % (i, suffix),
                  default_value=unreal.LinearColor(0, 0, 0, 0), group=GROUP + " Fields")
            rgba = op("AppendVector", p, "", p, "A", x + 220, y + row * 90)
            pins.append(("F%d%s" % (i, suffix), rgba))
    return pins


def furrow_node(wpxy, edge_jitter, x, y):
    pins = field_inputs(x - 500, y)
    arrays = []
    for suffix in FIELD_SUFFIXES:
        arrays += [MAX_FIELDS, ", ".join("F%d%s" % (i, suffix) for i in range(MAX_FIELDS))]
    code = FURROW_HLSL_HEAD % tuple(arrays) + FURROW_HLSL_BODY % MAX_FIELDS
    furrow = G.custom(code, unreal.CustomMaterialOutputType.CMOT_FLOAT4, "FieldFurrows",
                      ["WorldXY", "EdgeFade", "Bounds", "FieldCount", "EdgeJitter"] + [n for n, _ in pins], x, y)
    C(wpxy, "", furrow, "WorldXY")
    C(const(FIELD_EDGE_FADE, x - 160, y - 80), "", furrow, "EdgeFade")
    bounds = N("VectorParameter", x - 500, y - 260, parameter_name="FieldBounds",
               default_value=unreal.LinearColor(0, 0, 0, 0), group=GROUP + " Fields")
    C(op("AppendVector", bounds, "", bounds, "A", x - 280, y - 260), "", furrow, "Bounds")
    count = N("ScalarParameter", x - 500, y - 160, parameter_name="FieldCount", default_value=0.0,
              group=GROUP + " Fields")
    C(count, "", furrow, "FieldCount")
    C(edge_jitter, "", furrow, "EdgeJitter")
    for name, node in pins:
        C(node, "", furrow, name)
    return furrow


def build_furrows_function():
    function = G.function_asset(FURROWS_NAME, "Ridge-and-furrow soil inside the farm fields written by "
                                "ls_detail_build.write_fields() (Field<i>_A-E). Returns the attributes and FieldMask.")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    attributes = G.function_input("Attributes", G.ATTRIBUTES, 0, X0 + 1200, Y0 - 500, "Ground before the fields.")
    soil = G.function_input("SoilColor", G.VECTOR3, 1, X0 + 1200, Y0 - 160, "Bare-soil colour of the map.")
    brk = N("BreakMaterialAttributes", X0 + 1400, Y0 - 500)
    C(attributes, "", brk, "Attr")

    G.comment("Field test: one custom node checks each field's oriented outline (fences can be skewed). Pixels "
              "outside FieldBounds exit at once; inside, a radius check (Field<i>_B.z) skips far fields before the "
              "four edge tests. Out: R = field (fades over the edge), G = ridge (1 crop row, -1 furrow), BA = ridge "
              "slope. The 3.7 m noise roughens the outline so fields are not laid-down rugs.",
              X0 - 60, Y0 - 120, 1100, 760, G.INPUT_COLOR)
    wpxy, noise_detail = detail_noise(X0, Y0)
    edge_jitter = op("Multiply", op("Subtract", noise_detail, "R", const(0.5, X0 + 600, Y0 + 200), "",
                                    X0 + 720, Y0 + 200), "",
                     const(FIELD_EDGE_JITTER, X0 + 720, Y0 + 260), "", X0 + 860, Y0 + 220)
    furrow = furrow_node(wpxy, edge_jitter, X0 + 700, Y0 + 500)
    field = mask(furrow, "", "R", X0 + 1000, Y0 + 400)
    ridge = mask(furrow, "", "G", X0 + 1000, Y0 + 480)

    G.comment("Soil colour: field soil (Furrow_Soil*) with narrow dark furrow bottoms (30 % desaturated, "
              "Furrow_Darken) and lit ridge tops (Furrow_RidgeLight).", X0 + 1140, Y0 - 260, 1300, 900)
    neg_ridge = op("Multiply", ridge, "", const(-1.0, X0 + 1000, Y0 + 560), "", X0 + 1140, Y0 + 500)
    furrow_bottom = smooth(const(0.2, X0 + 1140, Y0 + 580), const(0.85, X0 + 1140, Y0 + 640), neg_ridge, "",
                           X0 + 1300, Y0 + 520)
    ridge_top = smooth(const(0.55, X0 + 1140, Y0 + 700), const(1.0, X0 + 1140, Y0 + 760), ridge, "",
                       X0 + 1300, Y0 + 680)
    soil_amount = op("Multiply", field, "", S("Furrow_SoilAmount", 0.8, X0 + 1140, Y0 + 320), "",
                     X0 + 1300, Y0 + 340)
    field_soil = op("Multiply", soil, "", S("Furrow_SoilBrightness", 0.85, X0 + 1400, Y0 - 120), "",
                    X0 + 1560, Y0 - 160)
    bc_soil = lerp(brk, "BaseColor", field_soil, "", soil_amount, "", X0 + 1720, Y0 - 200)
    furrow_color = op("Multiply", G.desaturate(bc_soil, 0.3, X0 + 1720, Y0 - 120), "",
                      S("Furrow_Darken", 0.75, X0 + 1720, Y0 - 40), "", X0 + 1880, Y0 - 100)
    bc_furrow = lerp(bc_soil, "", furrow_color, "", op("Multiply", furrow_bottom, "", field, "", X0 + 1720, Y0 + 40),
                     "", X0 + 2040, Y0 - 160)
    ridge_amount = op("Multiply", op("Multiply", ridge_top, "", field, "", X0 + 1400, Y0 + 180), "",
                      S("Furrow_RidgeLight", 0.08, X0 + 1400, Y0 + 240), "", X0 + 1560, Y0 + 200)
    ridge_light = op("Add", const(1.0, X0 + 1560, Y0 + 140), "", ridge_amount, "", X0 + 1720, Y0 + 160)
    bc = op("Multiply", bc_furrow, "", ridge_light, "", X0 + 2200, Y0 - 120)

    G.comment("Normal: the furrow slope (height = ridge * Furrow_Depth / 2) tilts the normal.",
              X0 + 1140, Y0 + 840, 1300, 300)
    slope = op("AppendVector", mask(furrow, "", "BA", X0 + 1000, Y0 + 900), "", const(0.0, X0 + 1000, Y0 + 980), "",
               X0 + 1160, Y0 + 900)
    depth = op("Multiply", S("Furrow_Depth", 9.0, X0 + 1160, Y0 + 1000), "", field, "", X0 + 1320, Y0 + 980)
    tilt_ws = op("Multiply", slope, "", op("Multiply", depth, "", const(-0.5, X0 + 1320, Y0 + 1060), "",
                                         X0 + 1480, Y0 + 1020), "", X0 + 1640, Y0 + 920)
    tilt_ts = N("Transform", X0 + 1800, Y0 + 920,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    C(tilt_ws, "", tilt_ts, "")
    n_sum = op("Add", brk, "Normal", tilt_ts, "", X0 + 2200, Y0 + 900)
    n_norm = N("Normalize", X0 + 2360, Y0 + 900)
    C(n_sum, "", n_norm, "")

    make = N("MakeMaterialAttributes", X0 + 2600, Y0 - 100)
    C(bc, "", make, "BaseColor")
    C(n_norm, "", make, "Normal")
    for pin in ["Metallic", "Specular", "Roughness", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    G.function_output("Attributes", make, "", 0, X0 + 2850, Y0 - 100, "Ground with the farm fields.")
    G.function_output("FieldMask", field, "", 1, X0 + 2850, Y0 + 400, "1 inside a field.")
    G.function_output("DetailNoise", noise_detail, "RGBA", 2, X0 + 2850, Y0 + 520,
                      "The 3.7 m detail noise sample; pass it to MF_RTS_LS_WetRuts to share the fetch.")
    return G.finish_function(function)


def build_wet_ruts_function():
    function = G.function_asset(WET_RUTS_NAME, "Mud in tank ruts on damp grass ground: fills from the rut centre "
                                "out, at a level that varies along the rut.")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    attributes = G.function_input("Attributes", G.ATTRIBUTES, 0, X0 + 1500, Y0 - 300, "Ground before the mud.")
    centre = G.function_input("RutCentre", G.SCALAR, 1, X0 + 800, Y0 + 560, "Rut depth on soft ground, 0..1.")
    damp = G.function_input("Damp", G.SCALAR, 2, X0 + 800, Y0 + 900, "Damp mask, 0..1.")
    grass = G.function_input("GrassWeight", G.SCALAR, 3, X0 + 800, Y0 + 1000, "Grass layer weight.")
    soil = G.function_input("SoilColor", G.VECTOR3, 4, X0 + 1500, Y0 + 300, "Bare-soil colour of the map.")
    brk = N("BreakMaterialAttributes", X0 + 1700, Y0 - 300)
    C(attributes, "", brk, "Attr")

    G.comment("Mud level: varies along the rut with the 3.7 m noise, so mud comes and goes in tapered streaks, not "
              "planks. Mud fills where the rut is deeper than the level, on damp grass only.",
              X0 - 60, Y0 + 180, 1600, 960, G.INPUT_COLOR)
    _, own_noise = detail_noise(X0, Y0 + 260)
    # Shared with the furrows when the master connects their DetailNoise; on its own the function samples it itself.
    noise_detail = G.function_input("DetailNoise", G.VECTOR4, 5, X0 + 600, Y0 + 400,
                                    "Optional: MF_RTS_LS_FieldFurrows' DetailNoise, saves a texture fetch.",
                                    own_noise, "RGBA")
    mud_noise = mask(noise_detail, "", "B", X0 + 720, Y0 + 600)
    mud_level = op("Add", const(0.25, X0 + 720, Y0 + 640), "",
                   op("Multiply", mud_noise, "", const(0.35, X0 + 720, Y0 + 700), "", X0 + 860, Y0 + 680), "",
                   X0 + 1000, Y0 + 660)
    mud_top = op("Add", mud_level, "", const(0.25, X0 + 1000, Y0 + 760), "", X0 + 1140, Y0 + 740)
    rut_damp = smooth(const(0.15, X0 + 1000, Y0 + 900), const(0.6, X0 + 1000, Y0 + 960), damp, "",
                      X0 + 1140, Y0 + 900)
    on_grass = smooth(const(0.3, X0 + 1000, Y0 + 1020), const(0.7, X0 + 1000, Y0 + 1080), grass, "",
                      X0 + 1140, Y0 + 1020)
    wet_rut = op("Multiply", smooth(mud_level, mud_top, centre, "", X0 + 1280, Y0 + 680), "",
                 op("Multiply", rut_damp, "", on_grass, "", X0 + 1280, Y0 + 900), "", X0 + 1420, Y0 + 760)

    G.comment("Mud: darkened, desaturated soil (Wet_RutMud*); wet ground is slicker but never below "
              "Wet_RoughnessFloor, Lumen's trace limit.", X0 + 1640, Y0 - 420, 1100, 1000)
    mud_color = op("Multiply", G.desaturate(soil, 0.4, X0 + 1720, Y0 + 300), "",
                   S("Wet_RutMudDarken", 0.45, X0 + 1720, Y0 + 380), "", X0 + 1880, Y0 + 300)
    rut_mud = op("Multiply", wet_rut, "", S("Wet_RutMudAmount", 0.7, X0 + 1720, Y0 + 460), "", X0 + 1880, Y0 + 440)
    bc = lerp(brk, "BaseColor", mud_color, "", rut_mud, "", X0 + 2040, Y0 + 200)
    rough_scaled = op("Multiply", brk, "Roughness", S("Wet_RoughnessMul", 0.7, X0 + 1880, Y0 - 40), "",
                      X0 + 2040, Y0 - 60)
    rough_floor = op("Max", rough_scaled, "", S("Wet_RoughnessFloor", 0.45, X0 + 1880, Y0 + 20), "",
                     X0 + 2200, Y0 - 40)
    rough = lerp(brk, "Roughness", rough_floor, "", wet_rut, "", X0 + 2360, Y0 - 60)

    make = N("MakeMaterialAttributes", X0 + 2560, Y0 - 100)
    C(bc, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    for pin in ["Metallic", "Specular", "Normal", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    G.function_output("Attributes", make, "", 0, X0 + 2800, Y0 - 100, "Ground with muddy ruts.")
    return G.finish_function(function)


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
        values = fields[i][1:-1] if i < len(fields) else (zero,) * len(FIELD_SUFFIXES)
        for suffix, value in zip(FIELD_SUFFIXES, values):
            mel.set_material_instance_vector_parameter_value(mi, "Field%d_%s" % (i, suffix), value)
    corners = [c for f in fields for c in f[-1]]
    bounds = unreal.LinearColor(min(c[0] for c in corners) - FIELD_BOUNDS_MARGIN,
                                min(c[1] for c in corners) - FIELD_BOUNDS_MARGIN,
                                max(c[0] for c in corners) + FIELD_BOUNDS_MARGIN,
                                max(c[1] for c in corners) + FIELD_BOUNDS_MARGIN) if corners else zero
    mel.set_material_instance_vector_parameter_value(mi, "FieldBounds", bounds)
    mel.set_material_instance_scalar_parameter_value(mi, "FieldCount", float(len(fields)))
    mel.update_material_instance(mi)
    return [f[0] for f in fields]


def field_parameters(actor, spline):
    """(label, A, B, C, D, E, world corners) for one field; the edge planes face inwards (corners are ordered
    counter-clockwise, so the inside is left of every edge)."""
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
    area = sum(corners[k][0] * corners[(k + 1) % 4][1] - corners[(k + 1) % 4][0] * corners[k][1] for k in range(4))
    if area < 0:
        corners.reverse()
    normals, offsets = [], []
    for k in range(4):
        (x0, y0), (x1, y1) = corners[k], corners[(k + 1) % 4]
        length = max(math.hypot(x1 - x0, y1 - y0), 0.001)
        normal = (-(y1 - y0) / length, (x1 - x0) / length)
        normals.append(normal)
        offsets.append(normal[0] * x0 + normal[1] * y0)
    return (actor.get_actor_label(),
            unreal.LinearColor(origin.x, origin.y, math.cos(yaw), math.sin(yaw)),
            unreal.LinearColor(2 * math.pi / spacing, first_row, 0, 0),
            unreal.LinearColor(normals[0][0], normals[0][1], normals[1][0], normals[1][1]),
            unreal.LinearColor(normals[2][0], normals[2][1], normals[3][0], normals[3][1]),
            unreal.LinearColor(*offsets),
            corners)
