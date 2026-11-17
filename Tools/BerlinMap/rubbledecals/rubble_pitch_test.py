"""Throwaway test (RTS_ToCopySmall, nothing saved): do the rubble decals vanish under a pitched camera?

Four decals (cells 0, 1, 5, 11) in a row, all-mips textures + finish_loading_before_screenshot, then for two cameras
(top-down from 24 m; pitch -48 from 45 m like the critic views) a warm-up, an on and an off picture.
"""
import json
import os
import time

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/pitch_test/'
FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/'
NAMES = {0: 'BrickSpillFan', 1: 'BrickHeapSpread', 5: 'ConcreteRebar', 11: 'SettsTornUp'}
S = {'t': 0.0, 'phase': 'wait', 'log': [], 'decals': []}


def log(m):
    S['log'].append(m)
    unreal.log('RUBPITCH ' + m)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def gz(w, x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit is not None and hit.to_tuple()[0] else 0.0


def place():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for i, (cell, name) in enumerate(sorted(NAMES.items())):
        y = -1050 + i * 700
        a = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(0, y, gz(w, 0, y) + 30), unreal.Rotator(0, -90, 30 * i))
        a.decal.set_decal_material(unreal.load_asset(FOLDER + 'MI_BerlinRubbleDecal_%02d_%s' % (cell, name)))
        a.decal.set_editor_property('decal_size', unreal.Vector(180.0, 250.0, 250.0))
        a.decal.set_editor_property('decal_color', unreal.LinearColor(1.0, 0.8, 1.0 if i % 2 else 0.0, 0.95))
        a.decal.set_editor_property('sort_order', 3)
        a.decal.set_editor_property('fade_screen_size', 0.001)
        S['decals'].append(a)
    z = gz(w, 0, 0)
    S['views'] = [('top', unreal.Vector(0, 0, z + 2400), unreal.Rotator(0, -90, 0)),
                  ('pitch', unreal.Vector(-4000, 0, z + 4500), unreal.Rotator(0, -48, 0))]
    for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS'):
        unreal.load_asset(FOLDER + n).set_editor_property('never_stream', True)
    for a in actors().get_all_level_actors():
        if a.get_class().get_name() == 'DirectionalLight':
            a.light_component.set_light_function_material(None)
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    S['shots'] = [(v, tag) for v in S['views'] for tag in ('warm', 'on', 'off')]


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        if S['phase'] == 'wait' and S['t'] > 15.0:
            os.makedirs(ROOT, exist_ok=True)
            place()
            S['phase'], S['mark'] = 'shots', time.time() + 60.0
        elif S['phase'] == 'shots' and time.time() - S['mark'] > 10.0:
            if S.get('cam'):
                actors().destroy_actor(S['cam'])
                S['cam'] = None
            if not S['shots']:
                json.dump(S['log'], open(ROOT + 'log.json', 'w'), indent=1)
                S['phase'] = 'done'
                unreal.SystemLibrary.quit_editor()
                return
            (name, loc, rot), tag = S['shots'].pop(0)
            for a in S['decals']:
                a.set_is_temporarily_hidden_in_editor(tag == 'off')
            S['mark'] = time.time()
            cam = actors().spawn_actor_from_class(unreal.CameraActor, loc, rot)
            cam.camera_component.set_field_of_view(60)
            cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
            S['cam'] = cam
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, '%s%s_%s.png' % (ROOT, name, tag), cam)
            log('shot %s %s' % (name, tag))
    except Exception as e:  # noqa: BLE001
        log('ERROR %r' % e)
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        S['busy'] = False


unreal.register_slate_post_tick_callback(tick)
log('loaded')
