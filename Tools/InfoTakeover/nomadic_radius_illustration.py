"""Builds the nomadic conversion takeover's second illustration WBP_NomCnv_Radius (build radius page): widget tree, look,
opacity tracks and the RenderTransform tracks of the moving parts, from scratch, in GameUI/InfoTakeover/NomadicConversion.

Run inside the live editor through UECP's execute_uecp_script (it needs the `uecp` object) with allow_destructive=true,
e.g. exec(open(r'<this file>').read()). Same art rules as nomadic_conversion_illustration.py: no drawn art except the
cursor; game unit icons and plain UMG. Anything that fades is fill-only (render opacity does not fade a rounded box's
outline), so the radius ring appears and leaves by scale.

The loop (RadiusLoop, 10 s), from the game (ACPPController: bUseBuildRadius = NomadicVehicle != M_PlayerHQ; the nomadic
preview carries the radius its building will give, NomadicPreviewAttachmentState):
- 0.8 s: the HQ truck is selected; its preview, with the build radius the HQ will give, is dragged to open ground far
  from anything and placed at 2.85 s: the HQ needs no build radius. The truck turns into the HQ; its radius stays.
- 5.35 s: a refinery truck is selected. Its preview is dragged outside the HQ's radius: red, and the click at 6.6 s is
  refused ("Outside the build radius"). Dragged inside, it turns green and is placed at 8.05 s.
- 9.3 to 10 s: everything resets.
"""
import re

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion'
CURSOR = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls/T_CamCtl_Cursor'
BOX_DECAL = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls/M_UI_SelectionDecal_Box'
ICONS = '/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Ger_Buildings/'
HQ_ICON = ICONS + 'GerHQ/T_Ger_HQ'
HQ_TRUCK_ICON = ICONS + 'GerHQ/T_Ger_HQTruck'
REF_ICON = ICONS + 'GerRef/T_Ger_Refinery'
REF_TRUCK_ICON = ICONS + 'GerRef/T_Ger_RefineryTruck'
WIDGET = 'WBP_NomCnv_Radius'
ANIMATION = 'RadiusLoop'
DURATION = 10.0
SIZE = (520, 292)

# Option G (Rotsignal) label red for captions and click glows, as the BXP pack-up illustration.
ACC = '(SpecifiedColor=(R=0.548,G=0.0658,B=0.0385,A=%s))'
ACC_LIN = '(R=0.548,G=0.0658,B=0.0385,A=%s)'
TXT_LIN = '(R=0.83,G=0.80,B=0.72,A=%s)'
LINE_LIN = '(R=0.16,G=0.18,B=0.20,A=1)'
OUT = '(SpecifiedColor=(R=0.16,G=0.18,B=0.20,A=1))'
CLEAR = '(SpecifiedColor=(R=0,G=0,B=0,A=0))'
KEY_FILL = '(SpecifiedColor=(R=0.03,G=0.034,B=0.038,A=1))'
TOOLTIP_FILL = '(SpecifiedColor=(R=0.005,G=0.006,B=0.007,A=0.94))'
GROUND_FILL = '(SpecifiedColor=(R=0.018,G=0.02,B=0.018,A=1))'
# The building preview's footprint: a valid spot (as WBP_NomCnv_Deploy) and a refused one.
PREVIEW_GOOD = '(SpecifiedColor=(R=0.1,G=0.75,B=0.25,A=0.32))'
PREVIEW_BAD = '(SpecifiedColor=(R=0.8,G=0.1,B=0.06,A=0.36))'
RING_FILL = '(SpecifiedColor=(R=0.83,G=0.80,B=0.72,A=0.05))'
RING_LINE = '(SpecifiedColor=(R=0.83,G=0.80,B=0.72,A=0.6))'
REPORT = []

# Layout (illustration pixels). Both trucks start bottom left; the HQ goes to open ground, the refinery beside it.
TRUCK_CENTRE = (90, 205)
HQ_CENTRE = (300, 115)
RADIUS = 115
REF_CENTRE = (380, 175)
OUTSIDE_CENTRE = (120, 80)
TRUCK_BOX = 80
HQ_PREVIEW = (104, 92)
REF_PREVIEW = (70, 62)


def p(x, y, w, h):
    return {'position': [x, y], 'size': [w, h], 'anchors': 'top_left'}


def centred(centre, size):
    return p(centre[0] - size[0] / 2.0, centre[1] - size[1] / 2.0, size[0], size[1])


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


def fill_slot(padding='0'):
    return {'Padding': padding, 'HorizontalAlignment': 'Fill', 'VerticalAlignment': 'Fill'}


def icon_fit(texture_path, box):
    """Image size that fits the texture's aspect inside box (w, h)."""
    texture = unreal.load_asset(texture_path)
    width, height = float(texture.blueprint_get_size_x()), float(texture.blueprint_get_size_y())
    scale = min(box[0] / width, box[1] / height)
    return [round(width * scale), round(height * scale)]


def preview_group(name, centre, size, fills):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '0'}, 'slot': centred(centre, size),
            'children': [{'name': name + 'Stack', 'type': 'Overlay', 'children':
                          [{'name': fill, 'type': 'Image', 'slot': fill_slot()} for fill in fills] +
                          [{'name': name + 'Icon', 'type': 'Image', 'slot': fill_slot('6')}]}]}


def truck_group(name):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '0'},
            'slot': centred(TRUCK_CENTRE, (TRUCK_BOX, TRUCK_BOX)),
            'children': [{'name': name + 'Stack', 'type': 'Overlay', 'children': [
                {'name': name + 'Decal', 'type': 'Image', 'slot': fill_slot()},
                {'name': name + 'Icon', 'type': 'Image', 'slot': fill_slot('8')}]}]}


def tooltip_box(name, value, rect):
    return {'name': name, 'type': 'Border', 'properties': {'padding': '(Left=8,Top=3,Right=8,Bottom=3)'},
            'slot': p(*rect), 'children': [text(name + 'Text', value, 13, TXT_LIN % '1',
                                                {'HorizontalAlignment': 'Center', 'VerticalAlignment': 'Center'})]}


def layout():
    return [{'name': 'Ground', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])},
            # The HQ's build radius: rides with the HQ preview, then stays around the HQ.
            {'name': 'RadiusRing', 'type': 'Image', 'slot': centred(HQ_CENTRE, (2 * RADIUS, 2 * RADIUS))},
            {'name': 'HQBuilding', 'type': 'Image', 'slot': centred(HQ_CENTRE, HQ_PREVIEW)},
            {'name': 'RefBuilding', 'type': 'Image', 'slot': centred(REF_CENTRE, REF_PREVIEW)},
            preview_group('HQPreview', HQ_CENTRE, HQ_PREVIEW, ['HQPreviewFill']),
            preview_group('RefPreview', REF_CENTRE, REF_PREVIEW, ['RefPreviewBad', 'RefPreviewGood']),
            truck_group('HQTruck'),
            truck_group('RefTruck'),
            text('CaptionHQ', 'YOUR HQ: ANYWHERE IT FITS', 13, ACC_LIN % '1', p(14, 10, 300, 20), 'Left'),
            text('CaptionOther', 'OTHER TRUCKS: INSIDE A BUILD RADIUS', 13, ACC_LIN % '1', p(14, 10, 340, 20),
                 'Left'),
            tooltip_box('TooltipOutside', 'Outside the build radius', (30, 118, 190, 26)),
            {'name': 'MiniMouseBody', 'type': 'Border', 'slot': p(476, 234, 32, 50)},
            {'name': 'LeftButtonGlow', 'type': 'Image', 'slot': p(478, 236, 13, 20)},
            {'name': 'MiniSplit', 'type': 'Image', 'slot': p(491.25, 236, 1.5, 20)},
            {'name': 'MiniLine', 'type': 'Image', 'slot': p(478, 256, 28, 1.5)},
            # Positioned by its translation track (tip coordinates minus the image's tip offset).
            {'name': 'MouseCursor', 'type': 'Image', 'slot': p(0, 0, 26, 26)},
            {'name': 'ScreenFrame', 'type': 'Border', 'slot': p(0, 0, SIZE[0], SIZE[1])}]


def rebuild():
    path = FOLDER + '/' + WIDGET
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        uecp.asset_management.create_asset(asset_type='WidgetBlueprint', name=WIDGET, save_path=FOLDER)
    try:
        uecp.widget.remove_widget_animation(widget_path=path, animation_name=ANIMATION)
    except Exception:
        pass
    uecp.widget.create_widget_from_layout(widget_path=path, layout=[{'name': 'Root', 'type': 'CanvasPanel',
                                                                     'children': layout()}],
                                          replace=True, canvas_size='%d,%d' % SIZE, design_mode='Custom')
    uecp.widget.add_widget_animation(widget_path=path, animation_name=ANIMATION, duration=DURATION)
    return path


def look(path):
    edits = rb('Ground', 'Background', GROUND_FILL, (2, 2, 2, 2))
    for group in ('HQPreview', 'RefPreview', 'HQTruck', 'RefTruck'):
        edits += [(group, 'Background.DrawAs', 'NoDrawType')]
    edits += rb('RadiusRing', 'Brush', RING_FILL, (RADIUS,) * 4, 2.0, RING_LINE)
    edits += [('RadiusRing', 'Brush.ImageSize', '(X=%d,Y=%d)' % (2 * RADIUS, 2 * RADIUS))]
    # Overlay slots may not stretch their child, so the fills carry their full size in the brush.
    edits += rb('HQPreviewFill', 'Brush', PREVIEW_GOOD, (4, 4, 4, 4))
    edits += [('HQPreviewFill', 'Brush.ImageSize', '(X=%d,Y=%d)' % HQ_PREVIEW)]
    edits += rb('RefPreviewBad', 'Brush', PREVIEW_BAD, (4, 4, 4, 4))
    edits += rb('RefPreviewGood', 'Brush', PREVIEW_GOOD, (4, 4, 4, 4))
    edits += [('RefPreviewBad', 'Brush.ImageSize', '(X=%d,Y=%d)' % REF_PREVIEW),
              ('RefPreviewGood', 'Brush.ImageSize', '(X=%d,Y=%d)' % REF_PREVIEW)]
    edits += [('HQPreviewIcon', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.6)'),
              ('RefPreviewIcon', 'ColorAndOpacity', '(R=1,G=1,B=1,A=0.6)')]
    edits += rb('TooltipOutside', 'Background', TOOLTIP_FILL, (4, 4, 4, 4))
    edits += rb('MiniMouseBody', 'Background', KEY_FILL, (15, 15, 13, 13), 1.5, OUT)
    edits += rb('LeftButtonGlow', 'Brush', ACC % '0.85', (12, 2, 2, 2))
    edits += [('MiniSplit', 'ColorAndOpacity', LINE_LIN), ('MiniLine', 'ColorAndOpacity', LINE_LIN)]
    edits += rb('ScreenFrame', 'Background', CLEAR, (3, 3, 3, 3), 2, OUT)
    for name in ('HQPreview', 'RefPreview', 'RefPreviewGood', 'HQTruckDecal', 'RefTruckDecal', 'RefTruckIcon',
                 'HQBuilding', 'RefBuilding', 'CaptionOther', 'TooltipOutside', 'LeftButtonGlow'):
        edits += [(name, 'RenderOpacity', '0')]
    # Brushes first: set_image_brush resets the brush, so draw settings go on afterwards.
    brushes = [{'widget_name': 'HQTruckIcon', 'texture_path': HQ_TRUCK_ICON,
                'image_size': icon_fit(HQ_TRUCK_ICON, (64, 64))},
               {'widget_name': 'RefTruckIcon', 'texture_path': REF_TRUCK_ICON,
                'image_size': icon_fit(REF_TRUCK_ICON, (64, 64))},
               {'widget_name': 'HQTruckDecal', 'material_path': BOX_DECAL, 'image_size': [TRUCK_BOX, TRUCK_BOX]},
               {'widget_name': 'RefTruckDecal', 'material_path': BOX_DECAL, 'image_size': [TRUCK_BOX, TRUCK_BOX]},
               {'widget_name': 'HQPreviewIcon', 'texture_path': HQ_ICON,
                'image_size': icon_fit(HQ_ICON, (HQ_PREVIEW[0] - 12, HQ_PREVIEW[1] - 12))},
               {'widget_name': 'RefPreviewIcon', 'texture_path': REF_ICON,
                'image_size': icon_fit(REF_ICON, (REF_PREVIEW[0] - 12, REF_PREVIEW[1] - 12))},
               {'widget_name': 'HQBuilding', 'texture_path': HQ_ICON, 'image_size': icon_fit(HQ_ICON, HQ_PREVIEW)},
               {'widget_name': 'RefBuilding', 'texture_path': REF_ICON, 'image_size': icon_fit(REF_ICON, REF_PREVIEW)},
               {'widget_name': 'MouseCursor', 'texture_path': CURSOR, 'image_size': [26, 26]}]
    uecp.widget.set_image_brush(widget_path=path, brushes=brushes)
    # set_image_brush resets the ring's brush too if it is listed; it is not, so its rounded box stays.
    uecp.widget.edit_widget_properties(widget_path=path, edits=[{'widget_name': a, 'property_name': b, 'value': c}
                                                                for a, b, c in edits])


# ----------------------------------------------------------------------------------------------------- opacity tracks
def track(path, widget, prop, keys):
    uecp.widget.add_widget_animation_track(widget_path=path, animation_name=ANIMATION, widget_name=widget,
                                           property_name=prop)
    for time, value in keys:
        uecp.widget.add_widget_animation_keyframe(widget_path=path, animation_name=ANIMATION, widget_name=widget,
                                                  property_name=prop, time=time, value=value, interp_mode='linear')


def window(on, off, fade=0.12, low=0.0, high=1.0):
    """Opacity that is `high` from `on` to `off` and `low` elsewhere in the loop."""
    return [(0.0, low), (on, low), (on + fade, high), (off, high), (off + fade, low), (DURATION, low)]


def clicks(times, hold=0.15):
    keys = [(0.0, 0.0)]
    for time in times:
        keys += [(time, 0.0), (time + 0.05, 1.0), (time + 0.05 + hold, 1.0), (time + 0.1 + hold, 0.0)]
    keys.append((DURATION, 0.0))
    return keys


# Mouse clicks: select the HQ truck, place the HQ, select the refinery truck, the refused click, place the refinery.
CLICKS = [0.75, 2.8, 5.3, 6.55, 8.0]


def opacity_tracks(path):
    # Widgets that only move or scale get a constant track, so the transform pass finds a binding.
    for name in ('MouseCursor', 'RadiusRing'):
        track(path, name, 'opacity', [(0.0, 1.0), (DURATION, 1.0)])
    track(path, 'LeftButtonGlow', 'opacity', clicks(CLICKS))
    track(path, 'CaptionHQ', 'opacity', window(0.2, 4.6, 0.2))
    track(path, 'CaptionOther', 'opacity', window(4.8, 9.2, 0.2))
    # Act 1: the HQ truck is selected, its preview placed on open ground, and it becomes the HQ.
    track(path, 'HQTruckDecal', 'opacity', window(0.85, 2.95, 0.1))
    track(path, 'HQTruckIcon', 'opacity', [(0, 1), (2.95, 1), (3.4, 0), (9.75, 0), (DURATION, 1)])
    track(path, 'HQPreview', 'opacity', [(0, 0), (1.05, 0), (1.2, 1), (2.9, 1), (3.1, 0), (DURATION, 0)])
    track(path, 'HQBuilding', 'opacity', [(0, 0), (2.95, 0), (3.4, 1), (9.3, 1), (9.5, 0), (DURATION, 0)])
    # Act 2: the refinery truck; its preview is refused outside the radius, then placed inside it.
    track(path, 'RefTruckIcon', 'opacity', [(0, 0), (4.7, 0), (5.0, 1), (8.1, 1), (8.5, 0), (DURATION, 0)])
    track(path, 'RefTruckDecal', 'opacity', window(5.4, 8.1, 0.1))
    track(path, 'RefPreview', 'opacity', [(0, 0), (5.55, 0), (5.7, 1), (8.1, 1), (8.3, 0), (DURATION, 0)])
    track(path, 'RefPreviewBad', 'opacity', [(0, 1), (7.28, 1), (7.38, 0), (9.5, 0), (9.6, 1), (DURATION, 1)])
    track(path, 'RefPreviewGood', 'opacity', [(0, 0), (7.28, 0), (7.38, 1), (9.5, 1), (9.6, 0), (DURATION, 0)])
    track(path, 'TooltipOutside', 'opacity', window(6.62, 7.05, 0.1))
    track(path, 'RefBuilding', 'opacity', [(0, 0), (8.1, 0), (8.5, 1), (9.3, 1), (9.5, 0), (DURATION, 0)])


# --------------------------------------------------------------------------------------------------- transform tracks
# Same technique as nomadic_conversion_transform_tracks.py: a MovieScene2DTransformTrack goes through
# SetRenderTransform (a float track on a RenderTransform sub-property moves nothing on screen).
CHANNEL_DEFAULTS = {'Translation.X': 0.0, 'Translation.Y': 0.0, 'Angle': 0.0, 'Scale.X': 1.0, 'Scale.Y': 1.0,
                    'Shear.X': 0.0, 'Shear.Y': 0.0}
SMOOTH = 'smooth'
LINEAR = 'linear'
CURSOR_TIP = (3.0, 2.0)
CURSOR_PATH = [(0.0, 260, 150), (0.3, 260, 150), (0.75, 90, 205), (1.05, 90, 205), (2.6, 300, 115), (3.05, 300, 115),
               (3.6, 450, 200), (4.9, 450, 200), (5.3, 90, 205), (5.55, 90, 205), (6.3, 120, 80), (7.0, 120, 80),
               (7.9, 380, 175), (8.3, 380, 175), (8.8, 460, 210), (9.4, 460, 210), (9.9, 260, 150),
               (10.0, 260, 150)]


def offset(frm, to):
    return frm[0] - to[0], frm[1] - to[1]


HQ_START = offset(TRUCK_CENTRE, HQ_CENTRE)
REF_START = offset(TRUCK_CENTRE, REF_CENTRE)
REF_OUTSIDE = offset(OUTSIDE_CENTRE, REF_CENTRE)
GROW = [(0.0, 0.7), (2.95, 0.7), (3.4, 1.0), (9.5, 1.0), (9.55, 0.7), (DURATION, 0.7)]
REF_GROW = [(0.0, 0.7), (8.1, 0.7), (8.5, 1.0), (9.5, 1.0), (9.55, 0.7), (DURATION, 0.7)]
RING_SCALE = [(0.0, 0.0), (1.05, 0.0), (1.35, 1.0), (9.3, 1.0), (9.5, 0.0), (DURATION, 0.0)]


def cursor_channel(axis):
    return [(time, (x, y)[axis] - CURSOR_TIP[axis]) for time, x, y in CURSOR_PATH]


def hq_drag(axis):
    # Rides on the cursor from the truck (1.05 s) to the open spot (2.6 s); held there for the rest of the loop.
    return [(0.0, HQ_START[axis]), (1.05, HQ_START[axis]), (2.6, 0.0), (9.5, 0.0), (9.55, HQ_START[axis]),
            (DURATION, HQ_START[axis])]


def ref_drag(axis):
    return [(0.0, REF_START[axis]), (5.55, REF_START[axis]), (6.3, REF_OUTSIDE[axis]), (7.0, REF_OUTSIDE[axis]),
            (7.9, 0.0), (9.5, 0.0), (9.55, REF_START[axis]), (DURATION, REF_START[axis])]


SPECS = {
    'MouseCursor': {'Translation.X': (SMOOTH, cursor_channel(0)), 'Translation.Y': (SMOOTH, cursor_channel(1))},
    'HQPreview': {'Translation.X': (SMOOTH, hq_drag(0)), 'Translation.Y': (SMOOTH, hq_drag(1))},
    'RadiusRing': {'defaults': {'Scale.X': 0.0, 'Scale.Y': 0.0},
                   'Translation.X': (SMOOTH, hq_drag(0)), 'Translation.Y': (SMOOTH, hq_drag(1)),
                   'Scale.X': (LINEAR, RING_SCALE), 'Scale.Y': (LINEAR, RING_SCALE)},
    'RefPreview': {'Translation.X': (SMOOTH, ref_drag(0)), 'Translation.Y': (SMOOTH, ref_drag(1))},
    'HQBuilding': {'defaults': {'Scale.X': 0.7, 'Scale.Y': 0.7},
                   'Scale.X': (SMOOTH, GROW), 'Scale.Y': (SMOOTH, GROW)},
    'RefBuilding': {'defaults': {'Scale.X': 0.7, 'Scale.Y': 0.7},
                    'Scale.X': (SMOOTH, REF_GROW), 'Scale.Y': (SMOOTH, REF_GROW)},
}


def smooth_interpolation():
    # Smart auto tangents do not overshoot between held keys.
    if hasattr(unreal.MovieSceneKeyInterpolation, 'SMART_AUTO'):
        return unreal.MovieSceneKeyInterpolation.SMART_AUTO
    return unreal.MovieSceneKeyInterpolation.LINEAR


def channel_key(channel):
    return re.sub(r'_\d+$', '', str(channel.get_name()))


def remove_transform_tracks(binding):
    for existing_track in list(binding.get_tracks()):
        if isinstance(existing_track, unreal.MovieScene2DTransformTrack):
            binding.remove_track(existing_track)


def build_transform_track(binding, widget_spec, tick_rate):
    transform_track = binding.add_track(unreal.MovieScene2DTransformTrack)
    transform_track.set_property_name_and_path('RenderTransform', 'RenderTransform')
    section = transform_track.add_section()
    section.set_start_frame_bounded(False)
    section.set_end_frame_bounded(False)
    defaults = widget_spec.get('defaults', {})
    added = []
    for channel in section.get_all_channels():
        name = channel_key(channel)
        if name not in CHANNEL_DEFAULTS:
            continue
        channel.set_default(defaults.get(name, CHANNEL_DEFAULTS[name]))
        if name not in widget_spec:
            continue
        interp_name, keys = widget_spec[name]
        interpolation = smooth_interpolation() if interp_name == SMOOTH else unreal.MovieSceneKeyInterpolation.LINEAR
        for seconds, value in keys:
            frame = unreal.FrameNumber(int(round(seconds * tick_rate)))
            channel.add_key(frame, float(value), 0.0, unreal.MovieSceneTimeUnit.TICK_RESOLUTION, interpolation)
        added.append('%s:%d' % (name, len(channel.get_keys())))
    return added


def transform_tracks(path):
    animation = unreal.load_object(None, '%s.%s:%s' % (path, WIDGET, ANIMATION))
    tick_resolution = unreal.MovieSceneSequenceExtensions.get_tick_resolution(animation)
    tick_rate = tick_resolution.numerator / float(tick_resolution.denominator)
    done = []
    for binding in animation.get_bindings():
        widget_name = str(binding.get_name())
        if widget_name not in SPECS:
            continue
        remove_transform_tracks(binding)
        done.append('%s[%s]' % (widget_name, ' '.join(build_transform_track(binding, SPECS[widget_name], tick_rate))))
    missing = set(SPECS) - {str(binding.get_name()) for binding in animation.get_bindings()}
    REPORT.append('tracks: %s%s' % (' '.join(done), (' MISSING BINDINGS %s' % sorted(missing)) if missing else ''))


def build_radius():
    path = rebuild()
    look(path)
    opacity_tracks(path)
    uecp.blueprint.compile_blueprint(blueprint_path=path)
    transform_tracks(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(path))
    REPORT.append('%s saved=%s' % (WIDGET, unreal.EditorAssetLibrary.save_asset(path, False)))


build_radius()
print('\n'.join(REPORT))
