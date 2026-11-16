"""Fire on the map table's strike markers (3.6 Germany, 4.2 USA, 6.2b Operation Svarog): each burn ring catches
fire when its clock reaches it, flames and sparks climb from the scorch, smoke rises, a flickering light pool lies on
the paper, and the city pin glows red-hot and chars black. Everything runs off the burn-ring clocks
(MPC Rings36 / Rings42 / Rings62 plus each ring's Delay and Duration), so nothing new is keyed in Sequencer.

Lifecycle of one strike (t = clock - Delay, D = Duration of the scorch):
  0 .. 0.12 s     flames shoot up (taller for a moment: the strike itself)
  .. D + 0.6 s    full fire while the scorch eats outward
  .. D + 3.4 s    the fire dies down; sparks until D + 6 s
  .. D + 7.5 s    the last smoke thins out; after that only the scorch and the charred pin remain.
The clocks are sticky across shots and are parked at 60 s where a map shot must show no fire (see the manifest).

Colours: the archive grade (M_Intro_Grade) keeps only bright, blue-free orange; flame colours stay inside that window
(blue near zero, red above 2 before tone mapping) so the fire keeps its colour in the drained 1945 look.
Run inside the throwaway editor.
"""
import importlib

import unreal

import intro_lib as lib
import intro_materials as mats

importlib.reload(mats)

mel = unreal.MaterialEditingLibrary
M = mats.MAT_DIR + "/"

LIFE = """
float clock = Group < 0.5 ? C36 : (Group < 1.5 ? C42 : C62);
float t = clock - Delay;
float D = Duration;
float on = step(0.0, t);
float fire = on * smoothstep(0.0, 0.12, t) * (1.0 - smoothstep(D + 0.6, D + 3.4, t));
float flare = 1.0 + 0.9 * exp(-max(t, 0.0) * 3.0) * on;
float smoke = on * smoothstep(0.15, 1.0, t) * (1.0 - smoothstep(D + 3.0, D + 7.5, t));
float sparks = on * smoothstep(0.05, 0.4, t) * (1.0 - smoothstep(D + 2.0, D + 6.0, t));
float Burn = saturate(t / max(D, 0.01));
"""

# Two crossed upright cards per fire (one faces the +X cameras, one the -Y / +Y ones); the python material API cannot
# reach World Position Offset, so there is no billboard. Each card fades out where it is seen edge-on.
CARD_COORDS = """
float2 UV = float2(LP.x / 100.0 + 0.5, (WP.z - AP.z) / H + 0.5);
float3 toCam = Cam - WP;
float2 flat = toCam.xy / max(length(toCam.xy), 0.001);
float facing = abs(dot(flat, normalize(N.xy + float2(1e-4, 0.0))));
"""

FLAME_CODE = mats.VNOISE + LIFE + CARD_COORDS + """
float X = (UV.x - 0.5) * W / Size;           // across, in ring sizes
float Y = UV.y * H / Size;                   // up from the paper, in ring sizes
float S = Seed * 17.31;
float tt = Time;
float bw = 0.16 + 0.5 * Burn;                // flame base half-width follows the scorch front
float Hf = (0.75 + 0.45 * Burn) * flare;     // flame height (a strike is a city, not a state)
float yy = Y / max(Hf, 0.05);
// Domain-warped turbulence, advected upward: separate licks that tear off at the top.
float2 q = float2(X * 2.6 + S, Y * 1.5 - tt * 2.6);
float w1 = 0.0; float amp = 0.5; float2 qq = q;
for (int o = 0; o < 4; o++) { w1 += amp * vnoise(qq); qq = qq * 2.03 + float2(1.7, -tt * 0.9); amp *= 0.5; }
float2 r = float2(X * 4.2 - S + w1 * 1.6, Y * 2.4 - tt * 3.8 + w1 * 1.2);
float w2 = 0.0; amp = 0.5;
for (int o2 = 0; o2 < 4; o2++) { w2 += amp * vnoise(r); r = r * 2.07 + float2(-2.3, -tt * 1.3); amp *= 0.5; }
// Envelope: wide and dense at the scorch front, narrowing and thinning with height; the licks sway as they rise.
float sway = (vnoise(float2(Y * 0.8 - tt * 1.1, S + 4.0)) - 0.5) * 0.45 * yy;
float env = saturate(1.0 - abs(X - sway) / max(bw * (1.15 - 0.55 * saturate(yy)), 0.001));
float dens = env * 1.25 - yy * 1.05 + (w2 - 0.5) * (0.9 + 0.9 * yy);
float flame = smoothstep(0.0, 0.22, dens) * smoothstep(0.0, 0.16 + 0.16 * w1, Y) * fire;
float heat = smoothstep(0.05, 0.85, dens) * (1.1 - 0.5 * saturate(yy));
float3 fcol = lerp(float3(0.75, 0.08, 0.0), float3(2.3, 0.72, 0.02), smoothstep(0.08, 0.4, heat));
fcol = lerp(fcol, float3(3.3, 1.95, 0.1), smoothstep(0.5, 0.95, heat));
fcol *= Bright * (0.82 + 0.3 * vnoise(float2(tt * 11.0, S)));
// Sparks: small hot grains carried up past the flame tips, drifting.
float sp = 0.0;
for (int k = 0; k < 2; k++) {
  float2 g = float2(X * 7.0 + k * 13.1 + S, Y * 7.0 - tt * (4.0 + 1.5 * k));
  float2 id = floor(g);
  float2 f = frac(g);
  float h = H2(id + k * 7.3);
  float2 c = float2(0.25 + 0.5 * H2(id + 3.7), 0.25 + 0.5 * H2(id + 9.1));
  c.x += sin(tt * 4.0 + h * 30.0) * 0.15;
  sp += step(0.93, h) * smoothstep(0.11, 0.03, length(f - c));
}
sp *= sparks * smoothstep(Hf * 0.4, Hf * 0.9, Y) * (1.0 - smoothstep(Hf * 1.2, 4.4, Y)) * smoothstep(1.0, 0.2, abs(X));
// Smoke: a dark plume from the flame tips, curling and leaning, under-lit by the fire near its base.
float sy = Y - Hf * 0.45;
float drift = (vnoise(float2(Y * 0.5 - tt * 0.35, S + 7.0)) - 0.5) * Y * 0.35 + Y * 0.06;
float sx = X - drift;
float sw = 0.22 + 0.17 * max(sy, 0.0) + 0.12 * Burn;
float sn = vnoise(float2(sx * 2.4 + S, Y * 1.3 - tt * 0.9)) * 0.6 + vnoise(float2(sx * 5.5 - S, Y * 2.8 - tt * 1.5)) * 0.4;
float plume = smoothstep(sw, sw * 0.1, abs(sx)) * smoothstep(0.0, 0.6, sy) * (1.0 - smoothstep(2.8, H / Size, Y));
float sa = plume * smoothstep(0.25, 0.7, sn) * smoke * 0.85;
float3 scol = float3(0.045, 0.04, 0.036) + float3(0.6, 0.15, 0.01) * fire * (1.0 - smoothstep(Hf * 0.5, Hf * 1.5, Y));
float3 col = fcol * flame + float3(2.6, 0.9, 0.04) * sp + scol * (1.0 - flame);
float a = saturate(flame * 0.95 + sa * (1.0 - flame) + saturate(sp)) * smoothstep(0.2, 0.55, facing);
return float4(col, a);
"""

GLOW_CODE = mats.VNOISE + LIFE + """
float2 p = (UV - 0.5) * 2.0;
float r2 = dot(p, p);
float S = Seed * 17.31;
float flick = 0.72 + 0.22 * vnoise(float2(Time * 7.0, S)) + 0.12 * vnoise(float2(Time * 19.0, S + 3.0));
float pool = exp(-r2 * 4.5) * (1.0 - smoothstep(0.75, 1.0, sqrt(r2)));
return float4(float3(1.0, 0.36, 0.04) * pool * flick * fire * flare * Bright, 0.0);
"""

PIN_CODE = LIFE + """
float heat = on * smoothstep(0.0, 0.35, t) * (1.0 - smoothstep(D + 0.8, D + 5.0, t));
float charred = on * smoothstep(0.1, 1.4, t);
return float4(heat, charred, 0.0, 0.0);
"""

def _clock_inputs(material, y=300):
    return [("Group", mats._scalar(material, "Group", 0.0, y), ""), ("Delay", mats._scalar(material, "Delay", 0.0, y + 100), ""),
            ("Duration", mats._scalar(material, "Duration", 1.6, y + 200), ""),
            ("C36", mats._collection(material, "Rings36", y + 300), ""), ("C42", mats._collection(material, "Rings42", y + 400), ""),
            ("C62", mats._collection(material, "Rings62", y + 500), "")]


def _local_position(material, x=-1300, y=-600):
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, x, y)
    local = mel.create_material_expression(material, unreal.MaterialExpressionTransformPosition, x + 200, y)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    return local


def _float3(material, code, inputs, y):
    return mats._custom(material, code, inputs, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, y=y)


def build_flame_material():
    """M_Intro_RingFlame: an upright card over a burn ring with flames, sparks and smoke (two crossed per fire)."""
    material = mats._new_material("M_Intro_RingFlame", blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                                  shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    width = mats._scalar(material, "W", 5.0, -300)
    height = mats._scalar(material, "H", 10.0, -200)
    local = _local_position(material)
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, -700)
    camera = mel.create_material_expression(material, unreal.MaterialExpressionCameraPositionWS, -1300, -800)
    actor = mel.create_material_expression(material, unreal.MaterialExpressionActorPositionWS, -1300, -900)
    normal = mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1300, -1000)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 100)
    custom = mats._custom(material, FLAME_CODE, [
        ("LP", local, ""), ("WP", world, ""), ("Cam", camera, ""), ("AP", actor, ""), ("N", normal, ""),
        ("Time", time, ""), ("W", width, ""), ("H", height, ""),
        ("Size", mats._scalar(material, "Size", 2.0, 150), ""), ("Seed", mats._scalar(material, "Seed", 0.0, 200), ""),
        ("Bright", mats._scalar(material, "Bright", 1.0, 250), "")] + _clock_inputs(material))
    mel.connect_material_property(mats._mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # The card meets the paper in a soft fade, not a hard horizontal cut.
    fade = mel.create_material_expression(material, unreal.MaterialExpressionDepthFade, -200, 250)
    fade.set_editor_property("fade_distance_default", 0.6)
    mel.connect_material_expressions(mats._mask(material, custom, "a", y=150), "", fade, "Opacity")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    return mats._finish(material)


def build_glow_material():
    """M_Intro_RingGlow: additive, flat on the paper: the fire's flickering light pool."""
    material = mats._new_material("M_Intro_RingGlow", blend=unreal.BlendMode.BLEND_ADDITIVE,
                                  shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    coords = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 100)
    custom = mats._custom(material, GLOW_CODE, [
        ("UV", coords, ""), ("Time", time, ""), ("Seed", mats._scalar(material, "Seed", 0.0, 200), ""),
        ("Bright", mats._scalar(material, "Bright", 0.7, 250), "")] + _clock_inputs(material))
    mel.connect_material_property(mats._mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return mats._finish(material)


def build_pin_material():
    """M_Intro_PinBurn: the map pin's own textures; red-hot in the fire, then charred black."""
    material = mats._new_material("M_Intro_PinBurn")

    def sample(name, y, sampler, default):
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler)
        # The default must match the sampler type or the material fails in the render process (README).
        node.set_editor_property("texture", unreal.load_asset(default))
        return node

    tex = lib.ROOT + "/Props/Textures/T_Intro_MapPin_"
    base = sample("BC", -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, tex + "BC")
    normal = sample("N", 50, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, tex + "N")
    orm = sample("ORM", 300, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, tex + "ORM")
    state = mats._custom(material, PIN_CODE, _clock_inputs(material, 700))
    look = mats._custom(material, """
float3 char_ = float3(0.018, 0.016, 0.014);
float3 col = lerp(BC, char_, State.y * 0.92);
return float4(col, lerp(ORM.g, 0.92, State.y));
""", [("BC", base, "RGB"), ("ORM", orm, ""), ("State", state, "")], y=0)
    glow = mats._custom(material, "return float3(1.6, 0.22, 0.02) * State.x * (0.8 + 0.2 * sin(Time * 23.0));",
                        [("State", state, ""), ("Time", mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 650), "")],
                        output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, y=400)
    metal = mats._custom(material, "return lerp(ORM.b, 0.0, State.y);", [("ORM", orm, ""), ("State", state, "")],
                         output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, y=500)
    mel.connect_material_property(mats._mask(material, look, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mats._mask(material, look, "a", y=100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    return mats._finish(material)


def build_all():
    build_flame_material()
    build_glow_material()
    build_pin_material()


def ring_instances(label, group, delay, duration, size, seed):
    """Material instances for one ring's flame card and light pool. Returns (flame MI path, glow MI path)."""
    shared = {"Group": group, "Delay": delay, "Duration": duration, "Seed": seed}
    flame = dict(shared, Size=size, W=2.4 * size, H=5.0 * size)
    mats.instance("MI_Intro_Flame_" + label, unreal.load_asset(M + "M_Intro_RingFlame"), scalars=flame)
    mats.instance("MI_Intro_Glow_" + label, unreal.load_asset(M + "M_Intro_RingGlow"), scalars=shared)
    return M + "MI_Intro_Flame_" + label, M + "MI_Intro_Glow_" + label


def pin_instance(label, group, delay, duration, seed):
    mats.instance("MI_Intro_PinBurn_" + label, unreal.load_asset(M + "M_Intro_PinBurn"),
                  scalars={"Group": group, "Delay": delay, "Duration": duration, "Seed": seed})
    return M + "MI_Intro_PinBurn_" + label
