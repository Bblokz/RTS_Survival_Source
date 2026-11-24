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
LUCHS = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII_Luchs/'
         'Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C')
# Which walls: Rus walls (wall along actor X, across = actor right) or the Berlin ruin wall pieces
# (BREACH_WALL_PREFIX=BP_BerlinRuin_B, BREACH_ACROSS=forward: their wall runs along the actor's Y).
WALL_PREFIX = os.environ.get('BREACH_WALL_PREFIX', 'Rus_Wall_Concrete')
ACROSS = os.environ.get('BREACH_ACROSS', 'right')
# The Luchs (cannot breach) starts this far along the wall from the Tiger (3 m let vehicle avoidance push the
# Tiger aside): it must not drive straight through.
LUCHS_ALONG = 800.0
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


def nav_building(world):
    fn = getattr(unreal.NavigationSystemV1, 'is_navigation_being_built', None)
    try:
        return bool(fn(world)) if fn else False
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


def across_of(wall):
    return wall.get_actor_forward_vector() if ACROSS == 'forward' else wall.get_actor_right_vector()


def along_of(wall):
    return wall.get_actor_right_vector() if ACROSS == 'forward' else wall.get_actor_forward_vector()


def pick_wall(world, walls):
    """A Soviet wall with ground on both sides at the wall's height (actor Y is across the wall)."""
    for wall in walls:
        info = wall_info(wall)
        if not info['breach'] or info['owner'] == 1:
            continue
        origin, extent = wall.get_actor_bounds(False)
        across = across_of(wall)
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
    # Both ends onto the navmesh first (the ground trace can sit above or below it on a mission map's terrain).
    for name in ('a', 'b'):
        point = a if name == 'a' else b
        try:
            projected = unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None,
                                                                             unreal.Vector(300, 300, 600))
        except Exception:  # noqa: BLE001
            projected = None
        if projected is not None:
            if name == 'a':
                a = projected
            else:
                b = projected
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, querier,
                                                                         unreal.load_class(None, HEAVY_FILTER))
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:120])
    if not pts:
        return dict(length=None)
    # Through the wall: no point of the path strays more than 3 m from the straight line front -> back.
    line = b - a
    side = max(abs((pt - a).cross(line).length() / max(line.length(), 1.0)) for pt in pts)
    return dict(length=round(sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))),
                straight=round(line.length()), partial=bool(p.is_partial()), side_cm=round(side),
                through=bool(side < 300 and not p.is_partial()))


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
        # Placed in the editor world before play (Python cannot spawn into the PIE world); the map is never saved.
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        walls = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                 if a.get_class().get_name().startswith(WALL_PREFIX)]
        pick = pick_wall(world, walls)
        if pick is None:
            log('no Soviet wall with open ground on both sides')
            finish()
            return
        wall, front, back, origin = pick
        S.update(wall_loc=wall.get_actor_location(), front=front, back=back, origin=origin)
        yaw = unreal.MathLibrary.find_look_at_rotation(front, back).yaw
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        tank = actors.spawn_actor_from_class(unreal.load_class(None, TIGER), front + unreal.Vector(0, 0, 60),
                                             unreal.Rotator(0, 0, yaw))
        rts = tank.get_component_by_class(unreal.RTSComponent)
        if rts is not None:
            rts.set_editor_property('owning_player', 1)
        along = along_of(wall)
        luchs = actors.spawn_actor_from_class(unreal.load_class(None, LUCHS), front + along * LUCHS_ALONG
                                              + unreal.Vector(0, 0, 60), unreal.Rotator(0, 0, yaw))
        luchs_rts = luchs.get_component_by_class(unreal.RTSComponent)
        if luchs_rts is not None:
            luchs_rts.set_editor_property('owning_player', 1)
        actors.spawn_actor_from_class(unreal.SceneCapture2D, origin + unreal.Vector(0, 0, 3000))
        log(f'picked {wall.get_name()} at {[round(v) for v in origin.to_tuple()]}; Tiger placed at {front}')
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'pie_started', S['t']
    elif phase == 'pie_started' and game_world() is not None and S['t'] - S['mark'] > 25.0:
        world = game_world()
        unreal.GameplayStatics.set_game_paused(world, False)
        walls = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                 if a.get_class().get_name().startswith(WALL_PREFIX)]
        S['result']['map'] = world.get_path_name()
        S['result']['walls'] = {w.get_name(): wall_info(w) for w in walls}
        log(f'{len(walls)} {WALL_PREFIX}* walls on {world.get_path_name()}')
        S['wall'] = min(walls, key=lambda w: (w.get_actor_location() - S['wall_loc']).length())
        tanks = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster)
        S['tank'] = min(tanks, key=lambda t: (t.get_actor_location() - S['front']).length())
        luchs_spot = S['front'] + along_of(S['wall']) * LUCHS_ALONG
        S['luchs'] = min([t for t in tanks if t != S['tank']],
                         key=lambda t: (t.get_actor_location() - luchs_spot).length())
        capture = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D),
                      key=lambda c: (c.get_actor_location() - S['origin']).length())
        capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp = capture.capture_component2d
        S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                                 unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp.set_editor_property('texture_target', S['rt'])
        comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property('capture_every_frame', False)
        comp.set_editor_property('fov_angle', 70.0)
        S['capture'] = capture
        rts = S['tank'].get_component_by_class(unreal.RTSComponent)
        log(f'runtime wall {S["wall"].get_name()} tank {S["tank"].get_name()} '
            f'owner {rts.get_editor_property("owning_player") if rts else None}')
        S['phase'], S['mark'] = 'spawned', S['t']
    elif phase == 'spawned' and S['t'] - S['mark'] > 6.0:
        world = game_world()
        # No path queries here: on Ger_IntroMission Python's synchronous queries return nothing (even 6 m from any
        # wall) and the nav-building probe fired an engine ensure (2026-11-16). The verdict is the drive itself.
        tank = S['tank']
        eye = S['origin'] + across_of(S['wall']) * 1800 + along_of(S['wall']) * 900 + unreal.Vector(0, 0, 900)
        S['eye'] = eye
        shoot('mission_before', eye, S['origin'])
        log(f'MoveToLocation -> {tank.move_to_location(S["back"], True, unreal.Rotator(0, 0, 0), False)}')
        luchs_goal = S['back'] + along_of(S['wall']) * LUCHS_ALONG
        log(f'Luchs MoveToLocation -> {S["luchs"].move_to_location(luchs_goal, True, unreal.Rotator(0, 0, 0), False)}')
        S['phase'], S['mark'] = 'moving', S['t']
        S['pics'] = [(S['t'] + t, f'mission_{n}') for t, n in ((5.0, 't5'), (8.0, 't8'), (14.0, 't14'))]
    elif phase == 'moving':
        # The drive itself: a tank only drives where its planned route goes (its own path following, which Python
        # cannot read), so a drive that stays near the straight line front -> back while it crosses the wall line was
        # planned through the wall. Recorded the moment it crosses (the Soviets on the map may kill it later).
        for label, unit, offset in (('drive', S['tank'], 0.0), ('luchs_drive', S['luchs'], LUCHS_ALONG)):
            if not alive(unit) or label in S['result']:
                continue
            loc = unit.get_actor_location()
            a = S['front'] + along_of(S['wall']) * offset
            line = (S['back'] + along_of(S['wall']) * offset) - a
            lateral = (loc - a).cross(line).length() / max(line.length(), 1.0)
            S[label + '_max_lateral'] = max(S.get(label + '_max_lateral', 0.0), lateral)
            if (loc - S['origin']).dot(across_of(S['wall'])) < -500.0:
                S['result'][label] = dict(crossed_at_s=round(S['t'] - S['mark'], 1),
                                          max_lateral_cm=round(S[label + '_max_lateral']),
                                          through=bool(S[label + '_max_lateral'] < 300.0))
                log(f'{label} {json.dumps(S["result"][label])}')
        if S['pics'] and S['t'] >= S['pics'][0][0]:
            shoot(S['pics'].pop(0)[1], S['eye'], S['origin'])
        if S['t'] - S['mark'] > 30.0 and not S['pics']:
            tank = S['tank']
            loc = tank.get_actor_location() if alive(tank) else None
            across = across_of(S['wall'])
            side = (loc - S['origin']).dot(across) if loc else None
            state = wall_info(S['wall']) if alive(S['wall']) else 'gone'
            holes = S['wall'].get_component_by_class(unreal.WallHoleDamageComponent) if alive(S['wall']) else None
            S['result']['after'] = dict(tank_alive=alive(tank), tank_side_cm=round(side) if side is not None else None,
                                        wall=state, breaches=holes.get_breach_count() if holes else None,
                                        removed=round(holes.get_removed_face_share(), 3) if holes else None)
            drive = S['result'].get('drive', {})
            # Negative control: the Luchs (light, cannot breach) must not have driven straight through the wall.
            luchs_ok = not S['result'].get('luchs_drive', {}).get('through', False)
            S['result']['luchs_ok'] = luchs_ok
            # Through the wall: the Tiger crossed and the picked wall was breached (a tank that went round breaks
            # nothing). The drift off the straight line is information: on a city map the route bends round rubble.
            S['result']['passed'] = bool(drive.get('crossed_at_s') is not None and S['result']['after']['breaches']
                                         and luchs_ok)
            log(f'luchs {json.dumps(S["result"].get("luchs_drive"))} ok {luchs_ok}')
            log(f'after {json.dumps(S["result"]["after"], default=str)}')
            log(f'MISSION PASSED {S["result"]["passed"]}')
            finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
