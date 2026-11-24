"""Builds the info takeover look and applies it to every takeover Blueprint. OPTION picks the look from
options_spec.py: 'G' ("Rotsignal", black oxide + dull signal red, picked 2026-11-12) is live; 'D' (the brass
instrument panel it replaced) still works, so a revert is one switch and a rerun. Run headless with the DebugGame
editor (the DLL must have FInfoTakeoverStyle::PictureGlass and the page lamp brushes):

  python Source/Tools/InfoTakeover/Style/gen_style_art_dark.py G      (D: gen_style_art.py D)
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/Style/build_takeover_style.py -unattended -nullrhi

Assets, all in /Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Style/:
- Textures/T_InfoTakeover_Plate (1240 x 920 units) and T_InfoTakeover_Button_<n|h|p> (560 x 96, 2 texels per unit);
- M_InfoTakeover_GlassOverArt: draws the art with a vertical 3-slice (top 150 / bottom 124 rows fixed, the middle
  cropped, never stretched) plus an optional rect of the shared M_UI_GlassPane HLSL on top (off for option D);
- MI_InfoTakeover_Plate, MI_InfoTakeover_Button_<n|h|p>: its instances;
- MI_InfoTakeover_PaneGlass (frosted body panes) and MI_InfoTakeover_PictureGlass (gauge glass over every picture):
  instances of the shared /Game/RTS_Survival/Textures/GameUI/Skins/_Shared/M_UI_GlassPane (master unchanged).
Before the first apply of an option, every Blueprint's M_Style is saved to
Saved/InfoTakeover/Style/style_backup_before_<OPTION>.json; restore() puts that back.
"""
import json
import os
import re
import sys
import traceback

import unreal

try:
    HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:
    HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/Style'
ROOT_DIR = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))
GEN = os.path.join(ROOT_DIR, 'UISkins', '_generator')
for p in (HERE, GEN):
    if p not in sys.path:
        sys.path.insert(0, p)
import importlib  # noqa: E402
import options_spec  # noqa: E402
importlib.reload(options_spec)
import ui_glass_panes as G  # noqa: E402
import ui_skin_material  # noqa: E402

OPTION = 'G'
ART_DIR = os.path.join(ROOT_DIR, 'Saved', 'InfoTakeover', 'Style', 'art')
BACKUP = os.path.join(ROOT_DIR, 'Saved', 'InfoTakeover', 'Style', 'style_backup_before_%s.json' % OPTION)
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
TAKEOVERS = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
FOLDER = TAKEOVERS + '/Style'
TEX_FOLDER = FOLDER + '/Textures'
MASTER_NAME = 'M_InfoTakeover_GlassOverArt'
MASTER = FOLDER + '/' + MASTER_NAME
GLASS_MASTER = '/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/M_UI_GlassPane'
BLUEPRINTS = [TAKEOVERS + '/WBP_VehicleInfoTakeover', TAKEOVERS + '/WBP_CameraControlsTakeover',
              TAKEOVERS + '/Cover/WBP_CoverTakeover', TAKEOVERS + '/SpacedArmour/WBP_SpacedArmourTakeover',
              TAKEOVERS + '/NomadicConversion/WBP_NomadicConversionTakeover',
              TAKEOVERS + '/BxpPackUp/WBP_BxpPackUpTakeover', TAKEOVERS + '/VulturesAt/WBP_VulturesAtTakeover',
              TAKEOVERS + '/RattleFeed/WBP_RattleFeedTakeover',
              TAKEOVERS + '/WallBreach/WBP_WallBreachTakeover']
# The live asset names stay the same whichever option is applied; only the art behind them changes.
TEXTURES = {'T_InfoTakeover_Plate': 'T_TKO_%s_Plate' % OPTION,
            'T_InfoTakeover_Button_n': 'T_TKO_%s_Button_n' % OPTION,
            'T_InfoTakeover_Button_h': 'T_TKO_%s_Button_h' % OPTION,
            'T_InfoTakeover_Button_p': 'T_TKO_%s_Button_p' % OPTION,
            'T_InfoTakeover_PageLamp_On': 'T_TKO_%s_PageLamp_on' % OPTION,
            'T_InfoTakeover_PageLamp_Off': 'T_TKO_%s_PageLamp_off' % OPTION}
PAGE_LAMP_UNITS = 24.0

PRELUDE = r'''
float2 tkoPx = 1.0 / max(fwidth(UV), 0.000001);
float tkoScale = tkoPx.x / ArtWidth;
float2 tkoArt = UV * tkoPx / tkoScale;
float tkoH = tkoPx.y / tkoScale;
float tkoYTop = min(tkoArt.y, ArtHeight - 0.5);
float tkoYBot = clamp(ArtHeight - (tkoH - tkoArt.y), 0.5, ArtHeight - 0.5);
float tkoBlend = saturate((tkoArt.y - (tkoH - BottomBand)) / max(BlendTexels, 1.0) + 1.0) * step(0.5, BottomBand);
float4 tkoTop = Texture2DSample(Art, ArtSampler, float2(tkoArt.x / ArtWidth, tkoYTop / ArtHeight));
float4 tkoBot = Texture2DSample(Art, ArtSampler, float2(tkoArt.x / ArtWidth, tkoYBot / ArtHeight));
float4 tkoArtColor = lerp(tkoTop, tkoBot, tkoBlend);
float tkoGy1 = GlassHeight > 0.0 ? GlassTop + GlassHeight : tkoH - GlassBottom;
float2 tkoRectMin = float2(GlassLeft, GlassTop);
float2 tkoRectSize = max(float2(ArtWidth - GlassRight - GlassLeft, tkoGy1 - GlassTop), 1.0);
float2 gUV = (tkoArt - tkoRectMin) / tkoRectSize;
float tkoInRect = step(0.0, gUV.x) * step(gUV.x, 1.0) * step(0.0, gUV.y) * step(gUV.y, 1.0);
'''
GLASS_RETURN = 'return float4(premultiplied / max(alpha, 0.0001), alpha * inside);'
EPILOGUE = r'''
float tkoGlassA = alpha * inside * tkoInRect * GlassEnable;
float3 tkoGlassRgb = premultiplied / max(alpha, 0.0001);
// blend in display space, like Slate blends a separate M_UI_GlassPane image over the art
float3 tkoRgb = pow(lerp(pow(max(tkoArtColor.rgb, 0.0), 1.0 / 2.2), pow(max(tkoGlassRgb, 0.0), 1.0 / 2.2), tkoGlassA), 2.2);
float tkoA = tkoArtColor.a + tkoGlassA * (1.0 - tkoArtColor.a);
return float4(tkoRgb, tkoA);
'''
EXTRA_SCALARS = {'ArtWidth': 1240.0, 'ArtHeight': 920.0, 'BottomBand': 124.0, 'BlendTexels': 40.0,
                 'GlassLeft': 0.0, 'GlassTop': 0.0, 'GlassRight': 0.0, 'GlassBottom': 0.0, 'GlassHeight': 0.0,
                 'GlassEnable': 0.0}


def log(msg):
    unreal.log('TKO_STYLE ' + msg)


def import_texture(asset_name, png_name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', os.path.join(ART_DIR, png_name + '.png'))
    task.set_editor_property('destination_path', TEX_FOLDER)
    task.set_editor_property('destination_name', asset_name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(TEX_FOLDER + '/' + asset_name)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('never_stream', True)
    tex.set_editor_property('srgb', True)
    EAL.save_asset(TEX_FOLDER + '/' + asset_name, only_if_is_dirty=False)
    return tex


def build_master(default_texture):
    hlsl = PRELUDE + re.sub(r'\bUV\b', 'gUV', G.HLSL).replace(GLASS_RETURN, EPILOGUE)
    assert EPILOGUE in hlsl, 'the M_UI_GlassPane HLSL return line changed; update GLASS_RETURN'
    if EAL.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        for _ in range(8):
            MEL.delete_all_material_expressions(material)
            if MEL.get_num_material_expressions(material) == 0:
                break
    else:
        material = TOOLS.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property('code', hlsl)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description', 'InfoTakeover GlassOverArt')
    scalars = dict(EXTRA_SCALARS)
    scalars.update(G.SCALARS)
    inputs = []
    for name in ['UV', 'Art'] + list(scalars) + list(G.VECTORS):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -300)
    MEL.connect_material_expressions(uv, '', custom, 'UV')
    tex = MEL.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -900, -200)
    tex.set_editor_property('parameter_name', 'Art')
    tex.set_editor_property('texture', default_texture)
    tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    MEL.connect_material_expressions(tex, '', custom, 'Art')
    y = 0
    for name, value in scalars.items():
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', value)
        MEL.connect_material_expressions(node, '', custom, name)
        y += 70
    for name, value in G.VECTORS.items():
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, y)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', unreal.LinearColor(*value))
        rgba = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, -700, y)
        MEL.connect_material_expressions(node, '', rgba, 'A')
        MEL.connect_material_expressions(node, 'A', rgba, 'B')
        MEL.connect_material_expressions(rgba, '', custom, name)
        y += 110
    ui_skin_material.connect_rgb_and_alpha(material, custom)
    MEL.recompile_material(material)
    EAL.save_asset(MASTER, only_if_is_dirty=False)
    return material


def make_instance(name, parent, scalars, vectors=None, texture=None):
    path = FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        inst = unreal.load_asset(path)
    else:
        inst = TOOLS.create_asset(name, FOLDER, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
    inst.set_editor_property('parent', parent)
    MEL.clear_all_material_instance_parameters(inst)
    for k, v in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(inst, k, float(v))
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(*v))
    if texture is not None:
        MEL.set_material_instance_texture_parameter_value(inst, 'Art', texture)
    MEL.update_material_instance(inst)
    EAL.save_asset(path, only_if_is_dirty=False)
    return inst


def button_instance(master, state, texture, glass):
    """UNDERSTOOD / CONTINUE / PREVIOUS art; with glass the takeover drives its Hover / HoverSweep / Pressed."""
    scalars = dict(options_spec.BUTTON_ART, GlassEnable=0.0)
    vectors = {}
    if glass:
        glass_scalars, vectors = split_glass(glass)
        scalars.update(glass_scalars)
        left, top, right, bottom, height = glass['rect']
        scalars.update({'GlassLeft': left, 'GlassTop': top, 'GlassRight': right, 'GlassBottom': bottom,
                        'GlassHeight': height, 'GlassEnable': 1.0})
    return make_instance('MI_InfoTakeover_Button_' + state, master, scalars, vectors, texture)


def split_glass(spec):
    scalars = {k: v for k, v in spec.items() if k != 'rect' and not isinstance(v, (tuple, list))}
    vectors = {k: v for k, v in spec.items() if isinstance(v, (tuple, list)) and k != 'rect'}
    return scalars, vectors


def image_brush(brush, resource):
    brush.set_editor_property('resource_object', resource)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('tiling', unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property('tint_color', unreal.SlateColor(specified_color=unreal.LinearColor(1, 1, 1, 1)))
    return brush


def style_of(bp_path):
    name = bp_path.rsplit('/', 1)[1]
    cdo = unreal.get_default_object(unreal.load_object(None, bp_path + '.' + name + '_C'))
    return cdo, cdo.get_editor_property('m_style')


def backup_styles():
    record = {}
    if os.path.exists(BACKUP):
        with open(BACKUP) as handle:
            record = json.load(handle)
    for bp_path in BLUEPRINTS:
        if bp_path in record:
            continue  # keep the very first (pre-restyle) state
        record[bp_path] = style_of(bp_path)[1].export_text()
    os.makedirs(os.path.dirname(BACKUP), exist_ok=True)
    with open(BACKUP, 'w') as handle:
        json.dump(record, handle, indent=1)


def lamp_brush(brush, texture):
    brush.set_editor_property('resource_object', texture)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('tiling', unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property('tint_color', unreal.SlateColor(specified_color=unreal.LinearColor(1, 1, 1, 1)))
    return brush


def slate_color(hex_color):
    """FSlateColor from an sRGB hex colour (the struct holds linear colour, like FColor::FromHex does)."""
    srgb = unreal.LinearColor(*[int(hex_color.lstrip('#')[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], 1.0)
    return unreal.SlateColor(specified_color=unreal.LinearColor(*[c ** 2.2 for c in (srgb.r, srgb.g, srgb.b)], 1.0))


def restyle(bp_path, plate, buttons, pane, picture, lamps=None, colors=None):
    cdo, style = style_of(bp_path)
    skin = style.get_editor_property('skin')
    card = skin.get_editor_property('card')
    card.set_editor_property('normal', image_brush(card.get_editor_property('normal'), plate))
    skin.set_editor_property('card', card)
    confirm = skin.get_editor_property('confirm_button')
    for state, prop in (('n', 'normal'), ('h', 'hovered'), ('p', 'pressed')):
        confirm.set_editor_property(prop, image_brush(confirm.get_editor_property(prop), buttons[state]))
    skin.set_editor_property('confirm_button', confirm)
    skin.set_editor_property('variant_section_glass', image_brush(skin.get_editor_property('variant_section_glass'), pane))
    if colors:
        # section labels, kind label and divider use LabelColor; the vehicle's weapon pips use AccentColor
        skin.set_editor_property('label_color', slate_color(colors['label_color']))
        skin.set_editor_property('accent_color', slate_color(colors['accent_color']))
    style.set_editor_property('skin', skin)
    style.set_editor_property('picture_glass', image_brush(style.get_editor_property('picture_glass'), picture))
    if lamps is not None:
        style.set_editor_property('page_lamp_lit', lamp_brush(style.get_editor_property('page_lamp_lit'), lamps['on']))
        style.set_editor_property('page_lamp_unlit',
                                  lamp_brush(style.get_editor_property('page_lamp_unlit'), lamps['off']))
    cdo.set_editor_property('m_style', style)
    unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(bp_path))
    log('restyled %s saved=%s' % (bp_path, EAL.save_asset(bp_path, only_if_is_dirty=False)))


def build():
    spec = options_spec.OPTIONS[OPTION]
    for path in (FOLDER, TEX_FOLDER):
        if not EAL.does_directory_exist(path):
            EAL.make_directory(path)
    textures = {name: import_texture(name, png) for name, png in TEXTURES.items()}
    master = build_master(textures['T_InfoTakeover_Plate'])
    plate = make_instance('MI_InfoTakeover_Plate', master, dict(options_spec.PLATE_ART, GlassEnable=0.0), None,
                          textures['T_InfoTakeover_Plate'])
    buttons = {s: button_instance(master, s, textures['T_InfoTakeover_Button_' + s], spec.get('button_glass'))
               for s in 'nhp'}
    glass_master = unreal.load_asset(GLASS_MASTER)
    pane = make_instance('MI_InfoTakeover_PaneGlass', glass_master, *split_glass(spec['section_glass']))
    picture = make_instance('MI_InfoTakeover_PictureGlass', glass_master, *split_glass(spec['well_glass']))
    if not hasattr(unreal.InfoTakeoverStyle(), 'picture_glass'):
        raise RuntimeError('this editor DLL has no FInfoTakeoverStyle::PictureGlass; build DebugGame first')
    backup_styles()
    lamps = {'on': textures['T_InfoTakeover_PageLamp_On'], 'off': textures['T_InfoTakeover_PageLamp_Off']}
    if not hasattr(unreal.InfoTakeoverStyle(), 'page_lamp_lit'):
        raise RuntimeError('this editor DLL has no FInfoTakeoverStyle::PageLampLit; build DebugGame first')
    for bp_path in BLUEPRINTS:
        restyle(bp_path, plate, buttons, pane, picture, lamps, spec)
    log('DONE')


def restore():
    """Puts every Blueprint's M_Style back as it was before the first build() (the Style assets stay)."""
    with open(BACKUP) as handle:
        record = json.load(handle)
    for bp_path, text in record.items():
        cdo, style = style_of(bp_path)
        style.import_text(text)
        cdo.set_editor_property('m_style', style)
        unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(bp_path))
        EAL.save_asset(bp_path, only_if_is_dirty=False)
        log('restored ' + bp_path)


# Importing the module with TKO_STYLE_IMPORT_ONLY=1 in the environment only defines the functions.
if os.environ.get('TKO_STYLE_IMPORT_ONLY') != '1':
    try:
        restore() if 'restore' in sys.argv[1:] else build()
    except Exception:
        log('FAILED\n' + traceback.format_exc())
