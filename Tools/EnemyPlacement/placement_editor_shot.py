"""Throwaway-editor pictures of the placed enemy mission actors (never run in the shared editor).
Places an attack and a patrol wave spawner, a convoy route (with its drive-path preview), a strongpoint (with a
trench arc and the attack spawner linked as reinforcements) and two air drops (a parachute drop that starts when the
strongpoint is destroyed, and a cargo plane landing) on the startup map, then takes two HighResShots from above:
nothing selected (summary layer only) and the strongpoint, convoy route and parachute drop selected (detail too).
Run:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Writes Saved/EnemyPlacementTest/editor_shot_log.json; the pictures land in Saved/Screenshots/WindowsEditor.
"""
import json
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/EnemyPlacementTest/editor_shot_log.json'
CONVOY_DEFINITION = '/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_Fuel.DA_Convoy_Fuel'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': []}
CAMERA_OFFSET = unreal.Vector(1500, -1500, 15000)
CAMERA_ROTATION = unreal.Rotator(0, -80, 90)


def log(message):
    S['log'].append(message)
    unreal.log('PLACEMENTSHOT ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def ground_at(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return unreal.Vector(x, y, hit.to_tuple()[4].z if hit else 0.0)


def difficulty(name):
    # Python spells enum values in upper snake case; match by letters only so NewToRTS is found however it is split.
    # The enum ERTSGameDifficulty shares its Python name with the struct FRTSGameDifficulty, so look for the enum type.
    wanted = name.replace('_', '').lower()
    enum_type = type(unreal.EnemyStrongpointPost().get_editor_property('from_difficulty'))
    for attribute in dir(enum_type):
        if attribute.replace('_', '').lower() == wanted:
            return getattr(enum_type, attribute)
    raise AttributeError(f'no difficulty {name} in {enum_type}: {[a for a in dir(enum_type) if a.isupper()]}')


def vehicle(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.VEHICLE)
    unit.set_editor_property('vehicle_subtype', subtype)
    return unit


def squad(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.SQUAD)
    unit.set_editor_property('squad_subtype', subtype)
    return unit


def unit_entry(unit, count):
    entry = unreal.EnemyWaveSpawnerUnitEntry()
    entry.set_editor_property('unit_options', [unit])
    entry.set_editor_property('count', count)
    return entry


def place_wave_spawners(actors, world):
    attack = actors.spawn_actor_from_class(unreal.EnemyWaveSpawner, ground_at(world, -2000, -3500))
    attack.set_editor_property('spawner_id', 'Berlin_NorthAttack')
    attack.set_editor_property('spawn_points', [unreal.Vector(0, 0, 0), unreal.Vector(0, 700, 0)])
    attack.set_editor_property('waypoints', [unreal.Vector(2500, 600, 0), unreal.Vector(4500, -900, 0)])
    units = attack.get_editor_property('units')
    units.set_editor_property('base_units', [unit_entry(vehicle(unreal.TankSubtype.TANK_SU_85), 2)])
    attack.set_editor_property('units', units)
    timing = attack.get_editor_property('timing')
    timing.set_editor_property('start_on_begin_play', False)
    attack.set_editor_property('timing', timing)

    patrol = actors.spawn_actor_from_class(unreal.EnemyWaveSpawner, ground_at(world, -2000, 1500))
    patrol.set_editor_property('spawner_id', 'Berlin_ParkPatrol')
    patrol.set_editor_property('behaviour', unreal.EnemyWaveSpawnerBehaviour.PATROL_WAVE)
    patrol.set_editor_property('waypoints', [unreal.Vector(2500, 0, 0), unreal.Vector(4500, 1200, 0),
                                             unreal.Vector(3500, -1800, 0)])
    units = patrol.get_editor_property('units')
    units.set_editor_property('base_units', [unit_entry(squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN), 2)])
    patrol.set_editor_property('units', units)
    return attack, patrol


def place_convoy_route(actors, world):
    route = actors.spawn_actor_from_class(unreal.EnemyConvoyRoute, ground_at(world, -3000, 5500))
    route.set_editor_property('route_id', 'Ore_ValleyConvoy')
    route.set_editor_property('definition', unreal.load_object(None, CONVOY_DEFINITION))
    route.set_editor_property('route_points', [unreal.Vector(0, 0, 0), unreal.Vector(5000, -800, 0),
                                               unreal.Vector(10000, 0, 0)])
    try:
        route.call_method('PreviewDrivePath')
        log('drive path preview built')
    except Exception as error:  # noqa: BLE001 - the picture still shows the route without the preview
        log(f'no drive path preview: {error!r}')
    return route


def place_strongpoint(actors, world, reinforcements):
    strongpoint = actors.spawn_actor_from_class(unreal.EnemyStrongpoint, ground_at(world, 5500, -1500))
    strongpoint.set_editor_property('strongpoint_id', 'Berlin_BridgeHold')
    posts = []
    layout = [((700, 0, 0), 0, vehicle(unreal.TankSubtype.TANK_SU_85), difficulty('NewToRTS'), True),
              ((0, 800, 0), 90, vehicle(unreal.TankSubtype.TANK_SU_85), difficulty('Hard'), True),
              ((-500, -400, 0), 0, squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN), difficulty('NewToRTS'),
               False),
              ((-300, 500, 0), 45, squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN), difficulty('Brutal'),
               False)]
    for location, yaw, unit, from_difficulty, dig_in in layout:
        post = unreal.EnemyStrongpointPost()
        post.set_editor_property('post', unreal.Transform(unreal.Vector(*location), unreal.Rotator(0, 0, yaw)))
        post.set_editor_property('unit_options', [unit])
        post.set_editor_property('from_difficulty', from_difficulty)
        post.set_editor_property('dig_in', dig_in)
        posts.append(post)
    strongpoint.set_editor_property('posts', posts)
    trenches = strongpoint.get_editor_property('trenches')
    trenches.set_editor_property('dig_trenches', True)
    trenches.set_editor_property('perimeter_radius', 2200.0)
    trenches.set_editor_property('threat_point', unreal.Vector(-4000, 0, 0))
    strongpoint.set_editor_property('trenches', trenches)
    links = strongpoint.get_editor_property('reinforcements')
    links.set_editor_property('spawners', [reinforcements])
    strongpoint.set_editor_property('reinforcements', links)
    return strongpoint


def air_drop_entry(unit):
    entry = unreal.EnemyAirDropUnitEntry()
    entry.set_editor_property('unit_options', [unit])
    entry.set_editor_property('count', 1)
    return entry


def place_air_drops(actors, world, linked_actor):
    para = actors.spawn_actor_from_class(unreal.EnemyAirDrop, ground_at(world, -6500, -2000))
    para.set_editor_property('air_drop_id', 'Berlin_ParkParadrop')
    para.set_editor_property('landing_points', [unreal.Vector(0, 0, 0), unreal.Vector(1800, 1200, 0)])
    units = para.get_editor_property('units')
    units.set_editor_property('base_units', [air_drop_entry(squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN)),
                                             air_drop_entry(vehicle(unreal.TankSubtype.TANK_SU_85))])
    units.set_editor_property('extra_units_hard', [air_drop_entry(squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN))])
    para.set_editor_property('units', units)
    link = para.get_editor_property('link')
    link.set_editor_property('rule', unreal.EnemyAirDropLinkRule.START_WHEN_DESTROYED)
    link.set_editor_property('linked_actor', linked_actor)
    para.set_editor_property('link', link)

    cargo = actors.spawn_actor_from_class(unreal.EnemyAirDrop, ground_at(world, 9500, 3500))
    cargo.set_editor_property('air_drop_id', 'Ore_AirfieldLanding')
    delivery = cargo.get_editor_property('delivery')
    delivery.set_editor_property('delivery', unreal.EnemyAirDropDelivery.CARGO_PLANE)
    cargo.set_editor_property('delivery', delivery)
    units = cargo.get_editor_property('units')
    units.set_editor_property('base_units', [air_drop_entry(vehicle(unreal.TankSubtype.TANK_T34_76))])
    cargo.set_editor_property('units', units)
    return para, cargo


def place_all(world):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    attack, patrol = place_wave_spawners(actors, world)
    route = place_convoy_route(actors, world)
    strongpoint = place_strongpoint(actors, world, attack)
    para, cargo = place_air_drops(actors, world, strongpoint)
    # set_editor_property already re-ran each actor's construction (its drawing); a second pass covers the link to the
    # attack spawner, which was set before the strongpoint's own posts.
    strongpoint.set_editor_property('strongpoint_id', 'Berlin_BridgeHold')
    actors.select_nothing()
    S['detail'] = [strongpoint, route, para]
    centre = ground_at(world, 1500, 0)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        centre + CAMERA_OFFSET, CAMERA_ROTATION)
    log(f'placed attack {attack.get_actor_location()} route {route.get_actor_location()} '
        f'strongpoint {strongpoint.get_actor_location()}')


def shoot(name):
    S['busy'] = True
    try:
        unreal.SystemLibrary.execute_console_command(None, 'HighResShot 1920x1080')
    finally:
        S['busy'] = False
    log(f'requested HighResShot {name}')


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 20.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            place_all(world)
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 6.0:
            shoot('summary')
            S['phase'], S['mark'] = 'select', S['t']
        elif phase == 'select' and S['t'] - S['mark'] > 6.0:
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors(S['detail'])
            S['phase'], S['mark'] = 'settle_selected', S['t']
        elif phase == 'settle_selected' and S['t'] - S['mark'] > 4.0:
            shoot('selected')
            S['phase'], S['mark'] = 'wait_shot', S['t']
        elif phase == 'wait_shot' and S['t'] - S['mark'] > 6.0:
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
