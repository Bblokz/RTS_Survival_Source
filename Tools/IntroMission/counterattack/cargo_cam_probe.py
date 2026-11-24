"""Diagnostic: where is the PIE camera, and does moving the player pawn move it? (no saves)"""
import time
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
C = {'st': 0, 't': time.time()}


def info(gw, tag):
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    pc = unreal.GameplayStatics.get_player_controller(gw, 0)
    cm = unreal.GameplayStatics.get_player_camera_manager(gw, 0)
    vt = pc.get_view_target() if pc else None
    arm = pawn.get_component_by_class(unreal.SpringArmComponent) if pawn else None
    print(tag, 'pawn', pawn.get_class().get_name() if pawn else None, pawn.get_actor_location() if pawn else None,
          'viewtarget', vt.get_name() if vt else None, 'cam', cm.get_camera_location(), cm.get_camera_rotation(),
          'fov', cm.get_fov_angle(), 'arm', arm.get_editor_property('target_arm_length') if arm else None,
          arm.get_world_rotation() if arm else None)


def step():
    gw = ues.get_game_world()
    if C['st'] == 0:
        ew = ues.get_editor_world()
        for a in unreal.GameplayStatics.get_all_actors_of_class(ew, unreal.Actor):
            if a.get_class().get_name() == 'BP_MissionManager_C':
                C['m'], C['missions'] = a, list(a.get_editor_property('missions'))
                a.set_editor_property('missions', [])
        les.editor_request_begin_play()
        C['st'], C['t'] = 1, time.time()
        return False
    if gw is not None and unreal.GameplayStatics.is_game_paused(gw):
        unreal.GameplayStatics.set_game_paused(gw, False)
    if C['st'] == 1 and gw is not None and time.time() - C['t'] > 10:
        pc = unreal.GameplayStatics.get_player_controller(gw, 0)
        try:
            pc.call_method('ClickedStartGameButton')
        except Exception as exc:  # noqa
            print('no start', exc)
        info(gw, 'BEFORE')
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        pawn.set_actor_location(unreal.Vector(19000.0, 12500.0, 950.0), False, False)
        C['st'], C['t'] = 2, time.time()
        return False
    if C['st'] == 2 and time.time() - C['t'] > 2:
        info(gw, 'AFTER2S')
        C['st'], C['t'] = 3, time.time()
        return False
    if C['st'] == 3 and time.time() - C['t'] > 3:
        info(gw, 'AFTER5S')
        les.editor_request_end_play()
        C['st'] = 4
        return False
    if C['st'] == 4 and gw is None:
        C['m'].set_editor_property('missions', C['missions'])
        return True
    if time.time() - C['t'] > 200:
        les.editor_request_end_play()
        C['st'] = 4
    return False


_defer = step
