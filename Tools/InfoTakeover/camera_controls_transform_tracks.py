"""Authors the moving parts of the camera controls illustrations as UMG RenderTransform tracks
(MovieScene2DTransformTrack), one per widget.

Why: a float track on a RenderTransform sub-property (what a "translation_x" keyframe tool makes) only writes the
UPROPERTY memory; Slate never sees it, so the widget does not move. The 2D transform track goes through
SetRenderTransform. Opacity tracks (RenderOpacity has a setter) are left alone; they are authored in the editor.

Idempotent: every run removes the RenderTransform tracks (2D and float) of the widgets listed below and builds them
again from SPECS. Run it in the editor's Python (it only needs engine classes), then the widgets are compiled and saved.
"""
import re

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls/'
CHANNEL_DEFAULTS = {'Translation.X': 0.0, 'Translation.Y': 0.0, 'Angle': 0.0, 'Scale.X': 1.0, 'Scale.Y': 1.0,
                    'Shear.X': 0.0, 'Shear.Y': 0.0}
SMOOTH = 'smooth'
LINEAR = 'linear'


def key_press(start):
    """A key pressed for 0.85 s from start: the cap sinks 3 px."""
    keys = [(0.0, 0.0), (start, 0.0), (start + 0.08, 3.0), (start + 0.85, 3.0), (start + 0.95, 0.0), (4.0, 0.0)]
    unique = []
    for time, value in keys:
        if not unique or abs(unique[-1][0] - time) > 1e-6:
            unique.append((time, value))
    return unique


# Zooming in shows less of the map: the view frame over the minimap shrinks, then grows back when zooming out.
VIEW_FRAME_ZOOM_SCALE = [(0.0, 1.0), (0.3, 1.0), (1.7, 0.5), (2.2, 0.5), (3.6, 1.0), (4.0, 1.0)]
# Panning: the mouse moves right (Mouse X adds yaw), so the ground on screen turns the other way; up tilts the view
# (the ground squashes along the screen's vertical); Alt + middle click puts both back (PanReset).
PANNING_TURN = [(0.0, 0.0), (0.5, 0.0), (2.4, -40.0), (5.65, -40.0), (6.1, 0.0), (8.0, 0.0)]
PANNING_TILT = [(0.0, 1.0), (2.6, 1.0), (3.9, 0.84), (5.65, 0.84), (6.1, 1.0), (8.0, 1.0)]
# The box's pivot is its top-left corner (set in the designer); it is hidden (opacity) while collapsed.
MARQUEE_SCALE = [(0.0, 0.02), (0.6, 0.02), (2.0, 1.0), (2.45, 1.0), (2.5, 0.02), (5.0, 0.02)]

# widget blueprint -> (animation, {widget: {'defaults': {channel: value}, channel: (interp, [(seconds, value)])}})
SPECS = {
    'WBP_CamCtl_KeyboardPan': ('PanLoop', {
        'Key_Right': {'Translation.Y': (LINEAR, key_press(0.0))},
        'Key_Up': {'Translation.Y': (LINEAR, key_press(1.0))},
        'Key_Left': {'Translation.Y': (LINEAR, key_press(2.0))},
        'Key_Down': {'Translation.Y': (LINEAR, key_press(3.0))},
        # The camera's view frame moves over the minimap the way the pressed key moves the camera.
        'ViewFrame': {'Translation.X': (SMOOTH, [(0, 0), (0.15, 0), (0.9, 32), (2.15, 32), (2.9, 0), (4.0, 0)]),
                      'Translation.Y': (SMOOTH, [(0, 0), (1.15, 0), (1.9, -32), (3.15, -32), (3.9, 0), (4.0, 0)])},
    }),
    'WBP_CamCtl_EdgePan': ('EdgeLoop', {
        'MouseCursor': {'Translation.X': (SMOOTH, [(0, 0), (0.85, 160), (2.65, 160), (3.5, -147), (5.25, -147), (6.0, 0)]),
                        'Translation.Y': (SMOOTH, [(0, 0), (0.85, -8), (2.65, -8), (3.5, 10), (5.25, 10), (6.0, 0)])},
        # The zoomed minimap is the screen: it slides away from the pushed edge, faster the longer the cursor stays,
        # like UPlayerCameraController::EdgeScroll.
        'MinimapImage': {'Translation.X': (LINEAR, [(0, 0), (0.95, 0), (1.6, -14), (2.1, -36), (2.6, -70), (3.55, -70),
                                                    (4.2, -56), (4.7, -34), (5.2, 0), (6.0, 0)])},
    }),
    'WBP_CamCtl_Zoom': ('ZoomLoop', {
        'ViewFrame': {'Scale.X': (SMOOTH, VIEW_FRAME_ZOOM_SCALE), 'Scale.Y': (SMOOTH, VIEW_FRAME_ZOOM_SCALE)},
        # Rolling forward moves the wheel's top surface away from the player: the ridges run up.
        'RidgeStrip': {'Translation.Y': (LINEAR, [(0, 0), (0.3, 0), (1.7, -24), (2.2, -24), (3.6, 0), (4.0, 0)])},
        'ArrowIn': {'Translation.Y': (LINEAR, [(0, 0), (0.3, 0), (0.55, -3), (0.8, 0), (1.05, -3), (1.3, 0),
                                               (4.0, 0)])},
        'ArrowOut': {'Translation.Y': (LINEAR, [(0, 0), (2.2, 0), (2.45, 3), (2.7, 0), (2.95, 3), (3.2, 0), (4.0, 0)])},
    }),
    'WBP_CamCtl_Panning': ('PanningLoop', {
        'MinimapImage': {'Angle': (SMOOTH, PANNING_TURN)},
        'TiltLayer': {'Scale.Y': (SMOOTH, PANNING_TILT)},
        # North on the dial turns with the ground.
        'CompassNeedle': {'Angle': (SMOOTH, PANNING_TURN)},
        'MouseCursor': {'Translation.X': (SMOOTH, [(0, 0), (0.5, 0), (2.4, 120), (6.6, 120), (7.6, 0), (8.0, 0)]),
                        'Translation.Y': (SMOOTH, [(0, 0), (2.6, 0), (3.9, -55), (6.6, -55), (7.6, 0), (8.0, 0)])},
        'Key_Alt': {'Translation.Y': (LINEAR, [(0, 0), (5.2, 0), (5.28, 3), (6.3, 3), (6.4, 0), (8.0, 0)])},
    }),
    # Marquee page: press at the box corner, drag (the box grows from its top-left pivot with the cursor), release at 2 s,
    # the units' selection decals light up (opacity tracks), the cursor drifts back.
    'WBP_CamCtl_Marquee': ('MarqueeLoop', {
        'MouseCursor': {'Translation.X': (SMOOTH, [(0, -40), (0.5, 0), (0.6, 0), (2.0, 320), (3.8, 320), (5.0, -40)]),
                        'Translation.Y': (SMOOTH, [(0, -30), (0.5, 0), (0.6, 0), (2.0, 192), (3.8, 192), (5.0, -30)])},
        'MarqueeBox': {'Scale.X': (LINEAR, MARQUEE_SCALE), 'Scale.Y': (LINEAR, MARQUEE_SCALE)},
    }),
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
    for track in list(binding.get_tracks()):
        if isinstance(track, unreal.MovieScene2DTransformTrack):
            binding.remove_track(track)
        elif isinstance(track, unreal.MovieSceneFloatTrack) and \
                str(track.get_property_path()).startswith('RenderTransform.'):
            binding.remove_track(track)


def build_transform_track(binding, widget_spec, tick_rate):
    track = binding.add_track(unreal.MovieScene2DTransformTrack)
    track.set_property_name_and_path('RenderTransform', 'RenderTransform')
    section = track.add_section()
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


def author_widget(widget, animation_name, widget_specs):
    path = FOLDER + widget
    blueprint = unreal.load_asset(path)
    animation = unreal.load_object(None, '%s.%s:%s' % (path, widget, animation_name))
    tick_resolution = unreal.MovieSceneSequenceExtensions.get_tick_resolution(animation)
    tick_rate = tick_resolution.numerator / float(tick_resolution.denominator)
    report = []
    for binding in animation.get_bindings():
        widget_name = str(binding.get_name())
        if widget_name not in widget_specs:
            continue
        remove_transform_tracks(binding)
        report.append('%s[%s]' % (widget_name, ' '.join(build_transform_track(binding, widget_specs[widget_name],
                                                                                 tick_rate))))
    missing = set(widget_specs) - {str(binding.get_name()) for binding in animation.get_bindings()}
    print('CAMCTL_TRACKS %s %s: %s%s' % (widget, animation_name, ' '.join(report),
                                         (' MISSING BINDINGS %s' % sorted(missing)) if missing else ''))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    print('CAMCTL_TRACKS saved %s: %s' % (widget, unreal.EditorAssetLibrary.save_asset(path, False)))


def main():
    # Set CAMCTL_TRACK_TARGETS before exec() to author only some widgets, e.g. ['WBP_CamCtl_Marquee'].
    for widget, (animation_name, widget_specs) in SPECS.items():
        if widget in globals().get('CAMCTL_TRACK_TARGETS', list(SPECS)):
            author_widget(widget, animation_name, widget_specs)


main()
