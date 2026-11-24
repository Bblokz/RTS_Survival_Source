"""Throwaway-editor test of the fence breaches (never the shared editor or a mission map).
Launch: BREACH_SCRIPT=fence_breach_test.py sh run_breach_test.sh <run> [timeout_s]
FENCE_FROM_DISK=1 tests the Blueprints as saved; otherwise fence_breach_setup.configure_fence() runs first in this
editor (its saves may fail while another editor holds the files; the compiled classes in memory are what PIE uses).
Each lane is a row of chained copies of one fence (rotated so its wall line runs along world X at y ~ 0, at least
~10 m long); the vehicle starts 24 m in front (+Y) of the middle piece and is ordered 24 m behind (-Y).
  wire fences (any vehicle, any side): Luchs, enemy Pz IV -> breach, cross, fence damaged
  concrete fences (any side, medium/heavy): Pz IV / Tiger (one enemy-owned) -> breach; Luchs -> no breach, goes round
Paths before (per agent, own unit as querier) and after; shell test hits on a wire and a concrete fence.
Results: Saved/WallChunks/Breach/<run>/result.json + PNGs.
"""
import json
import math
import os
import unreal

RUN = os.environ.get('BREACH_RUN', 'fence')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/{RUN}/'
FROM_DISK = os.environ.get('FENCE_FROM_DISK') == '1'
ENV = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
CW = ENV + 'Military/ConcreteWall/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
# fence: (Blueprint path, line axis in actor space, line offset across it, span min, span max, kind)
FENCES = {
    'industrial': (ENV + 'Industrial/BP_Industrial_Dest_FenceWire', 'Y', -8.0, -174.6, 190.5, 'wire'),
    'airport': (ENV + 'Airport/BP_Airport_Dest_FenceWire', 'X', 0.0, -182.3, 193.7, 'wire'),
    'military1': (CW + 'BP_Dest_Military_ConcreteFence_1', 'X', -4.0, -326.0, 326.0, 'concrete'),
    'military2': (CW + 'BP_Dest_Military_ConcreteFence_2', 'X', -4.0, -978.0, 978.0, 'concrete'),
    'postapo': (CW + 'BP_Dest_PostApo_ConcreteFence', 'Y', 0.0, -170.0, 170.0, 'concrete'),
    'postapo_long': (CW + 'BP_Dest_PostApo_ConcreteFenceLong', 'Y', 0.0, -510.0, 510.0, 'concrete'),
    'gate': (CW + 'BP_Dest_PostApo_ConcreteFenceGate', 'Y', -5.0, -3.0, 338.0, 'concrete'),
    'corner': (CW + 'BP_Dest_PostApo_ConcreteFenceCorner', 'Y', 0.0, -170.0, 0.0, 'concrete'),
}
# key: (lane x, fence, vehicle, owner, expected). Enemy lanes far from the player lanes (they shoot each other).
LANES = {
    'industrial_luchs': (0, 'industrial', 'Luchs', 1, 'breach'),
    'airport_luchs': (2600, 'airport', 'Luchs', 1, 'breach'),
    'military1_pziv': (5200, 'military1', 'PzIV_G', 1, 'breach'),
    'military2_tiger': (7800, 'military2', 'TigerH1', 1, 'breach'),
    'military1_luchs': (10400, 'military1', 'Luchs', 1, 'blocked'),
    'postapo_pziv': (13000, 'postapo', 'PzIV_G', 1, 'breach'),
    'postapo_luchs': (15600, 'postapo', 'Luchs', 1, 'blocked'),
    'gate_pziv': (18200, 'gate', 'PzIV_G', 1, 'breach'),
    'corner_tiger': (20800, 'corner', 'TigerH1', 1, 'breach'),
    'airport_enemy_pziv': (-16000, 'airport', 'PzIV_G', 2, 'breach'),
    'postapo_long_enemy_tiger': (-19000, 'postapo_long', 'TigerH1', 2, 'breach'),
}
SHOTS = {'industrial': -8000, 'postapo': -10600}
MIN_ROW = 1000.0
START_Y = 2400.0
MOVE_SECONDS = 80.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'walls': {}, 'rows': {}, 'tanks': {}, 'result': {},
     'track': {}, 'pics': [], 'shot_fences': {}}


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


def fence_class(fence):
    path = FENCES[fence][0]
    return unreal.load_class(None, f'{path}.{path.rsplit("/", 1)[1]}_C')


def placement(fence):
    """(yaw, world x of the span centre relative to the actor, world y of the line, span width)."""
    _, axis, offset, lo, hi, _ = FENCES[fence]
    if axis == 'Y':
        # Yaw 90: actor (x, y) -> world (-y, x).
        return 90.0, -(lo + hi) / 2.0, offset, hi - lo
    return 0.0, (lo + hi) / 2.0, offset, hi - lo


# A corner piece alone is a 1.7 m leg: tanks rightly drive round it. Flanked by straight fences it closes the row.
FLANKS = {'corner': 'postapo_long'}


def row_count(fence):
    width = placement(fence)[3]
    count = max(1, math.ceil(MIN_ROW / width))
    return count if count % 2 == 1 else count + 1


def row_pieces(lane_x, fence):
    """[(fence, actor x, actor y, yaw)] with the middle piece's span centred on lane_x."""
    flank = FLANKS.get(fence)
    if flank:
        width = placement(fence)[3]
        flank_width = placement(flank)[3]
        centres = [(flank, lane_x - (width + flank_width) / 2.0), (fence, lane_x),
                   (flank, lane_x + (width + flank_width) / 2.0)]
    else:
        width = placement(fence)[3]
        count = row_count(fence)
        centres = [(fence, lane_x + (i - count // 2) * width) for i in range(count)]
    out = []
    for piece, centre in centres:
        yaw, centre_dx, line_y, _ = placement(piece)
        out.append((piece, centre - centre_dx, -line_y, yaw))
    return out


def row_positions(lane_x, fence):
    pieces = row_pieces(lane_x, fence)
    return [(px, py) for _, px, py, _ in pieces], pieces[0][3]


def configure():
    namespace = {'__name__': 'fence_breach_library'}
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Breach/fence_breach_setup.py').read(), namespace)
    namespace['configure_fence'](save=False)
    log('configured in memory')


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, fence, vehicle, owner, _) in LANES.items():
        positions = row_pieces(x, fence)
        for piece, px, py, yaw in positions:
            actors.spawn_actor_from_class(fence_class(piece), unreal.Vector(px, py, ground_z(world, px, py)),
                                          unreal.Rotator(0, 0, yaw))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, START_Y, ground_z(world, x, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed {len(positions)} x {fence} and {vehicle} owner {owner} set {set_owner(tank, owner)}')
    for fence, x in SHOTS.items():
        pieces = row_pieces(x, fence)
        _, px, py, _ = pieces[len(pieces) // 2]
        actors.spawn_actor_from_class(fence_class(fence), unreal.Vector(px, py, ground_z(world, px, py)),
                                      unreal.Rotator(0, 0, placement(fence)[0]))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 12000, 2000))
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))


def find_runtime(world):
    expected = {}
    for key, (x, fence, _, _, _) in LANES.items():
        positions, _ = row_positions(x, fence)
        for i, (px, py) in enumerate(positions):
            expected[(round(px), round(py))] = (key, i, len(positions))
    shot_at = {}
    for f, x in SHOTS.items():
        pieces = row_pieces(x, f)
        shot_at[(round(pieces[len(pieces) // 2][1]), round(pieces[len(pieces) // 2][2]))] = f
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
        loc = actor.get_actor_location()
        for (px, py), (key, i, count) in expected.items():
            if abs(loc.x - px) < 30 and abs(loc.y - py) < 30:
                S['rows'].setdefault(key, {})[i] = actor
                if i == count // 2:
                    S['walls'][key] = actor
        for (px, py), fence in shot_at.items():
            if abs(loc.x - px) < 30 and abs(loc.y - py) < 30:
                S['shot_fences'][fence] = actor
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _, _) in LANES.items():
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
    log(f'runtime walls {len(S["walls"])}/{len(LANES)} tanks {len(S["tanks"])}/{len(LANES)} '
        f'shot fences {sorted(S["shot_fences"])}')


def wall_mesh(wall):
    return [c for c in wall.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def actor_state(wall):
    if not alive(wall):
        return dict(gone=True)
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    mesh = wall_mesh(wall)
    health = wall.get_editor_property('health')
    breach = wall.get_component_by_class(unreal.WallTankBreachComponent)
    settings = breach.get_editor_property('settings') if breach else None
    return dict(holes=holes.get_hole_count() if holes else None, breaches=holes.get_breach_count() if holes else None,
                removed=round(holes.get_removed_face_share(), 3) if holes else None, health=round(health),
                alive=bool(health > 0.0), mesh=mesh.static_mesh.get_name() if mesh.static_mesh else None,
                material=mesh.get_material(0).get_name() if mesh.get_material(0) else None,
                breach_comp=breach is not None,
                any_vehicle=bool(settings.get_editor_property('any_vehicle_breaks')) if settings else None,
                any_side=bool(settings.get_editor_property('any_side_breaks')) if settings else None)


def wall_state(key):
    return actor_state(S['walls'].get(key))


def neighbours_state(key):
    row = S['rows'].get(key, {})
    mid = len(row) // 2
    return {i: actor_state(a).get('breaches', 'gone') for i, a in sorted(row.items()) if i != mid}


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
    x = LANES[key][0]
    z = S['ground'][key] + 20
    a, b = unreal.Vector(x, 900, z), unreal.Vector(x, -900, z)
    queriers = {'PlayerHeavy': ('HeavyTank', S['tanks'].get('military2_tiger')),
                'PlayerMedium': ('MediumTank', S['tanks'].get('military1_pziv')),
                'PlayerLight': ('LightTank', S['tanks'].get('industrial_luchs')),
                'EnemyMedium': ('MediumTank', S['tanks'].get('airport_enemy_pziv')),
                'Character': ('Character', None)}
    out = {'Planner': path(world, a, b, S['nav']['LightTank'], unreal.RTSClosedBreachableWallsQueryFilter)}
    for label, (agent, unit) in queriers.items():
        context = unit.get_controller() if unit is not None and alive(unit) else None
        out[label] = path(world, a, b, context if context is not None else S['nav'][agent],
                          unreal.load_class(None, FILTERS[agent]))
    return out


def aim_and_shoot(name, eye, look):
    capture = S['capture']
    capture.set_actor_location(eye, False, True)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    capture.capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), name + '.png')
    log(f'picture {name}')


def lane_view(x, z, side=1.0):
    return unreal.Vector(x + 1000, side * 1400, z + 750), unreal.Vector(x, 0, z + 200)


def queue_picture(name, at, eye, look):
    S['pics'].append((at, name, eye, look))
    S['pics'].sort(key=lambda row: row[0])


def take_due_pictures():
    while S['pics'] and S['t'] >= S['pics'][0][0]:
        _, name, eye, look = S['pics'].pop(0)
        aim_and_shoot(name, eye, look)


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0]
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
        if (state.get('breaches') or 0) > 0 and track['breach_at'] is None:
            track['breach_at'] = round(S['t'] - S['order_time'], 2)
            log(f'{key}: first breach {json.dumps(state)} tank at {track["last"]}')
            eye, look = lane_view(LANES[key][0], S['ground'][key])
            queue_picture(f'{key}_push', S['t'] + 0.2, eye, look)
            queue_picture(f'{key}_after', S['t'] + 6.0, eye, look)
            queue_picture(f'{key}_after_back', S['t'] + 6.2, lane_view(LANES[key][0], S['ground'][key], -1.0)[0],
                          look)
        if len(track['states']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.5:
            track['sample_t'] = S['t']
            track['states'].append((round(S['t'] - S['order_time'], 1), state.get('breaches'), state.get('removed'),
                                    state.get('health'), track['last']))


def shot_test():
    out = {}
    for fence, x in SHOTS.items():
        actor = S['shot_fences'].get(fence)
        if actor is None:
            out[fence] = dict(passed=False, error='missing')
            continue
        holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
        loc = actor.get_actor_location()
        z = ground_z(game_world(), x, 1500)
        origin, extent = actor.get_actor_bounds(True)
        mid_z = origin.z
        before = actor_state(actor)
        # Wall line on world y = 0, hits at mid height. A debug hit is a splash point: the 20 mm one (17 cm hole) must
        # sit on the face (5 cm), the 75 mm one reaches 40 cm.
        removed_20 = holes.debug_fire_test_hit(unreal.Vector(x - 90, 5, mid_z), unreal.Vector(0, -1, 0), 20)
        removed = holes.debug_fire_test_hit(unreal.Vector(x + 70, 40, mid_z), unreal.Vector(0, -1, 0), 75)
        unreal.GameplayStatics.apply_damage(actor, 60.0, None, None, unreal.DamageType)
        state = actor_state(actor)
        ok = bool(removed_20 > 0 and removed > 0 and state.get('holes', 0) >= 2 and state.get('alive')
                  and state.get('health') < before.get('health'))
        out[fence] = dict(removed_20=removed_20, removed_75=removed, before=before, state=state, passed=ok,
                          actor_z=round(loc.z), mid_z=round(mid_z), extent=str(extent))
        log(f'shot test {fence} {json.dumps(out[fence], default=str)}')
        eye, look = lane_view(x, z)
        queue_picture(f'shot_{fence}', S['t'] + 0.6, eye, look)
    S['result']['shots'] = out


def verdict(key, world):
    x, fence, vehicle, owner, expected = LANES[key]
    kind = FENCES[fence][5]
    track = S['track'].get(key, {})
    state = wall_state(key)
    crossed = track.get('cross_side') is not None
    before = S['result']['paths_before'][key]
    after = across_paths(world, key)
    S['result'].setdefault('paths_after', {})[key] = after
    health0 = S['result']['setup'][key].get('health') or 0
    breached = (state.get('breaches') or 0) > 0 or track.get('breach_at') is not None
    damaged = bool(state.get('gone') or not state.get('alive') or (state.get('health') or 0) < health0)
    if kind == 'wire':
        nav_ok = all(before[q].get('through') for q in ('PlayerHeavy', 'PlayerMedium', 'PlayerLight', 'EnemyMedium'))
    else:
        nav_ok = bool(before['PlayerHeavy'].get('through') and before['PlayerMedium'].get('through')
                      and before['EnemyMedium'].get('through') and not before['PlayerLight'].get('through'))
    nav_ok = nav_ok and not before['Character'].get('through') and bool(before['Planner'].get('through'))
    material_ok = state.get('alive') or state.get('gone') or not str(state.get('material')).startswith('MID_')
    if expected == 'breach':
        ok = bool(breached and damaged and crossed and abs(track['cross_side']) < 300 and nav_ok
                  and after['Character'].get('through') and material_ok)
    elif expected == 'breach_only':
        ok = bool(breached and damaged and material_ok)
    else:
        ok = bool(not breached and not damaged and (not crossed or abs(track['cross_side']) > 300))
    out = dict(fence=fence, vehicle=vehicle, owner=owner, expected=expected, passed=ok, breached=breached,
               damaged=damaged, crossed=crossed, cross_dx=track.get('cross_side'), cross_time=track.get('cross_time'),
               breach_at=track.get('breach_at'), min_speed_near=track.get('min_speed_near'), nav_ok=nav_ok,
               material_ok=material_ok, health0=health0, end_state=state, neighbours=neighbours_state(key),
               states=track.get('states'))
    log(f'{key} verdict {json.dumps({k: v for k, v in out.items() if k != "states"})}')
    log(f'{key} paths before {json.dumps(before)}')
    log(f'{key} paths after {json.dumps(after)}')
    return out


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
                log('testing the Blueprints as saved on disk')
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
            shot_test()
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                target = unreal.Vector(LANES[key][0], -START_Y, S['ground'][key] + 20)
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
            out = {key: verdict(key, world) for key in LANES}
            S['result']['lanes'] = out
            shots = S['result'].get('shots', {})
            for fence, row in shots.items():
                row['end_state'] = actor_state(S['shot_fences'].get(fence))
            lanes_ok = all(r['passed'] for r in out.values())
            shots_ok = all(r.get('passed') for r in shots.values())
            S['result']['all_passed'] = bool(lanes_ok and shots_ok)
            log(f'failed lanes {[k for k, r in out.items() if not r["passed"]]}')
            log(f'lanes_ok {lanes_ok} shots_ok {shots_ok}')
            log(f'ALL PASSED {S["result"]["all_passed"]}')
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 480.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
