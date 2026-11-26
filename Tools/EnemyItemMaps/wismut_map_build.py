"""In-editor build of EnemyItem_WismutPlateau (enemy item map 1, Ore Mountains area) from wismut_terrain_gen.py output.

Runs inside a throwaway full editor driven by item_editor_server.py / item_cmd.py:

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/EnemyItemMaps")
    import wismut_map_build as wmb; importlib.reload(wmb)
    wmb.load_layout(); wmb.step_duplicate(); wmb.step_strip(); wmb.step_landscape(); wmb.step_fit_level_actors()
    wmb.step_lighting(); wmb.step_resources(); wmb.step_trees(); wmb.step_roads(); wmb.step_site(); wmb.step_mission()

Every generated actor carries the WP_ label prefix and sits under the outliner folder WismutPlateau/, so a step can
clear and redo its own part. Positions are Unreal cm from the map centre, +X north, +Y east (MapDesigner frame).
"""
import array
import json
import math
import random

import unreal

TEMPLATE_MAP = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap"
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/EnemyItem_WismutPlateau"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/EnemyItemMaps/WismutPlateau/terrain/WP_layout.json"
SHOTS = "D:/UE5Projects/RTS_Survival/Saved/EnemyItemMaps/WismutPlateau/shots/"
LS_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/"
# the Ger_2ndOreMountains landscape instance: same shader, same tuning (owner: "the same landscape shader")
LANDSCAPE_MATERIAL = LS_ROOT + "MI_RTS_Landscape_DryCastle_OreMountains"
LAYER_INFOS = {
    "Desert": LS_ROOT + "DATA/Desert_LayerInfo",
    "GrassCastle": LS_ROOT + "DATA/GrassCastle_LayerInfo",
    "RockDesert": LS_ROOT + "DATA/RockDesert_LayerInfo",
    "GraySand": LS_ROOT + "DATA/GraySand_LayerInfo",
    "RTSSand": "/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35_sharedassets/RTSSand_LayerInfo",
    "MountainTop": "/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35_sharedassets/MountainTop_LayerInfo",
    "Asphalt": "/Game/MilitaryAirport/Landscape/LandscapeMaterial/Asphalt_LayerInfo",
    "DarkEarth": LS_ROOT + "DATA/DarkEarth_LayerInfo",
    "IronOxide": LS_ROOT + "DATA/IronOxide_LayerInfo",
    "Ochre": LS_ROOT + "DATA/Ochre_LayerInfo",
    "SlateScree": LS_ROOT + "DATA/SlateScree_LayerInfo",
}
LANDSCAPE_LABEL = "WP_Landscape"
RVT_ROOT = "/Game/RTS_Survival/Meshes/RVTs/"
PREFIX = "WP_"
FOLDER = "WismutPlateau/"
Z_LOW, Z_HIGH = -1000.0, 6500.0
NAV_LOW, NAV_HIGH = -900.0, 3800.0
ENEMY = 2

LAYOUT = {}
HEIGHTS = None


def log(text):
    unreal.log_warning("WP_MAP " + str(text))
    print(text)


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def all_actors():
    return actors_sub().get_all_level_actors()


def require_map():
    path = world().get_path_name()
    if not path.split(".")[0] == MAP_PATH:
        raise RuntimeError("open level is %s, expected %s" % (path, MAP_PATH))


def load_layout():
    global LAYOUT, HEIGHTS
    with open(LAYOUT_FILE, "r") as handle:
        LAYOUT = json.load(handle)
    HEIGHTS = None
    return LAYOUT


def feature(fid):
    return next(f for f in LAYOUT["features"] if f["id"] == fid)


def region(rid):
    return next(r for r in LAYOUT["regions"] if r["id"] == rid)


def path(pid):
    return next(p for p in LAYOUT["paths"] if p["id"] == pid)


def by_label():
    return {actor.get_actor_label(): actor for actor in all_actors()}


def save_level():
    require_map()
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([world().get_outermost()], False)
    log("save_packages %s" % ok)
    return ok


def clear_prefix(prefix):
    count = 0
    for actor in all_actors():
        if actor.get_actor_label().startswith(prefix):
            actors_sub().destroy_actor(actor)
            count += 1
    return count


def ground_z(x, y):
    """Landscape height from the generator file (bilinear): placed actors can never get in the way."""
    global HEIGHTS
    lay = LAYOUT["landscape"]
    if HEIGHTS is None:
        with open(LAYOUT["files"]["height"], "rb") as handle:
            HEIGHTS = array.array("H", handle.read())
    vx, vy = lay["verts_x"], lay["verts_y"]
    fx = min(max((x - lay["corner"][0]) / lay["quad_cm"], 0.0), vx - 1.001)
    fy = min(max((y - lay["corner"][1]) / lay["quad_cm"], 0.0), vy - 1.001)
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    tx, ty = fx - ix, fy - iy

    def h(i, j):
        return (HEIGHTS[j * vx + i] - 32768.0) * 100.0 / 128.0

    return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) +
            h(ix, iy + 1) * (1 - tx) * ty + h(ix + 1, iy + 1) * tx * ty)


def nav_ready():
    """True once no navmesh build is running (use as a _defer before saving)."""
    return not unreal.NavigationSystemV1.get_navigation_system(world()).is_navigation_being_built(world())


def point_in_poly(x, y, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            inside = not inside
        j = i
    return inside


def dist_polyline(x, y, pts):
    best = 1e12
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy + 1e-9)))
        best = min(best, math.hypot(x - ax - t * dx, y - ay - t * dy))
    return best


# ---- map file ---------------------------------------------------------------------------------------------------
def step_duplicate():
    """Copy the template map (RTS_ToCopyMap) to the new path (once) and open it."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.make_directory(MAP_PATH.rsplit("/", 1)[0])
        if unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE_MAP, MAP_PATH) is None:
            raise RuntimeError("duplicate_asset failed")
        unreal.EditorAssetLibrary.save_asset(MAP_PATH, False)
        log("duplicated %s -> %s" % (TEMPLATE_MAP, MAP_PATH))
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    require_map()
    return world().get_path_name()


def step_strip():
    """Remove the template's test units; keep the game actors, lighting and the old landscape (replaced next)."""
    require_map()
    keep = ("RTS_BP_EnemyController_C", "BP_FogOfWar_Manager_C", "RTS_PLayerStart_C", "PlayerStart",
            "NavMeshBoundsVolume", "RecastNavMesh", "BP_MissionManager_C", "WorldSettings", "BlockingVolume",
            "AbstractNavData", "WorldDataLayers", "DirectionalLight", "SkyLight", "ExponentialHeightFog",
            "PostProcessVolume", "SkyAtmosphere", "VolumetricCloud", "Landscape", "LandscapeGizmoActiveActor",
            "DefaultPhysicsVolume", "GameplayDebuggerPlayerManager", "Brush")
    kept, removed = [], []
    for actor in all_actors():
        name = actor.get_class().get_name()
        if name in keep:
            kept.append("%s(%s)" % (actor.get_actor_label(), name))
            continue
        removed.append("%s(%s)" % (actor.get_actor_label(), name))
        actors_sub().destroy_actor(actor)
    log("kept %s" % kept)
    log("removed %s" % removed)
    return removed


def step_landscape():
    require_map()
    lay = LAYOUT["landscape"]
    files = LAYOUT["files"]
    old = [a for a in all_actors() if isinstance(a, unreal.Landscape)]
    body_instance = None
    phys_material = unreal.load_asset("/Game/RTS_Survival/PhysicalMaterials/PM_Sand")
    for landscape in old:
        if body_instance is None:
            # Units only drive on ground that blocks the Landscape trace channel: keep the template's profile.
            body_instance = landscape.get_editor_property("body_instance")
            phys_material = landscape.get_editor_property("default_phys_material") or phys_material
        actors_sub().destroy_actor(landscape)
    names = list(LAYOUT["layers"])
    infos = [unreal.load_asset(LAYER_INFOS[name]) for name in names]
    missing = [n for n, info in zip(names, infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s" % missing)
    material = unreal.load_asset(LANDSCAPE_MATERIAL)
    if material is None:
        raise RuntimeError("missing " + LANDSCAPE_MATERIAL)
    corner = lay["corner"]
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        world(), unreal.Vector(corner[0], corner[1], 0.0), unreal.Vector(lay["quad_cm"], lay["quad_cm"], 100.0),
        lay["components_x"], lay["components_y"], lay["quads_per_component"], material, files["height"], infos,
        [files["layers"][n] for n in names], LANDSCAPE_LABEL)
    if landscape is None:
        raise RuntimeError("create_painted_landscape failed (see LogRTSShortsLandscape)")
    if body_instance is not None:
        landscape.set_editor_property("body_instance", body_instance)
    if phys_material is not None:
        landscape.set_editor_property("default_phys_material", phys_material)
    landscape.set_editor_property("runtime_virtual_textures", [unreal.load_asset(RVT_ROOT + "RVT_Environment"),
                                                               unreal.load_asset(RVT_ROOT + "RVT_Height")])
    landscape.set_editor_property("virtual_texture_render_pass_type", unreal.RuntimeVirtualTextureMainPassType.ALWAYS)
    landscape.set_actor_label(LANDSCAPE_LABEL)
    landscape.set_folder_path(FOLDER + "Terrain")
    log("landscape created; layers %s" % [str(n) for n in landscape.get_target_layer_names()])
    return landscape


def step_reimport_heights():
    require_map()
    landscape = [a for a in all_actors() if isinstance(a, unreal.Landscape)][0]
    return unreal.RTSShortsLandscapeLibrary.import_stage_heights(landscape, LAYOUT["files"]["height"])


def _set_box(actor, cx, cy, cz, ex, ey, ez):
    """Scale a brush volume to the wanted world half extents, axis aligned."""
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    origin, extent = actor.get_actor_bounds(False)
    scale = actor.get_actor_scale3d()
    actor.set_actor_scale3d(unreal.Vector(scale.x * ex / max(extent.x, 1.0), scale.y * ey / max(extent.y, 1.0),
                                          scale.z * ez / max(extent.z, 1.0)))
    actor.set_actor_location(unreal.Vector(cx, cy, cz), False, False)


def step_fit_level_actors():
    """Fog of war, nav bounds, RVT volumes, boundary blockers and the start actors onto the new play area."""
    require_map()
    lay = LAYOUT["landscape"]
    b = LAYOUT["bounds"]
    half_x, half_y = (b["xmax"] - b["xmin"]) / 2.0, (b["ymax"] - b["ymin"]) / 2.0
    start = feature("hq")["points"][0]
    start_z = ground_z(start[0], start[1])
    actors = by_label()
    landscape_actor = actors.get(LANDSCAPE_LABEL)
    blockers = []
    report = {}
    for actor in actors.values():
        name = actor.get_class().get_name()
        if name == "BP_FogOfWar_Manager_C":
            actor.set_actor_location(unreal.Vector(0.0, 0.0, 100.0), False, False)
            actor.set_editor_property("map_extent", max(half_x, half_y))
            report["fow_extent"] = max(half_x, half_y)
        elif name == "NavMeshBoundsVolume":
            _set_box(actor, 0.0, 0.0, (NAV_HIGH + NAV_LOW) / 2.0, half_x + 200.0, half_y + 200.0, (NAV_HIGH - NAV_LOW) / 2.0)
            unreal.NavigationSystemV1.get_navigation_system(world()).on_navigation_bounds_updated(actor)
        elif name == "BlockingVolume":
            blockers.append(actor)
        elif name == "RTS_PLayerStart_C":
            actor.set_actor_location(unreal.Vector(start[0], start[1], start_z + 43.0), False, False)
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 45.0), False)      # facing north-east, at the base
        elif name == "PlayerStart":
            actor.set_actor_location(unreal.Vector(start[0], start[1], start_z + 100.0), False, False)
        elif name in ("RTS_BP_EnemyController_C", "BP_MissionManager_C"):
            offset = 300.0 if name == "BP_MissionManager_C" else -300.0
            actor.set_actor_location(unreal.Vector(start[0] - 2500.0, start[1] + offset, start_z + 1500.0), False, False)
        elif name in ("DirectionalLight", "SkyLight", "ExponentialHeightFog", "PostProcessVolume"):
            actor.set_actor_location(unreal.Vector(0.0, 0.0, 6000.0), False, False)
    thick = 600.0
    spots = [("BoundaryTop", half_x + thick, 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryBottom", -(half_x + thick), 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryRight", 0.0, half_y + thick, half_x + 2.0 * thick, thick),
             ("BoundaryLeft", 0.0, -(half_y + thick), half_x + 2.0 * thick, thick)]
    for actor, (label, x, y, ex, ey) in zip(sorted(blockers, key=lambda a: a.get_actor_label()), spots):
        _set_box(actor, x, y, 4000.0, ex, ey, 9000.0)
        actor.set_actor_label(label)
        actor.set_folder_path("MapBoundaries")
    report["blockers"] = len(blockers)
    clear_prefix("RVT_Env")
    for label, texture in (("RVT_Env", "RVT_Environment"), ("RVT_Env2", "RVT_Height")):
        volume = actors_sub().spawn_actor_from_class(unreal.RuntimeVirtualTextureVolume,
                                                     unreal.Vector(lay["corner"][0], lay["corner"][1], Z_LOW))
        volume.set_actor_scale3d(unreal.Vector(lay["size_x"], lay["size_y"], Z_HIGH - Z_LOW))
        volume.set_actor_label(label)
        volume.set_folder_path(FOLDER + "Terrain")
        comp = volume.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
        comp.set_editor_property("virtual_texture", unreal.load_asset(RVT_ROOT + texture))
        comp.set_editor_property("bounds_align_actor", landscape_actor)
        report[label] = texture
    log("fit %s" % report)
    return report


def step_lighting():
    """Same hazy, dusty daylight as Ger_2ndOreMountains (ore_map_build.step_lighting)."""
    require_map()
    report = []
    for actor in all_actors():
        name = actor.get_class().get_name()
        if name == "DirectionalLight":
            comp = actor.light_component
            comp.set_intensity(4.2)
            comp.set_light_color(unreal.LinearColor(1.0, 0.95, 0.86, 1.0))
            comp.set_editor_property("atmosphere_sun_light", True)
            actor.set_actor_rotation(unreal.Rotator(0.0, -46.0, -152.0), False)
            report.append("sun")
        elif name == "SkyLight":
            comp = actor.light_component
            comp.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
            comp.set_intensity(1.0)
            comp.set_light_color(unreal.LinearColor(1.0, 0.97, 0.92, 1.0))
            comp.recapture_sky()
            report.append("sky")
        elif name == "ExponentialHeightFog":
            comp = actor.component
            comp.set_fog_density(0.006)
            comp.set_fog_height_falloff(0.08)
            comp.set_fog_inscattering_color(unreal.LinearColor(0.62, 0.56, 0.46, 1.0))
            comp.set_fog_max_opacity(0.55)
            comp.set_start_distance(6000.0)
            comp.set_directional_inscattering_color(unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
            report.append("fog")
        elif name == "PostProcessVolume":
            settings = actor.settings
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.9, 0.9, 0.9, 1.0))
            settings.set_editor_property("override_white_temp", True)
            settings.set_editor_property("white_temp", 6500.0)
            actor.set_editor_property("settings", settings)
            report.append("post")
    if not [a for a in all_actors() if a.get_class().get_name() == "SkyAtmosphere"]:
        sky = actors_sub().spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0))
        sky.set_actor_label("SkyAtmosphere")
        report.append("sky atmosphere added")
    log("lighting %s" % report)
    return report


# ---- captures ---------------------------------------------------------------------------------------------------
def shot_top(name, centre=(0.0, 0.0), width=None, size=2048):
    """Orthographic top-down capture, north up (east right), to Saved/EnemyItemMaps/WismutPlateau/shots/<name>.png."""
    lay = LAYOUT["landscape"]
    width = width or lay["size_x"]
    rt = unreal.RenderingLibrary.create_render_target2d(world(), size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = actors_sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(centre[0], centre[1], 30000.0),
                                              unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property("ortho_width", width)
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + ".png")
    actors_sub().destroy_actor(cap)
    log("shot %s%s.png" % (SHOTS, name))


def shot_persp(name, eye, target, fov=50.0, size=(1920, 1080)):
    rt = unreal.RenderingLibrary.create_render_target2d(world(), size[0], size[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    eye_v, tgt_v = unreal.Vector(*eye), unreal.Vector(*target)
    rot = unreal.MathLibrary.find_look_at_rotation(eye_v, tgt_v)
    cap = actors_sub().spawn_actor_from_class(unreal.SceneCapture2D, eye_v, rot)
    comp = cap.capture_component2d
    comp.set_editor_property("fov_angle", fov)
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + ".png")
    actors_sub().destroy_actor(cap)
    log("shot %s%s.png" % (SHOTS, name))


# ---- resources ----------------------------------------------------------------------------------------------------
RESOURCE_ROOT = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
RADIXITE = ("Resource_Radixite_Crystal_Large", "Resource_Radixite_Crystal", "Resource_Radixite_Crystal_LargePoint")


def _spawn_class(cls, label, x, y, yaw, folder, dz=0.0):
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(x, y) + dz), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def step_resources():
    """The 17 Ore metal nodes of the drawing (Resource_Metal_Ore_<type> per point) and a small radixite field."""
    require_map()
    removed = clear_prefix(PREFIX + "Res_")
    rng = random.Random(7)
    classes, placed = {}, []
    for f in LAYOUT["features"]:
        if f["kind"] != "resource":
            continue
        if f["raw"].get("resource") == "metal":
            for index, (p, kind) in enumerate(zip(f["points"], f["raw"]["types"])):
                bp = RESOURCE_ROOT + "Metal/OreMountains/" + kind
                if bp not in classes:
                    classes[bp] = unreal.EditorAssetLibrary.load_blueprint_class(bp)
                label = "%sRes_%s_%s_%d" % (PREFIX, f["id"], kind.split("_")[-1], index)
                _spawn_class(classes[bp], label, p[0], p[1], rng.uniform(0, 360), FOLDER + "Resources/Metal")
                placed.append(label)
        elif f["raw"].get("resource") == "radixite":
            cx, cy = f["points"][0]
            for index, (dx, dy) in enumerate(((0, 0), (700, 500), (-500, 650), (300, -700))):
                bp = RESOURCE_ROOT + "Radixite/" + RADIXITE[index % len(RADIXITE)]
                if bp not in classes:
                    classes[bp] = unreal.EditorAssetLibrary.load_blueprint_class(bp)
                label = "%sRes_Radixite_%d" % (PREFIX, index)
                _spawn_class(classes[bp], label, cx + dx, cy + dy, rng.uniform(0, 360), FOLDER + "Resources/Radixite")
                placed.append(label)
    log("resources: removed %d, placed %d" % (removed, len(placed)))
    return placed


# ---- dead spruce woods ----------------------------------------------------------------------------------------------
TREE_ROOT = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_"
TREE_MIX = (("Skeleton_11m", 5), ("Skeleton_9m", 5), ("Skeleton_11m_B", 4), ("Skeleton_13m", 3), ("Silver_10m", 3),
            ("Leaning_11m", 2), ("Snapped_7m", 3), ("Black_8m", 1), ("Survivor_9m", 1))
WOODS = (("woods_w", 0.55), ("woods_nw", 0.55), ("woods_e", 0.6), ("woods_se", 0.5))
TREE_SPACING = 520.0
TREE_CLEAR_ROADS = 650.0


def _tree_spots():
    rng = random.Random(1945)
    lines = [path(p)["points"] for p in ("haul_road", "east_track", "woods_track", "north_track")]
    ramps = [f.get("centreline") or f["points"] for f in LAYOUT["features"] if f["kind"] == "ramp"]
    spots = []
    for rid, keep in WOODS:
        reg = region(rid)
        poly = reg["points"]
        xs = [p[0] for p in poly]
        ys = [p[1] for p in poly]
        x = min(xs)
        while x < max(xs):
            y = min(ys)
            while y < max(ys):
                px = x + rng.uniform(-0.35, 0.35) * TREE_SPACING
                py = y + rng.uniform(-0.35, 0.35) * TREE_SPACING
                y += TREE_SPACING
                if rng.random() > keep or not point_in_poly(px, py, poly):
                    continue
                b = LAYOUT["bounds"]
                if not (b["xmin"] + 300 < px < b["xmax"] - 300 and b["ymin"] + 300 < py < b["ymax"] - 300):
                    continue
                if any(dist_polyline(px, py, line) < TREE_CLEAR_ROADS for line in lines):
                    continue
                if any(dist_polyline(px, py, r) < 1100.0 for r in ramps if len(r) > 1):
                    continue
                spots.append((px, py, rng.uniform(0, 360)))
            x += TREE_SPACING
    return spots


def step_trees():
    """Crushable BP_OreDeadSpruce_* (AWpoTree: medium and heavy tanks crush them) in the four dead spruce woods."""
    require_map()
    removed = clear_prefix(PREFIX + "Tree_")
    rng = random.Random(51)
    bag = [name for name, weight in TREE_MIX for _ in range(weight)]
    classes, counts = {}, {}
    spots = _tree_spots()
    for index, (x, y, yaw) in enumerate(spots):
        kind = rng.choice(bag)
        if kind not in classes:
            classes[kind] = unreal.EditorAssetLibrary.load_blueprint_class(TREE_ROOT + kind)
        actor = actors_sub().spawn_actor_from_class(classes[kind], unreal.Vector(x, y, ground_z(x, y) - 8.0),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label("%sTree_%s_%03d" % (PREFIX, kind, index))
        actor.set_folder_path(FOLDER + "Trees")
        counts[kind] = counts.get(kind, 0) + 1
    log("trees: removed %d, placed %d %s" % (removed, len(spots), counts))
    return counts


# ---- roads ----------------------------------------------------------------------------------------------------------
PATH_ROOT = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/"


def _path_actor(label, bp, points, width, material=None):
    cls = unreal.load_class(None, PATH_ROOT + bp + "." + bp.split("/")[-1] + "_C")
    first = points[0]
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(first[0], first[1], ground_z(first[0], first[1]) + 150.0))
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER + "Roads")
    spline = actor.get_component_by_class(unreal.SplineComponent)
    # the Blueprint snaps each tile by tracing down from the spline: every point starts above the ground
    spline.set_spline_points([unreal.Vector(x, y, ground_z(x, y) + 150.0) for x, y in points],
                             unreal.SplineCoordinateSpace.WORLD, True)
    spline.set_editor_property("spline_has_been_edited", True)   # or the construction script resets the spline
    if material is not None:
        actor.set_editor_property("PathMaterial", material)
    actor.set_editor_property("PathWidth", width + 1.0)          # a changed value reruns construction
    actor.set_editor_property("PathWidth", width)
    tiles = len(actor.get_components_by_class(unreal.SplineMeshComponent))
    log("%s: %d spline points, %d tiles" % (label, len(points), tiles))
    return actor


def _resample(points, step):
    """Points about `step` cm apart along a polyline (owner rule: even spacing, about one mesh length)."""
    out = [list(points[0])]
    carry = 0.0
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        seg = math.hypot(bx - ax, by - ay)
        d = step - carry
        while d < seg:
            out.append([ax + (bx - ax) * d / seg, ay + (by - ay) * d / seg])
            d += step
        carry = seg - (d - step)
    if math.hypot(out[-1][0] - points[-1][0], out[-1][1] - points[-1][1]) > step * 0.4:
        out.append(list(points[-1]))
    else:
        out[-1] = list(points[-1])
    return out


def step_roads():
    """The haul road (start -> pit -> main gate ramp -> base) as the old macadam road spline, the tracks as
    two-track dirt paths. Spline tiles have no collision and no nav effect."""
    require_map()
    removed = clear_prefix(PREFIX + "Road_")
    worn = unreal.load_asset(PATH_ROOT + "Road/MI_GroundPath_Road_WarTorn")
    # the drawing's road names the ramps by id (= their centre point); run it along each ramp's centre line instead,
    # or the spline cuts across the pit wall and the plateau face next to the ramp
    sw = feature("haul_sw").get("centreline") or feature("haul_sw")["points"]          # rim -> pit floor
    ne = feature("haul_ne").get("centreline") or feature("haul_ne")["points"]          # rim -> pit floor
    gate = feature("ramp_gate").get("centreline") or feature("ramp_gate")["points"]    # plateau top -> foot
    start = path("haul_road")["points"][:2]
    haul = start + list(sw) + list(reversed(ne)) + [[7000.0, 600.0]] + list(reversed(gate)) + [[13000.0, 8000.0]]
    _path_actor(PREFIX + "Road_Haul", "Road/BP_GroundPathSpline_Road", _resample(haul, 1200.0), 900.0, worn)
    for pid in ("east_track", "woods_track", "north_track"):
        _path_actor(PREFIX + "Road_" + pid, "BP_GroundPathSpline_TwoTrack", _resample(path(pid)["points"], 1500.0), 420.0)
    return removed


# ---- the Soviet base, item site director, slots ----------------------------------------------------------------------
BX = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
RB = BX + "RusBuildings/"
RK = BX + "RusBunkers/"
FIXED = [
    # (name, asset, x, y, yaw): the hand-placed base that stands for every item type (owner pick D2)
    ("Barracks", RB + "Rus_Building_Barracks", 9200, 17600, 200),
    ("AssemblyYard", RB + "AssemblyYard/Rus_Building_AssemblyYard", 10200, 14300, 200),
    ("CommandBunker", RB + "Rus_Building_AdminBunker", 12600, 15900, 200),
    ("FuelStore", RB + "Rus_Building_FuelStorage", 19700, 20700, 270),
    ("Tower_Gate", RK + "Rus_BXP_GuardTowerMG_Soviet", 8500, 7500, 225),
    ("Tower_North", RK + "Rus_BXP_GuardTowerMG_Soviet", 21300, 4300, 315),
    ("Tower_East", RK + "Rus_BXP_GuardTowerMG_Soviet", 20700, 19900, 45),
    ("Megaphone", RB + "Rus_Bxp_Megaphone", 11400, 11900, 225),
]
# AEnemyItemSignatureSlot: (slot id, feature id, half extents (forward, right) cm, yaw)
SIGNATURE_SLOTS = [
    ("LongSlot", "strip", (6500.0, 1000.0), 90.0),
    ("L1", "slot_L1", (1500.0, 1500.0), 200.0),
    ("L2", "slot_L2", (1500.0, 1500.0), 200.0),
    ("L3", "slot_L3", (1500.0, 1500.0), 200.0),
    ("S1", "slot_S1", (700.0, 700.0), 0.0),
    ("S2", "slot_S2", (700.0, 700.0), 0.0),
    ("S3", "slot_S3", (700.0, 700.0), 0.0),
    ("G1", "slot_G1", (900.0, 700.0), 215.0),
]
# AEnemyItemStrengthSlot: (slot id, x, y, yaw, defence asset, fortification, min strength %)
STRENGTH_SLOTS = [
    ("Bunker_Road", 8600, -3900, 200, RK + "Rus_BXP_MGBunker", "BUNKERS", 0),
    ("Bunker_Incline", 7700, 16700, 180, RK + "Rus_BXP_MGBunker", "BUNKERS", 0),
    ("Mines_Chute", 2600, 11600, 0, "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Mines/Rus_AnitTank_LightMineField",
     "MINEFIELDS", 0),
    ("AA_SouthEast", 8600, 20300, 0, RK + "Rus_BXP_Bofors", "AA_BATTERY", 0),
    ("AA_North", 21500, 5600, 0, RK + "Rus_BXP_Bofors", "AA_BATTERY", 0),
    ("Garrison", 11300, 20300, 200, RK + "Rus_BXP_DomeBunkerGarrison", "NONE", 50),
]


def _set_owner(actor):
    for comp in actor.get_components_by_class(unreal.RTSComponent):
        comp.set_editor_property("owning_player", ENEMY)


def _enum(enum_type, wanted):
    key = wanted.replace("_", "").lower()
    for name in dir(enum_type):
        if name.replace("_", "").lower() == key:
            return getattr(enum_type, name)
    raise RuntimeError("no %s.%s" % (enum_type.__name__, wanted))


def step_site():
    """Hand-placed Soviet base (owner 2), the AEnemyItemSiteDirector at the enemy start, the signature slots (content is
    set per item type later) and the strength slots. Labels WP_Base_*, WP_Site_*, WP_Slot_*, WP_Strength_*."""
    require_map()
    removed = sum(clear_prefix(PREFIX + p) for p in ("Base_", "Site_", "Slot_", "Strength_"))
    placed = []
    for name, asset, x, y, yaw in FIXED:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(asset)
        if cls is None:
            raise RuntimeError("missing " + asset)
        actor = _spawn_class(cls, PREFIX + "Base_" + name, x, y, yaw, FOLDER + "SovietBase", dz=-5.0)
        _set_owner(actor)
        placed.append(actor.get_actor_label())
    base = feature("enemy_base")["points"][0]
    director = _spawn_class(unreal.EnemyItemSiteDirector, PREFIX + "Site_Director", base[0], base[1], 225.0,
                            FOLDER + "SovietBase", dz=50.0)
    director.set_editor_property("base_radius", 6000.0)
    placed.append(director.get_actor_label())
    for slot_id, fid, (hx, hy), yaw in SIGNATURE_SLOTS:
        f = feature(fid)
        pts = f["points"]
        x = sum(p[0] for p in pts) / len(pts)
        y = sum(p[1] for p in pts) / len(pts)
        slot = _spawn_class(unreal.EnemyItemSignatureSlot, PREFIX + "Slot_" + slot_id, x, y, yaw, FOLDER + "SovietBase/Slots",
                            dz=10.0)
        slot.set_editor_property("slot_id", slot_id)
        slot.set_editor_property("footprint_half_extent", unreal.Vector2D(hx, hy))
        placed.append(slot.get_actor_label())
    for slot_id, x, y, yaw, asset, fort, min_strength in STRENGTH_SLOTS:
        slot = _spawn_class(unreal.EnemyItemStrengthSlot, PREFIX + "Strength_" + slot_id, x, y, yaw,
                            FOLDER + "SovietBase/StrengthSlots", dz=10.0)
        slot.set_editor_property("slot_id", slot_id)
        slot.set_editor_property("defence_class", unreal.EditorAssetLibrary.load_blueprint_class(asset))
        condition = slot.get_editor_property("condition")
        condition.set_editor_property("required_fortification", _enum(unreal.WorldFortificationStrength, fort))
        condition.set_editor_property("minimum_total_strength_percentage", min_strength)
        slot.set_editor_property("condition", condition)
        placed.append(slot.get_actor_label())
    log("site: removed %d, placed %d: %s" % (removed, len(placed), placed))
    return placed


def step_mission():
    """UDestroyEnemyItemSiteMission in the map's BP_MissionManager Missions (the item-map win condition)."""
    require_map()
    managers = [a for a in all_actors() if a.get_class().get_name() == "BP_MissionManager_C"]
    if len(managers) != 1:
        raise RuntimeError("expected one BP_MissionManager, found %d" % len(managers))
    manager = managers[0]
    missions = list(manager.get_editor_property("missions"))
    kept = [m for m in missions if m is not None and not isinstance(m, unreal.DestroyEnemyItemSiteMission)]
    mission = unreal.new_object(unreal.DestroyEnemyItemSiteMission, outer=manager)
    manager.set_editor_property("missions", kept + [mission])
    log("missions: %s" % [m.get_class().get_name() for m in manager.get_editor_property("missions")])
    return manager.get_editor_property("missions")


def rebuild_nav_and_save():
    """Blocking nav build, then a _defer-able wait and save (see headless-map-save-navmesh)."""
    require_map()
    unreal.SystemLibrary.execute_console_command(world(), "RebuildNavigation")

    def _wait():
        if not nav_ready():
            return False
        return save_level()
    return _wait
