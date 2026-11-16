"""Builds the info takeover page lamps: a detailed signal pilot lamp shader that lights the panel around it.

Run inside any editor (the open one is fine, it only creates or rebuilds these assets and the settings ini entry):
  py D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/Style/build_page_lamps.py
or headless with -run=pythonscript -script=<this file>. preview(out_dir) draws a contact sheet without PIE.

Assets:
- /Game/RTS_Survival/Textures/GameUI/Skins/_Shared/AnalogGauges/M_UI_SignalLamp: UI material, AlphaComposite
  (premultiplied), so one master draws two passes that UW_InfoTakeover places as two widgets per lamp:
    Pass 0 = the lamp: knurled steel bezel with a raised crest, recessed seat, domed fresnel-ringed red glass with a
             hot filament core when lit, caustic and window reflections when dark;
    Pass 1 = the halo, drawn under the lamps on a quad HaloScale times the lamp: a soft contact shadow of the bezel on
             the plate plus additive light spill (rgb added, alpha 0), so the plate and the title around it light up.
  Power (0..1) fades both passes; C++ ramps it like a warming filament. Time ignores pause (the takeover pauses).
- /Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Style/MI_InfoTakeover_PageLamp: the Rotsignal look (dull
  signal red on gunmetal), set as UIndicatorLampSettings::PageLampMaterial in Config/DefaultGame.ini.
"""
import os
import sys

import unreal

try:
    HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:
    HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/Style'
ROOT_DIR = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))
GEN = os.path.join(ROOT_DIR, 'UISkins', '_generator')
if GEN not in sys.path:
    sys.path.insert(0, GEN)
import ui_skin_material  # noqa: E402

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

MASTER_FOLDER = '/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/AnalogGauges'
MASTER_NAME = 'M_UI_SignalLamp'
INSTANCE_FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Style'
INSTANCE_NAME = 'MI_InfoTakeover_PageLamp'
PLATE_TEXTURE = INSTANCE_FOLDER + '/Textures/T_InfoTakeover_Plate'

HLSL = r'''
float T = Seconds + TimeOffset;
float S = Pass > 0.5 ? max(HaloScale, 1.0) : 1.0;
float2 uvPerPx = max(fwidth(UV), 0.000001);
float aa = max(uvPerPx.x, uvPerPx.y) * S;          // one screen pixel in lamp space (lamp radius 0.5)
float2 d = (UV - 0.5) * S;
float r = length(d);
float2 dir = d / max(r, 1e-5);
float2 L = normalize(float2(-0.55, -0.83));        // light from the top left, screen y down
float facing = dot(dir, L);
float power = saturate(Power);
float flick = 1.0 - FlickerAmount * (0.5 + 0.5 * sin(T * 31.0) * sin(T * 11.3 + 0.7));
float on = power * flick;
float3 lamp = LampColor.rgb;

// ======================================================================================== halo pass
if (Pass > 0.5)
{
    float edge = saturate(1.0 - r / (0.5 * S));
    edge *= edge;
    // the bezel stands proud of the plate: soft contact shadow, down and to the right
    float rs = length(d - float2(ShadowX, ShadowY));
    float shadow = ShadowStrength * (1.0 - smoothstep(0.40, 0.70, rs)) * edge;
    // spill: a bright ring hugging the bezel and a long soft tail, zero at the quad edge
    float out_r = max(r - 0.47, 0.0);
    float spill = exp(-out_r / GlowFalloff) * 0.6 + exp(-(r * r) / (GlowWide * GlowWide)) * 0.4;
    float glow = GlowIntensity * on * spill * edge;
    float3 rgb = GlowColor.rgb * glow;
    float a = shadow * (1.0 - saturate(glow * 6.0));
    return float4(rgb * VertexAlpha, a);
}

// ======================================================================================== lamp pass
float rOut = 0.495;
float lensR = LensRadius;
float crestR = CrestRadius;
float cover = saturate((rOut - r) / aa + 0.5);

// ---- steel bezel: inner bevel falls to the recessed lens seat, the crest catches the light, the collar rolls off
float slope;
if (r < crestR)
{
    float t = saturate((r - lensR) / (crestR - lensR));
    slope = -0.85 * sin(3.14159 * (0.15 + 0.7 * t));     // inner bevel faces the centre
}
else
{
    float t = saturate((r - crestR) / (rOut - crestR));
    slope = 0.95 * sin(1.5708 * t);                       // outer collar faces outwards
}
float diffuse = slope * facing;
float3 steel = SteelColor.rgb;
float3 metal = steel * (0.75 + 1.5 * diffuse) + pow(saturate(diffuse), 10.0) * 0.32;
// turned grooves on the bevel and knurling on the collar, faded out when they would alias
float ang = atan2(d.y, d.x);
float knurlPitch = 6.2832 * r / max(Knurls, 3.0);
float knurlVis = saturate(knurlPitch / (aa * 3.0) - 0.4) * step(crestR + 0.015, r);
metal *= 1.0 - 0.32 * knurlVis * (0.5 + 0.5 * sin(ang * Knurls));
float grooveVis = saturate(0.012 / (aa * 2.5) - 0.3) * step(r, crestR);
metal *= 1.0 - 0.18 * grooveVis * (0.5 + 0.5 * sin(r * 523.6));
float lineW = aa * 1.1 + 0.004;
metal += steel * 3.5 * exp(-pow((r - crestR) / lineW, 2.0)) * (0.25 + 0.75 * saturate(facing + 0.2));   // crest
metal *= 1.0 - 0.85 * exp(-pow((r - lensR) / lineW, 2.0));                                              // seat
metal *= 1.0 - 0.6 * exp(-pow((r - rOut) / (lineW * 1.4), 2.0));                                       // outer edge
// a lit lamp throws its light onto the bevel around it and warms the whole ring a little
metal += lamp * on * (0.55 * exp(-max(r - lensR, 0.0) / 0.045) * step(lensR, r) + 0.035);

// ---- lens: domed red glass with fresnel rings
float2 c = d / lensR;
float rn = length(c);
float rings = max(Rings, 1.0);
float f = frac(rn * rings);
float ringVis = saturate((lensR / rings) / (aa * 2.5) - 0.4);
float ringShade = lerp(1.0, 0.72 + 0.56 * f, ringVis);
float ringLine = ringVis * exp(-pow((1.0 - f) * (lensR / rings) / (aa * 0.9 + 0.002), 2.0));
// dark: daylight enters at the top left and gathers at the bottom right inside the glass
float caustic = exp(-dot(c - float2(0.30, 0.36), c - float2(0.30, 0.36)) / 0.20);
float3 dark = LensDark.rgb * (0.55 + 2.6 * caustic) * ringShade;
dark += LensDark.rgb * 1.5 * ringLine * saturate(-dot(normalize(c + 1e-5), L));
// lit: saturated red, a hot core with the coiled filament across it, rings glowing along their steps
float core = exp(-rn * rn * 4.0);
float filament = exp(-(c.x * c.x) / 0.09 - (c.y * c.y) / 0.010) * (0.75 + 0.25 * sin(c.x * 60.0));
float3 lit = lamp * (0.28 + 0.72 * (1.0 - rn * rn)) * ringShade;
lit += HotColor.rgb * (core * 0.55 + filament * 0.45);
lit += lamp * ringLine * 0.35;
float3 lens = lerp(dark, lit, on);
// the bezel shades the glass at its top left rim; light leaving the dome glints at the bottom right rim
lens *= 1.0 - 0.6 * exp(-(1.0 - rn) / 0.10) * saturate(0.45 + facing);
lens += lerp(LensDark.rgb * 3.0, lamp, on) * exp(-pow((rn - 0.9) / 0.07, 2.0)) * saturate(-facing) * 0.6;
// ---- glass reflections: a window streak, a sharp sparkle and a soft sheen over the top
float2 q = c - float2(-0.33, -0.42);
float qx = (q.x - q.y) * 0.7071;
float qy = (q.x + q.y) * 0.7071;
float streak = exp(-(qx * qx) / 0.030 - (qy * qy) / 0.0055);
float2 q2 = c - float2(0.12, -0.60);
float sparkle = exp(-dot(q2, q2) / 0.0045);
float sheen = saturate(-c.y) * (1.0 - rn) * 0.08;
lens += SpecColor.rgb * (streak * 0.55 + sparkle * 0.45 + sheen) * SpecIntensity * (1.0 - 0.35 * on);

float inLens = saturate((lensR - r) / aa + 0.5);
float3 col = lerp(metal, lens, inLens) * Brightness;
return float4(col * cover * VertexAlpha, cover);
'''

SCALARS = {
    'Pass': 0.0, 'Power': 1.0, 'HaloScale': 3.0, 'TimeOffset': 0.0, 'FlickerAmount': 0.04,
    'LensRadius': 0.335, 'CrestRadius': 0.425, 'Knurls': 40.0, 'Rings': 4.0, 'SpecIntensity': 1.0, 'Brightness': 1.0,
    'GlowIntensity': 0.11, 'GlowFalloff': 0.15, 'GlowWide': 0.80,
    'ShadowStrength': 0.55, 'ShadowX': 0.03, 'ShadowY': 0.05,
}
VECTORS = {
    'LampColor': (0.78, 0.060, 0.024, 1.0),
    'HotColor': (1.00, 0.52, 0.30, 1.0),
    'LensDark': (0.050, 0.0055, 0.0040, 1.0),
    'GlowColor': (1.00, 0.20, 0.07, 1.0),
    'SteelColor': (0.050, 0.054, 0.058, 1.0),
    'SpecColor': (0.85, 0.88, 0.92, 1.0),
}
# Rotsignal (option G): the dull signal red of the labels and the button strip, on black-oxide gunmetal.
INSTANCE_SCALARS = {}
INSTANCE_VECTORS = {}


def log(msg):
    unreal.log('PAGE_LAMPS ' + msg)


def build_master():
    return build_premultiplied_master(MASTER_NAME, HLSL, SCALARS, VECTORS,
                                      'SignalLamp (premultiplied: lamp pass 0, halo pass 1)')


def build_premultiplied_master(name, hlsl, scalars, vectors, description):
    """Fresh UI material in AlphaComposite: one Custom node fed by UV, a pause-ignoring Time, vertex alpha and the
    parameters."""
    path = MASTER_FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        material = unreal.load_asset(path)
        for _ in range(8):
            MEL.delete_all_material_expressions(material)
            if MEL.get_num_material_expressions(material) == 0:
                break
    else:
        material = TOOLS.create_asset(name, MASTER_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property('code', hlsl)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description', description)
    inputs = []
    for input_name in ['UV', 'Seconds', 'VertexAlpha'] + list(scalars) + list(vectors):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', input_name)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -400)
    MEL.connect_material_expressions(uv, '', custom, 'UV')
    seconds = MEL.create_material_expression(material, unreal.MaterialExpressionTime, -900, -320)
    seconds.set_editor_property('ignore_pause', True)
    MEL.connect_material_expressions(seconds, '', custom, 'Seconds')
    # Slate multiplies the output by the vertex colour, so alpha already gets the widget's opacity but premultiplied
    # rgb would not; the HLSL scales rgb (only) by it, so a fading widget stays premultiplied correctly.
    vertex = MEL.create_material_expression(material, unreal.MaterialExpressionVertexColor, -900, -240)
    MEL.connect_material_expressions(vertex, 'A', custom, 'VertexAlpha')
    y = -150
    for param, value in scalars.items():
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
        node.set_editor_property('parameter_name', param)
        node.set_editor_property('default_value', float(value))
        MEL.connect_material_expressions(node, '', custom, param)
        y += 70
    for param, value in vectors.items():
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, y)
        node.set_editor_property('parameter_name', param)
        node.set_editor_property('default_value', unreal.LinearColor(*value))
        rgba = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, -700, y)
        MEL.connect_material_expressions(node, '', rgba, 'A')
        MEL.connect_material_expressions(node, 'A', rgba, 'B')
        MEL.connect_material_expressions(rgba, '', custom, param)
        y += 110
    ui_skin_material.connect_rgb_and_alpha(material, custom)
    MEL.recompile_material(material)
    log('%s saved=%s' % (name, EAL.save_asset(path, only_if_is_dirty=False)))
    return material


def build_instance(master):
    path = INSTANCE_FOLDER + '/' + INSTANCE_NAME
    if EAL.does_asset_exist(path):
        inst = unreal.load_asset(path)
    else:
        inst = TOOLS.create_asset(INSTANCE_NAME, INSTANCE_FOLDER, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
    inst.set_editor_property('parent', master)
    MEL.clear_all_material_instance_parameters(inst)
    for key, value in INSTANCE_SCALARS.items():
        MEL.set_material_instance_scalar_parameter_value(inst, key, float(value))
    for key, value in INSTANCE_VECTORS.items():
        MEL.set_material_instance_vector_parameter_value(inst, key, unreal.LinearColor(*value))
    MEL.update_material_instance(inst)
    log('instance saved=%s' % EAL.save_asset(path, only_if_is_dirty=False))
    return inst


# ------------------------------------------------------------------------------------------------ HUD indicator halo
# The idle unit buttons' blinking jewel lamps (M_UI_IndicatorLamp instances) get the same kind of halo. It is its own
# master so the shared lamp master stays untouched; FIndicatorLamp copies the lamp instance's blink parameters and
# colour onto its halo, and both read the same pause-ignoring clock, so the halo pulses in step with the jewel.
HALO_NAME = 'M_UI_IndicatorLampHalo'
HALO_HLSL = r'''
float T = Seconds + TimeOffset + BlinkPhase;
float S = max(HaloScale, 1.0);
float2 d = (UV - 0.5) * S;                         // lamp space, lamp radius 0.5
float r = length(d);
// ---- the same blink as M_UI_IndicatorLamp
float mode = round(BlinkMode);
float on = 1.0;
float s = frac(T * BlinkHz);
if (mode > 0.5 && mode < 1.5)
    on = smoothstep(0.0, 0.05, s) * (1.0 - smoothstep(Duty, Duty + 0.07, s));
else if (mode > 1.5 && mode < 2.5)
{
    on = 0.5 - 0.5 * cos(T * BlinkHz * 6.2832);
    on = on * on * (3.0 - 2.0 * on);
}
else if (mode > 2.5 && mode < 3.5)
    on = smoothstep(0.0, 0.03, s) * (1.0 - smoothstep(0.07, 0.11, s)) + smoothstep(0.17, 0.2, s) * (1.0 - smoothstep(0.24, 0.28, s));
else if (mode > 3.5)
{
    float drop = frac(sin(floor(T * 9.0) * 12.9898) * 43758.5453);
    on = (0.86 + 0.14 * sin(T * 31.0) * sin(T * 17.0)) * (drop < 0.08 ? 0.3 : 1.0);
}
else
    on = 0.94 + 0.06 * sin(T * 29.0) * sin(T * 13.0);
on = lerp(MinOn, 1.0, saturate(on)) * saturate(Power);
// ---- contact shadow of the bezel plus additive light spill, zero at the quad edge
float edge = saturate(1.0 - r / (0.5 * S));
edge *= edge;
float rs = length(d - float2(ShadowX, ShadowY));
float shadow = ShadowStrength * (1.0 - smoothstep(0.40, 0.70, rs)) * edge;
float spill = exp(-max(r - 0.47, 0.0) / GlowFalloff) * 0.6 + exp(-(r * r) / (GlowWide * GlowWide)) * 0.4;
float glow = GlowIntensity * on * spill * edge;
float3 rgb = lerp(LampColor.rgb, float3(1.0, 1.0, 1.0), 0.12) * glow;
float a = shadow * (1.0 - saturate(glow * 6.0));
return float4(rgb * VertexAlpha, a);
'''
HALO_SCALARS = {'TimeOffset': 0.0, 'BlinkMode': 0.0, 'BlinkHz': 1.0, 'BlinkPhase': 0.0, 'Duty': 0.5, 'MinOn': 0.0,
                'Power': 1.0, 'HaloScale': 3.5, 'GlowIntensity': 0.12, 'GlowFalloff': 0.16, 'GlowWide': 0.85,
                'ShadowStrength': 0.45, 'ShadowX': 0.03, 'ShadowY': 0.05}
HALO_VECTORS = {'LampColor': (1.0, 0.12, 0.05, 1.0)}


def build_indicator_halo():
    return build_premultiplied_master(HALO_NAME, HALO_HLSL, HALO_SCALARS, HALO_VECTORS,
                                      'IndicatorLamp halo (premultiplied: shadow alpha + additive light)')


def build():
    master = build_master()
    build_instance(master)
    build_indicator_halo()
    log('DONE')


# ------------------------------------------------------------------------------------------------ preview
def preview(out_dir, scale=4.0, lit_index=1, lamp_count=3, filename='page_lamps.png', time_offset=0.0,
            powers=None):
    """Contact sheet without PIE: a crop of the takeover plate's title placard (right end), the halos, then the
    lamps, all at the real 24-unit size and 6-unit gap times scale pixels per unit."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    lamp_units, gap_units = 24.0, 6.0
    crop = (1240.0 - 260.0, 0.0, 260.0, 120.0)   # x, y, w, h of the plate art in units
    width, height = int(crop[2] * scale), int(crop[3] * scale)
    target = unreal.RenderingLibrary.create_render_target2d(world, width, height,
                                                           unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                           unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    canvas, size, context = unreal.RenderingLibrary.begin_draw_canvas_to_render_target(world, target)
    plate = unreal.load_asset(PLATE_TEXTURE)
    canvas.draw_texture(plate, unreal.Vector2D(0, 0), unreal.Vector2D(width, height),
                        unreal.Vector2D(crop[0] / 1240.0, crop[1] / 920.0),
                        unreal.Vector2D(crop[2] / 1240.0, crop[3] / 920.0),
                        unreal.LinearColor(1, 1, 1, 1), unreal.BlendMode.BLEND_OPAQUE)
    inst = unreal.load_asset(INSTANCE_FOLDER + '/' + INSTANCE_NAME)
    halo_scale = 3.0
    # title placard lamp seat: right inset 50 units (40 panel padding + 10), top 47 (30 + 17)
    row_right = crop[2] - 50.0
    top = 47.0
    lefts = [row_right - (lamp_count - i) * lamp_units - (lamp_count - 1 - i) * gap_units for i in range(lamp_count)]
    for lamp_pass in (1, 0):
        for index, left in enumerate(lefts):
            mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, inst)
            power = powers[index] if powers else (1.0 if index == lit_index else 0.0)
            mid.set_scalar_parameter_value('Pass', float(lamp_pass))
            mid.set_scalar_parameter_value('Power', power)
            mid.set_scalar_parameter_value('TimeOffset', time_offset + index * 1.7)
            mid.set_scalar_parameter_value('HaloScale', halo_scale)
            s = halo_scale if lamp_pass == 1 else 1.0
            edge = lamp_units * s
            x = left + lamp_units * 0.5 - edge * 0.5
            y = top + lamp_units * 0.5 - edge * 0.5
            canvas.draw_material(mid, unreal.Vector2D(x * scale, y * scale), unreal.Vector2D(edge * scale, edge * scale),
                                 unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))
    unreal.RenderingLibrary.end_draw_canvas_to_render_target(world, context)
    unreal.RenderingLibrary.export_render_target(world, target, out_dir, filename)
    log('preview %s/%s' % (out_dir, filename))
    return os.path.join(out_dir, filename)


PREVIEW_DIR = os.path.join(ROOT_DIR, 'Saved', 'InfoTakeover', 'PageLamps')

# build_page_lamps.py            -> build (headless commandlet is fine)
# build_page_lamps.py preview    -> preview sheets (needs a rendering editor; never nest it in UECP execute_python)
if os.environ.get('PAGE_LAMPS_IMPORT_ONLY') != '1' and __name__ == '__main__':
    try:
        if 'preview' in sys.argv[1:]:
            preview(PREVIEW_DIR, scale=6.0, lit_index=1, filename='page_lamps_lit1.png')
            preview(PREVIEW_DIR, scale=6.0, lit_index=0, lamp_count=1, filename='page_lamps_single.png')
            log('PREVIEW DONE')
        else:
            build()
    except Exception:
        import traceback
        log('FAILED\n' + traceback.format_exc())
