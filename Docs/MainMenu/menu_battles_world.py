"""World building for the main menu battles: sky, three extra themed landscapes around the StartGame desert, and the
dressing of every battle region. Used by menu_battles_setup.py; every actor it makes carries the MenuWorld_ label
prefix so a rerun replaces it.

Regions (battle axis along the arena arrow, Germans behind the arena origin):
  Desert  - the original StartGame landscape (DesertPaintable), wrecks and dead trees around its four arenas.
  Steppe  - dry grassland (DryCastle, the Ger_Trains look) with a farmstead, an orchard, tractors and old cars.
  Meadow  - green grass (Brushify) ringed by spruce and fir forest, leaf-tree copses as cover.
  Winter  - snow (Brushify) with dark spruces on the ridges, dead trees, tank wrecks and boulders.
"""
import math
import os
import random
import struct

import unreal

PREFIX = "MenuWorld_"
SAVE_DIR = "D:/UE5Projects/RTS_Survival/Saved/MenuBattles"
LANDSCAPE_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/"
BRUSHIFY = "/Game/Brushify/Materials/Landscape/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
MESHES = "/Game/RTS_Survival/Meshes/"

DESERT_TRAIL_MATERIAL = LANDSCAPE_ROOT + "DesertPaintable/MI_RTS_Landscape_DesertPaintable"

# Industrial yards: a grid of concrete ground tiles (BP_IndustrialZone_Ground02 scaled to the tile, no overlap), a
# back row of hangars and props in front; on the generated landscapes the ground is flattened under the yard.
YARD_TILE_CM = 2500.0
YARD_COLUMNS = 4
YARD_ROWS = 3
YARD_HEIGHT_CM = 420.0
YARD_FLATTEN_BLEND_CM = 1800.0

COMPONENTS = 7
QUADS = 63
VERTS = COMPONENTS * QUADS + 1
QUAD_CM = 100.0
HALF_SIZE = (VERTS - 1) * QUAD_CM / 2.0

# name, centre, battle yaw, material, layer infos, rim height (cm), terrain seed
REGIONS = {
    "Steppe": dict(centre=(0.0, 80000.0), yaw=0.0, rim=1600.0, bumps=110.0, seed=11,
                   material=LANDSCAPE_ROOT + "CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Trains",
                   layers=["GrassCastle", "DryGrass", "Desert", "GraySand", "RockDesert"],
                   layer_root=LANDSCAPE_ROOT + "CastleGrassDesertDry/DATA/"),
    "Meadow": dict(centre=(80000.0, 0.0), yaw=90.0, rim=2600.0, bumps=140.0, seed=23, yards=[(-1000.0, -10200.0)],
                   material=BRUSHIFY + "MI_Landscape",
                   layers=["Grass", "GrassDry", "Forest", "ForestDry", "Desert"],
                   layer_root=BRUSHIFY + "LayerInfos/"),
    "Farm": dict(centre=(-80000.0, 0.0), yaw=0.0, rim=1400.0, bumps=55.0, seed=51,
                 material=LANDSCAPE_ROOT + "CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Inst",
                 layers=["GrassCastle", "DryGrass", "Desert", "GraySand", "RockDesert"],
                 layer_root=LANDSCAPE_ROOT + "CastleGrassDesertDry/DATA/"),
    "Winter": dict(centre=(0.0, -80000.0), yaw=0.0, rim=3600.0, bumps=170.0, seed=37, yards=[(1500.0, 10200.0)],
                   material=BRUSHIFY + "MI_Landscape",
                   layers=["Snow", "GrassDry", "ForestDry", "Desert"],
                   layer_root=BRUSHIFY + "LayerInfos/"),
}


def log(text):
    unreal.log_warning("MENU_BATTLES_WORLD " + str(text))


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


class Terrain:
    """Height (cm above the landscape's base) and paint masks of one region in its battle frame."""

    def __init__(self, name):
        spec = REGIONS[name]
        self.name = name
        self.centre = spec["centre"]
        self.yaw = math.radians(spec["yaw"])
        self.rim = spec["rim"]
        self.bumps = spec["bumps"]
        rng = random.Random(spec["seed"])
        self.phases = [rng.uniform(0, math.tau) for _ in range(12)]
        self.yards = spec.get("yards", [])

    def local(self, x, y):
        """World XY to (along battle axis, sideways) in cm."""
        dx, dy = x - self.centre[0], y - self.centre[1]
        cos_yaw, sin_yaw = math.cos(self.yaw), math.sin(self.yaw)
        return dx * cos_yaw + dy * sin_yaw, -dx * sin_yaw + dy * cos_yaw

    def value_noise(self, x, y, salt):
        """Smooth value noise in [-1, 1]: random values on an integer grid, smoothstep-interpolated."""
        x0, y0 = math.floor(x), math.floor(y)
        fx, fy = x - x0, y - y0
        sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

        def corner(cx, cy):
            h = (cx * 73856093) ^ (cy * 19349663) ^ (salt * 83492791)
            h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
            return (h & 0xFFFF) / 32767.5 - 1.0

        top = corner(x0, y0) + (corner(x0 + 1, y0) - corner(x0, y0)) * sx
        bottom = corner(x0, y0 + 1) + (corner(x0 + 1, y0 + 1) - corner(x0, y0 + 1)) * sx
        return top + (bottom - top) * sy

    def noise(self, u, v, wavelength, index):
        # Two octaves of blotchy value noise; sines gave the paint layers straight diagonal stripes.
        salt = index * 7919 + int(self.phases[index] * 1000)
        return (self.value_noise(u / wavelength, v / wavelength, salt)
                + 0.5 * self.value_noise(u / (wavelength * 0.45), v / (wavelength * 0.45), salt + 1))

    def rim_amount(self, u, v):
        # Elliptical bowl: the battle field (up to ~110 m along the axis, ~60 m sideways) stays open.
        radius = math.sqrt((u / 19000.0) ** 2 + (v / 15000.0) ** 2)
        return smoothstep(0.78, 1.12, radius)

    def height(self, x, y):
        u, v = self.local(x, y)
        rim = self.rim_amount(u, v)
        ridges = 0.65 + 0.35 * self.noise(u, v, 2600.0, 0)
        field = self.bumps * (self.noise(u, v, 1800.0, 3) + 0.4 * self.noise(u, v, 700.0, 6))
        raw = 400.0 + field + rim * self.rim * ridges
        return self.flatten_for_yards(u, v, raw)

    def flatten_for_yards(self, u, v, raw):
        half_u = YARD_COLUMNS * YARD_TILE_CM / 2.0 + 400.0
        half_v = YARD_ROWS * YARD_TILE_CM / 2.0 + 400.0
        for yard_u, yard_v in self.yards:
            outside_u = max(0.0, abs(u - yard_u) - half_u)
            outside_v = max(0.0, abs(v - yard_v) - half_v)
            flat = 1.0 - smoothstep(0.0, YARD_FLATTEN_BLEND_CM, math.hypot(outside_u, outside_v))
            raw = raw * (1.0 - flat) + YARD_HEIGHT_CM * flat
        return raw

    def weights(self, x, y):
        u, v = self.local(x, y)
        rim = self.rim_amount(u, v)
        patch = 0.5 + 0.5 * self.noise(u, v, 1400.0, 1)
        patch2 = 0.5 + 0.5 * self.noise(u, v, 900.0, 5)
        lane = math.exp(-(v / 4500.0) ** 2) * (1.0 - smoothstep(9000.0, 13000.0, abs(u)))
        if self.name == "Steppe":
            # GrassCastle, DryGrass, Desert, GraySand, RockDesert
            return [0.55 * (1 - rim), 0.25 + 0.35 * patch, 0.12 * patch2, 0.45 * lane * patch2, 0.9 * rim]
        if self.name == "Farm":
            # Lush pasture; worn tracks along the battle lane.
            return [0.8 * (1 - rim), 0.12 * patch, 0.04 * patch2, 0.35 * lane * patch2, 0.7 * rim]
        if self.name == "Meadow":
            # Grass, GrassDry, Forest, ForestDry, Desert
            return [0.7 * (1 - rim), 0.25 * patch, 0.9 * rim, 0.25 * rim * patch2, 0.22 * lane * patch2]
        # Winter: Snow, GrassDry, ForestDry, Desert (frozen mud along the battle lane)
        return [0.8, 0.12 * patch * (1 - rim), 0.4 * rim * patch2, 0.35 * lane * patch2]


def ensure_save_dir():
    os.makedirs(SAVE_DIR, exist_ok=True)


def build_landscape(world, actors, name, source_body_instance):
    terrain = Terrain(name)
    spec = REGIONS[name]
    corner_x = spec["centre"][0] - HALF_SIZE
    corner_y = spec["centre"][1] - HALF_SIZE
    heights = []
    layer_bytes = [bytearray() for _ in spec["layers"]]
    for row in range(VERTS):
        y = corner_y + row * QUAD_CM
        for column in range(VERTS):
            x = corner_x + column * QUAD_CM
            heights.append(max(0, min(65535, 32768 + round(terrain.height(x, y) * 128.0 / 100.0))))
            values = [max(0.0, value) for value in terrain.weights(x, y)]
            total = sum(values) or 1.0
            normalized = [int(value * 255.0 / total) for value in values]
            # Weights sum to 255 per vertex: the rounding remainder goes to the strongest layer.
            strongest = max(range(len(normalized)), key=lambda index: normalized[index])
            normalized[strongest] += 255 - sum(normalized)
            for buffer, value in zip(layer_bytes, normalized):
                buffer.append(value)
    ensure_save_dir()
    height_path = os.path.join(SAVE_DIR, name + "_height.raw")
    with open(height_path, "wb") as handle:
        handle.write(struct.pack("<%dH" % len(heights), *heights))
    layer_paths = []
    for layer_name, buffer in zip(spec["layers"], layer_bytes):
        path = os.path.join(SAVE_DIR, "%s_%s.raw" % (name, layer_name))
        with open(path, "wb") as handle:
            handle.write(bytes(buffer))
        layer_paths.append(path)
    layer_infos = [unreal.load_asset(spec["layer_root"] + layer + "_LayerInfo") for layer in spec["layers"]]
    missing = [layer for layer, info in zip(spec["layers"], layer_infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s for %s" % (missing, name))
    material = unreal.load_asset(spec["material"])
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        world, unreal.Vector(corner_x, corner_y, 0.0), unreal.Vector(QUAD_CM, QUAD_CM, 100.0), COMPONENTS, COMPONENTS,
        QUADS, material, height_path, layer_infos, layer_paths, PREFIX + "Landscape_" + name)
    if landscape is None:
        raise RuntimeError("landscape %s was not created" % name)
    landscape.set_actor_label(PREFIX + "Landscape_" + name)
    # Units only drive on ground that blocks the Landscape trace channel: copy the desert landscape's custom profile.
    if source_body_instance is not None:
        landscape.set_editor_property("body_instance", source_body_instance)
    log("landscape %s created at corner (%.0f, %.0f)" % (name, corner_x, corner_y))
    return terrain


def ground_height(world, terrains, x, y):
    for terrain in terrains.values():
        u, v = terrain.local(x, y)
        if abs(x - terrain.centre[0]) <= HALF_SIZE and abs(y - terrain.centre[1]) <= HALF_SIZE:
            return terrain.height(x, y)
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    # None past every landscape's edge: dressing there would float over the bare sky atmosphere ground.
    return hit.to_tuple()[4].z if hit is not None else None


class Dresser:
    def __init__(self, world, actors, terrains):
        self.world = world
        self.actors = actors
        self.terrains = terrains
        self.count = 0

    def _finish(self, actor, label, folder):
        self.count += 1
        actor.set_actor_label(PREFIX + label)
        actor.set_folder_path("MenuBattles/" + folder)
        return actor

    def blueprint(self, path, label, folder, x, y, yaw=0.0, scale=1.0, sink=0.0):
        actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if actor_class is None:
            log("MISSING blueprint " + path)
            return None
        ground = ground_height(self.world, self.terrains, x, y)
        if ground is None:
            return None
        z = ground - sink
        actor = self.actors.spawn_actor_from_class(actor_class, unreal.Vector(x, y, z),
                                                   unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        if actor is None:
            return None
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        return self._finish(actor, label, folder)

    def mesh(self, path, label, folder, x, y, yaw=0.0, scale=1.0, sink=20.0):
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING mesh " + path)
            return None
        bounds = mesh.get_bounds()
        bottom = (bounds.origin.z - bounds.box_extent.z) * scale
        ground = ground_height(self.world, self.terrains, x, y)
        if ground is None:
            return None
        z = ground - bottom - sink
        actor = self.actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                                   unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        return self._finish(actor, label, folder)

    def decal(self, material_path, label, folder, x, y, size, yaw=0.0):
        material = unreal.load_asset(material_path)
        if material is None:
            log("MISSING decal " + material_path)
            return None
        z = ground_height(self.world, self.terrains, x, y)
        if z is None:
            return None
        actor = self.actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z),
                                                   unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw))
        decal = actor.get_component_by_class(unreal.DecalComponent)
        decal.set_decal_material(material)
        decal.set_editor_property("decal_size", unreal.Vector(400.0, size, size))
        return self._finish(actor, label, folder)


def world_point(terrain, u, v):
    cos_yaw, sin_yaw = math.cos(terrain.yaw), math.sin(terrain.yaw)
    return terrain.centre[0] + u * cos_yaw - v * sin_yaw, terrain.centre[1] + u * sin_yaw + v * cos_yaw


WPO = ENV + "WpoTreeFoliage/"
SPRUCES = [WPO + "Spruces/BP_WpoSpruce_1", WPO + "Spruces/BP_WpoSpruce_Tall", WPO + "Spruces/BP_WpoSpruce_Small",
           WPO + "Firs/BP_WpoFir", WPO + "Firs/BP_WpoFir_Small"]
DARK_SPRUCES = [WPO + "Spruces/BP_WpoSpruce_TallDark", WPO + "Spruces/BP_WpoSpruce_SmallDark", WPO + "Spruces/BP_WpoSpruce_1"]
LEAF_TREES = [WPO + "LeafTrees/BP_WpoLeafTree_Large", WPO + "SmallTrees/BP_WpoLeafTreeSmall", WPO + "LeafTrees/BP_WpoBush_1",
              WPO + "LeafTrees/BP_WpoBush_2"]
ORCHARD = [ENV + "OrchardTrees/BP_OrchardTree_" + kind for kind in
           ("Apple_A", "Apple_B", "Pear_A", "Pear_B", "Cherry_A", "Cherry_B")]
DEAD_TREES = [MESHES + "Nature/Trees/DeadTrees/SM_RTS_DeadTree_" + kind for kind in ("1", "2", "04", "05", "06")]
STONES = ["/Game/RTS_Survival/Blueprints/Environment/GroundStones/Meshes/SM_GS_" + kind for kind in
          ("Chunk_A", "Chunk_B", "Chunk_C", "Chunk_D", "Cluster_A")]
CARS = [ENV + "Props/OldCars/Cover/BP_Cover_OldCar_" + kind for kind in
        ("Sedan", "Pickup", "PanelVan", "Coupe", "Limousine", "Compact")]
TRACTORS = [ENV + "DestructableEnvActor/NaturalCover/Tractors/BP_Cover_Tractor_" + kind for kind in ("S65", "STZ1")]
FARM = ENV + "DestructableEnvActor/Farm/BP_Farm_Dest_"
CRATER_DECALS = ["/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Crater_Scorched_Scatter_01",
                 "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Crater_Scorched_Scatter_02",
                 "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Scorch_01"]


def wreck_meshes():
    root = "D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Blueprints/Environment/ScavengeObjects/DestroyedTanks"
    paths = []
    for directory, _, files in os.walk(root):
        for file in files:
            if file.startswith("SM_Destroyed") and file.endswith(".uasset"):
                relative = os.path.relpath(os.path.join(directory, file[:-7]), "D:/UE5Projects/RTS_Survival/Content")
                paths.append("/Game/" + relative.replace("\\", "/"))
    return sorted(paths)


def ring(rng, count, radius_min, radius_max, spread_u=19000.0, spread_v=15000.0):
    """Points on the bowl's rim, as (u, v) in the battle frame."""
    points = []
    for index in range(count):
        angle = (index + rng.uniform(-0.35, 0.35)) / count * math.tau
        radius = rng.uniform(radius_min, radius_max)
        points.append((math.cos(angle) * spread_u * radius, math.sin(angle) * spread_v * radius))
    return points


def flank_point(rng, side, along_max=11000.0, lateral_min=7500.0, lateral_max=11000.0):
    return rng.uniform(-along_max, along_max), side * rng.uniform(lateral_min, lateral_max)


def dress_steppe(dresser, terrain):
    rng = random.Random(101)
    folder = "Steppe"
    # Farmstead on the north flank, facing the battle.
    for kind, u, v, yaw in [("FarmHouse", -3500, 11500, 180), ("Barn", 3000, 12500, 175), ("HayShed", 7500, 10500, 160),
                            ("Stable", -8500, 12000, 195)]:
        x, y = world_point(terrain, u, v)
        dresser.blueprint(FARM + kind, "Steppe_" + kind, folder, x, y, yaw=math.degrees(terrain.yaw) + yaw)
    # Orchard rows on the south flank.
    for row in range(3):
        for column in range(10):
            u = -9000 + column * 1900 + rng.uniform(-200, 200)
            v = -9200 - row * 1500 + rng.uniform(-150, 150)
            x, y = world_point(terrain, u, v)
            dresser.blueprint(ORCHARD[(row * 10 + column) % len(ORCHARD)], "Steppe_Orchard_%02d" % (row * 10 + column),
                              folder, x, y, yaw=rng.uniform(0, 360))
    # Abandoned machinery as cover between the lines.
    for index in range(6):
        u, v = rng.uniform(-3500, 3500), rng.uniform(-6000, 6000)
        x, y = world_point(terrain, u, v)
        path = TRACTORS[index % 2] if index < 2 else CARS[index % len(CARS)]
        dresser.blueprint(path, "Steppe_Cover_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360))
    for index, (u, v) in enumerate(ring(rng, 26, 0.9, 1.08)):
        x, y = world_point(terrain, u, v)
        dresser.blueprint(LEAF_TREES[index % 2], "Steppe_RimTree_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360),
                          scale=rng.uniform(0.9, 1.25))
    for index in range(10):
        u, v = rng.uniform(-14000, 14000), rng.uniform(-6500, 6500)
        x, y = world_point(terrain, u, v)
        dresser.mesh(STONES[index % len(STONES)], "Steppe_Stone_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(0.8, 1.4))


def dress_meadow(dresser, terrain):
    rng = random.Random(202)
    folder = "Meadow"
    for index, (u, v) in enumerate(ring(rng, 70, 0.86, 1.12)):
        x, y = world_point(terrain, u, v)
        dresser.blueprint(SPRUCES[index % len(SPRUCES)], "Meadow_Forest_%02d" % index, folder, x, y,
                          yaw=rng.uniform(0, 360), scale=rng.uniform(0.9, 1.35))
    # Copses on both flanks: cover for the infantry and a frame for the camera.
    for side in (-1, 1):
        for copse in range(2):
            centre_u, centre_v = rng.uniform(-9000, 9000), side * rng.uniform(8000, 10500)
            for index in range(6):
                u, v = centre_u + rng.uniform(-1300, 1300), centre_v + rng.uniform(-900, 900)
                x, y = world_point(terrain, u, v)
                dresser.blueprint(LEAF_TREES[index % len(LEAF_TREES)], "Meadow_Copse_%d_%d_%d" % (side + 1, copse, index),
                                  folder, x, y, yaw=rng.uniform(0, 360), scale=rng.uniform(0.85, 1.2))
    for index in range(14):
        u, v = rng.uniform(-15000, 15000), rng.uniform(-8000, 8000)
        x, y = world_point(terrain, u, v)
        dresser.mesh(STONES[index % len(STONES)], "Meadow_Stone_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(0.7, 1.5))


def dress_winter(dresser, terrain, wrecks):
    rng = random.Random(303)
    folder = "Winter"
    for index, (u, v) in enumerate(ring(rng, 56, 0.84, 1.1)):
        x, y = world_point(terrain, u, v)
        dresser.blueprint(DARK_SPRUCES[index % len(DARK_SPRUCES)], "Winter_Forest_%02d" % index, folder, x, y,
                          yaw=rng.uniform(0, 360), scale=rng.uniform(1.0, 1.45))
    for index in range(12):
        u, v = flank_point(rng, -1 if index % 2 else 1, lateral_min=6500, lateral_max=11500)
        x, y = world_point(terrain, u, v)
        dresser.mesh(DEAD_TREES[index % len(DEAD_TREES)], "Winter_DeadTree_%02d" % index, folder, x, y,
                     yaw=rng.uniform(0, 360), scale=rng.uniform(0.8, 1.2))
    for index in range(7):
        u, v = rng.uniform(-9000, 9000), rng.uniform(-7000, 7000)
        x, y = world_point(terrain, u, v)
        dresser.mesh(wrecks[(index * 5) % len(wrecks)], "Winter_Wreck_%02d" % index, folder, x, y,
                     yaw=rng.uniform(0, 360), sink=35.0)
    for index in range(16):
        u, v = rng.uniform(-16000, 16000), rng.uniform(-9000, 9000)
        x, y = world_point(terrain, u, v)
        dresser.mesh(STONES[index % len(STONES)], "Winter_Stone_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360),
                     scale=rng.uniform(0.9, 1.8))


def dress_desert_arenas(dresser, arenas, wrecks):
    """Old fighting around each desert arena: wrecks and dead trees on the flanks, scorch marks in no man's land."""
    rng = random.Random(404)
    for label, (x0, y0), yaw, _, region in arenas:
        if region != "Desert":
            continue
        frame = Terrain("Steppe")
        frame.centre, frame.yaw = (x0, y0), math.radians(yaw)
        short = label.replace("MenuArena_", "")
        for index in range(3):
            u, v = flank_point(rng, 1 if index % 2 else -1, along_max=7000, lateral_min=6500, lateral_max=9000)
            x, y = world_point(frame, u, v)
            dresser.mesh(wrecks[(index * 7 + len(short)) % len(wrecks)], "Desert_%s_Wreck_%d" % (short, index), "Desert",
                         x, y, yaw=rng.uniform(0, 360), sink=35.0)
        for index in range(3):
            u, v = flank_point(rng, -1 if index % 2 else 1, along_max=9000, lateral_min=7000, lateral_max=10000)
            x, y = world_point(frame, u, v)
            dresser.mesh(DEAD_TREES[index % len(DEAD_TREES)], "Desert_%s_DeadTree_%d" % (short, index), "Desert", x, y,
                         yaw=rng.uniform(0, 360))
        for index in range(4):
            u, v = rng.uniform(-3500, 3500), rng.uniform(-4500, 4500)
            x, y = world_point(frame, u, v)
            dresser.decal(CRATER_DECALS[index % len(CRATER_DECALS)], "Desert_%s_Crater_%d" % (short, index), "Desert",
                          x, y, rng.uniform(500, 900), yaw=rng.uniform(0, 360))


# The start camera's view: overlapping ground patches, an asphalt yard, tiles and road strips from an old layout.
START_VIEW_CLEAR_CLASSES = ("BP_Scorch_EmptyGround1_C", "BP_Scorch_EmptyGround2_C", "BP_RoadSpline_C")
START_VIEW_CLEAR_MESHES = ("SM_Scorch_EmptyGround1", "SM_Ground_1", "SM_Carpark_Asphalt", "SM_TileBig", "SM_Road_Hangar_01")
CACTI = [MESHES + "Desert/Cacti/SM_Desert_Cactus%d" % index for index in range(1, 5)]
CACTUS_BUSHES = [MESHES + "Desert/Cacti/CactusBush/SM_RTS_CactusBush_%s_%s_0%d" % (size, colour, index)
                 for size in ("Large", "Medium", "Small") for colour in ("Green", "Blue") for index in (1, 2, 3)]
DESERT_ROCKS = [MESHES + "Desert/Rocks/SM_Rock_0%d" % index for index in range(1, 8)]
DEAD_SHRUBS = ["/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/S_Deadshrub-01_vdknafgha_Var%d_lod0" % index
               for index in (1, 2, 4, 5, 6, 8, 9, 10, 11, 12)]


def clear_start_view(actors):
    """Removes the old ground patches and road strips; the desert landscape underneath shows again."""
    removed = 0
    for actor in actors.get_all_level_actors():
        name = actor.get_class().get_name()
        mesh_name = ""
        if name == "StaticMeshActor":
            mesh = actor.static_mesh_component.static_mesh
            mesh_name = mesh.get_name() if mesh else ""
        if name in START_VIEW_CLEAR_CLASSES or mesh_name in START_VIEW_CLEAR_MESHES:
            actors.destroy_actor(actor)
            removed += 1
    log("start view cleared: %d ground patches and road pieces" % removed)


def dress_start_view(dresser, wrecks, centre, yaw):
    """Desert around the start-view arena: cacti, cactus bushes, dead shrubs and rocks on the flanks, wrecks and craters
    in the field, kept sparse where the lines meet."""
    rng = random.Random(505)
    frame = Terrain("Steppe")
    frame.centre, frame.yaw = centre, math.radians(yaw)
    folder = "StartView"

    def place(kind_paths, label, count, lateral_min, lateral_max, along=12000.0, scale=(0.8, 1.3), mesh_sink=20.0):
        for index in range(count):
            u = rng.uniform(-along, along)
            v = rng.choice((-1, 1)) * rng.uniform(lateral_min, lateral_max)
            x, y = world_point(frame, u, v)
            dresser.mesh(kind_paths[index % len(kind_paths)], "StartView_%s_%02d" % (label, index), folder, x, y,
                         yaw=rng.uniform(0, 360), scale=rng.uniform(*scale), sink=mesh_sink)

    place(CACTI, "Cactus", 34, 5500, 13000, scale=(0.8, 1.4))
    place(CACTUS_BUSHES, "CactusBush", 70, 3000, 13500, scale=(0.7, 1.3))
    place(DEAD_SHRUBS, "DeadShrub", 50, 1500, 13500, scale=(0.8, 1.4), mesh_sink=5.0)
    place(DESERT_ROCKS, "Rock", 18, 7000, 13500, scale=(0.6, 1.4), mesh_sink=40.0)
    place(DEAD_TREES, "DeadTree", 9, 7000, 12500, scale=(0.8, 1.15))
    for index in range(7):
        u, v = rng.uniform(-9000, 9000), rng.choice((-1, 1)) * rng.uniform(3500, 8000)
        x, y = world_point(frame, u, v)
        dresser.mesh(wrecks[(index * 3 + 1) % len(wrecks)], "StartView_Wreck_%02d" % index, folder, x, y,
                     yaw=rng.uniform(0, 360), sink=35.0)
    for index in range(14):
        u, v = rng.uniform(-6000, 6000), rng.uniform(-5000, 5000)
        x, y = world_point(frame, u, v)
        dresser.decal(CRATER_DECALS[index % len(CRATER_DECALS)], "StartView_Crater_%02d" % index, folder, x, y,
                      rng.uniform(500, 1100), yaw=rng.uniform(0, 360))


YARD_GROUND = ENV + "GroundEnvActors/Airport/BP_IndustrialZone_Ground02"
HANGARS = [ENV + "DestructableEnvActor/PostApo/BP_Dest_PostApo_Hangar1",
           ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_Hangar",
           ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_Hangar"]
YARD_PROPS = [ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_Container",
              ENV + "DestructableEnvActor/PostApo/BP_Dest_PostApo_ContainerBox1",
              ENV + "DestructableEnvActor/PostApo/BP_Dest_PostApo_ContainerBox2",
              ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_OilTank1",
              ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_OilTankBarrels1",
              ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_OilTankBarrels2",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_BarrelStack2",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_BarrelStack3",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack01",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack03"]
SCORCH_TREES = [ENV + "DestructableEnvActor/BP_Scorch_Dest_DeadTree", ENV + "DestructableEnvActor/BP_Scorch_Dest_DeadWillow"]
SCORCH_POWERLINE = ENV + "DestructableEnvActor/BP_Scorch_Dest_Powerline_1"
LOGS = [MESHES + "Nature/Trees/DeadTrees/Logs/SM_RTS_DeadTree_Log_0%d" % index for index in range(1, 10)]
MEADOW_SHRUBS = (["/Game/Megascans/3D_Plants/Elderberry_wfzobb2ia/S_Elderberry_wfzobb2ia_Var%d_lod3" % index
                  for index in (1, 2, 3, 10, 11)] +
                 ["/Game/Megascans/3D_Plants/Dwarf_Birch_xblnaabqx/S_Dwarf_Birch_xblnaabqx_Var%d_lod0" % index
                  for index in (10, 11, 12, 13)])


def scale_ground_tile(actor, size):
    """Scales a ground blueprint so its mesh covers exactly size x size: neighbouring tiles meet without overlap."""
    components = actor.get_components_by_class(unreal.StaticMeshComponent)
    meshes = [component for component in components if component.get_editor_property("static_mesh")]
    if not meshes:
        return
    bounds = meshes[0].get_editor_property("static_mesh").get_bounds()
    actor.set_actor_scale3d(unreal.Vector(size / (2.0 * bounds.box_extent.x), size / (2.0 * bounds.box_extent.y), 1.0))


def build_industrial_yard(dresser, frame, yard_u, yard_v, label, folder, seed):
    """A concrete yard on a battle's flank: ground tile grid, hangars on the row away from the battle, containers, oil
    tanks and stacks in front, scorch powerline poles and scorched trees around it."""
    rng = random.Random(seed)
    yaw = math.degrees(frame.yaw)
    away = 1.0 if yard_v >= 0 else -1.0
    for row in range(YARD_ROWS):
        for column in range(YARD_COLUMNS):
            u = yard_u + (column - (YARD_COLUMNS - 1) / 2.0) * YARD_TILE_CM
            v = yard_v + (row - (YARD_ROWS - 1) / 2.0) * YARD_TILE_CM
            x, y = world_point(frame, u, v)
            tile = dresser.blueprint(YARD_GROUND, "%s_Ground_%d_%d" % (label, row, column), folder, x, y, yaw=yaw)
            if tile is not None:
                scale_ground_tile(tile, YARD_TILE_CM)
    back_v = yard_v + away * YARD_TILE_CM
    for index, offset in enumerate((-0.25, 0.25)):
        x, y = world_point(frame, yard_u + offset * YARD_COLUMNS * YARD_TILE_CM, back_v)
        dresser.blueprint(HANGARS[(seed + index) % len(HANGARS)], "%s_Hangar_%d" % (label, index), folder, x, y,
                          yaw=yaw + (180.0 if away > 0 else 0.0))
    front_v = yard_v - away * YARD_TILE_CM * 0.5
    for index in range(9):
        u = yard_u + (index % 5 - 2) * YARD_TILE_CM * 0.8 + rng.uniform(-300, 300)
        v = front_v - away * (index // 5) * 1300.0 + rng.uniform(-250, 250)
        x, y = world_point(frame, u, v)
        dresser.blueprint(YARD_PROPS[(seed + index) % len(YARD_PROPS)], "%s_Prop_%d" % (label, index), folder, x, y,
                          yaw=yaw + rng.choice((0.0, 90.0, 180.0, 270.0)) + rng.uniform(-8, 8))
    pole_v = yard_v + away * (YARD_ROWS * YARD_TILE_CM / 2.0 + 900.0)
    for index in range(6):
        x, y = world_point(frame, yard_u + (index - 2.5) * 3200.0, pole_v)
        dresser.blueprint(SCORCH_POWERLINE, "%s_Powerline_%d" % (label, index), folder, x, y, yaw=yaw)
    for index in range(5):
        u = yard_u + rng.choice((-1, 1)) * rng.uniform(YARD_COLUMNS * YARD_TILE_CM / 2.0 + 600, 9000)
        v = yard_v + rng.uniform(-YARD_TILE_CM, YARD_TILE_CM)
        x, y = world_point(frame, u, v)
        dresser.blueprint(SCORCH_TREES[index % len(SCORCH_TREES)], "%s_ScorchTree_%d" % (label, index), folder, x, y,
                          yaw=rng.uniform(0, 360))


def scatter(dresser, frame, rng, paths, label, folder, count, along, lateral_min, lateral_max, scale=(0.8, 1.2),
            sink=20.0, blueprint=False):
    for index in range(count):
        u = rng.uniform(-along, along)
        v = rng.choice((-1, 1)) * rng.uniform(lateral_min, lateral_max)
        x, y = world_point(frame, u, v)
        if blueprint:
            dresser.blueprint(paths[index % len(paths)], "%s_%02d" % (label, index), folder, x, y, yaw=rng.uniform(0, 360))
        else:
            dresser.mesh(paths[index % len(paths)], "%s_%02d" % (label, index), folder, x, y, yaw=rng.uniform(0, 360),
                         scale=rng.uniform(*scale), sink=sink)


def craters(dresser, frame, rng, label, folder, count, along=8000.0, lateral=5500.0):
    for index in range(count):
        x, y = world_point(frame, rng.uniform(-along, along), rng.uniform(-lateral, lateral))
        dresser.decal(CRATER_DECALS[index % len(CRATER_DECALS)], "%s_Crater_%02d" % (label, index), folder, x, y,
                      rng.uniform(450, 1000), yaw=rng.uniform(0, 360))


def detail_steppe(dresser, terrain):
    rng = random.Random(111)
    folder = "Steppe"
    # A powerline between the battlefield and the farmstead, and scorched trees where the shells already fell.
    for index in range(10):
        x, y = world_point(terrain, -15000.0 + index * 3300.0, 8600.0)
        dresser.blueprint(SCORCH_POWERLINE, "Steppe_Powerline_%d" % index, folder, x, y, yaw=math.degrees(terrain.yaw))
    scatter(dresser, terrain, rng, SCORCH_TREES, "Steppe_ScorchTree", folder, 8, 15000, 6500, 13000, blueprint=True)
    scatter(dresser, terrain, rng, LOGS, "Steppe_Log", folder, 8, 14000, 6000, 12000)
    scatter(dresser, terrain, rng, DEAD_SHRUBS, "Steppe_Shrub", folder, 30, 15000, 3000, 13000, sink=5.0)
    craters(dresser, terrain, rng, "Steppe", folder, 10)


def detail_meadow(dresser, terrain):
    rng = random.Random(222)
    folder = "Meadow"
    yard_u, yard_v = REGIONS["Meadow"]["yards"][0]
    build_industrial_yard(dresser, terrain, yard_u, yard_v, "Meadow_Sawmill", folder, 7)
    scatter(dresser, terrain, rng, MEADOW_SHRUBS, "Meadow_Shrub", folder, 40, 16000, 2500, 12500, scale=(0.8, 1.3),
            sink=5.0)
    scatter(dresser, terrain, rng, LOGS, "Meadow_Log", folder, 12, 15000, 5000, 12000)
    scatter(dresser, terrain, rng, SCORCH_TREES, "Meadow_ScorchTree", folder, 5, 12000, 6000, 9500, blueprint=True)
    craters(dresser, terrain, rng, "Meadow", folder, 8)


def detail_winter(dresser, terrain, wrecks):
    rng = random.Random(333)
    folder = "Winter"
    yard_u, yard_v = REGIONS["Winter"]["yards"][0]
    build_industrial_yard(dresser, terrain, yard_u, yard_v, "Winter_Depot", folder, 11)
    for index in range(10):
        x, y = world_point(terrain, -15000.0 + index * 3300.0, -9200.0)
        dresser.blueprint(SCORCH_POWERLINE, "Winter_Powerline_%d" % index, folder, x, y, yaw=math.degrees(terrain.yaw))
    scatter(dresser, terrain, rng, wrecks, "Winter_FieldWreck", folder, 4, 9000, 2500, 6000, scale=(1.0, 1.0), sink=35.0)
    scatter(dresser, terrain, rng, LOGS, "Winter_Log", folder, 8, 14000, 6000, 12000)
    craters(dresser, terrain, rng, "Winter", folder, 12)


def detail_desert(dresser, arenas):
    """The desert arenas get the start view's desert dressing at a lighter density, and the East Plain an industrial
    yard on its south flank next to the old base buildings."""
    rng = random.Random(444)
    for label, (x0, y0), yaw, _, region in arenas:
        if region != "Desert":
            continue
        frame = Terrain("Steppe")
        frame.centre, frame.yaw, frame.yards = (x0, y0), math.radians(yaw), []
        short = label.replace("MenuArena_", "")
        folder = "Desert"
        scatter(dresser, frame, rng, CACTI, "Desert_%s_Cactus" % short, folder, 10, 11000, 6500, 12000, scale=(0.8, 1.3))
        scatter(dresser, frame, rng, CACTUS_BUSHES, "Desert_%s_CactusBush" % short, folder, 25, 11000, 3500, 12000,
                scale=(0.7, 1.2))
        scatter(dresser, frame, rng, DEAD_SHRUBS, "Desert_%s_Shrub" % short, folder, 20, 11000, 2000, 12000, sink=5.0)
        scatter(dresser, frame, rng, DESERT_ROCKS, "Desert_%s_Rock" % short, folder, 6, 11000, 7500, 12000,
                scale=(0.6, 1.3), sink=40.0)
    yard_frame = Terrain("Steppe")
    yard_frame.centre, yard_frame.yaw, yard_frame.yards = (16500.0, -8200.0), 0.0, []
    build_industrial_yard(dresser, yard_frame, 0.0, -1.0, "Desert_Refinery", "Desert", 3)


CROPS = "/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/FarmCrops/"
ANIMALS = ENV + "Animated/FarmAnimals/"
FLOCKS = ENV + "Animated/Birds/DesertFlocks/BP_BirdFlock_"
FENCE_STEP_CM = 250.0
GATE_WIDTH_CM = 400.0

# Farm fields in the farm landscape's battle frame (u along the battle axis, v sideways): crop blueprint, row yaw
# relative to the frame, corners (u, v), index of the edge that gets the fence gate.
FARM_FIELDS = [
    ("Wheat", 0.0, [(-13000, 7500), (-7200, 7700), (-7000, 12000), (-12800, 12200)], 0),
    ("Wheat", 90.0, [(-5600, 7600), (300, 7400), (500, 11600), (-5400, 11800)], 0),
    ("WheatBurned", 4.0, [(2000, 7200), (7600, 7000), (7800, 11000), (2200, 11200)], 0),
    ("Pumpkin", 0.0, [(-13200, -7600), (-8000, -7800), (-7800, -11600), (-13000, -11400)], 0),
    ("Vineyard", 0.0, [(8200, -7400), (13600, -7600), (13800, -11000), (8400, -10800)], 0),
]
FARM_BUILDINGS = [("FarmHouse", 10500, 11800, 180), ("Barn", 14600, 10400, 170), ("HayShed", 12200, 8300, 185),
                  ("Stable", 16200, 12800, 200)]
HAY_MESHES = ["SM_HayStack_A", "SM_HayStack_B", "SM_HayBale_Round_A", "SM_HayBale_Round_B", "SM_HayBale_Round_Stack"]
# (animal, count, centre u, centre v, spread)
FARM_HERDS = [("Sheep", 10, -2000, -10000, 2200), ("Cow", 6, 3500, -10500, 2000), ("Goat", 6, -4500, -12000, 1600),
              ("Pig", 5, 13500, 7400, 900), ("Chicken", 12, 10200, 9800, 1000)]
# (region, flock, ellipse centre u, v, yaw)
REGION_FLOCKS = [("Farm", "WhiteStork", 0, 4000, 30), ("Farm", "Dove", -6000, -6000, 200), ("Steppe", "Crow", 2000, 3000, 70),
                 ("Meadow", "Dove", 0, -3000, 120), ("Winter", "Crow", -3000, 2000, 300),
                 ("Winter", "InkRaven", 5000, -4000, 140)]
DESERT_FLOCKS = [("InkRaven", (-11500, 0), 40), ("CobaltRoller", (9000, 2500), 160), ("Crow", (-12000, 14000), 260)]


def spline_points(spline, points, closed):
    spline.clear_spline_points(False)
    for point in points:
        spline.add_spline_point(point, unreal.SplineCoordinateSpace.LOCAL, False)
    for index in range(len(points)):
        spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(closed, False)
    spline.update_spline()
    spline.set_editor_property("spline_has_been_edited", True)


def place_crop_field(dresser, terrain, crop, row_yaw, corners, seed, label):
    """A crushable crop field over the polygon (as on Ger_Trains): the outline spline sets its shape."""
    world_corners = [world_point(terrain, u, v) for u, v in corners]
    centre_x = sum(point[0] for point in world_corners) / len(world_corners)
    centre_y = sum(point[1] for point in world_corners) / len(world_corners)
    yaw = math.degrees(terrain.yaw) + row_yaw
    field = dresser.blueprint(CROPS + "BP_CropField_" + crop, label, "Farm", centre_x, centre_y, yaw=yaw)
    if field is None:
        return
    cos_yaw, sin_yaw = math.cos(math.radians(-yaw)), math.sin(math.radians(-yaw))
    local = [unreal.Vector((x - centre_x) * cos_yaw - (y - centre_y) * sin_yaw,
                           (x - centre_x) * sin_yaw + (y - centre_y) * cos_yaw, 0) for x, y in world_corners]
    spline_points(field.get_editor_property("field_outline"), local, True)
    # Setting a property reruns the construction script, which lays out the plants.
    layout = field.get_editor_property("layout")
    layout.set_editor_property("random_seed", seed)
    field.set_editor_property("layout", layout)


def place_farm_fence(dresser, terrain, corners, gate_edge, label):
    """A crushable fence around the field, open at a gate in the middle of one edge."""
    world_corners = [world_point(terrain, u, v) for u, v in corners]
    count = len(world_corners)
    a, b = world_corners[gate_edge], world_corners[(gate_edge + 1) % count]
    edge = math.dist(a, b)
    start = (a[0] + (b[0] - a[0]) * (0.5 + GATE_WIDTH_CM / 2 / edge), a[1] + (b[1] - a[1]) * (0.5 + GATE_WIDTH_CM / 2 / edge))
    end = (a[0] + (b[0] - a[0]) * (0.5 - GATE_WIDTH_CM / 2 / edge), a[1] + (b[1] - a[1]) * (0.5 - GATE_WIDTH_CM / 2 / edge))
    path = [start] + [world_corners[(gate_edge + 1 + step) % count] for step in range(count)] + [end]
    points = []
    for index in range(len(path) - 1):
        first, second = path[index], path[index + 1]
        steps = max(1, math.ceil(math.dist(first, second) / FENCE_STEP_CM))
        for step in range(steps):
            t = step / steps
            points.append((first[0] + (second[0] - first[0]) * t, first[1] + (second[1] - first[1]) * t))
    points.append(path[-1])
    fence = dresser.blueprint(CROPS + "BP_DestSpline_FarmFence", label, "Farm", points[0][0], points[0][1])
    if fence is None:
        return
    origin = fence.get_actor_location()
    local = [unreal.Vector(x - origin.x, y - origin.y, terrain.height(x, y) - origin.z) for x, y in points]
    spline_points(fence.get_editor_property("spline"), local, False)
    fence.set_editor_property("piece_health", fence.get_editor_property("piece_health"))


def dress_farm(dresser, terrain):
    rng = random.Random(606)
    folder = "Farm"
    yaw = math.degrees(terrain.yaw)
    for index, (crop, row_yaw, corners, gate) in enumerate(FARM_FIELDS):
        place_crop_field(dresser, terrain, crop, row_yaw, corners, 2000 + index * 41, "Farm_Field_%d_%s" % (index, crop))
        place_farm_fence(dresser, terrain, corners, gate, "Farm_Fence_%d" % index)
    for kind, u, v, building_yaw in FARM_BUILDINGS:
        x, y = world_point(terrain, u, v)
        dresser.blueprint(FARM + kind, "Farm_" + kind, folder, x, y, yaw=yaw + building_yaw)
    for index in range(10):
        corners = FARM_FIELDS[index % 3][2]
        u = rng.uniform(min(c[0] for c in corners) - 900, max(c[0] for c in corners) + 900)
        v = max(c[1] for c in corners) + rng.uniform(500, 1200)
        x, y = world_point(terrain, u, v)
        dresser.mesh(CROPS + "Meshes/" + HAY_MESHES[index % len(HAY_MESHES)], "Farm_Hay_%02d" % index, folder, x, y,
                     yaw=rng.uniform(0, 360), scale=1.0, sink=5.0)
    for animal, count, centre_u, centre_v, spread in FARM_HERDS:
        for index in range(count):
            x, y = world_point(terrain, centre_u + rng.uniform(-spread, spread), centre_v + rng.uniform(-spread, spread))
            dresser.blueprint(ANIMALS + animal + "/BP_Wanderer_" + animal, "Farm_%s_%02d" % (animal, index), folder, x, y,
                              yaw=rng.uniform(0, 360))
    for index, path in enumerate(TRACTORS + CARS[:2]):
        x, y = world_point(terrain, rng.uniform(-12000, 12000), rng.choice((-1, 1)) * rng.uniform(4500, 6500))
        dresser.blueprint(path, "Farm_Machine_%d" % index, folder, x, y, yaw=rng.uniform(0, 360))
    for index, (u, v) in enumerate(ring(rng, 30, 0.9, 1.08)):
        x, y = world_point(terrain, u, v)
        dresser.blueprint(LEAF_TREES[index % 2], "Farm_RimTree_%02d" % index, folder, x, y, yaw=rng.uniform(0, 360),
                          scale=rng.uniform(0.9, 1.3))
    for index in range(10):
        x, y = world_point(terrain, -15500.0 + index * 3400.0, -13800.0)
        dresser.blueprint(SCORCH_POWERLINE, "Farm_Powerline_%d" % index, folder, x, y, yaw=yaw)
    craters(dresser, terrain, rng, "Farm", folder, 6, lateral=4500.0)


def place_flocks(dresser, terrains):
    for index, (region, flock, u, v, flock_yaw) in enumerate(REGION_FLOCKS):
        terrain = terrains[region]
        x, y = world_point(terrain, u, v)
        dresser.blueprint(FLOCKS + flock, "%s_Flock_%d_%s" % (region, index, flock), region, x, y,
                          yaw=math.degrees(terrain.yaw) + flock_yaw)
    for index, (flock, (x, y), flock_yaw) in enumerate(DESERT_FLOCKS):
        dresser.blueprint(FLOCKS + flock, "Desert_Flock_%d_%s" % (index, flock), "Desert", x, y, yaw=flock_yaw)


def add_sky(actors, all_actors):
    """StartGame had no sky: an atmosphere lit by the existing sun, and a sky light that captures it."""
    if not any(actor.get_class().get_name() == "SkyAtmosphere" for actor in all_actors):
        sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        sky.set_actor_label(PREFIX + "SkyAtmosphere")
        sky.set_folder_path("MenuBattles/Sky")
    for actor in all_actors:
        if actor.get_class().get_name() != "SkyLight":
            continue
        component = actor.get_component_by_class(unreal.SkyLightComponent)
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
        component.set_editor_property("real_time_capture", True)
        component.set_editor_property("lower_hemisphere_is_black", False)


def add_nav_bounds(actors, all_actors):
    source = next((actor for actor in all_actors if actor.get_actor_label() == "RTS_NavMeshVolume"), None)
    if source is None:
        raise RuntimeError("no RTS_NavMeshVolume to copy")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for name, spec in REGIONS.items():
        volume = actors.duplicate_actor(source, world, unreal.Vector(0, 0, 0))
        volume.set_actor_label(PREFIX + "NavBounds_" + name)
        volume.set_folder_path("MenuBattles/Nav")
        volume.set_actor_location(unreal.Vector(spec["centre"][0], spec["centre"][1], 1500), False, False)
        brush_half = 100.0
        volume.set_actor_scale3d(unreal.Vector(HALF_SIZE / brush_half, HALF_SIZE / brush_half, 40))


def remove_previous(actors):
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors.destroy_actor(actor)


def build(world, actors, arenas, start_view=None):
    """Builds everything; returns the region terrains so the arenas can stand on them.
    start_view: (centre, yaw) of the arena the start camera looks at, dressed as desert after its patches are gone."""
    remove_previous(actors)
    clear_start_view(actors)
    all_actors = actors.get_all_level_actors()
    source = next((actor for actor in all_actors if actor.get_class().get_name() == "Landscape"), None)
    body_instance = source.get_editor_property("body_instance") if source else None
    if source is not None:
        # The instance has the tank trail ruts switched on (TankTrailStrength 1); the master StartGame used has not.
        source.set_editor_property("landscape_material", unreal.load_asset(DESERT_TRAIL_MATERIAL))
    terrains = {name: build_landscape(world, actors, name, body_instance) for name in REGIONS}
    all_actors = actors.get_all_level_actors()
    add_sky(actors, all_actors)
    add_nav_bounds(actors, all_actors)
    dresser = Dresser(world, actors, terrains)
    wrecks = wreck_meshes()
    dress_steppe(dresser, terrains["Steppe"])
    dress_meadow(dresser, terrains["Meadow"])
    dress_winter(dresser, terrains["Winter"], wrecks)
    detail_steppe(dresser, terrains["Steppe"])
    detail_meadow(dresser, terrains["Meadow"])
    detail_winter(dresser, terrains["Winter"], wrecks)
    dress_farm(dresser, terrains["Farm"])
    place_flocks(dresser, terrains)
    dress_desert_arenas(dresser, [arena for arena in arenas if start_view is None or arena[1] != start_view[0]], wrecks)
    if start_view is not None:
        dress_start_view(dresser, wrecks, start_view[0], start_view[1])
    detail_desert(dresser, [arena for arena in arenas if start_view is None or arena[1] != start_view[0]])
    log("dressing actors=%d wrecks available=%d" % (dresser.count, len(wrecks)))
    return terrains
