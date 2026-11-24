"""Imports the dig-in announcer line and sets it on the counterattack settings (2026-11-22).

    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/import_digin_line.py"
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate

Source: BlenderTanks Announcer/AAAGerMissions/IntroMission/IntroMission_DigInPositions.wav (ElevenLabs take ->
make_mission_lines.py). Saves only the new SoundWave and DA_Intro_SovietCounterattack; no map or mission Blueprint.
Needs a DebugGame build that has FSovietCounterattackDigInIntroSettings (an older DLL would drop the field).
"""
import json

import unreal

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/VoiceLines/Announcer/AAAGerMissions/IntroMission/IntroMission_DigInPositions.wav'
WAVE_DIR = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission'
WAVE = WAVE_DIR + '/IntroMission_DigInPositions'
SCLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions'
DA = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/DA_Intro_SovietCounterattack'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/import_digin_line.json'

out = {}
task = unreal.AssetImportTask()
task.set_editor_property('filename', SOURCE)
task.set_editor_property('destination_path', WAVE_DIR)
task.set_editor_property('automated', True)
task.set_editor_property('replace_existing', True)
task.set_editor_property('save', False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = unreal.load_asset(WAVE)
out['wave'] = wave.get_path_name() if wave else None
if wave:
    wave.set_editor_property('sound_class_object', unreal.load_asset(SCLASS))
    out['wave_seconds'] = wave.get_editor_property('duration')
    out['wave_saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(wave, False))

da = unreal.load_asset(DA)
if da and wave:
    intro = da.get_editor_property('dig_in_intro')
    intro.set_editor_property('voice_line', wave)
    da.set_editor_property('dig_in_intro', intro)
    out['da_voice_line'] = str(da.get_editor_property('dig_in_intro').get_editor_property('voice_line'))
    out['da_marker_count'] = len(da.get_editor_property('dig_in_intro').get_editor_property('marker_locations'))
    out['da_saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(da, False))
with open(REPORT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('IMPORT_DIGIN ' + json.dumps(out))
