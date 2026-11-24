"""PIE visual check of BP_SovietCargoTrain on Ger_IntroMission (2026-11-18).

Exec'd through train_editor_server.py in a throwaway DebugGame editor (`python train_cmd.py cargo_train_visual_test.py
--timeout 600`). Before PIE the editor-world mission manager's Missions are emptied (map NOT saved; restored after),
fog-of-war post process is turned off in PIE. A 6-flatcar train (baked cargo tanks) + 1 goods wagon is spawned with
MissionMapTestLibrary.map_test_spawn_actor and driven from the north edge to the stop; then HideCargo + Depart.
HighResShots go to Saved/CargoTrainTest/<tag>_*.png, results to Saved/CargoTrainTest/<tag>_result.json.
"""
import json
import math
import os
import time

import unreal

TAG = globals().get('TEST_TAG', 'r1')
OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/CargoTrainTest'
os.makedirs(OUT_DIR, exist_ok=True)
PATH_JSON = 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/train_path.json'
BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/BP_SovietCargoTrain.BP_SovietCargoTrain_C'
CT = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/CargoTrain/'
TANKS = ['SM_CargoTank_T3476', 'SM_CargoTank_KV1E', 'SM_CargoTank_IS2'] * 2
ARM_FACTOR = globals().get('ARM_FACTOR', 1.0)

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
DATA = json.load(open(PATH_JSON))
PTS = [unreal.Vector(*p) for p in DATA['points']]
STOP = unreal.Vector(*DATA['stop_front_location'])
T = {'stage': 'start', 't0': time.time(), 'mark': 0.0, 'log': [], 'checks': {}, 'samples': [], 'shots': []}


def log(msg):
    T['log'].append('%7.1f %s' % (time.time() - T['t0'], msg))
    unreal.log_warning('CARGOTRAINTEST ' + msg)


def check(name, ok, detail=''):
    T['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log('CHECK %s: %s %s' % (name, 'PASS' if ok else 'FAIL', detail))


def manager(world):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_class().get_name() == 'BP_MissionManager_C':
            return a
    return None


def rail_z(x, y):
    best = (1e18, 0.0)
    for a, b in zip(PTS, PTS[1:]):
        dx, dy = b.x - a.x, b.y - a.y
        l2 = dx * dx + dy * dy
        t = max(0.0, min(1.0, ((x - a.x) * dx + (y - a.y) * dy) / l2)) if l2 > 0 else 0.0
        px, py = a.x + dx * t, a.y + dy * t
        d = math.hypot(px - x, py - y)
        if d < best[0]:
            best = (d, a.z + (b.z - a.z) * t)
    return best[1], best[0]


def cars(train):
    comps = [c for c in train.get_components_by_class(unreal.StaticMeshComponent)]
    car_comps = [c for c in comps if not isinstance(c.get_attach_parent(), unreal.StaticMeshComponent)]
    cargo = [c for c in comps if c not in car_comps]
    return car_comps, cargo


def car_rows(train):
    rows = []
    car_comps, _ = cars(train)
    for c in car_comps:
        mb = c.static_mesh.get_bounds()
        # bounds bottom centre of the mesh, through the car's own (pitched) transform
        bottom = c.get_world_transform().transform_location(
            unreal.Vector(mb.origin.x, mb.origin.y, mb.origin.z - mb.box_extent.z))
        origin = c.get_world_transform().transform_location(mb.origin)
        rz, rd = rail_z(bottom.x, bottom.y)
        rows.append({'mesh': c.static_mesh.get_name(), 'centre': [round(origin.x), round(origin.y), round(origin.z)],
                     'bottom_minus_rail': round(bottom.z - rz, 1), 'rail_dist': round(rd),
                     'pitch': round(c.get_world_rotation().pitch, 1),
                     'visible': c.is_visible()})
    return rows


def shot(gw, name):
    fn = '%s/%s_%s.png' % (OUT_DIR, TAG, name)
    unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + fn)
    T['shots'].append(fn)
    log('shot ' + fn)


def move_cam(gw, x, y, yaw=0.0, arm=None):
    T['cam'] = (x, y, yaw, arm)
    pin_cam(gw)


def pin_cam(gw):
    if 'cam' not in T:
        return
    x, y, yaw, arm = T['cam']
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    if pawn is None:
        return
    pawn.set_actor_location(unreal.Vector(x, y, 950.0), False, False)
    pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
    springarm = pawn.get_component_by_class(unreal.SpringArmComponent)
    if springarm is not None:
        if 'arm0' not in T:
            T['arm0'] = springarm.get_editor_property('target_arm_length')
            log('arm length %.0f' % T['arm0'])
        springarm.set_editor_property('target_arm_length', T['arm0'] * (arm or ARM_FACTOR))


def finish_pie():
    les.editor_request_end_play()
    T['stage'] = 'ending'


def write_result():
    ew = ues.get_editor_world()
    m = manager(ew)
    if m is not None and 'missions0' in T:
        m.set_editor_property('missions', T['missions0'])
    passed = sum(1 for c in T['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(T['checks']), 'checks': T['checks'], 'log': T['log'],
               'samples': T['samples'], 'shots': T['shots']}, open('%s/%s_result.json' % (OUT_DIR, TAG), 'w'), indent=1)
    print('RESULT %d/%d' % (passed, len(T['checks'])))
    print('\n'.join(T['log']))


def follow_point(train, index):
    car_comps, _ = cars(train)
    if not car_comps:
        return None
    return car_comps[index].get_world_location()


def step():
    gw = ues.get_game_world()
    st = T['stage']
    # The start screen / error dialogs pause the PIE game (ACPPController::PauseGame): keep it running.
    if gw is not None and unreal.GameplayStatics.is_game_paused(gw):
        unreal.GameplayStatics.set_game_paused(gw, False)
    if gw is not None:
        pin_cam(gw)
    if st not in ('start', 'ending', 'done') and time.time() - T['t0'] > 480:
        check('overall_timeout', False, 'stage %s after 480 s' % st)
        if gw is None:
            write_result()
            return True
        finish_pie()
        return False
    if st == 'start':
        ew = ues.get_editor_world()
        m = manager(ew)
        if m is not None:
            T['missions0'] = list(m.get_editor_property('missions'))
            m.set_editor_property('missions', [])
        log('missions emptied on the editor world instance (%s)' % (m is not None))
        les.editor_request_begin_play()
        T['stage'], T['mark'] = 'wait_pie', time.time()
        return False
    if st == 'wait_pie':
        if gw is None:
            if time.time() - T['mark'] > 180:
                check('pie_started', False)
                T['stage'] = 'done'
                write_result()
                return True
            return False
        if 'pie_wall' not in T:
            T['pie_wall'] = time.time()
        gt = unreal.GameplayStatics.get_time_seconds(gw)
        if time.time() - T['pie_wall'] < 8.0:
            return False
        unreal.SystemLibrary.execute_console_command(gw, 't.IdleWhenNotForeground 0')
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
        pc = unreal.GameplayStatics.get_player_controller(gw, 0)
        try:
            pc.call_method('ClickedStartGameButton')
        except Exception as exc:  # noqa
            log('no start button: %s' % exc)
        cls = unreal.load_class(None, BP)
        train = unreal.MissionMapTestLibrary.map_test_spawn_actor(gw, cls, PTS[0], unreal.Rotator(0, 0, 0))
        check('train_spawned', train is not None, train)
        if train is None:
            finish_pie()
            return False
        meshes = [unreal.load_asset(CT + n) for n in TANKS]
        train.call_method('StartRun', (PTS, STOP, meshes, 1))
        car_comps, cargo = cars(train)
        check('consist_built', len(car_comps) == 8 and len(cargo) == 6, '%d cars %d cargo' % (len(car_comps), len(cargo)))
        T['train'], T['gt0'], T['stage'] = train, gt, 'arriving'
        T['next_shot'] = gt + 9.0
        T['roll_k'] = 0
        return False
    if gw is None:
        if st == 'ending':
            write_result()
            return True
        return False
    gt = unreal.GameplayStatics.get_time_seconds(gw)
    train = T.get('train')
    alive = train is not None and unreal.SystemLibrary.is_valid(train)
    if st == 'arriving':
        if alive and int(gt * 2) != T.get('last_sample'):
            T['last_sample'] = int(gt * 2)
            rows = car_rows(train)
            T['samples'].append({'t': round(gt - T['gt0'], 1), 'loco': rows[0]['centre'], 'tail': rows[-1]['centre']})
        p = follow_point(train, 2) if alive else None
        if p is not None and gt >= T['next_shot'] and T['roll_k'] < 2:
            move_cam(gw, p.x, p.y, 0.0, 1.6)
            T['next_shot'] = gt + 6.0
            T['roll_k'] += 1
            T['pending_shot'] = (gt + 1.0, 'rolling_in_%d' % T['roll_k'])
        if 'pending_shot' in T and gt >= T['pending_shot'][0]:
            shot(gw, T.pop('pending_shot')[1])
        if alive and train.call_method('GetHasArrived'):
            T['arrive_t'] = gt - T['gt0']
            rows = car_rows(train)
            check('arrived', True, 'after %.1f s' % T['arrive_t'])
            loco = rows[0]['centre']
            check('stop_reached', abs(loco[1] - (STOP.y + 963)) < 150, 'loco centre %s (front ~%.0f)' % (loco, loco[1] - 963))
            worst = max(abs(r['bottom_minus_rail']) for r in rows)
            check('cars_on_rails', worst < 30.0, 'max |bottom-rail| %.1f cm; rows %s' % (worst, rows))
            check('tail_position', True, 'tail car centre %s' % rows[-1]['centre'])
            cargo = train.call_method('GetCargoWorldLocations')
            log('cargo %s' % [(round(c.x), round(c.y), round(c.z)) for c in cargo])
            T['stage'], T['mark'] = 'stopped', gt
            T['views'] = [('stopped_station', 20700.0, 13000.0, 0.0, 2.6),
                          ('stopped_flatcar0', 21000.0, 11561.0, 0.0, 0.8),
                          ('stopped_flatcar2', 20977.0, 15860.0, 0.0, 0.8),
                          ('stopped_flatcar4', 20909.0, 20140.0, 0.0, 0.8),
                          ('stopped_north', 20900.0, 19500.0, 0.0, 2.2),
                          ('stopped_tail', 20875.0, 23300.0, 0.0, 1.6)]
            T['view_i'], T['view_t'] = 0, None
        elif gt - T['gt0'] > 90:
            check('arrived', False, 'not arrived after 90 s')
            finish_pie()
        return False
    if st == 'stopped':
        views = T['views']
        if T['view_i'] < len(views):
            name, x, y, yaw, arm = views[T['view_i']]
            if T['view_t'] is None:
                move_cam(gw, x, y, yaw, arm)
                T['view_t'] = gt
            elif gt - T['view_t'] > 1.5 and not T.get('view_shot'):
                shot(gw, name)
                T['view_shot'] = True
            elif gt - T['view_t'] > 3.0:
                T['view_i'] += 1
                T['view_t'], T['view_shot'] = None, False
            return False
        if not T.get('hidden'):
            train.call_method('HideCargo')
            T['hidden'] = True
            move_cam(gw, 21000.0, 12600.0, 0.0, 1.2)
            T['hide_t'] = gt
            return False
        if gt - T['hide_t'] > 1.5 and not T.get('hide_shot'):
            shot(gw, 'cargo_hidden')
            T['hide_shot'] = True
            return False
        if gt - T['hide_t'] > 3.0:
            train.call_method('Depart')
            T['stage'], T['dep_t'], T['dep_k'], T['next_shot'] = 'departing', gt, 0, gt + 8.0
        return False
    if st == 'departing':
        if alive:
            if int(gt * 2) != T.get('last_sample'):
                T['last_sample'] = int(gt * 2)
                rows = car_rows(train)
                T['samples'].append({'t': round(gt - T['gt0'], 1), 'loco': rows[0]['centre'], 'tail': rows[-1]['centre'],
                                     'dep': True})
            if gt >= T['next_shot'] and T['dep_k'] < 2:
                p = follow_point(train, -1)
                move_cam(gw, p.x, p.y, 0.0, 1.6)
                T['dep_k'] += 1
                T['next_shot'] = gt + 7.0
                T['pending_shot'] = (gt + 1.0, 'departing_%d' % T['dep_k'])
            if 'pending_shot' in T and gt >= T['pending_shot'][0]:
                shot(gw, T.pop('pending_shot')[1])
            if gt - T['dep_t'] > 120:
                check('departed_destroyed', False, 'still alive after 120 s')
                finish_pie()
            return False
        check('departed_destroyed', True, 'gone %.1f s after Depart' % (gt - T['dep_t']))
        left = [a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.SovietCargoTrain)
                if unreal.SystemLibrary.is_valid(a)]
        check('no_train_left', len(left) == 0, left)
        T['end_t'] = gt
        T['stage'] = 'wait_end'
        return False
    if st == 'wait_end':
        if gt - T['end_t'] > 1.5:
            finish_pie()
        return False
    return False


_defer = step
