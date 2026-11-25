"""KV-3 air drop + map data asset folder (owner, 2026-11-24). Editor Python with Ger_2ndOreMountains open.

1. Every data asset of this map moves to a folder named like the map, next to the level file:
   /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains/ (convoy definitions, counterattack settings, KV-3 drop).
   rename_asset fixes the referencers; leftover redirectors are fixed up and removed.
2. DA_Ore_KV3AirDrop (UEnemyAirDropSettings): 90 s after StartAirDrop, KV-3 x 1 / 2 / 3 / 4 (New to RTS / Normal /
   Hard / Brutal + Ironman).
3. The AEnemyAirDrop ORE_KV3AirDrop (AirDropId ORE_KV3Drop) at the west end of the ridge bench: the westernmost flat
   bench spot (same height as the base) with a navmesh path to the base. Landing cargo planes (can be shot down), the
   mission orders the tanks (not handed to the enemy AI), started by the Mission_Ore_KV3Drop stage.
Re-runnable; saves the assets and the level. Prints ORE_KV3 lines.
"""
import unreal

MAP_FOLDER = '/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains'
ORE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/'
MOVES = [ORE + 'Convoys/DA_Convoy_Ore_%d' % n for n in range(1, 7)] + [
    ORE + 'DA_Ore_LightCounterattack', ORE + 'DA_Ore_HeavyCounterattack']
KV3_ASSET = 'DA_Ore_KV3AirDrop'
LABEL = 'ORE_KV3AirDrop'
AIR_DROP_ID = 'ORE_KV3Drop'
HQ = unreal.Vector(-21000.0, 8000.0, 1800.0)
BENCH_Z = (1700.0, 1950.0)          # the base bench sits at ~1800
SEARCH_X = (-24000.0, -13000.0)     # >= 45 m inside the map edge: the cargo planes need room
SEARCH_Y = (-24000.0, -6000.0)
STEP = 500.0
FLAT_RANGE_CM = 60.0                # max height spread over a 12 x 12 m patch
EAL = unreal.EditorAssetLibrary


def log(text):
    print('ORE_KV3 ' + str(text))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def move_data_assets():
    if not EAL.does_directory_exist(MAP_FOLDER):
        EAL.make_directory(MAP_FOLDER)
    for old in MOVES:
        name = old.rsplit('/', 1)[-1]
        new = MAP_FOLDER + '/' + name
        if EAL.does_asset_exist(new):
            log('already moved %s' % name)
            continue
        if not EAL.does_asset_exist(old):
            log('MISSING %s' % old)
            continue
        log('move %s -> %s: %s' % (name, MAP_FOLDER, EAL.rename_asset(old, new)))


def fix_redirectors(folders):
    """Python has no fixup_referencers: re-save every package that still refers to a redirector (it loads through the
    redirector and saves the real path), then delete the redirector once nothing refers to it."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                    include_hard_package_references=True)
    level_package = world().get_outermost().get_name()
    removed = 0
    for folder in folders:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != 'ObjectRedirector':
                continue
            package = str(data.package_name)
            for referencer in registry.get_referencers(package, options) or []:
                referencer = str(referencer)
                if referencer == level_package:
                    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
                    continue
                for asset in EAL.list_assets(referencer, recursive=False, include_folder=False) or [referencer]:
                    EAL.save_asset(asset.split('.')[0], only_if_is_dirty=False)
                log('re-saved %s (referred to %s)' % (referencer, package))
            left = registry.get_referencers(package, options) or []
            if [r for r in left if str(r) != package]:
                log('redirector %s still referenced by %s' % (package, [str(r) for r in left]))
                continue
            EAL.delete_asset(package)
            removed += 1
    log('redirectors removed: %d' % removed)


def make_kv3_asset():
    path = MAP_FOLDER + '/' + KV3_ASSET
    if EAL.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.EnemyAirDropSettings)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(KV3_ASSET, MAP_FOLDER,
                                                                         unreal.EnemyAirDropSettings, factory)
    asset.set_editor_property('delay_seconds', 90.0)
    asset.set_editor_property('vehicle_options', [unreal.EnemyPlacementUnit(
        kind=unreal.EnemyPlacementUnitKind.VEHICLE, vehicle_subtype=unreal.TankSubtype.TANK_KV3)])
    for field, count in (('count_new_to_rts', 1), ('count_normal', 2), ('count_hard', 3), ('count_brutal', 4)):
        asset.set_editor_property(field, count)
    log('KV-3 asset saved %s' % EAL.save_asset(path, only_if_is_dirty=False))
    return asset


def ground(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 20000), unreal.Vector(x, y, -5000),
                                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                  unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, None
    values = hit.to_tuple()
    actor = values[9]
    return values[4].z, (actor.get_actor_label() if actor else '')


def is_flat_bench(x, y):
    heights = []
    for dx in (-600, 0, 600):
        for dy in (-600, 0, 600):
            z, label = ground(x + dx, y + dy)
            if z is None or not label.startswith('Ore_Landscape'):
                return None
            heights.append(z)
    if max(heights) - min(heights) > FLAT_RANGE_CM or not BENCH_Z[0] <= heights[4] <= BENCH_Z[1]:
        return None
    return heights[4]


def reaches_base(point):
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world(), point, HQ)
    if path is None or not path.is_valid() or path.is_partial():
        return False
    return True


def find_landing():
    candidates = []
    y = SEARCH_Y[0]
    while y <= SEARCH_Y[1]:
        x = SEARCH_X[0]
        while x <= SEARCH_X[1]:
            z = is_flat_bench(x, y)
            if z is not None:
                candidates.append((y, x, z))
            x += STEP
        y += STEP
    candidates.sort()
    log('flat bench candidates: %d' % len(candidates))
    for y, x, z in candidates:
        point = unreal.Vector(x, y, z)
        if reaches_base(point):
            return point
    return None


def place_air_drop(asset, landing):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = [a for a in actors.get_all_level_actors() if a.get_actor_label() == LABEL]
    drop = existing[0] if existing else actors.spawn_actor_from_class(unreal.EnemyAirDrop, landing)
    drop.modify()
    drop.set_actor_label(LABEL)
    drop.set_actor_location(landing, False, False)
    drop.set_folder_path('OreMountains/Enemy')
    drop.set_editor_property('air_drop_id', AIR_DROP_ID)
    drop.set_editor_property('landing_points', [unreal.Vector(0, 0, 0)])
    drop.set_editor_property('settings_asset', asset)
    delivery = drop.get_editor_property('delivery')
    delivery.set_editor_property('delivery', unreal.EnemyAirDropDelivery.CARGO_PLANE)
    delivery.set_editor_property('hand_units_to_enemy_ai', False)
    delivery.set_editor_property('warn_player', True)
    drop.set_editor_property('delivery', delivery)
    timing = drop.get_editor_property('timing')
    timing.set_editor_property('start_on_begin_play', False)
    timing.set_editor_property('repeat', unreal.EnemyWaveSpawnerRepeat.ONCE)
    drop.set_editor_property('timing', timing)
    log('air drop %s at (%.0f, %.0f, %.0f)' % (LABEL, landing.x, landing.y, landing.z))


def run():
    if world().get_name() != 'Ger_2ndOreMountains':
        raise RuntimeError('open Ger_2ndOreMountains first')
    move_data_assets()
    fix_redirectors([ORE.rstrip('/'), MAP_FOLDER])
    asset = make_kv3_asset()
    landing = find_landing()
    if landing is None:
        log('NO landing spot found on the west bench')
        return
    place_air_drop(asset, landing)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log('level saved %s' % unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())


run()
