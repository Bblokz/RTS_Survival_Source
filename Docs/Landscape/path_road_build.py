"""Builds the old macadam road for ground path splines (Docs/Environment/GroundPathRoad.md).

run() imports the textures from the Blender pipeline (gen_road.py), rebuilds M_GroundPath_Road, its instances and
BP_GroundPathSpline_Road (child of BP_GroundPathSpline). Re-runnable: textures reimport over themselves, the
material graph is cleared and rebuilt, instances and the Blueprint are reused.

Material: physically based (Default Lit: linear albedo 0.06-0.45, roughness 0.6-0.95, AO, normal map, metallic 0),
shaded with the landscape normal plus the road detail normal so 2 m tiles read as one surface,
masked with a dithered edge, so the road writes depth and hides tank ruts and grass under it,
while the ragged shoulder fades into the real landscape. Road textures follow the ribbon UVs; the cracks come
from a world-space crack field: a crack shows where its random value is below a threshold that world noise and
the distance to the road edge raise and lower, and it widens the further it is past that threshold. So cracks
appear, grow and vanish along a road instead of repeating with the 32 m texture.
"""
import unreal

SRC = "D:/Blender/BlenderProjects/BlenderTanks/GroundPath/out_road"
DIR = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Road"
MAT = DIR + "/M_GroundPath_Road"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
RVT_ENV = "/Game/RTS_Survival/Meshes/RVTs/RVT_Environment"
RVT_HEIGHT = "/Game/RTS_Survival/Meshes/RVTs/RVT_Height"
DITHER = "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"
PARENT_BP = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/BP_GroundPathSpline"
GROUP = "Road"
# The road atlas covers PathWidth across and PathWidth * TileLengthRatio along (900 x 3240 cm).
PATH_WIDTH = 900.0
TILE_LENGTH_RATIO = 3.6
GROUND_OFFSET = 20.0

mel = unreal.MaterialEditingLibrary
failures = []

TEXTURES = {
    # name: (compression, srgb, lod group)
    "T_GroundPath_Road_BC": (unreal.TextureCompressionSettings.TC_DEFAULT, True, unreal.TextureGroup.TEXTUREGROUP_WORLD),
    "T_GroundPath_Road_N": (unreal.TextureCompressionSettings.TC_NORMALMAP, False,
                            unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP),
    "T_GroundPath_Road_M": (unreal.TextureCompressionSettings.TC_MASKS, False, unreal.TextureGroup.TEXTUREGROUP_WORLD),
    # BC7 keeps the crack distances precise enough for hairline widths (BC3 would band them to 5 bits).
    "T_GroundPath_RoadCracks": (unreal.TextureCompressionSettings.TC_BC7, False, unreal.TextureGroup.TEXTUREGROUP_WORLD),
}

ROAD_HLSL = """float3 Colour = BC.rgb;
float X = abs(U * 2.0 - 1.0);
float Asphalt = M.a;
float NoiseLarge = NL.r;
float NoiseMedium = NM.g;
float NoiseFine = NM.b;

// Dusty and damp stretches, so neighbouring 32 m tiles do not match.
Colour = lerp(Colour, Colour * DustTint.rgb, smoothstep(0.55, 0.85, NoiseLarge) * DustAmount);
Colour = lerp(Colour, Colour * 0.84, smoothstep(0.38, 0.15, NoiseLarge) * DampAmount);

// Main cracks: more near the crumbling edges and where the large noise is high.
float EdgeFactor = smoothstep(0.45, 0.74, X);
float Threshold = saturate(CrackAmount * (0.3 + 1.2 * NoiseLarge) + EdgeCrackBoost * EdgeFactor);
float Grow = saturate((Threshold - C1.g) / 0.25);
float Width = CrackWidth * (0.35 + 1.65 * Grow);
// Never thinner than about a screen pixel (fwidth of the distance field), with the contrast lowered to match, so
// cracks stay visible as faint lines from the RTS camera instead of vanishing or shimmering.
float CrackPixel = max(fwidth(C1.r), 0.0001);
float CrackDrawn = max(Width, CrackPixel * 1.2);
float Crack = (1.0 - smoothstep(CrackDrawn * 0.5, CrackDrawn, C1.r)) * step(C1.g, Threshold)
    * saturate(Width / CrackDrawn * 1.5);

// Alligator cracking: small blocks in patches.
float BlockThreshold = AlligatorAmount * smoothstep(0.6, 0.85, NoiseMedium);
float BlockDrawn = max(0.05, max(fwidth(C2.r), 0.0001) * 1.2);
float Block = (1.0 - smoothstep(BlockDrawn * 0.5, BlockDrawn, C2.r)) * step(C2.g, BlockThreshold)
    * saturate(0.05 / BlockDrawn * 1.5);

float Cracked = saturate(max(Crack, Block)) * Asphalt;
float HairDrawn = max(0.045, max(fwidth(C1.b), 0.0001) * 1.2);
float Hair = (1.0 - smoothstep(HairDrawn * 0.45, HairDrawn, C1.b)) * saturate(0.045 / HairDrawn * 1.5)
    * smoothstep(0.5, 0.78, NoiseMedium) * HairAmount * Asphalt * (1.0 - Cracked);
// Some cracks have dry grass growing in them.
float Weed = smoothstep(1.0 - WeedAmount, 1.0, NoiseFine);
float3 CrackColour = lerp(CrackColor.rgb, WeedColor.rgb, Weed);
Colour = lerp(Colour, CrackColour, Cracked * 0.9);
Colour *= 1.0 - Hair * 0.35;

// The shoulder takes on the colour of the landscape it fades into.
float ToGround = (1.0 - smoothstep(0.45, 0.98, BC.a)) * GroundBlend;
Colour = lerp(Colour, GroundColor, ToGround);

Roughness = lerp(M.r, 0.95, Cracked);
AO = M.g * (1.0 - 0.55 * Cracked - 0.2 * Hair);
Opacity = saturate(BC.a * 1.35 - 0.2 + (NoiseFine - 0.5) * 0.35);
return Colour;"""


def import_textures():
    tasks = []
    for name in TEXTURES:
        task = unreal.AssetImportTask()
        task.filename = "%s/%s.png" % (SRC, name)
        task.destination_path = DIR
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name, (compression, srgb, group) in TEXTURES.items():
        tex = unreal.load_asset("%s/%s" % (DIR, name))
        tex.set_editor_property("compression_settings", compression)
        tex.set_editor_property("srgb", srgb)
        tex.set_editor_property("lod_group", group)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, False)


def N(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, getattr(unreal, "MaterialExpression" + cls), x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(mat, a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        failures.append("%s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def S(mat, name, value, x, y):
    return N(mat, "ScalarParameter", x, y, parameter_name=name, default_value=value, group=GROUP)


def V(mat, name, rgb, x, y):
    return N(mat, "VectorParameter", x, y, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0),
             group=GROUP)


def sample(mat, texture, sampler, x, y, name=None):
    if name:
        return N(mat, "TextureSampleParameter2D", x, y, texture=unreal.load_asset(texture), sampler_type=sampler,
                 parameter_name=name, group=GROUP)
    return N(mat, "TextureSample", x, y, texture=unreal.load_asset(texture), sampler_type=sampler)


def world_uv(mat, wpxy, scale, x, y, swap=False):
    uv = wpxy
    if swap:
        swapped = N(mat, "AppendVector", x - 80, y + 40)
        g = N(mat, "ComponentMask", x - 160, y + 60, r=False, g=True, b=False, a=False)
        r = N(mat, "ComponentMask", x - 160, y + 120, r=True, g=False, b=False, a=False)
        C(mat, wpxy, "", g, "")
        C(mat, wpxy, "", r, "")
        C(mat, g, "", swapped, "A")
        C(mat, r, "", swapped, "B")
        uv = swapped
    d = N(mat, "Divide", x, y)
    C(mat, uv, "", d, "A")
    C(mat, scale, "", d, "B")
    return d


USED = ["TextureCoordinate", "ComponentMask", "TextureSampleParameter2D", "TextureSample", "WorldPosition",
        "ScalarParameter", "VectorParameter", "Divide", "AppendVector", "RuntimeVirtualTextureSample", "Subtract",
        "Custom", "MaterialFunctionCall", "Constant", "Constant2Vector", "Add", "Transform", "VertexNormalWS",
        "Normalize"]


def clear_material(mat):
    """delete_all_material_expressions silently leaves nodes behind; remove every node class this builder makes."""
    mel.delete_all_material_expressions(mat)
    for cls in USED:
        for index in range(0, 200):
            expression = unreal.find_object(mat, "MaterialExpression%s_%d" % (cls, index))
            if expression:
                mel.delete_material_expression(mat, expression)


def landscape_shading_normal(mat, detail_normal, rvt_env):
    """The tiles' own tilt would light every 2 m tile a little differently, so the normal is the landscape normal
    from RVT_Environment with the road's detail normal added on top (world space), and neighbouring tiles shade
    as one surface. (Moving vertices onto RVT_Height in the vertex shader was tried and dropped: virtual texture
    pages are only made resident by pixel requests, so sunk tiles never fetched their real height and stayed sunk.)
    """
    detail_ws = N(mat, "Transform", -1400, -400,
                  transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                  transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    C(mat, detail_normal, "RGB", detail_ws, "")
    tile_up = N(mat, "VertexNormalWS", -1400, -300)
    detail_delta = N(mat, "Subtract", -1200, -380)
    C(mat, detail_ws, "", detail_delta, "A")
    C(mat, tile_up, "", detail_delta, "B")
    combined = N(mat, "Add", -1000, -360)
    C(mat, rvt_env, "Normal", combined, "A")
    C(mat, detail_delta, "", combined, "B")
    world_normal = N(mat, "Normalize", -850, -360)
    C(mat, combined, "", world_normal, "")
    return world_normal


def get_or_create_material():
    mat = unreal.load_asset(MAT)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_GroundPath_Road", DIR, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def build_material():
    mat = get_or_create_material()
    clear_material(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("used_with_spline_meshes", True)
    mat.set_editor_property("two_sided", False)
    # The normal is built in world space from the landscape normal, see landscape_shading_normal().
    mat.set_editor_property("tangent_space_normal", False)

    T = unreal.MaterialSamplerType
    uv0 = N(mat, "TextureCoordinate", -2400, -400)
    u = N(mat, "ComponentMask", -2200, -460, r=True, g=False, b=False, a=False)
    C(mat, uv0, "", u, "")
    bc = sample(mat, DIR + "/T_GroundPath_Road_BC", T.SAMPLERTYPE_COLOR, -2000, -700, "RoadColor")
    nrm = sample(mat, DIR + "/T_GroundPath_Road_N", T.SAMPLERTYPE_NORMAL, -2000, -400, "RoadNormal")
    msk = sample(mat, DIR + "/T_GroundPath_Road_M", T.SAMPLERTYPE_MASKS, -2000, -100, "RoadMasks")
    for t in (bc, nrm, msk):
        C(mat, uv0, "", t, "UVs")

    wp = N(mat, "WorldPosition", -2800, 300)
    wpxy = N(mat, "ComponentMask", -2600, 300, r=True, g=True, b=False, a=False)
    C(mat, wp, "", wpxy, "")
    crack_scale = S(mat, "CrackScale", 1200.0, -2600, 420)
    block_scale = S(mat, "AlligatorScale", 420.0, -2600, 560)
    large_scale = S(mat, "NoiseLargeScale", 6000.0, -2600, 700)
    medium_scale = S(mat, "NoiseMediumScale", 1500.0, -2600, 840)
    cracks = sample(mat, DIR + "/T_GroundPath_RoadCracks", T.SAMPLERTYPE_LINEAR_COLOR, -2000, 300, "CrackField")
    C(mat, world_uv(mat, wpxy, crack_scale, -2200, 300), "", cracks, "UVs")
    blocks = sample(mat, DIR + "/T_GroundPath_RoadCracks", T.SAMPLERTYPE_LINEAR_COLOR, -2000, 560)
    C(mat, world_uv(mat, wpxy, block_scale, -2200, 560, swap=True), "", blocks, "UVs")
    noise_large = sample(mat, NOISE, T.SAMPLERTYPE_MASKS, -2000, 800)
    C(mat, world_uv(mat, wpxy, large_scale, -2200, 800), "", noise_large, "UVs")
    noise_medium = sample(mat, NOISE, T.SAMPLERTYPE_MASKS, -2000, 1040)
    C(mat, world_uv(mat, wpxy, medium_scale, -2200, 1040), "", noise_medium, "UVs")

    # Landscape colour and normal under the road, from the landscape's runtime virtual texture.
    rvt_env = N(mat, "RuntimeVirtualTextureSample", -2000, 1300, virtual_texture=unreal.load_asset(RVT_ENV),
                material_type=unreal.RuntimeVirtualTextureMaterialType.BASE_COLOR_NORMAL_SPECULAR)
    # No RVT_Height here: on Ger_Trains it reads hilltops too low (its volume clips them), which faded whole tiles out.
    # The parent Blueprint already snaps every 2.2 m tile end to the ground.

    custom = N(mat, "Custom", -1100, 200, code=ROAD_HLSL, description="RoadShade",
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs_spec = [
        ("BC", bc, "RGBA"), ("M", msk, "RGBA"), ("C1", cracks, "RGBA"), ("C2", blocks, "RGBA"),
        ("NL", noise_large, "RGBA"), ("NM", noise_medium, "RGBA"), ("U", u, ""),
        ("CrackAmount", S(mat, "CrackAmount", 0.45, -1500, -600), ""),
        ("CrackWidth", S(mat, "CrackWidth", 0.07, -1500, -500), ""),
        ("EdgeCrackBoost", S(mat, "EdgeCrackBoost", 0.25, -1500, -400), ""),
        ("AlligatorAmount", S(mat, "AlligatorAmount", 0.35, -1500, -300), ""),
        ("HairAmount", S(mat, "HairAmount", 0.6, -1500, -200), ""),
        ("WeedAmount", S(mat, "WeedAmount", 0.25, -1500, -100), ""),
        ("DustAmount", S(mat, "DustAmount", 0.5, -1500, 0), ""),
        ("DampAmount", S(mat, "DampAmount", 0.5, -1500, 100), ""),
        ("CrackColor", V(mat, "CrackColor", (0.035, 0.032, 0.028), -1500, 200), ""),
        ("WeedColor", V(mat, "WeedColor", (0.10, 0.10, 0.045), -1500, 300), ""),
        ("DustTint", V(mat, "DustTint", (1.14, 1.09, 1.0), -1500, 400), ""),
        ("GroundColor", rvt_env, "BaseColor"),
        ("GroundBlend", S(mat, "GroundBlend", 0.85, -1500, 600), ""),
    ]
    inputs = []
    for name, _, _ in inputs_spec:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    outs = []
    for name in ("Roughness", "AO", "Opacity"):
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", name)
        co.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        outs.append(co)
    custom.set_editor_property("additional_outputs", outs)
    for name, node, out in inputs_spec:
        C(mat, node, out, custom, name)

    world_normal = landscape_shading_normal(mat, nrm, rvt_env)

    dither = N(mat, "MaterialFunctionCall", -700, 500)
    dither.set_material_function(unreal.load_asset(DITHER))
    C(mat, custom, "Opacity", dither, "Alpha Threshold")

    for prop, node, out in (
            (unreal.MaterialProperty.MP_BASE_COLOR, custom, ""),
            (unreal.MaterialProperty.MP_ROUGHNESS, custom, "Roughness"),
            (unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, custom, "AO"),
            (unreal.MaterialProperty.MP_NORMAL, world_normal, ""),
            (unreal.MaterialProperty.MP_OPACITY_MASK, dither, "Result")):
        if not mel.connect_material_property(node, out, prop):
            failures.append("%s.%s -> %s" % (node.get_name(), out, prop))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat


def make_instance(name, scalars):
    path = "%s/%s" % (DIR, name)
    mi = unreal.load_asset(path)
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(MAT))
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


# Texture slices along the road. The parent Blueprint makes each tile PathWidth * TileLengthRatio / slice count long,
# so 16 slices give 2.2 m tiles that follow crests and dips; 4 quarter meshes made 8.1 m tiles that sank into hills.
SLICES = 16


def make_meshes():
    """Ribbon slices with Nanite off: copies of the ribbon whose V covers 1/SLICES of the texture each.
    32-triangle tiles gain nothing from Nanite, and a masked material there would take its slower programmable
    raster path."""
    meshes = []
    for index in range(SLICES):
        path = "%s/SM_GroundPath_Road_S%02d" % (DIR, index)
        mesh = unreal.load_asset(path)
        if mesh is None:
            mesh = unreal.EditorAssetLibrary.duplicate_asset(
                "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/SM_GroundPath_Ribbon_Q0", path)
        description = mesh.get_static_mesh_description(0)
        for vertex in range(description.get_vertex_instance_count()):
            instance = unreal.VertexInstanceID(vertex)
            position = description.get_vertex_position(description.get_vertex_instance_vertex(instance))
            uv = description.get_vertex_instance_uv(instance, 0)
            # The ribbon is 100 uu long along X; this slice covers [index, index + 1) / SLICES of V.
            v = index / SLICES + position.x / (100.0 * SLICES)
            description.set_vertex_instance_uv(instance, unreal.Vector2D(uv.x, v), 0)
        mesh.build_from_static_mesh_descriptions([description], False)
        settings = mesh.get_editor_property("nanite_settings")
        settings.enabled = False
        mesh.set_editor_property("nanite_settings", settings)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        meshes.append(mesh)
    return meshes


def make_blueprint(material_instance, meshes):
    path = DIR + "/BP_GroundPathSpline_Road"
    bp = unreal.load_asset(path)
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_asset(PARENT_BP).generated_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_GroundPathSpline_Road", DIR, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("PathMaterial", material_instance)
    cdo.set_editor_property("PathWidth", PATH_WIDTH)
    cdo.set_editor_property("TileLengthRatio", TILE_LENGTH_RATIO)
    cdo.set_editor_property("PathMeshes", meshes)
    # A 9 m rigid tile crossing a side slope pokes into the hill at 8 cm; 20 cm clears mild slopes and is invisible
    # from the RTS camera.
    cdo.set_editor_property("GroundOffset", GROUND_OFFSET)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    return bp


# Demo on Ger_Trains, 118 m east-west with three bends, in the open band between the S_Desert_Mountain_01 backdrop
# (no collision, bounds reach Y -20428, so line traces miss it) and TrainTrack_2 (10.5 m from the road). The tracks
# are BP_RoadSpline meshes that do not block visibility traces either: check spline distances, not just traces.
DEMO_LABEL = "GroundPathDemo_Road"
DEMO_POINTS = [(-16000, -15800), (-13600, -17400), (-11000, -15200), (-8600, -16900), (-6600, -15600)]
# GroundStones/ue_tag_exclusions.py tag; re-placing the actor would otherwise drop it and stones regenerate on the road.
STONE_EXCLUSION_TAG = "PCG_NoStonesPath"


def ground_height(x, y):
    """Landscape height under a point, from a visibility trace (ignores no-collision backdrop meshes)."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    for hit in hits:
        actor = hit.to_tuple()[9]
        if actor and actor.get_actor_label().startswith("Landscape"):
            return hit.to_tuple()[4].z
    return 300.0


def place_demo():
    """(Re)places the demo road and saves the level."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label() == DEMO_LABEL:
            actors.destroy_actor(actor)
    road_class = unreal.load_class(None, DIR + "/BP_GroundPathSpline_Road.BP_GroundPathSpline_Road_C")
    first = DEMO_POINTS[0]
    road = actors.spawn_actor_from_class(road_class, unreal.Vector(first[0], first[1], 300))
    road.set_actor_label(DEMO_LABEL)
    road.set_folder_path("GroundPaths")
    road.tags = [unreal.Name(STONE_EXCLUSION_TAG)]
    spline = road.get_component_by_class(unreal.SplineComponent)
    # The Blueprint snaps each tile by tracing down from the spline, so every point must start above the ground.
    spline.set_spline_points([unreal.Vector(x, y, ground_height(x, y) + 100.0) for x, y in DEMO_POINTS],
                             unreal.SplineCoordinateSpace.WORLD, True)
    # Without this flag the construction script resets the spline to its default.
    spline.set_editor_property("spline_has_been_edited", True)
    # Setting PathWidth to the value it already has does not rerun construction; nudge it.
    road.set_editor_property("PathWidth", PATH_WIDTH + 1.0)
    road.set_editor_property("PathWidth", PATH_WIDTH)
    tiles = len(road.get_components_by_class(unreal.SplineMeshComponent))
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("demo road tiles", tiles, "level saved", saved)
    return road


def run(textures=True):
    failures.clear()
    if textures:
        import_textures()
    build_material()
    road = make_instance("MI_GroundPath_Road", {"CrackAmount": 0.62, "CrackWidth": 0.095, "EdgeCrackBoost": 0.3})
    make_instance("MI_GroundPath_Road_WarTorn", {"CrackAmount": 0.75, "AlligatorAmount": 0.65, "HairAmount": 0.9,
                                                 "WeedAmount": 0.1, "DustAmount": 0.7})
    make_blueprint(road, make_meshes())
    print("road build failures:", failures)
    return failures
