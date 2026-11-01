"""Builds the ground decal material of the Soviet bases (pads, roads, junctions, gate aprons) and its surfaces.
Safe to run again: replaces what is there. Run headless under the debuggame lock:
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi

One material, M_SovietGroundDecal. Every surface is one of the landscape's own layer textures (MI_RTS_Landscape_DesertPaintable:
rippled sand, western gravel, weedy cracked mud, scorched concrete) times the landscape's brightness (0.592) and a grey shade, so
roads, pads and aprons share the desert's palette and differ in how dark and how worn they are (main road darkest, paths lightest).
(A stain/modulate blend was tried in round 2 and painted everything white.) ASurvivalSovietBasePad gives every decal a dynamic instance with its own frame
(ShapeCenter, ShapeAxis, ShapeHalf), so edges, ruts and fades are measured in cm, not in decal UVs:
- pads: a rounded rectangle whose edge is broken by world-space noise, fully opaque inside, so overlapping pads of one
  surface merge into one yard instead of doubling their tint;
- roads: soft verges with gravel spill along both sides, two wheel ruts that follow the road, ends that fade out;
- concrete: expansion joints on a grid in the shape's own frame."""
import unreal

FOLDER = '/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Pads'
CONCRETE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/T_concrete2_bc'
ROAD_GRAVEL = '/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/T_GroundPath_Gravel_BC'
ROAD_DIRT = '/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/T_GroundPath_Dirt_BC'
# The landscape's own layers.
LS_SAND = '/Game/RTS_Survival/Materials/ground/Landscape/Textures/05_Thai_Rippled_Sand_2x2_M_td1hcimn/td1hcimn_8K_Albedo'
LS_GRAVEL = '/Game/Megascans/Surfaces/Desert_Western_Ground_Gravel_Slate_01_venoehv/T_Desert_Western_Ground_Gravel_Slate_01_venoehv_2K_D'
LS_MUD = '/Game/RTS_Survival/Meshes/Desert/Materials/T_Weedy_Cracked_Mud_vgxlahm_2K_D'
LS_CONCRETE = '/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/ScorchConcrete/T_ScorchConcrete_color'
LS_BRIGHTNESS = 0.592

NOISE = """struct FSovietNoise
{
	float Hash(float2 P) { return frac(sin(dot(P, float2(127.1, 311.7))) * 43758.5453); }
	float Value(float2 P)
	{
		float2 I = floor(P);
		float2 F = frac(P);
		F = F * F * (3.0 - 2.0 * F);
		return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), F.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), F.x), F.y);
	}
	float Fbm(float2 P) { return Value(P) * 0.6 + Value(P * 2.3 + 5.2) * 0.28 + Value(P * 5.1 + 1.7) * 0.12; }
};
FSovietNoise N;
float2 D = Pos.xy - Center.xy;
float2 Ax = normalize(Axis.xy + float2(1e-5, 0));
float2 L = float2(dot(D, Ax), dot(D, float2(-Ax.y, Ax.x)));
"""

GRAIN_HLSL = """float Luma = dot(Tex, float3(0.3, 0.59, 0.11));
// Strength 1 keeps the texture as it is; lower values flatten its contrast around its mean.
float3 Flat = Tex * (Mean / max(Luma, 0.02));
return Tint.rgb * lerp(Flat, Tex, Strength);"""

# Half: x along, y across, z corner radius. Edge: x soft edge cm, y edge noise cm, z mode (0 pad, 1 road), alpha verge strength.
# Fade: x fade at start, y fade at end, z fade length cm.
OPACITY_HLSL = NOISE + """float EdgeNoise = (N.Fbm(Pos.xy / 330.0) - 0.5) * 2.0 * Edge.y;
float Fine = (N.Value(Pos.xy / 38.0) - 0.5) * Edge.x * 0.45;
float Dist;
if (Edge.z < 0.5)
{
	float2 Q = abs(L) - Half.xy + Half.z;
	Dist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Half.z;
}
else
{
	Dist = abs(L.y) - Half.y;
}
float Alpha = saturate((-Dist + EdgeNoise + Fine) / max(Edge.x, 1.0));
if (Edge.z > 0.5)
{
	float Wobble = (N.Fbm(Pos.xy / 210.0) - 0.5) * Fade.z * 0.5;
	float Ends = 1.0;
	if (Fade.x > 0.5) { Ends *= saturate((L.x + Half.x + Wobble) / max(Fade.z, 1.0)); }
	if (Fade.y > 0.5) { Ends *= saturate((Half.x - L.x + Wobble) / max(Fade.z, 1.0)); }
	if (Fade.x < 0.5 && Fade.y < 0.5) { Ends = saturate((Half.x - abs(L.x)) / 20.0); }
	Alpha *= Ends;
}
return Alpha * Opacity;"""

# Ruts: x offset as share of the half width, y rut width cm (the ruts wander by about as much), z darkness. Joint: x spacing cm (0 = none), y darkness.
SHADE_HLSL = NOISE + """float Across = abs(L.y) / max(Half.y, 1.0);
float Verge;
float Rut = 0.0;
if (Edge.z > 0.5)
{
	Verge = smoothstep(0.55, 1.05, Across) * (0.55 + 0.45 * N.Value(Pos.xy / 70.0));
	float Wob = (N.Fbm(float2(L.x / 900.0, Center.x * 0.0007)) - 0.5) * 1.6 * Ruts.y;
	float Off = Ruts.x * Half.y;
	float RutDistance = min(abs(L.y - Off - Wob), abs(L.y + Off - Wob));
	Rut = exp(-(RutDistance * RutDistance) / max(Ruts.y * Ruts.y, 1.0)) * Ruts.z * (0.65 + 0.7 * N.Value(Pos.xy / 160.0));
}
else
{
	float2 Q = abs(L) - Half.xy + Half.z;
	float Dist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Half.z;
	Verge = smoothstep(-Edge.x * 1.6, Edge.y, Dist) * (0.5 + 0.5 * N.Value(Pos.xy / 60.0));
}
float JointLine = 0.0;
if (Joint.x > 1.0)
{
	float2 G = abs(frac(L / Joint.x + 0.5) - 0.5) * Joint.x;
	JointLine = (1.0 - smoothstep(1.5, 5.0, min(G.x, G.y))) * Joint.y;
}
float Broad = 0.88 + 0.24 * N.Fbm(Pos.xy / 1400.0);
float Grain = 0.93 + 0.14 * N.Value(Pos.xy / 55.0);
float3 Shade = lerp(float3(1, 1, 1), VergeTint.rgb, saturate(Verge) * VergeStrength) * Broad * Grain;
return Shade * (1.0 - saturate(Rut)) * (1.0 - JointLine);"""

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def fresh(name, cls, factory):
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, FOLDER, cls, factory)


def scalar(mat, name, value, x, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def vector(mat, name, value, x, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*value))
    return node


def custom(mat, code, output_type, names, x, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output_type)
    inputs = []
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    node.set_editor_property('inputs', inputs)
    return node


def make_material():
    mat = fresh('M_SovietGroundDecal', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1600, 0)
    params = {
        'Center': vector(mat, 'ShapeCenter', (0, 0, 0, 0), -1600, 200),
        'Axis': vector(mat, 'ShapeAxis', (1, 0, 0, 0), -1600, 400),
        'Half': vector(mat, 'ShapeHalf', (500, 500, 120, 0), -1600, 600),
        'Edge': vector(mat, 'ShapeEdge', (260, 140, 0, 1), -1600, 800),
        'Fade': vector(mat, 'ShapeFade', (0, 0, 900, 0), -1600, 1000),
        'Ruts': vector(mat, 'Ruts', (0.42, 28, 0.0, 22), -1600, 1200),
        'Joint': vector(mat, 'Joint', (0, 0.35, 0, 0), -1600, 1400),
        'VergeTint': vector(mat, 'VergeTint', (1.05, 0.95, 0.85, 1), -1600, 1600),
    }
    opacity_value = scalar(mat, 'Opacity', 1.0, -1600, 1800)
    # World-aligned surface texture times a tint.
    mask = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1300, -300)
    mask.set_editor_property('r', True)
    mask.set_editor_property('g', True)
    lib.connect_material_expressions(pos, '', mask, '')
    div = lib.create_material_expression(mat, unreal.MaterialExpressionDivide, -1150, -300)
    lib.connect_material_expressions(mask, '', div, 'A')
    lib.connect_material_expressions(scalar(mat, 'TilingCm', 840.0, -1300, -150), '', div, 'B')
    tex = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -950, -300)
    tex.set_editor_property('parameter_name', 'Surface')
    tex.set_editor_property('texture', unreal.load_asset(ROAD_GRAVEL))
    lib.connect_material_expressions(div, '', tex, 'UVs')
    tint = vector(mat, 'Tint', (1, 1, 1, 1), -950, -100)
    # The texture gives grain only (its luminance around its mean), never its colour.
    tinted = custom(mat, GRAIN_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ['Tex', 'Tint', 'Strength', 'Mean'], -700, -300)
    lib.connect_material_expressions(tex, 'RGB', tinted, 'Tex')
    lib.connect_material_expressions(tint, '', tinted, 'Tint')
    lib.connect_material_expressions(scalar(mat, 'GrainStrength', 0.5, -950, 50), '', tinted, 'Strength')
    lib.connect_material_expressions(scalar(mat, 'GrainMean', 0.45, -950, 150), '', tinted, 'Mean')

    shade = custom(mat, SHADE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ['Pos', 'Center', 'Axis', 'Half', 'Edge', 'Ruts', 'Joint', 'VergeTint', 'VergeStrength'], -900, 300)
    for name in ['Center', 'Axis', 'Half', 'Edge', 'Ruts', 'Joint', 'VergeTint']:
        lib.connect_material_expressions(params[name], '', shade, name)
    # A vector parameter reaches a custom node as float3: the edge's verge strength travels in its alpha output.
    lib.connect_material_expressions(params['Edge'], 'A', shade, 'VergeStrength')
    lib.connect_material_expressions(pos, '', shade, 'Pos')
    color = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, -200)
    lib.connect_material_expressions(tinted, '', color, 'A')
    lib.connect_material_expressions(shade, '', color, 'B')
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -450, 100)
    rough.set_editor_property('r', 0.93)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    opacity = custom(mat, OPACITY_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                     ['Pos', 'Center', 'Axis', 'Half', 'Edge', 'Fade', 'Opacity'], -900, 900)
    for name in ['Center', 'Axis', 'Half', 'Edge', 'Fade']:
        lib.connect_material_expressions(params[name], '', opacity, name)
    lib.connect_material_expressions(pos, '', opacity, 'Pos')
    lib.connect_material_expressions(opacity_value, '', opacity, 'Opacity')
    lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def make_instance(name, parent, texture, tint, tiling, verge, ruts=None, joint=None, opacity=1.0, grain=0.5):
    inst = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    inst.set_editor_property('parent', parent)
    lib.set_material_instance_texture_parameter_value(inst, 'Surface', unreal.load_asset(texture))
    lib.set_material_instance_vector_parameter_value(inst, 'Tint', unreal.LinearColor(*tint))
    lib.set_material_instance_vector_parameter_value(inst, 'VergeTint', unreal.LinearColor(*verge))
    lib.set_material_instance_scalar_parameter_value(inst, 'TilingCm', tiling)
    lib.set_material_instance_scalar_parameter_value(inst, 'Opacity', opacity)
    lib.set_material_instance_scalar_parameter_value(inst, 'GrainStrength', grain)
    if ruts is not None:
        lib.set_material_instance_vector_parameter_value(inst, 'Ruts', unreal.LinearColor(*ruts))
    if joint is not None:
        lib.set_material_instance_vector_parameter_value(inst, 'Joint', unreal.LinearColor(*joint))
    lib.update_material_instance(inst)
    unreal.EditorAssetLibrary.save_loaded_asset(inst, only_if_is_dirty=False)


material = make_material()
def shade(value):
    # The landscape's brightness times a grey shade: 1 matches the landscape's own layer, lower is darker (more driven).
    v = LS_BRIGHTNESS * value
    return (v, v, v, 1)


VERGE = (0.97, 0.97, 0.97, 1)
# Roads from the landscape's gravel, darker the more they are driven; tracks and yard paths are worn cracked earth.
make_instance('MI_SovietRoad_Main', material, LS_GRAVEL, shade(0.66), 600.0, VERGE, ruts=(0.40, 30.0, 0.34), grain=0.9)
make_instance('MI_SovietRoad_Lane', material, LS_GRAVEL, shade(0.78), 600.0, VERGE, ruts=(0.48, 24.0, 0.28), grain=0.8)
make_instance('MI_SovietRoad_Track', material, LS_MUD, shade(0.88), 700.0, VERGE, ruts=(0.55, 22.0, 0.22), opacity=0.9, grain=0.8)
# Scorched concrete slabs through the gates and under workshops, with slab joints. The scorched-concrete layer is a cool grey-green;
# a sand cast and faint, wide joints keep it from reading as a grey-green grid next to the warm sand (level critic, round 6).
def sand_cast(value):
    v = LS_BRIGHTNESS * value
    return (v * 1.14, v * 1.0, v * 0.80, 1)


# Round 2 (critic: still a grey slab grid): gate aprons are packed gravel like the main road, a little lighter and without ruts,
# so the road runs through the gate; concrete pads are the same packed gravel a shade lighter, no joints.
make_instance('MI_SovietRoad_Apron', material, LS_GRAVEL, shade(0.74), 600.0, VERGE, grain=0.9)
make_instance('MI_SovietPad_Concrete', material, LS_GRAVEL, shade(0.86), 600.0, VERGE, grain=0.85)
# Pads stay close to the ground: gravel a shade darker than the sand, packed earth under the defences darker still.
make_instance('MI_SovietPad_Gravel', material, LS_GRAVEL, shade(0.95), 600.0, VERGE, grain=0.8)
make_instance('MI_SovietPad_PackedEarth', material, LS_MUD, shade(0.85), 700.0, VERGE, opacity=0.9, grain=0.8)
names = ['M_SovietGroundDecal', 'MI_SovietPad_Concrete', 'MI_SovietPad_Gravel', 'MI_SovietPad_PackedEarth', 'MI_SovietRoad_Main',
         'MI_SovietRoad_Lane', 'MI_SovietRoad_Track', 'MI_SovietRoad_Apron']
for name in names:
    print('SOVIET_DECAL', name, unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + name))
