"""Music and effects of the intro cinematic from ONE list (SOUNDS): the same entries become audio sections on the
master sequence (heard in the game) and lines of Saved/IntroCinematic/audio/sfx.json (the offline ffmpeg mix that is
muxed with the Movie Render Queue frames, see mix_audio.py). All sounds are existing project waves, played 2D.

An entry: (shot id, offset, wave name, gain dB, options). offset is seconds from the shot start; a string 'f0.3'
means 30 % into the shot; options: dur (seconds to keep), fade_in, fade_out, track ('music' or 'sfx').
"""
import json
import os

import unreal

import intro_build as b
import intro_lib as lib
import intro_shots as shots

AUDIO_DIR = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/audio"

SOUNDS = [
    # Cold open: Geiger tick in the dark, the radio comes on with the narrator.
    ("0.1", 0.3, "LogoGeiger", -9.0, {"dur": 10.0, "fade_in": 1.5, "fade_out": 1.5}),
    ("0.1", "f0.78", "fire_burning_flames_crackle_loop_01", -10.0, {"dur": 3.2, "fade_in": 0.3, "fade_out": 1.0}),
    ("1.1", 1.6, "TurnOnRadio_1", -6.0, {}),
    ("1.1", 0.0, "Secret_Situation", -19.0, {"dur": 14.0, "fade_in": 2.0, "fade_out": 2.5, "track": "music"}),
    # Sections 2 and 3 up to the skyline.
    ("2.1", 0.0, "Observe", -17.0, {"dur": 46.5, "fade_in": 2.0, "fade_out": 2.5, "track": "music"}),
    ("2.1", "f0.24", "V2RocketSFX", -7.0, {"dur": 9.0, "fade_in": 0.4, "fade_out": 2.0}),
    ("2.2", "f0.41", "metal_impact_light_thud_01", -8.0, {}),
    ("2.2", "f0.61", "metal_impact_light_thud_01", -8.0, {}),
    ("2.2", "f0.79", "metal_impact_light_thud_01", -8.0, {}),
    ("3.1", "f0.34", "V2RocketSFX", -7.0, {"dur": 7.5, "fade_in": 0.4, "fade_out": 1.5}),
    ("3.2", 0.0, "cinematic_deep_bass_pass_whoosh_02", -12.0, {}),
    # Berlin: the music is gone; flash, then a low hum.
    ("3.3", 0.0, "DesertWind20Sec", -16.0, {"dur": 6.3, "fade_in": 1.0, "fade_out": 0.3}),
    ("3.4a", "f0.35", "cinematic_deep_boom_impact_01", -5.0, {}),
    ("3.4b", 0.2, "cinematic_deep_bass_rumble_02", -5.0, {}),
    ("3.4b", "f0.5", "cinematic_deep_bass_rumble_01", -9.0, {"fade_out": 2.0, "dur": 4.0}),
    ("3.5", 0.0, "DesertWind20Sec", -13.0, {"dur": 6.7, "fade_in": 1.0, "fade_out": 1.0}),
    # Map table, the Purple Death.
    ("3.6", 0.0, "The_Loss", -17.0, {"dur": 73.0, "fade_in": 2.5, "fade_out": 3.0, "track": "music"}),
    ("4.2", 1.0, "fire_burning_flames_crackle_loop_02", -14.0, {"dur": 8.0, "fade_in": 0.5, "fade_out": 1.5}),
    ("5.1", "f0.2", "cinematic_deep_bass_pass_whoosh_04", -12.0, {}),
    ("5.5", 0.0, "DesertWind20Sec", -14.0, {"dur": 6.5, "fade_in": 1.0, "fade_out": 1.0}),
    # Svarog, the Shield, the Directorate, Radixite.
    ("6.1", 0.0, "PostApoOrchestra_Dystopia_Wav", -18.0, {"dur": 101.0, "fade_in": 2.0, "fade_out": 3.5, "track": "music"}),
    ("6.1", 0.0, "S_Medium_Engine_Rus", -13.0, {"dur": 7.6, "fade_in": 0.5, "fade_out": 0.8}),
    ("6.2", "f0.09", "cinematic_deep_boom_impact_01", -8.0, {}),
    ("6.2", "f0.37", "cinematic_deep_low_whoosh_impact_03", -10.0, {}),
    ("6.2", "f0.63", "cinematic_deep_boom_impact_01", -8.0, {}),
    ("6.3", 0.0, "S_Medium_Engine_Rus", -14.0, {"dur": 12.0, "fade_in": 0.5, "fade_out": 1.5}),
    ("6.3", "f0.11", "metal_gate_fence_impact_01", -6.0, {}),
    ("6.3", "f0.9", "cinematic_deep_boom_impact_01", -5.0, {}),
    ("7.1", "f0.6", "metal_drum_impact_thud_02", -9.0, {}),
    ("8.2", 0.5, "cinematic_deep_bass_rumble_01", -11.0, {}),
    ("8.3", "f0.55", "cinematic_deep_low_whoosh_impact_05", -9.0, {}),
    ("8.4a", 0.0, "DesertWind20Sec", -13.0, {"dur": 13.0, "fade_in": 1.0, "fade_out": 1.5}),
    # 1976.
    ("9.1", 0.0, "DesertWind20Sec", -15.0, {"dur": 13.4, "fade_in": 1.0, "fade_out": 1.0}),
    ("9.2", 0.3, "Main_Theme", -15.0, {"dur": 33.2, "fade_in": 2.0, "fade_out": 0.15, "track": "music"}),
    ("9.5", 0.0, "S_Medium_Engine_Rus", -12.0, {"dur": 8.0, "fade_in": 0.5, "fade_out": 0.1}),
    ("9.6", "f0.9", "cinematic_deep_boom_impact_01", -4.0, {}),
]

# A stamp lands with a thud.
STAMP_SOUND = ("metal_drum_impact_thud_03", -5.0)


def _wave(name):
    path = shots.find_asset(name)
    return unreal.load_asset(path) if path else None


def _export(wave):
    """Exports a SoundWave to a wav for the offline mix; returns the file path."""
    os.makedirs(AUDIO_DIR + "/src", exist_ok=True)
    path = "%s/src/%s.wav" % (AUDIO_DIR, wave.get_name())
    if not os.path.exists(path):
        task = unreal.AssetExportTask()
        task.object = wave
        task.filename = path
        task.automated = True
        task.replace_identical = True
        task.prompt = False
        task.exporter = unreal.SoundExporterWAV()
        if not unreal.Exporter.run_asset_export_task(task):
            print("  export failed:", wave.get_name())
            return None
    return path


def add_sounds(timeline, cues):
    """Adds the SOUNDS (and a thud per stamp) to the master sequence and writes sfx.json for mix_audio.py."""
    master = unreal.load_asset(b.MASTER)
    by_id = {placed["id"]: placed for placed in timeline}
    tracks = {}
    for kind in ("music", "sfx"):
        track = master.add_track(unreal.MovieSceneAudioTrack)
        track.set_display_name("Music" if kind == "music" else "Effects")
        tracks[kind] = track
    entries = list(SOUNDS)
    for cue in cues:
        if cue["type"] == "Stamp":
            entries.append((None, cue["start"] + 0.1, STAMP_SOUND[0], STAMP_SOUND[1], {}))
    mix = []
    rows = {"music": 0, "sfx": 0}
    for shot_id, offset, name, gain_db, options in entries:
        if shot_id is None:
            start = offset
        else:
            placed = by_id.get(shot_id)
            if placed is None:
                continue
            start = placed["start"] + (float(offset[1:]) * placed["duration"] if isinstance(offset, str) else offset)
        wave = _wave(name)
        if wave is None:
            print("  sound missing:", name)
            continue
        duration = min(options.get("dur", wave.get_editor_property("duration")), wave.get_editor_property("duration"))
        kind = options.get("track", "sfx")
        section = tracks[kind].add_section()
        section.set_sound(wave)
        section.set_range(b.frames(start), b.frames(start + duration))
        section.set_row_index(rows[kind] % 4)
        rows[kind] += 1
        linear = 10.0 ** (gain_db / 20.0)
        for channel in section.get_all_channels():
            if "Volume" in channel.get_name():
                channel.set_default(linear)
        file_path = _export(wave)
        if file_path:
            mix.append({"file": file_path, "start": round(start, 3), "gain_db": gain_db, "duration": round(duration, 3),
                        "fade_in": options.get("fade_in", 0.0), "fade_out": options.get("fade_out", 0.0)})
    # The logo sequence carries its own Geiger sound; the offline mix needs it too.
    logo = by_id.get("9.7")
    geiger = _wave("LogoGeiger")
    if logo and geiger:
        file_path = _export(geiger)
        if file_path:
            mix.append({"file": file_path, "start": round(logo["start"], 3), "gain_db": 0.0})
    with open(AUDIO_DIR + "/sfx.json", "w", encoding="utf-8") as handle:
        json.dump(mix, handle, indent=1)
    lib.safe_save([master.get_outermost()])
    print("add_sounds", len(mix), "sounds")
