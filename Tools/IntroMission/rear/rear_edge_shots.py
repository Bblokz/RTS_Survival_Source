"""Pictures at the camera's east limit (pawn pushed to x 25400, arm pulled out), fog of war ON then OFF, to check that
the landscape extension hides the end of the world. exec after rear_lib + rear_build. Writes shots/edge_*.png."""
import time
unreal.SystemLibrary.execute_console_command(world(), 't.IdleWhenNotForeground 0')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.editor_request_begin_play()
SPOTS = [('edge_road', 25400.0, -1300.0), ('edge_station', 25400.0, 12000.0), ('edge_depot', 25400.0, -18000.0)]
PLAN_SHOTS = [(n + ('_fog' if fog else '_nofog'), x, y, fog) for fog in (True, False) for (n, x, y) in SPOTS]
_e = {'n': 0, 't': time.time(), 'i': 0, 'moved': False, 'shot': False, 'fogoff': False, 'started': False}


def _defer():
    _e['n'] += 1
    gw = ues.get_game_world()
    if gw is None:
        return time.time() - _e['t'] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _e['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    gt = unreal.GameplayStatics.get_time_seconds(gw)
    if gt < 12.0:
        return False
    if _e['i'] >= len(PLAN_SHOTS):
        les.editor_request_end_play()
        return True
    name, x, y, fog = PLAN_SHOTS[_e['i']]
    if not fog and not _e['fogoff']:
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
        _e['fogoff'] = True
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    if not _e['moved']:
        pawn.set_actor_location(unreal.Vector(x, y, 144.0), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        arm = pawn.get_component_by_class(unreal.SpringArmComponent)
        if arm:
            arm.set_editor_property('target_arm_length', 5200.0)
        _e['moved'], _e['mark'] = True, gt
        return False
    if not _e['shot']:
        # the unattended editor edge-scrolls the camera (cursor in a corner): pin the pawn every tick until the shot
        pawn.set_actor_location(unreal.Vector(x, y, 144.0), False, False)
    if gt - _e['mark'] > 3.0 and not _e['shot']:
        cam = unreal.GameplayStatics.get_player_camera_manager(gw, 0)
        loc = cam.get_camera_location()
        arm = pawn.get_component_by_class(unreal.SpringArmComponent)
        print(name, 'pawn (%.0f,%.0f) cam (%.0f,%.0f,%.0f) arm %s' % (pawn.get_actor_location().x, pawn.get_actor_location().y,
              loc.x, loc.y, loc.z, arm.get_editor_property('target_arm_length') if arm else None))
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + SHOTS + name + '.png')
        _e['shot'], _e['mark'] = True, gt
        return False
    if _e['shot'] and gt - _e['mark'] > 1.5:
        _e['i'] += 1
        _e['moved'], _e['shot'] = False, False
    return False
