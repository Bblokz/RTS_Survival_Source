"""Throwaway test (RTS_ToCopySmall, nothing saved): why do the Berlin rubble decals draw as dark opaque squares?

Four cell-1 decals in a row (y = -900, -300, 300, 900), top-down camera, pictures at 90 s and 300 s after placing:
  A default DecalColor (orient test setup)      B DecalColor (1, 0.7, 1, 0.9)
  C DecalColor (1, 0.7, 0, 0.9) (mirrored)      D default colour, fade_screen_size 0.001 + sort order 3
Textures are NOT forced to full mips. Logs texture sizes / streaming state each picture.
"""
import json
import os

import unreal

ROOT = os.environ.get('RUBCOLOR_OUT', 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/color_test/')
FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/'
S = {'t': 0.0, 'phase': 'wait_load', 'log': [], 'shots': [90.0, 300.0]}


def log(m):
    S['log'].append(m)
    unreal.log('RUBCOLOR ' + m)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ground_z(w, x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit is not None and hit.to_tuple()[0] else 0.0


def place():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mi = unreal.load_asset(FOLDER + 'MI_BerlinRubbleDecal_01_BrickHeapSpread')
    rows = [(-900, None, False), (-300, (1, 0.7, 1, 0.9), False), (300, (1, 0.7, 0, 0.9), False), (900, None, True)]
    for y, col, extra in rows:
        z = ground_z(w, 0, y)
        a = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(0, y, z + 30), unreal.Rotator(0, -90, 0))
        c = a.decal
        c.set_decal_material(mi)
        c.set_editor_property('decal_size', unreal.Vector(180.0, 250.0, 250.0))
        if col:
            c.set_editor_property('decal_color', unreal.LinearColor(*col))
        if extra:
            c.set_editor_property('fade_screen_size', 0.001)
            c.set_editor_property('sort_order', 3)
        else:
            c.set_editor_property('fade_screen_size', 0.0)
    if os.environ.get('RUBCOLOR_CVAR'):
        unreal.SystemLibrary.execute_console_command(w, os.environ['RUBCOLOR_CVAR'])
        log('cvar ' + os.environ['RUBCOLOR_CVAR'])
    S['z'] = ground_z(w, 0, 0)
    cam = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, S['z'] + 2400), unreal.Rotator(0, -90, 0))
    cam.camera_component.set_field_of_view(60)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
    S['cam'] = cam
    for a in actors().get_all_level_actors():
        if a.get_class().get_name() == 'DirectionalLight':
            a.light_component.set_light_function_material(None)


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            os.makedirs(ROOT, exist_ok=True)
            place()
            S['phase'], S['t0'] = 'shots', S['t']
        elif S['phase'] == 'shots':
            if not S['shots']:
                json.dump(S['log'], open(ROOT + 'log.json', 'w'), indent=1)
                S['phase'] = 'done'
                unreal.SystemLibrary.quit_editor()
                return
            if S['t'] - S['t0'] >= S['shots'][0]:
                at = S['shots'].pop(0)
                info = []
                for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS'):
                    t = unreal.load_asset(FOLDER + n)
                    info.append('%s %s' % (n[-3:], t.blueprint_get_size_x()))
                log('shot at %.0f s textures %s' % (at, info))
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + 'at_%03d.png' % at, S['cam'])
    except Exception as e:  # noqa: BLE001
        log('ERROR %r' % e)
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        S['busy'] = False
    if S['t'] > 600 and S['phase'] != 'done':
        log('timeout')
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log('loaded')
