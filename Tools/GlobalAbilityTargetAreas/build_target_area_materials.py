"""Builds the procedural target-area shaders of the global abilities (Docs/Abilities/GlobalAbilityTargetAreas.md).
Idempotent: every material is emptied and rebuilt. Run through the UECP bridge:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityTargetAreas/build_target_area_materials.py").read())
The aim actor and ARadixiteNuke pass all sizes in cm: PlaneLength, PlaneWidth, StripLength, AreaRadius, InnerRadius,
MarkerCount and IsDrawing. p below is the pixel in cm from the plane centre: +x runs along the strip, towards its end."""
import unreal

FOLDER = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas"
SCALARS = ["PlaneLength", "PlaneWidth", "StripLength", "AreaRadius", "InnerRadius", "MarkerCount", "IsDrawing"]

# Custom nodes have no shared includes; every shader starts with these helpers.
COMMON = """
struct FArea
{
    float Stroke(float Distance, float HalfWidth, float Softness) { return 1.0 - smoothstep(HalfWidth - Softness, HalfWidth + Softness, abs(Distance)); }
    float Inside(float Distance, float Softness) { return 1.0 - smoothstep(-Softness, Softness, Distance); }
    float Stadium(float2 Position, float HalfLength, float Radius) { float2 Offset = Position; Offset.x -= clamp(Position.x, -HalfLength, HalfLength); return length(Offset) - Radius; }
    float Dashes(float Position, float Period, float Duty) { return step(frac(Position / Period), Duty); }
    float Chevron(float2 Position, float Size) { float2 Folded = float2(Position.x - Size * 0.5 + abs(Position.y), Position.y); return max(abs(Folded.x) - Size * 0.16, abs(Position.y) - Size * 0.5); }
    float Eye(float2 Position, float HalfWidth) { float Curve = HalfWidth * 1.25; float Upper = length(Position - float2(0.0, -Curve * 0.72)) - Curve; float Lower = length(Position - float2(0.0, Curve * 0.72)) - Curve; return max(Upper, Lower); }
    float4 Over(float4 Below, float3 Color, float Alpha) { return float4(Below.rgb * (1.0 - Alpha) + Color * Alpha, Below.a + Alpha * (1.0 - Below.a)); }
};
FArea Area;
float2 p = (UV - 0.5) * float2(PlaneLength, PlaneWidth);
float HalfLength = StripLength * 0.5;
float4 Result = float4(0.0, 0.0, 0.0, 0.0);
"""

RECON = COMMON + """
float3 Pale = float3(0.55, 0.9, 1.0);
float3 White = float3(1.0, 1.0, 1.0);
float3 Dark = float3(0.0, 0.03, 0.06);
float Reach = Area.Stadium(p, HalfLength, AreaRadius);

// Faint fill with lines running across the flight, like a survey photo strip.
float Lines = Area.Stroke(frac((p.x + HalfLength) / 400.0) - 0.5, 0.03, 0.02);
Result = Area.Over(Result, Pale, Area.Inside(Reach, 40.0) * (0.10 + 0.08 * Lines));

// Outline of what will be revealed: dark halo, then dashes running towards the end of the flight.
Result = Area.Over(Result, Dark, Area.Stroke(Reach, 60.0, 30.0) * 0.55);
float Perimeter = atan2(p.y, p.x - clamp(p.x, -HalfLength, HalfLength)) * AreaRadius + p.x;
float Dash = Area.Dashes(Perimeter - Time * 260.0, 420.0, 0.62);
Result = Area.Over(Result, Pale, Area.Stroke(Reach, 26.0, 10.0) * Dash);

// Flight line and arrow heads once the strip is drawn.
float Heading = saturate(StripLength / 300.0);
float Inner = saturate((HalfLength - abs(p.x)) / 100.0);
Result = Area.Over(Result, Pale, Area.Stroke(p.y, 12.0, 6.0) * Inner * Heading * 0.7);
float ChevronCell = frac((p.x - Time * 500.0) / 900.0) * 900.0 - 450.0;
Result = Area.Over(Result, White, Area.Inside(Area.Chevron(float2(ChevronCell, p.y), 340.0), 10.0) * Inner * Heading);

// The eye sweeps the strip as the aircraft will; before the first click it waits at the cursor.
float EyeTravel = frac(Time * 0.22);
float EyeX = lerp(-HalfLength, HalfLength, EyeTravel) * IsDrawing;
// Across the flight: a strip leaves the cursor up the screen, and the eye then reads as an eye.
float2 EyePosition = (p - float2(EyeX, 0.0)).yx;
float EyeSize = min(AreaRadius * 0.46, 1000.0);
float EyeShape = Area.Eye(EyePosition, EyeSize);
Result = Area.Over(Result, Dark, Area.Inside(EyeShape - 34.0, 16.0) * 0.72);
Result = Area.Over(Result, White, Area.Stroke(EyeShape, 24.0, 8.0));
float Iris = length(EyePosition) - EyeSize * 0.40;
Result = Area.Over(Result, Pale, Area.Stroke(Iris, 24.0, 8.0));
Result = Area.Over(Result, White, Area.Inside(length(EyePosition) - EyeSize * 0.17, 8.0));

// The ring around the eye is the radius the aircraft sees at any moment.
float EyeRing = length(EyePosition) - AreaRadius;
Result = Area.Over(Result, White, Area.Stroke(EyeRing, 14.0, 6.0) * IsDrawing * 0.8);
return Result;
"""

FLARE = COMMON + """
float3 Warm = float3(1.0, 0.72, 0.28);
float3 Hot = float3(1.0, 0.95, 0.72);
float3 Dark = float3(0.07, 0.03, 0.0);
float Count = max(MarkerCount, 1.0);
float Spacing = StripLength / max(Count - 1.0, 1.0);
float Index = clamp(floor((p.x + HalfLength) / max(Spacing, 1.0) + 0.5), 0.0, Count - 1.0);
float2 FromFlare = p - float2(-HalfLength + Index * Spacing, 0.0);
float Reach = length(FromFlare) - AreaRadius;

// Everything a flare lights, as overlapping pools of light.
float Pool = saturate(1.0 - length(FromFlare) / AreaRadius);
Result = Area.Over(Result, Warm, Area.Inside(Reach, 40.0) * (0.08 + 0.22 * Pool * Pool));
Result = Area.Over(Result, Dark, Area.Stroke(Reach, 56.0, 28.0) * 0.5);
float Around = atan2(FromFlare.y, FromFlare.x) * AreaRadius;
Result = Area.Over(Result, Warm, Area.Stroke(Reach, 24.0, 10.0) * Area.Dashes(Around + Time * 200.0, 380.0, 0.6));

// A flare marker at every release point, staggered in time as the bomber will drop them.
float Pulse = 0.5 + 0.5 * sin(Time * 3.0 - Index * 0.9);
float Radial = length(FromFlare);
float Angle = atan2(FromFlare.y, FromFlare.x);
float Rays = smoothstep(0.55, 0.95, abs(cos(Angle * 4.0))) * Area.Inside(Radial - (300.0 + 90.0 * Pulse), 24.0);
Result = Area.Over(Result, Dark, Area.Inside(Radial - 200.0, 30.0) * 0.55);
Result = Area.Over(Result, Hot, Rays * 0.9);
Result = Area.Over(Result, Hot, Area.Inside(Radial - 130.0, 12.0));
Result = Area.Over(Result, Warm, Area.Stroke(Radial - 200.0, 16.0, 6.0));

// Bomber line through the markers.
float Inner = saturate((HalfLength - abs(p.x)) / 100.0);
float Ahead = Area.Dashes(p.x - Time * 500.0, 520.0, 0.5);
Result = Area.Over(Result, Warm, Area.Stroke(p.y, 12.0, 6.0) * Inner * Ahead * 0.8);
return Result;
"""

MINEFIELD = COMMON + """
float3 Red = float3(1.0, 0.16, 0.08);
float3 Amber = float3(1.0, 0.62, 0.12);
float3 Dark = float3(0.05, 0.0, 0.0);
float2 Box = abs(p) - float2(HalfLength, AreaRadius);
float Reach = length(max(Box, 0.0)) + min(max(Box.x, Box.y), 0.0) - 60.0;

// Warning hatch over the denied ground.
float Hatch = Area.Dashes(p.x + p.y, 620.0, 0.5);
Result = Area.Over(Result, Red, Area.Inside(Reach, 30.0) * (0.09 + 0.07 * Hatch));
Result = Area.Over(Result, Dark, Area.Stroke(Reach, 62.0, 30.0) * 0.55);
float Around = abs(p.x) > HalfLength ? p.y : p.x;
Result = Area.Over(Result, Amber, Area.Stroke(Reach, 28.0, 10.0) * Area.Dashes(Around + Time * 160.0, 520.0, 0.68));

// The flight line of the minelayer.
float Inner = saturate((HalfLength - abs(p.x)) / 100.0);
float ChevronCell = frac((p.x - Time * 420.0) / 1100.0) * 1100.0 - 550.0;
Result = Area.Over(Result, Amber, Area.Inside(Area.Chevron(float2(ChevronCell, p.y), 300.0), 10.0) * Inner * saturate(StripLength / 300.0) * 0.8);
return Result;
"""

NUKE = COMMON + """
float3 Radixite = float3(0.22, 1.0, 0.34);
float3 Core = float3(0.86, 1.0, 0.24);
float3 White = float3(0.92, 1.0, 0.9);
float3 Dark = float3(0.0, 0.05, 0.015);
float Radial = length(p);
float Angle = atan2(p.y, p.x);
float Reach = Radial - AreaRadius;
float CoreReach = Radial - InnerRadius;
float Urgency = lerp(1.0, 2.6, IsDrawing);

// Shock rings run inwards to where it will hit.
float Rings = frac(Radial / 900.0 + Time * 0.45 * Urgency);
Result = Area.Over(Result, Radixite, Area.Inside(Reach, 50.0) * (0.10 + 0.14 * Rings * Rings));

// The blast edge with its range ticks.
Result = Area.Over(Result, Dark, Area.Stroke(Reach, 80.0, 40.0) * 0.6);
Result = Area.Over(Result, Radixite, Area.Stroke(Reach, 32.0, 12.0));
float Ticks = Area.Dashes(Angle * AreaRadius + Time * 90.0, 640.0, 0.3);
Result = Area.Over(Result, White, Area.Stroke(Reach + 110.0, 60.0, 8.0) * Ticks);

// The core nothing survives: filled and hatched.
float Hatch = Area.Dashes(p.x - p.y + Time * 300.0, 520.0, 0.5);
Result = Area.Over(Result, Core, Area.Inside(CoreReach, 30.0) * (0.14 + 0.16 * Hatch));
Result = Area.Over(Result, Dark, Area.Stroke(CoreReach, 54.0, 24.0) * 0.55);
Result = Area.Over(Result, Core, Area.Stroke(CoreReach, 24.0, 10.0) * Area.Dashes(Angle * InnerRadius - Time * 200.0, 420.0, 0.6));

// Radiation sign, turning slowly.
float SignRadius = min(InnerRadius * 0.62, 1500.0);
float Turned = Angle + Time * 0.35 * Urgency;
float Blades = step(0.5, frac(Turned * 3.0 / 6.2831853 + 0.25));
float Blade = Blades * step(SignRadius * 0.26, Radial) * Area.Inside(Radial - SignRadius, 14.0);
float SignDisc = Area.Inside(Radial - SignRadius * 1.1, 14.0);
Result = Area.Over(Result, Dark, SignDisc * 0.62);
Result = Area.Over(Result, Core, Blade);
Result = Area.Over(Result, Core, Area.Inside(Radial - SignRadius * 0.17, 10.0));
Result = Area.Over(Result, Radixite, Area.Stroke(Radial - SignRadius * 1.1, 22.0, 8.0));
return Result;
"""

SHOCKWAVE = """
float2 p = (UV - 0.5) * PlaneLength;
float Radial = length(p);
float Front = Progress * AreaRadius;
float Width = lerp(160.0, 520.0, Progress);
float Behind = saturate((Front - Radial) / Width);
float Ahead = 1.0 - smoothstep(0.0, 60.0, Radial - Front);
float Ring = Ahead * (1.0 - Behind) * (1.0 - Progress * Progress);
float3 Color = lerp(float3(0.3, 1.0, 0.42), float3(0.95, 1.0, 0.9), Ahead * (1.0 - Behind) * (1.0 - Progress));
return float4(Color * Ring * 6.0, Ring);
"""

SCAR = """
// Ragged circle of burnt ground with cracks that glow and die down over the radiation.
float2 p = UV * 2.0 - 1.0;
float Radial = length(p);
float Best = 10.0;
float Second = 10.0;
float2 Grid = p * 15.0 + 0.7 * float2(sin(p.y * 13.0), cos(p.x * 11.0));
float2 Cell = floor(Grid);
for (int y = -1; y <= 1; y++)
{
    for (int x = -1; x <= 1; x++)
    {
        float2 Neighbour = Cell + float2(x, y);
        float2 Jitter = frac(sin(float2(dot(Neighbour, float2(127.1, 311.7)), dot(Neighbour, float2(269.5, 183.3)))) * 43758.5453);
        float Distance = length(Neighbour + Jitter - Grid);
        if (Distance < Best) { Second = Best; Best = Distance; }
        else if (Distance < Second) { Second = Distance; }
    }
}
float Crack = 1.0 - smoothstep(0.0, 0.035, Second - Best);
float Patchy = smoothstep(-0.2, 0.5, sin(p.x * 9.0 + 1.7) * sin(p.y * 7.0 - 0.6) + 0.3 * sin((p.x + p.y) * 21.0));
float Rough = 0.08 * sin(atan2(p.y, p.x) * 11.0) + 0.05 * sin(atan2(p.y, p.x) * 23.0 + 1.3);
float Mask = 1.0 - smoothstep(0.68 + Rough, 0.86 + Rough, Radial);
float CrackReach = (1.0 - smoothstep(0.05, 0.72, Radial)) * Patchy;
float Glowing = Crack * CrackReach * Mask;
float Burnt = lerp(0.03, 0.11, saturate(Radial * 1.2)) * (0.8 + 0.4 * Best);
return float4(Burnt, Glowing * Glow, 0.0, Mask);
"""

GHOST = """
float Pulse = 0.72 + 0.28 * sin(Time * 4.0 + WorldPosition.x * 0.004);
return float4(float3(1.0, 0.42, 0.1) * 1.1 * Pulse, 0.85);
"""


def empty(material):
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)


def get_material(name, factory_new=True):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def add_custom(material, code, scalars, output_x=-300, extra_inputs=()):
    lib = unreal.MaterialEditingLibrary
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, output_x, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "TargetArea")
    inputs = []
    for name in ["UV", "Time"] + list(scalars) + list(extra_inputs):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, output_x - 400, -200)
    lib.connect_material_expressions(uv, "", custom, "UV")
    time = lib.create_material_expression(material, unreal.MaterialExpressionTime, output_x - 400, -120)
    lib.connect_material_expressions(time, "", custom, "Time")
    for index, name in enumerate(scalars):
        parameter = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter,
                                                   output_x - 400, -40 + index * 80)
        parameter.set_editor_property("parameter_name", name)
        lib.connect_material_expressions(parameter, "", custom, name)
    return custom


def mask(material, source, r, g, b, a, y):
    lib = unreal.MaterialEditingLibrary
    node = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -80, y)
    node.set_editor_property("r", r)
    node.set_editor_property("g", g)
    node.set_editor_property("b", b)
    node.set_editor_property("a", a)
    lib.connect_material_expressions(source, "", node, "")
    return node


def build_overlay(name, code, scalars, blend=unreal.BlendMode.BLEND_ALPHA_COMPOSITE):
    lib = unreal.MaterialEditingLibrary
    material = get_material(name)
    empty(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", blend)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("disable_depth_test", True)
    custom = add_custom(material, code, scalars)
    lib.connect_material_property(mask(material, custom, True, True, True, False, -100), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(mask(material, custom, False, False, False, True, 100), "",
                                  unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    print("built", name, unreal.EditorAssetLibrary.save_loaded_asset(material, False))
    return material


def build_scar():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_RadixiteNukeScar")
    empty(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    custom = add_custom(material, SCAR, ["Glow"])

    burnt = mask(material, custom, True, False, False, False, -220)
    glowing = mask(material, custom, False, True, False, False, -60)
    opacity = mask(material, custom, False, False, False, True, 100)
    color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -80, 240)
    color.set_editor_property("constant", unreal.LinearColor(0.2, 1.0, 0.32, 1.0))
    multiply = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 120, 120)
    lib.connect_material_expressions(glowing, "", multiply, "A")
    lib.connect_material_expressions(color, "", multiply, "B")
    strength = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 280, 120)
    strength.set_editor_property("const_b", 6.0)
    lib.connect_material_expressions(multiply, "", strength, "A")

    lib.connect_material_property(burnt, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(strength, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    print("built M_RadixiteNukeScar", unreal.EditorAssetLibrary.save_loaded_asset(material, False))


def build_ghost():
    lib = unreal.MaterialEditingLibrary
    material = get_material("M_DropPointGhost")
    empty(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("disable_depth_test", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    custom = add_custom(material, GHOST, [], extra_inputs=["WorldPosition"])
    position = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -700, 120)
    lib.connect_material_expressions(position, "", custom, "WorldPosition")
    lib.connect_material_property(mask(material, custom, True, True, True, False, -100), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    print("built M_DropPointGhost", unreal.EditorAssetLibrary.save_loaded_asset(material, False))


unreal.EditorAssetLibrary.make_directory(FOLDER)
build_overlay("M_TargetArea_Recon", RECON, SCALARS)
build_overlay("M_TargetArea_FlareCurtain", FLARE, SCALARS)
build_overlay("M_TargetArea_Minefield", MINEFIELD, SCALARS)
build_overlay("M_TargetArea_RadixiteNuke", NUKE, SCALARS)
build_overlay("M_RadixiteNukeShockwave", SHOCKWAVE, ["PlaneLength", "AreaRadius", "Progress"])
build_scar()
build_ghost()
