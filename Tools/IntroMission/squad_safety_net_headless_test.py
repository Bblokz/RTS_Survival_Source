"""Throwaway-editor PIE test of the intro squad safety net (setup_squad_safety_net.py), 2026-11-25.

Exec through rear_editor_server (rear_cmd.py, after rear_lib.py) on Ger_IntroMission. The settings are applied to the
editor-world mission manager in memory only (the map does not need to be saved; do NOT save it after this test).
Flow: start -> close the opening takeovers -> the player's Vulture squads stand (safety net armed) -> two squads are
killed (MapTest_KillSquad) -> one paratrooper drop of 2 Vulture squads at the nearest landing point -> 3 squads again
-> no second drop while the cooldown runs.
Writes Saved/SquadSafetyNetTest/result.json; sets _defer so rear_cmd waits for the end.
"""
import json
import os
import sys
import time

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission'
sys.path.insert(0, HERE)
import setup_squad_safety_net as net_setup  # noqa: E402

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/SquadSafetyNetTest'
VULTURE = 'Vulture'   # class name part of the player's Vulture squad BPs
T = {'wall0': time.time(), 'phase': 'setup', 'mark': time.time(), 'log': [], 'checks': {}, 'done': False}


def wall():
    return time.time() - T['wall0']


def log(message):
    T['log'].append(f'{wall():7.1f} {message}')
    unreal.log_warning('SQUADNET ' + message)


def check(name, ok, detail=''):
    T['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def go(phase):
    T['phase'], T['mark'] = phase, time.time()


def since():
    return time.time() - T['mark']


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def owner(actor):
    comp = actor.get_component_by_class(unreal.RTSComponent)
    return comp.call_method('GetOwningPlayer') if comp is not None else -1


def vulture_squads(world):
    """The player's Vulture squad controllers (dead ones included; the safety net's own count is the living one)."""
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SquadController)
            if owner(a) == 1 and VULTURE in a.get_class().get_name()]


def kill_all_but_one(net):
    """Kills living squads until one is left; MapTest_KillSquad returns 0 for a squad that is already dead."""
    living = net.get_last_counted_squads()
    killed = 0
    for squad in vulture_squads(game_world()):
        if killed >= living - 1:
            break
        if unreal.PlayerSquadSafetyNetSubsystem.map_test_kill_squad(squad) > 0:
            killed += 1
    return killed


def close_takeovers(world):
    for w in unreal.ObjectIterator(unreal.UserWidget):
        if w.get_world() == world and w.is_in_viewport() and 'Takeover' in w.get_class().get_name():
            try:
                w.call_method('HandleUnderstoodClicked')
            except Exception:  # noqa: BLE001 - widgets without the handler
                pass


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    passed = sum(1 for c in T['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(T['checks']), 'checks': T['checks'], 'log': T['log']},
              open(OUT_DIR + '/result.json', 'w'), indent=1)
    log(f'RESULT {passed}/{len(T["checks"])}')
    if game_world() is not None:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    T['phase'], T['done'] = 'done', True


def step(world):
    phase = T['phase']
    if phase == 'setup':
        net_setup.apply_map()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        go('wait_pie')
        return
    if phase == 'wait_pie':
        if world is not None and since() > 8.0:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc is not None:
                pc.call_method('ClickedStartGameButton')
                log('PIE up, start clicked')
                go('wait_armed')
        elif since() > 150.0:
            check('pie_started', False)
            finish()
        return
    if int(since() * 2) % 2 == 0:
        close_takeovers(world)
    net = unreal.PlayerSquadSafetyNetSubsystem.get_player_squad_safety_net_subsystem(world)
    if phase == 'wait_armed':
        squads = net.get_last_counted_squads() if net is not None else 0
        if net is not None and net.get_is_armed() and squads >= 2:
            T['start_squads'] = squads
            check('safety_net_armed', True, f'{squads} Vulture squads, watching {net.get_is_watching()}')
            check('no_drop_while_squads_stand', net.get_drop_count() == 0, net.get_drop_count())
            go('kill')
        elif since() > 180.0:
            check('safety_net_armed', False, f'net {net} squads {squads}')
            finish()
        return
    if phase == 'kill':
        if since() < 5.0:
            return
        killed = kill_all_but_one(net)
        log(f'killed {killed} squads of {net.get_last_counted_squads()}; one squad left')
        go('wait_drop')
        return
    if phase == 'wait_drop':
        if net.get_drop_count() >= 1:
            loc = net.get_last_drop_location()
            points = net_setup.LANDING_POINTS
            on_point = min(((loc.x - p[0]) ** 2 + (loc.y - p[1]) ** 2) ** 0.5 for p in points) < 1.0
            check('drop_sent_after_losses', True, f'{since():.1f} s after the kill, drop {net.get_drop_count()} '
                                                  f'at {loc}, counted {net.get_last_counted_squads()}')
            check('drop_on_a_landing_point', on_point, loc)
            go('wait_landed')
        elif since() > 30.0:
            check('drop_sent_after_losses', False, f'counted {net.get_last_counted_squads()}')
            finish()
        return
    if phase == 'wait_landed':
        squads = net.get_last_counted_squads()
        if squads >= 3 or since() > 90.0:
            check('back_to_three_squads', squads == 3, f'{squads} Vulture squads {since():.0f} s after drop')
            check('one_drop_only', net.get_drop_count() == 1, net.get_drop_count())
            go('cooldown')
        return
    if phase == 'cooldown':
        if since() < 3.0:
            return
        kill_all_but_one(net)
        go('cooldown_watch')
        return
    if phase == 'cooldown_watch':
        if since() > 8.0:
            check('no_second_drop_during_cooldown', net.get_drop_count() == 1,
                  f'drops {net.get_drop_count()} after losing squads again within the cooldown')
            finish()


def deferred():
    if T['done']:
        return True
    try:
        step(game_world())
    except Exception as exc:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        check('no_exception', False, str(exc))
        finish()
    return T['done']


os.makedirs(OUT_DIR, exist_ok=True)
log('squad safety net test loaded')
_defer = deferred
