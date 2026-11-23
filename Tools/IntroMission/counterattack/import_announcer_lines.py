"""Imports finished announcer lines from BlenderTanks into the IntroMission voice line folder (2026-11-22).

    set ANNOUNCER_LINES=IntroMission_DigInFirstHeld,IntroMission_DigInAllHeld,IntroMission_ArrivalCue
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/import_announcer_lines.py"
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate

Each NAME comes from BlenderTanks Announcer/AAAGerMissions/IntroMission/NAME.wav (made by make_mission_lines.py) and
becomes a SoundWave with the cinematic/transmission sound class. Re-importing replaces the asset. Saves only the waves;
run new_voice_lines.py afterwards for the subtitle rows.
"""
import json
import os

import unreal

SOURCE_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/VoiceLines/Announcer/AAAGerMissions/IntroMission/'
WAVE_DIR = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission'
SCLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/import_announcer_lines.json'

names = [n.strip() for n in os.environ.get('ANNOUNCER_LINES', '').split(',') if n.strip()]
sound_class = unreal.load_asset(SCLASS)
out = {}
for name in names:
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', SOURCE_DIR + name + '.wav')
    task.set_editor_property('destination_path', WAVE_DIR)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    wave = unreal.load_asset(WAVE_DIR + '/' + name)
    if wave is None:
        out[name] = {'imported': False}
        continue
    wave.set_editor_property('sound_class_object', sound_class)
    out[name] = {'imported': True, 'seconds': wave.get_editor_property('duration'),
                 'saved': bool(unreal.EditorAssetLibrary.save_loaded_asset(wave, False))}
with open(REPORT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('IMPORT_LINES ' + json.dumps(out))
