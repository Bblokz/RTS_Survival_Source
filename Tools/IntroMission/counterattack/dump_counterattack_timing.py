"""Read-only: writes the difficulty timings and the centre spawn of DA_Intro_SovietCounterattack to a json (2026-11-23).

    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/dump_counterattack_timing.py"
"""
import json

import unreal

DA = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/DA_Intro_SovietCounterattack'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/counterattack_timing.json'
FIELDS = ('seconds_until_soviets_arrive', 'wave_count', 'seconds_between_waves', 'min_units_first_wave',
          'max_units_first_wave', 'units_added_per_wave')

da = unreal.load_asset(DA)
out = {}
for level in ('new_to_rts', 'normal', 'hard', 'brutal', 'ironman'):
    block = da.get_editor_property(level)
    out[level] = {f: block.get_editor_property(f) for f in FIELDS}
spawns = da.get_editor_property('spawns')
c = spawns.get_editor_property('center_spawn_location')
out['center_spawn_location'] = [c.x, c.y, c.z]
out['attack_route'] = [[p.x, p.y, p.z] for p in da.get_editor_property('attack_route')]
with open(OUT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('TIMING ' + json.dumps(out))
