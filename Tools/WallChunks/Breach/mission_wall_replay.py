"""Read-only replay of the Rus wall breaches on a real mission map (default Ger_IntroMission) in a throwaway DebugGame
editor: nothing is saved. Launch:
    BREACH_SCRIPT=mission_wall_replay.py BREACH_MAP=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission \
        sh run_breach_test.sh <run> [timeout_s]
In PIE: every Rus wall's runtime setup (field mode, breach component, owner, nav), errors from the wall code, then a
player Tiger spawned 15 m in front of one Soviet wall with open ground on both sides and ordered 15 m past it.
Results: Saved/WallChunks/Breach/<run>/result.json + PNGs.
"""
import json
import os
import unreal

RUN = os.environ.get('BREACH_RUN', 'mission')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/{RUN}/'
TIGER = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/'
         'Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C')
HEAVY_FILTER = '/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_HeavyTanks.RTS_NavFilter_HeavyTanks_C'
STAND_OFF = 1500.0
S = {'phase': 'wait_pie', 't': 0.0, 'mark': 0.0, 'log': [], 'result': {}}


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


def ground(world, x, y, z):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 3000), unreal.Vector(x, y, z - 3000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else None


def wall_info(wall):
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    breach = wall.get_component_by_class(unreal.WallTankBreachComponent)
    rts = wall.get_component_by_class(unreal.RTSComponent)
    mesh = [c for c in wall.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh']
    navs = sorted(set(c.get_editor_property('area_class').get_name()
                      for c in wall.get_components_by_class(unreal.NavModifierComponent)
                      if c.get_editor_property('area_class')))
    return dict(cls=wall.get_class().get_name(), owner=rts.get_editor_property('owning_player') if rts else None,
                holes=holes.get_hole_count() if holes else None, breach=breach is not None,
                material=mesh[0].get_material(0).get_name() if mesh and mesh[0].get_material(0) else None,
                health=round(wall.get_editor_property('health')), nav_areas=navs,
                loc=[round(v) for v in wall.get_actor_location().to_tuple()])


def pick_wall(world, walls):
    """A Soviet wall with ground on both sides at the wall's height (actor Y is across the wall)."""
    for wall in walls:
        info = wall_info(wall)
        if not info['breach'] or info['owner'] == 1:
            continue
        origin, extent = wall.get_actor_bounds(False)
        across = wall.get_actor_right_vector()
        front = origin + across * STAND_OFF
        back = origin - across * STAND_OFF
        zf, zb = ground(world, front.x, front.y, origin.z), ground(world, back.x, back.y, origin.z)
        z0 = origin.z - extent.z
        if zf is not None and zb is not None and abs(zf - z0) < 250 and abs(zb - z0) < 250:
            return wall, unreal.Vector(front.x, front.y, zf), unreal.Vector(back.x, back.y, zb), origin
    return None


def shoot(name, eye, look):
    capture = S['capture']
    capture.set_actor_location(eye, False, True)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    capture.capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), name + '.png')
    log(f'picture {name}')


def make_capture(world):
    transform = unreal.Transform(unreal.Vector(0, 0, 3000), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
    capture = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(world, unreal.SceneCapture2D, transform)
    unreal.GameplayStatics.finish_spawning_actor(capture, transform)
    comp = capture.capture_component2d
    S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                             unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property('texture_target', S['rt'])
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('fov_angle', 70.0)
    S['capture'] = capture


def heavy_path(world, a, b, querier):
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, querier,
                                                                         unreal.load_class(None, HEAVY_FILTER))
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:120])
    if not pts:
        return dict(length=None)
    return dict(length=round(sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))),
                straight=round((b - a).length()), partial=bool(p.is_partial()))


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        step()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {S["phase"]}: {error!r}')
        finish()
    finally:
        S['busy'] = False
    if S['t'] > 400.0 and S['phase'] != 'done':
        log('timeout')
        finish()


def step():
    phase = S['phase']
    if phase == 'wait_pie' and S['t'] > 25.0:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'pie_started', S['t']
    elif phase == 'pie_started' and game_world() is not None and S['t'] - S['mark'] > 25.0:
        world = game_world()
        unreal.GameplayStatics.set_game_paused(world, False)
        walls = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                 if a.get_class().get_name().startswith('Rus_Wall_Concrete')]
        S['result']['map'] = world.get_path_name()
        S['result']['walls'] = {w.get_name(): wall_info(w) for w in walls}
        log(f'{len(walls)} Rus walls on {world.get_path_name()}')
        for name, info in S['result']['walls'].items():
            log(f'wall {name} {json.dumps(info)}')
        make_capture(world)
        pick = pick_wall(world, walls)
        if pick is None:
            log('no Soviet wall with open ground on both sides')
            finish()
            return
        wall, front, back, origin = pick
        S.update(wall=wall, front=front, back=back, origin=origin)
        yaw = unreal.MathLibrary.find_look_at_rotation(front, back).yaw
        transform = unreal.Transform(front + unreal.Vector(0, 0, 60), unreal.Rotator(0, 0, yaw), unreal.Vector(1, 1, 1))
        tank = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(world, unreal.load_class(None, TIGER),
                                                                            transform)
        rts = tank.get_component_by_class(unreal.RTSComponent)
        if rts is not None:
            rts.set_editor_property('owning_player', 1)
        unreal.GameplayStatics.finish_spawning_actor(tank, transform)
        S['tank'] = tank
        log(f'picked {wall.get_name()} at {[round(v) for v in origin.to_tuple()]}; Tiger at {front}')
        S['phase'], S['mark'] = 'spawned', S['t']
    elif phase == 'spawned' and S['t'] - S['mark'] > 6.0:
        world = game_world()
        tank = S['tank']
        S['result']['heavy_path_before'] = heavy_path(world, S['front'], S['back'], tank.get_controller())
        log(f'heavy path before {json.dumps(S["result"]["heavy_path_before"])}')
        eye = S['origin'] + S['wall'].get_actor_right_vector() * 1800 + S['wall'].get_actor_forward_vector() * 900 \
            + unreal.Vector(0, 0, 900)
        S['eye'] = eye
        shoot('mission_before', eye, S['origin'])
        log(f'MoveToLocation -> {tank.move_to_location(S["back"], True, unreal.Rotator(0, 0, 0), False)}')
        S['phase'], S['mark'] = 'moving', S['t']
        S['pics'] = [(S['t'] + t, f'mission_{n}') for t, n in ((5.0, 't5'), (8.0, 't8'), (14.0, 't14'))]
    elif phase == 'moving':
        if S['pics'] and S['t'] >= S['pics'][0][0]:
            shoot(S['pics'].pop(0)[1], S['eye'], S['origin'])
        if S['t'] - S['mark'] > 30.0 and not S['pics']:
            tank = S['tank']
            loc = tank.get_actor_location() if alive(tank) else None
            across = S['wall'].get_actor_right_vector()
            side = (loc - S['origin']).dot(across) if loc else None
            state = wall_info(S['wall']) if alive(S['wall']) else 'gone'
            holes = S['wall'].get_component_by_class(unreal.WallHoleDamageComponent) if alive(S['wall']) else None
            S['result']['after'] = dict(tank_alive=alive(tank), tank_side_cm=round(side) if side is not None else None,
                                        wall=state, breaches=holes.get_breach_count() if holes else None,
                                        removed=round(holes.get_removed_face_share(), 3) if holes else None)
            crossed = side is not None and side < -500
            S['result']['passed'] = bool(crossed and S['result']['after']['breaches'])
            log(f'after {json.dumps(S["result"]["after"], default=str)}')
            log(f'MISSION PASSED {S["result"]["passed"]}')
            finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
