"""In-game RTS-camera pictures (PIE in the throwaway editor, fog-of-war post process off, camera pawn moved to each
spot). Usage from a fol_cmd request:

    SPOTS = [('name', x, y, yaw), ...]; TAG = 'r1'
    exec(open(FOL + 'fol_shots.py').read())     -> sets _defer; writes Saved/IntroFoliage/shots/<TAG>_<name>.png
"""
import os
import time

import unreal

SHOT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/shots/'
os.makedirs(SHOT_DIR, exist_ok=True)
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.SystemLibrary.execute_console_command(_ues.get_editor_world(), 't.IdleWhenNotForeground 0')
_les.editor_request_begin_play()
_s = {'n': 0, 't': time.time(), 'stage': 1, 'i': 0, 'moved': False, 'shot': False}
_SPOTS = list(SPOTS)
_TAG = TAG
_RES = globals().get('RES', '1600x900')


def _gz2(gw, x, y):
    hit = unreal.SystemLibrary.line_trace_single(gw, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 150.0


def _shots_defer():
    _s['n'] += 1
    now = time.time()
    gw = _ues.get_game_world()
    if gw is None:
        if now - _s['t'] > 240:
            print('PIE never started')
            return True
        return False
    if unreal.GameplayStatics.is_game_paused(gw) and _s['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s['stage'] == 1 and game_t > 12.0:
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
        _s['stage'], _s['mark'] = 2, game_t
        return False
    if _s['stage'] == 2:
        i = _s['i']
        if i >= len(_SPOTS):
            _les.editor_request_end_play()
            print('shots done', len(_SPOTS))
            return True
        name, x, y, yaw = _SPOTS[i]
        if not _s['moved']:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, _gz2(gw, x, y) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            _s['moved'], _s['mark'] = True, game_t
            return False
        if game_t - _s['mark'] > 3.0 and not _s['shot']:
            unreal.SystemLibrary.execute_console_command(
                gw, 'HighResShot %s filename=%s%s_%s.png' % (_RES, SHOT_DIR, _TAG, name))
            _s['shot'], _s['mark'] = True, game_t
            return False
        if _s['shot'] and game_t - _s['mark'] > 1.5:
            _s['i'] += 1
            _s['moved'], _s['shot'] = False, False
    if now - _s['t'] > 900:
        _les.editor_request_end_play()
        print('timeout at spot', _s['i'])
        return True
    return False


_defer = _shots_defer
