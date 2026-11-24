"""Sets the base reinforcements announcer line on DA_Intro_SovietCounterattack (2026-11-23).

Run it in the editor that has DA_Intro_SovietCounterattack loaded (it holds the file), after that editor runs a DLL
with USovietCounterattackSettings::BaseReinforcements:
    py "D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/set_base_reinforcements_line.py"
(or its text through UECP execute_python; it imports nothing but unreal and json).

Why on the asset: a value equal to the class default is not written into the data asset, so a soft/hard reference
set only in C++ would never be cooked. Also checks that the arrival cinematic still allows skipping.
Saves only the data asset.
"""
import json

import unreal

DA = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/DA_Intro_SovietCounterattack'
LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/'
        'IntroMission_BaseHeavyReinforcements')

da = unreal.load_asset(DA)
wave = unreal.load_asset(LINE)
out = {'wave': wave is not None}
# get_editor_property returns a copy of the struct: change it, then write the whole struct back.
reinforcements = da.get_editor_property('base_reinforcements')
reinforcements.set_editor_property('voice_line', wave)
da.set_editor_property('base_reinforcements', reinforcements)
arrival = da.get_editor_property('arrival_cinematic')
if not arrival.get_editor_property('allow_skipping'):
    arrival.set_editor_property('allow_skipping', True)
    da.set_editor_property('arrival_cinematic', arrival)
    out['skipping_turned_back_on'] = True
check = da.get_editor_property('base_reinforcements')
out['voice_line'] = str(check.get_editor_property('voice_line'))
out['breakthrough'] = [str(v) for v in check.get_editor_property('breakthrough_vehicles')]
out['strike_division'] = [str(v) for v in check.get_editor_property('strike_division_vehicles')]
out['allow_skipping'] = bool(da.get_editor_property('arrival_cinematic').get_editor_property('allow_skipping'))
out['saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(da, False))
print('SET_BASE_REINFORCEMENTS ' + json.dumps(out))
