"""Landscape grass for Ger_Trains: grass types from the GroundFoliage atlas meshes plus a switched Landscape Grass
Output in M_RTS_Landscape_DryCastle (Docs/Environment/GroundGrass.md).

Re-runnable: grass type assets are updated in place; master nodes carry DESC and are removed first.
Weights follow what the player sees, not the painted layers: green grass and flower patches use the breakup chain's
final grassiness (LS_NoiseBreakup target, "g_t"), dry grass uses the soft sand layers where the ground is not green.
StaticSwitchParameter LS_GroundGrass defaults to False (every weight is constant 0, no instances); only
MI_RTS_Landscape_DryCastle_Trains turns it on.
Usage (UECP execute_python): sys.path + import ls_grass_build; ls_grass_build.run()
"""
import unreal

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
MI_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Trains"
# Top-down clump cards (GroundFoliage/build_topdown_*.py): made for the RTS camera looking down from 30-60 m.
MESH_DIR = "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/MeshesTop/"
GRASS_DIR = "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Grass"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
DESC = "LSGroundGrass"
GROUP = "Ground Grass"
SWITCH = "LS_GroundGrass"
BREAKUP_DESC = "LSBreakup"
# Editor positions of the breakup chain's nodes (ls_breakup_build.py X0=-3200, Y0=-3400).
GRASSINESS_POS = (-1200, -3000)
SOFT_GROUND_POS = (-1820, -2440)

# Density is instances per 100 m2 at full weight. Cull distances are from the camera; the RTS camera sits 30-60 m
# from the ground, so grass ends just past the far edge of the view and never casts dynamic shadows.
TUFT_CULL = (7000, 9000)
GROUND_CULL = (6000, 8000)
# Each card is a whole clump about 0.9 m across (several carry companion clumps), drawn 1.1-1.75x so a clump covers
# 15-25 screen pixels from the RTS camera; about one green clump per 1.6 m2 at full weight.
VARIETIES = {
    "LGT_Trains_Green": [
        ("SM_GFT_Green_A", 18.0, (1.1, 1.75), TUFT_CULL),
        ("SM_GFT_Green_B", 16.0, (1.1, 1.7), TUFT_CULL),
        ("SM_GFT_Green_C", 16.0, (1.1, 1.7), TUFT_CULL),
        ("SM_GFT_Green_D", 13.0, (1.05, 1.6), TUFT_CULL),
    ],
    "LGT_Trains_Flowers": [
        ("SM_GFT_Poppy", 6.0, (1.0, 1.5), GROUND_CULL),
        ("SM_GFT_Chamomile", 5.0, (1.0, 1.5), GROUND_CULL),
        ("SM_GFT_Cornflower", 4.0, (1.0, 1.5), GROUND_CULL),
    ],
    "LGT_Trains_Dry": [
        ("SM_GFT_DryStar_A", 12.0, (1.1, 1.75), TUFT_CULL),
        ("SM_GFT_DryStar_B", 9.0, (1.1, 1.7), TUFT_CULL),
        ("SM_GFT_OliveTransition", 9.0, (1.1, 1.7), TUFT_CULL),
        ("SM_GFT_StrawLitter", 8.0, (1.1, 1.6), GROUND_CULL),
    ],
}
# Flower patches: world-space noise, about 25 m across, threshold picks roughly a fifth of the meadow.
FLOWER_PATCH_SCALE = 2500.0
FLOWER_PATCH_MIN = 0.58
FLOWER_PATCH_MAX = 0.72
# Lake beds are painted grass too; nothing grows below the water surface (Ger_Trains lake top is about -310).
GRASS_MIN_HEIGHT = -300.0
GRASS_HEIGHT_FADE = 80.0

mel = unreal.MaterialEditingLibrary
failures = []

def tagged_expressions(material, desc):
    """Every expression of the material carrying desc. Walks the material's real expressions: looking them up by
    name index misses nodes once repeated rebuilds push the numbering past any fixed range."""
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression)
            if e.get_outer() == material and e.get_editor_property("desc") == desc]

mat = None


def make_variety(mesh_name, density, scale, cull):
    variety = unreal.GrassVariety()
    variety.set_editor_property("grass_mesh", unreal.load_asset(MESH_DIR + mesh_name))
    variety.set_editor_property("grass_density", unreal.PerPlatformFloat(default=density))
    variety.set_editor_property("start_cull_distance", unreal.PerPlatformInt(default=cull[0]))
    variety.set_editor_property("end_cull_distance", unreal.PerPlatformInt(default=cull[1]))
    variety.set_editor_property("scaling", unreal.GrassScaling.UNIFORM)
    variety.set_editor_property("scale_x", unreal.FloatInterval(min=scale[0], max=scale[1]))
    variety.set_editor_property("random_rotation", True)
    variety.set_editor_property("align_to_surface", True)
    variety.set_editor_property("use_grid", True)
    variety.set_editor_property("placement_jitter", 1.0)
    # Thin weights (layer edges, patch borders) give smaller plants instead of a hard line.
    variety.set_editor_property("weight_attenuates_max_scale", True)
    variety.set_editor_property("max_scale_weight_attenuation", 0.4)
    variety.set_editor_property("cast_dynamic_shadow", False)
    variety.set_editor_property("cast_contact_shadow", True)
    variety.set_editor_property("affect_distance_field_lighting", False)
    variety.set_editor_property("receives_decals", True)
    variety.set_editor_property("use_landscape_lightmap", False)
    return variety


def create_grass_types():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    grass_types = {}
    for asset_name, entries in VARIETIES.items():
        path = GRASS_DIR + "/" + asset_name
        grass_type = unreal.load_asset(path)
        if not grass_type:
            grass_type = tools.create_asset(asset_name, GRASS_DIR, unreal.LandscapeGrassType,
                                            unreal.LandscapeGrassTypeFactory())
        grass_type.set_editor_property("grass_varieties", [make_variety(*entry) for entry in entries])
        # The Foliage density slider drives grass.DensityScale; only types with this on follow it.
        grass_type.set_editor_property("enable_density_scaling", True)
        unreal.EditorAssetLibrary.save_loaded_asset(grass_type, False)
        grass_types[asset_name] = grass_type
    return grass_types


def N(cls, x, y, **props):
    e = mel.create_material_expression(mat, getattr(unreal, "MaterialExpression" + cls), x, y)
    e.set_editor_property("desc", DESC)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        failures.append("%s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def remove_previous():
    removed = 0
    for e in tagged_expressions(mat, DESC):
        mel.delete_material_expression(mat, e)
        removed += 1
    return removed


def find_breakup_node(position):
    for e in tagged_expressions(mat, BREAKUP_DESC):
        if e.get_class().get_name() != "MaterialExpressionSaturate":
            continue
        if (e.get_editor_property("material_expression_editor_x"),
                e.get_editor_property("material_expression_editor_y")) == position:
            return e
    raise RuntimeError("breakup node at %s not found (run ls_breakup_build.py first)" % (position,))


def field_free_mask(x, y):
    """1 - field mask from the ground-detail furrow node; 1 everywhere when that chain is not built."""
    furrow = None
    for e in tagged_expressions(mat, "LSGroundDetail"):
        if e.get_class().get_name() == "MaterialExpressionCustom" and                 e.get_editor_property("description") == "FieldFurrows":
            furrow = e
    if furrow is None:
        return N("Constant", x, y, r=1.0)
    field = N("ComponentMask", x, y, r=True, g=False, b=False, a=False)
    C(furrow, "", field, "")
    free = N("OneMinus", x + 160, y)
    C(field, "", free, "")
    return free


def gated(weight, weight_out, x, y):
    """LS_GroundGrass on: the weight; off: constant 0, so other maps on this master spawn no grass."""
    switch = N("StaticSwitchParameter", x, y, parameter_name=SWITCH, default_value=False, group=GROUP)
    C(weight, weight_out, switch, "True")
    C(N("Constant", x - 160, y + 80, r=0.0), "", switch, "False")
    return switch


def set_grass_inputs(output, grass_types):
    inputs = []
    for asset_name in VARIETIES:
        grass_input = unreal.GrassInput()
        grass_input.set_editor_property("name", asset_name)
        grass_input.set_editor_property("grass_type", grass_types[asset_name])
        inputs.append(grass_input)
    output.set_editor_property("grass_types", inputs)


def build(grass_types):
    X0, Y0 = -3200, -1300
    N("Comment", X0 - 60, Y0 - 120, text="Ground grass (LS_GroundGrass): GroundFoliage atlas cards as landscape grass. "
      "Green and flowers follow the breakup's final grassiness, dry grass the soft sand layers.")
    grassiness = find_breakup_node(GRASSINESS_POS)
    soft_ground = find_breakup_node(SOFT_GROUND_POS)
    # Farm fields (ls_detail_build.py FieldFurrows, R = field mask) are tilled soil: no meadow grass inside them.
    not_field = field_free_mask(X0 - 400, Y0 + 900)

    wp = N("WorldPosition", X0, Y0 + 400)
    # Above the water: 0 below Grass_MinHeight, 1 from Grass_MinHeight + fade.
    wp_z = N("ComponentMask", X0 + 160, Y0 + 600, r=False, g=False, b=True, a=False)
    C(wp, "", wp_z, "")
    min_height = N("ScalarParameter", X0 + 160, Y0 + 700, parameter_name="Grass_MinHeight",
                   default_value=GRASS_MIN_HEIGHT, group=GROUP)
    max_height = N("Add", X0 + 320, Y0 + 700, const_b=GRASS_HEIGHT_FADE)
    C(min_height, "", max_height, "A")
    dry_land = N("SmoothStep", X0 + 480, Y0 + 620)
    C(min_height, "", dry_land, "Min")
    C(max_height, "", dry_land, "Max")
    C(wp_z, "", dry_land, "Value")
    green_land = N("Multiply", X0 + 640, Y0 - 100)
    C(grassiness, "", green_land, "A")
    C(dry_land, "", green_land, "B")
    green = N("Multiply", X0 + 800, Y0 - 100)
    C(green_land, "", green, "A")
    C(not_field, "", green, "B")

    not_green = N("OneMinus", X0, Y0 + 200)
    C(grassiness, "", not_green, "")
    dry_soft = N("Multiply", X0 + 160, Y0 + 200)
    C(soft_ground, "", dry_soft, "A")
    C(not_green, "", dry_soft, "B")
    dry_on_land = N("Multiply", X0 + 640, Y0 + 200)
    C(dry_soft, "", dry_on_land, "A")
    C(dry_land, "", dry_on_land, "B")
    dry = N("Multiply", X0 + 800, Y0 + 200)
    C(dry_on_land, "", dry, "A")
    C(not_field, "", dry, "B")

    wpxy = N("ComponentMask", X0 + 160, Y0 + 400, r=True, g=True, b=False, a=False)
    C(wp, "", wpxy, "")
    patch_uv = N("Divide", X0 + 320, Y0 + 400, const_b=FLOWER_PATCH_SCALE)
    C(wpxy, "", patch_uv, "A")
    noise = N("TextureSample", X0 + 480, Y0 + 400, texture=unreal.load_asset(NOISE),
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(patch_uv, "", noise, "UVs")
    patches = N("SmoothStep", X0 + 720, Y0 + 400, const_min=FLOWER_PATCH_MIN, const_max=FLOWER_PATCH_MAX)
    C(noise, "G", patches, "Value")
    flowers = N("Multiply", X0 + 900, Y0 + 300)
    C(green, "", flowers, "A")
    C(patches, "", flowers, "B")

    output = N("LandscapeGrassOutput", X0 + 1400, Y0 + 200)
    set_grass_inputs(output, grass_types)
    C(gated(green, "", X0 + 1100, Y0), "", output, "LGT_Trains_Green")
    C(gated(flowers, "", X0 + 1100, Y0 + 300), "", output, "LGT_Trains_Flowers")
    C(gated(dry, "", X0 + 1100, Y0 + 150), "", output, "LGT_Trains_Dry")


def enable_on_trains():
    mi = unreal.load_asset(MI_PATH)
    mel.set_material_instance_static_switch_parameter_value(mi, SWITCH, True)
    mel.update_material_instance(mi)
    return mi


# The editor renders grass maps only in Landscape mode or on Build > Build Grass Maps Only. Run that after this
# script (once shaders finished compiling, else components are skipped silently) and save the map: -game and cooked
# runs read the grass maps serialized in the level.


def tune_look():
    """Grass types only (meshes, densities, scales): no landscape recompile, grass maps stay valid."""
    create_grass_types()


def run(enable=True):
    global mat
    mat = unreal.load_asset(MAT_PATH)
    failures.clear()
    grass_types = create_grass_types()
    removed = remove_previous()
    build(grass_types)
    mel.recompile_material(mat)
    print("removed", removed, "failures", failures)
    if enable:
        enable_on_trains()
    return failures
