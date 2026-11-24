"""Throwaway-editor test of the crushable Berlin rubble cover (cover_build.py CRUSH table), 2026-11-13.
Never on Ger_Berlin, never in the shared editor. Launch with cover_crush_run.sh <logname> [timeout_s] (same folder).
The map (RTS_ToCopySmall) is never saved; the editor quits at the end.

Lanes run along world Y. Each piece stands at (x, 0), yaw 90; its vehicle starts at (x, +START_Y) and gets
MoveToLocation (x, -START_Y): the straight line goes through the middle of the piece.
  crush lanes    the vehicle's class may crush the piece: it must drive through, the piece must die by the crush
                 (lean + sink, mesh visible meanwhile), and afterwards the actor, its cover points and its nav area
                 must be gone.
  detour lanes   the vehicle's class may not crush: it must path around, the piece must survive untouched.
Before the orders: for every piece a path across it is queried on each agent's navmesh with that agent's
RTS_NavFilter_* (what the units themselves use): only the crushing classes may get the straight path.
Results: Saved/Berlin/crush_test/result.json (+ a PNG of the first crush, about 1.3 s after it started).
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/crush_test/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G(Medium)': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'Luchs(Light)': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
    'TigerH1(Heavy)': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
}
# lane key: (x, Blueprint name, vehicle, expected)
LANES = {
    'A_low_medium': (-9000, 'BP_BerlinCover_06_Breastwork', 'PzIV_G(Medium)', 'crush'),
    'B_low_light': (-6000, 'BP_BerlinCover_01_WallRun', 'Luchs(Light)', 'detour'),
    'C_tall_heavy': (-3000, 'BP_BerlinCover_03_HeapChunk', 'TigerH1(Heavy)', 'crush'),
    'D_tall_medium': (0, 'BP_BerlinCover_10_MasonryBlock', 'PzIV_G(Medium)', 'detour'),
    'E_heap_heavy': (3000, 'BP_BerlinCoverHeap_Brick', 'TigerH1(Heavy)', 'detour'),
    'F_low_heavy': (6000, 'BP_BerlinCover_04_ToppledSlab', 'TigerH1(Heavy)', 'crush'),
}
START_Y = 2400.0
MOVE_SECONDS = 110.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'tanks': {}, 'result': {}, 'track': {},
     'last_sample': 0.0}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('CRUSHTEST ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT, 'w'), indent=1, default=str)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def alive(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def nav_building(world):
    fn = getattr(unreal.NavigationSystemV1, 'is_navigation_being_built', None)
    try:
        return bool(fn(world)) if fn else False
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def mesh_of(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'MainMesh'][0]


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, bp, vehicle, _) in LANES.items():
        cls = unreal.load_class(None, f'{BP_DIR}{bp}.{bp}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 90))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, START_Y, ground_z(world, x, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed {bp} at x {x} and {vehicle}, owner set {set_owner(tank, 1)}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        for key, (x, bp, _, _) in LANES.items():
            if name == bp + '_C' and abs(actor.get_actor_location().x - x) < 200:
                S['pieces'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    log(f'runtime pieces {sorted(S["pieces"])} tanks {sorted(S["tanks"])}')


def navmeshes(world):
    out = {}
    names = []
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        names.append(nav.get_name())
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
            if n not in out.values()]
    if rest:
        out['Character'] = rest[0]
    log(f'navmesh actors {names} -> {dict((k, v.get_name()) for k, v in out.items())}')
    return out


def path(world, a, b, nav_data, filter_class):
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data, filter_class)
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:100])
    if not pts:
        return dict(length=None)
    length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))
    side = max(abs(pt.x - a.x) for pt in pts)
    return dict(length=round(length), straight=round((b - a).length()), partial=bool(p.is_partial()),
                side_cm=round(side), through=bool(side < 30 and not p.is_partial()))


def across_paths(world, key):
    """A 12 m path across the piece for every agent on its own navmesh with its own query filter."""
    x = LANES[key][0]
    z = S['origins'][key].z + 20
    a, b = unreal.Vector(x, 600, z), unreal.Vector(x, -600, z)
    out = {}
    for agent, nav in S['nav'].items():
        out[agent] = path(world, a, b, nav, unreal.load_class(None, FILTERS[agent]))
    return out


def cover_subsystem(world):
    if S.get('cover_sub') is not None and alive(S['cover_sub']):
        return S['cover_sub']
    for sub in unreal.ObjectIterator(unreal.RTSCoverFinderWorldSubsystem):
        if sub.get_outer() == world or sub.get_world() == world:
            S['cover_sub'] = sub
            return sub
    raise RuntimeError('no RTSCoverFinderWorldSubsystem in the PIE world')


def socket_points_published(world, key):
    """How many of the piece's sockets have a published cover point within 40 cm (2D)."""
    points = list(cover_subsystem(world).find_cover_points_in_radius(S['origins'][key], 900.0))
    count = 0
    for loc in S['sockets'][key]:
        if any(math.hypot(p.get_editor_property('location').x - loc.x, p.get_editor_property('location').y - loc.y) <= 40
               for p in points):
            count += 1
    return count


def piece_state(key):
    piece = S['pieces'][key]
    if not alive(piece):
        return None
    mesh = mesh_of(piece)
    rot = mesh.get_world_rotation()
    hc = piece.get_component_by_class(unreal.HealthComponent)
    return dict(z=round(piece.get_actor_location().z - S['origins'][key].z, 1), pitch=round(rot.pitch, 1),
                roll=round(rot.roll, 1), yaw=round(rot.yaw, 1), visible=bool(mesh.is_visible()),
                collision=str(mesh.get_collision_enabled()).split('.')[-1].split(':')[0],
                life=round(piece.get_life_span(), 1), health=round(hc.get_editor_property('CurrentHealth')) if hc else None)


def setup_report(key):
    piece = S['pieces'][key]
    mesh = mesh_of(piece)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('EnemyObj', ch.ECC_ENEMY_OBJ), ('Projectile', ch.ECC_PROJECTILE),
                         ('Pawn', ch.ECC_PAWN))}
    boxes = [c for c in piece.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock_')]
    areas = sorted(set(str(b.get_editor_property('area_class_override').get_name()) for b in boxes))
    info = dict(cls=piece.get_class().get_name(), overlap_events=bool(mesh.get_editor_property('generate_overlap_events')), responses=resp,
                nav_boxes=len(boxes), nav_areas=areas, sockets=len(mesh.get_all_socket_names()))
    for prop in ('Crushable', 'CrushDestructionType', 'Overlap Tanks', 'CrushSinkDeltaZ', 'CrushTipSpeedScale'):
        try:
            info[prop] = str(piece.get_editor_property(prop))
        except Exception as error:  # noqa: BLE001
            info[prop] = 'ERR ' + repr(error)[:60]
    holes = piece.get_component_by_class(unreal.WallHoleDamageComponent)
    if holes:
        info['collapse_on_death'] = bool(holes.get_editor_property('CollapseSettings').get_editor_property(
            'collapse_into_chunks_on_death'))
    return info


def sample():
    """Vehicle track and piece state; the first change of a piece (crush) starts a dense log and a picture."""
    for key, tank in S['tanks'].items():
        x = LANES[key][0]
        track = S['track'].setdefault(key, dict(min_y=1e9, max_side_near=0.0, cross_side=None, samples=[], crush=None,
                                                gone_at=None, last=None))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            track['min_y'] = min(track['min_y'], loc.y)
            if abs(loc.y) < 450:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < 0:
                track['cross_side'] = round(loc.x - x)
                track['cross_time'] = round(S['t'] - S['order_time'], 1)
        else:
            track['last'] = 'vehicle destroyed'
        state = piece_state(key)
        if state is None:
            if track['gone_at'] is None:
                track['gone_at'] = round(S['t'] - S['order_time'], 1)
                log(f'{key}: piece actor gone at {track["gone_at"]} s after the order')
            continue
        changed = state['z'] != 0 or abs(state['pitch']) > 0.5 or abs(state['roll']) > 0.5 or state['life'] > 0 \
            or not state['visible']
        if changed and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], first=state)
            log(f'{key}: piece changed {json.dumps(track["crush"])}')
            if not S.get('pic_queue'):
                aim_camera(key)
                S.setdefault('pic_queue', []).append((key, S['t'] + 1.3))
        if changed and len(track['samples']) < 40 and S['t'] - track.get('sample_t', 0.0) >= 0.25:
            track['sample_t'] = S['t']
            track['samples'].append((round(S['t'] - S['order_time'], 2), state['z'], state['pitch'], state['roll'],
                                     state['visible'], state['collision']))


def aim_camera(key):
    o = S['origins'][key]
    eye, look = o + unreal.Vector(900, -1100, 800), o + unreal.Vector(0, 0, 60)
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def picture(key):
    S['busy'] = True   # the screenshot pumps the tick callback
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}crush_{key}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture crush_{key}')


def verdicts():
    out = {}
    for key, (x, bp, vehicle, expected) in LANES.items():
        track = S['track'].get(key, {})
        piece_alive = alive(S['pieces'][key])
        crushed = track.get('crush') is not None
        if expected == 'crush':
            ok = crushed and not piece_alive and track.get('cross_side') is not None and abs(track['cross_side']) < 150
        else:
            ok = (not crushed) and piece_alive and track.get('cross_side') is not None and \
                track.get('max_side_near', 0) > 100
        out[key] = dict(piece=bp, vehicle=vehicle, expected=expected, passed=bool(ok), crushed=crushed,
                        piece_alive_at_end=piece_alive, vehicle_crossed_y0_at_dx=track.get('cross_side'),
                        vehicle_cross_time_s=track.get('cross_time'), vehicle_max_side_offset_near_piece=round(
                            track.get('max_side_near', 0)), vehicle_last=track.get('last'), crush=track.get('crush'),
                        actor_gone_s=track.get('gone_at'), samples=track.get('samples'))
    return out


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(editor_world, 'RebuildNavigation')
            S['phase'], S['mark'] = 'editor_nav', S['t']
        elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if nav_building(editor_world) and S['t'] - S['mark'] < 120.0:
                return
            log(f'editor navmesh rebuilt in {S["t"] - S["mark"]:.1f} s')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                find_runtime(world)
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
                S['sockets'] = {k: [mesh_of(p).get_socket_location(n) for n in mesh_of(p).get_all_socket_names()]
                                for k, p in S['pieces'].items()}
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            S['nav'] = navmeshes(world)
            S['result']['setup'] = {k: setup_report(k) for k in S['pieces']}
            S['result']['paths_before'] = {k: across_paths(world, k) for k in S['pieces']}
            S['result']['cover_points_before'] = {k: f'{socket_points_published(world, k)}/{len(S["sockets"][k])}'
                                                  for k in S['pieces']}
            for key in S['pieces']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            log(f'cover points before {S["result"]["cover_points_before"]}')
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                x = LANES[key][0]
                target = unreal.Vector(x, -START_Y, S['origins'][key].z + 20)
                try:
                    result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation {target} -> {result}')
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            queue = S.get('pic_queue', [])
            if queue and S['t'] >= queue[0][1]:
                picture(queue.pop(0)[0])
            elapsed = S['t'] - S['mark']
            arrived = all(isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 400
                          for t in S['track'].values()) and len(S['track']) == len(S['tanks'])
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 20.0 and not queue):
                S['phase'], S['mark'] = 'settle', S['t']
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
        elif phase == 'settle':
            sample()
            if S['t'] - S['mark'] > 6.0:
                world = game_world()
                S['result']['lanes'] = verdicts()
                S['result']['paths_after'] = {k: across_paths(world, k) for k in S['pieces']}
                S['result']['cover_points_after'] = {k: f'{socket_points_published(world, k)}/{len(S["sockets"][k])}'
                                                     for k in S['pieces']}
                for key, row in S['result']['lanes'].items():
                    short = {k: v for k, v in row.items() if k != 'samples'}
                    log(f'{key} verdict {json.dumps(short)}')
                    log(f'{key} samples {row["samples"]}')
                    log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
                log(f'cover points after {S["result"]["cover_points_after"]}')
                S['result']['all_passed'] = all(r['passed'] for r in S['result']['lanes'].values())
                log(f'ALL PASSED {S["result"]["all_passed"]}')
                finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 400.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
