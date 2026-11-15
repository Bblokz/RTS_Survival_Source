"""In-editor build steps for Ger_PripyatMe410 (map creation only: no units, no mission scripting).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap")
    import pripyat_map_build as pmb; importlib.reload(omb)
    pmb.load_layout(); pmb.step_duplicate(); pmb.step_strip(); pmb.step_landscape(); pmb.step_fit_level_actors()

Layout and raw files come from pripyat_terrain_gen.py (Saved/Pripyat/terrain). Generated actors carry the ORE_ label
prefix and sit under the outliner folder Pripyat/, so a step can clear and redo its own part.
"""
import array
import json
import math

import unreal

TEMPLATE_MAP = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap"
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_PripyatMe410"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Pripyat/terrain/Pripyat_layout.json"
SHOTS = "D:/UE5Projects/RTS_Survival/Saved/Pripyat/shots/"
LS_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/Pripyat/"
LANDSCAPE_MATERIAL = LS_ROOT + "MI_RTS_Landscape_Pripyat"
LAYER_NAMES = ["PripPeat", "PripMoss", "PripMud", "PripGrass", "PripSand", "PripGravel", "PripEarth", "PripAsh", "PripAlgae"]
LAYER_INFOS = {name: LS_ROOT + "DATA/%s_LayerInfo" % name for name in LAYER_NAMES}
LANDSCAPE_LABEL = "Pripyat_Landscape"
RVT_ROOT = "/Game/RTS_Survival/Meshes/RVTs/"
PREFIX = "PRP_"
FOLDER = "Pripyat/"
Z_LOW, Z_HIGH = -800.0, 3200.0           # landscape runs from about -300 to 1250 cm
NAV_LOW, NAV_HIGH = -600.0, 2600.0

LAYOUT = {}
HEIGHTS = None
SHOT_BRIGHTNESS = 0.12
SHOT_EXPOSURE = 0.0        # scene captures of this map read about 2.5 stops darker than the game


def log(text):
    unreal.log_warning("PRP_MAP " + str(text))
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
    """Copy the template map to Ger_PripyatMe410 (once) and open it."""
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


def landscape_material():
    """MI_RTS_Landscape_Pripyat, built by Source/Docs/Landscape/ls_pripyat_build.py."""
    mi = unreal.load_asset(LANDSCAPE_MATERIAL)
    if mi is None:
        raise RuntimeError("run ls_pripyat_build.run() first")
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
    start = feature("start")["points"][0]
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
            actor.set_actor_location(unreal.Vector(start[0] + 1500.0, start[1] + offset, start_z + 1500.0), False, False)
        elif name in ("DirectionalLight", "SkyLight", "ExponentialHeightFog", "PostProcessVolume"):
            actor.set_actor_location(unreal.Vector(0.0, 0.0, 6000.0), False, False)
    thick = 600.0
    spots = [("BoundaryTop", half_x + thick, 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryBottom", -(half_x + thick), 0.0, thick, half_y + 2.0 * thick),
             ("BoundaryRight", 0.0, half_y + thick, half_x + 2.0 * thick, thick),
             ("BoundaryLeft", 0.0, -(half_y + thick), half_x + 2.0 * thick, thick)]
    for actor, (label, x, y, ex, ey) in zip(sorted(blockers, key=lambda a: a.get_actor_label()), spots):
        _set_box(actor, x, y, 3000.0, ex, ey, 6000.0)
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
    south-west (Pripyat: cold overcast marsh light, thin grey-green mist)."""
    require_map()
    report = []
    for actor in all_actors():
        name = actor.get_class().get_name()
        if name == "DirectionalLight":
            comp = actor.light_component
            comp.set_intensity(3.8)
            comp.set_light_color(unreal.LinearColor(0.93, 0.95, 1.0, 1.0))
            comp.set_editor_property("atmosphere_sun_light", True)
            actor.set_actor_rotation(unreal.Rotator(0.0, -42.0, -128.0), False)
            report.append("sun")
        elif name == "SkyLight":
            comp = actor.light_component
            comp.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
            comp.set_intensity(1.25)
            comp.set_light_color(unreal.LinearColor(0.92, 0.96, 1.0, 1.0))
            comp.recapture_sky()
            report.append("sky")
        elif name == "ExponentialHeightFog":
            comp = actor.component
            comp.set_fog_density(0.010)
            comp.set_fog_height_falloff(0.10)
            comp.set_fog_inscattering_color(unreal.LinearColor(0.50, 0.53, 0.52, 1.0))
            comp.set_fog_max_opacity(0.6)
            comp.set_start_distance(4500.0)
            comp.set_directional_inscattering_color(unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
            report.append("fog")
        elif name == "PostProcessVolume":
            settings = actor.settings
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.86, 0.86, 0.86, 1.0))
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
    """Orthographic top-down capture, north up (east right), written to Saved/Pripyat/shots/<name>.png."""
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
    _pp = comp.get_editor_property("post_process_settings")
    _pp.set_editor_property("override_auto_exposure_bias", True)
    _pp.set_editor_property("auto_exposure_bias", SHOT_EXPOSURE)
    for _name, _value in (("auto_exposure_min_brightness", SHOT_BRIGHTNESS), ("auto_exposure_max_brightness", SHOT_BRIGHTNESS)):
        _pp.set_editor_property("override_" + _name, True)
        _pp.set_editor_property(_name, _value)
    comp.set_editor_property("post_process_settings", _pp)
    comp.set_editor_property("post_process_blend_weight", 1.0)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + ".png")
    actors_sub().destroy_actor(cap)
    unreal.RenderingLibrary.release_render_target2d(rt)
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
    _pp = comp.get_editor_property("post_process_settings")
    _pp.set_editor_property("override_auto_exposure_bias", True)
    _pp.set_editor_property("auto_exposure_bias", SHOT_EXPOSURE)
    for _name, _value in (("auto_exposure_min_brightness", SHOT_BRIGHTNESS), ("auto_exposure_max_brightness", SHOT_BRIGHTNESS)):
        _pp.set_editor_property("override_" + _name, True)
        _pp.set_editor_property(_name, _value)
    comp.set_editor_property("post_process_settings", _pp)
    comp.set_editor_property("post_process_blend_weight", 1.0)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + ".png")
    actors_sub().destroy_actor(cap)
    unreal.RenderingLibrary.release_render_target2d(rt)
    log("shot %s%s.png" % (SHOTS, name))



# ---- shared helpers (from the Ore Mountains build) ---------------------------------------------------------------
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


PATH_ROOT = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
ROCK_ROOT = "/Game/RTS_Survival/Meshes/Nature/Rocks/RockMoss/SM_RTS_RockMoss_"


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



# ---- water ----------------------------------------------------------------------------------------------------------
WATER_PARENT = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
PRIPYAT_ENV = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/"
WATER_MI = PRIPYAT_ENV + "Water/MI_Pripyat_AlgaeWater"
WATER_LOOK = {
    "scalars": {"WaveStrength_1": 0.0, "Panning Speed_1": 0.02, "Normal Intensity_1": 0.06, "MacroNormalIntensity_1": 0.12,
                "ShoreWidth": 70.0, "BandSpacing": 40.0, "Specular": 0.45, "Tiling_1": 3.0, "BandSpeed": 0.04, "BandWobble": 1.2, "ContactWidth": 18.0,
                "ContactStrength": 0.6, "FoamBreakup": 0.85, "FoamIntensity": 0.6, "FoamOpacity": 0.6,
                "WaterOpacity": 0.93, "OpacityDepth": 110.0, "Roughness": 0.4, "FoamRoughness": 0.6,
                "EmissiveStrength": 0.0, "ColorOpacity": 0.0},
    "vectors": {"Color_1": (0.26, 0.07, 0.30, 0.03), "FoamColor": (0.44, 0.27, 0.56, 1.0)},
}


def water_material():
    """Stagnant purple algae water: a child of the shore-foam water instance (the shared instance is left alone)."""
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(WATER_MI):
        mi = unreal.load_asset(WATER_MI)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            WATER_MI.split("/")[-1], WATER_MI.rsplit("/", 1)[0], unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, unreal.load_asset(WATER_PARENT))
    for name, value in WATER_LOOK["scalars"].items():
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    for name, value in WATER_LOOK["vectors"].items():
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(*value))
    mel.set_material_instance_static_switch_parameter_value(mi, "UseRadixite", False)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def step_water():
    """One water sheet at the bog water table (z 0) under the whole landscape: every pool, river and lake is ground
    carved below it."""
    require_map()
    removed = clear_prefix(PREFIX + "Water")
    lay = LAYOUT["landscape"]
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    comp.set_material(0, water_material())
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_cast_shadow(False)
    actor.set_actor_scale3d(unreal.Vector(lay["size_x"] / 100.0, lay["size_y"] / 100.0, 1.0))
    actor.set_actor_label(PREFIX + "Water_Bog")
    actor.set_folder_path(FOLDER + "Water")
    log("water: removed %d" % removed)
    return actor


# ---- the airfield kit (CustomMapItems/PrypyatAirfield) ----------------------------------------------------------------
AIRFIELD_CONTRACT = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PrypyatAirfield/airfield_placement.json"
# kit defaults put the AA platform on the top of the north ramp: both platforms are placed inside the kit frame here
RIDGE_MODULE_AT = {"Prypyat_Defense_AA": (3800.0, 9200.0), "Prypyat_Defense_Pit": (-6500.0, 9400.0)}


def step_airfield(state="closed"):
    """The 200 x 30 m Soviet airfield with bays, tower, hangar, dumps, service props, sandbags and the two defensive
    platforms, from the placement contract of the kit, at the LAYOUT airfield transform (heading 45). state "closed"
    adds the crushable repair barricades across the runway (the ruined strip the mission has to repair)."""
    require_map()
    removed = clear_prefix("Prypyat_")
    with open(AIRFIELD_CONTRACT, "r") as handle:
        contract = json.load(handle)
    air = LAYOUT["airfield"]
    yaw = air["yaw"]
    cosine, sine = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    origin = (air["centre"][0], air["centre"][1], air["z"])
    records = [{"id": "Prypyat_Runway_And_Buildings", "kind": "static_mesh", "asset_path": contract["runway_mesh"],
                "location_cm": [0, 0, 0], "folder": "Runway"}]
    records += contract["operational_props"] + contract["fortifications"]
    for ridge in contract["ridge_modules"]:
        dx = RIDGE_MODULE_AT[ridge["id"]][0] - ridge["location_cm"][0]
        dy = RIDGE_MODULE_AT[ridge["id"]][1] - ridge["location_cm"][1]
        for part in ridge["parts"]:
            moved = dict(part)
            moved["location_cm"] = [part["location_cm"][0] + dx, part["location_cm"][1] + dy, part["location_cm"][2]]
            records.append(moved)
    if state == "closed":
        records += contract["closed_runway_props"]
    count = 0
    for record in records:
        lx, ly, lz = record["location_cm"]
        loc = unreal.Vector(origin[0] + cosine * lx - sine * ly, origin[1] + sine * lx + cosine * ly, origin[2] + lz)
        rot = unreal.Rotator(0.0, 0.0, yaw + record.get("yaw_degrees", 0.0))
        if record["kind"] == "blueprint":
            cls = unreal.EditorAssetLibrary.load_blueprint_class(record["asset_path"])
            if cls is None:
                log("airfield: missing %s" % record["asset_path"])
                continue
            actor = actors_sub().spawn_actor_from_class(cls, loc, rot)
        else:
            mesh = unreal.load_asset(record["asset_path"])
            if mesh is None:
                log("airfield: missing %s" % record["asset_path"])
                continue
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_label(record["id"])
        actor.set_folder_path(FOLDER + "Airfield/" + record.get("folder", "Airfield"))
        count += 1
    log("airfield: removed %d, placed %d (%s)" % (removed, count, state))
    apply_compound_overrides()
    return count


# ---- dressing (pripyat_dress_gen.py) ------------------------------------------------------------------------------------
DRESS_FILE = "D:/UE5Projects/RTS_Survival/Saved/Pripyat/terrain/Pripyat_dress.json"


def dress():
    with open(DRESS_FILE, "r") as handle:
        return json.load(handle)


def step_dress(only=None):
    """Every generated actor: PRP_D_* blueprint actors, PRP_I_* instance holders, PRP_Note_* editor-only notes.
    only = "bps" / "inst" / "notes" redoes one kind."""
    require_map()
    data = dress()
    report = {}
    if only in (None, "bps"):
        removed = clear_prefix(PREFIX + "D_")
        classes, missing, placed = {}, set(), 0
        for label, path, x, y, yaw, dz, folder, scale in data["bps"]:
            if path not in classes:
                classes[path] = unreal.EditorAssetLibrary.load_blueprint_class(path)
            if classes[path] is None:
                missing.add(path)
                continue
            actor = actors_sub().spawn_actor_from_class(classes[path], unreal.Vector(x, y, ground_z(x, y) + dz), unreal.Rotator(0.0, 0.0, yaw))
            if actor is None:
                missing.add(path)
                continue
            if scale != 1.0:
                actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            actor.set_actor_label(PREFIX + "D_" + label)
            actor.set_folder_path(FOLDER + folder)
            placed += 1
        report["bps"] = (removed, placed, sorted(missing))
        report["overrides"] = apply_compound_overrides()
    if only in (None, "inst"):
        removed = clear_prefix(PREFIX + "I_")
        total = 0
        for group, spec in sorted(data["inst"].items()):
            mesh = unreal.load_asset(spec["mesh"])
            if mesh is None:
                report.setdefault("missing meshes", []).append(spec["mesh"])
                continue
            transforms = []
            for x, y, dz, yaw, scale, pitch, roll in spec["items"]:
                z = spec["abs_z"] if "abs_z" in spec else ground_z(x, y) + dz
                transforms.append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll, pitch, yaw), unreal.Vector(scale, scale, scale)))
            actor = _instance_actor(PREFIX + "I_" + group, FOLDER + "Instances", mesh, transforms, collision=spec.get("collision", False))
            comp = actor.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            if comp and spec.get("material"):
                override = unreal.load_asset(spec["material"])
                if override is not None:
                    for slot in range(comp.get_num_materials()):
                        comp.set_material(slot, override)
            if comp and not spec.get("shadow", True):
                comp.set_cast_shadow(False)
            total += len(transforms)
        report["inst"] = (removed, len(data["inst"]), total)
    if only in (None, "notes"):
        removed = clear_prefix(PREFIX + "Note_")
        for nid, words, x, y in data["notes"]:
            actor = actors_sub().spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, ground_z(x, y) + 320.0), unreal.Rotator(0.0, 90.0, 180.0))
            comp = actor.text_render
            comp.set_text(words)
            comp.set_world_size(300.0)
            comp.set_text_render_color(unreal.Color(255, 230, 40, 255))
            comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
            actor.set_actor_label(PREFIX + "Note_" + nid)
            actor.set_folder_path(FOLDER + "Notes")
            actor.set_editor_property("is_editor_only_actor", True)
        report["notes"] = (removed, len(data["notes"]))
    log("dress %s" % report)
    return report


LANDSCAPE_TUNE = {
    "scalars": {"PripSand_Desaturation": 0.7, "PripGravel_Desaturation": 0.55, "PripGrass_Desaturation": 0.45,
                "PripAlgae_Desaturation": 0.95, "PripAlgae_Roughness": 0.62, "PripMud_Roughness": 0.66, "PripPeat_Roughness": 0.85,
                "PripMud_TileSize": 4.5, "PripMud_Desaturation": 0.5},
    "vectors": {"PripGravel_Tint": (1.25, 1.18, 1.05), "PripSand_Tint": (0.40, 0.375, 0.32), "PripGrass_Tint": (0.86, 0.80, 0.58),
                "PripMoss_Tint": (0.52, 0.58, 0.36), "PripPeat_Tint": (0.40, 0.33, 0.27), "PripAlgae_Tint": (0.34, 0.20, 0.40),
                "PripMud_Tint": (0.13, 0.105, 0.09), "PripAsh_Tint": (0.62, 0.61, 0.62)},
}


def tune_landscape_material():
    """Instance overrides after the first in-game look (the master defaults stay as built)."""
    mel = unreal.MaterialEditingLibrary
    mi = unreal.load_asset(LANDSCAPE_MATERIAL)
    for name, value in LANDSCAPE_TUNE["scalars"].items():
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    for name, value in LANDSCAPE_TUNE["vectors"].items():
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)


def step_fx():
    """Niagara actors from the dress file (smouldering peat). Labels PRP_FX_*."""
    require_map()
    removed = clear_prefix(PREFIX + "FX_")
    count = 0
    for label, system, x, y, dz, scale in dress().get("fx", []):
        asset = unreal.load_asset(system)
        if asset is None:
            log("fx: missing %s" % system)
            continue
        actor = actors_sub().spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(x, y, ground_z(x, y) + dz))
        comp = actor.niagara_component
        comp.set_asset(asset)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_actor_label(PREFIX + "FX_" + label)
        actor.set_folder_path(FOLDER + "FX")
        count += 1
    log("fx: removed %d, placed %d" % (removed, count))
    return count


def step_splines():
    """Ground-path splines from the dress file (road surfaces over the paint). Labels PRP_S_*."""
    require_map()
    removed = clear_prefix(PREFIX + "S_")
    for label, bp, points, width, material in dress().get("splines", []):
        _path_actor(PREFIX + "S_" + label, bp, points, width, unreal.load_asset(material) if material else None)
    return removed


def step_decals():
    """Crater and scorch decals from the dress file. Labels PRP_Decal_*."""
    require_map()
    removed = clear_prefix(PREFIX + "Decal_")
    count = 0
    for label, material, x, y, yaw, size in dress().get("decals", []):
        mat = unreal.load_asset(material)
        if mat is None:
            log("decal: missing %s" % material)
            continue
        actor = actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, ground_z(x, y) + 40.0), unreal.Rotator(0.0, -90.0, yaw))
        actor.decal.set_decal_material(mat)
        actor.decal.set_editor_property("decal_size", unreal.Vector(160.0, size / 2.0, size / 2.0))
        actor.set_actor_label(PREFIX + "Decal_" + label)
        actor.set_folder_path(FOLDER + "Decals")
        count += 1
    log("decals: removed %d, placed %d" % (removed, count))
    return count


def map_materials():
    """Map-only instances of kit materials (the kits keep their own looks)."""
    mel = unreal.MaterialEditingLibrary
    runway = unreal.load_asset(PRIPYAT_ENV + "Airfield/Materials/MI_Prypyat_Runway")
    if runway is not None:
        # the first ruin pass drew every slab joint as a wide ochre line: thin, patchy joints instead
        for name, value in (("JointGrass", 0.3), ("JointWidth", 5.0), ("CrackGrass", 0.75), ("MarginGrass", 0.9), ("ConcreteDarken", 0.55)):
            mel.set_material_instance_scalar_parameter_value(runway, name, value)
        mel.update_material_instance(runway)
        unreal.EditorAssetLibrary.save_loaded_asset(runway, False)
    def child(name, parent, scalars=(), vectors=()):
        path = PRIPYAT_ENV + "Map/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            made = unreal.load_asset(path)
        else:
            made = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PRIPYAT_ENV + "Map", unreal.MaterialInstanceConstant,
                                                                           unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(made, unreal.load_asset(parent))
        for key, value in scalars:
            mel.set_material_instance_scalar_parameter_value(made, key, value)
        for key, value in vectors:
            mel.set_material_instance_vector_parameter_value(made, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        mel.update_material_instance(made)
        unreal.EditorAssetLibrary.save_loaded_asset(made, False)

    # the ring's surviving macadam: dusty and dark, close to the two-track gravel
    child("MI_Pripyat_Road_Dark", "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Road/MI_GroundPath_Road_WarTorn",
          scalars=(("DustAmount", 1.0), ("WeedAmount", 0.8), ("GroundBlend", 1.0), ("DampAmount", 0.6), ("CrackAmount", 1.0), ("AlligatorAmount", 1.0)), vectors=(("DustTint", (0.30, 0.28, 0.25)),))
    # Phytophage growth on walls and trunks: the solid dark plum, matte
    child("MI_Pripyat_Phytophage_Dull", PRIPYAT_ENV + "Phytophage/MI_Phytophage_BlackPlum_Solid",
          scalars=(("DryRoughness", 0.8), ("WetRoughness", 0.6)), vectors=(("FibreTint", (0.16, 0.06, 0.2)), ("DarkTint", (0.03, 0.012, 0.04))))
    for name, tint in (("MI_Pripyat_BogFoliage_Olive", (0.24, 0.25, 0.15)), ("MI_Pripyat_BogFoliage_Brown", (0.26, 0.21, 0.14))):
        path = PRIPYAT_ENV + "Map/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            extra = unreal.load_asset(path)
        else:
            extra = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PRIPYAT_ENV + "Map", unreal.MaterialInstanceConstant,
                                                                            unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(extra, unreal.load_asset(PRIPYAT_ENV + "BogFoliage/Materials/MI_Prypyat_BogFoliage"))
        mel.set_material_instance_vector_parameter_value(extra, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mel.update_material_instance(extra)
        unreal.EditorAssetLibrary.save_loaded_asset(extra, False)
    path = PRIPYAT_ENV + "Map/MI_Pripyat_BogFoliage_Dark"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.split("/")[-1], path.rsplit("/", 1)[0], unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, unreal.load_asset(PRIPYAT_ENV + "BogFoliage/Materials/MI_Prypyat_BogFoliage"))
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(0.33, 0.29, 0.19, 1.0))
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    compound_materials()
    return mi


# ---- airfield compound material pass (2026-11-14): the three value outliers of the final regrade ----------------------
# Pads: the kit's own earth material (only the four Prypyat_Defense_*_Core/Ramp meshes use it) gets a dark wet-earth colour.
RIDGE_EARTH = PRIPYAT_ENV + "Airfield/Materials/M_Prypyat_RidgeEarth"
RIDGE_EARTH_COLOUR = (0.31, 0.27, 0.21)          # kit default 0.72 0.57 0.38 (pale tan)
# The shared masters of the hangar (MM_RTS_Env), the bunker (M_RusBunker_Concrete) and the dump nets
# (M_Russian_CamoPattern_Tent) have no tint: Map/ holds copies with Desaturation(WeatherDesaturation) x WeatherTint
# in front of the base colour (everything else in the graph, WallGate VAT included, is the original).
ENV_WEATHERED = PRIPYAT_ENV + "Map/M_Pripyat_Env_Weathered"
BUNKER_WEATHERED = PRIPYAT_ENV + "Map/M_Pripyat_BunkerConcrete_Weathered"
CAMO_WEATHERED = PRIPYAT_ENV + "Map/M_Pripyat_CamoTent_Weathered"
WEATHERED_MASTERS = {ENV_WEATHERED: "/Game/RTS_Survival/Materials/MasterMaterials/MM_RTS_Env",
                     BUNKER_WEATHERED: "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete",
                     CAMO_WEATHERED: "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FactoryMeshes/Materials/M_Russian_CamoPattern_Tent"}
# name -> (parent master copy, source material whose parameter values are carried over, look)
# - hangar: shared BP_Industrial_Dest_Hangar, MI_RTS_Hangar (white corrugated sheet)
# - command post: shared BP_Military_Dest_Bunker05, MI_Rus_BunkerConcrete_05; the master lerps the concrete toward a deep
#   red by SandStrength (the pink), and Metallic_Multiplier 0.6 made the bare concrete read silver
# - generator fuel tank: shared BP_Industrial_Dest_OilTank2, MI_BarrelsOil_Atlas (white tank)
# - dump camo nets: slot 6 of the kit runway mesh, the orange desert pattern
WEATHERED_MIS = {
    "MI_Pripyat_Hangar_Weathered": (ENV_WEATHERED, "/Game/RTS_Survival/Meshes/Industrial/Hangar/Materials/MI_RTS_Hangar",
                                    {"scalars": {"WeatherDesaturation": 0.6, "MetalMlt": 0.25}, "vectors": {"WeatherTint": (0.28, 0.255, 0.22)}}),
    "MI_Pripyat_CommandPost_Concrete": (BUNKER_WEATHERED, "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05",
                                        {"scalars": {"SandStrength": 0.0, "Metallic_Multiplier": 0.0, "Roughness_Multiplier": 1.5, "WeatherDesaturation": 0.5},
                                         "vectors": {"WeatherTint": (0.42, 0.40, 0.37)}}),
    "MI_Pripyat_OilTank_Weathered": (ENV_WEATHERED, "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/Meshes/Materials/MI_BarrelsOil_Atlas",
                                     {"scalars": {"WeatherDesaturation": 0.6, "MetalMlt": 0.0}, "vectors": {"WeatherTint": (0.25, 0.235, 0.21)}}),
    "MI_Pripyat_DumpCamoNet": (CAMO_WEATHERED, CAMO_WEATHERED, {"scalars": {"WeatherDesaturation": 0.65}, "vectors": {"WeatherTint": (0.50, 0.47, 0.40)}}),
}
# (actor label, component name, slot, material: a WEATHERED_MIS name or a full asset path). The bunker's collapse chunks
# (geometry collection, assigned only at death) take the same instance in slot 0, or the ruin turns red again once
# destroyed. The hangar's and the tank's death swaps keep the slot override on their wreck meshes (same UV layouts).
COMPOUND_OVERRIDES = [(PREFIX + "D_Workshop", "StaticMesh", 0, "MI_Pripyat_Hangar_Weathered"),
                      (PREFIX + "D_CommandPost", "StaticMesh", 0, "MI_Pripyat_CommandPost_Concrete"),
                      (PREFIX + "D_CommandPost", "GeometryCollection", 0, "MI_Pripyat_CommandPost_Concrete"),
                      (PREFIX + "D_Gen_Fuel", "StaticMesh", 0, "MI_Pripyat_OilTank_Weathered"),
                      ("Prypyat_Runway_And_Buildings", "StaticMeshComponent0", 6, "MI_Pripyat_DumpCamoNet")]


def _map_mi(name, parent_path):
    """The instance under Prypyat/Map, parented to parent_path, with every parameter override cleared (callers set them)."""
    mel = unreal.MaterialEditingLibrary
    path = PRIPYAT_ENV + "Map/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        made = unreal.load_asset(path)
    else:
        made = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PRIPYAT_ENV + "Map", unreal.MaterialInstanceConstant,
                                                                       unreal.MaterialInstanceConstantFactoryNew())
    parent = unreal.load_asset(parent_path)
    if made.get_editor_property("parent") != parent:
        mel.set_material_instance_parent(made, parent)
    mel.clear_all_material_instance_parameters(made)
    return made


def _weathered_master(path, source_path):
    """Copy of source_path with base colour -> Desaturation(WeatherDesaturation) x WeatherTint -> base colour."""
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    master = unreal.EditorAssetLibrary.duplicate_asset(source_path, path)
    colour = mel.get_material_property_input_node(master, unreal.MaterialProperty.MP_BASE_COLOR)
    desat = mel.create_material_expression(master, unreal.MaterialExpressionDesaturation, -300, -400)
    amount = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -560, -300)
    amount.set_editor_property("parameter_name", "WeatherDesaturation")
    amount.set_editor_property("default_value", 0.0)
    tint = mel.create_material_expression(master, unreal.MaterialExpressionVectorParameter, -300, -250)
    tint.set_editor_property("parameter_name", "WeatherTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mult = mel.create_material_expression(master, unreal.MaterialExpressionMultiply, -100, -350)
    # a texture sample's colour output is "RGB"; a lerp's or a vector parameter's first output has no name
    out = "RGB" if isinstance(colour, unreal.MaterialExpressionTextureSample) else ""
    mel.connect_material_expressions(colour, out, desat, "")
    mel.connect_material_expressions(amount, "", desat, "Fraction")
    mel.connect_material_expressions(desat, "", mult, "A")
    mel.connect_material_expressions(tint, "", mult, "B")
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(master)
    unreal.EditorAssetLibrary.save_loaded_asset(master, False)
    return master


def compound_materials():
    """Assets of the compound pass: dark pads (kit material edited in place), the weathered master copies and their
    instances (hangar, command-post ruin, generator fuel tank, dump camo nets)."""
    mel = unreal.MaterialEditingLibrary
    earth = unreal.load_asset(RIDGE_EARTH)
    colour = mel.get_material_property_input_node(earth, unreal.MaterialProperty.MP_BASE_COLOR)
    for node in mel.get_inputs_for_material_expression(earth, colour):
        if isinstance(node, unreal.MaterialExpressionConstant3Vector):
            node.set_editor_property("constant", unreal.LinearColor(RIDGE_EARTH_COLOUR[0], RIDGE_EARTH_COLOUR[1], RIDGE_EARTH_COLOUR[2], 1.0))
    mel.recompile_material(earth)
    unreal.EditorAssetLibrary.save_loaded_asset(earth, False)

    for path, source_path in WEATHERED_MASTERS.items():
        _weathered_master(path, source_path)
    for mi_name, (parent, source_path, look) in WEATHERED_MIS.items():
        source = unreal.load_asset(source_path)
        made = _map_mi(mi_name, parent)
        if isinstance(source, unreal.MaterialInstance):
            # the source instance's textures, scalars and switches, carried over to the weathered copy of its master
            for name in mel.get_texture_parameter_names(source):
                mel.set_material_instance_texture_parameter_value(made, name, mel.get_material_instance_texture_parameter_value(source, name))
            for name in mel.get_scalar_parameter_names(source):
                mel.set_material_instance_scalar_parameter_value(made, name, mel.get_material_instance_scalar_parameter_value(source, name))
            for name in mel.get_static_switch_parameter_names(source):
                mel.set_material_instance_static_switch_parameter_value(made, name, mel.get_material_instance_static_switch_parameter_value(source, name))
        for name, value in look.get("scalars", {}).items():
            mel.set_material_instance_scalar_parameter_value(made, name, value)
        for name, value in look.get("vectors", {}).items():
            mel.set_material_instance_vector_parameter_value(made, name, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        for name, value in look.get("textures", {}).items():
            mel.set_material_instance_texture_parameter_value(made, name, unreal.load_asset(value))
        mel.update_material_instance(made)
        unreal.EditorAssetLibrary.save_loaded_asset(made, False)


def apply_compound_overrides():
    """Per-actor material overrides on this map only (the shared Blueprints and meshes stay as they are). Called at the
    end of step_airfield and of the step_dress Blueprint pass, which respawn these actors."""
    labels = by_label()
    done = []
    for label, comp_name, slot, mi_name in COMPOUND_OVERRIDES:
        actor = labels.get(label)
        mi = unreal.load_asset(mi_name if mi_name.startswith("/") else PRIPYAT_ENV + "Map/" + mi_name)
        if actor is None:
            continue           # step_airfield and step_dress each call this; the other step's actors may not exist yet
        if mi is None:
            log("compound override: missing %s / %s" % (label, mi_name))
            continue
        for comp in actor.get_components_by_class(unreal.MeshComponent):
            if comp.get_name() != comp_name:
                continue
            # set the override array directly: a geometry collection has no slots until its rest collection is assigned
            overrides = list(comp.get_editor_property("override_materials"))
            overrides += [None] * (slot + 1 - len(overrides))
            overrides[slot] = mi
            comp.modify()
            comp.set_editor_property("override_materials", overrides)
            if slot < comp.get_num_materials():
                comp.set_material(slot, mi)          # refreshes the render state
            done.append("%s.%s[%d]" % (label, comp_name, slot))
    log("compound overrides %s" % done)
    return done


def clear_orphans():
    """Helper actors that Blueprints spawn beside themselves (nav blockers) outlive their owner when a step clears
    and respawns it. Call before step_airfield + step_dress, which spawn fresh ones."""
    count = 0
    for actor in all_actors():
        if actor.get_class().get_name().startswith("RTS_BP_NavBlocker") and not actor.get_actor_label().startswith(PREFIX):
            actors_sub().destroy_actor(actor)
            count += 1
    log("orphans removed %d" % count)
    return count
