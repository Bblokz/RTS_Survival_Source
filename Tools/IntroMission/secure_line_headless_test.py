"""Throwaway-editor PIE test of Mission_Intro_ClearSovietRig ("Secure the line") and the glider gate behind it.

    UnrealEditor.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env SECURE_TEST_OUT   = result json path
    env SECURE_TEST_SHOTS = 1 takes a HighResShot per tour shot into Saved/Screenshots (default 1)

The saved chain is cut in the editor world before PIE (not saved): Missions = [ClearSovietRig with its trigger dropped,
BaseBuilding with its "Mission Class Completed = ClearSovietRig" trigger kept]. So the run starts where the ridge tanks
have just died and checks the order the player sees:
  tour (HUD hidden, bars, camera through the shots, back to the player's view) -> base life (4 Mosin + 1 PTRS squads
  marching, fuel truck parked at the rear, crane unloading) -> 7 targets with markers + minimap icons -> no glider while
  any target lives (killed one by one through the damage path) -> after the last one ClearSovietRig completes and
  BaseBuilding's Mammut glider comes.
"""
import json
import os
import time

import unreal

OUT = os.environ.get('SECURE_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/SecureLineTest/result.json')
SHOTS = os.environ.get('SECURE_TEST_SHOTS', '1') == '1'
CLEAR = 'Mission_Intro_ClearSovietRig_C'
BASE = 'Mission_Intro_BaseBuilding_C'
GLIDER = 'BP_GliderTankDelivery_Mammut_C'
TARGET_CLASSES = {'Rus_Building_CoolingTowers_Concrete_C': 2, 'Rus_Building_Barracks_Concrete_C': 2,
                  'Rus_BXP_GuardTowerMG_C': 2, 'Rus_BXP_LongCamoBunker_C': 1}
WANT_MOSIN, WANT_PTRS = 4, 1
S = {'wall0': time.time(), 'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'shots_seen': [], 'pawn': {}}


def wall():
    """Wall-clock seconds since the script loaded; the slate tick deltas under-count at low frame rates."""
    return time.time() - S['wall0']


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('SECURETEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'pawn': S['pawn'], 'log': S['log'],
               'pose_trace_pre': S.get('pose_trace_pre', [])[-5:], 'pose_trace_post': S.get('pose_trace_post', [])},
              open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def actors_of(world, class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def mission(world, class_name):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == class_name and obj.get_world() == world:
            return obj
    return None


def note(world):
    """Per tick: mission completion and the first glider are remembered the moment they happen (completed missions
    get garbage collected in PIE)."""
    m = mission(world, CLEAR)
    if m is not None and 'clear_done' not in S and m.call_method('GetIsMissionComplete'):
        S['clear_done'] = S['t']
        log('ClearSovietRig complete')
    if 'glider' not in S and actors_of(world, GLIDER):
        S['glider'] = S['t']
        log('Mammut glider seen')
    if m is not None and not S['shots_seen'] and not m.call_method('GetIsCameraTourPlaying'):
        S['pre_tour_pose'] = pawn_pose(world)
        S.setdefault('pose_trace_pre', []).append([round(wall(), 2)] + [round(v) for v in S['pre_tour_pose']])
    if 'tour_end_wall' in S and wall() - S['tour_end_wall'] < 3.0:
        S.setdefault('pose_trace_post', []).append([round(wall() - S['tour_end_wall'], 2)] +
                                                   [round(v) for v in pawn_pose(world)])
    if m is not None and 'tour_wall' in S and 'truck_park_wall' not in S and m.call_method('GetHasFuelTruckParked'):
        S['truck_park_wall'] = wall() - S['tour_wall']
        log(f"fuel truck parked {round(S['truck_park_wall'], 1)} s (wall) after the tour started")
    if m is not None and 'tour_wall' in S and wall() - S.get('truck_sample', 0.0) > 2.0:
        truck = m.call_method('GetFuelTruck')
        if truck is not None:
            S['truck_sample'] = wall()
            log(f"truck at {[round(c) for c in (truck.get_actor_location().x, truck.get_actor_location().y)]} "
                f"{round(wall() - S['tour_wall'], 1)} s into the tour")
    if SHOTS and S.get('ui_shot_due') and wall() > S['ui_shot_due']:
        # On its own tick: with the UI, so the cinema bars and the caption (or, after the tour, the HUD) show.
        S['ui_shot_due'] = None
        unreal.SystemLibrary.execute_console_command(world, 'shot showui')
        log('UI screenshot requested')
    if m is not None:
        shot = m.call_method('GetCameraTourShotIndex')
        if shot is not None and shot >= 0 and shot not in S['shots_seen']:
            S['shots_seen'].append(shot)
            S['shot_start_' + str(shot)] = wall()
            log(f'tour shot {shot}')
        if shot is not None and shot >= 0 and SHOTS and 'shot_pic_' + str(shot) not in S \
                and wall() - S['shot_start_' + str(shot)] > 2.5:
            S['shot_pic_' + str(shot)] = True
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720')
            if shot == 2:
                S['ui_shot_due'] = wall() + 0.8
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if pawn is not None:
                l = pawn.get_actor_location()
                r = pawn.get_actor_rotation()
                S['pawn'][str(shot)] = [round(l.x), round(l.y), round(l.z), round(r.pitch, 1), round(r.yaw, 1)]
            log(f'picture of shot {shot}, pawn {S["pawn"].get(str(shot))}')


def visible_cargo(truck):
    return sum(1 for c in truck.get_components_by_class(unreal.SceneComponent)
               if c.get_name().startswith('Cargo') and c.is_visible())


def objective_text(m):
    state = m.get_editor_property('mission_widget_state')
    return str(state.get_editor_property('mission_title')), str(state.get_editor_property('mission_description'))


def pawn_pose(world):
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is None:
        return None
    l = pawn.get_actor_location()
    r = pawn.get_actor_rotation()
    return [l.x, l.y, l.z, r.pitch, r.yaw]


def life_checks(world, m):
    life = m.call_method('GetBaseLifeActors') or []
    names = [a.get_class().get_name() for a in life]
    mosins = names.count('Rus_Squad_Mosin_C')
    ptrs = names.count('Rus_Squad_PTRS_C')
    check('life_squads_spawned', mosins >= WANT_MOSIN and ptrs >= WANT_PTRS, f'{names}')
    marching = m.call_method('GetMarchingSquadCount')
    check('life_squads_marching', marching >= WANT_MOSIN + WANT_PTRS - 1, f'{marching} of {mosins + ptrs} march')
    # Every squad stands inside the base walls.
    inside = [a for a in life if a.get_class().get_name() != 'Rus_ArmoredCar_ZilFuel_C'
              and -2700 < a.get_actor_location().x < 4150 and -4250 < a.get_actor_location().y < 5250]
    check('life_squads_inside_base', len(inside) == mosins + ptrs, f'{len(inside)} inside')


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world = game_world()
        if world is not None and phase not in ('wait_load', 'wait_pie', 'done'):
            note(world)

        if phase == 'wait_load' and S['t'] > 15.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            manager = actors_of(editor_world, 'BP_MissionManager_C')[0]
            nodes = {}
            for top in manager.get_editor_property('missions'):
                while top is not None:
                    nodes.setdefault(top.get_class().get_name(), top)
                    top = top.get_editor_property('next_mission')
            clear, base = nodes.get(CLEAR), nodes.get(BASE)
            check('chain_has_clear_and_base', clear is not None and base is not None, list(nodes))
            if clear is None or base is None:
                finish()
                return
            check('clear_is_secure_base_mission', isinstance(clear, unreal.SecureBaseMission),
                  clear.get_class().get_name())
            trigger = base.get_editor_property('mission_trigger')
            check('base_waits_for_clear', trigger is not None and CLEAR.rstrip('_C') in str(
                trigger.get_editor_property('mission_class_to_watch')), str(trigger))
            clear.set_editor_property('mission_trigger', None)
            manager.set_editor_property('missions', [clear, base])
            log('chain cut to [ClearSovietRig, BaseBuilding] (editor world only)')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return

        if phase == 'wait_pie':
            if world is not None and elapsed > 8.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                if pc is not None:
                    pc.call_method('ClickedStartGameButton')
                    S['start_pose'] = pawn_pose(world)
                    log(f'PIE up, start game clicked, pawn {S["start_pose"]}')
                    S['phase'], S['mark'] = 'wait_tour', S['t']
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return

        m = mission(world, CLEAR) if world is not None else None

        if phase == 'wait_tour':
            if m is not None and m.call_method('GetIsCameraTourPlaying') and m.call_method('GetCameraTourShotIndex') >= 0:
                check('tour_started', True, f'{round(elapsed, 1)} s after start')
                S['tour_wall'] = wall()
                overlays = [w for w in unreal.ObjectIterator(unreal.UserWidget)
                            if w.get_class().get_name() == 'W_MissionCameraTour' and w.get_world() == world
                            and w.is_in_viewport()]
                check('tour_overlay_shown', len(overlays) == 1, f'{len(overlays)} overlays')
                targets = m.call_method('GetLivingTargets') or []
                counts = {}
                for t in targets:
                    counts[t.get_class().get_name()] = counts.get(t.get_class().get_name(), 0) + 1
                check('targets_found', counts == TARGET_CLASSES and m.call_method('GetTargetCount') == 7, counts)
                markers = actors_of(world, 'BP_Hint_DestroyTarget_C')
                check('target_markers', len(markers) == 7, f'{len(markers)} markers')
                near = all(min(((mk.get_actor_location().x - t.get_actor_location().x) ** 2 +
                                (mk.get_actor_location().y - t.get_actor_location().y) ** 2) ** 0.5
                               for mk in markers) < 600 for t in targets) if markers else False
                check('markers_over_targets', near)
                bubbles = actors_of(world, 'GlobalAbilityVisionBubble')
                check('base_revealed', len(bubbles) >= 4, f'{len(bubbles)} vision bubbles')
                S['bubbles_at_start'] = len(bubbles)
                check('no_glider_at_tour', 'glider' not in S)
                S['phase'], S['mark'] = 'tour', S['t']
            elif elapsed > 60.0:
                check('tour_started', False, 'no tour after 60 s')
                finish()
            return

        if phase == 'tour':
            if m is not None and m.call_method('GetCameraTourShotIndex') == 1 and 'life_checked' not in S:
                S['life_checked'] = True
                life_checks(world, m)
            if m is not None and m.call_method('GetHasTourFinished'):
                check('tour_all_shots', S['shots_seen'] == list(range(len(m.get_editor_property('tour_shots')))),
                      S['shots_seen'])
                S['tour_end_wall'] = wall()
                S['ui_shot_due'] = wall() + 2.0
                tour_seconds = wall() - S['tour_wall']
                check('tour_length', 30.0 < tour_seconds < 45.0, f'{round(tour_seconds, 1)} s wall clock')
                overlays = [w for w in unreal.ObjectIterator(unreal.UserWidget)
                            if w.get_class().get_name() == 'W_MissionCameraTour' and w.get_world() == world
                            and w.is_in_viewport()]
                check('tour_overlay_gone', not overlays, f'{len(overlays)} overlays')
                end = pawn_pose(world)
                start = S.get('pre_tour_pose')
                moved = ((end[0] - start[0]) ** 2 + (end[1] - start[1]) ** 2) ** 0.5 if end and start else -1
                # The unattended editor's cursor sits in a screen corner, so edge scrolling drifts the camera
                # (diagonally) in the frames between the tour handing control back and this check.
                check('camera_back_to_player', 0 <= moved < 2500 and abs(end[4] - start[4]) < 1.0
                      and abs(end[3] - start[3]) < 1.0, f'start {start} end {end} moved {round(moved)}')
                check('clear_not_complete_after_tour', 'clear_done' not in S)
                S['phase'], S['mark'] = 'crane', S['t']
            elif elapsed > 90.0:
                check('tour_finished', False, 'tour still running after 90 s')
                finish()
            return

        if phase == 'crane':
            crane = m.call_method('GetFuelCrane') if m is not None else None
            parked = m.call_method('GetHasFuelTruckParked') if m is not None else False
            unloaded = crane.call_method('GetUnloadedCount') if crane is not None else 0
            truck = m.call_method('GetFuelTruck') if m is not None else None
            if truck is not None and parked and 'cargo_at_park' not in S:
                S['cargo_at_park'] = visible_cargo(truck)
                log(f"truck cargo visible when parked: {S['cargo_at_park']}")
            if parked and unloaded >= 2:
                cargo_now = visible_cargo(truck) if truck is not None else -1
                check('crane_takes_truck_cargo', S.get('cargo_at_park', 0) > 0 and 0 <= cargo_now < S['cargo_at_park'],
                      f"truck pallets visible {S.get('cargo_at_park')} at park, {cargo_now} after {unloaded} unloaded")
                parked_at = S.get('truck_park_wall')
                if truck is not None:
                    tl, ty = truck.get_actor_location(), truck.get_actor_rotation().yaw
                    off = ((tl.x - 4640) ** 2 + (tl.y + 1717) ** 2) ** 0.5
                    check('truck_parked_parallel_in_crane_reach', off < 250 and abs(((ty - 90) + 180) % 360 - 180) < 20,
                          f'truck at ({round(tl.x)}, {round(tl.y)}) yaw {round(ty, 1)}, {round(off)} cm from (4640,-1717)')
                check('truck_parks_during_bunker_shot', parked_at is not None and 22.0 < parked_at < 32.0,
                      f'parked {parked_at} s after the tour started (bunker shot ~26-34 s)')
                check('fuel_truck_parked_and_unloading', True,
                      f'truck at {truck.get_actor_location() if truck else None}, unloaded {unloaded}, '
                      f'{round(elapsed, 1)} s after the tour')
                S['phase'], S['mark'] = 'hold', S['t']
            elif elapsed > 90.0:
                check('fuel_truck_parked_and_unloading', False, f'parked {parked} crane {crane} unloaded {unloaded}')
                S['phase'], S['mark'] = 'hold', S['t']
            return

        if phase == 'hold':
            # Nothing may hand on while every target still stands.
            if elapsed > 15.0:
                check('no_glider_while_targets_live', 'glider' not in S and 'clear_done' not in S,
                      f"glider {S.get('glider')} clear {S.get('clear_done')}")
                S['phase'], S['mark'] = 'kill', S['t']
                S['kill_index'] = 0
            return

        if phase == 'kill':
            if elapsed < 4.0:
                return
            living = m.call_method('GetLivingTargets') or []
            destroyed = m.call_method('GetDestroyedTargetCount')
            markers = len(actors_of(world, 'BP_Hint_DestroyTarget_C'))
            index = S['kill_index']
            if index == 1:
                title, description = objective_text(m)
                check('objective_panel_counts', title == 'Secure the Line' and '1/7 destroyed' in description,
                      f'title {title!r} description {description!r}')
            if index > 0:
                ok = destroyed == index and markers == 7 - index and 'glider' not in S and 'clear_done' not in S
                S.setdefault('kill_ok', []).append(ok)
                log(f'after kill {index}: destroyed {destroyed}, markers {markers}, living {len(living)}, '
                    f'glider {S.get("glider")}')
            if not living:
                check('kills_counted_no_early_glider', all(S.get('kill_ok', [False])[:-1]),
                      S.get('kill_ok'))
                S['phase'], S['mark'] = 'after_kills', S['t']
                return
            target = living[0]
            log(f'killing {target.get_class().get_name()} at {target.get_actor_location()}')
            unreal.GameplayStatics.apply_damage(target, 1.0e7, None, None, unreal.DamageType)
            S['kill_index'] = index + 1
            S['mark'] = S['t']
            return

        if phase == 'after_kills':
            if 'clear_done' in S and 'glider' in S:
                check('clear_completed_after_last_target', True, f"at {round(S['clear_done'], 1)}")
                check('glider_after_clear', S['glider'] >= S['clear_done'],
                      f"glider {round(S['glider'], 1)} clear {round(S['clear_done'], 1)}")
                bubbles = actors_of(world, 'GlobalAbilityVisionBubble')
                check('reveal_expired', len(bubbles) == 0, f'{len(bubbles)} bubbles left')
                finish()
            elif elapsed > 45.0:
                check('clear_completed_after_last_target', 'clear_done' in S, S.get('clear_done'))
                check('glider_after_clear', 'glider' in S, S.get('glider'))
                finish()
            return
    except Exception as error:  # report and stop instead of spinning
        check('exception', False, repr(error))
        finish()


unreal.register_slate_post_tick_callback(tick)
