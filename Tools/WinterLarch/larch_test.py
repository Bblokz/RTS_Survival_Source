"""Throwaway-editor test of the Frozen Lake winter larches: crush gates, navigation, real tank fire -> wood holes, death.
Derived from Tools/DesertRocks rock_crush_test.py + rock_fire_test.py. Never in the shared editor; RTS_ToCopySmall is
never saved; the editor quits at the end. Launch: powershell Source/Tools/WinterLarch/run_larch_test.ps1
Crush lanes along world Y (tree at y 0, vehicle starts 24 m before it and is ordered 24 m past it):
  A larch F (AnyTank)        Luchs (light)  -> crush
  B larch D (MediumOrHeavy)  Luchs (light)  -> path round, tree untouched
  C larch A (MediumOrHeavy)  Pz IV G        -> crush
  D larch E (MediumOrHeavy)  Pz IV G        -> crush
Fire lanes: larch B and C, one player Pz IV G 35 m in front ordered to attack (AttackGround on the trunk as fallback).
Then larch B takes lethal damage. Results: Saved/WinterLarch/test/result.json + PNGs.
"""
import json
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WinterLarch/test/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/WinterLarch/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
LANES = {
    'A_F_light': (-9000, 'F', 'Luchs', 'crush'),
    'B_D_light': (-6000, 'D', 'Luchs', 'detour'),
    'C_A_medium': (-3000, 'A', 'PzIV_G', 'crush'),
    'D_E_medium': (0, 'E', 'PzIV_G', 'crush'),
}
FIRE = {'FB': (5000, 'B'), 'FC': (9000, 'C')}
START_Y = 2400.0
FIRE_TANK_Y = 3500.0
MOVE_SECONDS = 100.0
FIRE_SECONDS = 40.0
ATTACK_GROUND_AFTER = 12.0
TRUNK_AIM_Z = 150.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'trees': {}, 'tanks': {}, 'result': {}, 'track': {},
     'fire_state': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('LARCHTEST ' + message)


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


def tree_mesh(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Tree'][0]


def hp(actor):
    comp = actor.get_component_by_class(unreal.HealthComponent)
    if comp is None:
        return None
    for name in ('get_current_health', 'GetCurrentHealth'):
        fn = getattr(comp, name, None)
        if fn:
            return round(float(fn()))
    return round(float(comp.get_editor_property('CurrentHealth')))


def holes(actor):
    comp = actor.get_component_by_class(unreal.WreckHoleDamageComponent)
    return comp.get_hole_count() if comp else -1


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def bp_class(letter):
    name = f'BP_FrozenLake_WinterLarch_{letter}'
    return unreal.load_class(None, f'{BP_DIR}{name}.{name}_C')


# ---------------------------------------------------------------- setup

def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, letter, vehicle, _) in LANES.items():
        actors.spawn_actor_from_class(bp_class(letter), unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 0))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, START_Y, ground_z(world, x, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed larch {letter} at x {x} and {vehicle}, owner set {set_owner(tank, 1)}')
    for key, (x, letter) in FIRE.items():
        actors.spawn_actor_from_class(bp_class(letter), unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 0))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES['PzIV_G']),
                                             unreal.Vector(x, FIRE_TANK_Y, ground_z(world, x, FIRE_TANK_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed larch {letter} at x {x} and a firing Pz IV, owner set {set_owner(tank, 1)}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 2000))


def find_runtime(world):
    every = {**{k: (v[0], v[1]) for k, v in LANES.items()}, **FIRE}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, letter) in every.items():
            if name == f'BP_FrozenLake_WinterLarch_{letter}_C' and abs(loc.x - x) < 200 and abs(loc.y) < 200:
                S['trees'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
        for key, (x, _) in FIRE.items():
            if abs(loc.x - x) < 300 and abs(loc.y - FIRE_TANK_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 2000)).length())
    log(f'runtime trees {sorted(S["trees"])} tanks {sorted(S["tanks"])}')


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
    return dict(length=round(length), partial=bool(p.is_partial()), side_cm=round(side),
                through=bool(side < 30 and not p.is_partial()))


def across_paths(world, key):
    o = S['origins'][key]
    a, b = unreal.Vector(o.x, 600, o.z + 20), unreal.Vector(o.x, -600, o.z + 20)
    return {agent: path(world, a, b, nav, unreal.load_class(None, FILTERS[agent])) for agent, nav in S['nav'].items()}


def tree_state(key):
    tree = S['trees'][key]
    if not alive(tree):
        return None
    mesh = tree_mesh(tree)
    rot = mesh.get_world_rotation()
    return dict(z=round(mesh.get_world_location().z - S['origins'][key].z, 1), pitch=round(rot.pitch, 1),
                roll=round(rot.roll, 1), visible=bool(mesh.is_visible()),
                collision=str(mesh.get_collision_enabled()).split('.')[-1].split(':')[0],
                life=round(tree.get_life_span(), 1), health=hp(tree))


def setup_report(key):
    tree = S['trees'][key]
    mesh = tree_mesh(tree)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('EnemyObj', ch.ECC_ENEMY_OBJ),
                         ('Projectile', ch.ECC_PROJECTILE), ('Pawn', ch.ECC_PAWN))}
    navs = tree.get_components_by_class(unreal.NavModifierComponent)
    return dict(cls=tree.get_class().get_name(), mesh=mesh.get_editor_property('static_mesh').get_name(),
                materials=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())],
                overlap_events=bool(mesh.get_editor_property('generate_overlap_events')), responses=resp,
                nav_areas=[n.get_editor_property('area_class').get_name() for n in navs],
                crush=str(tree.get_editor_property('TankDestructionType')), health=hp(tree), holes=holes(tree))


# ---------------------------------------------------------------- crush

def sample():
    for key in LANES:
        tank = S['tanks'].get(key)
        x = LANES[key][0]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, crush=None, gone_at=None,
                                                last=None, samples=[]))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 450:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < 0:
                track['cross_side'] = round(loc.x - x)
        state = tree_state(key)
        if state is None:
            if track['gone_at'] is None:
                track['gone_at'] = round(S['t'] - S['order_time'], 1)
                log(f'{key}: tree actor gone at {track["gone_at"]} s')
            if track['crush'] is None:
                track['crush'] = dict(time=track['gone_at'], vehicle_at=track['last'], first='gone')
            continue
        start = S['initial'][key]
        changed = abs(state['z']) > 1 or abs(state['pitch'] - start['pitch']) > 1 or \
            abs(state['roll'] - start['roll']) > 1 or state['life'] > 0 or not state['visible'] or \
            state['collision'] != start['collision'] or (state['health'] is not None and state['health'] != start['health'])
        if changed and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], first=state)
            log(f'{key}: tree changed {json.dumps(track["crush"])}')
            S.setdefault('pic_queue', []).append((key, S['t'] + 0.8))
        if changed and len(track['samples']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.3:
            track['sample_t'] = S['t']
            track['samples'].append((round(S['t'] - S['order_time'], 2), state))


# ---------------------------------------------------------------- fire

def order_fire(key):
    tank, tree = S['tanks'].get(key), S['trees'].get(key)
    if tank is None or tree is None:
        log(f'{key}: no tank or tree to fire')
        return
    try:
        result = tank.attack_actor(tree, True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'[:160]
    log(f'{key}: AttackActor -> {result}')


def attack_ground(key):
    o = S['origins'][key]
    try:
        result = S['tanks'][key].attack_ground(unreal.Vector(o.x, o.y, o.z + TRUNK_AIM_Z), True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'[:160]
    log(f'{key}: AttackGround on the trunk -> {result}')


def watch_fire():
    for key in FIRE:
        tree = S['trees'].get(key)
        if not alive(tree):
            continue
        mesh = tree_mesh(tree)
        state = (hp(tree), holes(tree), mesh.get_material(0).get_name())
        if S['fire_state'].get(key) != state:
            S['fire_state'][key] = state
            log(f'{key}: health {state[0]} holes {state[1]} slot0 {state[2]}')


# ---------------------------------------------------------------- pictures

def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def shoot(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


# ---------------------------------------------------------------- phases

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
                unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
                find_runtime(world)
                S['origins'] = {k: t.get_actor_location() for k, t in S['trees'].items()}
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            S['nav'] = navmeshes(world)
            S['initial'] = {k: tree_state(k) for k in S['trees']}
            S['result']['setup'] = {k: setup_report(k) for k in S['trees']}
            S['result']['paths_before'] = {k: across_paths(world, k) for k in S['trees']}
            for key in S['trees']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            o = S['origins']['C_A_medium']
            aim(o + unreal.Vector(1500, 2600, 900), o + unreal.Vector(0, 0, 500))
            S['phase'], S['mark'] = 'look_pic', S['t']
        elif phase == 'look_pic' and S['t'] - S['mark'] > 2.5:
            shoot('look_larches')
            S['phase'] = 'orders'
        elif phase == 'orders':
            S['order_time'] = S['t']
            for key in LANES:
                x = LANES[key][0]
                target = unreal.Vector(x, -START_Y, S['origins'][key].z + 20)
                try:
                    result = S['tanks'][key].move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation -> {result}')
            aim(unreal.Vector(-4500, -2600, S['origins']['A_F_light'].z + 2600),
                unreal.Vector(-4500, 0, S['origins']['A_F_light'].z))
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            queue = S.get('pic_queue', [])
            if queue and S['t'] >= queue[0][1]:
                key = queue.pop(0)[0]
                o = S['origins'][key]
                aim(o + unreal.Vector(900, -1100, 700), o + unreal.Vector(0, 0, 150))
                shoot('crush_' + key)
            elapsed = S['t'] - S['mark']
            arrived = len(S['track']) == len(LANES) and all(
                isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 400 for t in S['track'].values())
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 20.0 and not queue):
                S['phase'], S['mark'] = 'settle', S['t']
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
        elif phase == 'settle':
            sample()
            if S['t'] - S['mark'] > 6.0:
                world = game_world()
                out = {}
                for key, (x, letter, vehicle, expected) in LANES.items():
                    track = S['track'].get(key, {})
                    crushed = track.get('crush') is not None
                    if expected == 'crush':
                        ok = crushed and track.get('cross_side') is not None and abs(track['cross_side']) < 150
                    else:
                        ok = (not crushed) and alive(S['trees'][key]) and track.get('cross_side') is not None and \
                            track.get('max_side_near', 0) > 100
                    out[key] = dict(larch=letter, vehicle=vehicle, expected=expected, passed=bool(ok), crushed=crushed,
                                    tree_alive_at_end=alive(S['trees'][key]), end_state=tree_state(key),
                                    vehicle_crossed_y0_at_dx=track.get('cross_side'),
                                    vehicle_max_side_offset_near_tree=round(track.get('max_side_near', 0)),
                                    vehicle_last=track.get('last'), crush=track.get('crush'),
                                    actor_gone_s=track.get('gone_at'), samples=track.get('samples'))
                S['result']['lanes'] = out
                S['result']['paths_after'] = {k: across_paths(world, k) for k in LANES}
                for key, row in out.items():
                    log(f'{key} verdict {json.dumps({k: v for k, v in row.items() if k != "samples"})}')
                    log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
                for key in FIRE:
                    order_fire(key)
                o = S['origins']['FB']
                aim(o + unreal.Vector(2000, 2400, 800), o + unreal.Vector(2000, 0, 200))
                S['phase'], S['mark'] = 'fire', S['t']
        elif phase == 'fire':
            watch_fire()
            elapsed = S['t'] - S['mark']
            if elapsed > ATTACK_GROUND_AFTER and not S.get('ground_ordered'):
                S['ground_ordered'] = True
                for key in FIRE:
                    if S['fire_state'].get(key, (None, 0))[1] <= 0:
                        attack_ground(key)
            if elapsed > FIRE_SECONDS:
                S['result']['fire'] = {k: dict(health_start=S['result']['setup'][k]['health'],
                                               health_end=S['fire_state'].get(k, (None,))[0],
                                               holes=S['fire_state'].get(k, (None, None))[1],
                                               slot0=S['fire_state'].get(k, (None, None, None))[2],
                                               alive=alive(S['trees'].get(k)))
                                       for k in FIRE}
                log(f'fire result {json.dumps(S["result"]["fire"])}')
                for key in FIRE:
                    tank = S['tanks'].get(key)
                    if alive(tank):
                        try:
                            tank.stop_behaviour_tree_ai() if hasattr(tank, 'stop_behaviour_tree_ai') else None
                            tank.move_to_location(tank.get_actor_location(), True, unreal.Rotator(0, 0, -90), False)
                        except Exception:  # noqa: BLE001
                            pass
                S['pic_keys'] = [k for k in FIRE if alive(S['trees'].get(k))]
                S['phase'], S['mark'] = 'fire_pic', S['t']
        elif phase == 'fire_pic':
            if not S['pic_keys']:
                S['phase'] = 'kill'
                return
            key = S['pic_keys'][0]
            if S.get('aimed') != key:
                o = S['origins'][key]
                aim(o + unreal.Vector(250, 420, 260), o + unreal.Vector(0, 0, TRUNK_AIM_Z))
                S['aimed'], S['mark'] = key, S['t']
            elif S['t'] - S['mark'] > 2.5:
                shoot('holes_' + key)
                S['pic_keys'].pop(0)
        elif phase == 'kill':
            tree = S['trees'].get('FB')
            if alive(tree):
                unreal.GameplayStatics.apply_damage(tree, 1.0e7, None, None, unreal.DamageType)
                log(f'killed FB: {json.dumps(tree_state("FB"))}')
                o = S['origins']['FB']
                aim(o + unreal.Vector(1200, 1800, 900), o + unreal.Vector(0, 0, 300))
            S['phase'], S['mark'] = 'kill_pic', S['t']
        elif phase == 'kill_pic' and S['t'] - S['mark'] > 1.0:
            shoot('kill_FB')
            S['phase'], S['mark'] = 'kill_wait', S['t']
        elif phase == 'kill_wait' and S['t'] - S['mark'] > 10.0:
            world = game_world()
            S['result']['kill'] = dict(gone_10s_later=not alive(S['trees'].get('FB')), state=tree_state('FB'),
                                       paths_after=across_paths(world, 'FB'))
            log(f'kill {json.dumps(S["result"]["kill"])}')
            fire = S['result']['fire']
            S['result']['all_passed'] = all(r['passed'] for r in S['result']['lanes'].values()) and all(
                (v['holes'] or 0) > 0 for v in fire.values())
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
