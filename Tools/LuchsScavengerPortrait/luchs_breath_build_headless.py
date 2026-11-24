"""Throwaway-editor rebuild + capture of M_UI_LuchsScavengerRide (adds the filter breath, 2026-11-14).
Run in a throwaway editor, never the shared one:
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Rebuilds the material and brush (build_luchs_ride.run('material')), waits for the shaders, then draws the material at
the same editor clock with Breath 1 and Breath 0 (so the difference is the vapour only), plus a breath-only strip with
the other motion off. Frames and result.json go to Saved/LightTankPortrait.
"""
import json
import os

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/LuchsScavengerPortrait/'
OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/LightTankPortrait/'
TIMES = [round(0.1 * i, 2) for i in range(40)]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'ns': {}}


def log(message):
    S['log'].append(f"{S['t']:.2f} {message}")
    unreal.log('LUCHSBREATH ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(S['log'], open(OUT_DIR + 'breath_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    if S['phase'] == 'done':
        return
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            ns = S['ns']
            exec(open(TOOLS + 'build_luchs_ride.py', encoding='utf8').read(), ns)
            log('built ' + str(ns['run']('material')))
            S['phase'], S['mark'] = 'wait_shaders', S['t']
        elif phase == 'wait_shaders' and S['t'] - S['mark'] > 45.0:
            capture = S['ns']['capture']
            # One tick = one editor clock, so the on/off frames line up except for the vapour.
            log('on ' + str(capture(times=TIMES, prefix='breath_on')))
            log('off ' + str(capture(times=TIMES, prefix='breath_off', Breath=0.0)))
            log('still ' + str(capture(times=(0.0,), prefix='breath_still', Jolt=0.0, Nod=0.0, Flow=0.0,
                                       Flutter=0.0, Breath=0.0)))
            log('solo ' + str(capture(times=TIMES[:16], prefix='breath_solo', Jolt=0.0, Nod=0.0, Flow=0.0,
                                      Flutter=0.0)))
            finish()
        if S['t'] > 240.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
