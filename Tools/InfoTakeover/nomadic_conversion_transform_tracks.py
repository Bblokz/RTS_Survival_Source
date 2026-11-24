"""Authors the moving parts of the nomadic conversion illustration (WBP_NomCnv_Deploy, DeployLoop) as UMG
RenderTransform tracks (MovieScene2DTransformTrack), one per widget.

Why a script: a float track on a RenderTransform sub-property only writes the UPROPERTY; Slate never sees it, so nothing
moves. The 2D transform track goes through SetRenderTransform (same as camera_controls_transform_tracks.py, which is
tied to the CameraControls folder). Opacity tracks come from nomadic_conversion_illustration.py; run that first, it
also creates the bindings of widgets that only move.

Idempotent: every run removes the RenderTransform tracks of the widgets listed below and builds them again from SPECS,
so a retime in the designer is overwritten; change SPECS instead. Run it in the live editor's Python (UECP
execute_python: exec(open(r'<this file>').read())); it needs only engine classes, not the new C++ class.
"""
import re

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion/'
WIDGET = 'WBP_NomCnv_Deploy'
ANIMATION = 'DeployLoop'
CHANNEL_DEFAULTS = {'Translation.X': 0.0, 'Translation.Y': 0.0, 'Angle': 0.0, 'Scale.X': 1.0, 'Scale.Y': 1.0,
                    'Shear.X': 0.0, 'Shear.Y': 0.0}
SMOOTH = 'smooth'
LINEAR = 'linear'

# T_CamCtl_Cursor's tip sits this far inside the image; the cursor slot is at (0, 0), so its translation places the tip.
CURSOR_TIP = (3.0, 2.0)
# Where the cursor's tip is (illustration pixels): from the middle to the truck (click at 0.85 s), to the convert button
# (click at 1.95 s), dragging the preview to the building spot (3.6 s), placing it (5 s), aside while the truck drives
# and unfolds, back to hover the button that now packs up (8.45 s), and back to the start.
CURSOR_PATH = [(0.0, 300, 170), (0.3, 300, 170), (0.8, 100, 130), (1.25, 100, 130), (1.75, 262, 258),
               (2.1, 262, 258), (3.6, 400, 118), (5.15, 400, 118), (6.0, 470, 190), (7.9, 470, 190),
               (8.45, 262, 258), (9.2, 262, 258), (9.9, 300, 170), (10.0, 300, 170)]
# The preview's centre is (400, 118) in the designer; it rides on the cursor while it is dragged (2.1 to 3.6 s).
PREVIEW_CENTRE = (400.0, 118.0)
PREVIEW_START = (262 - PREVIEW_CENTRE[0], 258 - PREVIEW_CENTRE[1])
# The truck's centre is (100, 130); it drives to the building spot.
TRUCK_DRIVE = (300.0, -12.0)


def cursor_channel(axis):
    offset = CURSOR_TIP[axis]
    return [(time, (x, y)[axis] - offset) for time, x, y in CURSOR_PATH]


def key_press(times, depth=3.0, hold=0.12):
    keys = [(0.0, 0.0)]
    for time in times:
        keys += [(time, 0.0), (time + 0.05, depth), (time + 0.05 + hold, depth), (time + 0.1 + hold, 0.0)]
    keys.append((10.0, 0.0))
    return keys


TRUCK_SHRINK = [(0.0, 1.0), (6.9, 1.0), (7.6, 0.7), (9.35, 0.7), (9.4, 1.0), (10.0, 1.0)]
BUILDING_GROW = [(0.0, 0.7), (6.9, 0.7), (7.6, 1.0), (9.4, 1.0), (9.45, 0.7), (10.0, 0.7)]

# widget -> {'defaults': {channel: value}, channel: (interp, [(seconds, value)])}
SPECS = {
    'MouseCursor': {'Translation.X': (SMOOTH, cursor_channel(0)), 'Translation.Y': (SMOOTH, cursor_channel(1))},
    'PreviewGroup': {
        'Translation.X': (SMOOTH, [(0.0, PREVIEW_START[0]), (2.1, PREVIEW_START[0]), (3.6, 0.0), (10.0, 0.0)]),
        'Translation.Y': (SMOOTH, [(0.0, PREVIEW_START[1]), (2.1, PREVIEW_START[1]), (3.6, 0.0), (10.0, 0.0)])},
    # Two presses of the period key (IA_RotateRight), RotationDegrees = 10 each; reset while hidden.
    'PreviewFill': {'Angle': (LINEAR, [(0.0, 0.0), (3.85, 0.0), (3.9, 10.0), (4.35, 10.0), (4.4, 20.0), (9.4, 20.0),
                                       (9.45, 0.0), (10.0, 0.0)])},
    'Key_Period': {'Translation.Y': (LINEAR, key_press([3.8, 4.3]))},
    # Drives after the placing click; snaps back while the group is faded out (9.3 to 9.8 s).
    'TruckGroup': {
        'Translation.X': (SMOOTH, [(0.0, 0.0), (5.3, 0.0), (6.8, TRUCK_DRIVE[0]), (9.3, TRUCK_DRIVE[0]), (9.35, 0.0),
                                   (10.0, 0.0)]),
        'Translation.Y': (SMOOTH, [(0.0, 0.0), (5.3, 0.0), (6.8, TRUCK_DRIVE[1]), (9.3, TRUCK_DRIVE[1]), (9.35, 0.0),
                                   (10.0, 0.0)])},
    # The truck folds away while the building rises out of it.
    'TruckIcon': {'Scale.X': (LINEAR, TRUCK_SHRINK), 'Scale.Y': (LINEAR, TRUCK_SHRINK)},
    'BuildingIcon': {'defaults': {'Scale.X': 0.7, 'Scale.Y': 0.7},
                     'Scale.X': (SMOOTH, BUILDING_GROW), 'Scale.Y': (SMOOTH, BUILDING_GROW)},
    # Pivot (0, 0.5) is set by the illustration script, so the bar fills from the left.
    'ProgressFill': {'defaults': {'Scale.X': 0.0},
                     'Scale.X': (LINEAR, [(0.0, 0.0), (6.9, 0.0), (7.9, 1.0), (9.0, 1.0), (9.05, 0.0), (10.0, 0.0)])},
}


def smooth_interpolation():
    # Smart auto tangents do not overshoot between held keys; plain auto would make the cursor drift past its stop.
    if hasattr(unreal.MovieSceneKeyInterpolation, 'SMART_AUTO'):
        return unreal.MovieSceneKeyInterpolation.SMART_AUTO
    return unreal.MovieSceneKeyInterpolation.LINEAR


def channel_key(channel):
    # Channel names carry a numeric suffix, e.g. "Translation.X_93".
    return re.sub(r'_\d+$', '', str(channel.get_name()))


def remove_transform_tracks(binding):
    for existing_track in list(binding.get_tracks()):
        if isinstance(existing_track, unreal.MovieScene2DTransformTrack):
            binding.remove_track(existing_track)
        elif isinstance(existing_track, unreal.MovieSceneFloatTrack) and \
                str(existing_track.get_property_path()).startswith('RenderTransform.'):
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


def author_deploy_tracks():
    path = FOLDER + WIDGET
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        raise RuntimeError('NOMCNV_TRACKS %s is missing; run nomadic_conversion_illustration.py first' % path)
    animation = unreal.load_object(None, '%s.%s:%s' % (path, WIDGET, ANIMATION))
    tick_resolution = unreal.MovieSceneSequenceExtensions.get_tick_resolution(animation)
    tick_rate = tick_resolution.numerator / float(tick_resolution.denominator)
    report = []
    for binding in animation.get_bindings():
        widget_name = str(binding.get_name())
        if widget_name not in SPECS:
            continue
        remove_transform_tracks(binding)
        report.append('%s[%s]' % (widget_name, ' '.join(build_transform_track(binding, SPECS[widget_name],
                                                                                 tick_rate))))
    missing = set(SPECS) - {str(binding.get_name()) for binding in animation.get_bindings()}
    print('NOMCNV_TRACKS %s %s: %s%s' % (WIDGET, ANIMATION, ' '.join(report),
                                         (' MISSING BINDINGS %s' % sorted(missing)) if missing else ''))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    print('NOMCNV_TRACKS saved %s: %s' % (WIDGET, unreal.EditorAssetLibrary.save_asset(path, False)))


author_deploy_tracks()
