import unreal
paths = ["/Game/RTS_Survival/Sounds/Ambient/DesertWind20Sec_Cue","/Game/RTS_Survival/Sounds/Ambient/DesertWind20Sec",
 "/Game/RTS_Survival/Sounds/VoiceLines/RadioEffects/TurnOnRadio_1","/Game/RTS_Survival/Sounds/UI/Notification/Notification1",
 "/Game/RTS_Survival/Sounds/UI/Button/S_ButtonClick1","/Game/RTS_Survival/Sounds/UI/WidgetEnterView1"]
for p in paths:
    a = unreal.load_asset(p)
    if not a: print("PROBE missing", p); continue
    out = [p, a.get_class().get_name()]
    for prop in ["sound_class_object","concurrency_set","looping","volume_multiplier","attenuation_settings","duration"]:
        try: out.append("%s=%s" % (prop, a.get_editor_property(prop)))
        except Exception as e: pass
    print("PROBE", " | ".join(str(x) for x in out))
for p in ["/Game/RTS_Survival/Sounds/Settings/UI/SC_RTS_UI","/Game/RTS_Survival/Sounds/Settings/VoiceLInes/SC_VoiceLines"]:
    a = unreal.load_asset(p)
    if a:
        c = a.get_editor_property("concurrency")
        print("PROBE", p, "max", c.get_editor_property("max_count"), "rule", c.get_editor_property("resolution_rule"), "limit_owner", c.get_editor_property("limit_to_owner"))
f = unreal.SoundCueFactoryNew()
print("PROBE factory props initial_sound_waves =", f.get_editor_property("initial_sound_waves"))
