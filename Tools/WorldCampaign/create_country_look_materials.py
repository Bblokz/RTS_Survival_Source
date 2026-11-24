"""Creates (or rebuilds) the two world-map country materials used by UWorldCountryOccupationRegulator:

  M_CountryStaffMap     fill: hatched occupied ground, clean free ground, a liberation front that spreads from up
                        to eight cleared targets
  M_CountryBorderState  border: one colour when a country has one owner, two alternating colours while contested

Run headless (editor closed), from the project root:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=Source/Tools/WorldCampaign/create_country_look_materials.py -nullrhi -unattended
"""
import unreal

FOLDER = '/Game/RTS_Survival/WorldCampaign/CountriesShading'
MEL = unreal.MaterialEditingLibrary
FRONT_POINT_COUNT = 8

FILL_CODE = '''
float2 MapPosition = WorldPos.xy;
float Warp = (sin(MapPosition.x * NoiseFrequency + MapPosition.y * NoiseFrequency * 0.61)
    + sin(MapPosition.y * NoiseFrequency * 1.37 - MapPosition.x * NoiseFrequency * 0.83)
    + sin((MapPosition.x + MapPosition.y) * NoiseFrequency * 2.1)) * NoiseStrength;
float FreeMask = saturate(FullyFree);
float3 FrontPoints[8] = { P0, P1, P2, P3, P4, P5, P6, P7 };
for (int PointIndex = 0; PointIndex < 8; PointIndex++)
{
    float Radius = FrontPoints[PointIndex].z;
    if (Radius > 1.0)
    {
        float Distance = length(MapPosition - FrontPoints[PointIndex].xy) + Warp;
        FreeMask = max(FreeMask, 1.0 - smoothstep(Radius - FrontSoftness, Radius, Distance));
    }
}
float AngleRadians = radians(HatchAngle);
float HatchCoordinate = dot(MapPosition, float2(cos(AngleRadians), sin(AngleRadians))) / max(HatchSpacing, 1.0);
float FromLineCentre = abs(frac(HatchCoordinate) - 0.5) * 2.0;
float Smoothing = fwidth(HatchCoordinate) * 2.0;
float HatchMask = 1.0 - smoothstep(HatchWidth - Smoothing, HatchWidth + Smoothing, FromLineCentre);
float3 OccupiedGround = lerp(OccupiedColor, HatchColor, HatchMask);
float EdgeMask = saturate(1.0 - abs(FreeMask * 2.0 - 1.0));
EdgeMask = EdgeMask * EdgeMask;
float3 Ground = lerp(OccupiedGround, FreeColor, FreeMask);
Ground = lerp(Ground, FrontEdgeColor, EdgeMask * 0.85);
Ground = Ground * (1.0 + Hover * 0.45);
// Border band inside the country, from the distance-to-edge picture (0 = on the edge, 1 = 64 map units inside).
float EdgeDistance = EdgeDistance01 * 64.0;
float BandSmoothing = max(fwidth(EdgeDistance), 0.75);
float Band = (1.0 - smoothstep(BorderWidth - BandSmoothing, BorderWidth + BandSmoothing, EdgeDistance))
    * step(0.5, BorderWidth);
float Along = (MapPosition.x + MapPosition.y) / max(DashLength, 1.0) + Time * DashSpeed;
float Across = (MapPosition.x - MapPosition.y) / max(DashLength, 1.0);
float Checker = frac((floor(Along) + floor(Across)) * 0.5) * 2.0;
float3 BandColour = lerp(BorderColor, SecondColor, Checker * saturate(Dashed)) * BorderBrightness;
return lerp(Ground, BandColour, Band);
'''

EDGE_UV_CODE = '''
return (WorldPos.xy - BoundsMin.xy) / max(BoundsSize.xy, float2(1.0, 1.0));
'''

BORDER_CODE = '''
float2 MapPosition = WorldPos.xy;
float Along = (MapPosition.x + MapPosition.y) / max(DashLength, 1.0) + Time * DashSpeed;
float Across = (MapPosition.x - MapPosition.y) / max(DashLength, 1.0);
float Checker = frac((floor(Along) + floor(Across)) * 0.5) * 2.0;
float3 Colour = lerp(BorderColor, SecondColor, Checker * saturate(Dashed));
return Colour * Brightness;
'''


def make_material(name):
    path = '%s/%s' % (FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        MEL.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material,
                                                                         unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
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


def custom(material, code, inputs, output_type=None, connect_to_emissive=True):
    """inputs: list of (hlsl name, source node) or (hlsl name, source node, source output name)."""
    node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output_type or unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = [entry if len(entry) == 3 else (entry[0], entry[1], '') for entry in inputs]
    custom_inputs = []
    for hlsl_name, _source, _output in inputs:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', hlsl_name)
        custom_inputs.append(custom_input)
    node.set_editor_property('inputs', custom_inputs)
    for hlsl_name, source, output_name in inputs:
        if not MEL.connect_material_expressions(source, output_name, node, hlsl_name):
            unreal.log_error('RTS_COUNTRY_LOOK could not connect %s' % hlsl_name)
    if connect_to_emissive:
        MEL.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return node


def finish(material):
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log('RTS_COUNTRY_LOOK saved %s %s' % (material.get_path_name(), saved))


def build_fill():
    material = make_material('M_CountryStaffMap')
    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, -200)
    inputs = [('WorldPos', world)]
    y = 0
    for name, value in (('OccupiedColor', (0.060, 0.018, 0.014)), ('HatchColor', (0.42, 0.045, 0.030)),
                        ('FreeColor', (0.105, 0.215, 0.275)), ('FrontEdgeColor', (0.80, 0.74, 0.58))):
        inputs.append((name, vector(material, name, value, y)))
        y += 220
    for name, value in (('HatchSpacing', 30.0), ('HatchWidth', 0.2), ('HatchAngle', 45.0), ('FrontSoftness', 45.0),
                        ('NoiseFrequency', 0.011), ('NoiseStrength', 14.0), ('FullyFree', 0.0), ('Hover', 0.0)):
        inputs.append((name, scalar(material, name, value, y)))
        y += 110
    for index in range(FRONT_POINT_COUNT):
        inputs.append(('P%d' % index, vector(material, 'FrontPoint%d' % index, (0.0, 0.0, 0.0), y)))
        y += 220
    # Border band: a per-country distance-to-edge picture, set at runtime; white = far from any edge = no band.
    time = MEL.create_material_expression(material, unreal.MaterialExpressionTime, -900, -100)
    inputs.append(('Time', time))
    for name, value in (('BorderColor', (0.80, 0.10, 0.06)), ('SecondColor', (0.80, 0.74, 0.58))):
        inputs.append((name, vector(material, name, value, y)))
        y += 220
    for name, value in (('BorderWidth', 0.0), ('Dashed', 0.0), ('DashLength', 38.0), ('DashSpeed', 0.5),
                        ('BorderBrightness', 1.6)):
        inputs.append((name, scalar(material, name, value, y)))
        y += 110
    bounds_min = vector(material, 'EdgeBoundsMin', (0.0, 0.0, 0.0), y)
    bounds_size = vector(material, 'EdgeBoundsSize', (1.0, 1.0, 0.0), y + 220)
    edge_uv = custom(material, EDGE_UV_CODE, [('WorldPos', world), ('BoundsMin', bounds_min), ('BoundsSize', bounds_size)],
                     output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2, connect_to_emissive=False)
    edge_texture = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
    edge_texture.set_editor_property('parameter_name', 'EdgeDistance')
    edge_texture.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    if not MEL.connect_material_expressions(edge_uv, '', edge_texture, 'UVs'):
        unreal.log_error('RTS_COUNTRY_LOOK could not connect the edge distance UVs')
    inputs.append(('EdgeDistance01', edge_texture, 'R'))
    custom(material, FILL_CODE, inputs)
    finish(material)


def build_border():
    material = make_material('M_CountryBorderState')
    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, -200)
    time = MEL.create_material_expression(material, unreal.MaterialExpressionTime, -900, -100)
    inputs = [('WorldPos', world), ('Time', time)]
    y = 0
    for name, value in (('BorderColor', (0.80, 0.10, 0.06)), ('SecondColor', (0.80, 0.74, 0.58))):
        inputs.append((name, vector(material, name, value, y)))
        y += 220
    for name, value in (('Dashed', 0.0), ('DashLength', 45.0), ('DashSpeed', 0.6), ('Brightness', 1.0)):
        inputs.append((name, scalar(material, name, value, y)))
        y += 110
    custom(material, BORDER_CODE, inputs)
    finish(material)


build_fill()
build_border()
