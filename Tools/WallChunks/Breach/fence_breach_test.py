"""Throwaway-editor test of the wire fence breaches (any vehicle, any side; never the shared editor or a mission map).
Launch: BREACH_SCRIPT=fence_breach_test.py sh run_breach_test.sh <run> [timeout_s]
The setup (fence_breach_setup.configure_fence) runs first in this editor, so the test also works while another editor
holds the Blueprint (its save then fails, the compiled class in memory is what PIE uses).
Each lane is a row of 5 chained BP_Industrial_Dest_FenceWire (yaw 90: the row runs along world X, ~18 m); the vehicle
starts 24 m in front (+Y) of the middle fence and is ordered 24 m behind (-Y):
  Luchs / Pz IV / Tiger of player 1 and a Luchs of player 2 -> breach the middle fence, cross, fence loses health.
Before: every vehicle plans through the row, infantry go round. After: the gap is open for everyone.
A separate fence takes 20 mm and 75 mm test hits plus 100 damage: holes, health down, still standing.
Results: Saved/WallChunks/Breach/<run>/result.json + PNGs.
"""
import json
import os
import unreal

RUN = os.environ.get('BREACH_RUN', 'fence')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/{RUN}/'
FENCE = ('/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/'
         'BP_Industrial_Dest_FenceWire.BP_Industrial_Dest_FenceWire_C')
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
    'Puma': V + 'Ger_ArmoredCar/Puma/Ger_ArmoredCar_Puma.Ger_ArmoredCar_Puma_C',
    'Kettenkrad': V + 'Ger_ArmoredCar/Kettenkrad/Ger_ArmoredCar_Kettenkrad.Ger_ArmoredCar_Kettenkrad_C',
}
# FENCE_FROM_DISK=1: test the saved Blueprint as it is (no in-memory setup first).
FROM_DISK = os.environ.get('FENCE_FROM_DISK') == '1'
# key: (lane x, vehicle, owner). The enemy lane is far from the others: player tanks shoot enemy test tanks.
LANES = {
    'luchs': (0, 'Luchs', 1),
    'pziv': (4000, 'PzIV_G', 1),
    'tiger': (8000, 'TigerH1', 1),
    'puma': (16000, 'Puma', 1),
    'kettenkrad': (20000, 'Kettenkrad', 1),
    'enemy_luchs': (-16000, 'Luchs', 2),
}
SHOT_X = 12000
FENCE_WIDTH = 364.4
FENCES_PER_ROW = 5
# The chain-link line sits at actor-local (x=-8, y=-174..190); yawed 90 that is world (x-190..x+174, y-8).
LINE_DX, LINE_DY = -8.0, -8.0
START_Y = 2400.0
MOVE_SECONDS = 70.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'walls': {}, 'rows': {}, 'tanks': {}, 'result': {},
     'track': {}, 'pics': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('BREACHTEST ' + message)


def finish():
    try:
        os.makedirs(ROOT, exist_ok=True)
        json.dump(dict(log=S['log'], result=S['result']), open(ROOT + 'result.json', 'w'), indent=1, default=str)
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


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def row_fence_x(lane_x, index):
    return lane_x + (index - FENCES_PER_ROW // 2) * FENCE_WIDTH


def configure():
    namespace = {'__name__': 'fence_breach_library'}
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Breach/fence_breach_setup.py').read(), namespace)
    namespace['configure_fence']()
    cdo = unreal.get_default_object(unreal.load_class(None, FENCE))
    breach = cdo.get_component_by_class(unreal.WallTankBreachComponent) if hasattr(cdo, 'get_component_by_class') \
        else None
    log(f'configured; cdo breach component {breach is not None}')


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, FENCE)
    for key, (x, vehicle, owner) in LANES.items():
        for i in range(FENCES_PER_ROW):
            fx = row_fence_x(x, i)
            actors.spawn_actor_from_class(cls, unreal.Vector(fx, 0, ground_z(world, fx, 0)), unreal.Rotator(0, 0, 90))
        tx = x + LINE_DX
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(tx, START_Y, ground_z(world, tx, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed row and {vehicle} owner {owner} set {set_owner(tank, owner)}')
    actors.spawn_actor_from_class(cls, unreal.Vector(SHOT_X, 0, ground_z(world, SHOT_X, 0)), unreal.Rotator(0, 0, 90))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 12000, 2000))
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        if name == 'BP_Industrial_Dest_FenceWire_C' and abs(loc.y) < 50:
            if abs(loc.x - SHOT_X) < 50:
                S['shot_fence'] = actor
            for key, (x, _, _) in LANES.items():
                for i in range(FENCES_PER_ROW):
                    if abs(loc.x - row_fence_x(x, i)) < 50:
                        S['rows'].setdefault(key, {})[i] = actor
                        if i == FENCES_PER_ROW // 2:
                            S['walls'][key] = actor
        if name.startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    capture = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    comp = capture.capture_component2d
    S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                             unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property('texture_target', S['rt'])
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    comp.set_editor_property('fov_angle', 70.0)
    capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    S['capture'] = capture
    log(f'runtime walls {sorted(S["walls"])} rows {[len(r) for r in S["rows"].values()]} tanks {sorted(S["tanks"])} '
        f'shot fence {"shot_fence" in S}')


def wall_mesh(wall):
    return [c for c in wall.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def actor_state(wall):
    if not alive(wall):
        return dict(gone=True)
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    mesh = wall_mesh(wall)
    health = wall.get_editor_property('health')
    return dict(holes=holes.get_hole_count(), breaches=holes.get_breach_count(),
                removed=round(holes.get_removed_face_share(), 3), health=round(health), alive=bool(health > 0.0),
                mesh=mesh.static_mesh.get_name() if mesh.static_mesh else None,
                visible=bool(mesh.is_visible()),
                material=mesh.get_material(0).get_name() if mesh.get_material(0) else None,
                breach_comp=wall.get_component_by_class(unreal.WallTankBreachComponent) is not None,
                any_vehicle=_any_vehicle(wall), field=_field(wall))


def _any_vehicle(wall):
    comp = wall.get_component_by_class(unreal.WallTankBreachComponent)
    try:
        return bool(comp.get_editor_property('settings').get_editor_property('any_vehicle_breaks')) if comp else None
    except Exception as error:  # noqa: BLE001
        return repr(error)[:60]


def _field(wall):
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    try:
        return bool(holes.get_is_field_mode())
    except Exception:  # noqa: BLE001
        return None


def wall_state(key):
    return actor_state(S['walls'].get(key))


def neighbours_state(key):
    row = S['rows'].get(key, {})
    return {i: actor_state(a).get('breaches', 'gone') for i, a in sorted(row.items()) if i != FENCES_PER_ROW // 2}


def navmeshes(world):
    out = {}
    navs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
    for nav in navs:
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in navs if n not in out.values()]
    if rest:
        out['Character'] = rest[0]
    return out


def path(world, a, b, nav_data, filter_class):
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data, filter_class)
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:100])
    if not pts:
        return dict(length=None)
    side = max(abs(pt.x - a.x) for pt in pts)
    return dict(length=round(sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))),
                partial=bool(p.is_partial()), side_cm=round(side), through=bool(side < 260 and not p.is_partial()))


def across_paths(world, key):
    x = LANES[key][0] + LINE_DX
    z = S['ground'][key] + 20
    a, b = unreal.Vector(x, 900, z), unreal.Vector(x, -900, z)
    queriers = {'PlayerHeavy': ('HeavyTank', S['tanks'].get('tiger')),
                'PlayerMedium': ('MediumTank', S['tanks'].get('pziv')),
                'PlayerLight': ('LightTank', S['tanks'].get('luchs')),
                'EnemyLight': ('LightTank', S['tanks'].get('enemy_luchs')),
                'Character': ('Character', None)}
    out = {}
    # A planner without a unit (convoy routes, strategic AI): fences are plain ground, as before.
    out['Planner'] = path(world, a, b, S['nav']['LightTank'], unreal.RTSClosedBreachableWallsQueryFilter)
    for label, (agent, unit) in queriers.items():
        context = unit.get_controller() if unit is not None and alive(unit) else None
        out[label] = path(world, a, b, context if context is not None else S['nav'][agent],
                          unreal.load_class(None, FILTERS[agent]))
        out[label]['querier'] = 'unit' if context is not None else 'navdata'
    return out


def aim_and_shoot(name, eye, look):
    capture = S['capture']
    capture.set_actor_location(eye, False, True)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    capture.capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), name + '.png')
    log(f'picture {name}')


def lane_view(x, z, side=1.0):
    return unreal.Vector(x + 900, side * 1300, z + 650), unreal.Vector(x, 0, z + 200)


def queue_picture(name, at, eye, look):
    S['pics'].append((at, name, eye, look))
    S['pics'].sort(key=lambda row: row[0])


def take_due_pictures():
    while S['pics'] and S['t'] >= S['pics'][0][0]:
        _, name, eye, look = S['pics'].pop(0)
        aim_and_shoot(name, eye, look)


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0] + LINE_DX
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, last=None, breach_at=None,
                                                states=[], min_speed_near=None))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 500:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
                speed = tank.get_velocity().length()
                track['min_speed_near'] = round(speed if track['min_speed_near'] is None
                                                else min(track['min_speed_near'], speed))
            if track['cross_side'] is None and loc.y < -500:
                track['cross_side'] = round(loc.x - x)
                track['cross_time'] = round(S['t'] - S['order_time'], 1)
        state = wall_state(key)
        if state.get('breaches', 0) > 0 and track['breach_at'] is None:
            track['breach_at'] = round(S['t'] - S['order_time'], 2)
            log(f'{key}: first breach {json.dumps(state)} tank at {track["last"]}')
            eye, look = lane_view(LANES[key][0], S['ground'][key])
            queue_picture(f'{key}_push', S['t'] + 0.2, eye, look)
            queue_picture(f'{key}_fallen', S['t'] + 1.0, eye, look)
            queue_picture(f'{key}_after', S['t'] + 6.0, eye, look)
            queue_picture(f'{key}_after_back', S['t'] + 6.2, lane_view(LANES[key][0], S['ground'][key], -1.0)[0],
                          look)
        if len(track['states']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.5:
            track['sample_t'] = S['t']
            track['states'].append((round(S['t'] - S['order_time'], 1), state.get('breaches'), state.get('removed'),
                                    state.get('health'), track['last']))


def shot_test():
    fence = S.get('shot_fence')
    holes = fence.get_component_by_class(unreal.WallHoleDamageComponent)
    z = ground_z(game_world(), SHOT_X, 1500)
    removed = holes.debug_fire_test_hit(unreal.Vector(SHOT_X - 80, 30, z + 150), unreal.Vector(0, -1, 0), 20)
    removed += holes.debug_fire_test_hit(unreal.Vector(SHOT_X + 60, 30, z + 320), unreal.Vector(0, -1, 0), 75)
    unreal.GameplayStatics.apply_damage(fence, 100.0, None, None, unreal.DamageType)
    state = actor_state(fence)
    # 20 mm cuts a 17 cm hole, under MinHoleRadius (also in slot mode): health only. 75 mm cuts a hole.
    ok = bool(state.get('holes', 0) >= 1 and state.get('alive') and state.get('health', 500) <= 25)
    S['result']['shot'] = dict(removed=removed, state=state, passed=ok)
    log(f'shot test {json.dumps(S["result"]["shot"], default=str)}')
    eye, look = lane_view(SHOT_X, z)
    queue_picture('shot_fence', S['t'] + 0.5, eye, look)


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        step()
    finally:
        S['busy'] = False


def step():
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'
            if FROM_DISK:
                log('testing the Blueprint as saved on disk')
            else:
                configure()
            place_all()
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(editor_world, 'RebuildNavigation')
            S['phase'], S['mark'] = 'editor_nav', S['t']
        elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if nav_building(editor_world) and S['t'] - S['mark'] < 120.0:
                return
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
                find_runtime(world)
                S['ground'] = {k: ground_z(world, LANES[k][0], 1500) for k in LANES}
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 7.0:
                return
            S['nav'] = navmeshes(world)
            S['result']['setup'] = {k: wall_state(k) for k in LANES}
            S['result']['paths_before'] = {k: across_paths(world, k) for k in LANES}
            for key in LANES:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            shot_test()
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                x = LANES[key][0] + LINE_DX
                target = unreal.Vector(x, -START_Y, S['ground'][key] + 20)
                try:
                    result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation -> {result}')
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            take_due_pictures()
            elapsed = S['t'] - S['mark']
            arrived = all(isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 500
                          for t in S['track'].values()) and len(S['track']) == len(S['tanks'])
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 15.0 and not S['pics']):
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle':
            take_due_pictures()
            world = game_world()
            if S['t'] - S['mark'] < 4.0 or S['pics'] or (nav_building(world) and S['t'] - S['mark'] < 30.0):
                return
            out = {}
            for key, (x, vehicle, owner) in LANES.items():
                track = S['track'].get(key, {})
                state = wall_state(key)
                crossed = track.get('cross_side') is not None
                before = S['result']['paths_before'][key]
                after = across_paths(world, key)
                S['result'].setdefault('paths_after', {})[key] = after
                breached = state.get('breaches', 0) > 0 or (track.get('breach_at') is not None)
                damaged = bool(state.get('gone') or not state.get('alive') or state.get('health', 500) < 500)
                vehicles_through = all(before[q].get('through') for q in ('PlayerHeavy', 'PlayerMedium',
                                                                          'PlayerLight', 'EnemyLight'))
                infantry_round = not before['Character'].get('through')
                ok = bool(breached and damaged and crossed and abs(track['cross_side']) < 300 and vehicles_through
                          and infantry_round and after['Character'].get('through')
                          and before['Planner'].get('through'))
                out[key] = dict(vehicle=vehicle, owner=owner, passed=ok, breached=breached, damaged=damaged,
                                crossed=crossed, cross_dx=track.get('cross_side'), cross_time=track.get('cross_time'),
                                breach_at=track.get('breach_at'), max_side_near=round(track.get('max_side_near', 0)),
                                min_speed_near=track.get('min_speed_near'), vehicles_through=vehicles_through,
                                infantry_round=infantry_round, last=track.get('last'), end_state=state,
                                neighbours=neighbours_state(key), states=track.get('states'))
                log(f'{key} verdict {json.dumps({k: v for k, v in out[key].items() if k != "states"})}')
                log(f'{key} paths after {json.dumps(after)}')
            S['result']['lanes'] = out
            shot = S['result'].get('shot', {})
            shot['end_state'] = actor_state(S.get('shot_fence'))
            lanes_ok = all(r['passed'] for r in out.values())
            S['result']['all_passed'] = bool(lanes_ok and shot.get('passed'))
            log(f'lanes_ok {lanes_ok} shot_ok {shot.get("passed")} shot_end {json.dumps(shot["end_state"])}')
            log(f'ALL PASSED {S["result"]["all_passed"]}')
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 420.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
