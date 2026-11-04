"""In-editor build steps for SM_Kessel_01 (run inside the editor, one step per bridge call).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools")
    import kessel_map_build as kmb; importlib.reload(kmb)
    kmb.load_layout(); kmb.step_landscape()

Uses the layout json of kessel_terrain_gen.py. Dressing actors carry the KSL_ label prefix and sit in outliner folder
KesselDressing, so a step can clear and redo its own part. Seats by the visible ground, never by actor bounds.
"""
import array
import json
import math
import random
import zlib

import unreal

MAP_PATH = "/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Kessel/terrain/SM_Kessel_01_layout.json"
# A child of MI_RTS_Landscape_DryCastle_Inst, the Marder mission's instance: same shader and settings, with the master's
# grass clumps switched on and ragged radixite blight edges. Ground breakup stays off: re-thresholding the 1 m weightmap
# gave stair-stepped soil splotches from the game camera.
LANDSCAPE_MATERIAL = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Kessel"
LANDSCAPE_LABEL = "SM_Landscape"
PREFIX = "KSL_"
FOLDER = "KesselDressing"
LAYER_INFOS = {
    "Desert": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA/Desert_LayerInfo",
    "GrassCastle": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA/GrassCastle_LayerInfo",
    "RockDesert": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA/RockDesert_LayerInfo",
    "GraySand": "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA/GraySand_LayerInfo",
    "RTSSand": "/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35_sharedassets/RTSSand_LayerInfo",
    "MountainTop": "/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35_sharedassets/MountainTop_LayerInfo",
}
DEST_ROOT = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/"
# Kept by the guns: ammunition barrels, a revetment log, a few loose bags.
PIT_STORES = ["Airport/BP_Airport_Dest_BarrelStack2", "NaturalCover/Logs/BP_Cover_DeadTreeLog_04",
              "NaturalCover/SandBagWalls/BP_Cover_RusSandbags0", "Airport/BP_Airport_Dest_BarrelStack3",
              "NaturalCover/Logs/BP_Cover_DeadTreeLog_07"]
POLE_BLUEPRINT = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric1"
SANDBAG_WALL = "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_SandBagWall_FieldConstruction"
# The blueprint's nav blocker is 400 cm deep round a 180 cm mesh. Infantry take cover against the bags, and a unit standing in
# the blocker has no start poly: every path from there fails and the squad stays pinned. Fit it to the mesh.
SANDBAG_NAV_EXTENT = (95.0, 450.0, 100.0)
# Same trap as the sandbags: the trees' infantry blockers are 100-125 cm round trunks of 18-30 cm (measure_tree_trunks), and a
# rifleman pushed past a trunk into one stood off the navmesh and stayed pinned. Fit them to the trunk collision. Kinds not
# listed keep theirs: their collision already reaches past the blocker.
TREE_NAV_EXTENT = {"fir": 20.0, "fir_small": 20.0, "spruce": 30.0, "leaf_small": 30.0, "orchard": 26.0}
ROCK_ROOT = "/Game/Brushify/Materials/Landscape/ProceduralMeshes/Rocks/"
CONTENT_GRAPH = "/Game/RTS_Survival/SurvivalMode/Maps/PCG/PCG_Kessel_Content"
SURVIVAL_BP = "/Game/RTS_Survival/SurvivalMode/Blueprints/"
TREE_ROOT = "/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/"
TREE_BLUEPRINTS = {
    "fir": TREE_ROOT + "Firs/BP_WpoFir",
    "fir_small": TREE_ROOT + "Firs/BP_WpoFir_Small",
    "spruce": TREE_ROOT + "Spruces/BP_WpoSpruce_Tall",
    "spruce_dark": TREE_ROOT + "Spruces/BP_WpoSpruce_TallDark",
    "leaf": TREE_ROOT + "LeafTrees/BP_WpoLeafTree_Large",
    "leaf_small": TREE_ROOT + "SmallTrees/BP_WpoLeafTreeSmall",
    "bush": TREE_ROOT + "LeafTrees/BP_WpoBush_1",
    "dead": "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/BP_Scorch_Dest_DeadTree",
}

LAYOUT = {}


def log(text):
    unreal.log_warning("KESSEL_MAP " + str(text))
    print(text)


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


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


def by_label():
    return {actor.get_actor_label(): actor for actor in actors_sub().get_all_level_actors()}


def save_level():
    require_map()
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([world().get_outermost()], False)
    log("save_packages %s" % ok)
    return ok


def clear_prefix(prefix):
    count = 0
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(prefix):
            actors_sub().destroy_actor(actor)
            count += 1
    return count


def spawn_blueprint(path, label, folder, x, y, z, yaw=0.0, scale=1.0):
    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if actor_class is None:
        raise RuntimeError("no blueprint " + path)
    actor = actors_sub().spawn_actor_from_class(actor_class, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    if scale != 1.0:
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    return actor


HEIGHTS = None


def ground_z(x, y):
    """The landscape height from the generator file: dressing and generated actors can never get in the way."""
    global HEIGHTS
    if HEIGHTS is None:
        with open(LAYOUT["files"]["height"], "rb") as handle:
            HEIGHTS = array.array("H", handle.read())
    lay = LAYOUT["landscape"]
    verts = lay["components"] * lay["quads_per_component"] + 1
    fx = (x - lay["corner"][0]) / lay["quad_cm"]
    fy = (y - lay["corner"][1]) / lay["quad_cm"]
    j, i = int(math.floor(fx)), int(math.floor(fy))
    if not (0 <= i < verts - 1 and 0 <= j < verts - 1):
        return None
    tx, ty = fx - j, fy - i
    h00, h01 = HEIGHTS[i * verts + j], HEIGHTS[i * verts + j + 1]
    h10, h11 = HEIGHTS[(i + 1) * verts + j], HEIGHTS[(i + 1) * verts + j + 1]
    raw = (h00 * (1 - tx) + h01 * tx) * (1 - ty) + (h10 * (1 - tx) + h11 * tx) * ty
    return (raw - 32768.0) * 100.0 / 128.0


def lowest_ground(x, y, radius):
    """Lowest ground under a round footprint: seating to this leaves no gap on the downhill side."""
    points = [(x, y)] + [(x + radius * math.cos(k * math.pi / 4.0), y + radius * math.sin(k * math.pi / 4.0)) for k in range(8)]
    heights = [ground_z(px, py) for px, py in points]
    return min(h for h in heights if h is not None)


def step_strip_template():
    """Everything of the copied template that a survival match does not need, and its old environment."""
    require_map()
    keep_classes = ("RTS_BP_EnemyController_C", "BP_FogOfWar_Manager_C", "RTS_PLayerStart_C", "PlayerStart",
                    "NavMeshBoundsVolume", "RecastNavMesh", "BP_MissionManager_C", "WorldSettings", "BlockingVolume",
                    "AbstractNavData", "WorldDataLayers", "BrushComponent")
    kept, removed = [], []
    for actor in actors_sub().get_all_level_actors():
        name = actor.get_class().get_name()
        if name in keep_classes:
            kept.append(actor.get_actor_label())
            continue
        if isinstance(actor, unreal.Landscape):
            continue
        removed.append("%s(%s)" % (actor.get_actor_label(), name))
        actors_sub().destroy_actor(actor)
    log("kept %s" % kept)
    log("removed %d: %s" % (len(removed), removed[:60]))
    return kept


def step_landscape():
    require_map()
    lay = LAYOUT["landscape"]
    files = LAYOUT["files"]
    old = [a for a in actors_sub().get_all_level_actors() if isinstance(a, unreal.Landscape)]
    body_instance = None
    phys_material = unreal.load_asset("/Game/RTS_Survival/PhysicalMaterials/PM_Sand")
    for landscape in old:
        if body_instance is None:
            # Units only drive on ground that blocks the Landscape trace channel: keep the custom profile.
            body_instance = landscape.get_editor_property("body_instance")
            phys_material = landscape.get_editor_property("default_phys_material") or phys_material
        actors_sub().destroy_actor(landscape)
    names = list(files["layers"].keys())
    infos = [unreal.load_asset(LAYER_INFOS[name]) for name in names]
    missing = [n for n, info in zip(names, infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s" % missing)
    material = unreal.load_asset(LANDSCAPE_MATERIAL)
    corner = lay["corner"]
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        world(), unreal.Vector(corner[0], corner[1], 0.0), unreal.Vector(lay["quad_cm"], lay["quad_cm"], 100.0),
        lay["components"], lay["components"], lay["quads_per_component"], material, files["height"], infos,
        [files["layers"][n] for n in names], LANDSCAPE_LABEL)
    if landscape is None:
        raise RuntimeError("create_painted_landscape failed (see LogRTSShortsLandscape)")
    if body_instance is not None:
        landscape.set_editor_property("body_instance", body_instance)
    if phys_material is not None:
        landscape.set_editor_property("default_phys_material", phys_material)
    landscape.set_editor_property("runtime_virtual_textures", [unreal.load_asset("/Game/RTS_Survival/Meshes/RVTs/RVT_Environment"),
                                                               unreal.load_asset("/Game/RTS_Survival/Meshes/RVTs/RVT_Height")])
    landscape.set_editor_property("virtual_texture_render_pass_type", unreal.RuntimeVirtualTextureMainPassType.ALWAYS)
    landscape.set_actor_label(LANDSCAPE_LABEL)
    log("landscape created; layers %s" % [str(n) for n in landscape.get_target_layer_names()])
    return landscape


def step_reimport_heights():
    """Height-only iteration on the same grid."""
    require_map()
    landscape = [a for a in actors_sub().get_all_level_actors() if isinstance(a, unreal.Landscape)][0]
    return unreal.RTSShortsLandscapeLibrary.import_stage_heights(landscape, LAYOUT["files"]["height"])


def step_fit_level_actors():
    """FOW, nav bounds, RVT volumes, boundary blockers and the start actors onto the new map."""
    require_map()
    half = LAYOUT["playable_half"]
    land_half = LAYOUT["landscape"]["size_cm"] / 2.0
    core_z = LAYOUT["core"]["z"]
    actors = by_label()
    report = {}
    landscape_actor = actors.get(LANDSCAPE_LABEL)
    for actor in actors.values():
        name = actor.get_class().get_name()
        if name == "BP_FogOfWar_Manager_C":
            actor.set_actor_location(unreal.Vector(0.0, 0.0, core_z), False, False)
            actor.set_editor_property("map_extent", half)
        elif name == "NavMeshBoundsVolume":
            origin, extent = actor.get_actor_bounds(False)
            scale = actor.get_actor_scale3d()
            z_low, z_high = -600.0, 3400.0
            want = unreal.Vector(half + 200.0, half + 200.0, (z_high - z_low) / 2.0)
            actor.set_actor_scale3d(unreal.Vector(scale.x * want.x / extent.x, scale.y * want.y / extent.y,
                                                  scale.z * want.z / extent.z))
            actor.set_actor_location(unreal.Vector(0.0, 0.0, (z_high + z_low) / 2.0), False, False)
        elif name == "RuntimeVirtualTextureVolume":
            # The box maps the RVT on the landscape: it must cover exactly the landscape, as on the Marder map.
            comp = actor.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
            actor.set_actor_location(unreal.Vector(-land_half, -land_half, -600.0), False, False)
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
            actor.set_actor_scale3d(unreal.Vector(2.0 * land_half, 2.0 * land_half, 3200.0))
            # A pasted volume loses its texture reference.
            texture = "RVT_Height" if actor.get_actor_label() == "RVT_Env2" else "RVT_Environment"
            comp.set_editor_property("virtual_texture", unreal.load_asset("/Game/RTS_Survival/Meshes/RVTs/" + texture))
            comp.set_editor_property("bounds_align_actor", landscape_actor)
            report[actor.get_actor_label()] = texture
        elif name in ("RTS_PLayerStart_C", "PlayerStart"):
            actor.set_actor_location(unreal.Vector(0.0, 0.0, core_z + (100.0 if name == "PlayerStart" else 43.0)), False, False)
        elif name in ("RTS_BP_EnemyController_C", "BP_MissionManager_C"):
            actor.set_actor_location(unreal.Vector(0.0, 0.0, core_z + 1500.0), False, False)
    log("fit %s" % report)
    return report


def set_blocker(actor, x, y, ex, ey):
    origin, extent = actor.get_actor_bounds(False)
    scale = actor.get_actor_scale3d()
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    origin, extent = actor.get_actor_bounds(False)
    scale = actor.get_actor_scale3d()
    actor.set_actor_scale3d(unreal.Vector(scale.x * ex / max(extent.x, 1.0), scale.y * ey / max(extent.y, 1.0),
                                          scale.z * 5000.0 / max(extent.z, 1.0)))
    actor.set_actor_location(unreal.Vector(x, y, 2000.0), False, False)


def step_boundaries():
    require_map()
    half = LAYOUT["playable_half"]
    blockers = [a for a in actors_sub().get_all_level_actors() if a.get_class().get_name() == "BlockingVolume"]
    log("blockers %s" % [(a.get_actor_label(), a.get_actor_location().x, a.get_actor_location().y) for a in blockers])
    thick = 600.0
    length = half + 1200.0
    spots = [(half + thick, 0.0, thick, length), (-(half + thick), 0.0, thick, length),
             (0.0, half + thick, length, thick), (0.0, -(half + thick), length, thick)]
    for actor, spot in zip(blockers, spots):
        set_blocker(actor, *spot)
    return len(blockers)


def step_survival_actors():
    require_map()
    clear_prefix("Survival")
    core = LAYOUT["core"]
    folder = "Survival"
    start = spawn_blueprint(SURVIVAL_BP + "BP_SurvivalStartLocation", "SurvivalStart_Centre", folder, 0.0, 0.0, core["z"])
    start.set_editor_property("build_area_radius", core["build_radius"])
    start.set_editor_property("display_name", unreal.Text("Kessel"))
    director = spawn_blueprint(SURVIVAL_BP + "BP_SurvivalModeDirector", "SurvivalModeDirector", folder, 0.0, 0.0, 3000.0)
    director.set_editor_property("settings", unreal.load_asset("/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalMode_Kessel"))
    for k, zone in enumerate(LAYOUT["edge_zones"]):
        actor = spawn_blueprint(SURVIVAL_BP + "BP_SurvivalEdgeSpawnZone", "SurvivalEdge_%d" % k, folder, zone["x"], zone["y"],
                                zone["z"], zone["yaw"])
    settings = world().get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.load_class(None, SURVIVAL_BP + "BP_SurvivalGameMode.BP_SurvivalGameMode_C"))
    log("survival actors placed, game mode %s" % settings.get_editor_property("default_game_mode"))


def step_sites():
    """One marker per candidate site; the content graph reads these."""
    require_map()
    clear_prefix("Site_")
    categories = {"Capture": unreal.SurvivalMapSiteCategory.CAPTURE, "Radixite": unreal.SurvivalMapSiteCategory.RADIXITE,
                  "Metal": unreal.SurvivalMapSiteCategory.METAL, "Wreck": unreal.SurvivalMapSiteCategory.WRECK,
                  "VehicleParts": unreal.SurvivalMapSiteCategory.VEHICLE_PARTS, "Animals": unreal.SurvivalMapSiteCategory.ANIMALS}
    counts = {}
    for site in LAYOUT["sites"]:
        name = site["category"]
        counts[name] = counts.get(name, 0) + 1
        z = ground_z(site["x"], site["y"])
        actor = actors_sub().spawn_actor_from_class(unreal.SurvivalMapSite, unreal.Vector(site["x"], site["y"], z or site["z"]),
                                                    unreal.Rotator(0.0, 0.0, site["yaw"]))
        actor.set_actor_label("Site_%s_%02d" % (name, counts[name]))
        actor.set_folder_path("KesselSites/" + name)
        actor.set_editor_property("category", categories[name])
        actor.set_editor_property("site_radius", site["radius"])
    log("sites %s" % counts)
    return counts


def step_power_lines():
    """The poles wire themselves at BeginPlay through their DestructibleWire and ConnectToActor."""
    require_map()
    clear_prefix(PREFIX + "Pole")
    count = 0
    for line in LAYOUT["power_lines"]:
        actors = []
        for k, pole in enumerate(line["poles"]):
            z = ground_z(pole["x"], pole["y"])
            actor = spawn_blueprint(POLE_BLUEPRINT, "%sPole_%s_%02d" % (PREFIX, line["name"], k),
                                    FOLDER + "/Power/" + line["name"], pole["x"], pole["y"], (z or pole["z"]) - 25.0, pole["yaw"])
            actors.append(actor)
            count += 1
        for a, b in zip(actors[:-1], actors[1:]):
            a.set_editor_property("ConnectToActor", [b])
    log("poles %d" % count)
    return count


def measure_tree_trunks():
    """Per tree Blueprint: its nav blocker extent and the radius of its trunk collision, traced in from eight sides."""
    require_map()
    out = {}
    try:
        measure_each_tree(out)
    finally:
        clear_prefix(PREFIX + "TreeProbe")
    return out


def measure_each_tree(out):
    for kind, path in TREE_BLUEPRINTS.items():
        actor = spawn_blueprint(path, PREFIX + "TreeProbe", FOLDER, 0.0, 0.0, 20000.0, 0.0)
        blockers = [c.get_editor_property("failsafe_extent") for c in actor.get_components_by_class(unreal.NavModifierComponent)]
        meshes = [c for c in actor.get_components_by_class(unreal.PrimitiveComponent)
                  if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION]
        radius = 0.0
        for height in (30.0, 90.0, 160.0):
            for k in range(8):
                a = math.radians(45.0 * k)
                start = unreal.Vector(600.0 * math.cos(a), 600.0 * math.sin(a), 20000.0 + height)
                for comp in meshes:
                    hit = comp.line_trace_component(start, unreal.Vector(0.0, 0.0, 20000.0 + height), False, False, False)
                    if hit is not None:
                        radius = max(radius, math.hypot(hit[0].x, hit[0].y))
        out[kind] = dict(blocker=[(round(b.x), round(b.y), round(b.z)) for b in blockers], trunk_radius=round(radius))
        actor.destroy_actor()


def fit_tree_blocker(actor, kind):
    extent = TREE_NAV_EXTENT.get(kind)
    if extent is None:
        return
    for comp in actor.get_components_by_class(unreal.NavModifierComponent):
        height = comp.get_editor_property("failsafe_extent").z
        comp.set_editor_property("failsafe_extent", unreal.Vector(extent, extent, height))
    remeasure_nav_modifiers(actor)


def remeasure_nav_modifiers(actor):
    """A nav modifier caches its box when it registers; a transform change makes it measure again."""
    loc = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(loc.x + 1.0, loc.y, loc.z), False, False)
    actor.set_actor_location(loc, False, False)


def refit_tree_blockers():
    """Fit the blockers of the trees already in the level, without respawning them."""
    require_map()
    kinds = {k: tree["kind"] for k, tree in enumerate(LAYOUT["trees"])}
    count = 0
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX + "Tree_"):
            kind = kinds.get(int(label[len(PREFIX) + 5:]))
            if kind in TREE_NAV_EXTENT:
                fit_tree_blocker(actor, kind)
                count += 1
    return count


def step_trees():
    require_map()
    clear_prefix(PREFIX + "Tree")
    count = 0
    for k, tree in enumerate(LAYOUT["trees"]):
        z = ground_z(tree["x"], tree["y"])
        actor = spawn_blueprint(TREE_BLUEPRINTS[tree["kind"]], "%sTree_%03d" % (PREFIX, k), FOLDER + "/Trees", tree["x"],
                                tree["y"], (z or tree["z"]) - 10.0, (k * 137.5) % 360.0)
        fit_tree_blocker(actor, tree["kind"])
        count += 1
    log("trees %d" % count)
    return count


def step_pit_stores():
    """Ammunition crates and a log in each knoll gun pit, so the walls hold something."""
    require_map()
    clear_prefix(PREFIX + "PitStore")
    groups = {}
    for bag in LAYOUT["sandbags"]:
        if bag["group"].startswith("knoll_"):
            groups.setdefault(bag["group"], []).append(bag)
    count = 0
    for k, (group, bags) in enumerate(sorted(groups.items())):
        cx, cy = sum(b["x"] for b in bags) / len(bags), sum(b["y"] for b in bags) / len(bags)
        knoll = min(LAYOUT["knolls"], key=lambda q: math.hypot(q["x"] - cx, q["y"] - cy))
        dx, dy = knoll["x"] - cx, knoll["y"] - cy
        length = math.hypot(dx, dy) or 1.0
        # Behind the walls, on the knoll side.
        x, y = cx + dx / length * 380.0, cy + dy / length * 380.0
        store = PIT_STORES[k % len(PIT_STORES)]
        crate = spawn_blueprint(DEST_ROOT + store, "%sPitStore_%d" % (PREFIX, k),
                                FOLDER + "/Sandbags", x, y, knoll["z"], math.degrees(math.atan2(dy, dx)) + 20.0 * k)
        seat_on_visible_mesh(crate, 4.0)
        count += 1
    return count


def step_sandbags():
    """German field construction walls: infantry garrison them, so the defenders fight from behind the bags."""
    require_map()
    clear_prefix(PREFIX + "Sandbag")
    count = 0
    for k, bag in enumerate(LAYOUT["sandbags"]):
        z = ground_z(bag["x"], bag["y"])
        # Knoll rims: the top is level, so the knoll height is the ground.
        wall = spawn_blueprint(SANDBAG_WALL, "%sSandbag_%s_%02d" % (PREFIX, bag["group"], k), FOLDER + "/Sandbags", bag["x"], bag["y"],
                               z or LAYOUT["core"]["z"], bag["yaw"])
        for comp in wall.get_components_by_class(unreal.NavModifierComponent):
            comp.set_editor_property("failsafe_extent", unreal.Vector(*SANDBAG_NAV_EXTENT))
        remeasure_nav_modifiers(wall)
        count += 1
    log("sandbag walls %d" % count)
    return count


def step_rocks():
    require_map()
    clear_prefix(PREFIX + "Rock")
    count = 0
    for k, rock in enumerate(LAYOUT["rocks"]):
        name = "Rock_0%d" % rock["kind"]
        mesh = unreal.load_asset("%s%s/S_%s" % (ROCK_ROOT, name, name))
        material = unreal.load_asset("%s%s/MI_%s_Desert" % (ROCK_ROOT, name, name))
        if mesh is None:
            continue
        ground = lowest_ground(rock["x"], rock["y"], rock["height"] * 0.55)
        bounds = mesh.get_bounds()
        low, high = bounds.origin.z - bounds.box_extent.z, bounds.origin.z + bounds.box_extent.z
        scale = rock["height"] / (high - low)
        z = ground - rock["height"] * rock["bury"] - low * scale
        actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(rock["x"], rock["y"], z),
                                                    unreal.Rotator(0.0, 0.0, rock["yaw"]))
        comp = actor.static_mesh_component
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        comp.set_static_mesh(mesh)
        if material is not None:
            comp.set_material(0, material)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        if not rock["collision"]:
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property("can_ever_affect_navigation", False)
        actor.set_actor_label("%sRock_%03d" % (PREFIX, k))
        actor.set_folder_path(FOLDER + "/Rocks")
        count += 1
    log("rocks %d" % count)
    return count


def step_content_volume():
    require_map()
    clear_prefix("KesselContent")
    half = LAYOUT["playable_half"]
    actor = actors_sub().spawn_actor_from_class(unreal.SurvivalMapContentVolume, unreal.Vector(0.0, 0.0, 1000.0))
    actor.set_actor_label("KesselContentVolume")
    actor.set_folder_path("Survival")
    box = actor.get_component_by_class(unreal.BoxComponent)
    box.set_box_extent(unreal.Vector(half, half, 3000.0), True)
    pcg = actor.get_component_by_class(unreal.PCGComponent)
    pcg.set_graph(unreal.load_asset(CONTENT_GRAPH))
    log("content volume graph %s" % pcg.get_editor_property("graph_instance"))
    return actor


def step_generated_audit(path):
    """Measures every actor the content graph spawned against the ground: the visible mesh must touch or enter it."""
    require_map()
    rows = []
    for actor in actors_sub().get_all_level_actors():
        if not str(actor.get_folder_path()).endswith("KesselContentVolume_Generated"):
            continue
        rows.append(audit_actor(actor))
    with open(path, "w") as handle:
        json.dump(rows, handle)
    return len(rows)


def audit_actor(actor):
    """Gap between the visible LOD0 vertices and the ground, per quadrant round the pivot."""
    centre = actor.get_actor_location()
    quadrant_gap = [None, None, None, None]
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(comp, unreal.InstancedStaticMeshComponent) or not comp.is_visible():
            continue
        mesh = comp.get_editor_property("static_mesh")
        if mesh is None:
            continue
        transform = comp.get_world_transform()
        for section in range(mesh.get_num_sections(0)):
            vertices = list(unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)[0])
            for vertex in vertices[::max(1, len(vertices) // 400)]:
                world_point = transform.transform_location(vertex)
                ground = ground_z(world_point.x, world_point.y)
                if ground is None:
                    continue
                gap = world_point.z - ground
                index = (1 if world_point.x > centre.x else 0) + (2 if world_point.y > centre.y else 0)
                quadrant_gap[index] = gap if quadrant_gap[index] is None else min(quadrant_gap[index], gap)
    gaps = sorted(g for g in quadrant_gap if g is not None)
    # Tracks of capture vehicles are skeletal, so the static hull alone reads as clear of the ground.
    skeletal_low = None
    for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.is_visible() and comp.get_editor_property("skeletal_mesh_asset") is not None:
            origin, extent, _ = unreal.SystemLibrary.get_component_bounds(comp)
            ground = ground_z(origin.x, origin.y)
            gap = origin.z - extent.z - ground
            skeletal_low = gap if skeletal_low is None else min(skeletal_low, gap)
    if skeletal_low is not None:
        gaps = sorted(gaps + [skeletal_low, skeletal_low])
    return dict(cls=actor.get_class().get_name(), x=centre.x, y=centre.y, z=centre.z, lowest=gaps[0] if gaps else None,
                second=gaps[1] if len(gaps) > 1 else None, highest=gaps[-1] if gaps else None)


# ------------------------------------------------------------------------------------------------ points of interest
ENV_BP = "/Game/RTS_Survival/Blueprints/Environment/"
VEHICLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/"
# The plain dirt craters: the sandy ones have a bright rim that read as white rings on this ground.
CRATER = "/Game/RTS_Survival/Materials/Decals/CraterDirt/BP_RTS_RandomDirtCraterDecal"
ORCHARD = ENV_BP + "OrchardTrees/BP_OrchardTree_"
FARM_MESHES = ENV_BP + "CrushableInstancedActor/FarmCrops/Meshes/"
DEST = ENV_BP + "DestructableEnvActor/"
# One scene per approach, in the frame of its pad: u away from the base along the road, v to the side, yaw relative to
# the road. ("bp", path) spawns a Blueprint, ("mesh", path) a static mesh, ("prop", path) a small static mesh that only
# sits on the ground, ("decal", path) a crater seated on its pivot. Each scene tells what happened there.
POI_SCENES = {
    # A farm still worked until the front came: hay in from the fields, an orchard with one tree cut down.
    "farmstead": [
        ("bp", DEST + "Farm/BP_Farm_Dest_FarmHouse", 0.0, 0.0, 90.0),
        ("bp", DEST + "Farm/BP_Farm_Dest_Barn", -400.0, 2300.0, 0.0),
        ("bp", DEST + "Farm/BP_Farm_Dest_HayShed", 1800.0, 700.0, 180.0),
        ("bp", DEST + "NaturalCover/Tractors/BP_Cover_Tractor_STZ1", 900.0, -1200.0, 35.0),
        ("prop", FARM_MESHES + "SM_HayBale_Round_A", 1350.0, 1750.0, 20.0),
        ("prop", FARM_MESHES + "SM_HayBale_Round_B", 1650.0, 1950.0, 75.0),
        ("prop", FARM_MESHES + "SM_HayBale_Round_Stack", 2700.0, -200.0, 10.0),
        ("prop", FARM_MESHES + "SM_HayStack", -1700.0, 3300.0, 0.0),
        ("bp", DEST + "NaturalCover/Logs/BP_Cover_DeadTreeLog_03", -4300.0, 1100.0, 30.0),
        ("prop", ENV_BP + "OrchardTrees/Meshes/SM_AppleTree_Stump", -2740.0, 260.0, 0.0),
        ("tree", ENV_BP + "WpoTreeFoliage/SmallTrees/BP_WpoLeafTreeSmall", 3100.0, 3100.0, 0.0),
    ] + [("tree", ORCHARD + kind, -3500.0 + 760.0 * (k % 3), -500.0 + 760.0 * (k // 3), 37.0 * k)
         for k, kind in enumerate(["Apple_A", "Pear_A", "Cherry_A", "Apple_B", None, "Cherry_B"]) if kind],
    # A Soviet supply column caught on the road; its T-34 escort knocked out at the head.
    # Nose to tail on the old road through the pad (kessel_terrain_gen SCENE_TRACKS "old_road"), heading away from the
    # base; the fuel truck jackknifed when it was hit, two civilian cars were shoved off the road.
    "convoy": [
        # Gaps of 7 to 25 m: the column bunched when the first truck was hit; the ammo truck swerved off the road.
        ("bp", VEHICLES + "ZIS5V/Env_Dest_ZIS5V", -4300.0, -500.0, 8.0),
        ("bp", VEHICLES + "ZilTroops/Env_Dest_ZilTroops", -2700.0, -20.0, -6.0),
        ("bp", VEHICLES + "ZilCargo/Env_Dest_Zil_Fuel", -1900.0, 300.0, 62.0),
        ("bp", VEHICLES + "ZilCargo/Env_Dest_Zil_Ammo", 600.0, 950.0, 27.0),
        ("bp", ENV_BP + "Props/OldCars/Cover/BP_Cover_OldCar_Pickup", -1200.0, 1400.0, 70.0),
        ("bp", ENV_BP + "Props/OldCars/Cover/BP_Cover_OldCar_PanelVan", 2300.0, -1500.0, -120.0),
        ("bp", DEST + "Military/DestTanks/BP_Env_DestroyedT34", 3400.0, -500.0, -14.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack02", -1700.0, 800.0, 35.0),
        ("decal", CRATER, -3000.0, 650.0, 0.0), ("decal", CRATER, -200.0, -800.0, 0.0),
        ("decal", CRATER, 1900.0, 650.0, 0.0), ("decal", CRATER, 3000.0, -1400.0, 0.0),
        ("decal", CRATER, -4400.0, -1300.0, 0.0), ("decal", CRATER, 600.0, 1900.0, 0.0),
    ],
    # A transport came down short of the airfield: it ploughed a furrow, scattered its cargo and burned the trees.
    "crash": [
        ("mesh", ENV_BP + "ScavengeObjects/CrashedPlanes/DC3/SM_RTS_CrashedPlane_DC3", 0.0, 600.0, 28.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_BarrelStack", 2300.0, -1200.0, 15.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_BarrelStack3", -2300.0, 2500.0, 70.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack03", 1100.0, -900.0, 50.0),
        ("bp", DEST + "BP_Scorch_Dest_DeadTree", 1700.0, 2700.0, 0.0),
        ("bp", DEST + "BP_Scorch_Dest_DeadTree", -1300.0, -2500.0, 140.0),
        ("bp", DEST + "BP_Scorch_Dest_DeadWillow", -4300.0, -3000.0, 200.0),
        # Cargo thrown out along the furrow as it ploughed in.
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack01", -3600.0, -1300.0, 64.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_BarrelStack2", -5600.0, -2200.0, 12.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack02", -1900.0, 300.0, 130.0),
        ("decal", CRATER, -2100.0, -1500.0, 0.0), ("decal", CRATER, -3300.0, -900.0, 0.0),
        ("decal", CRATER, 2400.0, 2300.0, 0.0), ("decal", CRATER, -1700.0, -100.0, 0.0),
        ("decal", CRATER, -4800.0, -1800.0, 0.0),
    ],
    # A Soviet forward line overrun: trenches, wire, a camouflaged dugout, and the Panzer III that broke through.
    # The trench line itself comes from the layout (kessel_terrain_gen TRENCH_LINE).
    "trenches": [
        ("bp", DEST + "Military/Barricades/BP_Military_Dest_BarbedWireLong", 2250.0, -900.0, 96.0),
        ("bp", DEST + "Military/Barricades/BP_Military_Dest_BarbedWireLong", 2700.0, 2000.0, 78.0),
        ("bp", DEST + "Military/BP_Military_Dest_Camonet", -2700.0, -1500.0, 15.0),
        ("bp", DEST + "Military/Barricades/BP_Military_Dest_BarbedWire", 1900.0, -2500.0, 88.0),
        ("bp", DEST + "Military/Barricades/BP_Military_Dest_BarbedWire", 2100.0, 400.0, 81.0),
        # Debris along the parapet: logs for the revetment, ammunition crates, a few loose sandbags.
        ("bp", DEST + "NaturalCover/Logs/BP_Cover_DeadTreeLog_05", 300.0, -1400.0, 80.0),
        ("bp", DEST + "NaturalCover/Logs/BP_Cover_DeadTreeLog_02", 1900.0, 700.0, 12.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack03", -500.0, 1300.0, 40.0),
        ("bp", DEST + "NaturalCover/SandBagWalls/BP_Cover_RusSandbags1", 1800.0, -2200.0, 100.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_CrateStack01", -2300.0, 700.0, 0.0),
        ("bp", DEST + "Military/DestTanks/BP_Env_DestroyedPzIII", 3900.0, 900.0, 200.0),
    ] + [("bp", DEST + "Military/Barricades/BP_Military_Dest_Hedgehog", u, v, yaw) for u, v, yaw in
         # A loose belt with gaps where the tanks came through, not a row.
         ((2300.0, -1700.0, 10.0), (3500.0, -1100.0, 47.0), (2700.0, -650.0, 80.0), (4100.0, 300.0, 5.0),
          (2450.0, 900.0, 33.0), (3300.0, 1500.0, 61.0), (3900.0, 2600.0, 15.0), (2600.0, 3100.0, 72.0))],
    # A pumping station that kept the farms watered: tower, silos, a pipe run never laid, a car left behind.
    "waterworks": [
        ("bp", DEST + "PostApo/BP_Dest_PostApo_WaterTower", 0.0, 0.0, 0.0),
        ("bp", DEST + "PostApo/BP_Dest_PostApo_Silo", 1300.0, 1500.0, 40.0),
        ("bp", DEST + "PostApo/BP_Dest_PostApo_Silo2", 1400.0, -1300.0, 0.0),
        ("bp", DEST + "Airport/BP_Airport_Dest_BarrelStack2", -1300.0, 1300.0, 20.0),
        ("bp", DEST + "NaturalCover/Tractors/BP_Cover_Tractor_S65", -1200.0, -1000.0, 300.0),
        ("bp", DEST + "Industrial/Pipes/BP_Dest_Crushable_RusPipes2", -300.0, -2300.0, 10.0),
        # The run that feeds the silos from the tower.
        ("bp", DEST + "Industrial/Pipes/BP_Dest_Crushable_RusPipes1", 650.0, 750.0, 49.0),
        ("bp", DEST + "Industrial/Pipes/BP_Dest_Crushable_RusPipes3", 700.0, -650.0, -43.0),
        ("bp", ENV_BP + "Props/OldCars/Cover/BP_Cover_OldCar_Sedan", -2700.0, 2200.0, 40.0),
    ],
}
# Seeded offsets so nothing in a scene stands on a ruler line: (cm along u and v, degrees of yaw) per kind of item.
POI_JITTER = {"bp": (160.0, 8.0), "mesh": (0.0, 0.0), "prop": (120.0, 25.0), "tree": (110.0, 40.0), "decal": (250.0, 0.0)}
FIELD_ROOT = ENV_BP + "CrushableInstancedActor/FarmCrops/"
FIELD_FENCE_STEP = 380.0
FIELD_GATE_WIDTH = 500.0
YARD_FENCE_MARGIN = 450.0
YARD_GATE_WIDTH = 900.0


def visible_footprint(actor):
    """XY box of the visible static meshes (no root sprite, no collision helpers)."""
    box = None
    for comp in actor.get_components_by_class(unreal.MeshComponent):
        if isinstance(comp, unreal.InstancedStaticMeshComponent) or not comp.is_visible():
            continue
        # Geometry collections report bounds that reach to the world origin.
        if "GeometryCollection" in comp.get_class().get_name():
            continue
        if isinstance(comp, unreal.StaticMeshComponent) and comp.get_editor_property("static_mesh") is None:
            continue
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(comp)
        b = (origin.x - extent.x, origin.y - extent.y, origin.x + extent.x, origin.y + extent.y)
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def seat_on_visible_mesh(actor, sink):
    """Lowest visible LOD0 point onto the lowest ground under the footprint, so no corner hangs in the air."""
    import kessel_pcg_build as kp
    low = kp.lowest_visible_point(actor)
    box = visible_footprint(actor)
    loc = actor.get_actor_location()
    if box is None:
        ground = ground_z(loc.x, loc.y)
    else:
        xs, ys = (box[0], (box[0] + box[2]) / 2.0, box[2]), (box[1], (box[1] + box[3]) / 2.0, box[3])
        ground = min(ground_z(x, y) for x in xs for y in ys)
    target = ground - (low if low is not None else 0.0) - sink
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, target), False, False)
    return low, box


def step_pois():
    require_map()
    clear_prefix(PREFIX + "POI")
    count = 0
    for poi in LAYOUT["pois"]:
        a = math.radians(poi["yaw"])
        ux, uy = math.cos(a), math.sin(a)
        # Local v points away from the road, whichever side of it the scene stands on.
        vx, vy = -uy * poi["side"], ux * poi["side"]
        rng = random.Random(zlib.crc32(poi["kind"].encode()))
        for k, (kind, path, u, v, yaw) in enumerate(POI_SCENES[poi["kind"]]):
            spread, turn = POI_JITTER[kind]
            u, v, yaw = u + rng.uniform(-spread, spread), v + rng.uniform(-spread, spread), yaw + rng.uniform(-turn, turn)
            x = poi["x"] + u * ux + v * vx
            y = poi["y"] + u * uy + v * vy
            z = ground_z(x, y)
            label = "%sPOI_%s_%02d" % (PREFIX, poi["kind"], k)
            folder = FOLDER + "/POI/" + poi["kind"]
            if kind in ("mesh", "prop"):
                actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                                            unreal.Rotator(0.0, 0.0, poi["yaw"] + yaw))
                actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
                actor.set_actor_label(label)
                actor.set_folder_path(folder)
            elif kind == "decal":
                # Shells of different calibres: no two craters the same size or turned the same way.
                actor = spawn_crater(label, folder, x, y, rng.uniform(0.6, 1.6), rng.uniform(0.0, 360.0), rng)
            else:
                actor = spawn_blueprint(path, label, folder, x, y, z, poi["yaw"] + yaw)
            if kind in ("bp", "mesh", "prop"):
                seat_on_visible_mesh(actor, {"bp": 4.0, "mesh": 25.0, "prop": 6.0}[kind])
            elif kind == "tree":
                actor.set_actor_location(unreal.Vector(x, y, z - 10.0), False, False)
                fit_tree_blocker(actor, "orchard")
            count += 1
    # The furrow a crashed plane ploughed: overlapping stretched crater decals of turned dark earth, widening to the nose.
    # The landscape has no dark soil layer, so paint alone reads as nothing from the game camera.
    rng = random.Random(11)
    for k, scar in enumerate(LAYOUT.get("scars", [])):
        (ax, ay), (bx, by) = scar["start"], scar["end"]
        heading = math.degrees(math.atan2(by - ay, bx - ax))
        steps = max(2, int(math.dist((ax, ay), (bx, by)) / SCAR_DECAL_STEP))
        for m in range(steps):
            t = (m + 0.5) / steps
            x, y = ax + (bx - ax) * t + rng.uniform(-80.0, 80.0), ay + (by - ay) * t + rng.uniform(-80.0, 80.0)
            actor = spawn_crater("%sPOI_scar_%d_%02d" % (PREFIX, k, m), FOLDER + "/POI/" + scar["kind"], x, y, 1.0,
                                 heading + rng.uniform(-6.0, 6.0), rng)
            comp = actor.get_component_by_class(unreal.DecalComponent)
            half_width = (scar["w0"] + (scar["w1"] - scar["w0"]) * t) * 0.55
            comp.set_editor_property("decal_size", unreal.Vector(CRATER_DEPTH, half_width, SCAR_DECAL_STEP * 0.75))
            count += 1
    for k, piece in enumerate(LAYOUT.get("trench_line", [])):
        # The Red Line pieces stand on the ground with their own earth berms; seat them by the visible mesh.
        actor = spawn_blueprint(ENV_BP + piece.get("path", "RedLine/" + piece["bp"]), "%sPOI_trenches_L%02d" % (PREFIX, k),
                                FOLDER + "/POI/trenches", piece["x"], piece["y"], ground_z(piece["x"], piece["y"]), piece["yaw"])
        # The dugout's dome sits half in the ground, like a dug-in shelter, not a tent on top of it.
        seat_on_visible_mesh(actor, TRENCH_PIECE_SINK.get(piece["bp"], 4.0))
        # The kit's earth is a reddish desert stone: give it this ground's soil so the berms belong to the pad.
        earth = unreal.load_asset(TRENCH_EARTH)
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            comp.set_material_by_name("RL_Earth", earth)
        count += 1
    for k, trench in enumerate(LAYOUT.get("trenches", [])):
        # Seated by pivot just under the ground: the landscape hides the block's flat top, its cut goes into the ground
        # dug out for it.
        spawn_blueprint(ENV_BP + "Trenches/" + trench["name"], "%sPOI_trenches_T%d" % (PREFIX, k), FOLDER + "/POI/trenches",
                        trench["x"], trench["y"], trench["z"] - TRENCH_SINK, trench["yaw"])
        count += 1
    log("poi actors %d" % count)
    return count


def set_linear_spline(spline, points, closed):
    spline.clear_spline_points(False)
    for point in points:
        spline.add_spline_point(point, unreal.SplineCoordinateSpace.LOCAL, False)
    for k in range(len(points)):
        spline.set_spline_point_type(k, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(closed, False)
    spline.update_spline()
    # Without this flag the construction script puts the Blueprint's default spline back.
    spline.set_editor_property("spline_has_been_edited", True)


def fence_round(corners, gate_edge, gate_width=FIELD_GATE_WIDTH):
    """World points every FIELD_FENCE_STEP round the field, open at a gate in the middle of one edge."""
    n = len(corners)
    a, b = corners[gate_edge], corners[(gate_edge + 1) % n]
    edge = math.dist(a, b)
    t1, t0 = 0.5 + gate_width / 2.0 / edge, 0.5 - gate_width / 2.0 / edge
    path = [(a[0] + (b[0] - a[0]) * t1, a[1] + (b[1] - a[1]) * t1)] + [corners[(gate_edge + 1 + k) % n] for k in range(n)]
    path.append((a[0] + (b[0] - a[0]) * t0, a[1] + (b[1] - a[1]) * t0))
    points = []
    for p, q in zip(path[:-1], path[1:]):
        steps = max(1, math.ceil(math.dist(p, q) / FIELD_FENCE_STEP))
        points += [(p[0] + (q[0] - p[0]) * k / steps, p[1] + (q[1] - p[1]) * k / steps) for k in range(steps)]
    return points + [path[-1]]


def step_fields():
    """The farmstead's crop fields (crushable crops) with a fence and a gate each; the generator keeps them clear."""
    require_map()
    clear_prefix(PREFIX + "Field")
    report = []
    for k, field in enumerate(LAYOUT.get("fields", [])):
        corners = [tuple(c) for c in field["corners"]]
        cx, cy = field["x"], field["y"]
        origin = unreal.Vector(cx, cy, ground_z(cx, cy))
        crop = spawn_blueprint(FIELD_ROOT + field["bp"], "%sField_%d" % (PREFIX, k), FOLDER + "/Fields", cx, cy, origin.z,
                               field["yaw"])
        c, s = math.cos(math.radians(-field["yaw"])), math.sin(math.radians(-field["yaw"]))
        local = [unreal.Vector((x - cx) * c - (y - cy) * s, (x - cx) * s + (y - cy) * c, 0.0) for x, y in corners]
        set_linear_spline(crop.get_editor_property("field_outline"), local, True)
        layout = crop.get_editor_property("layout")
        layout.set_editor_property("random_seed", 1000 + 37 * k)
        # Setting a property reruns the construction script.
        crop.set_editor_property("layout", layout)
        points = fence_round(corners, field["seed"] % 4)
        fx, fy = points[0]
        base = ground_z(fx, fy)
        fence = spawn_blueprint(FIELD_ROOT + "BP_DestSpline_FarmFence", "%sField_Fence_%d" % (PREFIX, k), FOLDER + "/Fields",
                                fx, fy, base, 0.0)
        set_linear_spline(fence.get_editor_property("spline"),
                          [unreal.Vector(x - fx, y - fy, ground_z(x, y) - base) for x, y in points], False)
        fence.set_editor_property("piece_health", fence.get_editor_property("piece_health"))
        report.append((field["bp"], crop.get_num_plants(), len(points) - 1))
    report.append(("yard fence", place_yard_fence()))
    log("fields %s" % report)
    return report


def convex_hull(points):
    points = sorted(set(points))
    if len(points) < 3:
        return points

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def place_yard_fence():
    """A farm fence round the farmstead's buildings, open towards the road; the orchard stays outside."""
    poi = next(q for q in LAYOUT["pois"] if q["kind"] == "farmstead")
    corners = []
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if not label.startswith(PREFIX + "POI_farmstead_") or "OrchardTree" in actor.get_class().get_name():
            continue
        if "WpoLeafTree" in actor.get_class().get_name() or "DeadTreeLog" in actor.get_class().get_name():
            continue
        box = visible_footprint(actor)
        if box is None:
            continue
        margin = YARD_FENCE_MARGIN
        corners += [(box[0] - margin, box[1] - margin), (box[2] + margin, box[1] - margin),
                    (box[2] + margin, box[3] + margin), (box[0] - margin, box[3] + margin)]
    hull = convex_hull([(round(x), round(y)) for x, y in corners])
    a = math.radians(poi["yaw"])
    road = (poi["x"] + 3600.0 * math.sin(a) * poi["side"], poi["y"] - 3600.0 * math.cos(a) * poi["side"])
    mids = [((hull[k][0] + hull[(k + 1) % len(hull)][0]) / 2.0, (hull[k][1] + hull[(k + 1) % len(hull)][1]) / 2.0)
            for k in range(len(hull))]
    gate = min(range(len(hull)), key=lambda k: math.dist(mids[k], road))
    points = fence_round(hull, gate, YARD_GATE_WIDTH)
    fx, fy = points[0]
    base = ground_z(fx, fy)
    fence = spawn_blueprint(FIELD_ROOT + "BP_DestSpline_FarmFence", PREFIX + "Field_YardFence", FOLDER + "/Fields", fx, fy, base, 0.0)
    set_linear_spline(fence.get_editor_property("spline"), [unreal.Vector(x - fx, y - fy, ground_z(x, y) - base) for x, y in points],
                      False)
    fence.set_editor_property("piece_health", fence.get_editor_property("piece_health"))
    return len(points) - 1


TWO_TRACK = ENV_BP + "Splines/GroundPath/BP_GroundPathSpline_TwoTrack"
RUT_RIBBONS = ("old_road",)
PATH_POINT_STEP = 700.0
# Old shell craters on the plateau: (x, y) clear of the centre and the lanes' hubs.
BASE_CRATERS = [(3900.0, -2600.0), (-4700.0, 1800.0), (2200.0, 4900.0), (-1500.0, -5200.0), (5600.0, 1400.0)]


def thin_points(points, step):
    out = [points[0]]
    for p in points[1:]:
        if math.dist(p, out[-1]) >= step:
            out.append(p)
    if math.dist(points[-1], out[-1]) > 1.0:
        out.append(points[-1])
    return out


def spawn_crater(label, folder, x, y, size, yaw, rng):
    """A plain decal actor with one of the brown Kessel crater instances. The crater Blueprint picks its material again
    every time the map loads, so a tint set on it never reaches the game."""
    actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, ground_z(x, y)),
                                                unreal.Rotator(0.0, -90.0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    comp = actor.get_component_by_class(unreal.DecalComponent)
    comp.set_editor_property("decal_size", unreal.Vector(CRATER_DEPTH, CRATER_HALF_SIZE * size, CRATER_HALF_SIZE * size))
    comp.set_decal_material(unreal.load_asset(KESSEL_CRATER % ("%02d" % rng.randint(1, 6))))
    return actor


def place_two_track(label, points):
    first = points[0]
    actor = spawn_blueprint(TWO_TRACK, label, FOLDER + "/Paths", first[0], first[1], ground_z(*first) + 100.0, 0.0)
    spline = actor.get_component_by_class(unreal.SplineComponent)
    # The Blueprint snaps each tile by tracing down from the spline: every point starts above the ground.
    spline.set_spline_points([unreal.Vector(x, y, ground_z(x, y) + 100.0) for x, y in points],
                             unreal.SplineCoordinateSpace.WORLD, True)
    # Without this flag the construction script resets the spline to its default.
    spline.set_editor_property("spline_has_been_edited", True)
    width = actor.get_editor_property("PathWidth")
    # Re-setting the same width does not rerun construction; nudge it.
    actor.set_editor_property("PathWidth", width + 1.0)
    actor.set_editor_property("PathWidth", width)
    return len(actor.get_components_by_class(unreal.SplineMeshComponent))


def step_paths():
    """Wheel ruts (two-track ribbons) on the convoy's old road, the farm and waterworks tracks and the base lanes, plus a
    few old shell craters on the plateau."""
    require_map()
    clear_prefix(PREFIX + "Path")
    report = {}
    rim = LAYOUT["plateau_rim"]
    for route in LAYOUT["routes"]:
        name = route["name"]
        points = [tuple(p) for p in route["points"]]
        # Only the convoy's old road: from the RTS camera the ribbons' far mips read as near-black strokes, so the base
        # lanes and yard tracks are paint only.
        if name not in RUT_RIBBONS:
            continue
        if len(points) < 2:
            continue
        report[name] = place_two_track("%sPath_%s" % (PREFIX, name), thin_points(points, PATH_POINT_STEP))
    rng = random.Random(7)
    for k, (x, y) in enumerate(BASE_CRATERS):
        # A cluster per spot: one big crater and one or two smaller ones from the same salvo.
        for m in range(rng.randint(2, 3)):
            cx, cy = x + rng.uniform(-900.0, 900.0) * (m > 0), y + rng.uniform(-900.0, 900.0) * (m > 0)
            size = rng.uniform(0.9, 1.3) if m == 0 else rng.uniform(0.45, 0.8)
            spawn_crater("%sPath_Crater_%d_%d" % (PREFIX, k, m), FOLDER + "/Paths", cx, cy, size, rng.uniform(0.0, 360.0), rng)
    log("paths %s" % report)
    return report


def step_poi_overlap_report():
    """Pairs of point-of-interest actors whose visible footprints overlap, and anything on a road."""
    require_map()
    boxes = []
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX + "POI") and "Crater" not in actor.get_class().get_name():
            box = visible_footprint(actor)
            if box is not None:
                boxes.append((label, box))
    clashes = []
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            a, b = boxes[i][1], boxes[j][1]
            if a[0] < b[2] - 40 and b[0] < a[2] - 40 and a[1] < b[3] - 40 and b[1] < a[3] - 40:
                clashes.append((boxes[i][0][-14:], boxes[j][0][-14:]))
    log("poi overlaps %s" % clashes)
    return clashes


TRENCH_EARTH = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_Kessel_Soil"
TRENCH_PIECE_SINK = {"BP_RL_Dugout": 90.0}
KESSEL_CRATER = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/MI_Kessel_Crater_%s"
# The crater Blueprint's decal box (half extents: depth, then across).
CRATER_DEPTH = 128.0
CRATER_HALF_SIZE = 256.0
SCAR_DECAL_STEP = 900.0
TRENCH_PROFILE_STEP = 50.0
TRENCH_SINK = 8.0


def profile_trench_blueprints(names, path):
    """Height of each trench Blueprint's ground block on a grid in its own frame, so the terrain can be dug under it."""
    require_map()
    profiles = {}
    try:
        profile_each_trench(names, profiles)
    finally:
        # The probe floats 200 m over the start; one left behind is where the HQ lands.
        clear_prefix(PREFIX + "TrenchProfile")
    with open(path, "w") as handle:
        json.dump(profiles, handle)
    return {k: (v["nx"], v["ny"], min(z for z in v["z"] if z is not None)) for k, v in profiles.items()}


def profile_each_trench(names, profiles):
    for name in names:
        actor = spawn_blueprint(ENV_BP + "Trenches/" + name, PREFIX + "TrenchProfile", FOLDER, 0.0, 0.0, 20000.0, 0.0)
        meshes = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh is not None]
        box = visible_footprint(actor)
        x0, y0 = math.floor(box[0] / TRENCH_PROFILE_STEP) * TRENCH_PROFILE_STEP, math.floor(box[1] / TRENCH_PROFILE_STEP) * TRENCH_PROFILE_STEP
        nx, ny = int((box[2] - x0) / TRENCH_PROFILE_STEP) + 2, int((box[3] - y0) / TRENCH_PROFILE_STEP) + 2
        heights = []
        for j in range(ny):
            for i in range(nx):
                x, y = x0 + i * TRENCH_PROFILE_STEP, y0 + j * TRENCH_PROFILE_STEP
                best = None
                for comp in meshes:
                    hit = comp.line_trace_component(unreal.Vector(x, y, 21000.0), unreal.Vector(x, y, 19000.0), True, False, False)
                    # None when the trace misses, else (location, normal, bone, hit result).
                    if hit is not None:
                        z = hit[0].z - 20000.0
                        best = z if best is None else max(best, z)
                heights.append(None if best is None else round(best, 1))
        profiles[name] = dict(x0=x0, y0=y0, nx=nx, ny=ny, step=TRENCH_PROFILE_STEP, z=heights)
        actor.destroy_actor()


def step_poi_footprints(path):
    """Visible footprint and pivot offset of every point-of-interest actor, to lay the scenes out by real sizes."""
    rows = []
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX + "POI") and "Crater" not in actor.get_class().get_name():
            box = visible_footprint(actor)
            loc = actor.get_actor_location()
            if box is not None:
                rows.append((label[len(PREFIX) + 4:], actor.get_class().get_name(), round(box[2] - box[0]), round(box[3] - box[1]),
                             round(actor.get_actor_rotation().yaw)))
    with open(path, "w") as handle:
        json.dump(rows, handle)
    return len(rows)


def step_poi_overlap_file(path):
    with open(path, "w") as handle:
        json.dump(step_poi_overlap_report(), handle)
