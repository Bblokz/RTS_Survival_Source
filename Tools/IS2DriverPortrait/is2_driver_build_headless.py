"""Throwaway-editor build + capture of the SovietIS2Driver "Pribory" portrait (2026-11-15).
Run in a throwaway editor, never the shared one (needs an editor built with SovietIS2Driver for the 'point' step):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Engine/Maps/Entry
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Imports the textures, builds the material and brush, points W_BP_Portrait at the brush (build_is2_driver.run()), waits
for the shaders, then draws the material:
  still_*  all motion off (Tremor, Needles, Glow, Sway 0): must match the render with the canteen at rest.
  geo_*    40 frames 0.1 s apart with Glow 0: only geometry moves, so the head must not change at all.
  talk_*   the same 40 frames with everything on.
  quiet_*  the same 40 frames with Voice 0 (idle engine).
Frames and result.json go to Saved/IS2Portrait.
"""
import json
import os

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/IS2DriverPortrait/'
OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/IS2Portrait/'
TIMES = [round(0.1 * i, 2) for i in range(40)]
STEP = os.environ.get('IS2_DRIVER_STEP', 'all')
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'ns': {}}


def log(message):
    S['log'].append(f"{S['t']:.2f} {message}")
    unreal.log('IS2DRIVER ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(S['log'], open(OUT_DIR + 'build_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    if S['phase'] == 'done':
        return
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            # Imports and saves tick Slate from their progress dialogs, which re-enters this callback: leave the phase
            # first so the build never runs twice (the first run crashed in a nested save, 2026-11-15).
            S['phase'] = 'building'
            ns = S['ns']
            exec(open(TOOLS + 'build_is2_driver.py', encoding='utf8').read(), ns)
            log('built ' + str(ns['run'](STEP)))
            S['phase'], S['mark'] = 'wait_shaders', S['t']
        elif phase == 'wait_shaders' and S['t'] - S['mark'] > 45.0:
            S['phase'] = 'capturing'
            capture = S['ns']['capture']
            # One tick = one editor clock, so the frame sets line up.
            log('still ' + str(capture(times=(0.0,), prefix='still', Tremor=0.0, Needles=0.0, Glow=0.0,
                                       Sway=0.0)))
            log('geo ' + str(capture(times=TIMES, prefix='geo', Glow=0.0)))
            log('talk ' + str(capture(times=TIMES, prefix='talk')))
            log('quiet ' + str(capture(times=TIMES, prefix='quiet', Voice=0.0)))
            finish()
        if S['t'] > 300.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
