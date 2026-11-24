"""-ExecutePythonScript in a throwaway DebugGame editor opened on Ger_IntroMission (2026-11-19): imports the layer
weights from paint_gen.py over the full landscape grid (paint only, heights untouched) and saves the map.
Aborts when the paint on the map no longer matches the export paint_gen planned from. Log lines start with PAINTAPPLY.
"""
import os
import shutil
import sys
import traceback
from array import array

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear')
import rear_lib  # noqa: E402

GRID = (105, 75, 825, 585)
T = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/'
SRC = 'D:/UE5Projects/RTS_Survival/Saved/IntroPaint/'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_IntroMission.umap'
BACKUP = SRC + 'Ger_IntroMission_before_paint.umap'
LAYERS = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']


def log(m):
    unreal.log_warning('PAINTAPPLY ' + str(m))


def read8(path):
    a = array('B')
    with open(path, 'rb') as handle:
        a.frombytes(handle.read())
    return a


try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if 'Ger_IntroMission' not in world.get_path_name():
        raise RuntimeError('wrong map open')
    rear_lib.export_region('paint_check', GRID)
    bad = 0
    for n in LAYERS:
        now, plan = read8(T + 'paint_check_%s.r8' % n), read8(T + 'now_full_%s.r8' % n)
        bad += sum(1 for a, b in zip(now, plan) if abs(a - b) > 3)
    log('paint check: %d layer values differ from the planning export' % bad)
    if bad > 200:
        raise RuntimeError('the paint changed since the planning export: re-export and re-run paint_gen')
    for n in LAYERS:
        shutil.copy2(SRC + 'paint_full_%s.r8' % n, T + 'paint_full_%s.r8' % n)
    log('import %s' % rear_lib.import_region('paint_full', GRID, heights=False))
    shutil.copy2(UMAP, BACKUP)
    size_before = os.path.getsize(UMAP)
    # the editor rebuilds the navmesh on load; saving before that finished drops the tiles (umap ~30% smaller)
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    ok =unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission')
    size_after = os.path.getsize(UMAP)
    log('save %s size %d -> %d (%.3f)' % (ok, size_before, size_after, size_after / size_before))
    if size_after < 0.9 * size_before:
        log('SIZE DROP: restoring the backup')
        shutil.copy2(BACKUP, UMAP)
    log('DONE')
except Exception:  # noqa: BLE001
    log('EXCEPTION ' + traceback.format_exc())
