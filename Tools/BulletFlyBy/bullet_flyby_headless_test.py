"""Throwaway-editor test of the bullet fly-bys (UBulletFlyByAudioSubsystem). Never run in the shared editor:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -LogCmds="LogBulletFlyBy Verbose" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>

Starts PIE, then fires fake tracers across the camera focus with RTS.Audio.TestBulletFlyBy:
  1. 5 tracers 0.15 s apart, 200 cm beside the focus -> expect Scheduled + Played lines (budget allows 3 at once,
     refilling at 6/s, so some may be dropped as over budget).
  2. 3 tracers 2000 cm beside the focus -> outside PassRadius, expect no new lines.
The log lines (BFTEST + LogBulletFlyBy) are the result.
"""
import unreal

S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0}


def log(message):
    unreal.log('BFTEST ' + message)


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def run(world, command):
    log('exec ' + command)
    unreal.SystemLibrary.execute_console_command(world, command)


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 10.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                controller = unreal.GameplayStatics.get_player_controller(world, 0)
                pawn = controller.get_controlled_pawn() if controller else None
                camera = controller.player_camera_manager if controller else None
                log('pawn %s at %s, camera at %s, view target %s' % (
                    pawn.get_name() if pawn else None, pawn.get_actor_location() if pawn else None,
                    camera.get_camera_location() if camera else None,
                    controller.get_view_target().get_name() if controller else None))
                run(world, 'RTS.Audio.TestBulletFlyBy 5 200')
                S['phase'], S['mark'] = 'near', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                S['phase'] = 'quit'
        elif phase == 'near' and S['t'] - S['mark'] > 3.0:
            log('--- far pass, expect nothing ---')
            run(game_world(), 'RTS.Audio.TestBulletFlyBy 3 2000')
            S['phase'], S['mark'] = 'far', S['t']
        elif phase == 'far' and S['t'] - S['mark'] > 3.0:
            log('done')
            S['phase'] = 'quit'
        elif phase == 'quit':
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
    except Exception as error:  # noqa: BLE001 - any failure ends the run
        log('error %r' % error)
        S['phase'] = 'quit'


unreal.register_slate_post_tick_callback(tick)
