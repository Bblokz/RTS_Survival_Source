"""The shared concrete of the world map pieces: M_WorldPiece_Concrete and its instances.

The game's bunker concrete (M_BF_SootedFortress_Procedural) cannot be used on the pieces: its pattern is in object
centimetres with fixed sizes for a real bunker (noise every 13 to 140 cm, pours of at least 35 cm), so on a 60 cm piece
it is one blotch, and on the real flak tower shrunk to 60 cm it is noise. This material samples the same Sooted
Fortress base colour, normal and ORM textures, but every size is a parameter in object centimetres:
box projection (no mesh UVs needed), pour and panel joints with relief, macro colour variation, rain streaks, and
ground grime at the foot of the walls.
"""
import unreal

import piece_lib as lib

BF_MATERIAL_FOLDER = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker/Material/"
TEST_FOLDER = lib.SHARED_FOLDER + "/ConcreteTest"
library = unreal.MaterialEditingLibrary

PROJECT_CODE = """// Box projection in object centimetres: roofs use XY, walls the two axes of their plane.
float3 an=abs(normalize(LN));
return (an.z>0.65)?P.xy:((an.x>an.y)?P.yz:P.xz);"""

STREAK_UV_CODE = """// Thin runs: narrow across the wall, long down it.
return float2(Pp.x/max(StreakCm,0.01),Pp.y/max(StreakCm*14.0,0.01))+0.23;"""

LAYERS_CODE = """// Cast concrete at any size. Output: R=albedo multiplier, G=height (object cm), B=roughness delta, A=ground grime.
struct WPConcrete
{
    float hash(float2 p)
    {
        float3 q=frac(float3(p.x,p.y,p.x)*0.1031);
        q+=dot(q,q.yzx+33.33);
        return frac((q.x+q.y)*q.z);
    }
};
WPConcrete f;
float3 n=normalize(LN);
float3 an=abs(n);
float wall=1.0-smoothstep(0.35,0.6,an.z);
float up=smoothstep(0.55,0.85,n.z);
float pour=max(Pour,0.001),panel=max(Panel,0.001),joint=max(Joint,0.0001);
float2 grid=float2(Pp.x/panel+0.21,Pp.y/pour+0.13);
grid.y+=(Macro.g-0.5)*0.10;
float row=floor(grid.y);
// Neighbouring pours have their panel joints offset, so it does not read as brickwork.
grid.x+=(f.hash(float2(row,Seed))-0.5)*0.7;
float2 d=(frac(grid)-0.5)*float2(panel,pour);
float aa=max(fwidth(Pp.y)+fwidth(Pp.x),joint*0.2);
float lineH=1.0-smoothstep(joint*0.5,joint*0.5+aa,abs(d.y));
float lineV=(1.0-smoothstep(joint*0.3,joint*0.3+aa,abs(d.x)))*0.55;
float groove=max(lineH,lineV)*lerp(0.5,1.0,Macro.b);
// A pale squeezed-out fin under each pour joint.
float fin=exp2(-pow((d.y+joint*1.1)/max(joint*0.6,aa),2.0))*lerp(0.2,1.0,Macro.a);
float batch=f.hash(floor(grid)+float2(Seed,3.7))-0.5;
float tint=1.0+((Macro.r-0.5)*0.34+(Macro.g-0.5)*0.12)*Variation+batch*0.20*Variation;
tint+=fin*0.16-groove*0.38;
float run=Runs.a*0.7+Runs.b*0.3;
float below=frac(grid.y+0.5);
float streak=wall*smoothstep(0.45,0.75,run)*smoothstep(0.25,0.6,Macro.a)*lerp(0.35,1.0,below*below);
tint*=1.0-streak*0.42;
tint+=up*(Macro.r-0.35)*0.16;
float edge=max(Grime,0.001)*(0.55+0.45*Macro.g);
float z=P.z-GrimeBase;
float ground=(1.0-smoothstep(edge*0.45,edge,z+(Macro.b-0.5)*Grime*0.35))*(1.0-up);
float h=(fin*0.35-groove*0.6)*joint;
float rough=groove*0.05+streak*0.07-(Macro.r-0.5)*0.12*Variation+ground*0.05;
return float4(clamp(tint,0.45,1.5),h,rough,saturate(ground));"""

NORMAL_CODE = """// Object-space normal: the tangent-space texture normal laid on the box projection, then the screen-space gradient
// of the centimetre height H (W and H are both object centimetres, so the slope has no unit and no scale).
float3 n=normalize(LN);
float3 an=abs(n);
float3 T=(an.z>0.65)?float3(1,0,0):((an.x>an.y)?float3(0,1,0):float3(1,0,0));
float3 B=(an.z>0.65)?float3(0,1,0):float3(0,0,1);
T=normalize(T-n*dot(n,T));
B=normalize(B-n*dot(n,B));
float3 t=float3(TN.xy*Strength,TN.z);
float3 base=normalize(t.x*T+t.y*B+t.z*n);
float3 dx=ddx(W),dy=ddy(W);
float3 r1=cross(dy,n),r2=cross(n,dx);
float det=dot(dx,r1);
float safeDet=(det>=0.0?1.0:-1.0)*max(abs(det),1.e-12);
float3 gradient=(ddx(H)*r1+ddy(H)*r2)/safeDet*Relief;
gradient=gradient/max(1.0,length(gradient)/1.35);
return normalize(base-gradient);"""

# Sizes in centimetres of a piece modelled at map size (200 cm across). 'lengths' are multiplied by length_scale.
LENGTH_PARAMETERS = ("TileCm", "MacroCm", "PourHeightCm", "PanelWidthCm", "JointWidthCm", "GrimeHeightCm",
                     "GrimeBaseCm", "StreakCm")
DEFAULTS = {"TileCm": 30.0, "MacroCm": 70.0, "PourHeightCm": 9.0, "PanelWidthCm": 20.0, "JointWidthCm": 0.5,
            "GrimeHeightCm": 7.0, "GrimeBaseCm": 0.0, "StreakCm": 9.0, "ReliefStrength": 1.0, "NormalStrength": 1.0,
            "Variation": 1.0, "RoughnessOffset": 0.0, "Seed": 17.0}


def _connect(source, output, target, target_input):
    if not library.connect_material_expressions(source, output, target, target_input):
        lib.log("CONNECT FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), target_input))


def _node(material, node_class, x, y, **properties):
    node = library.create_material_expression(material, node_class, x, y)
    for name, value in properties.items():
        node.set_editor_property(name, value)
    return node


def _custom(material, description, code, output_type, input_names, x, y):
    inputs = []
    for name in input_names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    return _node(material, unreal.MaterialExpressionCustom, x, y, description=description, code=code,
                 output_type=output_type, inputs=inputs)


def _scalar(material, name, y):
    return _node(material, unreal.MaterialExpressionScalarParameter, -2600, y, parameter_name=name,
                 default_value=DEFAULTS[name])


def _texture_parameter(material, name, texture_name, sampler, y):
    return _node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1500, y, parameter_name=name,
                 texture=unreal.load_asset(BF_MATERIAL_FOLDER + texture_name), sampler_type=sampler)


def _mask(material, source, channel, y):
    node = _node(material, unreal.MaterialExpressionComponentMask, -500, y, r=channel == "r", g=channel == "g",
                 b=channel == "b", a=channel == "a")
    _connect(source, "", node, "")
    return node


def _binary(material, node_class, a, b, x, y, a_output="", b_output=""):
    node = _node(material, node_class, x, y)
    _connect(a, a_output, node, "A")
    _connect(b, b_output, node, "B")
    return node


def ensure_concrete_material():
    """Builds M_WorldPiece_Concrete from scratch on every run, so this file is the source of the graph."""
    material, _is_new = lib._new_material("M_WorldPiece_Concrete")
    # One call leaves about half of the nodes behind (it removes from the list it walks), so repeat until empty.
    for _attempt in range(12):
        if library.get_num_material_expressions(material) == 0:
            break
        library.delete_all_material_expressions(material)
    lib.log("old concrete graph cleared, %d nodes left" % library.get_num_material_expressions(material))
    material.set_editor_property("tangent_space_normal", False)
    sampler = unreal.MaterialSamplerType
    output = unreal.CustomMaterialOutputType
    scalars = {name: _scalar(material, name, -900 + index * 110) for index, name in enumerate(DEFAULTS)}

    world_position = _node(material, unreal.MaterialExpressionWorldPosition, -2600, -1500)
    local_position = _node(material, unreal.MaterialExpressionTransformPosition, -2350, -1500,
                           transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMPOSSOURCE_WORLD
                           if hasattr(unreal.MaterialVectorCoordTransformSource, "TRANSFORMPOSSOURCE_WORLD")
                           else unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                           transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    _connect(world_position, "", local_position, "")
    world_normal = _node(material, unreal.MaterialExpressionVertexNormalWS, -2600, -1300)
    local_normal = _node(material, unreal.MaterialExpressionTransform, -2350, -1300,
                         transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                         transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    _connect(world_normal, "", local_normal, "")

    projected = _custom(material, "WP_ConcreteProject", PROJECT_CODE, output.CMOT_FLOAT2, ["P", "LN"], -2050, -1400)
    _connect(local_position, "", projected, "P")
    _connect(local_normal, "", projected, "LN")
    tile_uv = _binary(material, unreal.MaterialExpressionDivide, projected, scalars["TileCm"], -1800, -1500)
    macro_uv = _binary(material, unreal.MaterialExpressionDivide, projected, scalars["MacroCm"], -1800, -1350)
    streak_uv = _custom(material, "WP_ConcreteStreakUV", STREAK_UV_CODE, output.CMOT_FLOAT2, ["Pp", "StreakCm"],
                        -1800, -1200)
    _connect(projected, "", streak_uv, "Pp")
    _connect(scalars["StreakCm"], "", streak_uv, "StreakCm")

    base_color = _texture_parameter(material, "BaseColor", "T_BF_Sooted_Fortress_BaseColor",
                                    sampler.SAMPLERTYPE_COLOR, -1700)
    normal = _texture_parameter(material, "Normal", "T_BF_Sooted_Fortress_NormalDX", sampler.SAMPLERTYPE_NORMAL, -1450)
    masks = _texture_parameter(material, "ORM", "T_BF_Sooted_Fortress_ORM_Linear", sampler.SAMPLERTYPE_MASKS, -1200)
    for node in (base_color, normal, masks):
        _connect(tile_uv, "", node, "UVs")
    noise = unreal.load_asset(BF_MATERIAL_FOLDER + "T_BF_ConcreteNoise")
    macro = _node(material, unreal.MaterialExpressionTextureSample, -1500, -950, texture=noise,
                  sampler_type=sampler.SAMPLERTYPE_LINEAR_COLOR)
    _connect(macro_uv, "", macro, "UVs")
    runs = _node(material, unreal.MaterialExpressionTextureSample, -1500, -700, texture=noise,
                 sampler_type=sampler.SAMPLERTYPE_LINEAR_COLOR)
    _connect(streak_uv, "", runs, "UVs")

    layer_inputs = ["Pp", "P", "LN", "Macro", "Runs", "Pour", "Panel", "Joint", "Grime", "GrimeBase", "Seed",
                    "Variation"]
    layers = _custom(material, "WP_ConcreteLayers", LAYERS_CODE, output.CMOT_FLOAT4, layer_inputs, -1000, -700)
    _connect(projected, "", layers, "Pp")
    _connect(local_position, "", layers, "P")
    _connect(local_normal, "", layers, "LN")
    _connect(macro, "RGBA", layers, "Macro")
    _connect(runs, "RGBA", layers, "Runs")
    for pin, parameter in (("Pour", "PourHeightCm"), ("Panel", "PanelWidthCm"), ("Joint", "JointWidthCm"),
                           ("Grime", "GrimeHeightCm"), ("GrimeBase", "GrimeBaseCm"), ("Seed", "Seed"),
                           ("Variation", "Variation")):
        _connect(scalars[parameter], "", layers, pin)
    tint_mask = _mask(material, layers, "r", -900)
    height_mask = _mask(material, layers, "g", -750)
    rough_mask = _mask(material, layers, "b", -600)
    grime_mask = _mask(material, layers, "a", -450)

    tint = _node(material, unreal.MaterialExpressionVectorParameter, -1500, -1950, parameter_name="Tint",
                 default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = _binary(material, unreal.MaterialExpressionMultiply, base_color, tint, -300, -1700, a_output="RGB")
    varied = _binary(material, unreal.MaterialExpressionMultiply, tinted, tint_mask, -150, -1600)
    grime_color = _node(material, unreal.MaterialExpressionConstant3Vector, -300, -1400,
                        constant=unreal.LinearColor(0.10, 0.08, 0.055, 1.0))
    grime_amount = _binary(material, unreal.MaterialExpressionMultiply, grime_mask,
                           _node(material, unreal.MaterialExpressionConstant, -500, -300, r=0.6), -300, -400)
    final_color = _node(material, unreal.MaterialExpressionLinearInterpolate, 0, -1500)
    _connect(varied, "", final_color, "A")
    _connect(grime_color, "", final_color, "B")
    _connect(grime_amount, "", final_color, "Alpha")
    library.connect_material_property(final_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_sum = _binary(material, unreal.MaterialExpressionAdd, masks, rough_mask, -300, -1100, a_output="G")
    rough_shift = _binary(material, unreal.MaterialExpressionAdd, rough_sum, scalars["RoughnessOffset"], -150, -1100)
    roughness = _node(material, unreal.MaterialExpressionSaturate, 0, -1100)
    _connect(rough_shift, "", roughness, "")
    library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(masks, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    normal_inputs = ["W", "LN", "TN", "H", "Relief", "Strength"]
    object_normal = _custom(material, "WP_ConcreteNormal", NORMAL_CODE, output.CMOT_FLOAT3, normal_inputs, -300, -800)
    _connect(local_position, "", object_normal, "W")
    _connect(local_normal, "", object_normal, "LN")
    _connect(normal, "RGB", object_normal, "TN")
    _connect(height_mask, "", object_normal, "H")
    _connect(scalars["ReliefStrength"], "", object_normal, "Relief")
    _connect(scalars["NormalStrength"], "", object_normal, "Strength")
    world_space_normal = _node(material, unreal.MaterialExpressionTransform, 0, -800,
                               transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    _connect(object_normal, "", world_space_normal, "")
    library.connect_material_property(world_space_normal, "", unreal.MaterialProperty.MP_NORMAL)

    library.recompile_material(material)
    lib.save(material)
    lib.log("concrete material built with %d expressions" % library.get_num_material_expressions(material))
    return material


def concrete_instance(material, folder, name, length_scale=1.0, tint=None, **overrides):
    """length_scale: object centimetres per centimetre of a map-size piece (1 for the models, 1 / scale for a real
    game mesh that is shrunk on the piece)."""
    values = dict(DEFAULTS, **overrides)
    scalars = {key: value * (length_scale if key in LENGTH_PARAMETERS else 1.0) for key, value in values.items()}
    return lib.make_instance(folder, name, material, scalars=scalars, vectors={"Tint": tint} if tint else None)
