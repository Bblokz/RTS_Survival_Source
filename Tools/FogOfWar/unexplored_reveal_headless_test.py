"""Throwaway-editor test for the per-map unexplored fog reveal (MissionManager.bRevealUnexploredFog).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

1. Ticks the checkbox on the map's mission manager, starts PIE, reads MPC_FogOfWar.UnexploredReveal back (proves the
   C++ path MissionManager -> FowManager -> MPC).
2. Pictures of the same view at the C++ value, at 0 (old look) and at the 0.1 maximum.
"""
import json
import os
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/FogOfWar/'
os.makedirs(ROOT, exist_ok=True)
OUT = ROOT + 'unexplored_reveal_result.json'
MPC = '/Game/RTS_Survival/Blueprints/FowSystem/Materials/Collections/MPC_FogOfWar'
REVEAL = 0.06
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': [('cpp', None), ('off', 0.0), ('max', 0.1)]}


def log(message):
    S['log'].append(message)
    unreal.log('FOWTEST ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def place_camera(world):
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    focus = pawn.get_actor_location() if pawn else unreal.Vector(0, 0, 0)
    focus.z = 0.0
    eye = focus + unreal.Vector(-9000, 0, 9000)
    # Placed in the editor world before PIE (spawning from python in PIE is not exposed); static until made movable.
    camera = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
              if a.get_actor_label().startswith('FowTestCam') or a.get_name().startswith('FowTestCam')]
    camera = camera[0] if camera else unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)[-1]
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, focus), True)
    S['camera'] = camera
    log(f'camera at {eye} looking at {focus}')


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            managers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MissionManager)
            log(f'mission managers {[m.get_name() for m in managers]}')
            for manager in managers:
                manager.set_editor_property('reveal_unexplored_fog', True)
                manager.set_editor_property('unexplored_fog_reveal', REVEAL)
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 9000))
            camera.set_actor_label('FowTestCam')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                mpc = unreal.load_asset(MPC)
                value = unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, 'UnexploredReveal')
                log(f'MPC UnexploredReveal in PIE = {value:.3f} (expected {REVEAL})')
                place_camera(world)
                S['phase'], S['mark'] = 'shots', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'shots':
            world = game_world()
            if not S['shots']:
                if S['t'] - S['mark'] > 3.0:
                    finish()
                return
            if S['t'] - S['mark'] > 4.0:
                name, value = S['shots'].pop(0)
                if value is not None:
                    unreal.MaterialLibrary.set_scalar_parameter_value(
                        world, unreal.load_asset(MPC), 'UnexploredReveal', value)
                S['pending'] = name
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 2.0 and 'pending' in S:
            name = S.pop('pending')
            S['phase'], S['mark'] = 'shots', S['t']
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}reveal_{name}.png',
                                                                  camera=S['camera'])
            finally:
                S['busy'] = False
            log(f'shot picture {name}')
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
