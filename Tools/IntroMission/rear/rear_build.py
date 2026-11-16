"""Builds the Ger_IntroMission rear areas on top of the terrain from rear_terrain_gen.py (exec'd in the editor
after rear_lib.py). Every step clears and respawns its own REAR_<Step>_* actors, so any step can be re-run alone.

    exec(open(LIB).read()); exec(open(BUILD).read()); build_all()

Steps: step_water, step_roads, step_rail, step_station, step_depot, step_detail. Asset choices are existing project
Blueprints / meshes only (no train or unit Blueprints: the parked train is static meshes).
"""
import json
import math
import random

PLAN = json.load(open(ROOT + 'rear_plan.json'))
GRID_FULL = tuple(PLAN['grid'])
load_heights('rear_full', GRID_FULL)
TOP_Z = PLAN['station_top_z']

ENV = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
PATHS = '/Game/RTS_Survival/Blueprints/Environment/Splines/'
TRAIN = '/Game/RTS_Survival/Blueprints/GroundVehicles/Trains/Soviet/Meshes/'
ROCKS = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'


def gz(x, y):
    return ground_z(x, y)


def _vec(x, y, dz=0.0):
    return (float(x), float(y), gz(x, y) + dz)


def catmull_pts(points, step=400.0):
    p = [points[0]] + list(points) + [points[-1]]
    out = []
    for k in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[k - 1], p[k], p[k + 1], p[k + 2]
        n = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / step))
        for i in range(n):
            t = i / n
            out.append(tuple(0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t * t +
                                    (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t ** 3) for c in (0, 1)))
    out.append(tuple(points[-1]))
    return out


# ---- water -----------------------------------------------------------------------------------------------------------
def step_water():
    clear_prefix('REAR_Water_')
    mat = '/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore'
    for name, pd in PLAN['ponds'].items():
        cx, cy = pd['centre']
        a = math.radians(pd['yaw'])
        us = [(x - cx) * math.cos(a) + (y - cy) * math.sin(a) for x, y in PLAN['shores'][name]]
        vs = [-(x - cx) * math.sin(a) + (y - cy) * math.cos(a) for x, y in PLAN['shores'][name]]
        mu, mv = (max(us) + min(us)) / 2, (max(vs) + min(vs)) / 2
        px, py = cx + mu * math.cos(a) - mv * math.sin(a), cy + mu * math.sin(a) + mv * math.cos(a)
        sx = (max(us) - min(us) + 700.0) / 100.0
        sy = (max(vs) - min(vs) + 700.0) / 100.0
        a = spawn_mesh('/Engine/BasicShapes/Plane', 'REAR_Water_' + name, (px, py, pd['water_z']), pd['yaw'], 'Water',
                       scale=(sx, sy, 1.0), collision=False, materials=[mat])
        a.static_mesh_component.set_editor_property('cast_shadow', False)
    log('water: %d ponds' % len(PLAN['ponds']))


# ---- roads -----------------------------------------------------------------------------------------------------------
def _path_actor(label, points, width=420.0):
    cls = unreal.load_class(None, PATHS + 'GroundPath/BP_GroundPathSpline_TwoTrack.BP_GroundPathSpline_TwoTrack_C')
    first = points[0]
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(*_vec(first[0], first[1], 150.0)))
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER + 'Roads')
    spline = actor.get_component_by_class(unreal.SplineComponent)
    spline.set_spline_points([unreal.Vector(*_vec(x, y, 150.0)) for x, y in points], unreal.SplineCoordinateSpace.WORLD,
                             True)
    spline.set_editor_property('spline_has_been_edited', True)
    actor.set_editor_property('PathMaterial', unreal.load_asset(
        PATHS + 'GroundPath/MI_GroundPath_TwoTrack_Desert'))
    actor.set_editor_property('PathWidth', width + 1.0)
    actor.set_editor_property('PathWidth', width)
    tiles = len(actor.get_components_by_class(unreal.SplineMeshComponent))
    log('%s: %d points, %d tiles' % (label, len(points), tiles))
    return actor


def step_roads():
    clear_prefix('REAR_Road_')
    names = {'D': 'REAR_Road_Depot', 'E': 'REAR_Road_EastFade', 'S': 'REAR_Road_Station'}
    for key, pts in PLAN['roads'].items():
        pts = [tuple(p) for p in pts]
        # thin the dense curve to ~8 m spacing for the spline
        thin = [pts[0]]
        for p in pts[1:]:
            if math.hypot(p[0] - thin[-1][0], p[1] - thin[-1][1]) >= 800.0:
                thin.append(p)
        if thin[-1] != pts[-1]:
            thin.append(pts[-1])
        _path_actor(names[key], thin)


# ---- rail ------------------------------------------------------------------------------------------------------------
RAIL_MESH = '/Game/RTS_Survival/Meshes/Buildings/Industrial/RailRoad/SM_RustedRefinery_RailRoad'


def _rail(label, pts):
    cls = unreal.load_class(None, PATHS + 'BP_RoadSpline.BP_RoadSpline_C')
    pts = catmull_pts(pts, 600.0)
    a = actors_sub().spawn_actor_from_class(cls, unreal.Vector(pts[0][0], pts[0][1], TOP_Z))
    a.set_actor_label(label)
    a.set_folder_path(FOLDER + 'Station')
    sp = a.get_component_by_class(unreal.SplineComponent)
    sp.set_spline_points([unreal.Vector(x, y, gz(x, y) + 2.0) for x, y in pts], unreal.SplineCoordinateSpace.WORLD, True)
    a.set_editor_property('RoadMesh', unreal.load_asset(RAIL_MESH))
    a.set_editor_property('bUseCollision', True)
    a.set_editor_property('bUseCollision', False)
    a.set_editor_property('bAllowDecals', False)
    log('%s: %d spline meshes' % (label, len(a.get_components_by_class(unreal.SplineMeshComponent))))
    return a


def step_rail():
    clear_prefix('REAR_Rail_')
    st = PLAN['station']
    main = [tuple(p) for p in st['track_x']]
    main[0] = (main[0][0], 25700.0)
    _rail('REAR_Rail_Main', main)
    sid = [tuple(p) for p in st['siding_x']]
    # the siding leaves the main line with a gentle S: start on the main track north of the platform
    sid = [(21600.0, 20500.0), (21350.0, 18200.0)] + sid
    _rail('REAR_Rail_Siding', sid)


# ---- station ---------------------------------------------------------------------------------------------------------
def _mesh_len(path):
    m = unreal.load_asset(path)
    b = m.get_bounds()
    return m, b.origin, b.box_extent


def _train(prefix, x, y_front, cars, folder='Station'):
    """Static train facing -Y (towards the buffer stops), cars laid back along +Y on the track."""
    y = y_front
    for k, mesh in enumerate(cars):
        m, o, e = _mesh_len(TRAIN + mesh)
        long_x = e.x >= e.y
        half = e.x if long_x else e.y
        yaw = -90.0 if long_x else 180.0
        cy = y + half
        # mesh origin offset along its long axis
        off = o.x if long_x else o.y
        spawn_mesh(TRAIN + mesh, '%s_%02d' % (prefix, k), (x, cy + off, TOP_Z + 2.0 - (o.z - e.z)), yaw, folder)
        y = cy + half + 120.0


def step_station():
    clear_prefix('REAR_Station_')
    z = TOP_Z
    B = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget/'
    # platform: two loading ramps end to end beside the siding
    spawn_mesh(B + 'SM_Berlin_RailRamp', 'REAR_Station_Platform_A', (20380.0, 10350.0, z), 0.0, 'Station')
    spawn_mesh(B + 'SM_Berlin_RailRamp', 'REAR_Station_Platform_B', (20380.0, 12600.0, z), 180.0, 'Station')
    # station office = brick signal box at the head of the platform
    spawn_mesh(B + 'SM_Berlin_SwitchTower', 'REAR_Station_Office', (19900.0, 14100.0, z), 90.0, 'Station')
    # goods shed south of the forecourt
    spawn_mesh('/Game/RTS_Survival/Meshes/Industrial/Zone/Meshes/Warehouses/SM_Warehouse_03', 'REAR_Station_GoodsShed',
               (20250.0, 6450.0, z), 90.0, 'Station')
    spawn_bp(ENV + 'PostApo/BP_Dest_PostApo_WaterTower', 'REAR_Station_WaterTower', (22350.0, 12200.0, z), 0.0, 'Station')
    spawn_bp(ENV + 'PostApo/BP_Dest_PostApo_WaterTank', 'REAR_Station_WaterTank', (22400.0, 9200.0, z), 90.0, 'Station')
    spawn_mesh('/Game/RTS_Survival/Meshes/Civilian/ForgottenHousesNuclear/SM_ForgottenHouse_5',
               'REAR_Station_House_A', (19950.0, 15600.0, z), 0.0, 'Station')
    spawn_mesh('/Game/RTS_Survival/Meshes/Civilian/ForgottenHousesNuclear/SM_ForgottenHouse_8',
               'REAR_Station_House_B', (20000.0, 17000.0, z), 90.0, 'Station')
    # buffer stops
    spawn_bp(ENV + 'Military/Barricades/BP_Military_Dest_ConcreteBarrier', 'REAR_Station_Buffer_Main', (21650.0, 4950.0, z), 0.0,
             'Station')
    spawn_bp(ENV + 'Military/Barricades/BP_Military_Dest_ConcreteBarrier', 'REAR_Station_Buffer_Siding', (21000.0, 7350.0, z),
             0.0, 'Station')
    # parked trains: fuel train on the main line, two goods cars on the siding at the platform
    _train('REAR_Station_TrainMain', 21650.0, 6100.0,
           ['SM_SovietTrain_Locomotive', 'SM_SovietTrain_FuelCar', 'SM_SovietTrain_FuelCar', 'SM_SovietTrain_CargoCar',
            'SM_SovietTrain_FuelCar'])
    _train('REAR_Station_TrainSiding', 21000.0, 9400.0, ['SM_SovietTrain_CargoCar', 'SM_SovietTrain_CargoCar'])
    # freight on the platform
    for k, (x, y, bp, yaw) in enumerate([
            (20250, 9600, 'Airport/BP_Airport_Dest_CrateStack02', 0), (20450, 10900, 'Airport/BP_Airport_Dest_BarrelStack', 30),
            (20300, 12100, 'Airport/BP_Airport_Dest_CrateStack03', 90), (20450, 13300, 'Airport/BP_Airport_Dest_BarrelStack2', 0),
            (19700, 9200, 'PostApo/BP_Dest_PostApo_ContainerBox1', 15), (19600, 11800, 'Airport/BP_Airport_Dest_CrateStack01', 70)]):
        spawn_bp(ENV + bp, 'REAR_Station_Freight_%02d' % k, (x, y, z), yaw, 'Station')
    # telegraph poles along the line (east side) and up the station road
    k = 0
    for y in range(5600, 25600, 3200):
        x = 22350.0 if y > 13500 or y < 11000 else 22650.0
        spawn_bp(ENV + 'Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric1', 'REAR_Station_Pole_%02d' % k, (x, float(y), z),
                 90.0, 'Station')
        k += 1
    log('station built')


# ---- depot -----------------------------------------------------------------------------------------------------------
def step_depot():
    clear_prefix('REAR_Depot_')
    dp = PLAN['depot']
    cx, cy = dp['centre']
    yaw = dp['yaw']
    a = math.radians(yaw)
    nu, nv = dp['tiles']
    step = 2400.0
    tile_top = 146.0

    def W(u, v):
        return cx + u * math.cos(a) - v * math.sin(a), cy + u * math.sin(a) + v * math.cos(a)
    k = 0
    for iu in range(nu):
        for iv in range(nv):
            # SM_Ground_1's pivot is its top (+X, +Y) corner: the slab spans -2400..0 in local X/Y, -200..0 in Z
            u = (iu - (nu - 1) / 2.0) * step + step / 2.0
            v = (iv - (nv - 1) / 2.0) * step + step / 2.0
            x, y = W(u, v)
            spawn_bp('/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Airport/BP_Airport_Ground1',
                     'REAR_Depot_Tile_%02d' % k, (x, y, tile_top), yaw, 'Depot')
            k += 1
    props = [
        ('Soviet/BP_Dest_SovietCrane_1', -2600, 1400, 90), ('Industrial/BP_Industrial_Dest_OilTank1', 2700, -1650, 0),
        ('Industrial/BP_Industrial_Dest_OilTank1', 2700, -900, 0), ('Industrial/BP_Industrial_Dest_OilTankBarrels1', 2750, -100, 0),
        ('PostApo/BP_Dest_PostApo_Container', -2500, -1800, 0), ('PostApo/BP_Dest_PostApo_Container', -2500, -1460, 0),
        ('PostApo/BP_Dest_PostApo_Container', -1550, -1800, 90), ('Military/BP_Military_Dest_Camonet', 200, -1750, 0),
        ('Airport/BP_Airport_Dest_CrateStack02', 0, -1950, 10), ('Airport/BP_Airport_Dest_CrateStack03', 450, -1550, 80),
        ('Airport/BP_Airport_Dest_BarrelStack3', 1500, -1900, 0), ('Airport/BP_Airport_Dest_BarrelStack', 2000, 1700, 40),
        ('Airport/BP_Airport_Dest_DrainPipeStack', -1200, 1900, 90), ('PostApo/BP_Dest_PostApo_ContainerBox1', -3100, -600, 15),
        ('PostApo/BP_Dest_PostApo_ContainerBox2', -3150, -200, 0), ('Airport/BP_Airport_Dest_Building01', 600, 3300, 0),
        ('Military/DestTanks/BP_Env_DestroyedT34', -4300, 2600, 140),
    ]
    for k, (bp, u, v, yw) in enumerate(props):
        x, y = W(u, v)
        try:
            spawn_bp(ENV + bp, 'REAR_Depot_Prop_%02d' % k, (x, y, gz(x, y) if abs(u) > 3600 or abs(v) > 2400 else tile_top),
                     yaw + yw, 'Depot')
        except Exception as ex:
            log('depot prop %s failed: %s' % (bp, ex))
    log('depot built')


# ---- detail ----------------------------------------------------------------------------------------------------------
def _slope(x, y):
    d = 150.0
    return math.degrees(math.atan(math.hypot(gz(x + d, y) - gz(x - d, y), gz(x, y + d) - gz(x, y - d)) / (2 * d)))


def _near_road(x, y, r):
    for pts in PLAN['roads'].values():
        for p in pts:
            if (p[0] - x) ** 2 + (p[1] - y) ** 2 < r * r:
                return True
    return False


def _inside_poly(poly, x, y):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-9) + xi:
            inside = not inside
        j = i
    return inside


def _pond_dist(x, y):
    """Signed-ish distance to the nearest shoreline (negative inside a pond)."""
    best = 1e9
    for poly in PLAN['shores'].values():
        d = min(math.hypot(x - a, y - b) for a, b in poly)
        best = min(best, -d if _inside_poly(poly, x, y) else d)
    return best


def _in_pond(x, y, pad=0.0):
    return _pond_dist(x, y) < pad


def _in_station(x, y):
    return 19000 < x < 23200 and 4300 < y < 25800


def _in_depot(x, y):
    dp = PLAN['depot']
    a = math.radians(dp['yaw'])
    u = (x - dp['centre'][0]) * math.cos(a) + (y - dp['centre'][1]) * math.sin(a)
    v = -(x - dp['centre'][0]) * math.sin(a) + (y - dp['centre'][1]) * math.cos(a)
    return abs(u) < 5200 and abs(v) < 4000


def _free(x, y):
    return (x > 8200 or y > -1000) and x < 24500 and -24200 < y < 25000 and not _in_pond(x, y, 400) \
        and not _near_road(x, y, 700) and not _in_station(x, y) and not _in_depot(x, y) \
        and not (3900 < x < 8200 and -6400 < y < -900)


def step_detail():
    clear_prefix('REAR_Detail_')
    rng = random.Random(1976)
    n = {'rock': 0, 'tree': 0, 'cactus': 0, 'shrub': 0, 'stone': 0}
    big_rocks = ['M1_Block', 'M2_Slab', 'M3_Tilted', 'M4_Cracked', 'M5_Pair', 'M6_Cube', 'M7_Long', 'M8_Wedge']
    large_rocks = ['L1_Block', 'L2_Cracked', 'L3_Tumbled', 'L5_Pile', 'L6_Wedge', 'L7_Slab', 'L8_Leaning']
    shrubs = ['/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/S_Deadshrub-01_vdknafgha_Var%d_lod0' % i
              for i in (1, 3, 5, 7, 9, 11, 13, 15)]
    cacti = ['Small_Blue_01', 'Small_Green_02', 'Medium_Blue_02', 'Medium_Green_01', 'Small_Green_03', 'Large_Green_01']
    stones = ['/Game/RTS_Survival/Meshes/Desert/Rocks/SM_Rock_0%d' % i for i in (1, 2, 3, 6, 7)]
    # ridge rocks: on flanks steeper than 16 deg, poisson-ish
    placed = []

    def ok(x, y, r):
        return all((x - a) ** 2 + (y - b) ** 2 > r * r for a, b in placed)
    tries = 0
    while n['rock'] < 46 and tries < 30000:
        tries += 1
        x, y = rng.uniform(5000, 24300), rng.uniform(-24000, 25000)
        if not _free(x, y) or _slope(x, y) < 16.0 or not ok(x, y, 1500):
            continue
        name = rng.choice(large_rocks if rng.random() < 0.35 else big_rocks)
        spawn_bp(ROCKS + 'BP_DesertRock_' + name, 'REAR_Detail_Rock_%03d' % n['rock'], (x, y, gz(x, y) - 30.0),
                 rng.uniform(0, 360), 'Detail')
        placed.append((x, y))
        n['rock'] += 1
    # small stones on crests and around the big rocks (no collision)
    tries = 0
    while n['stone'] < 140 and tries < 40000:
        tries += 1
        x, y = rng.uniform(5000, 24500), rng.uniform(-24000, 25500)
        if _in_pond(x, y, 300) or _near_road(x, y, 450) or (3900 < x < 8200 and -6400 < y < -900):
            continue
        if gz(x, y) < 330.0 and rng.random() > 0.08:
            continue
        spawn_mesh(rng.choice(stones), 'REAR_Detail_Stone_%03d' % n['stone'], (x, y, gz(x, y) - 10.0),
                   rng.uniform(0, 360), 'Detail', scale=rng.uniform(0.35, 0.9), collision=False)
        n['stone'] += 1
    # dead trees in groves: willows along the shores, gaunt trees in a few clumps on the plain, some loners
    groves = []
    for poly in PLAN['shores'].values():
        for k in rng.sample(range(len(poly)), 3):
            groves.append((poly[k][0], poly[k][1], 'BP_Scorch_Dest_DeadWillow', rng.randint(3, 5), 700.0, True))
    for _ in range(7):
        for _t in range(300):
            x, y = rng.uniform(7000, 23500), rng.uniform(-23000, 24000)
            if _free(x, y) and _slope(x, y) < 10:
                groves.append((x, y, 'BP_Scorch_Dest_DeadTree', rng.randint(2, 5), 900.0, False))
                break
    for _ in range(9):
        for _t in range(300):
            x, y = rng.uniform(6500, 24000), rng.uniform(-23500, 24500)
            if _free(x, y) and _slope(x, y) < 12:
                groves.append((x, y, 'BP_Scorch_Dest_DeadTree', 1, 0.0, False))
                break
    t = 0
    for (gx, gy, bp, count, spread, shore) in groves:
        for _ in range(count):
            for _t in range(60):
                x, y = gx + rng.gauss(0, spread * 0.6), gy + rng.gauss(0, spread * 0.6)
                pdist = _pond_dist(x, y)
                if shore and not (250 < pdist < 1400):
                    continue
                if not shore and not _free(x, y):
                    continue
                if _near_road(x, y, 650) or _slope(x, y) > 14:
                    continue
                spawn_bp(ENV + bp, 'REAR_Detail_Tree_%02d' % t, (x, y, gz(x, y)), rng.uniform(0, 360), 'Detail')
                t += 1
                n['tree'] += 1
                break
    # cactus clumps (cover blueprints)
    for k in range(26):
        for _ in range(200):
            x, y = rng.uniform(6000, 24000), rng.uniform(-23500, 24500)
            if not _free(x, y) or _slope(x, y) > 12 or _pond_dist(x, y) < 1500:
                continue
            for j in range(rng.randint(1, 3)):
                xx, yy = x + rng.uniform(-500, 500), y + rng.uniform(-500, 500)
                spawn_bp(ENV + 'NaturalCover/Cactus/BP_Cover_CactusBush_%s' % rng.choice(cacti),
                         'REAR_Detail_Cactus_%02d_%d' % (k, j), (xx, yy, gz(xx, yy)), rng.uniform(0, 360), 'Detail')
                n['cactus'] += 1
            break
    # dead shrubs: denser near water and road margins
    for k in range(220):
        for _ in range(100):
            x, y = rng.uniform(5200, 24500), rng.uniform(-24000, 25500)
            if _in_pond(x, y, 250) or _near_road(x, y, 380) or _in_station(x, y) or _in_depot(x, y) \
                    or (3900 < x < 8200 and -6400 < y < -900):
                continue
            pd = _pond_dist(x, y)
            if pd > 2500 and rng.random() < 0.55:
                continue
            spawn_mesh(rng.choice(shrubs), 'REAR_Detail_Shrub_%03d' % k, (x, y, gz(x, y) - 5.0), rng.uniform(0, 360),
                       'Detail', scale=rng.uniform(0.8, 1.5), collision=False)
            n['shrub'] += 1
            break
    # power line poles from the base hub to the station along the station road
    road = PLAN['roads']['S']
    acc, last, k = 0.0, None, 0
    for i in range(1, len(road)):
        a0, b0 = road[i - 1], road[i]
        acc += math.hypot(b0[0] - a0[0], b0[1] - a0[1])
        if acc >= 3500.0 and i < len(road) - 4:
            acc = 0.0
            dx, dy = b0[0] - a0[0], b0[1] - a0[1]
            L = math.hypot(dx, dy) or 1.0
            px, py = b0[0] - dy / L * 650.0, b0[1] + dx / L * 650.0
            spawn_bp(ENV + 'Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric2', 'REAR_Detail_Pole_%02d' % k,
                     (px, py, gz(px, py)), math.degrees(math.atan2(dy, dx)), 'Detail')
            k += 1
    log('detail: %s, poles %d' % (n, k))


def build_all():
    require_map()
    step_water()
    step_roads()
    step_rail()
    step_station()
    step_depot()
    step_detail()
    step_extras()
    step_decals()
    step_edges()
    step_skirt()


# ---- extras: saddle defences, depot edges, station yard odds and ends, road-end wreck, ground decals --------------
def _decal(label, mat, x, y, yaw, size_yz, depth=180.0):
    d = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, gz(x, y)),
                                            unreal.Rotator(0.0, -90.0, yaw))
    d.set_actor_label(label)
    d.set_folder_path(FOLDER + 'Decals')
    c = d.get_component_by_class(unreal.DecalComponent)
    c.set_decal_material(unreal.load_asset(mat))
    c.set_editor_property('decal_size', unreal.Vector(depth, size_yz, size_yz))
    return d


def step_extras():
    clear_prefix('REAR_Extra_')
    rng = random.Random(41)
    MIL = ENV + 'Military/'
    # saddle: hedgehogs either side of the cutting, sandbag arcs on the crest shoulders facing the base
    sx, sy = 12550.0, -9500.0
    for k, (dx, dy) in enumerate([(-500, 650), (-200, 900), (450, -700), (750, -950), (-850, 420)]):
        x, y = sx + dx, sy + dy
        spawn_bp(MIL + 'Barricades/BP_Military_Dest_Hedgehog4', 'REAR_Extra_Hedgehog_%d' % k, (x, y, gz(x, y)),
                 rng.uniform(0, 90), 'Extras')
    for k, (x, y, yaw) in enumerate([(11800, -8000, 140), (13550, -10700, 140)]):
        spawn_bp(ENV + 'NaturalCover/SandBagWalls/BP_Cover_RusSandbags_LongArc', 'REAR_Extra_Sandbags_%d' % k,
                 (x, y, gz(x, y)), yaw, 'Extras')
    # depot: flood lights on the corners, concrete barriers along the north-west edge
    dp = PLAN['depot']
    a = math.radians(dp['yaw'])
    cx, cy = dp['centre']

    def W(u, v):
        return cx + u * math.cos(a) - v * math.sin(a), cy + u * math.sin(a) + v * math.cos(a)
    for k, (u, v) in enumerate([(-3800, -2600), (3800, -2600), (3800, 2600), (-3800, 2600)]):
        x, y = W(u, v)
        spawn_bp(ENV + 'Industrial/Poles/BP_Industrial_Dest_CrushablePoleLight_1', 'REAR_Extra_DepotLight_%d' % k,
                 (x, y, gz(x, y)), dp['yaw'] + 45 + 90 * k, 'Extras')
    for k in range(7):
        u = -3300 + k * 650
        if -900 < u < 300:      # gap where road D comes in
            continue
        x, y = W(u, 2750)
        spawn_bp(MIL + 'Barricades/BP_Military_Dest_ConcreteBarrier', 'REAR_Extra_DepotBarrier_%d' % k,
                 (x, y, gz(x, y)), dp['yaw'], 'Extras')
    # station yard north: sleeper piles + a lineman's hut by the line
    for k, (x, y) in enumerate([(20300, 18800), (20450, 19600), (22300, 21000)]):
        spawn_bp(ENV + 'NaturalCover/Logs/BP_Cover_Log%d' % (k + 1), 'REAR_Extra_Sleepers_%d' % k, (x, y, gz(x, y)),
                 rng.uniform(80, 100), 'Extras')
    spawn_mesh('/Game/MillitaryBase/Meshes/SM_Shed', 'REAR_Extra_LinemanHut', (20250.0, 21600.0, gz(20250, 21600)), 90.0,
               'Extras')
    # road-end wreck: an abandoned KV-1 where the east track runs out
    spawn_bp(MIL + 'DestTanks/BP_Env_Destroyed_KV1E', 'REAR_Extra_RoadEndWreck', (23650.0, -400.0, gz(23650, -400)),
             -70.0, 'Extras')
    log('extras placed')


# ---- decals: craters on their terrain hollows, erosion channels on the gullies, wear and debris ----------------------
DECALS = '/Game/RTS_Survival/Materials/Decals/'
SCATTER = ['/Game/Crater_Decals/Decals/Dirt/MI_Decal_Dirt_Scatter_0%d' % i for i in (1, 2, 3)]
EROSION = [DECALS + 'DirtTracks/MI_Decal_Erosion_Channel_0%d' % i for i in (2, 3, 4, 5)]
TRACKS = [DECALS + 'DirtTracks/MI_Decal_Dirt_TracksSandy_0%d' % i for i in (1, 2, 3, 4)]
DEBRIS = [DECALS + 'Concrete/MI_RTS_Decal_Concrete_Debris_0%d_Sandy' % i for i in (1, 2, 3, 5)]
ARID = '/Game/Megascans/Decals/Arid_Debris_vbzebfrh/MI_Arid_Debris_vbzebfrh_2K'


def step_decals():
    """Every crater decal sits on the bowl rear_terrain_gen.py dug for it (plan 'craters'); erosion channels lie in the
    gullies it cut; tyre ruts at the junctions, debris on the depot slab, dirt scatter around wrecks and rocks.
    Only decal materials the map already uses."""
    clear_prefix('REAR_Decal_')
    clear_prefix('REAR_CraterStone_')
    rng = random.Random(77)
    stones = ['/Game/RTS_Survival/Meshes/Desert/Rocks/SM_Rock_0%d' % i for i in (1, 2, 3, 6, 7)]
    n = {'crater': 0, 'scatter': 0, 'erosion': 0, 'tracks': 0, 'debris': 0, 'arid': 0, 'stones': 0}
    for k, c in enumerate(PLAN['craters']):
        x, y, r = c['x'], c['y'], c['r']
        _decal('REAR_Decal_Crater_%02d' % k, c['mat'], x, y, c['yaw'], r * 1.25, depth=260.0)
        n['crater'] += 1
        if c['scatter']:
            sx, sy = x + math.cos(c['scatter_a']) * r * 0.9, y + math.sin(c['scatter_a']) * r * 0.9
            _decal('REAR_Decal_CraterScatter_%02d' % k, rng.choice(SCATTER), sx, sy, rng.uniform(0, 360), r * 1.1)
            n['scatter'] += 1
        for j in range(rng.randint(1, 3)):          # thrown-out stones on the rim
            a = rng.uniform(0, 6.28)
            px, py = x + math.cos(a) * r * rng.uniform(1.05, 1.5), y + math.sin(a) * r * rng.uniform(1.05, 1.5)
            spawn_mesh(rng.choice(stones), 'REAR_CraterStone_%02d_%d' % (k, j), (px, py, gz(px, py) - 8.0),
                       rng.uniform(0, 360), 'Decals', scale=rng.uniform(0.25, 0.5), collision=False)
            n['stones'] += 1
    for k, g in enumerate(PLAN['gullies']):
        a, b = g['pts'][0], g['pts'][-1]
        mx, my = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0
        heading = math.degrees(math.atan2(a[1] - b[1], a[0] - b[0]))
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        actor = _decal('REAR_Decal_Erosion_%02d' % k, rng.choice(EROSION), mx, my, heading, 190.0, depth=300.0)
        actor.get_component_by_class(unreal.DecalComponent).set_editor_property(
            'decal_size', unreal.Vector(300.0, 190.0, length * 0.55))
        n['erosion'] += 1
    # tyre ruts where the roads fork and where they reach the depot and the station forecourt
    for k, (x, y, hd) in enumerate([(8600, -1600, 30), (10100, -350, 80), (16400, -13500, 20), (19450, 9300, 70),
                                    (17200, -12800, 5), (6800, -800, 10)]):
        _decal('REAR_Decal_Tracks_%02d' % k, rng.choice(TRACKS), x, y, hd + 90.0, rng.uniform(300, 420))
        n['tracks'] += 1
    # debris on the depot slab and the station forecourt (decals also project onto the concrete tiles)
    dp = PLAN['depot']
    aa = math.radians(dp['yaw'])
    for k in range(10):
        u, v = rng.uniform(-3300, 3300), rng.uniform(-2100, 2100)
        x = dp['centre'][0] + u * math.cos(aa) - v * math.sin(aa)
        y = dp['centre'][1] + u * math.sin(aa) + v * math.cos(aa)
        _decal('REAR_Decal_Debris_%02d' % k, rng.choice(DEBRIS), x, y, rng.uniform(0, 360), rng.uniform(220, 380),
               depth=200.0)
        n['debris'] += 1
    # dirt scatter + arid debris around wrecks, big rocks and the station goods shed
    anchors = [a for a in all_actors() if a.get_actor_label().startswith(('REAR_Detail_Rock', 'REAR_Extra_RoadEndWreck',
                                                                          'REAR_Depot_Prop_16', 'REAR_Station_GoodsShed'))]
    rng.shuffle(anchors)
    for k, a in enumerate(anchors[:24]):
        o = a.get_actor_location()
        ang = rng.uniform(0, 6.28)
        x, y = o.x + math.cos(ang) * 250.0, o.y + math.sin(ang) * 250.0
        if _in_pond(x, y, 300.0):
            continue
        if k % 3 == 0:
            _decal('REAR_Decal_Arid_%02d' % k, ARID, x, y, rng.uniform(0, 360), rng.uniform(220, 320))
            n['arid'] += 1
        else:
            _decal('REAR_Decal_Scatter_%02d' % k, rng.choice(SCATTER), x, y, rng.uniform(0, 360), rng.uniform(250, 400))
            n['scatter'] += 1
    log('decals: %s' % n)


# ---- horizon skirt: the survival map's end-of-map planes around the whole landscape ------------------------------
SKIRT_MATERIAL = '/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_HorizonSkirt'
SKIRT_Z = 97.0           # just under the lowest landscape boundary height (99): the planes never poke through
LAND_RECT = (-41100.0, -25201.0, 30900.0, 25799.0)
HOLE_SOUTH_Z = 142.0     # south-west hole border is 144 all round
HOLE_NORTH_Z = 121.0     # north-west hole border runs 123..144
SKIRT_REACH = 150000.0


def _skirt_plane(label, cx, cy, sx, sy):
    a = spawn_mesh('/Engine/BasicShapes/Plane', label, (cx, cy, SKIRT_Z), 0.0, 'Edges', scale=(sx / 100.0, sy / 100.0, 1.0),
                   collision=False, materials=[SKIRT_MATERIAL])
    comp = a.static_mesh_component
    comp.set_editor_property('cast_shadow', False)
    comp.set_editor_property('hidden_in_scene_capture', True)    # keep it out of the minimap capture
    return a


def step_skirt():
    """Same planes as SMD_HorizonSkirt_* on SM_Desert_01: flat landscape sand past the landscape edge out to 1.5 km, no
    collision/nav/shadow, hidden from scene captures. Four strips around the landscape rectangle (20 cm under its edge)
    plus fills for the missing landscape sections in the two west corners. Tucked 4 m under the landscape edge."""
    clear_prefix('REAR_Skirt_')
    x0, y0, x1, y1 = LAND_RECT
    ov = 400.0          # tucked 4 m under the landscape so no sky shows through the edge step at RTS angles
    R = SKIRT_REACH
    _skirt_plane('REAR_Skirt_PX', (x1 - ov + R) / 2.0, 0.0, R - (x1 - ov), 2.0 * R)
    _skirt_plane('REAR_Skirt_NX', (x0 + ov - R) / 2.0, 0.0, (x0 + ov) + R, 2.0 * R)
    _skirt_plane('REAR_Skirt_PY', (x0 + x1) / 2.0, (y1 - ov + R) / 2.0, x1 - x0, R - (y1 - ov))
    _skirt_plane('REAR_Skirt_NY', (x0 + x1) / 2.0, (y0 + ov - R) / 2.0, x1 - x0, (y0 + ov) + R)
    # missing 15 m sections: merge each row's run of missing components into one plane
    n = 0
    for row in _missing_component_runs():
        cx0, cx1, cy0, cy1 = row
        a = _skirt_plane('REAR_Skirt_Hole_%02d' % n, (cx0 + cx1) / 2.0, (cy0 + cy1) / 2.0, cx1 - cx0 + 2 * ov,
                         cy1 - cy0 + 2 * ov)
        # the holes border the user's dressed ground (not eased): sit just under that border's lowest height
        o = a.get_actor_location()
        a.set_actor_location(unreal.Vector(o.x, o.y, HOLE_SOUTH_Z if o.y < 0 else HOLE_NORTH_Z), False, False)
        n += 1
    log('skirt: 4 strips + %d hole fills at z %.0f' % (n, SKIRT_Z))


def _missing_component_runs():
    """World rectangles of missing landscape components (one per row run). A component is missing where every paint
    layer weighs 0 in the full-grid export (terrain/check_ext_*), sampled at the component's centre vertex."""
    import array
    gx0, gy0, gx1, gy1 = PLAN['grid']
    sx = gx1 - gx0 + 1
    total = None
    for name in LAYER_NAMES:
        with open(TERRAIN + 'check_ext_%s.r8' % name, 'rb') as handle:
            data = array.array('B', handle.read())
        total = data if total is None else array.array('H', (p + q for p, q in zip(total, data)))
    x0, y0 = LAND_RECT[0], LAND_RECT[1]
    ncx, ncy = (gx1 - gx0) // 15, (gy1 - gy0) // 15
    runs = []
    for cj in range(ncy):
        start = None
        for ci in range(ncx + 1):
            missing = ci < ncx and total[(cj * 15 + 7) * sx + ci * 15 + 7] == 0
            if missing and start is None:
                start = ci
            if not missing and start is not None:
                runs.append((x0 + start * 1500.0, x0 + ci * 1500.0, y0 + cj * 1500.0, y0 + (cj + 1) * 1500.0))
                start = None
    return runs


def _landscape_under(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True) or []
    return any(h.to_tuple()[9] is not None and h.to_tuple()[9].get_class().get_name() == 'Landscape' for h in hits)


# ---- landscape extension -------------------------------------------------------------------------------------------
def extend_landscape():
    """Adds the 4 x 34 components east of the old landscape edge (vertex x 765 -> 825, world x 24900 -> 30900) once,
    and stretches both runtime virtual texture volumes over them. Heights/paint come from import_region('rear', ...)."""
    info = grid_info()
    if info[2] < 825:
        added = unreal.RTSShortsLandscapeLibrary.add_landscape_components(landscape(), 51, 5, 54, 38)
        log('added %d landscape components' % added)
    else:
        log('landscape already extended: %s' % info)
    for a in all_actors():
        if a.get_class().get_name() != 'RuntimeVirtualTextureVolume':
            continue
        loc, sc = a.get_actor_location(), a.get_actor_scale3d()
        want = 30900.0 - loc.x
        if abs(sc.x - want) > 1.0:
            a.set_actor_scale3d(unreal.Vector(want, sc.y, sc.z))
            log('%s scale x %.0f -> %.0f' % (a.get_actor_label(), sc.x, want))


# ---- map edges over the new ridges -----------------------------------------------------------------------------------
def step_edges():
    """The map's own edge blockers span z 0..200 only; the station ridge and the depot ridge rise over them where they
    meet the east, north and south edges. These copies of TopBlocker cover the rear edges up to ~3 m above the ground
    (kept low so aircraft flying in over the edges never touch them)."""
    clear_prefix('REAR_Edge_')
    src = [a for a in all_actors() if a.get_actor_label() == 'TopBlocker'][0]
    # (label, centre x, centre y, half x, half y, z bottom, z top) in cm; the source brush is a 200 cm cube
    walls = [('REAR_Edge_East', 24775.0, 500.0, 600.0, 25600.0, -200.0, 900.0),
             ('REAR_Edge_North', 17500.0, 25130.0, 13600.0, 600.0, -200.0, 1500.0),
             ('REAR_Edge_South', 17500.0, -24387.0, 13600.0, 600.0, -200.0, 1500.0)]
    for label, x, y, hx, hy, z0, z1 in walls:
        a = actors_sub().duplicate_actor(src)
        a.set_actor_label(label)
        a.set_folder_path(FOLDER + 'Edges')
        a.set_actor_location(unreal.Vector(x, y, (z0 + z1) / 2.0), False, False)
        a.set_actor_scale3d(unreal.Vector(hx / 100.0, hy / 100.0, (z1 - z0) / 200.0))
        o, e = a.get_actor_bounds(False)
        log('%s bounds o (%.0f,%.0f,%.0f) e (%.0f,%.0f,%.0f)' % (label, o.x, o.y, o.z, e.x, e.y, e.z))
