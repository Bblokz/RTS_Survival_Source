"""Throwaway full-editor job: place the Ore mission content into Ger_2ndOreMountains and save.

Order: Soviet sites (Base B radio mast first), convoy content (routes pick up the mast), exploration objects; then
the blocking RebuildNavigation, a size check against the backup, and the save. Launch from PowerShell:
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
The editor quits when this returns, so everything is synchronous.
"""
import importlib
import os
import sys

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission'
sys.path.append(HERE)
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains.umap'


def log(text):
    unreal.log_warning('ORE_PLACE ' + str(text))


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != 'Ger_2ndOreMountains':
        log('ABORT wrong map ' + world.get_name())
        return
    before = os.path.getsize(UMAP)
    import place_soviet_sites as sites
    import place_convoy_content as convoy
    import place_exploration as explore
    for module in (sites, convoy, explore):
        importlib.reload(module)
    for name, call in (('sites', sites.run), ('convoy', convoy.run), ('explore', explore.place_all)):
        try:
            log(f'{name}: {call()}')
        except Exception as error:  # keep going: each script is idempotent and can be re-run on its own
            log(f'{name} FAILED: {error!r}')
    try:
        log('sites check: ' + str(sites.check()))
    except Exception as error:
        log(f'sites check FAILED: {error!r}')
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    after = os.path.getsize(UMAP)
    log(f'saved={saved} size {before} -> {after} ({after / max(before, 1):.3f})')


main()
