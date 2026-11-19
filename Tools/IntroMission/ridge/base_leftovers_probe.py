"""Throwaway DebugGame editor probe on Ger_IntroMission (2026-11-18): what Soviet base buildings leave behind when
they die, plus a fresh terrain export and an actor survey for the ridge rework. Nothing is saved.

    -ExecCmds="py <this file>"   (startup map Ger_IntroMission, as in the other IntroMission tests)
Writes Saved/RidgeRework/leftovers.json, survey.json and Saved/IntroRear/terrain/now_full_height.r16 (+ layers).
"""
import json
import os
import sys
import time

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear')
OUT = 'D:/UE5Projects/RTS_Survival/Saved/RidgeRework/'
BOX = (-3600.0, 5600.0, -6200.0, 7200.0)          # x0, x1, y0, y1: the base plateau with its rim
SURVEY = (-9000.0, 12000.0, -10000.0, 10000.0)
PLACE = None   # BuildingPlacement = GameTraceChannel8 (python enum names differ per version)
for _name in ('ECC_GAME_TRACE_CHANNEL8', 'GAME_TRACE_CHANNEL8', 'BUILDING_PLACEMENT', 'BUILDINGPLACEMENT', 'ECC_BUILDING_PLACEMENT'):
    if hasattr(unreal.CollisionChannel, _name):
        PLACE = getattr(unreal.CollisionChannel, _name)
if PLACE is None:
    unreal.log_warning('LEFTOVERS channel names: ' + ', '.join(n for n in dir(unreal.CollisionChannel) if not n.startswith('_')))
S = {'t': 0.0, 'phase': 'load', 'mark': 0.0, 'log': []}


def log(m):
    S['log'].append(m)
    unreal.log_warning('LEFTOVERS ' + m)


def inside(loc, box):
    return box[0] < loc.x < box[1] and box[2] < loc.y < box[3]


def owner(a):
    c = a.get_component_by_class(unreal.RTSComponent)
    return c.call_method('GetOwningPlayer') if c is not None else -1


def blocking_components(a):
    rows = []
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        try:
            enabled = c.get_collision_enabled()
            if enabled == unreal.CollisionEnabled.NO_COLLISION:
                continue
            resp = c.get_collision_response_to_channel(PLACE) if PLACE is not None else 'unknown'
            if PLACE is not None and 'IGNORE' in str(resp).upper():
                continue
            try:
                lo, hi = c.get_local_bounds()
                e = unreal.Vector((hi.x - lo.x) / 2, (hi.y - lo.y) / 2, (hi.z - lo.z) / 2)
            except Exception:  # noqa: BLE001
                e = unreal.Vector(0, 0, 0)
            mesh = ''
            if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                mesh = c.static_mesh.get_name()
            rows.append({'comp': c.get_name(), 'cls': c.get_class().get_name(), 'mesh': mesh, 'resp': str(resp),
                         'visible': c.is_visible(), 'ext': [round(e.x), round(e.y), round(e.z)]})
        except Exception as exc:  # noqa: BLE001
            rows.append({'comp': c.get_name(), 'err': str(exc)})
    return rows


def actors(world):
    return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)


def survey(world):
    rows = []
    for a in actors(world):
        loc = a.get_actor_location()
        if not inside(loc, SURVEY) or isinstance(a, (unreal.Pawn, unreal.Controller)):
            continue
        o, e = a.get_actor_bounds(False)
        parent = a.get_class()
        chain = []
        while parent is not None and len(chain) < 6:
            chain.append(parent.get_name())
            parent = unreal.SystemLibrary.get_super_class(parent) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
        rows.append({'label': a.get_actor_label(), 'cls': a.get_class().get_name(), 'chain': chain,
                     'loc': [round(loc.x), round(loc.y), round(loc.z)], 'yaw': round(a.get_actor_rotation().yaw),
                     'bo': [round(o.x), round(o.y), round(o.z)], 'be': [round(e.x), round(e.y), round(e.z)],
                     'folder': str(a.get_folder_path())})
    return rows


def tick(dt):
    S['t'] += dt
    el = S['t'] - S['mark']
    try:
        if S['phase'] == 'load' and S['t'] > 15:
            os.makedirs(OUT, exist_ok=True)
            import rear_lib
            rear_lib.export_region('now_full', (105, 75, 825, 585))
            ew = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            rows = survey(ew)
            json.dump(rows, open(OUT + 'survey.json', 'w'), indent=0)
            log(f'survey {len(rows)} actors; terrain exported')
            mgr = [a for a in actors(ew) if a.get_class().get_name() == 'BP_MissionManager_C'][0]
            mgr.set_editor_property('missions', [])
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'pie', S['t']
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if S['phase'] == 'pie' and world is not None and el > 10:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc:
                pc.call_method('ClickedStartGameButton')
                S['phase'], S['mark'] = 'kill', S['t']
            return
        if S['phase'] == 'kill' and el > 5:
            S['before'] = {a.get_name() for a in actors(world)}
            targets = []
            for a in actors(world):
                loc = a.get_actor_location()
                if not inside(loc, BOX) or isinstance(a, (unreal.Pawn, unreal.Controller)):
                    continue
                name = a.get_class().get_name()
                if owner(a) == 2 or 'Fuel' in name or 'Barrel' in name:
                    if a.get_component_by_class(unreal.HealthComponent) is None:
                        continue
                    targets.append(a)
            S['targets'] = {a.get_name(): {'cls': a.get_class().get_name(), 'label': a.get_actor_label(),
                                           'loc': [round(v) for v in (a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)],
                                           'owner': owner(a)} for a in targets}
            S['target_actors'] = targets
            for a in targets:
                unreal.GameplayStatics.apply_damage(a, 1.0e7, None, None, unreal.DamageType)
            log(f'killed {len(targets)} targets')
            S['phase'], S['mark'] = 'wait', S['t']
            return
        if S['phase'] == 'wait' and el > 35:
            result = {'targets': S['targets'], 'still_there': {}, 'new_actors': []}
            alive_names = {a.get_name(): a for a in actors(world)}
            for name, info in S['targets'].items():
                a = alive_names.get(name)
                if a is not None:
                    result['still_there'][name] = dict(info, blocking=blocking_components(a),
                                                       life=a.call_method('GetLifeSpan'))
            for a in actors(world):
                if a.get_name() in S['before'] or not inside(a.get_actor_location(), (BOX[0] - 2000, BOX[1] + 2000, BOX[2] - 2000, BOX[3] + 2000)):
                    continue
                if isinstance(a, (unreal.Pawn, unreal.Controller)):
                    continue
                loc = a.get_actor_location()
                result['new_actors'].append({'name': a.get_name(), 'cls': a.get_class().get_name(),
                                             'loc': [round(loc.x), round(loc.y), round(loc.z)],
                                             'life': a.call_method('GetLifeSpan'), 'blocking': blocking_components(a)})
            json.dump(result, open(OUT + 'leftovers.json', 'w'), indent=1)
            log(f"still there {len(result['still_there'])}, new {len(result['new_actors'])}")
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
    except Exception:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log('probe loaded')
