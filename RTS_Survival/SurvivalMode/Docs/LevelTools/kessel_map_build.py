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

import unreal

MAP_PATH = "/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Kessel/terrain/SM_Kessel_01_layout.json"
LANDSCAPE_MATERIAL = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Inst"
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
POLE_BLUEPRINT = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric1"
SANDBAG_WALL = "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_SandBagWall_FieldConstruction"
# The blueprint's nav blocker is 400 cm deep round a 180 cm mesh. Infantry take cover against the bags, and a unit standing in
# the blocker has no start poly: every path from there fails and the squad stays pinned. Fit it to the mesh.
SANDBAG_NAV_EXTENT = (95.0, 450.0, 100.0)
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


def step_trees():
    require_map()
    clear_prefix(PREFIX + "Tree")
    count = 0
    for k, tree in enumerate(LAYOUT["trees"]):
        z = ground_z(tree["x"], tree["y"])
        spawn_blueprint(TREE_BLUEPRINTS[tree["kind"]], "%sTree_%03d" % (PREFIX, k), FOLDER + "/Trees", tree["x"], tree["y"],
                        (z or tree["z"]) - 10.0, (k * 137.5) % 360.0)
        count += 1
    log("trees %d" % count)
    return count


def step_sandbags():
    """German field construction walls: infantry garrison them, so the defenders fight from behind the bags."""
    require_map()
    clear_prefix(PREFIX + "Sandbag")
    count = 0
    for k, bag in enumerate(LAYOUT["sandbags"]):
        z = ground_z(bag["x"], bag["y"])
        wall = spawn_blueprint(SANDBAG_WALL, "%sSandbag_%s_%02d" % (PREFIX, bag["group"], k), FOLDER + "/Sandbags", bag["x"], bag["y"],
                               z or LAYOUT["core"]["z"], bag["yaw"])
        for comp in wall.get_components_by_class(unreal.NavModifierComponent):
            comp.set_editor_property("failsafe_extent", unreal.Vector(*SANDBAG_NAV_EXTENT))
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
