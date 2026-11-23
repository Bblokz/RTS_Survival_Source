"""Throwaway-editor job: wire the Frozen Lake stages into Ger_FrozenLake and save (see wire_fl_stages.py).

Launch from PowerShell (Git Bash mangles /Game paths):
  UnrealEditor.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake
    -ExecutePythonScript=D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission/run_wire_fl_stages.py
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
    -abslog=D:/UE5Projects/RTS_Survival/Saved/FrozenLake/wire_stages.log
The editor quits right after this returns, so everything is synchronous. Set DRY_RUN = True to only log.
It can also run in an open editor that has Ger_FrozenLake loaded (execute the file there).
"""
import importlib
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission')
import wire_fl_stages as wfs  # noqa: E402

importlib.reload(wfs)

DRY_RUN = False
MAP_NAME = 'Ger_FrozenLake'


def log(text):
    unreal.log_warning('FL_WIRE ' + text)


def describe_tagged(world):
    """Logs each tagged kit actor with its class chain, so we can see whether it can be destroyed."""
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        tags = [str(t) for t in actor.tags if str(t).startswith('FL_')]
        if not tags:
            continue
        chain, cls = [], actor.get_class()
        while cls and len(chain) < 6:
            chain.append(cls.get_name())
            cls = unreal.SystemLibrary.get_super_class(cls) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
        log(f'tagged {actor.get_actor_label()} {tags} class {" < ".join(chain)}')


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != MAP_NAME:
        log(f'ABORT wrong map open: {world.get_name()}')
        return
    log('before: ' + ' | '.join(wfs.describe(world)))
    if DRY_RUN:
        log('dry run, nothing changed')
        return
    changes = wfs.wire(world)
    log('changes: ' + ('; '.join(changes) if changes else 'none'))
    log('after: ' + ' | '.join(wfs.describe(world)))
    describe_tagged(world)
    # Opening the map starts a navmesh rebuild; saving mid-build would drop the tiles (and the ice nav areas).
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log(f'saved={saved}')


main()
