"""Headless import of the Oder-Warthe radio kit (run in an UnrealEditor-Cmd -run=pythonscript commandlet).

Imports Wavs/*.wav to /Game/RTS_Survival/Sounds/OderWarthe/<sub>, sets sound classes AFTER import
(replace_existing resets them), builds Random cues via SoundCueFactoryNew.initial_sound_waves,
creates the two concurrency assets and verifies everything by reloading. Prints lines tagged OWKIT.
"""
import json
import os
import unreal

HERE = "D:/UE5Projects/RTS_Survival/Source/Tools/OderWartheRadio"
ROOT = "/Game/RTS_Survival/Sounds/OderWarthe"
CLASS_TRANSMISSIONS = "/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions"
CLASS_UI = "/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_UI"
BURST_TYPES = ["Routine", "Reply", "AttackOrder"]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
manifest = json.load(open(os.path.join(HERE, "manifest.json"), encoding="utf-8"))
cue_volume = manifest["cue_volume"]
failures = []


def log(message):
    unreal.log("OWKIT " + message)


def fail(message):
    failures.append(message)
    unreal.log_error("OWKIT FAIL " + message)


def import_wave(name, sub_folder, sound_class_path, looping=False, volume=None):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(HERE, "Wavs", name + ".wav").replace("\\", "/")
    task.destination_path = f"{ROOT}/{sub_folder}"
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    wave = unreal.load_asset(f"{ROOT}/{sub_folder}/{name}")
    if wave is None:
        fail(f"import {name}")
        return None
    wave.set_editor_property("sound_class_object", unreal.load_asset(sound_class_path))
    wave.set_editor_property("looping", looping)
    if volume is not None:
        wave.set_editor_property("volume", volume)
    eal.save_loaded_asset(wave, only_if_is_dirty=False)
    return wave


def make_concurrency(name):
    path = f"{ROOT}/{name}"
    asset = unreal.load_asset(path)
    if asset is None:
        asset = asset_tools.create_asset(name, ROOT, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    settings = asset.get_editor_property("concurrency")
    settings.set_editor_property("max_count", 1)
    settings.set_editor_property("limit_to_owner", False)
    settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
    asset.set_editor_property("concurrency", settings)
    eal.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def make_cue(name, waves, sound_class_path, volume, concurrency=None):
    path = f"{ROOT}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    factory = unreal.SoundCueFactoryNew()
    factory.set_editor_property("initial_sound_waves", waves)
    if len(factory.get_editor_property("initial_sound_waves")) != len(waves):
        fail(f"{name}: initial_sound_waves did not take the waves")
    cue = asset_tools.create_asset(name, ROOT, unreal.SoundCue, factory)
    cue.set_editor_property("sound_class_object", unreal.load_asset(sound_class_path))
    cue.set_editor_property("volume_multiplier", volume)
    if concurrency is not None:
        cue.set_editor_property("concurrency_set", {concurrency})
    eal.save_loaded_asset(cue, only_if_is_dirty=False)
    return cue


def main():
    morse = {t: [import_wave(e["name"], "Morse", CLASS_TRANSMISSIONS) for e in manifest["morse"][t]] for t in BURST_TYPES}
    static = {t: [import_wave(n, "Static", CLASS_TRANSMISSIONS) for n in manifest["static"][t]] for t in BURST_TYPES}
    lock_on = [import_wave(n, "Capture", CLASS_UI) for n in manifest["lock_on"]]
    # The jam bed is played as the looping wave itself (a cue's wave player would not loop).
    import_wave(manifest["jammer"], "Jammer", CLASS_TRANSMISSIONS, looping=True, volume=cue_volume["JammerBed"])

    burst_concurrency = make_concurrency("SC_OW_RadioBurst")
    static_concurrency = make_concurrency("SC_OW_RadioStatic")
    for burst_type in BURST_TYPES:
        make_cue(f"Cue_OW_Burst_{burst_type}", morse[burst_type], CLASS_TRANSMISSIONS,
                 cue_volume[f"Burst_{burst_type}"], burst_concurrency)
        make_cue(f"Cue_OW_Static_{burst_type}", static[burst_type], CLASS_TRANSMISSIONS,
                 cue_volume[f"Static_{burst_type}"], static_concurrency)
    make_cue("Cue_OW_StationLockOn", lock_on, CLASS_UI, cue_volume["StationLockOn"])
    verify()


def verify():
    for asset_path in sorted(eal.list_assets(ROOT, recursive=True, include_folder=False)):
        asset = unreal.load_asset(asset_path.split(".")[0])
        kind = asset.get_class().get_name()
        if kind == "SoundConcurrency":
            settings = asset.get_editor_property("concurrency")
            log(f"{asset_path} | {kind} | max={settings.get_editor_property('max_count')} "
                f"rule={settings.get_editor_property('resolution_rule')}")
            continue
        sound_class = asset.get_editor_property("sound_class_object")
        duration = asset.get_editor_property("duration")
        line = (f"{asset_path} | {kind} | class={sound_class.get_name() if sound_class else None} "
                f"| duration={duration:.2f}")
        if kind == "SoundWave":
            line += f" | looping={asset.get_editor_property('looping')} | volume={asset.get_editor_property('volume'):.2f}"
        else:
            concurrency = [c.get_name() for c in asset.get_editor_property("concurrency_set")]
            line += f" | volume={asset.get_editor_property('volume_multiplier'):.2f} | concurrency={concurrency}"
            if duration <= 0.05:
                fail(f"{asset_path}: cue has no playable duration (empty graph?)")
        if sound_class is None:
            fail(f"{asset_path}: no sound class")
        log(line)
    log("RESULT " + ("PASS" if not failures else "FAIL: " + "; ".join(failures)))


main()
