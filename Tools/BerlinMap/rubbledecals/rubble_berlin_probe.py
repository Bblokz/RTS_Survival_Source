"""Throwaway diagnostic on Ger_Berlin (nothing saved): do freshly spawned decals of ANY material draw as dark squares
there, or only the rubble decals? Four decals in a row on open ground, one top-down picture after
finish_loading_before_screenshot, another 120 s later.
  y -900 MI_BerlinRubbleDecal_01   y -300 MI_LandscapeImpactDecal (cell 15)   y 300 MI_Decal_Crater_Dirt_01   y 900 _15 grit
"""
import json
import os
import time

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/berlin_probe/'
SPOT = (13141.7 + 1500.0, 16824.7)
MATS = [(-900, '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/MI_BerlinRubbleDecal_01_BrickHeapSpread', None),
        (-300, '/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_LandscapeImpactDecal', (15.5 / 16.0, 0.5, 1, 1)),
        (300, '/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_01', None),
        (900, '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/MI_BerlinRubbleDecal_15_GritSpill', None)]
S = {'t': 0.0, 'phase': 'wait', 'log': []}


def log(m):
    S['log'].append(m)
    unreal.log('RUBPROBE ' + m)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def lz(w, x, y):
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if 'Landscape' in t[9].get_class().get_name():
            return t[4].z
        ignore.append(t[9])
    return 0.0


def place():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for dy, path, col in MATS:
        x, y = SPOT[0], SPOT[1] + dy
        a = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, lz(w, x, y) + 30), unreal.Rotator(0, -90, 0))
        a.decal.set_decal_material(unreal.load_asset(path))
        a.decal.set_editor_property('decal_size', unreal.Vector(180.0, 250.0, 250.0))
        a.decal.set_editor_property('fade_screen_size', 0.0)
        if col:
            a.decal.set_editor_property('decal_color', unreal.LinearColor(*col))
        tex = [p.get_name() for p in []]
        log('placed %s at %s' % (path.split('/')[-1], (x, y)))
    z = lz(w, SPOT[0], SPOT[1])
    cam = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(SPOT[0], SPOT[1], z + 2400), unreal.Rotator(0, -90, 0))
    cam.camera_component.set_field_of_view(60)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
    S['cam'] = cam
    for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS'):
        t = unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/' + n)
        log('%s size %s lod_bias %s never_stream %s group %s' % (n, t.blueprint_get_size_x(), t.get_editor_property('lod_bias'),
            t.get_editor_property('never_stream'), t.get_editor_property('lod_group')))


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        if S['phase'] == 'wait' and S['t'] > 20.0:
            os.makedirs(ROOT, exist_ok=True)
            place()
            S['phase'], S['t0'] = 'shot1', time.time()
        elif S['phase'] == 'shot1' and time.time() - S['t0'] > 20.0:
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            S['phase'] = 'shot1b'
            S['t1'] = time.time()
        elif S['phase'] == 'shot1b' and time.time() - S['t1'] > 5.0:
            S['phase'] = 'shot2'
            S['t2'] = time.time()
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + 'a_early.png', S['cam'])
            log('shot early')
        elif S['phase'] == 'shot2' and time.time() - S['t2'] > 120.0:
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            S['phase'] = 'shot2b'
            S['t3'] = time.time()
        elif S['phase'] == 'shot2b' and time.time() - S['t3'] > 5.0:
            S['phase'] = 'end'
            S['t4'] = time.time()
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + 'b_late.png', S['cam'])
            log('shot late')
        elif S['phase'] == 'end' and time.time() - S['t4'] > 8.0:
            json.dump(S['log'], open(ROOT + 'log.json', 'w'), indent=1)
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
    except Exception as e:  # noqa: BLE001
        log('ERROR %r' % e)
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        S['busy'] = False


unreal.register_slate_post_tick_callback(tick)
log('loaded')
