"""Builds the Ger_Berlin rail yard version D1 "Sperrbahnhof - Kompakt" (owner's pick 2026-11-22; BerlinRailYard.md,
concept Docs/Concepts/BerlinRailYard/v02). Runs inside the throwaway editor (ry_editor_server.py + ry_cmd.py).

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import rail_yard_d1 as ry; importlib.reload(ry)
    ry.build_target_bps()        # the two scavengeable sabotage targets (Blueprints, saved)
    ry.place()                   # map edit (labels RY_, folder Berlin/RailYard); logs every removal to Saved/BerlinRailYard
    ry.nav_report()              # after the navmesh rebuilt: path checks for the four agents
    ry.save_when_nav_ready()     # sets _defer: saves the level once the navmesh is built

Not the generators: berlin_map_build.step_water_and_rails / step_custom_poi / berlin_dress_gen.set_pieces would put the
old track, ramp, tower and yard dressing back (the map has been hand-edited since; see BerlinRailYard.md "Built").

Layout (Unreal cm, +X north, +Y east), fitted to the real module sizes (probe 2026-11-22, rail_yard_probe.py):
  - court 45.2 x 45.2 m = 2 x 2 Rus_Wall_Concrete_Long (22.6 m, pivot at the module's +X end, 5 m high), X -4862..-342,
    Y 12100..16620; east wall: BP_RusWallGateA (opening 6.9 m) X -342..-1292, BP_RusWallGateC (opening ~10.3 m) on the
    track X -1292..-2392, one long module, and a 2.1 m broken gap at the south-east corner (infantry only: a light
    tank needs 2.4 m);
  - wooden track: BP_RoadSpline + SM_RustedRefinery_RailRoad along rail_east into the court, sunk 85 cm (sleeper tops
    ~10 cm, rail tops ~28 cm above the ground), replacing the BERW_Rails panels of that line (TRACK_EXTENT);
  - targets: BP_Berlin_RailRamp_Sabotage where the ramp stands today, BP_Berlin_SwitchTower_Sabotage at the east
    throat (-2900, 23300);
  - 2 Rus_BXP_GuardTowerMG_Soviet (north-west and south-east inside corners), AEnemyStrongpoint "RailYard": dug-in
    KV-1 on the west wall, dug-in T-34/76 by the gates, two Mosin squads (court, outside gate A).
"""
import datetime
import time
import json
import math
import os

import unreal

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ELL = unreal.EditorLevelLibrary
NS = unreal.NavigationSystemV1
OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard"
FOLDER = "Berlin/RailYard"
PREFIX = "RY_"

W = "/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/"
WALL_LONG = W + "Rus_Wall_Concrete_Long"
GATE_A = W + "Gates/BP_RusWallGateA"
GATE_C = W + "Gates/BP_RusWallGateC"
TOWER_MG = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG_Soviet"
ROAD_SPLINE = "/Game/RTS_Survival/Blueprints/Environment/Splines/BP_RoadSpline"
RAIL_MESH = "/Game/RTS_Survival/Meshes/Buildings/Industrial/RailRoad/SM_RustedRefinery_RailRoad"
RAIL_MATERIAL = None   # Ger_Trains main lines (TrainTrack_0..2) keep the mesh's own wood; its base siding uses
                       # .../RailRoad/Textures/M_YellowRail (slot 0 only), the other option
DRAINPIPES = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/BP_Airport_Dest_DrainPipeStack"
SCAV_BASE = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/BP_ScavengeableObject"
TARGET_HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget"
TARGETS = {  # name: (mesh, scavenge seconds, health)
    "BP_Berlin_RailRamp_Sabotage": (TARGET_HOME + "/SM_Berlin_RailRamp", 40.0, 2400.0),
    "BP_Berlin_SwitchTower_Sabotage": (TARGET_HOME + "/SM_Berlin_SwitchTower", 30.0, 1800.0),
}
SOVIET = 2

# ---------------------------------------------------------------- layout
X_N, X_S, Y_W, Y_E = -342.0, -4862.0, 12100.0, 16620.0
RAMP_AT = (-2102.0, 13389.0, 180.0)          # today's ramp transform (kept)
SWITCH_TOWER_AT = (-2900.0, 23300.0, 180.0)
TRACK_X = -1842.0
BUFFER_Y = 12650.0
SINK = 85.0
# rail_east (Ger_Berlin layout) + the yard leg to the buffer stop
RAIL_EAST = [(-10833.0, 50000.0), (-11917.0, 46444.0), (-6500.0, 38889.0), (-2492.0, 31000.0), (-1842.0, 23333.0),
             (-1842.0, 18056.0), (-1842.0, BUFFER_Y)]
TRACK_EXTENT = "rail_east"   # "rail_east": the whole Ostbahnhof line + yard (default); "yard": from Y 24500 only
LONG_HALF = 1130.0           # SM_Wall_14 spans local X -2260..0: centre at -1130
GATE_HALF = {"A": 460.0, "C": 535.0}       # frame local X -935..15 / -1085..15: centre at -460 / -535


def walls():
    """(kind, centre x, centre y, yaw): yaw 0 = module along +X (north), 90 = along +Y (east)."""
    mid_x = (X_N + X_S) / 2.0
    out = [("long", X_N - LONG_HALF, Y_W, 0.0), ("long", X_N - 3 * LONG_HALF, Y_W, 0.0),       # west wall
           ("long", X_N, Y_W + LONG_HALF, 90.0), ("long", X_N, Y_W + 3 * LONG_HALF, 90.0),     # north wall
           ("long", X_S, Y_W + LONG_HALF, 90.0), ("long", X_S, Y_W + 3 * LONG_HALF, 90.0),     # south wall
           ("gateA", X_N - 475.0, Y_E, 0.0), ("gateC", TRACK_X, Y_E, 0.0),                       # east wall
           ("long", TRACK_X - 550.0 - LONG_HALF, Y_E, 0.0)]
    return out, mid_x


TOWERS = [("RY_Tower_NW", -800.0, 12750.0, 0.0), ("RY_Tower_SE", -4300.0, 16000.0, 0.0)]
STRONGPOINT_AT = (-2602.0, 14360.0)
POSTS = [  # id, world x, y, yaw, kind, subtype attr, dig in
    ("WestKV", -3500.0, 13300.0, 270.0, "vehicle", "TANK_KV_1", True),
    ("GateT34", -3300.0, 15700.0, 90.0, "vehicle", "TANK_T34_76", True),
    # squads stay > OpenRadius (14 m) + margin from both gates, or AWallGate holds the gates open for them
    ("CourtSquad", -3200.0, 14000.0, 90.0, "squad", "SQUAD_RUS_MOSIN", False),
    ("GateSquad", -300.0, 19500.0, 90.0, "squad", "SQUAD_RUS_MOSIN", False),
]
REMOVE_LABELS = ["POI_RailYard_UnloadingRamp", "POI_RailYard_SwitchTower", "BER_YardCrane", "BER_YardShed",
                 "BER_YardFence_00"]
REMOVE_PREFIXES = ["BER_YardContainer_", "BER_YardBarrels_"]


# ---------------------------------------------------------------- helpers
def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y):
    """Landscape height (cm), ignoring every non-landscape hit."""
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return 0.0


def by_label():
    return {a.get_actor_label(): a for a in EAS.get_all_level_actors()}


def spawn(asset, label, x, y, z, yaw):
    loc, rot = unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw)
    if isinstance(asset, unreal.Blueprint) or isinstance(asset, unreal.StaticMesh):
        actor = EAS.spawn_actor_from_object(asset, loc, rot)
    else:
        actor = EAS.spawn_actor_from_class(asset, loc, rot)
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER)
    return actor


def set_owner(actor, player=SOVIET):
    ok = False
    for comp in actor.get_components_by_class(unreal.RTSComponent):
        comp.set_editor_property("owning_player", player)
        ok = True
    return ok


def catmull(pts, step):
    P = [pts[0]] + list(pts) + [pts[-1]]
    dense = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        n = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / 50.0))
        for k in range(n):
            t = k / n
            dense.append(tuple(0.5 * (2 * p1[c] + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t * t
                                      + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t * t * t) for c in range(2)))
    dense.append(tuple(pts[-1]))
    out = [dense[0]]
    acc = 0.0
    for a, b in zip(dense, dense[1:]):
        acc += math.hypot(b[0] - a[0], b[1] - a[1])
        if acc >= step:
            out.append(b)
            acc = 0.0
    if out[-1] != dense[-1]:
        out.append(dense[-1])
    return out


def seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def track_points():
    pts = RAIL_EAST if TRACK_EXTENT == "rail_east" else [p for p in RAIL_EAST if p[1] <= 24500.0]
    if TRACK_EXTENT != "rail_east":
        pts = [(-2050.0, 26500.0)] + pts
    return catmull(pts, PANEL)


# ---------------------------------------------------------------- 1. target Blueprints
def build_target_bps():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    parent = unreal.EditorAssetLibrary.load_blueprint_class(SCAV_BASE)
    report = {}
    for name, (mesh_path, scav_time, health) in TARGETS.items():
        path = TARGET_HOME + "/" + name
        bp = unreal.load_asset(path)
        if bp is None:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            bp = tools.create_asset(name, TARGET_HOME, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        mesh_set = False
        for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
            obj = lib.get_object_for_blueprint(lib.get_data(handle), bp)
            if isinstance(obj, unreal.StaticMeshComponent) and obj.get_name().startswith("Mesh"):
                obj.set_editor_property("static_mesh", load(mesh_path))
                mesh_set = True
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property("scavenge_time", scav_time)
        cdo.set_editor_property("give_player_resource_rewards", False)
        cdo.set_editor_property("health", health)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
        report[name] = dict(mesh_set=mesh_set, saved=saved, scavenge_time=cdo.get_editor_property("scavenge_time"),
                            rewards=cdo.get_editor_property("give_player_resource_rewards"),
                            health=cdo.get_editor_property("health"))
    print(json.dumps(report, indent=1))
    return report


# ---------------------------------------------------------------- 2. map edit
def _xf(actor):
    loc, rot = actor.get_actor_location(), actor.get_actor_rotation()
    return [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1), round(rot.yaw, 2)]


def clear_own():
    n = 0
    for a in EAS.get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            EAS.destroy_actor(a)
            n += 1
    return n


def remove_old(log):
    labels = by_label()
    for label, actor in labels.items():
        if label in REMOVE_LABELS or any(label.startswith(p) for p in REMOVE_PREFIXES):
            log["actors"].append(dict(label=label, cls=actor.get_class().get_path_name(), xf=_xf(actor),
                                      folder=str(actor.get_folder_path())))
            EAS.destroy_actor(actor)
    rails = labels.get("BERW_Rails")
    if rails is None:
        return
    line = RAIL_EAST if TRACK_EXTENT == "rail_east" else [p for p in RAIL_EAST if p[1] <= 24500.0]
    for comp in rails.get_components_by_class(unreal.InstancedStaticMeshComponent):
        drop = []
        for i in range(comp.get_instance_count()):
            t = comp.get_instance_transform(i, True)
            p = (t.translation.x, t.translation.y)
            if min(seg_dist(p, a, b) for a, b in zip(line, line[1:])) < 300.0:
                drop.append(i)
                log["rail_instances"].append(dict(i=i, comp=comp.get_name(), loc=[t.translation.x, t.translation.y,
                                                                                  t.translation.z],
                                                  yaw=t.rotation.rotator().yaw))
        for i in sorted(drop, reverse=True):
            comp.remove_instance(i)


PANEL = 2551.0          # SM_RustedRefinery_RailRoad length: ARoadSplineActor stretches ONE panel per spline segment,
                        # so the points must sit about a panel apart (Ger_Trains: 30-45 m segments)
SLEEPER_TOP = 95.0      # sleeper tops above the mesh pivot; with SINK 85 they stand ~10 cm proud of the ground
PROFILE_TOLERANCE = 20.0


def _track_profile(spline):
    """Per segment: worst |sleeper top - ground - (SLEEPER_TOP - SINK)| (cm), sampled every metre."""
    n = spline.get_number_of_spline_points()
    worst = []
    for i in range(n - 1):
        d0 = spline.get_distance_along_spline_at_spline_point(i)
        d1 = spline.get_distance_along_spline_at_spline_point(i + 1)
        dev = 0.0
        d = d0
        while d <= d1:
            loc = spline.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
            dev = max(dev, abs(loc.z + SLEEPER_TOP - ground(loc.x, loc.y) - (SLEEPER_TOP - SINK)))
            d += 100.0
        worst.append(dev)
    return worst


def lay_track(actor):
    """Points a panel apart along the line, sunk SINK; then split any segment whose sleepers would end up buried or
    floating by more than PROFILE_TOLERANCE (up to 4 passes). Returns (points, worst deviation cm)."""
    pts = [(x, y) for x, y in track_points()]
    spline = actor.get_component_by_class(unreal.SplineComponent)
    worst = []
    for _ in range(5):
        spline.set_spline_points([unreal.Vector(x, y, ground(x, y) - SINK) for x, y in pts],
                                 unreal.SplineCoordinateSpace.WORLD, True)
        worst = _track_profile(spline)
        bad = [i for i, w in enumerate(worst) if w > PROFILE_TOLERANCE]
        if not bad:
            break
        for i in reversed(bad):
            a, b = pts[i], pts[i + 1]
            pts.insert(i + 1, ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0))
    actor.set_editor_property("RoadMesh", load(RAIL_MESH))
    actor.set_editor_property("RoadMaterial", load(RAIL_MATERIAL) if RAIL_MATERIAL else None)
    actor.set_editor_property("bUseCollision", False)
    actor.set_editor_property("bAllowDecals", False)
    return len(pts), (round(max(worst), 1) if worst else None)


def place_track(report):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(ROAD_SPLINE)
    p0 = track_points()[0]
    spawn(cls, "RY_Rail_Main", p0[0], p0[1], ground(*p0) - SINK, 0.0)
    # the owner's rule (2026-11-22): one even spacing over the whole railway, no short segments
    report["track"] = lay_track_even_tangents(TRACK_SPACING, max_sink=TRACK_MAX_SINK)


def fix_track():
    """Re-lays the existing RY_Rail_Main (2026-11-22 fix: the first build put points 6 m apart, squeezing each 25.5 m
    panel into 6 m)."""
    actor = by_label()["RY_Rail_Main"]
    n, worst = lay_track(actor)
    meshes = actor.get_components_by_class(unreal.SplineMeshComponent)
    spline = actor.get_component_by_class(unreal.SplineComponent)
    out = dict(points=n, worst_profile_cm=worst, spline_meshes=len(meshes),
               mean_segment_m=round(spline.get_spline_length() / max(n - 1, 1) / 100.0, 1),
               materials=[m.get_name() if m else None for m in meshes[0].get_materials()] if meshes else None)
    print(json.dumps(out))
    return out


def fix_posts():
    """Re-writes the strongpoint posts (2026-11-22 fix: squads within a gate's OpenRadius held the gates open)."""
    sp = by_label()["RY_Strongpoint"]
    sx, sy = STRONGPOINT_AT
    posts = list(sp.get_editor_property("posts"))
    want = {p[0]: p for p in POSTS}
    for post in posts:
        pid = str(post.get_editor_property("post_id"))
        if pid not in want:
            continue
        _, x, y, yaw, kind, _, _ = want[pid]
        post.set_editor_property("post", unreal.Transform(unreal.Vector(x - sx, y - sy, 0.0),
                                                          unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1, 1, 1)))
        post.set_editor_property("take_nearby_cover", False)
    sp.set_editor_property("posts", posts)
    print([(str(p.get_editor_property("post_id")), p.get_editor_property("post").translation,
            p.get_editor_property("take_nearby_cover")) for p in sp.get_editor_property("posts")])


def place_walls(report):
    mods, _ = walls()
    assets = {"long": load(WALL_LONG), "gateA": load(GATE_A), "gateC": load(GATE_C)}
    rows = []
    for k, (kind, cx, cy, yaw) in enumerate(mods):
        a = math.radians(yaw)
        d = (math.cos(a), math.sin(a))
        half = LONG_HALF if kind == "long" else GATE_HALF[kind[-1]]
        span = 2 * (LONG_HALF if kind == "long" else (475.0 if kind == "gateA" else 550.0))
        px, py = cx + d[0] * half, cy + d[1] * half
        zs = [ground(cx + d[0] * s * span / 2, cy + d[1] * s * span / 2) for s in (-1, -0.5, 0, 0.5, 1)]
        actor = spawn(assets[kind], "RY_Wall_%02d_%s" % (k, kind), px, py, min(zs) - 10.0, yaw)
        owner = set_owner(actor)
        rows.append([actor.get_actor_label(), round(px), round(py), round(min(zs) - 10.0), yaw, owner])
    report["walls"] = rows


def place_targets(report):
    ramp = load(TARGET_HOME + "/BP_Berlin_RailRamp_Sabotage")
    tower = load(TARGET_HOME + "/BP_Berlin_SwitchTower_Sabotage")
    x, y, yaw = RAMP_AT
    spawn(ramp, "RY_Target_Ramp", x, y, ground(x, y), yaw)
    x, y, yaw = SWITCH_TOWER_AT
    z = min(ground(x + dx, y + dy) for dx in (-400, 0, 400) for dy in (-500, 0, 500))
    spawn(tower, "RY_Target_SwitchTower", x, y, z, yaw)
    report["targets"] = [RAMP_AT, SWITCH_TOWER_AT]


def place_towers(report):
    bp = load(TOWER_MG)
    rows = []
    for label, x, y, yaw in TOWERS:
        z = min(ground(x + dx, y + dy) for dx in (-280, 0, 280) for dy in (-280, 0, 390))
        actor = spawn(bp, label, x, y, z - 3.0, yaw)
        rows.append([label, x, y, round(z), set_owner(actor)])
    report["towers"] = rows


def place_strongpoint(report):
    sx, sy = STRONGPOINT_AT
    sp = spawn(unreal.EnemyStrongpoint, "RY_Strongpoint", sx, sy, ground(sx, sy), 0.0)
    sp.set_editor_property("strongpoint_id", "RailYard")
    posts = []
    for post_id, x, y, yaw, kind, sub, dig in POSTS:
        unit = unreal.EnemyPlacementUnit()
        if kind == "vehicle":
            unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.VEHICLE)
            unit.set_editor_property("vehicle_subtype", getattr(unreal.TankSubtype, sub))
        else:
            unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.SQUAD)
            unit.set_editor_property("squad_subtype", getattr(unreal.SquadSubtype, sub))
        post = unreal.EnemyStrongpointPost()
        post.set_editor_property("post_id", post_id)
        post.set_editor_property("post", unreal.Transform(unreal.Vector(x - sx, y - sy, 0.0),
                                                          unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1, 1, 1)))
        post.set_editor_property("unit_options", [unit])
        post.set_editor_property("dig_in", dig)
        post.set_editor_property("take_nearby_cover", False)
        posts.append(post)
    sp.set_editor_property("posts", posts)
    report["strongpoint"] = dict(posts=[p[0] for p in POSTS], at=STRONGPOINT_AT)


def place_dressing(report):
    gx, gy = X_S + 105.0, Y_E          # the broken south-east corner gap
    pipes = load(DRAINPIPES)
    a = spawn(pipes, "RY_Dress_DrainPipes", gx - 700.0, gy + 600.0, ground(gx - 700.0, gy + 600.0), 90.0)
    for comp in a.get_components_by_class(unreal.PrimitiveComponent):
        comp.set_editor_property("can_ever_affect_navigation", False)
    report["dressing"] = ["RY_Dress_DrainPipes"]


def place():
    os.makedirs(OUT_DIR, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log = {"actors": [], "rail_instances": [], "track_extent": TRACK_EXTENT}
    report = {"cleared_own": clear_own()}
    remove_old(log)
    with open(os.path.join(OUT_DIR, "removed_%s.json" % stamp), "w") as f:
        json.dump(log, f, indent=1)
    report["removed_actors"] = [a["label"] for a in log["actors"]]
    report["removed_rail_instances"] = len(log["rail_instances"])
    place_track(report)
    place_walls(report)
    place_targets(report)
    place_towers(report)
    place_strongpoint(report)
    place_dressing(report)
    print(json.dumps(report, indent=1, default=str))
    return report


# ---------------------------------------------------------------- 3. checks
HQ = (13900.0, 43300.0)
LANE_W = (-6500.0, 10556.0)
LANE_MID = (-2708.0, 24444.0)
AGENTS = ["Character", "LightTank", "MediumTank", "HeavyTank"]


def _navs():
    return {n.get_name().replace("RecastNavMesh-", ""): n
            for n in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.RecastNavMesh)}


def _project(nav, x, y, ext=300.0):
    q = NS.project_point_to_navigation(world(), unreal.Vector(x, y, ground(x, y) + 50.0), nav, None,
                                       unreal.Vector(ext, ext, 400.0))
    if q is None or (q.x == 0 and q.y == 0 and q.z == 0):
        return None
    return q


def _path(nav, a, b, ext=300.0, closed=True):
    """closed: the game's planner filter for queries without a unit (URTSClosedBreachableWallsQueryFilter):
    breachable Soviet walls and closed gates block, as they do for a player unit that has not breached yet."""
    pa, pb = _project(nav, *a, ext=ext), _project(nav, *b, ext=ext)
    if pa is None or pb is None:
        return "off-nav"
    flt = unreal.RTSClosedBreachableWallsQueryFilter.static_class() if closed else None
    p = NS.find_path_to_location_synchronously(world(), pa, pb, nav, flt)
    pts = list(p.path_points) if p else []
    if not pts:
        return "none"
    ln = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100.0
    end = pts[-1]
    if p.is_partial() or math.hypot(end.x - pb.x, end.y - pb.y) > 100:
        return "partial %.0f m" % ln
    return "ok %.0f m" % ln


def nav_report():
    navs = _navs()
    tests = [("HQ -> lane at the yard", HQ, LANE_MID), ("lane at the yard -> lane SW", LANE_MID, LANE_W),
             ("E5 gap across", (-4200.0, 20150.0), (-5300.0, 20150.0)),
             ("outside SE gap -> ramp", (-5600.0, 17200.0), (-2700.0, 14800.0)),
             ("ramp -> lane at the yard (response)", (-2700.0, 14800.0), LANE_MID),
             ("lane SW -> switch tower", LANE_W, (-3600.0, 23900.0)),
             ("lane SW -> west wall outside", LANE_W, (-3000.0, 11500.0)),
             ("inside court -> outside north (walls closed)", (-2600.0, 15000.0), (-2600.0, 17600.0)),
             ("SE gap: outside -> inside, straight", (-5100.0, 16900.0), (-4300.0, 16300.0))]
    res = {}
    for name, a, b in tests:
        res[name] = {ag: _path(navs[ag], a, b) for ag in AGENTS if ag in navs}
        print("%-38s %s" % (name, res[name]))
    # scavenge spots: a ring round each target (half size + 1.5 m), infantry
    for label, (x, y, yaw), hx, hy in (("ramp", RAMP_AT, 503.0, 1125.0), ("switch tower", SWITCH_TOWER_AT, 414.0, 503.0)):
        ok = 0
        for k in range(12):
            t = 2 * math.pi * k / 12
            px, py = x + math.cos(t) * (hx + 150.0), y + math.sin(t) * (hy + 150.0)
            if _project(navs["Character"], px, py, ext=80.0) is not None:
                ok += 1
        res["scavenge spots " + label] = "%d/12 on infantry nav" % ok
        print("scavenge spots %-14s %d/12 on infantry nav" % (label, ok))
    with open(os.path.join(OUT_DIR, "nav_report.json"), "w") as f:
        json.dump(res, f, indent=1)
    return res


SAVE_STATE = {"ticks": 0}


def save_when_nav_ready():
    SAVE_STATE["ticks"] = 0

    def _defer():
        SAVE_STATE["ticks"] += 1
        if SAVE_STATE["ticks"] < 60:
            return False
        if NS.get_navigation_system(world()).is_navigation_being_built(world()):
            return False
        ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        print("saved", ok, "after ticks", SAVE_STATE["ticks"])
        return True
    return _defer


def wait_nav():
    SAVE_STATE["ticks"] = 0

    def _defer():
        SAVE_STATE["ticks"] += 1
        if SAVE_STATE["ticks"] < 60:
            return False
        return not NS.get_navigation_system(world()).is_navigation_being_built(world())
    return _defer


# ---------------------------------------------------------------- 4. play-in-editor check (walls and gates add their
# nav areas at BeginPlay: RuntimeGeneration=DynamicModifiersOnly, so editor-world queries cannot see them)
PIE = {"ticks": 0, "phase": "start", "res": {}}
PIE_TESTS = [
    ("court -> north outside (through the east wall)", (-2600.0, 15000.0), (-2600.0, 17800.0)),
    ("court -> west outside (through the west wall)", (-2600.0, 13000.0), (-2600.0, 11300.0)),
    ("outside SE gap -> ramp", (-5300.0, 17000.0), (-2700.0, 14800.0)),
    ("lane at the yard -> lane SW", LANE_MID, LANE_W),
    ("HQ -> lane at the yard", HQ, LANE_MID),
    ("E5 gap across", (-4200.0, 20150.0), (-5300.0, 20150.0)),
    ("lane SW -> switch tower", LANE_W, (-3600.0, 23900.0)),
]


PIE_SECONDS = 25.0
PIE_WALL_SECONDS = 90.0      # real-time cap: a mission stage may pause the game at start


def _pie_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _pie_checks(w):
    navs = {n.get_name().replace("RecastNavMesh-", ""): n
            for n in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)}
    flt = unreal.RTSClosedBreachableWallsQueryFilter.static_class()
    res = {}

    def proj(nav, x, y):
        q = NS.project_point_to_navigation(w, unreal.Vector(x, y, ground(x, y) + 50.0), nav, flt,
                                           unreal.Vector(300.0, 300.0, 400.0))
        return None if q is None or (q.x == 0 and q.y == 0 and q.z == 0) else q
    for name, a, b in PIE_TESTS:
        row = {}
        for ag in AGENTS:
            nav = navs.get(ag)
            if nav is None:
                continue
            pa, pb = proj(nav, *a), proj(nav, *b)
            if pa is None or pb is None:
                row[ag] = "off-nav"
                continue
            p = NS.find_path_to_location_synchronously(w, pa, pb, nav, flt)
            pts = list(p.path_points) if p else []
            if not pts:
                row[ag] = "none"
                continue
            ln = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100.0
            end = pts[-1]
            row[ag] = ("partial %.0f m" if p.is_partial() or math.hypot(end.x - pb.x, end.y - pb.y) > 100
                       else "ok %.0f m") % ln
        res[name] = row
    # what came up live
    live = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        label = a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name()
        cls = a.get_class().get_name()
        loc = a.get_actor_location()
        if not (X_S - 1500 < loc.x < X_N + 2500 and Y_W - 1500 < loc.y < Y_E + 8000):
            continue
        if any(k in cls for k in ("Rus_Vehicle", "Squad", "GuardTower", "Sabotage", "RusWallGate", "Rus_Wall")):
            live.setdefault(cls, 0)
            live[cls] += 1
    res["live actors in and round the court"] = live
    units = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn):
        loc = a.get_actor_location()
        if X_S - 1500 < loc.x < X_N + 2500 and Y_W - 1500 < loc.y < Y_E + 3000:
            units.setdefault(a.get_class().get_name(), []).append([round(loc.x), round(loc.y)])
    res["pawns near the court"] = units
    navbox = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        cls = a.get_class().get_name()
        if cls.startswith("Rus_Wall") or "RusWallGate" in cls:
            comps = a.get_components_by_class(unreal.ActorComponent)
            names = [c.get_class().get_name() for c in comps]
            navbox[a.get_name()] = {n: names.count(n) for n in set(names)}
    res["wall components"] = {k: v for k, v in list(navbox.items())[:2]}
    res["game seconds"] = unreal.GameplayStatics.get_time_seconds(w)
    states = {}
    for act in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        cls = act.get_class().get_name()
        if "RusWallGate" in cls:
            states[act.get_name()] = _try(act, ["get_gate_state"], ["gate_state", "m_gate_state"])
        elif cls.startswith("Rus_Vehicle"):
            dig = act.get_component_by_class(unreal.DigInComponent) if hasattr(unreal, "DigInComponent") else None
            states[cls] = dict(digin=_try(dig, ["get_is_dug_in", "get_dig_in_status"], ["m_dig_in_status"]) if dig else
                               "no DigInComponent", walls_near=_dig_walls_near(w, act.get_actor_location()))
        elif "GuardTower" in cls:
            states[act.get_name()] = _try(act, ["get_bxp_status", "get_building_status", "get_is_built",
                                                "get_construction_state"], ["bxp_status", "m_bxp_status"])
        elif "Sabotage" in cls:
            states[cls] = _try(act, ["get_can_scavenge"], ["can_scavenge", "b_can_scavenge", "scavenge_time"])
    res["states"] = states
    return res


def _try(obj, methods, props):
    if obj is None:
        return "none"
    for m in methods:
        f = getattr(obj, m, None)
        if callable(f):
            try:
                return str(f())
            except Exception as e:
                return "%s: %s" % (m, e)
    for pr in props:
        try:
            return "%s=%s" % (pr, obj.get_editor_property(pr))
        except Exception:
            continue
    return "n/a (not exposed to python)"


def _dig_walls_near(w, loc):
    """Dug-in vehicles raise a dig-in wall actor round them (DigInWall)."""
    n = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        if "DigIn" in a.get_class().get_name() and (a.get_actor_location() - loc).length() < 1200:
            n += 1
    return n


def pie_check():
    PIE.update(ticks=0, phase="start", res={})
    PIE.pop("wall0", None)
    PIE.pop("unpaused", None)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()

    def _defer():
        PIE["ticks"] += 1
        w = _pie_world()
        if PIE["phase"] == "start":
            if w is not None:
                PIE["phase"], PIE["ticks"] = "settle", 0
            elif PIE["ticks"] > 3000:
                print("PIE never started")
                return True
            return False
        if PIE["phase"] == "settle":
            if "wall0" not in PIE:
                PIE["wall0"] = time.time()
            waited = time.time() - PIE["wall0"]
            if unreal.GameplayStatics.is_game_paused(w):
                # the Berlin mission start (intro / briefing) pauses the game; the check needs the world running
                unreal.GameplayStatics.set_game_paused(w, False)
                PIE["unpaused"] = PIE.get("unpaused", 0) + 1
            game_s = unreal.GameplayStatics.get_time_seconds(w)
            if game_s < PIE_SECONDS and waited < PIE_WALL_SECONDS:
                return False
            if NS.get_navigation_system(w).is_navigation_being_built(w) and waited < PIE_WALL_SECONDS + 60:
                return False
            print("settled: game seconds %.1f, real seconds %.0f, unpaused %d times, paused %s, dilation %s" % (
                game_s, waited, PIE.get("unpaused", 0), unreal.GameplayStatics.is_game_paused(w),
                unreal.GameplayStatics.get_global_time_dilation(w)))
            PIE["res"] = _pie_checks(w)
            for k, v in PIE["res"].items():
                print("%-48s %s" % (k, v))
            with open(os.path.join(OUT_DIR, "pie_check.json"), "w") as f:
                json.dump(PIE["res"], f, indent=1)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            PIE["phase"], PIE["ticks"] = "end", 0
            return False
        return _pie_world() is None or PIE["ticks"] > 600
    return _defer


# ---------------------------------------------------------------- 5. pictures (scene capture in the editor world)
SHOTS = {  # name: (target x, y, distance cm, pitch deg, yaw deg looking direction, fov)
    "railyard_d1_game_west": (-2700.0, 15600.0, 11000.0, -55.0, -90.0, 60.0),
    "railyard_d1_game_close": (-2600.0, 14800.0, 6500.0, -50.0, -90.0, 60.0),
    "railyard_d1_switchtower": (-2600.0, 23000.0, 4500.0, -50.0, -90.0, 60.0),
    "railyard_d1_top": (-2600.0, 15800.0, 9000.0, -89.9, 0.0, 70.0),
}


def capture_shots(out_dir="D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard/shots", res=(1600, 1000)):
    os.makedirs(out_dir, exist_ok=True)
    w = world()
    rt = unreal.RenderingLibrary.create_render_target2d(w, res[0], res[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    done = []
    for name, (tx, ty, dist, pitch, yaw, fov) in SHOTS.items():
        p, yw = math.radians(-pitch), math.radians(yaw)
        tz = ground(tx, ty)
        loc = unreal.Vector(tx - math.cos(yw) * math.cos(p) * dist, ty - math.sin(yw) * math.cos(p) * dist,
                            tz + math.sin(p) * dist)
        cap = EAS.spawn_actor_from_class(unreal.SceneCapture2D, loc, unreal.Rotator(0.0, pitch, yaw))
        comp = cap.capture_component2d
        comp.set_editor_property("texture_target", rt)
        comp.set_editor_property("fov_angle", fov)
        comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.capture_scene()
        comp.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, rt, out_dir, name + ".png")
        EAS.destroy_actor(cap)
        done.append(name)
    print("shots", done, out_dir)
    return done


def _near_classes(w, loc, radius=1500.0):
    out = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        if (a.get_actor_location() - loc).length() < radius:
            c = a.get_class().get_name()
            out[c] = out.get(c, 0) + 1
    return out


def pie_probe_units():
    """PIE: what stands round the two tanks (dig-in walls?) and what the guard towers expose (built state)."""
    PIE.update(ticks=0, phase="start", res={})
    PIE.pop("wall0", None)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()

    def _defer():
        w = _pie_world()
        if PIE["phase"] == "start":
            if w is not None:
                PIE["phase"] = "settle"
                PIE["wall0"] = time.time()
            return False
        if PIE["phase"] == "settle":
            if unreal.GameplayStatics.is_game_paused(w):
                unreal.GameplayStatics.set_game_paused(w, False)
            if time.time() - PIE["wall0"] < 60.0:
                return False
            for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
                cls = a.get_class().get_name()
                if cls.startswith("Rus_Vehicle"):
                    loc = a.get_actor_location()
                    print(cls, "at", [round(loc.x), round(loc.y), round(loc.z)], "near:", _near_classes(w, loc, 900.0))
                    dig = a.get_component_by_class(unreal.DigInComponent)
                    print("   dig-in component props:", [p for p in dir(dig) if "dig" in p.lower() or "status" in p.lower()][:20])
                elif "GuardTower" in cls:
                    hc = a.get_health_component()
                    hp = {}
                    for n in dir(hc):
                        if n.startswith("get") and ("health" in n.lower() or "percent" in n.lower()):
                            try:
                                hp[n] = getattr(hc, n)()
                            except Exception:
                                pass
                    print(cls, a.get_name(), "health:", hp, "near:", _near_classes(w, a.get_actor_location(), 700.0))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            PIE["phase"] = "end"
            PIE["wall0"] = time.time()
            return False
        return _pie_world() is None or time.time() - PIE["wall0"] > 30.0
    return _defer


# ---------------------------------------------------------------- even spacing (owner request 2026-11-22: no short
# segments; one spline mesh per segment, so short ones only add vertices; the same spacing along the whole line)
SPACINGS = (3200.0, 3000.0, 2800.0, 2700.0, 2600.0, PANEL)
TRACK_SPACING = 3000.0      # built: 13 segments of 30.8-31.2 m over the 405 m line (Ger_Trains: 30-45 m)
TRACK_MAX_SINK = 8.0        # cm the sleeper tops may sink into a hump; over steps / dips the track stands proud
EVEN_TOLERANCE = 25.0       # cm: worst sleeper-top error accepted along the line


def even_points(spacing):
    """The rail_east line as a smooth curve, cut into equal arc-length segments as close to `spacing` as divides the
    whole length (never shorter than one panel)."""
    pts = RAIL_EAST if TRACK_EXTENT == "rail_east" else [(-2050.0, 26500.0)] + [p for p in RAIL_EAST if p[1] <= 24500.0]
    dense = catmull(pts, 50.0)
    cum = [0.0]
    for a, b in zip(dense, dense[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    total = cum[-1]
    n = max(1, int(total // spacing))           # floor: segments are >= spacing
    step = total / n
    out, j = [], 0
    for k in range(n + 1):
        d = min(k * step, total)
        while j < len(cum) - 2 and cum[j + 1] < d:
            j += 1
        t = 0.0 if cum[j + 1] == cum[j] else (d - cum[j]) / (cum[j + 1] - cum[j])
        a, b = dense[j], dense[j + 1]
        out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return out, step


def lay_track_even(actor=None, spacing=None):
    """Tries the spacings widest first and keeps the first whose worst sleeper error is within EVEN_TOLERANCE
    (or the panel length if none is). Prints every try."""
    actor = actor or by_label()["RY_Rail_Main"]
    spline = actor.get_component_by_class(unreal.SplineComponent)
    tries = []
    chosen = None
    for s in ([spacing] if spacing else SPACINGS):
        pts, step = even_points(s)
        spline.set_spline_points([unreal.Vector(x, y, ground(x, y) - SINK) for x, y in pts],
                                 unreal.SplineCoordinateSpace.WORLD, True)
        worst = max(_track_profile(spline))
        tries.append(dict(target_m=s / 100.0, segment_m=round(step / 100.0, 2), points=len(pts),
                          worst_cm=round(worst, 1)))
        if worst <= EVEN_TOLERANCE:
            chosen = (s, pts, step, worst)
            break
    if chosen is None:
        s = SPACINGS[-1]
        pts, step = even_points(s)
        spline.set_spline_points([unreal.Vector(x, y, ground(x, y) - SINK) for x, y in pts],
                                 unreal.SplineCoordinateSpace.WORLD, True)
        chosen = (s, pts, step, max(_track_profile(spline)))
    actor.set_editor_property("RoadMesh", load(RAIL_MESH))
    actor.set_editor_property("RoadMaterial", load(RAIL_MATERIAL) if RAIL_MATERIAL else None)
    actor.set_editor_property("bUseCollision", False)
    actor.set_editor_property("bAllowDecals", False)
    segs = [spline.get_distance_along_spline_at_spline_point(i + 1) - spline.get_distance_along_spline_at_spline_point(i)
            for i in range(spline.get_number_of_spline_points() - 1)]
    out = dict(tries=tries, chosen_segment_m=round(chosen[2] / 100.0, 2), points=len(chosen[1]),
               worst_cm=round(chosen[3], 1), spline_meshes=len(actor.get_components_by_class(unreal.SplineMeshComponent)),
               segment_min_m=round(min(segs) / 100.0, 2), segment_max_m=round(max(segs) / 100.0, 2))
    print(json.dumps(out, indent=1))
    return out


def _segment_errors(spline):
    """Per segment: list of (sleeper top - ground - target) samples every metre; positive = floating."""
    n = spline.get_number_of_spline_points()
    out = []
    for i in range(n - 1):
        d0 = spline.get_distance_along_spline_at_spline_point(i)
        d1 = spline.get_distance_along_spline_at_spline_point(i + 1)
        errs = []
        d = d0
        while d <= d1:
            loc = spline.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
            errs.append(loc.z + SLEEPER_TOP - ground(loc.x, loc.y) - (SLEEPER_TOP - SINK))
            d += 100.0
        out.append(errs)
    return out


def _rebuild(actor):
    """ARoadSplineActor rebuilds its spline meshes on a property change; setting the same value again does not."""
    actor.set_editor_property("RoadMesh", None)
    actor.set_editor_property("RoadMesh", load(RAIL_MESH))


MAX_FLOAT = 8.0     # cm: sleeper tops may float at most this much above their target height


def lay_track_even_fitted(spacing=3000.0, passes=10):
    """Equal segments of ~spacing along the whole line; each point's height is then fitted (minimax over its two
    neighbouring segments) so the sleepers neither float nor sink far anywhere. Same spacing over the whole railway."""
    actor = by_label()["RY_Rail_Main"]
    spline = actor.get_component_by_class(unreal.SplineComponent)
    pts, step = even_points(spacing)
    zs = [ground(x, y) - SINK for x, y in pts]
    for _ in range(passes):
        spline.set_spline_points([unreal.Vector(x, y, z) for (x, y), z in zip(pts, zs)],
                                 unreal.SplineCoordinateSpace.WORLD, True)
        errs = _segment_errors(spline)
        new = []
        for i in range(len(pts)):
            near = []
            if i > 0:
                near += errs[i - 1][len(errs[i - 1]) // 2:]
            if i < len(errs):
                near += errs[i][:len(errs[i]) // 2 + 1]
            if not near:
                shift = 0.0
            elif max(near) > MAX_FLOAT:
                shift = max(near) - MAX_FLOAT          # never let sleepers hover: sink into the bump instead
            else:
                shift = max(0.0, (max(near) + min(near)) / 2.0)
            new.append(zs[i] - 0.8 * shift)
        zs = new
    spline.set_spline_points([unreal.Vector(x, y, z) for (x, y), z in zip(pts, zs)],
                             unreal.SplineCoordinateSpace.WORLD, True)
    errs = _segment_errors(spline)
    actor.set_editor_property("RoadMaterial", load(RAIL_MATERIAL) if RAIL_MATERIAL else None)
    actor.set_editor_property("bUseCollision", False)
    actor.set_editor_property("bAllowDecals", False)
    _rebuild(actor)
    segs = [spline.get_distance_along_spline_at_spline_point(i + 1) - spline.get_distance_along_spline_at_spline_point(i)
            for i in range(spline.get_number_of_spline_points() - 1)]
    flat = [e for s in errs for e in s]
    out = dict(segment_m=round(step / 100.0, 2), points=len(pts),
               spline_meshes=len(actor.get_components_by_class(unreal.SplineMeshComponent)),
               segment_min_m=round(min(segs) / 100.0, 2), segment_max_m=round(max(segs) / 100.0, 2),
               worst_float_cm=round(max(flat), 1), worst_sunk_cm=round(-min(flat), 1),
               within_15cm_pct=round(100.0 * sum(1 for e in flat if abs(e) <= 15.0) / len(flat), 1),
               per_segment_worst=[round(max(abs(e) for e in s)) for s in errs])
    print(json.dumps(out))
    return out


def _apply_ground_tangents(spline, pts, zs):
    """Keeps each point's horizontal tangent, sets its vertical part to the ground slope there (sampled +-3 m), so the
    cubic between points follows humps that a 30 m segment would otherwise cut through or bridge."""
    n = len(pts)
    for i in range(n):
        tan = spline.get_tangent_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
        h = math.hypot(tan.x, tan.y)
        if h < 1e-3:
            continue
        ux, uy = tan.x / h, tan.y / h
        x, y = pts[i]
        slope = (ground(x + ux * 300.0, y + uy * 300.0) - ground(x - ux * 300.0, y - uy * 300.0)) / 600.0
        spline.set_tangent_at_spline_point(i, unreal.Vector(tan.x, tan.y, slope * h),
                                           unreal.SplineCoordinateSpace.WORLD, False)
    spline.update_spline()


def lay_track_even_tangents(spacing=3000.0, passes=10, max_float=15.0, max_sink=None):
    """Even ~spacing segments over the whole line; ground-slope tangents; heights fitted minimax with the float capped
    at max_float. Prints the error spread; the same spacing everywhere."""
    actor = by_label()["RY_Rail_Main"]
    spline = actor.get_component_by_class(unreal.SplineComponent)
    pts, step = even_points(spacing)
    zs = [ground(x, y) - SINK for x, y in pts]

    def apply():
        spline.set_spline_points([unreal.Vector(x, y, z) for (x, y), z in zip(pts, zs)],
                                 unreal.SplineCoordinateSpace.WORLD, True)
        _apply_ground_tangents(spline, pts, zs)
    for _ in range(passes):
        apply()
        errs = _segment_errors(spline)
        new = []
        for i in range(len(pts)):
            near = (errs[i - 1][len(errs[i - 1]) // 2:] if i > 0 else []) + \
                   (errs[i][:len(errs[i]) // 2 + 1] if i < len(errs) else [])
            if not near:
                new.append(zs[i])
                continue
            shift = (max(near) + min(near)) / 2.0
            if max_sink is not None:
                # the mesh's stringers reach 1.2 m under the sleeper tops: a raised stretch still looks carried,
                # a buried one vanishes, so cap the sinking and let the track stand proud over steps and dips
                if min(near) - shift < -max_sink:
                    shift = min(near) + max_sink
            elif max(near) - shift > max_float:
                shift = max(near) - max_float
            new.append(zs[i] - 0.8 * shift)
        zs = new
    apply()
    errs = _segment_errors(spline)
    actor.set_editor_property("RoadMaterial", load(RAIL_MATERIAL) if RAIL_MATERIAL else None)
    actor.set_editor_property("bUseCollision", False)
    actor.set_editor_property("bAllowDecals", False)
    _rebuild(actor)
    segs = [spline.get_distance_along_spline_at_spline_point(i + 1) - spline.get_distance_along_spline_at_spline_point(i)
            for i in range(spline.get_number_of_spline_points() - 1)]
    flat = [e for s in errs for e in s]
    out = dict(segment_m=round(step / 100.0, 2), points=len(pts),
               spline_meshes=len(actor.get_components_by_class(unreal.SplineMeshComponent)),
               segment_min_m=round(min(segs) / 100.0, 2), segment_max_m=round(max(segs) / 100.0, 2),
               worst_float_cm=round(max(flat), 1), worst_sunk_cm=round(-min(flat), 1),
               within_15cm_pct=round(100.0 * sum(1 for e in flat if abs(e) <= 15.0) / len(flat), 1),
               per_segment_worst=[round(max(abs(e) for e in s)) for s in errs])
    print(json.dumps(out))
    return out
