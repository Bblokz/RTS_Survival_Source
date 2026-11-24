"""Game-camera pictures at the camera limits of Ger_Berlin (PIE in the throwaway editor; exec through the skirt
command server after berlin_skirt_survey.py). Set SHOT_TAG ('before' / 'after') first.

The camera pawn is clamped to the fog-of-war square (+-500 m on both axes); the player turns it freely in yaw and by
+-18 degrees in pitch (RTSCameraDeveloperSettings), and zooms out to an arm of 5200. Every spot is shot at the widest
zoom. 'out' spots look outward with the flattest pitch (worst case for seeing past the end of the map).
Writes Saved/MapSkirt/Ger_Berlin/shots_<tag>/<name>.png and probe_<tag>.txt (real pawn / arm / camera values).
"""
import os
import time

SHOT_TAG = globals().get('SHOT_TAG', 'before')
SHOTS = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/shots_%s/' % SHOT_TAG
os.makedirs(SHOTS, exist_ok=True)
L = 50000.0
ARM = 5200.0
PITCH = 18.0
# name, x, y, yaw, pitch, fog-of-war post process on
SPOTS = [
    ('edge_N_default', L, 0.0, 0.0, 0.0, False), ('edge_S_default', -L, 0.0, 0.0, 0.0, False),
    ('edge_E_default', 0.0, L, 0.0, 0.0, False), ('edge_W_default', 0.0, -L, 0.0, 0.0, False),
    ('corner_NE_default', L, L, 0.0, 0.0, False), ('corner_NW_default', L, -L, 0.0, 0.0, False),
    ('corner_SE_default', -L, L, 0.0, 0.0, False), ('corner_SW_default', -L, -L, 0.0, 0.0, False),
    ('edge_N_out', L, 0.0, 0.0, PITCH, False), ('edge_S_out', -L, 0.0, 180.0, PITCH, False),
    ('edge_E_out', 0.0, L, 90.0, PITCH, False), ('edge_W_out', 0.0, -L, -90.0, PITCH, False),
    ('corner_NE_out', L, L, 45.0, PITCH, False), ('corner_NW_out', L, -L, -45.0, PITCH, False),
    ('corner_SE_out', -L, L, 135.0, PITCH, False), ('corner_SW_out', -L, -L, -135.0, PITCH, False),
    ('river_W_mouth', 13500.0, -L, -90.0, PITCH, False), ('river_W_mouth_down', 13500.0, -L, -90.0, 0.0, False),
    ('river_N_mouth', L, 23400.0, 0.0, PITCH, False), ('river_N_mouth_down', L, 23400.0, 0.0, 0.0, False),
    ('rail_E_end', -10845.0, 47000.0, 90.0, 0.0, False), ('rail_E_end_out', -10845.0, L, 90.0, PITCH, False),
    ('join_hill_E', -20000.0, L, 90.0, 0.0, False), ('join_flat_S', -L, 30000.0, 180.0, 0.0, False),
    ('fog_edge_E_out', 0.0, L, 90.0, PITCH, True), ('fog_edge_N_out', L, 0.0, 0.0, PITCH, True),
    ('fog_edge_W_default', 0.0, -L, 0.0, 0.0, True), ('fog_corner_SE_out', -L, L, 135.0, PITCH, True),
]
SPOTS = globals().get('SHOT_SPOTS', SPOTS)
unreal.SystemLibrary.execute_console_command(world(), 't.IdleWhenNotForeground 0')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.editor_request_begin_play()
_e = {'n': 0, 't': time.time(), 'i': 0, 'moved': False, 'shot': False, 'fog': True, 'probe': []}


def _ground(gw, x, y):
    hit = unreal.SystemLibrary.line_trace_single(gw, unreal.Vector(x, y, 60000), unreal.Vector(x, y, -30000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None or not hit.to_tuple()[0]:
        return 0.0
    return hit.to_tuple()[4].z


def _finish():
    with open(SHOTS + '../probe_%s.txt' % SHOT_TAG, 'w') as f:
        f.write('\n'.join(_e['probe']))
    les.editor_request_end_play()
    return True


def _defer():
    _e['n'] += 1
    gw = ues.get_game_world()
    if gw is None:
        if time.time() - _e['t'] > 240:
            print('no game world after 240 s')
            return True
        return False
    if unreal.GameplayStatics.is_game_paused(gw) and _e['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    gt = unreal.GameplayStatics.get_time_seconds(gw)
    if gt < 12.0:
        return False
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    if pawn is None:
        if time.time() - _e['t'] > 300:
            print('no pawn')
            return _finish()
        return False
    arm = pawn.get_component_by_class(unreal.SpringArmComponent)
    cam = unreal.GameplayStatics.get_player_camera_manager(gw, 0)
    if 'z0' not in _e:
        p = pawn.get_actor_location()
        _e['z0'] = p.z - _ground(gw, p.x, p.y)
        _e['probe'].append('pawn %s at %s rot %s; height over ground %.0f' % (pawn.get_class().get_name(), p, pawn.get_actor_rotation(), _e['z0']))
        if arm:
            _e['probe'].append('arm length %.0f relative rotation %s' % (arm.get_editor_property('target_arm_length'),
                                                                     arm.get_editor_property('relative_rotation')))
        _e['probe'].append('camera at %s rot %s fov %.1f' % (cam.get_camera_location(), cam.get_camera_rotation(), cam.get_fov_angle()))
        print('\n'.join(_e['probe']))
    if _e['i'] >= len(SPOTS):
        return _finish()
    name, x, y, yaw, pitch, fog = SPOTS[_e['i']]
    if fog != _e['fog']:
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial %d' % (1 if fog else 0))
        _e['fog'] = fog
    z = _ground(gw, x, y) + _e['z0']
    if not _e['shot']:
        # the unattended editor edge-scrolls the camera: pin the pawn every tick until the shot
        pawn.set_actor_location(unreal.Vector(x, y, z), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, pitch, yaw), False)
        if arm:
            arm.set_editor_property('target_arm_length', ARM)
    if not _e['moved']:
        _e['moved'], _e['mark'] = True, gt
        return False
    if gt - _e['mark'] > 2.5 and not _e['shot']:
        loc, rot = cam.get_camera_location(), cam.get_camera_rotation()
        line = '%s pawn (%.0f,%.0f,%.0f) cam (%.0f,%.0f,%.0f) pitch %.1f yaw %.1f' % (
            name, x, y, z, loc.x, loc.y, loc.z, rot.pitch, rot.yaw)
        _e['probe'].append(line)
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + SHOTS + name + '.png')
        _e['shot'], _e['mark'] = True, gt
        return False
    if _e['shot'] and gt - _e['mark'] > 1.2:
        _e['i'] += 1
        _e['moved'], _e['shot'] = False, False
    return False
