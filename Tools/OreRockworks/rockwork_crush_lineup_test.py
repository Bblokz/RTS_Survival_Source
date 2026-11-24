"""Throwaway-editor test of the Ore rockworks, part 2: no-crush lanes, navigation and the lineup pictures.
Derived from Tools/DesertRocks/rock_crush_test.py and rock_lineup_shot.py. Never in the shared editor; the map
(UnitTests/RTS_ToCopySmall) is never saved; the editor quits at the end.
Launch: ROCK_SCRIPT=rockwork_crush_lineup_test.py sh fire_test_run.sh <logname> 600

Lanes along world Y; each vehicle starts 24 m before its rockwork and is ordered 24 m past it:
  A  R02_Stack        Pz IV G  (MediumTank)  -> must not crush: path round (or stop), rock untouched
  B  R06_FallenBlock  Tiger H1 (HeavyTank)   -> same
Lineup (y = LOOK_Y): all 8 rockworks in a row, a parked player Pz IV G, the cliff kit piece SM_OreCliff_Block2
(seated by its mesh bounds: its pivot is at the centre) and the medium crushable BP_OreRock_M04_Split.
Results: Saved/OreMountains/rockworks/lineup/result.json + PNGs.
"""
import json
import os

import unreal

# ORW_HOLES_ONLY=1: only the R01 close-up and the debug-holes close-up (material look check), into lineup/holes_only/
HOLES_ONLY = os.environ.get('ORW_HOLES_ONLY') == '1'
# ORW_PICS_ONLY=1: every look shot (lineup, comparison, close-ups, holes), no lanes; for a re-shoot after a material change
PICS_ONLY = os.environ.get('ORW_PICS_ONLY') == '1'
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/rockworks/lineup/' + ('holes_only/' if HOLES_ONLY else '')
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks/Blueprints/'
ROCKS_JSON = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRockworks/out/rocks.json'
CRUSH_BP = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreRock_M04_Split'
CLIFF_MESH = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Meshes/SM_OreCliff_Block2'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G(Medium)': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1(Heavy)': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
}
LANES = {
    'A_stack_pz4': (-6000, 'BP_OreRockwork_R02_Stack', 'PzIV_G(Medium)'),
    'B_fallen_tiger': (0, 'BP_OreRockwork_R06_FallenBlock', 'TigerH1(Heavy)'),
}
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
START_Y = 2400.0
MOVE_SECONDS = 90.0
LOOK_Y = 9000.0
GAP = 350.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'tanks': {}, 'result': {}, 'track': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('ORWLINE ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT, 'w'), indent=1, default=str)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


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


def lineup_spots():
    rocks = json.load(open(ROCKS_JSON))
    items = sorted((info['index'], name, info['dims'][0] * 100.0) for name, info in rocks.items())
    width = sum(w for _, _, w in items) + GAP * (len(items) - 1)
    x = -width / 2
    spots = {}
    for _, name, w in items:
        spots[name] = x + w / 2
        x += w + GAP
    S['lineup_span'] = (-width / 2, width / 2)
    return spots


def place_all():
    world = editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, bp, vehicle) in LANES.items():
        cls = unreal.load_class(None, f'{BP_DIR}{bp}.{bp}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 90))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, START_Y, ground_z(world, x, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed {bp} at x {x} and {vehicle}, owner set {set_owner(tank, 1)}')
    S['spots'] = lineup_spots()
    for name, x in S['spots'].items():
        cls = unreal.load_class(None, f'{BP_DIR}BP_OreRockwork_{name}.BP_OreRockwork_{name}_C')
        if cls is None:
            log(f'missing BP {name}')
            continue
        actors.spawn_actor_from_class(cls, unreal.Vector(x, LOOK_Y, ground_z(world, x, LOOK_Y)), unreal.Rotator(0, 0, 0))
    right = S['lineup_span'][1]
    # comparison group at the right end: a parked player Pz IV, the medium crushable rock, one cliff piece
    tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES['PzIV_G(Medium)']),
                                         unreal.Vector(right + 450, LOOK_Y - 150, ground_z(world, right + 450, LOOK_Y) + 50),
                                         unreal.Rotator(0, 0, 0))
    log(f'lineup Pz IV owner set {set_owner(tank, 1)}')
    cls = unreal.load_class(None, CRUSH_BP + '.BP_OreRock_M04_Split_C')
    actors.spawn_actor_from_class(cls, unreal.Vector(right + 1150, LOOK_Y - 100, ground_z(world, right + 1150, LOOK_Y)),
                                  unreal.Rotator(0, 0, 20))
    mesh = unreal.load_asset(CLIFF_MESH)
    box = mesh.get_bounding_box()
    lift = -box.min.z                     # the cliff pieces have their pivot at the centre: seat by the mesh bounds
    cx = right + 1700 + (box.max.x - box.min.x) * 0.5
    cliff = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                          unreal.Vector(cx, LOOK_Y, ground_z(world, cx, LOOK_Y) + lift - 5),
                                          unreal.Rotator(0, 0, 0))
    cliff.static_mesh_component.set_static_mesh(mesh)
    S['cliff_x'] = cx
    log(f'cliff piece {mesh.get_name()} at x {cx:.0f}, lifted {lift:.0f} cm (mesh min z {box.min.z:.0f})')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 20000, 3000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, bp, _) in LANES.items():
            if name == bp + '_C' and abs(loc.x - x) < 200 and abs(loc.y) < 200:
                S['pieces'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 20000, 3000)).length())
    log(f'runtime pieces {sorted(S["pieces"])} tanks {sorted(S["tanks"])}')


def navmeshes(world):
    out = {}
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
            if n not in out.values()]
    if rest:
        out['Character'] = rest[0]
    log(f'navmeshes {dict((k, v.get_name()) for k, v in out.items())}')
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
    x = S['origins'][key].x
    z = S['origins'][key].z + 20
    a, b = unreal.Vector(x, 900, z), unreal.Vector(x, -900, z)
    return {agent: path(world, a, b, nav, unreal.load_class(None, FILTERS[agent])) for agent, nav in S['nav'].items()}


def piece_state(key):
    piece = S['pieces'][key]
    if not alive(piece):
        return None
    mesh = mesh_of(piece)
    rot = mesh.get_world_rotation()
    hc = piece.get_component_by_class(unreal.HealthComponent)
    return dict(z=round(mesh.get_world_location().z - S['origins'][key].z, 1), pitch=round(rot.pitch, 1),
                roll=round(rot.roll, 1), visible=bool(mesh.is_visible()),
                collision=str(mesh.get_collision_enabled()).split('.')[-1].split(':')[0],
                life=round(piece.get_life_span(), 1), health=round(hc.get_editor_property('CurrentHealth')) if hc else None)


def setup_report(key):
    piece = S['pieces'][key]
    mesh = mesh_of(piece)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('EnemyObj', ch.ECC_ENEMY_OBJ),
                         ('Projectile', ch.ECC_PROJECTILE), ('Pawn', ch.ECC_PAWN))}
    boxes = [c for c in piece.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock')]
    info = dict(cls=piece.get_class().get_name(), mesh=mesh.get_editor_property('static_mesh').get_name(),
                material=mesh.get_material(0).get_name(), responses=resp, nav_boxes=len(boxes),
                nav_areas=sorted(set(str(b.get_editor_property('area_class_override').get_name()) for b in boxes)))
    for prop in ('Crushable', 'Overlap Tanks'):
        try:
            info[prop] = str(piece.get_editor_property(prop))
        except Exception as error:  # noqa: BLE001
            info[prop] = 'ERR ' + repr(error)[:60]
    hc = piece.get_component_by_class(unreal.HealthComponent)
    info['health'] = round(hc.get_editor_property('CurrentHealth')) if hc else None
    return info


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, changed=None, last=None,
                                                min_dist=1e9))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            track['min_dist'] = min(track['min_dist'], round((loc - S['origins'][key]).length()))
            if abs(loc.y) < 600:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < 0:
                track['cross_side'] = round(loc.x - x)
        state = piece_state(key)
        if state is None:
            track['gone'] = True
            continue
        changed = state['z'] != 0 or abs(state['pitch']) > 0.5 or abs(state['roll']) > 0.5 or state['life'] > 0 \
            or not state['visible'] or (state['health'] is not None and state['health'] < S['start_health'][key])
        if changed and track['changed'] is None:
            track['changed'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], state=state)
            log(f'{key}: rock changed {json.dumps(track["changed"])}')


def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def shoot(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def look_shots():
    z = S['look_z']
    lo, hi = S['lineup_span']
    span = hi - lo
    sp = S['spots']
    names = sorted(sp, key=lambda n: sp[n])
    right = hi
    cx = S['cliff_x']
    v = unreal.Vector
    return [
        ('_warmup', v(0, LOOK_Y + 5000, z + 4000), v(0, LOOK_Y, z)),
        ('lineup_rts_angle', v((lo + cx) / 2, LOOK_Y + 4200, z + 5200), v((lo + cx) / 2, LOOK_Y - 300, z)),
        ('lineup_low', v((lo + cx) / 2, LOOK_Y + span * 0.62, z + 1400), v((lo + cx) / 2, LOOK_Y, z + 200)),
        ('lineup_left', v(sp[names[1]], LOOK_Y + 2600, z + 1300), v(sp[names[1]], LOOK_Y, z + 220)),
        ('lineup_right', v(sp[names[5]] + 300, LOOK_Y + 2600, z + 1300), v(sp[names[5]] + 300, LOOK_Y, z + 220)),
        ('compare_tank_crush_cliff', v(right + 700, LOOK_Y + 2000, z + 900), v(right + 600, LOOK_Y, z + 150)),
        ('close_R01_Outcrop', v(sp['R01_Outcrop'] - 300, LOOK_Y + 1300, z + 650), v(sp['R01_Outcrop'], LOOK_Y, z + 180)),
        ('close_R04_SplitTor', v(sp['R04_SplitTor'] + 250, LOOK_Y + 1300, z + 650),
         v(sp['R04_SplitTor'], LOOK_Y, z + 200)),
        ('close_R03_LeaningSlab', v(sp['R03_LeaningSlab'] - 600, LOOK_Y + 1100, z + 500),
         v(sp['R03_LeaningSlab'], LOOK_Y, z + 150)),
    ]


def queue_holes_shot(name):
    """The hole close-up goes last; the hits are applied just before it, so every other picture shows clean rocks."""
    sp = S['spots'][name]
    z = S['look_z']
    S['shots'].append((f'holes_fill_{name}', unreal.Vector(sp + 200, LOOK_Y + 1200, z + 450),
                       unreal.Vector(sp, LOOK_Y, z + 160)))


def holes_on_lineup_rock(world, name):
    """Three debug hits (75 / 88 / 105 mm) on the camera side of one lineup rock, for the hole look close-up."""
    rock = None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name() == f'BP_OreRockwork_{name}_C':
            rock = actor
    if rock is None:
        log(f'holes: {name} not found')
        return
    mesh = mesh_of(rock)
    holes = rock.get_component_by_class(unreal.WallHoleDamageComponent)
    o = rock.get_actor_location()
    for dx, dz, cal in ((-250, 150, 75), (60, 210, 88), (300, 110, 105)):
        start = o + unreal.Vector(dx, 3000, dz)
        end = o + unreal.Vector(dx, -3000, dz)
        hit = mesh.line_trace_component(start, end, False, False, False)
        if not hit:
            log(f'holes: {name} trace at dx {dx} missed')
            continue
        removed = holes.debug_fire_test_hit(hit[0], (end - start).normal(), float(cal))
        log(f'holes: {name} hit {cal} mm at dx {dx} removed {removed:.2f} m2')
    log(f'holes: {name} count {holes.get_hole_count()} slot0 {mesh.get_material(0).get_name()}')


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.SystemLibrary.execute_console_command(editor_world(), 'RebuildNavigation')
            S['phase'], S['mark'] = 'editor_nav', S['t']
        elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
            if nav_building(editor_world()) and S['t'] - S['mark'] < 120.0:
                return
            log(f'editor navmesh rebuilt in {S["t"] - S["mark"]:.1f} s')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                find_runtime(world)
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
                S['start_health'] = {k: piece_state(k)['health'] for k in S['pieces']}
                S['look_z'] = ground_z(world, 0, LOOK_Y)
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
            for key in S['pieces']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            S['shots'] = look_shots()
            if HOLES_ONLY:
                # material check only: R01 close-up (surface) and the debug holes on R08, then quit
                S['shots'] = [s for s in S['shots'] if s[0] in ('_warmup', 'close_R01_Outcrop')]
            queue_holes_shot('R08_Spur')
            S['phase'], S['mark'] = 'look', S['t']
        elif phase == 'look':
            if not S['shots']:
                if HOLES_ONLY or PICS_ONLY:
                    finish()
                    return
                S['phase'] = 'orders'
                return
            name, eye, look = S['shots'][0]
            if name.startswith('holes_fill_') and not S.get('holes_applied'):
                S['holes_applied'] = True
                holes_on_lineup_rock(game_world(), name[len('holes_fill_'):])
            if S.get('aimed') != name:
                aim(eye, look)
                S['aimed'], S['mark'] = name, S['t']
            elif S['t'] - S['mark'] > 3.0:
                shoot(name)
                S['shots'].pop(0)
        elif phase == 'orders':
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                x = LANES[key][0]
                target = unreal.Vector(x, -START_Y, S['origins'][key].z + 20)
                try:
                    result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation {target} -> {result}')
            o = S['origins']['A_stack_pz4']
            aim(unreal.Vector(-3000, 2600, o.z + 3200), unreal.Vector(-3000, 0, o.z))
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            elapsed = S['t'] - S['mark']
            if not S.get('mid_pic') and elapsed > 9.0:
                S['mid_pic'] = True
                shoot('lanes_driving')
            arrived = len(S['track']) == len(S['tanks']) and all(
                isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 400 for t in S['track'].values())
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 20.0):
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle':
            sample()
            if S['t'] - S['mark'] > 4.0:
                world = game_world()
                out = {}
                for key, (x, bp, vehicle) in LANES.items():
                    track = S['track'].get(key, {})
                    piece_alive = alive(S['pieces'][key])
                    untouched = piece_alive and track.get('changed') is None
                    went_round = track.get('cross_side') is not None and track.get('max_side_near', 0) > 150
                    stopped = track.get('cross_side') is None
                    out[key] = dict(rock=bp, vehicle=vehicle, passed=bool(untouched and (went_round or stopped)),
                                    rock_untouched=untouched, rock_alive=piece_alive, went_round=went_round,
                                    stopped_before=stopped, vehicle_crossed_y0_at_dx=track.get('cross_side'),
                                    vehicle_max_side_offset_near_rock=round(track.get('max_side_near', 0)),
                                    vehicle_min_distance_to_rock_pivot=track.get('min_dist'),
                                    vehicle_last=track.get('last'), change=track.get('changed'))
                S['result']['lanes'] = out
                S['result']['paths_after'] = {k: across_paths(world, k) for k in LANES}
                for key, row in out.items():
                    log(f'{key} verdict {json.dumps(row)}')
                    log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
                o = S['origins']['B_fallen_tiger']
                aim(o + unreal.Vector(0, -2600, 2200), o)
                S['phase'], S['mark'] = 'after_pic', S['t']
        elif phase == 'after_pic' and S['t'] - S['mark'] > 2.5:
            shoot('lanes_after')
            S['result']['all_passed'] = all(r['passed'] for r in S['result']['lanes'].values())
            log(f'ALL PASSED {S["result"]["all_passed"]}')
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 560.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
