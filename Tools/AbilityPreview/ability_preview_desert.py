"""Builds the ability preview desert in RTS_FactionSelection: a 441 m painted landscape 2 km from the hangar with dunes,
a far rocky ridge, two mesas and a small pond (weedy, darker shore), red and orange stone decals, dead trees, cacti,
rocks, a water plane, nav bounds and the AAbilityPreviewDirector the commander screen cuts to.

Re-runnable: every actor it makes is labelled AbilityPreview_ and replaced on the next run. Run it in a throwaway full
editor (commandlets crash saving landscape maps, see Docs/AbilityPreview/AbilityPreview.md):

  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/FactionSelection/RTS_FactionSelection
      -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/AbilityPreview/ability_preview_desert.py"
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

It rebuilds the navmesh, waits for it, saves the map and quits. ABILITY_PREVIEW_NO_SAVE=1 builds without saving.
"""
import math
import os
import random
import struct

import unreal

PREFIX = "AbilityPreview_"
FOLDER = "AbilityPreview/"
MAP_PATH = "/Game/RTS_Survival/FactionSelection/RTS_FactionSelection"
ASSET_DIR = "/Game/RTS_Survival/FactionSelection/AbilityPreview/"
SAVE_DIR = "D:/UE5Projects/RTS_Survival/Saved/AbilityPreview"
LOG_PATH = SAVE_DIR + "/ability_preview_desert.log"

# Must match FactionPlayerControllerAbilityPreview::DefaultDirectorLocation.
CENTRE = (200000.0, 0.0)
COMPONENTS = 7
QUADS = 63
VERTS = COMPONENTS * QUADS + 1
QUAD_CM = 100.0
HALF_SIZE = (VERTS - 1) * QUAD_CM / 2.0

LANDSCAPE_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/"
LANDSCAPE_MATERIAL = LANDSCAPE_ROOT + "MI_RTS_Landscape_DesertPaintable"
LAYERS = ["Sand", "DesertGravel", "DesertRockDark", "ScorchSandyStone", "Weedy"]

# The camera looks along +X from 55 m behind the centre: +Y is the right of the screen.
ARENA_RADIUS = 2800.0
POND = (1700.0, -3500.0, 780.0)  # u, v, radius of the water's edge
POND_DEPTH = 150.0
MESAS = [(4600.0, 6800.0, 2300.0, 950.0), (6200.0, -7600.0, 2600.0, 1150.0)]  # u, v, radius, height

MESHES = "/Game/RTS_Survival/Meshes/"
DEAD_TREES = [MESHES + "Nature/Trees/DeadTrees/SM_RTS_DeadTree_" + kind for kind in ("1", "2", "04", "05", "06")]
CACTI = [MESHES + "Desert/Cacti/SM_Desert_Cactus%d" % index for index in range(1, 5)]
CACTUS_BUSHES = [MESHES + "Desert/Cacti/CactusBush/SM_RTS_CactusBush_%s_%s_0%d" % (size, colour, index)
                 for size in ("Large", "Medium", "Small") for colour in ("Green", "Blue") for index in (1, 2, 3)]
ROCKS = [MESHES + "Desert/Rocks/SM_Rock_0%d" % index for index in range(1, 8)]
DEAD_SHRUBS = ["/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/S_Deadshrub-01_vdknafgha_Var%d_lod0" % index
               for index in (1, 2, 4, 5, 6, 8, 9, 10, 11, 12)]
WATER_MATERIAL = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_OasisWater"
PLANE_MESH = "/Engine/BasicShapes/Plane"

DECAL_PARENT_DIR = "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/"
# name, source instance (stone debris on M_Decal_Parallax), BaseColour_Tint
STONE_DECALS = [
    ("MI_AP_Decal_RedStones_01", "MI_RTS_Decal_Dirt_Debris_01", (1.0, 0.42, 0.30, 1.0)),
    ("MI_AP_Decal_RedStones_02", "MI_RTS_Decal_Dirt_Debris_02", (0.95, 0.38, 0.27, 1.0)),
    ("MI_AP_Decal_OrangeStones_01", "MI_RTS_Decal_Dirt_Debris_03", (1.0, 0.64, 0.33, 1.0)),
    ("MI_AP_Decal_OrangeStones_02", "MI_RTS_Decal_Dirt_Scatter_04", (1.0, 0.70, 0.40, 1.0)),
]
SANDY_DECALS = ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_04_SandyRock",
                "/Game/Megascans/Decals/Arid_Debris_vbzebfrh/MI_Arid_Debris_vbzebfrh_2K"]


def log(text):
    unreal.log("ABILITY_PREVIEW_DESERT " + text)
    with open(LOG_PATH, "a") as handle:
        handle.write(text + "\n")


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


class Terrain:
    """Heights and layer weights in centimetres, in the frame u = x - centre x, v = y - centre y."""

    def __init__(self, seed=71):
        self.seed = seed

    def _hash(self, ix, iy, salt):
        value = math.sin(ix * 127.1 + iy * 311.7 + salt * 74.7 + self.seed * 19.19) * 43758.5453
        return value - math.floor(value)

    def value_noise(self, x, y, salt):
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b = self._hash(ix, iy, salt), self._hash(ix + 1, iy, salt)
        c, d = self._hash(ix, iy + 1, salt), self._hash(ix + 1, iy + 1, salt)
        return lerp(lerp(a, b, sx), lerp(c, d, sx), sy)

    def noise(self, u, v, wavelength, salt):
        """-1..1"""
        return self.value_noise(u / wavelength, v / wavelength, salt) * 2.0 - 1.0

    def base_height(self, u, v):
        r = math.hypot(u, v)
        dunes = 70.0 * self.noise(u, v, 2600.0, 1) + 28.0 * self.noise(u, v, 950.0, 2) + 8.0 * self.noise(u, v, 300.0, 3)
        calm = lerp(0.18, 1.0, smoothstep(ARENA_RADIUS * 0.6, ARENA_RADIUS * 1.6, r))
        height = dunes * calm
        # Far rocky ridge across the back of the view.
        ridge = smoothstep(7500.0, 11500.0, u) * (1100.0 + 450.0 * self.noise(0.0, v, 4200.0, 4))
        ridge += smoothstep(9000.0, 12000.0, u) * 220.0 * self.noise(u, v, 700.0, 5)
        height += ridge
        for mu, mv, radius, top in MESAS:
            d = math.hypot(u - mu, v - mv) * (1.0 + 0.12 * self.noise(u, v, 900.0, 6))
            height += top * (1.0 - smoothstep(radius * 0.62, radius, d)) + 60.0 * self.noise(u, v, 350.0, 7) * (
                    1.0 - smoothstep(radius * 0.7, radius, d))
        # The landscape's edges rise, so a low camera never sees past them.
        height += smoothstep(15000.0, 21000.0, r) * 2600.0
        return height

    def pond_level(self):
        return self.base_height(POND[0], POND[1]) - 45.0

    def height(self, u, v):
        height = self.base_height(u, v)
        pu, pv, radius = POND
        d = math.hypot(u - pu, v - pv) * (1.0 + 0.08 * self.noise(u, v, 500.0, 8))
        if d < radius * 2.2:
            level = self.pond_level()
            # Flat shore around the water, a bowl under it.
            shore = lerp(level + 45.0, height, smoothstep(radius * 1.3, radius * 2.2, d))
            bowl = level - POND_DEPTH * (1.0 - smoothstep(radius * 0.25, radius, d))
            height = bowl if d < radius else lerp(level, shore, smoothstep(radius, radius * 1.3, d))
        return height

    def slope(self, u, v):
        step = 100.0
        dx = self.height(u + step, v) - self.height(u - step, v)
        dy = self.height(u, v + step) - self.height(u, v - step)
        return math.hypot(dx, dy) / (2.0 * step)

    def pond_distance(self, u, v):
        return math.hypot(u - POND[0], v - POND[1])

    def weights(self, u, v):
        sand = 1.0
        gravel = max(0.0, self.noise(u, v, 1800.0, 11) * 1.6 - 0.35) + 0.5 * max(0.0, self.noise(u, v, 500.0, 12) - 0.3)
        sandy_stone = max(0.0, self.noise(u, v, 2400.0, 13) * 1.5 - 0.55)
        rock = smoothstep(0.35, 0.75, self.slope(u, v)) * 2.5
        for mu, mv, radius, _ in MESAS:
            if math.hypot(u - mu, v - mv) < radius * 0.7:
                rock += 0.8 + 0.6 * self.noise(u, v, 300.0, 14)
        rock += smoothstep(9500.0, 12500.0, u) * 1.2
        weedy = 0.0
        d = self.pond_distance(u, v)
        radius = POND[2]
        # The shore reads green and damp from the camera: weeds on a ring, dark wet rock at the water's edge.
        weedy += 2.6 * (1.0 - smoothstep(radius * 1.2, radius * 2.6, d)) * (0.75 + 0.25 * self.noise(u, v, 260.0, 15))
        rock += 2.2 * (1.0 - smoothstep(radius * 0.7, radius * 1.15, d))
        sand *= 1.0 - 0.8 * smoothstep(radius * 1.2, radius * 2.2, -d + radius * 3.4)
        return [sand, gravel, rock, sandy_stone, weedy]


def build_landscape(world, terrain, source_body_instance):
    corner_x, corner_y = CENTRE[0] - HALF_SIZE, CENTRE[1] - HALF_SIZE
    heights = []
    layer_bytes = [bytearray() for _ in LAYERS]
    for row in range(VERTS):
        v = corner_y + row * QUAD_CM - CENTRE[1]
        for column in range(VERTS):
            u = corner_x + column * QUAD_CM - CENTRE[0]
            heights.append(max(0, min(65535, 32768 + round(terrain.height(u, v) * 128.0 / 100.0))))
            values = [max(0.0, value) for value in terrain.weights(u, v)]
            total = sum(values) or 1.0
            normalized = [int(value * 255.0 / total) for value in values]
            strongest = max(range(len(normalized)), key=lambda index: normalized[index])
            normalized[strongest] += 255 - sum(normalized)
            for buffer, value in zip(layer_bytes, normalized):
                buffer.append(value)
    height_path = os.path.join(SAVE_DIR, "desert_height.raw")
    with open(height_path, "wb") as handle:
        handle.write(struct.pack("<%dH" % len(heights), *heights))
    layer_paths = []
    for layer_name, buffer in zip(LAYERS, layer_bytes):
        path = os.path.join(SAVE_DIR, "desert_%s.raw" % layer_name)
        with open(path, "wb") as handle:
            handle.write(bytes(buffer))
        layer_paths.append(path)
    layer_infos = [unreal.load_asset(LANDSCAPE_ROOT + layer + "_LayerInfo") for layer in LAYERS]
    if any(info is None for info in layer_infos):
        raise RuntimeError("missing layer infos")
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        world, unreal.Vector(corner_x, corner_y, 0.0), unreal.Vector(QUAD_CM, QUAD_CM, 100.0), COMPONENTS, COMPONENTS,
        QUADS, unreal.load_asset(LANDSCAPE_MATERIAL), height_path, layer_infos, layer_paths, PREFIX + "Landscape")
    if landscape is None:
        raise RuntimeError("landscape was not created")
    landscape.set_actor_label(PREFIX + "Landscape")
    landscape.set_folder_path(FOLDER + "Ground")
    if source_body_instance is not None:
        landscape.set_editor_property("body_instance", source_body_instance)
    log("landscape at corner (%.0f, %.0f), pond level %.0f" % (corner_x, corner_y, terrain.pond_level()))
    return landscape


def make_stone_decal_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    made = []
    for name, source_name, tint in STONE_DECALS:
        path = ASSET_DIR + name
        source = unreal.load_asset(DECAL_PARENT_DIR + source_name)
        if source is None:
            log("MISSING decal source " + source_name)
            continue
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = tools.create_asset(name, ASSET_DIR.rstrip("/"), unreal.MaterialInstanceConstant, factory)
        # A child of the stone debris instance: same textures and parallax, tinted red or orange.
        instance.set_editor_property("parent", source)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance, "BaseColour_Tint", unreal.LinearColor(*tint))
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
        made.append(path)
    log("stone decal materials %s" % made)
    return made


class Dresser:
    def __init__(self, actors, terrain):
        self.actors = actors
        self.terrain = terrain
        self.count = 0

    def ground(self, u, v):
        return self.terrain.height(u, v)

    def _finish(self, actor, label, folder):
        self.count += 1
        actor.set_actor_label(PREFIX + label)
        actor.set_folder_path(FOLDER + folder)
        return actor

    def mesh(self, path, label, folder, u, v, yaw=0.0, scale=1.0, sink=15.0, collision=True):
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING mesh " + path)
            return None
        bounds = mesh.get_bounds()
        bottom = (bounds.origin.z - bounds.box_extent.z) * scale
        z = self.ground(u, v) - bottom - sink
        actor = self.actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(CENTRE[0] + u, CENTRE[1] + v, z),
                                                   unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_editor_property("can_ever_affect_navigation", False)
        if not collision:
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        return self._finish(actor, label, folder)

    def decal(self, material_path, label, u, v, size, yaw=0.0):
        material = unreal.load_asset(material_path)
        if material is None:
            log("MISSING decal " + material_path)
            return None
        z = self.ground(u, v)
        actor = self.actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(CENTRE[0] + u, CENTRE[1] + v, z),
                                                   unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw))
        decal = actor.get_component_by_class(unreal.DecalComponent)
        decal.set_decal_material(material)
        decal.set_editor_property("decal_size", unreal.Vector(300.0, size, size))
        return self._finish(actor, label, "Decals")


def scatter_points(rng, count, r_min, r_max, keep_out=(), forward_bias=0.0):
    """Points in a ring around the centre, skipping the pond and keep-out circles (u, v, radius)."""
    points = []
    attempts = 0
    while len(points) < count and attempts < count * 60:
        attempts += 1
        angle = rng.uniform(0.0, math.tau)
        radius = rng.uniform(r_min, r_max)
        u, v = math.cos(angle) * radius, math.sin(angle) * radius
        if forward_bias and u < -2000.0 and rng.random() < forward_bias:
            continue
        if math.hypot(u - POND[0], v - POND[1]) < POND[2] * 1.15:
            continue
        if any(math.hypot(u - ku, v - kv) < kr for ku, kv, kr in keep_out):
            continue
        if any(math.hypot(u - pu, v - pv) < 250.0 for pu, pv in points):
            continue
        points.append((u, v))
    return points


def dress(dresser, stone_decals):
    rng = random.Random(907)
    # Dead trees on the flanks and on the ridge; one leans over the pond.
    for index, (u, v) in enumerate(scatter_points(rng, 9, ARENA_RADIUS + 900.0, 11000.0, forward_bias=0.7)):
        dresser.mesh(DEAD_TREES[index % len(DEAD_TREES)], "DeadTree_%d" % index, "Trees", u, v,
                     yaw=rng.uniform(0, 360), scale=rng.uniform(0.55, 0.9), sink=25.0)
    dresser.mesh(DEAD_TREES[2], "DeadTree_Pond", "Trees", POND[0] + 900.0, POND[1] - 650.0, yaw=200.0, scale=0.6, sink=25.0)
    for index, (u, v) in enumerate(scatter_points(rng, 26, ARENA_RADIUS, 9000.0, forward_bias=0.6)):
        dresser.mesh(CACTI[index % len(CACTI)], "Cactus_%d" % index, "Cacti", u, v, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(1.6, 2.6), sink=8.0)
    for index, (u, v) in enumerate(scatter_points(rng, 34, ARENA_RADIUS - 400.0, 9500.0, forward_bias=0.5)):
        dresser.mesh(CACTUS_BUSHES[index % len(CACTUS_BUSHES)], "CactusBush_%d" % index, "Cacti", u, v,
                     yaw=rng.uniform(0, 360), scale=rng.uniform(0.8, 1.3), sink=5.0, collision=False)
    # Cactus bushes on the damp shore read greener than the desert ones.
    for index in range(7):
        angle = index / 7.0 * math.tau + rng.uniform(-0.3, 0.3)
        radius = POND[2] * rng.uniform(1.25, 1.9)
        dresser.mesh(CACTUS_BUSHES[(index * 4) % len(CACTUS_BUSHES)], "PondBush_%d" % index, "Pond",
                     POND[0] + math.cos(angle) * radius, POND[1] + math.sin(angle) * radius, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(0.9, 1.4), sink=5.0, collision=False)
    for index, (u, v) in enumerate(scatter_points(rng, 30, 1200.0, 9000.0)):
        dresser.mesh(DEAD_SHRUBS[index % len(DEAD_SHRUBS)], "Shrub_%d" % index, "Shrubs", u, v, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(0.8, 1.4), sink=4.0, collision=False)
    for index, (u, v) in enumerate(scatter_points(rng, 18, ARENA_RADIUS + 300.0, 10000.0)):
        dresser.mesh(ROCKS[index % len(ROCKS)], "Rock_%d" % index, "Rocks", u, v, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(1.0, 2.8), sink=30.0)
    for mu, mv, radius, _ in MESAS:
        for index in range(6):
            angle = rng.uniform(0, math.tau)
            d = radius * rng.uniform(0.85, 1.05)
            dresser.mesh(ROCKS[(index + 3) % len(ROCKS)], "MesaRock_%d_%d" % (int(mu), index), "Rocks",
                         mu + math.cos(angle) * d, mv + math.sin(angle) * d, yaw=rng.uniform(0, 360),
                         scale=rng.uniform(2.0, 3.6), sink=40.0)
    # Red and orange stone decals over the whole view, a few in the arena too; sandy rock patches between them.
    for index, (u, v) in enumerate(scatter_points(rng, 30, 400.0, 8500.0)):
        dresser.decal(stone_decals[index % len(stone_decals)], "StoneDecal_%d" % index, u, v,
                      rng.uniform(260.0, 620.0), yaw=rng.uniform(0, 360))
    for index, (u, v) in enumerate(scatter_points(rng, 8, 1500.0, 8000.0)):
        dresser.decal(SANDY_DECALS[index % len(SANDY_DECALS)], "SandyDecal_%d" % index, u, v, rng.uniform(500.0, 900.0),
                      yaw=rng.uniform(0, 360))


def add_water(actors, terrain):
    mesh = unreal.load_asset(PLANE_MESH)
    material = unreal.load_asset(WATER_MATERIAL)
    level = terrain.pond_level()
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                          unreal.Vector(CENTRE[0] + POND[0], CENTRE[1] + POND[1], level),
                                          unreal.Rotator(0.0, 0.0, 0.0))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    # The plane's corners sit under the raised shore, so the square never shows.
    size = POND[2] * 2.3 / 100.0
    actor.set_actor_scale3d(unreal.Vector(size, size, 1.0))
    actor.set_actor_label(PREFIX + "PondWater")
    actor.set_folder_path(FOLDER + "Pond")


def add_nav_bounds(actors, world):
    source = next((actor for actor in actors.get_all_level_actors()
                   if actor.get_class().get_name() == "NavMeshBoundsVolume" and not actor.get_actor_label().startswith(PREFIX)),
                  None)
    if source is None:
        raise RuntimeError("no NavMeshBoundsVolume to copy")
    volume = actors.duplicate_actor(source, world, unreal.Vector(0, 0, 0))
    volume.set_actor_label(PREFIX + "NavBounds")
    volume.set_folder_path(FOLDER + "Nav")
    volume.set_actor_location(unreal.Vector(CENTRE[0], CENTRE[1], 1500.0), False, False)
    # The source brush is a 100 cm half box scaled to 500 m: cover the playable 150 m around the centre.
    brush_half = source.get_actor_scale3d()
    origin, extent = source.get_actor_bounds(False)
    unit_half = extent.x / brush_half.x
    volume.set_actor_scale3d(unreal.Vector(15000.0 / unit_half, 15000.0 / unit_half, 3500.0 / unit_half))


def add_director(actors, terrain):
    actor = actors.spawn_actor_from_class(unreal.AbilityPreviewDirector,
                                          unreal.Vector(CENTRE[0], CENTRE[1], terrain.height(0.0, 0.0)),
                                          unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label(PREFIX + "Director")
    actor.set_folder_path(FOLDER)
    return actor


def disable_static_shadows(actors):
    """The hangar's sky light is stationary: a static primitive that casts static shadows without baked lighting counts
    as "lighting needs to be rebuilt" in development builds. The desert is lit dynamically (movable sun), so its
    primitives cast no static shadows instead of baking the whole hangar map again."""
    changed = 0
    for actor in actors.get_all_level_actors():
        if not actor.get_actor_label().startswith(PREFIX):
            continue
        if isinstance(actor, unreal.LandscapeProxy):
            # The proxy's setting is pushed to its components when they are recreated.
            actor.set_editor_property("cast_static_shadow", False)
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            if component.get_editor_property("cast_static_shadow"):
                component.set_editor_property("cast_static_shadow", False)
                changed += 1
    log("static shadows off on %d primitives" % changed)


def remove_previous(actors):
    removed = 0
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors.destroy_actor(actor)
            removed += 1
    log("removed %d previous actors" % removed)


def build():
    os.makedirs(SAVE_DIR, exist_ok=True)
    open(LOG_PATH, "w").close()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(MAP_PATH):
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    remove_previous(actors)
    source = next((actor for actor in actors.get_all_level_actors() if actor.get_class().get_name() == "Landscape"
                   and not actor.get_actor_label().startswith(PREFIX)), None)
    body_instance = source.get_editor_property("body_instance") if source else None
    terrain = Terrain()
    build_landscape(world, terrain, body_instance)
    stone_decals = make_stone_decal_materials()
    dresser = Dresser(actors, terrain)
    dress(dresser, stone_decals)
    add_water(actors, terrain)
    add_nav_bounds(actors, world)
    add_director(actors, terrain)
    disable_static_shadows(actors)
    log("dressing actors=%d" % dresser.count)
    return world


_state = {"handle": None, "ticks": 0, "world": None, "saved": False}


def _tick(_delta):
    _state["ticks"] += 1
    world = _state["world"]
    if _state["ticks"] < 60:
        return
    building = unreal.NavigationSystemV1.is_navigation_being_built(world)
    if building and _state["ticks"] < 6000:
        return
    if not _state["saved"]:
        _state["saved"] = True
        ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
        log("navmesh built=%s after %d ticks, save_map=%s" % (not building, _state["ticks"], ok))
        unreal.unregister_slate_post_tick_callback(_state["handle"])
        unreal.SystemLibrary.quit_editor()


def patch_static_shadows():
    """ABILITY_PREVIEW_PATCH_SHADOWS=1: only turn static shadows off on an existing desert, then save."""
    os.makedirs(SAVE_DIR, exist_ok=True)
    open(LOG_PATH, "w").close()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(MAP_PATH):
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    disable_static_shadows(unreal.get_editor_subsystem(unreal.EditorActorSubsystem))
    return world


def main():
    try:
        world = patch_static_shadows() if os.environ.get("ABILITY_PREVIEW_PATCH_SHADOWS") == "1" else build()
    except Exception as error:  # noqa: BLE001 - logged for the headless run, then the editor quits
        log("FAILED %r" % (error,))
        unreal.SystemLibrary.quit_editor()
        raise
    if os.environ.get("ABILITY_PREVIEW_NO_SAVE") == "1":
        log("built without saving")
        return
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    _state["world"] = world
    _state["handle"] = unreal.register_slate_post_tick_callback(_tick)


main()
