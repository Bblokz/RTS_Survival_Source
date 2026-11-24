"""Sets the counterattack's timings and centre spawn on DA_Intro_SovietCounterattack (2026-11-23).

    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/set_counterattack_timing.py"

User: "make sure the attacks are 30 seconds faster" -> every difficulty's prepare countdown and the time between waves
are 30 s shorter than on 2026-11-22 (the Survive timer = waves x interval shrinks with it). The centre spawn moves to
the north map edge between the rail line and where the aircraft come in (z 0 = traced to the ground at runtime).
Values match the C++ constructor defaults of the same date; set explicitly so an old override in the asset goes.
Saves only the data asset.
"""
import json

import unreal

DA = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/DA_Intro_SovietCounterattack'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/set_counterattack_timing.json'
# difficulty: (prepare seconds, seconds between waves); was 180/100, 180/90, 150/90, 120/80, 120/80
TIMING = {'new_to_rts': (150, 70), 'normal': (150, 60), 'hard': (120, 60), 'brutal': (90, 50), 'ironman': (90, 50)}
CENTRE_SPAWN = unreal.Vector(15500.0, 23900.0, 0.0)

da = unreal.load_asset(DA)
out = {}
for level, (prepare, between) in TIMING.items():
    block = da.get_editor_property(level)
    before = (block.get_editor_property('seconds_until_soviets_arrive'),
              block.get_editor_property('seconds_between_waves'))
    block.set_editor_property('seconds_until_soviets_arrive', prepare)
    block.set_editor_property('seconds_between_waves', between)
    da.set_editor_property(level, block)
    out[level] = {'before': before, 'after': (prepare, between)}
spawns = da.get_editor_property('spawns')
old = spawns.get_editor_property('center_spawn_location')
spawns.set_editor_property('center_spawn_location', CENTRE_SPAWN)
da.set_editor_property('spawns', spawns)
out['center_spawn'] = {'before': [old.x, old.y, old.z], 'after': [CENTRE_SPAWN.x, CENTRE_SPAWN.y, CENTRE_SPAWN.z]}
out['saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(da, False))
with open(REPORT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('TIMING_SET ' + json.dumps(out))
