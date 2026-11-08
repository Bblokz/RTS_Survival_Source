"""Evening / crimson lighting pass for Ger_MarderStugMission.

Run inside the editor (UECP execute_python):
    exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/Lighting/ue_marderstug_evening.py').read())
    apply_evening(world, stages=('sun', 'sky', 'grade'))
world = PIE world for a live preview, editor world for the real (saved) change.
The cloud-shadow light function on the Sun (MI_LF_CNC_MarderGodRays) is left untouched.
"""
import unreal

EVENING = {
    # Sun: low raking angle + warm temperature. Original: pitch -45.24, 6500 K, 11 lux.
    'sun_pitch': -42.0,               # -21 (2.6x height) and -33 (1.5x) still hid the factory yard; -42 is ~1.1x
    # A heavily tinted sun turns every texture the same rose colour (washed out), so the tint is mild and
    # most of the mood comes from the low angle, darker fill and warm-vs-cool contrast.
    'sun_temperature': 5400.0,
    'sun_color': (255, 232, 226),     # faint crimson lean (low green = red, not orange)
    'sun_intensity': 10.0,
    # Skylight: original 1.3 with a cyan tint (194,236,234).
    'sky_intensity': 1.1,            # enough fill that units in shadow stay readable
    'sky_color': (215, 218, 235),     # slightly cool fill
    # Post-process grade on PP_CNC_MarderLighting.
    'exposure_bias': -0.15,
    'contrast': 1.06,
    'saturation': 1.0,
    'gain_highlights': (1.02, 0.99, 0.98),
    'gain_midtones': (1.0, 1.0, 1.0),
    'gain_shadows': (0.96, 0.97, 1.03),
    'offset_shadows': (0.0, 0.0, 0.0),
    'vignette': 0.25,
}


def _actors(world, cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world, cls)


def apply_evening(world, stages=('sun', 'sky', 'grade'), p=None):
    p = dict(EVENING, **(p or {}))
    log = []
    if 'sun' in stages:
        sun_actor = _actors(world, unreal.DirectionalLight)[0]
        sun = sun_actor.get_component_by_class(unreal.DirectionalLightComponent)
        rot = sun_actor.get_actor_rotation()
        sun_actor.set_actor_rotation(unreal.Rotator(roll=rot.roll, pitch=p['sun_pitch'], yaw=rot.yaw), False)
        sun.set_use_temperature(True)
        sun.set_temperature(p['sun_temperature'])
        sun.set_light_color(unreal.LinearColor(*[c / 255.0 for c in p['sun_color']], 1.0), True)
        sun.set_intensity(p['sun_intensity'])
        log.append(f"sun rot={sun_actor.get_actor_rotation()} T={p['sun_temperature']} I={p['sun_intensity']}")
    if 'sky' in stages:
        sky = _actors(world, unreal.SkyLight)[0].get_component_by_class(unreal.SkyLightComponent)
        sky.set_intensity(p['sky_intensity'])
        sky.set_light_color(unreal.LinearColor(*[c / 255.0 for c in p['sky_color']], 1.0))
        sky.recapture_sky()
        log.append(f"sky I={p['sky_intensity']} color={p['sky_color']}")
    if 'grade' in stages:
        pp = next(v for v in _actors(world, unreal.PostProcessVolume) if v.get_editor_property('priority') == 15.0)
        s = pp.get_editor_property('settings')

        def v4(t, w=1.0):
            return unreal.Vector4(t[0], t[1], t[2], w)

        values = {
            'auto_exposure_bias': p['exposure_bias'],
            'color_contrast': v4((p['contrast'],) * 3),
            'color_saturation': v4((p['saturation'],) * 3),
            'color_gain_highlights': v4(p['gain_highlights']),
            'color_gain_midtones': v4(p['gain_midtones']),
            'color_gain_shadows': v4(p['gain_shadows']),
            'color_offset_shadows': v4(p['offset_shadows'], 0.0),
            'vignette_intensity': p['vignette'],
        }
        for name, value in values.items():
            s.set_editor_property('override_' + name, True)
            s.set_editor_property(name, value)
        pp.set_editor_property('settings', s)
        log.append('grade applied')
    print('\n'.join(log))
