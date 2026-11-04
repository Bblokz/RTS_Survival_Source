"""Builds the procedural ground shaders of the command, ammo resupply, heal and range auras (Docs/UI/AuraRadii.md).
Run through the UECP bridge:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/AuraRadii/build_aura_radius_materials.py").read())
A new material is built from scratch; an existing one only gets its Custom node code replaced, because pooled radius
actors may hold it and emptying a live material can crash the editor.
The radius pool scales the 100 cm engine plane to the aura diameter and sets RadiusCm, so p below is the pixel in cm
from the aura centre. The host vehicle covers the centre, so every motif sits on the ring."""
import unreal

FOLDER = "/Game/RTS_Survival/Materials/RadiusMaterials"
SCALARS = {"RadiusCm": 1500.0, "Brightness": 1.6}

# Custom nodes have no shared includes; every shader starts with these helpers.
COMMON = """
struct FAura
{
    float Stroke(float Distance, float HalfWidth, float Softness) { return 1.0 - smoothstep(HalfWidth - Softness, HalfWidth + Softness, abs(Distance)); }
    float Inside(float Distance, float Softness) { return 1.0 - smoothstep(-Softness, Softness, Distance); }
    float4 Over(float4 Below, float3 Color, float Alpha) { return float4(Below.rgb * (1.0 - Alpha) + Color * Alpha, Below.a + Alpha * (1.0 - Below.a)); }
    float Box(float2 Position, float2 HalfSize) { float2 Offset = abs(Position) - HalfSize; return length(max(Offset, 0.0)) + min(max(Offset.x, Offset.y), 0.0); }
    // Rifle cartridge pointing along +x, Size is its full length.
    float Cartridge(float2 Position, float Size)
    {
        float HalfWidth = Size * 0.13;
        float Body = Box(Position - float2(-Size * 0.14, 0.0), float2(Size * 0.36, HalfWidth));
        float Rim = Box(Position - float2(-Size * 0.48, 0.0), float2(Size * 0.03, HalfWidth * 1.15));
        float2 NosePosition = (Position - float2(Size * 0.22, 0.0)) / float2(Size * 0.28, HalfWidth * 0.78);
        float Nose = max((length(NosePosition) - 1.0) * HalfWidth * 0.78, Size * 0.22 - Position.x);
        return min(min(Body, Rim), Nose);
    }
    float NoseMask(float2 Position, float Size) { return step(Size * 0.22, Position.x); }
};
FAura Aura;
float Radius = max(RadiusCm, 100.0);
float2 p = (UV - 0.5) * 2.0 * Radius;
float Radial = length(p);
float Angle = atan2(p.y, p.x);
float Edge = Radial - Radius;
// One screen pixel in cm keeps every stroke crisp from any zoom.
float Pixel = max(fwidth(Radial), 1.0);
float4 Result = float4(0.0, 0.0, 0.0, 0.0);
"""

COMMAND = COMMON + """
float3 Blue = float3(0.22, 0.55, 1.0);
float3 Pale = float3(0.78, 0.92, 1.0);
float3 Gold = float3(1.0, 0.74, 0.22);
float3 Dark = float3(0.0, 0.02, 0.07);
float Motif = clamp(Radius * 0.06, 60.0, 140.0);

// The ground that is commanded: a blue wash, stronger towards the edge.
float Covered = Aura.Inside(Edge, Pixel);
Result = Aura.Over(Result, Blue, Covered * (0.06 + 0.10 * smoothstep(Radius * 0.35, Radius, Radial)));

// Orders go out: rings leave the commander and fade before the edge.
for (int Wave = 0; Wave < 3; ++Wave)
{
    float Phase = frac(Time * 0.32 + Wave / 3.0);
    float WaveRadius = Phase * (Radius - Motif * 2.2);
    float Fade = smoothstep(0.0, 0.12, Phase) * (1.0 - Phase) * (1.0 - Phase);
    Result = Aura.Over(Result, Pale, Aura.Stroke(Radial - WaveRadius, 10.0 + 14.0 * Phase, Pixel) * Fade * 0.55);
}

// Border: dark halo for contrast, bright blue line, thin gold line inside it.
Result = Aura.Over(Result, Dark, Aura.Stroke(Edge + 10.0, 42.0, 20.0) * 0.45);
Result = Aura.Over(Result, Blue, Aura.Stroke(Edge, 14.0, Pixel));
Result = Aura.Over(Result, Pale, Aura.Stroke(Edge, 5.0, Pixel) * 0.8);
Result = Aura.Over(Result, Gold, Aura.Stroke(Edge + Motif * 0.55, 4.0, Pixel) * 0.85);
return Result;
"""

AMMO = COMMON + """
float3 Brass = float3(1.0, 0.76, 0.26);
float3 Copper = float3(0.95, 0.42, 0.14);
float3 Belt = float3(0.42, 0.07, 0.02);
float3 Warm = float3(1.0, 0.5, 0.12);
float3 Dark = float3(0.06, 0.015, 0.0);
float Motif = clamp(Radius * 0.055, 55.0, 130.0);

// The ground that is resupplied: a warm wash, stronger towards the edge.
float Covered = Aura.Inside(Edge, Pixel);
Result = Aura.Over(Result, Warm, Covered * (0.05 + 0.09 * smoothstep(Radius * 0.35, Radius, Radial)));

// Border: an ammo belt runs round the edge, its rounds pointing out, feeding slowly.
float BeltInner = Radius - Motif * 1.55;
float BeltCentre = (Radius + BeltInner) * 0.5;
float BeltHalf = (Radius - BeltInner) * 0.5;
Result = Aura.Over(Result, Dark, Aura.Stroke(Radial - BeltCentre, BeltHalf + 26.0, 18.0) * 0.5);
Result = Aura.Over(Result, Belt, Aura.Stroke(Radial - BeltCentre, BeltHalf, Pixel) * 0.75);
Result = Aura.Over(Result, Brass, Aura.Stroke(Edge, 6.0, Pixel));
Result = Aura.Over(Result, Brass, Aura.Stroke(Radial - BeltInner, 6.0, Pixel));
float LinkSpacing = Motif * 1.25;
float Count = max(18.0, floor(6.2831853 * BeltCentre / LinkSpacing));
float Feeding = Angle / 6.2831853 + Time * 0.006;
float Cell = (frac(Feeding * Count) - 0.5) * 6.2831853 * BeltCentre / Count;
float2 Link = float2(Radial - BeltCentre, Cell);
float LinkSize = BeltHalf * 1.75;
float LinkShape = Aura.Cartridge(Link, LinkSize);
Result = Aura.Over(Result, Dark, Aura.Inside(LinkShape - 5.0, Pixel) * 0.8);
Result = Aura.Over(Result, lerp(Brass, Copper, Aura.NoseMask(Link, LinkSize)), Aura.Inside(LinkShape, Pixel));
return Result;
"""

HEAL = COMMON + """
float3 Green = float3(0.25, 1.0, 0.42);
float3 Pale = float3(0.84, 1.0, 0.88);
float3 Band = float3(0.02, 0.24, 0.08);
float3 Red = float3(0.92, 0.06, 0.05);
float3 Dark = float3(0.0, 0.04, 0.01);
float Motif = clamp(Radius * 0.055, 55.0, 130.0);

// A heartbeat: two quick beats, then rest.
float Beat = frac(Time / 1.6);
float Pulse = exp(-pow((Beat - 0.05) / 0.04, 2.0)) + 0.6 * exp(-pow((Beat - 0.21) / 0.05, 2.0));

// The ground that is healed: a green wash, stronger towards the edge, that beats.
float Covered = Aura.Inside(Edge, Pixel);
Result = Aura.Over(Result, Green, Covered * (0.05 + 0.09 * smoothstep(Radius * 0.35, Radius, Radial) + 0.05 * Pulse));

// Border: a green band of medic roundels, turning slowly, its lines glowing with each beat.
float BandInner = Radius - Motif * 1.55;
float BandCentre = (Radius + BandInner) * 0.5;
float BandHalf = (Radius - BandInner) * 0.5;
Result = Aura.Over(Result, Dark, Aura.Stroke(Radial - BandCentre, BandHalf + 26.0, 18.0) * 0.5);
Result = Aura.Over(Result, Band, Aura.Stroke(Radial - BandCentre, BandHalf, Pixel) * 0.8);
Result = Aura.Over(Result, Green, Aura.Stroke(Radial - BandInner + 20.0, 24.0, 24.0) * Pulse * 0.45);
float LineGlow = 0.75 + 0.25 * Pulse;
Result = Aura.Over(Result, Pale, Aura.Stroke(Edge, 6.0, Pixel) * LineGlow);
Result = Aura.Over(Result, Pale, Aura.Stroke(Radial - BandInner, 6.0, Pixel) * LineGlow);
float Count = max(14.0, floor(6.2831853 * BandCentre / (Motif * 2.4)));
float Turning = Angle / 6.2831853 + Time * 0.004;
float Cell = (frac(Turning * Count) - 0.5) * 6.2831853 * BandCentre / Count;
float2 Local = float2(Cell, Radial - BandCentre);
float Roundel = length(Local) - BandHalf * 0.8;
float Arm = BandHalf * 0.56;
float Bar = BandHalf * 0.19;
float Cross = min(Aura.Box(Local, float2(Arm, Bar)), Aura.Box(Local, float2(Bar, Arm)));
Result = Aura.Over(Result, Dark, Aura.Inside(Roundel - 5.0, Pixel) * 0.8);
Result = Aura.Over(Result, float3(1.0, 1.0, 1.0), Aura.Inside(Roundel, Pixel));
Result = Aura.Over(Result, Red, Aura.Inside(Cross, Pixel));
return Result;
"""

RANGE = COMMON + """
float3 Teal = float3(0.12, 0.95, 0.85);
float3 Pale = float3(0.82, 1.0, 0.97);
float3 Dark = float3(0.0, 0.04, 0.04);
float Motif = clamp(Radius * 0.045, 60.0, 160.0);
float Covered = Aura.Inside(Edge, Pixel);

// The ground in range: a teal wash, stronger towards the edge.
Result = Aura.Over(Result, Teal, Covered * (0.04 + 0.07 * smoothstep(Radius * 0.35, Radius, Radial)));

// Dashed range rings at a third and two thirds of the radius.
for (int RingIndex = 1; RingIndex <= 2; ++RingIndex)
{
    float RingRadius = Radius * RingIndex / 3.0;
    float DashCount = max(12.0, floor(6.2831853 * RingRadius / (Motif * 1.6)));
    float Dash = step(frac(Angle / 6.2831853 * DashCount), 0.55);
    Result = Aura.Over(Result, Pale, Aura.Stroke(Radial - RingRadius, 4.0, Pixel) * Dash * 0.35);
}

// The radar sweep: a beam turning once every eight seconds with a fading trail behind it.
float SweepAngle = frac(Time * 0.125) * 6.2831853;
float Behind = frac((SweepAngle - Angle) / 6.2831853);
float FromCentre = smoothstep(Radius * 0.08, Radius * 0.2, Radial);
Result = Aura.Over(Result, Teal, exp(-Behind * 12.0) * Covered * FromCentre * 0.3);
float BeamDistance = min(Behind, 1.0 - Behind) * 6.2831853 * Radial;
Result = Aura.Over(Result, Pale, Aura.Stroke(BeamDistance, 5.0, Pixel) * Covered * FromCentre * 0.8);

// Border: a rangefinder scale, minor ticks every 2 degrees and major ticks every 10.
float ScaleInner = Radius - Motif * 0.95;
Result = Aura.Over(Result, Dark, Aura.Stroke(Radial - (Radius + ScaleInner) * 0.5, Motif * 0.5 + 24.0, 18.0) * 0.5);
float MinorArc = (frac(Angle / 6.2831853 * 180.0) - 0.5) * 6.2831853 * Radial / 180.0;
float MajorArc = (frac(Angle / 6.2831853 * 36.0) - 0.5) * 6.2831853 * Radial / 36.0;
float Minor = Aura.Box(float2(MinorArc, Radial - (Radius - Motif * 0.22)), float2(3.0, Motif * 0.22));
float Major = Aura.Box(float2(MajorArc, Radial - (Radius - Motif * 0.45)), float2(5.0, Motif * 0.45));
Result = Aura.Over(Result, Pale, Aura.Inside(min(Minor, Major), Pixel) * 0.9);
Result = Aura.Over(Result, Teal, Aura.Stroke(Edge, 9.0, Pixel));
Result = Aura.Over(Result, Pale, Aura.Stroke(Edge, 3.5, Pixel));
Result = Aura.Over(Result, Teal, Aura.Stroke(Radial - ScaleInner, 3.0, Pixel) * 0.7);
return Result;
"""


def find_custom(material):
    for index in range(8):
        found = unreal.find_object(None, material.get_path_name() + ":MaterialExpressionCustom_" + str(index))
        if found:
            return found
    return None


def build_new(name, code):
    lib = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "AuraRadius")
    inputs = []
    for input_name in ["UV", "Time", "RadiusCm"]:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, -200)
    lib.connect_material_expressions(uv, "", custom, "UV")
    time = lib.create_material_expression(material, unreal.MaterialExpressionTime, -700, -120)
    lib.connect_material_expressions(time, "", custom, "Time")
    radius = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, -40)
    radius.set_editor_property("parameter_name", "RadiusCm")
    radius.set_editor_property("default_value", SCALARS["RadiusCm"])
    lib.connect_material_expressions(radius, "", custom, "RadiusCm")

    colour = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -80, -100)
    for channel, keep in zip("rgba", (True, True, True, False)):
        colour.set_editor_property(channel, keep)
    lib.connect_material_expressions(custom, "", colour, "")
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -80, 100)
    for channel, keep in zip("rgba", (False, False, False, True)):
        opacity.set_editor_property(channel, keep)
    lib.connect_material_expressions(custom, "", opacity, "")
    brightness = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -80, -220)
    brightness.set_editor_property("parameter_name", "Brightness")
    brightness.set_editor_property("default_value", SCALARS["Brightness"])
    emissive = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 120, -100)
    lib.connect_material_expressions(colour, "", emissive, "A")
    lib.connect_material_expressions(brightness, "", emissive, "B")

    lib.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    return material


def build(name, code):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        custom = find_custom(material)
        if custom is None:
            raise RuntimeError("No Custom node in " + path)
        custom.set_editor_property("code", code)
    else:
        material = build_new(name, code)
    unreal.MaterialEditingLibrary.recompile_material(material)
    print("built", name, unreal.EditorAssetLibrary.save_loaded_asset(material, False))
    return material


build("M_CommandAuraRadius", COMMAND)
build("M_AmmoResupplyAuraRadius", AMMO)
build("M_HealAuraRadius", HEAL)
build("M_RangeAuraRadius", RANGE)
