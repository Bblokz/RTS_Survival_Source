"""Build an isolated two-set capture map using existing desert and industrial assets."""
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
WORK.mkdir(parents=True, exist_ok=True)
MAP = '/Game/RTS_Survival/Maps/Shorts/RTS_Shorts_TeamWeapons'
TAG = unreal.Name('ShortsOnly_TeamWeapons')
ROUTE = (280000, 50000)
YARD = (300000, 50000)
BASE_Z = 144
created = []


def height(x, y):
    route_blend = max(0., min(1., (294000 - x) / 4500))
    ridge = 160 + 135 * math.sin((x - 279000) / 1350) + 55 * math.sin((x - 278000) / 650)
    sides = max(0., min(1., (abs(y - 50000) - 3500) / 4500))
    return BASE_Z + route_blend * (ridge + sides * (340 + 180 * math.sin(x / 2900) ** 2))


def mark(actor, label):
    actor.set_actor_label(label)
    actor.set_folder_path('Shorts/TeamWeapons')
    actor.set_editor_property('tags', [TAG])
    created.append(actor)
    return actor


def mesh_actor(path, label, x, y, size=None, scale=1., yaw=0):
    mesh = unreal.load_asset(path)
    assert mesh, path
    bounds = mesh.get_bounds()
    factor = size / max(1., max(bounds.box_extent.x, bounds.box_extent.y) * 2) if size else scale
    z = height(x, y) - (bounds.origin.z - bounds.box_extent.z) * factor
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(factor, factor, factor))
    return mark(actor, label)


def blueprint(path, label, x, y, yaw=0, scale=1.):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    assert cls, path
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, height(x, y)), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    assert actor, path
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    origin, extent = actor.get_actor_bounds(False)
    location = actor.get_actor_location()
    location.z += height(x, y) - (origin.z - extent.z)
    actor.set_actor_location(location, False, False)
    return mark(actor, label)


def create_terrain():
    components, quads = 8, 63
    size = components * quads + 1
    layer_names = ['Sand', 'AridSoil', 'DesertGravel', 'DesertRockDark', 'ScorchSandyStone', 'Weedy']
    weights = [bytearray() for _ in layer_names]
    heights = []
    for row in range(size):
        y = 24800 + row * 100
        for column in range(size):
            x = 264800 + column * 100
            z = height(x, y) - BASE_Z
            heights.append(32768 + round(z * 128 / 100))
            road = math.exp(-((y - ROUTE[1]) / 2200) ** 2)
            gravel = .16 * (.5 + .5 * math.sin(x / 1600 + y / 1100))
            slope = min(.3, max(0, (abs(y - 50000) - 4500) / 17000))
            scorch = .12 * math.exp(-((x - 282000) / 1500) ** 2 - ((y - 54300) / 900) ** 2)
            values = [.48, .17 + .30 * road, gravel, slope, scorch, .025 * (1 - road)]
            total = sum(values)
            normalized = [round(value * 255 / total) for value in values]
            normalized[0] += 255 - sum(normalized)
            for layer, value in zip(weights, normalized):
                layer.append(value)
    height_path = WORK / 'route_height.raw'
    height_path.write_bytes(struct.pack('<%dH' % len(heights), *heights))
    material_root = '/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/'
    layers = [unreal.load_asset(material_root + name + '_LayerInfo') for name in layer_names]
    assert all(layers)
    paths = []
    for name, layer in zip(layer_names, weights):
        path = WORK / (name + '.raw')
        path.write_bytes(layer)
        paths.append(str(path))
    terrain = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(world, unreal.Vector(264800, 24800, BASE_Z),
        unreal.Vector(100, 100, 100), components, components, quads,
        unreal.load_asset(material_root + 'MI_RTS_Landscape_DesertPaintable'), str(height_path), layers, paths, 'TW_DesertLandscape')
    assert terrain
    assert unreal.RTSShortsLandscapeLibrary.configure_vehicle_ground_landscape(terrain)
    mark(terrain, 'TW_DesertLandscape')


def build_route():
    rng = random.Random(20261027)
    nature = '/Game/RTS_Survival/Meshes/'
    for index in range(48):
        x = ROUTE[0] + rng.uniform(-4500, 10000)
        y = ROUTE[1] + rng.choice([-1, 1]) * rng.uniform(3400, 7800)
        mesh_actor(nature + 'Desert/Cacti/SM_Desert_Cactus%d' % (index % 4 + 1),
            'TW_Cactus_%02d' % index, x, y, scale=rng.uniform(.8, 1.5), yaw=rng.uniform(-180, 180))
    for index in range(18):
        x = ROUTE[0] + rng.uniform(-4000, 11000)
        y = ROUTE[1] + rng.choice([-1, 1]) * rng.uniform(4200, 8500)
        mesh_actor(nature + 'Nature/Trees/DeadTrees/SM_RTS_DeadTree_%d' % (index % 3 + 1),
            'TW_DeadTree_%02d' % index, x, y, scale=rng.uniform(.7, 1.1), yaw=rng.uniform(-180, 180))
    bushes = list((PROJECT / 'Content/RTS_Survival/Meshes/Desert/Cacti/CactusBush').glob('SM_*Green*.uasset'))
    assert bushes
    for index in range(120):
        path = bushes[index % len(bushes)]
        package = '/Game/' + path.relative_to(PROJECT / 'Content').with_suffix('').as_posix()
        mesh_actor(package, 'TW_DesertFoliage_%03d' % index, ROUTE[0] + rng.uniform(-4500, 10000),
            ROUTE[1] + rng.choice([-1, 1]) * rng.uniform(2900, 8500), scale=rng.uniform(.5, 1.1), yaw=rng.uniform(-180, 180))
    wrecks = list((PROJECT / 'Content/RTS_Survival/Blueprints/Environment/ScavengeObjects/DestroyedTanks').rglob('SM_Destroyed*.uasset'))
    assert wrecks
    for index in range(7):
        path = wrecks[index * 7 % len(wrecks)]
        mesh_actor('/Game/' + path.relative_to(PROJECT / 'Content').with_suffix('').as_posix(),
            'TW_RouteWreck_%02d' % index, ROUTE[0] - 2000 + index * 1800,
            ROUTE[1] + (-1 if index % 2 else 1) * (4300 + index % 3 * 650), yaw=index * 53)


def build_yard():
    ground_class = '/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Airport/BP_Airport_Ground1'
    for row in range(-2, 3):
        for column in range(-2, 3):
            floor = actors.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(ground_class),
                unreal.Vector(YARD[0] + column * 3000 + 1500, YARD[1] + row * 3000 + 1500, BASE_Z + 2))
            floor.set_actor_scale3d(unreal.Vector(1.25, 1.25, 1))
            mark(floor, 'TW_Concrete_%d_%d' % (row, column))
    hangar = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_Hangar1'
    for index, (x, y, yaw) in enumerate([(1000, -6500, 0), (4000, 6500, 180), (-3500, 6500, 180)]):
        blueprint(hangar, 'TW_RustyHangar_%d' % index, YARD[0] + x, YARD[1] + y, yaw)
    fence = '/Game/RTS_Survival/Meshes/Buildings/Hangars/SM_Post_Hangar_BarbedFenceModular1'
    for side in [-1, 1]:
        for index in range(13):
            mesh_actor(fence, 'TW_Fence_%d_%02d' % (side, index), YARD[0] - 6000 + index * 1000,
                       YARD[1] + side * 5700, size=1000, yaw=90)
    environment = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/'
    for index, (x, y) in enumerate([(-1000, -3000), (500, 3900), (2200, -3200), (4600, 3900), (5700, -3900), (-2800, 3600)]):
        blueprint(environment + 'BP_Airport_Dest_BarrelStack' + ('3' if index % 2 else '2'),
                  'TW_DestructibleBarrels_%02d' % index, YARD[0] + x, YARD[1] + y, yaw=index * 42)
        blueprint(environment + 'BP_Airport_Dest_CrateStack03', 'TW_DestructibleAmmo_%02d' % index,
                  YARD[0] + x + 450, YARD[1] + y + 300, yaw=index * 35)


def build():
    global world, actors
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    else:
        assert unreal.EditorLevelLibrary.new_level_from_template(MAP, '/Game/RTS_Survival/Maps/Shorts/RTS_Shorts')
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().startswith(MAP + '.')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    # This file authors only the new map. Keep its first saved copy as a recovery point.
    if any(actor.get_actor_label().startswith('TW_') for actor in all_actors):
        raise RuntimeError('Stage already exists. Review it before regenerating.')
    create_terrain()
    source_stage = next(actor for actor in all_actors if actor.get_actor_label() == 'Stage_DesertBattle')
    for label, point in [('TeamWeaponDesert', ROUTE), ('TeamWeaponIndustrial', YARD)]:
        stage = actors.duplicate_actor(source_stage, world, unreal.Vector(0, 0, 0))
        mark(stage, 'Stage_' + label)
        stage.rename('Stage_' + label)
        stage.set_actor_location(unreal.Vector(point[0], point[1], height(*point) + 40), False, False)
    for original_label, new_label in [('DesertBattle_Sun', 'TeamWeapons_Sun'), ('DB_Sky', 'TW_Sky'), ('DesertBattle_Fog', 'TW_Fog')]:
        source = next((actor for actor in all_actors if actor.get_actor_label() == original_label), None)
        if source:
            mark(actors.duplicate_actor(source, world, unreal.Vector(0, 0, 0)), new_label)
    source_nav = next(actor for actor in all_actors if actor.get_actor_label() == 'RTS_NavMeshVolume')
    navigation = mark(actors.duplicate_actor(source_nav, world, unreal.Vector(0, 0, 0)), 'TW_NavMeshBounds')
    navigation.set_actor_location(unreal.Vector(290000, 50000, 1000), False, False)
    navigation.set_actor_scale3d(unreal.Vector(265, 265, 60))
    build_route()
    build_yard()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    assert unreal.RTSShortsLandscapeLibrary.rebuild_stage_navigation(world)
    print('TEAM_WEAPONS_STAGE_NAV_BUILD_STARTED actors=%d' % len(created))


def finish_when_ready(delta):
    global nav_idle_since
    if unreal.NavigationSystemV1.is_navigation_being_built(world):
        nav_idle_since = time.monotonic()
        return
    if time.monotonic() - nav_idle_since < 15:
        return
    unreal.unregister_slate_post_tick_callback(callback)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    report = {'map': MAP, 'created_actors': len(created), 'navigation_baked': True,
              'ground_material': '/Game/RTS_Survival/Meshes/PresetRegion_PlayerStart/AirfieldGround/Materials/M_RTSGround_WithSand',
              'route_cacti': 48, 'dead_trees': 18, 'foliage': 120, 'wrecks': 7, 'ground_tiles': 25,
              'hangars': 3, 'fences': 26, 'destructible_barrel_stacks': 6, 'destructible_ammo_stacks': 6}
    (WORK / 'StageAudit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('TEAM_WEAPONS_STAGE_SAVED', json.dumps(report))
    unreal.SystemLibrary.quit_editor()


try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    build()
    nav_idle_since = time.monotonic()
    callback = unreal.register_slate_post_tick_callback(finish_when_ready)
except Exception:
    traceback.print_exc()
    print('TEAM_WEAPONS_STAGE_FAILED')
    unreal.SystemLibrary.quit_editor()
