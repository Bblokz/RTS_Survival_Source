"""Throwaway DebugGame editor, -ExecCmds="py <this>" on Ger_IntroMission (2026-11-19). NEVER saves (unless
PAINT_SAVE=1, after the in-session check looked right).

The one-shot import (paint_apply.py: import, save, quit without ticking) broke landscape components (blocky squares,
black components). This runs the import with the editor ticking: only the four changed layers (Sand, DesertRockDark,
Weedy, DesertGravel) from Saved/IntroPaint/tick_full_*.r8, waits PAINT_SETTLE real seconds, then takes PIE shots of the
same spots plus a control block (pure sand that only got a 1/255 Weedy trace: allocation without a visible change).
Log lines start with PAINTTICK.
"""
import json
import os
import shutil
import sys
import time

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear')
import rear_lib  # noqa: E402

GRID = (105, 75, 825, 585)
SRC = 'D:/UE5Projects/RTS_Survival/Saved/IntroPaint/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/IntroPaint/'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_IntroMission.umap'
CHANGED = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel']
SETTLE = float(os.environ.get('PAINT_SETTLE', '240'))
SAVE = os.environ.get('PAINT_SAVE', '0') == '1'
ctrl = open(SRC + 'control.txt').read().split()
SHOTS = {'tick_south_of_base': (0.0, -8500.0, 6000.0), 'tick_west_plain': (-20000.0, 2000.0, 6000.0),
         'tick_rear': (12000.0, 5000.0, 6000.0), 'tick_control': (float(ctrl[2]), float(ctrl[3]), 6000.0)}
S = {'t': 0.0, 'phase': 'load', 'k': 0}


def log(m):
    unreal.log_warning('PAINTTICK ' + str(m))


def ew():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def gw():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def aim(world, x, y, arm):
    p = unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
    for sa in p.get_components_by_class(unreal.SpringArmComponent):
        sa.set_editor_property('target_arm_length', arm)
    cam = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    loc, rot = cam.get_camera_location(), cam.get_camera_rotation()
    f = rot.get_forward_vector()
    if f.z >= -0.05:
        return
    t = (300.0 - loc.z) / f.z
    hx, hy = loc.x + f.x * t, loc.y + f.y * t
    pl = p.get_actor_location()
    p.set_actor_location(unreal.Vector(pl.x + (x - hx), pl.y + (y - hy), pl.z), False, True)


def do_import():
    names = rear_lib.LAYER_NAMES
    infos = rear_lib.layer_infos()
    sel = [infos[names.index(n)] for n in CHANGED]
    files = [SRC + 'tick_full_%s.r8' % n for n in CHANGED]
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(rear_lib.landscape(), GRID[0], GRID[1], GRID[2], GRID[3],
                                                                  '', sel, files)
    log('import of %s -> %s' % (CHANGED, ok))


def tick(dt):
    S['t'] += dt
    try:
        if S['phase'] == 'load' and S['t'] > 15:
            do_import()
            S['settle_until'] = time.time() + SETTLE
            S['phase'] = 'settle'
            return
        if S['phase'] == 'settle' and time.time() > S['settle_until']:
            if SAVE:
                shutil.copy2(UMAP, OUT + 'Ger_IntroMission_before_paint_tick.umap')
                before = os.path.getsize(UMAP)
                unreal.SystemLibrary.execute_console_command(ew(), 'RebuildNavigation')
                ok = unreal.EditorLoadingAndSavingUtils.save_map(ew(), '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission')
                after = os.path.getsize(UMAP)
                log('save %s size %d -> %d (%.3f)' % (ok, before, after, after / before))
                if after < 0.9 * before:
                    shutil.copy2(OUT + 'Ger_IntroMission_before_paint_tick.umap', UMAP)
                    log('SIZE DROP: restored')
                S['phase'] = 'done'
                unreal.SystemLibrary.quit_editor()
                return
            mgr = [a for a in unreal.GameplayStatics.get_all_actors_of_class(ew(), unreal.Actor)
                   if a.get_class().get_name() == 'BP_MissionManager_C'][0]
            mgr.set_editor_property('missions', [])
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['pie_at'] = 'pie', time.time()
            return
        if S['phase'] == 'pie' and gw() is not None and time.time() - S['pie_at'] > 15:
            pc = unreal.GameplayStatics.get_player_controller(gw(), 0)
            if pc:
                pc.call_method('ClickedStartGameButton')
                unreal.SystemLibrary.execute_console_command(gw(), 'ShowFlag.PostProcessMaterial 0')
                S['phase'], S['next'] = 'shots', time.time() + 30.0
            return
        if S['phase'] == 'shots':
            names = list(SHOTS)
            if S['k'] >= len(names):
                S['phase'] = 'done'
                log('shots done')
                unreal.SystemLibrary.quit_editor()
                return
            now = time.time()
            name = names[S['k']]
            if now < S['next'] - 6.0:
                return
            if now < S['next'] - 3.0:
                aim(gw(), *SHOTS[name])
                return
            if now >= S['next'] and not S.get('sent'):
                unreal.SystemLibrary.execute_console_command(gw(), 'HighResShot 1600x900 filename=%s%s.png' % (OUT, name))
                S['sent'], S['next2'] = True, now + 3.0
                return
            if S.get('sent') and now > S['next2']:
                S['k'] += 1
                S['sent'] = False
                S['next'] = now + 8.0
    except Exception:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log('loaded, settle %.0f s, save %s' % (SETTLE, SAVE))
