"""Builds the Sonnengewehr materials (beam, impact glow, scorch decal, target area) in
/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/Sonnengewehr. Idempotent: every material is emptied and rebuilt.

Headless (with author_sonnengewehr_template.py, see run_sonnengewehr_headless.py):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<run_sonnengewehr_headless.py>
ASonnengewehrBeam sets the parameters:
  M_SonnengewehrBeam    Color (vector), Intensity, Strength, EdgePower; on the engine cylinder and sphere.
  M_SonnengewehrImpact  Color, Intensity, Strength; on a ground plane under the beam.
  M_SonnengewehrScorch  StartX/StartY/DirX/DirY (strip in world space), Head (burnt length), HalfWidth, HotLength, Glow.
  M_TargetArea_Sonnengewehr  the target-area scalars of APlayerAimAbility (PlaneLength ... IsDrawing).
"""
import os
import sys

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/Sonnengewehr"
LOG_PREFIX = "[Sonnengewehr] "
AREA_SCALARS = ["PlaneLength", "PlaneWidth", "StripLength", "AreaRadius", "InnerRadius", "MarkerCount", "IsDrawing"]

# The target-area shaders share their helper code; reuse it instead of copying it.
_TARGET_AREAS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "GlobalAbilityTargetAreas")
if _TARGET_AREAS not in sys.path:
    sys.path.append(_TARGET_AREAS)
import build_target_area_materials as target_areas  # noqa: E402

BEAM = """
// Brightest where the beam faces the camera, fading to its silhouette: a column of light, not a tube.
float Facing = saturate(abs(dot(normalize(Normal), normalize(CameraVector))));
float Body = pow(Facing, EdgePower);
// Heat shimmer running down the beam.
float Shimmer = 0.9 + 0.1 * sin(WorldPosition.z * 0.004 + Time * 38.0) * sin(WorldPosition.z * 0.0011 - Time * 15.0);
// The wide layers fade into the ground instead of ending in a hard ellipse; the impact glow covers the foot.
float GroundBlend = GroundFade > 0.0 ? smoothstep(GroundZ, GroundZ + GroundFade, WorldPosition.z) : 1.0;
return Color * Intensity * Strength * Body * Shimmer * GroundBlend;
"""

IMPACT = """
float2 p = UV * 2.0 - 1.0;
float Radial = length(p);
float Glow = pow(saturate(1.0 - Radial), 2.4);
float Core = exp(-Radial * Radial * 30.0);
float Flicker = 0.9 + 0.1 * sin(Time * 31.0) * sin(Time * 17.0 + 1.3);
float3 WhiteHot = float3(1.0, 0.93, 0.72);
return (Color * Glow * 0.7 + WhiteHot * Core * 2.0) * Intensity * Strength * Flicker;
"""

SCORCH = """
struct FNoise
{
    float Hash(float2 Cell) { return frac(sin(dot(Cell, float2(127.1, 311.7))) * 43758.5453); }
    float Value(float2 Position)
    {
        float2 Cell = floor(Position);
        float2 Fraction = frac(Position);
        float2 Blend = Fraction * Fraction * (3.0 - 2.0 * Fraction);
        float A = Hash(Cell);
        float B = Hash(Cell + float2(1.0, 0.0));
        float C = Hash(Cell + float2(0.0, 1.0));
        float D = Hash(Cell + float2(1.0, 1.0));
        return lerp(lerp(A, B, Blend.x), lerp(C, D, Blend.x), Blend.y);
    }
};
FNoise Noise;
float2 Position = WorldPosition.xy;
float2 Direction = float2(DirX, DirY);
float2 FromStart = Position - float2(StartX, StartY);
float Along = dot(FromStart, Direction);
float Across = dot(FromStart, float2(-Direction.y, Direction.x));

float Coarse = Noise.Value(Position / 260.0);
float Fine = Noise.Value(Position / 70.0 + 13.1);
float Grain = Coarse * 0.65 + Fine * 0.35;

// A ragged burnt path with round ends, as long as the beam has travelled.
float Edge = HalfWidth * (0.85 + 0.5 * (Coarse - 0.5));
float2 FromPath = float2(Along - clamp(Along, 0.0, max(Head, 0.0)), Across);
float Mask = 1.0 - smoothstep(Edge * 0.8, Edge, length(FromPath));

// Fused, glassy ground along the middle, where the beam was hottest.
float Fused = 1.0 - smoothstep(0.0, HalfWidth * 0.4, abs(Across) + (Fine - 0.5) * 80.0);
float Burnt = lerp(0.012, 0.055, Grain) * (1.0 - 0.6 * Fused);

// Glows right behind the beam and cools further back; embers stay in the fused middle until Glow runs out.
float Behind = max(Head - Along, 0.0);
float Hot = exp(-Behind / max(HotLength, 1.0)) * (0.55 + 0.45 * Fused);
float Ember = Fused * smoothstep(0.5, 0.85, Fine) * (0.6 + 0.4 * sin(Time * 2.0 + Coarse * 6.0));
float Emissive = (Hot * 7.0 + Ember * 1.5) * Glow * Mask;
return float4(Burnt, Emissive, 0.0, Mask * 0.95);
"""

TARGET_AREA = target_areas.COMMON + """
float3 Gold = float3(1.0, 0.78, 0.22);
float3 White = float3(1.0, 0.96, 0.82);
float3 Dark = float3(0.06, 0.03, 0.0);
float Reach = Area.Stadium(p, HalfLength, AreaRadius);
float Drawn = saturate(StripLength / 300.0);

// The path that will burn: a warm fill with heat hatching drifting along it.
float Hatch = Area.Dashes(p.x - p.y * 0.5 - Time * 300.0, 360.0, 0.35);
Result = Area.Over(Result, Gold, Area.Inside(Reach, 30.0) * (0.12 + 0.10 * Hatch));
Result = Area.Over(Result, Dark, Area.Stroke(Reach, 60.0, 30.0) * 0.55);
float Perimeter = abs(p.x) > HalfLength ? atan2(p.y, p.x - clamp(p.x, -HalfLength, HalfLength)) * AreaRadius : p.x;
Result = Area.Over(Result, Gold, Area.Stroke(Reach, 24.0, 10.0) * Area.Dashes(Perimeter + Time * 220.0, 420.0, 0.65));

// Arrows from where the beam comes down to where it ends.
float Inner = saturate((HalfLength - abs(p.x)) / 100.0);
float ChevronCell = frac((p.x - Time * 600.0) / 900.0) * 900.0 - 450.0;
float ChevronSize = min(AreaRadius * 0.75, 300.0);
Result = Area.Over(Result, Gold, Area.Inside(Area.Chevron(float2(ChevronCell, p.y), ChevronSize), 10.0) * Inner * Drawn * 0.7);

// A bright line sweeps the strip the way the beam will.
float SweepX = lerp(-HalfLength, HalfLength, frac(Time * 0.25));
float Sweep = Area.Stroke(p.x - SweepX, 36.0, 26.0) * Area.Inside(abs(p.y) - AreaRadius, 20.0) * Drawn;
Result = Area.Over(Result, White, Sweep * 0.85);

// A sun where the beam comes down.
float2 FromStart = p - float2(-HalfLength, 0.0);
float Radial = length(FromStart);
float Angle = atan2(FromStart.y, FromStart.x);
float SunRadius = min(AreaRadius * 0.7, 420.0);
float Rays = smoothstep(0.6, 0.95, abs(cos(Angle * 6.0 + Time * 0.8))) * Area.Inside(Radial - SunRadius * 1.7, 16.0)
    * step(SunRadius * 1.1, Radial);
Result = Area.Over(Result, Dark, Area.Inside(Radial - SunRadius * 1.25, 20.0) * 0.5);
Result = Area.Over(Result, White, Area.Inside(Radial - SunRadius * 0.55, 10.0));
Result = Area.Over(Result, Gold, Area.Stroke(Radial - SunRadius * 0.85, 14.0, 6.0));
Result = Area.Over(Result, Gold, Rays);
return Result;
"""


def log(text):
    unreal.log(LOG_PREFIX + text)


def get_material(name):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def custom_node(material, code, output_type, x=-300, y=0):
    lib = unreal.MaterialEditingLibrary
    node = lib.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", "Sonnengewehr")
    return node


def set_inputs(node, names):
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    node.set_editor_property("inputs", inputs)


def scalar(material, custom, name, default, x, y):
    lib = unreal.MaterialEditingLibrary
    parameter = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    parameter.set_editor_property("parameter_name", name)
    parameter.set_editor_property("default_value", default)
    lib.connect_material_expressions(parameter, "", custom, name)


def rgb_parameter(material, custom, name, default, x, y):
    """Vector parameter through an RGB mask, so the Custom node gets a float3."""
    lib = unreal.MaterialEditingLibrary
    parameter = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    parameter.set_editor_property("parameter_name", name)
    parameter.set_editor_property("default_value", unreal.LinearColor(*default))
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, x + 200, y)
    for channel, value in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(channel, value)
    lib.connect_material_expressions(parameter, "", rgb, "")
    lib.connect_material_expressions(rgb, "", custom, name)


def connect_node(material, expression_class, custom, name, x, y):
    lib = unreal.MaterialEditingLibrary
    node = lib.create_material_expression(material, expression_class, x, y)
    lib.connect_material_expressions(node, "", custom, name)
    return node


def finish(material, name):
    unreal.MaterialEditingLibrary.recompile_material(material)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    log("built %s saved=%s" % (name, saved))
    return saved


def build_beam():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_SonnengewehrBeam")
    lib.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)
    custom = custom_node(material, BEAM, unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_inputs(custom, ["Normal", "CameraVector", "WorldPosition", "Time", "Color", "Intensity", "Strength",
                        "EdgePower", "GroundZ", "GroundFade"])
    connect_node(material, unreal.MaterialExpressionVertexNormalWS, custom, "Normal", -900, -300)
    connect_node(material, unreal.MaterialExpressionCameraVectorWS, custom, "CameraVector", -900, -220)
    connect_node(material, unreal.MaterialExpressionWorldPosition, custom, "WorldPosition", -900, -140)
    connect_node(material, unreal.MaterialExpressionTime, custom, "Time", -900, -60)
    rgb_parameter(material, custom, "Color", (1.0, 0.85, 0.3, 1.0), -1000, 20)
    scalar(material, custom, "Intensity", 20.0, -700, 120)
    scalar(material, custom, "Strength", 1.0, -700, 200)
    scalar(material, custom, "EdgePower", 1.0, -700, 280)
    scalar(material, custom, "GroundZ", -100000.0, -700, 360)
    scalar(material, custom, "GroundFade", 0.0, -700, 440)
    lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(material, "M_SonnengewehrBeam")


def build_impact():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_SonnengewehrImpact")
    lib.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    # Light on the ground: it may not be cut by bumps of the landscape the flat plane goes through.
    material.set_editor_property("disable_depth_test", True)
    custom = custom_node(material, IMPACT, unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_inputs(custom, ["UV", "Time", "Color", "Intensity", "Strength"])
    connect_node(material, unreal.MaterialExpressionTextureCoordinate, custom, "UV", -900, -200)
    connect_node(material, unreal.MaterialExpressionTime, custom, "Time", -900, -120)
    rgb_parameter(material, custom, "Color", (1.0, 0.62, 0.1, 1.0), -1000, -20)
    scalar(material, custom, "Intensity", 10.0, -700, 100)
    scalar(material, custom, "Strength", 1.0, -700, 180)
    lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(material, "M_SonnengewehrImpact")


def build_scorch():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_SonnengewehrScorch")
    lib.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    custom = custom_node(material, SCORCH, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = ["StartX", "StartY", "DirX", "DirY", "Head", "HalfWidth", "HotLength", "Glow"]
    set_inputs(custom, ["WorldPosition", "Time"] + names)
    connect_node(material, unreal.MaterialExpressionWorldPosition, custom, "WorldPosition", -900, -300)
    connect_node(material, unreal.MaterialExpressionTime, custom, "Time", -900, -220)
    defaults = {"DirX": 1.0, "HalfWidth": 350.0, "HotLength": 900.0, "Glow": 1.0}
    for index, name in enumerate(names):
        scalar(material, custom, name, defaults.get(name, 0.0), -700, -140 + index * 70)

    burnt = target_areas.mask(material, custom, True, False, False, False, -220)
    glowing = target_areas.mask(material, custom, False, True, False, False, -60)
    opacity = target_areas.mask(material, custom, False, False, False, True, 100)
    color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -80, 240)
    color.set_editor_property("constant", unreal.LinearColor(1.0, 0.42, 0.07, 1.0))
    emissive = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 120, 120)
    lib.connect_material_expressions(glowing, "", emissive, "A")
    lib.connect_material_expressions(color, "", emissive, "B")
    # Charred earth is a warm brown-black, fully rough, without the sky's reflection.
    tint = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 120, -220)
    lib.connect_material_expressions(burnt, "", tint, "A")
    warm = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -80, -320)
    warm.set_editor_property("constant", unreal.LinearColor(1.0, 0.72, 0.5, 1.0))
    lib.connect_material_expressions(warm, "", tint, "B")
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, 120, -380)
    rough.set_editor_property("r", 1.0)
    no_specular = lib.create_material_expression(material, unreal.MaterialExpressionConstant, 120, -440)
    no_specular.set_editor_property("r", 0.0)
    lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(no_specular, "", unreal.MaterialProperty.MP_SPECULAR)
    lib.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    return finish(material, "M_SonnengewehrScorch")


def build_target_area():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_TargetArea_Sonnengewehr")
    lib.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("disable_depth_test", True)
    custom = target_areas.add_custom(material, TARGET_AREA, AREA_SCALARS)
    lib.connect_material_property(target_areas.mask(material, custom, True, True, True, False, -100), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(target_areas.mask(material, custom, False, False, False, True, 100), "",
                                  unreal.MaterialProperty.MP_OPACITY)
    return finish(material, "M_TargetArea_Sonnengewehr")


def run():
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    results = [build_beam(), build_impact(), build_scorch(), build_target_area()]
    return all(results)


if __name__ == "__main__":
    run()
