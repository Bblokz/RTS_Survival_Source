"""In-editor build of Field_BerlinKanal (convoy field map, Berlin, owner pick B "Kanalufer").

Design: MapDesigner/ConvoyMaps/Berlin/convoy_berlin_B.layout.json. Terrain + dressing: bku_terrain_gen.py
(Saved/FieldMaps/BerlinKanal/terrain). Runs inside a throwaway full editor driven by bku_editor_server.py / bku_cmd.py:

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/FieldMaps")
    import bku_map_build as bb; importlib.reload(bb)
    bb.step_base()                     # copy the template, strip it, Berlin landscape, level actors, Berlin light
    bb.step_dress()                    # canal + crossings, rails, ruins, skirts, cover, trees, wrecks, decals, props
    bb.step_convoy()                   # definition data asset, route, director, strength slots, mission
    bb.check_routes()                  # blocking nav build + drive paths of the route and the branch
    _defer = bb.W.rebuild_nav_and_save()

Reuses wismut_map_build (EnemyItemMaps) for the generic steps by retargeting its module globals, as ovk_map_build does
for Field_OreViadukt. Every placed actor has the BKU_ label prefix and sits under the outliner folder BerlinKanal/;
each step clears and redoes its own part.
"""
import importlib
import json
import math
import random
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/EnemyItemMaps")
import wismut_map_build as W  # noqa: E402

importlib.reload(W)

SAVED = "D:/UE5Projects/RTS_Survival/Saved/FieldMaps/BerlinKanal/"
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/Field_BerlinKanal"
DATA = MAP_PATH + "/"                  # owner rule: the map's data assets in a folder named like the map
PREFIX, FOLDER = "BKU_", "BerlinKanal/"
LS = "/Game/RTS_Survival/Materials/ground/Landscape/Berlin/"
W.MAP_PATH = MAP_PATH
W.LAYOUT_FILE = SAVED + "terrain/BKU_layout.json"
W.SHOTS = SAVED + "shots/"
W.LANDSCAPE_LABEL = PREFIX + "Landscape"
W.LANDSCAPE_MATERIAL = LS + "MI_RTS_Landscape_Berlin"          # the Ger_Berlin landscape instance
W.LAYER_INFOS = {name: LS + "DATA/%s_LayerInfo" % name for name in
                 ("BerlinRubble", "BerlinAsh", "BerlinCobble", "BerlinGlass", "BerlinScrub", "BerlinMud")}
W.PREFIX, W.FOLDER = PREFIX, FOLDER
W.Z_LOW, W.Z_HIGH = -800.0, 2400.0
W.NAV_LOW, W.NAV_HIGH = -600.0, 1800.0
ENEMY = 2
W.load_layout()                         # reload(W) above emptied its LAYOUT

DRESS_FILE = SAVED + "terrain/BKU_dress.json"
KIT_PROBE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/probe_kit.json"
REPORT = []

ENV = "/Game/RTS_Survival/Blueprints/Environment/"
DEST = ENV + "DestructableEnvActor/"
RUIN_KIT = ENV + "Berlin/RuinKit/"
ORE = ENV + "OreMountains/"
BX = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
RK = BX + "RusBunkers/"
WATER_MI = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
RAIL_MESH = "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01"
TANK_BLOCKER = "/Game/RTS_Survival/Blueprints/Nav/BlockingActors/BP_NavBlocker_VolumeBlockTanks"
SRC_DEFINITION = "/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_VehicleParts"
DEFINITION = DATA + "DA_Convoy_Field_BerlinKanal"
FIRST_CONVOY_DELAY = 60.0
TEST_TRUCKS = 6

# per component: tint RGB, soot, rubble desaturation (berlin_map_build.RUBBLE_TINTS / FOOT_TINTS)
RUBBLE_TINTS = ((0.90, 0.90, 0.90, 1.20, 0.35), (1.00, 0.97, 0.93, 1.00, 0.10),
                (0.80, 0.81, 0.82, 1.30, 0.45), (0.95, 0.93, 0.90, 0.90, 0.25))
FOOT_TINTS = ((0.95, 0.95, 0.95, 0.80, 0.20), (0.85, 0.86, 0.87, 0.90, 0.35),
              (1.00, 0.97, 0.94, 0.60, 0.10), (0.80, 0.81, 0.82, 0.90, 0.30))


def log(text):
    REPORT.append(str(text))
    W.log("BKU " + str(text))


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


def kit_mesh(name):
    if not hasattr(kit_mesh, "paths"):
        kit_mesh.paths = {p.rsplit("/", 1)[1]: p for p in json.load(open(KIT_PROBE))["meshes"]}
    path = kit_mesh.paths[name]
    return unreal.load_asset(path.split(".")[0])


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


def spawn_mesh(mesh, label, x, y, z, yaw, folder, collision=True, profile=None, scale=None, material=None,
               affects_nav=True):
    mesh = unreal.load_asset(mesh) if isinstance(mesh, str) else mesh
    if mesh is None:
        raise RuntimeError("missing mesh for " + label)
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                                unreal.Rotator(0.0, 0.0, yaw))
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


def set_cpd(comp, values):
    for index, value in enumerate(values):
        comp.set_default_custom_primitive_data_float(index, float(value))


def instance_actor(label, folder, mesh, transforms, collision=False, material=None, cpd=None):
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
    if material is not None:
        comp.set_material(0, material)
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    if cpd is not None:
        set_cpd(comp, cpd)
    comp.add_instances(transforms, False, True)
    return actor


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


def on_dam(x, y, pad=0.0):
    for fid, d in L()["dams"].items():
        cx, cy = d["centre"]
        dx, dy = d["dir"]
        along = (x - cx) * dx + (y - cy) * dy
        across = -(x - cx) * dy + (y - cy) * dx
        if abs(across) <= d["half_width_cm"] + pad and abs(along) <= 4500.0:
            return fid
    return None


# ======================================================================================================== base
def step_lighting():
    """Ger_Berlin's ash-grey overcast daylight (berlin_map_build.step_lighting): no engine height fog."""
    W.require_map()
    report = []
    for actor in W.all_actors():
        name = actor.get_class().get_name()
        if name == "DirectionalLight":
            comp = actor.light_component
            comp.set_intensity(3.4)
            comp.set_light_color(unreal.LinearColor(0.93, 0.95, 1.0, 1.0))
            comp.set_editor_property("atmosphere_sun_light", True)
            actor.set_actor_rotation(unreal.Rotator(0.0, -48.0, -120.0), False)
            report.append("sun")
        elif name == "SkyLight":
            comp = actor.light_component
            comp.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
            comp.set_intensity(1.15)
            comp.set_light_color(unreal.LinearColor(0.88, 0.92, 1.0, 1.0))
            comp.recapture_sky()
            report.append("sky")
        elif name == "ExponentialHeightFog":
            actors_sub().destroy_actor(actor)
            report.append("fog removed")
        elif name == "PostProcessVolume":
            settings = actor.settings
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.86, 0.86, 0.86, 1.0))
            settings.set_editor_property("override_white_temp", True)
            settings.set_editor_property("white_temp", 6900.0)
            actor.set_editor_property("settings", settings)
            report.append("post")
    if not [a for a in W.all_actors() if a.get_class().get_name() == "SkyAtmosphere"]:
        sky = actors_sub().spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0))
        sky.set_actor_label("SkyAtmosphere")
        report.append("sky atmosphere added")
    log("lighting %s" % report)


def step_base():
    L()
    W.step_duplicate()
    W.step_strip()
    W.step_landscape()
    W.step_fit_level_actors()
    step_lighting()
    log("base done")


# ======================================================================================================== canal
def _edge_runs(offset):
    """Points along the canal axis offset to one bank (cm), cut where a dam crosses; returns lists of points."""
    axis = [tuple(p) for p in L()["canal"]["axis"]]
    total = polyline_s(axis)[-1]
    lim = L()["landscape"]["size_x"] / 2.0 - 200.0
    runs, cur = [], []
    s = 0.0
    while s <= total:
        a, b = point_at(axis, s), point_at(axis, min(total, s + 50.0))
        tx, ty = b[0] - a[0], b[1] - a[1]
        n = math.hypot(tx, ty) or 1.0
        nx, ny = ty / n, -tx / n
        p = (a[0] + nx * offset, a[1] + ny * offset)
        inside = abs(p[0]) < lim and abs(p[1]) < lim
        if inside and not on_dam(p[0], p[1], pad=60.0):
            cur.append(p)
        elif cur:
            runs.append(cur)
            cur = []
        s += 100.0
    if cur:
        runs.append(cur)
    return [r for r in runs if len(r) > 2]


def step_canal():
    """Water planes, DBK concrete quay walls on both banks, the three crossings: concrete deck slabs (no collision, the
    earth dam under them is the ground), low concrete parapets (block), concrete facings on the dam sides, and a tank
    blocker on the lock footbridge (infantry only)."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Canal_")
    c = L()["canal"]
    axis = [tuple(p) for p in c["axis"]]
    lim = L()["landscape"]["size_x"] / 2.0 - 300.0
    total = polyline_s(axis)[-1]
    planes, s = 0, 0.0
    while s < total:
        a, b = point_at(axis, s), point_at(axis, min(total, s + 2500.0))
        s += 2500.0
        cx, cy = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0
        if abs(cx) > lim or abs(cy) > lim:
            continue
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        spawn_mesh("/Engine/BasicShapes/Plane", "Canal_Water_%02d" % planes, cx, cy, c["water_z"], yaw, "Canal",
                   collision=False, scale=((seg + 200.0) / 100.0, (2 * c["half_cm"] + 40.0) / 100.0, 1.0),
                   material=WATER_MI)
        planes += 1
    # quay walls: DBK_Concete_Wall_Full (300 along X, 20 deep, 300 high), top 15 cm above the bank
    wall = kit_mesh("DBK_Concete_Wall_Full")
    walls = []
    for side in (-1, 1):
        for run in _edge_runs(side * (c["half_cm"] + 10.0)):
            s_run = polyline_s(run)
            k = 0.0
            while k + 300.0 <= s_run[-1]:
                a, b = point_at(run, k), point_at(run, k + 300.0)
                yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
                walls.append(unreal.Transform(unreal.Vector(a[0], a[1], 15.0 - 300.0), unreal.Rotator(0.0, 0.0, yaw),
                                              unreal.Vector(1.0, 1.0, 1.0)))
                k += 298.0
    instance_actor(PREFIX + "Canal_QuayWalls", FOLDER + "Canal", wall, walls,
                   material=unreal.load_asset(RUIN_KIT + "MI_Berlin_Ruin_Concrete"))
    # crossings
    floor = kit_mesh("DBK_Concete_Floor_Full")
    top = kit_mesh("DBK_Concrete_Wall_Top")
    concrete = unreal.load_asset(RUIN_KIT + "MI_Berlin_Ruin_Concrete")
    deck_t, para_t, face_t = [], [], []
    for fid, d in L()["dams"].items():
        cx, cy = d["centre"]
        dx, dy = d["dir"]
        yaw = math.degrees(math.atan2(dy, dx))
        half_w = d["half_width_cm"]

        def at(lx, ly):
            return cx + lx * dx - ly * dy, cy + lx * dy + ly * dx

        def span(ly):
            """Along-range of the canal (water) under one line of the dam: a skewed crossing reaches far along."""
            inside = [a for a in range(-5000, 5001, 25) if W.dist_polyline(*at(a, ly), axis) < c["half_cm"]]
            return (min(inside), max(inside)) if inside else (-c["half_cm"], c["half_cm"])

        n_wide = max(1, int(math.ceil((2 * half_w - 40.0) / 300.0)))
        sx = (2 * half_w - 40.0) / (n_wide * 300.0)
        for j in range(n_wide):
            ly = -(half_w - 20.0) + j * 300.0 * sx
            lo = min(span(ly)[0], span(ly + 300.0 * sx)[0]) - 300.0      # the deck runs 3 m onto both banks
            hi = max(span(ly)[1], span(ly + 300.0 * sx)[1]) + 300.0
            lx = lo
            while lx < hi:
                px, py = at(lx, ly)
                deck_t.append(unreal.Transform(unreal.Vector(px, py, -14.0), unreal.Rotator(0.0, 0.0, yaw),
                                               unreal.Vector(1.0, sx, 0.9)))
                lx += 300.0
        for side in (-1, 1):
            ly = side * (half_w - 10.0)
            lo, hi = span(side * half_w)
            lx = lo - 300.0
            while lx < hi + 300.0:
                px, py = at(lx, ly)
                # DBK_Concrete_Wall_Top spans local Y -20..0 and is 60 cm high (x1.4 = 84 cm)
                para_t.append(unreal.Transform(unreal.Vector(px, py, 0.0), unreal.Rotator(0.0, 0.0, yaw),
                                               unreal.Vector(1.0, 1.0, 1.4)))
                lx += 300.0
            # facing walls on the dam sides, from bank to bank, down into the water
            ly2 = side * (half_w + 10.0)
            lo, hi = span(ly2)
            lx = lo - 100.0
            while lx < hi + 100.0:
                px, py = at(lx, ly2)
                face_t.append(unreal.Transform(unreal.Vector(px, py, -300.0), unreal.Rotator(0.0, 0.0, yaw),
                                               unreal.Vector(1.0, 1.0, 1.0)))
                lx += 298.0
    instance_actor(PREFIX + "Canal_Decks", FOLDER + "Canal", floor, deck_t, material=concrete)
    instance_actor(PREFIX + "Canal_Parapets", FOLDER + "Canal", top, para_t, collision=True, material=concrete)
    instance_actor(PREFIX + "Canal_DamFacings", FOLDER + "Canal", wall, face_t, material=concrete)
    # lock footbridge: infantry only
    d = L()["dams"]["weir"]
    blocker = unreal.EditorAssetLibrary.load_blueprint_class(TANK_BLOCKER)
    actor = actors_sub().spawn_actor_from_class(blocker, unreal.Vector(d["centre"][0], d["centre"][1], 100.0),
                                                unreal.Rotator(0.0, 0.0, math.degrees(math.atan2(d["dir"][1], d["dir"][0]))))
    actor.set_actor_scale3d(unreal.Vector((2 * c["half_cm"] + 1200.0) / 64.0, (2 * d["half_width_cm"] + 200.0) / 64.0, 8.0))
    actor.set_actor_label(PREFIX + "Canal_LockTankBlocker")
    actor.set_folder_path(FOLDER + "Nav")
    log("canal: removed %d, %d water planes, %d quay wall modules, %d deck tiles, %d parapet modules, %d facings" % (
        removed, planes, len(walls), len(deck_t), len(para_t), len(face_t)))


def step_rails():
    """The freight line: track panels (scenery, no collision) from edge to edge, over the rail bridge deck."""
    W.require_map()
    removed = W.clear_prefix(PREFIX + "Rails")
    mesh = unreal.load_asset(RAIL_MESH)
    box = mesh.get_bounding_box()
    size = (box.max.x - box.min.x, box.max.y - box.min.y)
    along_x = size[0] >= size[1]
    length = max(size)
    centre = ((box.max.x + box.min.x) / 2.0, (box.max.y + box.min.y) / 2.0)
    lim = L()["landscape"]["size_x"] / 2.0 - 300.0
    pts = [tuple(p) for p in L()["rail_line"]]
    total = polyline_s(pts)[-1]
    transforms, s = [], 0.0
    while s < total:
        a, b = point_at(pts, s), point_at(pts, min(total, s + length))
        s += length * 0.985
        tx, ty = b[0] - a[0], b[1] - a[1]
        n = math.hypot(tx, ty) or 1.0
        mid = (a[0] + tx / 2.0, a[1] + ty / 2.0)
        if abs(mid[0]) > lim or abs(mid[1]) > lim:
            continue
        yaw = math.degrees(math.atan2(ty, tx)) + (0.0 if along_x else -90.0)
        rad = math.radians(yaw)
        ox = centre[0] * math.cos(rad) - centre[1] * math.sin(rad)
        oy = centre[0] * math.sin(rad) + centre[1] * math.cos(rad)
        transforms.append(unreal.Transform(unreal.Vector(mid[0] - ox, mid[1] - oy, gz(mid[0], mid[1]) - 35.0),
                                           unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0)))
    instance_actor(PREFIX + "Rails", FOLDER + "Rail", mesh, transforms)
    log("rails: removed %d, %d panels" % (removed, len(transforms)))


# ======================================================================================================== ruins + dressing
def _road_runout():
    """The main road's straight run-out past both play edges (the landscape keeps the road going to its edge)."""
    road = [tuple(p) for p in W.path("main_road")["points"]]
    out = []
    for a, b in ((road[0], road[1]), (road[-1], road[-2])):
        ux, uy = a[0] - b[0], a[1] - b[1]
        n = math.hypot(ux, uy)
        out.append([a, (a[0] + ux / n * 12000.0, a[1] + uy / n * 12000.0)])
    return out


def _on_runout(x, y, clear=2200.0):
    return any(W.dist_polyline(x, y, seg) < clear for seg in _road_runout())


def step_ruins():
    """Play-area ruins: one destructible BP_BerlinRuin_<piece> per entry (shell holes, health, death), tint + soot in
    custom primitive data. Backdrop ruins in the camera margin: instanced plain meshes, no collision."""
    W.require_map()
    ruins = json.load(open(DRESS_FILE))["ruins"]
    removed = W.clear_prefix(PREFIX + "Ruin_")
    classes, counts, backdrop = {}, {}, {}
    for index, e in enumerate(ruins):
        piece = e["piece"]
        if e["kind"] == "backdrop":
            if not _on_runout(e["x"], e["y"]):
                backdrop.setdefault(piece, []).append(e)
            continue
        if piece not in classes:
            classes[piece] = unreal.EditorAssetLibrary.load_blueprint_class(RUIN_KIT + "Blueprints/BP_BerlinRuin_" + piece)
        actor = actors_sub().spawn_actor_from_class(classes[piece], unreal.Vector(e["x"], e["y"], e["z"]),
                                                    unreal.Rotator(0.0, 0.0, e["yaw"]))
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        if comp is not None:
            t = e["tint"]
            comp.set_default_custom_primitive_data_vector4(0, unreal.Vector4(t[0], t[1], t[2], e["soot"]))
        actor.set_actor_label("%sRuin_%s_%04d" % (PREFIX, piece, index))
        actor.set_folder_path(FOLDER + "Ruins/" + e["kind"])
        counts[piece] = counts.get(piece, 0) + 1
    for piece, items in backdrop.items():
        mesh = unreal.load_asset(RUIN_KIT + "Meshes/SM_BerlinRuin_" + piece)
        transforms = [unreal.Transform(unreal.Vector(e["x"], e["y"], e["z"]), unreal.Rotator(0.0, 0.0, e["yaw"]),
                                       unreal.Vector(1.0, 1.0, 1.0)) for e in items]
        instance_actor("%sRuin_Backdrop_%s" % (PREFIX, piece), FOLDER + "Ruins/Backdrop", mesh, transforms,
                       cpd=(0.9, 0.9, 0.88, 1.1))
    log("ruins: removed %d, actors %s, backdrop instances %d" % (removed, counts, sum(len(v) for v in backdrop.values())))


def step_feet():
    """The kit's footprint skirts under every ruin (instanced, no collision, no nav effect)."""
    W.require_map()
    feet = json.load(open(DRESS_FILE))["feet"]
    removed = W.clear_prefix(PREFIX + "Foot_")
    material = unreal.load_asset(RUIN_KIT + "Materials/MI_BerlinRuin_Rubble_Foot")
    groups = {}
    b = L()["bounds"]
    for mesh, x, y, z, yaw, scale, bucket in feet:
        if (abs(x) > b["xmax"] or abs(y) > b["ymax"]) and _on_runout(x, y):
            continue
        groups.setdefault((mesh, bucket), []).append(
            unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(scale, scale, scale)))
    missing = set()
    for (name, bucket), items in sorted(groups.items()):
        mesh = unreal.load_asset(RUIN_KIT + "Footprints/" + name)
        if mesh is None:
            missing.add(name)
            continue
        instance_actor("%sFoot_%s_%d" % (PREFIX, name.replace("SM_BerlinFoot_", ""), bucket), FOLDER + "Ruins/Footprints",
                       mesh, items, material=material, cpd=FOOT_TINTS[bucket])
    log("feet: removed %d, placed %d in %d components, missing %s" % (removed, len(feet), len(groups), sorted(missing)))


def step_actors():
    """Heap cover, ambush cover pieces, dead trees, wrecks, quay crates and crane from the dress file."""
    W.require_map()
    entries = json.load(open(DRESS_FILE))["actors"]
    removed = W.clear_prefix(PREFIX + "Dress_")
    classes, missing, placed = {}, set(), 0
    for e in entries:
        bp = e["bp"]
        if bp not in classes:
            classes[bp] = unreal.EditorAssetLibrary.load_blueprint_class(bp) \
                if unreal.EditorAssetLibrary.does_asset_exist(bp) else None
        if classes[bp] is None:
            missing.add(bp)
            continue
        actor = actors_sub().spawn_actor_from_class(classes[bp], unreal.Vector(e["x"], e["y"], e["z"]),
                                                    unreal.Rotator(0.0, 0.0, e["yaw"]))
        s = e["scale"]
        if s != [1.0, 1.0, 1.0]:
            actor.set_actor_scale3d(unreal.Vector(s[0], s[1], s[2]))
        if "tint" in e:
            comp = actor.get_component_by_class(unreal.StaticMeshComponent)
            if comp is not None:
                set_cpd(comp, RUBBLE_TINTS[e["tint"]])
        actor.set_actor_label(PREFIX + "Dress_" + e["label"])
        actor.set_folder_path(FOLDER + e["folder"])
        placed += 1
    log("actors: removed %d, placed %d, missing %s" % (removed, placed, sorted(missing)))


def step_decals():
    W.require_map()
    decals = json.load(open(DRESS_FILE))["decals"]
    removed = W.clear_prefix(PREFIX + "Decal_")
    materials, missing = {}, set()
    for index, e in enumerate(decals):
        path = e["mat"]
        if path not in materials:
            materials[path] = unreal.load_asset(path)
        if materials[path] is None:
            missing.add(path)
            continue
        actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(e["x"], e["y"], e["z"] + 20.0),
                                                    unreal.Rotator(0.0, -90.0, e["yaw"]))
        comp = actor.decal
        comp.set_decal_material(materials[path])
        half = e["size"] / 2.0
        comp.set_editor_property("decal_size", unreal.Vector(160.0, half, half))
        comp.set_fade_screen_size(0.004)
        actor.set_actor_label("%sDecal_%s_%03d" % (PREFIX, e["kind"], index))
        actor.set_folder_path(FOLDER + "Decals")
    log("decals: removed %d, placed %d, missing %s" % (removed, len(decals) - len(missing), sorted(missing)))


def step_props():
    """Freight yard (the enemy start), the flak battery's ammo bunker (branch destination), the radio post on the north
    bank, branch destination marker."""
    W.require_map()
    removed = sum(W.clear_prefix(PREFIX + p) for p in ("Yard_", "Battery_", "Radio_", "Marker_"))
    rng = random.Random(5)
    crates = DEST + "Airport/BP_Airport_Dest_CrateStack01"
    fuel = BX + "RusBuildings/FuelStorage/BP_Dest_RusFuel1"
    barrels = BX + "GerBxps/Bunkers/BreakthroughShared/FieldSupply/SM_BF_Barrel_3_Oil"
    dx_, dy_ = xy("director")
    for k, (lx, ly, yaw) in enumerate(((1800.0, -2600.0, 0.0), (2400.0, -1800.0, 30.0), (-1800.0, -2800.0, 80.0),
                                       (-2600.0, 1500.0, 10.0), (2000.0, 2400.0, 60.0))):
        spawn_bp(crates, "Yard_Crates_%d" % k, dx_ + lx, dy_ + ly, yaw, "FreightYard")
    spawn_bp(DEST + "Soviet/BP_Dest_SovietCrane_1", "Yard_Crane", dx_ - 3000.0, dy_ + 2000.0, 90.0, "FreightYard")
    spawn_bp(fuel, "Yard_Fuel", dx_ + 2800.0, dy_ + 600.0, 0.0, "FreightYard")
    for k in range(4):
        x, y = dx_ - 1500.0 + rng.uniform(-400, 400), dy_ + 2600.0 + rng.uniform(-400, 400)
        spawn_mesh(barrels, "Yard_Barrels_%d" % k, x, y, gz(x, y), rng.uniform(0, 360), "FreightYard")
    # the battery's ammo bunker at the hill foot: crates, a fuel tank, barrels round the destination
    bx_, by_ = xy("battery_dest")
    for k, (lx, ly, yaw) in enumerate(((900.0, 700.0, 20.0), (-700.0, 900.0, 70.0), (1100.0, -600.0, 10.0))):
        spawn_bp(crates, "Battery_Crates_%d" % k, bx_ + lx, by_ + ly, yaw, "FlakBattery")
    spawn_bp(fuel, "Battery_Fuel", bx_ - 900.0, by_ - 900.0, 45.0, "FlakBattery")
    for k in range(3):
        x, y = bx_ + rng.uniform(-600, 600), by_ + 1400.0 + rng.uniform(-200, 200)
        spawn_mesh(barrels, "Battery_Barrels_%d" % k, x, y, gz(x, y), rng.uniform(0, 360), "FlakBattery")
    marker = actors_sub().spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(bx_, by_, gz(bx_, by_) + 50.0))
    marker.set_actor_label(PREFIX + "Marker_BranchDest")
    marker.set_folder_path(FOLDER + "Convoy")
    # radio post (the Ore radio hut is a Rus container hut child, owner 2) + antenna
    rx, ry = xy("radio_post")
    spawn_bp(ORE + "Sites/RadioOutpost/BP_OreRadio_Funkbude", "Radio_Funkbude", rx, ry, 200.0, "RadioPost", owner=ENEMY)
    spawn_mesh("/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna2", "Radio_Antenna", rx + 350.0, ry - 250.0,
               gz(rx + 350.0, ry - 250.0), 0.0, "RadioPost", collision=False, scale=(1.3, 1.3, 1.3))
    log("props: removed %d; freight yard, battery bunker, radio post, marker placed" % removed)


def step_dress():
    for step in (step_canal, step_rails, step_ruins, step_feet, step_actors, step_decals, step_props):
        step()


# ======================================================================================================== convoy
def _struct_copy(value):
    out = type(value)()
    out.import_text(value.export_text())
    return out


def step_definition():
    """DA_Convoy_Field_BerlinKanal in the map's data folder: DA_Convoy_VehicleParts (Zil ammo trucks, calibrated
    1372 cm/s -> 686 cm/s pace) with an Ammo-cargo truck at column positions 2 and 6, so 3-8 truck convoys always carry
    1-2 trucks for the flak battery branch."""
    if not unreal.EditorAssetLibrary.does_asset_exist(DEFINITION):
        unreal.EditorAssetLibrary.make_directory(DATA)
        if unreal.EditorAssetLibrary.duplicate_asset(SRC_DEFINITION, DEFINITION) is None:
            raise RuntimeError("duplicate_asset failed for the definition")
    da = unreal.load_asset(DEFINITION)
    trucks = list(da.get_editor_property("trucks"))
    if not trucks:
        raise RuntimeError("source definition has no trucks")
    base = _struct_copy(trucks[0])
    base.set_editor_property("count", 1)
    base.set_editor_property("override_cargo_type", False)
    ammo = _struct_copy(base)
    ammo.set_editor_property("override_cargo_type", True)
    ammo.set_editor_property("cargo_type_override", unreal.ConvoyCargoType.AMMO)
    column = [base, ammo, _struct_copy(base), _struct_copy(base)]
    da.set_editor_property("trucks", column)
    unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    log("definition cargo_type %s" % da.get_editor_property("cargo").get_editor_property("cargo_type"))
    log("definition %s: trucks %s, calibrated %s" % (
        DEFINITION, [(str(t.get_editor_property("training_option")), str(t.get_editor_property("override_cargo_type")))
                     for t in column], [t.get_editor_property("calibrated_cruise_speed_cms") for t in column]))
    return da


def _vec(p, lift=50.0):
    return unreal.Vector(p[0], p[1], gz(p[0], p[1]) + lift)


def _rel(origin, p, lift=50.0):
    v = _vec(p, lift)
    return unreal.Vector(v.x - origin.x, v.y - origin.y, v.z - origin.z)


def route_points():
    main = [tuple(p) for p in W.path("main_road")["points"]]
    s0, s1 = project_s(main, xy("route_entry")), project_s(main, xy("route_exit"))
    pts = sub_line(main, s0, s1, 2500.0)
    # both ends of the road bridge are control points: the drive path must take the bridge
    d = L()["dams"]["road_bridge"]
    half = L()["canal"]["half_cm"] + 600.0
    a = (d["centre"][0] - d["dir"][0] * half, d["centre"][1] - d["dir"][1] * half)
    b = (d["centre"][0] + d["dir"][0] * half, d["centre"][1] + d["dir"][1] * half)
    sa, sb = project_s(main, a), project_s(main, b)
    out = [p for p in pts if not (min(sa, sb) - 300.0 < project_s(main, p) < max(sa, sb) + 300.0)]
    out += [point_at(main, min(sa, sb)), tuple(d["centre"]), point_at(main, max(sa, sb))]
    out.sort(key=lambda p: project_s(main, p))
    return out


def step_route():
    """AEnemyConvoyRoute: entry 5 m inside the N edge, exit 5 m inside the S edge; ammo branch to the flak battery,
    two ambush zones with the radio post. No stop, no hunters (concept B)."""
    W.require_map()
    da = unreal.load_asset(DEFINITION)
    removed = W.clear_prefix(PREFIX + "Convoy_Route")
    pts = route_points()
    origin = _vec(pts[0])
    route = actors_sub().spawn_actor_from_class(unreal.EnemyConvoyRoute, origin, unreal.Rotator(0.0, 0.0, 0.0))
    route.set_actor_label(PREFIX + "Convoy_Route")
    route.set_folder_path(FOLDER + "Convoy")
    route.set_editor_property("route_id", "BKU_Convoy")
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
    branch = route.get_editor_property("branch")
    branch.set_editor_property("enabled", True)
    branch.set_editor_property("cargo_type", unreal.ConvoyCargoType.AMMO)
    branch.set_editor_property("destination_actor", labels[PREFIX + "Marker_BranchDest"])
    route.set_editor_property("branch", branch)
    bpts = [tuple(p) for p in W.path("branch_road")["points"]]
    branch_pts = [bpts[0]] + [tuple(p) for p in W._resample(bpts, 3000.0)[1:-1]] + [tuple(xy("battery_dest"))]
    route.set_editor_property("branch_points", [_rel(origin, p) for p in branch_pts])
    ambush = route.get_editor_property("ambush")
    ambush.set_editor_property("radio_post_actor", labels[PREFIX + "Radio_Funkbude"])
    route.set_editor_property("ambush", ambush)
    zones = [feat(z)["points"][0] for z in ("zone_1", "zone_2")]
    route.set_editor_property("ambush_zone_points", [_rel(origin, p) for p in zones])
    log("route: removed %d, %d route points, %d branch points" % (removed, len(pts), len(branch_pts)))
    return route


STRENGTH = {
    # feature id: (slot id, defence Blueprint, fortification, min total strength %)
    "slot_1": ("Bunker_BridgeheadN", RK + "Rus_BXP_MGBunker", "BUNKERS", 0),
    "slot_2": ("AA_FlakHill", RK + "Rus_BXP_Bofors", "AA_BATTERY", 0),
    "slot_3": ("Mines_SouthQuay", "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Mines/Rus_AnitTank_LightMineField",
               "MINEFIELDS", 0),
    "slot_4": ("Garrison_Warehouse", RK + "Rus_BXP_DomeBunkerGarrison", "NONE", 50),
    "slot_5": ("ATGun_RailBridge", RK + "Rus_BXP_Bunker02_ZIS", "NONE", 25),
}
# the drawing's slot 5 sits on the freight line: 12.5 m west of it instead
SLOT_AT = {"slot_5": (7000.0, -28200.0)}


def _yaw_to(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def step_director():
    """AEnemyItemSiteDirector at the freight yard (convoy battle on SovietSupplyConvoy; TestContext lets the owner press
    Play on the map), five strength slots, UDestroyEnemyItemSiteMission."""
    W.require_map()
    removed = sum(W.clear_prefix(PREFIX + p) for p in ("Site_", "Strength_"))
    labels = W.by_label()
    route = labels[PREFIX + "Convoy_Route"]
    dx, dy = xy("director")
    director = actors_sub().spawn_actor_from_class(
        unreal.EnemyItemSiteDirector, unreal.Vector(dx, dy, gz(dx, dy) + 50.0),
        unreal.Rotator(0.0, 0.0, _yaw_to((dx, dy), xy("hq"))))
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
        x, y = SLOT_AT.get(fid, xy(fid))
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
    """Blocking nav build, then UConvoyRouteLibrary drive paths for the main route and the ammo branch."""
    W.require_map()
    world = W.world()
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    route = W.by_label()[PREFIX + "Convoy_Route"]
    origin = route.get_actor_location()

    def world_pts(prop):
        return [unreal.Vector(origin.x + p.x, origin.y + p.y, origin.z + p.z) for p in route.get_editor_property(prop)]

    problems = []
    for name, pts in (("main", world_pts("route_points")), ("branch", world_pts("branch_points"))):
        built, reached = unreal.ConvoyRouteLibrary.build_navigable_route(route, pts, 800.0, "BKU_" + name)
        bpts = built.get_editor_property("points")
        count = len(bpts)
        length = unreal.ConvoyRouteLibrary.get_route_length(built) if count > 1 else 0.0
        straight = sum(math.hypot(b.x - a.x, b.y - a.y) for a, b in zip(pts, pts[1:]))
        on_bridge = sum(1 for p in bpts if on_dam(p.x, p.y) == "road_bridge")
        log("drive path %s: reached=%s points=%d length=%.0f m (control %.0f m), points on the road bridge %d"
            % (name, reached, count, length / 100.0, straight / 100.0, on_bridge))
        if not reached or count < 2:
            problems.append(name + " does not reach its end")
        elif length > straight * 1.2 + 3000.0:
            problems.append("%s detours: %.0f m for %.0f m" % (name, length / 100.0, straight / 100.0))
        if name == "main" and on_bridge == 0:
            problems.append("main drive path never touches the road bridge")
    log("route problems: %s" % (problems or "none"))
    return problems
