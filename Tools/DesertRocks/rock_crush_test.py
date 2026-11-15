"""Throwaway-editor crush test of the desert rocks (ue_blueprints.py): crush lanes, navigation, death by damage.
Derived from the Ore rock_engine_test.py (look phase dropped). Never in the shared editor; RTS_ToCopySmall is never
saved; the editor quits at the end. Launch: ROCK_SCRIPT=rock_crush_test.py sh fire_test_run.sh <logname> 560
Lanes along world Y; each vehicle starts 24 m before its rock and is ordered 24 m past it:
  A medium M4_Cracked (MediumOrHeavy)   Luchs (light)  -> path round, rock untouched
  B medium M1_Block   (MediumOrHeavy)   Pz IV G        -> crush
  C medium M7_Long    (MediumOrHeavy)   Pz IV G        -> crush
  D large  L1_Block   (not crushable)   Pz IV G        -> path round, rock untouched
Then two crushable medium rocks take lethal damage: debris burst, actor gone (the crushable death-by-damage path).
Results: Saved/DesertRocks/crush/result.json + PNGs.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/crush/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'
CLIFF = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Meshes/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G(Medium)': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'Luchs(Light)': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
LANES = {
    'A_medium_light': (-9000, 'BP_DesertRock_M4_Cracked', 'Luchs(Light)', 'detour'),
    'B_medium_medium': (-6000, 'BP_DesertRock_M1_Block', 'PzIV_G(Medium)', 'crush'),
    'C_medium_medium': (-3000, 'BP_DesertRock_M7_Long', 'PzIV_G(Medium)', 'crush'),
    'D_large_medium': (0, 'BP_DesertRock_L1_Block', 'PzIV_G(Medium)', 'detour'),
}
KILL = {'E_kill_medium': (3000, 'BP_DesertRock_M6_Cube'), 'F_kill_medium': (4000, 'BP_DesertRock_M8_Wedge')}
ROCKS = []
LOOK_Y = 6000.0
# cliff kit pieces (pivot at their centre, lifted onto the ground) with one medium rock right beside each
CLIFF_PIECES = ()
PAIR_ROCKS = ()
HIT = (5200, 'BP_DesertRock_M2_Slab')     # one non-lethal hit: does a health bar show?
START_Y = 2400.0
MOVE_SECONDS = 100.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'tanks': {}, 'result': {}, 'track': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('ROCKTEST ' + message)


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


def look_positions():
    out = {}
    for i, rock in enumerate(ROCKS):
        if rock.startswith('S'):
            out[rock] = (-1500 + (i % 8) * 260, LOOK_Y - 300, 15 * i)
        else:
            out[rock] = (-1900 + (i % 8) * 420, LOOK_Y + 350, 25 * i)
    return out


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
    for key, (x, bp) in KILL.items():
        cls = unreal.load_class(None, f'{BP_DIR}{bp}.{bp}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 30))
    for rock, (x, y, yaw) in look_positions().items():
        cls = unreal.load_class(None, f'{BP_DIR}BP_OreRock_{rock}.BP_OreRock_{rock}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(world, x, y)), unreal.Rotator(0, 0, yaw))
    for piece, x, y, yaw in CLIFF_PIECES:
        mesh = unreal.load_asset(CLIFF + piece)
        lift = -mesh.get_bounding_box().min.z          # cliff pieces have their pivot at the centre
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, ground_z(world, x, y) + lift),
                                          unreal.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_label('LOOK_' + piece)
    for rock, x, y, yaw in PAIR_ROCKS:
        cls = unreal.load_class(None, f'{BP_DIR}BP_OreRock_{rock}.BP_OreRock_{rock}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(world, x, y)), unreal.Rotator(0, 0, yaw))
    cls = unreal.load_class(None, f'{BP_DIR}{HIT[1]}.{HIT[1]}_C')
    actors.spawn_actor_from_class(cls, unreal.Vector(HIT[0], 0, ground_z(world, HIT[0], 0)), unreal.Rotator(0, 0, 0))
    # a parked player Luchs beside the look rows: its vision lifts the fog of war there, and it gives the scale
    scout = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES['Luchs(Light)']),
                                          unreal.Vector(900, LOOK_Y - 1000, ground_z(world, 900, LOOK_Y - 1000) + 50),
                                          unreal.Rotator(0, 0, 0))
    log(f'look scout owner set {set_owner(scout, 1)}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, bp, _, _) in LANES.items():
            if name == bp + '_C' and abs(loc.x - x) < 200 and abs(loc.y) < 200:
                S['pieces'][key] = actor
        for key, (x, bp) in KILL.items():
            if name == bp + '_C' and abs(loc.x - x) < 200 and abs(loc.y) < 200:
                S['pieces'][key] = actor
        if name == HIT[1] + '_C' and abs(loc.x - HIT[0]) < 200 and abs(loc.y) < 200:
            S['hit_rock'] = actor
        if (name.startswith('BP_OreRock_') or name == 'StaticMeshActor') and abs(loc.y - LOOK_Y) < 1500:
            S.setdefault('look_actors', []).append(actor)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 2000)).length())
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
    a, b = unreal.Vector(x, 600, z), unreal.Vector(x, -600, z)
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
                material=mesh.get_material(0).get_name(), mobility=str(mesh.get_editor_property('mobility')),
                overlap_events=bool(mesh.get_editor_property('generate_overlap_events')), responses=resp,
                nav_boxes=len(boxes),
                nav_areas=sorted(set(str(b.get_editor_property('area_class_override').get_name()) for b in boxes)))
    for prop in ('Crushable', 'CrushDestructionType', 'Overlap Tanks', 'CrushSinkDeltaZ', 'CrushSinkTime'):
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
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, samples=[], crush=None,
                                                gone_at=None, last=None))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 450:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < 0:
                track['cross_side'] = round(loc.x - x)
                track['cross_time'] = round(S['t'] - S['order_time'], 1)
        state = piece_state(key)
        if state is None:
            if track['gone_at'] is None:
                track['gone_at'] = round(S['t'] - S['order_time'], 1)
                log(f'{key}: rock actor gone at {track["gone_at"]} s after the order')
            continue
        changed = state['z'] != 0 or abs(state['pitch']) > 0.5 or abs(state['roll']) > 0.5 or state['life'] > 0 \
            or not state['visible']
        if changed and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], first=state)
            log(f'{key}: rock changed {json.dumps(track["crush"])}')
            if key not in S.setdefault('pictured', set()) and len(S['pictured']) < 2:
                S['pictured'].add(key)
                S.setdefault('pic_queue', []).append((key, S['t'] + 0.6))
        if changed and len(track['samples']) < 40 and S['t'] - track.get('sample_t', 0.0) >= 0.25:
            track['sample_t'] = S['t']
            track['samples'].append((round(S['t'] - S['order_time'], 2), state['z'], state['pitch'], state['roll'],
                                     state['visible']))


def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def colour_probes(eye):
    """Shot positions (1600 x 900) of a lit top point and a camera-facing side point of the cliff pieces and the
    medium rocks beside them; sampled from look_close_cliff.png afterwards."""
    pc = unreal.GameplayStatics.get_player_controller(game_world(), 0)
    size = unreal.WidgetLayoutLibrary.get_viewport_size(game_world())
    vx, vy = size.x, size.y
    wanted = ['SM_OreRock_' + r for r, _, _, _ in PAIR_ROCKS]
    out = {}
    for actor in S.get('look_actors', []):
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = comp.get_editor_property('static_mesh') if comp else None
        if mesh is None or not (mesh.get_name().startswith('SM_OreCliff') or mesh.get_name() in wanted):
            continue
        if mesh.get_name() in wanted and abs(actor.get_actor_location().x - 2200) > 800:
            continue
        o, ext = actor.get_actor_bounds(False)
        to_cam = unreal.Vector(eye.x - o.x, eye.y - o.y, 0).normal()
        pts = {'top': unreal.Vector(o.x, o.y, o.z + ext.z - 3),
               'side': unreal.Vector(o.x + to_cam.x * ext.x * 0.95, o.y + to_cam.y * ext.y * 0.95, o.z)}
        row = {}
        for key, p in pts.items():
            ok, sp = unreal.GameplayStatics.project_world_to_screen(pc, p, False)
            if ok:
                row[key] = [round(sp.x / vx * 1600), round(sp.y / vy * 900)]
        out[mesh.get_name()] = row
    return out


def shoot(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def look_shots():
    return []


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
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
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
            try:
                st = unreal.MaterialEditingLibrary.get_statistics(unreal.load_asset(
                    '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Materials/M_DesertRock'))
                S['result']['material_stats'] = dict(pixel=st.num_pixel_shader_instructions,
                                                     vertex=st.num_vertex_shader_instructions,
                                                     samplers=st.num_samplers, textures=st.num_pixel_texture_samples)
            except Exception as error:  # noqa: BLE001
                S['result']['material_stats'] = repr(error)
            log(f'material stats {S["result"]["material_stats"]}')
            S['result']['setup'] = {k: setup_report(k) for k in S['pieces']}
            S['result']['paths_before'] = {k: across_paths(world, k) for k in S['pieces']}
            for key in S['pieces']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            S['shots'] = look_shots()
            S['phase'], S['mark'] = 'look', S['t']
        elif phase == 'look':
            if not S['shots']:
                S['phase'] = 'orders'
                return
            name, eye, look = S['shots'][0]
            if S.get('aimed') != name:
                aim(eye, look)
                S['aimed'], S['mark'] = name, S['t']
            elif S['t'] - S['mark'] > 2.5:
                if name == 'look_close_cliff':
                    try:
                        S['result']['colour_probes'] = colour_probes(eye)
                    except Exception as error:  # noqa: BLE001
                        S['result']['colour_probes'] = repr(error)
                    log(f'colour probes {json.dumps(S["result"]["colour_probes"])}')
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
            aim(unreal.Vector(-4500, -1800, S['origins']['A_medium_light'].z + 2600),
                unreal.Vector(-4500, 0, S['origins']['A_medium_light'].z))
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            queue = S.get('pic_queue', [])
            if queue and S['t'] >= queue[0][1]:
                key = queue.pop(0)[0]
                o = S['origins'][key]
                S['camera'].set_actor_location(o + unreal.Vector(700, -900, 600), False, True)
                S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
                    o + unreal.Vector(700, -900, 600), o + unreal.Vector(0, 0, 40)), True)
                shoot('crush_' + key)
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
                out = {}
                for key, (x, bp, vehicle, expected) in LANES.items():
                    track = S['track'].get(key, {})
                    piece_alive = alive(S['pieces'][key])
                    crushed = track.get('crush') is not None
                    if expected == 'crush':
                        ok = crushed and not piece_alive and track.get('cross_side') is not None and \
                            abs(track['cross_side']) < 150
                    else:
                        ok = (not crushed) and piece_alive and track.get('cross_side') is not None and \
                            track.get('max_side_near', 0) > 100
                    out[key] = dict(rock=bp, vehicle=vehicle, expected=expected, passed=bool(ok), crushed=crushed,
                                    rock_alive_at_end=piece_alive, vehicle_crossed_y0_at_dx=track.get('cross_side'),
                                    vehicle_max_side_offset_near_rock=round(track.get('max_side_near', 0)),
                                    vehicle_last=track.get('last'), crush=track.get('crush'),
                                    actor_gone_s=track.get('gone_at'), samples=track.get('samples'))
                S['result']['lanes'] = out
                S['result']['paths_after'] = {k: across_paths(world, k) for k in LANES}
                for key, row in out.items():
                    log(f'{key} verdict {json.dumps({k: v for k, v in row.items() if k != "samples"})}')
                    log(f'{key} samples {row["samples"]}')
                    log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
                S['phase'], S['mark'] = 'kill', S['t']
        elif phase == 'kill':
            rock = S.get('hit_rock')
            S['phase'] = 'kill_now'
            if alive(rock):
                unreal.GameplayStatics.apply_damage(rock, 400.0, None, None, unreal.DamageType)
                hc = rock.get_component_by_class(unreal.HealthComponent)
                S['result']['hit'] = dict(health_after=round(hc.get_editor_property('CurrentHealth')) if hc else None,
                                          alive=alive(rock))
                log(f'non-lethal hit {json.dumps(S["result"]["hit"])}')
                o = rock.get_actor_location()
                S['camera'].set_actor_location(o + unreal.Vector(-500, -900, 700), False, True)
                S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
                    o + unreal.Vector(-500, -900, 700), o + unreal.Vector(0, 0, 60)), True)
                S['phase'], S['mark'] = 'hit_pic', S['t']
        elif phase == 'hit_pic' and S['t'] - S['mark'] > 1.0:
            shoot('hit_health_bar')
            S['phase'] = 'kill_now'
        elif phase == 'kill_now':
            kills = {}
            for key in KILL:
                piece = S['pieces'].get(key)
                if not alive(piece):
                    kills[key] = 'missing'
                    continue
                unreal.GameplayStatics.apply_damage(piece, 1.0e7, None, None, unreal.DamageType)
                st = piece_state(key)
                kills[key] = dict(valid=alive(piece), state=st)
                log(f'killed {key}: {json.dumps(st)}')
            S['result']['kills'] = kills
            o = S['origins']['E_kill_medium']
            S['camera'].set_actor_location(o + unreal.Vector(-300, -1000, 650), False, True)
            S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
                o + unreal.Vector(-300, -1000, 650), o + unreal.Vector(400, 0, 60)), True)
            S['phase'], S['mark'] = 'kill_pic', S['t']
        elif phase == 'kill_pic' and S['t'] - S['mark'] > 0.35:
            shoot('kill_burst')
            S['phase'], S['mark'] = 'kill_wait', S['t']
        elif phase == 'kill_wait' and S['t'] - S['mark'] > 12.0:
            world = game_world()
            for key in KILL:
                S['result']['kills'][key]['gone_12s_later'] = not alive(S['pieces'].get(key))
            S['result']['paths_after_kill'] = {k: across_paths(world, k) for k in KILL}
            log(f'kills {json.dumps(S["result"]["kills"])}')
            log(f'paths after kill {json.dumps(S["result"]["paths_after_kill"])}')
            S['result']['all_passed'] = all(r['passed'] for r in S['result']['lanes'].values()) and all(
                isinstance(v, dict) and v.get('gone_12s_later') for v in S['result']['kills'].values())
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
