"""In-editor build steps for a survival map (run inside the UE editor, e.g. via the UECP `execute_python` action).

Each step is a separate function so one bridge call stays short (long python calls + editor autosave crashed the
editor before). Typical use from execute_python:

    import importlib, sys
    sys.path.insert(0, r"D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools")
    import survival_map_build as smb; importlib.reload(smb)
    smb.load_layout(r"<scratch>/terrain/SM_Desert_01_layout.json")
    smb.step_cleanup_template()      # remove test units / test FOW actors from the ToCopy template
    smb.step_replace_landscape()     # new painted landscape from the generator's raw files
    smb.step_fit_level_actors()      # FOW extent, boundaries, nav bounds, player starts
    smb.save_level()                 # then check the .umap mtime on disk

Requires the open level to be the survival map (checked by `require_map`).
"""
import json
import math

import unreal

LAYOUT = {}
LAYER_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/"
LANDSCAPE_MATERIAL = LAYER_ROOT + "MI_RTS_Landscape_DesertPaintable"
LANDSCAPE_LABEL = "SM_Landscape"
DRESS_FOLDER = "SurvivalDressing"
DRESS_PREFIX = "SMD_"

# Template actors that do not belong in a survival map (pre-placed units and FOW test helpers).
REMOVE_LABEL_PREFIXES = ["Rus_Vehicle_T2", "RTS_BP_EmbeddedT_T26", "TestInFog", "BP_TestFowVisiblity"]


def log(text):
    unreal.log_warning("SURVIVAL_MAP " + str(text))
    print(text)


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_map():
    path = world().get_path_name()
    expected = LAYOUT.get("map", "")
    if not expected or not path.split(".")[0].endswith("/" + expected):
        raise RuntimeError("open level is %s, expected %s" % (path, expected))


def load_layout(path):
    global LAYOUT
    with open(path, "r") as handle:
        LAYOUT = json.load(handle)
    return LAYOUT


def by_label():
    return {actor.get_actor_label(): actor for actor in actors_sub().get_all_level_actors()}


def step_cleanup_template():
    require_map()
    removed = []
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if any(label.startswith(prefix) for prefix in REMOVE_LABEL_PREFIXES):
            removed.append("%s (%s)" % (label, actor.get_class().get_name()))
            actors_sub().destroy_actor(actor)
    log("removed: %s" % removed)
    return removed


def step_replace_landscape():
    require_map()
    lay = LAYOUT["landscape"]
    files = LAYOUT["files"]
    actors = by_label()
    old = [a for a in actors.values() if isinstance(a, unreal.Landscape)]
    body_instance = None
    phys_material = unreal.load_asset("/Game/RTS_Survival/PhysicalMaterials/PM_Sand")
    for landscape in old:
        if body_instance is None:
            # Units only drive on ground that blocks the Landscape trace channel: keep the template's custom profile.
            body_instance = landscape.get_editor_property("body_instance")
            phys_material = landscape.get_editor_property("default_phys_material") or phys_material
    for landscape in old:
        log("destroying old landscape %s" % landscape.get_actor_label())
        actors_sub().destroy_actor(landscape)
    layer_names = list(files["layers"].keys())
    layer_infos = [unreal.load_asset(LAYER_ROOT + name + "_LayerInfo") for name in layer_names]
    missing = [n for n, info in zip(layer_names, layer_infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s" % missing)
    material = unreal.load_asset(LANDSCAPE_MATERIAL)
    corner = lay["corner"]
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        world(), unreal.Vector(corner[0], corner[1], 0.0), unreal.Vector(lay["quad_cm"], lay["quad_cm"], 100.0),
        lay["components"], lay["components"], lay["quads_per_component"], material, files["height"], layer_infos,
        [files["layers"][n] for n in layer_names], LANDSCAPE_LABEL)
    if landscape is None:
        raise RuntimeError("create_painted_landscape failed (see LogRTSShortsLandscape)")
    if body_instance is not None:
        landscape.set_editor_property("body_instance", body_instance)
    landscape.set_editor_property("default_phys_material", phys_material)
    landscape.set_actor_label(LANDSCAPE_LABEL)
    log("landscape created; target layers %s" % [str(n) for n in landscape.get_target_layer_names()])
    return landscape


def step_reimport_heights(height_file):
    """Height-only iteration on the existing landscape (same grid)."""
    require_map()
    landscape = [a for a in actors_sub().get_all_level_actors() if isinstance(a, unreal.Landscape)][0]
    ok = unreal.RTSShortsLandscapeLibrary.import_stage_heights(landscape, height_file)
    log("reimport heights %s" % ok)
    return ok


def step_fit_level_actors(fow_margin=600.0):
    """FOW extent + centre, boundary volumes, nav bounds, player starts."""
    require_map()
    lay = LAYOUT["landscape"]
    half = lay["size_cm"] / 2.0
    centre_x = lay["corner"][0] + half
    centre_y = lay["corner"][1] + half
    actors = by_label()
    fow = actors.get("BP_FogOfWar_Manager")
    if fow is not None:
        fow.set_actor_location(unreal.Vector(centre_x, centre_y, lay["base_z"]), False, False)
        fow.set_editor_property("map_extent", half - fow_margin)
    nav = actors.get("RTS_NavMeshVolume")
    if nav is not None:
        # brush volume: scale a unit brush? keep the brush and scale to the wanted extent
        origin, extent = nav.get_actor_bounds(False)
        scale = nav.get_actor_scale3d()
        z_low, z_high = -1500.0, 4500.0
        want = unreal.Vector(half + 200.0, half + 200.0, (z_high - z_low) / 2.0)
        nav.set_actor_scale3d(unreal.Vector(scale.x * want.x / extent.x, scale.y * want.y / extent.y,
                                            scale.z * want.z / extent.z))
        nav.set_actor_location(unreal.Vector(centre_x, centre_y, (z_high + z_low) / 2.0), False, False)
    walls = {"BoundaryTop": (1, 0), "BoundaryBottom": (-1, 0), "BoundaryRight": (0, 1), "BoundaryLeft": (0, -1)}
    for label, (sx, sy) in walls.items():
        wall = actors.get(label)
        if wall is None:
            continue
        # the template's Left/Right walls are yawed -90: zero it so scale axes match world axes
        wall.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        origin, extent = wall.get_actor_bounds(False)
        scale = wall.get_actor_scale3d()
        thickness = 600.0
        length = half + 1200.0
        if sx != 0:  # wall at +-X, long along Y
            want = unreal.Vector(thickness, length, 5000.0)
            loc = unreal.Vector(centre_x + sx * (half + thickness), centre_y, 2000.0)
        else:
            want = unreal.Vector(length, thickness, 5000.0)
            loc = unreal.Vector(centre_x, centre_y + sy * (half + thickness), 2000.0)
        wall.set_actor_scale3d(unreal.Vector(scale.x * want.x / max(extent.x, 1.0), scale.y * want.y / max(extent.y, 1.0),
                                             scale.z * want.z / max(extent.z, 1.0)))
        wall.set_actor_location(loc, False, False)
    first = LAYOUT["starts"][0]
    for label in ("PlayerStart", "RTS_PLayerStart"):
        actor = actors.get(label)
        if actor is not None:
            z = first["z"] + (100.0 if label == "PlayerStart" else 43.0)
            actor.set_actor_location(unreal.Vector(first["x"], first["y"], z), False, False)
    for label in ("RTS_BP_EnemyController", "BP_MissionManager"):
        actor = actors.get(label)
        if actor is not None:
            actor.set_actor_location(unreal.Vector(centre_x, centre_y - 2000.0 * (label == "BP_MissionManager"),
                                                   lay["base_z"] + 1500.0), False, False)
    report = {}
    for label in ["BP_FogOfWar_Manager", "RTS_NavMeshVolume", "BoundaryTop", "BoundaryBottom", "BoundaryLeft",
                  "BoundaryRight", "PlayerStart", "RTS_PLayerStart"]:
        actor = actors.get(label)
        if actor is not None:
            origin, extent = actor.get_actor_bounds(False)
            report[label] = ((round(origin.x), round(origin.y), round(origin.z)),
                             (round(extent.x), round(extent.y), round(extent.z)))
    log("fitted: %s" % report)
    return report


def save_level():
    require_map()
    pkg = world().get_outermost()
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    log("save_packages %s" % ok)
    return ok


# ---------------------------------------------------------------------------------------------------------------------
# Ground queries and dressing
# ---------------------------------------------------------------------------------------------------------------------
def ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4].z if t[0] else None


def clear_dressing():
    n = 0
    for actor in actors_sub().get_all_level_actors():
        # BP_Scorch_Dest_DeadTree spawns an RTS_BP_NavBlocker_CharOnly companion actor that keeps its own label
        label = actor.get_actor_label()
        if label.startswith(DRESS_PREFIX) or label.startswith("RTS_BP_NavBlocker_CharOnly"):
            actors_sub().destroy_actor(actor)
            n += 1
    log("cleared %d dressing actors" % n)
    return n


def spawn_blueprint(path, label, folder, x, y, yaw=0.0, scale=1.0, sink=0.0):
    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if actor_class is None:
        log("MISSING blueprint " + path)
        return None
    z = ground_z(x, y)
    if z is None:
        return None
    actor = actors_sub().spawn_actor_from_class(actor_class, unreal.Vector(x, y, z - sink),
                                                unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if actor is None:
        return None
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_label(DRESS_PREFIX + label)
    actor.set_folder_path(DRESS_FOLDER + "/" + folder)
    return actor


def spawn_mesh(path, label, folder, x, y, yaw=0.0, scale=1.0, sink=20.0, material=None, pitch=0.0, roll=0.0,
               collision=True):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("MISSING mesh " + path)
        return None
    z = ground_z(x, y)
    if z is None:
        return None
    bounds = mesh.get_bounds()
    bottom = (bounds.origin.z - bounds.box_extent.z) * scale
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z - bottom - sink),
                                                unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    if material is not None:
        mat = unreal.load_asset(material)
        if mat is not None:
            for i in range(comp.get_num_materials()):
                comp.set_material(i, mat)
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_label(DRESS_PREFIX + label)
    actor.set_folder_path(DRESS_FOLDER + "/" + folder)
    return actor


def spawn_decal(material_path, label, folder, x, y, size, yaw=0.0):
    material = unreal.load_asset(material_path)
    if material is None:
        log("MISSING decal " + material_path)
        return None
    z = ground_z(x, y)
    if z is None:
        return None
    actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z),
                                                unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw))
    decal = actor.get_component_by_class(unreal.DecalComponent)
    decal.set_decal_material(material)
    decal.set_editor_property("decal_size", unreal.Vector(400.0, size, size))
    actor.set_actor_label(DRESS_PREFIX + label)
    actor.set_folder_path(DRESS_FOLDER + "/" + folder)
    return actor


def nav_check(points, agents=("RecastNavMesh-Character", "RecastNavMesh-LightTank", "RecastNavMesh-MediumTank",
                               "RecastNavMesh-HeavyTank"), extent=(300.0, 300.0, 600.0)):
    """points: list of (name, x, y, z). Returns {name: {agent: bool}}."""
    actors = by_label()
    out = {}
    for name, x, y, z in points:
        row = {}
        for agent in agents:
            nav = actors.get(agent)
            if nav is None:
                row[agent] = None
                continue
            res = unreal.NavigationSystemV1.project_point_to_navigation(
                world(), unreal.Vector(x, y, z), nav, None, unreal.Vector(*extent))
            loc = res
            ok = loc is not None and not (abs(loc.x) < 1e-3 and abs(loc.y) < 1e-3 and abs(loc.z) < 1e-3)
            row[agent] = bool(ok)
        out[name] = row
    return out
