"""Throwaway-editor test for the cinematic overlay (UW_CinematicOverlay: subtitle strip at the top + countdown).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Starts PIE, plays LS_FirstMission_FuelDepot (its camera bindings are missing on this map; the audio tracks play) with
UW_CinematicOverlay.ShowForSequence and takes UI screenshots (shot showui) at wall-clock SHOTS seconds, logging the
sequence time of each. Log: Saved/PortraitSubtitles/cinematic_result.json.
"""
import json
import os
import time

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/PortraitSubtitles/'
SEQUENCE = '/Game/RTS_Survival/Cinematics/GerMissions/0FirstMission/LS_FirstMission_FuelDepot'
# S_DepotCinematic_1 plays 0-28.4 s, S_DepotCinematic_2 28.77-37.0 s; the sequence ends at 37.27 s
SHOTS = [2.0, 6.0, 12.0, 20.0, 27.0, 30.5, 34.0, 36.5, 39.0]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': list(SHOTS)}


def log(message):
    wall = time.perf_counter() - S['wall0'] if 'wall0' in S else 0.0
    S['log'].append(f"wall {wall:6.2f} {message}")
    unreal.log('CINEMATICOVERLAYTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(S['log'], open(OUT_DIR + 'cinematic_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def sequence_seconds(player):
    t = player.get_current_time()
    frames = t.time.frame_number.value + t.time.get_editor_property('sub_frame')
    return frames * t.rate.denominator / t.rate.numerator


def start_cinematic(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    sequence = unreal.load_asset(SEQUENCE)
    settings = unreal.MovieSceneSequencePlaybackSettings()
    player, actor = unreal.LevelSequencePlayer.create_level_sequence_player(world, sequence, settings)
    S['player'], S['actor'] = player, actor
    overlay = unreal.W_CinematicOverlay.show_for_sequence(pc, player)
    log(f'overlay {overlay.get_class().get_name() if overlay else None} cues {overlay.get_subtitle_cue_count() if overlay else -1}')
    player.play()


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
                unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
                S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud' and S['t'] - S['mark'] > 4.0:
            start_cinematic(game_world())
            S['wall0'] = time.perf_counter()
            S['phase'] = 'shots'
        elif phase == 'shots':
            world = game_world()
            since = time.perf_counter() - S['wall0']
            if S['shots'] and since >= S['shots'][0]:
                at = S['shots'].pop(0)
                log(f'shot at {at:.1f}s sequence {sequence_seconds(S["player"]):.2f}s playing {S["player"].is_playing()}')
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
            elif not S['shots'] and since > SHOTS[-1] + 2.0:
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
                finish()
        if S['t'] > 240.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
