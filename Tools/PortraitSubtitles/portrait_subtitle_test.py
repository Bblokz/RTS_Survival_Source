"""Throwaway-editor test for the cinematic portrait subtitle (UW_PortraitSubtitle under W_BP_Portrait).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Starts PIE, plays FirstMission_ClearRoad_1 with the German announcer portrait through UPlayerPortraitManager and takes
UI screenshots (shot showui) at SHOTS seconds after the request. Log: Saved/PortraitSubtitles/result.json.
"""
import json
import os
import time

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/PortraitSubtitles/'
LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/FirstMission/'
        'FirstMission_ClearRoad_1.FirstMission_ClearRoad_1')
# word times in the final file: "Commandant," ~0.9 s, sentence 1 ends ~4.9 s, "After" ~6.4 s (see the transcript)
SHOTS = [1.4, 3.0, 4.6, 5.9, 7.6, 12.0, 21.0]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': list(SHOTS)}


def log(message):
    S['log'].append(f"{S['t']:.2f} wall {time.perf_counter() - S.get('wall0', time.perf_counter()):.2f} {message}")
    unreal.log('PORTRAITSUBTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(S['log'], open(OUT_DIR + 'result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def start_line(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    log(f'controller {pc.get_class().get_name() if pc else None}')
    manager = pc.get_component_by_class(unreal.PlayerPortraitManager)
    sound = unreal.load_asset(LINE)
    log(f'manager {manager} sound {sound.get_name()} duration {sound.get_editor_property("duration"):.2f}')
    portrait = getattr(unreal, 'RTSPortraitTypes', None)
    kind = portrait.GERMAN_ANNOUNCER if portrait is not None else 1
    manager.play_portrait(kind, sound)


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
                # The map boots paused behind "Press To Start"; press it like the start button does.
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                pc.call_method('ClickedStartGameButton')
                log('pressed start')
                S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud':
            world = game_world()
            if S['t'] - S['mark'] > 4.0:
                start_line(world)
                # Wall clock: a long frame (the screenshot capture) is not in the summed Slate deltas,
                # but the voice line plays on through it.
                S['wall0'] = time.perf_counter()
                S['phase'], S['mark'] = 'shots', S['t']
        elif phase == 'shots':
            world = game_world()
            since = time.perf_counter() - S['wall0']
            if S['shots'] and since >= S['shots'][0]:
                at = S['shots'].pop(0)
                # Switch state first: the screenshot command can tick Slate again and re-enter this function.
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
