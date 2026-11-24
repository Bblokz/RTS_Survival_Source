"""Throwaway-editor PIE test of the first Ger_IntroMission missions (never run it in the shared editor).

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env INTRO_TEST_PATH  = kill | trigger   how Mission_Intro_SovietPatrols ends (kill the 3 squads / cross the
                                            sandbag line)
    env INTRO_TEST_FACTION = none | strike | breakthrough   backup faction (none: the game instance decides)
    env INTRO_TEST_DIFFICULTY = default | normal | hard | brutal | ironman   PIE startup difficulty (default: the
                                            RTS PIE Startup settings in DefaultGame.ini, Hard on 2026-11-12)
    env INTRO_TEST_OUT   = result json path
    The Development UnrealEditor.exe works too when its DLL is newer than the DebugGame one.

Chain under test (loaded from the saved map, so it also proves the chain persisted):
  Missions[0]: Mission_Intro_MoveTo -> Mission_Intro_SovietPatrols -> Mission_Intro_HangarT26s -> Mission_Intro_RidgeArmor
  Missions[1]: Mission_Intro_ClearSovietRig (trigger: Mission Class Completed = Mission_Intro_RidgeArmor)
  Missions[2]: Mission_Intro_BaseBuilding   (trigger: Mission Class Completed = Mission_Intro_ClearSovietRig)
Steps: player soldiers are teleported onto the move-to point; the patrol mission must spawn 2 more Mosin squads that
march; then either the 3 squads are destroyed or the soldiers are teleported over the sandbag line; the T-26 mission
must spawn 3 T-26s that leave the hangars towards the Vulture start plus 4 panzers of the faction's variant; the T-26s
are destroyed and the T-26 mission must complete.
Ridge mission: the Soviet wave (2 T-34/76 + 3 BT-7, +1 T-34/76 on Hard+, +1 T-34/85 on Brutal+) must spawn on its slots
west of the ridge gate without overlaps and drive towards the player units' centre; the faction's 7 reinforcements must
spawn in their 800 cm square at the road crossing and drive towards the same centre; the heavy-armor line must play; no
glider may come before the Soviet tanks die. The Soviet tanks are destroyed: the ridge mission completes. Since 2026-11-16
ClearSovietRig is "Secure the line" (USecureBaseMission): its camera tour must play, no glider may come while its 7 base
buildings stand; the test kills them through the damage path, ClearSovietRig completes and BaseBuilding starts (its
Mammut glider appears). secure_line_headless_test.py covers that mission in detail.
Fuel convoy (started by Mission_Intro_MoveTo when the camera takeover closes): one Zil fuel truck + one Mosin squad
marching behind it must spawn at the left map edge on the two-track path, cross the main road and drive on up the hill.
The run waits for the convoy after the missions end and logs when it crossed the road (seconds after the takeover).
"""
import json
import os

import unreal

PATH = os.environ.get('INTRO_TEST_PATH', 'kill')
FACTION = os.environ.get('INTRO_TEST_FACTION', 'none')
DIFFICULTY = os.environ.get('INTRO_TEST_DIFFICULTY', 'default')
OUT =os.environ.get('INTRO_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/IntroMissionTest/result.json')
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
# Mission_Intro_ClearSovietRig ("Secure the line", 2026-11-16) brings its own Zil fuel truck to the base's rear wall
# (x > 3000); the convoy never gets east of its hilltop exit (-640, 790).
BASE_REAR_MIN_X = 2500.0


def convoy_trucks(world):
    return [t for t in actors_of(world, CONVOY_TRUCK) if t.get_actor_location().x < BASE_REAR_MIN_X]
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
    trucks = convoy_trucks(world)
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
    gone = 'convoy_crossed' in S and not convoy_trucks(world)
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
    json.dump({'path': PATH, 'faction': FACTION, 'difficulty': DIFFICULTY, 'checks': S['checks'], 'samples': S['samples'], 'log': S['log']},
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


VULTURE_LANDED_MAX_Z = 600.0
VULTURE_LANDED_HOLD = 3.0


def vultures_landed(world):
    """True once player soldiers exist and all of them have stood on the ground (low and slow) for a few seconds."""
    soldiers = player_soldiers(world)
    grounded = bool(soldiers) and all(
        s.get_actor_location().z < VULTURE_LANDED_MAX_Z and s.get_velocity().z > -50.0 for s in soldiers)
    if not grounded:
        S.pop('vultures_grounded_since', None)
        return False
    since = S.setdefault('vultures_grounded_since', S['t'])
    if S['t'] - since < VULTURE_LANDED_HOLD:
        return False
    log(f'{len(soldiers)} Vultures landed by {round(S["t"], 1)} s')
    return True


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
                 'Mission_Intro_RidgeArmor_C', 'Mission_Intro_ClearSovietRig_C', 'Mission_Intro_BaseBuilding_C'):
        found += [o for o in unreal.ObjectIterator(unreal.MissionBase) if o.get_class().get_name() == name
                  and o.get_world() == world]
    return found


def note_completions(world):
    """Remembers when each mission is first seen complete. In PIE the manager empties its Missions array once they are
    activated, so a completed mission that nothing references any more can be garbage collected seconds later."""
    done = S.setdefault('completed', {})
    for mission in missions(world):
        name = mission.get_class().get_name()
        if name not in done and mission.call_method('GetIsMissionComplete'):
            done[name] = S['t']
            log(f'mission complete: {name}')


def mission_complete(world, class_name):
    if class_name in S.get('completed', {}):
        return True
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


PANZER_CLASSES = ('Ger_Vehicle_PanzerII_F_SpacedArmor_C', 'Ger_Vehicle_PanzerII_Luchs_SpacedArmor_C')
PANZER_BEHIND_VULTURES = 1500.0


def note_panzer_spawns(world):
    """Mission_Intro_HangarT26s sends each panzer, as it spawns, to a line 1500 cm behind the Vulture soldiers (away
    from the hangars). Remember where that point was when the panzers appeared, to check they drove there."""
    if 'panzer_rally' in S:
        return
    panzers = [p for c in PANZER_CLASSES for p in actors_of(world, c)]
    vultures = actors_of(world, 'Ger_Inf_Hazmat_Vulture_C')
    if not panzers or not vultures:
        return
    centre = unreal.Vector(sum(v.get_actor_location().x for v in vultures) / len(vultures),
                           sum(v.get_actor_location().y for v in vultures) / len(vultures), 0.0)
    hangar = unreal.Vector(sum(h.x for h in HANGARS) / len(HANGARS), sum(h.y for h in HANGARS) / len(HANGARS), 0.0)
    away = centre - hangar
    length = (away.x ** 2 + away.y ** 2) ** 0.5 or 1.0
    rally = unreal.Vector(centre.x + away.x / length * PANZER_BEHIND_VULTURES,
                          centre.y + away.y / length * PANZER_BEHIND_VULTURES, 0.0)
    S['panzer_rally'] = [round(rally.x), round(rally.y)]
    S['panzer_spawn'] = {p.get_name(): loc(p) for p in panzers}
    log(f'panzers seen; Vulture centre {round(centre.x)}, {round(centre.y)} -> expected rally {S["panzer_rally"]}')


PATROL_VOICE_ORDER = ['IntroMission_SovietInfantrySpotted', 'IntroMission_MosinGermansSpotted']

# Mission_Intro_HangarT26s: 5 s after the first panzer arrives the spaced armour takeover opens, and on its close the
# vehicle info takeover of the T-26 (library row T26).
PANZER_TAKEOVERS = ['WBP_SpacedArmourTakeover_C', 'WBP_VehicleInfoTakeover_C']
PANZER_TAKEOVER_DELAY = 5.0


def handle_panzer_takeovers(world):
    """Notes when the first panzer appears and each panzer takeover opens, closes it like a player after 1.5 s, and
    holds the phase clock while one is open (the game is paused, so nothing the phases wait for moves)."""
    if 'panzer_first_seen' not in S and any(actors_of(world, c) for c in PANZER_CLASSES):
        S['panzer_first_seen'] = S['t']
    opened = S.setdefault('panzer_takeovers', {})
    for class_name in PANZER_TAKEOVERS:
        panels = takeovers(world, class_name)
        if not panels:
            continue
        S['mark'] += S.get('last_delta', 0.0)
        if class_name not in opened:
            opened[class_name] = {'t': S['t'], 'paused': unreal.GameplayStatics.is_game_paused(world)}
            log(f'{class_name} opened at {round(S["t"], 1)}')
        elif S['t'] - opened[class_name]['t'] > 1.5 and 'closed' not in opened[class_name]:
            opened[class_name]['closed'] = S['t']
            panels[0].call_method('HandleUnderstoodClicked')


def panzer_takeover_checks():
    opened = S.get('panzer_takeovers', {})
    spaced, vehicle = (opened.get(c) for c in PANZER_TAKEOVERS)
    first = S.get('panzer_first_seen')
    # The 5 s delay runs in game time from the first panzer's OnSpawned; the test clock is wall time and the test
    # can see a freshly spawned panzer a couple of seconds late, so the window is wide.
    check('spaced_armour_takeover_after_panzers', spaced is not None and first is not None and spaced['paused']
          and 1.0 <= spaced['t'] - first <= PANZER_TAKEOVER_DELAY + 15.0,
          f'first panzer seen at {first}, spaced armour takeover {spaced}')
    check('t26_info_takeover_after_spaced_armour', vehicle is not None and spaced is not None
          and 'closed' in spaced and vehicle['t'] >= spaced['closed'] and vehicle['paused'],
          f'spaced armour {spaced}, vehicle info {vehicle}')


def note_voice_lines(world):
    """Records each mission voice line the first time an audio component plays it (seconds of test time)."""
    heard = S.setdefault('voice', {})
    for comp in unreal.ObjectIterator(unreal.AudioComponent):
        if comp.get_world() != world or not comp.is_playing():
            continue
        sound = comp.get_editor_property('sound')
        name = sound.get_name() if sound else ''
        if name.startswith('IntroMission_') and name not in heard:
            heard[name] = round(S['t'], 1)
            log(f'voice line playing: {name}')


# Mission_Intro_MoveTo queues the three Briefing lines after the welcome line (camera takeover On Closed).
BRIEFING_VOICE_ORDER = ['IntroMission_AdvisorWelcome', 'IntroMission_SovietStrongholds',
                        'IntroMission_ClearStronghold', 'IntroMission_TreadCarefully']


def briefing_voice_check():
    """The welcome line, then the three briefing lines, each starting after the one before has finished."""
    heard = S.get('voice', {})
    times = [heard.get(n) for n in BRIEFING_VOICE_ORDER]
    ok = all(t is not None for t in times) and all(b - a >= 5.0 for a, b in zip(times, times[1:]))
    check('briefing_voice_lines_in_order', ok, str(dict(zip(BRIEFING_VOICE_ORDER, times))))


def patrol_voice_check():
    """After the cover takeover closes: the advisor's infantry line, then the Mosin's (queued by the portraits)."""
    heard = S.get('voice', {})
    closed = S.get('cover_seen')
    times = [heard.get(n) for n in PATROL_VOICE_ORDER]
    ok = closed is not None and all(t is not None and t >= closed for t in times) and times == sorted(times)
    check('patrol_voice_lines_in_order', ok, f'cover closed at {closed}; {dict(zip(PATROL_VOICE_ORDER, times))}')


def note_t26_spawns(world):
    first_seen = S.setdefault('t26_first', {})
    for tank in actors_of(world, 'Rus_Vehicle_T26_C'):
        if tank.get_name() not in first_seen:
            first_seen[tank.get_name()] = loc(tank)
            log(f'T-26 {tank.get_name()} first seen at {loc(tank)}')


SOVIET_CLASSES = ('Rus_Vehicle_T3476_C', 'Rus_Vehicle_BT-7_C', 'Rus_Vehicle_T3485_C')
# Mission_Intro_RidgeArmor class defaults: slots in front of (west of) BP_Dest_PostApo_ConcreteFenceGate (-646, -266).
SOVIET_SLOTS = [(-1650, -366), (-1650, 434), (-2400, -766), (-2400, 34), (-2400, 834), (-1650, 1234), (-3150, 1234)]
RIDGE_GATE_X = -646.0
# 2026-11-12 (user): the reinforcements come from the west map edge, so they have a long drive to the player.
REINFORCEMENT_CENTRE = (-30984.0, 3366.0)
# Eight flanking T-26s from the fuel convoy's start (two-track map edge, Y -24760), 2 wide x 4 deep, 800 cm apart.
FLANK_CLASS = 'Rus_Vehicle_T26_C'
FLANK_SLOTS = [(x, y) for y in (-24300, -23500, -22700, -21900) for x in (-6180, -5380)]
REINFORCEMENT_OFFSETS = [(800, -800), (800, 0), (800, 800), (0, -800), (0, 800), (-800, -800), (-800, 800)]
REINFORCEMENTS = {
    'strike': {'Ger_Vehicle_PanzerIII_J_C': 3, 'Ger_Vehicle_PanzerIII_AA_Flak41_C': 2, 'Ger_Vehicle_Wespe_C': 1,
               'Ger_Vehicle_PantherD_C': 1},
    'breakthrough': {'Ger_Vehicle_PanzerIV_F1_C': 3, 'Ger_Vehicle_Sdkfz7Flak_Flak36_C': 2,
                     'Ger_Vehicle_Brummbar_C': 1, 'Ger_Vehicle_TigerH1_C': 1},
}
REINFORCEMENT_CLASSES = tuple(c for kit in REINFORCEMENTS.values() for c in kit)
PLAYER_CENTRE_CLASSES = ('Ger_Inf_Hazmat_Vulture_C',) + PANZER_CLASSES
DIFFICULTIES = {'normal': 'NORMAL', 'hard': 'HARD', 'brutal': 'BRUTAL', 'ironman': 'IRONMAN'}


# Mission_Intro_RidgeArmor's two minimap division markers (UMissionBase::TrackUnitGroupOnMiniMap group IDs).
MAIN_GROUP = 'RidgeMainForce'
FLANK_GROUP = 'FlankingT26s'
# Markers are refreshed every 0.25 s; a tank drives up to ~3 m in that time, more under load.
MARKER_TOLERANCE = 800.0


def group_centroid(world, classes, names):
    tanks = [t for c in classes for t in actors_of(world, c) if t.get_name() in names]
    if not tanks:
        return None
    return (sum(xy(t)[0] for t in tanks) / len(tanks), sum(xy(t)[1] for t in tanks) / len(tanks))


def marker_state(world):
    """(tracked group count, {group: marker xy or None}) from the ridge mission."""
    ridge = mission_object(world, 'Mission_Intro_RidgeArmor_C')
    if ridge is None:
        return None, {}
    count = ridge.call_method('GetTrackedUnitGroupCount')
    spots = {}
    for group in (MAIN_GROUP, FLANK_GROUP):
        spot = ridge.call_method('GetUnitGroupMarkerLocation', (unreal.Name(group),))
        spots[group] = None if spot is None or (abs(spot.x) < 1 and abs(spot.y) < 1) else (spot.x, spot.y)
    return count, spots


def check_markers(world, tag, want_main, want_flank):
    """Both markers while their groups live, each at its group's centroid; gone once the group is dead."""
    count, spots = marker_state(world)
    main_centre = group_centroid(world, SOVIET_CLASSES, S.get('soviet_first', {}))
    flank_centre = group_centroid(world, (FLANK_CLASS,), S.get('flank_first', {}))
    rows, ok = [], count == int(want_main) + int(want_flank)
    for group, want, centre in ((MAIN_GROUP, want_main, main_centre), (FLANK_GROUP, want_flank, flank_centre)):
        spot = spots.get(group)
        if want:
            off = dist_xy(spot, centre) if spot and centre else None
            ok = ok and off is not None and off < MARKER_TOLERANCE
            rows.append(f'{group} marker {[round(v) for v in spot] if spot else None}, group centroid '
                        f'{[round(v) for v in centre] if centre else None}, {round(off) if off is not None else None} cm')
        else:
            ok = ok and spot is None
            rows.append(f'{group} marker {[round(v) for v in spot] if spot else "gone"} (group dead)')
    check(f'ridge_markers_{tag}', ok, f'tracked groups {count}; ' + '; '.join(rows))


def mission_object(world, class_name):
    return next((m for m in missions(world) if m.get_class().get_name() == class_name), None)


def xy(actor):
    location = actor.get_actor_location()
    return location.x, location.y


def dist_xy(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def min_pair_distance(points):
    pairs = [dist_xy(a, b) for i, a in enumerate(points) for b in points[i + 1:]]
    return round(min(pairs)) if pairs else None


def note_ridge_spawns(world):
    """First-seen positions of the ridge mission's Soviet tanks and German reinforcements."""
    # Tanks of these classes that were already there (the map has a Pz III J / Flak 41 / Wespe group placed at its
    # west edge) are not the mission's.
    before = S.get('ridge_preexisting', set())
    # The hangar T-26s are all dead once the ridge mission starts, so every T-26 seen from then on is a flanker.
    for key, classes, centre_key in (('soviet_first', SOVIET_CLASSES, 'ridge_centre_expected'),
                                     ('rein_first', REINFORCEMENT_CLASSES, 'ridge_centre_expected'),
                                     ('flank_first', (FLANK_CLASS,), 'ridge_panzer_expected')):
        seen = S.setdefault(key, {})
        for class_name in classes:
            for tank in actors_of(world, class_name):
                if tank.get_name() not in seen and tank.get_name() not in before:
                    seen[tank.get_name()] = (class_name, xy(tank), S['t'])
                    log(f'{key} {class_name} {tank.get_name()} at {loc(tank)}')
                centre = S.get(centre_key)
                if tank.get_name() in seen and centre:
                    # Best progress towards the centre so far: attack-moving tanks stop to fight (and may turn
                    # towards the enemy), and a tank killed in the fight keeps what it drove.
                    progress = dist_xy(seen[tank.get_name()][1], centre) - dist_xy(xy(tank), centre)
                    best = S.setdefault('ridge_best', {})
                    best[tank.get_name()] = max(best.get(tank.get_name(), progress), progress)
    note_enemies_near_reinforcements(world)
    alive = [t for c in SOVIET_CLASSES + (FLANK_CLASS,) for t in actors_of(world, c) if t.get_name() not in before]
    if S.get('soviet_first') and not alive and 'soviets_gone' not in S:
        # The reinforcements can win the fight on their own before the test kills the rest.
        S['soviets_gone'] = S['t']
        log('every ridge Soviet tank is gone')


ENGAGE_RANGE = 6000.0


def note_enemies_near_reinforcements(world):
    """Once a second: the closest enemy (owning player 2) to each reinforcement. An attack-move stops to fight, so a
    reinforcement that met enemies near its spawn (placed Soviet infantry, the incoming tanks) may not drive at all."""
    if S['t'] - S.get('enemy_scan_t', -99.0) < 1.0:
        return
    S['enemy_scan_t'] = S['t']
    enemies = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        comp = actor.get_component_by_class(unreal.RTSComponent)
        if comp is not None and comp.get_editor_property('owning_player') == 2:
            enemies.append((xy(actor), actor.get_name()))
    if not enemies:
        return
    nearest = S.setdefault('rein_enemy_nearest', {})
    detail = S.setdefault('rein_enemy_detail', {})
    for class_name in REINFORCEMENT_CLASSES:
        for tank in actors_of(world, class_name):
            if tank.get_name() not in S.get('rein_first', {}):
                continue
            here = xy(tank)
            closest_spot, closest_name = min(enemies, key=lambda e: dist_xy(here, e[0]))
            closest = dist_xy(here, closest_spot)
            if closest < nearest.get(tank.get_name(), 1e12):
                nearest[tank.get_name()] = closest
                detail[tank.get_name()] = (f'{closest_name} at {[round(v) for v in closest_spot]}, '
                                           f't {round(S["t"] - S.get("ridge_start", 0.0), 1)} s after ridge start')


def reinforcement_arrival_check(world):
    """End of the run: every surviving reinforcement must stand near PlayersCentre plus its square offset."""
    centre = S.get('ridge_centre')
    rows, arrived = [], 0
    # Attack-move stops to fight: a reinforcement still short of its spot with an enemy within ENGAGE_RANGE (on the
    # 2026-11-12 runs, the placed BP_Solo_Rus_* soldiers at the sandbag line) counts as in combat, not as lost.
    enemies = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        comp = actor.get_component_by_class(unreal.RTSComponent)
        if comp is not None and comp.get_editor_property('owning_player') == 2:
            enemies.append((xy(actor), actor.get_name()))
    for class_name in REINFORCEMENT_CLASSES:
        for tank in actors_of(world, class_name):
            first = S.get('rein_first', {}).get(tank.get_name())
            if first is None or centre is None:
                continue
            offset = (first[1][0] - REINFORCEMENT_CENTRE[0], first[1][1] - REINFORCEMENT_CENTRE[1])
            goal = (centre[0] + offset[0], centre[1] + offset[1])
            off = dist_xy(xy(tank), goal)
            near = min(enemies, key=lambda e: dist_xy(xy(tank), e[0])) if enemies else None
            in_combat = off >= 1500 and near is not None and dist_xy(xy(tank), near[0]) < ENGAGE_RANGE
            arrived += 1 if off < 1500 or in_combat else 0
            rows.append(f'{tank.get_name().replace("_C_", "#")} at {[round(v) for v in xy(tank)]}, '
                        f'{round(off)} cm from its spot' +
                        (f' (in combat: {near[1]} {round(dist_xy(xy(tank), near[0]))} cm away)' if in_combat else ''))
    check('ridge_reinforcements_arrived', rows and arrived == len(rows),
          f'{arrived}/{len(rows)} surviving reinforcements within 15 m of PlayersCentre + slot offset, or in combat, '
          f'{round(S["t"] - S.get("ridge_start", 0.0))} s after the ridge start: {rows}')
    for name, text in S.get('rein_enemy_detail', {}).items():
        log(f'closest enemy to {name}: {round(S["rein_enemy_nearest"][name])} cm, {text}')


def gliders_seen(world):
    if actors_of(world, 'BP_GliderTankDelivery_Mammut_C') and 'glider_seen' not in S:
        S['glider_seen'] = S['t']
        log('Mammut glider seen')


def set_pie_difficulty():
    if DIFFICULTY not in DIFFICULTIES:
        return
    settings = unreal.get_default_object(unreal.RTSGameInstancePIEDeveloperSettings)
    # ERTSGameDifficulty and FRTSGameDifficulty share the Python name RTSGameDifficulty; take the enum from the value.
    enum_type = type(settings.get_editor_property('difficulty_level'))
    settings.set_editor_property('difficulty_level', getattr(enum_type, DIFFICULTIES[DIFFICULTY]))
    log(f'PIE startup difficulty set to {DIFFICULTY}')


def expected_soviet_wave(world):
    # PIE takes the difficulty from the RTS PIE Startup settings (URTSGameInstance::ApplyPIEStartupOverrides).
    settings = unreal.get_default_object(unreal.RTSGameInstancePIEDeveloperSettings)
    level = str(settings.get_editor_property('difficulty_level'))
    try:
        manager = actors_of(world, 'BP_MissionManager_C')[0]
        level = str(manager.call_method('GetCurrentGameDifficulty').get_editor_property('difficulty_level'))
    except Exception:  # noqa: BLE001 - the struct shares its Python name with the enum; the settings value stands
        pass
    hard = any(n in level.upper() for n in ('HARD', 'BRUTAL', 'IRONMAN'))
    brutal = any(n in level.upper() for n in ('BRUTAL', 'IRONMAN'))
    wave = {'Rus_Vehicle_T3476_C': 2 + (1 if hard else 0), 'Rus_Vehicle_BT-7_C': 3,
            'Rus_Vehicle_T3485_C': 1 if brutal else 0}
    return level, wave


def ridge_spawn_checks(world):
    level, wave = expected_soviet_wave(world)
    first = S.get('soviet_first', {})
    counts = {c: sum(1 for v in first.values() if v[0] == c) for c in SOVIET_CLASSES}
    check('ridge_soviet_wave', counts == wave and len(first) == sum(wave.values()),
          f'difficulty {level}: spawned {counts}, expected {wave} ({sum(wave.values())} tanks)')
    ridge = mission_object(world, 'Mission_Intro_RidgeArmor_C')
    tracked = len(ridge.get_editor_property('SpawnedSoviets')) if ridge else -1
    tracked_flank = len(ridge.get_editor_property('SpawnedFlankingT26s')) if ridge else -1
    check('ridge_tracks_every_soviet', tracked == sum(wave.values()) and tracked_flank == len(FLANK_SLOTS),
          f'main force group {tracked} (expected {sum(wave.values())}), flanking group {tracked_flank} (expected '
          f'{len(FLANK_SLOTS)}); the mission completes when both groups are dead')
    flank = [v[1] for v in S.get('flank_first', {}).values()]
    on_flank_slot = sum(1 for p in flank if min(dist_xy(p, s) for s in FLANK_SLOTS) < 400)
    check('flank_t26_spawned', len(flank) == len(FLANK_SLOTS) and on_flank_slot == len(flank),
          f'{len(flank)} T-26s, {on_flank_slot} within 4 m of a slot at the convoy start; first seen {flank}')
    check('flank_t26_no_overlap', len(flank) < 2 or min_pair_distance(flank) > 500,
          f'closest two T-26 spawns {min_pair_distance(flank)} cm apart')
    players = S.get('ridge_centre_expected')
    check('flank_t26_from_player_left', flank and players and all(p[1] < players[1] - 15000 for p in flank),
          f'T-26s spawn south (-Y, the left of a player facing the ridge at +X) of the player centre '
          f'{[round(v) for v in players] if players else None}: max spawn Y {round(max(p[1] for p in flank)) if flank else None}')
    panzer = ridge.get_editor_property('PanzerCentre') if ridge else None
    panzer_samples = [s for s in (S.get('ridge_panzer_expected'), S.get('ridge_panzer_pre')) if s]
    panzer_off = min((dist_xy((panzer.x, panzer.y), s) for s in panzer_samples), default=None) if panzer else None
    check('ridge_panzer_centre', panzer_off is not None and panzer_off < 600,
          f'mission PanzerCentre {[round(panzer.x), round(panzer.y)] if panzer else None}, test average of the '
          f'Panzer IIs {[[round(v) for v in s] for s in panzer_samples]}')
    points = [v[1] for v in first.values()]
    on_slot = sum(1 for p in points if min(dist_xy(p, s) for s in SOVIET_SLOTS) < 400)
    west = sum(1 for p in points if p[0] < RIDGE_GATE_X - 500)
    check('ridge_soviets_in_front_of_gate', points and on_slot == len(points) and west == len(points),
          f'{on_slot}/{len(points)} within 4 m of a slot, {west} west of the gate; first seen {points}')
    check('ridge_soviets_no_overlap', len(points) < 2 or min_pair_distance(points) > 500,
          f'closest two Soviet spawns {min_pair_distance(points)} cm apart')
    rein = S.get('rein_first', {})
    got = {c: sum(1 for v in rein.values() if v[0] == c) for c in REINFORCEMENT_CLASSES}
    got = {c: n for c, n in got.items() if n}
    if FACTION in REINFORCEMENTS:
        want = REINFORCEMENTS[FACTION]
    else:
        want = next((kit for kit in REINFORCEMENTS.values() if kit == got), REINFORCEMENTS['strike'])
    check('ridge_reinforcements_composition', got == want,
          f'faction {FACTION} (none: either kit, whole): spawned {got}, expected {want}')
    rpoints = [v[1] for v in rein.values()]
    slots = [(REINFORCEMENT_CENTRE[0] + a, REINFORCEMENT_CENTRE[1] + b) for a, b in REINFORCEMENT_OFFSETS]
    in_square = sum(1 for p in rpoints if min(dist_xy(p, s) for s in slots) < 400)
    check('ridge_reinforcements_in_square', rpoints and in_square == len(rpoints),
          f'{in_square}/{len(rpoints)} within 4 m of an 800 cm square slot; first seen {rpoints}')
    check('ridge_reinforcements_no_overlap', len(rpoints) < 2 or min_pair_distance(rpoints) > 500,
          f'closest two reinforcement spawns {min_pair_distance(rpoints)} cm apart')
    centre = ridge.get_editor_property('PlayersCentre') if ridge else None
    expected = S.get('ridge_centre_expected')
    # The pinned Vultures are teleported every 0.5 s, so take the closer of the samples just before the T-26 kill and
    # at the tick the ridge mission was seen starting.
    samples = [s for s in (expected, S.get('ridge_centre_pre')) if s]
    if centre and samples:
        expected = min(samples, key=lambda s: dist_xy((centre.x, centre.y), s))
    off = dist_xy((centre.x, centre.y), expected) if centre and expected else None
    check('ridge_players_centre', off is not None and off < 600,
          f'mission PlayersCentre {[round(centre.x), round(centre.y)] if centre else None}, test average of '
          f'Vultures + Panzer IIs at the start {[round(v) for v in expected] if expected else None}')
    S['ridge_centre'] = (centre.x, centre.y) if centre else expected


def ridge_move_checks(world):
    centre = S.get('ridge_centre')
    for key, need, check_name in (('soviet_first', 1500, 'ridge_soviets_attack_move'),
                                  ('rein_first', 1000, 'ridge_reinforcements_attack_move'),
                                  ('flank_first', 1500, 'flank_t26_attack_move_to_panzers')):
        rows, ok_count = [], 0
        for tank_name in S.get(key, {}):
            progress = round(S.get('ridge_best', {}).get(tank_name, 0.0))
            enemy = S.get('rein_enemy_nearest', {}).get(tank_name) if key == 'rein_first' else None
            engaged = enemy is not None and enemy < ENGAGE_RANGE
            ok_count += 1 if progress > need or engaged else 0
            rows.append(f'{tank_name.replace("_C_", "#")}: {progress}' +
                        (f' (enemy {round(enemy)} cm)' if enemy is not None and progress <= need else ''))
        check(check_name, rows and ok_count == len(rows),
              f'most cm each got closer to its target (T-26s: the Panzer II centre '
              f'{[round(v) for v in S.get("ridge_panzer_expected", ())]}; others: the player centre '
              f'{[round(v) for v in centre] if centre else None}) '
              f'(need {need}, or an enemy within {round(ENGAGE_RANGE)} cm: attack-move stops to fight): {rows}')


def tick(delta):
    S['t'] += delta
    S['last_delta'] = delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world_now = game_world()
        if world_now is not None and phase not in ('wait_load', 'done'):
            keep_soldiers_away(world_now)
            note_voice_lines(world_now)
            note_t26_spawns(world_now)
            handle_panzer_takeovers(world_now)
            watch_convoy(world_now)
            note_completions(world_now)
            if 'ridge_start' not in S and mission_complete(world_now, 'Mission_Intro_HangarT26s_C') is True:
                # Mission_Intro_RidgeArmor starts now (the panzers may kill the T-26s before the test does) and
                # takes the centre of the Vultures and Panzer IIs as its target.
                units = [u for c in PLAYER_CENTRE_CLASSES for u in actors_of(world_now, c)]
                if units:
                    S['ridge_centre_expected'] = (sum(xy(u)[0] for u in units) / len(units),
                                                  sum(xy(u)[1] for u in units) / len(units))
                panzers = [u for c in PANZER_CLASSES for u in actors_of(world_now, c)]
                if panzers:
                    S['ridge_panzer_expected'] = (sum(xy(u)[0] for u in panzers) / len(panzers),
                                                  sum(xy(u)[1] for u in panzers) / len(panzers))
                else:
                    S['ridge_panzer_expected'] = S.get('ridge_centre_expected')
                S['ridge_start'] = S['t']
                log('T-26 mission complete: ridge mission starts')
            if 'ridge_start' in S and 'ridge_killed' not in S:
                # From the T-26 kill on, so the first-seen spots are the spawn spots.
                note_ridge_spawns(world_now)
        if phase == 'wait_load' and S['t'] > 15.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {editor_world.get_path_name()} path={PATH} faction={FACTION}')
            manager = actors_of(editor_world, 'BP_MissionManager_C')[0]
            chains = []
            for top in manager.get_editor_property('missions'):
                chain, mission = [], top
                while mission is not None:
                    trigger = mission.get_editor_property('mission_trigger')
                    watched = (trigger.get_editor_property('mission_class_to_watch').get_name()
                               if isinstance(trigger, unreal.MissionClassCompletedMissionTrigger) else None)
                    chain.append(mission.get_class().get_name() + (f' after {watched}' if watched else ''))
                    mission = mission.get_editor_property('next_mission')
                chains.append(chain)
            check('chain_saved', chains == [
                ['Mission_Intro_MoveTo_C', 'Mission_Intro_SovietPatrols_C', 'Mission_Intro_HangarT26s_C',
                 'Mission_Intro_RidgeArmor_C'],
                ['Mission_Intro_ClearSovietRig_C after Mission_Intro_RidgeArmor_C'],
                ['Mission_Intro_BaseBuilding_C after Mission_Intro_ClearSovietRig_C']], str(chains))
            set_pie_difficulty()
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
            # Since 2026-11-15 the Vultures parachute in from aircraft (DropVultures global ability) instead of being
            # placed: wait until they have all landed before teleporting them, or 60 s at most.
            unreal.GameplayStatics.set_game_paused(world, False)
            if vultures_landed(world) or elapsed > 60.0:
                log(f'pie up; soldiers {len(player_soldiers(world))} mosin squads '
                    f'{len(actors_of(world, "Rus_Squad_Mosin_C"))} triggers '
                    f'{len(actors_of(world, "BP_TriggerArea_Rectangle_C"))} missions '
                    f'{[m.get_class().get_name() for m in missions(world)]}')
                check('moveto_trigger_created', len(actors_of(world, 'BP_TriggerArea_Rectangle_C')) >= 1)
                check('moveto_hint_spawned', len(actors_of(world, 'BP_Hint_MoveToWayPoint_C')) >= 1)
                S['ridge_preexisting'] = {a.get_name() for c in SOVIET_CLASSES + REINFORCEMENT_CLASSES
                                          for a in actors_of(world, c)}
                log(f"tanks of the ridge classes placed on the map: {sorted(S['ridge_preexisting'])}")
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
            note_panzer_spawns(world)
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
            rally = S.get('panzer_rally')
            to_rally = []
            for tank in strike + luchs:
                spawn = S.get('panzer_spawn', {}).get(tank.get_name(), loc(tank))
                now = tank.get_actor_location()
                start_dist = ((spawn[0] - rally[0]) ** 2 + (spawn[1] - rally[1]) ** 2) ** 0.5 if rally else None
                to_rally.append([round(((now.x - spawn[0]) ** 2 + (now.y - spawn[1]) ** 2) ** 0.5),
                                 round(((now.x - rally[0]) ** 2 + (now.y - rally[1]) ** 2) ** 0.5) if rally else None,
                                 round(start_dist) if start_dist else None])
            # The test keeps the Vultures far back, so the rally line can be ~75 m off and the four panzers queue on
            # the way: each must be on the line (4 slots 500 cm apart: within ~12 m) or have closed a quarter of the
            # distance after 25 s (2026-11-12 runs: 32-68% under load, 61-80% on a quiet machine).
            check('panzers_drove_behind_vultures', rally is not None and to_rally and all(
                dist is not None and (dist < 1200 or dist < 0.75 * start) for moved, dist, start in to_rally),
                  f'expected rally {rally}; (cm moved, cm from rally, cm spawn to rally) per panzer {to_rally}')
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
            units = [u for c in PLAYER_CENTRE_CLASSES for u in actors_of(world, c)]
            if units:
                S['ridge_centre_pre'] = (sum(xy(u)[0] for u in units) / len(units),
                                         sum(xy(u)[1] for u in units) / len(units))
            panzers = [u for c in PANZER_CLASSES for u in actors_of(world, c)]
            if panzers:
                S['ridge_panzer_pre'] = (sum(xy(u)[0] for u in panzers) / len(panzers),
                                         sum(xy(u)[1] for u in panzers) / len(panzers))
            for tank in actors_of(world, 'Rus_Vehicle_T26_C'):
                tank.destroy_actor()
            log('destroyed all T-26s')
            S['phase'], S['mark'] = 'wait_end', S['t']
        elif phase == 'wait_end' and elapsed > 6.0:
            patrol_voice_check()
            briefing_voice_check()
            panzer_takeover_checks()
            world = game_world()
            check('t26_mission_completed', mission_complete(world, 'Mission_Intro_HangarT26s_C') is True)
            S['phase'], S['mark'] = 'wait_ridge', S['t']
        elif phase == 'wait_ridge':
            # The ridge mission spawns its Soviet wave west of the ridge gate and the faction's reinforcements.
            world = game_world()
            note_ridge_spawns(world)
            gliders_seen(world)
            _, wave = expected_soviet_wave(world)
            if (len(S.get('soviet_first', {})) >= sum(wave.values()) and len(S.get('rein_first', {})) >= 7
                    and len(S.get('flank_first', {})) >= len(FLANK_SLOTS) and elapsed > 4.0) or elapsed > 120.0:
                ridge_spawn_checks(world)
                S['phase'], S['mark'] = 'watch_ridge', S['t']
        elif phase == 'watch_ridge':
            world = game_world()
            note_ridge_spawns(world)
            gliders_seen(world)
            if elapsed > 30.0 or 'soviets_gone' in S:
                ridge_move_checks(world)
                heard = S.get('voice', {}).get('IntroMission_HeavyArmorApproaching')
                # Heard times are rounded to 0.1 s.
                check('ridge_heavy_armor_line', heard is not None and heard >= S.get('ridge_start', 0.0) - 0.1,
                      f"heard at {heard}, ridge mission started at {round(S.get('ridge_start', 0.0), 1)}")
                gone = S.get('soviets_gone')
                check('ridge_no_glider_before_soviets_die',
                      'glider_seen' not in S or (gone is not None and S['glider_seen'] >= gone),
                      f"glider seen at {S.get('glider_seen')}, last Soviet tank gone at {gone}")
                if gone is None:
                    check('ridge_mission_running', mission_complete(world, 'Mission_Intro_RidgeArmor_C') is False and
                          mission_complete(world, 'Mission_Intro_ClearSovietRig_C') is False)
                    check_markers(world, 'both_groups_alive', True, True)
                    ridge = mission_object(world, 'Mission_Intro_RidgeArmor_C')
                    title = str(ridge.get_editor_property('mission_widget_state').get_editor_property('mission_title'))                         if ridge is not None else ''
                    _, wave = expected_soviet_wave(world)
                    total = sum(wave.values()) + len(FLANK_SLOTS)
                    check('ridge_panel_kill_counter', 'Soviet tanks destroyed' in title and f'/{total}<' in title,
                          f'mission title {title!r}, expected a counter out of {total}')
                killed = 0
                # Kill the ridge wave first and check the mission waits for the flanking T-26s too.
                for class_name in SOVIET_CLASSES:
                    for tank in actors_of(world, class_name):
                        tank.destroy_actor()
                        killed += 1
                S['ridge_wave_killed'] = S['t']
                log(f'destroyed {killed} ridge Soviet tanks; the flanking T-26s are still alive')
                S['phase'], S['mark'] = 'wait_flank_kill', S['t']
        elif phase == 'wait_flank_kill' and elapsed > 6.0:
            world = game_world()
            gliders_seen(world)
            flankers = [t for t in actors_of(world, FLANK_CLASS) if t.get_name() in S.get('flank_first', {})]
            if flankers:
                # The main force's marker went with its last tank; the T-26s' marker stays on them.
                check_markers(world, 'main_dead_flank_alive', False, True)
                check('ridge_waits_for_flanking_t26s', mission_complete(world, 'Mission_Intro_RidgeArmor_C') is False
                      and 'glider_seen' not in S,
                      f'{len(flankers)} T-26s alive 6 s after the ridge wave died; ridge mission complete='
                      f'{mission_complete(world, "Mission_Intro_RidgeArmor_C")}, glider seen at {S.get("glider_seen")}')
            else:
                log('the flanking T-26s were already dead; ridge_waits_for_flanking_t26s not checked')
            for tank in flankers:
                tank.destroy_actor()
            log(f'destroyed {len(flankers)} flanking T-26s')
            S['ridge_killed'] = S['t']
            S['phase'], S['mark'] = 'flank_marker_gone', S['t']
        elif phase == 'flank_marker_gone' and elapsed > 1.0:
            # Within a marker update or two the T-26s' marker goes too, which completes the mission. A completed
            # mission can be garbage collected soon after, so only look while the object is still there.
            world = game_world()
            if mission_object(world, 'Mission_Intro_RidgeArmor_C') is not None:
                check_markers(world, 'both_groups_dead', False, False)
            S['phase'], S['mark'] = 'wait_ridge_end', S['t']
        elif phase == 'wait_ridge_end' and elapsed > 6.0:
            world = game_world()
            check('ridge_mission_completed', mission_complete(world, 'Mission_Intro_RidgeArmor_C') is True)
            # Since 2026-11-16 ClearSovietRig is "Secure the line": a camera tour over the Soviet base, then the
            # player must destroy its 7 buildings before BaseBuilding (and its glider) may start.
            check('secure_line_running_after_ridge',
                  mission_complete(world, 'Mission_Intro_ClearSovietRig_C') is False and 'glider_seen' not in S,
                  f"clear complete={mission_complete(world, 'Mission_Intro_ClearSovietRig_C')}, "
                  f"glider {S.get('glider_seen')}")
            S['phase'], S['mark'] = 'secure_tour', S['t']
        elif phase == 'secure_tour':
            world = game_world()
            gliders_seen(world)
            secure = mission_object(world, 'Mission_Intro_ClearSovietRig_C')
            if secure is not None and secure.call_method('GetHasTourFinished'):
                check('secure_line_tour_played', True, f'{round(elapsed, 1)} s after the ridge ended')
                check('secure_line_targets', secure.call_method('GetTargetCount') == 7,
                      f"{secure.call_method('GetTargetCount')} targets")
                S['phase'], S['mark'] = 'secure_hold', S['t']
            elif elapsed > 120.0:
                check('secure_line_tour_played', False, 'no finished tour 120 s after the ridge ended')
                S['phase'], S['mark'] = 'secure_hold', S['t']
        elif phase == 'secure_hold' and elapsed > 12.0:
            world = game_world()
            gliders_seen(world)
            check('no_glider_while_base_stands', 'glider_seen' not in S
                  and mission_complete(world, 'Mission_Intro_ClearSovietRig_C') is False,
                  f"glider {S.get('glider_seen')}")
            secure = mission_object(world, 'Mission_Intro_ClearSovietRig_C')
            targets = (secure.call_method('GetLivingTargets') or []) if secure is not None else []
            for target in targets:
                unreal.GameplayStatics.apply_damage(target, 1.0e7, None, None, unreal.DamageType)
            log(f'damaged {len(targets)} Soviet base targets to death')
            S['ridge_killed'] = S['t']
            S['phase'], S['mark'] = 'secure_end', S['t']
        elif phase == 'secure_end':
            world = game_world()
            note_completions(world)
            if mission_complete(world, 'Mission_Intro_ClearSovietRig_C') is True or elapsed > 20.0:
                check('clear_rig_completed', mission_complete(world, 'Mission_Intro_ClearSovietRig_C') is True,
                      f'{round(elapsed, 1)} s after the targets died')
                S['phase'], S['mark'] = 'wait_base_start', S['t']
        elif phase == 'wait_base_start':
            # Mission_Intro_BaseBuilding starts once ClearSovietRig completes: it calls the HQ in by Mammut glider.
            world = game_world()
            gliders = actors_of(world, 'BP_GliderTankDelivery_Mammut_C')
            if gliders or elapsed > 20.0:
                gliders_seen(world)
                check('base_mission_started_after_ridge', bool(gliders),
                      f'{len(gliders)} Mammut glider(s); first seen {S.get("glider_seen")}, ridge tanks killed at '
                      f'{round(S.get("ridge_killed", 0.0), 1)}')
                S['phase'], S['mark'] = 'watch_convoy', S['t']
        elif phase == 'watch_convoy':
            # The convoy outlives the missions; wait until it is over the hill (or gone at its exit).
            world = game_world()
            # The base mission pauses for its nomadic conversion takeover once the HQ is out of the glider; close it
            # like a player so the convoy keeps driving.
            later = [w for w in unreal.ObjectIterator(unreal.UserWidget)
                     if w.get_world() == world and w.is_in_viewport() and w.get_class().get_name().startswith('WBP_')
                     and w.get_class().get_name().endswith('Takeover_C')]
            if later and S['t'] - S.get('base_takeover_press_t', -99.0) > 1.5:
                S['base_takeover_press_t'] = S['t']
                S['base_takeover_presses'] = S.get('base_takeover_presses', 0) + 1
                log(f'closing {later[0].get_class().get_name()} (press {S["base_takeover_presses"]})')
                later[0].call_method('HandleUnderstoodClicked')
            last = S.get('convoy_truck_last')
            gone = 'convoy_crossed' in S and not convoy_trucks(world)
            if gone or S['t'] - S.get('camera_closed', S['t']) > 330.0:
                convoy_checks(world)
                reinforcement_arrival_check(world)
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
