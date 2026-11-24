"""Builds the building expansion pack-up takeover's looping illustration WBP_BxpPack_Loop (widget tree, look, opacity
tracks) from scratch, in GameUI/InfoTakeover/BxpPackUp.

Run inside the live editor through UECP's execute_uecp_script (it needs the `uecp` object) with allow_destructive=true,
e.g. exec(open(r'<this file>').read()). It needs no new C++ class. Then run bxp_packup_transform_tracks.py for the
moving parts (cursor, expansions flying into the truck, the truck's drive, previews, growing, progress bars).

The loop (PackLoop, 12 s), all from the real game (checked 2026-11-12):
- The German HQ with two expansions: the radar (BTX_GerHQRadar, a Socket expansion, slot picture T_GER_BXP_HQRadar
  from DT_Items) on the building, and a 37 mm Flak (BTX_37mmFlak, a Free expansion, slot picture
  T_BXP_37mmFlak_Render) beside it. The HQ is used, not the refinery: the Strike Division refinery has no free
  expansion (AddStrikeDivisionBxpOptions removes its 37 mm Flak).
- 1.15 s: the cursor clicks the pack-up button (DT_TruckButtons Nomadic_GerHQ: T_Ger_HQTruck, "Convert back into HQ
  Truck"). Both slots show W_BuildingItem's BXS_BeingPackedUp picture (T_IsPackingBXP); the radar and the flak fold
  into the building, which turns into the truck. Then both slots show BXS_PackedUp (T_PlacePackedUpBuilding,
  "Place Packed Up Building"), greyed out while it is a truck (UMainGameUI::OnTruckConverted).
- 4.0 s: the truck is converted again ("Convert into HQ building"), placed, drives there and unfolds.
- 7.0 s: the radar rebuilds by itself at its old place (OnConvertToBuilding_PlacePackedBxps): its slot shows
  BXS_BeingUnpacked (T_CancelPackedUpBuilding) and then the radar again. The flak slot stays packed.
- 8.6 s: the cursor clicks the flak's slot; a preview inside the expansion range follows the cursor and a left click
  places it (no payment for a packed expansion); it builds and its slot shows the flak again.
- 11.6 to 12 s: the second building fades into the first for the next loop.

Art rule (user, 2026-11-11): no drawn art except the mouse/cursor (T_CamCtl_Cursor); the rest is game art or plain UMG.
Anything that fades is fill-only: render opacity does not fade a rounded box's outline.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/BxpPackUp'
CAMERA_FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls'
CURSOR = CAMERA_FOLDER + '/T_CamCtl_Cursor'
ICONS = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/'
HQ_ICON = ICONS + 'Ger_Buildings/GerHQ/T_Ger_HQ'
HQ_TRUCK_ICON = ICONS + 'Ger_Buildings/GerHQ/T_Ger_HQTruck'
RADAR_ICON = ICONS + 'BuildingExpansions/T_GER_BXP_HQRadar'
FLAK_ICON = ICONS + 'BuildingExpansions/T_BXP_37mmFlak_Render'
SLOT_ICONS = '/Game/RTS_Survival/Textures/GameUI/BuildingButtons/BuildingPlacement/'
SLOT_PACKING = SLOT_ICONS + 'T_IsPackingBXP_v002_0000'
SLOT_PACKED = SLOT_ICONS + 'T_PlacePackedUpBuilding_v001_0000'
SLOT_UNPACKING = SLOT_ICONS + 'T_CancelPackedUpBuilding_v001_0000'
WIDGET = 'WBP_BxpPack_Loop'
ANIMATION = 'PackLoop'
DURATION = 12.0
SIZE = (520, 292)

# The other field manuals' palette, so they read alike.
# Accent (progress bars, button glows) = the takeover style's label red (option G Rotsignal, #c24a3a in linear).
ACC = '(SpecifiedColor=(R=0.548,G=0.0658,B=0.0385,A=%s))'
ACC_LIN = '(R=0.548,G=0.0658,B=0.0385,A=%s)'
TXT_LIN = '(R=0.83,G=0.80,B=0.72,A=%s)'
LINE_LIN = '(R=0.16,G=0.18,B=0.20,A=1)'
OUT = '(SpecifiedColor=(R=0.16,G=0.18,B=0.20,A=1))'
CLEAR = '(SpecifiedColor=(R=0,G=0,B=0,A=0))'
KEY_FILL = '(SpecifiedColor=(R=0.03,G=0.034,B=0.038,A=1))'
HUD_FILL = '(SpecifiedColor=(R=0.006,G=0.007,B=0.008,A=0.92))'
TOOLTIP_FILL = '(SpecifiedColor=(R=0.005,G=0.006,B=0.007,A=0.94))'
GROUND_FILL = '(SpecifiedColor=(R=0.018,G=0.02,B=0.018,A=1))'
DIM_FILL = '(SpecifiedColor=(R=0.004,G=0.005,B=0.006,A=1))'
# A valid building preview footprint, as in the nomadic conversion illustration.
PREVIEW_FILL = '(SpecifiedColor=(R=0.1,G=0.75,B=0.25,A=0.32))'
# The free expansion's placing range: a faint disc around the building.
RANGE_FILL = '(SpecifiedColor=(R=0.1,G=0.75,B=0.25,A=0.08))'
REPORT = []

# Layout (illustration pixels). The first HQ stands left; the truck drives DRIVE_X to the right.
DRIVE_X = 240
HQ_A = (80, 40, 120, 106)                 # centre (140, 93); T_Ger_HQ is 256 x 226
RADAR_A = (190, 50, 44, 44)               # centre (212, 72), on the building's corner
FLAK_A = (40, 150, 52, 52)                # centre (66, 176), free-standing
PROGRESS_A = (95, 152, 90, 6)
TRUCK_GROUP = (98, 51, 84, 84)            # centre (140, 93)
CARGO_RADAR = (150, 45, 22, 22)
CARGO_FLAK = (128, 45, 22, 22)
HQ_B = (HQ_A[0] + DRIVE_X, 40, 120, 106)  # centre (380, 93)
RADAR_B = (RADAR_A[0] + DRIVE_X, 50, 44, 44)
RADAR_PROGRESS = (RADAR_B[0], 98, 44, 4)
FLAK_B = (274, 150, 52, 52)               # centre (300, 176)
FLAK_PROGRESS = (276, 206, 48, 4)
PROGRESS_B = (PROGRESS_A[0] + DRIVE_X, 152, 90, 6)
FLAK_PREVIEW = (268, 144, 64, 64)         # centre (300, 176)
RANGE_DISC = (230, -57, 300, 300)         # centre (380, 93)
# Bottom panel: the convert / pack-up button and the two expansion slots.
BUTTON_FRAME = (150, 234, 52, 52)         # centre (176, 260)
SLOT_FRAMES = ((250, 234, 52, 52), (310, 234, 52, 52))  # centres (276, 260), (336, 260)
SLOT_STATES = ('Built', 'Packing', 'Packed', 'Unpacking')


def p(x, y, w, h):
    return {'position': [x, y], 'size': [w, h], 'anchors': 'top_left'}


def pr(rect):
    return p(*rect)


def inset(rect, by):
    return (rect[0] + by, rect[1] + by, rect[2] - 2 * by, rect[3] - 2 * by)


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


def preview_group(name, rect, icon_padding):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '0'}, 'slot': pr(rect),
            'children': [{'name': name + 'Stack', 'type': 'Overlay', 'children': [
                {'name': name + 'Fill', 'type': 'Image',
                 'slot': {'Padding': '0', 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}},
                {'name': name + 'Icon', 'type': 'Image',
                 'slot': {'Padding': icon_padding, 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}}]}]}


def progress(name, rect):
    return [{'name': name + 'Back', 'type': 'Image', 'slot': pr(rect)},
            {'name': name + 'Fill', 'type': 'Image', 'slot': pr(rect)}]


def tooltip_box(name, value, rect):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '(Left=8,Top=3,Right=8,Bottom=3)'},
            'slot': pr(rect), 'children': [text(name + 'Text', value, 13, TXT_LIN % '1',
                                                {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}


CAPTION_LIN = ACC_LIN


def caption(name, value):
    return text(name, value, 12, CAPTION_LIN % '1', p(12, 8, 400, 18), 'Left')


def slot_widgets(index):
    frame = SLOT_FRAMES[index]
    name = 'Slot%d' % (index + 1)
    children = [{'name': name + 'Frame', 'type': 'Border', 'slot': pr(frame)}]
    children += [{'name': '%s_%s' % (name, state), 'type': 'Image', 'slot': pr(inset(frame, 4))}
                 for state in SLOT_STATES]
    return children


def layout():
    children = [{'name': 'Ground', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])},
                {'name': 'RangeDisc', 'type': 'Image', 'slot': pr(RANGE_DISC)},
                preview_group('FlakPreview', FLAK_PREVIEW, '(Left=7,Top=7,Right=7,Bottom=7)'),
                preview_group('HqPreview', HQ_B, '(Left=6,Top=6,Right=6,Bottom=6)'),
                # The first HQ with its radar on the building and its flak beside it.
                {'name': 'HQ_A', 'type': 'Image', 'slot': pr(HQ_A)},
                {'name': 'Radar_A', 'type': 'Image', 'slot': pr(RADAR_A)},
                {'name': 'Flak_A', 'type': 'Image', 'slot': pr(FLAK_A)}]
    children += progress('ProgressA', PROGRESS_A)
    # The redeployed HQ: the radar comes back by itself, the flak is placed from its slot.
    children += [{'name': 'HQ_B', 'type': 'Image', 'slot': pr(HQ_B)},
                 {'name': 'Radar_B', 'type': 'Image', 'slot': pr(RADAR_B)},
                 {'name': 'Flak_B', 'type': 'Image', 'slot': pr(FLAK_B)}]
    children += progress('RadarProgress', RADAR_PROGRESS)
    children += progress('FlakProgress', FLAK_PROGRESS)
    children += progress('ProgressB', PROGRESS_B)
    children += [{'name': 'TruckGroup', 'type': 'Border', 'properties': {'padding': '0'}, 'slot': pr(TRUCK_GROUP),
                  'children': [{'name': 'TruckIcon', 'type': 'Image',
                                'slot': {'Padding': '8', 'HorizontalAlignment': 'Fill',
                                         'VerticalAlignment': 'Fill'}}]},
                 # The packed expansions ride with the truck (they stay in its slots).
                 {'name': 'CargoFlak', 'type': 'Image', 'slot': pr(CARGO_FLAK)},
                 {'name': 'CargoRadar', 'type': 'Image', 'slot': pr(CARGO_RADAR)},
                 caption('CaptionPack', 'PACK UP: THE EXPANSIONS FOLD AWAY'),
                 caption('CaptionDrive', 'MOVE AND DEPLOY AGAIN'),
                 caption('CaptionAuto', 'ATTACHED: REBUILDS BY ITSELF'),
                 caption('CaptionPlace', 'FREE-STANDING: PLACE IT FROM ITS SLOT'),
                 # The bottom panel with the pack-up / convert button and the two expansion slots.
                 {'name': 'HudStrip', 'type': 'Border', 'slot': p(0, 226, SIZE[0], 66)},
                 {'name': 'HudLine', 'type': 'Image', 'slot': p(0, 226, SIZE[0], 1.5)},
                 {'name': 'ButtonFrame', 'type': 'Border', 'slot': pr(BUTTON_FRAME)},
                 {'name': 'ButtonImage_Truck', 'type': 'Image', 'slot': p(154, 238, 44, 44)},
                 {'name': 'ButtonImage_Building', 'type': 'Image', 'slot': p(154, 241, 44, 39)},
                 {'name': 'ButtonGlow', 'type': 'Image', 'slot': p(152, 236, 48, 48)}]
    children += slot_widgets(0)
    children += slot_widgets(1)
    children += [{'name': 'Slot2Glow', 'type': 'Image', 'slot': p(312, 236, 48, 48)},
                 # Greyed out while it is a truck: the game disables the slots (UMainGameUI::OnTruckConverted).
                 {'name': 'SlotsDim', 'type': 'Image', 'slot': p(248, 232, 116, 56)},
                 tooltip_box('TooltipPack', 'Convert back into HQ Truck', (66, 194, 220, 28)),
                 tooltip_box('TooltipBuild', 'Convert into HQ building', (66, 194, 220, 28)),
                 tooltip_box('TooltipPlace', 'Place Packed Up Building', (226, 194, 220, 28)),
                 {'name': 'MiniMouseBody', 'type': 'Border', 'slot': p(476, 234, 32, 50)},
                 {'name': 'LeftButtonGlow', 'type': 'Image', 'slot': p(478, 236, 13, 20)},
                 {'name': 'MiniSplit', 'type': 'Image', 'slot': p(491.25, 236, 1.5, 20)},
                 {'name': 'MiniLine', 'type': 'Image', 'slot': p(478, 256, 28, 1.5)},
                 # Positioned by its translation track (tip coordinates minus the image's tip offset).
                 {'name': 'MouseCursor', 'type': 'Image', 'slot': p(0, 0, 26, 26)},
                 {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])}]
    return children


def hidden_at_start():
    names = ['RangeDisc', 'FlakPreview', 'HqPreview', 'HQ_B', 'Radar_B', 'Flak_B', 'TruckGroup', 'CargoFlak',
             'CargoRadar', 'CaptionPack', 'CaptionDrive', 'CaptionAuto', 'CaptionPlace', 'ButtonImage_Building',
             'ButtonGlow', 'Slot2Glow', 'SlotsDim', 'TooltipPack', 'TooltipBuild', 'TooltipPlace', 'LeftButtonGlow']
    for bar in ('ProgressA', 'RadarProgress', 'FlakProgress', 'ProgressB'):
        names += [bar + 'Back', bar + 'Fill']
    for index in (1, 2):
        names += ['Slot%d_%s' % (index, state) for state in SLOT_STATES[1:]]
    return names


def look(path):
    edits = rb('Ground', 'Background', GROUND_FILL, (2, 2, 2, 2))
    edits += rb('RangeDisc', 'Brush', RANGE_FILL, (150, 150, 150, 150))
    edits += [('RangeDisc', 'Brush.ImageSize', '(X=%d,Y=%d)' % RANGE_DISC[2:])]
    for group, rect in (('FlakPreview', FLAK_PREVIEW), ('HqPreview', HQ_B)):
        edits += [(group, 'Background.DrawAs', 'NoDrawType')]
        edits += rb(group + 'Fill', 'Brush', PREVIEW_FILL, (4, 4, 4, 4))
        # Overlay slots may not stretch their child, so the fills carry their full size in the brush.
        edits += [(group + 'Fill', 'Brush.ImageSize', '(X=%d,Y=%d)' % rect[2:]),
                  (group + 'Icon', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.6)')]
    edits += [('TruckGroup', 'Background.DrawAs', 'NoDrawType')]
    for bar in ('ProgressA', 'RadarProgress', 'FlakProgress', 'ProgressB'):
        edits += [(bar + 'Back', 'ColorAndOpacity', '(R=0.003,G=0.004,B=0.005,A=0.9)'),
                  (bar + 'Fill', 'ColorAndOpacity', ACC_LIN % '1'),
                  (bar + 'Fill', 'RenderTransformPivot', '(X=0,Y=0.5)')]
    edits += rb('HudStrip', 'Background', HUD_FILL, (0, 0, 0, 0))
    edits += [('HudLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ButtonFrame', 'Background', KEY_FILL, (6, 6, 6, 6), 1.5, OUT)
    edits += rb('ButtonGlow', 'Brush', ACC % '0.45', (5, 5, 5, 5))
    edits += rb('Slot2Glow', 'Brush', ACC % '0.45', (5, 5, 5, 5))
    for index in (1, 2):
        edits += rb('Slot%dFrame' % index, 'Background', KEY_FILL, (6, 6, 6, 6), 1.5, OUT)
    edits += rb('SlotsDim', 'Brush', DIM_FILL, (6, 6, 6, 6))
    for name in ('TooltipPack', 'TooltipBuild', 'TooltipPlace'):
        edits += rb(name, 'Background', TOOLTIP_FILL, (4, 4, 4, 4))
    edits += [('CargoFlak', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.95)'),
              ('CargoRadar', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.95)')]
    edits += rb('MiniMouseBody', 'Background', KEY_FILL, (15, 15, 13, 13), 1.5, OUT)
    edits += rb('LeftButtonGlow', 'Brush', ACC % '0.85', (12, 2, 2, 2))
    edits += [('MiniSplit', 'ColorAndOpacity', LINE_LIN), ('MiniLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    edits += [(name, 'RenderOpacity', '0') for name in hidden_at_start()]
    slot_size = [SLOT_FRAMES[0][2] - 8, SLOT_FRAMES[0][3] - 8]
    # Brushes first: set_image_brush resets the brush, so draw settings go on afterwards.
    brushes = [{'widget_name': 'HQ_A', 'texture_path': HQ_ICON, 'image_size': list(HQ_A[2:])},
               {'widget_name': 'HQ_B', 'texture_path': HQ_ICON, 'image_size': list(HQ_B[2:])},
               {'widget_name': 'Radar_A', 'texture_path': RADAR_ICON, 'image_size': list(RADAR_A[2:])},
               {'widget_name': 'Radar_B', 'texture_path': RADAR_ICON, 'image_size': list(RADAR_B[2:])},
               {'widget_name': 'Flak_A', 'texture_path': FLAK_ICON, 'image_size': list(FLAK_A[2:])},
               {'widget_name': 'Flak_B', 'texture_path': FLAK_ICON, 'image_size': list(FLAK_B[2:])},
               {'widget_name': 'CargoRadar', 'texture_path': RADAR_ICON, 'image_size': list(CARGO_RADAR[2:])},
               {'widget_name': 'CargoFlak', 'texture_path': FLAK_ICON, 'image_size': list(CARGO_FLAK[2:])},
               {'widget_name': 'FlakPreviewIcon', 'texture_path': FLAK_ICON, 'image_size': [50, 50]},
               {'widget_name': 'HqPreviewIcon', 'texture_path': HQ_ICON, 'image_size': [108, 94]},
               {'widget_name': 'TruckIcon', 'texture_path': HQ_TRUCK_ICON, 'image_size': [84, 84]},
               {'widget_name': 'ButtonImage_Truck', 'texture_path': HQ_TRUCK_ICON, 'image_size': [44, 44]},
               {'widget_name': 'ButtonImage_Building', 'texture_path': HQ_ICON, 'image_size': [44, 39]},
               {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}]
    slot_textures = {'Packing': SLOT_PACKING, 'Packed': SLOT_PACKED, 'Unpacking': SLOT_UNPACKING}
    for index, built_icon in ((1, RADAR_ICON), (2, FLAK_ICON)):
        brushes.append({'widget_name': 'Slot%d_Built' % index, 'texture_path': built_icon, 'image_size': slot_size})
        for state, texture in slot_textures.items():
            brushes.append({'widget_name': 'Slot%d_%s' % (index, state), 'texture_path': texture,
                            'image_size': slot_size})
    uecp.widget.set_image_brush(widget_path=path, brushes=brushes)
    uecp.widget.edit_widget_properties(widget_path=path, edits=[{'widget_name': a, 'property_name': b, 'value': c}
                                                                for a, b, c in edits])


def slot_tracks(path):
    # W_BuildingItem's picture per status: built (the expansion), BeingPackedUp, PackedUp, BeingUnpacked.
    # The radar slot is unpacked by itself after the deploy; the flak slot when the player places it.
    track(path, 'Slot1_Built', 'opacity', [(0, 1), (1.3, 1), (1.35, 0), (8.55, 0), (8.6, 1), (DURATION, 1)])
    track(path, 'Slot1_Packing', 'opacity', window(1.3, 3.25, 0.05))
    track(path, 'Slot1_Packed', 'opacity', window(3.25, 7.0, 0.05))
    track(path, 'Slot1_Unpacking', 'opacity', window(7.0, 8.55, 0.05))
    track(path, 'Slot2_Built', 'opacity', [(0, 1), (1.3, 1), (1.35, 0), (11.05, 0), (11.1, 1), (DURATION, 1)])
    track(path, 'Slot2_Packing', 'opacity', window(1.3, 3.25, 0.05))
    track(path, 'Slot2_Packed', 'opacity', window(3.25, 8.6, 0.05))
    track(path, 'Slot2_Unpacking', 'opacity', window(8.6, 11.05, 0.05))
    # Greyed out from the end of the pack-up until the building stands again.
    track(path, 'SlotsDim', 'opacity', window(3.25, 6.9, 0.1, high=0.55))


def button_tracks(path):
    # The pack-up button (truck picture) until it is clicked; the convert button (building picture) while a truck.
    track(path, 'ButtonImage_Truck', 'opacity', [(0, 1), (1.3, 1), (1.4, 0), (6.9, 0), (7.0, 1), (DURATION, 1)])
    track(path, 'ButtonImage_Building', 'opacity', window(3.25, 4.05, 0.08))
    track(path, 'TooltipPack', 'opacity', window(0.95, 1.2, 0.08))
    track(path, 'TooltipBuild', 'opacity', window(3.75, 4.0, 0.08))
    track(path, 'TooltipPlace', 'opacity', window(8.25, 8.6, 0.08))
    track(path, 'ButtonGlow', 'opacity', clicks([1.15, 4.0], 0.2))
    track(path, 'Slot2Glow', 'opacity', clicks([8.6], 0.2))
    track(path, 'LeftButtonGlow', 'opacity', clicks([1.15, 4.0, 5.1, 8.6, 9.85]))


def scene_tracks(path):
    # Pack-up (1.3 to 3.25 s): the expansions fold into the building, which turns into the truck.
    track(path, 'CaptionPack', 'opacity', window(1.2, 3.3, 0.15))
    track(path, 'Radar_A', 'opacity', [(0, 1), (1.35, 1), (2.7, 0), (11.6, 0), (11.9, 1), (DURATION, 1)])
    track(path, 'Flak_A', 'opacity', [(0, 1), (1.5, 1), (2.8, 0), (11.6, 0), (11.9, 1), (DURATION, 1)])
    track(path, 'HQ_A', 'opacity', [(0, 1), (2.5, 1), (3.2, 0), (11.6, 0), (11.9, 1), (DURATION, 1)])
    track(path, 'ProgressABack', 'opacity', window(1.3, 3.25, 0.1))
    track(path, 'ProgressAFill', 'opacity', window(1.3, 3.25, 0.1))
    track(path, 'TruckGroup', 'opacity', [(0, 0), (2.5, 0), (3.2, 1), (6.0, 1), (6.7, 0), (DURATION, 0)])
    track(path, 'CargoRadar', 'opacity', window(2.7, 6.0, 0.25))
    track(path, 'CargoFlak', 'opacity', window(2.8, 6.0, 0.25))
    # Converted again (4.0 s), placed (5.1 s), drives and unfolds (to 6.9 s).
    track(path, 'CaptionDrive', 'opacity', window(3.4, 6.9, 0.15))
    track(path, 'HqPreview', 'opacity', [(0, 0), (4.05, 0), (4.15, 1), (5.1, 1), (5.2, 0.45), (6.4, 0.45),
                                         (6.9, 0), (DURATION, 0)])
    track(path, 'HQ_B', 'opacity', [(0, 0), (6.0, 0), (6.9, 1), (11.6, 1), (11.9, 0), (DURATION, 0)])
    track(path, 'ProgressBBack', 'opacity', window(5.95, 6.9, 0.1))
    track(path, 'ProgressBFill', 'opacity', window(5.95, 6.9, 0.1))
    # The radar rebuilds by itself at its socket (7.0 to 8.55 s).
    track(path, 'CaptionAuto', 'opacity', window(7.0, 8.4, 0.15))
    track(path, 'Radar_B', 'opacity', [(0, 0), (7.0, 0), (7.1, 0.5), (8.55, 1), (11.6, 1), (11.9, 0),
                                       (DURATION, 0)])
    track(path, 'RadarProgressBack', 'opacity', window(7.0, 8.55, 0.1))
    track(path, 'RadarProgressFill', 'opacity', window(7.0, 8.55, 0.1))
    # The flak is placed from its slot (8.6 s), inside the expansion range, and builds (9.85 to 11.05 s).
    track(path, 'CaptionPlace', 'opacity', window(8.5, 11.4, 0.15))
    track(path, 'RangeDisc', 'opacity', window(8.65, 9.9, 0.15))
    track(path, 'FlakPreview', 'opacity', window(8.65, 9.9, 0.1))
    track(path, 'Flak_B', 'opacity', [(0, 0), (9.85, 0), (9.95, 0.5), (11.05, 1), (11.6, 1), (11.9, 0),
                                      (DURATION, 0)])
    track(path, 'FlakProgressBack', 'opacity', window(9.9, 11.05, 0.1))
    track(path, 'FlakProgressFill', 'opacity', window(9.9, 11.05, 0.1))


def opacity_tracks(path):
    # Widgets that only move get a constant track, so the transform script finds a binding.
    for name in ('MouseCursor', 'TruckIcon'):
        bind(path, name)
    slot_tracks(path)
    button_tracks(path)
    scene_tracks(path)


def build_loop():
    path = rebuild(layout())
    look(path)
    opacity_tracks(path)
    uecp.blueprint.compile_blueprint(blueprint_path=path)
    REPORT.append('%s saved=%s' % (WIDGET, unreal.EditorAssetLibrary.save_asset(path, False)))


build_loop()
print('\n'.join(REPORT))
