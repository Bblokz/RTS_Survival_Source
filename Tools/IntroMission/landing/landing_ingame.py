"""In-game check of the landing field (PIE in the throwaway editor, fog-of-war post process off):
a temporary LANDTEST_AirDrop (cargo plane, 1 T-34/76 + 1 Mosin squad) on pad 1 shows the Pe-8 landing on a pad; the
RTS camera pawn sits over the field and takes HighResShots of the approach, landing and unload, then views of the
field. The test actor lives only in the unsaved editor world and is destroyed afterwards (never saved).
Writes Saved/IntroLanding/shots/<TAG>_ig_*.png and ingame.txt (aircraft positions per shot)."""
import time
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
TAG = globals().get('SHOT_TAG', 'r0')
PAD = globals().get('TEST_PAD', (-1850.0, 1250.0))
unreal.SystemLibrary.execute_console_command(world(), 't.IdleWhenNotForeground 0')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

clear_prefix('LANDTEST_')
_x, _y = W(*PAD)
_top = ground_hit(_x, _y)[0]
drop = actors_sub().spawn_actor_from_class(unreal.EnemyAirDrop, unreal.Vector(_x, _y, _top + 20.0), unreal.Rotator(0, 0, 0))
drop.set_actor_label('LANDTEST_AirDrop')
units = drop.get_editor_property('units')
veh = unreal.EnemyPlacementUnit(); veh.set_editor_property('kind', unreal.EnemyPlacementUnitKind.VEHICLE)
veh.set_editor_property('vehicle_subtype', unreal.TankSubtype.TANK_T34_76)
sq = unreal.EnemyPlacementUnit(); sq.set_editor_property('kind', unreal.EnemyPlacementUnitKind.SQUAD)
sq.set_editor_property('squad_subtype', unreal.SquadSubtype.SQUAD_RUS_MOSIN)
e1 = unreal.EnemyAirDropUnitEntry(); e1.set_editor_property('unit_options', [veh]); e1.set_editor_property('count', 1)
e2 = unreal.EnemyAirDropUnitEntry(); e2.set_editor_property('unit_options', [sq]); e2.set_editor_property('count', 1)
units.set_editor_property('base_units', [e1, e2])
drop.set_editor_property('units', units)
dl = drop.get_editor_property('delivery')
dl.set_editor_property('delivery', unreal.EnemyAirDropDelivery.CARGO_PLANE)
dl.set_editor_property('warn_player', False)
drop.set_editor_property('delivery', dl)
tm = drop.get_editor_property('timing')
tm.set_editor_property('first_drop_delay', 1.0)
drop.set_editor_property('timing', tm)
drop.set_editor_property('landing_points', [unreal.Vector(0, 0, 0)])
print('test drop problem: "%s"' % drop.get_configuration_problem() if hasattr(drop, 'get_configuration_problem') else 'n/a')

les.editor_request_begin_play()
_s = {'n': 0, 't': time.time(), 'stage': 1, 'i': 0, 'log': []}
CAM = W(-300, -600)
VIEWS = [('field_a', W(-600, 900), 0.0, None), ('field_b', W(900, -400), 135.0, None), ('gate', W(-1600, 3600), -60.0, None),
         ('wide_a', W(0, 0), 0.0, 2.2), ('wide_b', W(0, 0), 140.0, 2.2), ('wide_c', W(0, 0), -110.0, 1.6)]


def _aircraft(gw):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.AircraftMaster):
        p = a.get_actor_location()
        u, v = L(p.x, p.y)
        out.append('%s u %.0f v %.0f z %.0f yaw %.0f' % (a.get_name(), u, v, p.z, a.get_actor_rotation().yaw))
    return out


def _shot(gw, name):
    unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + SHOTS + '%s_ig_%s.png' % (TAG, name))


def _zoom(gw, factor):
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    arm = pawn.get_component_by_class(unreal.SpringArmComponent)
    if arm is None:
        return
    if 'arm0' not in _s:
        _s['arm0'] = arm.get_editor_property('target_arm_length')
    arm.set_editor_property('target_arm_length', _s['arm0'] * (factor or 1.0))


def _move(gw, xy, yaw):
    pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
    pawn.set_actor_location(unreal.Vector(xy[0], xy[1], _top + 43.0), False, False)
    pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)


def _defer():
    _s['n'] += 1
    now = time.time()
    gw = ues.get_game_world()
    if gw is None:
        if now - _s['t'] > 240:
            print('no game world'); return True
        return False
    if unreal.GameplayStatics.is_game_paused(gw) and _s['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    gt = unreal.GameplayStatics.get_time_seconds(gw)
    if _s['stage'] == 1 and gt > 10.0:
        unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
        _move(gw, CAM, 0.0)
        _s['stage'], _s['mark'], _s['k'] = 2, gt, 0
        return False
    if _s['stage'] == 2:
        # the drop: a shot every 4 s for 72 s, logging aircraft positions
        if gt - _s['mark'] > 4.0:
            _move(gw, CAM, 0.0)
            _shot(gw, 'drop_%02d' % _s['k'])
            _s['log'].append('t %.1f shot drop_%02d: %s' % (gt, _s['k'], '; '.join(_aircraft(gw)) or 'no aircraft'))
            _s['k'] += 1
            _s['mark'] = gt
            if _s['k'] >= globals().get('DROP_SHOTS', 18):
                _s['stage'], _s['i'], _s['moved'] = 3, 0, False
        return False
    if _s['stage'] == 3:
        if _s['i'] >= len(VIEWS):
            open(ROOT + 'ingame.txt', 'w').write('\n'.join(_s['log']))
            les.editor_request_end_play()
            _s['stage'] = 4
            return False
        name, xy, yaw, zoom = VIEWS[_s['i']]
        if not _s['moved']:
            _move(gw, xy, yaw)
            _zoom(gw, zoom)
            _s['moved'], _s['mark'], _s['shot'] = True, gt, False
            return False
        if gt - _s['mark'] > 3.5 and not _s['shot']:
            _shot(gw, name)
            _s['shot'], _s['mark'] = True, gt
            return False
        if _s['shot'] and gt - _s['mark'] > 1.5:
            _s['i'] += 1
            _s['moved'] = False
        return False
    return False


def _finish():
    if _s['stage'] == 4 and ues.get_game_world() is None:
        clear_prefix('LANDTEST_')
        print('\n'.join(_s['log']))
        return True
    _defer_main()
    return False


_defer_main = _defer
_defer = _finish
