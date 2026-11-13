"""Builds the nomadic conversion takeover's looping illustration WBP_NomCnv_Deploy (widget tree, look, opacity tracks)
from scratch, in GameUI/InfoTakeover/NomadicConversion.

Run inside the live editor through UECP's execute_uecp_script (it needs the `uecp` object) with allow_destructive=true,
e.g. exec(open(r'<this file>').read()). It needs no new C++ class. Then run nomadic_conversion_transform_tracks.py for
the moving parts (cursor, preview, truck, unfolding, progress bar, rotate key).

The loop (DeployLoop, 10 s), all from the real game:
- 0.9 s: the cursor clicks the HQ truck (T_Ger_HQTruck, its unit icon) and its selection decal lights
  (M_UI_SelectionDecal_Box, the camera controls takeover's UI copy of the units' decal).
- 1.15 s: the bottom panel shows the truck's convert button with the building's picture (DT_TruckButtons row
  Nomadic_GerHQ: BuildingBrush SB_Ger_HQ = T_Ger_HQ) and its hotkey badge (IA_NomadicExpansion, Ctrl + Q). Hovering it
  shows the row's tooltip, "Convert into HQ building".
- 2.0 s: click; the building preview follows the cursor to a free spot; two presses of the period key (IA_RotateRight)
  turn its footprint 10 degrees each (RotationDegrees); 5.0 s: left click places it.
- 5.3 to 6.8 s: the truck drives there; 6.9 to 7.9 s: it unfolds into the HQ, with a conversion progress bar.
- 8.05 s: the same button now shows the truck's picture (TruckBrush SB_GerHqTruck = T_Ger_HQTruck); hovering it shows
  "Convert back into HQ Truck". The hotkey badge is hidden: Ctrl + Q does not pack a building up.
- 9.2 to 10 s: everything resets.

Art rule (user, 2026-11-11): no drawn art except the mouse/cursor (T_CamCtl_Cursor); the rest is game art or plain UMG.
Anything that fades is fill-only: render opacity does not fade a rounded box's outline.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion'
CAMERA_FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls'
CURSOR = CAMERA_FOLDER + '/T_CamCtl_Cursor'
BOX_DECAL = CAMERA_FOLDER + '/M_UI_SelectionDecal_Box'
HQ_ICON = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Ger_Buildings/GerHQ/T_Ger_HQ'
HQ_TRUCK_ICON = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Ger_Buildings/GerHQ/T_Ger_HQTruck'
WIDGET = 'WBP_NomCnv_Deploy'
ANIMATION = 'DeployLoop'
DURATION = 10.0
SIZE = (520, 292)

# The camera controls illustrations' palette, so the field manuals read alike.
ACC = '(SpecifiedColor=(R=0.04,G=0.58,B=1.0,A=%s))'
ACC_LIN = '(R=0.04,G=0.58,B=1.0,A=%s)'
TXT_LIN = '(R=0.83,G=0.80,B=0.72,A=%s)'
LINE_LIN = '(R=0.16,G=0.18,B=0.20,A=1)'
OUT = '(SpecifiedColor=(R=0.16,G=0.18,B=0.20,A=1))'
CLEAR = '(SpecifiedColor=(R=0,G=0,B=0,A=0))'
KEY_FILL = '(SpecifiedColor=(R=0.03,G=0.034,B=0.038,A=1))'
BASE_FILL = '(SpecifiedColor=(R=0.003,G=0.004,B=0.005,A=1))'
HUD_FILL = '(SpecifiedColor=(R=0.006,G=0.007,B=0.008,A=0.92))'
TOOLTIP_FILL = '(SpecifiedColor=(R=0.005,G=0.006,B=0.007,A=0.94))'
GROUND_FILL = '(SpecifiedColor=(R=0.018,G=0.02,B=0.018,A=1))'
# The building preview's footprint: a valid spot.
PREVIEW_FILL = '(SpecifiedColor=(R=0.1,G=0.75,B=0.25,A=0.32))'
REPORT = []

# Layout (illustration pixels). The truck starts left; the building goes right.
TRUCK_GROUP = (50, 80, 100, 100)           # centre (100, 130)
PREVIEW_GROUP = (340, 65, 120, 106)        # centre (400, 118)
BUILDING_ICON = (345, 70, 110, 97)         # T_Ger_HQ is 256 x 226
PROGRESS = (355, 176, 90, 6)
BUTTON_FRAME = (236, 234, 52, 52)          # centre (262, 260)


def p(x, y, w, h):
    return {'position': [x, y], 'size': [w, h], 'anchors': 'top_left'}


def pr(rect):
    return p(*rect)


def rb(widget, prefix, tint, radii, outline_width=0.0, outline_color=CLEAR):
    return [(widget, prefix + '.DrawAs', 'RoundedBox'), (widget, prefix + '.TintColor', tint),
            (widget, prefix + '.OutlineSettings.CornerRadii', '(X=%g,Y=%g,Z=%g,W=%g)' % radii),
            (widget, prefix + '.OutlineSettings.RoundingType', 'FixedRadius'),
            (widget, prefix + '.OutlineSettings.Width', str(outline_width)),
            (widget, prefix + '.OutlineSettings.Color', outline_color)]


def text(name, value, size, color, slot, justification='Center'):
    return {'name': name, 'type': 'TextBlock', 'slot': slot,
            'properties': {'text': value, 'font_size': size, 'color_and_opacity': color,
                           'justification': justification}}


def rebuild(children):
    path = FOLDER + '/' + WIDGET
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        uecp.asset_management.create_asset(asset_type='WidgetBlueprint', name=WIDGET, save_path=FOLDER)
    try:
        uecp.widget.remove_widget_animation(widget_path=path, animation_name=ANIMATION)
    except Exception:
        pass
    uecp.widget.create_widget_from_layout(widget_path=path, layout=[{'name': 'Root', 'type': 'CanvasPanel',
                                                                     'children': children}],
                                          replace=True, canvas_size='%d,%d' % SIZE, design_mode='Custom')
    uecp.widget.add_widget_animation(widget_path=path, animation_name=ANIMATION, duration=DURATION)
    return path


def track(path, widget, prop, keys):
    uecp.widget.add_widget_animation_track(widget_path=path, animation_name=ANIMATION, widget_name=widget,
                                           property_name=prop)
    for time, value in keys:
        uecp.widget.add_widget_animation_keyframe(widget_path=path, animation_name=ANIMATION, widget_name=widget,
                                                  property_name=prop, time=time, value=value, interp_mode='linear')


def bind(path, widget):
    # A constant opacity track gives the transform script a binding for the widget.
    track(path, widget, 'opacity', [(0.0, 1.0), (DURATION, 1.0)])


def window(on, off, fade=0.12, low=0.0, high=1.0):
    """Opacity that is `high` from `on` to `off` and `low` elsewhere in the loop."""
    return [(0.0, low), (on, low), (on + fade, high), (off, high), (off + fade, low), (DURATION, low)]


def clicks(times, hold=0.15):
    keys = [(0.0, 0.0)]
    for time in times:
        keys += [(time, 0.0), (time + 0.05, 1.0), (time + 0.05 + hold, 1.0), (time + 0.1 + hold, 0.0)]
    keys.append((DURATION, 0.0))
    return keys


def key_cap(name, x, y, glyph):
    return [{'name': 'KeyBase_' + name, 'type': 'Border', 'slot': p(x, y + 4, 30, 30)},
            {'name': 'Key_' + name, 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(x, y, 30, 30),
             'children': [{'name': 'KeyStack_' + name, 'type': 'Overlay', 'children': [
                 {'name': 'KeyGlow_' + name, 'type': 'Image', 'slot': {'Padding': '3'}},
                 text('Glyph_' + name, glyph, 18, TXT_LIN % '1',
                      {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}]}]


def tooltip_box(name, value, rect):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '(Left=8,Top=3,Right=8,Bottom=3)'},
            'slot': pr(rect), 'children': [text(name + 'Text', value, 13, TXT_LIN % '1',
                                                {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}


def layout():
    children = [{'name': 'Ground', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])},
                # The placed preview stays as a faint ghost until the building stands.
                {'name': 'PreviewGroup', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': pr(PREVIEW_GROUP),
                 'children': [{'name': 'PreviewStack', 'type': 'Overlay', 'children': [
                     {'name': 'PreviewFill', 'type': 'Image',
                      'slot': {'Padding': '0', 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}},
                     {'name': 'PreviewIcon', 'type': 'Image',
                      'slot': {'Padding': '(Left=6,Top=6,Right=6,Bottom=6)', 'HorizontalAlignment': 'Fill',
                               'VerticalAlignment': 'Fill'}}]}]},
                {'name': 'TruckGroup', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': pr(TRUCK_GROUP),
                 'children': [{'name': 'TruckStack', 'type': 'Overlay', 'children': [
                     {'name': 'TruckDecal', 'type': 'Image',
                      'slot': {'Padding': '0', 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}},
                     {'name': 'TruckIcon', 'type': 'Image',
                      'slot': {'Padding': '8', 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}}]}]},
                {'name': 'BuildingIcon', 'type': 'Image', 'slot': pr(BUILDING_ICON)},
                {'name': 'ProgressBack', 'type': 'Image', 'slot': pr(PROGRESS)},
                {'name': 'ProgressFill', 'type': 'Image', 'slot': pr(PROGRESS)},
                # The bottom panel strip of the HUD with the convert button.
                {'name': 'HudStrip', 'type': 'Border', 'slot': p(0, 226, SIZE[0], 66)},
                {'name': 'HudLine', 'type': 'Image', 'slot': p(0, 226, SIZE[0], 1.5)},
                {'name': 'ButtonFrame', 'type': 'Border', 'slot': pr(BUTTON_FRAME)},
                {'name': 'ButtonImage_Building', 'type': 'Image', 'slot': p(240, 241, 44, 39)},
                {'name': 'ButtonImage_Truck', 'type': 'Image', 'slot': p(240, 238, 44, 44)},
                {'name': 'ButtonGlow', 'type': 'Image', 'slot': p(238, 236, 48, 48)},
                text('HotkeyBadge', 'CTRL Q', 12, ACC_LIN % '1', p(292, 250, 64, 20)),
                tooltip_box('TooltipBuild', 'Convert into HQ building', (150, 194, 220, 28)),
                tooltip_box('TooltipPack', 'Convert back into HQ Truck', (150, 194, 220, 28))]
    children += key_cap('Comma', 400, 242, ',')
    children += key_cap('Period', 436, 242, '.')
    children += [{'name': 'MiniMouseBody', 'type': 'Border', 'slot': p(476, 234, 32, 50)},
                 {'name': 'LeftButtonGlow', 'type': 'Image', 'slot': p(478, 236, 13, 20)},
                 {'name': 'MiniSplit', 'type': 'Image', 'slot': p(491.25, 236, 1.5, 20)},
                 {'name': 'MiniLine', 'type': 'Image', 'slot': p(478, 256, 28, 1.5)},
                 # Positioned by its translation track (tip coordinates minus the image's tip offset).
                 {'name': 'MouseCursor', 'type': 'Image', 'slot': p(0, 0, 26, 26)},
                 {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])}]
    return children


def look(path):
    edits = rb('Ground', 'Background', GROUND_FILL, (2, 2, 2, 2))
    edits += [('PreviewGroup', 'Background.DrawAs', 'NoDrawType'), ('TruckGroup', 'Background.DrawAs', 'NoDrawType')]
    edits += rb('PreviewFill', 'Brush', PREVIEW_FILL, (4, 4, 4, 4))
    # Overlay slots may not stretch their child, so the fills carry their full size in the brush.
    edits += [('PreviewFill', 'Brush.ImageSize', '(X=%d,Y=%d)' % PREVIEW_GROUP[2:])]
    for name in ('Comma', 'Period'):
        edits += [('KeyGlow_' + name, 'Brush.ImageSize', '(X=24,Y=24)')]
    edits += [('PreviewIcon', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.6)'), ('PreviewGroup', 'RenderOpacity', '0')]
    edits += [('TruckDecal', 'RenderOpacity', '0'), ('BuildingIcon', 'RenderOpacity', '0')]
    edits += [('ProgressBack', 'ColorAndOpacity', '(R=0.003,G=0.004,B=0.005,A=0.9)'),
              ('ProgressFill', 'ColorAndOpacity', ACC_LIN % '1'),
              ('ProgressFill', 'RenderTransformPivot', '(X=0,Y=0.5)'),
              ('ProgressBack', 'RenderOpacity', '0'), ('ProgressFill', 'RenderOpacity', '0')]
    edits += rb('HudStrip', 'Background', HUD_FILL, (0, 0, 0, 0))
    edits += [('HudLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ButtonFrame', 'Background', KEY_FILL, (6, 6, 6, 6), 1.5, OUT)
    edits += rb('ButtonGlow', 'Brush', ACC % '0.45', (5, 5, 5, 5))
    for name in ('ButtonImage_Building', 'ButtonImage_Truck', 'ButtonGlow', 'HotkeyBadge', 'TooltipBuild',
                 'TooltipPack', 'LeftButtonGlow', 'KeyGlow_Comma', 'KeyGlow_Period'):
        edits += [(name, 'RenderOpacity', '0')]
    edits += rb('TooltipBuild', 'Background', TOOLTIP_FILL, (4, 4, 4, 4))
    edits += rb('TooltipPack', 'Background', TOOLTIP_FILL, (4, 4, 4, 4))
    for name in ('Comma', 'Period'):
        edits += rb('KeyBase_' + name, 'Background', BASE_FILL, (6, 6, 6, 6))
        edits += rb('Key_' + name, 'Background', KEY_FILL, (5, 5, 5, 5), 1.5, OUT)
        edits += rb('KeyGlow_' + name, 'Brush', ACC % '0.55', (3, 3, 3, 3))
    edits += rb('MiniMouseBody', 'Background', KEY_FILL, (15, 15, 13, 13), 1.5, OUT)
    edits += rb('LeftButtonGlow', 'Brush', ACC % '0.85', (12, 2, 2, 2))
    edits += [('MiniSplit', 'ColorAndOpacity', LINE_LIN), ('MiniLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    # Brushes first: set_image_brush resets the brush, so draw settings go on afterwards.
    brushes = [{'widget_name': 'TruckIcon', 'texture_path': HQ_TRUCK_ICON, 'image_size': [84, 84]},
               {'widget_name': 'TruckDecal', 'material_path': BOX_DECAL, 'image_size': [100, 100]},
               {'widget_name': 'PreviewIcon', 'texture_path': HQ_ICON, 'image_size': [108, 94]},
               {'widget_name': 'BuildingIcon', 'texture_path': HQ_ICON, 'image_size': [110, 97]},
               {'widget_name': 'ButtonImage_Building', 'texture_path': HQ_ICON, 'image_size': [44, 39]},
               {'widget_name': 'ButtonImage_Truck', 'texture_path': HQ_TRUCK_ICON, 'image_size': [44, 44]},
               {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}]
    uecp.widget.set_image_brush(widget_path=path, brushes=brushes)
    uecp.widget.edit_widget_properties(widget_path=path, edits=[{'widget_name': a, 'property_name': b, 'value': c}
                                                                for a, b, c in edits])


def opacity_tracks(path):
    # Widgets that only move get a constant track, so the transform script finds a binding.
    for name in ('MouseCursor', 'Key_Period', 'PreviewFill'):
        bind(path, name)
    # Selecting the truck (click at 0.9 s) lights its decal and fills the bottom panel's button.
    track(path, 'TruckDecal', 'opacity', window(0.95, 9.2, 0.1))
    track(path, 'ButtonImage_Building', 'opacity', window(1.05, 7.9, 0.12))
    track(path, 'HotkeyBadge', 'opacity', window(1.05, 7.9, 0.12))
    # Hovering the button shows its DT_TruckButtons tooltip.
    track(path, 'TooltipBuild', 'opacity', window(1.75, 2.05, 0.1))
    track(path, 'ButtonGlow', 'opacity', clicks([1.95], 0.2))
    track(path, 'LeftButtonGlow', 'opacity', clicks([0.85, 1.95, 4.95]))
    # The preview follows the cursor from the button, is placed at 5 s and stays as a ghost until the HQ stands.
    track(path, 'PreviewGroup', 'opacity', [(0, 0), (2.1, 0), (2.25, 1), (5.0, 1), (5.1, 0.45), (6.9, 0.45),
                                            (7.3, 0), (DURATION, 0)])
    # Two presses of the period key (IA_RotateRight) turn the footprint.
    track(path, 'KeyGlow_Period', 'opacity', clicks([3.8, 4.3], 0.12))
    # The truck unfolds into the building, with the conversion progress bar.
    track(path, 'TruckIcon', 'opacity', [(0, 1), (6.9, 1), (7.6, 0), (9.35, 0), (9.4, 1), (DURATION, 1)])
    track(path, 'BuildingIcon', 'opacity', [(0, 0), (6.9, 0), (7.6, 1), (9.2, 1), (9.4, 0), (DURATION, 0)])
    track(path, 'ProgressBack', 'opacity', window(6.85, 7.9, 0.1))
    track(path, 'ProgressFill', 'opacity', window(6.85, 7.9, 0.1))
    # The same button now packs the building up.
    track(path, 'ButtonImage_Truck', 'opacity', window(7.95, 9.2, 0.12))
    track(path, 'TooltipPack', 'opacity', window(8.45, 9.1, 0.1))
    # The truck fades out to jump back to its start, then fades in for the next loop.
    track(path, 'TruckGroup', 'opacity', [(0, 1), (9.2, 1), (9.3, 0), (9.8, 0), (DURATION, 1)])


def build_deploy():
    path = rebuild(layout())
    look(path)
    opacity_tracks(path)
    uecp.blueprint.compile_blueprint(blueprint_path=path)
    REPORT.append('%s saved=%s' % (WIDGET, unreal.EditorAssetLibrary.save_asset(path, False)))


build_deploy()
print('\n'.join(REPORT))
