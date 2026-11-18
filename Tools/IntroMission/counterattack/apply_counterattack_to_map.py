"""One-shot (-ExecutePythonScript) in a throwaway DebugGame editor opened on Ger_IntroMission: creates / updates the
counterattack data asset + mission Blueprint, adds Missions[3] (rewire_ridge_chain), rebuilds navigation and saves the
map with a backup and a size check. Log lines start with COUNTERAPPLY. 2026-11-18.
"""
import os
import shutil
import sys
import traceback

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission')
sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack')
import rewire_ridge_chain  # noqa: E402
import setup_counterattack_assets  # noqa: E402

UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_IntroMission.umap'
BACKUP = 'D:/UE5Projects/RTS_Survival/Saved/Counterattack/Ger_IntroMission_before_counterattack.umap'


def log(msg):
    unreal.log_warning('COUNTERAPPLY ' + str(msg))


try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('world ' + world.get_path_name())
    if 'Ger_IntroMission' not in world.get_path_name():
        raise RuntimeError('wrong map open')
    for line in setup_counterattack_assets.setup():
        log(line)
    for line in rewire_ridge_chain.rewire(world):
        log(line)
    for line in rewire_ridge_chain.describe(world):
        log(line)
    os.makedirs(os.path.dirname(BACKUP), exist_ok=True)
    shutil.copy2(UMAP, BACKUP)
    size_before = os.path.getsize(UMAP)
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission')
    size_after = os.path.getsize(UMAP)
    log(f'save {ok} size {size_before} -> {size_after} ({size_after / size_before:.3f})')
    if size_after < 0.9 * size_before:
        log('SIZE DROP: restoring the backup')
        shutil.copy2(BACKUP, UMAP)
    log('DONE')
except Exception:  # noqa: BLE001
    log('EXCEPTION ' + traceback.format_exc())
