"""Throwaway-editor job: wire the Berlin stages into Ger_Berlin and save (see wire_berlin_stages.py).

Launch from PowerShell (Git Bash mangles /Game paths):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_Berlin
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
The editor quits right after this returns, so everything is synchronous. Set BERLIN_WIRE_DRY_RUN = True to only log.
"""
import importlib
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
import wire_berlin_stages as wbs  # noqa: E402

importlib.reload(wbs)

BERLIN_WIRE_DRY_RUN = False
MAP_NAME = 'Ger_Berlin'


def log(text):
    unreal.log_warning('BERLIN_WIRE ' + text)


def describe_player_start(world):
    """What the map gives the player at the start: PlayerStart actors and any player units placed in the level."""
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        if 'PlayerStart' in name or 'StartLocation' in name or 'Nomadic' in name or 'Spawn' in name:
            log(f'start actor {actor.get_actor_label()} class={name} at {actor.get_actor_location()}')


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != MAP_NAME:
        log(f'ABORT wrong map open: {world.get_name()}')
        return
    log('before: ' + ' | '.join(wbs.describe(world)))
    describe_player_start(world)
    if BERLIN_WIRE_DRY_RUN:
        log('dry run, nothing changed')
        return
    changes = wbs.wire(world)
    log('changes: ' + ('; '.join(changes) if changes else 'none'))
    log('after: ' + ' | '.join(wbs.describe(world)))
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log(f'saved={saved}')


main()
