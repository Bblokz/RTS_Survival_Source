"""Throwaway-editor picture of AEnemyWaveSpawner's editor visuals (never run in the shared editor).
Places an attack and a patrol spawner on the startup map, selects the attack one (shows the point diamonds),
aims the level viewport down at them and takes a HighResShot; then starts PIE and takes the same view in game, where
the spawners must not show. Run:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Writes Saved/EnemyWaveSpawnerTest/editor_shot_log.json; the editor picture lands in Saved/Screenshots/WindowsEditor,
the PIE one is Saved/EnemyWaveSpawnerTest/pie_view.png.
"""
import json
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/EnemyWaveSpawnerTest/editor_shot_log.json'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': []}
PIE_SHOT = 'D:/UE5Projects/RTS_Survival/Saved/EnemyWaveSpawnerTest/pie_view.png'
CAMERA_OFFSET = unreal.Vector(3250, -4900, 9000)
CAMERA_ROTATION = unreal.Rotator(0, -75, 90)


def log(message):
    S['log'].append(message)
    unreal.log('WAVESPAWNERSHOT ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def ground_at(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return unreal.Vector(x, y, hit.to_tuple()[4].z if hit else 0.0)


def place_spawners(world):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    attack = actors.spawn_actor_from_class(unreal.EnemyWaveSpawner, ground_at(world, 0, 0))
    attack.set_editor_property('spawner_id', 'Berlin_NorthAttack')
    attack.set_editor_property('spawn_points', [unreal.Vector(0, 0, 0), unreal.Vector(0, 700, 0)])
    attack.set_editor_property('waypoints', [unreal.Vector(2500, 600, 0), unreal.Vector(4500, -900, 0),
                                             unreal.Vector(6500, 0, 0)])
    patrol = actors.spawn_actor_from_class(unreal.EnemyWaveSpawner, ground_at(world, 0, -4500))
    patrol.set_editor_property('spawner_id', 'Berlin_ParkPatrol')
    patrol.set_editor_property('behaviour', unreal.EnemyWaveSpawnerBehaviour.PATROL_WAVE)
    patrol.set_editor_property('waypoints', [unreal.Vector(2500, 0, 0), unreal.Vector(4500, 1200, 0),
                                             unreal.Vector(3500, -1800, 0)])
    actors.set_selected_level_actors([attack])
    log(f'attack at {attack.get_actor_location()} patrol at {patrol.get_actor_location()}')
    S['centre'] = attack.get_actor_location()
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        S['centre'] + CAMERA_OFFSET, CAMERA_ROTATION)
    # Duplicated into the PIE world; the PIE picture is taken from it.
    actors.spawn_actor_from_class(unreal.CameraActor, S['centre'] + CAMERA_OFFSET, CAMERA_ROTATION)


def report_pie_components(world):
    for spawner in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.EnemyWaveSpawner):
        parts = []
        for component in spawner.get_components_by_class(unreal.PrimitiveComponent):
            parts.append(f'{component.get_class().get_name()} visible={component.is_visible()} '
                         f'hidden_in_game={component.get_editor_property("hidden_in_game")}')
        log(f'PIE {spawner.get_name()} actor_hidden={spawner.get_editor_property("hidden")} parts={parts}')


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 20.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            place_spawners(world)
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 6.0:
            S['phase'], S['mark'] = 'shot', S['t']
            S['busy'] = True
            try:
                unreal.SystemLibrary.execute_console_command(None, 'HighResShot 1920x1080')
            finally:
                S['busy'] = False
            log('requested HighResShot')
        elif phase == 'shot' and S['t'] - S['mark'] > 8.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie' and S['t'] - S['mark'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is None:
                log('pie never started')
                finish()
                return
            unreal.GameplayStatics.set_game_paused(world, False)
            report_pie_components(world)
            placed_at = S['centre'] + CAMERA_OFFSET
            camera = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor),
                         key=lambda c: (c.get_actor_location() - placed_at).length())
            unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)
            S['phase'], S['mark'] = 'pie_shot', S['t']
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, PIE_SHOT, camera=camera)
            finally:
                S['busy'] = False
            log(f'PIE shot with camera {camera.get_name()} in {camera.get_world().get_name()}')
        elif phase == 'pie_shot' and S['t'] - S['mark'] > 8.0:
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
