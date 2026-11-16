"""Materials of the 1976 intro cinematic, built by script (re-runnable): the parameter collection that Sequencer
animates, the two-grade post process, the map table paper, burn rings, the purple stain, text planes.

Most of the look sits in Custom (HLSL) nodes so one script can rebuild it. Run inside the throwaway editor.
"""
import importlib

import unreal

import intro_lib as lib

importlib.reload(lib)

MAT_DIR = lib.ROOT + "/Materials"
MPC_PATH = MAT_DIR + "/MPC_Intro"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Scalars Sequencer animates through a Material Parameter Collection track (name: default).
MPC_SCALARS = {
    "Archive": 0.0,       # 0 = present grade (1976), 1 = archive grade (1944-1975)
    "Flash": 0.0,         # white-out (Berlin)
    "Darken": 0.0,        # lamp lowered / flicker: darkens the whole frame slightly
    "Stain": 0.0,         # purple stain growth on the map, 0..1
    "StainOpacity": 0.0,  # stain plane on/off
    "Heat": 0.0,          # heat shimmer strength in the present grade
    "Rings36": 0.0,       # clocks in seconds: each burn ring starts at its own Delay on its group clock
    "Rings42": 0.0,
    "Rings62": 0.0,
    "Pencil": 0.0,        # the red pencil label on the map, 0..1
    "Mould": 0.0,         # purple growth in the trench, 0..1
    "Crystal": 0.0,       # Radixite glow pulse, 0..1
    "Banner": 0.0,        # banners unrolled, 0..1
    "Rain": 0.0,          # weather drawn by the grade: slanted rain streaks and a wet haze, 0..1
    "Sand": 0.0,          # weather drawn by the grade: wind-driven sand streaming across the frame, 0..1
    "PurpleKeep": 1.0,    # 1 = the archive grade keeps purple at full colour; 0 in Berlin (no purple there, doc)
}

# HLSL has no nested functions and a Custom node body is itself a function, so the noise lives in a local struct.
VNOISE = """
struct FIntroNoise {
  float h(float2 v) { return frac(sin(dot(v, float2(127.1, 311.7))) * 43758.5453); }
  float n(float2 q) { float2 i = floor(q); float2 f = frac(q); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y); }
};
FIntroNoise IntroNoise;
#define H2(v) IntroNoise.h(v)
#define vnoise(q) IntroNoise.n(q)
"""


# Branching vein network (warped Voronoi edges at three sizes); also used by intro_purple for the trench mould.
PD_VEINS = """
struct FPurpleDeath {
  float h(float2 v) { return frac(sin(dot(v, float2(127.1, 311.7))) * 43758.5453); }
  float2 h2(float2 v) { return frac(sin(float2(dot(v, float2(127.1, 311.7)), dot(v, float2(269.5, 183.3)))) * 43758.5453); }
  float n(float2 q) { float2 i = floor(q); float2 f = frac(q); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y); }
  float fbm(float2 q) { float s = 0.0; float a = 0.5;
    for (int k = 0; k < 4; k++) { s += a * n(q); q = q * 2.03 + 1.7; a *= 0.5; } return s; }
  float edge(float2 q) { float2 i = floor(q); float2 f = frac(q); float d1 = 8.0; float d2 = 8.0;
    for (int y = -1; y <= 1; y++) { for (int x = -1; x <= 1; x++) {
      float2 g = float2(x, y); float2 r = g + h2(i + g) - f; float d = dot(r, r);
      if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; } } }
    return sqrt(d2) - sqrt(d1); }
  // Ridged multifractal: long branching vessels (river-like), deliberately not Voronoi cells, so the Purple Death
  // never rhymes with the cracked plates of the Radixite crater (sections 5 and 8 stay apart, doc).
  float ridged(float2 q) { float s = 0.0; float a = 0.55; float w = 1.0;
    for (int k = 0; k < 5; k++) { float r = 1.0 - abs(n(q) * 2.0 - 1.0); r = r * r; s += r * a * w;
      w = saturate(r * 1.6); q = q * 2.07 + float2(3.1, 1.7); a *= 0.5; }
    return s; }
  float big(float2 q) {
    float2 w = q + (float2(fbm(q * 0.5), fbm(q * 0.5 + 5.2)) - 0.5) * 1.3;
    float r = 1.0 - abs(n(w * 0.7) * 2.0 - 1.0);
    return smoothstep(0.9, 0.985, r); }
  float veins(float2 q) {
    float2 w = q + (float2(fbm(q * 0.5), fbm(q * 0.5 + 5.2)) - 0.5) * 1.3;
    return saturate(smoothstep(0.66, 0.88, ridged(w * 0.8)) + big(q)); }
};
FPurpleDeath PD;
"""


def _new_material(name, domain=None, blend=None, shading=None, two_sided=False):
    path = MAT_DIR + "/" + name
    # Reuse the asset on a rebuild: deleting it would null every reference placed actors hold.
    material = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if material is None:
        material = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    else:
        mel.delete_all_material_expressions(material)
    if domain is not None:
        material.set_editor_property("material_domain", domain)
    if blend is not None:
        material.set_editor_property("blend_mode", blend)
    if shading is not None:
        material.set_editor_property("shading_model", shading)
    material.set_editor_property("two_sided", two_sided)
    return material


def _custom(material, code, inputs, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, x=-500, y=0):
    """Custom node; inputs: list of (name, expression, output name)."""
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    pins = []
    for name, _expression, _output in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    for name, expression, output in inputs:
        mel.connect_material_expressions(expression, output, node, name)
    return node


def _mask(material, source, channels, x=-200, y=0):
    node = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in "rgba":
        node.set_editor_property(channel, channel in channels)
    mel.connect_material_expressions(source, "", node, "")
    return node


def _scalar(material, name, default, y=0):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def _vector(material, name, default, y=0):
    node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*default))
    return node


def _collection(material, name, y=0):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCollectionParameter, -900, y)
    node.set_editor_property("collection", unreal.load_asset(MPC_PATH))
    node.set_editor_property("parameter_name", name)
    return node


def _finish(material):
    mel.recompile_material(material)
    lib.safe_save([material.get_outermost()])
    print("material", material.get_name())
    return material


def instance(name, parent, scalars=None, vectors=None, textures=None):
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if mi is None:
        mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*value))
    for key, value in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, key, value)
    lib.safe_save([mi.get_outermost()])
    return mi


# ---------------------------------------------------------------------------------------------------------------------
def build_mpc():
    mpc = unreal.load_asset(MPC_PATH)
    if mpc is None:
        mpc = tools.create_asset("MPC_Intro", MAT_DIR, unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    # Existing parameters are kept as they are: a material's collection node finds its scalar by the parameter's
    # id, so replacing the list would break every material built before (they fail to compile in the render
    # process: "CollectionParameter has invalid parameter"). New scalars are only appended.
    parameters = list(mpc.get_editor_property("scalar_parameters"))
    known = {str(p.get_editor_property("parameter_name")) for p in parameters}
    for name, default in MPC_SCALARS.items():
        if name in known:
            continue
        parameter = unreal.CollectionScalarParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", default)
        parameters.append(parameter)
    mpc.set_editor_property("scalar_parameters", parameters)
    lib.safe_save([mpc.get_outermost()])
    print("mpc", [str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("scalar_parameters")])
    return mpc


GRADE_CODE = VNOISE + """
float2 uv = UV;
float t = floor(Time * 24.0);
// Archive: gate weave (the whole picture drifts a little, per film frame).
float2 weave = (float2(H2(float2(t, 1.3)), H2(float2(t, 7.7))) - 0.5) * 0.0022 * Archive;
float3 c = SceneTextureLookup(uv + weave, 14, false).rgb;
// Present: heat shimmer, strongest low in the frame.
float shimmer = Heat * (1.0 - Archive);
if (shimmer > 0.001) {
  float2 off = float2(vnoise(float2(uv.x * 40.0, uv.y * 90.0 - Time * 2.2)) - 0.5, 0.0) * 0.004 * shimmer * smoothstep(0.35, 0.9, uv.y);
  c = SceneTextureLookup(uv + off, 14, false).rgb;
}
// Weather, drawn into the picture before the grade so the archive look takes it too.
if (Rain > 0.001) {
  float2 ruv = float2(uv.x * 1.7778 + uv.y * 0.22, uv.y);
  float streaks = 0.0;
  for (int L = 0; L < 3; L++) {
    float cols = 46.0 + 38.0 * L;
    float colId = floor(ruv.x * cols);
    float h = H2(float2(colId, 3.1 + L * 7.3));
    float y = frac(ruv.y * (0.7 + 0.45 * L) - Time * (1.5 + 0.9 * h + 0.5 * L) + h * 9.0);
    float fx = abs(frac(ruv.x * cols) - 0.5);
    float line_ = smoothstep(0.07, 0.0, fx) * smoothstep(0.0, 0.03, y) * smoothstep(0.16 + 0.12 * h, 0.03, y);
    streaks += line_ * step(0.45, h) * (0.5 - 0.12 * L);
  }
  c = lerp(c, c * 0.82 + 0.07, 0.45 * Rain);
  c += streaks * Rain * 0.22;
}
if (Sand > 0.001) {
  float gust = vnoise(float2(uv.x * 2.2 - Time * 1.3, uv.y * 34.0)) * vnoise(float2(uv.x * 0.9 - Time * 0.5, uv.y * 5.0 + 3.0));
  float sheet = vnoise(float2(uv.x * 1.4 - Time * 0.8, uv.y * 2.6 + Time * 0.07));
  float amount = saturate(Sand * (0.18 + 1.5 * gust + 0.35 * sheet) * (0.35 + 0.65 * smoothstep(0.15, 0.95, uv.y)));
  float lum0 = dot(c, float3(0.299, 0.587, 0.114));
  c = lerp(c, float3(0.86, 0.6, 0.34) * (0.35 + 0.75 * lum0), amount * 0.75);
}
float luma = dot(c, float3(0.299, 0.587, 0.114));
// Archive: mostly drained colour, warm brown, lifted blacks, soft contrast.
float3 sepia = luma * float3(1.07, 0.93, 0.74);
float cmax = max(c.r, max(c.g, c.b));
// Only purple survives the archive grade (the doc wants "it is purple" to land in 1945), and hot orange glow.
float purple = smoothstep(0.06, 0.26, (min(c.r, c.b) - c.g) / max(cmax, 0.001)) * smoothstep(0.03, 0.15, cmax) * PurpleKeep;
float ember = smoothstep(0.55, 0.9, (c.r - c.b) / max(cmax, 0.001)) * smoothstep(0.85, 1.0, cmax)
              * smoothstep(0.18, 0.32, c.g / max(c.r, 0.001)) * 0.7;   // orange glow stays, pure red (pencil, string) does not
float3 arch = lerp(c, sepia, 0.84 * (1.0 - max(0.85 * purple, ember)));
arch = arch * 0.93 + 0.022;
uint3 gv = uint3(uint(uv.x * 1920.0), uint(uv.y * 1080.0), uint(floor(Time * 30.0)));
gv = gv * 1664525u + 1013904223u;
gv.x += gv.y * gv.z; gv.y += gv.z * gv.x; gv.z += gv.x * gv.y;
gv ^= gv >> 16u;
gv.x += gv.y * gv.z; gv.y += gv.z * gv.x; gv.z += gv.x * gv.y;
float grain = (float(gv.x & 0xffffu) / 65535.0 - 0.5) * 0.075;
arch += grain * (0.6 + 0.4 * (1.0 - luma));
// Scratches: a few thin vertical lines, new places every few film frames.
float st = floor(t / 3.0);
for (int k = 0; k < 3; k++) {
  float sx = H2(float2(st, k * 5.1 + 0.37)) + (H2(float2(t, k + 11.0)) - 0.5) * 0.0016;   // weaves frame to frame
  float on = step(0.8 + 0.06 * k, H2(float2(st * 1.7, k + 9.0)));
  float seg = step(H2(float2(st, k + 3.0)), uv.y + 0.4) * step(uv.y - 0.5, H2(float2(st, k + 4.0)))
              * step(0.35, vnoise(float2(uv.y * 38.0, st * 3.0 + k)));                      // broken, not one hairline
  float w = 0.0005 + 0.0012 * H2(float2(st, k + 6.0));
  arch += on * seg * smoothstep(w, 0.0, abs(uv.x - sx)) * (0.05 + 0.11 * H2(float2(st, k + 7.0)));
}
// Flicker of the projector lamp.
arch *= 1.0 + (H2(float2(t, 4.2)) - 0.5) * 0.05;
float2 d = uv - 0.5;
float vig = 1.0 - smoothstep(0.30, 0.95, dot(d, d) * 2.2);
arch *= lerp(1.0, vig, 0.55);
// Present grade: the game look, a touch of warmth and a light vignette only.
float3 pres = c * float3(1.03, 1.0, 0.96);
pres *= lerp(1.0, vig, 0.22);
float3 o = lerp(pres, arch, Archive);
o *= saturate(1.0 - 0.8 * Darken);
o = lerp(o, float3(1.0, 0.99, 0.96), saturate(Flash));
return float4(o, 1.0);
"""


def build_grade():
    """Post process material: archive grade (1944-1975) and present grade (1976), mixed by MPC_Intro.Archive."""
    material = _new_material("M_Intro_Grade", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -900, -200)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionScreenPosition, -900, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 100)
    inputs = [("Scene", scene, "Color"), ("UV", uv, "ViewportUV"), ("Time", time, "")]
    for index, name in enumerate(("Archive", "Flash", "Darken", "Heat", "Rain", "Sand", "PurpleKeep")):
        inputs.append((name, _collection(material, name, 200 + index * 90), ""))
    custom = _custom(material, GRADE_CODE, inputs)
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return _finish(material)


MAP_CODE = VNOISE + """
float inside = (P.x > B.x && P.x < B.y && P.y > B.z && P.y < B.w) ? 1.0 : 0.0;
float n = vnoise(P.xy * 0.004) * 0.6 + vnoise(P.xy * 0.03) * 0.4;
float3 col = Tint.rgb * (0.86 + 0.2 * n);
return float4(col, inside);
"""


def build_map_materials():
    """Paper map: every country mesh shares one frame (1 unit = 1 km, X north, Y east); the mask cuts the sheet."""
    material = _new_material("M_Intro_Map", blend=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, 0)
    local = mel.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1100, 0)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    bounds = _vector(material, "Bounds", (2600.0, 11800.0, -13900.0, 8900.0), 150)
    tint = _vector(material, "Tint", (0.62, 0.55, 0.40, 1.0), 350)
    # A vector parameter's first output is RGB only; the bounds need all four values.
    bounds_rgba = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -700, 150)
    mel.connect_material_expressions(bounds, "", bounds_rgba, "A")
    mel.connect_material_expressions(bounds, "A", bounds_rgba, "B")
    custom = _custom(material, MAP_CODE, [("P", local, ""), ("B", bounds_rgba, ""), ("Tint", tint, "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_mask(material, custom, "a", y=150), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 300)
    rough.set_editor_property("r", 0.93)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    tones = [(0.64, 0.56, 0.40), (0.58, 0.52, 0.38), (0.68, 0.60, 0.44), (0.60, 0.50, 0.35), (0.66, 0.58, 0.46)]
    for index, tone in enumerate(tones):
        instance("MI_Intro_Map_Land%d" % index, material, vectors={"Tint": tone + (1.0,)})
    # The Reich a shade darker and redder, so the eye finds it.
    instance("MI_Intro_Map_Reich", material, vectors={"Tint": (0.52, 0.36, 0.28, 1.0)})


SIMPLE_CODE = VNOISE + """
float n = vnoise(UV * 60.0) * 0.5 + vnoise(UV * 7.0) * 0.5;
return float4(Tint.rgb * (0.85 + 0.25 * n), 1.0);
"""


def build_simple():
    """M_Intro_Flat: lit, tinted, slightly blotchy surface for greybox pieces (sea sheet, string, pins)."""
    material = _new_material("M_Intro_Flat", two_sided=True)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -100)
    tint = _vector(material, "Tint", (0.5, 0.5, 0.5, 1.0), 100)
    custom = _custom(material, SIMPLE_CODE, [("UV", uv, ""), ("Tint", tint, "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_scalar(material, "Roughness", 0.85, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_scalar(material, "Metallic", 0.0, 400), "", unreal.MaterialProperty.MP_METALLIC)
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 500)
    mel.connect_material_expressions(tint, "", emissive, "A")
    mel.connect_material_expressions(_scalar(material, "Glow", 0.0, 500), "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(material)
    instance("MI_Intro_Sea", material, vectors={"Tint": (0.36, 0.40, 0.38, 1.0)}, scalars={"Roughness": 0.95})
    instance("MI_Intro_String", material, vectors={"Tint": (0.55, 0.04, 0.03, 1.0)}, scalars={"Roughness": 0.8})
    instance("MI_Intro_PinRed", material, vectors={"Tint": (0.60, 0.05, 0.04, 1.0)}, scalars={"Roughness": 0.35})
    instance("MI_Intro_PinBrass", material, vectors={"Tint": (0.55, 0.42, 0.16, 1.0)}, scalars={"Roughness": 0.4, "Metallic": 1.0})
    instance("MI_Intro_InkLine", material, vectors={"Tint": (0.05, 0.04, 0.035, 1.0)}, scalars={"Roughness": 0.9})
    instance("MI_Intro_RedLine", material, vectors={"Tint": (0.50, 0.05, 0.04, 1.0)}, scalars={"Roughness": 0.9})
    instance("MI_Intro_DarkWood", material, vectors={"Tint": (0.10, 0.065, 0.04, 1.0)}, scalars={"Roughness": 0.7})
    instance("MI_Intro_Concrete", material, vectors={"Tint": (0.30, 0.29, 0.27, 1.0)}, scalars={"Roughness": 0.95})
    instance("MI_Intro_Flame", material, vectors={"Tint": (1.0, 0.55, 0.18, 1.0)}, scalars={"Glow": 40.0})


def build_text_material():
    """M_Intro_TextPlane: a texture with alpha on a plane (pencil label, date line); masked and lit."""
    material = _new_material("M_Intro_TextPlane", blend=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    material.set_editor_property("dither_opacity_mask", True)
    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
    texture.set_editor_property("parameter_name", "Texture")
    mel.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    fade = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 200)
    mel.connect_material_expressions(texture, "A", fade, "A")
    reveal = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -450, 350)
    one = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -700, 300)
    one.set_editor_property("r", 1.0)
    mel.connect_material_expressions(one, "", reveal, "A")
    mel.connect_material_expressions(_collection(material, "Pencil", 400), "", reveal, "B")
    mel.connect_material_expressions(_scalar(material, "UsePencil", 0.0, 500), "", reveal, "Alpha")
    mel.connect_material_expressions(reveal, "", fade, "B")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    _finish(material)
    # Masked: burnt paper falls away to ash behind the burn front (Char 0 keeps every pixel: date line, blueprint).
    opaque = _new_material("M_Intro_PaperTex", blend=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    texture = mel.create_material_expression(opaque, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    texture.set_editor_property("parameter_name", "Texture")
    # Char 0..1: the paper browns, then blackens, with a glowing rim while it burns.
    char = _scalar(opaque, "Char", 0.0, 250)
    uv = mel.create_material_expression(opaque, unreal.MaterialExpressionTextureCoordinate, -900, 400)
    code = VNOISE + """
float n = vnoise(UV * 9.0) * 0.6 + vnoise(UV * 31.0) * 0.4;
Char = max(Char, saturate((C42 - Delay) / 2.2));
// The burn starts at the corner nearest the strike and its front sweeps across, ragged, not in spots.
float front = UV.x * 0.75 + (1.0 - UV.y) * 0.25 + (n - 0.5) * 0.35;
float reach = Char * 1.5 - 0.15;
float behind = reach - front;
float scorch = smoothstep(-0.25, 0.0, behind);
float3 col = lerp(Tex, Tex * float3(0.42, 0.27, 0.13), scorch);
col = lerp(col, float3(0.012, 0.01, 0.008), smoothstep(0.0, 0.06, behind));
float rim = saturate(1.0 - abs(behind) * 16.0) * step(0.02, Char) * step(Char, 0.98);
// ash: well behind the front the paper crumbles away in holes
float ash = step(0.3 + 0.5 * vnoise(UV * 17.0 + 3.0) + AshShift, behind);
// long-burnt paper greys to ash where it still holds together
col = lerp(col, float3(0.05, 0.047, 0.043), smoothstep(0.3, 0.9, behind) * 0.6 * step(0.001, AshShift));
// The flat tag gives way to the curled one at the same Char on the same clock: VanishAt >= 0 hides the flat tag
// from that Char on, a negative VanishAt shows the curled tag only from -VanishAt on (spawned for the whole shot,
// so it never pops in late or smears with spawn motion blur).
float gone = VanishAt >= 0.0 ? step(VanishAt, Char) : step(Char, -VanishAt);
return float4(col, rim - (ash + gone) * 2.0);
"""
    custom = _custom(opaque, code, [("Tex", texture, "RGB"), ("Char", char, ""), ("UV", uv, ""),
                                    ("Delay", _scalar(opaque, "Delay", 9999.0, 500), ""),
                                    ("C42", _collection(opaque, "Rings42", 600), ""),
                                    ("VanishAt", _scalar(opaque, "VanishAt", 9.0, 700), ""),
                                    ("AshShift", _scalar(opaque, "AshShift", 0.0, 800), "")])
    mel.connect_material_property(_mask(opaque, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(opaque, unreal.MaterialExpressionMultiply, -150, 300)
    ember = mel.create_material_expression(opaque, unreal.MaterialExpressionConstant3Vector, -400, 400)
    ember.set_editor_property("constant", unreal.LinearColor(3.2, 0.95, 0.05, 1.0))
    rim = mel.create_material_expression(opaque, unreal.MaterialExpressionSaturate, -300, 300)
    mel.connect_material_expressions(_mask(opaque, custom, "a", y=300), "", rim, "")
    mel.connect_material_expressions(rim, "", glow, "A")
    keep = _custom(opaque, "return A < -0.5 ? 0.0 : 1.0;", [("A", _mask(opaque, custom, "a", y=450), "")],
                   output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, y=450)
    mel.connect_material_property(keep, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_expressions(ember, "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(opaque)


RING_CODE = VNOISE + """
float clock = Group < 0.5 ? C36 : (Group < 1.5 ? C42 : C62);
float Burn = saturate((clock - Delay) / max(Duration, 0.01));
float2 p = UV - 0.5;
float r = length(p) * 2.0;
float a = atan2(p.y, p.x);
float n = vnoise(float2(a * 2.2 + 3.0, r * 5.0)) * 0.5 + vnoise(UV * 26.0) * 0.5;
float reach = Burn * 1.15;
float edge = r + (n - 0.5) * 0.36;
float burnt = smoothstep(reach, reach - 0.10, edge);
float hole = smoothstep(0.34 * Burn, 0.20 * Burn, edge);
float ember = saturate(1.0 - abs(edge - reach) / 0.09) * step(0.01, Burn) * (1.0 - smoothstep(0.75, 1.0, Burn));
float3 col = lerp(float3(0.09, 0.06, 0.035), float3(0.012, 0.01, 0.008), saturate(1.0 - edge / max(reach, 0.01)));
col *= 0.7 + 0.6 * vnoise(UV * 60.0);                       // charred paper, not a flat puck
float alpha = saturate(burnt * (0.97 - 0.3 * hole) + ember * 0.8);
return float4(col + float3(2.2, 0.62, 0.03) * ember * (0.6 + 0.4 * n), alpha);
"""


def build_ring_material():
    """M_Intro_BurnRing: a scorch that eats outward from a pin (Burn 0..1), glowing at its edge while it grows."""
    material = _new_material("M_Intro_BurnRing", blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    custom = _custom(material, RING_CODE, [
        ("UV", uv, ""), ("Group", _scalar(material, "Group", 0.0, 200), ""), ("Delay", _scalar(material, "Delay", 0.0, 300), ""),
        ("Duration", _scalar(material, "Duration", 1.6, 400), ""), ("C36", _collection(material, "Rings36", 500), ""),
        ("C42", _collection(material, "Rings42", 600), ""), ("C62", _collection(material, "Rings62", 700), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(_mask(material, custom, "a", y=150), "", unreal.MaterialProperty.MP_OPACITY)
    return _finish(material)


# Seeds in sheet UV (u = east, v = north), set by intro_sets from real places; the stain bleeds from all at once.
STAIN_CODE = VNOISE + PD_VEINS + """
float2 UV = float2(P.y / 100.0 + 0.5, P.x / 100.0 + 0.5);
float2 seeds[9] = { SEEDS };
float n = vnoise(UV * float2(34.0, 14.0)) * 0.55 + vnoise(UV * float2(120.0, 50.0)) * 0.45;
float field = 10.0;
for (int k = 0; k < 9; k++) {
  float2 d = (UV - seeds[k]) * float2(2.48, 1.0);
  float grow = Stain * (0.20 + 0.10 * H2(seeds[k] * 9.0));
  field = min(field, length(d) - grow);
}
float body = smoothstep(0.012, -0.03, field + (n - 0.5) * 0.085);
float veins = smoothstep(0.10, 0.0, abs(field + (n - 0.5) * 0.16)) * 0.45 * step(0.005, Stain);
float wet = vnoise(UV * float2(46.0, 19.0));
float3 col = lerp(float3(0.04, 0.008, 0.13), float3(0.022, 0.005, 0.08), wet);   // bruise-dark, not pastel
// Veins: a branching network (warped Voronoi edges) that reaches out ahead of the front like roots, and shows
// through the body as livid lines. It moves with the front, so it creeps on as the stain grows.
float2 q = UV * float2(64.0, 25.8);
float net = PD.veins(q);
float fine = PD.veins(q * 2.4 + 4.0);
float ahead = saturate(1.0 - max(field, 0.0) / (0.05 + 0.05 * Stain)) * step(0.0, field + 0.02);
float tendril = saturate(net * 1.2 + fine * 0.5 * ahead) * ahead * ahead * (1.0 - body) * step(0.005, Stain);
col = lerp(col, float3(0.03, 0.005, 0.06), net * 0.6 * body);   // veins through the body darker, not a pale veil
col = lerp(col, float3(0.05, 0.008, 0.09), tendril);
float alpha = saturate(body * 0.86 + veins + tendril * 0.95) * Opacity * step(0.002, Stain);   // coastlines show through
return float4(col, alpha);
"""


def build_stain_material(seed_uvs):
    """M_Intro_Stain: the Purple Death bleeding over the map from several points at once (MPC Stain 0..1)."""
    material = _new_material("M_Intro_Stain", blend=unreal.BlendMode.BLEND_TRANSLUCENT, two_sided=True)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    seeds = ", ".join("float2(%.4f, %.4f)" % uv for uv in seed_uvs[:9])
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, 0)
    local = mel.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1100, 0)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    custom = _custom(material, STAIN_CODE.replace("SEEDS", seeds),
                     [("P", local, ""), ("Stain", _collection(material, "Stain", 150), ""),
                      ("Opacity", _collection(material, "StainOpacity", 300), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_mask(material, custom, "a", y=150), "", unreal.MaterialProperty.MP_OPACITY)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 300)
    rough.set_editor_property("r", 0.18)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return _finish(material)


SKY_CODE = """
float v = saturate(UV.y);
float3 c = lerp(Top.rgb, Horizon.rgb, pow(v, Power));
float2 d = UV - float2(SunU, 0.93);
c += Horizon.rgb * 2.5 * exp(-dot(d, d) * 26.0) * Sun;
return float4(c * Bright, 1.0);
"""

EARTH_CODE = VNOISE + """
float2 q = UV * float2(150.0, 75.0);
float land = smoothstep(0.52, 0.60, vnoise(q) * 0.6 + vnoise(q * 3.1) * 0.4);
float cloud = smoothstep(0.50, 0.72, vnoise(q * 2.3 + 7.0) * 0.55 + vnoise(q * 7.0) * 0.45);
float3 sea = float3(0.02, 0.07, 0.16);
float3 ground = lerp(float3(0.10, 0.12, 0.06), float3(0.20, 0.17, 0.11), vnoise(q * 5.0));
float3 col = lerp(lerp(sea, ground, land), float3(0.82, 0.84, 0.86), cloud * 0.85);
return float4(col, 1.0);
"""


def build_sky_materials():
    """M_Intro_Sky: unlit gradient backdrop (dawn behind the skyline, night behind the clouds).
    M_Intro_Earth: the planet below the rocket (procedural seas, land and cloud, blue rim)."""
    sky = _new_material("M_Intro_Sky", shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = mel.create_material_expression(sky, unreal.MaterialExpressionTextureCoordinate, -900, -100)
    custom = _custom(sky, SKY_CODE, [
        ("UV", uv, ""), ("Top", _vector(sky, "Top", (0.05, 0.08, 0.16, 1.0), 0), ""),
        ("Horizon", _vector(sky, "Horizon", (1.0, 0.62, 0.30, 1.0), 200), ""), ("Power", _scalar(sky, "Power", 2.2, 400), ""),
        ("SunU", _scalar(sky, "SunU", 0.5, 500), ""), ("Sun", _scalar(sky, "Sun", 1.0, 600), ""),
        ("Bright", _scalar(sky, "Bright", 1.0, 700), "")])
    mel.connect_material_property(_mask(sky, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(sky)
    instance("MI_Intro_Sky_Dawn", sky, vectors={"Top": (0.10, 0.13, 0.22, 1.0), "Horizon": (1.0, 0.70, 0.42, 1.0)},
             scalars={"Power": 1.6, "Bright": 2.4})
    instance("MI_Intro_Sky_Night", sky, vectors={"Top": (0.004, 0.005, 0.012, 1.0), "Horizon": (0.03, 0.02, 0.06, 1.0)},
             scalars={"Power": 1.0, "Sun": 0.0, "Bright": 1.0})
    instance("MI_Intro_Sky_Space", sky, vectors={"Top": (0.0, 0.0, 0.002, 1.0), "Horizon": (0.0, 0.002, 0.008, 1.0)},
             scalars={"Sun": 0.0})
    instance("MI_Intro_Sky_Ash", sky, vectors={"Top": (0.20, 0.19, 0.18, 1.0), "Horizon": (0.52, 0.48, 0.42, 1.0)},
             scalars={"Power": 1.2, "Sun": 0.3, "Bright": 1.6})
    earth = _new_material("M_Intro_Earth")
    uv = mel.create_material_expression(earth, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    custom = _custom(earth, EARTH_CODE, [("UV", uv, "")])
    mel.connect_material_property(_mask(earth, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    fresnel = mel.create_material_expression(earth, unreal.MaterialExpressionFresnel, -500, 300)
    fresnel.set_editor_property("exponent", 3.0)
    glow = mel.create_material_expression(earth, unreal.MaterialExpressionMultiply, -250, 300)
    blue = mel.create_material_expression(earth, unreal.MaterialExpressionConstant3Vector, -500, 450)
    blue.set_editor_property("constant", unreal.LinearColor(0.25, 0.55, 1.6, 1.0))
    mel.connect_material_expressions(fresnel, "", glow, "A")
    mel.connect_material_expressions(blue, "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(earth)


EARTH_VIEW_CODE = VNOISE + """
// The planet seen from the rocket, drawn on the sky dome: ray against a sphere, so the limb is a true curve.
float3 ro = Cam - Centre.xyz;
float3 rd = -normalize(CamVec);
float b = dot(ro, rd);
float c = dot(ro, ro) - Radius * Radius;
float h = b * b - c;
float3 L = normalize(SunDir.xyz);
float3 blue = float3(0.25, 0.5, 1.0);
if (h < 0.0 || b > 0.0) {
  float d = sqrt(max(dot(ro, ro) - b * b, 0.0));
  float alt = max(d - Radius, 0.0) / (Radius * 0.012);
  float halo = exp(-alt * 3.0) * step(b, 0.0);
  float2 sq = floor(rd.xy / max(abs(rd.z) + 0.35, 0.35) * 420.0);
  float star = step(0.9975, H2(sq)) * H2(sq + 7.0);
  return float4(blue * halo * 0.9 + star * 0.5, 1.0);
}
float t = -b - sqrt(h);
float3 n = (ro + rd * t) / Radius;
float2 q = n.xy * 30.0 + float2(Time * 0.004, 0.0);
float land = 0.0; float amp = 0.5; float2 qq = q * 0.8 + 3.0;
for (int i = 0; i < 5; i++) { land += amp * vnoise(qq); qq = qq * 2.03 + 17.0; amp *= 0.5; }
float warp = vnoise(q * 1.3 + 40.0);
float cloud = 0.0; amp = 0.5; qq = q * 2.1 + warp * 2.5 + float2(Time * 0.01, 9.0);
for (int j = 0; j < 6; j++) { cloud += amp * vnoise(qq); qq = qq * 2.07 + 31.0; amp *= 0.5; }
float wisp = 0.0; amp = 0.5; qq = q * float2(7.0, 2.4) + 70.0;
for (int k = 0; k < 4; k++) { wisp += amp * vnoise(qq); qq = qq * 2.1 + 5.0; amp *= 0.5; }
float isLand = smoothstep(0.50, 0.55, land);
float3 surface = lerp(float3(0.015, 0.05, 0.12), lerp(float3(0.07, 0.09, 0.05), float3(0.2, 0.2, 0.19), smoothstep(0.55, 0.8, land)), isLand);
float cover = saturate(smoothstep(0.42, 0.72, cloud) + smoothstep(0.55, 0.8, wisp) * 0.5);
float3 col = lerp(surface, float3(0.9, 0.92, 0.95), cover);
float diff = saturate(dot(n, L) * 1.25 + 0.12);
float rim = pow(1.0 - saturate(dot(n, -rd)), 4.0);
col = col * diff + blue * rim * 1.4 * saturate(diff + 0.2);
return float4(col * Bright, 1.0);
"""


def build_earth_view():
    material = _new_material("M_Intro_EarthView", shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    cam = mel.create_material_expression(material, unreal.MaterialExpressionCameraPositionWS, -900, -300)
    vec = mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -900, -200)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, -100)
    custom = _custom(material, EARTH_VIEW_CODE, [
        ("Cam", cam, ""), ("CamVec", vec, ""), ("Time", time, ""),
        ("Centre", _vector(material, "Centre", (0.0, 0.0, -630000.0, 0.0), 0), ""),
        ("SunDir", _vector(material, "SunDir", (-0.73, 0.61, 0.31, 0.0), 200), ""),
        ("Radius", _scalar(material, "Radius", 600000.0, 400), ""), ("Bright", _scalar(material, "Bright", 1.6, 500), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return _finish(material)


CLOUD_CODE = VNOISE + """
// One sheet of night cloud: soft billows, holes between them, lit violet from inside where it is thickest.
float2 q = UV * Scale + float2(Time * Speed, Seed);
float d = 0.0; float amp = 0.5; float2 qq = q;
for (int i = 0; i < 6; i++) { d += amp * vnoise(qq); qq = qq * 2.03 + 11.0; amp *= 0.5; }
float core = 0.0; amp = 0.5; qq = q * 0.45 + 40.0 + Seed;
for (int j = 0; j < 4; j++) { core += amp * vnoise(qq); qq = qq * 2.1 + 3.0; amp *= 0.5; }
float2 e = abs(UV - 0.5) * 2.0;
float edge = saturate(1.0 - max(e.x, e.y)) * 4.0;
float alpha = smoothstep(Cover, Cover + 0.22, d) * saturate(edge) * Opacity;
float glow = smoothstep(0.45, 0.8, core) * smoothstep(Cover + 0.05, Cover + 0.4, d);
float3 col = lerp(float3(0.012, 0.008, 0.03), float3(0.10, 0.04, 0.20), saturate(d * 1.4 - 0.3));
col += float3(0.75, 0.16, 1.5) * glow * Glow;
return float4(col, alpha);
"""


def build_cloud_material():
    material = _new_material("M_Intro_CloudLayer", blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -200)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, -100)
    custom = _custom(material, CLOUD_CODE, [
        ("UV", uv, ""), ("Time", time, ""), ("Scale", _scalar(material, "Scale", 4.0, 0), ""),
        ("Speed", _scalar(material, "Speed", 0.01, 100), ""), ("Seed", _scalar(material, "Seed", 0.0, 200), ""),
        ("Cover", _scalar(material, "Cover", 0.42, 300), ""), ("Opacity", _scalar(material, "Opacity", 1.0, 400), ""),
        ("Glow", _scalar(material, "Glow", 1.0, 500), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(_mask(material, custom, "a"), "", unreal.MaterialProperty.MP_OPACITY)
    _finish(material)
    for index, (scale, speed, cover, glow) in enumerate(((5.5, 0.008, 0.40, 1.5), (8.0, 0.014, 0.46, 0.9), (11.0, 0.026, 0.5, 0.3),
                                                         (15.0, 0.045, 0.56, 0.0))):
        instance("MI_Intro_Cloud_%d" % index, material, scalars={"Scale": scale, "Speed": speed, "Seed": index * 13.7,
                                                                  "Cover": cover, "Glow": glow})


def build_ground_material():
    """M_Intro_Ground: world-aligned soil for the big flat sets (no UVs needed, a second large sample breaks the tiling)."""
    folder = "/Game/Megascans/Surfaces/Arid_Soil_Ground_wihhfexbw/"
    material = _new_material("M_Intro_Ground")
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, 0)
    xy = _mask(material, world, "rg", -1150, 0)

    def sample(tile, texture, sampler, y):
        scaled = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -1000, y)
        mel.connect_material_expressions(xy, "", scaled, "A")
        scaled.set_editor_property("const_b", 1.0 / tile)
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -800, y)
        node.set_editor_property("texture", unreal.load_asset(folder + texture))
        node.set_editor_property("sampler_type", sampler)
        mel.connect_material_expressions(scaled, "", node, "UVs")
        return node

    colour = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    near = sample(2300.0, "T_Arid_Soil_Ground_wihhfexbw_2K_D", colour, -200)
    # The same soil again, turned and at another size: no straight rows at a grazing angle.
    turned = _custom(material, "return float2(P.x * 0.8 - P.y * 0.6, P.x * 0.6 + P.y * 0.8) / 1370.0;", [("P", xy, "")],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT2, -1000, -400)
    near2 = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -800, -400)
    near2.set_editor_property("texture", unreal.load_asset(folder + "T_Arid_Soil_Ground_wihhfexbw_2K_D"))
    mel.connect_material_expressions(turned, "", near2, "UVs")
    far = sample(17000.0, "T_Arid_Soil_Ground_wihhfexbw_2K_D", colour, 0)
    vast = sample(210000.0, "T_Arid_Soil_Ground_wihhfexbw_2K_D", colour, 200)
    normal = sample(900.0, "T_Arid_Soil_Ground_wihhfexbw_2K_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 400)
    code = """
float macro = lerp(0.55, 1.35, saturate(Far.r * 2.2)) * lerp(0.7, 1.25, saturate(Vast.g * 2.4));
return float4(lerp(Near.rgb, Near2.rgb, 0.5) * macro * Tint.rgb, 1.0);
"""
    custom = _custom(material, code, [("Near", near, "RGB"), ("Near2", near2, "RGB"), ("Far", far, "RGB"), ("Vast", vast, "RGB"),
                                      ("Tint", _vector(material, "Tint", (1.0, 1.0, 1.0, 1.0), 600), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 300)
    rough.set_editor_property("r", 0.92)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    instance("MI_Intro_Ground_Ural", material, vectors={"Tint": (1.7, 1.55, 1.4, 1.0)})


NOTICE_CODE = VNOISE + """
float2 p = (UV - 0.5) * float2(1.7778, 1.0);
float n = vnoise(p * 7.0) * 0.75 + vnoise(p * 23.0) * 0.25;
float edge = length(p) + (n - 0.5) * 0.34;
float radius = Burn * 1.5 - 0.2;
float hole = smoothstep(radius - 0.012, radius + 0.012, edge);
float rim = saturate(1.0 - abs(edge - radius) / 0.055) * step(0.001, Burn);
float scorch = saturate(1.0 - (edge - radius) / 0.16) * hole;
float3 glow = float3(1.0, 0.42, 0.08) * rim * 3.0 + float3(0.10, 0.045, 0.02) * scorch;
return float4(glow, saturate(max(hole, rim)));
"""


def build_notice_material():
    """UI/M_Intro_NoticeBurn: the black behind the archival notice; Burn 0..1 eats a hole outward from the centre
    with a glowing rim, like film catching on a projector lamp. The HUD widget sets Burn."""
    path = lib.ROOT + "/UI/M_Intro_NoticeBurn"
    material = unreal.load_asset(path)
    if material is None:
        material = tools.create_asset("M_Intro_NoticeBurn", lib.ROOT + "/UI", unreal.Material, unreal.MaterialFactoryNew())
    else:
        mel.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    custom = _custom(material, NOTICE_CODE, [("UV", uv, ""), ("Burn", _scalar(material, "Burn", 0.0, 200), "")])
    mel.connect_material_property(_mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(_mask(material, custom, "a", y=150), "", unreal.MaterialProperty.MP_OPACITY)
    return _finish(material)


def import_textures():
    """Imports Saved/IntroCinematic/tex/*.png into <ROOT>/Textures."""
    import glob
    tasks = []
    for path in glob.glob("D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/tex/*.png"):
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = lib.ROOT + "/Textures"
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    packages = []
    for task in tasks:
        for object_path in task.imported_object_paths:
            packages.append(unreal.load_asset(str(object_path).split(".")[0]).get_outermost())
    lib.safe_save(packages)
    print("import_textures", len(packages))


def build_all(seed_uvs):
    build_mpc()
    build_grade()
    build_map_materials()
    build_simple()
    build_text_material()
    build_ring_material()
    build_stain_material(seed_uvs)
    build_sky_materials()
    extras()


def build_contact_shadow():
    """M_Intro_ContactShadow: a soft translucent black blob on the ground under a tank (a plane just above the
    ground under the hull). Far from the camera the sun's shadow map leaves a lit strip between the tracks and their
    shadow, so the tanks read as hovering; this grounds them, darkest under the belly and along the track lines."""
    material = _new_material("M_Intro_ContactShadow", blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    code = """
float2 p = abs(UV - 0.5) * 2.0;
// dark right out to near the edge: the lit strip the shadow-map bias leaves lies in the outer band, between
// the tracks and the (pushed out) real shadow
return Dark * (1.0 - smoothstep(0.78, 1.0, p.x)) * (1.0 - smoothstep(0.8, 1.0, p.y));
"""
    custom = _custom(material, code, [("UV", uv, ""), ("Dark", _scalar(material, "Dark", 0.66, 200), "")],
                     output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    black = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, -200)
    black.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(black, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_OPACITY)
    _finish(material)


def extras():
    """Material instances that only need textures or tints."""
    T = lib.ROOT + "/Textures/"
    text = unreal.load_asset(MAT_DIR + "/M_Intro_TextPlane")
    paper = unreal.load_asset(MAT_DIR + "/M_Intro_PaperTex")
    flat = unreal.load_asset(MAT_DIR + "/M_Intro_Flat")
    instance("MI_Intro_PencilPurpleDeath", text, textures={"Texture": unreal.load_asset(T + "T_Intro_PencilPurpleDeath")},
             scalars={"UsePencil": 1.0})
    instance("MI_Intro_DateLine", paper, textures={"Texture": unreal.load_asset(T + "T_Intro_DateStrip")})
    instance("MI_Intro_Blueprint", paper, textures={"Texture": unreal.load_asset(T + "T_Intro_Blueprint")})
    for index, name in enumerate(("LosAlamos", "OakRidge", "Hanford")):
        # flat tag: vanishes at Char 0.545 (1.2 s into its burn); the curled tag (same paper) takes over then
        instance("MI_Intro_Tag_" + name, paper, textures={"Texture": unreal.load_asset(T + "T_Intro_Tag_" + name)},
                 scalars={"Delay": 3.6 + index * 1.7, "VanishAt": 0.545})
        # curled tag: shown from Char 0.545 on; less ash so the curl still reads once it has burnt through
        instance("MI_Intro_TagCurled_" + name, paper, textures={"Texture": unreal.load_asset(T + "T_Intro_Tag_" + name)},
                 scalars={"Delay": 3.6 + index * 1.7, "VanishAt": -0.545, "AshShift": 0.45})
    instance("MI_Intro_Dirt", flat, vectors={"Tint": (0.075, 0.055, 0.04, 1.0)}, scalars={"Roughness": 0.95})
    instance("MI_Intro_Mould", flat, vectors={"Tint": (0.16, 0.02, 0.22, 1.0)}, scalars={"Roughness": 0.22})
    instance("MI_Intro_DarkStone", flat, vectors={"Tint": (0.05, 0.05, 0.055, 1.0)}, scalars={"Roughness": 0.9})
    instance("MI_Intro_Silhouette", flat, vectors={"Tint": (0.004, 0.004, 0.005, 1.0)}, scalars={"Roughness": 1.0})
    instance("MI_Intro_Steel", flat, vectors={"Tint": (0.20, 0.21, 0.22, 1.0)}, scalars={"Roughness": 0.45, "Metallic": 1.0})
    instance("MI_Intro_Gilt", flat, vectors={"Tint": (0.30, 0.22, 0.08, 1.0)}, scalars={"Roughness": 0.5, "Metallic": 0.8})
    instance("MI_Intro_BannerRed", flat, vectors={"Tint": (0.33, 0.03, 0.025, 1.0)}, scalars={"Roughness": 0.85})
