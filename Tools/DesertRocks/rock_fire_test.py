"""Throwaway-editor test of the desert rocks (ue_blueprints.py): real tank fire, holes, death, nav.
Derived from Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py (same phases). Never in the shared editor; the map
(RTS_ToCopySmall) is never saved; the editor quits at the end. Launch: sh fire_test_run.sh <logname> [timeout_s].
Rocks stand in a row along X at y 0 (yaw 90), one player Pz IV G 35 m in front of each (+Y, sunlit side).
Results: Saved/DesertRocks/test/result.json + PNGs.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/test/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'
TANK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C'
# key: (x position, camera eye offset, look offset) in world space; ruins face +Y.
import os
_ONLY = [k for k in os.environ.get('DROCK_ONLY', '').split(',') if k]
# one large slot and one medium slot per run: DROCK_ONLY=<L rock>,<M rock> (default L1_Block,M4_Cracked)
_SLOT = {'L': (-3000, (1000, 2300, 900), (0, 0, 170)), 'M': (2000, (450, 1200, 500), (0, 0, 70))}
_ONLY = _ONLY or ['L1_Block', 'M4_Cracked']
_ALL = {k: _SLOT[k[0]] for k in _ONLY}
RUINS = {k: v for k, v in _ALL.items() if not _ONLY or k in _ONLY}
# B8 is rubble only (no hole material): it only gets debug hits and the kill, no tank and no pictures.
NO_TANK = ()
# A ruin whose health has not moved this long into the fire phase gets AttackGround on its first aim point instead.
ATTACK_GROUND_AFTER = 12.0
TANK_Y = 3500.0
FIRE_SECONDS = 36.0
# Screenshots land a frame or more after the request (the throwaway editor renders slowly): the camera must stay put
# this long after each request, or the picture shows the next view.
PICTURE_HOLD = 2.5
KILL = list(RUINS)
DEBUG_CALIBRES = [75, 88, 75, 105, 75]
PIECE_HEALTH = {}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'ruins': {}, 'tanks': {}, 'pics': [], 'hp': {},
     'first_hole': {}, 'kills': {}}


def hp(actor):
    comp = actor.get_component_by_class(unreal.HealthComponent)
    for name in ('get_current_health', 'GetCurrentHealth'):
        fn = getattr(comp, name, None)
        if fn:
            return float(fn())
    return float(comp.get_editor_property('CurrentHealth'))


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('DROCKTEST ' + message)


def finish():
    try:
        json.dump(S['log'], open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def alive(actor):
    """IsValid that also copes with an actor Python already lost (destroyed and collected)."""
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


def nav_boxes_of(actor):
    return [c for c in actor.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock')]


# ---------------------------------------------------------------- setup

def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, _, _) in RUINS.items():
        z = ground_z(world, x, 0)
        cls = unreal.load_class(None, f'{BP_DIR}BP_DesertRock_{key}.BP_DesertRock_{key}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 90))
        if key in NO_TANK:
            log(f'placed {key} at ({x},0,{z:.0f}), no tank')
            continue
        tz = ground_z(world, x, TANK_Y)
        tank = actors.spawn_actor_from_class(unreal.load_class(None, TANK), unreal.Vector(x, TANK_Y, tz + 50),
                                             unreal.Rotator(0, 0, -90))
        rts = tank.get_component_by_class(unreal.RTSComponent) if tank else None
        if rts is not None:
            rts.set_editor_property('owning_player', 1)
        log(f'placed {key} at ({x},0,{z:.0f}) and a tank at ({x},{TANK_Y:.0f},{tz:.0f})')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        for key in RUINS:
            if name == f'BP_DesertRock_{key}_C':
                S['ruins'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        location = tank.get_actor_location()
        for key, (x, _, _) in RUINS.items():
            if abs(location.x - x) < 300 and abs(location.y - TANK_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    log(f'runtime ruins {sorted(S["ruins"])} tanks {sorted(S["tanks"])}')
    for key, ruin in S['ruins'].items():
        PIECE_HEALTH[key] = hp(ruin)


def report_setup(key, actor):
    mesh = mesh_of(actor)
    channel = unreal.CollisionChannel
    responses = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].rstrip('>').split(':')[0]
                 for n, c in (('Projectile', channel.ECC_PROJECTILE), ('TraceEnemy', channel.ECC_ENEMY),
                              ('TracePlayer', channel.ECC_PLAYER), ('Visibility', channel.ECC_VISIBILITY),
                              ('Pawn', channel.ECC_PAWN), ('BuildingPlacement', channel.ECC_BUILDING_PLACEMENT))}
    boxes = nav_boxes_of(actor)
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    aims = [(round(v.x), round(v.y), round(v.z)) for v in actor.get_editor_property('AimOffsetPoints')]
    log(f'{key}: health {hp(actor):.0f}'
        f' aims {aims}')
    log(f'{key}: mesh objtype {mesh.get_collision_object_type()} enabled {mesh.get_collision_enabled()} responses {responses}'
        f' affectsNav {mesh.get_editor_property("can_ever_affect_navigation")} mobility {mesh.get_editor_property("mobility")}'
        f' parent {mesh.get_attach_parent().get_name() if mesh.get_attach_parent() else None}'
        f' root {actor.root_component.get_name()} slots {[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())]}')
    if boxes:
        b = boxes[0]
        log(f'{key}: {len(boxes)} nav boxes; first dynObstacle {b.get_editor_property("dynamic_obstacle")} area '
            f'{b.get_editor_property("area_class_override")} collision {b.get_collision_enabled()} '
            f'affectsNav {b.get_editor_property("can_ever_affect_navigation")} visible {b.is_visible()}')
    if holes:
        for name in ('HoleSettings', 'GridSettings', 'CollapseSettings'):
            log(f'{key}: {name} {holes.get_editor_property(name).export_text()[:260]}')


def on_navmesh(world, point, extent=(25.0, 25.0, 250.0)):
    nav = unreal.NavigationSystemV1
    fn = getattr(nav, 'k2_project_point_to_navigation', None) or getattr(nav, 'project_point_to_navigation', None)
    if fn is None:
        return 'ERR no project_point_to_navigation in ' + str([n for n in dir(nav) if 'project' in n.lower()])
    try:
        result = fn(world, point, None, None, unreal.Vector(*extent))
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'
    if isinstance(result, tuple):
        return bool(result[0])
    if isinstance(result, bool):
        return result
    return result is not None


def nav_points(actor):
    """World points at the centre of every nav box (expect off the navmesh) and on a ring of open ground 2.5 m outside
    the boxes' union plus interior grid points at least 2.5 m from every box (expect on)."""
    # Points sit at the actor's ground height (the test map is flat): a downward trace would land on top of the ruin.
    inside, open_points = [], []
    transform = actor.get_actor_transform()
    boxes = nav_boxes_of(actor)
    local = []
    for box in boxes:
        centre = transform.inverse_transform_location(box.get_world_location())
        extent = box.get_unscaled_box_extent()
        local.append((centre.x - extent.x, centre.y - extent.y, centre.x + extent.x, centre.y + extent.y))
        # The centre plus points along the box's long axis, 25 cm in from its ends.
        long_x = extent.x >= extent.y
        half = max(extent.x, extent.y) - 25.0
        count = max(1, int(half * 2 // 150))
        samples = [0.0] + [(-half + i * (2 * half) / count) for i in range(count + 1)] if half > 0 else [0.0]
        inside.append([transform.transform_location(unreal.Vector(centre.x + (s if long_x else 0), centre.y + (0 if long_x else s), 20))
                       for s in samples])
    if not local:
        return inside, open_points
    x0, y0 = min(b[0] for b in local), min(b[1] for b in local)
    x1, y1 = max(b[2] for b in local), max(b[3] for b in local)
    step = 150.0
    x = x0 - 250
    while x <= x1 + 250:
        y = y0 - 250
        while y <= y1 + 250:
            clear = all(not (b[0] - 250 <= x <= b[2] + 250 and b[1] - 250 <= y <= b[3] + 250) for b in local)
            if clear:
                open_points.append(transform.transform_location(unreal.Vector(x, y, 20)))
            y += step
        x += step
    return inside, open_points


def heavy_navmesh(world):
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        if 'HeavyTank' in nav.get_name():
            return nav
    return None


def path_across(world, ends, nav_data=None):
    """Path length from 4 m in front of the ruin to 4 m behind it, through its middle, against the straight line."""
    a, b = ends
    try:
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data)
        points = list(path.get_editor_property('path_points')) if path else []
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'[:80]
    if not points:
        return 'no path'
    length = sum((points[i + 1] - points[i]).length() for i in range(len(points) - 1))
    return f'{length:.0f} cm (straight {(b - a).length():.0f}, partial {path.is_partial()})'


def across_ends(actor):
    transform = actor.get_actor_transform()
    boxes = nav_boxes_of(actor)
    reach = max([abs(transform.inverse_transform_location(b.get_world_location()).x) + b.get_unscaled_box_extent().x
                 for b in boxes] or [500.0]) + 400.0
    return (transform.transform_location(unreal.Vector(reach, 0, 20)), transform.transform_location(unreal.Vector(-reach, 0, 20)))


def nav_check(world, key, actor_or_points, label):
    inside, open_points = nav_points(actor_or_points) if not isinstance(actor_or_points, tuple) else actor_or_points
    extent = (20.0, 20.0, 80.0)
    per_box = [[on_navmesh(world, p, extent) for p in samples] for samples in inside]
    open_on = [on_navmesh(world, p, extent) for p in open_points]
    flat = [v for box in per_box for v in box] + open_on
    on_in = sum(1 for box in per_box for v in box if v is True)
    total_in = sum(len(box) for box in per_box)
    leaky = [f'box{i}:{sum(1 for v in box if v is True)}/{len(box)}' for i, box in enumerate(per_box) if any(v is True for v in box)]
    path = path_across(world, S['across'][key]) if key in S.get('across', {}) else 'n/a'
    heavy = path_across(world, S['across'][key], heavy_navmesh(world)) if key in S.get('across', {}) else 'n/a'
    log(f'nav {label} {key}: points inside nav boxes on navmesh {on_in}/{total_in} {leaky}; open points on navmesh '
        f'{sum(1 for v in open_on if v is True)}/{len(open_points)}; path across {path}; HeavyTank path {heavy}; '
        f'nav building {nav_building(world)}'
        + (f' (errors {[v for v in flat if isinstance(v, str)][:1]})' if any(isinstance(v, str) for v in flat) else ''))
    return inside, open_points


# ---------------------------------------------------------------- fire

def order_fire(key):
    tank, ruin = S['tanks'].get(key), S['ruins'].get(key)
    if key in NO_TANK:
        return
    if tank is None or ruin is None:
        log(f'{key}: no tank or ruin to fire')
        return
    attempts = []
    for how in ('method', 'interface', 'call_method'):
        try:
            if how == 'method':
                result = tank.attack_actor(ruin, True)
            elif how == 'interface':
                result = unreal.Commands.attack_actor(tank, ruin, True)
            else:
                result = tank.call_method('AttackActor', (ruin, True))
            attempts.append(f'{how}={result}')
            log(f'{key}: AttackActor via {how} -> {result}')
            if 'NO_ERROR' in str(result).upper() or 'NOERROR' in str(result).upper():
                return
            break
        except Exception as error:  # noqa: BLE001
            attempts.append(f'{how} failed {error!r}'[:160])
    log(f'{key}: AttackActor attempts {attempts}; falling back to AttackGround on the first aim point')
    aim = ruin.get_actor_transform().transform_location(ruin.get_editor_property('AimOffsetPoints')[0])
    try:
        result = tank.attack_ground(aim, True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'
    log(f'{key}: AttackGround -> {result}')


def watch(world):
    for key, ruin in S['ruins'].items():
        if not alive(ruin):
            continue
        holes = ruin.get_component_by_class(unreal.WallHoleDamageComponent)
        state = (round(hp(ruin)), holes.get_hole_count() if holes else -1)
        if S['hp'].get(key) != state:
            S['hp'][key] = state
            log(f'{key}: health {state[0]} holes {state[1]}')
            if state[1] > 0 and key not in S['first_hole']:
                S['first_hole'][key] = mesh_of(ruin).get_material(0).get_name()
                log(f'{key}: first hole -> slot0 {S["first_hole"][key]}')
    track = S.setdefault('proj', {})
    for projectile in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Projectile):
        name = projectile.get_name()
        hidden = projectile.get_editor_property('hidden')
        location = projectile.get_actor_location()
        entry = track.get(name)
        if entry is None or (entry['hidden'] and not hidden):
            # A (pooled) shell starts a flight: attribute it to the nearest tank.
            owner = min(S['tanks'], key=lambda k: (S['tanks'][k].get_actor_location() - location).length()) if S['tanks'] else None
            S.setdefault('shots', {}).setdefault(owner, []).append(None)
            entry = {'owner': owner, 'hidden': hidden, 'last': location}
            track[name] = entry
        if hidden and not entry['hidden']:
            # Went dormant: the flight ended at its last visible location.
            shots = S['shots'][entry['owner']]
            if shots and shots[-1] is None:
                shots[-1] = entry['last']
        entry['hidden'], entry['last'] = hidden, location


def debug_hits(key):
    ruin = S['ruins'][key]
    mesh = mesh_of(ruin)
    holes = ruin.get_component_by_class(unreal.WallHoleDamageComponent)
    transform = ruin.get_actor_transform()
    for index, aim in enumerate(ruin.get_editor_property('AimOffsetPoints')):
        target = transform.transform_location(aim)
        yaw = math.radians([90, 70, 110, 80, 100][index % 5])
        start = target + unreal.Vector(math.cos(yaw) * 3000, math.sin(yaw) * 3000, 200 - aim.z * 0.2)
        direction = (target - start).normal()
        hit = mesh.line_trace_component(start, start + direction * 6000, False, False, False)
        if not hit:
            log(f'{key} debug aim {index}: missed the collision')
            continue
        complex_ = mesh.line_trace_component(start, start + direction * 6000, True, False, False)
        gap = ((complex_[0] - start).length() - (hit[0] - start).length()) if complex_ else -1
        removed = holes.debug_fire_test_hit(hit[0], direction, float(DEBUG_CALIBRES[index % 5]))
        local = transform.inverse_transform_location(hit[0])
        log(f'{key} DEBUG hit aim {index} at ({local.x:.0f},{local.y:.0f},{local.z:.0f}) gap {gap:.0f} cm removed {removed:.2f} m2')
    log(f'{key}: after debug hits holes {holes.get_hole_count()} slot0 {mesh.get_material(0).get_name()}')


# ---------------------------------------------------------------- pictures

def place_camera(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def view_of(key, origin=None):
    x, eye, look = RUINS[key]
    base = origin or unreal.Vector(x, 0, 0)
    return base + unreal.Vector(*eye), base + unreal.Vector(*look)


def close_view(key):
    """Oblique close-up of the first aim point from 9 m, 35 degrees off the front."""
    ruin = S['ruins'][key]
    target = ruin.get_actor_transform().transform_location(ruin.get_editor_property('AimOffsetPoints')[0])
    eye = target + unreal.Vector(math.sin(math.radians(35)) * 900, math.cos(math.radians(35)) * 900, 150)
    return eye, target


def queue_pictures(prefix, keys, close=()):
    for key in keys:
        if key in NO_TANK:
            continue
        if key in S['ruins'] and alive(S['ruins'][key]):
            S['origins'] = S.get('origins', {})
            S['origins'][key] = S['ruins'][key].get_actor_location()
            S['pics'].append((f'{prefix}_{key}', view_of(key, S['origins'][key])))
    for key in close:
        if key in S['ruins'] and alive(S['ruins'][key]):
            S['pics'].append((f'{prefix}_{key}_close', close_view(key)))


def picture(name):
    S['busy'] = True
    try:
        unreal.SystemLibrary.execute_console_command(game_world(), 'ShowFlag.PostProcessMaterial 0')
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def run_pictures(next_phase):
    """Returns True while pictures are pending; call each tick."""
    if S['t'] - S['mark'] < (1.2 if S.get('pending') else PICTURE_HOLD):
        return True
    if S.get('pending'):
        name = S.pop('pending')
        S['mark'] = S['t']
        picture(name)
        return True
    if S['pics']:
        name, (eye, look) = S['pics'].pop(0)
        place_camera(eye, look)
        S['pending'], S['mark'] = name, S['t']
        return True
    S['phase'], S['mark'] = next_phase, S['t']
    return False


# ---------------------------------------------------------------- main loop

def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            # The PIE navmesh is a copy of the editor one: rebuild it with the ruins in, as a map build would.
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
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                find_runtime(world)
                for key, ruin in S['ruins'].items():
                    report_setup(key, ruin)
                # A throwaway first picture: the first screenshot of a session came out with the next camera's view.
                S['pics'].append(('_warmup', view_of(list(RUINS)[0])))
                queue_pictures('undamaged', list(RUINS), close=tuple(RUINS))
                S['phase'], S['mark'] = 'pics_undamaged', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'pics_undamaged':
            run_pictures('nav_alive')
        elif phase == 'nav_alive':
            # The PIE navmesh rebuilds on load (bForceRebuildOnLoad); check it once the build is done.
            world = game_world()
            if nav_building(world) and S['t'] - S['mark'] < 40.0:
                return
            log(f'nav build done after {S["t"] - S["mark"]:.1f} s (building {nav_building(world)})')
            S['across'] = {key: across_ends(ruin) for key, ruin in S['ruins'].items()}
            S['navpts'] = {key: nav_check(world, key, ruin, 'alive') for key, ruin in S['ruins'].items()}
            S['phase'], S['mark'] = 'fire', S['t']
        elif phase == 'fire':
            for key in RUINS:
                order_fire(key)
            S['phase'], S['mark'] = 'firing', S['t']
        elif phase == 'firing':
            watch(game_world())
            if S['t'] - S['mark'] > ATTACK_GROUND_AFTER and not S.get('ground_ordered'):
                S['ground_ordered'] = True
                for key, ruin in S['ruins'].items():
                    if key in NO_TANK or not alive(ruin) or key not in S['tanks']:
                        continue
                    cur_hp = hp(ruin)
                    start_hp = PIECE_HEALTH.get(key)
                    if start_hp is not None and cur_hp >= start_hp:
                        aim = ruin.get_actor_transform().transform_location(ruin.get_editor_property('AimOffsetPoints')[0])
                        try:
                            result = S['tanks'][key].attack_ground(aim, True)
                        except Exception as error:  # noqa: BLE001
                            result = repr(error)[:80]
                        log(f'{key}: no damage after {ATTACK_GROUND_AFTER:.0f} s of AttackActor -> AttackGround on aim 0: {result}')
            if S['t'] - S['mark'] > FIRE_SECONDS:
                for owner, shots in S.get('shots', {}).items():
                    ruin = S['ruins'].get(owner)
                    ends = []
                    for end in [e for e in shots if e is not None][:6]:
                        local = ruin.get_actor_transform().inverse_transform_location(end) if alive(ruin) else end
                        ends.append((round(local.x), round(local.y), round(local.z)))
                    log(f'{owner} tank: {len(shots)} projectiles (nearest-tank attribution, includes MG); first flight ends '
                        f'in ruin space {ends}')
                for key, tank in S['tanks'].items():
                    if alive(tank):
                        loc = tank.get_actor_location()
                        log(f'{key} tank at ({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) yaw {tank.get_actor_rotation().yaw:.0f}')
                for key, ruin in S['ruins'].items():
                    if not alive(ruin):
                        log(f'{key}: died under fire')
                        continue
                    holes = ruin.get_component_by_class(unreal.WallHoleDamageComponent)
                    log(f'{key}: after real fire holes {holes.get_hole_count()} health '
                        f'{hp(ruin):.0f} slot0 {mesh_of(ruin).get_material(0).get_name()}')
                    if holes.get_hole_count() < 2 or key in NO_TANK:
                        debug_hits(key)
                for tank in S['tanks'].values():
                    try:
                        tank.stop_command()
                    except Exception:  # noqa: BLE001
                        pass
                queue_pictures('holes', list(RUINS), close=tuple(RUINS))
                S['phase'], S['mark'] = 'pics_holes', S['t']
        elif phase == 'pics_holes':
            watch(game_world())
            run_pictures('kill')
        elif phase == 'kill':
            S['kill_queue'] = [k for k in KILL if k in S['ruins'] and alive(S['ruins'][k])]
            S['phase'], S['mark'] = 'kill_next', S['t']
        elif phase == 'kill_next':
            if not S['kill_queue']:
                S['phase'], S['mark'] = 'nav_after', S['t']
                return
            key = S['kill_queue'].pop(0)
            ruin = S['ruins'][key]
            S['kills'][key] = nav_points(ruin)
            origin = ruin.get_actor_location()
            eye, look = view_of(key, origin)
            place_camera(eye, look)
            unreal.GameplayStatics.apply_damage(ruin, 1.0e7, None, None, unreal.DamageType)
            log(f'killed {key}: valid {alive(ruin)} lifespan {ruin.get_life_span():.1f} '
                f'mesh visible {mesh_of(ruin).is_visible()} collision {mesh_of(ruin).get_collision_enabled()}')
            S['death_key'], S['death_shots'] = key, [0.4, 2.0]
            S['phase'], S['mark'] = 'death_pics', S['t']
        elif phase == 'death_pics':
            if S['death_shots'] and S['t'] - S['mark'] >= S['death_shots'][0]:
                at = S['death_shots'].pop(0)
                picture(f'death_{S["death_key"]}_{str(at).replace(".", "_")}s')
            elif not S['death_shots'] and S['t'] - S['mark'] > 2.0 + PICTURE_HOLD:
                S['phase'], S['mark'] = 'kill_next', S['t']
        elif phase == 'nav_after' and S['t'] - S['mark'] > 9.0:
            world = game_world()
            for key, points in S.get('navpts', {}).items():
                ruin = S['ruins'][key]
                state = 'alive_end' if alive(ruin) else 'dead'
                log(f'{key}: actor valid 9 s after the last kill: {alive(ruin)}')
                nav_check(world, key, points, state)
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 420.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
