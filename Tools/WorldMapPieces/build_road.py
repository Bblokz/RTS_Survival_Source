"""Builds the worn road of the world map and points the road renderer at it (headless, -nullrhi).

The roads were one flat colour on a translucent plane, which the map's exposure blew out to a glowing cream band.
M_WorldRoad_Worn is a lit, masked surface without emission:
  - asphalt texture projected top-down in world space (no stretch on spline pieces of different length);
  - across the road (texture V of the engine plane): two tank ruts, a dark earth verge and a ragged cut edge;
  - dust patches from noise;
  - the vector parameter 'Color' tints the result; the roads above the fog use it to dim (UWorldFogHintSettings).
Tune the look on the instance MI_WorldRoad_Worn; ROAD_LOOK below is what this script writes into it.
WORLDROAD_REBUILD=1 deletes and rebuilds the material (close the editor first). WORLDROAD_REVERT=1 puts the old
material and height back on the renderer.
Log: Saved/WorldMapPieces/build_road.txt
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

GENERAL = "/Game/RTS_Survival/WorldCampaign/MapObjects/General"
FOLDER = GENERAL + "/Materials"
MATERIAL_NAME = "M_WorldRoad_Worn"
INSTANCE_NAME = "MI_WorldRoad_Worn"
RENDERER = GENERAL + "/BP_WorldConnectionRenderer"
OLD_MATERIAL = FOLDER + "/M_WorldConnection"
OLD_Z_OFFSET = 5.0
OLD_WIDTH = 20.0
# The translucent country fill lies at ground + 6; the road must be above it or the fill tints it.
ROAD_Z_OFFSET = 8.0
ROAD_WIDTH = float(os.environ.get("WORLDROAD_WIDTH", "32"))
AIRFIELD = "/Game/RTS_Survival/Meshes/PresetRegion_PlayerStart/AirfieldGround/Textures/"

ROAD_LOOK = dict(
    scalars=dict(TileCm=30.0, SurfaceBrightness=2.1, RutStrength=0.38, RutCentre=0.34, RutHalfWidth=0.09,
                 DustAmount=0.4, VergeStart=0.74, RoadEdge=0.93, EdgeRagged=0.16,
                 Roughness=0.88),
    vectors=dict(SurfaceTint=(0.85, 0.80, 0.72), DustColor=(0.46, 0.36, 0.24), VergeColor=(0.045, 0.035, 0.028),
                 Color=(1.0, 1.0, 1.0)),
)

# Noise features per world unit; fixed in the material (a structural value of the noise node).
NOISE_SCALE = 0.15

library = unreal.MaterialEditingLibrary


def sampler_type_of(texture):
    if texture.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if texture.get_editor_property("srgb"):
        return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


class Graph:
    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, expression_class, **properties):
        self.row += 1
        node = library.create_material_expression(self.material, expression_class, -1800 + (self.row % 9) * 190,
                                                  (self.row // 9) * 220)
        for name, value in properties.items():
            node.set_editor_property(name, value)
        return node

    def link(self, source, target, target_input, source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            lib.log("LINK FAILED %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def scalar(self, name):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name,
                         default_value=ROAD_LOOK["scalars"][name])

    def vector(self, name):
        colour = ROAD_LOOK["vectors"][name]
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))

    def binary(self, expression_class, a, b, a_output="", b_output=""):
        """a and b are nodes or numbers; a number becomes the node's constant."""
        node = self.node(expression_class)
        for value, pin, constant, output in ((a, "A", "const_a", a_output), (b, "B", "const_b", b_output)):
            if isinstance(value, (int, float)):
                node.set_editor_property(constant, float(value))
            else:
                self.link(value, node, pin, output)
        return node

    def unary(self, expression_class, source, source_output=""):
        node = self.node(expression_class)
        self.link(source, node, "", source_output)
        return node

    def lerp(self, a, b, alpha, a_output="", b_output=""):
        node = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, node, "A", a_output)
        self.link(b, node, "B", b_output)
        self.link(alpha, node, "Alpha")
        return node


def build_material():
    path = FOLDER + "/" + MATERIAL_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if os.environ.get("WORLDROAD_REBUILD", "") != "1":
            return unreal.load_asset(path)
        lib.log("delete old material: %s" % unreal.EditorAssetLibrary.delete_asset(path))
    material = lib.asset_tools.create_asset(MATERIAL_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    graph = Graph(material)
    multiply, add, subtract, divide = (unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd,
                                       unreal.MaterialExpressionSubtract, unreal.MaterialExpressionDivide)
    saturate = unreal.MaterialExpressionSaturate

    # Top-down world projection of the road surface.
    world = graph.node(unreal.MaterialExpressionWorldPosition)
    world_xy = graph.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    graph.link(world, world_xy, "")
    surface_uv = graph.binary(divide, world_xy, graph.scalar("TileCm"))
    base_texture = unreal.load_asset(AIRFIELD + "T_Road_Asphalt_BC")
    normal_texture = unreal.load_asset(AIRFIELD + "T_Road_Asphalt_N")
    surface = graph.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Surface",
                         texture=base_texture, sampler_type=sampler_type_of(base_texture))
    graph.link(surface_uv, surface, "UVs")
    normal = graph.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="SurfaceNormal",
                        texture=normal_texture, sampler_type=sampler_type_of(normal_texture))
    graph.link(surface_uv, normal, "UVs")

    # Noise 0..1 in world space: dust patches, uneven ruts, ragged edge.
    # No Position input: the node then takes the world position itself, times its scale.
    noise = graph.node(unreal.MaterialExpressionNoise, scale=NOISE_SCALE, levels=3, output_min=0.0, output_max=1.0)

    # 0 on the centre line, 1 on both edges. The engine plane has V across the spline's forward axis.
    coordinate = graph.node(unreal.MaterialExpressionTextureCoordinate)
    across_v = graph.node(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    graph.link(coordinate, across_v, "")
    across = graph.unary(unreal.MaterialExpressionAbs, graph.binary(subtract, graph.binary(multiply, across_v, 2.0),
                                                                    1.0))
    ragged = graph.binary(multiply, graph.binary(subtract, noise, 0.5), graph.scalar("EdgeRagged"))
    edge = graph.binary(add, across, ragged)

    # Surface colour, dust, ruts, verge.
    colour = graph.binary(multiply, surface, graph.vector("SurfaceTint"), a_output="RGB")
    colour = graph.binary(multiply, colour, graph.scalar("SurfaceBrightness"))
    dust_mask = graph.binary(multiply, graph.unary(saturate, graph.binary(subtract, graph.binary(
        multiply, noise, 1.6), 0.5)), graph.scalar("DustAmount"))
    colour = graph.lerp(colour, graph.vector("DustColor"), dust_mask)
    rut_distance = graph.unary(unreal.MaterialExpressionAbs, graph.binary(subtract, across,
                                                                          graph.scalar("RutCentre")))
    rut_shape = graph.unary(saturate, graph.unary(unreal.MaterialExpressionOneMinus, graph.binary(
        divide, rut_distance, graph.scalar("RutHalfWidth"))))
    rut_uneven = graph.binary(add, graph.binary(multiply, noise, 0.8), 0.6)
    rut_mask = graph.unary(saturate, graph.binary(multiply, graph.binary(multiply, rut_shape, rut_uneven),
                                                  graph.scalar("RutStrength")))
    colour = graph.binary(multiply, colour, graph.unary(unreal.MaterialExpressionOneMinus, rut_mask))
    verge_mask = graph.unary(saturate, graph.binary(divide, graph.binary(subtract, edge, graph.scalar("VergeStart")),
                                                    0.12))
    colour = graph.lerp(colour, graph.vector("VergeColor"), verge_mask)
    colour = graph.binary(multiply, colour, graph.vector("Color"))
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity = graph.unary(saturate, graph.binary(add, graph.binary(multiply, graph.binary(
        subtract, graph.scalar("RoadEdge"), edge), 20.0), 0.5))
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(graph.scalar("Roughness"), "", unreal.MaterialProperty.MP_ROUGHNESS)
    specular = graph.node(unreal.MaterialExpressionConstant, r=0.25)
    library.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    # Without this flag a -game run draws the default grey material on spline meshes.
    lib.log("spline mesh usage: %s" % (library.set_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_SPLINE_MESH),))
    library.recompile_material(material)
    lib.save(material)
    lib.log("material %s: %d nodes" % (MATERIAL_NAME, library.get_num_material_expressions(material)))
    return material


def set_renderer(material, z_offset, width):
    blueprint = unreal.load_asset(RENDERER)
    defaults = unreal.get_default_object(blueprint.generated_class())
    lib.log("renderer before: material %s, z %.1f, width %.1f" % (
        defaults.get_editor_property("m_ribbon_material").get_path_name(),
        defaults.get_editor_property("m_ribbon_z_offset"), defaults.get_editor_property("m_ribbon_width")))
    blueprint.modify()
    defaults.set_editor_property("m_ribbon_material", material)
    defaults.set_editor_property("m_ribbon_z_offset", z_offset)
    defaults.set_editor_property("m_ribbon_width", width)
    lib.save(blueprint)
    lib.log("renderer now: material %s, z %.1f, width %.1f" % (material.get_path_name(), z_offset, width))


def main():
    if os.environ.get("WORLDROAD_REVERT", "") == "1":
        set_renderer(unreal.load_asset(OLD_MATERIAL), OLD_Z_OFFSET, OLD_WIDTH)
        return
    material = build_material()
    instance = lib.make_instance(FOLDER, INSTANCE_NAME, material, vectors=ROAD_LOOK["vectors"],
                                 scalars=ROAD_LOOK["scalars"])
    lib.save(instance)
    set_renderer(instance, ROAD_Z_OFFSET, ROAD_WIDTH)


lib.LINES.clear()
try:
    main()
except Exception:
    lib.log(traceback.format_exc())
lib.write_log("build_road.txt")
