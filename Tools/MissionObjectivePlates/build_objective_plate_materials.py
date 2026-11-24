"""Builds the mission objective plate materials (design A1-c, Source/Docs/UI/MissionObjectivePlates.md).

Run headless (no editor open on these assets):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash -nosound

Three procedural UI materials, each one Custom HLSL node (same pattern as UISkins/_generator/ui_glass_resource_icons.py):
  M_UI_ObjectiveShield     chrome bezel + enamel shield with two hex nuts and a drop shadow. NO icon in it.
  M_UI_ObjectiveGlassIcon  the star (IconShape 0) or diamond (IconShape 1) as faceted coloured glass, drawn as its own
                           image on top of the shield so its glass can be tuned on its own instance.
  M_UI_ObjectiveKnob       knurled expand / collapse knob (PointUp, Hover, Press are set per button state at runtime).
Instances: MI_ObjectiveShield_Major / _Bonus, MI_ObjectiveGlassIcon_Star / _Diamond.
"""
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/UISkins/_generator')
import ui_skin_material  # noqa: E402

FOLDER = '/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates'

SHIELD_HLSL = r'''
struct FShield
{
    // Signed distance to the shield outline (negative inside); y points down, the tip is at the bottom.
    float Sd(float2 p, float hx, float shoulder)
    {
        float2 v[5];
        v[0] = float2(-hx, -1.0);
        v[1] = float2(hx, -1.0);
        v[2] = float2(hx, shoulder);
        v[3] = float2(0.0, 1.0);
        v[4] = float2(-hx, shoulder);
        float d = dot(p - v[0], p - v[0]);
        float s = 1.0;
        int j = 4;
        for (int i = 0; i < 5; i++)
        {
            float2 e = v[j] - v[i];
            float2 w = p - v[i];
            float2 b = w - e * saturate(dot(w, e) / dot(e, e));
            d = min(d, dot(b, b));
            bool c0 = p.y >= v[i].y;
            bool c1 = p.y < v[j].y;
            bool c2 = e.x * w.y > e.y * w.x;
            if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) { s = -s; }
            j = i;
        }
        return s * sqrt(d);
    }
    float SdHex(float2 p, float r)
    {
        const float3 k = float3(-0.866025404, 0.5, 0.577350269);
        p = abs(p);
        p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
        p -= float2(clamp(p.x, -k.z * r, k.z * r), r);
        return length(p) * sign(p.y);
    }
};
FShield shield;

// Shield drawn at 90 % of the image so its drop shadow fits; x is scaled by the image aspect.
float2 p = ((UV - 0.5) * 2.0 * float2(Aspect, 1.0) - float2(-0.02, -0.05)) / 0.9;
float hx = Aspect;
float sd = shield.Sd(p, hx, Shoulder) - Rounding;
float aa = max(fwidth(sd), 0.0005);
float coverage = saturate(0.5 - sd / aa);

// ---- chrome bezel: rounded profile lit from the top left, dark lines at both edges
float t = saturate(-sd / BezelWidth);
float2 n = normalize(float2(ddx(sd), ddy(sd)) + 0.000001);
float2 toLight = normalize(float2(-0.55, -0.83));
float facing = t < 0.5 ? dot(n, toLight) : -dot(n, toLight);
float profile = 1.0 - abs(2.0 * t - 1.0);
float topLight = saturate(0.5 - p.y * 0.5);
float3 chrome = BezelColor.rgb * (0.55 + 0.6 * topLight) + float3(0.55, 0.57, 0.6) * saturate(facing) * profile;
chrome += float3(0.25, 0.26, 0.27) * pow(saturate(facing), 6.0) * profile;
float outerLine = saturate(1.0 - (-sd) / (aa * 1.6));
float innerLine = saturate(1.0 - abs(-sd - BezelWidth) / (aa * 1.4));
chrome *= 1.0 - 0.75 * saturate(outerLine + innerLine);

// ---- enamel: lighter at the top, inner shadow along the bezel, soft gloss at the top left
float enamelMask = saturate(0.5 - (sd + BezelWidth) / aa);
float depth = -(sd + BezelWidth);
float3 enamel = lerp(EnamelTop.rgb, EnamelBottom.rgb, saturate((p.y + 1.0) * 0.5));
enamel *= 1.0 - 0.45 * pow(saturate(1.0 - depth / 0.2), 2.0);
float2 glossP = (p - float2(-0.32 * hx, -0.6)) / float2(0.6 * hx, 0.3);
enamel += EnamelTop.rgb * GlossIntensity * pow(saturate(1.0 - length(glossP)), 2.0);
float3 color = lerp(chrome, enamel, enamelMask);

// ---- two hex nuts holding the shield on the plate
for (int nutIndex = 0; nutIndex < 2; nutIndex++)
{
    float2 nutCentre = float2((nutIndex == 0 ? -1.0 : 1.0) * (hx - NutInset), -1.0 + NutInset);
    float2 q = p - nutCentre;
    float nd = shield.SdHex(q, NutSize);
    float nutMask = saturate(0.5 - nd / aa);
    float nutLight = saturate(0.55 - q.y / NutSize * 0.45 - q.x / NutSize * 0.25);
    float3 nut = BezelColor.rgb * 0.6 + float3(0.5, 0.52, 0.55) * nutLight;
    nut *= 1.0 - 0.7 * saturate(1.0 - (-nd) / (aa * 1.5));
    color = lerp(color, nut, nutMask);
}

// ---- drop shadow under the shield (straight alpha composite of shield over black shadow)
float shadowSd = shield.Sd(p - ShadowOffset.xy, hx, Shoulder) - Rounding;
float shadowAlpha = saturate(0.5 - shadowSd / (aa * 3.0)) * ShadowOpacity;
float alpha = coverage + shadowAlpha * (1.0 - coverage);
return float4(color * coverage / max(alpha, 0.0001), alpha);
'''

SHIELD_SCALARS = {'Aspect': 0.8235, 'Shoulder': 0.3, 'Rounding': 0.05, 'BezelWidth': 0.13, 'GlossIntensity': 0.55,
                  'NutSize': 0.075, 'NutInset': 0.24, 'ShadowOpacity': 0.55}
SHIELD_VECTORS = {'EnamelTop': (0.14, 0.33, 0.63, 1.0), 'EnamelBottom': (0.024, 0.077, 0.19, 1.0),
                  'BezelColor': (0.11, 0.12, 0.13, 1.0), 'ShadowOffset': (0.05, 0.09, 0.0, 0.0)}

ICON_HLSL = r'''
struct FIcon
{
    float SdStar5(float2 p, float r, float rf)
    {
        const float2 k1 = float2(0.809016994375, -0.587785252292);
        const float2 k2 = float2(-k1.x, k1.y);
        p.x = abs(p.x);
        p -= 2.0 * max(dot(k1, p), 0.0) * k1;
        p -= 2.0 * max(dot(k2, p), 0.0) * k2;
        p.x = abs(p.x);
        p.y -= r;
        float2 ba = rf * float2(-k1.y, k1.x) - float2(0.0, 1.0);
        float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
        return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
    }
    float SdRhombus(float2 p, float2 b)
    {
        p = abs(p);
        float2 c = b - 2.0 * p;
        float h = clamp((c.x * b.x - c.y * b.y) / dot(b, b), -1.0, 1.0);
        float d = length(p - 0.5 * b * float2(1.0 - h, 1.0 + h));
        return d * sign(p.x * b.y + p.y * b.x - b.x * b.y);
    }
    float Sd(float2 p, float shape)
    {
        // The star SDF expects y up; UI uv has y down.
        return shape < 0.5 ? SdStar5(float2(p.x, -(p.y - 0.06)), 0.86, 0.46) : SdRhombus(p, float2(0.66, 0.9));
    }
};
FIcon icon;

float2 p = (UV - 0.5) * 2.0 / max(IconScale, 0.01);
float sd = icon.Sd(p, IconShape);
float aa = max(fwidth(sd), 0.0005);
float coverage = saturate(0.5 - sd / aa);

// ---- facets: each star arm has a lit and a shaded half; the diamond has four faces
float2 facet;
if (IconShape < 0.5)
{
    float a = atan2(p.x, -(p.y - 0.06));
    float sector = 6.28318530718 / 5.0;
    float axis = round(a / sector) * sector;
    float side = sign(a - axis);
    float2 along = float2(sin(axis), -cos(axis));
    float2 across = float2(cos(axis), sin(axis));
    facet = across * side + along * 0.35;
}
else
{
    facet = float2(sign(p.x) * 0.8, sign(p.y) * 0.6);
}
float3 normal = normalize(float3(facet * FacetStrength, 1.0));
float3 toLight = normalize(float3(-0.45, -0.65, 0.62));
float diffuse = saturate(dot(normal, toLight));

// ---- coloured glass: deep colour in the shaded faces, glass colour in the lit ones, light glowing up from below
float3 glass = lerp(DeepColor.rgb, GlassColor.rgb, diffuse);
float inner = saturate(1.0 - length(p - float2(0.0, 0.32)) * 1.1);
glass += GlassColor.rgb * GlowIntensity * inner * inner;

// ---- rim: bright on the side facing the light, a dark groove just inside it
float depth = -sd;
float edge = saturate(1.0 - depth / 0.1);
float2 outward = normalize(float2(ddx(sd), ddy(sd)) + 0.000001);
float rimLit = saturate(dot(outward, normalize(toLight.xy)));
glass += RimColor.rgb * edge * edge * (0.2 + 0.9 * rimLit);
glass *= 1.0 - 0.3 * saturate(1.0 - abs(depth - 0.13) / 0.035);

// ---- specular glint on the facets and a thin diagonal streak across the upper left
float3 reflected = reflect(-toLight, normal);
float spec = pow(saturate(reflected.z), 28.0) * SpecIntensity;
float streakPos = dot(p, normalize(float2(1.0, 1.0))) + 0.3;
float streak = exp(-(streakPos * streakPos) / 0.0036) * StreakIntensity * saturate(depth / 0.06);
glass += (spec + streak).xxx;
float glassAlpha = saturate(Opacity + edge * 0.35 + spec + streak);

// ---- outer glow in the glass colour and a drop shadow, composited under the glass
float glowAlpha = exp(-max(sd, 0.0) / 0.07) * OuterGlow * (1.0 - coverage);
float shadowSd = icon.Sd(p - ShadowOffset.xy, IconShape);
float shadowAlpha = saturate(0.5 - shadowSd / (aa * 2.5)) * ShadowOpacity;
float topAlpha = glassAlpha * coverage;
float3 premult = glass * topAlpha;
float underAlpha = glowAlpha + shadowAlpha * (1.0 - glowAlpha);
float3 underPremult = GlassColor.rgb * glowAlpha;
premult += underPremult * (1.0 - topAlpha);
float alpha = topAlpha + underAlpha * (1.0 - topAlpha);
return float4(premult / max(alpha, 0.0001), alpha);
'''

ICON_SCALARS = {'IconShape': 0.0, 'IconScale': 0.92, 'FacetStrength': 0.9, 'GlowIntensity': 0.55,
                'SpecIntensity': 0.6, 'StreakIntensity': 0.45, 'Opacity': 0.88, 'OuterGlow': 0.35,
                'ShadowOpacity': 0.6}
ICON_VECTORS = {'GlassColor': (0.4, 0.62, 0.91, 1.0), 'DeepColor': (0.012, 0.042, 0.11, 1.0),
                'RimColor': (0.85, 0.92, 1.0, 1.0), 'ShadowOffset': (0.06, 0.1, 0.0, 0.0)}

KNOB_HLSL = r'''
struct FKnob
{
    float Segment(float2 p, float2 a, float2 b)
    {
        float2 pa = p - a;
        float2 ba = b - a;
        float h = saturate(dot(pa, ba) / dot(ba, ba));
        return length(pa - ba * h);
    }
};
FKnob knob;

float2 p = (UV - 0.5) * 2.0;
float r = length(p);
float aa = max(fwidth(r), 0.0005);
float coverage = saturate((0.97 - r) / aa + 0.5);
float topLight = saturate(0.5 - p.y / max(r, 0.0001) * 0.5);

// ---- knurled chrome ring
float a = atan2(p.y, p.x);
float teeth = 0.5 + 0.5 * cos(a * Teeth);
float3 ring = RingColor.rgb * (0.35 + 0.45 * teeth + 0.55 * topLight) * (1.0 + 0.25 * Hover);
float ringOuter = saturate(1.0 - abs(r - 0.955) / (aa * 1.5));
float ringInner = saturate(1.0 - abs(r - DomeRadius) / (aa * 1.5));
ring *= 1.0 - 0.7 * saturate(ringOuter + ringInner);

// ---- dark dome with a soft highlight, lit a little on hover, darker when pressed
float domeMask = saturate((DomeRadius - r) / aa + 0.5);
float highlight = saturate(1.0 - length(p - float2(-0.2, -0.28)) / 0.75);
float3 dome = float3(0.018, 0.02, 0.022) + float3(0.11, 0.115, 0.12) * highlight * highlight * (1.0 + 0.6 * Hover);
dome *= 1.0 - 0.4 * Press;
dome *= 1.0 - 0.5 * pow(saturate(r / DomeRadius), 6.0);
float3 color = lerp(ring, dome, domeMask);

// ---- chevron: points down (expand) or up (collapse)
float flip = PointUp > 0.5 ? -1.0 : 1.0;
float2 cp = float2(p.x, (p.y - 0.03 * Press) * flip);
float chev = min(knob.Segment(cp, float2(-0.27, -0.12), float2(0.0, 0.14)),
                 knob.Segment(cp, float2(0.0, 0.14), float2(0.27, -0.12))) - 0.075;
float chevMask = saturate(0.5 - chev / aa) * domeMask;
color = lerp(color, ChevronColor.rgb * (1.0 + 0.35 * Hover), chevMask);
return float4(color, coverage);
'''

KNOB_SCALARS = {'PointUp': 0.0, 'Hover': 0.0, 'Press': 0.0, 'Teeth': 30.0, 'DomeRadius': 0.68}
KNOB_VECTORS = {'RingColor': (0.42, 0.45, 0.48, 1.0), 'ChevronColor': (0.73, 0.71, 0.64, 1.0)}


def srgb_to_linear(hex_color):
    hex_color = hex_color.lstrip('#')
    channels = [int(hex_color[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in channels) + (1.0,)


def build_material(name, hlsl, scalars, vectors, description):
    lib = unreal.MaterialEditingLibrary
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        for _ in range(8):
            lib.delete_all_material_expressions(material)
            if lib.get_num_material_expressions(material) == 0:
                break
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property('code', hlsl)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description', description)
    inputs = []
    for input_name in ['UV'] + list(scalars) + list(vectors):
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', input_name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -200)
    lib.connect_material_expressions(uv, '', custom, 'UV')
    y = 0
    for param_name, value in scalars.items():
        node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
        node.set_editor_property('parameter_name', param_name)
        node.set_editor_property('default_value', value)
        lib.connect_material_expressions(node, '', custom, param_name)
        y += 70
    for param_name, value in vectors.items():
        node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, y)
        node.set_editor_property('parameter_name', param_name)
        node.set_editor_property('default_value', unreal.LinearColor(*value))
        rgba = lib.create_material_expression(material, unreal.MaterialExpressionAppendVector, -700, y)
        lib.connect_material_expressions(node, '', rgba, 'A')
        lib.connect_material_expressions(node, 'A', rgba, 'B')
        lib.connect_material_expressions(rgba, '', custom, param_name)
        y += 110
    ui_skin_material.connect_rgb_and_alpha(material, custom)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log('OBJECTIVE_PLATE_BUILD material %s' % path)
    return material


def build_instance(name, parent, scalars, vectors):
    lib = unreal.MaterialEditingLibrary
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property('parent', parent)
    for param_name, value in scalars.items():
        lib.set_material_instance_scalar_parameter_value(instance, param_name, value)
    for param_name, value in vectors.items():
        lib.set_material_instance_vector_parameter_value(instance, param_name, unreal.LinearColor(*value))
    lib.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log('OBJECTIVE_PLATE_BUILD instance %s' % path)


def build():
    shield = build_material('M_UI_ObjectiveShield', SHIELD_HLSL, SHIELD_SCALARS, SHIELD_VECTORS, 'ObjectiveShield')
    icon = build_material('M_UI_ObjectiveGlassIcon', ICON_HLSL, ICON_SCALARS, ICON_VECTORS, 'ObjectiveGlassIcon')
    build_material('M_UI_ObjectiveKnob', KNOB_HLSL, KNOB_SCALARS, KNOB_VECTORS, 'ObjectiveKnob')
    # Steel blue major / signal green bonus (sketch A1-c), sRGB -> linear.
    build_instance('MI_ObjectiveShield_Major', shield, {},
                   {'EnamelTop': srgb_to_linear('6a9bd0'), 'EnamelBottom': srgb_to_linear('2b4f78')})
    build_instance('MI_ObjectiveShield_Bonus', shield, {},
                   {'EnamelTop': srgb_to_linear('5fb47b'), 'EnamelBottom': srgb_to_linear('2a6a3e')})
    build_instance('MI_ObjectiveGlassIcon_Star', icon, {'IconShape': 0.0},
                   {'GlassColor': srgb_to_linear('6fa8e8'), 'DeepColor': srgb_to_linear('173a6a'),
                    'RimColor': srgb_to_linear('d6e8ff')})
    build_instance('MI_ObjectiveGlassIcon_Diamond', icon, {'IconShape': 1.0},
                   {'GlassColor': srgb_to_linear('62c98a'), 'DeepColor': srgb_to_linear('144a2a'),
                    'RimColor': srgb_to_linear('d8ffe4')})
    unreal.log('OBJECTIVE_PLATE_BUILD done')


build()
