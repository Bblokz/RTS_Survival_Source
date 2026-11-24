"""Builds the five camera controls takeover illustrations (widget tree, look, opacity tracks) from scratch.

Run inside the editor through UECP's execute_uecp_script (it needs the `uecp` object) with allow_destructive=true,
e.g. exec(open(<this file>).read()). Then run camera_controls_transform_tracks.py for the moving parts.

Art rules (user, 2026-11-11): no drawn art except the mouse/cursor and the marquee box.
- Page 1 shows the real minimap of Ger_IntroMission (T_Minimap_Ger_IntroMission, exported from the game's minimap
  terrain capture with -MinimapTerrainExport) with a camera view frame over it; keys, wheel and edge markers are plain
  UMG borders and text.
- Page 2 shows the game's unit icons (Panzer II F x3, Hazmat Vultures) and their real selection decals
  (M_UI_SelectionDecal_Box / _RoundThin: UI-domain copies of M_Selection_Box / M_Selection_Round_Thin, colour from
  MPC_Selection.Selected_Color) on a plain ground; the drag box uses the HUD's colour (FLinearColor::Blue, alpha 0.4).
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls'
MINIMAP = FOLDER + '/T_Minimap_Ger_IntroMission'
MINIMAP_UV_REGION = '(Min=(X=0.0,Y=0.08),Max=(X=0.68,Y=0.76),bIsValid=True)'
CURSOR = FOLDER + '/T_CamCtl_Cursor'
PANZER_ICON = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/LightTanks/T_Ger_LightTank_PanzerII_F'
VULTURE_ICON = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Squads/T_Ger_Squad_VultureHazmats'
BOX_DECAL = FOLDER + '/M_UI_SelectionDecal_Box'
ROUND_DECAL = FOLDER + '/M_UI_SelectionDecal_RoundThin'

ACC = '(SpecifiedColor=(R=0.04,G=0.58,B=1.0,A=%s))'
ACC_LIN = '(R=0.04,G=0.58,B=1.0,A=%s)'
TXT_LIN = '(R=0.83,G=0.80,B=0.72,A=%s)'
LINE_LIN = '(R=0.16,G=0.18,B=0.20,A=1)'
OUT = '(SpecifiedColor=(R=0.16,G=0.18,B=0.20,A=1))'
CLEAR = '(SpecifiedColor=(R=0,G=0,B=0,A=0))'
KEY_FILL = '(SpecifiedColor=(R=0.03,G=0.034,B=0.038,A=1))'
BASE_FILL = '(SpecifiedColor=(R=0.003,G=0.004,B=0.005,A=1))'
GROUND_FILL = '(SpecifiedColor=(R=0.018,G=0.02,B=0.018,A=1))'
REPORT = []


def p(x, y, w, h):
    return {'position': [x, y], 'size': [w, h], 'anchors': 'top_left'}


def rb(widget, prefix, tint, radii, outline_width=0.0, outline_color=CLEAR):
    return [(widget, prefix + '.DrawAs', 'RoundedBox'), (widget, prefix + '.TintColor', tint),
            (widget, prefix + '.OutlineSettings.CornerRadii', '(X=%g,Y=%g,Z=%g,W=%g)' % radii),
            (widget, prefix + '.OutlineSettings.RoundingType', 'FixedRadius'),
            (widget, prefix + '.OutlineSettings.Width', str(outline_width)),
            (widget, prefix + '.OutlineSettings.Color', outline_color)]


def text(name, value, size, color, slot):
    return {'name': name, 'type': 'TextBlock', 'slot': slot,
            'properties': {'text': value, 'font_size': size, 'color_and_opacity': color, 'justification': 'Center'}}


def rebuild(widget, animation, duration, children, size):
    path = FOLDER + '/' + widget
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        uecp.asset_management.create_asset(asset_type='WidgetBlueprint', name=widget, save_path=FOLDER)
    try:
        uecp.widget.remove_widget_animation(widget_path=path, animation_name=animation)
    except Exception:
        pass
    uecp.widget.create_widget_from_layout(widget_path=path, layout=[{'name': 'Root', 'type': 'CanvasPanel',
                                                                     'children': children}],
                                          replace=True, canvas_size='%d,%d' % size, design_mode='Custom')
    uecp.widget.add_widget_animation(widget_path=path, animation_name=animation, duration=duration)
    return path


def style(path, edits, brushes):
    # Brushes first: set_image_brush resets the brush, so draw settings and UV crops go on afterwards.
    if brushes:
        uecp.widget.set_image_brush(widget_path=path, brushes=brushes)
    uecp.widget.edit_widget_properties(widget_path=path, edits=[{'widget_name': a, 'property_name': b, 'value': c}
                                                                for a, b, c in edits])


def minimap_crop(widget):
    # Ger_IntroMission's playable features sit in the upper-left two thirds of its minimap; the rest is open sand.
    return [(widget, 'Brush.UVRegion', MINIMAP_UV_REGION)]


def track(path, animation, widget, prop, keys):
    uecp.widget.add_widget_animation_track(widget_path=path, animation_name=animation, widget_name=widget,
                                           property_name=prop)
    for time, value in keys:
        uecp.widget.add_widget_animation_keyframe(widget_path=path, animation_name=animation, widget_name=widget,
                                                  property_name=prop, time=time, value=value, interp_mode='linear')


def bind(path, animation, widget, duration):
    # A constant opacity track gives the transform script a binding for the widget.
    track(path, animation, widget, 'opacity', [(0.0, 1.0), (duration, 1.0)])


def pulse(start, offset, duration):
    keys = [(0, 0), (start + offset, 0)]
    time = start + offset
    is_on = True
    while time + 0.25 <= start + 1.6:
        time += 0.25
        keys.append((round(time, 3), 1.0 if is_on else 0.25))
        is_on = not is_on
    keys.append((round(time + 0.15, 3), 0))
    keys.append((duration, 0))
    return keys


def finish(path):
    uecp.blueprint.compile_blueprint(blueprint_path=path)
    REPORT.append('%s saved=%s' % (path.rsplit('/', 1)[1], unreal.EditorAssetLibrary.save_asset(path, False)))


# ---------------------------------------------------------------- page 1: arrow keys over the minimap
def build_keyboard_pan():
    animation = 'PanLoop'
    keys = {'Up': (66, 40, '↑'), 'Left': (14, 92, '←'), 'Down': (66, 92, '↓'),
            'Right': (118, 92, '→')}
    children = [{'name': 'KeyBase_' + n, 'type': 'Border', 'slot': p(x, y + 4, 46, 46)} for n, (x, y, _) in keys.items()]
    for n, (x, y, glyph) in keys.items():
        children.append({'name': 'Key_' + n, 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(x, y, 46, 46),
                         'children': [{'name': 'KeyStack_' + n, 'type': 'Overlay', 'children': [
                             {'name': 'KeyGlow_' + n, 'type': 'Image', 'slot': {'Padding': '3'}},
                             text('Glyph_' + n, glyph, 20, TXT_LIN % '1',
                                  {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}]})
    children.append({'name': 'MapWindow', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(168, 18, 150, 150),
                     'children': [{'name': 'MapCanvas', 'type': 'CanvasPanel', 'children': [
                         {'name': 'MinimapImage', 'type': 'Image', 'slot': p(0, 0, 150, 150)},
                         {'name': 'ViewFrame', 'type': 'Border', 'slot': p(47, 55, 56, 40)}]}]})
    children.append({'name': 'MapFrame', 'type': 'Border', 'slot': p(168, 18, 150, 150)})
    path = rebuild('WBP_CamCtl_KeyboardPan', animation, 4.0, children, (330, 186))
    edits = []
    for n in keys:
        edits += rb('KeyBase_' + n, 'Background', BASE_FILL, (7, 7, 7, 7))
        edits += rb('Key_' + n, 'Background', KEY_FILL, (6, 6, 6, 6), 1.5, OUT)
        edits += rb('KeyGlow_' + n, 'Brush', ACC % '0.55', (4, 4, 4, 4))
        edits += [('KeyGlow_' + n, 'RenderOpacity', '0')]
    edits += [('MapWindow', 'Background.DrawAs', 'NoDrawType'), ('MapWindow', 'Clipping', 'ClipToBounds')]
    edits += rb('ViewFrame', 'Background', ACC % '0.12', (2, 2, 2, 2), 2, ACC % '1')
    edits += rb('MapFrame', 'Background', CLEAR, (3, 3, 3, 3), 1.5, OUT)
    edits += minimap_crop('MinimapImage')
    style(path, edits, [{'widget_name': 'MinimapImage', 'texture_path': MINIMAP, 'image_size': [150, 150]}])
    for index, n in enumerate(['Right', 'Up', 'Left', 'Down']):
        start = float(index)
        glow = [(0.0, 0.0), (start, 0.0), (start + 0.08, 1.0), (start + 0.85, 1.0), (start + 0.95, 0.0), (4.0, 0.0)]
        track(path, animation, 'KeyGlow_' + n, 'opacity', [k for i, k in enumerate(glow) if i == 0 or k[0] != glow[i - 1][0]])
        bind(path, animation, 'Key_' + n, 4.0)
    bind(path, animation, 'ViewFrame', 4.0)
    finish(path)


# ---------------------------------------------------------------- page 1: edge panning on the zoomed minimap
def build_edge_pan():
    animation = 'EdgeLoop'
    children = [{'name': 'MapClip', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(0, 0, 330, 186),
                 'children': [{'name': 'MapCanvas', 'type': 'CanvasPanel', 'children': [
                     {'name': 'MinimapImage', 'type': 'Image', 'slot': p(-95, -167, 520, 520)}]}]},
                {'name': 'EdgeBar_R', 'type': 'Image', 'slot': p(322, 0, 8, 186)},
                {'name': 'EdgeBar_L', 'type': 'Image', 'slot': p(0, 0, 8, 186)},
                text('Chev_R', '»', 30, TXT_LIN % '1', p(278, 72, 36, 40)),
                text('Chev_L', '«', 30, TXT_LIN % '1', p(16, 72, 36, 40)),
                {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, 330, 186)},
                {'name': 'MouseCursor', 'type': 'Image', 'slot': p(150, 79, 26, 26)}]
    path = rebuild('WBP_CamCtl_EdgePan', animation, 6.0, children, (330, 186))
    edits = [('MapClip', 'Background.DrawAs', 'NoDrawType'), ('MapClip', 'Clipping', 'ClipToBounds')]
    for name in ('EdgeBar_R', 'EdgeBar_L', 'Chev_R', 'Chev_L'):
        edits += [(name, 'RenderOpacity', '0')]
    edits += [('EdgeBar_R', 'ColorAndOpacity', ACC_LIN % '0.9'), ('EdgeBar_L', 'ColorAndOpacity', ACC_LIN % '0.9')]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    edits += minimap_crop('MinimapImage')
    style(path, edits, [{'widget_name': 'MinimapImage', 'texture_path': MINIMAP, 'image_size': [520, 520]},
                        {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}])
    bind(path, animation, 'MouseCursor', 6.0)
    bind(path, animation, 'MinimapImage', 6.0)
    track(path, animation, 'EdgeBar_R', 'opacity', [(0, 0), (0.85, 0), (1.0, 1), (2.55, 1), (2.75, 0), (6.0, 0)])
    track(path, animation, 'EdgeBar_L', 'opacity', [(0, 0), (3.45, 0), (3.6, 1), (5.15, 1), (5.35, 0), (6.0, 0)])
    track(path, animation, 'Chev_R', 'opacity', pulse(0.95, 0.0, 6.0))
    track(path, animation, 'Chev_L', 'opacity', pulse(3.55, 0.0, 6.0))
    finish(path)


# ---------------------------------------------------------------- page 1: wheel zoom, the view frame shrinks and grows
def build_zoom():
    animation = 'ZoomLoop'
    ridges = [{'name': 'Ridge_%d' % i, 'type': 'Image', 'slot': {'Padding': '(Left=0,Top=2,Right=0,Bottom=2)'}}
              for i in range(16)]
    children = [{'name': 'MouseBody', 'type': 'Border', 'slot': p(30, 38, 70, 108)},
                {'name': 'MouseSplit', 'type': 'Image', 'slot': p(64.25, 40, 1.5, 38)},
                {'name': 'ButtonLine', 'type': 'Image', 'slot': p(32, 78, 66, 1.5)},
                {'name': 'WheelSlot', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(58, 48, 14, 28),
                 'children': [{'name': 'WheelCanvas', 'type': 'CanvasPanel', 'children': [
                     {'name': 'RidgeStrip', 'type': 'VerticalBox', 'slot': p(2, -24, 10, 96), 'children': ridges}]}]},
                text('ArrowIn', '↑', 18, ACC_LIN % '1', p(53, 8, 24, 24)),
                text('ArrowOut', '↓', 18, ACC_LIN % '1', p(53, 152, 24, 24)),
                {'name': 'ZoomWindow', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(128, 16, 188, 154),
                 'children': [{'name': 'ZoomCanvas', 'type': 'CanvasPanel', 'children': [
                     {'name': 'MinimapImage', 'type': 'Image', 'slot': p(17, 0, 154, 154)},
                     {'name': 'ViewFrame', 'type': 'Border', 'slot': p(52, 49, 84, 56)}]}]},
                {'name': 'ZoomFrame', 'type': 'Border', 'slot': p(128, 16, 188, 154)}]
    path = rebuild('WBP_CamCtl_Zoom', animation, 4.0, children, (330, 186))
    edits = rb('MouseBody', 'Background', KEY_FILL, (30, 30, 26, 26), 1.5, OUT)
    edits += rb('WheelSlot', 'Background', BASE_FILL, (6, 6, 6, 6), 1, ACC % '0.8')
    edits += [('WheelSlot', 'Clipping', 'ClipToBounds'), ('MouseSplit', 'ColorAndOpacity', LINE_LIN),
              ('ButtonLine', 'ColorAndOpacity', LINE_LIN), ('ZoomWindow', 'Background.DrawAs', 'NoDrawType'),
              ('ZoomWindow', 'Clipping', 'ClipToBounds'), ('ArrowIn', 'RenderOpacity', '0.25'),
              ('ArrowOut', 'RenderOpacity', '0.25')]
    for i in range(16):
        edits += [('Ridge_%d' % i, 'Brush.ImageSize', '(X=10,Y=2)'), ('Ridge_%d' % i, 'ColorAndOpacity', TXT_LIN % '0.8')]
    edits += rb('ViewFrame', 'Background', ACC % '0.12', (2, 2, 2, 2), 2, ACC % '1')
    edits += rb('ZoomFrame', 'Background', CLEAR, (3, 3, 3, 3), 1.5, OUT)
    edits += minimap_crop('MinimapImage')
    style(path, edits, [{'widget_name': 'MinimapImage', 'texture_path': MINIMAP, 'image_size': [154, 154]}])
    bind(path, animation, 'RidgeStrip', 4.0)
    bind(path, animation, 'ViewFrame', 4.0)
    track(path, animation, 'ArrowIn', 'opacity', [(0, 0.25), (0.3, 0.25), (0.4, 1), (1.6, 1), (1.75, 0.25), (4.0, 0.25)])
    track(path, animation, 'ArrowOut', 'opacity', [(0, 0.25), (2.2, 0.25), (2.3, 1), (3.5, 1), (3.65, 0.25), (4.0, 0.25)])
    finish(path)


# ---------------------------------------------------------------- page 2: marquee over three Panzer IIs and Vultures
PANZERS = [(132, 70), (236, 62), (156, 152)]
VULTURE = (300, 132)
UNIT_SIZE = 72
BOX_DECAL_SIZE = 100
ROUND_DECAL_SIZE = 124


def build_marquee():
    animation = 'MarqueeLoop'
    half_box = (BOX_DECAL_SIZE - UNIT_SIZE) / 2
    half_round = (ROUND_DECAL_SIZE - UNIT_SIZE) / 2
    children = [{'name': 'Ground', 'type': 'Border', 'slot': p(0, 0, 520, 292)}]
    for i, (x, y) in enumerate(PANZERS, 1):
        children.append({'name': 'Decal_%d' % i, 'type': 'Image',
                         'slot': p(x - half_box, y - half_box, BOX_DECAL_SIZE, BOX_DECAL_SIZE)})
    children.append({'name': 'Decal_4', 'type': 'Image', 'slot': p(VULTURE[0] - half_round, VULTURE[1] - half_round,
                                                                 ROUND_DECAL_SIZE, ROUND_DECAL_SIZE)})
    for i, (x, y) in enumerate(PANZERS, 1):
        children.append({'name': 'Unit_%d' % i, 'type': 'Image', 'slot': p(x, y, UNIT_SIZE, UNIT_SIZE)})
    children.append({'name': 'Unit_4', 'type': 'Image', 'slot': p(VULTURE[0], VULTURE[1], UNIT_SIZE, UNIT_SIZE)})
    children += [{'name': 'MarqueeBox', 'type': 'Image', 'slot': p(100, 48, 320, 192)},
                 {'name': 'MouseCursor', 'type': 'Image', 'slot': p(97, 46, 26, 26)},
                 {'name': 'MiniMouseBody', 'type': 'Border', 'slot': p(466, 222, 36, 54)},
                 {'name': 'LeftButtonGlow', 'type': 'Image', 'slot': p(468, 224, 15, 22)},
                 {'name': 'MiniSplit', 'type': 'Image', 'slot': p(483.25, 224, 1.5, 22)},
                 {'name': 'MiniLine', 'type': 'Image', 'slot': p(468, 246, 32, 1.5)},
                 {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, 520, 292)}]
    path = rebuild('WBP_CamCtl_Marquee', animation, 5.0, children, (520, 292))
    edits = rb('Ground', 'Background', GROUND_FILL, (2, 2, 2, 2))
    for i in range(1, 5):
        edits += [('Decal_%d' % i, 'RenderOpacity', '0')]
    for i in range(1, 4):
        edits += rb('Unit_%d' % i, 'Brush', '(SpecifiedColor=(R=1,G=1,B=1,A=1))', (8, 8, 8, 8), 1, OUT)
    edits += rb('Unit_4', 'Brush', '(SpecifiedColor=(R=1,G=1,B=1,A=1))', (36, 36, 36, 36), 1, OUT)
    # ACPPHUD draws the drag box in FLinearColor::Blue with alpha 0.4; fill only, as render opacity fades it.
    edits += rb('MarqueeBox', 'Brush', '(SpecifiedColor=(R=0,G=0,B=1,A=0.4))', (0, 0, 0, 0))
    edits += [('MarqueeBox', 'RenderTransformPivot', '(X=0,Y=0)'), ('MarqueeBox', 'RenderOpacity', '0')]
    edits += rb('MiniMouseBody', 'Background', KEY_FILL, (16, 16, 14, 14), 1.5, OUT)
    edits += rb('LeftButtonGlow', 'Brush', ACC % '0.85', (13, 2, 2, 2))
    edits += [('LeftButtonGlow', 'RenderOpacity', '0'), ('MiniSplit', 'ColorAndOpacity', LINE_LIN),
              ('MiniLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    brushes = [{'widget_name': 'Unit_%d' % i, 'texture_path': PANZER_ICON, 'image_size': [UNIT_SIZE, UNIT_SIZE]}
               for i in range(1, 4)]
    brushes += [{'widget_name': 'Unit_4', 'texture_path': VULTURE_ICON, 'image_size': [UNIT_SIZE, UNIT_SIZE]},
                {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}]
    brushes += [{'widget_name': 'Decal_%d' % i, 'material_path': BOX_DECAL,
                 'image_size': [BOX_DECAL_SIZE, BOX_DECAL_SIZE]} for i in range(1, 4)]
    brushes += [{'widget_name': 'Decal_4', 'material_path': ROUND_DECAL,
                 'image_size': [ROUND_DECAL_SIZE, ROUND_DECAL_SIZE]}]
    # set_image_brush resets DrawAs; the rounded corners go on after the textures.
    uecp.widget.set_image_brush(widget_path=path, brushes=brushes)
    style(path, edits, [])
    bind(path, animation, 'MouseCursor', 5.0)
    track(path, animation, 'LeftButtonGlow', 'opacity', [(0, 0), (0.5, 0), (0.58, 1), (2.0, 1), (2.08, 0), (5.0, 0)])
    track(path, animation, 'MarqueeBox', 'opacity', [(0, 0), (0.55, 0), (0.6, 1), (2.05, 1), (2.45, 0), (5.0, 0)])
    # The decals light up on release (2 s), like SelectUnitsFromMarquee after the HUD collects the boxed units.
    for i in range(1, 5):
        track(path, animation, 'Decal_%d' % i, 'opacity', [(0, 0), (2.0, 0), (2.12, 1), (4.3, 1), (4.6, 0), (5.0, 0)])
    finish(path)


# ---------------------------------------------------------------- panning page: the map turns and tilts, then resets
# What RTS_BP_PlayerController does while IA_PanCamera (middle mouse button or P) is held: Mouse X adds to the camera
# pawn's yaw, Mouse Y to its pitch (clamped to +-GetCameraPitchLimit); Alt + middle mouse (IA_PanReset, chord IA_Alt)
# calls UPlayerCameraController::PanReset. Turning the camera right turns the ground on screen the other way.
def build_panning():
    animation = 'PanningLoop'
    children = [{'name': 'MapClip', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(0, 0, 520, 292),
                 'children': [{'name': 'MapCanvas', 'type': 'CanvasPanel', 'children': [
                     # Tilt squashes the ground along the screen's vertical, so it sits outside the turning image.
                     {'name': 'TiltLayer', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(0, 0, 520, 292),
                      'children': [{'name': 'TiltCanvas', 'type': 'CanvasPanel', 'children': [
                          {'name': 'MinimapImage', 'type': 'Image', 'slot': p(-90, -204, 700, 700)}]}]}]}]},
                {'name': 'CompassDial', 'type': 'Border', 'slot': p(452, 14, 54, 54)},
                {'name': 'CompassNeedle', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(452, 14, 54, 54),
                 'children': [text('CompassN', 'N', 14, ACC_LIN % '1',
                                   {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Top',
                                    'Padding': '(Left=0,Top=3,Right=0,Bottom=0)'})]},
                {'name': 'MouseCursor', 'type': 'Image', 'slot': p(247, 133, 26, 26)},
                {'name': 'KeyBase_Alt', 'type': 'Border', 'slot': p(398, 240, 56, 38)},
                {'name': 'Key_Alt', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': p(398, 236, 56, 38),
                 'children': [{'name': 'KeyStack_Alt', 'type': 'Overlay', 'children': [
                     {'name': 'KeyGlow_Alt', 'type': 'Image', 'slot': {'Padding': '3'}},
                     text('Glyph_Alt', 'Alt', 15, TXT_LIN % '1',
                          {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}]},
                {'name': 'MiniMouseBody', 'type': 'Border', 'slot': p(466, 222, 36, 54)},
                {'name': 'MiniLine', 'type': 'Image', 'slot': p(468, 246, 32, 1.5)},
                {'name': 'MiniSplitTop', 'type': 'Image', 'slot': p(483.25, 224, 1.5, 4)},
                {'name': 'WheelSlot', 'type': 'Border', 'slot': p(479.5, 228, 9, 15)},
                {'name': 'WheelGlow', 'type': 'Image', 'slot': p(479.5, 228, 9, 15)},
                {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, 520, 292)}]
    path = rebuild('WBP_CamCtl_Panning', animation, 8.0, children, (520, 292))
    edits = [('MapClip', 'Background.DrawAs', 'NoDrawType'), ('MapClip', 'Clipping', 'ClipToBounds'),
             ('TiltLayer', 'Background.DrawAs', 'NoDrawType'), ('CompassNeedle', 'Background.DrawAs', 'NoDrawType')]
    edits += rb('CompassDial', 'Background', '(SpecifiedColor=(R=0.003,G=0.004,B=0.005,A=0.75))', (27, 27, 27, 27),
                1.5, OUT)
    edits += rb('KeyBase_Alt', 'Background', BASE_FILL, (7, 7, 7, 7))
    edits += rb('Key_Alt', 'Background', KEY_FILL, (6, 6, 6, 6), 1.5, OUT)
    edits += rb('KeyGlow_Alt', 'Brush', ACC % '0.55', (4, 4, 4, 4))
    edits += [('KeyGlow_Alt', 'RenderOpacity', '0')]
    edits += rb('MiniMouseBody', 'Background', KEY_FILL, (16, 16, 14, 14), 1.5, OUT)
    edits += [('MiniLine', 'ColorAndOpacity', LINE_LIN), ('MiniSplitTop', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('WheelSlot', 'Background', BASE_FILL, (4, 4, 4, 4), 1, ACC % '0.8')
    # Fill only: render opacity does not fade a rounded box's outline.
    edits += rb('WheelGlow', 'Brush', ACC % '0.9', (4, 4, 4, 4))
    edits += [('WheelGlow', 'RenderOpacity', '0')]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    edits += minimap_crop('MinimapImage')
    style(path, edits, [{'widget_name': 'MinimapImage', 'texture_path': MINIMAP, 'image_size': [700, 700]},
                        {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}])
    for name in ('MinimapImage', 'TiltLayer', 'CompassNeedle', 'MouseCursor', 'Key_Alt'):
        bind(path, animation, name, 8.0)
    # Held from 0.5 s while the mouse turns (to 2.4 s) and tilts (2.6 to 3.9 s), let go at 4 s; then Alt + a click
    # resets the view at 5.65 s.
    track(path, animation, 'WheelGlow', 'opacity', [(0, 0), (0.4, 0), (0.48, 1), (4.0, 1), (4.08, 0), (5.55, 0),
                                                   (5.6, 1), (6.0, 1), (6.08, 0), (8.0, 0)])
    track(path, animation, 'KeyGlow_Alt', 'opacity', [(0, 0), (5.2, 0), (5.28, 1), (6.3, 1), (6.4, 0), (8.0, 0)])
    finish(path)


# Set CAMCTL_TARGETS before exec() to rebuild only some of them, e.g. ['marquee'].
BUILDERS = {'keyboard': build_keyboard_pan, 'edge': build_edge_pan, 'zoom': build_zoom, 'panning': build_panning,
            'marquee': build_marquee}
for target in globals().get('CAMCTL_TARGETS', list(BUILDERS)):
    BUILDERS[target]()
print('\n'.join(REPORT))
