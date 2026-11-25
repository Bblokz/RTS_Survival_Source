"""Places the Ore Mountains exploration objects and the player's start base. Editor Python (unreal module only).

Run in an editor that has Ger_2ndOreMountains open (it does not save; save the map yourself):
    py "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission/place_exploration.py"
or import it and call place_all(). Rerunnable: every actor it places is labelled ORE_Explore_* (outliner folder
OreMountains/Exploration, actor tag ORE_Explore) and is deleted and placed again on each run.

What (provisional spots until the owner picks; Saved/OreMountains/spots/ore_spot_candidates.md/.json):
    lookout towers T1, T2                 BP_OreLookoutTower, yaw 0 (door south), footprint centred on the spot
    buried armour S1, P1, V1, L3, W1      BP_BuriedArmour_*, nose (+X) to the flatter, clearer end of the footprint
    heap stations HN1, HB3                BP_OreHeapStation, +X (drop-off side) facing the heap's metal
    HN1 metal                             2 Ore metal resources on spoil heap 1 (it had none), if none within 40 m
    start base                            Gamma Facility, refinery, barracks as pre-built buildings
                                          (bStartAsConvertedToBuilding); HQ see HQ_MODE
Ground: line trace onto the landscape, else the generator height file (Ore_height.raw), else the spot JSON z.
Buried armour and stations are nudged up to 10 m, base buildings up to 30 m, off colliding actors (units, flat road
wear and bounds over 25 m are ignored); base buildings also off ground uneven by more than 1.2 m, 8 m clear of the
straight lorry line ramp_main -> CapturedTruckDropOff, 18 m clear of the drop-off pad and off the HQ spot.
Heap stations need AOreHeapStation in the loaded DLL (DebugGame until Development is rebuilt); without it they are
skipped with a warning. Nothing here rebuilds navigation: the editor does that after the placement.
Report: Saved/OreMountains/exploration/place_report.json. Docs: Source/Docs/Campaign/OreMountainsExploration.md.
Test (throwaway file copy, saves nothing): test_place_exploration.py.
"""
import array
import json
import math
import os

import unreal

PREFIX = 'ORE_Explore_'
TAG = 'ORE_Explore'
FOLDER = 'OreMountains/Exploration'
SPOTS_JSON = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/spots/ore_spot_candidates.json'
LAYOUT_JSON = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_layout.json'
REPORT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/exploration/'

# Provisional picks (owner has not picked yet). Change a label here to move an item to another candidate.
PICKS = {'tower': ['T1', 'T2'], 'stug': ['S1'], 'pz4': ['P1'], 'sdkfz251': ['V1'], 'luchs': ['L3'], 'wespe': ['W1'],
         'heap_n': ['HN1'], 'heap_b': ['HB3']}

# 'profile': no HQ is placed; the profile's HQ truck spawns at RTS_PLayerStart (the HQ spot) as on every map.
# 'preplaced': also places Ger_HQ_Truck as a built HQ and turns on DoNotLoadProfileUnits on RTS_PLayerStart (else the
# profile spawns a second HQ truck). Then the Opening stage must call ACPPController::MissionOnHqSpawned once, which
# runs OnPlayerProfileLoadComplete (game-setting resources, HQ resource cards) as the Intro mission does. See the doc.
# 'airdrop' (owner, 2026-11-24, current): no base is placed and DoNotLoadProfileUnits is on; the Opening stage flies in
# the HQ, three harvesters and the Gamma / refinery / barracks trucks and calls MissionOnHqSpawned when the HQ lands.
HQ_MODE = 'airdrop'
PLACE_HN1_METAL = True

ENV = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
NOMADIC = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
METAL = '/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/OreMountains/'
CLASSES = {
    'tower': ENV + 'LookoutTowers/BP_OreLookoutTower',
    'stug': ENV + 'BuriedArmour/BP_BuriedArmour_StuGIII',
    'pz4': ENV + 'BuriedArmour/BP_BuriedArmour_PzIV_F1_Assault',
    'sdkfz251': ENV + 'BuriedArmour/BP_BuriedArmour_Sdkfz251_MortarTW',
    'luchs': ENV + 'BuriedArmour/BP_BuriedArmour_FlakLuchs',
    'wespe': ENV + 'BuriedArmour/BP_BuriedArmour_Wespe',
    'station': ENV + 'HeapStations/BP_OreHeapStation',
    'metal_a': METAL + 'Resource_Metal_Ore_Glaskopf',
    'metal_b': METAL + 'Resource_Metal_Ore_Katzengold',
}
VEHICLE_NAMES = {'stug': 'StuG', 'pz4': 'PzIV', 'sdkfz251': 'Sdkfz251', 'luchs': 'FlakLuchs', 'wespe': 'Wespe'}
# Base buildings: layout point, truck Blueprint, building mesh (footprint); yaw 0 = front north, over the valley.
BASE = [
    ('Gamma', 'b_gamma', NOMADIC + 'GerEco/OpelBlitz_RadiationAbsorber/Ger_GammaFacility_Truck',
     NOMADIC + 'GerEco/OpelBlitz_RadiationAbsorber/BuildingMeshes/SM_Ger_GammaFacility_BuildingMesh'),
    ('Refinery', 'b_refinery', NOMADIC + 'GerEco/OpelBlitz_Ref/Ger_Refinery_Truck',
     NOMADIC + 'GerEco/OpelBlitz_Ref/BuildingMeshes/SM_OpelRefinery_BuildingMesh'),
    ('Barracks', 'b_barracks', NOMADIC + 'GerT1/OpelBlitz_Barracks/Ger_Barracks_Truck',
     NOMADIC + 'GerT1/OpelBlitz_Barracks/BuildingMeshes/SM_Ger_Barracks_BuildingMesh'),
]
HQ = ('HQ', 'hq', NOMADIC + 'SKDFZ-7/Ger_HQ_Truck', NOMADIC + 'SKDFZ-7/BuildingMeshes/HQ_BuildingMesh')

TOWER_FOOTPRINT = (-360.0, 1020.0, -240.0, 1907.0)     # X min/max, Y min/max around the pivot (OreLookoutTowers.md)
BURIED_FOOTPRINT = (600.0, 450.0)                      # half length / width (12 x 9 m spot footprint)
STATION_HALF = 540.0                                   # placeholder ore bin 10.7 x 10.7 m
DROP_OFF_TAG = 'CapturedTruckDropOff'
PLAYER_START_FLAG = 'DoNotLoadProfileUnits'             # Blueprint variable on RTS_PLayerStart
ROUTE_CLEARANCE = 800.0
DROP_OFF_CLEARANCE = 1800.0
BASE_MAX_RANGE = 120.0                                 # height range (cm) allowed under a base building
BASE_PAD = 150.0                                       # free margin round a base building
NUDGE_RINGS = [r * 200.0 for r in range(1, 16)]         # base buildings: up to 30 m
BURIED_RINGS = [r * 200.0 for r in range(1, 6)]         # buried armour, stations: up to 10 m off the spot
FLAT_OBSTACLE_HALF_HEIGHT = 40.0
MAX_OBSTACLE_HALF = 2500.0
NOMINAL_OBSTACLE_HALF = 400.0
TOWER_ENTRANCE = (-420.0, 1350.0)                      # entrance_01 socket, kept clear (2 x 2 m)
FLAT_LABELS = ('Road_Wear', 'GroundPath', 'ORE_Road_', 'Streak', 'Decal')   # driven over, never obstacles
METAL_SEARCH_RADIUS = 4000.0

REPORT = {'placed': [], 'warnings': [], 'ground': {}}
_HEIGHTS = None
_LAYOUT = None


# ---------------------------------------------------------------------------------------------------- basics
def log(text):
    unreal.log_warning('ORE_EXPLORE ' + str(text))


def warn(text):
    REPORT['warnings'].append(text)
    log('WARN ' + text)


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def layout():
    global _LAYOUT
    if _LAYOUT is None:
        with open(LAYOUT_JSON, 'r') as handle:
            _LAYOUT = json.load(handle)
    return _LAYOUT


def feature_point(fid):
    return next(f for f in layout()['features'] if f['id'] == fid)['points'][0]


def spots():
    with open(SPOTS_JSON, 'r') as handle:
        return {c['label']: c for c in json.load(handle)['candidates']}


def clear_previous():
    count = 0
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors_sub().destroy_actor(actor)
            count += 1
    log('cleared %d previous ORE_Explore_ actors' % count)
    return count


# ---------------------------------------------------------------------------------------------------- ground
def height_file_z(x, y):
    """Bilinear height from the generator file (the landscape was imported from it)."""
    global _HEIGHTS
    lay = layout()['landscape']
    if _HEIGHTS is None:
        with open(layout()['files']['height'], 'rb') as handle:
            _HEIGHTS = array.array('H', handle.read())
    vx, vy = lay['verts_x'], lay['verts_y']
    fx = min(max((x - lay['corner'][0]) / lay['quad_cm'], 0.0), vx - 1.001)
    fy = min(max((y - lay['corner'][1]) / lay['quad_cm'], 0.0), vy - 1.001)
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    tx, ty = fx - ix, fy - iy

    def h(i, j):
        return (_HEIGHTS[j * vx + i] - 32768.0) * 100.0 / 128.0

    return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) +
            h(ix, iy + 1) * (1 - tx) * ty + h(ix + 1, iy + 1) * tx * ty)


def trace_z(x, y):
    """Landscape hit straight down, skipping anything that is not the landscape; None when nothing is hit."""
    ignore = []
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(
            world(), unreal.Vector(x, y, 12000.0), unreal.Vector(x, y, -3000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        values = hit.to_tuple()
        if not values[0]:
            return None
        actor = values[9]
        if actor is None or 'Landscape' in actor.get_class().get_name():
            return values[4].z
        ignore.append(actor)
    return None


def ground_z(x, y, fallback=None):
    z = trace_z(x, y)
    if z is not None:
        REPORT['ground']['trace'] = REPORT['ground'].get('trace', 0) + 1
        return z
    try:
        z = height_file_z(x, y)
        REPORT['ground']['height_file'] = REPORT['ground'].get('height_file', 0) + 1
        return z
    except (OSError, KeyError, ValueError) as error:
        if fallback is None:
            raise
        warn('no trace hit and no height file (%s); using the spot z' % error)
        REPORT['ground']['spot_z'] = REPORT['ground'].get('spot_z', 0) + 1
        return fallback


def local_to_world(cx, cy, yaw, lx, ly):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return cx + lx * c - ly * s, cy + lx * s + ly * c


def footprint_heights(cx, cy, yaw, x0, x1, y0, y1, steps=4):
    zs = []
    for i in range(steps + 1):
        for j in range(steps + 1):
            wx, wy = local_to_world(cx, cy, yaw, x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * j / steps)
            zs.append(height_file_z(wx, wy))
    return min(zs), max(zs)


# ---------------------------------------------------------------------------------------------------- obstacles
SKIP_CLASS_PARTS = ('Landscape', 'Volume', 'Light', 'Sky', 'Fog', 'PostProcess', 'Decal', 'Note', 'WorldSettings',
                    'PlayerStart', 'Camera', 'Niagara', 'Brush', 'Manager', 'NavigationData', 'RecastNavMesh',
                    'AbstractNavData', 'GameplayDebugger', 'WorldDataLayers', 'LevelBounds', 'Info')


def collect_obstacles(centres, radius):
    """Colliding bodies near the centres: (x, y, half_x, half_y, label), one per ISM instance or per actor."""
    out = []
    near = lambda x, y: any((x - cx) ** 2 + (y - cy) ** 2 < radius ** 2 for cx, cy in centres)
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            continue
        cls = actor.get_class().get_name()
        if any(part in cls for part in SKIP_CLASS_PARTS) or any(part in actor.get_actor_label() for part in FLAT_LABELS):
            continue
        # Units drive away (and other tools may still be placing them): their turrets and weapons hang off them.
        parent = actor.get_attach_parent_actor()
        if isinstance(actor, unreal.Pawn) or (parent is not None and isinstance(parent, unreal.Pawn)):
            continue
        instanced = [c for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
                     if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION]
        for comp in instanced:
            mesh = comp.get_editor_property('static_mesh')
            if mesh is None:
                continue
            b = mesh.get_bounding_box()
            size = max(b.max.x - b.min.x, b.max.y - b.min.y) * 0.5
            for index in range(comp.get_instance_count()):
                xf = comp.get_instance_transform(index, True)
                xf = xf[-1] if isinstance(xf, tuple) else xf
                loc, sc = xf.translation, xf.scale3d
                if near(loc.x, loc.y):
                    # Trunk and rock cores, not the crowns: 60 % of the mesh's half size.
                    r = size * max(abs(sc.x), abs(sc.y)) * 0.6
                    out.append((loc.x, loc.y, r, r, actor.get_actor_label()))
        if not instanced:
            origin, extent = actor.get_actor_bounds(True)
            # Flat things (road wear, ground paths, decal meshes) are driven over: not obstacles.
            if extent.z < FLAT_OBSTACLE_HALF_HEIGHT:
                continue
            if max(extent.x, extent.y) <= 0.0:
                continue
            if max(extent.x, extent.y) >= MAX_OBSTACLE_HALF:
                # Bounds over 25 m (half) come from stray components (widgets, effects): use the actor itself.
                origin = actor.get_actor_location()
                extent = unreal.Vector(NOMINAL_OBSTACLE_HALF, NOMINAL_OBSTACLE_HALF, 0.0)
            if near(origin.x, origin.y):
                out.append((origin.x, origin.y, extent.x, extent.y, actor.get_actor_label()))
        if 'TargetPoint' in cls and actor.actor_has_tag(DROP_OFF_TAG):
            loc = actor.get_actor_location()
            out.append((loc.x, loc.y, DROP_OFF_CLEARANCE, DROP_OFF_CLEARANCE, actor.get_actor_label()))
    return out


def rect_hits(obstacles, cx, cy, yaw, hl, hw):
    """Labels of obstacles whose box overlaps the yawed rectangle (separating axes on the rectangle's frame)."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    hits = []
    for ox, oy, ex, ey, label in obstacles:
        dx, dy = ox - cx, oy - cy
        lx, ly = dx * c + dy * s, -dx * s + dy * c
        px, py = abs(c) * ex + abs(s) * ey, abs(s) * ex + abs(c) * ey
        if abs(lx) < hl + px and abs(ly) < hw + py and abs(dx) < hl + hw + ex and abs(dy) < hl + hw + ey:
            hits.append(label)
    return hits


def segment_distance(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / max(vx * vx + vy * vy, 1.0)))
    return math.hypot(px - ax - t * vx, py - ay - t * vy)


def lorry_route(drop_off):
    """Straight line from the main ramp to the adit drop-off. Deliberately not a nav query, so the editor run and the
    headless test place the base the same way (a commandlet has no nav path)."""
    ramp = feature_point('ramp_main')
    REPORT['route'] = 'straight line ramp_main -> drop-off'
    return [(ramp[0], ramp[1]), (drop_off.x, drop_off.y)]


def route_distance(route, x, y):
    return min(segment_distance(x, y, *route[i], *route[i + 1]) for i in range(len(route) - 1))


# ---------------------------------------------------------------------------------------------------- spawning
def load_class(key_or_path):
    path = CLASSES.get(key_or_path, key_or_path)
    asset = unreal.load_asset(path)
    if asset is None:
        warn('missing Blueprint %s' % path)
        return None
    cls = asset.generated_class()
    if cls is None:
        warn('Blueprint %s has no generated class (parent missing in this DLL?)' % path)
    return cls


def spawn(cls, label, x, y, z, yaw, note):
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    if actor is None:
        warn('spawn failed %s' % label)
        return None
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER)
    actor.set_editor_property('tags', list(actor.get_editor_property('tags')) + [TAG, label[len(PREFIX):]])
    REPORT['placed'].append({'label': label, 'class': cls.get_name(), 'x': round(x), 'y': round(y), 'z': round(z),
                             'yaw': round(yaw, 1), 'note': note})
    log('placed %s at (%.0f, %.0f, %.0f) yaw %.0f %s' % (label, x, y, z, yaw, note))
    return actor


# ---------------------------------------------------------------------------------------------------- items
def place_towers(cands, obstacles):
    cls = load_class('tower')
    if cls is None:
        return
    x0, x1, y0, y1 = TOWER_FOOTPRINT
    off_x, off_y = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    hl, hw = (x1 - x0) / 2.0, (y1 - y0) / 2.0
    rings = [(0.0, 0.0)] + [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
                            for r in BURIED_RINGS for a in range(0, 360, 45)]
    for label in PICKS['tower']:
        spot = cands[label]
        # Yaw 0 (door, ladder and panel face south); the off-centre site is centred on the checked spot, nudged up
        # to 10 m when the site or the door point touches a colliding actor.
        first = None
        for dx, dy in rings:
            cx, cy = spot['x_cm'] + dx, spot['y_cm'] + dy
            px, py = cx - off_x, cy - off_y
            hits = rect_hits(obstacles, cx, cy, 0.0, hl, hw)
            hits += rect_hits(obstacles, px + TOWER_ENTRANCE[0], py + TOWER_ENTRANCE[1], 0.0, 100.0, 100.0)
            lo, hi = footprint_heights(px, py, 0.0, x0, x1, y0, y1)
            result = (px, py, hits, hi - lo, math.hypot(dx, dy))
            first = first or result
            if not hits and hi - lo <= 150.0:
                first = result
                break
        px, py, hits, spread, moved = first
        if hits:
            warn('tower %s: no clear site within 10 m; touches %s' % (label, sorted(set(hits))[:4]))
        if spread > 150.0:
            warn('tower %s: ground under the yaw-0 site varies %.2f m' % (label, spread / 100.0))
        spawn(cls, PREFIX + 'Tower_' + label, px, py, ground_z(px, py, spot['z_cm']), 0.0,
              'moved %.1f m, site height range %.2f m' % (moved / 100.0, spread / 100.0))


def nose_score(obstacles, x, y, yaw):
    """Lower is better: height spread and obstacles in a 10 x 6 m box in front of the nose."""
    zs = []
    for lx in (650.0, 1000.0, 1350.0, 1700.0):
        for ly in (-300.0, 0.0, 300.0):
            zs.append(height_file_z(*local_to_world(x, y, yaw, lx, ly)))
    fx, fy = local_to_world(x, y, yaw, 1150.0, 0.0)
    blocked = rect_hits(obstacles, fx, fy, yaw, 550.0, 300.0)
    return (max(zs) - min(zs)) + 400.0 * len(blocked), blocked


def choose_buried_pose(obstacles, spot, hq_xy):
    """Nose (+X) to the end of the checked footprint with flatter, clearer ground ahead; nudged up to 10 m when the
    footprint or the ground ahead touches a colliding actor. Ties face the HQ (the player arrives from there)."""
    hl, hw = BURIED_FOOTPRINT
    rings = [(0.0, 0.0)] + [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
                            for r in BURIED_RINGS for a in range(0, 360, 45)]
    fallback = None
    for dx, dy in rings:
        x, y = spot['x_cm'] + dx, spot['y_cm'] + dy
        options = []
        for yaw in (spot['yaw_deg'], spot['yaw_deg'] + 180.0):
            score, blocked = nose_score(obstacles, x, y, yaw)
            to_hq = math.degrees(math.atan2(hq_xy[1] - y, hq_xy[0] - x))
            facing = abs((yaw - to_hq + 180.0) % 360.0 - 180.0)
            hits = rect_hits(obstacles, x, y, yaw, hl, hw)
            lo, hi = footprint_heights(x, y, yaw, -hl, hl, -hw, hw)
            options.append((len(hits), round(score / 25.0), facing, yaw, score, blocked, hits, hi - lo))
        options.sort()
        best = options[0]
        result = (x, y, (best[3] + 180.0) % 360.0 - 180.0, best[4], best[5], best[6], best[7], math.hypot(dx, dy))
        if fallback is None:
            fallback = result
        if not best[6] and not best[5] and best[7] < 60.0:
            return result
    return fallback


def place_buried(cands, obstacles, hq_xy):
    for key in ('stug', 'pz4', 'sdkfz251', 'luchs', 'wespe'):
        cls = load_class(key)
        if cls is None:
            continue
        for label in PICKS[key]:
            spot = cands[label]
            x, y, yaw, score, blocked, hits, spread, moved = choose_buried_pose(obstacles, spot, hq_xy)
            if hits or blocked:
                warn('buried %s %s: no clear pose within 10 m; touches %s, ahead %s' %
                     (key, label, sorted(set(hits))[:4], sorted(set(blocked))[:4]))
            note = 'moved %.1f m, nose score %.0f, range %.2f m' % (moved / 100.0, score, spread / 100.0)
            spawn(cls, PREFIX + 'Buried_%s_%s' % (VEHICLE_NAMES[key], label), x, y,
                  ground_z(x, y, spot['z_cm']), yaw, note)


def metal_near(x, y, radius):
    found = []
    for actor in actors_sub().get_all_level_actors():
        if 'Resource_Metal' in actor.get_class().get_name():
            loc = actor.get_actor_location()
            if math.hypot(loc.x - x, loc.y - y) < radius:
                found.append(loc)
    return found


def place_hn1_metal(spot):
    """Spoil heap 1 had no metal: two Ore metal resources on its flattest ground, clear of the station."""
    if metal_near(spot['x_cm'], spot['y_cm'], METAL_SEARCH_RADIUS):
        log('HN1 already has metal within %.0f m' % (METAL_SEARCH_RADIUS / 100.0))
        return []
    heap = next(r for r in layout()['regions'] if r['id'] == 'spoil_heap_1')
    hx, hy = heap['centre']
    picks = []
    candidates = []
    for r in (0.0, 400.0, 800.0, 1100.0):
        for a in range(0, 360, 30):
            x, y = hx + r * math.cos(math.radians(a)), hy + r * math.sin(math.radians(a))
            if math.hypot(x - spot['x_cm'], y - spot['y_cm']) < 1300.0:
                continue
            lo, hi = footprint_heights(x, y, 0.0, -450.0, 450.0, -450.0, 450.0, steps=3)
            candidates.append((hi - lo, x, y))
    candidates.sort()
    for spread, x, y in candidates:
        if all(math.hypot(x - px, y - py) > 1000.0 for _, px, py in picks):
            picks.append((spread, x, y))
        if len(picks) == 2:
            break
    placed = []
    for index, (spread, x, y) in enumerate(picks):
        cls = load_class('metal_a' if index == 0 else 'metal_b')
        if cls is None:
            continue
        yaw = (index * 67.0) % 360.0
        if spawn(cls, PREFIX + 'HN1_Metal_%d' % (index + 1), x, y, ground_z(x, y), yaw,
                 'spoil heap 1, ground spread %.2f m' % (spread / 100.0)):
            placed.append(unreal.Vector(x, y, 0.0))
    return placed


def place_stations(cands, obstacles):
    # The HN1 metal goes in even when the station class is missing (Development DLL), so the heap is worth taking.
    hn1_metal = place_hn1_metal(cands['HN1']) if ('HN1' in PICKS['heap_n'] and PLACE_HN1_METAL) else []
    if getattr(unreal, 'OreHeapStation', None) is None:
        warn('AOreHeapStation is not in this DLL: heap stations skipped (rebuild Development or use DebugGame)')
        return
    cls = load_class('station')
    if cls is None:
        return
    for key in ('heap_n', 'heap_b'):
        for label in PICKS[key]:
            spot = cands[label]
            x, y = spot['x_cm'], spot['y_cm']
            metal = hn1_metal if label == 'HN1' else []
            metal = metal or metal_near(x, y, METAL_SEARCH_RADIUS)
            if metal:
                mx = sum(m.x for m in metal) / len(metal)
                my = sum(m.y for m in metal) / len(metal)
                target = 'metal x%d' % len(metal)
            else:
                heap = next(r for r in layout()['regions'] if r['id'] == 'spoil_heap_1')
                mx, my = heap['centre']
                target = 'heap centre'
                warn('station %s: no metal within %.0f m' % (label, METAL_SEARCH_RADIUS / 100.0))
            # Nudged up to 10 m off colliding actors; the bin and its drop-off point (8 m out on +X) must be clear.
            rings = [(0.0, 0.0)] + [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
                                    for r in BURIED_RINGS for a in range(0, 360, 45)]
            first = None
            for dx, dy in rings:
                px, py = spot['x_cm'] + dx, spot['y_cm'] + dy
                yaw = math.degrees(math.atan2(my - py, mx - px))
                hits = rect_hits(obstacles, px, py, yaw, STATION_HALF, STATION_HALF)
                hits += rect_hits(obstacles, *local_to_world(px, py, yaw, 800.0, 0.0), yaw, 250.0, 250.0)
                first = first or (px, py, yaw, hits)
                if not hits:
                    first = (px, py, yaw, hits)
                    break
            x, y, yaw, hits = first
            lo, hi = footprint_heights(x, y, yaw, -STATION_HALF, STATION_HALF, -STATION_HALF, STATION_HALF)
            if hits:
                boxes = [[round(v) for v in o[:4]] + [o[4]] for o in obstacles if o[4] in hits][:4]
                warn('station %s: no clear spot within 10 m; touches %s' % (label, boxes))
            # Sunk to the lowest ground under the bin, so no side floats on the uneven heap toe.
            z = min(lo, ground_z(x, y, spot['z_cm']))
            spawn(cls, PREFIX + 'Station_' + label, x, y, z, yaw,
                  'moved %.1f m, +X to %s at %.0f m, ground range %.2f m' % (
                      math.hypot(x - spot['x_cm'], y - spot['y_cm']) / 100.0, target,
                      math.hypot(mx - x, my - y) / 100.0, (hi - lo) / 100.0))


def building_half(mesh_path):
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        warn('building mesh missing %s; using 10 x 10 m' % mesh_path)
        return 500.0, 500.0, 0.0, 0.0
    b = mesh.get_bounding_box()
    return ((b.max.x - b.min.x) / 2.0, (b.max.y - b.min.y) / 2.0, (b.max.x + b.min.x) / 2.0, (b.max.y + b.min.y) / 2.0)


def find_base_spot(name, x, y, half, obstacles, reserved, route, drop_off):
    hl, hw, ox, oy = half
    rings = [(0.0, 0.0)] + [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
                            for r in NUDGE_RINGS for a in range(0, 360, 30)]
    best = None
    for dx, dy in rings:
        px, py = x + dx, y + dy
        cx, cy = px + ox, py + oy                  # footprint centre (yaw 0)
        reasons = []
        hits = rect_hits(obstacles + reserved, cx, cy, 0.0, hl + BASE_PAD, hw + BASE_PAD)
        if hits:
            reasons.append('touches %s' % sorted(set(hits))[:3])
        corner = math.hypot(hl, hw)
        if route_distance(route, cx, cy) < corner + ROUTE_CLEARANCE:
            reasons.append('lorry route %.0f m' % (route_distance(route, cx, cy) / 100.0))
        if math.hypot(cx - drop_off.x, cy - drop_off.y) < corner + DROP_OFF_CLEARANCE:
            reasons.append('drop-off pad')
        lo, hi = footprint_heights(cx, cy, 0.0, -hl, hl, -hw, hw)
        if hi - lo > BASE_MAX_RANGE:
            reasons.append('ground range %.2f m' % ((hi - lo) / 100.0))
        if not reasons:
            return px, py, math.hypot(dx, dy), hi - lo
        if best is None:
            best = reasons
    warn('base %s: no clear spot within %.0f m of the layout point (%s); placed at the layout point' %
         (name, NUDGE_RINGS[-1] / 100.0, best))
    return x, y, 0.0, -1.0


def set_player_start_for_hq_mode():
    """'preplaced' turns off the profile units (no second HQ truck); 'profile' only warns when they are off."""
    for actor in actors_sub().get_all_level_actors():
        if actor.get_class().get_name() != 'RTS_PLayerStart_C':
            continue
        current = actor.get_editor_property(PLAYER_START_FLAG)
        if HQ_MODE in ('preplaced', 'airdrop'):
            actor.set_editor_property(PLAYER_START_FLAG, True)
            log('RTS_PLayerStart: DoNotLoadProfileUnits on (the Opening must call MissionOnHqSpawned once)')
        elif current:
            warn('RTS_PLayerStart has DoNotLoadProfileUnits on but HQ_MODE is profile: no HQ will exist')
        REPORT['player_start_no_profile_units'] = actor.get_editor_property(PLAYER_START_FLAG)
        return
    warn('no RTS_PLayerStart in the level')


def place_base(obstacles, drop_off):
    set_player_start_for_hq_mode()
    if HQ_MODE == 'airdrop':
        log('start base: none placed (HQ_MODE airdrop: the Opening stage flies it in)')
        return
    route = lorry_route(drop_off)
    reserved = []
    if HQ_MODE == 'profile':
        # Keep the HQ spot free: the profile's HQ truck spawns there and the player deploys it there.
        hl, hw, ox, oy = building_half(HQ[3])
        hx, hy = feature_point('hq')
        reserved.append((hx + ox, hy + oy, hl, hw, 'HQ spot (reserved)'))
    entries = ([HQ] if HQ_MODE == 'preplaced' else []) + BASE
    for name, fid, truck, mesh in entries:
        cls = load_class(truck)
        if cls is None:
            continue
        half = building_half(mesh)
        x, y = feature_point(fid)
        px, py, moved, spread = find_base_spot(name, x, y, half, obstacles, reserved, route, drop_off)
        z = ground_z(px, py) + 10.0
        actor = spawn(cls, PREFIX + 'Base_' + name, px, py, z, 0.0,
                      'moved %.1f m from %s, ground range %.2f m, footprint %.0f x %.0f m' %
                      (moved / 100.0, fid, spread / 100.0, half[0] / 50.0, half[1] / 50.0))
        if actor is None:
            continue
        actor.set_editor_property('start_as_converted_to_building', True)
        hl, hw, ox, oy = half
        reserved.append((px + ox, py + oy, hl + BASE_PAD, hw + BASE_PAD, PREFIX + 'Base_' + name))


def find_drop_off():
    for actor in actors_sub().get_all_level_actors():
        if actor.actor_has_tag(DROP_OFF_TAG):
            return actor.get_actor_location()
    warn('no actor tagged %s; using the layout cargo_return point' % DROP_OFF_TAG)
    x, y = feature_point('cargo_return')
    return unreal.Vector(x, y, ground_z(x, y))


def place_all():
    REPORT['placed'], REPORT['warnings'], REPORT['ground'] = [], [], {}
    level = world().get_path_name()
    REPORT['level'] = level
    if 'OreMountains' not in level:
        warn('open level is %s, not an Ore Mountains map' % level)
    clear_previous()
    cands = spots()
    drop_off = find_drop_off()
    hq_xy = feature_point('hq')
    centres = [(c['x_cm'], c['y_cm']) for k, labels in PICKS.items() for c in (cands[l] for l in labels)]
    centres += [tuple(feature_point(fid)) for _, fid, _, _ in BASE + [HQ]]
    obstacles = collect_obstacles(centres, 4000.0)
    REPORT['obstacles'] = len(obstacles)
    place_towers(cands, obstacles)
    place_buried(cands, obstacles, hq_xy)
    place_stations(cands, obstacles)
    place_base(obstacles, drop_off)
    REPORT['hq_mode'] = HQ_MODE
    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(REPORT_DIR + 'place_report.json', 'w') as handle:
        json.dump(REPORT, handle, indent=1, default=str)
    log('done: %d placed, %d warnings, ground %s, route %s' % (len(REPORT['placed']), len(REPORT['warnings']),
                                                                REPORT['ground'], REPORT.get('route')))
    return REPORT


if __name__ == '__main__':
    place_all()
