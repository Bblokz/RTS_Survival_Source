"""Throwaway-editor test of the Berlin ruin wall breaches (heavy tanks only; never the shared editor or a mission map).
Launch: BREACH_SCRIPT=berlin_ruin_breach_test.py sh run_breach_test.sh <run> [timeout_s]
The ruin walls stand yawed 90 degrees so their wall line runs along world X at y = 0 (B5's first leg at y = 310); every
tank starts 24 m in front (+Y) and is ordered 24 m behind (-Y):
  B1/B2/B5/B6 Tiger -> breach, tank crosses;  B3 Tiger -> breach, the facade dies (KillOnFall)
  B2 Pz IV -> no breach (medium tanks go round ruins);  B4 killed first, Tiger -> the remnant is not breached
After the lanes: B6 (breached, standing) is killed: its remnant must keep the gap open (paths through for everyone).
Results: Saved/WallChunks/Breach/<run>/result.json + PNGs.
"""
import json
import os
import unreal

RUN = os.environ.get('BREACH_RUN', 'berlin')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/{RUN}/'
BP = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
# key: (lane x, piece, tank x offset from the wall actor, tank, expected)
LANES = {
    'B1_heavy': (-12000, 'B1_Facade1St_A', 0, 'TigerH1', 'breach'),
    'B2_heavy': (-8000, 'B2_Facade2St_A', 0, 'TigerH1', 'breach'),
    'B3_heavy_kill': (-4000, 'B3_Facade4St', 0, 'TigerH1', 'breach_kill'),
    'B5_heavy': (0, 'B5_CornerL', 0, 'TigerH1', 'breach'),
    'B6_heavy': (4000, 'B6_Stub_A', 0, 'TigerH1', 'breach'),
    'B2_medium': (8000, 'B2_Facade2St_B', 0, 'PzIV_G', 'detour'),
    'B4_dead_first': (12000, 'B4_Firewall', 0, 'TigerH1', 'remnant'),
}
START_Y = 2400.0
MOVE_SECONDS = 80.0
LIGHT_AT = (0.0, 9000.0)
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'walls': {}, 'tanks': {}, 'result': {}, 'track': {},
     'pics': []}


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


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, piece, dx, vehicle, _) in LANES.items():
        cls = unreal.load_class(None, f'{BP}BP_BerlinRuin_{piece}.BP_BerlinRuin_{piece}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, ground_z(world, x, 1500)), unreal.Rotator(0, 0, 90))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x + dx, START_Y, ground_z(world, x + dx, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed {piece} and {vehicle} owner set {set_owner(tank, 1)}')
    light = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES['Luchs']),
                                          unreal.Vector(LIGHT_AT[0], LIGHT_AT[1], ground_z(world, *LIGHT_AT) + 50),
                                          unreal.Rotator(0, 0, 0))
    set_owner(light, 1)
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 12000, 2000))
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, piece, _, _, _) in LANES.items():
            if name == f'BP_BerlinRuin_{piece}_C' and abs(loc.x - x) < 50 and abs(loc.y) < 50:
                S['walls'][key] = actor
        if name.startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, dx, _, _) in LANES.items():
            if abs(loc.x - x - dx) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
        if abs(loc.x - LIGHT_AT[0]) < 300 and abs(loc.y - LIGHT_AT[1]) < 500:
            S['light'] = tank
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
    log(f'runtime walls {sorted(S["walls"])} tanks {sorted(S["tanks"])} light {"light" in S}')


def wall_mesh(wall):
    return [c for c in wall.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def wall_state(key):
    wall = S['walls'].get(key)
    if not alive(wall):
        return dict(gone=True)
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    mesh = wall_mesh(wall)
    return dict(holes=holes.get_hole_count(), breaches=holes.get_breach_count(),
                removed=round(holes.get_removed_face_share(), 3), health=round(wall.get_editor_property('health')),
                alive=bool(wall.get_editor_property('health') > 0.0), z=round(wall.get_actor_location().z),
                material=mesh.get_material(0).get_name() if mesh.get_material(0) else None)


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
    x = LANES[key][0] + LANES[key][2]
    z = S['ground'][key] + 20
    a, b = unreal.Vector(x, 900, z), unreal.Vector(x, -900, z)
    queriers = {'PlayerHeavy': ('HeavyTank', S['tanks'].get('B1_heavy')),
                'PlayerMedium': ('MediumTank', S['tanks'].get('B2_medium')),
                'PlayerLight': ('LightTank', S.get('light')),
                'Character': ('Character', None)}
    out = {}
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


def lane_view(key, side=1.0, high=False):
    x = LANES[key][0]
    z = S['ground'][key]
    lift = 1500 if high else 800
    return unreal.Vector(x + 1300, side * 1700, z + lift), unreal.Vector(x, 0, z + (500 if high else 220))


def queue_picture(name, at, eye, look):
    S['pics'].append((at, name, eye, look))
    S['pics'].sort(key=lambda row: row[0])


def take_due_pictures():
    while S['pics'] and S['t'] >= S['pics'][0][0]:
        _, name, eye, look = S['pics'].pop(0)
        aim_and_shoot(name, eye, look)


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0] + LANES[key][2]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, last=None, breach_at=None,
                                                states=[]))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 500:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < -500:
                track['cross_side'] = round(loc.x - x)
                track['cross_time'] = round(S['t'] - S['order_time'], 1)
        state = wall_state(key)
        if state.get('breaches', 0) > 0 and track['breach_at'] is None:
            track['breach_at'] = round(S['t'] - S['order_time'], 2)
            log(f'{key}: first breach {json.dumps(state)} tank at {track["last"]}')
            high = key.startswith('B3')
            eye, look = lane_view(key, high=high)
            queue_picture(f'{key}_push', S['t'] + 0.25, eye, look)
            queue_picture(f'{key}_fallen', S['t'] + 1.4, eye, look)
            queue_picture(f'{key}_after', S['t'] + 7.0, eye, look)
            back_eye, _ = lane_view(key, -1.0, high=high)
            queue_picture(f'{key}_after_back', S['t'] + 7.2, back_eye, look)
        if len(track['states']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.5:
            track['sample_t'] = S['t']
            track['states'].append((round(S['t'] - S['order_time'], 1), state.get('breaches'), state.get('removed'),
                                    state.get('health'), track['last']))


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
                S['ground'] = {k: ground_z(world, LANES[k][0] + LANES[k][2], 1500) for k in LANES}
                # Killed before any tank comes: the remnant must not be breached later.
                unreal.GameplayStatics.apply_damage(S['walls']['B4_dead_first'], 1.0e7, None, None, unreal.DamageType)
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
                S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, last=None, breach_at=None,
                                                states=[]))['health0'] = S['result']['setup'][key].get('health')
            eye, look = lane_view('B4_dead_first', high=True)
            aim_and_shoot('B4_remnant_before', eye, look)
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                x = LANES[key][0] + LANES[key][2]
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
            crossed = all(t.get('cross_side') is not None for t in S['track'].values())
            if elapsed > MOVE_SECONDS or ((arrived or (crossed and elapsed > 25.0)) and elapsed > 15.0
                                          and not S['pics']):
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
                S['phase'], S['mark'] = 'verdict', S['t']
        elif phase == 'verdict':
            world = game_world()
            out = {}
            for key, (x, piece, _, vehicle, expected) in LANES.items():
                track = S['track'].get(key, {})
                state = wall_state(key)
                crossed = track.get('cross_side') is not None
                before = S['result']['paths_before'][key]
                after = across_paths(world, key)
                S['result'].setdefault('paths_after', {})[key] = after
                breached = state.get('breaches', 0) > 0 or (state.get('gone') and track.get('breach_at') is not None)
                heavy_only = bool(before['PlayerHeavy'].get('through') and not before['PlayerMedium'].get('through')
                                  and not before['PlayerLight'].get('through')
                                  and not before['Character'].get('through'))
                if expected in ('breach', 'breach_kill'):
                    ok = bool(breached and crossed and abs(track['cross_side']) < 400 and heavy_only
                              and after['Character'].get('through') and after['PlayerLight'].get('through'))
                    if expected == 'breach_kill':
                        ok = ok and state.get('alive') is False
                elif expected == 'detour':
                    ok = bool(state.get('breaches') == 0 and not before['PlayerMedium'].get('through'))
                else:
                    ok = bool(state.get('breaches') == 0 and not before['PlayerHeavy'].get('through')
                              and (not crossed or abs(track['cross_side']) > 400))
                out[key] = dict(piece=piece, vehicle=vehicle, expected=expected, passed=ok, crossed=crossed,
                                cross_dx=track.get('cross_side'), cross_time=track.get('cross_time'),
                                breach_at=track.get('breach_at'), max_side_near=round(track.get('max_side_near', 0)),
                                last=track.get('last'), end_state=state, states=track.get('states'))
                log(f'{key} verdict {json.dumps({k: v for k, v in out[key].items() if k != "states"})}')
                log(f'{key} paths after {json.dumps(after)}')
            S['result']['lanes'] = out
            # Breached and still standing: kill it; the remnant must keep the gap.
            wall = S['walls'].get('B6_heavy')
            S['result']['remnant_before_kill'] = wall_state('B6_heavy')
            if alive(wall):
                unreal.GameplayStatics.apply_damage(wall, 1.0e7, None, None, unreal.DamageType)
            eye, look = lane_view('B6_heavy')
            queue_picture('B6_remnant_dying', S['t'] + 0.6, eye, look)
            queue_picture('B6_remnant', S['t'] + 6.0, eye, look)
            queue_picture('B6_remnant_back', S['t'] + 6.2, lane_view('B6_heavy', -1.0)[0], look)
            S['phase'], S['mark'] = 'remnant', S['t']
        elif phase == 'remnant':
            take_due_pictures()
            if S['t'] - S['mark'] < 9.0 or S['pics']:
                return
            world = game_world()
            if nav_building(world) and S['t'] - S['mark'] < 30.0:
                return
            state = wall_state('B6_heavy')
            paths = across_paths(world, 'B6_heavy')
            ok = bool(state.get('alive') is False and paths['Character'].get('through')
                      and paths['PlayerLight'].get('through') and paths['PlayerHeavy'].get('through'))
            S['result']['remnant'] = dict(state=state, paths=paths, passed=ok)
            log(f'remnant {json.dumps(S["result"]["remnant"])}')
            lanes_ok = all(r['passed'] for r in S['result']['lanes'].values())
            S['result']['all_passed'] = lanes_ok and ok
            log(f'lanes_ok {lanes_ok} remnant_ok {ok}')
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
