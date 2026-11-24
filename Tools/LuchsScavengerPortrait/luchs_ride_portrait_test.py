"""Throwaway-editor test for the GermanLuchsScavenger portrait (W_BP_Portrait -> SB_Portrait_LuchsScavenger).
Run in a throwaway second editor, never the shared one:
  UnrealEditor.exe (Development, matches the shared editor DLL) RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Starts PIE, plays a Tiger crew voice line (only the portrait type matters) with the GermanLuchsScavenger portrait through
UPlayerPortraitManager and takes UI screenshots (shot showui) at SHOTS seconds. Log: Saved/LightTankPortrait/result.json.
"""
import json
import os
import time

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/LightTankPortrait/'
# A long line so the portrait stays up for the shots (only the portrait type matters).
LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/Portraits/RusHazmat/0FirstMission/DepotReinforcments/'
        'S_Hazmat_DepotReinforcements.S_Hazmat_DepotReinforcements')
# Shots are spaced by summed frame time (S['t']), not wall clock: each `shot showui` stalls the game for seconds and
# the material clock only advances by clamped frame deltas, so wall-clock spacing would capture near-identical frames.
SHOTS = [1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': list(SHOTS)}


def log(message):
    S['log'].append(f"{S['t']:.2f} wall {time.perf_counter() - S.get('wall0', time.perf_counter()):.2f} {message}")
    unreal.log('LUCHSRIDETEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(S['log'], open(OUT_DIR + 'result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def start_line(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    manager = pc.get_component_by_class(unreal.PlayerPortraitManager)
    sound = unreal.load_asset(LINE)
    log(f'manager {manager} sound {sound.get_name()} duration {sound.get_editor_property("duration"):.2f}')
    manager.play_portrait(unreal.RTSPortraitTypes.GERMAN_LUCHS_SCAVENGER, sound)


def tick(delta):
    if S['phase'] == 'done':
        return
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 8.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                pc.call_method('ClickedStartGameButton')
                log('pressed start')
                S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud':
            if S['t'] - S['mark'] > 4.0:
                start_line(game_world())
                S['wall0'] = time.perf_counter()
                S['phase'], S['mark'] = 'shots', S['t']
        elif phase == 'shots':
            world = game_world()
            since = S['t'] - S['mark']
            if S['shots'] and since >= S['shots'][0]:
                at = S['shots'].pop(0)
                log(f'shot at {at:.1f}s')
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
            elif not S['shots'] and since > SHOTS[-1] + 3.0:
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
                finish()
        if S['t'] > 180.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
