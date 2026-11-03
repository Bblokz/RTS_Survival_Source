"""Rebuilds the industrial yard of RTS_Shorts_TeamWeapons as a planned compound with a mountain backdrop.

Re-runnable: it removes the first yard's props and its own earlier output (labels TW_Yard_*, TW_Far*, TW_Mountain_*).
- Compound: one closed concrete wall (straight pieces at their true 340 cm length, matching corner pieces) with a gate
  toward the convoy road (-X), hangar row on +Y, process line and power plant on +X, fuel farm on -Y/+X, container yard
  on -Y/-X. The fight (local X -10..45 m, Y -25..45 m) stays clear.
- Everything stands on measured ground. seat() takes the lowest LOD0 point of the visible static meshes and traces the
  ground (slab tiles and landscapes only) across the footprint. Never actor bounds: these blueprints have a root editor
  sprite 128 cm tall, so bounds put their meshes 1.25 m in the air while reporting no gap, and geometry collections
  stretch them to the world origin. verify() reports anything that still stands clear of the ground.
- Power: five trellis lines wired through each pole's ConnectToActor, so the wires are the game's destructible ones.
- Backdrop: TW_FarLandscape (20 m quads, 10 km square) carries the ground past the route landscape into foothills, and
  a double ring of desert mountains closes the horizon. The mountain meshes are open shells, so they are sunk deep and
  backed by higher ground; their undersides would otherwise show sky.

Run in the full editor (saves the map and rebakes navigation):
  UnrealEditor.exe RTS_SurvivalShorts.uproject -ExecutePythonScript=Source/Tools/TeamWeaponShorts/rebuild_yard.py
Setup: ShortsCreator/Setups/TeamWeaponsBattle.json hides Landscape1 (it would poke through the far ground).
"""
from pathlib import Path
import json
import math
import random
import struct
import time
import traceback
import unreal

PROJECT = Path('D:/UE5Projects/RTS_Shorts')
WORK = PROJECT / 'Source/Tools/TeamWeaponShorts'
MAP = '/Game/RTS_Survival/Maps/Shorts/RTS_Shorts_TeamWeapons'
TAG = unreal.Name('ShortsOnly_TeamWeapons')
YARD = (300000.0, 50000.0)
BASE_Z = 144.0
SLAB_Z = 146.0          # top of the BP_Airport_Ground1 tiles
SLAB_HALF = 7500.0
WALL_HALF = 6970.0      # 41 pieces of 340 cm between the corner pivots
PIECE = 340.0
GATE_HALF = 680.0
INNER = (264800.0, 315200.0, 24800.0, 75200.0)  # route landscape, X and Y range
FAR_QUAD = 2000.0
FAR_COMPONENTS, FAR_QUADS = 8, 63
FAR_Z_SCALE = 200.0       # +-512 m: the rim stands up to 450 m
ROUTE_END_X = 292400.0    # the route's rolling ground has flattened out before the slab starts at 292500
GROUND = ('TW_Concrete', 'TW_DesertLandscape', 'TW_FarLandscape')
TRACE_TOP = 60000.0
SINK_CM = 2.0
CACTUS_SINK_CM = 25.0     # they taper to a tip at the foot
MAX_GAP_CM = 3.0
POLE_SPAN_MIN, POLE_SPAN_MAX = 2600.0, 4600.0
RANDOM_MESH_REACH = 40.0   # BPs that pick a random mesh at BeginPlay, and spiky-rooted trees, are seated by pivot
ROUTE_EDGE_FADE = 3000.0   # the route ground fades to the far ground's height at its rim
MOUNTAIN_SINK = 1600.0    # of the 2900 cm mesh: the holes in its shell stay under the ground

E = '/Game/RTS_Survival/Blueprints/Environment/'
D = E + 'DestructableEnvActor/'
WALLS = D + 'Military/ConcreteWall/Meshes/'
POSTAPO = D + 'PostApo/BP_Dest_PostApo_'
INDUSTRIAL = D + 'Industrial/BP_Industrial_Dest_'
AIRPORT = D + 'Airport/BP_Airport_Dest_'
SCORCH = D + 'BP_Scorch_Dest_'
SCORCH_MESH = D + 'PostApo/Meshes/SM_Scorch_'
ELECTRIC = D + 'Industrial/Electricity/BP_Electricity_'
CRATERS = '/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d'
OLD_PREFIXES = ('TW_RustyHangar', 'TW_Fence', 'TW_DestructibleBarrels', 'TW_DestructibleAmmo', 'TW_Yard_', 'TW_Far',
                'TW_Mountain_')
ROUTE_PREFIXES = ('TW_Cactus', 'TW_DeadTree', 'TW_DesertFoliage', 'TW_RouteWreck')

report = {'props': 0, 'route_seated': 0, 'route_max_correction_cm': 0.0}
counters = {}
ignored = []          # non-ground actors the traces met
support_cache = {}
DIRECTIONS = [(x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)]


def route_height(x, y):
    """The route landscape's height above BASE_Z (build_stage.height, flat before the slab)."""
    route_blend = max(0.0, min(1.0, (ROUTE_END_X - x) / 4500))
    ridge = 160 + 135 * math.sin((x - 279000) / 1350) + 55 * math.sin((x - 278000) / 650)
    sides = max(0.0, min(1.0, (abs(y - 50000) - 3500) / 4500))
    rim = min(x - INNER[0], INNER[1] - x, y - INNER[2], INNER[3] - y)
    edge = max(0.0, min(1.0, rim / ROUTE_EDGE_FADE))
    return edge * route_blend * (ridge + sides * (340 + 180 * math.sin(x / 2900) ** 2))


def mark(actor, label):
    number = counters.get(label, 0)
    counters[label] = number + 1
    actor.set_actor_label('TW_Yard_%s_%02d' % (label, number))
    actor.set_folder_path('Shorts/TeamWeapons/Yard')
    actor.set_editor_property('tags', [TAG])
    report['props'] += 1
    return actor


def ground_at(x, y):
    """The first slab or landscape surface under (x, y), looking through everything else."""
    for _ in range(40):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, TRACE_TOP), unreal.Vector(x, y, -20000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignored,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        values = hit.to_tuple()
        if not values[0]:
            return None
        if values[9].get_actor_label().startswith(GROUND):
            return values[4].z
        ignored.append(values[9])
    return None


def support_points(mesh_asset):
    """The LOD0 vertices furthest along 26 directions: enough for a mesh's lowest point and footprint at any yaw."""
    key = mesh_asset.get_path_name()
    if key in support_cache:
        return support_cache[key]
    best = {}
    for section in range(mesh_asset.get_num_sections(0)):
        for vertex in unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh_asset, 0, section)[0]:
            for direction in DIRECTIONS:
                reach = direction[0] * vertex.x + direction[1] * vertex.y + direction[2] * vertex.z
                if direction not in best or reach > best[direction][0]:
                    best[direction] = (reach, vertex)
    support_cache[key] = [entry[1] for entry in best.values()]
    return support_cache[key]


def visible_points(actor):
    """World support points of what renders: the static meshes, not sprites, geometry collections or hidden parts."""
    points = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        asset = component.static_mesh
        if asset is None or not component.is_visible() or component.get_editor_property('hidden_in_game'):
            continue
        transform = component.get_world_transform()
        points += [unreal.MathLibrary.transform_location(transform, vertex) for vertex in support_points(asset)]
    return points


def footprint(actor, random_mesh):
    """(bottom z, sample xy points): grid over the visible footprint, or round the pivot of a random-mesh BP."""
    location = actor.get_actor_location()
    points = [] if random_mesh else visible_points(actor)
    if not points:
        low_x, high_x, low_y, high_y = (location.x - RANDOM_MESH_REACH, location.x + RANDOM_MESH_REACH,
                                        location.y - RANDOM_MESH_REACH, location.y + RANDOM_MESH_REACH)
        bottom, samples = location.z, []
    else:
        low_x, high_x = min(p.x for p in points), max(p.x for p in points)
        low_y, high_y = min(p.y for p in points), max(p.y for p in points)
        lowest = min(points, key=lambda p: p.z)
        bottom, samples = lowest.z, [(lowest.x, lowest.y)]
    samples += [(low_x + (high_x - low_x) * i / 4, low_y + (high_y - low_y) * j / 4) for i in range(5) for j in range(5)]
    return bottom, samples


def seat(actor, random_mesh=False, sink=SINK_CM):
    """Lowest visible point on the lowest ground under the footprint: no daylight on any side. Returns the move."""
    bottom, samples = footprint(actor, random_mesh)
    grounds = [z for z in (ground_at(x, y) for x, y in samples) if z is not None]
    if not grounds:
        raise RuntimeError('no ground under ' + actor.get_actor_label())
    move = min(grounds) - sink - bottom
    location = actor.get_actor_location()
    location.z += move
    actor.set_actor_location(location, False, False)
    return move


def mesh(path, label, local_x, local_y, yaw=0.0, scale=1.0):
    asset = unreal.load_asset(path)
    assert asset, path
    x, y = YARD[0] + local_x, YARD[1] + local_y
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, SLAB_Z),
                                          unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    actor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(asset)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    mark(actor, label)
    seat(actor)
    return actor


def blueprint(path, label, local_x, local_y, yaw=0.0, scale=1.0, random_mesh=False):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    assert cls, path
    x, y = YARD[0] + local_x, YARD[1] + local_y
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, SLAB_Z), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    assert actor, path
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    mark(actor, label)
    seat(actor, random_mesh)
    return actor


def build_wall():
    straight = WALLS + 'SM_Scorch_Fence_Concrete'
    corner = WALLS + 'SM_Scorch_Fence_Concrete_Corner'
    positions = [-WALL_HALF + 2 * PIECE - PIECE / 2 + index * PIECE for index in range(40)]
    for along in positions:
        mesh(straight, 'WallEast', WALL_HALF, along)
        mesh(straight, 'WallNorth', along, WALL_HALF, yaw=90)
        mesh(straight, 'WallSouth', along, -WALL_HALF, yaw=90)
        if abs(along) > GATE_HALF:
            mesh(straight, 'WallWest', -WALL_HALF, along)
    for x, y, yaw in [(-1, 1, 0), (1, 1, -90), (1, -1, 180), (-1, -1, 90)]:
        mesh(corner, 'WallCorner', x * WALL_HALF, y * WALL_HALF, yaw=yaw)
    # the gate: a tower either side, the guard cabin behind the south one
    for side in (-1, 1):
        mesh(SCORCH_MESH + 'GuardTower', 'GateTower', -WALL_HALF + 260, side * (GATE_HALF + 260))
    mesh(SCORCH_MESH + 'Cabin', 'GateCabin', -WALL_HALF + 700, -1500, yaw=90)
    mesh(SCORCH_MESH + 'GuardTower', 'CornerTower', WALL_HALF - 260, WALL_HALF - 260)
    mesh(SCORCH_MESH + 'GuardTower', 'CornerTower', -WALL_HALF + 260, -WALL_HALF + 260)


def build_hangar_row():
    # four identical hangars on one front line, stores stacked in the gaps between them
    for x in (-4800, -3000, -1200, 600):
        blueprint(POSTAPO + 'Hangar1', 'Hangar', x, 5500)
    for x in (-3900, -2100, -300):
        blueprint(AIRPORT + 'BarrelStack2', 'HangarBarrels', x - 120, 4700)
        blueprint(AIRPORT + 'CrateStack03', 'HangarCrates', x + 170, 5200)
    blueprint(POSTAPO + 'WaterTower', 'WaterTower', -6200, 5900)


def build_plant():
    # power plant in the north-east corner, its two broken stacks in line with the hangars' back wall
    blueprint(POSTAPO + 'PwrStation3', 'PowerStation', 4700, 5300)
    for x in (2200, 3200):
        blueprint(POSTAPO + 'DestroyedChimney', 'Chimney', x, 6050)
    # process line along the east wall: four silos at one pitch
    for y in (-2400, -600, 1200, 3000):
        blueprint(POSTAPO + 'Silo2', 'ProcessSilo', 6050, y, yaw=90)
    # fuel farm in the south-east: the big tank with the small tanks along the south wall
    blueprint(POSTAPO + 'FuelSilo', 'FuelFarm', 4600, -4600)
    for x, name in ((-400, 'OilTank1'), (400, 'OilTank2'), (1200, 'OilTank1'), (2000, 'OilTank2')):
        blueprint(INDUSTRIAL + name, 'OilTank', x, -6200)
    for x in (0, 800, 1600):
        blueprint(AIRPORT + 'BarrelStack3', 'FuelBarrels', x, -5300)


def build_container_yard():
    # container rows in the south-west, all one way round, under the gantry crane
    for column, x in enumerate((-5800, -5300, -4800, -4300, -3800)):
        blueprint(INDUSTRIAL + 'Container', 'Container', x, -5200, random_mesh=True)
        if column != 2:
            blueprint(INDUSTRIAL + 'Container', 'Container', x, -3780, random_mesh=True)
    blueprint(POSTAPO + 'Crane2', 'Crane', -2850, -5200)
    for x, name in ((-2100, 'ContainerBox1'), (-1600, 'ContainerBox2'), (-1100, 'ContainerBox1')):
        blueprint(POSTAPO + name, 'ContainerBox', x, -6250)
    blueprint(AIRPORT + 'CrateStack01', 'YardCrates', -2100, -5400)


def build_apron():
    # a low inner fence divides the apron from the storage yards; stores wait along it and on a loading line behind the guns
    straight = WALLS + 'SM_Scorch_Fence_Concrete'
    for index in range(17):
        x = -3400 + PIECE / 2 + index * PIECE
        if abs(x + 500) > 2 * PIECE:
            mesh(straight, 'InnerFence', x, -2300, yaw=90)
    for index, x in enumerate((-3100, -2200, 300, 1200, 2100)):
        blueprint(AIRPORT + ('BarrelStack2' if index % 2 else 'CrateStack01'), 'ApronStores', x, -1950)
    for index, y in enumerate((-1800, -900, 0, 900, 1800, 2700, 3600)):
        blueprint(AIRPORT + ('CrateStack03' if index % 2 else 'BarrelStack3'), 'LoadingLine', -2700, y, yaw=90)
    blueprint(POSTAPO + 'ContainerBox2', 'LoadingBox', -3400, 1000)
    blueprint(POSTAPO + 'ContainerBox1', 'LoadingBox', -3400, 2300)


def build_scorch():
    rng = random.Random(20261030)
    # crater scars on the slab, off the two battle lines
    spots = [(-1800, -2600, 800), (1500, -2900, 650), (-3400, 1400, 900), (5100, 1900, 700), (900, 3700, 800), (-5400, -1500, 950),
             (2300, 1000, 520), (1700, -700, 480), (-1500, 900, 460), (-6200, 2600, 700), (3900, -1600, 600), (-3000, 3300, 620)]
    for index, (x, y, size) in enumerate(spots):
        decal_actor = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(YARD[0] + x, YARD[1] + y, SLAB_Z),
                                                    unreal.Rotator(roll=0.0, pitch=-90.0, yaw=rng.uniform(0, 360)))
        decal_actor.decal.set_decal_material(unreal.load_asset(CRATERS % (index % 6 + 1)))
        decal_actor.set_actor_scale3d(unreal.Vector(1.0, size / 512.0, size / 512.0))
        mark(decal_actor, 'Scar')
    # dead trees and willows standing round the outside of the wall, clear of the road to the gate
    for index in range(26):
        angle = rng.uniform(0, 2 * math.pi)
        radius = rng.uniform(9000, 16000)
        x, y = radius * math.cos(angle), radius * math.sin(angle)
        if x < 0 and abs(y) < 2500:
            continue
        blueprint(SCORCH + ('DeadWillow' if index % 3 == 0 else 'DeadTree'), 'DeadTree', x, y, yaw=rng.uniform(0, 360),
                  random_mesh=True)
    # scrap by the road outside the gate
    mesh(SCORCH_MESH + 'Crane_Destroyed', 'WreckedCrane', -9500, 4300, yaw=25)
    blueprint(POSTAPO + 'ContainerBox2', 'ScrapBox', -8200, 3200, yaw=70)


def pole(kind, local_x, local_y, yaw=0.0):
    return blueprint(ELECTRIC + kind, 'Trellis', local_x, local_y, yaw=yaw)


def connect(chain):
    """Each pole's ConnectToActor (instance-editable array) names the next: its wire component draws the span at BeginPlay."""
    for here, there in zip(chain, chain[1:]):
        links = list(here.get_editor_property('ConnectToActor'))
        links.append(there)
        here.set_editor_property('ConnectToActor', links)
        report['wire_spans'] = report.get('wire_spans', 0) + 1


def spaced(start, end, rng):
    """Pole positions from start to end, every span its own length: a fixed pitch reads as a pattern on screen."""
    direction = 1 if end > start else -1
    positions = [start]
    while abs(positions[-1] - start) < abs(end - start):
        positions.append(positions[-1] + direction * rng.uniform(POLE_SPAN_MIN, POLE_SPAN_MAX))
    return positions


def build_powerlines():
    """The cross-arms lie along local Y, so a line along X is yaw 0 and one along Y yaw 90; the pivots are at the foot."""
    rng = random.Random(2610)
    main_x = spaced(-31500, 31500, rng)
    main = [pole('TransmissionLIne', x, 9500) for x in main_x]
    connect(main)

    def main_pole_near(x):
        return main[min(range(len(main_x)), key=lambda index: abs(main_x[index] - x))]

    east_y = spaced(6000, -28000, rng)
    east = [pole('SinglePole3', 9800, y, yaw=90) for y in east_y]
    connect([main_pole_near(9800)] + east)
    # the plant's feed crosses the wall to the substation
    connect([main_pole_near(3900), pole('SinglePole1', 3900, 6450, yaw=90)])
    # along the road to the gate
    road = [pole('SinglePole2', -7800, 6600, yaw=90), pole('SinglePole2', -8000, 3600, yaw=90)]
    road += [pole('SinglePole2', x, 3600) for x in spaced(-11500, -36000, rng)]
    connect([main_pole_near(-7800)] + road)
    # along the south wall, off the east line
    south = [pole('SinglePole1', x, -7600) for x in spaced(6300, -31000, rng)]
    connect([east[min(range(len(east_y)), key=lambda index: abs(east_y[index] + 7600))]] + south)


def far_height(x, y):
    """Well under the route landscape, a thin margin at its edge; outside, foothills, then high ground behind the mountain
    rings and a rim: the mountain meshes are shells, so what stands behind them is what closes the horizon."""
    edge_x, edge_y = min(INNER[1], max(INNER[0], x)), min(INNER[3], max(INNER[2], y))
    distance = math.hypot(x - edge_x, y - edge_y)
    base = route_height(edge_x, edge_y)
    if distance == 0.0:
        inside = min(x - INNER[0], INNER[1] - x, y - INNER[2], INNER[3] - y)
        return base - 60.0 if inside < 2 * FAR_QUAD else -1500.0
    near = min(1.0, distance / 120000.0) ** 2
    high = max(0.0, min(1.0, (distance - 120000.0) / 140000.0)) ** 2
    rim = max(0.0, min(1.0, (distance - 300000.0) / 150000.0)) ** 2
    swell = 0.5 + 0.5 * math.sin(x / 17000.0 + 1.3) * math.sin(y / 23000.0 + 0.4)
    ripple = 0.5 + 0.5 * math.sin(x / 5300.0 + y / 7100.0)
    return base - 15.0 + near * (1500.0 + 6000.0 * swell + 900.0 * ripple) + high * 25000.0 * (0.6 + 0.4 * swell) + rim * 12000.0


def build_far_landscape():
    size = FAR_COMPONENTS * FAR_QUADS + 1
    half = (size - 1) * FAR_QUAD / 2
    origin = (YARD[0] - half, YARD[1] - half)
    layer_names = ['Sand', 'AridSoil', 'DesertGravel', 'DesertRockDark', 'ScorchSandyStone', 'Weedy']
    weights = [bytearray() for _ in layer_names]
    heights = []
    for row in range(size):
        y = origin[1] + row * FAR_QUAD
        for column in range(size):
            x = origin[0] + column * FAR_QUAD
            z = far_height(x, y)
            heights.append(32768 + round(z * 128 / FAR_Z_SCALE))
            rock = min(0.6, max(0.0, (z - 2500.0) / 9000.0))
            gravel = 0.14 * (0.5 + 0.5 * math.sin(x / 9000.0 + y / 6100.0))
            values = [0.5, 0.3, gravel, rock, 0.05, 0.02]
            total = sum(values)
            normalized = [round(value * 255 / total) for value in values]
            normalized[0] += 255 - sum(normalized)
            for layer, value in zip(weights, normalized):
                layer.append(value)
    height_path = WORK / 'far_height.raw'
    height_path.write_bytes(struct.pack('<%dH' % len(heights), *heights))
    material_root = '/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/'
    layers = [unreal.load_asset(material_root + name + '_LayerInfo') for name in layer_names]
    assert all(layers)
    paths = []
    for name, layer in zip(layer_names, weights):
        path = WORK / ('far_' + name + '.raw')
        path.write_bytes(layer)
        paths.append(str(path))
    terrain = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(world, unreal.Vector(origin[0], origin[1], BASE_Z),
        unreal.Vector(FAR_QUAD, FAR_QUAD, FAR_Z_SCALE), FAR_COMPONENTS, FAR_COMPONENTS, FAR_QUADS,
        unreal.load_asset(material_root + 'MI_RTS_Landscape_DesertPaintable'), str(height_path), layers, paths, 'TW_FarLandscape')
    assert terrain
    terrain.set_actor_label('TW_FarLandscape')
    terrain.set_folder_path('Shorts/TeamWeapons/Yard')
    terrain.set_editor_property('tags', [TAG])


def build_mountains():
    rng = random.Random(1944)
    paths = [E + 'Obstacles/Desert/BP_Desert_Obstacle_Mountain3', E + 'Obstacles/Desert/BP_Desert_Obstacle_Mountain5']
    index = 0
    for count, radius, scale_low, scale_high in ((12, 190000, 14.0, 17.0), (14, 275000, 19.0, 23.0)):
        for step in range(count):
            angle = 2 * math.pi * (step + rng.uniform(-0.2, 0.2)) / count
            x, y = YARD[0] + radius * math.cos(angle), YARD[1] + radius * math.sin(angle)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(paths[index % 2])
            scale = rng.uniform(scale_low, scale_high)
            actor = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, BASE_Z + far_height(x, y) - MOUNTAIN_SINK * scale),
                                                  unreal.Rotator(pitch=0, yaw=rng.uniform(0, 360), roll=0))
            assert actor
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            actor.set_actor_label('TW_Mountain_%02d' % index)
            actor.set_folder_path('Shorts/TeamWeapons/Yard')
            actor.set_editor_property('tags', [TAG])
            index += 1


def rebuild_route_landscape():
    """The first landscape was still rolling over the slab's west end and buried it; same weights, flat before the slab."""
    components, quads = 8, 63
    size = components * quads + 1
    heights = [32768 + round(route_height(264800 + column * 100, 24800 + row * 100) * 128 / 100)
               for row in range(size) for column in range(size)]
    height_path = WORK / 'route_height.raw'
    height_path.write_bytes(struct.pack('<%dH' % len(heights), *heights))
    layer_names = ['Sand', 'AridSoil', 'DesertGravel', 'DesertRockDark', 'ScorchSandyStone', 'Weedy']
    material_root = '/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/'
    layers = [unreal.load_asset(material_root + name + '_LayerInfo') for name in layer_names]
    paths = [str(WORK / (name + '.raw')) for name in layer_names]
    terrain = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(world, unreal.Vector(264800, 24800, BASE_Z),
        unreal.Vector(100, 100, 100), components, components, quads,
        unreal.load_asset(material_root + 'MI_RTS_Landscape_DesertPaintable'), str(height_path), layers, paths, 'TW_DesertLandscape')
    assert terrain
    assert unreal.RTSShortsLandscapeLibrary.configure_vehicle_ground_landscape(terrain)
    terrain.set_actor_label('TW_DesertLandscape')
    terrain.set_folder_path('Shorts/TeamWeapons')
    terrain.set_editor_property('tags', [TAG])


def seat_route_props():
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(ROUTE_PREFIXES):
            # one root spike on the ground would lift a dead tree on a mound and bury it on a slope
            move = seat(actor, label.startswith('TW_DeadTree'), CACTUS_SINK_CM if label.startswith('TW_Cactus') else SINK_CM)
            report['route_seated'] += 1
            report['route_max_correction_cm'] = max(report['route_max_correction_cm'], abs(move))


def verify():
    """Independent of the seating: any visible bottom more than MAX_GAP_CM clear of the ground under it is a failure."""
    floating = {}
    checked = 0
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if not label.startswith(ROUTE_PREFIXES + ('TW_Yard_',)) or '_Scar_' in label:
            continue
        random_mesh = '_Container_' in label or '_DeadTree_' in label
        bottom, samples = footprint(actor, random_mesh)
        ground = ground_at(*samples[0])
        checked += 1
        if ground is None or bottom - ground > MAX_GAP_CM:
            floating[label] = None if ground is None else round(bottom - ground, 1)
    report['verified'] = checked
    report['floating'] = floating


def build():
    global world, actors
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world.get_path_name().startswith(MAP + '.')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith('Landscape1'):
            ignored.append(actor)     # hidden at render time (the setup's HiddenActorPrefixes)
        elif label.startswith(OLD_PREFIXES) or label == 'TW_DesertLandscape':
            actors.destroy_actor(actor)
            removed += 1
    report['removed'] = removed
    rebuild_route_landscape()
    build_far_landscape()
    seat_route_props()
    build_wall()
    build_hangar_row()
    build_plant()
    build_container_yard()
    build_apron()
    build_scorch()
    build_powerlines()
    build_mountains()
    verify()
    print('TEAM_WEAPONS_YARD_BUILT', json.dumps(report))
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    assert unreal.RTSShortsLandscapeLibrary.rebuild_stage_navigation(world)


def finish_when_ready(delta):
    global nav_idle_since
    if unreal.NavigationSystemV1.is_navigation_being_built(world):
        nav_idle_since = time.monotonic()
        return
    if time.monotonic() - nav_idle_since < 15:
        return
    unreal.unregister_slate_post_tick_callback(callback)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    (WORK / 'YardAudit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('TEAM_WEAPONS_YARD_SAVED', json.dumps(report))
    unreal.SystemLibrary.quit_editor()


try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    build()
    nav_idle_since = time.monotonic()
    callback = unreal.register_slate_post_tick_callback(finish_when_ready)
except Exception:
    traceback.print_exc()
    print('TEAM_WEAPONS_YARD_FAILED')
    unreal.SystemLibrary.quit_editor()
