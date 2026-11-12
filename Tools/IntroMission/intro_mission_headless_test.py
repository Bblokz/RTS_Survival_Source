"""Throwaway-editor PIE test of the first Ger_IntroMission missions (never run it in the shared editor).

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env INTRO_TEST_PATH  = kill | trigger   how Mission_Intro_SovietPatrols ends (kill the 3 squads / cross the
                                            sandbag line)
    env INTRO_TEST_FACTION = none | strike | breakthrough   backup faction (none: the game instance decides)
    env INTRO_TEST_OUT   = result json path

Chain under test (loaded from the saved map, so it also proves the chain persisted):
  Mission_Intro_MoveTo -> Mission_Intro_SovietPatrols -> Mission_Intro_HangarT26s
Steps: player soldiers are teleported onto the move-to point; the patrol mission must spawn 2 more Mosin squads that
march; then either the 3 squads are destroyed or the soldiers are teleported over the sandbag line; the T-26 mission
must spawn 3 T-26s that leave the hangars towards the Vulture start plus 4 panzers of the faction's variant; the T-26s
are destroyed and the T-26 mission must complete.
Fuel convoy (started by Mission_Intro_MoveTo when the camera takeover closes): one Zil fuel truck + one Mosin squad
marching behind it must spawn at the left map edge on the two-track path, cross the main road and drive on up the hill.
The run waits for the convoy after the missions end and logs when it crossed the road (seconds after the takeover).
"""
import json
import os

import unreal

PATH = os.environ.get('INTRO_TEST_PATH', 'kill')
FACTION = os.environ.get('INTRO_TEST_FACTION', 'none')
OUT = os.environ.get('INTRO_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/IntroMissionTest/result.json')
MOVE_TO = unreal.Vector(-15696.8, -1275.0, 200.0)
PAST_SANDBAGS = unreal.Vector(-13900.0, 600.0, 200.0)
FAR_BEHIND_START = unreal.Vector(-25000.0, 4500.0, 200.0)
# Camera controls, camera panning, selecting units (ECameraControlsTakeoverPage).
CAMERA_TAKEOVER_PAGES = 3
# T-26 spawn points: the hangar mouths (a solid roof hull fills each hangar above ~3 m, so nothing fits inside).
HANGARS = [unreal.Vector(-11550, -2416, 160), unreal.Vector(-11550, -5375, 160), unreal.Vector(-13016, -6550, 160)]
# BP_GroundPathSpline_TwoTrack sampled every 15 m (same points as Mission_Intro_MoveTo.IntroConvoyRoutePoints).
TWO_TRACK = [(-5780, -24760), (-5780, -23261), (-5780, -21762), (-5780, -20260), (-5779, -18759), (-5665, -17264),
             (-5958, -15819), (-6913, -14667), (-8019, -13655), (-9284, -12854), (-10716, -12418), (-11739, -11342),
             (-13152, -11056), (-14642, -11019), (-15836, -10196), (-16370, -8804), (-16187, -7322), (-16057, -5834),
             (-16083, -4334), (-15946, -2847), (-15441, -1441), (-14532, -252), (-13354, 675), (-11913, 958),
             (-10413, 950), (-8912, 961), (-7414, 886), (-5917, 781), (-4434, 790), (-2960, 790), (-1460, 790),
             (-640, 790)]
ROAD_CROSSING = unreal.Vector(-15389, -1355, 175)
CONVOY_TRUCK = 'Rus_ArmoredCar_ZilFuel_C'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'samples': {}}


def route_distance(location):
    """Distance along the two-track (cm) of the closest point to a location, and how far off the path it is."""
    best, walked = None, 0.0
    for (ax, ay), (bx, by) in zip(TWO_TRACK, TWO_TRACK[1:]):
        dx, dy = bx - ax, by - ay
        seg = (dx * dx + dy * dy) ** 0.5
        u = max(0.0, min(1.0, ((location.x - ax) * dx + (location.y - ay) * dy) / (seg * seg)))
        px, py = ax + u * dx, ay + u * dy
        off = ((location.x - px) ** 2 + (location.y - py) ** 2) ** 0.5
        if best is None or off < best[1]:
            best = (walked + u * seg, off)
        walked += seg
    return best


def patrol_squads(world):
    # The convoy's escort squad is a Rus_Squad_Mosin too; the patrol checks must not count or kill it.
    return [q for q in actors_of(world, 'Rus_Squad_Mosin_C') if q.get_name() != S.get('convoy_squad')]


def enemy_soldiers_near(world, centre, radius):
    out = []
    for soldier in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
        comp = soldier.get_component_by_class(unreal.RTSComponent)
        if comp is None or comp.get_editor_property('owning_player') != 2:
            continue
        if (soldier.get_actor_location() - centre).length() < radius:
            out.append(soldier)
    return out


def watch_convoy(world):
    """Identifies the convoy truck and squad when they appear, then samples them every 5 s."""
    if 'camera_closed' not in S:
        return
    since = S['t'] - S['camera_closed']
    convoys = actors_of(world, 'Convoy')
    if convoys and 'convoy_seen' not in S:
        S['convoy_seen'] = since
        log(f'convoy actor seen {since:.1f} s after the camera takeover closed')
    trucks = actors_of(world, CONVOY_TRUCK)
    if trucks and 'convoy_truck' not in S:
        S['convoy_truck'] = trucks[0].get_name()
        S['convoy_truck_first'] = loc(trucks[0])
        log(f'convoy truck first seen at {loc(trucks[0])} ({since:.1f} s)')
    if 'convoy_squad' not in S:
        for squad in actors_of(world, 'Rus_Squad_Mosin_C'):
            if squad.get_actor_location().y < -15000.0:
                S['convoy_squad'], S['convoy_squad_first'] = squad.get_name(), loc(squad)
                log(f'convoy squad {squad.get_name()} first seen at {loc(squad)} ({since:.1f} s)')
    truck = next((t for t in trucks if t.get_name() == S.get('convoy_truck')), None)
    if truck is None:
        return
    S['convoy_truck_last'] = (since, loc(truck))
    if 'convoy_crossed' not in S and (truck.get_actor_location() - ROAD_CROSSING).length() < 900.0:
        S['convoy_crossed'] = since
        log(f'convoy truck crossed the main road {since:.1f} s after the camera takeover closed')
    if S['t'] - S.get('convoy_sample_t', -99.0) < 5.0:
        return
    S['convoy_sample_t'] = S['t']
    truck_d, truck_off = route_distance(truck.get_actor_location())
    row = {'t': round(since, 1), 'truck': loc(truck), 'truck_d': round(truck_d), 'truck_off': round(truck_off)}
    squad = next((q for q in actors_of(world, 'Rus_Squad_Mosin_C') if q.get_name() == S.get('convoy_squad')), None)
    if squad is not None:
        soldiers = enemy_soldiers_near(world, squad.get_actor_location(), 3000.0)
        centre = squad.get_actor_location()
        if soldiers:
            centre = sum((s.get_actor_location() for s in soldiers), unreal.Vector(0, 0, 0)) / len(soldiers)
        squad_d, squad_off = route_distance(centre)
        row.update({'squad': [round(centre.x), round(centre.y)], 'squad_d': round(squad_d),
                    'squad_off': round(squad_off), 'soldiers': len(soldiers)})
    if convoys:
        row['state'] = str(convoys[0].call_method('GetConvoyState'))
    S['samples'].setdefault('convoy', []).append(row)
    log(f'convoy {row}')


def convoy_checks(world):
    rows = S['samples'].get('convoy', [])
    check('convoy_started', 'convoy_seen' in S and S['convoy_seen'] < 15.0, f"seen at {S.get('convoy_seen')}")
    start = unreal.Vector(TWO_TRACK[0][0], TWO_TRACK[0][1], 150)
    first = S.get('convoy_truck_first')
    check('convoy_truck_at_map_edge', first is not None and (unreal.Vector(*first) - start).length() < 3500,
          f'truck first seen at {first}')
    check('convoy_squad_spawned', S.get('convoy_squad_first') is not None,
          f"squad first seen at {S.get('convoy_squad_first')}")
    paired = [r for r in rows if 'squad_d' in r and r['t'] > 15.0]
    behind = [r for r in paired if r['squad_d'] < r['truck_d'] and r['truck_d'] - r['squad_d'] < 5000]
    check('convoy_squad_follows_truck', paired and len(behind) >= 0.8 * len(paired),
          f'{len(behind)} of {len(paired)} samples with the squad 0-50 m behind the truck along the path')
    on_path = [r for r in rows if r['truck_off'] < 1200]
    check('convoy_truck_on_two_track', rows and len(on_path) >= 0.9 * len(rows),
          f'{len(on_path)} of {len(rows)} samples within 12 m of the two-track')
    check('convoy_crossed_road', 'convoy_crossed' in S, f"{S.get('convoy_crossed')} s after the camera takeover")
    last = S.get('convoy_truck_last')
    far = last is not None and route_distance(unreal.Vector(*last[1]))[0] > 40000
    gone = 'convoy_crossed' in S and not actors_of(world, CONVOY_TRUCK)
    check('convoy_climbed_hill', far or gone, f'last truck sample {last} gone={gone}')
    # AConvoy despawns its units once the trucks are within ExitRadius of the route end (the hilltop).
    check('convoy_left_at_exit', gone, f'truck gone={gone}, last sample {last}')


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('INTROTEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': detail}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({'path': PATH, 'faction': FACTION, 'checks': S['checks'], 'samples': S['samples'], 'log': S['log']},
              open(OUT, 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def actors_of(world, class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def player_soldiers(world):
    out = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
        comp = actor.get_component_by_class(unreal.RTSComponent)
        if comp is not None and comp.get_editor_property('owning_player') == 1:
            out.append(actor)
    return out


def teleport_soldiers(world, spot):
    # Soldiers walk back to their squad controller, so the controllers go along.
    for controller in actors_of(world, 'Squad_Ger_HazmatVultures_C'):
        controller.set_actor_location(spot, False, True)
    soldiers = player_soldiers(world)
    for index, soldier in enumerate(soldiers):
        offset = unreal.Vector((index % 4) * 120.0, (index // 4) * 120.0, 0.0)
        soldier.set_actor_location(spot + offset, False, True)
    log(f'teleported {len(soldiers)} player soldiers to {spot}')
    return len(soldiers)


TAKEOVER_CLASS = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WBP_CameraControlsTakeover.WBP_CameraControlsTakeover_C'


def takeovers(world, class_name):
    # WidgetBlueprintLibrary is not exposed to this editor's Python; walk the live user widgets instead.
    return [w for w in unreal.ObjectIterator(unreal.UserWidget)
            if w.get_class().get_name() == class_name and w.get_world() == world and w.is_in_viewport()]


def camera_takeovers(world):
    return takeovers(world, 'WBP_CameraControlsTakeover_C')


def missions(world):
    found = []
    for name in ('Mission_Intro_MoveTo_C', 'Mission_Intro_SovietPatrols_C', 'Mission_Intro_HangarT26s_C',
                 'Mission_Intro_BaseBuilding_C'):
        found += [o for o in unreal.ObjectIterator(unreal.MissionBase) if o.get_class().get_name() == name
                  and o.get_world() == world]
    return found


def mission_complete(world, class_name):
    for mission in missions(world):
        if mission.get_class().get_name() == class_name:
            return bool(mission.call_method('GetIsMissionComplete'))
    return None


def loc(actor):
    l = actor.get_actor_location()
    return [round(l.x), round(l.y), round(l.z)]


def sample(world, key, class_name):
    S['samples'].setdefault(key, []).append([loc(a) for a in actors_of(world, class_name)])


def keep_soldiers_away(world):
    """The Vultures are jump infantry that charge any enemy in sight; while pinned they are put back far behind the
    Vulture start every half second, so they neither cross the sandbag line nor hold up the T-26s by fighting."""
    if not S.get('pin') or S['t'] - S.get('pin_t', 0.0) < 0.5:
        return
    S['pin_t'] = S['t']
    for controller in actors_of(world, 'Squad_Ger_HazmatVultures_C'):
        controller.set_actor_location(FAR_BEHIND_START, False, True)
    for index, soldier in enumerate(player_soldiers(world)):
        soldier.set_actor_location(FAR_BEHIND_START + unreal.Vector((index % 4) * 120.0, (index // 4) * 120.0, 0.0),
                                   False, True)


def note_t26_spawns(world):
    first_seen = S.setdefault('t26_first', {})
    for tank in actors_of(world, 'Rus_Vehicle_T26_C'):
        if tank.get_name() not in first_seen:
            first_seen[tank.get_name()] = loc(tank)
            log(f'T-26 {tank.get_name()} first seen at {loc(tank)}')


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world_now = game_world()
        if world_now is not None and phase not in ('wait_load', 'done'):
            keep_soldiers_away(world_now)
            note_t26_spawns(world_now)
            watch_convoy(world_now)
        if phase == 'wait_load' and S['t'] > 15.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {editor_world.get_path_name()} path={PATH} faction={FACTION}')
            manager = actors_of(editor_world, 'BP_MissionManager_C')[0]
            chain, mission = [], manager.get_editor_property('missions')[0]
            while mission is not None:
                chain.append(mission.get_class().get_name())
                mission = mission.get_editor_property('next_mission')
            check('chain_saved', chain == ['Mission_Intro_MoveTo_C', 'Mission_Intro_SovietPatrols_C',
                                           'Mission_Intro_HangarT26s_C', 'Mission_Intro_BaseBuilding_C'], str(chain))
            if FACTION in ('strike', 'breakthrough'):
                backup = manager.get_editor_property('player_faction_backup_data')
                backup.set_editor_property('set_faction_manually', True)
                backup.set_editor_property('player_faction', unreal.RTSFaction.GER_STRIKE_DIVISION if FACTION ==
                                           'strike' else unreal.RTSFaction.GER_BREAKTHROUGH_DOCTRINE)
                manager.set_editor_property('player_faction_backup_data', backup)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and elapsed > 15.0:
                S['phase'], S['mark'] = 'takeover', S['t']
            elif elapsed > 180.0:
                log('pie never started')
                finish()
        elif phase == 'takeover':
            # Mission_Intro_MoveTo opens the camera controls takeover (3 pages: camera controls, camera panning,
            # selecting units) before any announcer line. CONTINUE on each page, UNDERSTOOD on the last closes it.
            world = game_world()
            if 'start_pressed' not in S:
                # The start-game screen waits for the player; info takeovers open only after start is pressed.
                S['start_pressed'] = S['t']
                unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
                log('pressed start game')
                return
            panels = camera_takeovers(world)
            if panels and 'takeover_seen' not in S:
                S['takeover_seen'] = S['t']
                check('camera_takeover_shown', True, f'paused={unreal.GameplayStatics.is_game_paused(world)}')
                panels[0].call_method('HandleUnderstoodClicked')        # CONTINUE -> page 2
                S['takeover_presses'], S['takeover_press_t'], S['takeover_later_paused'] = 1, S['t'], []
            elif 'takeover_seen' in S and 'takeover_done' not in S and S['t'] - S['takeover_press_t'] > 1.5:
                if panels and S['takeover_presses'] < CAMERA_TAKEOVER_PAGES:
                    # Every later page must show on the still open, still paused takeover.
                    S['takeover_later_paused'].append(unreal.GameplayStatics.is_game_paused(world))
                    panels[0].call_method('HandleUnderstoodClicked')    # CONTINUE, then UNDERSTOOD on the last
                    S['takeover_presses'] += 1
                    S['takeover_press_t'] = S['t']
                else:
                    S['takeover_done'] = S['t']
                    later = S['takeover_later_paused']
                    check('takeover_later_pages_paused', len(later) == CAMERA_TAKEOVER_PAGES - 1 and all(later),
                          f'paused on pages 2..{CAMERA_TAKEOVER_PAGES}: {later}')
            elif 'takeover_done' in S and S['t'] - S['takeover_done'] > 0.5:
                check('takeover_closed_unpaused', not panels and not unreal.GameplayStatics.is_game_paused(world),
                      f'open={bool(panels)} paused={unreal.GameplayStatics.is_game_paused(world)} after '
                      f'{S["takeover_presses"]} presses')
                S['camera_closed'] = S['t']
                S['phase'], S['mark'] = 'pie_up', S['t']
            elif 'takeover_seen' not in S and elapsed > 30.0:
                check('camera_takeover_shown', False, 'no WBP_CameraControlsTakeover after 30 s')
                S['phase'], S['mark'] = 'pie_up', S['t']
        elif phase == 'pie_up':
            world = game_world()
            if True:
                unreal.GameplayStatics.set_game_paused(world, False)
                log(f'pie up; soldiers {len(player_soldiers(world))} mosin squads '
                    f'{len(actors_of(world, "Rus_Squad_Mosin_C"))} triggers '
                    f'{len(actors_of(world, "BP_TriggerArea_Rectangle_C"))} missions '
                    f'{[m.get_class().get_name() for m in missions(world)]}')
                check('moveto_trigger_created', len(actors_of(world, 'BP_TriggerArea_Rectangle_C')) >= 1)
                check('moveto_hint_spawned', len(actors_of(world, 'BP_Hint_MoveToWayPoint_C')) >= 1)
                teleport_soldiers(world, MOVE_TO)
                S['phase'], S['mark'] = 'cover_takeover', S['t']
        elif phase == 'cover_takeover':
            # Mission_Intro_SovietPatrols opens the cover takeover before its music and voice line.
            world = game_world()
            panels = takeovers(world, 'WBP_CoverTakeover_C')
            if panels and 'cover_seen' not in S:
                S['cover_seen'] = S['t']
                check('cover_takeover_shown', unreal.GameplayStatics.is_game_paused(world),
                      f'paused={unreal.GameplayStatics.is_game_paused(world)}')
                panels[0].call_method('HandleUnderstoodClicked')
            elif 'cover_seen' in S and S['t'] - S['cover_seen'] > 2.0:
                check('cover_takeover_closed_unpaused', not panels and not unreal.GameplayStatics.is_game_paused(world),
                      f'open={bool(panels)} paused={unreal.GameplayStatics.is_game_paused(world)}')
                S['phase'], S['mark'] = 'wait_patrols', S['t']
            elif 'cover_seen' not in S and elapsed > 30.0:
                check('cover_takeover_shown', False, 'no WBP_CoverTakeover after 30 s')
                S['phase'], S['mark'] = 'wait_patrols', S['t']
        elif phase == 'wait_patrols':
            world = game_world()
            squads = patrol_squads(world)
            if len(squads) >= 3 or elapsed > 40.0:
                check('moveto_completed', mission_complete(world, 'Mission_Intro_MoveTo_C') is True)
                check('patrol_squads_spawned', len(squads) == 3, f'{len(squads)} Rus_Squad_Mosin_C')
                check('moveto_hint_removed', len(actors_of(world, 'BP_Hint_MoveToWayPoint_C')) == 0)
                S['samples']['mosin_squads'] = [[loc(q) for q in squads]]
                S['pin'] = True
                S['phase'], S['mark'] = 'watch_march', S['t']
        elif phase == 'watch_march' and elapsed > 20.0:
            world = game_world()
            S['samples']['mosin_squads'].append([loc(q) for q in patrol_squads(world)])
            before, after = S['samples']['mosin_squads'][0], S['samples']['mosin_squads'][-1]
            moved = sum(1 for a, b in zip(sorted(before), sorted(after))
                        if abs(a[0] - b[0]) + abs(a[1] - b[1]) > 300)
            check('squads_marching', moved >= 1, f'{moved} squads moved >3 m in 20 s')
            if PATH == 'kill':
                for squad in patrol_squads(world):
                    squad.destroy_actor()
                log('destroyed the patrol Mosin squads (not the convoy escort)')
            else:
                S['pin'] = False
                teleport_soldiers(world, PAST_SANDBAGS)
            S['phase'], S['mark'] = 'wait_t26', S['t']
        elif phase == 'wait_t26':
            world = game_world()
            t26s = actors_of(world, 'Rus_Vehicle_T26_C')
            first_seen = S.setdefault('t26_first', {})
            for tank in t26s:
                first_seen.setdefault(tank.get_name(), loc(tank))
            if len(t26s) >= 3 or elapsed > 40.0:
                check('patrols_completed', mission_complete(world, 'Mission_Intro_SovietPatrols_C') is True)
                check('t26_spawned', len(t26s) == 3, f'{len(t26s)} first seen at {list(first_seen.values())}')
                near = sum(1 for p in first_seen.values()
                           if min((unreal.Vector(*p) - h).length() for h in HANGARS) < 900)
                check('t26_under_hangars', near == len(first_seen) and first_seen,
                      f'{near} of {len(first_seen)} first seen within 9 m of a hangar mouth')
                # Out of T-26 range, so a T-26 that stands still is not one stopped to fight.
                S['pin'] = True
                S['phase'], S['mark'] = 'watch_t26', S['t']
        elif phase == 'watch_t26' and elapsed > 25.0:
            world = game_world()
            strike = actors_of(world, 'Ger_Vehicle_PanzerII_F_SpacedArmor_C')
            luchs = actors_of(world, 'Ger_Vehicle_PanzerII_Luchs_SpacedArmor_C')
            if FACTION == 'none':
                ok = sorted([len(strike), len(luchs)]) == [0, 4]
            else:
                want, other = (strike, luchs) if FACTION == 'strike' else (luchs, strike)
                ok = len(want) == 4 and len(other) == 0
            check('panzers_spawned', ok, f'strike PzIIF SA {len(strike)} luchs SA {len(luchs)}')
            soldiers = player_soldiers(world)
            moved = []
            for tank in actors_of(world, 'Rus_Vehicle_T26_C'):
                start = unreal.Vector(*S['t26_first'].get(tank.get_name(), loc(tank)))
                nearest = min([(s.get_actor_location() - tank.get_actor_location()).length() for s in soldiers]
                              or [0])
                moved.append(((tank.get_actor_location() - start).length(), loc(tank), round(nearest)))
            check('t26_left_hangars', sum(1 for m in moved if m[0] > 1000) >= 2,
                  f'(distance from first seen, now, nearest player soldier) {moved}')
            target = unreal.Vector(-21200, 3500, 150)
            closer = [round((unreal.Vector(*S['t26_first'][name]) - target).length()
                            - (tank.get_actor_location() - target).length())
                      for name, tank in ((t.get_name(), t) for t in actors_of(world, 'Rus_Vehicle_T26_C'))
                      if name in S['t26_first']]
            check('t26_heading_to_target', sum(1 for c in closer if c > 1500) >= 2,
                  f'cm each T-26 got closer to the Vulture start: {closer}')
            for tank in actors_of(world, 'Rus_Vehicle_T26_C'):
                tank.destroy_actor()
            log('destroyed all T-26s')
            S['phase'], S['mark'] = 'wait_end', S['t']
        elif phase == 'wait_end' and elapsed > 6.0:
            world = game_world()
            check('t26_mission_completed', mission_complete(world, 'Mission_Intro_HangarT26s_C') is True)
            S['phase'], S['mark'] = 'wait_base_start', S['t']
        elif phase == 'wait_base_start':
            # Mission_Intro_BaseBuilding starts when the T-26s die: it calls the HQ in by Mammut glider at once.
            world = game_world()
            gliders = actors_of(world, 'BP_GliderTankDelivery_Mammut_C')
            if gliders or elapsed > 20.0:
                check('base_mission_started_after_t26', bool(gliders),
                      f'{len(gliders)} Mammut glider(s) {elapsed:.1f} s after the T-26 mission completed')
                S['phase'], S['mark'] = 'watch_convoy', S['t']
        elif phase == 'watch_convoy':
            # The convoy outlives the missions; wait until it is over the hill (or gone at its exit).
            world = game_world()
            last = S.get('convoy_truck_last')
            gone = 'convoy_crossed' in S and not actors_of(world, CONVOY_TRUCK)
            if gone or S['t'] - S.get('camera_closed', S['t']) > 330.0:
                convoy_checks(world)
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
