"""Throwaway-editor PIE test of Mission_Intro_SovietCounterattack (prepare timer -> waves from the centre, the landing
field and the train -> survive timer -> kill the remaining Soviets). 2026-11-18.

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env COUNTER_TEST_OUT = result json path

Before PIE (editor world + data asset in memory only, nothing saved): the manager's Missions are cut to one
counterattack instance without trigger, and every difficulty block of its data asset gets short test values
(prepare 15 s, 2 waves 70 s apart, 6 then 9 units). The cut chain has no player HQ, so groups head for the fallback.
"""
import json
import os
import time

import unreal

OUT = os.environ.get('COUNTER_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/CounterattackTest/result.json')
SHOT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackTest'
DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/'
BP = 'Mission_Intro_SovietCounterattack'
PREP, WAVES, BETWEEN, FIRST, ADDED = 15, 2, 70, 6, 3
S = {'wall0': time.time(), 'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'units': {},
     'samples': [], 'trains': {}, 'shots': set()}


def wall():
    return time.time() - S['wall0']


def log(message):
    S['log'].append(f"{wall():7.1f} {message}")
    unreal.log_warning('COUNTERTEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    units = {k: {kk: vv for kk, vv in v.items() if kk != 'actor'} for k, v in S['units'].items()}
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'log': S['log'],
               'units': units, 'samples': S['samples'][-40:]}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def v(x):
    return (x.x, x.y, x.z)


def d2(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def mission(world):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == BP + '_C' and obj.get_world() == world:
            return obj
    return None


def timer_text(world):
    for widget in unreal.ObjectIterator(unreal.W_MissionTimer):
        if widget.get_world() == world and widget.is_in_viewport():
            label = widget.get_editor_property('m_timer_text')
            clock = widget.get_editor_property('m_clock_text')
            return (str(label.get_text()) if label else '') + ' | ' + (str(clock.get_text()) if clock else '')
    return None


def objective_title(m):
    return str(m.get_editor_property('mission_widget_state').get_editor_property('mission_title'))


def shot(name, ui=False):
    if name in S['shots']:
        return
    S['shots'].add(name)
    world = game_world()
    if ui:
        unreal.SystemLibrary.execute_console_command(world, f'shot showui')
    else:
        unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1600x900 filename={SHOT_DIR}/{name}.png')
    log('shot ' + name)


def look_at(world, x, y):
    pawn = unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
    if pawn is not None:
        z = pawn.get_actor_location().z
        pawn.set_actor_location(unreal.Vector(x - 2500.0, y, z), False, True)


def settings_asset():
    return unreal.load_asset(DIR + 'DA_Intro_SovietCounterattack')


def shorten(da):
    for field in ('new_to_rts', 'normal', 'hard', 'brutal', 'ironman'):
        block = da.get_editor_property(field)
        block.set_editor_property('seconds_until_soviets_arrive', PREP)
        block.set_editor_property('wave_count', WAVES)
        block.set_editor_property('seconds_between_waves', BETWEEN)
        block.set_editor_property('min_units_first_wave', FIRST)
        block.set_editor_property('max_units_first_wave', FIRST)
        block.set_editor_property('units_added_per_wave', ADDED)
        da.set_editor_property(field, block)
    # 2026-11-22: the dig-in intro (takeover + tour) and the first-wave cinematic have their own test
    # (counterattack_presentation_test.py); off here so this test's timings stay as they were.
    for field in ('dig_in_intro', 'arrival_cinematic'):
        block = da.get_editor_property(field)
        block.set_editor_property('enabled', False)
        da.set_editor_property(field, block)


def classify(loc, da):
    """Which spawn a unit came from, by where it was first seen."""
    spawns = da.get_editor_property('spawns')
    centre = v(spawns.get_editor_property('center_spawn_location'))
    station = v(spawns.get_editor_property('station_spawn_location'))
    pads = [v(p) for p in spawns.get_editor_property('landing_pad_centers')]
    if d2(loc, centre) < 3500:
        return 'center'
    if d2(loc, station) < 3500:
        return 'train'
    if any(d2(loc, p) < 2500 for p in pads):
        return 'air'
    return 'elsewhere'


def sample(world, m, da):
    now = wall()
    living = m.call_method('GetLivingSovietUnits') or []
    locations = m.call_method('GetLivingSovietUnitLocations') or []
    for actor, location in zip(living, locations):
        key = actor.get_name()
        loc = v(location)
        row = S['units'].get(key)
        if row is None:
            row = {'actor': actor, 'class': actor.get_class().get_name(), 'first': loc, 'first_t': round(now, 1),
                   'wave': m.call_method('GetSpawnedWaveCount'), 'spawn': classify(loc, da), 'best_hq': 1e9}
            S['units'][key] = row
            log(f"new unit {row['class']} wave {row['wave']} at {tuple(round(c) for c in loc)} -> {row['spawn']}")
        row['last'] = loc
        row['best_hq'] = min(row['best_hq'], d2(loc, S['hq']))
    trains = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovietCargoTrain)
    for train in trains:
        rec = S['trains'].setdefault(train.get_name(), {'first': v(train.get_actor_location()), 'track': []})
        rec['track'].append((round(now, 1), tuple(round(c) for c in v(train.get_actor_location()))))
    planes = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AircraftMaster)]
    S['samples'].append({'t': round(now, 1), 'phase': m.call_method('GetPhase'), 'living': len(living),
                         'undelivered': m.call_method('GetUndeliveredUnitCount'), 'trains': len(trains),
                         'planes': len(planes), 'timer': timer_text(world), 'title': objective_title(m)})
    return S['samples'][-1]


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    world = game_world()
    if phase == 'wait_load' and S['t'] <= 15.0:
        return
    if phase == 'wait_load':
        editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.Actor)
                   if a.get_class().get_name() == 'BP_MissionManager_C'][0]
        found = None
        for top in manager.get_editor_property('missions'):
            if top is not None and top.get_class().get_name() == BP + '_C':
                found = top
        check('mission_in_saved_chain', found is not None,
              'Missions[] entry with trigger ' + str(found.get_editor_property('mission_trigger') if found else None))
        if found is None:
            found = unreal.new_object(unreal.load_asset(DIR + BP).generated_class(), outer=manager)
        else:
            trigger = found.get_editor_property('mission_trigger')
            watched = trigger.get_editor_property('mission_class_to_watch') if trigger else None
            check('trigger_waits_for_base_building', watched is not None
                  and watched.get_name() == 'Mission_Intro_BaseBuilding_C', watched)
        da = settings_asset()
        S['da'] = da
        check('settings_assigned', found.get_editor_property('settings') == da, found.get_editor_property('settings'))
        found.set_editor_property('settings', da)
        shorten(da)
        S['hq'] = v(da.get_editor_property('fallback_hq_location'))
        found.set_editor_property('mission_trigger', None)
        manager.set_editor_property('missions', [found])
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'wait_pie', S['t']
        return

    if phase == 'wait_pie':
        if world is not None and elapsed > 8.0:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc is not None:
                pc.call_method('ClickedStartGameButton')
                log('PIE up, start clicked')
                S['phase'], S['mark'] = 'wait_mission', S['t']
        elif elapsed > 120.0:
            check('pie_started', False)
            finish()
        return

    m = mission(world) if world is not None else None
    da = S['da']

    if phase == 'wait_mission':
        if m is not None and m.call_method('GetPhase') == 0 and timer_text(world):
            S['start'] = wall()
            text = timer_text(world)
            check('prepare_timer_shown', text.startswith('Prepare to defend'), text)
            check('prepare_objective', 'Prepare' in objective_title(m), objective_title(m))
            look_at(world, 6000.0, -1500.0)
            S['phase'], S['mark'] = 'prep', S['t']
        elif elapsed > 90.0:
            check('mission_started', False, 'no prepare timer 90 s after start')
            finish()
        return

    since = wall() - S['start']
    if phase == 'prep':
        if since > 4.0:
            shot('ui_prepare', ui=True)
        if m.call_method('GetPhase') == 1:
            S['waves_start'] = wall()
            check('waves_start_on_time', abs(since - PREP) < 4.0, f'{since:.1f} s after the start (prepare {PREP})')
            S['phase'], S['mark'], S['next_sample'] = 'waves', S['t'], 0.0
        elif since > PREP + 20:
            check('waves_start_on_time', False, f'still phase {m.call_method("GetPhase")} at {since:.1f} s')
            finish()
        return

    wsince = wall() - S.get('waves_start', wall())
    if phase in ('waves', 'remaining') and wall() >= S['next_sample']:
        S['next_sample'] = wall() + 1.5
        rec = sample(world, m, da)
        if len(S['samples']) % 6 == 0:
            log(f"t+{wsince:.0f} {rec}")
        if wsince > 3 and 'survive_text' not in S:
            S['survive_text'] = rec['timer']
            check('survive_timer_shown', rec['timer'] is not None and rec['timer'].startswith('Survive'), rec['timer'])
            shot('ui_survive', ui=True)
        trains = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovietCargoTrain)
        if trains and 'train_shot' not in S and wsince > 6:
            S['train_shot'] = True
            loc = trains[0].get_actor_location()
            look_at(world, loc.x, loc.y - 1500)
        elif 'train_shot' in S and 'train_shot_done' not in S and wsince > 9:
            S['train_shot_done'] = True
            shot('train_on_rails')

    if phase == 'waves':
        if m.call_method('GetPhase') == 2:
            S['remaining_start'] = wall()
            total = WAVES * BETWEEN
            check('survive_ends_on_time', abs(wsince - total) < 5.0, f'{wsince:.1f} s (expected {total})')
            check('both_waves_spawned', m.call_method('GetSpawnedWaveCount') == WAVES, m.call_method('GetSpawnedWaveCount'))
            check('timer_removed', timer_text(world) is None, timer_text(world))
            title = objective_title(m)
            check('remaining_objective', title.startswith('Kill remaining Soviet forces'), title)
            shot('ui_remaining', ui=True)
            S['phase'], S['mark'] = 'remaining', S['t']
        elif wsince > WAVES * BETWEEN + 20:
            check('survive_ends_on_time', False, f'phase {m.call_method("GetPhase")} at {wsince:.1f} s')
            evaluate()
            finish()
        return

    if phase == 'remaining':
        rsince = wall() - S['remaining_start']
        if rsince > 20.0 and m.call_method('GetPhase') == 2:
            if 'evaluated' not in S:
                S['evaluated'] = True
                evaluate()
            # Kill whatever lives; stragglers still in the air come later.
            for actor in m.call_method('GetLivingSovietUnits') or []:
                actor.destroy_actor()
        if m.call_method('GetPhase') == 3 or m.call_method('GetIsMissionComplete'):
            check('completes_when_all_dead', True, f'{rsince:.1f} s into the remaining phase')
            finish()
        elif rsince > 200.0:
            check('completes_when_all_dead', False,
                  f'phase {m.call_method("GetPhase")} undelivered {m.call_method("GetUndeliveredUnitCount")}')
            finish()


def evaluate():
    units = list(S['units'].values())
    by_wave = {}
    for u in units:
        by_wave.setdefault(u['wave'], []).append(u)
    w1 = by_wave.get(1, [])
    w2 = by_wave.get(2, [])
    check('wave1_size', len(w1) == FIRST, f'{len(w1)} units (expected {FIRST})')
    check('wave2_size', len(w2) >= FIRST + ADDED - 1, f'{len(w2)} units (expected {FIRST + ADDED}; late drops may count in wave 2)')
    for wave, rows in by_wave.items():
        spawns = {}
        for u in rows:
            spawns[u['spawn']] = spawns.get(u['spawn'], 0) + 1
        log(f'wave {wave} spawns {spawns}')
    spawns1 = {}
    for u in w1:
        spawns1[u['spawn']] = spawns1.get(u['spawn'], 0) + 1
    check('wave1_even_split', all(spawns1.get(k, 0) == FIRST // 3 for k in ('center', 'air', 'train')), spawns1)
    check('no_unit_elsewhere', not [u for u in units if u['spawn'] == 'elsewhere'],
          [(u['class'], tuple(round(c) for c in u['first'])) for u in units if u['spawn'] == 'elsewhere'])
    trains = S['trains']
    check('train_came', len(trains) >= 1, {k: (t['first'], t['track'][-1]) for k, t in trains.items()})
    # Groups drove towards the HQ: progress of at least 40 m for units that lived 40 s+.
    old = [u for u in units if u.get('last') and u['first_t'] < S['remaining_start'] - 40]
    moved = [u for u in old if d2(u['first'], S['hq']) - u['best_hq'] > 4000]
    check('groups_attack_move_to_hq', old and len(moved) >= 0.7 * len(old),
          f'{len(moved)}/{len(old)} moved 40 m+ closer to the HQ; best distances '
          + str(sorted(round(u['best_hq'] / 100) for u in old)))


def guarded(delta):
    try:
        tick(delta)
    except Exception as exc:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        if S['phase'] != 'done':
            check('no_exception', False, str(exc))
            finish()


unreal.register_slate_post_tick_callback(guarded)
log('counterattack test loaded')
