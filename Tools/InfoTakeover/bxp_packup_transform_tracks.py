"""Authors the moving parts of the building expansion pack-up illustration (WBP_BxpPack_Loop, PackLoop) as UMG
RenderTransform tracks (MovieScene2DTransformTrack), one per widget.

Why a script: a float track on a RenderTransform sub-property only writes the UPROPERTY; Slate never sees it, so nothing
moves. The 2D transform track goes through SetRenderTransform (same as nomadic_conversion_transform_tracks.py).
Opacity tracks come from bxp_packup_illustration.py; run that first, it also creates the bindings of widgets that only
move.

Idempotent: every run removes the RenderTransform tracks of the widgets listed below and builds them again from SPECS,
so a retime in the designer is overwritten; change SPECS instead. Run it in the live editor's Python (UECP
execute_python: exec(open(r'<this file>').read())); it needs only engine classes, not the C++ takeover class.
"""
import re

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/BxpPackUp/'
WIDGET = 'WBP_BxpPack_Loop'
ANIMATION = 'PackLoop'
DURATION = 12.0
CHANNEL_DEFAULTS = {'Translation.X': 0.0, 'Translation.Y': 0.0, 'Angle': 0.0, 'Scale.X': 1.0, 'Scale.Y': 1.0,
                    'Shear.X': 0.0, 'Shear.Y': 0.0}
SMOOTH = 'smooth'
LINEAR = 'linear'

# T_CamCtl_Cursor's tip sits this far inside the image; the cursor slot is at (0, 0), so its translation places the tip.
CURSOR_TIP = (3.0, 2.0)
# Where the cursor's tip is (illustration pixels): to the pack-up button (click 1.15 s), aside while the HQ packs up, to
# the convert button (click 4.0 s), dragging the building preview to the new spot (click 5.1 s), aside while the truck
# drives, unfolds and the radar rebuilds, to the flak's packed slot (click 8.6 s), dragging the flak preview into the
# expansion range (click 9.85 s), aside, and back to the start.
CURSOR_PATH = [(0.0, 300, 170), (0.3, 300, 170), (0.9, 176, 260), (1.3, 176, 260), (1.8, 470, 190),
               (3.4, 470, 190), (3.85, 176, 260), (4.1, 176, 260), (5.0, 380, 93), (5.25, 380, 93),
               (5.7, 470, 190), (7.7, 470, 190), (8.2, 336, 260), (8.7, 336, 260), (9.7, 300, 176),
               (10.0, 300, 176), (10.5, 470, 190), (11.5, 470, 190), (12.0, 300, 170)]
# The truck drives this far right; the second HQ stands there.
DRIVE_X = 240.0
# The HQ preview's centre is (380, 93); it starts on the convert button (176, 260).
HQ_PREVIEW_START = (176 - 380.0, 260 - 93.0)
# The flak preview's centre is (300, 176); it starts on the flak's slot (336, 260).
FLAK_PREVIEW_START = (336 - 300.0, 260 - 176.0)
# The radar (212, 72) and the flak (66, 176) fold into the building's centre (140, 93).
RADAR_FOLD = (140 - 212.0, 93 - 72.0)
FLAK_FOLD = (140 - 66.0, 93 - 176.0)


def cursor_channel(axis):
    offset = CURSOR_TIP[axis]
    return [(time, (x, y)[axis] - offset) for time, x, y in CURSOR_PATH]


def fold(start, end, value, reset=11.5):
    """0 until `start`, `value` from `end`, back to 0 while hidden at `reset`."""
    return [(0.0, 0.0), (start, 0.0), (end, value), (reset, value), (reset + 0.05, 0.0), (DURATION, 0.0)]


def shrink(start, end, low, reset=11.5):
    return [(0.0, 1.0), (start, 1.0), (end, low), (reset, low), (reset + 0.05, 1.0), (DURATION, 1.0)]


def grow(start, end, low, reset=11.95):
    return [(0.0, low), (start, low), (end, 1.0), (reset, 1.0), (DURATION, low)]


def fill(start, end, hold_until):
    return [(0.0, 0.0), (start, 0.0), (end, 1.0), (hold_until, 1.0), (hold_until + 0.05, 0.0), (DURATION, 0.0)]


def preview(offset, start, end):
    return [(0.0, offset), (start, offset), (end, 0.0), (11.9, 0.0), (11.95, offset), (DURATION, offset)]


DRIVE = [(0.0, 0.0), (5.2, 0.0), (6.0, DRIVE_X), (7.0, DRIVE_X), (7.05, 0.0), (DURATION, 0.0)]
TRUCK_SCALE = [(0.0, 0.7), (2.5, 0.7), (3.2, 1.0), (6.0, 1.0), (6.7, 0.7), (DURATION, 0.7)]

# widget -> {'defaults': {channel: value}, channel: (interp, [(seconds, value)])}
SPECS = {
    'MouseCursor': {'Translation.X': (SMOOTH, cursor_channel(0)), 'Translation.Y': (SMOOTH, cursor_channel(1))},
    # Pack-up: the radar and the flak fold into the building, which shrinks into the truck.
    'Radar_A': {'Translation.X': (SMOOTH, fold(1.35, 2.6, RADAR_FOLD[0])),
                'Translation.Y': (SMOOTH, fold(1.35, 2.6, RADAR_FOLD[1])),
                'Scale.X': (LINEAR, shrink(1.35, 2.6, 0.3)), 'Scale.Y': (LINEAR, shrink(1.35, 2.6, 0.3))},
    'Flak_A': {'Translation.X': (SMOOTH, fold(1.5, 2.7, FLAK_FOLD[0])),
               'Translation.Y': (SMOOTH, fold(1.5, 2.7, FLAK_FOLD[1])),
               'Scale.X': (LINEAR, shrink(1.5, 2.7, 0.3)), 'Scale.Y': (LINEAR, shrink(1.5, 2.7, 0.3))},
    'HQ_A': {'Scale.X': (LINEAR, shrink(2.5, 3.2, 0.7)), 'Scale.Y': (LINEAR, shrink(2.5, 3.2, 0.7))},
    'TruckIcon': {'defaults': {'Scale.X': 0.7, 'Scale.Y': 0.7},
                  'Scale.X': (LINEAR, TRUCK_SCALE), 'Scale.Y': (LINEAR, TRUCK_SCALE)},
    # Drives after the placing click; snaps back while faded out (6.7 s on).
    'TruckGroup': {'Translation.X': (SMOOTH, DRIVE)},
    'CargoRadar': {'Translation.X': (SMOOTH, DRIVE)},
    'CargoFlak': {'Translation.X': (SMOOTH, DRIVE)},
    'HqPreview': {'defaults': {'Translation.X': HQ_PREVIEW_START[0], 'Translation.Y': HQ_PREVIEW_START[1]},
                  'Translation.X': (SMOOTH, preview(HQ_PREVIEW_START[0], 4.1, 5.0)),
                  'Translation.Y': (SMOOTH, preview(HQ_PREVIEW_START[1], 4.1, 5.0))},
    'HQ_B': {'defaults': {'Scale.X': 0.7, 'Scale.Y': 0.7},
             'Scale.X': (SMOOTH, grow(6.0, 6.9, 0.7)), 'Scale.Y': (SMOOTH, grow(6.0, 6.9, 0.7))},
    'Radar_B': {'defaults': {'Scale.X': 0.4, 'Scale.Y': 0.4},
                'Scale.X': (SMOOTH, grow(7.0, 8.55, 0.4)), 'Scale.Y': (SMOOTH, grow(7.0, 8.55, 0.4))},
    'FlakPreview': {'defaults': {'Translation.X': FLAK_PREVIEW_START[0], 'Translation.Y': FLAK_PREVIEW_START[1]},
                    'Translation.X': (SMOOTH, preview(FLAK_PREVIEW_START[0], 8.7, 9.7)),
                    'Translation.Y': (SMOOTH, preview(FLAK_PREVIEW_START[1], 8.7, 9.7))},
    'Flak_B': {'defaults': {'Scale.X': 0.4, 'Scale.Y': 0.4},
               'Scale.X': (SMOOTH, grow(9.9, 11.05, 0.4)), 'Scale.Y': (SMOOTH, grow(9.9, 11.05, 0.4))},
    # Pivot (0, 0.5) is set by the illustration script, so the bars fill from the left.
    'ProgressAFill': {'defaults': {'Scale.X': 0.0}, 'Scale.X': (LINEAR, fill(1.35, 3.25, 3.4))},
    'ProgressBFill': {'defaults': {'Scale.X': 0.0}, 'Scale.X': (LINEAR, fill(6.0, 6.9, 7.05))},
    'RadarProgressFill': {'defaults': {'Scale.X': 0.0}, 'Scale.X': (LINEAR, fill(7.05, 8.55, 8.7))},
    'FlakProgressFill': {'defaults': {'Scale.X': 0.0}, 'Scale.X': (LINEAR, fill(9.95, 11.05, 11.2))},
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


def author_pack_tracks():
    path = FOLDER + WIDGET
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        raise RuntimeError('BXPPACK_TRACKS %s is missing; run bxp_packup_illustration.py first' % path)
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
    print('BXPPACK_TRACKS %s %s: %s%s' % (WIDGET, ANIMATION, ' '.join(report),
                                          (' MISSING BINDINGS %s' % sorted(missing)) if missing else ''))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    print('BXPPACK_TRACKS saved %s: %s' % (WIDGET, unreal.EditorAssetLibrary.save_asset(path, False)))


author_pack_tracks()
