"""Berlin early patrols (BerlinMissionScripting.md section 8): per-difficulty patrol units in DA_BerlinForces_* and the
three placed BerlinPatrol_* wave spawners near the player start.

Run inside an editor that has Ger_Berlin open (UECP execute_python):
  import sys; sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
  import importlib, berlin_patrols as bp; importlib.reload(bp); bp.configure_forces(); bp.place_spawners()
The spawners are plain placed actors: move them (and drag their spawn / waypoint diamonds) in the editor freely.
"""
import unreal

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
DATA = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/'  # Berlin data assets live next to the map
FOLDER = 'Mission/Patrols'

# Reichstag forecourt: beaten squads run here (same point as StayUndetected's forecourt alarm).
RALLY = unreal.Vector(2600.0, -15994.0, 0.0)

# Per difficulty: (kind, subtype name, count). Normal is the owner's spec; the others are derived from it.
# Every list keeps vehicles, so the "vehicles dead -> infantry runs" rule always applies.
PATROL_UNITS = {
    'NewToRTS': [('squad', 'SQUAD_RUS_OKHOTNIK', 1), ('squad', 'SQUAD_RUS_MOSIN', 1), ('vehicle', 'TANK_BT7', 2)],
    'Normal': [('squad', 'SQUAD_RUS_OKHOTNIK', 2), ('vehicle', 'TANK_BT7', 2), ('vehicle', 'TANK_T34_76', 1)],
    'Hard': [('squad', 'SQUAD_RUS_OKHOTNIK', 2), ('squad', 'SQUAD_RUS_MOSIN', 1), ('vehicle', 'TANK_BT7', 2),
             ('vehicle', 'TANK_T34_76', 1)],
    'Brutal': [('squad', 'SQUAD_RUS_OKHOTNIK', 2), ('squad', 'SQUAD_RUS_MOSIN', 1), ('vehicle', 'TANK_BT7', 1),
               ('vehicle', 'TANK_T34_76', 2)],
}

# SpawnerId, rough centre (from BerlinMission.md "candidate patrol areas"), patrol loop offsets from the centre.
SPAWNERS = [
    ('BerlinPatrol_Central', (11000.0, 30000.0), [(2500.0, 0.0), (-1500.0, 2200.0), (-1500.0, -2200.0)]),
    ('BerlinPatrol_North', (18000.0, 30000.0), [(0.0, 2500.0), (2200.0, -1500.0), (-2200.0, -1500.0)]),
    ('BerlinPatrol_South', (-3000.0, 34000.0), [(2500.0, 0.0), (-1500.0, 2200.0), (-1500.0, -2200.0)]),
]
SPAWN_POINT_OFFSETS = [(0.0, 0.0), (600.0, 0.0)]
NAV_EXTENT = unreal.Vector(800.0, 800.0, 3000.0)


def log(text):
    unreal.log_warning('BERLIN_PATROLS ' + text)


def make_unit(kind, subtype):
    unit = unreal.EnemyPlacementUnit()
    if kind == 'squad':
        unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.SQUAD)
        unit.set_editor_property('squad_subtype', getattr(unreal.SquadSubtype, subtype))
    else:
        unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.VEHICLE)
        unit.set_editor_property('vehicle_subtype', getattr(unreal.TankSubtype, subtype))
    return unit


def make_entries(spec):
    entries = []
    for kind, subtype, count in spec:
        entry = unreal.EnemyWaveSpawnerUnitEntry()
        entry.set_editor_property('unit_options', [make_unit(kind, subtype)])
        entry.set_editor_property('count', count)
        entry.set_editor_property('spawn_point_index', -1)
        entries.append(entry)
    return entries


def make_fallback():
    settings = unreal.EnemyFallbackSettings()
    settings.set_editor_property('rally_location', RALLY)
    return settings


def configure_forces():
    for difficulty, spec in PATROL_UNITS.items():
        path = DATA + 'DA_BerlinForces_' + difficulty
        asset = unreal.load_asset(path)
        if asset is None:
            log(f'MISSING {path}')
            continue
        patrols = asset.get_editor_property('patrols')
        patrols.set_editor_property('patrol_units', make_entries(spec))
        patrols.set_editor_property('fallback', make_fallback())
        asset.set_editor_property('patrols', patrols)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        log(f'{difficulty}: {sum(c for _, _, c in spec)} units, saved={saved}')


def project(world, x, y, z_hint=0.0):
    point = unreal.NavigationSystemV1.project_point_to_navigation(world, unreal.Vector(x, y, z_hint), None, None,
                                                                   NAV_EXTENT)
    if point is None or (abs(point.x) < 1.0 and abs(point.y) < 1.0 and abs(point.z) < 1.0):
        return None
    return point


def project_near(world, x, y):
    """Nav point at (x, y), else a little closer to/further from it in a few tries."""
    for dx, dy in ((0, 0), (500, 0), (-500, 0), (0, 500), (0, -500), (1000, 1000), (-1000, -1000)):
        point = project(world, x + dx, y + dy)
        if point is not None:
            return point
    return None


def find_spawner(world, spawner_id):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.EnemyWaveSpawner):
        if str(actor.get_editor_property('spawner_id')) == spawner_id:
            return actor
    return None


def configure_spawner(actor, spawner_id, centre, spawn_points, waypoints):
    actor.set_actor_label(spawner_id)
    actor.set_folder_path(FOLDER)
    actor.set_editor_property('spawner_id', spawner_id)
    actor.set_editor_property('behaviour', unreal.EnemyWaveSpawnerBehaviour.PATROL_WAVE)
    actor.set_editor_property('spawn_points', [unreal.Vector(p.x - centre.x, p.y - centre.y, p.z - centre.z)
                                               for p in spawn_points])
    actor.set_editor_property('waypoints', [unreal.Vector(p.x - centre.x, p.y - centre.y, p.z - centre.z)
                                            for p in waypoints])
    units = unreal.EnemyWaveSpawnerUnits()
    # Preview only: Mission_Berlin_Patrols replaces these with the running difficulty's data asset (SetUnits).
    units.set_editor_property('base_units', make_entries(PATROL_UNITS['Normal']))
    actor.set_editor_property('units', units)
    timing = actor.get_editor_property('timing')
    timing.set_editor_property('start_on_begin_play', False)
    timing.set_editor_property('first_wave_delay', 2.0)
    timing.set_editor_property('repeat', unreal.EnemyWaveSpawnerRepeat.ONCE)
    actor.set_editor_property('timing', timing)
    patrol = actor.get_editor_property('patrol_settings')
    patrol.set_editor_property('infantry_flees_when_vehicles_lost', True)
    formation = patrol.get_editor_property('formation')
    formation.set_editor_property('max_formation_width', 2)
    patrol.set_editor_property('formation', formation)
    actor.set_editor_property('patrol_settings', patrol)
    minimap = actor.get_editor_property('minimap')
    minimap.set_editor_property('show_on_minimap', True)
    actor.set_editor_property('minimap', minimap)


def place_spawners():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != 'Ger_Berlin':
        log('ABORT wrong map ' + world.get_name())
        return False
    if unreal.NavigationSystemV1.is_navigation_being_built(world):
        log('navmesh still building, try again')
        return False
    ok = True
    for spawner_id, (cx, cy), loop in SPAWNERS:
        centre = project_near(world, cx, cy)
        if centre is None:
            log(f'{spawner_id}: no navmesh near centre')
            ok = False
            continue
        spawn_points = [project_near(world, centre.x + dx, centre.y + dy) or centre for dx, dy in SPAWN_POINT_OFFSETS]
        waypoints = [project_near(world, centre.x + dx, centre.y + dy) for dx, dy in loop]
        waypoints = [w for w in waypoints if w is not None]
        if len(waypoints) < 2:
            log(f'{spawner_id}: only {len(waypoints)} waypoints on the navmesh')
            ok = False
            continue
        actor = find_spawner(world, spawner_id)
        if actor is None:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.EnemyWaveSpawner, centre)
        else:
            actor.set_actor_location(centre, False, False)
        configure_spawner(actor, spawner_id, centre, spawn_points, waypoints)
        log(f'{spawner_id}: at {centre.x:.0f},{centre.y:.0f},{centre.z:.0f} waypoints '
            + ' '.join(f'({w.x:.0f},{w.y:.0f},{w.z:.0f})' for w in waypoints))
    return ok


def describe():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    lines = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.EnemyWaveSpawner):
        loc = actor.get_actor_location()
        lines.append(f"{actor.get_actor_label()} id={actor.get_editor_property('spawner_id')} "
                     f"at ({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) "
                     f"wps={len(actor.get_editor_property('waypoints'))} "
                     f"flee={actor.get_editor_property('patrol_settings').get_editor_property('infantry_flees_when_vehicles_lost')}")
    return lines
