"""Landscape grass for Ger_Trains: grass types from the GroundFoliage atlas meshes, and their weights as a material
function, MF_RTS_LS_GrassWeights (Docs/Environment/GroundGrass.md).

MF_RTS_LS_GrassWeights(Grassiness, SandWeight, FieldMask, BlightMask optional) -> Green, Flowers, Dry
  Weights follow what the player sees, not the painted layers: green grass and flower patches use the breakup's final
  grassiness, dry grass uses the soft sand layers where the ground is not green. Nothing grows under water
  (Grass_MinHeight), inside farm fields or on radixite-blighted ground; dead straw rings the blight.
M_RTS_Landscape_DryCastle feeds the weights to its LandscapeGrassOutput through StaticSwitchParameter LS_GroundGrass
(off: every weight is constant 0, no instances); only MI_RTS_Landscape_DryCastle_Trains turns it on
(ls_master_build.run()). Grass type assets are updated in place.
"""
import unreal

import ls_graph as G
from ls_graph import N, C, S, mask

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
MI_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Trains"
# Top-down clump cards (GroundFoliage/build_topdown_*.py): made for the RTS camera looking down from 30-60 m.
MESH_DIR = "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/MeshesTop/"
GRASS_DIR = "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Grass"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
FUNCTION_NAME = "MF_RTS_LS_GrassWeights"
GROUP = "Ground Grass"
SWITCH = "LS_GroundGrass"

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
# Dry-grass weight of the dead straw ring at the radixite blight's rim.
DEAD_TUFT_WEIGHT = 0.8
GRASS_HEIGHT_FADE = 80.0

mel = unreal.MaterialEditingLibrary


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


def set_grass_inputs(output, grass_types):
    inputs = []
    for asset_name in VARIETIES:
        grass_input = unreal.GrassInput()
        grass_input.set_editor_property("name", asset_name)
        grass_input.set_editor_property("grass_type", grass_types[asset_name])
        inputs.append(grass_input)
    output.set_editor_property("grass_types", inputs)


def build_function():
    function = G.function_asset(FUNCTION_NAME, "Landscape grass weights: green clumps and flower patches where the "
                                "ground looks green, dry tufts on soft sand elsewhere; none under water or in fields.")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    grassiness = G.function_input("Grassiness", G.SCALAR, 0, X0 - 300, Y0 - 100, "Visible grass amount, 0..1.")
    soft_ground = G.function_input("SandWeight", G.SCALAR, 1, X0 - 300, Y0 + 200, "Soft sand layers, 0..1.")
    field = G.function_input("FieldMask", G.SCALAR, 2, X0 - 300, Y0 + 900, "1 inside a farm field (tilled soil).")
    not_field = N("OneMinus", X0 - 100, Y0 + 900)
    C(field, "", not_field, "")
    blight = G.function_input("BlightMask", G.SCALAR, 3, X0 - 300, Y0 + 1100,
                              "Optional: 1 on dead ground around radixite (MF_RTS_LS_RadixiteBlight).",
                              G.const(0.0, X0 - 460, Y0 + 1100), "")
    alive = N("OneMinus", X0 - 100, Y0 + 1100)
    C(blight, "", alive, "")

    G.comment("Above the water: 0 below Grass_MinHeight, 1 from Grass_MinHeight + %d cm. Lake beds are painted "
              "grass too." % GRASS_HEIGHT_FADE, X0 - 60, Y0 + 520, 700, 300, G.INPUT_COLOR)
    wp = N("WorldPosition", X0, Y0 + 400)
    wp_z = mask(wp, "", "B", X0 + 160, Y0 + 600)
    min_height = S("Grass_MinHeight", GRASS_MIN_HEIGHT, X0 + 160, Y0 + 700)
    max_height = N("Add", X0 + 320, Y0 + 700, const_b=GRASS_HEIGHT_FADE)
    C(min_height, "", max_height, "A")
    dry_land = G.smooth(min_height, max_height, wp_z, "", X0 + 480, Y0 + 620)

    G.comment("Green: the visible grassiness on dry land outside the fields and the radixite blight. Dry: soft sand "
              "where the ground is not green, plus dead straw at the blight's rim.", X0 + 580, Y0 - 160, 560, 500)
    green = G.mul(G.mul(G.mul(grassiness, "", dry_land, "", X0 + 640, Y0 - 100), "", not_field, "", X0 + 800, Y0 - 100),
                  "", alive, "", X0 + 940, Y0 - 100)
    not_green = N("OneMinus", X0, Y0 + 200)
    C(grassiness, "", not_green, "")
    dry_soft = G.mul(soft_ground, "", not_green, "", X0 + 160, Y0 + 200)
    dry_alive = G.mul(G.mul(dry_soft, "", dry_land, "", X0 + 640, Y0 + 200), "", not_field, "", X0 + 800, Y0 + 200)
    # Dead straw at the blight's rim (4 b (1 - b) peaks at the edge), none on the dead ground itself.
    rim = G.mulc(G.mul(blight, "", alive, "", X0 + 640, Y0 + 1180), "", 4.0 * DEAD_TUFT_WEIGHT, X0 + 800, Y0 + 1180)
    dead_tufts = G.mul(G.mul(rim, "", dry_land, "", X0 + 940, Y0 + 1180), "", not_field, "", X0 + 1080, Y0 + 1180)
    dry = G.op("Add", G.mul(dry_alive, "", alive, "", X0 + 940, Y0 + 200), "", dead_tufts, "", X0 + 1080, Y0 + 200)

    G.comment("Flowers: about %d m noise patches (G channel) inside the green." % (FLOWER_PATCH_SCALE / 100),
              X0 + 100, Y0 + 340, 1000, 180, G.LAYER_COLOR)
    wpxy = mask(wp, "", "RG", X0 + 160, Y0 + 400)
    patch_uv = N("Divide", X0 + 320, Y0 + 400, const_b=FLOWER_PATCH_SCALE)
    C(wpxy, "", patch_uv, "A")
    noise = N("TextureSample", X0 + 480, Y0 + 400, texture=unreal.load_asset(NOISE),
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    C(patch_uv, "", noise, "UVs")
    patches = N("SmoothStep", X0 + 720, Y0 + 400, const_min=FLOWER_PATCH_MIN, const_max=FLOWER_PATCH_MAX)
    C(noise, "G", patches, "Value")
    flowers = G.mul(green, "", patches, "", X0 + 900, Y0 + 300)

    G.function_output("Green", green, "", 0, X0 + 1100, Y0 - 100, "Weight for LGT_Trains_Green.")
    G.function_output("Flowers", flowers, "", 1, X0 + 1100, Y0 + 300, "Weight for LGT_Trains_Flowers.")
    G.function_output("Dry", dry, "", 2, X0 + 1100, Y0 + 150, "Weight for LGT_Trains_Dry.")
    return G.finish_function(function)


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
