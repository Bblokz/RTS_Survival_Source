"""Creates (or rebuilds) the world-map build site marker material used by AWorldBuildNode:

  M_BuildSiteHexPlate   a riveted hex plate drawn on a flat plane: grey with a lock while locked, yellow rim with a
                        plus while open, gunmetal rim with the building's sign and a green lamp once built

The whole plate is drawn in one Custom node, so it needs no mesh of its own and no sign texture.

Run headless (editor closed), from the project root:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=Source/Tools/WorldCampaign/create_build_site_materials.py -nullrhi -unattended
"""
import unreal

FOLDER = '/Game/RTS_Survival/WorldCampaign/BuildSites'
NAME = 'M_BuildSiteHexPlate'
MEL = unreal.MaterialEditingLibrary

# UV space: the plane is twice as wide as the plate, so the open state's glow has room.
# State: 0 locked, 1 open, 2 built. Sign: 0 lock, 1 plus, 2 watch post, 3 supply depot, 4 airfield, 5 battery.
PLATE_CODE = '''
struct FPlateShapes
{
    float Hex(float2 Point)
    {
        Point = abs(Point);
        return max(Point.y, Point.x * 0.8660254 + Point.y * 0.5);
    }
    float Box(float2 Point, float2 HalfSize)
    {
        float2 Outside = abs(Point) - HalfSize;
        return length(max(Outside, 0.0)) + min(max(Outside.x, Outside.y), 0.0);
    }
    float Segment(float2 Point, float2 From, float2 To)
    {
        float2 Along = To - From;
        float2 Offset = Point - From;
        return length(Offset - Along * saturate(dot(Offset, Along) / dot(Along, Along)));
    }
    float Triangle(float2 Point, float Size)
    {
        const float Slope = 1.7320508;
        Point.x = abs(Point.x) - Size;
        Point.y = Point.y + Size / Slope;
        if (Point.x + Slope * Point.y > 0.0)
        {
            Point = float2(Point.x - Slope * Point.y, -Slope * Point.x - Point.y) / 2.0;
        }
        Point.x -= clamp(Point.x, -2.0 * Size, 0.0);
        return -length(Point) * sign(Point.y);
    }
    float Sign(float2 Point, float SignIndex)
    {
        if (SignIndex < 0.5)
        {
            // Lock: body and shackle.
            float Body = Box(Point - float2(0.0, 0.28), float2(0.50, 0.38)) - 0.06;
            float2 ShacklePoint = Point - float2(0.0, -0.22);
            float Shackle = ShacklePoint.y < 0.0
                ? abs(length(ShacklePoint) - 0.34)
                : max(abs(abs(ShacklePoint.x) - 0.34), ShacklePoint.y - 0.2);
            return min(Body, Shackle - 0.09);
        }
        if (SignIndex < 1.5)
        {
            return min(Box(Point, float2(0.78, 0.13)), Box(Point, float2(0.13, 0.78)));
        }
        if (SignIndex < 2.5)
        {
            // Watch post: open triangle with a dot.
            float2 Up = float2(Point.x, -Point.y - 0.1);
            return min(abs(Triangle(Up, 0.62)) - 0.08, length(Point - float2(0.0, 0.2)) - 0.13);
        }
        if (SignIndex < 3.5)
        {
            // Supply depot: crossed box.
            float Frame = abs(Box(Point, float2(0.62, 0.62))) - 0.08;
            float Cross = min(abs(Point.x - Point.y), abs(Point.x + Point.y)) * 0.7071 - 0.07;
            return min(Frame, max(Cross, Box(Point, float2(0.62, 0.62))));
        }
        if (SignIndex < 4.5)
        {
            // Airfield: three propeller blades.
            float Blades = min(Segment(Point, float2(0.0, 0.0), float2(0.0, -0.78)),
                min(Segment(Point, float2(0.0, 0.0), float2(0.675, 0.39)),
                    Segment(Point, float2(0.0, 0.0), float2(-0.675, 0.39)))) - 0.12;
            return min(Blades, length(Point) - 0.2);
        }
        // Battery: a shell standing on its base.
        float Case = Box(Point - float2(0.0, 0.3), float2(0.24, 0.42));
        float2 NosePoint = (Point - float2(0.0, -0.12)) / float2(0.24, 0.66);
        float Nose = Point.y < -0.12 ? (length(NosePoint) - 1.0) * 0.24 : 1.0;
        return min(Case, Nose);
    }
};
FPlateShapes Shapes;

float2 Point = (UV - 0.5) * 2.0;
float Smoothing = max(fwidth(Point.x) * 1.5, 0.004);
const float OuterRadius = 0.52;
const float RimWidth = 0.13;
float HexDistance = Shapes.Hex(Point);
float OutsidePlate = HexDistance - OuterRadius;
float PlateMask = 1.0 - smoothstep(-Smoothing, Smoothing, OutsidePlate);
float CentreMask = 1.0 - smoothstep(-Smoothing, Smoothing, OutsidePlate + RimWidth);

float IsOpen = step(0.5, State) * step(State, 1.5);
float IsBuilt = step(1.5, State);
float IsLocked = 1.0 - step(0.5, State);

// Rim: steel with a soft bevel, lighter towards the top-left like a lit plate.
float3 RimColour = RimLocked * IsLocked + RimOpen * IsOpen + RimBuilt * IsBuilt;
float AcrossRim = saturate(-OutsidePlate / RimWidth);
float Bevel = 0.72 + 0.5 * sin(AcrossRim * 3.14159);
float Light = 1.0 + dot(normalize(Point + 0.0001), float2(-0.5, -0.8)) * 0.22;
float3 Rim = RimColour * Bevel * Light;

// Six rivets on the corners of the rim.
float RivetDistance = 10.0;
for (int Corner = 0; Corner < 6; Corner++)
{
    float Angle = Corner * 1.0471976;
    float2 RivetCentre = float2(cos(Angle), sin(Angle)) * (OuterRadius - RimWidth * 0.5) / 0.8660254;
    RivetDistance = min(RivetDistance, length(Point - RivetCentre));
}
float RivetMask = 1.0 - smoothstep(0.032 - Smoothing, 0.032 + Smoothing, RivetDistance);
Rim = lerp(Rim, RimColour * 0.3, RivetMask);

// Centre: enemy hatching while locked, free ground otherwise; darker towards the rim so the plate reads as sunk.
float Hatch = abs(frac((Point.x + Point.y) * 4.5) - 0.5) * 2.0;
float HatchMask = 1.0 - smoothstep(0.3, 0.3 + Smoothing * 9.0, Hatch);
float3 LockedCentre = lerp(CentreEnemy, CentreEnemyHatch, HatchMask);
float3 Centre = lerp(lerp(CentreFree, CentreBuilt, IsBuilt), LockedCentre, IsLocked);
Centre *= lerp(0.6, 1.0, saturate((-OutsidePlate - RimWidth) / 0.12));

// Sign in the middle.
float SignDistance = Shapes.Sign(Point / 0.25, SignIndex) * 0.25;
float SignMask = 1.0 - smoothstep(-Smoothing, Smoothing, SignDistance);
float3 SignColour = lerp(SignColor, RimOpen, IsOpen);
Centre = lerp(Centre, SignColour * SignBrightness, SignMask);

float3 Colour = lerp(Rim, Centre, CentreMask);

// Status lamp on the top edge once built.
float2 LampPoint = Point - float2(0.0, -(OuterRadius - RimWidth * 0.5));
float LampDistance = length(LampPoint);
float LampRing = (1.0 - smoothstep(0.075 - Smoothing, 0.075 + Smoothing, LampDistance)) * IsBuilt;
float LampCore = (1.0 - smoothstep(0.048 - Smoothing, 0.048 + Smoothing, LampDistance)) * IsBuilt;
Colour = lerp(Colour, RimBuilt * 0.25, LampRing);
Colour = lerp(Colour, LampColor * 2.0, LampCore);

// Drop shadow down-right of the plate and, while open, a glow around it.
float ShadowDistance = Shapes.Hex(Point - float2(0.035, 0.05)) - OuterRadius;
float Shadow = (1.0 - smoothstep(-0.02, 0.06, ShadowDistance)) * 0.6;
float Glow = exp(-max(OutsidePlate, 0.0) * 7.0) * 0.5 * IsOpen * (1.0 - smoothstep(0.3, 0.46, OutsidePlate));
float Pulse = 0.8 + 0.2 * sin(Time * 2.4);
float3 Around = RimOpen * Glow * Pulse;
float AroundAlpha = max(Shadow, Glow * Pulse);
// Hovered: a bright line around the plate and a brighter plate.
float HoverLine = (1.0 - smoothstep(0.05 - Smoothing, 0.05 + Smoothing, abs(OutsidePlate - 0.06))) * saturate(Hover);
Around = lerp(Around, SignColor * 2.2, HoverLine);
AroundAlpha = max(AroundAlpha, HoverLine);
Colour = Colour * (1.0 + Hover * 0.8 + IsOpen * 0.25 * (Pulse - 0.8));

float3 OutColour = lerp(Around, Colour, PlateMask);
float OutAlpha = lerp(AroundAlpha, 1.0, PlateMask);
return float4(OutColour, OutAlpha);
'''


def make_material():
    path = '%s/%s' % (FOLDER, NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        MEL.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material,
                                                                         unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    return material


def scalar(material, name, value, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def vector(material, name, value, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def srgb(hex_colour):
    """'#RRGGBB' as the linear values a vector parameter wants."""
    def channel(text):
        value = int(text, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return channel(hex_colour[1:3]), channel(hex_colour[3:5]), channel(hex_colour[5:7])


def mask(material, source, channels, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -100, y)
    for channel in 'rgba':
        node.set_editor_property(channel, channel in channels)
    MEL.connect_material_expressions(source, '', node, '')
    return node


def build():
    material = make_material()
    inputs = [('UV', MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -300)),
              ('Time', MEL.create_material_expression(material, unreal.MaterialExpressionTime, -900, -200))]
    y = 0
    for name, value in (('State', 0.0), ('SignIndex', 0.0), ('Hover', 0.0), ('SignBrightness', 1.3)):
        inputs.append((name, scalar(material, name, value, y)))
        y += 110
    for name, colour in (('RimLocked', '#7C8288'), ('RimOpen', '#F2C230'), ('RimBuilt', '#B9C79A'),
                         ('CentreFree', '#24506C'), ('CentreBuilt', '#2A3324'), ('CentreEnemy', '#2B1412'),
                         ('CentreEnemyHatch', '#8A2A1E'), ('SignColor', '#E6DFC8'), ('LampColor', '#D8E6B0')):
        inputs.append((name, vector(material, name, srgb(colour), y)))
        y += 220

    node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    node.set_editor_property('code', PLATE_CODE)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom_inputs = []
    for hlsl_name, _source in inputs:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', hlsl_name)
        custom_inputs.append(custom_input)
    node.set_editor_property('inputs', custom_inputs)
    for hlsl_name, source in inputs:
        if not MEL.connect_material_expressions(source, '', node, hlsl_name):
            unreal.log_error('RTS_BUILD_SITE could not connect %s' % hlsl_name)

    MEL.connect_material_property(mask(material, node, 'rgb', -100), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(mask(material, node, 'a', 100), '', unreal.MaterialProperty.MP_OPACITY)
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log('RTS_BUILD_SITE saved %s %s' % (material.get_path_name(), saved))


build()
