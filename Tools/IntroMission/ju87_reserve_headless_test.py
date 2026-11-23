"""Throwaway-editor PIE test of the Okhotnik intel + Ju 87 reserve (setup_ju87_reserve.py), 2026-11-22.

Exec through rear_editor_server (rear_cmd.py, after rear_lib.py) on Ger_IntroMission with the map and
Mission_Intro_ClearSovietRig already saved by setup_ju87_reserve. Before PIE the editor-world chain is cut to
[ClearSovietRig] in memory only (do NOT save the map after this test). Flow:
  start -> global ability panel hidden -> tour -> player tank onto the ramp -> assault response -> 4 Ju 87s ->
  Okhotnik squad takeover (closed like a player) -> 60 s game time after the strike: Ju 87 reserve on the panel
  (2 charges, 180 s, free, 4 aircraft) -> use 1 -> blocked during the cooldown -> use 2 after it -> panel gone.
Writes Saved/Ju87ReserveTest/result.json and HighResShots next to it; sets _defer so rear_cmd waits for the end.
"""
import json
import os
import sys
import time

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission'
sys.path.insert(0, HERE)
import setup_assault_response as assault  # noqa: E402
import setup_ju87_reserve as reserve  # noqa: E402

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/Ju87ReserveTest'
CLEAR = 'Mission_Intro_ClearSovietRig_C'
TAKEOVER = 'WBP_VehicleInfoTakeover_C'
TANK = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/'
        'Ger_Vehicle_PanzerIV_F1.Ger_Vehicle_PanzerIV_F1_C')
JU87 = reserve.ability_enum().GA_JU87_STRAFING
T = {'wall0': time.time(), 'phase': 'setup', 'mark': time.time(), 'log': [], 'checks': {}, 'done': False}


def wall():
    return time.time() - T['wall0']


def log(message):
    T['log'].append(f'{wall():7.1f} {message}')
    unreal.log_warning('JU87RESERVE ' + message)


def check(name, ok, detail=''):
    T['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def owner(actor):
    comp = actor.get_component_by_class(unreal.RTSComponent)
    return comp.call_method('GetOwningPlayer') if comp is not None else -1


def clear_mission(world):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == CLEAR and obj.get_world() == world:
            return obj
    return None


def takeovers(world):
    return [w for w in unreal.ObjectIterator(unreal.UserWidget)
            if w.get_class().get_name() == TAKEOVER and w.get_world() == world and w.is_in_viewport()]


def ability_state(world):
    keys = ('loaded', 'panel_visible', 'count', 'charges', 'cooldown', 'cooldown_left', 'costs', 'aircraft')
    return dict(zip(keys, unreal.MissionMapTestLibrary.map_test_get_player_global_ability_state(world, JU87)))


def player_planes(world):
    return [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AircraftMaster) if owner(p) == 1]


def shot(name):
    unreal.SystemLibrary.execute_console_command(game_world(), f'HighResShot 1600x900 filename={OUT_DIR}/{name}.png')


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    passed = sum(1 for c in T['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(T['checks']), 'checks': T['checks'], 'log': T['log']},
              open(OUT_DIR + '/result.json', 'w'), indent=1)
    log(f'RESULT {passed}/{len(T["checks"])}')
    if game_world() is not None:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    T['phase'], T['done'] = 'done', True


def go(phase):
    T['phase'], T['mark'] = phase, time.time()


def since_mark():
    return time.time() - T['mark']


def step_setup():
    editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.Actor)
               if a.get_class().get_name() == 'BP_MissionManager_C'][0]
    check('map_hides_global_abilities', manager.get_editor_property('hide_global_abilities_for_player'))
    clears = reserve.map_clear_missions(manager)
    check('clear_mission_found', bool(clears))
    if not clears:
        return finish()
    resp = clears[0].get_editor_property('assault_response')
    grant = resp.get_editor_property('granted_ability')
    check('saved_values', resp.get_editor_property('air_support_aircraft_count') == reserve.STRIKE_AIRCRAFT
          and str(resp.get_editor_property('intel_squad_library_entry')) == reserve.INTEL_SQUAD_ROW
          and grant.get_editor_property('charges') == reserve.GRANT_CHARGES
          and grant.get_editor_property('cooldown_seconds') == reserve.GRANT_COOLDOWN_SECONDS,
          f"aircraft {resp.get_editor_property('air_support_aircraft_count')} intel "
          f"{resp.get_editor_property('intel_squad_library_entry')} grant {grant.get_editor_property('ability')} "
          f"line {resp.get_editor_property('granted_ability_line')}")
    clears[0].set_editor_property('mission_trigger', None)
    manager.set_editor_property('missions', [clears[0]])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    go('wait_pie')


def step(world):
    phase = T['phase']
    if phase == 'wait_pie':
        if world is not None and since_mark() > 8.0:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc is not None:
                pc.call_method('ClickedStartGameButton')
                log('PIE up, start clicked')
                go('panel_hidden')
        elif since_mark() > 150.0:
            check('pie_started', False)
            finish()
        return
    if phase == 'panel_hidden':
        if since_mark() > 6.0:
            state = ability_state(world)
            check('panel_hidden_at_start', not state['panel_visible'] and not state['loaded'], state)
            go('wait_tour_end')
        return
    m = clear_mission(world)
    if phase == 'wait_tour_end':
        if m is not None and m.call_method('GetHasTourFinished'):
            go('spawn_tank')
        elif since_mark() > 400:
            check('tour_finished', False)
            finish()
        return
    if phase == 'spawn_tank':
        if since_mark() < 4.0:
            return
        unreal.MissionMapTestLibrary.map_test_spawn_actor(world, unreal.load_class(None, TANK),
                                                          unreal.Vector(-7600.0, 0.0, 300.0), unreal.Rotator(0, 0, 0))
        go('wait_tank')
        return
    if phase == 'wait_tank':
        tanks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)
                 if a.get_class().get_name() == TANK.split('.')[-1] and owner(a) == 1]
        if tanks and since_mark() > 3.0:
            c = assault.TRIGGER_CENTRE
            tanks[0].set_actor_location(unreal.Vector(c[0], c[1], 650.0), False, True)
            log('tank teleported onto the ramp')
            go('wait_launch')
        elif since_mark() > 60.0:
            check('player_unit_available', False)
            finish()
        return
    if phase == 'wait_launch':
        if m.call_method('GetHasAirSupportLaunched'):
            T['launch_game'] = unreal.GameplayStatics.get_time_seconds(world)
            T['max_planes_before_use'] = 0
            log(f"strike launched at game {T['launch_game']:.1f}")
            go('wait_intel')
        elif since_mark() > 60.0:
            check('air_support_launched', False)
            finish()
        return
    T['max_planes_before_use'] = max(T.get('max_planes_before_use', 0), len(player_planes(world))) \
        if 'use1' not in T else T.get('max_planes_before_use', 0)
    if phase == 'wait_intel':
        panels = takeovers(world)
        if panels:
            game_since = unreal.GameplayStatics.get_time_seconds(world) - T['launch_game']
            check('okhotnik_takeover_opens', m.call_method('GetHasShownIntelSquadTakeover'),
                  f'{game_since:.1f} s game time after the strike took off (expect >= {reserve.INTEL_DELAY_SECONDS})')
            check('takeover_pauses_game', unreal.GameplayStatics.is_game_paused(world))
            go('intel_open')
        elif since_mark() > 90.0:
            check('okhotnik_takeover_opens', False, 'no squad takeover 90 s after the strike')
            go('wait_grant')
        return
    if phase == 'intel_open':
        if since_mark() > 2.0 and 'intel_shot' not in T:
            T['intel_shot'] = True
            shot('okhotnik_takeover')
        if since_mark() > 4.0:
            panels = takeovers(world)
            if panels:
                panels[0].call_method('HandleUnderstoodClicked')
            go('wait_grant')
        return
    if phase == 'wait_grant':
        if m.call_method('GetHasGrantedAbility'):
            game_since = unreal.GameplayStatics.get_time_seconds(world) - T['launch_game']
            check('strike_flew_four_ju87s', T['max_planes_before_use'] == reserve.STRIKE_AIRCRAFT,
                  f"max {T['max_planes_before_use']} player aircraft before the reserve")
            check('reserve_granted_after_60s', abs(game_since - reserve.GRANT_DELAY_SECONDS) < 3.0,
                  f'{game_since:.1f} s game time after the strike took off')
            state = ability_state(world)
            check('reserve_on_panel', state['loaded'] and state['panel_visible'] and state['count'] == 1
                  and state['charges'] == 2 and state['cooldown'] == 180 and state['cooldown_left'] == 0
                  and state['costs'] == 0 and state['aircraft'] == 4, state)
            go('use1')
        elif since_mark() > 200.0:
            check('reserve_granted_after_60s', False, 'not granted')
            finish()
        return
    if phase == 'use1':
        if since_mark() < 3.0:
            return
        if 'panel_shot' not in T:
            T['panel_shot'] = True
            shot('reserve_panel')
            return
        target = unreal.Vector(*assault.STRIKE_TARGET)
        T['use1'] = unreal.MissionMapTestLibrary.map_test_use_player_global_ability(world, JU87, target)
        state = ability_state(world)
        check('use_1_spends_a_charge', T['use1'] and state['loaded'] and state['charges'] == 1
              and state['cooldown_left'] >= 170, state)
        blocked = unreal.MissionMapTestLibrary.map_test_use_player_global_ability(world, JU87, target)
        check('blocked_during_cooldown', not blocked and ability_state(world)['charges'] == 1, ability_state(world))
        T['use1_game'] = unreal.GameplayStatics.get_time_seconds(world)
        go('wait_cooldown')
        return
    if phase == 'wait_cooldown':
        planes = len(player_planes(world))
        T['max_planes_after_use'] = max(T.get('max_planes_after_use', 0), planes)
        state = ability_state(world)
        if state['cooldown_left'] == 0:
            waited = unreal.GameplayStatics.get_time_seconds(world) - T['use1_game']
            check('cooldown_180s', 175.0 < waited < 186.0, f'{waited:.1f} s game time')
            # The first strike keeps strafing for 150 s, so its 4 and the reserve's 4 overlap.
            check('use_1_flew_four_more_ju87s', T['max_planes_after_use'] >= reserve.STRIKE_AIRCRAFT + 4,
                  f"max {T['max_planes_after_use']} player aircraft after use 1")
            used = unreal.MissionMapTestLibrary.map_test_use_player_global_ability(
                world, JU87, unreal.Vector(*assault.STRIKE_TARGET))
            after = ability_state(world)
            check('use_2_spends_last_charge_and_leaves_panel', used and not after['loaded']
                  and not after['panel_visible'] and after['count'] == 0, after)
            finish()
        elif since_mark() > 260.0:
            check('cooldown_180s', False, state)
            finish()


def deferred():
    if T['done']:
        return True
    try:
        if T['phase'] == 'setup':
            step_setup()
        else:
            step(game_world())
    except Exception as exc:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        check('no_exception', False, str(exc))
        finish()
    return T['done']


os.makedirs(OUT_DIR, exist_ok=True)
log('ju87 reserve test loaded')
_defer = deferred
