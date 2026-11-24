"""In-editor build steps for Ger_2ndOreMountains (map creation only: no units, no mission scripting).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
    import ore_map_build as omb; importlib.reload(omb)
    omb.load_layout(); omb.step_duplicate(); omb.step_strip(); omb.step_landscape(); omb.step_fit_level_actors()

Layout and raw files come from ore_terrain_gen.py (Saved/OreMountains/terrain). Generated actors carry the ORE_ label
prefix and sit under the outliner folder OreMountains/, so a step can clear and redo its own part.
"""
import array
import json
import math
import os

import unreal

TEMPLATE_MAP = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap"
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_layout.json"
SHOTS = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/shots/"
LS_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/"
# the map has its own master copy since 2026-11-17 (Docs/Landscape/ls_ore_master_build.py)
LANDSCAPE_PARENT = "/Game/RTS_Survival/Materials/ground/Landscape/OreMountains/MI_RTS_Landscape_OreMountains_Base"
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
LANDSCAPE_LABEL = "Ore_Landscape"
RVT_ROOT = "/Game/RTS_Survival/Meshes/RVTs/"
PREFIX = "ORE_"
FOLDER = "OreMountains/"
Z_LOW, Z_HIGH = -800.0, 11200.0          # landscape runs from about -130 to 9740 cm
NAV_LOW, NAV_HIGH = -600.0, 5200.0       # the play area tops out at the 38 m crest

LAYOUT = {}
HEIGHTS = None


def log(text):
    unreal.log_warning("ORE_MAP " + str(text))
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


# ---- map file ---------------------------------------------------------------------------------------------------
def step_duplicate():
    """Copy the template map to Ger_2ndOreMountains (once) and open it."""
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


# The Desert layer's orange blotches come from its variation-noise mask: thinned out for this map only.
LANDSCAPE_SCALARS = {"Noise_Mask_Power": 0.35, "Texture_Scale_Noise_Near": 8.0, "DarkEarth_Desaturation": 0.3}
# The dark earth layer, warmer and deeper than the master default for this sunlit ground.
LANDSCAPE_VECTORS = {"DarkEarth_Tint": (0.31, 0.26, 0.22)}


def tune_landscape_material(mi):
    """Only LANDSCAPE_SCALARS are overridden; every other scalar keeps following the shared parent instance."""
    current = {str(v.parameter_info.name): round(v.parameter_value, 4) for v in mi.get_editor_property("scalar_parameter_values")}
    vectors = {str(v.parameter_info.name): tuple(round(c, 4) for c in v.parameter_value.to_tuple()[:3])
               for v in mi.get_editor_property("vector_parameter_values")}
    if current == LANDSCAPE_SCALARS and vectors == LANDSCAPE_VECTORS:
        return False
    mi.set_editor_property("scalar_parameter_values", [])
    for name, value in LANDSCAPE_SCALARS.items():
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, name, value)
    for name, value in LANDSCAPE_VECTORS.items():
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log("landscape material scalars reset to %s" % LANDSCAPE_SCALARS)
    return True


def landscape_material():
    """Child instance of the shared DryCastle instance (tank trails on) with the radixite blight switched on.
    The shared master and its three existing instances are left alone."""
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(LANDSCAPE_MATERIAL):
        mi = unreal.load_asset(LANDSCAPE_MATERIAL)
        tune_landscape_material(mi)
        return mi
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        LANDSCAPE_MATERIAL.split("/")[-1], LS_ROOT.rstrip("/"), unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(LANDSCAPE_PARENT))
    try:
        mel.set_material_instance_static_switch_parameter_value(mi, "LS_RadixiteBlight", True)
    except Exception as error:
        log("static switch not set: %s" % error)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log("created %s" % LANDSCAPE_MATERIAL)
    return mi


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
    material = landscape_material()
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
            # the nav data keeps the template's old bounds until it is told the volume moved
            unreal.NavigationSystemV1.get_navigation_system(world()).on_navigation_bounds_updated(actor)
        elif name == "BlockingVolume":
            blockers.append(actor)
        elif name == "RTS_PLayerStart_C":
            actor.set_actor_location(unreal.Vector(start[0], start[1], start_z + 43.0), False, False)
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)     # facing north, over the valley
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
    """The template is lit for the desert at sunset (orange fog, dark). Here: hazy, dusty daylight, the sun from the
    north-north-east so the north-facing escarpment the player looks at is lit."""
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
    """Orthographic top-down capture, north up (east right), written to Saved/OreMountains/shots/<name>.png."""
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


# ---- first dressing pass (ore_dress_gen.py) -------------------------------------------------------------------------
DRESS_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_dress.json"
RESOURCE_ROOT = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
DEAD_TREE_ROOT = "/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/"
DEAD_TREES = ("SM_RTS_DeadTree_1", "SM_RTS_DeadTree_2", "SM_RTS_DeadTree_3", "SM_RTS_DeadTree_04", "SM_RTS_DeadTree_05",
              "SM_RTS_DeadTree_06")


def dress():
    with open(DRESS_FILE, "r") as handle:
        return json.load(handle)


def _instance_actor(label, folder, mesh, transforms, collision=True):
    """One actor holding one hierarchical instanced mesh component."""
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
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    comp.add_instances(transforms, False, True)
    return actor


def step_resources():
    require_map()
    removed = clear_prefix(PREFIX + "Res_")
    classes, placed, missing = {}, 0, set()
    for index, (name, x, y, yaw) in enumerate(dress()["resources"]):
        if name not in classes:
            classes[name] = unreal.EditorAssetLibrary.load_blueprint_class(RESOURCE_ROOT + name)
        if classes[name] is None:
            missing.add(name)
            continue
        actor = actors_sub().spawn_actor_from_class(classes[name], unreal.Vector(x, y, ground_z(x, y)),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label("%sRes_%s_%02d" % (PREFIX, name.split("_")[-1], index))
        actor.set_folder_path(FOLDER + ("Resources/Radixite" if "Radixite" in name else "Resources/Metal"))
        placed += 1
    log("resources: removed %d, placed %d, missing %s" % (removed, placed, sorted(missing)))


KIT_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/foliage/OreDeadForest_pieces.json"
KIT_ROOT = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/"
WILLOW_BP = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/BP_Scorch_Dest_DeadWillow"
WILLOW_MESHES = [DEAD_TREE_ROOT + "DeadWillows/SM_DeadWillow_%d" % i for i in range(1, 6)]


def step_trees():
    """The dead spruce forest (OreDeadForest kit, option B): one crushable BP_OreDeadSpruce_<Piece> actor per standing,
    leaning or surviving tree (AWpoTree: medium and heavy tanks crush it; its construction script turns and scales it),
    windthrown trees as instanced meshes with their collision, a few BP_Scorch_Dest_DeadWillow among the lone trees,
    and the fossil roots at the five gully spots with a black marker tree each. Labels ORE_Tree_*, ORE_Timber_*,
    ORE_Roots_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Trees_") + clear_prefix(PREFIX + "Tree_") + clear_prefix(PREFIX + "Timber_") + clear_prefix(PREFIX + "Roots_")
    with open(KIT_FILE, "r") as handle:
        kit = json.load(handle)
    entries = kit["pieces"].values() if isinstance(kit["pieces"], dict) else kit["pieces"]
    pieces = {p["mesh"].replace("SM_OreDead_", ""): p for p in entries}
    classes, counts, lying = {}, {}, {}
    willow = unreal.EditorAssetLibrary.load_blueprint_class(WILLOW_BP)
    willow_meshes = [unreal.load_asset(m) for m in WILLOW_MESHES]
    for index, (x, y, yaw, scale, kind, variant) in enumerate(dress()["trees"]):
        z = ground_z(x, y)
        if kind == "willow":
            actor = actors_sub().spawn_actor_from_class(willow, unreal.Vector(x, y, z - 12.0), unreal.Rotator(0.0, 0.0, yaw))
            for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                if comp.get_name() == "StaticMesh":
                    comp.set_static_mesh(willow_meshes[variant % len(willow_meshes)])
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        elif pieces[kind].get("blueprint"):
            if kind not in classes:
                classes[kind] = unreal.EditorAssetLibrary.load_blueprint_class(pieces[kind]["blueprint"])
            actor = actors_sub().spawn_actor_from_class(classes[kind], unreal.Vector(x, y, z - 8.0), unreal.Rotator(0.0, 0.0, yaw))
        else:
            lying.setdefault(kind, []).append((x, y, z, yaw))        # windthrown: an instanced mesh, no Blueprint
            counts[kind] = counts.get(kind, 0) + 1
            continue
        actor.set_actor_label("%sTree_%s_%03d" % (PREFIX, kind, index))
        actor.set_folder_path(FOLDER + "Trees/" + kind)
        counts[kind] = counts.get(kind, 0) + 1
    for kind, items in sorted(lying.items()):
        mesh = unreal.load_asset(pieces[kind]["asset"].split(".")[0])
        transforms = [unreal.Transform(unreal.Vector(x, y, z - 6.0), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0)) for x, y, z, yaw in items]
        _instance_actor("%sTimber_%s" % (PREFIX, kind), FOLDER + "Trees/Timber", mesh, transforms, collision=bool(pieces[kind].get("has_collision")))
    # fossil roots: the ribs across the gully cut (X along the gully, pitched to the floor), a root plate on the bank
    # and one blackened marker tree
    ribs = unreal.load_asset(pieces["Roots_R2_Ribs"]["asset"].split(".")[0])
    plate = unreal.load_asset(pieces["Roots_R1_Plate"]["asset"].split(".")[0])
    black = unreal.EditorAssetLibrary.load_blueprint_class(pieces["Black_8m"]["blueprint"])
    for spot in kit["fossil_root_spots"]:
        x, y = spot["at_cm"]
        for label, mesh, px, py, rot, dz in (("Ribs", ribs, x, y, unreal.Rotator(0.0, spot["suggested_pitch_deg"], 0.0), -25.0),
                                             ("Plate", plate, x + 250.0, y + 620.0, unreal.Rotator(0.0, 0.0, 40.0), -5.0)):
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px, py, ground_z(px, py) + dz), rot)
            comp = actor.static_mesh_component
            comp.set_static_mesh(mesh)
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property("can_ever_affect_navigation", False)
            actor.set_actor_label("%sRoots_%s_%s" % (PREFIX, label, spot["id"][-1]))
            actor.set_folder_path(FOLDER + "FossilRoots")
        mx, my = x - 200.0, y - 700.0
        marker = actors_sub().spawn_actor_from_class(black, unreal.Vector(mx, my, ground_z(mx, my) - 8.0), unreal.Rotator(0.0, 0.0, 0.0))
        marker.set_actor_label("%sRoots_Marker_%s" % (PREFIX, spot["id"][-1]))
        marker.set_folder_path(FOLDER + "FossilRoots")
    log("trees: removed %d, placed %s, fossil root spots %d" % (removed, counts, len(kit["fossil_root_spots"])))
    return counts


def step_placeholders():
    """Grey blocks with a label for every landmark that has no model yet, and flat notes for the sites that use
    existing content later. No collision, no nav effect. Labels ORE_PH_* and ORE_Note_*."""
    require_map()
    removed = clear_prefix(PREFIX + "PH_") + clear_prefix(PREFIX + "Note_")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    data = dress()

    def text(label, words, x, y, z, size, colour):
        actor = actors_sub().spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, z), unreal.Rotator(0.0, 90.0, 180.0))
        comp = actor.text_render
        comp.set_text(words)
        comp.set_world_size(size)
        comp.set_text_render_color(colour)
        comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER + "Placeholders")
        actor.set_editor_property("is_editor_only_actor", True)
        return actor

    for pid, label, x, y, sx, sy, sz, yaw in data["placeholders"]:
        z = ground_z(x, y)
        actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z + sz / 2.0), unreal.Rotator(0.0, 0.0, yaw))
        comp = actor.static_mesh_component
        comp.set_static_mesh(cube)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
        actor.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        actor.set_actor_label("%sPH_%s" % (PREFIX, pid))
        actor.set_folder_path(FOLDER + "Placeholders")
        if label:
            text("%sNote_PH_%s" % (PREFIX, pid), label, x - 500.0, y, z + sz + 300.0, 260.0, unreal.Color(255, 60, 30, 255))
    for fid, label, x, y in data["notes"]:
        text("%sNote_%s" % (PREFIX, fid), label, x, y, ground_z(x, y) + 250.0, 380.0, unreal.Color(255, 230, 40, 255))
    log("placeholders: removed %d, placed %d blocks, %d notes" % (removed, len(data["placeholders"]), len(data["notes"])))


# ---- detail pass: roads, checkpoint, rock faces ---------------------------------------------------------------------
PATH_ROOT = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
ROCK_ROOT = "/Game/RTS_Survival/Meshes/Nature/Rocks/RockMoss/SM_RTS_RockMoss_"


def _extended(points, length):
    (x0, y0), (x1, y1) = points[0], points[1]
    (xa, ya), (xb, yb) = points[-2], points[-1]
    d0 = math.hypot(x0 - x1, y0 - y1)
    d1 = math.hypot(xb - xa, yb - ya)
    return [[x0 + (x0 - x1) / d0 * length, y0 + (y0 - y1) / d0 * length]] + [list(p) for p in points] + \
           [[xb + (xb - xa) / d1 * length, yb + (yb - ya) / d1 * length]]


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


def step_roads():
    """The valley road as the old macadam road spline (running out through the camera margin), the ammo and depot
    tracks as two-track dirt paths. Spline tiles have no collision and no nav effect."""
    require_map()
    removed = clear_prefix(PREFIX + "Road_")
    worn = unreal.load_asset(PATH_ROOT + "Road/MI_GroundPath_Road_WarTorn")
    # every third sketch point is enough for the spline (the full 69 points put small kinks into the tiles), and the
    # run-out through the margin follows the heading of the last 40 m, not of the last 8 m
    pts = path("valley_road")["points"]
    thin = [pts[0]] + pts[3:-3:3] + [pts[-1]]
    lead_in = [[2.0 * thin[0][0] - thin[1][0], 2.0 * thin[0][1] - thin[1][1]], thin[0]]
    lead_out = [thin[-1], [2.0 * thin[-1][0] - thin[-2][0], 2.0 * thin[-1][1] - thin[-2][1]]]
    spline_pts = _extended(lead_in, 6000.0)[:1] + thin + _extended(lead_out, 6000.0)[-1:]
    _path_actor(PREFIX + "Road_Valley", "Road/BP_GroundPathSpline_Road", spline_pts, 900.0, worn)
    # the Base B road (same macadam): south-west margin -> across the valley road -> knoll west ramp -> Base B
    side = LAYOUT.get("base_b_road")
    if side:
        _path_actor(PREFIX + "Road_BaseB", "Road/BP_GroundPathSpline_Road", side[:1] + side[1:-1:2] + side[-1:], 900.0, worn)
    _path_actor(PREFIX + "Road_AmmoTrack", "BP_GroundPathSpline_TwoTrack", path("ammo_route")["points"], 420.0)
    _path_actor(PREFIX + "Road_DepotTrack", "BP_GroundPathSpline_TwoTrack", path("depot_track")["points"], 420.0)
    # realism round: the wear network routed by ore_terrain_gen.build_wear
    for k, pts in enumerate(LAYOUT.get("wear_tracks", [])):
        _path_actor("%sRoad_Wear_%02d" % (PREFIX, k), "BP_GroundPathSpline_TwoTrack", pts[::2] + pts[-1:], 380.0)
    return removed


def _spawn_bp(bp_path, label, x, y, yaw, folder, dz=0.0):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(x, y) + dz), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def _spawn_mesh(mesh_path, label, x, y, z, rot, folder, collision=False):
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), rot)
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset(mesh_path))
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def step_checkpoint():
    """Weighbridge checkpoint from existing destructible pieces: concrete fence either side of the 12 m gate across
    the road, barbed wire and hedgehogs carrying the line on, sandbags by the booth, and a raised boom barrier
    (no collision). The booth, the weighbridge deck and the two towers stay placeholders."""
    require_map()
    removed = clear_prefix(PREFIX + "CP_")
    folder = FOLDER + "Checkpoint"
    fence = path("cp_fence")["points"]
    gate = feature("cp_gate")["points"][0]
    y = gate[1]
    lo, hi = min(fence[0][0], fence[1][0]), max(fence[0][0], fence[1][0])
    half_gate = 600.0
    mil = ENV + "DestructableEnvActor/Military/"
    count = 0
    # concrete fence panels (3.4 m, long axis Y -> yaw 90 to run north-south), from the gate posts outwards
    for side, start, end in ((-1, gate[0] - half_gate, lo), (1, gate[0] + half_gate, hi)):
        x = start + side * 170.0
        while (side < 0 and x - 170.0 >= end - 60.0) or (side > 0 and x + 170.0 <= end + 60.0):
            _spawn_bp(mil + "ConcreteWall/BP_Dest_PostApo_ConcreteFence", "%sCP_Fence_%02d" % (PREFIX, count), x, y, 90.0, folder)
            count += 1
            x += side * 340.0
        # barbed wire (5.3 m) then hedgehogs carry the line on past the fence
        for k in range(2):
            _spawn_bp(mil + "Barricades/BP_Military_Dest_BarbedWireLong", "%sCP_Wire_%02d" % (PREFIX, count), x + side * (100.0 + k * 540.0),
                      y + (40.0 if k else -30.0), 90.0 + (6.0 if k else -4.0), folder)
            count += 1
        for k in range(3):
            _spawn_bp(mil + "Barricades/BP_Military_Dest_Hedgehog", "%sCP_Hedgehog_%02d" % (PREFIX, count),
                      x + side * (1250.0 + k * 330.0), y + (-120.0, 90.0, -40.0)[k], 35.0 * k, folder)
            count += 1
    # boom barrier: a post either side of the carriageway, bars raised
    props = "/Game/MilitaryAirport/Meshes/Props/"
    for side in (-1, 1):
        px = gate[0] + side * 520.0
        z = ground_z(px, y - 250.0)
        _spawn_mesh(props + "SM_GateBarrier_1", "%sCP_BarrierPost_%d" % (PREFIX, side), px, y - 250.0, z, unreal.Rotator(0.0, 0.0, 0.0), folder)
        # the pivot of the bar is at one end and it extends along -X: point it across the road, pitched up
        _spawn_mesh(props + "SM_GateBarrier_2_Bar", "%sCP_BarrierBar_%d" % (PREFIX, side), px, y - 250.0, z + 105.0,
                    unreal.Rotator(0.0, -68.0 if side < 0 else 68.0, 0.0 if side > 0 else 180.0), folder)
    sign_x = gate[0] - 640.0
    _spawn_mesh(props + "SM_Sign_6_Stop", PREFIX + "CP_StopSign", sign_x, y + 900.0, ground_z(sign_x, y + 900.0), unreal.Rotator(0.0, 0.0, 90.0), folder)
    booth = feature("cp_booth")["points"][0]
    sand = ENV + "DestructableEnvActor/NaturalCover/SandBagWalls/"
    _spawn_bp(sand + "BP_Cover_RusSandbags_SingleStraight", PREFIX + "CP_Sandbags_0", booth[0] - 350.0, booth[1] + 450.0, 0.0, folder)
    _spawn_bp(sand + "BP_Cover_RusSandbags_Cornered", PREFIX + "CP_Sandbags_1", gate[0] + 900.0, y + 500.0, 180.0, folder)
    log("checkpoint: removed %d, placed %d fence/wire/hedgehog pieces" % (removed, count))


def step_rocks():
    """Rock chunks bedded into the faces (instanced, no collision: the landscape under them does the blocking)."""
    require_map()
    removed = clear_prefix(PREFIX + "Rocks_")
    groups = {}
    for item in dress()["rocks"]:
        groups.setdefault(item[6], []).append(item)
    total = 0
    for variant, items in sorted(groups.items()):
        mesh = unreal.load_asset(ROCK_ROOT + str(variant + 1))
        box = mesh.get_bounding_box()
        cx, cy, cz = (box.min.x + box.max.x) / 2.0, (box.min.y + box.max.y) / 2.0, (box.min.z + box.max.z) / 2.0
        height = box.max.z - box.min.z
        transforms = []
        for x, y, yaw, pitch, roll, scale, _ in items:
            # the pivots of these meshes lie metres off their centres: put the bounding-box centre on the spot
            rad = math.radians(yaw)
            ox = (cx * math.cos(rad) - cy * math.sin(rad)) * scale
            oy = (cx * math.sin(rad) + cy * math.cos(rad)) * scale
            z = ground_z(x, y) - cz * scale - 0.3 * height * scale      # bedded in: a third of the chunk is underground
            transforms.append(unreal.Transform(unreal.Vector(x - ox, y - oy, z), unreal.Rotator(roll, pitch, yaw),
                                               unreal.Vector(scale, scale, scale)))
        _instance_actor("%sRocks_Face_%d" % (PREFIX, variant), FOLDER + "Rocks", mesh, transforms, collision=False)
        total += len(items)
    log("rocks: removed %d, placed %d" % (removed, total))


# ---- detail pass 2: props and instanced ground detail (ore_detail_gen.py) --------------------------------------------
DETAIL_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_detail.json"
# placeholders that step_props replaces with real content
REPLACED_PLACEHOLDERS = ("sp_tent", "smoulder_1", "smoulder_2", "wismut_sheds")


def step_props():
    """Blueprint props (destructible yard buildings, fence, stacks, wrecks, cars, poles, logs: they keep their own
    collision and nav effect), plain meshes and effects (no collision), and instanced ground detail (no collision,
    no nav effect). Labels ORE_Prop_* and ORE_Scatter_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Prop_") + clear_prefix(PREFIX + "Scatter_")
    with open(DETAIL_FILE, "r") as handle:
        data = json.load(handle)
    classes, missing, placed = {}, set(), 0
    for kind, asset, label, x, y, yaw, scale, dz in data["props"]:
        folder = FOLDER + "Props/" + label.split("_")[0]
        z = ground_z(x, y) + dz
        if kind == "bp":
            if asset not in classes:
                classes[asset] = unreal.EditorAssetLibrary.load_blueprint_class(asset) \
                    if unreal.EditorAssetLibrary.does_asset_exist(asset) else None
            if classes[asset] is None:
                missing.add(asset.split("/")[-1])
                continue
            actor = actors_sub().spawn_actor_from_class(classes[asset], unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        elif kind == "mesh":
            loaded = unreal.load_asset(asset)
            if loaded is None:
                missing.add(asset.split("/")[-1])
                continue
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
            comp = actor.static_mesh_component
            comp.set_static_mesh(loaded)
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property("can_ever_affect_navigation", False)
        else:
            system = unreal.load_asset(asset)
            if system is None:
                missing.add(asset.split("/")[-1])
                continue
            actor = actors_sub().spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
            actor.niagara_component.set_asset(system)
        if scale != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_actor_label(PREFIX + "Prop_" + label)
        actor.set_folder_path(folder)
        placed += 1
    total = 0
    for name, group in sorted(data["scatter"].items()):
        if not group["items"]:
            continue
        loaded = unreal.load_asset(group["mesh"])
        if loaded is None:
            missing.add(group["mesh"].split("/")[-1])
            continue
        box = loaded.get_bounding_box()
        transforms = [unreal.Transform(unreal.Vector(x, y, ground_z(x, y) - (box.min.z + group.get("sink", 0.08) * (box.max.z - box.min.z)) * s),
                                       unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(s, s, s)) for x, y, yaw, s in group["items"]]
        _instance_actor(PREFIX + "Scatter_" + name, FOLDER + "GroundDetail", loaded, transforms, collision=False)
        total += len(transforms)
    for pid in REPLACED_PLACEHOLDERS:
        clear_prefix(PREFIX + "PH_" + pid)
        clear_prefix(PREFIX + "Note_PH_" + pid)
    log("props: removed %d, placed %d props and %d instances, missing %s" % (removed, placed, total, sorted(missing)))


# ---- cliff kit (OreCliffKit: C on the escarpment and knoll, E on the crest face) --------------------------------------
CLIFF_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_cliffs.json"


def step_cliffs():
    """The modelled cliff kit over the landscape faces, from the kit's own layout file (ock_place.py in the Blender
    project). Instanced, no collision and no nav effect: the landscape under it does the blocking. Labels ORE_Cliff_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Cliff_")
    with open(CLIFF_FILE, "r") as handle:
        data = json.load(handle)
    cleared = spur_cleared_cliffs()
    groups = {}
    for index, item in enumerate(data["instances"]):
        if index in cleared:
            continue                                    # under the viaduct spur's landing
        groups.setdefault((item["face"], item["piece"]), []).append(item)
    total, missing = 0, set()
    for (face, piece), items in sorted(groups.items()):
        mesh = unreal.load_asset(data["mesh_path"].replace("<piece>", piece))
        if mesh is None:
            missing.add(piece)
            continue
        transforms = [unreal.Transform(unreal.Vector(i["x"], i["y"], i["z"]), unreal.Rotator(i["roll"], i["pitch"], i["yaw"]),
                                       unreal.Vector(i["scale"], i["scale"], i["scale"])) for i in items]
        actor = _instance_actor("%sCliff_%s_%s" % (PREFIX, face, piece), FOLDER + "Cliffs/" + face, mesh, transforms, collision=False)
        comp = actor.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)      # also stops per-triangle traces
        comp.set_editor_property("can_ever_affect_navigation", False)
        total += len(items)
    log("cliffs: removed %d actors, placed %d instances in %d components, missing %s" % (removed, total, len(groups), sorted(missing)))
    return total


# ---- the broken A4 viaduct (concept A1, CustomMapItems/Environment/A4Viaduct) -------------------------------------------
VIADUCT_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/viaduct/A4Viaduct_pieces.json"


def step_viaduct():
    """Four standing pieces (their UCX hulls block units and shots and cut the navmesh), the two fallen spans and the
    rubble fans (no collision, no nav effect: vehicles drive over them on the landscape). Removes the four viaduct
    placeholders. Labels ORE_Viaduct_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Viaduct_")
    with open(VIADUCT_FILE, "r") as handle:
        data = json.load(handle)
    folder = FOLDER + "Viaduct"

    def place(label, asset, loc, yaw, blocks):
        mesh = unreal.load_asset(asset)
        if mesh is None:
            raise RuntimeError("missing " + asset)
        actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(loc[0], loc[1], loc[2]), unreal.Rotator(0.0, 0.0, yaw))
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        comp.set_mobility(unreal.ComponentMobility.STATIC)
        if not blocks:
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property("can_ever_affect_navigation", False)
        actor.set_actor_label(PREFIX + "Viaduct_" + label)
        actor.set_folder_path(folder)
        return actor

    count = 0
    for piece in data["pieces"]:
        if "world_cm" not in piece:
            continue                                    # the reusable fans are placed from fan_placements
        place(piece["asset"].split("SM_A4Viaduct_")[-1], piece["asset"], piece["world_cm"], piece["yaw_deg"], bool(piece.get("collision")))
        count += 1
    for index, fan in enumerate(data.get("fan_placements", [])):
        place("Fan_%02d" % index, fan["asset"], fan["world_cm"], fan["yaw_deg"], False)
        count += 1
    for pid in ("viaduct_south", "viaduct_south_pier", "viaduct_north_pier", "viaduct_north"):
        clear_prefix(PREFIX + "PH_" + pid)
        clear_prefix(PREFIX + "Note_PH_" + pid)
    log("viaduct: removed %d, placed %d pieces" % (removed, count))
    return count


# ---- ambient sand and dust effects (ore_fx_gen.py) ------------------------------------------------------------------
FX_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_fx.json"


def apply_fx_preset(comp, preset):
    """User-parameter overrides of a Niagara component. Set in the editor they are stored on the placed component
    (instance parameter overrides), so the pack's system assets stay as they are."""
    for key, value in preset.get("floats", {}).items():
        comp.set_niagara_variable_float(key, float(value))
    for key, value in preset.get("colors", {}).items():
        comp.set_niagara_variable_linear_color(key, unreal.LinearColor(*value))
    for key, value in preset.get("vectors", {}).items():
        comp.set_niagara_variable_vec3(key, unreal.Vector(*value))


def step_fx():
    """Tuned sand and dust effects: sand streaks on open ground, dust off the rock lips, dust clouds, viaduct dust.
    Labels ORE_Fx_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Fx_")
    with open(FX_FILE, "r") as handle:
        data = json.load(handle)
    systems, placed = {}, {}
    for name, label, x, y, z_mode, z, yaw in data["items"]:
        preset = data["presets"][name]
        if preset["system"] not in systems:
            systems[preset["system"]] = unreal.load_asset(preset["system"])
        system = systems[preset["system"]]
        if system is None:
            raise RuntimeError("missing " + preset["system"])
        height = z if z_mode == "abs" else ground_z(x, y) + z
        actor = actors_sub().spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, height), unreal.Rotator(0.0, 0.0, yaw))
        scale = preset.get("scale", 1.0)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        comp = actor.niagara_component
        comp.set_asset(system)
        apply_fx_preset(comp, preset)
        actor.set_actor_label("%sFx_%s_%s" % (PREFIX, name, label))
        actor.set_folder_path(FOLDER + "Fx/" + name)
        placed[name] = placed.get(name, 0) + 1
    log("fx: removed %d, placed %s" % (removed, placed))
    return placed


# ---- ground decals ---------------------------------------------------------------------------------------------------
# Judged in game on valley sand, pale bench and dark earth (Saved/OreMountains/critic/decals/sheet.jpg). Left out:
# MI_Decal_Dirt_TracksSandy_* (brick red here, like the dirt crater BPs), the Zone MI_Decals_* (paint markings), embers.
DECAL_SCORCH = "/Game/Crater_Decals/Decals/Scorched/"
DECAL_DIRT = "/Game/RTS_Survival/Materials/Decals/DirtTracks/"
DECAL_IMPACT = "/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/"
DECALS = {
    "scorch": [DECAL_SCORCH + "MI_Decal_Scorch_01", DECAL_SCORCH + "MI_Decal_Scorch_02", DECAL_SCORCH + "MI_Decal_Scorch_03"],
    "soot": [DECAL_SCORCH + "MI_Decal_Scorch_04", DECAL_SCORCH + "MI_Decal_Scorch_03"],
    "crater": [DECAL_IMPACT + "MI_BombImpactDecal", DECAL_IMPACT + "MI_LandscapeImpactDecal",
               DECAL_SCORCH + "MI_Decal_Crater_Scorched_Dirt_01"],
    "debris": [DECAL_SCORCH + "MI_Decal_Crater_Scorched_Scatter_01", DECAL_SCORCH + "MI_Decal_Scorched_Dirt_Debris_01",
               "/Game/Crater_Decals/Decals/Dirt/MI_Decal_Dirt_Scatter_01"],
    "erosion": [DECAL_DIRT + "MI_Decal_Erosion_Channel_01", DECAL_DIRT + "MI_Decal_Erosion_Channel_03"],
}


def decal_plan():
    """[(kind, variant, x, y, half size cm, yaw, sort order)]: scorch, a crater where the ground could not be sunk and
    thrown debris at every shell crater; soot under the wrecks, smoulder spots and the fuel store; an erosion channel
    where each rill starts and half-way down it."""
    import math
    import random
    rng = random.Random(1947)
    plan = []
    for index, (x, y, radius, sunk) in enumerate(LAYOUT.get("craters", [])):
        plan.append(("scorch", index, x, y, radius * (1.35 if sunk else 1.6), rng.uniform(0.0, 360.0), 1))
        if not sunk:
            plan.append(("crater", index, x, y, radius * 1.05, rng.uniform(0.0, 360.0), 2))
        for _ in range(1 if sunk else 2):
            angle, reach = rng.uniform(0.0, 2.0 * math.pi), radius * rng.uniform(1.2, 1.9)
            plan.append(("debris", rng.randrange(3), x + reach * math.cos(angle), y + reach * math.sin(angle),
                         radius * rng.uniform(0.7, 1.0), rng.uniform(0.0, 360.0), 3))
    with open(DETAIL_FILE, "r") as handle:
        props = json.load(handle)["props"]
    for prop in props:
        label, x, y = prop[2], prop[3], prop[4]
        if label.startswith("Wreck_"):
            plan.append(("soot", 1, x, y, 520.0, rng.uniform(0.0, 360.0), 1))
            plan.append(("debris", rng.randrange(2), x + rng.uniform(-250.0, 250.0), y + rng.uniform(-250.0, 250.0), 380.0,
                         rng.uniform(0.0, 360.0), 2))
        elif label.startswith("Smoulder_") or label == "Yard_Fuel":
            plan.append(("soot", 0, x, y, rng.uniform(260.0, 360.0), rng.uniform(0.0, 360.0), 1))
    for index, line in enumerate(LAYOUT.get("tributaries", [])):
        for at in (1, len(line) // 2):
            if at + 1 >= len(line):
                continue
            (x0, y0), (x1, y1) = line[at], line[at + 1]
            yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
            plan.append(("erosion", index + at, x0, y0, rng.uniform(300.0, 400.0), yaw, 1))
    return plan


def step_decals():
    """Ground decals from decal_plan(). Labels ORE_Decal_*. Shared decal instances are used as they are."""
    require_map()
    removed = clear_prefix(PREFIX + "Decal_")
    materials = {kind: [unreal.load_asset(path) for path in paths] for kind, paths in DECALS.items()}
    missing = [paths[i] for kind, paths in DECALS.items() for i, m in enumerate(materials[kind]) if m is None]
    if missing:
        raise RuntimeError("missing decal materials %s" % missing)
    placed = {}
    for index, (kind, variant, x, y, half, yaw, order) in enumerate(decal_plan()):
        options = materials[kind]
        actor = actors_sub().spawn_actor_from_class(
            unreal.DecalActor, unreal.Vector(x, y, ground_z(x, y) + 30.0), unreal.Rotator(0.0, -90.0, yaw))
        actor.decal.set_decal_material(options[variant % len(options)])
        actor.decal.set_editor_property("decal_size", unreal.Vector(max(220.0, 0.6 * half), half, half))
        actor.decal.set_editor_property("sort_order", order)
        actor.set_actor_label("%sDecal_%s_%03d" % (PREFIX, kind, index))
        actor.set_folder_path(FOLDER + "Decals/" + kind)
        placed[kind] = placed.get(kind, 0) + 1
    log("decals: removed %d, placed %s" % (removed, placed))
    return placed


# ---- road junction ---------------------------------------------------------------------------------------------------
JUNCTION_BP = PATH_ROOT + "Road/Junction/BP_GroundPath_RoadJunction"


def step_junction():
    """The junction overlay where the Base B road crosses the valley road (asset and placement rules:
    Source/Docs/Landscape/path_road_junction_build.py). Pivot, yaw and height come from the placed splines.
    Label ORE_Junction_BaseB. Run after step_roads()."""
    require_map()
    removed = clear_prefix(PREFIX + "Junction_")
    actors = by_label()
    main, side = actors.get(PREFIX + "Road_Valley"), actors.get(PREFIX + "Road_BaseB")
    if main is None or side is None:
        raise RuntimeError("both road splines must exist before the junction")
    world_space = unreal.SplineCoordinateSpace.WORLD
    main_spline = main.get_component_by_class(unreal.SplineComponent)
    side_spline = side.get_component_by_class(unreal.SplineComponent)
    best = None
    distance = 0.0
    length = side_spline.get_spline_length()
    while distance < length:
        on_side = side_spline.get_location_at_distance_along_spline(distance, world_space)
        on_main = main_spline.find_location_closest_to_world_location(on_side, world_space)
        gap = math.hypot(on_side.x - on_main.x, on_side.y - on_main.y)
        if best is None or gap < best[0]:
            best = (gap, on_main, on_side, distance)
        distance += 25.0
    gap, pivot, on_side, at = best
    main_dir = main_spline.find_tangent_closest_to_world_location(pivot, world_space)
    side_dir = side_spline.get_tangent_at_distance_along_spline(at, world_space)
    main_yaw = math.degrees(math.atan2(main_dir.y, main_dir.x))
    side_yaw = math.degrees(math.atan2(side_dir.y, side_dir.x))
    skew = ((side_yaw - main_yaw) % 180.0) - 90.0
    tiles = main.get_components_by_class(unreal.SplineMeshComponent)
    near = min(tiles, key=lambda c: (unreal.SystemLibrary.get_component_bounds(c)[0] - pivot).length())
    tile_origin, tile_extent, _ = unreal.SystemLibrary.get_component_bounds(near)
    ground = ground_z(pivot.x, pivot.y)
    z = max(tile_origin.z, ground + 20.0) + 6.0
    cls = unreal.EditorAssetLibrary.load_blueprint_class(JUNCTION_BP)
    actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(pivot.x, pivot.y, z), unreal.Rotator(0.0, 0.0, main_yaw))
    actor.set_actor_label(PREFIX + "Junction_BaseB")
    actor.set_folder_path(FOLDER + "Roads")
    actor.get_component_by_class(unreal.StaticMeshComponent).set_default_custom_primitive_data_float(0, skew)
    info = {"pivot": [round(pivot.x), round(pivot.y), round(z)], "gap_cm": round(gap, 1), "main_yaw": round(main_yaw, 1),
            "side_yaw": round(side_yaw, 1), "skew": round(skew, 1), "ground": round(ground),
            "tile_z": round(tile_origin.z), "tile_half_height": round(tile_extent.z)}
    log("junction: removed %d, placed %s" % (removed, info))
    return info


# ---- the intact viaduct spur at Base B (concept C Kurve, CustomMapItems/Environment/A4Viaduct/Model/Spur) -------------------
SPUR_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/viaduct/A4ViaductSpur_pieces.json"
# rim pieces the spur file marks "shift or remove": removed too, their bounds reach onto the road
SPUR_ALSO_CLEARED = (664, 701)


def spur_data():
    if not os.path.exists(SPUR_FILE):
        return None
    with open(SPUR_FILE, "r") as handle:
        return json.load(handle)


def spur_cleared_cliffs():
    """Indices into Ore_cliffs.json instances that stand on the spur's landing."""
    data = spur_data()
    if data is None:
        return set()
    listed = {item["index"] for item in data["remove_cliff_kit_instances"]["remove"] if item.get("action") == "remove"}
    return listed | set(SPUR_ALSO_CLEARED)


def _inside(poly, x, y):
    hit = False
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) / (y1 - y0) * (x1 - x0):
            hit = not hit
    return hit


def step_spur():
    """The intact viaduct stretch on the Base B knoll: one standing mesh whose UCX hulls carry the drivable deck.
    Clears what would stand in or poke through it: trees inside its footprint, ground scatter on the approach slab,
    the stand-in face rocks the file lists. Run after step_cliffs() (which leaves the landing's rim pieces out),
    step_trees() and step_props(). Label ORE_Spur_Kurve."""
    require_map()
    data = spur_data()
    if data is None:
        raise RuntimeError("missing " + SPUR_FILE)
    removed = clear_prefix(PREFIX + "Spur_")
    piece = data["pieces"][0]
    mesh = unreal.load_asset(piece["asset"])
    if mesh is None:
        raise RuntimeError("missing " + piece["asset"])
    loc = piece["world_cm"]
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(loc[0], loc[1], loc[2]),
                                                unreal.Rotator(0.0, 0.0, piece["yaw_deg"]))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.set_actor_label(PREFIX + "Spur_Kurve")
    actor.set_folder_path(FOLDER + "Viaduct")
    foot = data["footprint_polygon_world_cm"]
    slab = data["approach_slab_polygon_world_cm"]
    rocks = [item["world_cm"] for item in data["dress_rocks"]["move_or_delete"]]
    trees = scatter = props = 0

    def near_slab(x, y, margin):
        if _inside(slab, x, y):
            return True
        for (x0, y0), (x1, y1) in zip(slab, slab[1:] + slab[:1]):
            t = max(0.0, min(1.0, ((x - x0) * (x1 - x0) + (y - y0) * (y1 - y0)) / ((x1 - x0) ** 2 + (y1 - y0) ** 2)))
            if math.hypot(x - x0 - t * (x1 - x0), y - y0 - t * (y1 - y0)) < margin:
                return True
        return False

    for other in all_actors():
        label = other.get_actor_label()
        if label.startswith(PREFIX + "Prop_"):
            # Base B's own props (a sandbag arc stood on the slab and shut medium and heavy tanks out of the deck)
            at = other.get_actor_location()
            if near_slab(at.x, at.y, 600.0):
                log("spur: removed %s from the landing" % label)
                actors_sub().destroy_actor(other)
                props += 1
            continue
        if label.startswith(PREFIX + "Tree_"):
            at = other.get_actor_location()
            if _inside(foot, at.x, at.y):
                actors_sub().destroy_actor(other)
                trees += 1
            continue
        if not (label.startswith(PREFIX + "Scatter_") or label.startswith(PREFIX + "Rocks_") or label.startswith(PREFIX + "Timber_")):
            continue
        comp = other.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        if comp is None:
            continue
        for index in range(comp.get_instance_count() - 1, -1, -1):
            at = comp.get_instance_transform(index, True).translation
            on_slab = _inside(slab, at.x, at.y)
            under = _inside(foot, at.x, at.y) and label.startswith(PREFIX + "Timber_")
            listed = any(math.hypot(at.x - r[0], at.y - r[1]) < 150.0 for r in rocks)
            if on_slab or under or listed:
                comp.remove_instance(index)
                scatter += 1
    info = {"removed_old": removed, "trees_removed": trees, "instances_removed": scatter, "props_removed": props,
            "cliff_instances_left_out": sorted(spur_cleared_cliffs())}
    log("spur: %s" % info)
    return info


# ---- crushable rocks (CrushableRocks kit; plan from ore_crush_rocks_gen.py) -------------------------------------------
CRUSH_ROCK_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_crush_rocks.json"


def step_crush_rocks():
    """Crushable rocks as single Blueprint actors (an instanced copy would have no crush, health or nav area): small
    ones any vehicle crushes, medium ones medium and heavy tanks crush. Flat bases, so each is sunk by the plan's
    amount for the slope it lies on. Rebuild navigation afterwards. Labels ORE_CrushRock_*."""
    require_map()
    removed = clear_prefix(PREFIX + "CrushRock_")
    with open(CRUSH_ROCK_FILE, "r") as handle:
        data = json.load(handle)
    classes, placed = {}, {}
    for index, (name, x, y, yaw, scale, sink, zone) in enumerate(data["rocks"]):
        if name not in classes:
            classes[name] = unreal.EditorAssetLibrary.load_blueprint_class(data["blueprint"].replace("<name>", name))
            if classes[name] is None:
                raise RuntimeError("missing crushable rock Blueprint " + name)
        actor = actors_sub().spawn_actor_from_class(classes[name], unreal.Vector(x, y, ground_z(x, y) - sink),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_actor_label("%sCrushRock_%s_%03d" % (PREFIX, name, index))
        actor.set_folder_path(FOLDER + "CrushRocks/" + zone)
        placed[zone] = placed.get(zone, 0) + 1
    log("crushable rocks: removed %d, placed %d %s" % (removed, len(data["rocks"]), placed))
    return placed


# ---- large rockworks (Rockworks kit; plan from ore_rockworks_gen.py) ------------------------------------------------------
ROCKWORK_FILE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_rockworks.json"
ROCKWORK_CLEAR_OF_OTHERS = 1200.0


def step_rockworks():
    """Large rockworks: not crushable, block every unit, take shell holes. Single Blueprint actors at scale 1 (a scaled
    rock stretches its holes). A spot is skipped when an actor this tool did not place (mission scripting, clue spots)
    stands within 12 m: the plan does not know those. Rebuild navigation afterwards. Labels ORE_Rockwork_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Rockwork_")
    with open(ROCKWORK_FILE, "r") as handle:
        data = json.load(handle)
    others = []
    for actor in all_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX) or label.startswith("Ore_") or actor.root_component is None:
            continue
        if isinstance(actor, (unreal.Landscape, unreal.LandscapeStreamingProxy, unreal.Volume, unreal.Light, unreal.SkyLight,
                              unreal.SkyAtmosphere, unreal.ExponentialHeightFog, unreal.RecastNavMesh, unreal.Brush)):
            continue
        loc = actor.get_actor_location()
        others.append((loc.x, loc.y, label))
    classes, placed, skipped = {}, [], []
    for index, (name, x, y, yaw, sink, zone) in enumerate(data["rocks"]):
        near = [label for ox, oy, label in others if math.hypot(ox - x, oy - y) < ROCKWORK_CLEAR_OF_OTHERS]
        if near:
            skipped.append((name, x, y, near[:3]))
            continue
        if name not in classes:
            classes[name] = unreal.EditorAssetLibrary.load_blueprint_class(data["blueprint"].replace("<name>", name))
            if classes[name] is None:
                raise RuntimeError("missing rockwork Blueprint " + name)
        actor = actors_sub().spawn_actor_from_class(classes[name], unreal.Vector(x, y, ground_z(x, y) - sink),
                                                    unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label("%sRockwork_%s_%02d" % (PREFIX, name, index))
        actor.set_folder_path(FOLDER + "Rockworks")
        placed.append(name)
    log("rockworks: removed %d, placed %d, skipped %s" % (removed, len(placed), skipped))
    return {"placed": len(placed), "skipped": skipped}


# ---- cliff decals (OreCliffKit/Decals/v02, built by a subagent; spec decals.json) ----------------------------------------
CLIFF_DECAL_SPEC = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCliffKit/Decals/v02/decals.json"
CLIFF_DECAL_MI = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/MI_OreCliffDecal_"
# decals per 20 m of face; the adit dressing and the tops and fans follow the rules in cliff_decal_plan()
# (halved after the first full placement: 486 decals striped every block)
CLIFF_DECAL_RATE = {"SeepCurtain_A": 0.45, "SeepCurtain_B": 0.4, "WeatherBand": 0.3, "LeachBand": 0.2, "CalciteCurtain": 0.14}
# image orientation on a placed decal, found on the first placement: roll for wall decals, yaw offset for down decals
# whose image top must point at the face
# A decal maps image U along its local Z and V along its local Y (seen on the first placement: text came out turned
# 90 degrees), so every decal is turned a quarter and its width and height swap places in DecalSize.
CLIFF_DECAL_WALL_ROLL = 90.0
CLIFF_DECAL_DOWN_YAW = 90.0


def cliff_decal_plan(only_adit=None):
    """[(name, location, (roll, pitch, yaw), half size (depth, width, height) cm, sort)] for the wall stains and bands
    on the face pieces, the patches on the lip tops, the fans at the foot and the adit dressing."""
    import random
    rng = random.Random(4711)
    with open(CLIFF_FILE, "r") as handle:
        cliffs = json.load(handle)
    with open(CLIFF_DECAL_SPEC, "r") as handle:
        spec = {d["name"]: d for d in json.load(handle)["decals"]}
    cleared = spur_cleared_cliffs()
    boxes = {}

    def box(piece):
        if piece not in boxes:
            mesh = unreal.load_asset(cliffs["mesh_path"].replace("<piece>", piece))
            bounds = mesh.get_bounding_box()
            boxes[piece] = (bounds.min, bounds.max)
        return boxes[piece]

    def world_point(item, lx, ly, lz):
        yaw, s = math.radians(item["yaw"]), item["scale"]
        return (item["x"] + s * (lx * math.cos(yaw) - ly * math.sin(yaw)),
                item["y"] + s * (lx * math.sin(yaw) + ly * math.cos(yaw)), item["z"] + s * lz)

    plan = []

    def wall(name, item, along=0.0, top=None, roll=0.0, sort=0):
        lo, hi = box(item["piece"])
        d = spec[name]
        width, height = d["size_m"][0] * 100.0, d["size_m"][1] * 100.0
        face_h = (hi.z - lo.z) * item["scale"]
        k = min(1.0, (face_h + 150.0) / height)
        width, height = width * max(k, 0.6), height * k
        top_z = (hi.z if top is None else top) * item["scale"]
        ly = along * max(0.0, (hi.y - lo.y) * 0.5 - width * 0.4 / item["scale"])
        x, y, _ = world_point(item, (lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5 + ly, 0.0)
        centre = (x, y, item["z"] + top_z + 40.0 - height * 0.5)
        depth = max(d["depth_m"] * 50.0, (hi.x - lo.x) * 0.5 * item["scale"] + 120.0)
        plan.append((name, centre, (CLIFF_DECAL_WALL_ROLL + roll, 0.0, item["yaw"] + 180.0), (depth, width * 0.5, height * 0.5), sort))

    def down(name, x, y, z, yaw, scale=1.0, sort=0):
        d = spec[name]
        plan.append((name, (x, y, z + 60.0), (0.0, -90.0, yaw + CLIFF_DECAL_DOWN_YAW),
                     (max(200.0, d["depth_m"] * 50.0), d["size_m"][0] * 50.0 * scale, d["size_m"][1] * 50.0 * scale), sort))

    adits = cliffs["adit_openings"]

    def face_offset(adit, out):
        """How far the real rock face stands from the adit point along the outward normal (mean over the face pieces
        within 16 m), so the adit's wall decals sit on the rock and not in front of it."""
        hits = []
        for item in cliffs["instances"]:
            if item["role"] == "face" and math.hypot(item["x"] - adit["x"], item["y"] - adit["y"]) < 1600.0:
                lo, hi = box(item["piece"])
                x, y, _ = world_point(item, (lo.x + hi.x) * 0.5, 0.0, 0.0)
                hits.append((x - adit["x"]) * out[0] + (y - adit["y"]) * out[1])
        return sum(hits) / len(hits) if hits else 0.0

    if only_adit is None:
        for index, item in enumerate(cliffs["instances"]):
            if index in cleared:
                continue
            lo, hi = box(item["piece"])
            width_m = (hi.y - lo.y) * item["scale"] / 100.0
            near_adit = any(math.hypot(item["x"] - a["x"], item["y"] - a["y"]) < 1400.0 for a in adits)
            if item["role"] == "face" and not near_adit:
                for name, rate in CLIFF_DECAL_RATE.items():
                    if rng.random() < rate * width_m / 20.0:
                        roll = rng.uniform(-8.0, 8.0) if name.endswith("Band") else 0.0
                        top = None
                        if name == "WeatherBand":
                            top = lo.z + (hi.z - lo.z) * rng.uniform(0.45, 0.7)
                        elif name == "CalciteCurtain":
                            top = lo.z + (hi.z - lo.z) * rng.uniform(0.6, 0.85)
                        wall(name, item, along=rng.uniform(-1.0, 1.0), top=top, roll=roll)
                if item["piece"] in ("Bench12", "Panel4") and rng.random() < 0.08:
                    wall("HalfCasts", item, along=rng.uniform(-1.0, 1.0))
            elif item["role"] == "lip" and rng.random() < 0.25:
                x, y, z = world_point(item, (lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, hi.z)
                down("DampPatch" if rng.random() < 0.55 else "LichenCrust", x, y, z, rng.uniform(0.0, 360.0), rng.uniform(0.8, 1.15))
            elif item["role"] == "talus" and rng.random() < 0.1 and not near_adit:
                x, y, _ = world_point(item, hi.x + 250.0, (lo.y + hi.y) * 0.5, 0.0)
                down("RockFlourFan", x, y, ground_z(x, y), item["yaw"], rng.uniform(0.75, 1.1))
    for number, adit in enumerate(adits):
        if only_adit is not None and number != only_adit:
            continue
        yaw = math.radians(adit["yaw"])
        out, side = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
        top = adit["z"] + adit["height_cm"]
        facing = (CLIFF_DECAL_WALL_ROLL, 0.0, adit["yaw"] + 180.0)
        back = face_offset(adit, out)
        adit = dict(adit, x=adit["x"] + out[0] * back, y=adit["y"] + out[1] * back)
        d = spec["BlastFan"]
        plan.append(("BlastFan", (adit["x"], adit["y"], top + d["size_m"][1] * 50.0 - 60.0), facing,
                     (650.0, d["size_m"][0] * 50.0, d["size_m"][1] * 50.0), 0))
        mark = ("Mark_Strosse", "Mark_Warning_B", "Mark_Warning_A")[number % 3]
        d = spec[mark]
        plan.append((mark, (adit["x"] + side[0] * 150.0, adit["y"] + side[1] * 150.0, top + 170.0), facing,
                     (350.0, d["size_m"][0] * 30.0, d["size_m"][1] * 30.0), 2))
        d = spec["SootCurtain"]
        plan.append(("SootCurtain", (adit["x"] + side[0] * 1150.0, adit["y"] + side[1] * 1150.0, top + 250.0), facing,
                     (450.0, d["size_m"][0] * 50.0, d["size_m"][1] * 50.0), 0))
        d = spec["RustField"]
        rx, ry = adit["x"] - side[0] * 1500.0, adit["y"] - side[1] * 1500.0
        plan.append(("RustField", (rx, ry, top + 150.0), facing, (450.0, d["size_m"][0] * 50.0, d["size_m"][1] * 50.0), 0))
        gx, gy = adit["x"] + out[0] * 700.0, adit["y"] + out[1] * 700.0
        down("SootFallout", gx, gy, ground_z(gx, gy), adit["yaw"], 1.0)
        gx, gy = adit["x"] + out[0] * 900.0 + side[0] * 500.0, adit["y"] + out[1] * 900.0 + side[1] * 500.0
        down("OreFines", gx, gy, ground_z(gx, gy), adit["yaw"], 1.0, 1)
        gx, gy = rx + out[0] * 600.0, ry + out[1] * 600.0
        down("RustWash", gx, gy, ground_z(gx, gy), adit["yaw"], 0.9)
        if number == 1:
            d = spec["Radixite"]
            plan.append(("Radixite", (adit["x"] + side[0] * 2300.0, adit["y"] + side[1] * 2300.0, top - 100.0), facing,
                         (400.0, d["size_m"][0] * 50.0, d["size_m"][1] * 50.0), 1))
    return plan


# In the engine the instances read far weaker than in the kit's Blender previews (first placement, 2026-11-14): the
# dark stains get more opacity and a darker colour, the pale ones more opacity and a slightly brighter colour.
CLIFF_DECAL_PALE = ("CalciteCurtain", "LeachBand", "RockFlourFan", "LichenCrust", "Mark_Strosse", "Mark_Warning_A", "Mark_Warning_B")
CLIFF_DECAL_TUNE = {"dark": {"BaseColour_OpacityScale": 1.45, "BaseColour_Brightness": 0.74, "OpacityLerpB": 1.0},
                    "pale": {"BaseColour_OpacityScale": 1.25, "BaseColour_Brightness": 0.98, "OpacityLerpB": 1.0}}


def tune_cliff_decals():
    """Writes CLIFF_DECAL_TUNE into the kit's decal instances and saves them."""
    mel = unreal.MaterialEditingLibrary
    with open(CLIFF_DECAL_SPEC, "r") as handle:
        names = [d["name"] for d in json.load(handle)["decals"]]
    for name in names:
        mi = unreal.load_asset(CLIFF_DECAL_MI + name)
        if mi is None:
            raise RuntimeError("missing " + CLIFF_DECAL_MI + name)
        for key, value in CLIFF_DECAL_TUNE["pale" if name in CLIFF_DECAL_PALE else "dark"].items():
            mel.set_material_instance_scalar_parameter_value(mi, key, value)
        mel.set_material_instance_scalar_parameter_value(mi, "AngleFadeAmount", 1.0)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log("cliff decal instances tuned: %d" % len(names))


def step_cliff_decals(only_adit=None):
    """Weathering decals on the cliff kit, shared instances used as they are. Labels ORE_CliffDecal_*.
    The adit placeholder boxes stop receiving decals (they stand in front of the portal the marks belong on)."""
    require_map()
    removed = clear_prefix(PREFIX + "CliffDecal_")
    for other in all_actors():
        if other.get_actor_label().startswith(PREFIX + "PH_adit"):
            for comp in other.get_components_by_class(unreal.PrimitiveComponent):
                comp.set_editor_property("receives_decals", False)
    plan = cliff_decal_plan(only_adit)
    materials, placed = {}, {}
    for index, (name, loc, rot, half, sort) in enumerate(plan):
        if name not in materials:
            materials[name] = unreal.load_asset(CLIFF_DECAL_MI + name)
            if materials[name] is None:
                raise RuntimeError("missing " + CLIFF_DECAL_MI + name)
        actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(loc[0], loc[1], loc[2]),
                                                    unreal.Rotator(rot[0], rot[1], rot[2]))
        actor.decal.set_decal_material(materials[name])
        actor.decal.set_editor_property("decal_size", unreal.Vector(half[0], half[2], half[1]))     # quarter turn: swapped
        actor.decal.set_editor_property("sort_order", sort)
        actor.set_actor_label("%sCliffDecal_%s_%03d" % (PREFIX, name, index))
        actor.set_folder_path(FOLDER + "CliffDecals/" + name)
        placed[name] = placed.get(name, 0) + 1
    log("cliff decals: removed %d, placed %d %s" % (removed, len(plan), placed))
    return placed


# ---- adit portals (concept D Bogenausbau, OreAditPortals kit) --------------------------------------------------------
ADIT_MODEL = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreAditPortals/Model/OreAditPortals_model.json"
ADIT_MESHES = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/AditPortals/Meshes/"
# store stacks moved out of the rubble cones: adit 2 PortalCut frame (along, out) in metres
ADIT_STORES = {"ORE_Prop_Adit_Stores_0_0": (-11.0, 22.5), "ORE_Prop_Adit_Stores_0_1": (10.5, 21.0)}


def adit_frames():
    """PortalCut pivots of adit 1 and adit 2 from Ore_cliffs.json: (x, y, z cm, yaw deg)."""
    with open(CLIFF_FILE, "r") as handle:
        cuts = [i for i in json.load(handle)["instances"] if i["piece"] == "PortalCut"]
    return {"adit_1": cuts[0], "adit_2": cuts[1]}


def adit_world(frame, along_m, out_m, up_m=0.0):
    """Piece frame metres (x along the face, y outward, z up) -> world cm (Unreal X outward, Y along, by the yaw)."""
    yaw = math.radians(frame["yaw"])
    lx, ly = out_m * 100.0, along_m * 100.0
    return unreal.Vector(frame["x"] + lx * math.cos(yaw) - ly * math.sin(yaw),
                         frame["y"] + lx * math.sin(yaw) + ly * math.cos(yaw), frame["z"] + up_m * 100.0)


def step_adits():
    """Mine adit portals on adits 1 and 2 (adit 3 keeps the miners' shelter): tube, rubble and jib crane on adit 2,
    rail drift and its rubble on adit 1, all on the PortalCut transforms (the meshes share its pivot and axes). Their
    UCX hulls block units and cut the navmesh. Adds the CapturedTruckDropOff target on the tube's DropOff socket, two
    warm lamps, removes the adit 1/2 placeholders and moves the adit 2 store stacks out of the rubble.
    Labels ORE_Adit_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Adit_")
    with open(ADIT_MODEL, "r") as handle:
        model = json.load(handle)["pieces"]
    frames = adit_frames()
    folder = FOLDER + "AditPortals"

    def mesh_actor(label, name, loc, yaw, decals=True):
        mesh = unreal.load_asset(ADIT_MESHES + name)
        if mesh is None:
            raise RuntimeError("missing " + ADIT_MESHES + name)
        actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0.0, 0.0, yaw))
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        comp.set_mobility(unreal.ComponentMobility.STATIC)
        comp.set_editor_property("receives_decals", decals)      # the cliff's adit soot / blast decals stay on the rock
        actor.set_actor_label(PREFIX + "Adit_" + label)
        actor.set_folder_path(folder)
        return actor

    f1, f2 = frames["adit_1"], frames["adit_2"]
    p2 = unreal.Vector(f2["x"], f2["y"], f2["z"])
    p1 = unreal.Vector(f1["x"], f1["y"], f1["z"])
    mesh_actor("Main_Tube", "SM_OreAdit_MainTube", p2, f2["yaw"], decals=False)
    mesh_actor("Main_Rubble", "SM_OreAdit_MainRubble", p2, f2["yaw"])
    crane = model["SM_OreAdit_MainTube"]["sockets"]["JibCrane"]
    mesh_actor("Main_JibCrane", "SM_OreAdit_JibCrane", adit_world(f2, crane[0], crane[1]), f2["yaw"], decals=False)
    mesh_actor("Drift", "SM_OreAdit_Drift", p1, f1["yaw"], decals=False)
    mesh_actor("Drift_Rubble", "SM_OreAdit_DriftRubble", p1, f1["yaw"])
    # captured convoy lorries drive to the nearest actor tagged CapturedTruckDropOff (ASurrenderingCargoTruck)
    drop = model["SM_OreAdit_MainTube"]["sockets"]["DropOff"]
    loc = adit_world(f2, drop[0], drop[1])
    loc.z = ground_z(loc.x, loc.y) + 20.0
    target = actors_sub().spawn_actor_from_class(unreal.TargetPoint, loc, unreal.Rotator(0.0, 0.0, f2["yaw"] + 180.0))
    target.tags = [unreal.Name("CapturedTruckDropOff")]
    target.set_actor_label(PREFIX + "Adit_DropOff")
    target.set_folder_path(folder)
    for label, frame, piece, sock, intensity, radius in (("Main_LampLight", f2, "SM_OreAdit_MainTube", "Light_Mouth", 18.0, 1300.0),
                                                         ("Drift_LampLight", f1, "SM_OreAdit_Drift", "Light_Drift", 8.0, 700.0)):
        s = model[piece]["sockets"][sock]
        light = actors_sub().spawn_actor_from_class(unreal.PointLight, adit_world(frame, s[0], s[1], s[2] - 0.25), unreal.Rotator(0, 0, 0))
        comp = light.point_light_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_intensity(intensity)
        comp.set_light_color(unreal.LinearColor(1.0, 0.68, 0.35, 1.0))
        comp.set_attenuation_radius(radius)
        comp.set_cast_shadows(False)
        light.set_actor_label(PREFIX + "Adit_" + label)
        light.set_folder_path(folder)
    for pid in ("adit_1", "adit_2"):
        clear_prefix(PREFIX + "PH_" + pid)
        clear_prefix(PREFIX + "Note_PH_" + pid)
    labels = by_label()
    moved = []
    for label, (along, out) in ADIT_STORES.items():
        actor = labels.get(label)
        if actor is None:
            continue
        p = adit_world(f2, along, out)
        p.z = ground_z(p.x, p.y)
        actor.set_actor_location(p, False, True)
        moved.append(label)
    log("adits: removed %d, placed 5 meshes + drop-off + 2 lights at %s, moved %s" % (removed, [round(c) for c in (loc.x, loc.y, loc.z)], moved))
    return loc
