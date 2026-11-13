"""In-editor build steps for Ger_Berlin (map creation only: no mission scripting).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap")
    import berlin_map_build as bmb; importlib.reload(bmb)
    bmb.load_layout(); bmb.step_duplicate(); bmb.step_strip(); bmb.step_landscape(); bmb.step_fit_level_actors()

Layout and raw files come from berlin_terrain_gen.py (Saved/Berlin/terrain). Generated actors carry the BER_ label
prefix and sit under the outliner folder Berlin/, so a step can clear and redo its own part.
"""
import array
import json
import math

import unreal

TEMPLATE_MAP = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap"
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/terrain/Ger_Berlin_layout.json"
LANDSCAPE_MATERIAL = "/Game/RTS_Survival/Materials/ground/Landscape/Berlin/MI_RTS_Landscape_Berlin"
LAYER_INFO_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/Berlin/DATA/"
LANDSCAPE_LABEL = "Berlin_Landscape"
RVT_ROOT = "/Game/RTS_Survival/Meshes/RVTs/"
PREFIX = "BER_"
Z_LOW, Z_HIGH = -1400.0, 3600.0

LAYOUT = {}
HEIGHTS = None


def log(text):
    unreal.log_warning("BERLIN_MAP " + str(text))
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


# ---- map file ---------------------------------------------------------------------------------------------------
def step_duplicate():
    """Copy the template map to Ger_Berlin (once) and open it."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
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
    infos = [unreal.load_asset(LAYER_INFO_ROOT + name + "_LayerInfo") for name in names]
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
    landscape.set_folder_path("Berlin/Terrain")
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
    start = feature("start")["points"][0]
    start_z = ground_z(start[0], start[1])
    actors = by_label()
    landscape_actor = actors.get(LANDSCAPE_LABEL)
    blockers = []
    report = {}
    for actor in actors.values():
        name = actor.get_class().get_name()
        if name == "BP_FogOfWar_Manager_C":
            # The fog grid is a square round the manager: it has to span the long (east-west) side.
            actor.set_actor_location(unreal.Vector(0.0, 0.0, 100.0), False, False)
            actor.set_editor_property("map_extent", max(half_x, half_y))
            report["fow_extent"] = max(half_x, half_y)
        elif name == "NavMeshBoundsVolume":
            _set_box(actor, 0.0, 0.0, (Z_HIGH + Z_LOW) / 2.0, half_x + 200.0, half_y + 200.0, (Z_HIGH - Z_LOW) / 2.0)
            # the nav data keeps the template's old bounds until it is told the volume moved
            unreal.NavigationSystemV1.get_navigation_system(world()).on_navigation_bounds_updated(actor)
        elif name == "BlockingVolume":
            blockers.append(actor)
        elif name == "RTS_PLayerStart_C":
            actor.set_actor_location(unreal.Vector(start[0], start[1], start_z + 43.0), False, False)
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, -90.0), False)   # facing west, into the city
        elif name == "PlayerStart":
            actor.set_actor_location(unreal.Vector(start[0], start[1], start_z + 100.0), False, False)
        elif name in ("RTS_BP_EnemyController_C", "BP_MissionManager_C"):
            offset = 300.0 if name == "BP_MissionManager_C" else -300.0
            actor.set_actor_location(unreal.Vector(start[0] + 2500.0, start[1] + offset, start_z + 1500.0), False, False)
        elif name in ("DirectionalLight", "SkyLight", "ExponentialHeightFog", "PostProcessVolume"):
            actor.set_actor_location(unreal.Vector(0.0, 0.0, 3000.0), False, False)
    # boundary blockers just outside the play area
    thick = 600.0
    spots = [("BoundaryTop", half_x + thick, 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryBottom", -(half_x + thick), 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryRight", 0.0, half_y + thick, half_x + 2.0 * thick, thick),
             ("BoundaryLeft", 0.0, -(half_y + thick), half_x + 2.0 * thick, thick)]
    for actor, (label, x, y, ex, ey) in zip(sorted(blockers, key=lambda a: a.get_actor_label()), spots):
        _set_box(actor, x, y, 1500.0, ex, ey, 5000.0)
        actor.set_actor_label(label)
        actor.set_folder_path("MapBoundaries")
    report["blockers"] = len(blockers)
    # runtime virtual texture volumes: the box must cover exactly the landscape (as on the Marder map)
    clear_prefix("RVT_Env")
    for label, texture in (("RVT_Env", "RVT_Environment"), ("RVT_Env2", "RVT_Height")):
        volume = actors_sub().spawn_actor_from_class(unreal.RuntimeVirtualTextureVolume,
                                                     unreal.Vector(lay["corner"][0], lay["corner"][1], Z_LOW))
        volume.set_actor_scale3d(unreal.Vector(lay["size_x"], lay["size_y"], Z_HIGH - Z_LOW))
        volume.set_actor_label(label)
        volume.set_folder_path("Berlin/Terrain")
        comp = volume.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
        comp.set_editor_property("virtual_texture", unreal.load_asset(RVT_ROOT + texture))
        comp.set_editor_property("bounds_align_actor", landscape_actor)
        report[label] = texture
    log("fit %s" % report)
    return report


# ---- dressing -------------------------------------------------------------------------------------------------------
DRESS_FILE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/terrain/Ger_Berlin_dress.json"
REICHSTAG_BP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/BP_ReichstagRuin_B2"
GATE_BP = "/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/BP_BrandenburgGateRuin"
GATE_MESH = "/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/SM_BrandenburgGate_Ruin"
FLAK_ROOT = "/Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower/"
TANK_BLOCKER = "/Game/RTS_Survival/Blueprints/Nav/BlockingActors/BP_NavBlocker_VolumeBlockTanks"
RAIL_MESH = "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01"
WATER_MATERIAL = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
FLAT_LITTER = ("DBK_Concrete_Rubble_Small_A", "DBK_Concrete_Rubble_Small_B", "DBK_Concrete_Rubble_Small_C",
               "DBK_Concrete_Rubble_Small_D", "DBK_Brick_Rubble_C", "DBK_Brick_Rubble_D")


def _instance_actor(label, folder, mesh, transforms, collision=True, material=None, cpd=None):
    """One actor holding one hierarchical instanced mesh component: thousands of kit pieces, a handful of actors."""
    actor = actors_sub().spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
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
        # custom primitive data 0-2 tint, 3 soot of the Berlin ruin material; one value per component
        comp.set_default_custom_primitive_data_vector4(0, unreal.Vector4(*cpd))
    comp.add_instances(transforms, False, True)
    return actor


# loose rubble heaps: cool, darker tints with more soot, so heaps of one mesh do not all glow the same brick pink
RUBBLE_TINTS = ((0.60, 0.64, 0.68, 1.30), (0.70, 0.70, 0.70, 1.10), (0.52, 0.55, 0.58, 1.35), (0.78, 0.74, 0.70, 0.95))
# a little darker than the ground they lie on: pale skirts read as sandbag rings from above
FOOT_TINTS = ((0.78, 0.78, 0.78, 1.1), (0.68, 0.70, 0.72, 1.3), (0.85, 0.82, 0.79, 1.0), (0.62, 0.64, 0.66, 1.35))


def foot_material():
    """The rubble instance with darker, thinner dust: the skirts fade to dust at their rim, and the kit's pale dust
    colour made them read as light rings on the ground from above."""
    path = RUIN_KIT + "Materials/MI_BerlinRuin_Rubble_Foot"
    mel = unreal.MaterialEditingLibrary
    mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_BerlinRuin_Rubble_Foot", RUIN_KIT + "Materials", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(RUIN_KIT + "Materials/MI_BerlinRuin_Rubble"))
    mel.set_material_instance_vector_parameter_value(mi, "RuinDustColor", unreal.LinearColor(0.115, 0.102, 0.09, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "RuinDustOpacity", 0.6)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def step_feet():
    """Modelled footprint skirts under the ruins (owner's pick A): instanced, no collision, no nav effect."""
    require_map()
    with open(DRESS_FILE, "r") as handle:
        feet = json.load(handle).get("feet", [])
    removed = clear_prefix("FOOT_")
    material = foot_material()
    groups = {}
    heaps = {"SM_BerlinFoot_CollapseHeap_S": [], "SM_BerlinFoot_CollapseHeap_L": []}
    for mesh, x, y, z, yaw, scale, bucket in feet:
        if mesh in heaps:
            heaps[mesh].append([x, y, z, yaw, 0.0, 0.0, scale])     # collapse heaps are natural cover actors
            continue
        groups.setdefault((mesh, bucket), []).append([x, y, z, yaw, 0.0, 0.0, scale])
    heap_cover = _heap_cover_actors("Collapse_S", heaps["SM_BerlinFoot_CollapseHeap_S"], "FOOT_HeapCover_", 0, FOOT_TINTS)
    heap_cover += _heap_cover_actors("Collapse_L", heaps["SM_BerlinFoot_CollapseHeap_L"], "FOOT_HeapCover_", heap_cover, FOOT_TINTS)
    log("feet: collapse heap cover actors %d" % heap_cover)
    missing = set()
    for (name, bucket), items in sorted(groups.items()):
        mesh = unreal.load_asset(RUIN_KIT + "Footprints/" + name)
        if mesh is None:
            missing.add(name)
            continue
        _instance_actor("FOOT_%s_%d" % (name.replace("SM_BerlinFoot_", ""), bucket), "Berlin/Ruins/Footprints", mesh,
                        [_transform(i) for i in items], collision=False, material=material, cpd=FOOT_TINTS[bucket])
    log("feet: removed %d, placed %d in %d components, missing %s" % (removed, len(feet), len(groups), sorted(missing)))
    return len(feet)


def _transform(item):
    x, y, z, yaw, pitch, roll, scale = item
    return unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll, pitch, yaw), unreal.Vector(scale, scale, scale))


PARTS_DONOR = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts1"


def _add_vehicle_parts(actor):
    """Per-instance: a scavengeable tank wreck also yields vehicle parts (its Blueprint gives metal and blueprints only).
    The reward entry is copied from the vehicle-parts pile, so key and struct types always match."""
    try:
        donor = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PARTS_DONOR))
        donor_map = donor.get_editor_property("scavenge_resources")
        rewards = actor.get_editor_property("scavenge_resources")
        for key in donor_map.keys():
            if "VehicleParts" in str(key) or "VEHICLE_PARTS" in str(key).upper():
                value = donor_map[key]
                value.set_editor_property("is_set_amount", False)
                value.set_editor_property("min", 40)
                value.set_editor_property("max", 80)
                rewards[key] = value
        actor.set_editor_property("scavenge_resources", rewards)
        return True
    except Exception as error:
        log("vehicle parts not added to %s: %s" % (actor.get_actor_label(), error))
        return False


FURNITURE_BP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Blueprints/BP_BerlinFurn_"


def step_furniture():
    """Crushable household wreckage (BP_BerlinFurn_<NN>_<Name>, any tank crushes it), labels FURN_*."""
    require_map()
    with open(DRESS_FILE, "r") as handle:
        items = json.load(handle).get("furniture", [])
    removed = clear_prefix("FURN_")
    classes, missing, placed = {}, set(), 0
    for index, (name, x, y, z, yaw) in enumerate(items):
        if name not in classes:
            path = FURNITURE_BP + name
            classes[name] = unreal.EditorAssetLibrary.load_blueprint_class(path)                 if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if classes[name] is None:
            missing.add(name)
            continue
        actor = actors_sub().spawn_actor_from_class(classes[name], unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label("FURN_%s_%03d" % (name, index))
        actor.set_folder_path("Berlin/Furniture")
        placed += 1
    log("furniture: removed %d, placed %d, missing %s" % (removed, placed, sorted(missing)))
    return placed


# Loose rubble heaps as natural cover (ruinkit/heap_cover_build.py): kit heap mesh -> BP_BerlinCoverHeap_<key>
HEAP_COVER = {"DBK_Brick_Rubble_A": "Brick", "DBK_Cinderblock_Rubble_A": "Block", "DBK_Concrete_Rubble_Mid_A": "Concrete_A",
              "DBK_Concrete_Rubble_Mid_B": "Concrete_B", "DBK_Concrete_Rubble_Mid_C": "Concrete_C"}
HEAP_COVER_MIN_SCALE = 0.8      # smaller heaps are too low to lie behind; they stay instanced dressing
HEAP_COVER_BP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCoverHeap_"


def _heap_cover_actors(key, items, label_prefix, start, tints=None):
    """One BP_BerlinCoverHeap_<key> actor per item [x, y, z, yaw, pitch, roll, scale]; returns how many were placed."""
    if not items:
        return 0
    cls = unreal.EditorAssetLibrary.load_blueprint_class(HEAP_COVER_BP + key)
    if cls is None:
        log("missing heap cover blueprint " + key)
        return 0
    tints = tints or RUBBLE_TINTS
    for index, (x, y, z, yaw, pitch, roll, scale) in enumerate(items):
        actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll, pitch, yaw))
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        if comp is not None:
            comp.set_default_custom_primitive_data_vector4(0, unreal.Vector4(*tints[(start + index) % len(tints)]))
        actor.set_actor_label("%s%s_%03d" % (label_prefix, key, start + index))
        actor.set_folder_path("Berlin/Cover/Heaps")
    return len(items)


def step_dress():
    """Ruins, rubble, trees, wrecks and the rail yard from berlin_dress_gen.py."""
    require_map()
    with open(DRESS_FILE, "r") as handle:
        dress = json.load(handle)
    removed = clear_prefix(PREFIX)
    materials = step_ruin_materials()
    count = 0
    heap_cover = 0
    for mesh_path, items in sorted(dress["ism"].items()):
        mesh = unreal.load_asset(mesh_path)
        if mesh is None:
            log("missing mesh " + mesh_path)
            continue
        name = mesh_path.rsplit("/", 1)[1]
        # flat street litter is walked and driven over: no collision, no nav cost
        if name in HEAP_COVER:
            # heaps big enough to lie behind are natural cover actors (destructible, prone sockets all round)
            cover_items = [i for i in items if i[6] >= HEAP_COVER_MIN_SCALE]
            items = [i for i in items if i[6] < HEAP_COVER_MIN_SCALE]
            heap_cover += _heap_cover_actors(HEAP_COVER[name], cover_items, PREFIX + "HeapCover_", heap_cover)
        if "Rubble" in name and name not in FLAT_LITTER:
            for k, tint in enumerate(RUBBLE_TINTS):
                part = items[k::len(RUBBLE_TINTS)]
                if part:
                    _instance_actor(PREFIX + "ISM_%s_%d" % (name, k), "Berlin/Ruins", mesh, [_transform(i) for i in part],
                                    material=ruin_material_for(name, materials), cpd=tint)
        else:
            _instance_actor(PREFIX + "ISM_" + name, "Berlin/Ruins", mesh, [_transform(i) for i in items],
                            collision=name not in FLAT_LITTER, material=ruin_material_for(name, materials))
        count += len(items)
    missing = []
    for entry in dress["actors"]:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(entry["bp"])
        if cls is None:
            missing.append(entry["bp"])
            continue
        actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(entry["x"], entry["y"], entry["z"]),
                                                    unreal.Rotator(0.0, 0.0, entry["yaw"]))
        actor.set_actor_label(entry["label"])
        actor.set_folder_path(entry["folder"])
        if entry.get("vehicle_parts"):
            _add_vehicle_parts(actor)
        s = entry["scale"]
        if s != [1.0, 1.0, 1.0]:
            actor.set_actor_scale3d(unreal.Vector(s[0], s[1], s[2]))
    blocker_class = unreal.EditorAssetLibrary.load_blueprint_class(TANK_BLOCKER)
    for entry in dress["tank_blockers"]:
        z = ground_z(entry["x"], entry["y"])
        actor = actors_sub().spawn_actor_from_class(blocker_class, unreal.Vector(entry["x"], entry["y"], z + 100.0),
                                                    unreal.Rotator(0.0, 0.0, entry["yaw"]))
        actor.set_actor_scale3d(unreal.Vector(entry["length"] / 64.0, entry["width"] / 64.0, 8.0))
        actor.set_actor_label(entry["label"])
        actor.set_folder_path("Berlin/Nav")
    log("dress: heap cover actors %d" % heap_cover)
    log("dress: removed %d, instances %d, actors %d (missing %s), tank blockers %d" % (
        removed, count, len(dress["actors"]), sorted(set(missing)), len(dress["tank_blockers"])))
    return count


def _resample(points, step):
    out = []
    for a, b in zip(points[:-1], points[1:]):
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(round(length / step)))
        t = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
        for i in range(n):
            out.append(((a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n), t))
    return out


def step_water_and_rails():
    require_map()
    clear_prefix("BERW_")
    lay = LAYOUT["landscape"]
    # The Spree: one plane under the whole north strip. Only the river trench is lower than the water level.
    x_low = 12000.0
    x_high = lay["size_x"] / 2.0
    plane = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor,
                                                unreal.Vector((x_low + x_high) / 2.0, 0.0, LAYOUT["water_z"]))
    comp = plane.static_mesh_component
    comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    comp.set_material(0, unreal.load_asset(WATER_MATERIAL))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_editor_property("cast_shadow", False)
    plane.set_actor_scale3d(unreal.Vector((x_high - x_low) / 100.0, lay["size_y"] / 100.0, 1.0))
    plane.set_actor_label("BERW_Spree_Water")
    plane.set_folder_path("Berlin/Terrain")
    # rails: scenery track panels along the rail lines of the sketch
    mesh = unreal.load_asset(RAIL_MESH)
    box = mesh.get_bounding_box()
    size = (box.max.x - box.min.x, box.max.y - box.min.y)
    along_x = size[0] >= size[1]
    length = max(size)
    centre = ((box.max.x + box.min.x) / 2.0, (box.max.y + box.min.y) / 2.0)
    transforms = []
    lines = [p["points"] for p in LAYOUT["paths"] if p["kind"] == "rail"] + [YARD_SPUR]
    for points in lines:
        for (pt, t) in _resample(points, length * 0.985):
            yaw = math.degrees(math.atan2(t[1], t[0])) + (0.0 if along_x else -90.0)
            mid = (pt[0] + t[0] * length / 2.0, pt[1] + t[1] * length / 2.0)
            if abs(mid[0]) > lay["size_x"] / 2.0 - 300.0 or abs(mid[1]) > lay["size_y"] / 2.0 - 300.0:
                continue
            rad = math.radians(yaw)
            ox = centre[0] * math.cos(rad) - centre[1] * math.sin(rad)
            oy = centre[0] * math.sin(rad) + centre[1] * math.cos(rad)
            transforms.append(unreal.Transform(
                # The panel's ballast skirt reaches 93 cm under its pivot: sunk so the ballast top is at ground level and the
                # rail top about 24 cm above it (the unloading ramp's deck height assumes this).
                unreal.Vector(mid[0] - ox, mid[1] - oy, ground_z(mid[0], mid[1]) - 35.0),
                unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0)))
    _instance_actor("BERW_Rails", "Berlin/RailYard", mesh, transforms, collision=False)
    log("water plane + %d rail panels (panel %.0f cm, along %s)" % (len(transforms), length, "X" if along_x else "Y"))
    return len(transforms)


def _note(label, x, y, text):
    note = actors_sub().spawn_actor_from_class(unreal.Note, unreal.Vector(x, y, ground_z(x, y) + 250.0))
    note.set_actor_label(label)
    note.set_folder_path("Berlin/POI")
    try:
        note.set_editor_property("text", text)
    except Exception:
        pass
    return note


def step_poi():
    """The landmark objects other sessions built (where they are in the project) and labelled stand-ins for the rest."""
    require_map()
    clear_prefix("POI_")
    report = {}
    rx, ry = LAYOUT["reichstag_at"]
    cls = unreal.EditorAssetLibrary.load_blueprint_class(REICHSTAG_BP)
    if cls is not None:
        # The kit is laid out x south, y west with the portico on its west side: yaw 90 turns the portico south,
        # toward the crater and the Gate.
        actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(rx, ry, ground_z(rx, ry)),
                                                    unreal.Rotator(0.0, 0.0, 90.0))
        actor.set_actor_label("POI_ReichstagRuin")
        actor.set_folder_path("Berlin/POI")
        origin, extent = actor.get_actor_bounds(False)
        actor.add_actor_world_offset(unreal.Vector(rx - origin.x, ry - origin.y, 0.0), False, False)
        origin, extent = actor.get_actor_bounds(False)
        report["reichstag"] = [round(origin.x), round(origin.y), round(extent.x), round(extent.y)]
    gx, gy = LAYOUT["gate_at"]
    mesh = unreal.load_asset(GATE_MESH)
    if mesh is not None:
        box = mesh.get_bounding_box()
        # The destructible Blueprint (health, shell holes, sink death) when it exists, else the plain mesh.
        gate_class = unreal.EditorAssetLibrary.load_blueprint_class(GATE_BP) if             unreal.EditorAssetLibrary.does_asset_exist(GATE_BP) else None
        if gate_class is not None:
            gate = actors_sub().spawn_actor_from_class(gate_class, unreal.Vector(0.0, 0.0, 0.0))
        else:
            gate = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
            gate.static_mesh_component.set_static_mesh(mesh)
        # long axis (mesh X) north-south: the passages then run east-west, as the central lane does
        gate.set_actor_location(unreal.Vector(gx - (box.max.x + box.min.x) / 2.0, gy - (box.max.y + box.min.y) / 2.0,
                                              ground_z(gx, gy) - box.min.z - 5.0), False, False)
        gate.set_actor_label("POI_BrandenburgGateRuin")
        gate.set_folder_path("Berlin/POI")
        origin, extent = gate.get_actor_bounds(False)
        report["gate"] = [round(origin.x), round(origin.y), round(extent.x), round(extent.y)]
    # Flak towers (imported by poi/flak_tower_import.py): the mostly intact one at Friedrichshain with its front to
    # the south, toward the start; the wrecked one at the Zoo with its front to the east, toward the approach.
    towers = unreal.EditorAssetLibrary.does_asset_exist(FLAK_ROOT + "SM_BF_FlakTower_Damaged")
    for label, fid, mesh_name, yaw in (("POI_FlakTower_Friedrichshain", "fhain_tower", "SM_BF_FlakTower_LightDamage", 180.0),
                                       ("POI_FlakTower_Zoo", "zoo_tower", "SM_BF_FlakTower_Damaged", 90.0)):
        x, y = feature(fid)["points"][0]
        if towers:
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, ground_z(x, y)),
                                                        unreal.Rotator(0.0, 0.0, yaw))
            actor.static_mesh_component.set_static_mesh(unreal.load_asset(FLAK_ROOT + mesh_name))
            origin, extent = actor.get_actor_bounds(False)
            actor.add_actor_world_offset(unreal.Vector(x - origin.x, y - origin.y, 0.0), False, False)
        else:
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, ground_z(x, y) + 1350.0))
            actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
            actor.set_actor_scale3d(unreal.Vector(40.0, 34.0, 27.0))
            label = label.replace("POI_", "POI_PLACEHOLDER_")
        actor.set_actor_label(label)
        actor.set_folder_path("Berlin/POI")
        origin, extent = actor.get_actor_bounds(False)
        report[fid] = [round(origin.x), round(origin.y), round(extent.x), round(extent.y)]
    notes = [("forge_station", "Forge survey station (custom POI: sampling rigs, cryo gear)"),
             ("survey_data", "Survey data capture point"), ("demeter_pod", "Demeter pod (hidden bonus)"),
             ("yard_target", "Rail yard sabotage target (ramps + switch tower)"), ("zoo_bunker", "Zoo radio bunker"),
             ("alexanderplatz", "Alexanderplatz: Forge relay mast"), ("moltke_bridge", "Moltke bridge (half collapsed, 6 m)"),
             ("potsdamer_platz", "Potsdamer Platz"), ("anhalter_bf", "Anhalter Bf ruins"),
             ("response_rail", "Response force entry: rail (east)"), ("response_road", "Response force entry: road (east)")]
    for fid, text in notes:
        x, y = feature(fid)["points"][0]
        _note("POI_Note_" + fid, x, y, text)
    log("poi %s" % report)
    return report


def step_lighting():
    """The template is lit for the desert (orange fog, sunset sky cube). Berlin is ash-grey and overcast-cold."""
    require_map()
    report = []
    for actor in all_actors():
        name = actor.get_class().get_name()
        if name == "DirectionalLight":
            comp = actor.light_component
            comp.set_intensity(3.4)
            comp.set_light_color(unreal.LinearColor(0.93, 0.95, 1.0, 1.0))
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
            comp = actor.component
            comp.set_fog_inscattering_color(unreal.LinearColor(0.36, 0.39, 0.43, 1.0))
            comp.set_fog_density(0.0035)
            report.append("fog")
        elif name == "PostProcessVolume":
            settings = actor.settings
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.86, 0.86, 0.86, 1.0))
            settings.set_editor_property("override_white_temp", True)
            settings.set_editor_property("white_temp", 6900.0)
            actor.set_editor_property("settings", settings)
            report.append("post")
    log("lighting %s" % report)
    return report


# ---- ruin kit materials ---------------------------------------------------------------------------------------------
RUIN_MI_ROOT = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/"
# The kit's defaults are freshly painted interiors (navy, pink, red). Berlin's shells burned out thirty years ago:
# soot-grey plaster remnants and mostly bare masonry. (family, parent, vectors, switches, scalars)
RUIN_MATERIALS = {
    "Brick": ("/Game/DBK/Material_Instances/DBK_Brick_Instance_Default",
              {"Paint Color": (0.115, 0.108, 0.098), "Transition Color": (0.2, 0.19, 0.175), "Dirt Color": (0.02, 0.019, 0.018)},
              ["Paint Color", "Transition Color", "Dirt Color"], {"Damage Amount": 0.93, "Dirt Range": 0.62}),
    "Cinderblock": ("/Game/DBK/Material_Instances/DBK_Cinderblock_Instance_Default",
                    {"Paint Color": (0.19, 0.185, 0.17), "Dirt Color": (0.02, 0.019, 0.018)},
                    ["Paint Color", "Dirt Color"], {"Damage Multiplier": 2.8, "Paint": 0.3}),
    "Concrete": ("/Game/DBK/Material_Instances/DBK_Concrete_Instance_Default",
                 {"Paint Color": (0.2, 0.195, 0.18), "Bare Concrete": (0.33, 0.33, 0.31)},
                 ["Paint Color", "Bare Concrete"], {"Paint": 0.3}),
}


def step_ruin_materials():
    """Create (or refresh) MI_Berlin_Ruin_<Family>, children of the kit's default instances."""
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    out = {}
    for family, (parent, vectors, switches, scalars) in RUIN_MATERIALS.items():
        name = "MI_Berlin_Ruin_" + family
        path = RUIN_MI_ROOT + name
        mi = unreal.load_asset(path)
        if mi is None:
            mi = tools.create_asset(name, RUIN_MI_ROOT.rstrip("/"), unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, unreal.load_asset(parent))
        for switch in switches:
            mel.set_material_instance_static_switch_parameter_value(mi, switch, True)
        for key, (r, g, b) in vectors.items():
            mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(r, g, b, 1.0))
        for key, value in scalars.items():
            mel.set_material_instance_scalar_parameter_value(mi, key, value)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
        out[family] = mi
    return out


def ruin_material_for(mesh_name, materials):
    if "Rubble" in mesh_name:
        # loose rubble heaps and litter: the kit's one-sample rubble instance instead of the DBK rubble materials
        return unreal.load_asset("/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Rubble")
    if "Car" in mesh_name:
        return None
    for family in ("Cinderblock", "Brick"):
        if "DBK_%s_" % family in mesh_name:
            return materials.get(family)
    if mesh_name.startswith("DBK_Concrete_") or mesh_name.startswith("DBK_Concete_"):
        return materials.get("Concrete")
    return None


# ---- the custom POI objects built for this map (Source/Tools/BerlinMap/poi/*_import.py) ----------------------------
BERLIN_ENV = "/Game/RTS_Survival/Blueprints/Environment/Berlin/"
# The rail siding that serves the unloading ramp: from the yard centre west along the yard's long axis.
RAMP_TRACK_X = -1842.0
RAMP_AT = (RAMP_TRACK_X - 260.0, 13389.0)      # ramp pivot: its track centreline is 260 cm to its local -X
YARD_SPUR = [[RAMP_TRACK_X, 18056.0], [RAMP_TRACK_X, 12700.0]]


def custom_poi_list():
    """(label, mesh path, x, y, yaw, note actor it replaces)."""
    forge = feature("forge_station")["points"][0]
    crater = region("crater")["centre"]
    to_crater = math.degrees(math.atan2(crater[1] - forge[1], crater[0] - forge[0]))
    pod = feature("demeter_pod")["points"][0]
    bunker = LAYOUT["zoo_bunker_at"]
    tower = feature("zoo_tower")["points"][0]
    # the bunker's cable-trench side (local -X) faces the flak tower, its entrance the lanes
    bunker_yaw = math.degrees(math.atan2(bunker[1] - tower[1], bunker[0] - tower[0]))
    return [
        ("POI_ForgeSurveyStation", BERLIN_ENV + "ForgeStation/SM_Berlin_ForgeStation", forge[0], forge[1], to_crater,
         "POI_Note_forge_station"),
        ("POI_DemeterPod", BERLIN_ENV + "DemeterPod/SM_Berlin_DemeterPod", pod[0], pod[1], 215.0, "POI_Note_demeter_pod"),
        ("POI_ZooRadioBunker", BERLIN_ENV + "ZooRadioBunker/SM_Berlin_ZooRadioBunker", bunker[0], bunker[1], bunker_yaw,
         "POI_Note_zoo_bunker"),
        # ramp: local +Y runs along the track toward the buffer stop, here to the west (world -Y)
        ("POI_RailYard_UnloadingRamp", BERLIN_ENV + "RailYardTarget/SM_Berlin_RailRamp", RAMP_AT[0], RAMP_AT[1], 180.0,
         "POI_Note_yard_target"),
        # switch tower: its rodding side (local -X) faces the track
        ("POI_RailYard_SwitchTower", BERLIN_ENV + "RailYardTarget/SM_Berlin_SwitchTower", RAMP_AT[0] - 1500.0,
         RAMP_AT[1] + 1500.0, 180.0, None),
    ]


def step_custom_poi():
    require_map()
    clear_prefix("POI_ForgeSurveyStation")
    clear_prefix("POI_DemeterPod")
    clear_prefix("POI_ZooRadioBunker")
    clear_prefix("POI_RailYard_")
    labels = by_label()
    report = {}
    for label, mesh_path, x, y, yaw, note in custom_poi_list():
        mesh = unreal.load_asset(mesh_path)
        if mesh is None:
            report[label] = "missing " + mesh_path
            continue
        actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, ground_z(x, y)),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_label(label)
        actor.set_folder_path("Berlin/POI")
        origin, extent = actor.get_actor_bounds(False)
        report[label] = [round(origin.x), round(origin.y), round(extent.x), round(extent.y), round(yaw)]
        if note and note in labels:
            actors_sub().destroy_actor(labels[note])
    log("custom poi %s" % report)
    return report


# ---- the destructible ruin kit (B slabs + E heroes and fillers) -----------------------------------------------------
RUIN_KIT = BERLIN_ENV + "RuinKit/"


def step_ruins():
    """One destructible actor per kit piece (BP_BerlinRuin_*: shell holes, health, death). Until a piece has its
    Blueprint the plain mesh stands in, so the layout can be judged early; rerun after the Blueprints exist."""
    require_map()
    with open(DRESS_FILE, "r") as handle:
        ruins = json.load(handle).get("ruins", [])
    removed = clear_prefix("RUIN_")
    classes, meshes, counts = {}, {}, {"blueprint": 0, "mesh": 0, "missing": 0}
    for index, entry in enumerate(ruins):
        piece = entry["piece"]
        if piece not in classes:
            path = RUIN_KIT + "Blueprints/BP_BerlinRuin_" + piece
            classes[piece] = unreal.EditorAssetLibrary.load_blueprint_class(path) \
                if unreal.EditorAssetLibrary.does_asset_exist(path) else None
            meshes[piece] = unreal.load_asset(RUIN_KIT + "Meshes/SM_BerlinRuin_" + piece)
        location = unreal.Vector(entry["x"], entry["y"], entry["z"])
        rotation = unreal.Rotator(0.0, 0.0, entry["yaw"])
        if classes[piece] is not None:
            actor = actors_sub().spawn_actor_from_class(classes[piece], location, rotation)
            comp = actor.get_component_by_class(unreal.StaticMeshComponent)
            counts["blueprint"] += 1
        elif meshes[piece] is not None:
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
            comp = actor.static_mesh_component
            comp.set_static_mesh(meshes[piece])
            counts["mesh"] += 1
        else:
            counts["missing"] += 1
            continue
        if comp is not None:
            # custom primitive data 0-2 tint, 3 soot amount: stored as defaults so they are there at game start
            tint = entry["tint"]
            comp.set_default_custom_primitive_data_vector4(0, unreal.Vector4(tint[0], tint[1], tint[2], entry["soot"]))
        actor.set_actor_label("RUIN_%s_%04d" % (piece, index))
        actor.set_folder_path("Berlin/Ruins/" + entry["kind"])
    log("ruins: removed %d, placed %s" % (removed, counts))
    return counts


# ---- ground decals (district marks, craters under wrecks, manholes) --------------------------------------------------
def step_decals():
    """Plain DecalActors (the crater Blueprints re-pick their material on load, so they are not used)."""
    require_map()
    with open(DRESS_FILE, "r") as handle:
        decals = json.load(handle).get("decals", [])
    removed = clear_prefix("DECAL_")
    materials, missing = {}, set()
    for index, entry in enumerate(decals):
        path = entry["mat"]
        if path not in materials:
            materials[path] = unreal.load_asset(path)
        if materials[path] is None:
            missing.add(path)
            continue
        # a decal projects along its local X: pitch -90 points it at the ground, yaw then spins it in plan
        actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(entry["x"], entry["y"], entry["z"] + 20.0),
                                                    unreal.Rotator(0.0, -90.0, entry["yaw"]))
        comp = actor.decal
        comp.set_decal_material(materials[path])
        half = entry["size"] / 2.0
        comp.set_editor_property("decal_size", unreal.Vector(160.0, half, half))
        comp.set_editor_property("sort_order", 2 if entry["kind"] in ("manhole", "utility") else 0)
        comp.set_fade_screen_size(0.004)
        if "cell" in entry:
            # furniture debris: the decal colour picks one of the 16 variants of the one decal material
            comp.set_editor_property("decal_color", unreal.LinearColor((entry["cell"] + 0.5) / 16.0, 0.0, 0.0, 1.0))
            comp.set_editor_property("sort_order", 3)
        actor.set_actor_label("DECAL_%s_%04d" % (entry["kind"], index))
        actor.set_folder_path("Berlin/Decals/" + entry["kind"])
    log("decals: removed %d, placed %d, missing %s" % (removed, len(decals) - len(missing), sorted(missing)))
    return len(decals)
