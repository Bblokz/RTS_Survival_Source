"""Saves the counterattack presentation's asset references on DA_Intro_SovietCounterattack (2026-11-22).

    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/set_counterattack_da_refs.py"
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate

Why: the marker class and the arrival sequence used to be C++ constructor defaults. A value equal to the class default
is not written into the data asset, so a cook would never see those references. They are now empty in C++ and set
here, on the asset, together with the announcer lines (when their SoundWaves exist):
  DigInIntro.MarkerClass          BP_Hint_DigIn
  ArrivalCinematic.Sequence       LS_Intro_FirstWaveArrival
  DigInIntro.FirstPositionHeldLine / AllPositionsHeldLine   IntroMission_DigInFirstHeld / IntroMission_DigInAllHeld
Needs a DebugGame build that has these fields (an older DLL would drop them on save).
Saves only the data asset.
"""
import json

import unreal

DA = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/DA_Intro_SovietCounterattack'
MARKER = '/Game/RTS_Survival/Blueprints/Missions/Hints/Digin/BP_Hint_DigIn'
SEQUENCE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/LS_Intro_FirstWaveArrival'
WAVES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/'
LINES = {'first_position_held_line': 'IntroMission_DigInFirstHeld',
         'all_positions_held_line': 'IntroMission_DigInAllHeld'}
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/set_counterattack_da_refs.json'

out = {}
da = unreal.load_asset(DA)
intro = da.get_editor_property('dig_in_intro')
intro.set_editor_property('marker_class', unreal.EditorAssetLibrary.load_blueprint_class(MARKER))
for field, name in LINES.items():
    if unreal.EditorAssetLibrary.does_asset_exist(WAVES + name):
        intro.set_editor_property(field, unreal.load_asset(WAVES + name))
        out[field] = name
da.set_editor_property('dig_in_intro', intro)
cine = da.get_editor_property('arrival_cinematic')
cine.set_editor_property('sequence', unreal.load_asset(SEQUENCE))
da.set_editor_property('arrival_cinematic', cine)

out['marker_class'] = str(da.get_editor_property('dig_in_intro').get_editor_property('marker_class'))
out['sequence'] = str(da.get_editor_property('arrival_cinematic').get_editor_property('sequence'))
out['voice_line'] = str(da.get_editor_property('dig_in_intro').get_editor_property('voice_line'))
out['saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(da, False))
with open(REPORT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('DA_REFS ' + json.dumps(out))
