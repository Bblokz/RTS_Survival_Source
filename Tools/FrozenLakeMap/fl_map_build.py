"""In-editor build steps for Ger_FrozenLake (first pass: terrain, light, placeholder ice and water, notes; no units,
no mission scripting, no kit meshes yet).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap")
    import fl_map_build as fmb; importlib.reload(fmb)
    fmb.load_layout(); fmb.step_duplicate(); fmb.step_strip(); fmb.step_landscape(); fmb.step_fit_level_actors()
    fmb.step_lighting(); fmb.step_ice_placeholder(); fmb.step_water(); fmb.step_notes()

Layout and raw files come from fl_terrain_gen.py (Saved/FrozenLake/terrain). Generated actors carry the FL_ label
prefix and sit under the outliner folder FrozenLake/, so a step can clear and redo its own part.
The ice is a separate plane (owner's decision 2026-11-17: vehicles must be able to break through and sink). This pass
places ONE placeholder plane at z = 0; the ice shader session (Source/Docs/Campaign/FrozenLakeIce.md) delivers the
real materials, and the three ice types (heavy / medium / infantry-only) replace it later.
"""
import array
import json
import math

import unreal

TEMPLATE_MAP = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap"
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake"
LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/FrozenLake/terrain/FL_layout.json"
SHOTS = "D:/UE5Projects/RTS_Survival/Saved/FrozenLake/shots/"
LS_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/FrozenLake/"
LANDSCAPE_MATERIAL = LS_ROOT + "MI_RTS_Landscape_FrozenLake"
LAYER_NAMES = ["FLSnow", "FLFrost", "FLMoss", "FLLichen", "FLHeath", "FLPeat", "FLGranite", "FLGravel", "FLBurnt"]
LAYER_INFOS = {name: LS_ROOT + "DATA/%s_LayerInfo" % name for name in LAYER_NAMES}
FL_ROOT = "/Game/RTS_Survival/Blueprints/Environment/FrozenLake/"
ICE_PLACEHOLDER_MI = FL_ROOT + "Map/MI_FL_IcePlaceholder"
WATER_MI = "/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore"
LANDSCAPE_LABEL = "FrozenLake_Landscape"
RVT_ROOT = "/Game/RTS_Survival/Meshes/RVTs/"
PREFIX = "FL_"
FOLDER = "FrozenLake/"
Z_LOW, Z_HIGH = -700.0, 1500.0           # landscape runs from about -480 to 560 cm
NAV_LOW, NAV_HIGH = -600.0, 1400.0
ICE_Z = 0.0
WATER_Z = -70.0                          # open water in the gorge and canal, under the ice level

LAYOUT = {}
HEIGHTS = None
SHOT_BRIGHTNESS = 0.12
SHOT_EXPOSURE = 0.0
SHOT_SOURCE = None                        # None = final colour; e.g. unreal.SceneCaptureSource.SCS_BASE_COLOR to measure


def log(text):
    unreal.log_warning("FL_MAP " + str(text))
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
    """Landscape height from the generator file (bilinear)."""
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


def surface_z(x, y):
    """Where a unit stands: the ice plane over the lake, else the landscape."""
    return max(ground_z(x, y), ICE_Z)


def nav_ready():
    """True once no navmesh build is running (use as a _defer before saving)."""
    return not unreal.NavigationSystemV1.get_navigation_system(world()).is_navigation_being_built(world())


# ---- map file ---------------------------------------------------------------------------------------------------
def step_duplicate():
    """Copy the template map to Ger_FrozenLake (once) and open it."""
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
    infos = [unreal.load_asset(LAYER_INFOS[name]) for name in names]
    missing = [n for n, info in zip(names, infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s" % missing)
    bad = [n for n, info in zip(names, infos) if str(info.get_editor_property("layer_name")) != n]
    if bad:
        raise RuntimeError("layer infos with the wrong LayerName %s (importing them crashes)" % bad)
    material = unreal.load_asset(LANDSCAPE_MATERIAL)
    if material is None:
        raise RuntimeError("missing %s (run Source/Docs/Landscape/ls_frozenlake_build.py)" % LANDSCAPE_MATERIAL)
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
    """Heights only, from the generator's raw file (paint stays)."""
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
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 45.0), False)     # facing the lake (north-east)
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
    """Karelian late winter afternoon: a low, cold, clear sun from the south-west, pale blue sky light, thin cold
    haze. Colder than every other map (the cryo pumps) so the white disc reads against the brown world round it."""
    require_map()
    report = []
    for actor in all_actors():
        name = actor.get_class().get_name()
        if name == "DirectionalLight":
            comp = actor.light_component
            comp.set_intensity(4.0)
            comp.set_light_color(unreal.LinearColor(0.96, 0.97, 1.0, 1.0))
            comp.set_editor_property("atmosphere_sun_light", True)
            actor.set_actor_rotation(unreal.Rotator(0.0, -34.0, 45.0), False)   # from the south-west, 34 deg up
            report.append("sun")
        elif name == "SkyLight":
            comp = actor.light_component
            comp.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
            comp.set_intensity(1.3)
            comp.set_light_color(unreal.LinearColor(0.90, 0.95, 1.0, 1.0))
            comp.recapture_sky()
            report.append("sky")
        elif name == "ExponentialHeightFog":
            comp = actor.component
            comp.set_fog_density(0.008)
            comp.set_fog_height_falloff(0.12)
            comp.set_fog_inscattering_color(unreal.LinearColor(0.62, 0.68, 0.76, 1.0))
            comp.set_fog_max_opacity(0.55)
            comp.set_start_distance(4500.0)
            comp.set_directional_inscattering_color(unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
            report.append("fog")
        elif name == "PostProcessVolume":
            settings = actor.settings
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.88, 0.88, 0.88, 1.0))
            settings.set_editor_property("override_white_temp", True)
            settings.set_editor_property("white_temp", 6900.0)
            actor.set_editor_property("settings", settings)
            report.append("post")
    if not [a for a in all_actors() if a.get_class().get_name() == "SkyAtmosphere"]:
        sky = actors_sub().spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0))
        sky.set_actor_label("SkyAtmosphere")
        report.append("sky atmosphere added")
    log("lighting %s" % report)
    return report


# The UVB80 mud and MilitaryAirport albedos are far darker than the sand and snow (median linear luminance ~0.1):
# without these the frost rim, bank faces, roads and forest floors read black next to the snow (2026-11-17 shots).
# MI tints, measured 2026-11-17 from a base-colour capture (albedo_top.png): the source textures are dark, so the
# tints scale each layer to a readable albedo (8-bit base colour): snow ~0.78, lichen ~0.45, granite ~0.35,
# gravel ~0.30, heath ~0.26/0.15/0.09, moss ~0.20/0.21/0.10, peat ~0.10, burnt ~0.11
LANDSCAPE_TUNE = {
    "vectors": {"FLFrost_Tint": (4.2, 4.3, 4.6), "FLGravel_Tint": (4.9, 4.7, 4.5),
                "FLMoss_Tint": (1.6, 1.56, 1.65), "FLLichen_Tint": (3.0, 3.1, 3.2),
                "FLHeath_Tint": (1.20, 0.98, 0.86), "FLPeat_Tint": (0.56, 0.55, 0.55),
                "FLGranite_Tint": (1.87, 2.17, 2.40), "FLBurnt_Tint": (2.6, 2.8, 2.9),
                # snow ruts (ls_frozenlake_snowruts.py): a shadowed trough, peat at the centre
                "SnowRut_Tint": (0.30, 0.34, 0.44)},
    "scalars": {"FLFrost_Desaturation": 0.9, "FLGravel_Desaturation": 0.45,
                "SnowRut_Expose": 0.6, "SnowRut_NormalStrength": 5.5, "SnowRut_BermBrightness": 0.3},
}


def tune_landscape_material():
    mel = unreal.MaterialEditingLibrary
    mi = unreal.load_asset(LANDSCAPE_MATERIAL)
    for name, (r, g, b) in LANDSCAPE_TUNE["vectors"].items():
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
    for name, value in LANDSCAPE_TUNE["scalars"].items():
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log("landscape MI tuned %s" % LANDSCAPE_TUNE)


# ---- placeholder ice and open water ----------------------------------------------------------------------------
def _plain_mi(path, colour, roughness=0.6):
    """A child of the engine BasicShapeMaterial with one colour (placeholders only)."""
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            path.split("/")[-1], path.rsplit("/", 1)[0], unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(
            mi, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, "Color", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def _plane(label, folder, centre, z, size_x, size_y, yaw, material, collision_from=None):
    """The engine 1 m plane scaled to size (cm). With collision_from, it copies that component's collision (the
    landscape's: units only drive on ground that blocks the Landscape trace channel)."""
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(centre[0], centre[1], z),
                                                unreal.Rotator(0.0, 0.0, yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    comp.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size_x / 100.0, size_y / 100.0, 1.0))
    if collision_from is None:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    else:
        comp.set_editor_property("body_instance", collision_from.get_editor_property("body_instance"))
        comp.set_editor_property("can_ever_affect_navigation", True)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def step_ice_placeholder():
    """ONE flat plane at z = 0 over the lake, with the landscape's collision, so units and the navmesh have ice to
    stand on until the ice session's three ice types replace it. The plane reaches under the shore (the landscape
    hides it there); it is cut to the lake's extent so the gorge and canal stay open water."""
    require_map()
    clear_prefix(PREFIX + "Ice")
    landscape = by_label()[LANDSCAPE_LABEL]
    mi = _plain_mi(ICE_PLACEHOLDER_MI, (0.78, 0.84, 0.90), 0.5)
    pts = [p for rid in ("lake_ice", "inlet_ice") for p in region(rid)["points"]]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    x0, x1, y0, y1 = min(xs) - 400.0, max(xs) + 300.0, min(ys) - 400.0, max(ys) + 400.0
    actor = _plane(PREFIX + "IcePlaceholder", FOLDER + "Ice", ((x0 + x1) / 2.0, (y0 + y1) / 2.0), ICE_Z,
                   x1 - x0, y1 - y0, 0.0, mi, collision_from=landscape)
    log("ice placeholder x %.0f..%.0f y %.0f..%.0f" % (x0, x1, y0, y1))
    return actor


ICE_MESH_DIR = FL_ROOT + "Map"
ICE_MESH_NAME = "SM_FL_IcePlaceholder"
ICE_OBJ = "D:/UE5Projects/RTS_Survival/Saved/FrozenLake/terrain/SM_FL_IcePlaceholder.obj"
ICE_TUCK_CM = 150.0      # the plane reaches this far under the shore (the land there is under 10 cm: no gap under it)


def _write_ice_obj():
    """Lake-shaped ice mesh: a fan from the lake centre over the blob outline (the blob is radial, so the fan never
    folds), pushed ICE_TUCK_CM out, plus the inlet strip as a quad strip. Written in UE cm (x north, y east, z up);
    the import is checked against the outline's bounds afterwards."""
    blob = region("lake_ice")["points"]
    out = []
    for x, y in blob:
        r = math.hypot(x, y)
        out.append((x * (r + ICE_TUCK_CM) / r, y * (r + ICE_TUCK_CM) / r))
    verts = [(0.0, 0.0)] + out
    faces = [(1, 2 + k, 2 + (k + 1) % len(out)) for k in range(len(out))]
    inlet = region("inlet_ice")
    line = inlet["raw"]["line"]
    half = inlet["raw"]["width"] / 2.0 + ICE_TUCK_CM
    (ax, ay), (bx, by) = line[0], line[-1]
    length = math.hypot(bx - ax, by - ay)
    nx, ny = -(by - ay) / length, (bx - ax) / length
    base = len(verts) + 1
    verts += [(ax + nx * half, ay + ny * half), (ax - nx * half, ay - ny * half),
              (bx - nx * half, by - ny * half), (bx + nx * half, by + ny * half)]
    faces += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    with open(ICE_OBJ, "w") as handle:
        handle.write("# Ger_FrozenLake placeholder ice (fl_map_build._write_ice_obj)\n")
        for x, y in verts:
            # the OBJ import mirrors y (right- to left-handed): write -y so the mesh lands in UE's frame
            handle.write("v %.1f %.1f 0.0\n" % (x, -y))
        for x, y in verts:
            handle.write("vt %.5f %.5f\n" % (x / 1000.0, y / 1000.0))
        for a, b, c in faces:
            # counter-clockwise seen from above in a right-handed x/y frame; checked after import
            handle.write("f %d/%d %d/%d %d/%d\n" % (a, a, c, c, b, b))
    xs, ys = [v[0] for v in verts], [v[1] for v in verts]
    return (min(xs), max(xs), min(ys), max(ys))


def step_ice_mesh():
    """Import the lake-shaped placeholder ice and put it in the level instead of the rectangle (the rectangle reached
    under the dam and the shore, and the navmesh grew a tunnel on it under the land)."""
    require_map()
    want = _write_ice_obj()
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", ICE_OBJ)
    task.set_editor_property("destination_path", ICE_MESH_DIR)
    task.set_editor_property("destination_name", ICE_MESH_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(ICE_MESH_DIR + "/" + ICE_MESH_NAME)
    if mesh is None:
        raise RuntimeError("ice mesh import failed: %s" % list(task.get_editor_property("imported_object_paths")))
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_material(0, _plain_mi(ICE_PLACEHOLDER_MI, (0.78, 0.84, 0.90), 0.5))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    box = mesh.get_bounding_box()
    got = (box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z)
    log("ice mesh bounds %s, wanted x %.0f..%.0f y %.0f..%.0f" % (tuple(round(v) for v in got), *want))
    clear_prefix(PREFIX + "Ice")
    landscape = by_label()[LANDSCAPE_LABEL]
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, ICE_Z))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_editor_property("body_instance", landscape.get_editor_property("body_instance"))
    comp.set_editor_property("can_ever_affect_navigation", True)
    actor.set_actor_label(PREFIX + "IcePlaceholder")
    actor.set_folder_path(FOLDER + "Ice")
    return got, want


# ---- real ice (fl_ice_gen.py): one lake mesh + state mask, lead holes, per-type nav areas -------------------------
ICE_DATA = "D:/UE5Projects/RTS_Survival/Saved/FrozenLake/ice/map/FL_IceNav.json"
ICE_MAP_DIR = FL_ROOT + "Map"
ICE_NAV_DIR = FL_ROOT + "Nav"
ICE_MASTER = FL_ROOT + "Ice/M_FrozenLakeIce"
ICE_LAKE_PARENT = FL_ROOT + "Ice/MI_FrozenLakeIce_Lake"      # the ice session's look instance (if it exists)
ICE_LAKE_MI = ICE_MAP_DIR + "/MI_FL_IceLake"
ICE_STATE_TEX = ICE_MAP_DIR + "/T_FL_IceState"
LEAD_WATER_Z = -30.0                      # water surface in the lead holes, under the ice
# per-agent meta areas; agent order is DefaultEngine.ini SupportedAgents: Character, LightTank, MediumTank, HeavyTank
ICE_NAV_AREA_BPS = {"NoHeavy": "BP_NavArea_FL_IceNoHeavy", "InfantryOnly": "BP_NavArea_FL_IceInfantryOnly"}
AGENT_ORDER = ["Character", "LightTank", "MediumTank", "HeavyTank"]


def _ice_data():
    with open(ICE_DATA, "r") as handle:
        return json.load(handle)


def _import(filename, folder, name, texture=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(folder + "/" + name)
    if asset is None:
        raise RuntimeError("import failed %s: %s" % (filename, list(task.get_editor_property("imported_object_paths"))))
    return asset


def _meta_area(name, blocked_agents):
    """Blueprint child of NavAreaMeta_SwitchByAgent: Null for the blocked agents, Default for the rest."""
    path = ICE_NAV_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.NavAreaMeta_SwitchByAgent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ICE_NAV_DIR, unreal.Blueprint, factory)
    cls = bp.generated_class()
    cdo = unreal.get_default_object(cls)
    for i, agent in enumerate(AGENT_ORDER):
        area = unreal.NavArea_Null if agent in blocked_agents else unreal.NavArea_Default
        cdo.set_editor_property("agent%d_area" % i, area.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    got = [str(cdo.get_editor_property("agent%d_area" % i).get_name()) for i in range(len(AGENT_ORDER))]
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log("meta area %s -> %s" % (name, got))
    return bp.generated_class()


def _ice_lake_mi(mask):
    tex = _import(mask["file"], ICE_MAP_DIR, ICE_STATE_TEX.split("/")[-1], texture=True)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    parent_path = ICE_LAKE_PARENT if unreal.EditorAssetLibrary.does_asset_exist(ICE_LAKE_PARENT) else ICE_MASTER
    if unreal.EditorAssetLibrary.does_asset_exist(ICE_LAKE_MI):
        mi = unreal.load_asset(ICE_LAKE_MI)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ICE_LAKE_MI.split("/")[-1], ICE_MAP_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, unreal.load_asset(parent_path))
    mel.set_material_instance_static_switch_parameter_value(mi, "UseStateMask", True)
    mel.set_material_instance_texture_parameter_value(mi, "StateMask", tex)
    ox, oy = mask["lake_origin"]
    mel.set_material_instance_vector_parameter_value(mi, "LakeOrigin", unreal.LinearColor(ox, oy, 0.0, 0.0))
    mel.set_material_instance_scalar_parameter_value(mi, "LakeSize", mask["lake_size"])
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log("ice MI parent %s, origin %s size %s, blend %s" % (parent_path, mask["lake_origin"], mask["lake_size"],
                                                          mi.get_blend_mode()))
    return mi


def _nav_box_actor(label, area_class, boxes, z_range):
    """One actor whose box components are nav-only dynamic obstacles (no collision, hidden in game): each box marks
    its volume with area_class on every navmesh (a meta area picks Null or Default per agent)."""
    actor = actors_sub().spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER + "IceNav")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    zc, ez = (z_range[0] + z_range[1]) / 2.0, (z_range[1] - z_range[0]) / 2.0
    made = 0
    for x, y, ex, ey in boxes:
        handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.BoxComponent, blueprint_context=None))
        comp = lib.get_object(lib.get_data(handle))
        if comp is None:
            raise RuntimeError("box component failed: %s" % fail)
        comp.set_box_extent(unreal.Vector(ex, ey, ez), False)
        comp.set_editor_property("relative_location", unreal.Vector(x, y, zc))
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("generate_overlap_events", False)
        comp.set_editor_property("hidden_in_game", True)
        comp.set_editor_property("can_ever_affect_navigation", True)
        comp.set_editor_property("dynamic_obstacle", True)
        comp.set_editor_property("use_system_default_obstacle_area_class", False)
        comp.set_editor_property("area_class_override", area_class)
        made += 1

    log("nav actor %s: %d boxes z %s" % (label, made, z_range))
    return actor


def step_ice_real():
    """Replace the placeholder with the real ice: SM_FL_IceLake (lake minus the leads, landscape collision, nav) with
    MI_FL_IceLake (state mask), water under the lead holes, and the per-type nav areas (fl_ice_gen.py data)."""
    require_map()
    data = _ice_data()
    landscape = by_label()[LANDSCAPE_LABEL]
    mi = _ice_lake_mi(data["state_mask"])
    mesh = _import(data["ice_obj"], ICE_MAP_DIR, "SM_FL_IceLake")
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_material(0, mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    water = _import(data["water_obj"], ICE_MAP_DIR, "SM_FL_IceLeadWater")
    lake_water = FL_ROOT + "Ice/MI_FrozenLakeWater"     # the ice session's water for holes, else the shore water
    water.set_material(0, unreal.load_asset(lake_water if unreal.EditorAssetLibrary.does_asset_exist(lake_water)
                                            else WATER_MI))
    unreal.EditorAssetLibrary.save_loaded_asset(water, False)
    box = mesh.get_bounding_box()
    log("ice mesh bounds x %.0f..%.0f y %.0f..%.0f, wanted %s" % (box.min.x, box.max.x, box.min.y, box.max.y,
                                                                  [round(v) for v in data["ice_bounds"]]))
    clear_prefix(PREFIX + "Ice")
    actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, ICE_Z))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_editor_property("body_instance", landscape.get_editor_property("body_instance"))
    comp.set_editor_property("can_ever_affect_navigation", True)
    actor.set_actor_label(PREFIX + "IceLake")
    actor.set_folder_path(FOLDER + "Ice")
    wact = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, LEAD_WATER_Z))
    wcomp = wact.static_mesh_component
    wcomp.set_static_mesh(water)
    wcomp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    wcomp.set_editor_property("can_ever_affect_navigation", False)
    wact.set_actor_label(PREFIX + "IceLeadWater")
    wact.set_folder_path(FOLDER + "Ice")
    # nav areas
    areas = {"Lead": unreal.NavArea_Null.static_class()}
    for key, name in ICE_NAV_AREA_BPS.items():
        areas[key] = _meta_area(name, data["nav_areas"][key]["agents_blocked"])
    for key, boxes in data["nav_boxes"].items():
        _nav_box_actor(PREFIX + "IceNav_" + key, areas[key], boxes, data["nav_areas"][key]["z"])
    return {"bounds": (box.min.x, box.max.x, box.min.y, box.max.y), "boxes": {k: len(v) for k, v in
                                                                             data["nav_boxes"].items()}}


def nav_data_by_agent():
    out = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.RecastNavMesh):
        out[a.get_actor_label() or a.get_name()] = a
    return out


def probe_paths(cases):
    """cases: [(label, (x, y, z), (x, y, z))]. Per agent navmesh: partial?, length (m), lowest z on the path."""
    w = world()
    rows = []
    for agent, nav in sorted(nav_data_by_agent().items()):
        for label, a, b in cases:
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                w, unreal.Vector(*a), unreal.Vector(*b), pathfinding_context=nav)
            pts = p.get_editor_property("path_points") if p else []
            if not pts:
                rows.append("%-11s %-28s none" % (agent, label))
                continue
            length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100.0
            end = pts[-1]
            miss = math.hypot(end.x - b[0], end.y - b[1]) / 100.0
            rows.append("%-11s %-28s partial %-5s %6.0f m  end miss %5.1f m  min z %6.0f" % (
                agent, label, p.is_partial(), length, miss, min(q.z for q in pts)))
    for r in rows:
        log(r)
    return rows


def step_water():
    """Open water planes in the pump 1 outflow gorge and the spillway canal (steam FX come later)."""
    require_map()
    clear_prefix(PREFIX + "Water")
    mi = unreal.load_asset(WATER_MI)
    count = 0
    for pid in ("outflow_gorge", "canal"):
        p = path(pid)
        pts = p["points"]
        for k, (a, b) in enumerate(zip(pts[:-1], pts[1:])):
            dx, dy = b[0] - a[0], b[1] - a[1]
            length = math.hypot(dx, dy)
            yaw = math.degrees(math.atan2(dy, dx))
            centre = ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0)
            _plane("%sWater_%s_%d" % (PREFIX, pid, k), FOLDER + "Water", centre, WATER_Z, length + p["width"],
                   p["width"] + 400.0, yaw, mi)
            count += 1
    log("water planes %d" % count)
    return count


NOTES = [
    ("hq", "START (landing shore)"), ("rig", "FORGE DRILL RIG (objective)"),
    ("shield_garrison", "AI START: Shield HQ"), ("pump_s", "CRYO PUMP 1"), ("pump_w", "CRYO PUMP 2"),
    ("pump_n", "CRYO PUMP 3 (in the dam)"), ("pump_e", "CRYO PUMP 4"), ("frozen_tiger", "frozen King Tiger"),
    ("spawn_n", "SHIELD N column"), ("spawn_e", "SHIELD E column"), ("spawn_s", "SHIELD S column"),
    ("soviet_airfield", "Soviet landing field"), ("airfield_control", "radio mast (open)"),
    ("blown_bridge", "blown bridge (repairable)"), ("canal_bridge", "canal bridge (placeholder fill)"),
    ("convoy_wreck_se", "burnt Forge convoy"), ("radixite_upwelling", "RICH RADIXITE upwelling"),
    ("metal_bay", "MAIN METAL wreck bay"), ("metal_yard", "metal: scrap yard (small)"),
    ("radixite_seep", "Radixite seep"), ("reveal", "reveal: vein under clear ice"),
    ("ice_lesson", "scripted heavy breaks through"), ("south_picket", "picket at the blown bridge"),
]


def step_notes():
    """Editor-only text labels at the sketch's features, so the map can be read before any kit mesh exists."""
    require_map()
    removed = clear_prefix(PREFIX + "Note_")
    for fid, words in NOTES:
        x, y = feature(fid)["points"][0]
        actor = actors_sub().spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, surface_z(x, y) + 320.0),
                                                    unreal.Rotator(0.0, 90.0, 180.0))
        comp = actor.text_render
        comp.set_text(words)
        comp.set_world_size(300.0)
        comp.set_text_render_color(unreal.Color(255, 230, 40, 255))
        comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        actor.set_actor_label(PREFIX + "Note_" + fid)
        actor.set_folder_path(FOLDER + "Notes")
        actor.set_editor_property("is_editor_only_actor", True)
    log("notes %d (removed %d)" % (len(NOTES), removed))
    return len(NOTES)


# ---- captures ---------------------------------------------------------------------------------------------------
def _capture(name, comp, rt, cap):
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", SHOT_SOURCE or unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("capture_every_frame", False)
    pp = comp.get_editor_property("post_process_settings")
    pp.set_editor_property("override_auto_exposure_bias", True)
    pp.set_editor_property("auto_exposure_bias", SHOT_EXPOSURE)
    for key, value in (("auto_exposure_min_brightness", SHOT_BRIGHTNESS), ("auto_exposure_max_brightness", SHOT_BRIGHTNESS)):
        pp.set_editor_property("override_" + key, True)
        pp.set_editor_property(key, value)
    comp.set_editor_property("post_process_settings", pp)
    comp.set_editor_property("post_process_blend_weight", 1.0)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + ".png")
    actors_sub().destroy_actor(cap)
    unreal.RenderingLibrary.release_render_target2d(rt)
    log("shot %s%s.png" % (SHOTS, name))


def shot_top(name, centre=(0.0, 0.0), width=None, size=2048):
    """Orthographic top-down capture, north up (east right)."""
    width = width or LAYOUT["landscape"]["size_x"]
    rt = unreal.RenderingLibrary.create_render_target2d(world(), size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = actors_sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(centre[0], centre[1], 30000.0),
                                              unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property("ortho_width", width)
    _capture(name, comp, rt, cap)


def shot_game(name, target, yaw=-21.0, pitch=-55.0, height=4000.0, fov=50.0, size=(1920, 1080)):
    """The RTS camera: about 40 m over the target, looking down at the pitch from the yaw's direction."""
    tx, ty = target
    tz = surface_z(tx, ty)
    back = height / math.tan(math.radians(-pitch))
    ex = tx - back * math.cos(math.radians(yaw))
    ey = ty - back * math.sin(math.radians(yaw))
    rt = unreal.RenderingLibrary.create_render_target2d(world(), size[0], size[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = actors_sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(ex, ey, tz + height),
                                              unreal.Rotator(0.0, pitch, yaw))
    comp = cap.capture_component2d
    comp.set_editor_property("fov_angle", fov)
    _capture(name, comp, rt, cap)
