"""Radixite blight as a material function, MF_RTS_LS_RadixiteBlight (Docs/Landscape/LandscapeMaster.md).

MF_RTS_LS_RadixiteBlight(Attributes, DirtColor optional) -> Attributes, Blight
  The ground around radixite crystals is dead: grass turns ash-grey, the soil darkens, and a faint radixite tint
  shows near the crystals. Blight is 1 on dead ground; landscape grass uses it (MF_RTS_LS_GrassWeights).
The blighted areas come from MPC_RadixiteBlight, which URadixiteBlightSubsystem fills per world from the level's
radixite resource actors: up to MAX_BLOBS circles RadixiteBlight<i> = (centre x, y, radius, 1 / radius^2), their bounds
RadixiteBlightBounds = (min x, min y, max x, max y) and RadixiteBlightCount. A collection, not instance parameters,
because the landscape instances are shared by many maps.
One custom node tests the bounds first: pixels outside every blob's bounds do no more work, and the edge noise is
only sampled near a blob. Masters call it before MF_RTS_ColorVariation_Landscape, so the RVT holds the dead ground.
"""
import unreal

import ls_graph as G
from ls_graph import N, C, S, V, mul, lerp, mask, const

FUNCTION_NAME = "MF_RTS_LS_RadixiteBlight"
MPC_NAME = "MPC_RadixiteBlight"
MPC_DIR = "/Game/RTS_Survival/Materials/ground/Landscape"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise"
GROUP = "Radixite Blight"
# Must match URadixiteBlightSubsystem's blob limit.
MAX_BLOBS = 8
# The edge noise tiles every 6 m (features well under a metre): a ragged, not lumpy, edge.
EDGE_NOISE_SCALE = 600.0

# Nearest blob in blob radii (0 at a centre, 1 on its rim), then the noise-broken mask.
# R = blight (1 dead, 0 alive), G = core (1 at a blob centre, for the radixite tint).
BLIGHT_HLSL_HEAD = """float4 Blobs[%d] = {%s};
"""
BLIGHT_HLSL_BODY = """float2 NoiseUV = WorldXY / NoiseScale;
// Derivatives before any branch: the noise is sampled inside one, where they are undefined.
float2 NoiseDdx = ddx(NoiseUV);
float2 NoiseDdy = ddy(NoiseUV);
if (Count < 0.5 || any(WorldXY < Bounds.xy) || any(WorldXY > Bounds.zw))
{
    return float2(0.0, 0.0);
}
// Squared distances scaled by 1 / radius^2 (w, precomputed), one square root at the end: the loop is unrolled per
// circle, and its size is what the editor's shader complexity view shows.
float nearestSquared = 10000.0;
int count = (int)Count;
[unroll] for (int i = 0; i < %d; i++)
{
    if (i >= count)
    {
        break;
    }
    float2 offset = WorldXY - Blobs[i].xy;
    nearestSquared = min(nearestSquared, dot(offset, offset) * Blobs[i].w);
}
float nearest = sqrt(nearestSquared);
if (nearest > 1.0 + EdgeNoise)
{
    return float2(0.0, 0.0);
}
float noise = Tex.SampleGrad(TexSampler, NoiseUV, NoiseDdx, NoiseDdy).r;
float warped = nearest + (noise - 0.5) * EdgeNoise * 2.0;
float blight = 1.0 - smoothstep(1.0 - EdgeWidth, 1.0, warped);
float core = 1.0 - smoothstep(0.0, CoreSize, nearest);
return float2(blight, core);"""


def blob_parameter(index):
    return "RadixiteBlight%d" % index


def collection():
    """MPC_RadixiteBlight with zero defaults: a map without radixite shows no blight."""
    path = MPC_DIR + "/" + MPC_NAME
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MPC_NAME, MPC_DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    mpc = unreal.load_asset(path)
    vectors = []
    for name in [blob_parameter(i) for i in range(MAX_BLOBS)] + ["RadixiteBlightBounds"]:
        parameter = unreal.CollectionVectorParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        vectors.append(parameter)
    count = unreal.CollectionScalarParameter()
    count.set_editor_property("parameter_name", "RadixiteBlightCount")
    count.set_editor_property("default_value", 0.0)
    mpc.set_editor_property("vector_parameters", vectors)
    mpc.set_editor_property("scalar_parameters", [count])
    unreal.EditorAssetLibrary.save_loaded_asset(mpc, False)
    return mpc


def collection_parameter(mpc, name, x, y):
    e = N("CollectionParameter", x, y, collection=mpc)
    e.set_editor_property("parameter_name", name)
    return e


def blight_node(mpc, x, y):
    """The custom node: float2(blight, core) from the collection's blobs."""
    pins = []
    for i in range(MAX_BLOBS):
        pins.append(("B%d" % i, collection_parameter(mpc, blob_parameter(i), x - 500, y + i * 90)))
    code = BLIGHT_HLSL_HEAD % (MAX_BLOBS, ", ".join(n for n, _ in pins)) + BLIGHT_HLSL_BODY % MAX_BLOBS
    names = ["WorldXY", "Bounds", "Count", "Tex", "NoiseScale", "EdgeNoise", "EdgeWidth", "CoreSize"]
    blight = G.custom(code, unreal.CustomMaterialOutputType.CMOT_FLOAT2, "RadixiteBlight",
                      names + [n for n, _ in pins], x, y)
    C(mask(N("WorldPosition", x - 500, y - 400), "", "RG", x - 340, y - 400), "", blight, "WorldXY")
    C(collection_parameter(mpc, "RadixiteBlightBounds", x - 500, y - 300), "", blight, "Bounds")
    C(collection_parameter(mpc, "RadixiteBlightCount", x - 500, y - 200), "", blight, "Count")
    C(N("TextureObject", x - 300, y - 300, texture=unreal.load_asset(NOISE),
        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS), "", blight, "Tex")
    C(const(EDGE_NOISE_SCALE, x - 300, y - 160), "", blight, "NoiseScale")
    C(S("Blight_EdgeNoise", 0.22, x - 300, y - 100), "", blight, "EdgeNoise")
    C(S("Blight_EdgeWidth", 0.14, x - 300, y - 40), "", blight, "EdgeWidth")
    C(S("Blight_CoreSize", 0.45, x - 300, y + 20), "", blight, "CoreSize")
    for name, node in pins:
        C(node, "", blight, name)
    return blight


def build_function():
    mpc = collection()
    function = G.function_asset(FUNCTION_NAME, "Dead ground around radixite crystals (MPC_RadixiteBlight, filled "
                                "by URadixiteBlightSubsystem): ash-grey, darker soil, radixite tint near the crystals. "
                                "Returns the attributes and Blight (1 = dead).")
    G.begin(function, group=GROUP)
    X0, Y0 = 0, 0
    attributes = G.function_input("Attributes", G.ATTRIBUTES, 0, X0 + 900, Y0 - 500, "Ground before the blight.")
    default_dirt = V("Blight_DirtColor", unreal.LinearColor(0.16, 0.13, 0.1, 1.0), X0 + 700, Y0 - 300)
    dirt = G.function_input("DirtColor", G.VECTOR3, 1, X0 + 900, Y0 - 300,
                            "Optional: the map's bare-soil colour (MF_RTS_LS_GroundBreakup's DirtColor).",
                            default_dirt, "")
    brk = N("BreakMaterialAttributes", X0 + 1100, Y0 - 500)
    C(attributes, "", brk, "Attr")

    G.comment("Blight mask: the nearest radixite blob (MPC_RadixiteBlight), its rim broken by noise "
              "(Blight_EdgeNoise, Blight_EdgeWidth). Pixels outside every blob's bounds do no more work.",
              X0 - 600, Y0 - 520, 1100, 1400, G.INPUT_COLOR)
    blight_core = blight_node(mpc, X0, Y0)
    blight = mask(blight_core, "", "R", X0 + 300, Y0)
    core = mask(blight_core, "", "G", X0 + 300, Y0 + 100)

    G.comment("Dead ground: the ground desaturated and ash-tinted, part bare soil, darkened; near the crystals a "
              "faint radixite tint. Dead ground is rough and matte.", X0 + 1040, Y0 - 620, 1500, 1100)
    ash = mul(G.desaturate(brk, 0.85, X0 + 1300, Y0 - 300), "",
              V("Blight_AshTint", unreal.LinearColor(0.95, 0.9, 0.8, 1.0), X0 + 1300, Y0 - 200), "",
              X0 + 1460, Y0 - 300)
    dead_soil = lerp(ash, "", dirt, "", S("Blight_SoilAmount", 0.35, X0 + 1460, Y0 - 120), "", X0 + 1620, Y0 - 260)
    dead = mul(dead_soil, "", S("Blight_Darken", 0.72, X0 + 1620, Y0 - 120), "", X0 + 1780, Y0 - 260)
    tinted = mul(G.desaturate(dead, 1.0, X0 + 1780, Y0 - 120), "",
                 V("Blight_RadixiteTint", unreal.LinearColor(0.85, 1.0, 0.35, 1.0), X0 + 1780, Y0 - 40), "",
                 X0 + 1940, Y0 - 120)
    tint_amount = mul(core, "", S("Blight_TintAmount", 0.3, X0 + 1780, Y0 + 60), "", X0 + 1940, Y0 + 40)
    dead_tinted = lerp(dead, "", tinted, "", tint_amount, "", X0 + 2100, Y0 - 200)
    bc = lerp(brk, "BaseColor", dead_tinted, "", blight, "", X0 + 2260, Y0 - 300)
    rough_dead = G.op("Max", brk, "Roughness", S("Blight_Roughness", 0.92, X0 + 2100, Y0 + 80), "",
                      X0 + 2260, Y0 + 40)
    rough = lerp(brk, "Roughness", rough_dead, "", blight, "", X0 + 2420, Y0 - 40)

    make = N("MakeMaterialAttributes", X0 + 2600, Y0 - 300)
    C(bc, "", make, "BaseColor")
    C(rough, "", make, "Roughness")
    for pin in ["Metallic", "Specular", "Normal", "AmbientOcclusion"]:
        C(brk, pin, make, pin)
    G.function_output("Attributes", make, "", 0, X0 + 2850, Y0 - 300, "Ground with the blight.")
    G.function_output("Blight", blight, "", 1, X0 + 2850, Y0, "1 on dead ground, 0 elsewhere.")
    return G.finish_function(function)
