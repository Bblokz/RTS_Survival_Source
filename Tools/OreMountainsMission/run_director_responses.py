"""Throwaway full-editor job: fill ORE_PressureDirector's tier and fuel responses in Ger_2ndOreMountains and save.

Order: blocking RebuildNavigation (so the nav leg checks have data), place_director_responses.run() and check(), then
RebuildNavigation again, the save and a size check. It saves only when check() passes; run() problems (uneven ground,
a tree at a post, a nav leg) are logged, not blocking. A shrunk file after the save is logged as a WARNING.
Launch from PowerShell (never Git Bash: it rewrites /Game/... arguments):
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
The editor quits when this returns, so everything is synchronous. A test copy under /Game/RTS_Survival/Maps/Tmp/ is
accepted too. Run it AFTER run_ore_placement.py: place_convoy_content.py resets the director's tier responses.
"""
import importlib
import os
import sys

import unreal

HERE = "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission"
CONTENT = "D:/UE5Projects/RTS_Survival/Content/"
sys.path.append(HERE)


def log(text):
    unreal.log_warning("ORE_DIRECTOR_RUN " + str(text))


def umap_file(package):
    return CONTENT + package[len("/Game/"):] + ".umap"


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    package = world.get_outermost().get_name()
    if not (package.endswith("/Ger_2ndOreMountains") or package.startswith("/Game/RTS_Survival/Maps/Tmp/")):
        log("ABORT wrong map " + package)
        return
    path = umap_file(package)
    before = os.path.getsize(path)
    import place_director_responses as pdr
    importlib.reload(pdr)
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    try:
        report = pdr.run()
        result = pdr.check()
    except Exception as error:
        log("FAILED, not saved: %r" % error)
        return
    log("run problems: %s" % report["problems"])
    if not result["ok"]:
        log("check failed, not saved: %s" % result["problems"])
        return
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    after = os.path.getsize(path)
    ratio = after / max(before, 1)
    log("saved=%s size %d -> %d (%.3f)%s" % (saved, before, after, ratio,
                                             "  WARNING: the map shrank, check the navmesh" if ratio < 0.95 else ""))


main()
