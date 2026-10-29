"""Builds M_RTS_Landscape_DryCastle's effect pipeline from the landscape material functions and lays out the master
(Docs/Landscape/LandscapeMaster.md).

Pipeline, left to right; each effect sits behind a static switch whose False input skips it:
  LandscapeLayerBlend_0 -> MF_RTS_LS_GroundBreakup [LS_NoiseBreakup] -> MF_RTS_LS_RadixiteBlight
  [LS_RadixiteBlight] -> MF_RTS_ColorVariation_Landscape (the RVT output reads here) -> MF_RTS_LS_TankTrails
  [LS_TankTrails] -> MF_RTS_LS_FieldFurrows -> MF_RTS_LS_WetRuts [LS_GroundDetail] -> BreakMaterialAttributes_2.
  MF_RTS_LS_GrassWeights -> [LS_GroundGrass] -> LandscapeGrassOutput.
Every switch defaults to False; MI_RTS_Landscape_DryCastle_Inst turns LS_TankTrails on, MI_RTS_Landscape_DryCastle_Trains
(Ger_Trains) turns all of them on. M_RTS_Landscape_DesertPaintable gets the radixite blight before its colour
variation too (wire_desert_paintable(); its tank trails come from ls_trails_build.run_desert_paintable()).
Re-runnable: master nodes carry DESC (older builder tags are removed too); the functions are rebuilt in place, keeping
their inputs and outputs, so every material that calls them stays connected.
The original hand-made graph is only moved and framed with comments. Comment sizes and the output node position
need URTSMaterialGraphLibrary; from an editor built without it, apply them afterwards with ls_editor_layout.py. After a run, once shaders compiled, use
Build > Build Grass Maps Only and save Ger_Trains: -game reads the grass maps stored in the level.
Usage (UECP python): sys.path.insert(0, <this folder>); import ls_master_build; ls_master_build.run()
"""
import importlib

import unreal

import ls_graph as G
import ls_breakup_build
import ls_trails_build
import ls_detail_build
import ls_grass_build
import ls_blight_build

for module in [G, ls_breakup_build, ls_trails_build, ls_detail_build, ls_grass_build, ls_blight_build]:
    importlib.reload(module)
from ls_graph import N, C

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
MI_INST_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Inst"
MI_TRAINS_PATH = ls_detail_build.MI_PATH
DESC = "LSMaster"
OLD_DESCS = ["LSBreakup", "LSTankTrails", "LSGroundDetail", "LSGroundGrass"]
SWITCHES = {"LS_NoiseBreakup": "Ground Breakup", "LS_RadixiteBlight": "Radixite Blight",
            "LS_TankTrails": "Tank Trails", "LS_GroundDetail": "Ground Detail", "LS_GroundGrass": "Ground Grass"}
DESERT_PAINTABLE_PATH = ls_trails_build.DESERT_PAINTABLE["material"]
# DesertPaintable's blight nodes: that master is not rebuilt as a whole, so its additions carry their own tag.
BLIGHT_DESC = "LSBlight"
# Soft layers: grass may grow into the sand ones; tank trails dent all of them (rock and roads keep no ruts).
GRASS_LAYER = "GrassCastle"
SAND_LAYERS = ["Desert", "RTSSand", "GraySand"]

# The original hand-made graph, framed with comments (node names, comment text, colour).
ORIGINAL_SECTIONS = [
    (["MaterialFunctionCall_2", "MaterialFunctionCall_13", "TextureObject_0", "TextureObject_1", "TextureObject_2",
      "TextureObject_3", "TextureSample_15", "MaterialFunctionCall_12", "StaticBool_0", "StaticBool_1",
      "TextureSample_3", "MakeMaterialAttributes_3", "LandscapeLayerCoords_0", "Divide_3",
      "NamedRerouteDeclaration_4", "TextureSample_8", "ScalarParameter_3", "Multiply_1", "TextureSample_13",
      "Saturate_0"],
     "Asphalt road layer: asphalt and concrete textures mixed by MF_RTS_TextureObject_To_TextureSample_01, colour "
     "varied by MF_RTS_Color_Variations_01. TilingAsphalt scales the road UVs (reroute AslphaltUVs). Goes into the "
     "layer blend.", G.LAYER_COLOR),
    (["TextureSample_7", "BlendMaterialAttributes_2", "NamedRerouteDeclaration_0", "TextureSample_4",
      "MaterialFunctionCall_19", "NamedRerouteUsage_2", "MakeMaterialAttributes_0", "WorldPosition_0",
      "ComponentMask_0", "Divide_1", "TextureSample_6", "ScalarParameter_0", "TextureSample_5"],
     "Stone ground: MF_Beach becomes the StoneGround reroute. Next to it, boulder textures in world space "
     "(TilingStoneGround) blended over the stone ground by the packed noise.", G.LAYER_COLOR),
    (["TextureSample_12", "Multiply_3", "TextureSample_9", "LinearInterpolate_0", "MakeMaterialAttributes_2",
      "WorldPosition_1", "ComponentMask_1", "NamedRerouteDeclaration_3", "Divide_2", "TextureSample_11",
      "ScalarParameter_2", "TextureSample_10", "Constant3Vector_2"],
     "Boulder layer (reroute Boulder): stone ground with tinted soil mixed in by the packed noise, world-space UVs "
     "(TilingBoulder). The ground breakup reuses its T_Soil_1_BC and T_StoneGround_1_N samples for bare soil.",
     G.LAYER_COLOR),
    (["LandscapeLayerCoords_1", "Divide_0", "NamedRerouteDeclaration_1", "ScalarParameter_1", "Comment_2",
      "NamedRerouteUsage_0", "TextureSample_0", "TextureSample_1", "TextureSample_2", "Multiply_0",
      "Constant3Vector_0", "Constant_2", "MakeMaterialAttributes_1", "NamedRerouteDeclaration_2"],
     "Thai sand layer (reroute ThaiSand): scanned sand textures on layer UVs (TillingSize, reroute TilledUVs), "
     "tinted.", G.LAYER_COLOR),
    (["Constant_3", "MaterialFunctionCall_4", "TextureSample_14", "MakeMaterialAttributes_4",
      "NamedRerouteDeclaration_5", "Constant_0", "NamedRerouteUsage_3", "Multiply_2", "TextureSample_17",
      "ScalarParameter_4", "TextureSample_16"],
     "Industrial concrete layer (reroute IndustrialConcrete): concrete textures (IndustrialConcreteTilling) with red "
     "sand blown over them (MF_RTS_RedSandPanner_Lerp).", G.LAYER_COLOR),
    (["TextureSample_22", "Constant_1", "MaterialFunctionCall_7", "TextureSample_18", "MakeMaterialAttributes_5",
      "NamedRerouteDeclaration_7", "Constant_4", "TextureSample_21", "NamedRerouteUsage_5", "Multiply_4",
      "TextureSample_20", "ScalarParameter_6"],
     "Second industrial concrete layer (reroute ConcreteIndustrial2): basic concrete "
     "(IndustrialConcrete2Tilling) with red sand blown over it.", G.LAYER_COLOR),
    (["MaterialFunctionCall_0", "MaterialFunctionCall_1", "MaterialFunctionCall_5", "MaterialFunctionCall_6",
      "NamedRerouteUsage_1", "NamedRerouteUsage_4", "NamedRerouteUsage_10", "LandscapeLayerBlend_0"],
     "Layer blend: the painted landscape layers (MF_RTS_Layer_* and the reroutes above) blended by their paint "
     "weights. Everything to the right of here works on this result.", G.INPUT_COLOR),
]
# The material output node, right of BreakMaterialAttributes_2 (placed by place_output()).
OUTPUT_POSITION = (5200, 240)
# Node footprint for comment frames (graph units); texture samples are the tallest common nodes.
NODE_WIDTH = 260
NODE_HEIGHT = 280
FRAME_MARGIN = 60
# The original layer sections (all but the layer blend, the last section) are stacked in one column, right edge at
# LAYER_COLUMN_RIGHT and top at LAYER_COLUMN_TOP, so their frames never overlap. Each keeps its own inner layout.
LAYER_COLUMN_RIGHT = -1900
LAYER_COLUMN_TOP = -1600
SECTION_GAP = 260


def find(material, name):
    e = unreal.find_object(material, "MaterialExpression" + name)
    if not e:
        raise RuntimeError("missing MaterialExpression" + name)
    return e


def find_sample(material, texture_name):
    """The master's own TextureSample of a texture; reusing it adds no texture fetch."""
    for e in unreal.ObjectIterator(unreal.MaterialExpressionTextureSample):
        if e.get_outer() != material or e.get_editor_property("desc"):
            continue
        texture = e.get_editor_property("texture")
        if texture and texture.get_name() == texture_name:
            return e
    raise RuntimeError("no sample of " + texture_name)


def move(node, x, y):
    node.set_editor_property("material_expression_editor_x", x)
    node.set_editor_property("material_expression_editor_y", y)


def switch(name, true_node, true_out, false_node, false_out, x, y):
    e = N("StaticSwitchParameter", x, y, parameter_name=name, default_value=False, group=SWITCHES[name])
    C(true_node, true_out, e, "True")
    C(false_node, false_out, e, "False")
    return e


def place_output(material):
    """Moves the material's output node next to the main break; False when this editor lacks the library."""
    library = getattr(unreal, "RTSMaterialGraphLibrary", None)
    return bool(library) and library.set_material_output_position(material, *OUTPUT_POSITION)


def section_bounds(nodes):
    """(left, top, right, bottom) of a section's frame."""
    xs = [n.get_editor_property("material_expression_editor_x") for n in nodes]
    ys = [n.get_editor_property("material_expression_editor_y") for n in nodes]
    return (min(xs) - FRAME_MARGIN, min(ys) - FRAME_MARGIN - 40,
            max(xs) + NODE_WIDTH + FRAME_MARGIN, max(ys) + NODE_HEIGHT + FRAME_MARGIN)


def arrange_original(material):
    """Stacks the original layer sections in one column left of the layer blend."""
    top = LAYER_COLUMN_TOP
    for names, _, _ in ORIGINAL_SECTIONS[:-1]:
        nodes = [find(material, name) for name in names]
        left, section_top, right, bottom = section_bounds(nodes)
        dx, dy = LAYER_COLUMN_RIGHT - right, top - section_top
        for n in nodes:
            move(n, n.get_editor_property("material_expression_editor_x") + dx,
                 n.get_editor_property("material_expression_editor_y") + dy)
        top += bottom - section_top + SECTION_GAP


def frame_original(material):
    """Comment frames around the original hand-made sections."""
    for names, text, color in ORIGINAL_SECTIONS:
        left, top, right, bottom = section_bounds([find(material, name) for name in names])
        G.comment(text, left, top, right - left, bottom - top, color)


def layer_weights(x, y):
    """Painted weights: grass, the sand layers (saturated), and every soft layer (saturated) for the trails."""
    grass = N("LandscapeLayerSample", x, y, parameter_name=GRASS_LAYER, preview_weight=0.0)
    total = None
    for index, layer in enumerate(SAND_LAYERS):
        weight = N("LandscapeLayerSample", x, y + 120 * (index + 1), parameter_name=layer, preview_weight=0.0)
        total = weight if total is None else G.op("Add", total, "", weight, "", x + 260, y + 120 * (index + 1))
    sand = G.sat(total, "", x + 420, y + 240)
    soft = G.sat(G.op("Add", grass, "", total, "", x + 420, y + 380), "", x + 560, y + 380)
    return grass, sand, soft


def build(material, functions):
    layer_blend = find(material, "LandscapeLayerBlend_0")
    grass_layer = find(material, "MaterialFunctionCall_1")
    color_var = find(material, "MaterialFunctionCall_3")
    out_break = find(material, "BreakMaterialAttributes_2")
    breakup, trails, furrows, wet_ruts, grass_weights, blight = functions
    Y = 240

    G.comment("Layer weights: the painted grass layer, the sand layers (grass grows into them) and every soft layer "
              "(tank trails dent them; rock and roads keep no ruts).", -1560, 1160, 860, 680, G.INPUT_COLOR)
    grass_w, sand_w, soft_w = layer_weights(-1500, 1300)

    G.comment("1  Ground breakup (LS_NoiseBreakup): noise-broken grass edges, dirt patches, dry grass, damp hollows. "
              "Also returns Grassiness, Damp and DirtColor for the effects after it. Bare soil reuses the boulder "
              "layer's soil samples.", -100, Y - 200, 700, 640)
    breakup_call = G.function_call(breakup, 0, Y)
    C(layer_blend, "", breakup_call, "LayerBlend")
    C(grass_layer, "", breakup_call, "GrassLayer")
    C(grass_w, "", breakup_call, "GrassWeight")
    C(sand_w, "", breakup_call, "SandWeight")
    C(find_sample(material, "T_Soil_1_BC"), "RGB", breakup_call, "SoilColor")
    C(find_sample(material, "T_StoneGround_1_N"), "RGB", breakup_call, "SoilNormal")
    breakup_switch = switch("LS_NoiseBreakup", breakup_call, "Attributes", layer_blend, "", 400, Y)

    G.comment("2  Radixite blight (LS_RadixiteBlight): dead, ash-grey ground around radixite crystals "
              "(MPC_RadixiteBlight, filled by URadixiteBlightSubsystem). Before the colour variation, so the RVT "
              "holds it. Blight also feeds the grass.", 640, Y - 200, 700, 640)
    blight_call = G.function_call(blight, 740, Y)
    C(breakup_switch, "", blight_call, "Attributes")
    C(breakup_call, "DirtColor", blight_call, "DirtColor")
    blight_switch = switch("LS_RadixiteBlight", blight_call, "Attributes", breakup_switch, "", 1140, Y)

    G.comment("3  Colour variation, then the runtime virtual texture output. The RVT caches its pages, so it reads "
              "the ground here: nothing after this point (tank trails, ground detail) may change it.",
              1440, -460, 1260, 1040, G.OUTPUT_COLOR)
    move(color_var, 1560, Y)
    C(blight_switch, "", color_var, "InputMaterial")
    move(find(material, "Constant_9"), 1460, Y + 220)
    move(find(material, "Constant_10"), 1460, Y + 300)
    move(find(material, "GetMaterialAttributes_0"), 1800, -300)
    move(find(material, "Transform_0"), 2060, -160)
    move(find(material, "WorldPosition_2"), 2060, -40)
    move(find(material, "RuntimeVirtualTextureOutput_0"), 2360, -300)

    G.comment("4  Tank trails (LS_TankTrails): ruts from RT_TankTrails on soft ground. Main pass only. RutCentre "
              "feeds the wet ruts.", 2760, Y - 200, 760, 640)
    trails_call = G.function_call(trails, 2860, Y)
    C(color_var, "", trails_call, "Attributes")
    C(soft_w, "", trails_call, "SoftGround")
    trails_switch = switch("LS_TankTrails", trails_call, "Attributes", color_var, "", 3260, Y)

    G.comment("5  Ground detail (LS_GroundDetail): ridge-and-furrow soil in the farm fields (Field<i>_* from "
              "ls_detail_build.write_fields), then mud in tank ruts on damp grass. Both use the breakup's dirt "
              "colour.", 3560, Y - 200, 1160, 640)
    furrows_call = G.function_call(furrows, 3660, Y)
    C(trails_switch, "", furrows_call, "Attributes")
    C(breakup_call, "DirtColor", furrows_call, "SoilColor")
    wet_call = G.function_call(wet_ruts, 4060, Y)
    C(furrows_call, "Attributes", wet_call, "Attributes")
    C(trails_call, "RutCentre", wet_call, "RutCentre")
    C(breakup_call, "Damp", wet_call, "Damp")
    C(grass_w, "", wet_call, "GrassWeight")
    C(breakup_call, "DirtColor", wet_call, "SoilColor")
    C(furrows_call, "DetailNoise", wet_call, "DetailNoise")
    detail_switch = switch("LS_GroundDetail", wet_call, "Attributes", trails_switch, "", 4460, Y)

    G.comment("Material outputs", 4760, Y - 200, 400, 640, G.OUTPUT_COLOR)
    move(out_break, 4860, Y)
    C(detail_switch, "", out_break, "Attr")

    G.comment("6  Landscape grass (LS_GroundGrass): top-down clump cards where the ground looks green, dry tufts on "
              "sand, none in fields, under water or on radixite blight. Off: every weight is 0. After a change, Build > Build Grass Maps "
              "Only and save the map.", 3560, Y + 860, 1600, 800)
    weights_call = G.function_call(grass_weights, 3660, Y + 1060)
    C(breakup_call, "Grassiness", weights_call, "Grassiness")
    C(sand_w, "", weights_call, "SandWeight")
    C(furrows_call, "FieldMask", weights_call, "FieldMask")
    blight_mask = switch("LS_RadixiteBlight", blight_call, "Blight", G.const(0.0, 3500, Y + 1440), "", 3660, Y + 1400)
    C(blight_mask, "", weights_call, "BlightMask")
    grass_output = N("LandscapeGrassOutput", 4600, Y + 1060)
    ls_grass_build.set_grass_inputs(grass_output, ls_grass_build.create_grass_types())
    for index, (pin, grass_type) in enumerate([("Green", "LGT_Trains_Green"), ("Flowers", "LGT_Trains_Flowers"),
                                               ("Dry", "LGT_Trains_Dry")]):
        y = Y + 1000 + index * 160
        gated = switch("LS_GroundGrass", weights_call, pin, G.const(0.0, 4060, y + 80), "", 4220, y)
        C(gated, "", grass_output, grass_type)


def wire_desert_paintable(blight):
    """DesertPaintable: the blight between its layer blend and colour variation (DirtColor keeps its default)."""
    material = unreal.load_asset(DESERT_PAINTABLE_PATH)
    color_var = find(material, "MaterialFunctionCall_3")
    G.remove_tagged(material, BLIGHT_DESC)
    layer_blend = find(material, "LandscapeLayerBlend_0")
    x = color_var.get_editor_property("material_expression_editor_x")
    y = color_var.get_editor_property("material_expression_editor_y") - 700
    G.begin(material, BLIGHT_DESC)
    G.comment("Radixite blight (LS_RadixiteBlight): dead, ash-grey ground around radixite crystals "
              "(MPC_RadixiteBlight, filled by URadixiteBlightSubsystem). Before the colour variation, so the RVT "
              "holds it.", x - 960, y - 200, 900, 560)
    call = G.function_call(blight, x - 860, y)
    C(layer_blend, "", call, "Attributes")
    blight_switch = N("StaticSwitchParameter", x - 400, y, parameter_name="LS_RadixiteBlight", default_value=False,
                      group=SWITCHES["LS_RadixiteBlight"])
    C(call, "Attributes", blight_switch, "True")
    C(layer_blend, "", blight_switch, "False")
    C(blight_switch, "", color_var, "InputMaterial")
    G.mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)


def enable_switches():
    """Trails on every DryCastle map (MI_Inst); Ger_Trains gets everything (MI_Trains, a child of MI_Inst)."""
    ls_trails_build.enable_in_instance(MI_INST_PATH)
    trains = unreal.load_asset(MI_TRAINS_PATH)
    for name in SWITCHES:
        G.mel.set_material_instance_static_switch_parameter_value(trains, name, True)
    G.mel.update_material_instance(trains)
    for path in [MI_INST_PATH, MI_TRAINS_PATH]:
        unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(path), False)


def run(enable=True):
    G.failures.clear()
    functions = [ls_breakup_build.build_function(), ls_trails_build.build_function(),
                 ls_detail_build.build_furrows_function(), ls_detail_build.build_wet_ruts_function(),
                 ls_grass_build.build_function(), ls_blight_build.build_function()]
    material = unreal.load_asset(MAT_PATH)
    removed = sum(G.remove_tagged(material, desc) for desc in OLD_DESCS + [DESC])
    G.begin(material, DESC)
    build(material, functions)
    arrange_original(material)
    frame_original(material)
    place_output(material)
    G.mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    wire_desert_paintable(functions[-1])
    if enable:
        enable_switches()
    print("removed", removed, "failures", G.failures)
    return G.failures
