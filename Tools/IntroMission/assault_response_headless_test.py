"""Throwaway-editor PIE test of the Secure-the-Line assault response (ramp trigger -> Mosin line, Soviet air drops,
advisor line + 2 Ju 87s strafing the base for 150 s). 2026-11-16.

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env ASSAULT_TEST_OUT = result json path

Before PIE (editor world only, nothing saved but the two sound waves when they are missing): the chain is cut to
[ClearSovietRig] and setup_assault_response.apply_defaults is applied to that map instance, so the run does not need
the Blueprint saved. After the camera tour a player unit is teleported onto the upper ramp.
"""
import json
import os
import sys
import time

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission'
sys.path.insert(0, HERE)
import setup_assault_response as setup  # noqa: E402

OUT = os.environ.get('ASSAULT_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/AssaultResponseTest/result.json')
SHOT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/AssaultResponseTest'
CLEAR = 'Mission_Intro_ClearSovietRig_C'
TANK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1.Ger_Vehicle_PanzerIV_F1_C'
SRC = 'D:/Blender/BlenderProjects/BlenderTanks/VoiceLines/Announcer/AAAGerMissions/IntroMission/'
SCLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions'
S = {'wall0': time.time(), 'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'samples': []}


def wall():
    return time.time() - S['wall0']


def log(message):
    S['log'].append(f"{wall():7.1f} {message}")
    unreal.log_warning('ASSAULTTEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'log': S['log'],
               'samples': S['samples']}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def owner(actor):
    comp = actor.get_component_by_class(unreal.RTSComponent)
    return comp.call_method('GetOwningPlayer') if comp is not None else -1


def mission(world):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == CLEAR and obj.get_world() == world:
            return obj
    return None


def d2(a, b):
    return ((a.x - b[0]) ** 2 + (a.y - b[1]) ** 2) ** 0.5


def ensure_sounds():
    tasks = []
    for name in ('IntroMission_MosinBaseUnderAttack', 'IntroMission_StukaSupport'):
        if unreal.EditorAssetLibrary.does_asset_exist(setup.LINES + name):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', SRC + name + '.wav')
        task.set_editor_property('destination_path', setup.LINES.rstrip('/'))
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', False)
        task.set_editor_property('save', False)
        tasks.append(task)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        sclass = unreal.load_asset(SCLASS)
        for task in tasks:
            for path in task.get_editor_property('imported_object_paths'):
                wave = unreal.load_asset(path)
                wave.set_editor_property('sound_class_object', sclass)
                unreal.EditorAssetLibrary.save_loaded_asset(wave, False)
                log('imported ' + path)


def shot(name):
    unreal.SystemLibrary.execute_console_command(game_world(), f'HighResShot 1600x900 filename={SHOT_DIR}/{name}.png')


DEEP_BASE = (-500.0, 4200.0, -4300.0, 5300.0)   # x0, x1, y0, y1: past the west wall, out of the ramp tank's reach


def in_deep_base(loc):
    return DEEP_BASE[0] < loc.x < DEEP_BASE[1] and DEEP_BASE[2] < loc.y < DEEP_BASE[3]


def snapshot_soviet_health(world):
    rows = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        hp = a.get_component_by_class(unreal.HealthComponent)
        if hp is None or owner(a) != 2 or not in_deep_base(a.get_actor_location()):
            continue
        rows[a.get_name()] = (a, hp.call_method('GetCurrentHealth'))
    return rows


def deep_base_damage():
    total, dead = 0.0, 0
    for name, (actor, start) in S['health0'].items():
        try:
            hp = actor.get_component_by_class(unreal.HealthComponent) if actor is not None else None
            now = hp.call_method('GetCurrentHealth') if hp is not None else 0.0
        except Exception:  # noqa: BLE001 - destroyed actor
            now = 0.0
        if now <= 0.0:
            dead += 1
        total += max(0.0, start - now)
    return round(total), dead


def sample(world, m):
    """Aircraft, the slung T-26 and dropped units near the landing points."""
    planes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AircraftMaster)
    rec = {'t': round(wall() - S['trigger_wall'], 1), 'player_planes': [], 'enemy_planes': [], 't26': [],
           'near_para': [0, 0]}
    for p in planes:
        loc = p.get_actor_location()
        entry = [p.get_class().get_name(), round(loc.x), round(loc.y), round(loc.z)]
        (rec['player_planes'] if owner(p) == 1 else rec['enemy_planes']).append(entry)
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        name = a.get_class().get_name()
        loc = a.get_actor_location()
        if name.startswith('Rus_Vehicle_T26'):
            parent = a.get_attach_parent_actor()
            rec['t26'].append([round(loc.x), round(loc.y), round(loc.z), parent.get_class().get_name() if parent else '',
                               round(parent.get_actor_location().z - loc.z) if parent else 0, owner(a)])
        elif owner(a) == 2 and a.get_name() not in S['enemy_ids_before'] and not name.startswith('Rus_Vehicle'):
            # Spawned after the trigger: dropped soldiers.
            for i, (point, _) in enumerate(setup.PARA_DROPS):
                if d2(loc, point) < 1500 and loc.z < 900:
                    rec['near_para'][i] += 1
    rec['deep_damage'], rec['deep_dead'] = deep_base_damage()
    S['samples'].append(rec)
    return rec


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    if True:
        world = game_world()
        if phase == 'wait_load' and S['t'] > 15.0:
            ensure_sounds()
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.Actor)
                       if a.get_class().get_name() == 'BP_MissionManager_C'][0]
            clear = None
            for top in manager.get_editor_property('missions'):
                while top is not None:
                    if top.get_class().get_name() == CLEAR:
                        clear = top
                    top = top.get_editor_property('next_mission')
            check('clear_mission_found', clear is not None)
            if clear is None:
                finish()
                return
            if os.environ.get('ASSAULT_TEST_USE_SAVED', '0') != '1':
                setup.apply_defaults(clear)
            else:
                log('using the saved Blueprint defaults (no apply_defaults on the instance)')
            resp = clear.get_editor_property('assault_response')
            check('defaults_applied', resp.get_editor_property('enabled')
                  and resp.get_editor_property('enemy_alarm_line') is not None
                  and resp.get_editor_property('air_support_line') is not None
                  and len(resp.get_editor_property('air_drops')) == 3,
                  f"alarm {resp.get_editor_property('enemy_alarm_line')} support {resp.get_editor_property('air_support_line')}")
            clear.set_editor_property('mission_trigger', None)
            manager.set_editor_property('missions', [clear])
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return

        if phase == 'wait_pie':
            if world is not None and elapsed > 8.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                if pc is not None:
                    pc.call_method('ClickedStartGameButton')
                    log('PIE up, start clicked')
                    S['phase'], S['mark'] = 'wait_tour_end', S['t']
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return

        m = mission(world) if world is not None else None

        if phase == 'wait_tour_end':
            if m is not None and m.call_method('GetHasTourFinished'):
                S['tour_end'] = wall()
                S['phase'], S['mark'] = 'pre_trigger', S['t']
            elif wall() > 400:
                check('tour_finished', False, 'no tour end')
                finish()
            return

        if phase == 'pre_trigger':
            if wall() - S['tour_end'] < 4.0:
                return
            check('no_response_without_player_on_ramp', not m.call_method('GetHasAssaultResponseStarted'))
            pawns = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)
            S['enemy_ids_before'] = {a.get_name() for a in pawns if owner(a) == 2}
            # The cut chain spawns no player units: spawn a player tank below the ramp through the mission's
            # own async vehicle spawn, then move it onto the ramp.
            S['spawned_tank'] = unreal.MissionMapTestLibrary.map_test_spawn_actor(
                world, unreal.load_class(None, TANK), unreal.Vector(-7600.0, 0.0, 300.0), unreal.Rotator(0.0, 0.0, 0.0))
            log(f"spawn request -> {S['spawned_tank']}")
            S['phase'], S['mark'] = 'wait_tank', S['t']
            return

        if phase == 'wait_tank':
            tanks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)
                     if a.get_class().get_name() == TANK.split('.')[-1] and owner(a) == 1]
            if not tanks:
                if elapsed > 60.0:
                    check('player_unit_available', False, 'no player tank 60 s after the spawn request')
                    finish()
                return
            unit = tanks[0]
            check('player_unit_available', True, f'{unit.get_class().get_name()} at {unit.get_actor_location()}')
            if elapsed < 3.0:
                return
            check('no_response_with_tank_below_ramp', not m.call_method('GetHasAssaultResponseStarted'))
            centre = setup.TRIGGER_CENTRE
            unit.set_actor_location(unreal.Vector(centre[0], centre[1], 650.0), False, True)
            log(f'teleported {unit.get_class().get_name()} onto the ramp at {unit.get_actor_location()}')
            S['phase'], S['mark'] = 'wait_trigger', S['t']
            S['teleport_wall'] = wall()
            return

        if phase == 'wait_trigger':
            if m.call_method('GetHasAssaultResponseStarted'):
                S['trigger_wall'] = wall()
                check('trigger_fires_on_ramp', True, f'{round(wall() - S["teleport_wall"], 2)} s after teleport')
                drops = m.call_method('GetResponseAirDrops') or []
                check('three_air_drops_spawned', len(drops) == 3, len(drops))
                S['health0'] = snapshot_soviet_health(world)
                log(f"tracking {len(S['health0'])} Soviet actors in the deep base")
                S['phase'], S['mark'] = 'watch', S['t']
                S['next_sample'] = 0.0
            elif wall() - S['teleport_wall'] > 5.0:
                check('trigger_fires_on_ramp', False, 'no response 5 s after the teleport')
                finish()
            return

        if phase == 'watch':
            since = wall() - S['trigger_wall']
            if since >= S['next_sample']:
                S['next_sample'] += 2.0
                rec = sample(world, m)
                log(f"t+{rec['t']} deep-base damage {rec['deep_damage']} dead {rec['deep_dead']} player planes {len(rec['player_planes'])} enemy planes {len(rec['enemy_planes'])} "
                    f"t26 {rec['t26']} near para {rec['near_para']}")
                hanging = [t for t in rec['t26'] if t[3]]
                if hanging:
                    S['hang_gap'] = max(S.get('hang_gap', 0), hanging[0][4])
                near = [t for t in hanging if ((t[0] - setup.T26_DROP[0][0]) ** 2 + (t[1] - setup.T26_DROP[0][1]) ** 2) ** 0.5 < 3000]
                if near and 'cam_t26' not in S:
                    S['cam_t26'] = True
                    pawn = unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
                    cam = pawn.get_actor_location()
                    pawn.set_actor_location(unreal.Vector(near[0][0] - 1500, near[0][1], cam.z), False, True)
                elif near and 'shot_t26' not in S:
                    S['shot_t26'] = True
                    shot('t26_on_ropes')
                if len(rec['player_planes']) >= 2 and 'shot_ju87' not in S and since > 20:
                    S['shot_ju87'] = True
                    shot('ju87_strafe')
            if 'support_launched_at' not in S and m.call_method('GetHasAirSupportLaunched'):
                S['support_launched_at'] = since
            if since > 85.0:
                evaluate()
                finish()


def evaluate():
    samples = S['samples']
    check('air_support_launched', 'support_launched_at' in S, f"t+{S.get('support_launched_at')}")
    max_player = max((len(s['player_planes']) for s in samples), default=0)
    check('two_ju87s', max_player == 2, f'max {max_player} player aircraft')
    def dmg(t):
        rows = [s for s in samples if s['t'] <= t]
        return rows[-1]['deep_damage'] if rows else 0
    # One pass over the base about every 20 s (dive circuit), so 20 s windows.
    windows = [(t0, dmg(t0 + 20) - dmg(t0)) for t0 in (15, 35, 55)]
    check('ju87s_damage_base_through_60s', all(d > 0 for _, d in windows),
          f'deep-base damage per 20 s window from t+15: {windows}, dead {samples[-1]["deep_dead"] if samples else 0}')
    late = [s for s in samples if 30 <= s['t'] <= 66]
    far = [max(((p[1] - setup.STRIKE_TARGET[0]) ** 2 + (p[2] - setup.STRIKE_TARGET[1]) ** 2) ** 0.5
               for p in s['player_planes']) for s in late if s['player_planes']]
    check('ju87s_stay_over_base', far and max(far) < 12000, f'max distance from the target t+30..66: {round(max(far)) if far else None} cm')
    max_enemy = max((len(s['enemy_planes']) for s in samples), default=0)
    check('three_soviet_transports', max_enemy >= 3, f'max {max_enemy} enemy aircraft at once')
    check('t26_hung_on_ropes', S.get('hang_gap', 0) > 900, f"aircraft {S.get('hang_gap')} cm above the T-26")
    last = samples[-1] if samples else {'t26': [], 'near_para': [0, 0]}
    landed = [t for t in last['t26'] if not t[3]]
    check('t26_landed_in_base', any(-2600 < t[0] < 4100 and -4200 < t[1] < 5200 and t[2] < 800 for t in landed)
          and all(t[5] == 2 for t in landed), last['t26'])
    landed = [s['near_para'] for s in samples if 30 <= s['t'] <= 50]
    low = [min((n[i] for n in landed), default=0) for i in range(2)]
    check('paratroopers_landed_at_both_points', all(v >= 3 for v in low), f'soldiers near points t+30..50 (min) {low}')


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
log('assault response test loaded')
