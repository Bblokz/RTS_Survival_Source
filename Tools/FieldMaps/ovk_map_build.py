"""In-editor build of Field_OreViadukt (convoy field map, Ore Mountains, owner pick B "Viadukt").

Design: MapDesigner/ConvoyMaps/Ore/convoy_ore_B.layout.json. Terrain: ovk_terrain_gen.py + ovk_cliffs.py (Saved/FieldMaps/
OreViadukt/terrain). Runs inside a throwaway full editor driven by field_editor_server.py / field_cmd.py:

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/FieldMaps")
    import ovk_map_build as ob; importlib.reload(ob)
    ob.step_base()                     # copy the template, strip it, landscape, level actors, lighting
    ob.step_dress()                    # roads, cliffs, crossings, water, trees, rocks, resources, props, wreck
    ob.step_convoy()                   # definition data asset, route, director, strength slots, mission
    ob.check_routes()                  # blocking nav build + drive paths of the route and the branch
    _defer = ob.W.rebuild_nav_and_save()

Reuses wismut_map_build (EnemyItemMaps) for the generic steps by retargeting its module globals. Every placed actor
has the OVK_ label prefix and sits under the outliner folder OreViadukt/; each step clears and redoes its own part.
"""
import importlib
import json
import math
import os
import random
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/EnemyItemMaps")
import wismut_map_build as W  # noqa: E402

importlib.reload(W)

SAVED = "D:/UE5Projects/RTS_Survival/Saved/FieldMaps/OreViadukt/"
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/Field_OreViadukt"
DATA = MAP_PATH + "/"                  # owner rule: the map's data assets in a folder named like the map
PREFIX, FOLDER = "OVK_", "OreViadukt/"
W.MAP_PATH = MAP_PATH
W.LAYOUT_FILE = SAVED + "terrain/OVK_layout.json"
W.SHOTS = SAVED + "shots/"
W.LANDSCAPE_LABEL = PREFIX + "Landscape"
W.PREFIX, W.FOLDER = PREFIX, FOLDER
W.Z_LOW, W.Z_HIGH = -2200.0, 2600.0     # the gorge floor is at -14 m, the river bed at -17 m
W.NAV_LOW, W.NAV_HIGH = -2000.0, 2200.0
ENEMY = 2
W.load_layout()                         # reload(W) above emptied its LAYOUT

CROSSINGS = json.load(open(SAVED + "crossings.json"))
PIECES_FILE = SAVED + "gorge_pieces.json"
CLIFF_FILE = SAVED + "terrain/OVK_cliffs.json"
REPORT = []

ENV = "/Game/RTS_Survival/Blueprints/Environment/"
ORE = ENV + "OreMountains/"
BX = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
FUEL = BX + "RusBuildings/FuelStorage/"
WATER_MI = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
SRC_DEFINITION = "/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_VehicleParts"
DEFINITION = DATA + "DA_Convoy_Field_OreViadukt"
ZIL_FUEL = ("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/"
            "Rus_ArmoredCar_ZilFuel.Rus_ArmoredCar_ZilFuel_C")
FIRST_CONVOY_DELAY = 60.0
TEST_TRUCKS = 6


def log(text):
    REPORT.append(str(text))
    W.log("OVK " + str(text))


def L():
    if not W.LAYOUT:
        W.load_layout()
    return W.LAYOUT


def feat(fid):
    L()
    return W.feature(fid)


def xy(fid):
    return feat(fid)["points"][0]


def gz(x, y):
    return W.ground_z(x, y)


def actors_sub():
    return W.actors_sub()


def spawn_bp(asset, label, x, y, yaw, folder, dz=0.0, z=None, owner=None, scale=None):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(asset)
    if cls is None:
        raise RuntimeError("missing " + asset)
    zz = (gz(x, y) if z is None else z) + dz
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, zz), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(PREFIX + label)
    actor.set_folder_path(FOLDER + folder)
    if scale is not None:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    if owner is not None:
        for comp in actor.get_components_by_class(unreal.RTSComponent):
            comp.set_editor_property("owning_player", owner)
    return actor


def spawn_mesh(asset, label, x, y, z, yaw, folder, collision=True, profile=None, scale=None, material=None,
               affects_nav=True, pitch=0.0, roll=0.0):
    mesh = unreal.load_asset(asset)
    if mesh is None:
        raise RuntimeError("missing " + asset)
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                                unreal.Rotator(roll, pitch, yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    if scale is not None:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    if material is not None:
        comp.set_material(0, unreal.load_asset(material))
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    else:
        if profile:
            comp.set_collision_profile_name(profile)
        comp.set_editor_property("can_ever_affect_navigation", affects_nav)
    actor.set_actor_label(PREFIX + label)
    actor.set_folder_path(FOLDER + folder)
    return actor


def local(cx, cy, yaw, lx, ly):
    a = math.radians(yaw)
    return cx + lx * math.cos(a) - ly * math.sin(a), cy + lx * math.sin(a) + ly * math.cos(a)


def crossing_ends(key):
    c = CROSSINGS[key]
    a = math.radians(c["yaw_deg"])
    half = c["length_cm"] / 2.0
    cx, cy = c["centre_cm"][0], c["centre_cm"][1]
    return (cx - math.cos(a) * half, cy - math.sin(a) * half), (cx + math.cos(a) * half, cy + math.sin(a) * half)


def on_crossing(x, y, key, pad=150.0):
    """True when (x, y) lies on a crossing's deck (between its ends, within half its clear width + pad)."""
    c = CROSSINGS[key]
    a = math.radians(c["yaw_deg"])
    dx, dy = x - c["centre_cm"][0], y - c["centre_cm"][1]
    along = dx * math.cos(a) + dy * math.sin(a)
    across = -dx * math.sin(a) + dy * math.cos(a)
    width = max(c["clear_width_cm"], 600.0)
    return abs(along) <= c["length_cm"] / 2.0 and abs(across) <= width / 2.0 + pad


def ground_or_deck(x, y):
    for key in ("viaduct", "trestle"):
        if on_crossing(x, y, key):
            return CROSSINGS[key]["deck_top_z_cm"]
    return gz(x, y)


def polyline_s(pts):
    s = [0.0]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        s.append(s[-1] + math.hypot(bx - ax, by - ay))
    return s


def project_s(pts, p):
    s = polyline_s(pts)
    best = (1e18, 0.0)
    for i, ((ax, ay), (bx, by)) in enumerate(zip(pts, pts[1:])):
        dx, dy = bx - ax, by - ay
        l2 = dx * dx + dy * dy or 1.0
        t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / l2))
        d = math.hypot(ax + t * dx - p[0], ay + t * dy - p[1])
        if d < best[0]:
            best = (d, s[i] + t * math.sqrt(l2))
    return best[1]


def point_at(pts, s_want):
    s = polyline_s(pts)
    s_want = max(0.0, min(s[-1], s_want))
    for i in range(len(pts) - 1):
        if s[i + 1] >= s_want:
            t = (s_want - s[i]) / max(1e-6, s[i + 1] - s[i])
            return (pts[i][0] + t * (pts[i + 1][0] - pts[i][0]), pts[i][1] + t * (pts[i + 1][1] - pts[i][1]))
    return tuple(pts[-1])


def sub_line(pts, s0, s1, step=200.0):
    n = max(2, int((s1 - s0) / step) + 1)
    return [point_at(pts, s0 + (s1 - s0) * k / (n - 1)) for k in range(n)]


def gorge_dist(x, y):
    return W.dist_polyline(x, y, L()["gorge"]["axis"])


# ======================================================================================================== base
def step_base():
    L()
    W.step_duplicate()
    W.step_strip()
    W.step_landscape()
    W.step_fit_level_actors()
    W.step_lighting()
    log("base done")


# ======================================================================================================== dressing
def step_roads():
    """Main road as the old macadam road spline in two parts (the viaduct deck is the road over the gorge); tracks as
    two-track dirt paths. Spline tiles have no collision and no nav effect."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Road_")
    worn = unreal.load_asset(W.PATH_ROOT + "Road/MI_GroundPath_Road_WarTorn")
    main = [tuple(p) for p in W.path("main_road")["points"]]
    via_sw, via_ne = crossing_ends("viaduct")
    s_sw, s_ne = project_s(main, via_sw), project_s(main, via_ne)
    s_end = polyline_s(main)[-1]
    west = sub_line(main, 0.0, s_sw - 100.0)
    east = sub_line(main, s_ne + 100.0, s_end)
    W._path_actor(PREFIX + "Road_Main_W", "Road/BP_GroundPathSpline_Road", W._resample(west, 1200.0), 1600.0, worn)
    W._path_actor(PREFIX + "Road_Main_E", "Road/BP_GroundPathSpline_Road", W._resample(east, 1200.0), 1600.0, worn)
    for pid, width in (("ne_track", 420.0), ("sw_track", 420.0), ("branch_track", 700.0)):
        W._path_actor(PREFIX + "Road_" + pid, "BP_GroundPathSpline_TwoTrack", W._resample(W.path(pid)["points"], 1500.0), width)
    # the trestle track starts at the trestle's SW end, not on the deck
    t_sw, _ = crossing_ends("trestle")
    tt = [tuple(p) for p in W.path("trestle_track")["points"]]
    W._path_actor(PREFIX + "Road_trestle_track", "BP_GroundPathSpline_TwoTrack", W._resample(tt, 1500.0), 420.0)
    # the ford: down the NE track, across, up the SW track
    ne = feat("ramp_ford_ne").get("centreline") or feat("ramp_ford_ne")["points"]
    sw = feat("ramp_ford_sw").get("centreline") or feat("ramp_ford_sw")["points"]
    ford = list(ne) + list(sw)
    W._path_actor(PREFIX + "Road_Ford", "BP_GroundPathSpline_TwoTrack", W._resample(ford, 1200.0), 500.0)
    log("roads: removed %d" % removed)


def _instance_actor(label, folder, mesh, transforms):
    actor = actors_sub().spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.add_instances(transforms, False, True)
    return actor


def step_cliffs():
    """The Ore cliff kit (kit C crag) on both gorge walls from OVK_cliffs.json: instanced, no collision, no nav effect
    (the landscape's 54-63 deg walls do the blocking)."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Cliff_")
    data = json.load(open(CLIFF_FILE))
    groups = {}
    for item in data["instances"]:
        groups.setdefault((item["face"], item["piece"]), []).append(item)
    total, missing = 0, set()
    for (face, piece), items in sorted(groups.items()):
        mesh = unreal.load_asset(data["mesh_path"].replace("<piece>", piece))
        if mesh is None:
            missing.add(piece)
            continue
        transforms = [unreal.Transform(unreal.Vector(i["x"], i["y"], i["z"]), unreal.Rotator(i["roll"], i["pitch"], i["yaw"]),
                                       unreal.Vector(i["scale"], i["scale"], i["scale"])) for i in items]
        _instance_actor("%sCliff_%s_%s" % (PREFIX, face, piece), FOLDER + "Cliffs/" + face, mesh, transforms)
        total += len(items)
    log("cliffs: removed %d, placed %d instances in %d components, missing %s" % (removed, total, len(groups), sorted(missing)))


def step_crossings():
    """Viaduct + trestle. With the Blender meshes (gorge_pieces.json) the Deck meshes get GroundNoBuilding (the ground
    channel tracked vehicles drive on) and the Structure meshes BlockAll; without them, placeholder slabs + parapets."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Cross_")
    pieces = json.load(open(PIECES_FILE))["pieces"] if os.path.exists(PIECES_FILE) else []
    real = [p for p in pieces if unreal.EditorAssetLibrary.does_asset_exist(p["asset"].split(".")[0])]
    placed = []
    if real and len(real) == len(pieces):
        for p in real:
            loc = p["world_cm"]
            name = p["asset"].split("/")[-1].split(".")[0].replace("SM_", "")
            profile = p.get("collision_profile", "BlockAll")
            spawn_mesh(p["asset"].split(".")[0], "Cross_" + name, loc[0], loc[1], loc[2] + 3.0, p["yaw_deg"], "Crossings",
                       profile=profile)
            placed.append(name + ":" + profile)
    else:
        cube = "/Engine/BasicShapes/Cube"
        concrete = ORE + "A4Viaduct/MI_A4_Concrete"
        for key in ("viaduct", "trestle"):
            c = CROSSINGS[key]
            cx, cy = c["centre_cm"][0], c["centre_cm"][1]
            yaw = c["yaw_deg"]
            length, width = c["length_cm"], c["clear_width_cm"]
            spawn_mesh(cube, "Cross_%s_DeckPH" % key, cx, cy, -40.0, yaw, "Crossings", profile="GroundNoBuilding",
                       scale=(length / 100.0, width / 100.0, 0.8), material=concrete)
            for side in (-1, 1):
                px, py = local(cx, cy, yaw, 0.0, side * (width / 2.0 + 40.0))
                spawn_mesh(cube, "Cross_%s_ParapetPH_%d" % (key, side), px, py, 50.0, yaw, "Crossings",
                           profile="BlockAll", scale=(length / 100.0, 0.6, 1.0), material=concrete)
            placed.append(key + " placeholder")
    log("crossings: removed %d, placed %s" % (removed, placed))
    return placed


def step_water():
    """The Floeha: water planes on the axis, 11 m wide, z -14.3 m (MI_Water_Shore); no collision, no nav effect."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Water_")
    g = L()["gorge"]
    axis = [tuple(p) for p in g["axis"]]
    half = L()["landscape"]["size_x"] / 2.0 - 500.0
    s_total = polyline_s(axis)[-1]
    step = 3000.0
    count = 0
    s = 0.0
    while s < s_total:
        a = point_at(axis, s)
        b = point_at(axis, min(s_total, s + step))
        s += step
        cx, cy = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0
        if abs(cx) > half or abs(cy) > half:
            continue
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        spawn_mesh("/Engine/BasicShapes/Plane", "Water_%02d" % count, cx, cy, g["water_z"], yaw, "Water", collision=False,
                   scale=((seg + 300.0) / 100.0, 11.0, 1.0), material=WATER_MI)
        count += 1
    log("water: removed %d, placed %d planes" % (removed, count))


TREE_ROOT = ORE + "DeadForest/BP_OreDeadSpruce_"
TREE_MIX = (("Skeleton_11m", 5), ("Skeleton_9m", 5), ("Skeleton_11m_B", 4), ("Skeleton_13m", 3), ("Silver_10m", 3),
            ("Leaning_11m", 2), ("Snapped_7m", 3), ("Black_8m", 1), ("Survivor_9m", 1))
WOODS = (("woods_rim", 0.55), ("woods_n", 0.55), ("woods_sw", 0.55), ("woods_nw", 0.5), ("woods_e", 0.5))


def _clear_lines():
    lay = L()
    lines = [W.path(p)["points"] for p in ("main_road", "ne_track", "sw_track", "trestle_track", "branch_track")]
    ramps = [f.get("centreline") or f["points"] for f in lay["features"] if f["kind"] == "ramp"]
    return lines, ramps


MAIN_ROAD_CLEAR = 1400.0               # 16 m road + flank escorts 9 m off the centre line


def step_trees():
    """Crushable BP_OreDeadSpruce_* in the five woods plus a thin scatter of lone dead spruce on open ground."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Tree_")
    rng = random.Random(1945)
    lay = L()
    b = lay["bounds"]
    lines, ramps = _clear_lines()
    keep_out = []                                       # (x, y, r): HQ build area, yards, metal, markers
    keep_out.append((*xy("hq"), 6500.0))
    for fid in ("convoy_stop", "branch_dest", "radio_post", "director", "b1_abutment", "b3_heap"):
        keep_out.append((*xy(fid), 1500.0))
    for rid in ("depot_yard", "shaft_yard", "shelf_start"):
        r = W.region(rid)
        keep_out.append((r["centre"][0], r["centre"][1], 3200.0))

    def ok(px, py, road_clear):
        if not (b["xmin"] + 300 < px < b["xmax"] - 300 and b["ymin"] + 300 < py < b["ymax"] - 300):
            return False
        if gorge_dist(px, py) < lay["gorge"]["rim_cm"] + 600.0:
            return False
        if any(math.hypot(px - x, py - y) < rr for x, y, rr in keep_out):
            return False
        if W.dist_polyline(px, py, lines[0]) < max(road_clear, MAIN_ROAD_CLEAR):
            return False
        if any(W.dist_polyline(px, py, line) < road_clear for line in lines[1:]):
            return False
        if any(W.dist_polyline(px, py, r_) < 1100.0 for r_ in ramps if len(r_) > 1):
            return False
        return True

    spots = []
    spacing = 520.0
    for rid, keep in WOODS:
        poly = W.region(rid)["points"]
        xs, ys = [p[0] for p in poly], [p[1] for p in poly]
        x = min(xs)
        while x < max(xs):
            y = min(ys)
            while y < max(ys):
                px = x + rng.uniform(-0.35, 0.35) * spacing
                py = y + rng.uniform(-0.35, 0.35) * spacing
                y += spacing
                if rng.random() > keep or not W.point_in_poly(px, py, poly):
                    continue
                if ok(px, py, 750.0):
                    spots.append((px, py, rng.uniform(0, 360)))
            x += spacing
    lone = 0
    x = b["xmin"]
    while x < b["xmax"]:
        y = b["ymin"]
        while y < b["ymax"]:
            px, py = x + rng.uniform(0, 1600), y + rng.uniform(0, 1600)
            y += 1600.0
            if rng.random() < 0.045 and ok(px, py, 1300.0):
                spots.append((px, py, rng.uniform(0, 360)))
                lone += 1
        x += 1600.0
    bag = [name for name, weight in TREE_MIX for _ in range(weight)]
    classes, counts = {}, {}
    for index, (px, py, yaw) in enumerate(spots):
        kind = rng.choice(bag)
        if kind not in classes:
            classes[kind] = unreal.EditorAssetLibrary.load_blueprint_class(TREE_ROOT + kind)
        actor = actors_sub().spawn_actor_from_class(classes[kind], unreal.Vector(px, py, gz(px, py) - 8.0),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label("%sTree_%s_%04d" % (PREFIX, kind, index))
        actor.set_folder_path(FOLDER + "Trees")
        counts[kind] = counts.get(kind, 0) + 1
    log("trees: removed %d, placed %d (%d lone) %s" % (removed, len(spots), lone, counts))


ROCKS = ORE + "CrushableRocks/Blueprints/BP_OreRock_"
ROCK_KINDS_M = ("M01_Block", "M02_Wedge", "M03_Slab", "M04_Split", "M05_Stack", "M06_Chunk", "M07_Tilted", "M08_Long")
ROCK_KINDS_S = ("S01_Block", "S02_Wedge", "S03_Slab", "S04_Chunk", "S05_Split", "S06_Shard", "S07_Low", "S08_Pair")


def step_rocks():
    """Crushable Ore rocks: clusters on the spoil heap, the radio knoll, by the exit crag, on the gorge floor talus and
    a thin scatter on open ground."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Rock_")
    rng = random.Random(77)
    lay = L()
    g = lay["gorge"]
    lines, ramps = _clear_lines()
    spots = []

    def cluster(cx, cy, n, r):
        for _ in range(n):
            a, d = rng.uniform(0, 2 * math.pi), r * math.sqrt(rng.random())
            spots.append((cx + math.cos(a) * d, cy + math.sin(a) * d))

    cluster(*xy("b3_heap"), 9, 2200.0)
    cluster(*xy("radio_post"), 6, 1800.0)
    cluster(*W.region("crag_exit")["centre"], 6, 2000.0)
    cluster(*W.region("heap_n")["centre"], 5, 1500.0)
    # gorge floor: talus along both wall feet, inside the play area
    axis = [tuple(p) for p in g["axis"]]
    total = polyline_s(axis)[-1]
    s = 0.0
    while s < total:
        p0, p1 = point_at(axis, s), point_at(axis, s + 100.0)
        tx, ty = p1[0] - p0[0], p1[1] - p0[1]
        n = math.hypot(tx, ty) or 1.0
        nx, ny = ty / n, -tx / n
        side = rng.choice((-1, 1))
        d = g["floor_edge_cm"] - rng.uniform(150.0, 900.0)
        spots.append((p0[0] + side * nx * d, p0[1] + side * ny * d))
        s += rng.uniform(900.0, 1800.0)
    b = lay["bounds"]
    for _ in range(70):
        spots.append((rng.uniform(b["xmin"], b["xmax"]), rng.uniform(b["ymin"], b["ymax"])))
    placed = 0
    classes = {}
    for index, (px, py) in enumerate(spots):
        if not (b["xmin"] + 300 < px < b["xmax"] - 300 and b["ymin"] + 300 < py < b["ymax"] - 300):
            continue
        gd = gorge_dist(px, py)
        if g["floor_edge_cm"] - 100.0 < gd < g["rim_cm"] + 400.0 or gd < g["river_half_cm"] + 300.0:
            continue                                        # not on the walls, not in the river
        if W.dist_polyline(px, py, lines[0]) < MAIN_ROAD_CLEAR + 200.0:
            continue
        if any(W.dist_polyline(px, py, line) < 800.0 for line in lines[1:]):
            continue
        if any(W.dist_polyline(px, py, r_) < 900.0 for r_ in ramps if len(r_) > 1):
            continue
        if math.hypot(px - xy("hq")[0], py - xy("hq")[1]) < 6000.0:
            continue
        kind = rng.choice(ROCK_KINDS_M if rng.random() < 0.45 else ROCK_KINDS_S)
        if kind not in classes:
            classes[kind] = unreal.EditorAssetLibrary.load_blueprint_class(ROCKS + kind)
        actor = actors_sub().spawn_actor_from_class(classes[kind], unreal.Vector(px, py, gz(px, py) - 5.0),
                                                    unreal.Rotator(0.0, 0.0, rng.uniform(0, 360)))
        actor.set_actor_label("%sRock_%s_%03d" % (PREFIX, kind, index))
        actor.set_folder_path(FOLDER + "Rocks")
        placed += 1
    log("rocks: removed %d, placed %d" % (removed, placed))


METAL_TYPES = {"metal_ne": ("Glaskopf", "Katzengold"), "metal_gorge": ("Blutbarren", "Gelbguss")}


def step_resources():
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Res_")
    rng = random.Random(7)
    placed = []
    for fid, kinds in METAL_TYPES.items():
        for index, (p, kind) in enumerate(zip(feat(fid)["points"], kinds)):
            label = "Res_%s_%s_%d" % (fid, kind, index)
            spawn_bp(W.RESOURCE_ROOT + "Metal/OreMountains/Resource_Metal_Ore_" + kind, label, p[0], p[1],
                     rng.uniform(0, 360), "Resources")
            placed.append(label)
    spawn_bp(ORE + "BuriedArmour/BP_BuriedArmour_FlakLuchs", "Wreck_BuriedLuchs", *xy("buried_luchs"), 35.0, "Resources")
    log("resources: removed %d, placed %s + buried Luchs" % (removed, placed))


def step_props():
    """Fuel depot (the stop), shaft 7 (the branch destination), the radio post hut on the knoll, the HQ attack marker."""
    W.require_map()
    removed = sum(W.clear_prefix(PREFIX + p) for p in ("Depot_", "Shaft_", "Radio_", "Marker_"))
    # fuel depot: south of the road at the stop, yard yaw 10
    d = W.region("depot_yard")
    cx, cy, yaw = d["centre"][0], d["centre"][1], 10.0
    depot = [("Depot_FuelLarge", FUEL + "BP_Dest_RusFuelLarge", -400.0, -900.0, 100.0),
             ("Depot_Fuel1_A", FUEL + "BP_Dest_RusFuel1", -900.0, 700.0, 10.0),
             ("Depot_Fuel1_B", FUEL + "BP_Dest_RusFuel1", -900.0, 1300.0, 10.0),
             ("Depot_Fuel2_A", FUEL + "BP_Dest_RusFuel2", 300.0, 1100.0, 100.0),
             ("Depot_Crates", ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack01", 600.0, -300.0, 30.0)]
    for label, asset, lx, ly, ryaw in depot:
        x, y = local(cx, cy, yaw, lx, ly)
        spawn_bp(asset, label, x, y, yaw + ryaw, "FuelDepot", dz=-3.0)
    stop = xy("convoy_stop")
    bx, by = local(stop[0], stop[1], 0.0, -1500.0, 0.0)
    spawn_bp(ORE + "Sites/Weighbridge/BP_OreWB_BoomBarrier", "Depot_Barrier", bx, by, 100.0, "FuelDepot")
    for k, (lx, ly) in enumerate(((800.0, -1300.0), (1100.0, -1000.0), (900.0, -700.0), (-1300.0, -200.0))):
        x, y = local(cx, cy, yaw, lx, ly)
        spawn_mesh(BX + "GerBxps/Bunkers/BreakthroughShared/FieldSupply/SM_BF_Barrel_3_Oil", "Depot_Barrels_%d" % k,
                   x, y, gz(x, y), random.Random(k).uniform(0, 360), "FuelDepot")
    # shaft 7: headframe, shed, fuel for the winding engine, destination marker in front of the headframe
    s = W.region("shaft_yard")
    sx, sy, syaw = s["centre"][0], s["centre"][1], -30.0
    hx, hy = local(sx, sy, syaw, 800.0, 600.0)
    spawn_mesh(ORE + "Sites/BaseB/Meshes/SM_OreHF_Headframe", "Shaft_Headframe", hx, hy, gz(hx, hy), syaw + 180.0,
               "Shaft7")
    nx_, ny_ = local(sx, sy, syaw, -900.0, 1300.0)
    spawn_bp(ORE + "Sites/BaseA/BP_OreBaseA_NissenShed", "Shaft_Shed", nx_, ny_, syaw + 90.0, "Shaft7")
    tx, ty = local(sx, sy, syaw, 900.0, -1100.0)
    spawn_bp(FUEL + "BP_Dest_RusFuel2", "Shaft_FuelTank", tx, ty, syaw, "Shaft7")
    cx2, cy2 = local(sx, sy, syaw, -700.0, -900.0)
    spawn_bp(ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack01", "Shaft_Crates", cx2, cy2, syaw + 40.0,
             "Shaft7")
    dest = xy("branch_dest")
    marker = actors_sub().spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(dest[0], dest[1], gz(*dest) + 50.0))
    marker.set_actor_label(PREFIX + "Marker_BranchDest")
    marker.set_folder_path(FOLDER + "Convoy")
    # radio post: the Ore radio outpost hut (Rus container hut child, owner 2) + an antenna, on the knoll top
    rx, ry = xy("radio_post")
    spawn_bp(ORE + "Sites/RadioOutpost/BP_OreRadio_Funkbude", "Radio_Funkbude", rx, ry, 230.0, "RadioPost", owner=ENEMY)
    spawn_mesh("/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna2", "Radio_Antenna", rx + 350.0, ry - 250.0,
               gz(rx + 350.0, ry - 250.0), 0.0, "RadioPost", collision=False, scale=(1.3, 1.3, 1.3))
    hq = xy("hq")
    hm = actors_sub().spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(hq[0], hq[1], gz(*hq) + 100.0))
    hm.set_actor_label(PREFIX + "Marker_HQ")
    hm.set_folder_path(FOLDER + "Convoy")
    log("props: removed %d; depot, shaft 7, radio post, markers placed" % removed)


def step_dress():
    for step in (step_roads, step_cliffs, step_crossings, step_water, step_trees, step_rocks, step_resources, step_props):
        step()


# ======================================================================================================== convoy
def _struct_copy(value):
    out = type(value)()
    out.import_text(value.export_text())
    return out


def step_definition():
    """DA_Convoy_Field_OreViadukt in the map's data folder: DA_Convoy_VehicleParts (ZilAmmo trucks) with a Zil fuel truck
    (cargo Fuel) at column position 2 and 6, so 3-8 truck convoys always carry 1-2 fuel trucks for the shaft 7 branch."""
    if not unreal.EditorAssetLibrary.does_asset_exist(DEFINITION):
        unreal.EditorAssetLibrary.make_directory(DATA)
        if unreal.EditorAssetLibrary.duplicate_asset(SRC_DEFINITION, DEFINITION) is None:
            raise RuntimeError("duplicate_asset failed for the definition")
    da = unreal.load_asset(DEFINITION)
    trucks = list(da.get_editor_property("trucks"))
    if not trucks:
        raise RuntimeError("source definition has no trucks")
    ammo_n = _struct_copy(trucks[0])
    ammo_n.set_editor_property("count", 1)
    fuel = unreal.ConvoyTruckSpec()
    fuel.import_text('(TruckClass="%s",TrainingOption=(UnitType=UNType_Tank,SubtypeValue=%d),Count=1,'
                     'CalibratedCruiseSpeedCms=1006.000000,bOverrideCargoType=True,CargoTypeOverride=Fuel)'
                     % (ZIL_FUEL, int(unreal.TankSubtype.TANK_ZIL_FUEL.value)))
    column = [ammo_n, fuel, _struct_copy(ammo_n), _struct_copy(ammo_n)]
    da.set_editor_property("trucks", column)
    unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    log("definition %s: trucks %s" % (DEFINITION, [str(t.get_editor_property("training_option")) for t in column]))
    return da


def _vec(p, lift=50.0):
    return unreal.Vector(p[0], p[1], ground_or_deck(p[0], p[1]) + lift)


def _rel(origin, p, lift=50.0):
    v = _vec(p, lift)
    return unreal.Vector(v.x - origin.x, v.y - origin.y, v.z - origin.z)


def route_points():
    main = [tuple(p) for p in W.path("main_road")["points"]]
    s0, s1 = project_s(main, xy("convoy_entry")), project_s(main, xy("convoy_exit"))
    pts = sub_line(main, s0, s1, 2500.0)
    # make sure both viaduct ends are control points (the drive path crosses the deck, never the gorge)
    a, b = crossing_ends("viaduct")
    sa, sb = project_s(main, a), project_s(main, b)
    out = [p for p in pts if not (min(sa, sb) - 200.0 < project_s(main, p) < max(sa, sb) + 200.0)]
    out += [point_at(main, sa - 300.0), point_at(main, (sa + sb) / 2.0), point_at(main, sb + 300.0)]
    out.sort(key=lambda p: project_s(main, p))
    return out


def step_route():
    """The AEnemyConvoyRoute: entry 15 m inside the W edge, exit 15 m inside the E edge; stop at the fuel depot, fuel
    branch to shaft 7, three ambush zones with the radio hut, hunters past the viaduct."""
    W.require_map()
    da = unreal.load_asset(DEFINITION)
    removed = W.clear_prefix(PREFIX + "Convoy_Route")
    pts = route_points()
    origin = _vec(pts[0])
    route = actors_sub().spawn_actor_from_class(unreal.EnemyConvoyRoute, origin, unreal.Rotator(0.0, 0.0, 0.0))
    route.set_actor_label(PREFIX + "Convoy_Route")
    route.set_folder_path(FOLDER + "Convoy")
    route.set_editor_property("route_id", "OVK_Convoy")
    route.set_editor_property("definition", da)
    route.set_editor_property("route_points", [_rel(origin, p) for p in pts])
    route.set_editor_property("enemy_player", ENEMY)
    timing = route.get_editor_property("timing")
    timing.set_editor_property("start_on_begin_play", False)
    timing.set_editor_property("first_convoy_delay", FIRST_CONVOY_DELAY)
    timing.set_editor_property("repeat", unreal.EnemyWaveSpawnerRepeat.ONCE)
    route.set_editor_property("timing", timing)
    own = route.get_editor_property("ownership")
    own.set_editor_property("has_owning_building", False)
    route.set_editor_property("ownership", own)
    labels = W.by_label()
    stop = route.get_editor_property("stop")
    stop.set_editor_property("enabled", True)
    stop.set_editor_property("stop_seconds", float(feat("convoy_stop")["raw"].get("stop_seconds", 60)))
    route.set_editor_property("stop", stop)
    route.set_editor_property("stop_point", _rel(origin, xy("convoy_stop")))
    branch = route.get_editor_property("branch")
    branch.set_editor_property("enabled", True)
    branch.set_editor_property("cargo_type", unreal.ConvoyCargoType.FUEL)
    branch.set_editor_property("destination_actor", labels[PREFIX + "Marker_BranchDest"])
    route.set_editor_property("branch", branch)
    bpts = [tuple(p) for p in W.path("branch_track")["points"]]
    branch_pts = [bpts[0]] + W._resample(bpts, 3000.0)[1:-1] + [tuple(xy("branch_dest"))]
    route.set_editor_property("branch_points", [_rel(origin, p) for p in branch_pts])
    ambush = route.get_editor_property("ambush")
    ambush.set_editor_property("radio_post_actor", labels[PREFIX + "Radio_Funkbude"])
    route.set_editor_property("ambush", ambush)
    route.set_editor_property("ambush_zone_points", [_rel(origin, xy(z)) for z in ("az_viaduct", "az_rim", "az_bend")])
    hunters = route.get_editor_property("hunters")
    hunters.set_editor_property("enabled", True)
    hunters.set_editor_property("squads_per_truck", [unreal.EnemyPlacementUnit(kind=unreal.EnemyPlacementUnitKind.SQUAD,
                                                                                squad_subtype=unreal.SquadSubtype.SQUAD_RUS_MOSIN)])
    hunters.set_editor_property("attack_target", labels[PREFIX + "Marker_HQ"])
    route.set_editor_property("hunters", hunters)
    route.set_editor_property("dismount_point", _rel(origin, xy("dismount")))
    hp = [tuple(p) for p in W.path("hunter_path")["points"]][1:]
    route.set_editor_property("hunter_waypoints", [_rel(origin, p) for p in hp])
    log("route: removed %d, %d route points, %d branch points" % (removed, len(pts), len(branch_pts)))
    return route


STRENGTH = {
    # feature id: (slot id, defence Blueprint, fortification, min total strength %)
    "str_bunker": ("Bunker_ViaductSW", BX + "RusBunkers/Rus_BXP_MGBunker", "BUNKERS", 0),
    "str_aa": ("AA_Depot", BX + "RusBunkers/Rus_BXP_Bofors", "AA_BATTERY", 0),
    "str_mines": ("Mines_AbutmentNE", "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Mines/Rus_AnitTank_LightMineField",
                  "MINEFIELDS", 0),
    "str_garrison": ("Garrison_Shaft7", BX + "RusBunkers/Rus_BXP_DomeBunkerGarrison", "NONE", 50),
    "str_mg_bend": ("MG_TrestleBend", BX + "RusDShKPit/Rus_BXP_DShKPit", "NONE", 25),
}


def _yaw_to(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def step_director():
    """AEnemyItemSiteDirector at the enemy start (convoy route battle on SovietSupplyConvoy; TestContext lets the
    owner press Play on the map directly), the five strength slots, UDestroyEnemyItemSiteMission."""
    W.require_map()
    removed = sum(W.clear_prefix(PREFIX + p) for p in ("Site_", "Strength_"))
    labels = W.by_label()
    route = labels[PREFIX + "Convoy_Route"]
    dx, dy = xy("director")
    director = actors_sub().spawn_actor_from_class(
        unreal.EnemyItemSiteDirector, unreal.Vector(dx, dy, gz(dx, dy) + 50.0),
        unreal.Rotator(0.0, 0.0, _yaw_to((dx, dy), xy("convoy_stop"))))
    director.set_actor_label(PREFIX + "Site_Director")
    director.set_folder_path(FOLDER + "Convoy")
    director.set_editor_property("base_radius", 3000.0)
    army = director.get_editor_property("army_settings")
    army.set_editor_property("convoy_route", route)
    director.set_editor_property("army_settings", army)
    ctx = director.get_editor_property("test_context")
    ctx.set_editor_property("is_division_operation", True)
    enemy_army = ctx.get_editor_property("army")
    enemy_army.set_editor_property("division_type", unreal.WorldFieldDivisions.SOVIET_SUPPLY_CONVOY)
    enemy_army.set_editor_property("convoy_truck_count", TEST_TRUCKS)
    ctx.set_editor_property("army", enemy_army)
    director.set_editor_property("test_context", ctx)
    placed = []
    for fid, (slot_id, asset, fort, minimum) in STRENGTH.items():
        x, y = xy(fid)
        slot = actors_sub().spawn_actor_from_class(unreal.EnemyItemStrengthSlot, unreal.Vector(x, y, gz(x, y) + 10.0),
                                                   unreal.Rotator(0.0, 0.0, _yaw_to((x, y), xy("hq"))))
        slot.set_actor_label(PREFIX + "Strength_" + slot_id)
        slot.set_folder_path(FOLDER + "StrengthSlots")
        slot.set_editor_property("slot_id", slot_id)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(asset)
        if cls is None:
            raise RuntimeError("missing " + asset)
        slot.set_editor_property("defence_class", cls)
        cond = slot.get_editor_property("condition")
        cond.set_editor_property("required_fortification", W._enum(unreal.WorldFortificationStrength, fort))
        cond.set_editor_property("minimum_total_strength_percentage", minimum)
        slot.set_editor_property("condition", cond)
        placed.append(slot_id)
    W.step_mission()
    log("director: removed %d, slots %s" % (removed, placed))
    return director


def step_convoy():
    step_definition()
    step_route()
    step_director()


# ======================================================================================================== checks
def check_routes():
    """Blocking nav build, then UConvoyRouteLibrary drive paths for the main route and the fuel branch."""
    W.require_map()
    world = W.world()
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    route = W.by_label()[PREFIX + "Convoy_Route"]
    origin = route.get_actor_location()

    def world_pts(prop):
        return [unreal.Vector(origin.x + p.x, origin.y + p.y, origin.z + p.z) for p in route.get_editor_property(prop)]

    problems = []
    main_pts = world_pts("route_points")
    branch = world_pts("branch_points")
    for name, pts in (("main", main_pts), ("branch", branch)):
        built, reached = unreal.ConvoyRouteLibrary.build_navigable_route(route, pts, 800.0, "OVK_" + name)
        count = len(built.get_editor_property("points"))
        length = unreal.ConvoyRouteLibrary.get_route_length(built) if count > 1 else 0.0
        straight = sum(math.hypot(b.x - a.x, b.y - a.y) for a, b in zip(pts, pts[1:]))
        on_deck = sum(1 for p in built.get_editor_property("points") if on_crossing(p.x, p.y, "viaduct"))
        log("drive path %s: reached=%s points=%d length=%.0f m (control %.0f m), points on the viaduct deck %d"
            % (name, reached, count, length / 100.0, straight / 100.0, on_deck))
        if not reached or count < 2:
            problems.append(name + " does not reach its end")
        elif length > straight * 1.2 + 3000.0:
            problems.append("%s detours: %.0f m for %.0f m" % (name, length / 100.0, straight / 100.0))
        if name == "main" and on_deck == 0:
            problems.append("main drive path never touches the viaduct deck")
    try:
        route.call_method("PreviewDrivePath")
    except Exception as error:
        log("PreviewDrivePath not called: %s" % error)
    log("route problems: %s" % (problems or "none"))
    return problems
