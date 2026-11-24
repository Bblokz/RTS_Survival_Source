"""Throwaway-editor test for the mission location intro (UMissionBase::ShowLocationIntro).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Starts PIE, presses start, runs the intro on a throwaway mission (MissionMapTestLibrary) and takes UI screenshots
(shot showui) at wall-clock SHOTS seconds. Then checks the end state: intro over, screen handed back (HUD and camera
input back), camera moved by the drift and turned back to its start rotation.
Log: Saved/LocationIntro/location_intro_result.json; screenshots in Saved/Screenshots/WindowsEditor.
Set LOCATION_INTRO_FADE=1 in the environment for a run with the fade from black.
Set LOCATION_INTRO_CHAIN=1 for the designer chain: an instant and a 2.5 s MoveCamera right before the node, and
the mission completed (into a next mission) straight after it; the intro must start where the move lands.
Set LOCATION_INTRO_BUSY=1 to lock the screen (PauseAndLockGame) before the node runs: the intro must wait (no screen
taken, camera still) and play once the lock is released.
"""
import json
import os
import time

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/LocationIntro/'
FADE_FROM_BLACK = os.environ.get('LOCATION_INTRO_FADE', '0') == '1'
SCREEN_BUSY = os.environ.get('LOCATION_INTRO_BUSY', '0') == '1'
CHAIN = os.environ.get('LOCATION_INTRO_CHAIN', '0') == '1'
CHAIN_MOVE_SECONDS = 2.5
CHAIN_INSTANT_OFFSET = unreal.Vector(-1500.0, 0.0, 0.0)
CHAIN_TIMED_OFFSET = unreal.Vector(-1500.0, 1500.0, 0.0)
BUSY_SECONDS = 4.0
# Camera spot with ground in view (the map's camera start is in a corner).
CAMERA_SPOT = unreal.Vector(-16000.0, -10000.0, 0.0)
# Long, so the ~6 s stall of each 'shot showui' does not run the intro out between shots.
DURATION = 16.0 if SCREEN_BUSY or CHAIN else 30.0
# In the chain run the intro first waits for the camera move to land.
START_DELAY = CHAIN_MOVE_SECONDS if CHAIN else 0.0
DRIFT = 1200.0
PAN_YAW = 12.0
SHOTS = [0.35, 0.9, 1.6, 2.6, 4.2, 9.0]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': list(SHOTS), 'checks': {}}


def log(message):
    wall = time.perf_counter() - S['wall0'] if 'wall0' in S else 0.0
    S['log'].append(f"wall {wall:6.2f} {message}")
    unreal.log('LOCATIONINTROTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump({'log': S['log'], 'checks': S['checks']}, open(OUT_DIR + 'location_intro_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def intro_state(world):
    playing, handed_back, location, rotation = unreal.MissionMapTestLibrary.map_test_get_location_intro_state(world)
    return playing, handed_back, location, rotation


def move_camera_to_spot(world):
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    location = pawn.get_actor_location()
    pawn.set_actor_location(unreal.Vector(CAMERA_SPOT.x, CAMERA_SPOT.y, location.z), False, False)


def set_screen_lock(world, locked):
    unreal.GameplayStatics.get_player_controller(world, 0).call_method('PauseAndLockGame', (locked,))
    log(f'screen lock {locked}')


def queue_camera_moves(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = pc.get_component_by_class(unreal.PlayerCameraController)
    start = unreal.GameplayStatics.get_player_pawn(world, 0).get_actor_location()
    for offset, seconds in ((CHAIN_INSTANT_OFFSET, 0.0), (CHAIN_TIMED_OFFSET, CHAIN_MOVE_SECONDS)):
        move = unreal.MovePlayerCamera()
        move.set_editor_property('move_to_location', unreal.Vector(start.x + offset.x, start.y + offset.y, start.z))
        move.set_editor_property('time_to_move', seconds)
        camera.move_camera_over_time(move)
    S['expected_start'] = unreal.Vector(start.x + CHAIN_TIMED_OFFSET.x, start.y + CHAIN_TIMED_OFFSET.y, start.z)
    log(f'queued instant + {CHAIN_MOVE_SECONDS}s camera moves')


def start_intro(world):
    texts = unreal.LocationIntroTexts()
    texts.set_editor_property('location', unreal.Text('Berlin'))
    texts.set_editor_property('location_subtitle', unreal.Text('Reichstag district, Germany'))
    texts.set_editor_property('date', unreal.Text('21 April 1945'))
    texts.set_editor_property('time_of_day', unreal.Text('05:40 hrs'))
    pan = unreal.LocationIntroPanSettings()
    pan.set_editor_property('yaw_degrees', PAN_YAW)
    presentation = unreal.LocationIntroPresentation()
    presentation.set_editor_property('fade_from_black', FADE_FROM_BLACK)
    _, _, location, rotation = intro_state(world)
    S['start_location'], S['start_rotation'] = location, rotation
    if CHAIN:
        queue_camera_moves(world)
        S['start_location'] = S['expected_start']
    started = unreal.MissionMapTestLibrary.map_test_start_location_intro(
        world, texts, unreal.LocationIntroDriftAxis.WORLD_X, DRIFT, DURATION, True, pan, presentation, CHAIN)
    S['checks']['started'] = bool(started)
    log(f'started {started} camera {location} rotation {rotation}')


def check_running(world):
    playing, handed_back, location, rotation = intro_state(world)
    S['checks']['running_playing'] = bool(playing)
    if CHAIN:
        # Still inside the 2.5 s camera move: queued, screen not taken yet.
        S['checks']['chain_waits_for_move'] = bool(playing) and bool(handed_back)
    else:
        S['checks']['running_screen_taken'] = not handed_back
    log(f'running: playing {playing} handed_back {handed_back} camera {location} rotation {rotation}')


def check_end(world):
    playing, handed_back, location, rotation = intro_state(world)
    start_location, start_rotation = S['start_location'], S['start_rotation']
    moved_x = location.x - start_location.x
    moved_y = location.y - start_location.y
    yaw_error = abs((rotation.yaw - start_rotation.yaw + 180.0) % 360.0 - 180.0)
    S['checks'].update({
        'end_not_playing': not playing,
        'end_screen_handed_back': bool(handed_back),
        'end_moved_x': round(moved_x, 1),
        'end_moved_y': round(moved_y, 1),
        'end_drift_ok': abs(moved_x - DRIFT) < 5.0 and abs(moved_y) < 5.0,
        'end_rotation_restored': yaw_error < 0.5 and abs(rotation.pitch - start_rotation.pitch) < 0.5,
    })
    log(f'end: playing {playing} handed_back {handed_back} moved ({moved_x:.1f}, {moved_y:.1f}) rotation {rotation}')


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
            world = game_world()
            move_camera_to_spot(world)
            unreal.SystemLibrary.execute_console_command(world, 'shot showui')
            S['phase'], S['mark'] = 'before_shot', time.perf_counter()
        elif phase == 'before_shot' and time.perf_counter() - S['mark'] > 2.0:
            world = game_world()
            if SCREEN_BUSY:
                set_screen_lock(world, True)
            start_intro(world)
            S['wall0'] = time.perf_counter()
            S['phase'] = 'busy' if SCREEN_BUSY else 'shots'
        elif phase == 'busy' and time.perf_counter() - S['wall0'] > BUSY_SECONDS:
            world = game_world()
            playing, handed_back, location, _ = intro_state(world)
            S['checks']['busy_waiting'] = bool(playing)
            S['checks']['busy_camera_still'] = abs(location.x - S['start_location'].x) < 0.5
            log(f'busy: playing {playing} handed_back {handed_back} camera {location}')
            set_screen_lock(world, False)
            S['wall0'] = time.perf_counter()
            S['phase'] = 'shots'
        elif phase == 'shots':
            world = game_world()
            since = time.perf_counter() - S['wall0']
            if S['shots'] and since >= S['shots'][0]:
                at = S['shots'].pop(0)
                if at == SHOTS[2]:
                    check_running(world)
                log(f'shot at {at:.2f}s')
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
            elif not S['shots'] and since > START_DELAY + DURATION - 0.7:
                log('shot during fade-out')
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
                S['phase'] = 'wait_end'
        elif phase == 'wait_end':
            since = time.perf_counter() - S['wall0']
            if since > START_DELAY + DURATION + 2.0:
                world = game_world()
                check_end(world)
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
                S['phase'], S['mark'] = 'after_shot', time.perf_counter()
        elif phase == 'after_shot' and time.perf_counter() - S['mark'] > 8.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            finish()
        if S['t'] > 400.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
