"""Second check after ridge_apply (2026-11-18): zoomed-out RTS-camera pictures of the new cliffs, and the fuel storage
wreck timeline (kill only Rus_Building_FuelStorageLarge, log the BP_Dest_RusFuelLarge wreck every second for 45 s).
Writes Saved/RidgeRework/verify2.json + shot2_*.png.
"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/RidgeRework/'
SHOTS = {'rear_extension_east_edge': (7300.0, 3000.0, 6500.0), 'north_rim': (-1500.0, 6200.0, 6000.0),
         'hollow_rim': (-2600.0, 3600.0, 6000.0), 'se_mound': (6800.0, -4600.0, 6000.0),
         'west_ramp_kept': (-5200.0, 0.0, 6500.0), 'east_climb_kept': (5600.0, -1300.0, 5500.0)}
import os
if os.environ.get('RIDGE_SHOTS'):
    SHOTS = json.loads(os.environ['RIDGE_SHOTS'])
SHOTS_ONLY = bool(os.environ.get('RIDGE_SHOTS'))
S = {'t': 0.0, 'phase': 'load', 'mark': 0.0, 'res': {'timeline': [], 'shots': []}}


def log(m):
    unreal.log_warning('RIDGEV2 ' + m)


def gw():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def pawn(world):
    return unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()


def aim(world, x, y, arm):
    p = pawn(world)
    for sa in p.get_components_by_class(unreal.SpringArmComponent):
        sa.set_editor_property('target_arm_length', arm)
    cam = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    loc, rot = cam.get_camera_location(), cam.get_camera_rotation()
    f = rot.get_forward_vector()
    if f.z >= -0.05:
        p.set_actor_location(unreal.Vector(x, y, p.get_actor_location().z), False, True)
        return
    t = (300.0 - loc.z) / f.z
    hit = unreal.Vector(loc.x + f.x * t, loc.y + f.y * t, 300.0)
    pl = p.get_actor_location()
    p.set_actor_location(unreal.Vector(pl.x + (x - hit.x), pl.y + (y - hit.y), pl.z), False, True)


def tick(dt):
    S['t'] += dt
    el = S['t'] - S['mark']
    try:
        if S['phase'] == 'load' and S['t'] > 15:
            ew = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            mgr = [a for a in unreal.GameplayStatics.get_all_actors_of_class(ew, unreal.Actor)
                   if a.get_class().get_name() == 'BP_MissionManager_C'][0]
            mgr.set_editor_property('missions', [])
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'pie', S['t']
            return
        world = gw()
        if S['phase'] == 'pie' and world is not None and el > 10:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc:
                pc.call_method('ClickedStartGameButton')
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                S['phase'], S['mark'], S['k'] = 'shots', S['t'], 0
            return
        if S['phase'] == 'shots':
            names = list(SHOTS)
            if S['k'] >= len(names):
                if SHOTS_ONLY:
                    json.dump(S['res'], open(OUT + 'verify2.json', 'w'), indent=1)
                    S['phase'] = 'done'
                    unreal.SystemLibrary.quit_editor()
                    return
                S['phase'], S['mark'] = 'kill', S['t']
                return
            name = names[S['k']]
            if el < 2.0:
                aim(world, *SHOTS[name])          # twice: the arm length settles over a frame or two
            elif el > 5.0 and not S.get('sent'):
                unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1600x900 filename={OUT}shot2_{name}.png')
                S['res']['shots'].append(name)
                S['sent'] = True
            elif el > 7.0:
                S['k'] += 1
                S['sent'] = False
                S['mark'] = S['t']
            return
        if S['phase'] == 'kill':
            fuel = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                    if a.get_class().get_name() == 'Rus_Building_FuelStorageLarge_C']
            log('fuel storages %d' % len(fuel))
            for a in fuel:
                unreal.GameplayStatics.apply_damage(a, 1.0e7, None, None, unreal.DamageType)
            S['phase'], S['mark'], S['next'] = 'watch', S['t'], 0.0
            return
        if S['phase'] == 'watch':
            if el >= S['next']:
                S['next'] += 1.0
                wrecks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                          if a.get_class().get_name() == 'BP_Dest_RusFuelLarge_C']
                S['res']['timeline'].append([round(el, 1), len(wrecks), [round(w.call_method('GetLifeSpan'), 1) for w in wrecks]])
            if el > 45.0:
                tl = S['res']['timeline']
                seen = [r[0] for r in tl if r[1] > 0]
                S['res']['wreck_first_seen'] = seen[0] if seen else None
                S['res']['wreck_last_seen'] = seen[-1] if seen else None
                log('wreck seen from %s to %s s after the kill' % (S['res']['wreck_first_seen'], S['res']['wreck_last_seen']))
                json.dump(S['res'], open(OUT + 'verify2.json', 'w'), indent=1)
                S['phase'] = 'done'
                unreal.SystemLibrary.quit_editor()
    except Exception:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log('loaded')
