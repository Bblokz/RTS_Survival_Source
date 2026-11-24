"""Throwaway DebugGame editor check after ridge_apply.py (2026-11-18), -ExecCmds="py <this>" on Ger_IntroMission.

Editor world: per nav agent, paths that must exist (ramps, the new rear area, the counterattack spawns to the HQ).
PIE (missions cut): RTS-camera pictures of the new cliffs and the rear extension, then every Soviet actor with health
in the base is killed: no loot pile / metal barrels may spawn, the fuel wreck must appear and be gone after 10 s, and
nothing new may block building placement except the barracks rubble (no placement collision).
Writes Saved/RidgeRework/verify.json and shots verify_*.png.
"""
import json
import math
import os
import time

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/RidgeRework/'
BOX = (-3600.0, 5600.0, -6200.0, 7200.0)
HQ = (1450.0, 300.0)
PATHS = {   # name: (start, end) that must connect
    'hq_to_rear_extension': ((1450.0, 300.0), (6000.0, 3200.0)),
    'west_ramp_to_hq': ((-6500.0, 300.0), HQ),
    'south_slope_to_hq': ((500.0, -6600.0), HQ),
    'hub_to_hq': ((6500.0, -700.0), HQ),
    'centre_spawn_to_hq': ((8400.0, -1500.0), HQ),
    'station_spawn_to_hq': ((19953.0, 15593.0), HQ),
    'pad1_to_hq': ((20446.0, -16317.0), HQ),
    'truck_spawn_to_crane': ((4640.0, -5200.0), (4640.0, -1717.0)),
}
SHOTS = {'rear_extension': (6000.0, 3000.0), 'north_rim': (-1500.0, 6200.0), 'hollow_rim': (-2600.0, 3500.0),
         'se_mound': (6800.0, -4500.0), 'west_ramp': (-5000.0, 0.0)}
PLACE = None
for _n in ('ECC_GAME_TRACE_CHANNEL8', 'GAME_TRACE_CHANNEL8', 'BUILDING_PLACEMENT'):
    if hasattr(unreal.CollisionChannel, _n):
        PLACE = getattr(unreal.CollisionChannel, _n)
S = {'t': 0.0, 'phase': 'load', 'mark': 0.0, 'log': [], 'res': {'paths': {}, 'shots': [], 'checks': {}}}


def log(m):
    S['log'].append(m)
    unreal.log_warning('RIDGEVERIFY ' + m)


def check(name, ok, detail=''):
    S['res']['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    passed = sum(1 for c in S['res']['checks'].values() if c['ok'])
    S['res']['log'] = S['log']
    json.dump(S['res'], open(OUT + 'verify.json', 'w'), indent=1)
    log(f"RESULT {passed}/{len(S['res']['checks'])}")
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def ew():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def gw():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -5000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 300.0


def nav_paths(world):
    navs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == 'RecastNavMesh']
    for nav in navs:
        agent = nav.get_actor_label() or nav.get_name()
        for name, (s, e) in PATHS.items():
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world, unreal.Vector(s[0], s[1], ground(world, *s) + 50.0), unreal.Vector(e[0], e[1], ground(world, *e) + 50.0), nav)
            pts = p.get_editor_property('path_points') if p else []
            ok = bool(pts) and math.hypot(pts[-1].x - e[0], pts[-1].y - e[1]) < 700.0
            length = sum(math.hypot(b.x - a.x, b.y - a.y) for a, b in zip(pts[:-1], pts[1:])) / 100.0
            S['res']['paths'].setdefault(name, {})[agent] = {'ok': ok, 'len_m': round(length)}
    for name, per in S['res']['paths'].items():
        check('nav_' + name, all(v['ok'] for v in per.values()), per)


def owner(a):
    c = a.get_component_by_class(unreal.RTSComponent)
    return c.call_method('GetOwningPlayer') if c is not None else -1


def inside(loc, box):
    return box[0] < loc.x < box[1] and box[2] < loc.y < box[3]


def blocking(a):
    rows = []
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        if c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        if PLACE is not None and 'IGNORE' in str(c.get_collision_response_to_channel(PLACE)).upper():
            continue
        rows.append(c.get_name())
    return rows


def camera_to(world, x, y):
    pawn = unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
    if pawn is not None:
        z = pawn.get_actor_location().z
        pawn.set_actor_location(unreal.Vector(x - 2600.0, y, z), False, True)


def tick(dt):
    S['t'] += dt
    el = S['t'] - S['mark']
    try:
        if S['phase'] == 'load' and S['t'] > 15:
            world = ew()
            unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
            nav_paths(world)
            mgr = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
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
                S['phase'], S['mark'], S['shot'] = 'shots', S['t'], 0
            return
        if S['phase'] == 'shots':
            names = list(SHOTS)
            k = S['shot']
            if k >= len(names):
                S['phase'], S['mark'] = 'kill', S['t']
                return
            if el < 1.0:
                camera_to(world, *SHOTS[names[k]])
            elif el > 4.0 and not S.get('shot_sent'):
                unreal.SystemLibrary.execute_console_command(
                    world, f'HighResShot 1600x900 filename={OUT}verify_{names[k]}.png')
                S['res']['shots'].append(names[k])
                S['shot_sent'] = True
            elif el > 6.0:
                S['shot'] += 1
                S['shot_sent'] = False
                S['mark'] = S['t']
            return
        if S['phase'] == 'kill':
            allact = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            S['before'] = {a.get_name() for a in allact}
            targets = [a for a in allact if inside(a.get_actor_location(), BOX)
                       and not isinstance(a, (unreal.Pawn, unreal.Controller)) and owner(a) == 2
                       and a.get_component_by_class(unreal.HealthComponent) is not None]
            log('killing %d: %s' % (len(targets), sorted({a.get_class().get_name() for a in targets})))
            for a in targets:
                unreal.GameplayStatics.apply_damage(a, 1.0e7, None, None, unreal.DamageType)
            S['phase'], S['mark'] = 'after5', S['t']
            return
        if S['phase'] == 'after5' and el > 5.0:
            new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                   if a.get_name() not in S['before']]
            wrecks = [a for a in new if a.get_class().get_name() == 'BP_Dest_RusFuelLarge_C']
            check('fuel_wreck_appears', len(wrecks) >= 1, '%d wrecks, life %s' % (
                len(wrecks), [round(w.call_method('GetLifeSpan'), 1) for w in wrecks]))
            S['phase'], S['mark'] = 'after16', S['t']
            return
        if S['phase'] == 'after16' and el > 11.0:
            new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                   if a.get_name() not in S['before'] and inside(a.get_actor_location(), BOX)]
            classes = sorted({a.get_class().get_name() for a in new})
            check('fuel_wreck_gone_after_10s', not any(a.get_class().get_name() == 'BP_Dest_RusFuelLarge_C' for a in new))
            check('no_loot_piles', not any(c in ('ScavObj_VehicleParts2_C', 'Resource_Metal_Barrels_C') for c in classes),
                  classes)
            blockers = [(a.get_class().get_name(), blocking(a)) for a in new if blocking(a)
                        and not isinstance(a, (unreal.Pawn, unreal.Controller))]
            check('no_new_placement_blockers', not blockers, blockers)
            finish()
    except Exception:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        finish()


unreal.register_slate_post_tick_callback(tick)
log('verify loaded')
