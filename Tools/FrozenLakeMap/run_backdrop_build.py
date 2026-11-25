"""Headless job: build the north backdrop + horizon skirt into Ger_FrozenLake and save (fl_backdrop_build.py).

    python Source/Tools/FrozenLakeMap/fl_backdrop_gen.py        (data first; the picked concept C)
    back up Content/RTS_Survival/Maps/GerMissions/Ger_FrozenLake.umap to Saved/FrozenLake/backup/
Then from PowerShell (Git Bash mangles /Game paths):
    UnrealEditor.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake
      -ExecutePythonScript=D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/run_backdrop_build.py
      -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
      -abslog=D:/UE5Projects/RTS_Survival/Saved/FrozenLake/logs/backdrop_build.log
The editor quits right after this returns. Re-runnable: it replaces the backdrop actor and its assets.
"""
import importlib
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap")
import fl_map_build as fmb  # noqa: E402
import fl_backdrop_build as fbb  # noqa: E402

importlib.reload(fmb)
importlib.reload(fbb)


def log(text):
    unreal.log_warning("FL_BACKDROP " + str(text))


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != "Ger_FrozenLake":
        log("ABORT wrong map open: %s" % world.get_name())
        return
    fmb.load_layout()
    fbb.SAVE = True
    fbb.build_material()
    actor = fbb.place()
    log("placed %s" % actor.get_actor_label())
    # Opening the map starts a navmesh rebuild; saving mid-build would drop the tiles (and the ice nav areas).
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("saved=%s" % saved)


main()
