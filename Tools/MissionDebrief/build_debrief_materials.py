"""Headless: builds the debrief stage's hand-made materials.

- M_Debrief_Paper: the order sheet with stamped ink (relief, satin wet-to-dry) and the burn (edge-in dissolve, char,
  scorch, glowing ember band; the stamped letters can be spared by a keep mask). Driven by MID scalars from C++.
- M_Debrief_Soot: unlit dark layer under the burning paper, opacity driven from C++.
- M_Debrief_SpriteAdd / M_Debrief_SpriteAlpha: flame/ember and smoke sprites on instanced quads; colour and alpha come
  from per-instance custom data 0..3.
- M_Debrief_MP44: the game's StG 44 with all of its PBR maps (the in-game material only uses albedo).
- M_Debrief_Black: rough black for the closed room around the stage.
Run: UnrealEditor-Cmd ... -run=pythonscript -script=<this>
"""
import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Missions/MissionDebrief"
MAT_DIR = ROOT + "/Materials"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()


def log(*a):
    unreal.log("DEBRIEF_MAT " + " ".join(str(x) for x in a))


def fresh_material(name):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def scalar(mat, name, val, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", val)
    return e


def vector(mat, name, rgb, x, y):
    e = expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def tex_obj(mat, name, path, x, y):
    e = expr(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    e.set_editor_property("parameter_name", name)
    t = unreal.load_asset(path)
    if t is None:
        raise RuntimeError("missing texture " + path)
    e.set_editor_property("texture", t)
    return e


def custom(mat, code, inputs, out_type, extra_outputs, x, y):
    c = expr(mat, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    outs = []
    for n, t in extra_outputs:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", n)
        co.set_editor_property("output_type", t)
        outs.append(co)
    c.set_editor_property("additional_outputs", outs)
    return c


def connect(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log("CONNECT FAILED", a.get_name(), a_out, "->", b.get_name(), b_in)
    return ok


def prop(a, a_out, mat, p):
    ok = MEL.connect_material_property(a, a_out, p)
    if not ok:
        log("PROPERTY CONNECT FAILED", a.get_name(), a_out, p)


CMOT = unreal.CustomMaterialOutputType
PAPER_CODE = r"""
float2 S = float2(2.1, 2.97);
float2 bp = UV * S;
float n1 = Texture2DSample(NoiseTex, NoiseTexSampler, float2(UV.x * 1.3, 1.0 - UV.y * 1.3)).r;
float n2 = Texture2DSample(NoiseTex, NoiseTexSampler, float2(UV.x * 4.1 + 0.37, 1.0 - (UV.y * 4.1 + 0.37))).r;
float fd = saturate(min(min(bp.x, S.x - bp.x), min(bp.y, S.y - bp.y)) / (S.x * 0.5));
float keepV = Texture2DSample(KeepTex, KeepTexSampler, UV).r;
float f = fd + (n1 - 0.5) * 0.34 + (n2 - 0.5) * 0.09 + smoothstep(0.3, 0.62, keepV) * KeepOn * 1.6;
float edge = f - Burn;
Opacity = edge >= 0.0 ? 1.0 : 0.0;
float3 col = Texture2DSample(PaperTex, PaperTexSampler, UV).rgb;
float inkA = Texture2DSample(InkTex, InkTexSampler, UV).a;
float ink = smoothstep(0.08, 0.55, inkA) * InkAmount;
float inkN = Texture2DSample(NoiseTex, NoiseTexSampler, float2(UV.x * 7.0 + 0.2, 1.0 - (UV.y * 7.0 + 0.2))).r;
float3 inkC = InkColor * (0.5 + 0.4 * inkN);
col = lerp(col, inkC, ink * 0.95);
float scorch = 1.0 - smoothstep(0.0, 0.2, edge);
col = lerp(col, col * float3(0.5, 0.33, 0.18), scorch * 0.9);
float crawl = Texture2DSample(NoiseTex, NoiseTexSampler, float2(UV.x * 5.0 + Time * 0.04, 1.0 - (UV.y * 5.0 - Time * 0.09))).r;
float charT = 1.0 - smoothstep(0.0, lerp(0.06, 0.12, KeepOn), edge);
col = lerp(col, float3(0.03, 0.022, 0.018), charT);
float emberW = lerp(0.028, 0.04 + crawl * 0.06, KeepOn);
float emberT = 1.0 - smoothstep(0.0, emberW, edge);
float flick = max(0.25, 0.75 + 0.35 * sin(Time * 11.0 + n2 * 40.0) * sin(Time * 7.3 + n1 * 25.0));
float3 hot = lerp(float3(1.0, 0.2, 0.03), float3(1.0, 0.78, 0.4), pow(emberT, 3.0));
Emissive = hot * emberT * flick * lerp(4.5, 9.0, KeepOn) * (0.6 + 0.8 * crawl) * EmberGain * (Burn > -0.5 ? 1.0 : 0.0);
Roughness = lerp(0.93, InkRough + 0.12 * n2, ink);
float2 px = float2(1.0 / 1024.0, 1.0 / 1448.0);
float hL = Texture2DSample(InkHeightTex, InkHeightTexSampler, UV - float2(px.x, 0)).r;
float hR = Texture2DSample(InkHeightTex, InkHeightTexSampler, UV + float2(px.x, 0)).r;
float hD = Texture2DSample(InkHeightTex, InkHeightTexSampler, UV - float2(0, px.y)).r;
float hU = Texture2DSample(InkHeightTex, InkHeightTexSampler, UV + float2(0, px.y)).r;
float k = BumpStrength * InkAmount;
TangentNormal = normalize(float3(-(hR - hL) * k, -(hU - hD) * k * BumpSignY, 1.0));
return col;
"""


def build_paper():
    m = fresh_material("M_Debrief_Paper")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tp = {
        "PaperTex": ROOT + "/Imported/SM_Debrief_Paper/paper_texture_0",
        "InkTex": ROOT + "/Textures/T_Debrief_Ink_Victory",
        "InkHeightTex": ROOT + "/Textures/T_Debrief_InkHeight_Victory",
        "KeepTex": ROOT + "/Textures/T_Debrief_Keep_Defeat",
        "NoiseTex": ROOT + "/Textures/T_Debrief_Noise",
    }
    texs = {k: tex_obj(m, k, v, -900, 100 + i * 120) for i, (k, v) in enumerate(tp.items())}
    texs["NoiseTex"].get_editor_property("texture").set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    sc = {k: scalar(m, k, v, -900, 800 + i * 70) for i, (k, v) in enumerate(
        [("InkAmount", 0.0), ("InkRough", 0.34), ("Burn", -1.0), ("KeepOn", 0.0), ("Time", 0.0), ("EmberGain", 1.0),
         ("BumpStrength", 9.0), ("BumpSignY", 1.0)])}
    ic = vector(m, "InkColor", (0.155, 0.006, 0.003), -900, 1450)
    inputs = ["UV", "PaperTex", "InkTex", "InkHeightTex", "KeepTex", "NoiseTex", "InkAmount", "InkRough", "Burn", "KeepOn",
              "Time", "EmberGain", "BumpStrength", "BumpSignY", "InkColor"]
    c = custom(m, PAPER_CODE, inputs, CMOT.CMOT_FLOAT3,
               [("Opacity", CMOT.CMOT_FLOAT1), ("Emissive", CMOT.CMOT_FLOAT3), ("Roughness", CMOT.CMOT_FLOAT1),
                ("TangentNormal", CMOT.CMOT_FLOAT3)], -300, 300)
    connect(uv, "", c, "UV")
    for k, e in texs.items():
        connect(e, "", c, k)
    for k, e in sc.items():
        connect(e, "", c, k)
    connect(ic, "", c, "InkColor")
    prop(c, "", m, unreal.MaterialProperty.MP_BASE_COLOR)
    prop(c, "Opacity", m, unreal.MaterialProperty.MP_OPACITY_MASK)
    prop(c, "Emissive", m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    prop(c, "Roughness", m, unreal.MaterialProperty.MP_ROUGHNESS)
    prop(c, "TangentNormal", m, unreal.MaterialProperty.MP_NORMAL)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_Debrief_Paper built")


def build_soot():
    m = fresh_material("M_Debrief_Soot")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
    t.set_editor_property("parameter_name", "SootAlpha")
    t.set_editor_property("texture", unreal.load_asset(ROOT + "/Textures/T_Debrief_SootAlpha"))
    t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    op = scalar(m, "Opacity", 0.0, -600, 250)
    mul = expr(m, unreal.MaterialExpressionMultiply, -300, 100)
    connect(t, "R", mul, "A"); connect(op, "", mul, "B")
    col = expr(m, unreal.MaterialExpressionConstant3Vector, -300, -100)
    col.set_editor_property("constant", unreal.LinearColor(0.0009, 0.0006, 0.0005, 1))
    prop(col, "", m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    prop(mul, "", m, unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m); EAL.save_loaded_asset(m); log("M_Debrief_Soot built")


def build_sprite(name, additive, tex_path):
    m = fresh_material(name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    MEL.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    t.set_editor_property("parameter_name", "Sprite")
    t.set_editor_property("texture", unreal.load_asset(tex_path))
    pcd = []
    for i in range(4):
        e = expr(m, unreal.MaterialExpressionPerInstanceCustomData, -700, 260 + i * 90)
        e.set_editor_property("data_index", i)
        pcd.append(e)
    a = expr(m, unreal.MaterialExpressionAppendVector, -450, 300); connect(pcd[0], "", a, "A"); connect(pcd[1], "", a, "B")
    b = expr(m, unreal.MaterialExpressionAppendVector, -300, 300); connect(a, "", b, "A"); connect(pcd[2], "", b, "B")
    em = expr(m, unreal.MaterialExpressionMultiply, -150, 100); connect(t, "RGB", em, "A"); connect(b, "", em, "B")
    gain = scalar(m, "Gain", 1.0, -300, 450)
    em2 = expr(m, unreal.MaterialExpressionMultiply, 0, 100); connect(em, "", em2, "A"); connect(gain, "", em2, "B")
    op = expr(m, unreal.MaterialExpressionMultiply, -150, 400); connect(t, "A", op, "A"); connect(pcd[3], "", op, "B")
    if additive:
        em3 = expr(m, unreal.MaterialExpressionMultiply, 150, 100); connect(em2, "", em3, "A"); connect(op, "", em3, "B")
        prop(em3, "", m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        prop(em2, "", m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        prop(op, "", m, unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m); EAL.save_loaded_asset(m); log(name, "built")


def build_mp44():
    base = "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/STG44/Meshes/"
    m = fresh_material("M_Debrief_MP44")

    def ts(name, x, y, st=None):
        t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        t.set_editor_property("parameter_name", name)
        t.set_editor_property("texture", unreal.load_asset(base + name))
        if st is not None:
            t.set_editor_property("sampler_type", st)
        return t

    al = ts("MP44_albedo", -600, -200)
    nm = ts("MP44_normal", -600, 50, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    ro = ts("MP44_roughness", -600, 300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    me = ts("MP44_metalness", -600, 550, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    ao = ts("MP44_ao", -600, 800, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    prop(al, "RGB", m, unreal.MaterialProperty.MP_BASE_COLOR)
    prop(nm, "RGB", m, unreal.MaterialProperty.MP_NORMAL)
    prop(ro, "R", m, unreal.MaterialProperty.MP_ROUGHNESS)
    prop(me, "R", m, unreal.MaterialProperty.MP_METALLIC)
    prop(ao, "R", m, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.recompile_material(m); EAL.save_loaded_asset(m); log("M_Debrief_MP44 built")


def build_black():
    m = fresh_material("M_Debrief_Black")
    c = expr(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
    c.set_editor_property("constant", unreal.LinearColor(0.004, 0.004, 0.004, 1))
    r = expr(m, unreal.MaterialExpressionConstant, -300, 150)
    r.set_editor_property("r", 1.0)
    prop(c, "", m, unreal.MaterialProperty.MP_BASE_COLOR)
    prop(r, "", m, unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m); EAL.save_loaded_asset(m); log("M_Debrief_Black built")


for fn in (build_paper, build_soot, lambda: build_sprite("M_Debrief_SpriteAdd", True, ROOT + "/Textures/T_Debrief_Sprite_Flame"),
           lambda: build_sprite("M_Debrief_SpriteAlpha", False, ROOT + "/Textures/T_Debrief_Sprite_Smoke"), build_mp44, build_black):
    try:
        fn()
    except Exception as e:  # noqa
        log("ERROR", e)
noise = unreal.load_asset(ROOT + "/Textures/T_Debrief_Noise")
noise.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP); noise.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
EAL.save_loaded_asset(noise)
log("done")
