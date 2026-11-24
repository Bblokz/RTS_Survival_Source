"""Throwaway-editor test of the Berlin rubble cover Blueprints (cover_build.py): cover sockets, real shells, death.
Never on Ger_Berlin, never in the shared editor. Launch with cover_run.sh <logname> [timeout_s] (same folder):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -LogCmds="LogWallHoleDamage Verbose, LogRTSCoverFinder Verbose" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
The map is never saved; the editor quits at the end.

All ten pieces stand in a row along world X at y = 0, yaw 90: their threat side (+X local) faces world +Y (sunlit).
1. COVER pieces (01, 03, 06, 07, 09, 05): a player squad (Squad_Ger_Mauser) 15 m behind each and an enemy squad
   (Rus_Squad_Mosin, player 2) 33 m in front; both sides' soldiers get 50000 health. In PIE the player squad is
   ordered (MoveToLocation) to 2 m behind the piece's bounds; once idle it takes cover automatically against the enemy.
   Checks: every socket is a published cover point (FindCoverPointsInRadius, same type, within 40 cm in 2D), paths
   into 09's pit / through 07's door (Character) and across 01 / 07 (HeavyTank), and soldiers on socket points
   (within 45 cm of the socket or, for standing cover, of its exposed peek position).
2. SHELL pieces (04, 08, 10, 02): a player Pz IV G 35 m in front of each gets AttackActor on its piece; holes are
   counted (and the RTS_WALL_HOLES hit lines in the log prove real shells); debug hits top up pieces with < 2 holes.
3. DEATH: kills 01, 09, 04 and 08 (apply_damage), pictures at 0.4 / 2 s; 9 s later: actor gone, its socket cover
   points gone from the subsystem, nav-box points back on the navmesh; soldier positions are logged before and after.
Results: Saved/Berlin/cover_test/result.json + PNGs.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/cover_test/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/'
TANK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C'
PLAYER_SQUAD = '/Game/RTS_Survival/Blueprints/Squads/Ger/Squad_Ger_Mauser.Squad_Ger_Mauser_C'
ENEMY_SQUAD = '/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C'
PIECES = {  # key: x
    '01_WallRun': -12000, '03_HeapChunk': -9800, '06_Breastwork': -7600, '07_Doorway': -5400, '09_CellarLip': -3200,
    '05_FallenChimney': -1000, '02_CornerL': 5000, '04_ToppledSlab': 7000, '08_FacadePanels': 9000, '10_MasonryBlock': 11000,
}
COVER = ['01_WallRun', '03_HeapChunk', '06_Breastwork', '07_Doorway', '09_CellarLip', '05_FallenChimney']
SHELL = ['04_ToppledSlab', '08_FacadePanels', '10_MasonryBlock', '02_CornerL']
KILL = ['01_WallRun', '09_CellarLip', '04_ToppledSlab', '08_FacadePanels']
TANK_Y = 3500.0
ENEMY_Y = 3300.0
SQUAD_START_Y = -1500.0
SQUAD_ORDER_Y = -350.0
COVER_SECONDS = 40.0
FIRE_SECONDS = 36.0
PICTURE_HOLD = 2.5
DEBUG_CALIBRES = [75, 88, 75]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'tanks': {}, 'squads': {}, 'pics': [],
     'hp': {}, 'result': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('COVERTEST ' + message)


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


def health_of(actor):
    hc = actor.get_component_by_class(unreal.HealthComponent)
    return (hc.get_editor_property('CurrentHealth'), hc.get_editor_property('MaxHealth'),
            hc.get_editor_property('DamageReductionSettings').get_editor_property('DamageReduction'))


def nav_boxes_of(actor):
    return [c for c in actor.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock_')]


def owner_of(actor):
    comp = actor.get_component_by_class(unreal.RTSComponent)
    return comp.get_editor_property('owning_player') if comp else None


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def world_pos(key, lx, ly, lz=0.0):
    """Local piece coordinates -> world; pieces stand at (x, 0) with yaw 90 (local +X = world +Y)."""
    return unreal.Vector(PIECES[key] - ly, lx, lz)


# ---------------------------------------------------------------- setup
def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, x in PIECES.items():
        z = ground_z(world, x, 0)
        cls = unreal.load_class(None, f'{BP_DIR}BP_BerlinCover_{key}.BP_BerlinCover_{key}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 90))
        if key in COVER:
            sq = actors.spawn_actor_from_class(unreal.load_class(None, PLAYER_SQUAD),
                                               unreal.Vector(x, SQUAD_START_Y, ground_z(world, x, SQUAD_START_Y) + 100),
                                               unreal.Rotator(0, 0, 90))
            en = actors.spawn_actor_from_class(unreal.load_class(None, ENEMY_SQUAD),
                                               unreal.Vector(x, ENEMY_Y, ground_z(world, x, ENEMY_Y) + 100),
                                               unreal.Rotator(0, 0, -90))
            log(f'placed {key} at x {x} with squads: player owner set {set_owner(sq, 1)} enemy owner set {set_owner(en, 2)}')
        if key in SHELL:
            tank = actors.spawn_actor_from_class(unreal.load_class(None, TANK),
                                                 unreal.Vector(x, TANK_Y, ground_z(world, x, TANK_Y) + 50),
                                                 unreal.Rotator(0, 0, -90))
            log(f'placed {key} at x {x} (ground z {z:.0f}) with a tank, owner set {set_owner(tank, 1)}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        for key in PIECES:
            if name == f'BP_BerlinCover_{key}_C':
                S['pieces'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key in SHELL:
            if abs(loc.x - PIECES[key]) < 300 and abs(loc.y - TANK_Y) < 500:
                S['tanks'][key] = tank
    squads = [q for q in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SquadController) if owner_of(q) == 1]
    for key in COVER:
        start = unreal.Vector(PIECES[key], SQUAD_START_Y, 0)
        near = [q for q in squads if math.hypot(q.get_actor_location().x - start.x, q.get_actor_location().y - start.y) < 1500]
        if near:
            S['squads'][key] = min(near, key=lambda q: math.hypot(q.get_actor_location().x - start.x,
                                                                   q.get_actor_location().y - start.y))
        else:
            log(f'{key}: no player squad near its start; player squads at '
                f'{[(round(q.get_actor_location().x), round(q.get_actor_location().y)) for q in squads]}')
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    log(f'runtime pieces {sorted(S["pieces"])} tanks {sorted(S["tanks"])} player squads {sorted(S["squads"])}')


def report_setup(key, actor):
    mesh = mesh_of(actor)
    channel = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].rstrip('>').split(':')[0]
            for n, c in (('Projectile', channel.ECC_PROJECTILE), ('TraceEnemy', channel.ECC_ENEMY),
                         ('Visibility', channel.ECC_VISIBILITY), ('Pawn', channel.ECC_PAWN))}
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    hp = health_of(actor)
    boxes = nav_boxes_of(actor)
    is_cover = unreal.MathLibrary.class_is_child_of(
        actor.get_class(), unreal.load_class(None, '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
                                                   'NaturalCover/BP_NaturalCover.BP_NaturalCover_C'))
    info = dict(health=hp, is_natural_cover=is_cover, mesh=mesh.get_editor_property('static_mesh').get_name(),
                collision=str(mesh.get_collision_enabled()), responses=resp,
                affects_nav=mesh.get_editor_property('can_ever_affect_navigation'),
                slots=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())],
                hole_mesh=holes.get_editor_property('WallMeshComponentName') if holes else None,
                nav_boxes=len(boxes), sockets=len(mesh.get_all_socket_names()),
                aims=[(round(v.x), round(v.y), round(v.z)) for v in actor.get_editor_property('AimOffsetPoints')])
    S['result'].setdefault('setup', {})[key] = info
    log(f'{key} setup {json.dumps(info, default=str)}')


def socket_points(key):
    """World location, facing and type of every cover socket of the piece's MainMesh."""
    actor = S['pieces'][key]
    mesh = mesh_of(actor)
    out = []
    for name in mesh.get_all_socket_names():
        t = mesh.get_socket_transform(name, unreal.RelativeTransformSpace.RTS_WORLD)
        n = str(name)
        kind = ('standing_right' if 'standing_right' in n else 'standing' if 'standing' in n else
                'crouch' if 'crouch' in n else 'prone' if 'prone' in n else 'cargo' if 'cargo' in n else '?')
        out.append(dict(name=n, kind=kind, loc=t.translation, fwd=t.rotation.get_axis_x()))
    return out


TYPE_ENUM = {'crouch': 'CROUCH', 'standing': 'STANDING_LEFT', 'standing_right': 'STANDING_RIGHT', 'prone': 'PRONE',
             'cargo': 'TRENCH_STAND_UP'}


def cover_subsystem(world):
    """The PIE world's cover finder (world subsystems are not reachable through a Python library here)."""
    if S.get('cover_sub') is not None and alive(S['cover_sub']):
        return S['cover_sub']
    for sub in unreal.ObjectIterator(unreal.RTSCoverFinderWorldSubsystem):
        if sub.get_outer() == world or sub.get_world() == world:
            S['cover_sub'] = sub
            return sub
    raise RuntimeError('no RTSCoverFinderWorldSubsystem in the PIE world')


def published(world, key, origin=None):
    """Cover points near the piece that match its sockets (2D within 40 cm, same type)."""
    sub = cover_subsystem(world)
    centre = origin or S['origins'][key]
    points = list(sub.find_cover_points_in_radius(centre, 800.0))
    socks = S['sockets'][key]
    matched, missing = [], []
    for s in socks:
        best = None
        for p in points:
            loc = p.get_editor_property('location')
            d = math.hypot(loc.x - s['loc'].x, loc.y - s['loc'].y)
            kind = str(p.get_editor_property('cover_type')).upper()
            if TYPE_ENUM[s['kind']] in kind and (best is None or d < best[0]):
                best = (d, loc)
        if best and best[0] <= 40.0:
            matched.append((s['name'], round(best[0]), round(s['loc'].z - best[1].z)))
        else:
            missing.append((s['name'], None if best is None else round(best[0])))
    return matched, missing, len(points)


def soldiers_near(world, centre, radius, player):
    out = []
    for unit in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SquadUnit):
        if not alive(unit):
            continue
        loc = unit.get_actor_location()
        # soldiers carry no RTSComponent of their own: the player's are the German infantry of the test squads
        is_player = unit.get_class().get_name().startswith('Ger_Inf')
        if math.hypot(loc.x - centre.x, loc.y - centre.y) <= radius and is_player == (player == 1):
            out.append(unit)
    return out


EXPOSED = {'standing': (-46.0, -110.0), 'standing_right': (-44.0, 62.0)}   # (toward cover, right), InfantryCoverSystemDesign


def hold_spots(s):
    """Where a soldier using the socket stands: on it, or for standing cover also at the exposed (peek) offset."""
    spots = [('hold', s['loc'])]
    if s['kind'] in EXPOSED:
        f = unreal.Vector(s['fwd'].x, s['fwd'].y, 0).normal()
        right = unreal.Vector(-f.y, f.x, 0)          # Up x Forward in Unreal
        toward, side = EXPOSED[s['kind']]
        spots.append(('exposed', s['loc'] + f * toward + right * side))
    return spots


def occupancy(world, key):
    socks = S['sockets'][key]
    units = soldiers_near(world, S['origins'][key], 900.0, 1)
    rows = []
    taken = set()
    for u in units:
        loc = u.get_actor_location()
        best = min(((math.hypot(loc.x - p.x, loc.y - p.y), s['name'] + ('' if how == 'hold' else '@exposed'))
                    for s in socks for how, p in hold_spots(s)), default=(1e9, None))
        holding = None
        try:
            holding = u.get_is_holding_cover_point()
        except Exception:  # noqa: BLE001
            pass
        rows.append((u.get_name()[-6:], round(best[0]), best[1], holding))
        if best[0] <= 45.0:
            taken.add(best[1].split('@')[0])
    return dict(soldiers=len(units), on_sockets=sorted(taken), rows=rows)


def toughen_test_soldiers(world):
    """The test is about cover, not the firefight: both sides' soldiers in the test rows get 50000 health so nobody
    dies before taking (or leaving) cover. Shells and grenades still land on the pieces."""
    count = 0
    for unit in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SquadUnit):
        loc = unit.get_actor_location()
        if not alive(unit) or not (-13500 < loc.x < 500 and -2500 < loc.y < 4500):
            continue
        hc = unit.get_component_by_class(unreal.HealthComponent)
        if hc is not None and hc.get_editor_property('MaxHealth') < 50000:
            hc.set_max_health(50000.0)
            hc.set_current_health(50000.0)
            count += 1
    log(f'toughened {count} test soldiers')


# ---------------------------------------------------------------- navigation
NAV_FILTERS = '/Game/RTS_Survival/Blueprints/Nav/Filters/'


def nav_filter(agent='Characters'):
    """The per-agent query filter the units use. Since the pieces became crushable (2026-11-13) their nav boxes are
    nav AREAS (RTS_NavArea_BlockCharLight / ...LightMedium), not holes: only a filtered query sees them as blocked."""
    return unreal.load_class(None, f'{NAV_FILTERS}RTS_NavFilter_{agent}.RTS_NavFilter_{agent}_C')


def on_navmesh(world, point, extent=(20.0, 20.0, 80.0)):
    nav = unreal.NavigationSystemV1
    fn = getattr(nav, 'k2_project_point_to_navigation', None) or getattr(nav, 'project_point_to_navigation', None)
    try:
        result = fn(world, point, None, nav_filter(), unreal.Vector(*extent))
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'
    if isinstance(result, tuple):
        return bool(result[0])
    return bool(result) if isinstance(result, bool) else result is not None


def box_points(actor):
    pts = []
    if not alive(actor):
        return pts
    for box in nav_boxes_of(actor):
        c = box.get_world_location()
        pts.append(unreal.Vector(c.x, c.y, S['origins_z'].get(actor.get_name(), c.z) + 20))
    return pts


def heavy_navmesh(world):
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        if 'HeavyTank' in nav.get_name():
            return nav
    return None


def path(world, a, b, nav_data=None):
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, a, b, nav_data, nav_filter('HeavyTanks' if nav_data is not None else 'Characters'))
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'[:80]
    if not pts:
        return 'no path'
    length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))
    return f'{length:.0f} cm (straight {(b - a).length():.0f}, partial {p.is_partial()})'


def path_checks(world):
    """Default (Character) navmesh: into 09's pit through the breach, through 07's door; HeavyTank: across 01 and 07."""
    z = lambda k: S['origins'][k].z + 20
    heavy = heavy_navmesh(world)
    out = {
        '09_into_pit_from_behind': path(world, world_pos('09_CellarLip', -650, 0, z('09_CellarLip')),
                                        world_pos('09_CellarLip', 0, 0, z('09_CellarLip'))),
        '09_into_pit_via_breach_side': path(world, world_pos('09_CellarLip', 0, 650, z('09_CellarLip')),
                                            world_pos('09_CellarLip', 0, 0, z('09_CellarLip'))),
        '07_through_door': path(world, world_pos('07_Doorway', -400, 10, z('07_Doorway')),
                                world_pos('07_Doorway', 400, 10, z('07_Doorway'))),
        '01_across_character': path(world, world_pos('01_WallRun', -450, 0, z('01_WallRun')),
                                    world_pos('01_WallRun', 450, 0, z('01_WallRun'))),
        '01_across_heavytank': path(world, world_pos('01_WallRun', -450, 0, z('01_WallRun')),
                                    world_pos('01_WallRun', 450, 0, z('01_WallRun')), heavy),
        '07_through_door_heavytank': path(world, world_pos('07_Doorway', -400, 10, z('07_Doorway')),
                                          world_pos('07_Doorway', 400, 10, z('07_Doorway')), heavy),
    }
    log(f'paths {json.dumps(out)} (heavy navmesh {heavy.get_name() if heavy else None})')
    return out


def nav_state(world, key, points):
    on = [on_navmesh(world, p) for p in points]
    return f'{sum(1 for v in on if v is True)}/{len(points)} nav-box centres walkable for infantry (Characters filter)'


# ---------------------------------------------------------------- fire
def order_fire(key):
    tank, piece = S['tanks'].get(key), S['pieces'].get(key)
    if tank is None or piece is None:
        log(f'{key}: no tank or piece to fire')
        return
    try:
        result = tank.attack_actor(piece, True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'[:120]
    log(f'{key}: AttackActor -> {result}')


def watch():
    for key, piece in S['pieces'].items():
        if not alive(piece):
            continue
        holes = piece.get_component_by_class(unreal.WallHoleDamageComponent)
        state = (round(health_of(piece)[0]), holes.get_hole_count() if holes else -1)
        if S['hp'].get(key) != state:
            S['hp'][key] = state
            log(f'{key}: health {state[0]} holes {state[1]} slot0 {mesh_of(piece).get_material(0).get_name()}')


def debug_hits(key):
    piece = S['pieces'][key]
    mesh = mesh_of(piece)
    holes = piece.get_component_by_class(unreal.WallHoleDamageComponent)
    transform = piece.get_actor_transform()
    for index, aim in enumerate(piece.get_editor_property('AimOffsetPoints')):
        target = transform.transform_location(aim)
        start = target + unreal.Vector(300 * (index - 1), 2500, 150)
        direction = (target - start).normal()
        hit = mesh.line_trace_component(start, start + direction * 5000, False, False, False)
        if not hit:
            log(f'{key} debug aim {index}: missed the collision')
            continue
        removed = holes.debug_fire_test_hit(hit[0], direction, float(DEBUG_CALIBRES[index % 3]))
        local = transform.inverse_transform_location(hit[0])
        log(f'{key} DEBUG hit aim {index} at ({local.x:.0f},{local.y:.0f},{local.z:.0f}) removed {removed:.2f} m2')
    log(f'{key}: after debug hits holes {holes.get_hole_count()} slot0 {mesh.get_material(0).get_name()}')


# ---------------------------------------------------------------- pictures
def place_camera(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def back_view(key, dist=1500.0, height=1300.0, side=500.0):
    """RTS-ish view from behind the piece (the socket side, world -Y) and a little to the side."""
    o = S['origins'][key]
    return o + unreal.Vector(side, -dist, height), o + unreal.Vector(0, 100, 60)


def front_view(key, dist=1100.0, height=650.0, side=-400.0):
    o = S['origins'][key]
    return o + unreal.Vector(side, dist, height), o + unreal.Vector(0, 0, 70)


def queue(prefix, keys, view):
    for key in keys:
        if key in S['pieces'] and alive(S['pieces'][key]):
            S['pics'].append((f'{prefix}_{key}', view(key)))


def picture(name):
    S['busy'] = True
    try:
        unreal.SystemLibrary.execute_console_command(game_world(), 'ShowFlag.PostProcessMaterial 0')
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def run_pictures(next_phase):
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
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
                S['origins_z'] = {p.get_name(): p.get_actor_location().z for p in S['pieces'].values()}
                S['sockets'] = {k: socket_points(k) for k in S['pieces']}
                for key, piece in S['pieces'].items():
                    report_setup(key, piece)
                toughen_test_soldiers(world)
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if nav_building(world) and S['t'] - S['mark'] < 40.0:
                return
            log(f'nav build idle after {S["t"] - S["mark"]:.1f} s')
            reg = {}
            for key in PIECES:
                if not alive(S['pieces'][key]):
                    log(f'{key}: already dead before the registration check')
                    continue
                matched, missing, total = published(world, key)
                reg[key] = dict(matched=len(matched), sockets=len(S['sockets'][key]), missing=missing,
                                nav_offset_cm=[m[1] for m in matched], points_in_8m=total)
                log(f'{key}: sockets published {len(matched)}/{len(S["sockets"][key])} missing {missing} '
                    f'(2D offsets {[m[1] for m in matched]}, dz {[m[2] for m in matched]})')
            S['result']['registered'] = reg
            S['navpts'] = {k: box_points(p) for k, p in S['pieces'].items()}
            S['result']['nav_alive'] = {k: nav_state(world, k, pts) for k, pts in S['navpts'].items()}
            S['result']['paths'] = path_checks(world)
            log(f'nav alive {S["result"]["nav_alive"]}')
            S['pics'].append(('_warmup', back_view('07_Doorway')))
            queue('undamaged', list(PIECES), front_view)
            S['phase'], S['mark'] = 'pics_undamaged', S['t']
        elif phase == 'pics_undamaged':
            watch()
            run_pictures('order')
        elif phase == 'order':
            toughen_test_soldiers(game_world())
            for key in SHELL:
                order_fire(key)
            for key, squad in S['squads'].items():
                lo = mesh_of(S['pieces'][key]).get_editor_property('static_mesh').get_bounding_box().min
                target = world_pos(key, lo.x - 200.0, 0.0, S['origins'][key].z + 20)
                try:
                    result = squad.move_to_location(target, True, unreal.Rotator(0, 0, 90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: player squad MoveToLocation {target} -> {result}')
            S['phase'], S['mark'] = 'battle', S['t']
        elif phase == 'battle':
            watch()
            elapsed = S['t'] - S['mark']
            for at in (8, 15, 25):
                if elapsed > at and not S.get(f'occ_{at}'):
                    S[f'occ_{at}'] = True
                    S['result'][f'occupancy_{at}s'] = {k: occupancy(game_world(), k) for k in COVER}
                    log(f'occupancy {at} s {json.dumps(S["result"][f"occupancy_{at}s"], default=str)}')
                    units = [u for u in unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.SquadUnit)
                             if alive(u) and u.get_class().get_name().startswith('Ger_Inf')]
                    log(f'player soldiers alive {len(units)} at '
                        f'{[(round(u.get_actor_location().x), round(u.get_actor_location().y)) for u in units]}')
            if elapsed > max(COVER_SECONDS, FIRE_SECONDS):
                world = game_world()
                S['result']['occupancy'] = {k: occupancy(world, k) for k in COVER}
                log(f'occupancy {COVER_SECONDS:.0f} s {json.dumps(S["result"]["occupancy"], default=str)}')
                holes = {}
                for key in SHELL:
                    piece = S['pieces'][key]
                    if not alive(piece):
                        holes[key] = 'died under fire'
                        continue
                    hc = piece.get_component_by_class(unreal.WallHoleDamageComponent)
                    holes[key] = dict(holes=hc.get_hole_count(), health=round(health_of(piece)[0]),
                                      slot0=mesh_of(piece).get_material(0).get_name(),
                                      slot1=mesh_of(piece).get_material(1).get_name())
                    log(f'{key}: after real fire {holes[key]}')
                    if hc.get_hole_count() < 2:
                        debug_hits(key)
                S['result']['real_fire'] = holes
                for tank in S['tanks'].values():
                    try:
                        tank.stop_command()
                    except Exception:  # noqa: BLE001
                        pass
                queue('cover', COVER, back_view)
                queue('cover_close', ['01_WallRun', '09_CellarLip', '07_Doorway'],
                      lambda k: back_view(k, dist=700.0, height=600.0, side=250.0))
                queue('holes', SHELL, front_view)
                S['phase'], S['mark'] = 'pics_battle', S['t']
        elif phase == 'pics_battle':
            watch()
            run_pictures('kill')
        elif phase == 'kill':
            S['kill_queue'] = [k for k in KILL if alive(S['pieces'].get(k))]
            S['phase'], S['mark'] = 'kill_next', S['t']
        elif phase == 'kill_next':
            if not S['kill_queue']:
                S['phase'], S['mark'] = 'after', S['t']
                return
            key = S['kill_queue'].pop(0)
            piece = S['pieces'][key]
            S.setdefault('occ_before_kill', {})[key] = occupancy(game_world(), key) if key in COVER else None
            place_camera(*back_view(key, dist=1300, height=1000))
            unreal.GameplayStatics.apply_damage(piece, 1.0e7, None, None, unreal.DamageType)
            log(f'killed {key}: valid {alive(piece)} lifespan {piece.get_life_span():.1f} '
                f'mesh visible {mesh_of(piece).is_visible()} collision {mesh_of(piece).get_collision_enabled()}')
            S['death_key'], S['death_shots'] = key, [0.4, 2.0]
            S['phase'], S['mark'] = 'death_pics', S['t']
        elif phase == 'death_pics':
            if S['death_shots'] and S['t'] - S['mark'] >= S['death_shots'][0]:
                at = S['death_shots'].pop(0)
                picture(f'death_{S["death_key"]}_{str(at).replace(".", "_")}s')
            elif not S['death_shots'] and S['t'] - S['mark'] > 2.0 + PICTURE_HOLD:
                S['phase'], S['mark'] = 'kill_next', S['t']
        elif phase == 'after' and S['t'] - S['mark'] > 9.0:
            world = game_world()
            after = {}
            for key in KILL:
                gone = not alive(S['pieces'][key])
                matched, missing, total = published(world, key, S['origins'][key])
                row = dict(actor_gone=gone, socket_points_left=len(matched), nav=nav_state(world, key, S['navpts'][key]))
                if key in COVER:
                    row['occupancy_after'] = occupancy(world, key)
                    row['occupancy_before'] = S['occ_before_kill'].get(key)
                after[key] = row
                log(f'{key} after death: {json.dumps(row, default=str)}')
            for key in PIECES:
                if key not in KILL:
                    after.setdefault('alive_nav', {})[key] = nav_state(world, key, S['navpts'][key])
            S['result']['after_death'] = after
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 480.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
