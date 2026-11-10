"""Throwaway-editor test for Refinery1 after its collision was rebuilt (64 convex hulls).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one. The project's EditorStartupMap is
StartGame (main-menu battles), so override it:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

1. Collision traces for the rays in Saved/WallChunks/Refinery/r09_coll.json (mesh space) -> stored as 'final', so
   collision_gap.py can compare them with the visible surface.
2. Debug hits at real collision points on the -X, -Y and +Y faces (75-105 mm), then HighResShot pictures.
"""
import json
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Refinery/'
OUT = ROOT + 'refinery1_headless_result.json'
BP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Zone_Refinery1.Rus_Zone_Refinery1_C'
TEST_MAP = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
SPOT = unreal.Vector(0, 0, 0)
# Mesh-space shots: start point, direction, calibre. Rows at 2-6 m on the main concrete faces.
SHOTS = [
    ((-2700, -900, 300), (1, 0, 0), 105), ((-2700, 0, 500), (1, 0, 0), 88), ((-2700, 900, 250), (1, 0, 0), 75),
    ((-500, -2700, 300), (0, 1, 0), 88), ((600, -2700, 450), (0, 1, 0), 75),
    ((-600, 2500, 300), (0, -1, 0), 88),
]
VIEWS = {'west': (-3600, -1400, 900), 'west_close': (-3300, 0, 450), 'south': (-1000, -4200, 900),
         'north': (-1000, 4000, 900)}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'views': list(VIEWS.items())}


def log(message):
    S['log'].append(message)
    unreal.log('REF1TEST ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def find(world, class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def mesh_of(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'BuildingMeshComponent'][0]


def measure_and_shoot(world):
    refinery = find(world, 'Rus_Zone_Refinery1_C')[0]
    mesh = mesh_of(refinery)
    transform = mesh.get_world_transform()
    log(f'mesh {mesh.static_mesh.get_name()} at {refinery.get_actor_location()}')
    data = json.load(open(ROOT + 'r09_coll.json'))
    hits = []
    for ray in data['rays']:
        start = transform.transform_location(unreal.Vector(ray[0], ray[1], ray[2]))
        end = transform.transform_location(unreal.Vector(ray[0] + ray[3] * ray[6], ray[1] + ray[4] * ray[6],
                                                         ray[2] + ray[5] * ray[6]))
        result = mesh.line_trace_component(start, end, False, False, False)
        hits.append((result[0] - start).length() if result else None)
    data['meshes']['final'] = hits
    json.dump(data, open(ROOT + 'r09_coll.json', 'w'))
    log(f'final collision hits {sum(1 for h in hits if h is not None)} of {len(hits)}')
    holes = refinery.get_component_by_class(unreal.WallHoleDamageComponent)
    health = refinery.get_component_by_class(unreal.HealthComponent)
    health.set_current_health(health.get_max_health() * 0.3)
    for start, direction, calibre in SHOTS:
        world_start = transform.transform_location(unreal.Vector(*start))
        world_end = transform.transform_location(unreal.Vector(start[0] + direction[0] * 4000,
                                                               start[1] + direction[1] * 4000,
                                                               start[2] + direction[2] * 4000))
        result = mesh.line_trace_component(world_start, world_end, False, False, False)
        if not result:
            log(f'shot {start} missed the collision')
            continue
        local = transform.inverse_transform_location(result[0])
        removed = holes.debug_fire_test_hit(result[0], unreal.Vector(*direction), float(calibre))
        log(f'shot {start} hit collision at local ({local.x:.0f},{local.y:.0f},{local.z:.0f}) removed {removed:.2f} m2')
    log(f'holes {holes.get_hole_count()} slot4 {mesh.get_material(4).get_name()}')


def aim_camera(world, eye_local):
    refinery = find(world, 'Rus_Zone_Refinery1_C')[0]
    # The map has cameras of its own: take the one this script placed near the refinery.
    placed_at = SPOT + unreal.Vector(-4000, 0, 1500)
    camera = min(find(world, 'CameraActor'), key=lambda c: (c.get_actor_location() - placed_at).length()
                 if not S.get('camera') else (0 if c == S['camera'] else 1))
    S['camera'] = camera
    # Placed in the level before PIE, so it is static until made movable.
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    centre = refinery.get_actor_location() + unreal.Vector(-300, 0, 500)
    eye = refinery.get_actor_location() + unreal.Vector(*eye_local)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, centre), True)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)
    mesh = mesh_of(refinery)
    log(f'camera {camera.get_name()} at {camera.get_actor_location()} looking at {centre}; refinery hidden='
        f'{refinery.get_editor_property("hidden")} mesh visible={mesh.get_editor_property("visible")} '
        f'hidden_in_game={mesh.get_editor_property("hidden_in_game")}')


def tick(delta):
    S['t'] += delta
    # take_high_res_screenshot keeps ticking Slate while it renders; nothing may move until it returns.
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            # The map comes from the command line's EditorStartupMap override; loading a map from this tick crashes.
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            ground = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(SPOT.x, SPOT.y, 20000),
                                                            unreal.Vector(SPOT.x, SPOT.y, -20000),
                                                            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                            unreal.DrawDebugTrace.NONE, True)
            if ground:
                SPOT.z = ground.to_tuple()[4].z
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            refinery = actors.spawn_actor_from_class(unreal.load_class(None, BP), SPOT)
            # Player-owned, so fog of war doesn't hide it from the camera.
            refinery.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', 1)
            actors.spawn_actor_from_class(unreal.CameraActor, SPOT + unreal.Vector(-4000, 0, 1500))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                measure_and_shoot(world)
                S['phase'], S['mark'] = 'shots', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'shots':
            world = game_world()
            if not S['views']:
                if S['t'] - S['mark'] > 4.0:
                    finish()
                return
            if S['t'] - S['mark'] > 3.0:
                name, eye = S['views'].pop(0)
                aim_camera(world, eye)
                S['pending'] = name
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 2.5 and 'pending' in S:
            name = S.pop('pending')
            # Switch phase first: the screenshot call ticks Slate again, which re-enters this function.
            S['phase'], S['mark'] = 'shots', S['t']
            # Rendered from this script's camera: the RTS camera pawn takes the view target back between frames.
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}Refinery1/headless_{name}.png',
                                                                  camera=S['camera'])
            finally:
                S['busy'] = False
            log(f'shot picture {name}')
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
