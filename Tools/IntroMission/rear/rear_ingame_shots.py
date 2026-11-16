"""In-game pictures of the rear areas (PIE in the throwaway editor, fog of war post process off, camera pawn moved to
each spot). exec after rear_lib + rear_build. Writes Saved/IntroRear/shots/ig_*.png."""
import time
unreal.SystemLibrary.execute_console_command(world(), 't.IdleWhenNotForeground 0')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.editor_request_begin_play()
_s = {'n': 0, 't': time.time(), 'stage': 1, 'i': 0}
SPOTS = [('ig_hub', 7200.0, -900.0, 0.0), ('ig_ponds', 15200.0, -1300.0, 0.0), ('ig_station', 19700.0, 10600.0, 0.0),
         ('ig_station_north', 20600.0, 17500.0, 0.0), ('ig_saddle', 12300.0, -9300.0, -40.0),
         ('ig_depot', 17600.0, -15200.0, 0.0), ('ig_road_end', 23000.0, -1100.0, 0.0), ('ig_wash', 12000.0, 9500.0, 0.0)]


def _defer():
    _s['n'] += 1
    now = time.time()
    gw = ues.get_game_world()
    if gw is None:
        return now - _s['t'] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s['stage'] == 1 and game_t > 12.0:
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
        _s['stage'], _s['mark'], _s['moved'] = 2, game_t, False
        return False
    if _s['stage'] == 2:
        i = _s['i']
        if i >= len(SPOTS):
            les.editor_request_end_play()
            return True
        name, x, y, yaw = SPOTS[i]
        if not _s['moved']:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, gz(x, y) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            _s['moved'], _s['mark'] = True, game_t
            return False
        if game_t - _s['mark'] > 3.0 and not _s.get('shot'):
            unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + SHOTS + name + '.png')
            _s['shot'], _s['mark'] = True, game_t
            return False
        if _s.get('shot') and game_t - _s['mark'] > 1.5:
            _s['i'] += 1
            _s['moved'], _s['shot'] = False, False
    return False
