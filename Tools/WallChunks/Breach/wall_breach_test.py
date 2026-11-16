"""Throwaway-editor test of the Rus wall field holes + tank breaches (never the shared editor, never a mission map).
Launch: sh D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Breach/run_breach_test.sh <logname> [timeout_s]
Lanes along world Y; each tank starts 24 m before its wall and is ordered 24 m past it:
  A Rus_Wall_Concrete_Long        Pz IV G (medium)   -> breach, tank crosses
  B Rus_Wall_Concrete_LongSandBags Tiger H1 (heavy)  -> breach, tank crosses
  C Rus_Wall_Concrete_Long_Soviet Luchs (light)      -> no breach, tank paths round the wall end
  D Rus_Wall_Concrete_Corner      Pz IV G (medium)   -> breach in the corner's first leg
Fire walls (debug hits, no health cap): E long wall, 45 clustered hits; F corner, hits on both legs. Then wall A takes
lethal damage after its breach (collapse). Results: Saved/WallChunks/Breach/<run>/result.json + PNGs.
"""
import json
import math
import os
import random
import unreal

RUN = os.environ.get('BREACH_RUN', 'run')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/{RUN}/'
OUT = ROOT + 'result.json'
WALLS = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
# key: (lane x, wall BP, actor x offset so the wall's middle sits on the lane, tank, expected)
LANES = {
    'A_long_medium': (-9000, os.environ.get('BREACH_LONG', 'Rus_Wall_Concrete_Long'), 1130, 'PzIV_G', 'breach'),
    'B_sandbags_heavy': (-3000, 'Rus_Wall_Concrete_LongSandBags', 1123, 'TigerH1', 'breach'),
    'C_soviet_light': (3000, os.environ.get('BREACH_LIGHT_WALL', 'Rus_Wall_Concrete_Long_Soviet'), 1130, 'Luchs',
                       'detour'),
    'D_corner_medium': (9000, 'Rus_Wall_Concrete_Corner', 570, 'PzIV_G', 'breach'),
    # A Soviet tank never breaks (or plans through) a Soviet wall.
    'G_soviet_own_wall': (-6000, 'Rus_Wall_Concrete_Long', 1130, 'PzIV_G', 'own_side'),
    # Two long walls end to end, the tank 3.5 m from the joint: the neighbour holds that end up (no free-end fall).
    'H_chain_medium': (15000, 'Rus_Wall_Concrete_Long', 350, 'PzIV_G', 'breach'),
}
CHAIN_NEIGHBOUR_X = 15000 + 350 + 2260
# A second Pz IV 5.6 m beside the J lane tank on the same wall: the pier left between the two gaps is too narrow to
# stand and must fall (a sight line through its middle passes afterwards).
LANES['J_pier_medium'] = (21000, 'Rus_Wall_Concrete_Long', 1130, 'PzIV_G', 'breach')
PIER_TANK_DX = 560
WALL_OWNER = 2
# Lanes off the main row (y of the wall); the Soviet lane is far from the player's tanks, which would shoot it.
LANE_Y = {'G_soviet_own_wall': -14000.0}


def lane_y(key):
    return LANE_Y.get(key, 0.0)
TANK_OWNER = {'G_soviet_own_wall': 2}
FILTER_BPS = ['RTS_NavFilter_Characters', 'RTS_NavFilter_LightTanks', 'RTS_NavFilter_MediumTanks',
              'RTS_NavFilter_HeavyTanks']
FIRE_Y = 7000.0
FIRE = {'E_long_fire': (-3000, os.environ.get('BREACH_LONG', 'Rus_Wall_Concrete_Long'), 1130), 'F_corner_fire': (4000, 'Rus_Wall_Concrete_Corner', 570)}
START_Y = 2400.0
# Every configured wall Blueprint once, on a far row: field mode, breach component and wall nav.
ALL_WALLS = ['Rus_Wall_Concrete_Corner', 'Rus_Wall_Concrete_CornerNoFlag', 'Rus_Wall_Concrete_CornerNoFlag_Soviet',
             'Rus_Wall_Concrete_Long', 'Rus_Wall_Concrete_Long_Holes', 'Rus_Wall_Concrete_Long_Soviet',
             'Rus_Wall_Concrete_LongSandBags', 'Rus_Wall_Concrete_LongSandBags_Soviet',
             'Rus_Wall_Concrete_LongSandBagsNoFlag']
ALL_WALLS_Y = -24000.0
# Real fire, far from the lanes: H a player Pz IV shoots a Soviet wall; I a Soviet Pz IV shoots a player Luchs parked
# right in front of a Soviet wall (splash must not hurt the wall).
FIRE_ROW_Y = -10000.0
REAL_FIRE = {'H': dict(wall_x=-12000.0, shooter=('PzIV_G', 1, -12000.0, 1800.0)),
             'I': dict(wall_x=12000.0, shooter=('PzIV_G', 2, 12000.0, 2200.0), target=('Luchs', 1, 12000.0, 260.0))}
MOVE_SECONDS = 70.0
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


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def reparent_filters():
    """In memory only: the nav filters on the per-side filter class (breachable wall areas, wall gates)."""
    parent = unreal.RTSWallGateNavQueryFilter.static_class()
    for name in FILTER_BPS:
        bp = unreal.load_asset(f'/Game/RTS_Survival/Blueprints/Nav/Filters/{name}')
        ok = unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'filter {name} reparented {ok} -> {bp.generated_class().get_super_class().get_name() if hasattr(bp.generated_class(), "get_super_class") else "?"}')


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, bp, offset, vehicle, _) in LANES.items():
        cls = unreal.load_class(None, f'{WALLS}{bp}.{bp}_C')
        y0 = lane_y(key)
        wall = actors.spawn_actor_from_class(cls, unreal.Vector(x + offset, y0, ground_z(world, x, y0 + 1500)),
                                             unreal.Rotator(0, 0, 0))
        set_owner(wall, WALL_OWNER)
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, y0 + START_Y, ground_z(world, x, y0 + START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        owner = TANK_OWNER.get(key, 1)
        log(f'{key}: placed {bp} (owner {WALL_OWNER}) and {vehicle} (owner {owner}) set {set_owner(tank, owner)}')
        if key == 'J_pier_medium':
            second = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                                   unreal.Vector(x + PIER_TANK_DX, y0 + START_Y,
                                                                 ground_z(world, x + PIER_TANK_DX, y0 + START_Y) + 50),
                                                   unreal.Rotator(0, 0, -90))
            set_owner(second, 1)
        if key == 'H_chain_medium':
            neighbour = actors.spawn_actor_from_class(cls, unreal.Vector(CHAIN_NEIGHBOUR_X, y0, ground_z(world, x, y0 + 1500)),
                                                      unreal.Rotator(0, 0, 0))
            set_owner(neighbour, WALL_OWNER)
    for key, (x, bp, offset) in FIRE.items():
        cls = unreal.load_class(None, f'{WALLS}{bp}.{bp}_C')
        wall = actors.spawn_actor_from_class(cls, unreal.Vector(x + offset, FIRE_Y, ground_z(world, x, FIRE_Y + 1500)),
                                             unreal.Rotator(0, 0, 0))
        set_owner(wall, 1)
    for key, cfg in REAL_FIRE.items():
        x = cfg['wall_x']
        cls = unreal.load_class(None, f'{WALLS}Rus_Wall_Concrete_Long_Holes.Rus_Wall_Concrete_Long_Holes_C')
        wall = actors.spawn_actor_from_class(cls, unreal.Vector(x + 1130, FIRE_ROW_Y,
                                                                ground_z(world, x, FIRE_ROW_Y + 1500)),
                                             unreal.Rotator(0, 0, 0))
        set_owner(wall, WALL_OWNER)
        for role in ('shooter', 'target'):
            if role not in cfg:
                continue
            vehicle, owner, ux, dy = cfg[role]
            unit = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                                 unreal.Vector(ux, FIRE_ROW_Y + dy,
                                                               ground_z(world, ux, FIRE_ROW_Y + dy) + 50),
                                                 unreal.Rotator(0, 0, -90))
            set_owner(unit, owner)
    for index, bp in enumerate(ALL_WALLS):
        cls = unreal.load_class(None, f'{WALLS}{bp}.{bp}_C')
        x = -12000 + index * 3000
        wall = actors.spawn_actor_from_class(cls, unreal.Vector(x, ALL_WALLS_Y, ground_z(world, x, ALL_WALLS_Y + 1500)),
                                             unreal.Rotator(0, 0, 0))
        wall.set_actor_label('ALLWALL_' + bp)
        set_owner(wall, WALL_OWNER)
    scout = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES['Luchs']),
                                          unreal.Vector(0, FIRE_Y + 2500, ground_z(world, 0, FIRE_Y + 2500) + 50),
                                          unreal.Rotator(0, 0, 0))
    set_owner(scout, 1)
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 2000))
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 9100, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, bp, offset, _, _) in LANES.items():
            if name == bp + '_C' and abs(loc.x - x - offset) < 50 and abs(loc.y - lane_y(key)) < 50:
                S['walls'][key] = actor
        for key, (x, bp, offset) in FIRE.items():
            if name == bp + '_C' and abs(loc.x - x - offset) < 50 and abs(loc.y - FIRE_Y) < 50:
                S['walls'][key] = actor
        if name.startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
            log('fog post process off')
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - lane_y(key) - START_Y) < 500:
                S['tanks'][key] = tank
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        loc = actor.get_actor_location()
        if actor.get_class().get_name() == 'Rus_Wall_Concrete_Long_C' and abs(loc.x - CHAIN_NEIGHBOUR_X) < 50                 and abs(loc.y) < 50:
            S['chain_neighbour'] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        if abs(loc.x - LANES['J_pier_medium'][0] - PIER_TANK_DX) < 150 and abs(loc.y - START_Y) < 500:
            S['pier_tank'] = tank
    captures = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)
    S['capture'] = captures[0] if captures else None
    if S['capture'] is not None:
        comp = S['capture'].capture_component2d
        S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                                 unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp.set_editor_property('texture_target', S['rt'])
        comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property('capture_every_frame', False)
        comp.set_editor_property('capture_on_movement', False)
        comp.set_editor_property('fov_angle', 70.0)
        S['capture'].root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    S['real'] = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        loc = actor.get_actor_location()
        for key, cfg in REAL_FIRE.items():
            if actor.get_class().get_name() == 'Rus_Wall_Concrete_Long_Holes_C' and                     abs(loc.x - cfg['wall_x'] - 1130) < 50 and abs(loc.y - FIRE_ROW_Y) < 50:
                S['real'].setdefault(key, {})['wall'] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, cfg in REAL_FIRE.items():
            for role in ('shooter', 'target'):
                if role in cfg and abs(loc.x - cfg[role][2]) < 200 and abs(loc.y - FIRE_ROW_Y - cfg[role][3]) < 200:
                    S['real'].setdefault(key, {})[role] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 2000)).length())
    log(f'runtime walls {sorted(S["walls"])} tanks {sorted(S["tanks"])}')


def holes_of(wall):
    return wall.get_component_by_class(unreal.WallHoleDamageComponent) if alive(wall) else None


def wall_mesh(wall):
    return [c for c in wall.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def wall_state(key):
    wall = S['walls'].get(key)
    holes = holes_of(wall)
    if holes is None:
        return None
    mesh = wall_mesh(wall)
    extras = {c.get_name(): bool(c.is_visible()) for c in wall.get_components_by_class(unreal.StaticMeshComponent)
              if c.get_name() != 'StaticMesh'}
    return dict(extras=extras, holes=holes.get_hole_count(), breaches=holes.get_breach_count(),
                removed=round(holes.get_removed_face_share(), 3), health=round(wall.get_editor_property('health')),
                material=mesh.get_material(0).get_class().get_name() if mesh.get_material(0) else None,
                visible=bool(mesh.is_visible()))


def setup_report(key):
    wall = S['walls'][key]
    mesh = wall_mesh(wall)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('EnemyObj', ch.ECC_ENEMY_OBJ),
                         ('Projectile', ch.ECC_PROJECTILE))}
    breach = wall.get_component_by_class(unreal.WallTankBreachComponent)
    navs = [(c.get_name(), c.get_editor_property('area_class').get_name() if c.get_editor_property('area_class') else None)
            for c in wall.get_components_by_class(unreal.NavModifierComponent)]
    children = [c.get_editor_property('child_actor_class').get_name()
                for c in wall.get_components_by_class(unreal.ChildActorComponent)
                if c.get_editor_property('child_actor_class')]
    return dict(cls=wall.get_class().get_name(), responses=resp, overlap_events=bool(mesh.get_editor_property(
        'generate_overlap_events')), affects_nav=bool(mesh.get_editor_property('can_ever_affect_navigation')),
                breach_component=breach is not None, nav_modifiers=navs, nav_children=children,
                state=wall_state(key))


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
                through=bool(side < 260 and not p.is_partial()),
                points=[(round(pt.x - a.x), round(pt.y)) for pt in pts])


def across_paths(world, key):
    """Paths across the wall for real units (the filter decides per querier and side): player medium, heavy and
    light tanks, the Soviet medium tank, and infantry (no unit: every breachable wall is closed)."""
    x = LANES[key][0]
    z = S['ground'][key] + 20
    y0 = lane_y(key)
    a, b = unreal.Vector(x, y0 + 900, z), unreal.Vector(x, y0 - 900, z)
    queriers = {'PlayerMedium': ('MediumTank', S['tanks'].get('A_long_medium')),
                'PlayerHeavy': ('HeavyTank', S['tanks'].get('B_sandbags_heavy')),
                'PlayerLight': ('LightTank', S['tanks'].get('C_soviet_light')),
                'SovietMedium': ('MediumTank', S['tanks'].get('G_soviet_own_wall')),
                'Character': ('Character', None)}
    out = {}
    for label, (agent, unit) in queriers.items():
        context = unit.get_controller() if unit is not None and alive(unit) else None
        out[label] = path(world, a, b, context if context is not None else S['nav'][agent],
                          unreal.load_class(None, FILTERS[agent]))
    return out


def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def shoot(name):
    # A scene capture in the PIE world: screenshots rendered the editor viewport or the RTS camera pawn instead.
    cam = S['camera']
    capture = S['capture']
    capture.set_actor_location(cam.get_actor_location(), False, True)
    capture.set_actor_rotation(cam.get_actor_rotation(), True)
    capture.capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), name + '.png')
    log(f'picture {name} at {cam.get_actor_location()}')
    log(f'picture {name}')


def queue_picture(name, at, eye, look):
    S['pics'].append((at, name, eye, look))
    S['pics'].sort(key=lambda row: row[0])


def lane_view(key, side=1.0):
    x = LANES[key][0]
    z = S['ground'][key]
    return unreal.Vector(x + 1100, side * 1500, z + 750), unreal.Vector(x, 0, z + 180)


def check_all_walls(world):
    """Each configured Blueprint: breach component, nav replaced by the breachable area, and a debug hit that swaps in
    the field material (a dynamic instance) and cuts a hole."""
    out = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()[:-2]
        loc = actor.get_actor_location()
        if name not in ALL_WALLS or abs(loc.y - ALL_WALLS_Y) > 50:
            continue
        holes = holes_of(actor)
        breach = actor.get_component_by_class(unreal.WallTankBreachComponent)
        mesh = wall_mesh(actor)
        centre, box, _ = unreal.SystemLibrary.get_component_bounds(mesh)
        # Strike the long face of the mesh bounds, a third of the way up; an L corner's bounds centre is the empty
        # inside of the L, so a corner is struck on its first leg (along -X from the actor, y 0..20).
        if 'Corner' in name:
            origin = actor.get_actor_location()
            hit, direction = unreal.Vector(origin.x - 300, origin.y - 30, origin.z + 160), unreal.Vector(0, 1, 0)
        elif box.x >= box.y:
            hit, direction = unreal.Vector(centre.x, centre.y + box.y + 20, centre.z - box.z * 0.3), unreal.Vector(0, -1, 0)
        else:
            hit, direction = unreal.Vector(centre.x + box.x + 20, centre.y, centre.z - box.z * 0.3), unreal.Vector(-1, 0, 0)
        removed = holes.debug_fire_test_hit(hit, direction, 75) if holes else None
        navs = sorted({c.get_editor_property('area_class').get_name()
                       for c in actor.get_components_by_class(unreal.RTSNavCollisionEnvObstacle)
                       if c.get_editor_property('area_class')})
        out[name] = dict(breach=breach is not None, holes=holes.get_hole_count() if holes else None,
                         removed_m2=removed, material=mesh.get_material(0).get_class().get_name(), nav=navs,
                         ok=bool(breach and holes and holes.get_hole_count() > 0
                                 and mesh.get_material(0).get_class().get_name() == 'MaterialInstanceDynamic'
                                 and navs == ['NavArea_BreachableWallEnemyOwned']))
    return out


def friendly_fire_checks():
    """Damage from the Soviet tank must not touch the Soviet wall; damage from a player tank must. A point hit inside a
    fallen breach (lane A) flies through."""
    out = {}
    wall = S['walls'].get('G_soviet_own_wall')
    soviet = S['tanks'].get('G_soviet_own_wall')
    player = S['tanks'].get('A_long_medium')
    if alive(wall) and alive(soviet) and alive(player):
        before = wall.get_editor_property('health')
        unreal.GameplayStatics.apply_damage(wall, 300.0, None, soviet, unreal.DamageType)
        after_soviet = wall.get_editor_property('health')
        unreal.GameplayStatics.apply_damage(wall, 300.0, None, player, unreal.DamageType)
        after_player = wall.get_editor_property('health')
        out['own_side'] = dict(before=before, after_soviet=after_soviet, after_player=after_player,
                               ok=after_soviet == before and after_player < after_soviet)
    breached = S['walls'].get('A_long_medium')
    if alive(breached) and alive(player):
        x = LANES['A_long_medium'][0]
        point = unreal.Vector(x, -10, S['ground']['A_long_medium'] + 150)
        before = breached.get_editor_property('health')
        hit = unreal.HitResult()
        unreal.GameplayStatics.apply_point_damage(breached, 200.0, unreal.Vector(0, -1, 0), unreal.HitResult(
            location=point, impact_point=point), None, player, unreal.DamageType)
        out['through_breach'] = dict(before=before, after=breached.get_editor_property('health'),
                                     ok=breached.get_editor_property('health') == before)
        # Sight passes the fallen gap and still stops at the standing wall beside it.
        z = S['ground']['A_long_medium'] + 150
        def wall_hit(tx):
            hit = unreal.SystemLibrary.line_trace_single(game_world(), unreal.Vector(tx, 400, z), unreal.Vector(tx, -400, z),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            if hit is None:
                return False
            values = hit.to_tuple()
            return bool(values[0]) and values[9] == breached if len(values) > 9 else bool(values[0])
        gap_blocked = wall_hit(x)
        side_blocked = wall_hit(x + 800)
        out['sight_gap'] = dict(gap_blocked=gap_blocked, side_blocked=side_blocked, ok=(not gap_blocked) and side_blocked)
    return out


def fire_hits():
    """E: 45 hits clustered over a 6 x 3 m patch (the old 6-slot mask merged these); F: hits on both corner legs."""
    random.seed(7)
    out = {}
    wall = S['walls']['E_long_fire']
    holes = holes_of(wall)
    z = S['fire_ground'] + 0
    removed = 0.0
    # 30 hits spread over the wall, then a tight cluster of 15 in a 3 x 2 m patch.
    spots = [(-3000 + random.uniform(-1000, 1000), random.uniform(60, 440)) for _ in range(30)]
    spots += [(-2600 + random.uniform(-150, 150), random.uniform(150, 350)) for _ in range(15)]
    for x, height in spots:
        hz = z + height
        removed += holes.debug_fire_test_hit(unreal.Vector(x, FIRE_Y + 30, hz), unreal.Vector(0, -1, 0),
                                            random.choice([37, 50, 75, 88]))
    out['E_long_fire'] = dict(state=wall_state('E_long_fire'), removed_m2=round(removed, 2))
    corner = S['walls']['F_corner_fire']
    holes = holes_of(corner)
    removed = 0.0
    origin = corner.get_actor_location()
    for i in range(14):
        # leg A along -X from the actor (y 0..20): shots from -Y; leg B along +Y at x -1140..-1120: shots from +X side
        removed += holes.debug_fire_test_hit(unreal.Vector(origin.x - 150 - i * 65, origin.y - 30, z + 120 + (i % 4) * 80),
                                            unreal.Vector(0, 1, 0), 50 + (i % 3) * 25)
        removed += holes.debug_fire_test_hit(unreal.Vector(origin.x - 1100, origin.y + 150 + i * 65,
                                                           z + 120 + ((i + 2) % 4) * 80),
                                            unreal.Vector(-1, 0, 0), 50 + (i % 3) * 25)
    out['F_corner_fire'] = dict(state=wall_state('F_corner_fire'), removed_m2=round(removed, 2))
    # A small shell on the BACK face of a sandbag wall (115 cm thick, no through-hole): it must cut and count.
    for actor in unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.Actor):
        if actor.get_class().get_name() == 'Rus_Wall_Concrete_LongSandBagsNoFlag_C'                 and abs(actor.get_actor_location().y - ALL_WALLS_Y) < 50:
            holes = holes_of(actor)
            centre, box, _ = unreal.SystemLibrary.get_component_bounds(wall_mesh(actor))
            before = holes.get_hole_count()
            removed = holes.debug_fire_test_hit(unreal.Vector(centre.x + 300, centre.y - box.y - 20, centre.z),
                                               unreal.Vector(0, 1, 0), 37)
            out['back_face'] = dict(removed_m2=round(removed, 3), holes_before=before, holes=holes.get_hole_count(),
                                    share=round(holes.get_removed_face_share(), 4),
                                    ok=bool(removed > 0 and holes.get_hole_count() > before))
    return out


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, last=None, breach_at=None,
                                                states=[]))
        if alive(tank):
            loc = tank.get_actor_location() - unreal.Vector(0, lane_y(key), 0)
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 500:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < -300:
                track['cross_side'] = round(loc.x - x)
                track['cross_time'] = round(S['t'] - S['order_time'], 1)
        state = wall_state(key)
        if state is None:
            continue
        if state['breaches'] > 0 and track['breach_at'] is None:
            track['breach_at'] = round(S['t'] - S['order_time'], 2)
            log(f'{key}: first breach {json.dumps(state)} tank at {track["last"]}')
            if key in ('A_long_medium', 'B_sandbags_heavy', 'D_corner_medium'):
                eye, look = lane_view(key)
                queue_picture(f'{key}_push', S['t'] + 0.2, eye, look)
                queue_picture(f'{key}_fallen', S['t'] + 1.4, eye, look)
                queue_picture(f'{key}_after', S['t'] + 6.0, eye, look)
                back_eye, _ = lane_view(key, -1.0)
                queue_picture(f'{key}_after_back', S['t'] + 6.5, back_eye, look)
                x0, z0 = LANES[key][0], S['ground'][key]
                queue_picture(f'{key}_rts', S['t'] + 7.0, unreal.Vector(x0 - 900, 2600, z0 + 3400),
                              unreal.Vector(x0, 0, z0))
        if len(track['states']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.5:
            track['sample_t'] = S['t']
            track['states'].append((round(S['t'] - S['order_time'], 1), state['breaches'], state['removed'],
                                    state['health'], track['last']))


def tick(delta):
    S['t'] += delta
    # Spawning Blueprints and capturing can re-enter the Slate tick; run one step at a time.
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
            if os.environ.get('BREACH_REPARENT', '1') == '1':
                reparent_filters()
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
                # Traced beside the walls: a trace on the wall line lands on the wall's top.
                S['ground'] = {k: ground_z(world, LANES[k][0], lane_y(k) + 1500) for k in LANES}
                S['fire_ground'] = ground_z(world, -3000, FIRE_Y + 1500)
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            S['nav'] = navmeshes(world)
            S['result']['setup'] = {k: setup_report(k) for k in S['walls']}
            for key in S['walls']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
            S['result']['paths_before'] = {k: across_paths(world, k) for k in LANES}
            for key in LANES:
                state = wall_state(key)
                S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, last=None, breach_at=None,
                                                states=[]))['health0'] = state['health'] if state else None
            for key in LANES:
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            S['result']['tanks'] = {k: t.get_class().get_name() for k, t in S['tanks'].items()}
            S['result']['all_walls'] = check_all_walls(world)
            log(f'all walls {json.dumps(S["result"]["all_walls"])}')
            S['result']['fire'] = fire_hits()
            log(f'fire {json.dumps(S["result"]["fire"])}')
            fz = S['fire_ground']
            queue_picture('E_fire_front', S['t'] + 1.5, unreal.Vector(-2700, FIRE_Y + 900, fz + 350),
                          unreal.Vector(-3000, FIRE_Y, fz + 230))
            queue_picture('E_fire_back', S['t'] + 2.5, unreal.Vector(-2700, FIRE_Y - 900, fz + 350),
                          unreal.Vector(-3000, FIRE_Y, fz + 230))
            queue_picture('E_fire_rts', S['t'] + 3.0, unreal.Vector(-3600, FIRE_Y + 2400, fz + 3000),
                          unreal.Vector(-3000, FIRE_Y, fz))
            queue_picture('F_corner_fire', S['t'] + 3.5, unreal.Vector(4000 + 1700, FIRE_Y - 1500, fz + 900),
                          unreal.Vector(4000, FIRE_Y + 300, fz + 200))
            S['phase'], S['mark'] = 'fire_pics', S['t']
        elif phase == 'fire_pics':
            if S['pics'] and S['t'] >= S['pics'][0][0]:
                _, name, eye, look = S['pics'][0]
                if S.get('aimed') != name:
                    aim(eye, look)
                    S['aimed'] = name
                    return
                S['pics'].pop(0)
                shoot(name)
            elif not S['pics']:
                S['phase'] = 'orders'
        elif phase == 'orders':
            S['order_time'] = S['t']
            for key, tank in S['tanks'].items():
                x = LANES[key][0]
                target = unreal.Vector(x, lane_y(key) - START_Y, S['ground'][key] + 20)
                try:
                    result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation {target} -> {result}')
            pier = S.get('pier_tank')
            if alive(pier):
                x = LANES['J_pier_medium'][0] + PIER_TANK_DX
                log(f'pier tank MoveToLocation -> '
                    f'{pier.move_to_location(unreal.Vector(x, -START_Y, S["ground"]["J_pier_medium"] + 20), True, unreal.Rotator(0, 0, -90), False)}')
            for key, units in S.get('real', {}).items():
                target = units.get('target') or units.get('wall')
                shooter = units.get('shooter')
                if alive(shooter) and alive(target):
                    S.setdefault('real_start', {})[key] = dict(
                        wall_health=units['wall'].get_editor_property('health') if alive(units.get('wall')) else None)
                    log(f'real fire {key}: AttackActor -> {shooter.attack_actor(target, True)}')
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            soviet = S['tanks'].get('G_soviet_own_wall')
            if not S.get('force_logged') and S['t'] - S['mark'] > 5.0:
                S['force_logged'] = True
                log(f'G: force check soviet={soviet is not None} alive={alive(soviet)} elapsed={S["t"] - S["mark"]:.1f}')
            if not S.get('forced') and S['t'] - S['mark'] > 6.0 and alive(soviet):
                # Push the Soviet tank onto its own wall: a real overlap must not break it.
                S['forced'] = True
                x = LANES['G_soviet_own_wall'][0]
                soviet.set_actor_location(unreal.Vector(x, lane_y('G_soviet_own_wall') + 60,
                                                        S['ground']['G_soviet_own_wall'] + 60), False, True)
                log(f'G: Soviet tank placed on its own wall at {soviet.get_actor_location()}')
            if S['pics'] and S['t'] >= S['pics'][0][0]:
                _, name, eye, look = S['pics'][0]
                if S.get('aimed') != name:
                    aim(eye, look)
                    S['aimed'] = name
                    return
                S['pics'].pop(0)
                shoot(name)
            elapsed = S['t'] - S['mark']
            arrived = all(isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 500
                          for t in S['track'].values()) and len(S['track']) == len(S['tanks'])
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 15.0 and not S['pics']):
                S['phase'], S['mark'] = 'verdict', S['t']
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
        elif phase == 'verdict':
            world = game_world()
            out = {}
            for key, (x, bp, _, vehicle, expected) in LANES.items():
                track = S['track'].get(key, {})
                state = wall_state(key)
                crossed = track.get('cross_side') is not None
                after = S['result'].setdefault('paths_after', {}).get(key) or across_paths(world, key)
                S['result']['paths_after'][key] = after
                before = S['result']['paths_before'][key]
                if expected == 'breach':
                    # Planned through the Soviet wall by the player's tanks only; once fallen, open for everyone.
                    ok = bool(state and state['breaches'] > 0 and crossed and abs(track['cross_side']) < 400
                              and before['PlayerMedium'].get('through') and not before['SovietMedium'].get('through')
                              and not before['Character'].get('through')
                              and after.get('Character', {}).get('through')
                              and after.get('PlayerLight', {}).get('through')
                              # The gap a medium tank broke fits another medium (the Soviet one, which cannot breach).
                              and (vehicle != 'PzIV_G' or after.get('SovietMedium', {}).get('through')))
                elif expected == 'own_side':
                    ok = bool(state and state['breaches'] == 0 and state['health'] == track.get('health0')
                              and not before['SovietMedium'].get('through'))
                else:
                    ok = bool(state and state['breaches'] == 0)
                out[key] = dict(wall=bp, vehicle=vehicle, expected=expected, passed=ok, crossed=crossed,
                                cross_dx=track.get('cross_side'), cross_time=track.get('cross_time'),
                                breach_at=track.get('breach_at'), max_side_near=round(track.get('max_side_near', 0)),
                                last=track.get('last'), end_state=state, states=track.get('states'))
                log(f'{key} verdict {json.dumps({k: v for k, v in out[key].items() if k != "states"})}')
                log(f'{key} states {out[key]["states"]}')
            S['result']['lanes'] = out
            real = {}
            for key, units in S.get('real', {}).items():
                wall = units.get('wall')
                holes = holes_of(wall)
                target = units.get('target')
                real[key] = dict(start=S.get('real_start', {}).get(key),
                                 wall_health=wall.get_editor_property('health') if alive(wall) else 'dead',
                                 holes=holes.get_hole_count() if holes else None,
                                 removed=round(holes.get_removed_face_share(), 3) if holes else None,
                                 target_alive=alive(target) if 'target' in units else None,
                                 target_health=None)
                if 'target' in units and alive(target):
                    hc = target.get_component_by_class(unreal.HealthComponent)
                    real[key]['target_health'] = hc.get_editor_property('CurrentHealth') if hc else None
            S['result']['real_fire'] = real
            neighbour = S.get('chain_neighbour')
            nh = holes_of(neighbour)
            S['result']['chain'] = dict(neighbour_breaches=nh.get_breach_count() if nh else None,
                                        neighbour_removed=round(nh.get_removed_face_share(), 3) if nh else None,
                                        near_removed=(wall_state('H_chain_medium') or {}).get('removed'))
            log(f'chain {json.dumps(S["result"]["chain"])}')
            pier_x = LANES['J_pier_medium'][0] + PIER_TANK_DX * 0.5
            pier_z = S['ground']['J_pier_medium'] + 200
            hit = unreal.SystemLibrary.line_trace_single(game_world(), unreal.Vector(pier_x, 400, pier_z),
                                                         unreal.Vector(pier_x, -400, pier_z),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            pier_blocked = bool(hit is not None and hit.to_tuple()[0])
            S['result']['pier'] = dict(blocked=pier_blocked, ok=not pier_blocked,
                                       wall=wall_state('J_pier_medium'))
            log(f'pier {json.dumps(S["result"]["pier"])}')
            log(f'real fire result {json.dumps(real, default=str)}')
            try:
                S['result']['friendly_fire'] = friendly_fire_checks()
            except Exception as error:  # noqa: BLE001
                S['result']['friendly_fire'] = dict(error=repr(error)[:200])
            log(f'friendly fire {json.dumps(S["result"]["friendly_fire"], default=str)}')
            for key in LANES:
                log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
            wall = S['walls'].get('A_long_medium')
            if alive(wall):
                unreal.GameplayStatics.apply_damage(wall, 1.0e7, None, None, unreal.DamageType)
                eye, look = lane_view('A_long_medium')
                aim(eye, look)
            S['phase'], S['mark'] = 'kill_pic', S['t']
        elif phase == 'kill_pic' and S['t'] - S['mark'] > 0.4:
            shoot('A_killed_collapse')
            S['phase'], S['mark'] = 'kill_wait', S['t']
        elif phase == 'kill_wait' and S['t'] - S['mark'] > 7.0:
            S['result']['kill'] = dict(gone=not alive(S['walls'].get('A_long_medium')))
            lanes_ok = all(r['passed'] for r in S['result']['lanes'].values())
            fire = S['result']['fire']
            # Clustered hits partly land in earlier holes and widen them (by design); separate holes must survive.
            fire_ok = fire['E_long_fire']['state']['holes'] >= 15 and fire['F_corner_fire']['state']['holes'] >= 20                 and bool(fire.get('back_face', {}).get('ok'))
            ff = S['result'].get('friendly_fire', {})
            ff_ok = bool(ff.get('own_side', {}).get('ok')) and bool(ff.get('through_breach', {}).get('ok'))                 and bool(ff.get('sight_gap', {}).get('ok'))
            walls_ok = all(v['ok'] for v in S['result']['all_walls'].values()) and len(S['result']['all_walls']) == 9
            log(f'friendly_fire_ok {ff_ok} all_walls_ok {walls_ok}')
            S['result']['all_passed'] = lanes_ok and fire_ok and S['result']['kill']['gone'] and ff_ok and walls_ok                 and bool(S['result'].get('pier', {}).get('ok'))
            log(f'kill {json.dumps(S["result"]["kill"])} lanes_ok {lanes_ok} fire_ok {fire_ok}')
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
