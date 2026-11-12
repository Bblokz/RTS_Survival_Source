"""Extra static cacti on SM_Desert_01 (SMD_CactusExtra_*), weighted toward the outskirts of the start islands.

The inner start areas (build radius + 9 000 + 3 000 cm round every deploy centre) are left alone: the runtime start
content (`USurvivalStartContentSettings::Cacti`) dresses them, and unpicked starts become Soviet bases. This script reads
the LIVE level (start locations, edge spawn zones, SMD_Route* / SMD_Oasis* actors, existing cacti, fog-of-war extent),
plans 55-65 cacti with a fixed seed, spawns them, rebuilds the navmesh, waits for it, saves the map and quits.

Run in a THROWAWAY full editor with the map open (tick callbacks run there; -ExecutePythonScript would quit too early).
Launch from PowerShell (Git Bash rewrites /Game/... arguments):

    & "<UE_5.5>/Engine/Binaries/Win64/UnrealEditor.exe" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" `
        /Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01 `
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools/survival_map_cacti.py" `
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log path>

Dry run (log the plan, change nothing, quit): set the environment variable CACTIEXTRA_DRY_RUN=1 before launching, or set
DRY_RUN = True below. Result lines in the log: `CACTIEXTRA DONE placed=N size=...` or `CACTIEXTRA FAILED <reason>`.

Safety: the .umap is copied to Saved/MapBackups before any change; after the save the new file must be at least
MIN_SIZE_RATIO of the backup and MIN_UMAP_BYTES (a save during a navmesh build drops the tiles and shrinks the file),
otherwise the backup is copied back. Re-running deletes the previous SMD_CactusExtra_* actors first.
"""
import json
import math
import os
import random
import shutil
import sys
import time
import traceback

import unreal

LEVEL_TOOLS = "D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools"
if LEVEL_TOOLS not in sys.path:
    sys.path.insert(0, LEVEL_TOOLS)

try:
    import survival_map_build as smb  # noqa: E402
    import survival_map_dress as smd  # noqa: E402
    IMPORT_ERROR = None
except Exception as import_error:  # an unattended editor must still log FAILED and quit
    smb = smd = None
    IMPORT_ERROR = import_error

# ---------------------------------------------------------------------------------------------------------------------
# Settings
# ---------------------------------------------------------------------------------------------------------------------
DRY_RUN = False
MAP_NAME = "SM_Desert_01"
MAP_PACKAGE = "/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01"
UMAP_PATH = "D:/UE5Projects/RTS_Survival/Content/RTS_Survival/SurvivalMode/Maps/SM_Desert_01.umap"
BACKUP_DIR = "D:/UE5Projects/RTS_Survival/Saved/MapBackups"
LAYOUT_JSON = LEVEL_TOOLS + "/SM_Desert_01_layout.json"

LABEL = "CactusExtra_%d"  # smb adds the SMD_ prefix
LABEL_PREFIX = "SMD_CactusExtra_"  # smb.DRESS_PREFIX + "CactusExtra_"
FOLDER = "Props/CactusExtra"  # under SurvivalDressing/

SEED = 20261112
TARGET_MIN = 55
TARGET_MAX = 65
MAX_ANCHOR_ATTEMPTS = 40000

# Exclusions (cm).
START_CONTENT_REACH = 9000.0  # runtime start content band past the build edge
START_EXTRA_CLEARANCE = 3000.0
OASIS_CLEARANCE = 1500.0
ROUTE_CLEARANCE = 800.0
ROUTE_POINT_CLEARANCE = 1300.0  # SMD_Route* actors without a spline (route dust decals, ~10 m bands)
EDGE_ZONE_CLEARANCE = 4000.0
PLAYABLE_MARGIN = 2500.0
OBSTACLE_CLEARANCE = 150.0  # from the XY bounds of rocks, poles, trees and wells
MAX_OBSTACLE_HALF_EXTENT = 6000.0  # bigger SMD_ actors (skirt, salt decal, volumes) are not obstacles
MIN_SPACING = 600.0
MAX_SLOPE_DEG = 25.0
SLOPE_SAMPLE = 200.0
FOOTPRINT_RING = 300.0
FOOTPRINT_RING_SAMPLES = 6

# Distribution.
OUTSKIRT_FALLOFF = 7000.0  # acceptance halves about every 4 900 cm past a start's exclusion ring
OUTSKIRT_FLOOR = 0.12  # the far desert still gets some
SINGLE_SHARE = 0.4  # the rest are clusters of 2-4
# No start gets more than this share above an even split (the +Y corners alone have room for half the cacti).
PER_START_CAP_FACTOR = 1.35
CLUSTER_MIN = 2
CLUSTER_MAX = 4
CLUSTER_RADIUS = 1100.0
CLUSTER_MEMBER_ATTEMPTS = 16
DESTRUCTIBLE_SHARE = 0.5
BUSH_SIZES = (("Large", 0.3), ("Medium", 0.4), ("Small", 0.3))
BUSH_COLOURS = (("Green", 0.7), ("Blue", 0.3))
CACTUS_SCALE = (0.8, 1.2)

# Tick state machine.
WARMUP_TICKS = 30
NAV_MIN_TICKS = 30
NAV_IDLE_TICKS = 30
NAV_BOUNDS_GRACE_SECONDS = 90.0
SAVE_ATTEMPTS = 5
SAVE_RETRY_TICKS = 30
TIMEOUT_SECONDS = 2400.0

# Size check: the brief expected ~128 MB, but the map on disk was 51 MB on 2026-11-12, so the check is relative.
MIN_UMAP_BYTES = 40 * 1024 * 1024
MIN_SIZE_RATIO = 0.9


def log(text):
    unreal.log_warning("CACTIEXTRA " + str(text))


def is_dry_run():
    return DRY_RUN or os.environ.get("CACTIEXTRA_DRY_RUN", "0") == "1"


# ---------------------------------------------------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------------------------------------------------
def segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    t = 0.0 if length_sq <= 1e-6 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def polyline_distance(px, py, points, closed=False):
    count = len(points)
    if count == 1:
        return math.hypot(px - points[0][0], py - points[0][1])
    last = count if closed else count - 1
    return min(segment_distance(px, py, points[i][0], points[i][1], points[(i + 1) % count][0], points[(i + 1) % count][1])
               for i in range(last))


def inside_polygon(px, py, polygon):
    inside = False
    count = len(polygon)
    for i in range(count):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % count]
        if (y1 > py) != (y2 > py) and px < (x2 - x1) * (py - y1) / (y2 - y1) + x1:
            inside = not inside
    return inside


def convex_hull(points):
    pts = sorted(set(points))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def box_distance(px, py, centre, half_x, half_y, yaw_deg):
    """Distance from a point to an oriented box (0 inside)."""
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    rx, ry = px - centre[0], py - centre[1]
    local_x, local_y = rx * c + ry * s, -rx * s + ry * c
    return math.hypot(max(abs(local_x) - half_x, 0.0), max(abs(local_y) - half_y, 0.0))


def polyline_bbox(points, margin):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs) - margin, max(xs) + margin, min(ys) - margin, max(ys) + margin


# ---------------------------------------------------------------------------------------------------------------------
# Ground
# ---------------------------------------------------------------------------------------------------------------------
# Previous SMD_CactusExtra_* actors: ignored by the planning traces, so a re-run or dry run plans exactly as the first run.
IGNORED_ACTORS = []


def ground_hit(x, y):
    """(z, hit actor) of the first visibility hit straight down, or (None, None) off the landscape."""
    hit = unreal.SystemLibrary.line_trace_single(smb.world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, IGNORED_ACTORS,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, None
    values = hit.to_tuple()
    if not values[0]:
        return None, None
    # Same index as smb.ground_z for the location; the actor is found by type so the tuple order does not matter.
    actor = next((value for value in values if isinstance(value, unreal.Actor)), None)
    return values[4].z, actor


def on_landscape(actor):
    return actor is None or isinstance(actor, unreal.LandscapeProxy)


def ground_ok(x, y):
    """Landscape (nothing standing there) under the spot and round its footprint, slope under MAX_SLOPE_DEG."""
    z0, actor = ground_hit(x, y)
    if z0 is None or not on_landscape(actor):
        return None
    heights = {}
    for name, (dx, dy) in {"px": (1, 0), "nx": (-1, 0), "py": (0, 1), "ny": (0, -1)}.items():
        z, actor = ground_hit(x + dx * SLOPE_SAMPLE, y + dy * SLOPE_SAMPLE)
        if z is None or not on_landscape(actor):
            return None
        heights[name] = z
    gradient = math.hypot((heights["px"] - heights["nx"]) / (2.0 * SLOPE_SAMPLE),
                          (heights["py"] - heights["ny"]) / (2.0 * SLOPE_SAMPLE))
    if math.degrees(math.atan(gradient)) > MAX_SLOPE_DEG:
        return None
    max_step = math.tan(math.radians(MAX_SLOPE_DEG)) * FOOTPRINT_RING
    for k in range(FOOTPRINT_RING_SAMPLES):
        angle = math.tau * k / FOOTPRINT_RING_SAMPLES
        z, actor = ground_hit(x + FOOTPRINT_RING * math.cos(angle), y + FOOTPRINT_RING * math.sin(angle))
        if z is None or not on_landscape(actor) or abs(z - z0) > max_step:
            return None
    return z0


# ---------------------------------------------------------------------------------------------------------------------
# Live level
# ---------------------------------------------------------------------------------------------------------------------
class Scene(object):
    def __init__(self):
        self.starts = []  # (x, y, exclusion radius, name)
        self.reserved = []  # (x, y, radius)
        self.edge_zones = []  # (centre, half_x, half_y, yaw, label)
        self.oasis_hull = []
        self.oasis_outline = []
        self.routes = []  # (points, bbox)
        self.route_points = []  # (x, y)
        self.existing_cacti = []  # (x, y)
        self.obstacles = {}  # grid cell -> [(x0, x1, y0, y1)]
        self.obstacle_count = 0
        self.playable = None  # (x0, x1, y0, y1)
        self.playable_source = ""
        self.previous_extra = []


GRID = 5000.0


def grid_cells(x0, x1, y0, y1):
    for cx in range(int(math.floor(x0 / GRID)), int(math.floor(x1 / GRID)) + 1):
        for cy in range(int(math.floor(y0 / GRID)), int(math.floor(y1 / GRID)) + 1):
            yield cx, cy


def read_layout():
    try:
        with open(LAYOUT_JSON, "r") as handle:
            return json.load(handle)
    except Exception as error:
        log("layout json not read (%s); live actors only" % error)
        return {}


def gather_scene():
    scene = Scene()
    layout = read_layout()
    actors = smb.actors_sub().get_all_level_actors()
    oasis_points = []
    for actor in actors:
        label = actor.get_actor_label()
        class_name = actor.get_class().get_name()
        if label.startswith(LABEL_PREFIX):
            scene.previous_extra.append(actor)
            continue
        if isinstance(actor, unreal.SurvivalStartLocation):
            read_start(scene, actor)
        elif isinstance(actor, unreal.SurvivalEdgeSpawnZone):
            read_edge_zone(scene, actor)
        elif label.startswith(smb.DRESS_PREFIX + "Route"):
            read_route(scene, actor)
        elif label.startswith(smb.DRESS_PREFIX + "Oasis"):
            read_oasis_actor(actor, label, oasis_points)
        elif "Cactus" in class_name or "Cactus" in label:
            location = actor.get_actor_location()
            scene.existing_cacti.append((location.x, location.y))
        elif label.startswith(smb.DRESS_PREFIX):
            read_obstacle(scene, actor)
        elif "FogOfWar_Manager" in label or "FogOfWar_Manager" in class_name:
            read_fog_of_war(scene, actor)
    IGNORED_ACTORS[:] = scene.previous_extra
    scene.oasis_hull = convex_hull(oasis_points)
    if layout.get("oasis", {}).get("outline"):
        scene.oasis_outline = [(p[0], p[1]) for p in layout["oasis"]["outline"]]
    if not scene.routes and layout.get("routes"):
        log("WARNING no SMD_Route spline found in the level; using the layout json route polylines")
        for points in layout["routes"].values():
            pts = [(p[0], p[1]) for p in points]
            scene.routes.append((pts, polyline_bbox(pts, ROUTE_CLEARANCE)))
    if scene.playable is None:
        read_playable_fallback(scene, layout, actors)
    return scene


def read_start(scene, actor):
    location = actor.get_actor_location()
    build_radius = float(actor.get_editor_property("build_area_radius"))
    scene.starts.append((location.x, location.y, build_radius + START_CONTENT_REACH + START_EXTRA_CLEARANCE,
                         actor.get_actor_label()))
    # ReservedContentSpots: actor-relative XY (not rotated, as the planner reads them), Z is the radius.
    for spot in actor.get_editor_property("reserved_content_spots"):
        scene.reserved.append((location.x + spot.x, location.y + spot.y, spot.z))


def read_edge_zone(scene, actor):
    box = actor.get_editor_property("spawn_area")
    if box is not None:
        centre = box.get_world_location()
        extent = box.get_scaled_box_extent()
        yaw = box.get_world_rotation().yaw
        scene.edge_zones.append(((centre.x, centre.y), extent.x, extent.y, yaw, actor.get_actor_label()))
        return
    origin, extent = actor.get_actor_bounds(False)
    scene.edge_zones.append(((origin.x, origin.y), extent.x, extent.y, 0.0, actor.get_actor_label()))


def read_route(scene, actor):
    spline = actor.get_component_by_class(unreal.SplineComponent)
    if spline is None:
        location = actor.get_actor_location()
        scene.route_points.append((location.x, location.y))
        return
    length = spline.get_spline_length()
    steps = max(1, int(length / 150.0))
    points = []
    for k in range(steps + 1):
        p = spline.get_location_at_distance_along_spline(length * k / steps, unreal.SplineCoordinateSpace.WORLD)
        points.append((p.x, p.y))
    scene.routes.append((points, polyline_bbox(points, ROUTE_CLEARANCE)))


def read_oasis_actor(actor, label, oasis_points):
    # The water planes are scaled ~35 m past the basin on every side, so they do not describe the oasis.
    if "Water" in label:
        return
    if "NavBox" in label:
        origin, extent = actor.get_actor_bounds(False)
        for sx in (-1, 1):
            for sy in (-1, 1):
                oasis_points.append((origin.x + sx * extent.x, origin.y + sy * extent.y))
        return
    location = actor.get_actor_location()
    oasis_points.append((location.x, location.y))


def read_obstacle(scene, actor):
    if isinstance(actor, (unreal.DecalActor, unreal.Volume)):
        return
    origin, extent = actor.get_actor_bounds(False)
    if max(extent.x, extent.y) > MAX_OBSTACLE_HALF_EXTENT or max(extent.x, extent.y) <= 0.0:
        return
    box = (origin.x - extent.x, origin.x + extent.x, origin.y - extent.y, origin.y + extent.y)
    for cell in grid_cells(box[0] - OBSTACLE_CLEARANCE, box[1] + OBSTACLE_CLEARANCE, box[2] - OBSTACLE_CLEARANCE,
                           box[3] + OBSTACLE_CLEARANCE):
        scene.obstacles.setdefault(cell, []).append(box)
    scene.obstacle_count += 1


def read_fog_of_war(scene, actor):
    try:
        extent = float(actor.get_editor_property("map_extent"))
    except Exception:
        return
    centre = actor.get_actor_location()
    scene.playable = (centre.x - extent, centre.x + extent, centre.y - extent, centre.y + extent)
    scene.playable_source = "fog of war %s" % actor.get_actor_label()


def read_playable_fallback(scene, layout, actors):
    play = layout.get("playable")
    if play:
        scene.playable = (play["x0"], play["x1"], play["y0"], play["y1"])
        scene.playable_source = "layout json (no fog of war manager found)"
        return
    landscapes = [a for a in actors if isinstance(a, unreal.LandscapeProxy)]
    if not landscapes:
        raise RuntimeError("no playable area: no fog of war manager, layout or landscape")
    origin, extent = landscapes[0].get_actor_bounds(False)
    scene.playable = (origin.x - extent.x, origin.x + extent.x, origin.y - extent.y, origin.y + extent.y)
    scene.playable_source = "landscape bounds"


# ---------------------------------------------------------------------------------------------------------------------
# Planning
# ---------------------------------------------------------------------------------------------------------------------
def exclusion_reason(scene, x, y):
    """None when the spot is allowed, else why not. Cheap tests first."""
    x0, x1, y0, y1 = scene.playable
    if x < x0 + PLAYABLE_MARGIN or x > x1 - PLAYABLE_MARGIN or y < y0 + PLAYABLE_MARGIN or y > y1 - PLAYABLE_MARGIN:
        return "playable"
    for sx, sy, radius, _ in scene.starts:
        if math.hypot(x - sx, y - sy) < radius:
            return "start"
    for rx, ry, radius in scene.reserved:
        if math.hypot(x - rx, y - ry) < radius:
            return "reserved"
    for centre, half_x, half_y, yaw, _ in scene.edge_zones:
        if box_distance(x, y, centre, half_x, half_y, yaw) < EDGE_ZONE_CLEARANCE:
            return "edge_zone"
    for polygon in (scene.oasis_hull, scene.oasis_outline):
        if len(polygon) >= 3 and (inside_polygon(x, y, polygon) or polyline_distance(x, y, polygon, True) < OASIS_CLEARANCE):
            return "oasis"
    for points, (bx0, bx1, by0, by1) in scene.routes:
        if bx0 <= x <= bx1 and by0 <= y <= by1 and polyline_distance(x, y, points) < ROUTE_CLEARANCE:
            return "route"
    for px, py in scene.route_points:
        if math.hypot(x - px, y - py) < ROUTE_POINT_CLEARANCE:
            return "route"
    cell = (int(math.floor(x / GRID)), int(math.floor(y / GRID)))
    for bx0, bx1, by0, by1 in scene.obstacles.get(cell, ()):
        dx = max(bx0 - x, 0.0, x - bx1)
        dy = max(by0 - y, 0.0, y - by1)
        if math.hypot(dx, dy) < OBSTACLE_CLEARANCE:
            return "obstacle"
    return None


def nearest_start(scene, x, y):
    return min(scene.starts, key=lambda s: math.hypot(x - s[0], y - s[1]))[3]


def outskirt_excess(scene, x, y):
    """How far past the nearest start's exclusion ring the spot lies."""
    return min(math.hypot(x - sx, y - sy) - radius for sx, sy, radius, _ in scene.starts)


def spacing_ok(x, y, placed, existing):
    for px, py in existing:
        if math.hypot(x - px, y - py) < MIN_SPACING:
            return False
    for item in placed:
        if math.hypot(x - item["x"], y - item["y"]) < MIN_SPACING:
            return False
    return True


def weighted_pick(rng, options):
    total = sum(weight for _, weight in options)
    roll = rng.uniform(0.0, total)
    for name, weight in options:
        roll -= weight
        if roll <= 0.0:
            return name
    return options[-1][0]


def pick_kind(rng):
    if rng.random() < DESTRUCTIBLE_SHARE:
        return smd.CACTUS, rng.uniform(*CACTUS_SCALE)
    size = weighted_pick(rng, BUSH_SIZES)
    colour = weighted_pick(rng, BUSH_COLOURS)
    # Cover bushes keep their authored size: their cover footprint is gameplay.
    return smd.COVER % (size, colour, rng.randint(1, 3)), 1.0


def try_spot(scene, rng, x, y, placed, stats):
    reason = exclusion_reason(scene, x, y)
    if reason is not None:
        stats[reason] = stats.get(reason, 0) + 1
        return None
    if not spacing_ok(x, y, placed, scene.existing_cacti):
        stats["spacing"] = stats.get("spacing", 0) + 1
        return None
    z = ground_ok(x, y)
    if z is None:
        stats["ground"] = stats.get("ground", 0) + 1
        return None
    path, scale = pick_kind(rng)
    return {"x": x, "y": y, "z": z, "path": path, "scale": scale, "yaw": rng.uniform(0.0, 360.0)}


def plan(scene):
    rng = random.Random(SEED)
    target = rng.randint(TARGET_MIN, TARGET_MAX)
    x0, x1, y0, y1 = scene.playable
    placed = []
    stats = {}
    clusters = 0
    per_start = {}
    per_start_cap = int(math.ceil(target * PER_START_CAP_FACTOR / len(scene.starts)))
    for _ in range(MAX_ANCHOR_ATTEMPTS):
        if len(placed) >= target:
            break
        x = rng.uniform(x0 + PLAYABLE_MARGIN, x1 - PLAYABLE_MARGIN)
        y = rng.uniform(y0 + PLAYABLE_MARGIN, y1 - PLAYABLE_MARGIN)
        if exclusion_reason(scene, x, y) is not None:
            stats["anchor_zone"] = stats.get("anchor_zone", 0) + 1
            continue
        start_name = nearest_start(scene, x, y)
        if per_start.get(start_name, 0) >= per_start_cap:
            stats["start_cap"] = stats.get("start_cap", 0) + 1
            continue
        accept = max(OUTSKIRT_FLOOR, math.exp(-outskirt_excess(scene, x, y) / OUTSKIRT_FALLOFF))
        if rng.random() > accept:
            stats["outskirt_weight"] = stats.get("outskirt_weight", 0) + 1
            continue
        anchor = try_spot(scene, rng, x, y, placed, stats)
        if anchor is None:
            continue
        clusters += 1
        anchor["cluster"] = clusters
        placed.append(anchor)
        size = 1 if rng.random() < SINGLE_SHARE else rng.randint(CLUSTER_MIN, CLUSTER_MAX)
        size = min(size, target - len(placed) + 1, per_start_cap - per_start.get(start_name, 0))
        for _member in range(size - 1):
            add_cluster_member(scene, rng, anchor, placed, stats, clusters)
        per_start[start_name] = sum(1 for item in placed if nearest_start(scene, item["x"], item["y"]) == start_name)
    return target, placed, stats, clusters


def add_cluster_member(scene, rng, anchor, placed, stats, cluster):
    for _ in range(CLUSTER_MEMBER_ATTEMPTS):
        angle = rng.uniform(0.0, math.tau)
        distance = rng.uniform(MIN_SPACING, CLUSTER_RADIUS)
        member = try_spot(scene, rng, anchor["x"] + distance * math.cos(angle), anchor["y"] + distance * math.sin(angle),
                          placed, stats)
        if member is not None:
            member["cluster"] = cluster
            placed.append(member)
            return True
    return False


def log_scene(scene):
    log("scene: starts=%d (%s) reserved=%d edge_zones=%d routes=%d route_points=%d oasis_hull=%d oasis_outline=%d "
        "existing_cacti=%d obstacles=%d previous_extra=%d playable=%s via %s" % (
            len(scene.starts), ", ".join("%s r%.0f" % (s[3], s[2]) for s in scene.starts), len(scene.reserved),
            len(scene.edge_zones), len(scene.routes), len(scene.route_points), len(scene.oasis_hull),
            len(scene.oasis_outline), len(scene.existing_cacti), scene.obstacle_count, len(scene.previous_extra),
            tuple(round(v) for v in scene.playable), scene.playable_source))


def log_plan(scene, target, placed, stats, clusters):
    per_start = {}
    for index, item in enumerate(placed):
        nearest = min(scene.starts, key=lambda s: math.hypot(item["x"] - s[0], item["y"] - s[1]))
        log("PLAN %d cluster=%d %s at (%.0f, %.0f, %.0f) yaw=%.0f scale=%.2f nearest=%s +%.0f" % (
            index, item["cluster"], item["path"].rsplit("/", 1)[-1], item["x"], item["y"], item["z"], item["yaw"],
            item["scale"], nearest[3], math.hypot(item["x"] - nearest[0], item["y"] - nearest[1]) - nearest[2]))
        per_start[nearest[3]] = per_start.get(nearest[3], 0) + 1
    destructible = sum(1 for item in placed if item["path"] == smd.CACTUS)
    log("plan: target=%d placed=%d clusters=%d destructible=%d cover=%d per_start=%s rejections=%s" % (
        target, len(placed), clusters, destructible, len(placed) - destructible, per_start, stats))


# ---------------------------------------------------------------------------------------------------------------------
# Level changes
# ---------------------------------------------------------------------------------------------------------------------
def check_map():
    path = smb.world().get_path_name()
    if "/" + MAP_NAME + "." in path:
        return
    log("open level is %s; loading %s" % (path, MAP_PACKAGE))
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PACKAGE)
    path = smb.world().get_path_name()
    if "/" + MAP_NAME + "." not in path:
        raise RuntimeError("open level is %s, expected %s" % (path, MAP_NAME))


def backup_map():
    if not os.path.isfile(UMAP_PATH):
        raise RuntimeError("map file missing: %s" % UMAP_PATH)
    if not os.path.isdir(BACKUP_DIR):
        os.makedirs(BACKUP_DIR)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    backup = "%s/%s_%s_pre_cactiextra.umap.bak" % (BACKUP_DIR, MAP_NAME, stamp)
    shutil.copy2(UMAP_PATH, backup)
    if os.path.getsize(backup) != os.path.getsize(UMAP_PATH):
        raise RuntimeError("backup size mismatch %s" % backup)
    log("backup %s (%d bytes)" % (backup, os.path.getsize(backup)))
    return backup


def delete_previous(scene):
    IGNORED_ACTORS[:] = []
    for actor in scene.previous_extra:
        smb.actors_sub().destroy_actor(actor)
    log("deleted %d previous %s* actors" % (len(scene.previous_extra), LABEL_PREFIX))


def spawn(placed):
    before = set(a.get_actor_label() for a in smb.actors_sub().get_all_level_actors())
    count = 0
    for index, item in enumerate(placed):
        actor = smb.spawn_blueprint(item["path"], LABEL % index, FOLDER, item["x"], item["y"], yaw=item["yaw"],
                                    scale=item["scale"])
        count += 1 if actor is not None else 0
    after = smb.actors_sub().get_all_level_actors()
    companions = [a.get_actor_label() for a in after
                  if a.get_actor_label() not in before and not a.get_actor_label().startswith(LABEL_PREFIX)]
    if companions:
        log("WARNING the cactus Blueprints spawned %d companion actors (not cleared by a re-run): %s" % (
            len(companions), companions[:10]))
    log("spawned %d of %d" % (count, len(placed)))
    return count


def nav_bounds_ready():
    for actor in smb.actors_sub().get_all_level_actors():
        if isinstance(actor, unreal.RecastNavMesh):
            _, extent = actor.get_actor_bounds(False)
            if extent.x > 0.0 or extent.y > 0.0:
                return True
    return False


def save_map():
    ok = False
    try:
        ok = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    except Exception as error:
        log("save_current_level raised %s" % error)
    if ok:
        return True
    try:
        ok = bool(unreal.EditorLoadingAndSavingUtils.save_packages([smb.world().get_outermost()], False))
    except Exception as error:
        log("save_packages raised %s" % error)
    return ok


def restore(backup, reason):
    try:
        shutil.copy2(backup, UMAP_PATH)
        log("restored %s from %s" % (UMAP_PATH, backup))
    except Exception as error:
        log("RESTORE FAILED (%s): copy %s over %s by hand" % (error, backup, UMAP_PATH))
    log("FAILED %s" % reason)


# ---------------------------------------------------------------------------------------------------------------------
# Tick state machine
# ---------------------------------------------------------------------------------------------------------------------
class Runner(object):
    def __init__(self, backup, mtime_before):
        self.backup = backup
        self.mtime_before = mtime_before
        self.state = "warmup"
        self.ticks = 0
        self.state_ticks = 0
        self.idle_ticks = 0
        self.started = time.time()
        self.state_started = time.time()
        self.placed = 0
        self.save_attempts = 0
        self.handle = None
        self.finished = False

    def set_state(self, state):
        self.state = state
        self.state_ticks = 0
        self.state_started = time.time()

    def finish(self, message):
        if self.finished:
            return
        self.finished = True
        log(message)
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        unreal.SystemLibrary.quit_editor()

    def tick(self, _delta_seconds):
        if self.finished:
            return
        try:
            self.ticks += 1
            self.state_ticks += 1
            if time.time() - self.started > TIMEOUT_SECONDS:
                self.finish("FAILED timeout in state %s after %.0f s (map not saved)" % (self.state, time.time() - self.started))
                return
            getattr(self, "tick_" + self.state)()
        except Exception as error:
            log(traceback.format_exc())
            self.finish("FAILED %s: %s" % (self.state, error))

    def tick_warmup(self):
        if self.state_ticks < WARMUP_TICKS:
            return
        scene = gather_scene()
        log_scene(scene)
        if not scene.starts:
            self.finish("FAILED no SurvivalStartLocation actors in the level")
            return
        if not scene.edge_zones:
            self.finish("FAILED no SurvivalEdgeSpawnZone actors in the level (edge clearance cannot be kept)")
            return
        target, placed, stats, clusters = plan(scene)
        log_plan(scene, target, placed, stats, clusters)
        if is_dry_run():
            self.finish("DONE dry run placed=%d (nothing changed, not saved)" % len(placed))
            return
        if not placed:
            self.finish("FAILED nothing could be placed (map not saved)")
            return
        delete_previous(scene)
        self.placed = spawn(placed)
        if self.placed == 0:
            self.finish("FAILED no cactus spawned (map not saved)")
            return
        unreal.SystemLibrary.execute_console_command(smb.world(), "RebuildNavigation")
        log("RebuildNavigation issued")
        self.set_state("nav")

    def tick_nav(self):
        building = unreal.NavigationSystemV1.is_navigation_being_built(smb.world())
        self.idle_ticks = 0 if building else self.idle_ticks + 1
        if self.state_ticks < NAV_MIN_TICKS or self.idle_ticks < NAV_IDLE_TICKS:
            return
        if not nav_bounds_ready():
            if time.time() - self.state_started < NAV_BOUNDS_GRACE_SECONDS:
                return
            log("WARNING RecastNavMesh bounds still zero after %.0f s; saving anyway, the size check decides" %
                NAV_BOUNDS_GRACE_SECONDS)
        log("navigation idle after %d ticks (%.1f s)" % (self.state_ticks, time.time() - self.state_started))
        self.set_state("save")

    def tick_save(self):
        if self.save_attempts > 0 and self.state_ticks < SAVE_RETRY_TICKS:
            return
        self.save_attempts += 1
        self.state_ticks = 0
        ok = save_map()
        log("save attempt %d ok=%s" % (self.save_attempts, ok))
        if not ok:
            if self.save_attempts >= SAVE_ATTEMPTS:
                self.finish("FAILED save failed %d times (map on disk unchanged)" % self.save_attempts)
            return
        self.check_saved_file()

    def check_saved_file(self):
        size = os.path.getsize(UMAP_PATH)
        backup_size = os.path.getsize(self.backup)
        if os.path.getmtime(UMAP_PATH) == self.mtime_before:
            self.finish("FAILED save reported ok but %s was not rewritten" % UMAP_PATH)
            return
        minimum = max(MIN_UMAP_BYTES, int(backup_size * MIN_SIZE_RATIO))
        if size < minimum:
            restore(self.backup, "saved map is %d bytes, below %d (backup %d): navmesh likely missing" % (
                size, minimum, backup_size))
            self.finish("FAILED size check (backup restored)")
            return
        self.finish("DONE placed=%d size=%d (%.1f MB, backup %.1f MB, ratio %.3f) backup=%s" % (
            self.placed, size, size / 1048576.0, backup_size / 1048576.0, size / float(backup_size), self.backup))


RUNNER = None


def start():
    """Entry point: backup, then the tick state machine does the work (or the dry run) and quits the editor."""
    global RUNNER
    try:
        log("start dry_run=%s seed=%d" % (is_dry_run(), SEED))
        if IMPORT_ERROR is not None:
            raise RuntimeError("could not import survival_map_build / survival_map_dress: %s" % IMPORT_ERROR)
        check_map()
        backup = None
        mtime_before = None
        if not is_dry_run():
            backup = backup_map()
            mtime_before = os.path.getmtime(UMAP_PATH)
        RUNNER = Runner(backup, mtime_before)
        RUNNER.handle = unreal.register_slate_post_tick_callback(RUNNER.tick)
    except Exception as error:
        log(traceback.format_exc())
        log("FAILED %s" % error)
        unreal.SystemLibrary.quit_editor()


if __name__ != "survival_map_cacti":
    start()
