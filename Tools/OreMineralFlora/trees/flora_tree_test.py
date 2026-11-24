"""Throwaway-editor test + pictures of the eight Ore mineral-flora trees. Never in the shared editor; RTS_ToCopySmall is
never saved; the editor quits at the end. Launch: powershell -File run_tree_test.ps1 (takes the flora lock).
Derived from Source/Tools/WinterLarch/larch_test.py.

1. Lineup row at y = +LINE_Y: BP_OreDeadSpruce_Survivor_9m (the user's "red tree") and a parked Pz IV G for scale,
   then the eight trees. Pictures from the RTS angle (55 deg down) and close-ups of every piece (Saved/.../flora/trees/).
2. Crush lanes along world Y (tree at y 0, vehicle starts START_Y before and is ordered START_Y past it):
   heavy-only trees vs light / medium / heavy, medium trees vs light / medium. Paths across every tree per agent
   (Character, LightTank, MediumTank, HeavyTank; own navmesh + own filter) before and after.
Results: Saved/OreMountains/flora/trees/test/result.json + PNGs.
"""
import json
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/rev2/test/'
PICS = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/rev2/'
OUT = ROOT + 'result.json'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Trees/'
SPRUCE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_Survivor_9m'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'Luchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
    'TigerII': V + 'Ger_Heavy/TigerII/Ger_Vehicle_TigerII.Ger_Vehicle_TigerII_C',
}
LOG_KEY = 'RustLog_7m'
LOG_HALF = 342.0        # the log lies along +X from its pivot: spawn it this far to -X so the lane crosses its middle
SKELETON = 'Skeleton_11m_DeadForest'     # the UNMODIFIED dead spruce kit tree, as a Tiger H1 control
CLASSES = {SKELETON: '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_Skeleton_11m'}
LANE_DX = 2300.0
# key: (tree, vehicle, expected, tree x offset from the lane line in cm); lane x = (index - 5.5) * LANE_DX
_LANES = [      # rev2: the changed pieces only (unchanged pieces passed in run 4)
    ('L1', 'RustSpruce_16m', 'PzIV_G', 'detour', 0.0),
    ('L2', 'OchreLarch_15m', 'Luchs', 'detour', 0.0),
    ('L3', 'RedBroadleaf_15m', 'PzIV_G', 'detour', 0.0),
    ('L4', 'RustSpruce_16m', 'TigerII', 'crush', 0.0),
    ('L5', 'OchreLarch_15m', 'TigerII', 'crush', 0.0),
    ('L6', 'RedBroadleaf_15m', 'TigerII', 'crush', 0.0),
    ('L7', 'OchreBirch_8m', 'PzIV_G', 'crush', 0.0),
    ('L8', 'OchreBirch_8m', 'Luchs', 'detour', 0.0),
    ('L9', 'OchreSnag_5m', 'PzIV_G', 'crush', 0.0),
    ('L10', 'OchreSnag_5m', 'Luchs', 'detour', 0.0),
]
LANES = {k: ((i - 4.5) * LANE_DX, tree, veh, exp) for i, (k, tree, veh, exp, _) in enumerate(_LANES)}
LANE_OFFSET = {k: off for k, _, _, _, off in _LANES}
LOG_KEY = 'RustLog_7m'
LOG_HALF = 342.0        # the log lies along +X from its pivot: spawn it this far to -X so the lane crosses its middle
SWATCHES = {}           # run 4's swatch planes could not be spawned from python; dropped
SWATCH_Y = -7200.0
ORDER = ['RustSpruce_16m', 'RustSpruce_8m', 'OchreLarch_15m', 'OchreLarch_7m', 'RedBroadleaf_15m', 'OchreBirch_8m', LOG_KEY, 'OchreSnag_5m']
LINE_Y = 5200.0
LINE_X0 = -5600.0
LINE_DX = 1300.0
START_Y = 2400.0
MOVE_SECONDS = 110.0
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'), ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'trees': {}, 'tanks': {}, 'line': {}, 'result': {}, 'track': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('FLORATEST ' + message)


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
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    return (t[4].z, True) if t and t[0] else (0.0, False)


def main_mesh(actor):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() in ('Tree', 'MainMesh'):
            return c
    return None


def hp(actor):
    comp = actor.get_component_by_class(unreal.HealthComponent)
    if comp is not None:
        for name in ('get_current_health', 'GetCurrentHealth'):
            fn = getattr(comp, name, None)
            if fn:
                return round(float(fn()))
    try:
        return round(float(actor.get_editor_property('Health')))
    except Exception:  # noqa: BLE001
        return None


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def tree_class(key):
    if key in CLASSES:
        p = CLASSES[key]
        return unreal.load_class(None, f'{p}.{p.split("/")[-1]}_C')
    name = 'BP_OreFlora_' + key
    return unreal.load_class(None, f'{HOME}{name}.{name}_C')


def class_name(key):
    return (CLASSES[key].split('/')[-1] if key in CLASSES else 'BP_OreFlora_' + key) + '_C'


def lane_tree_xy(key):
    x, tree = LANES[key][0] + LANE_OFFSET.get(key, 0.0), LANES[key][1]
    return (x - LOG_HALF if tree == LOG_KEY else x), 0.0


# ---------------------------------------------------------------- setup
def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    misses = []
    for key, (x, tree, vehicle, _) in LANES.items():
        tx, ty = lane_tree_xy(key)
        z, ok = ground_z(world, tx, ty)
        misses += [] if ok else [key]
        a = eas.spawn_actor_from_class(tree_class(tree), unreal.Vector(tx, ty, z), unreal.Rotator(0, 0, 0))
        a.set_actor_label(f'FT_{key}_{tree}')
        z, ok = ground_z(world, x, START_Y)
        tank = eas.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]), unreal.Vector(x, START_Y, z + 50), unreal.Rotator(0, 0, -90))
        log(f'{key}: {tree} at x {tx} and {vehicle}, owner set {set_owner(tank, 1)}')
    # lineup: comparison spruce, a parked Pz IV, then the eight pieces
    items = [('Spruce_Survivor_9m', unreal.load_class(None, SPRUCE + '.BP_OreDeadSpruce_Survivor_9m_C')), ('PzIV_parked', unreal.load_class(None, VEHICLES['PzIV_G']))] + \
        [(k, tree_class(k)) for k in ORDER]
    for i, (k, cls) in enumerate(items):
        x = LINE_X0 + LINE_DX * i - (LOG_HALF if k == LOG_KEY else 0.0)
        z, ok = ground_z(world, x, LINE_Y)
        misses += [] if ok else ['line_' + k]
        rot = unreal.Rotator(0, 0, 90 if k == 'PzIV_parked' else 0)
        a = eas.spawn_actor_from_class(cls, unreal.Vector(x, LINE_Y, z + (50 if k == 'PzIV_parked' else 0)), rot)
        a.set_actor_label('FTLINE_' + k)
        if k == 'PzIV_parked':
            set_owner(a, 1)
    for sw, (_, sx, keys) in SWATCHES.items():
        for i, k in enumerate(keys):
            x = sx + (i - 1) * 1000.0 - (LOG_HALF if k == LOG_KEY else 0.0)
            z, ok = ground_z(world, x, SWATCH_Y)
            misses += [] if ok else ['swatch_' + k]
            a = eas.spawn_actor_from_class(tree_class(k), unreal.Vector(x, SWATCH_Y, z + 2.0), unreal.Rotator(0, 0, 0))
            a.set_actor_label(f'FTSWATCH_{sw}_{k}')
    eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 2000))
    log(f'ground trace misses {misses}')


def spawn_swatches(world):
    """Flat coloured planes under the swatch trees, spawned in the PIE world (dynamic material, nothing saved)."""
    plane = unreal.load_asset('/Engine/BasicShapes/Plane')
    mat = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
    for sw, (col, sx, _) in SWATCHES.items():
        try:
            z = ground_z(world, sx, SWATCH_Y)[0] + 3.0
            tf = unreal.Transform(unreal.Vector(sx, SWATCH_Y, z), unreal.Rotator(0, 0, 0), unreal.Vector(34.0, 18.0, 1.0))
            a = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(world, unreal.StaticMeshActor, tf)
            a = unreal.GameplayStatics.finish_spawning_actor(a, tf)
            c = a.static_mesh_component
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_static_mesh(plane)
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            mid = c.create_dynamic_material_instance(0, mat)
            mid.set_vector_parameter_value('Color', col)
            a.set_actor_scale3d(unreal.Vector(34.0, 18.0, 1.0))
            log(f'swatch {sw} at x {sx} z {z:.0f}')
        except Exception as error:  # noqa: BLE001
            log(f'swatch {sw} failed {error!r}')


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        if abs(loc.y - LINE_Y) < 300:
            for i, k in enumerate(['Spruce_Survivor_9m', 'PzIV_parked'] + ORDER):
                x = LINE_X0 + LINE_DX * i - (LOG_HALF if k == LOG_KEY else 0.0)
                if abs(loc.x - x) < 150 and ('BP_Ore' in name or 'Vehicle' in name):
                    S['line'][k] = actor
            continue
        for key, (x, tree, _, _) in LANES.items():
            tx, ty = lane_tree_xy(key)
            if name == class_name(tree) and abs(loc.x - tx) < 200 and abs(loc.y - ty) < 200:
                S['trees'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _) in LANES.items():
            if abs(loc.x - x) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 2000)).length())
    log(f'runtime trees {sorted(S["trees"])} tanks {sorted(S["tanks"])} line {sorted(S["line"])}')


def navmeshes(world):
    out = {}
    allnav = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
    for nav in allnav:
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in allnav if n not in out.values()]
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
    return dict(length=round(length), partial=bool(p.is_partial()), side_cm=round(side), through=bool(side < 30 and not p.is_partial()))


def across_paths(world, key):
    x = LANES[key][0]
    z = S['origins'][key].z
    a, b = unreal.Vector(x, 700, z + 20), unreal.Vector(x, -700, z + 20)
    out = {agent: path(world, a, b, nav, unreal.load_class(None, FILTERS[agent])) for agent, nav in S['nav'].items()}
    if LANES[key][1] == LOG_KEY:       # also across the log's butt end and its crown end
        for tag, dx in (('butt', -LOG_HALF + 60), ('crown', LOG_HALF - 60)):
            a2, b2 = unreal.Vector(x + dx, 700, z + 20), unreal.Vector(x + dx, -700, z + 20)
            out.update({f'{agent}_{tag}': path(world, a2, b2, nav, unreal.load_class(None, FILTERS[agent])) for agent, nav in S['nav'].items()})
    return out


def tree_state(key):
    tree = S['trees'].get(key)
    if not alive(tree):
        return None
    mesh = main_mesh(tree)
    rot = mesh.get_world_rotation()
    return dict(z=round(mesh.get_world_location().z - S['origins'][key].z, 1), pitch=round(rot.pitch, 1), roll=round(rot.roll, 1),
                visible=bool(mesh.is_visible()), collision=str(mesh.get_collision_enabled()).split('.')[-1].split(':')[0],
                life=round(tree.get_life_span(), 1), health=hp(tree))


def setup_report(key):
    tree = S['trees'][key]
    mesh = main_mesh(tree)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('Projectile', ch.ECC_PROJECTILE), ('Pawn', ch.ECC_PAWN))}
    navs = [n.get_editor_property('area_class').get_name() for n in tree.get_components_by_class(unreal.NavModifierComponent)]
    for b in tree.get_components_by_class(unreal.BoxComponent):
        ac = b.get_editor_property('area_class_override')
        navs.append(('box', ac.get_name() if ac else None, str(b.get_scaled_box_extent())))
    crush = None
    for n in ('TankDestructionType', 'CrushDestructionType'):
        try:
            crush = str(tree.get_editor_property(n))
            break
        except Exception:  # noqa: BLE001
            pass
    return dict(cls=tree.get_class().get_name(), mesh=mesh.get_editor_property('static_mesh').get_name(),
                materials=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())], scale=str(tree.get_actor_scale3d()),
                overlap_events=bool(mesh.get_editor_property('generate_overlap_events')), responses=resp, nav=navs, crush=crush, health=hp(tree))


def sample():
    for key in LANES:
        tank = S['tanks'].get(key)
        x = LANES[key][0]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, crush=None, gone_at=None, last=None, samples=[]))
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
        changed = abs(state['z']) > 1 or abs(state['pitch'] - start['pitch']) > 1 or abs(state['roll'] - start['roll']) > 1 or \
            state['life'] > 0 or not state['visible'] or state['collision'] != start['collision']
        if changed and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], first=state)
            log(f'{key}: tree changed {json.dumps(track["crush"])}')
            S.setdefault('pic_queue', []).append((key, S['t'] + 0.7))
        if changed and len(track['samples']) < 30 and S['t'] - track.get('sample_t', 0.0) >= 0.3:
            track['sample_t'] = S['t']
            track['samples'].append((round(S['t'] - S['order_time'], 2), state))


# ---------------------------------------------------------------- pictures
def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def shoot(name, folder=None, w=1600, h=900):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(w, h, f'{folder or ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def rts_eye(target, dist, pitch=55.0, yaw_side=1.0):
    """Eye at dist cm from the target, pitch degrees down, looking towards -Y (from the +Y side)."""
    import math
    a = math.radians(pitch)
    return target + unreal.Vector(0.0, yaw_side * math.cos(a) * dist, math.sin(a) * dist)


def line_pos(k):
    names = ['Spruce_Survivor_9m', 'PzIV_parked'] + ORDER
    i = names.index(k)
    return unreal.Vector(LINE_X0 + LINE_DX * i, LINE_Y, S['line_z'])


def build_shots():
    """(name, eye, look, w, h) for the lineup and the close-ups."""
    shots = []
    names = ['Spruce_Survivor_9m', 'PzIV_parked'] + ORDER
    mid_a = (line_pos(names[0]) + line_pos(names[4])) * 0.5 + unreal.Vector(0, 0, 300)
    mid_b = (line_pos(names[5]) + line_pos(names[9])) * 0.5 + unreal.Vector(0, 0, 300)
    mid = (line_pos(names[0]) + line_pos(names[9])) * 0.5 + unreal.Vector(0, 0, 300)
    shots.append(('lineup_rts_all', rts_eye(mid, 7600.0), mid, 1920, 1080))
    shots.append(('lineup_rts_a', rts_eye(mid_a, 3600.0), mid_a, 1920, 1080))
    shots.append(('lineup_rts_b', rts_eye(mid_b, 3600.0), mid_b, 1920, 1080))
    shots.append(('lineup_low', line_pos(names[4]) + unreal.Vector(0, 3600, 700), line_pos(names[4]) + unreal.Vector(0, 0, 500), 1920, 900))
    top_mid = mid + unreal.Vector(0, 0, -300)
    shots.append(('top_all', top_mid + unreal.Vector(0, 1, 9500), top_mid, 1920, 1080))
    for tag, a, b in (('top_a', 0, 4), ('top_b', 5, 9)):
        p = (line_pos(names[a]) + line_pos(names[b])) * 0.5
        shots.append((tag, p + unreal.Vector(0, 1, 4600), p, 1920, 1080))
    for k in ['Spruce_Survivor_9m', 'RedBroadleaf_15m', 'OchreBirch_8m', 'OchreSnag_5m', 'RustSpruce_16m', 'OchreLarch_15m']:
        small = k in (LOG_KEY, 'OchreSnag_5m')
        p = line_pos(k) + unreal.Vector(0, 0, 200 if small else (650 if k.endswith(('15m', '16m')) else 380))
        dist = 1100.0 if small else (2300.0 if k.endswith(('15m', '16m')) else 1500.0)
        shots.append(('close_' + k, rts_eye(p, dist), p, 1600, 900))
        shots.append(('close_' + k + '_bg', rts_eye(p, dist), p, 1600, 900))     # same camera, the piece hidden
    for sw, (_, sx, _) in SWATCHES.items():
        p = unreal.Vector(sx, SWATCH_Y, S['line_z'] + 250.0)
        shots.append(('swatch_' + sw, rts_eye(p, 3000.0), p, 1920, 1080))
    return shots


def hide_for(name):
    """Hide the lineup piece for its _bg picture, show it again otherwise."""
    for k, a in S['line'].items():
        if alive(a):
            a.set_actor_hidden_in_game(name.endswith('_bg') and name == 'close_' + k + '_bg')


def bind_overlap_log():
    """Log every begin-overlap on the heavy-only trees (diagnoses which actor / agent the tank presents)."""
    def make(key):
        def cb(overlapped, other_actor, other_comp, index, sweep, hit):
            try:
                agent = str(other_actor.get_editor_property('NavAgentType')) if other_actor else None
            except Exception:  # noqa: BLE001
                agent = 'n/a'
            n = S.setdefault('overlaps', {}).setdefault(key, 0)
            if n < 6:
                log(f'{key} overlap: actor {other_actor.get_class().get_name() if other_actor else None} comp {other_comp.get_name() if other_comp else None} '
                    f'agent {agent}')
            S['overlaps'][key] = n + 1
        return cb
    for key, tree in S['trees'].items():
        mesh = main_mesh(tree)
        try:
            mesh.on_component_begin_overlap.add_callable(make(key))
        except Exception as error:  # noqa: BLE001
            log(f'{key}: overlap bind failed {error!r}')


def vehicle_agents():
    out = {}
    for key, tank in S['tanks'].items():
        try:
            out[key] = (tank.get_class().get_name(), str(tank.get_editor_property('NavAgentType')))
        except Exception as error:  # noqa: BLE001
            out[key] = repr(error)[:80]
    return out


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
                for cmd in ('r.TextureStreaming 0', 'r.ForceLOD -1'):
                    unreal.SystemLibrary.execute_console_command(world, cmd)
                find_runtime(world)
                S['origins'] = {k: t.get_actor_location() for k, t in S['trees'].items()}
                S['line_z'] = ground_z(world, LINE_X0, LINE_Y)[0]
                spawn_swatches(world)
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
            S['result']['line_setup'] = {k: dict(cls=a.get_class().get_name(), scale=str(a.get_actor_scale3d())) for k, a in S['line'].items()}
            S['result']['paths_before'] = {k: across_paths(world, k) for k in S['trees']}
            for key in S['trees']:
                log(f'{key} setup {json.dumps(S["result"]["setup"][key])}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            S['result']['vehicle_agents'] = vehicle_agents()
            log(f'vehicle agents {json.dumps(S["result"]["vehicle_agents"])}')
            bind_overlap_log()
            S['shots'] = build_shots()
            S['phase'], S['mark'] = 'pics', S['t']
            S['aimed'] = None
        elif phase == 'pics':
            # aim -> 2 s -> shoot -> 2 s cooldown (the high-res shot renders a frame later; moving the camera in the
            # next tick put the NEXT camera into the picture in run 1)
            if S.get('shot_at') is not None and S['t'] - S['shot_at'] < 2.0:
                return
            S['shot_at'] = None
            if not S['shots']:
                hide_for('')
                log_actor = S['line'].get(LOG_KEY)
                if alive(log_actor):
                    S['result']['log_kill_before'] = dict(health=hp(log_actor))
                    unreal.GameplayStatics.apply_damage(log_actor, 1.0e7, None, None, unreal.DamageType)
                    S['result']['log_kill_after_damage'] = dict(alive=alive(log_actor), health=hp(log_actor) if alive(log_actor) else None,
                                                                life=round(log_actor.get_life_span(), 1) if alive(log_actor) else None)
                    log(f'log kill: {json.dumps(S["result"]["log_kill_after_damage"])}')
                    p = line_pos(LOG_KEY) + unreal.Vector(0, 0, 80)
                    aim(rts_eye(p, 1100.0), p)
                S['phase'], S['mark'] = 'kill_pic', S['t']
                return
            name, eye, look, w, h = S['shots'][0]
            if S['aimed'] != name:
                hide_for(name)
                aim(eye, look)
                S['aimed'], S['mark'] = name, S['t']
            elif S['t'] - S['mark'] > 2.0:
                shoot(name, PICS, w, h)
                S['shots'].pop(0)
                S['shot_at'] = S['t']
        elif phase == 'kill_pic' and S['t'] - S['mark'] > 0.8:
            shoot('kill_log', ROOT, 1600, 900)
            S['phase'], S['mark'] = 'kill_wait', S['t']
        elif phase == 'kill_wait' and S['t'] - S['mark'] > 7.0:
            S['result']['log_kill_7s'] = dict(gone=not alive(S['line'].get(LOG_KEY)))
            log(f'log kill 7 s later: {json.dumps(S["result"]["log_kill_7s"])}')
            o = S['origins']['L4']
            aim(o + unreal.Vector(0, 2600, 900), o + unreal.Vector(0, 0, 300))
            S['phase'], S['mark'] = 'look_pic', S['t']
        elif phase == 'look_pic' and S['t'] - S['mark'] > 2.0:
            mid = unreal.Vector(0, 0, S['origins']['L5'].z + 200)
            aim(mid + unreal.Vector(0, 9000, 13000), mid)
            S['phase'], S['mark'] = 'lanes_pic', S['t']
        elif phase == 'lanes_pic' and S['t'] - S['mark'] > 2.0:
            shoot('lanes_before', ROOT, 1920, 1080)
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
            mid = unreal.Vector(0, 0, S['origins']['L5'].z + 200)
            aim(mid + unreal.Vector(0, 9000, 13000), mid)
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            queue = S.get('pic_queue', [])
            if queue and S['t'] >= queue[0][1]:
                key = queue.pop(0)[0]
                o = S['origins'][key]
                aim(o + unreal.Vector(900, -1300, 900), o + unreal.Vector(0, 0, 200))
                shoot('crush_' + key + '_' + LANES[key][1])
            elapsed = S['t'] - S['mark']
            arrived = len(S['track']) == len(LANES) and all(isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 400 for t in S['track'].values())
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 20.0 and not queue):
                S['phase'], S['mark'] = 'settle', S['t']
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
        elif phase == 'settle':
            sample()
            if S['t'] - S['mark'] > 8.0:
                world = game_world()
                out = {}
                for key, (x, tree, vehicle, expected) in LANES.items():
                    track = S['track'].get(key, {})
                    crushed = track.get('crush') is not None
                    if expected == 'crush':
                        ok = crushed and track.get('cross_side') is not None and abs(track['cross_side']) < 150
                    else:
                        ok = (not crushed) and alive(S['trees'][key]) and track.get('cross_side') is not None and track.get('max_side_near', 0) > 100
                    out[key] = dict(tree=tree, vehicle=vehicle, expected=expected, passed=bool(ok), crushed=crushed, tree_alive_at_end=alive(S['trees'][key]),
                                    end_state=tree_state(key), vehicle_crossed_y0_at_dx=track.get('cross_side'),
                                    vehicle_max_side_offset_near_tree=round(track.get('max_side_near', 0)), vehicle_last=track.get('last'),
                                    crush=track.get('crush'), actor_gone_s=track.get('gone_at'), samples=track.get('samples'))
                S['result']['lanes'] = out
                S['result']['paths_after'] = {k: across_paths(world, k) for k in LANES}
                for key, row in out.items():
                    log(f'{key} verdict {json.dumps({k: v for k, v in row.items() if k != "samples"})}')
                    log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
                S['result']['all_passed'] = all(r['passed'] for r in out.values())
                log(f'ALL PASSED {S["result"]["all_passed"]}')
                mid = unreal.Vector(0, 0, S['origins']['L5'].z + 200)
                aim(mid + unreal.Vector(0, 9000, 13000), mid)
                S['phase'], S['mark'] = 'end_pic', S['t']
        elif phase == 'end_pic' and S['t'] - S['mark'] > 2.0:
            shoot('lanes_after', ROOT, 1920, 1080)
            S['phase'], S['mark'] = 'end_wait', S['t']
        elif phase == 'end_wait' and S['t'] - S['mark'] > 2.5:
            S['result']['overlap_counts'] = S.get('overlaps', {})
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 620.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
