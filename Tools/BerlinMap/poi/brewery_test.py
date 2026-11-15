"""Throwaway-editor test of the brewery enclave Blueprints (brewery_import.py): shell holes and the remnant death.
Never on Ger_Berlin, never in the shared editor. Launch with brewery_test_run.sh <logname> (same folder).
The map (RTS_ToCopySmall) is never saved; the editor quits at the end.

1. Places BP_Berlin_BreweryHall at x -4500 and BP_Berlin_BreweryVaults at x +4500 (yaw 0: Unreal +Y = east), a camera,
   rebuilds navigation, starts PIE.
2. Pictures of both intact from the game camera (from the east, looking west, 55 deg down).
3. Six labelled Debug_FireTestHit shell holes per piece (105 mm) on brick, copper roof, kettle, tile front, tile
   wainscot, tile lining; logs which slots switched to their _WallHoles material; pictures (game camera + close-ups).
4. Kills each piece (apply_damage 1e7): samples mesh z / tilt / alive every tick, pictures at 0.5, 1.5, 3.5 s and as a
   remnant 8 s after the death. Then more damage to the remnant (must stay, no second death).
Results: Saved/Berlin/brewery/test/result.json + PNGs.
"""
import json
import math

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/brewery/test/'
BP = '/Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave/Blueprints/'
PIECES = {'BreweryHall': -4500.0, 'BreweryVaults': 4500.0}
GAME_EYE = {'BreweryHall': ((0, 4100, 5900), (0, 0, 500)), 'BreweryVaults': ((0, 3600, 5100), (0, 0, 100))}
CLOSE = {'BreweryHall': ((2200, 2600, 1900), (-300, 300, 700)), 'BreweryVaults': ((2600, 2400, 1300), (500, 300, 100))}
# (label, local point cm, shot direction) in the mesh frame (= actor frame at yaw 0)
HITS = {
    'BreweryHall': [
        ('east_wall_brick', (-300, 1040, 450), (0, -1, 0)),
        ('kettle_copper', (-660, 790, 350), (0, -1, 0)),
        ('roof_copper', (-40, -450, 1300), (0.5, 0.0, -0.85)),
        ('north_wall_brick', (360, 300, 600), (-1, 0, 0)),
        ('west_gable_brick', (-340, -1040, 1150), (0, 1, 0)),
        ('head_house_brick', (1060, -250, 200), (-1, 0, 0)),
    ],
    'BreweryVaults': [
        ('tile_front', (1262, 380, 150), (-1, 0, 0)),
        ('tile_wainscot', (-200, -1555, 120), (0, -1, 0)),
        ('vault_crown_brick', (-600, 1400, 250), (0, -0.6, -0.8)),
        ('tile_lining', (0, 1340, 120), (0, 1, 0)),
        ('vault_haunch_brick', (300, -100, 200), (0, -1, 0)),
        ('floor_slab_tile', (500, -830, 280), (0, 0, -1)),
    ],
}
CALIBRE = 105.0
PICTURE_TIMES = [0.5, 1.5, 3.5, 8.0]
SETTLE = 3.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'actors': {}, 'result': {}, 'busy': False}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('BREWERYTEST ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(ROOT + 'result.json', 'w'), indent=1, default=str)
    finally:
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def now(world):
    return unreal.GameplayStatics.get_time_seconds(world)


def alive(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def nav_building(world):
    fn = getattr(unreal.NavigationSystemV1, 'is_navigation_being_built', None)
    try:
        return bool(fn(world)) if fn else False
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def mesh_of(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def holes_of(actor):
    return actor.get_component_by_class(unreal.WallHoleDamageComponent)


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, x in PIECES.items():
        z = ground_z(world, x, 0)
        cls = unreal.load_class(None, f'{BP}BP_Berlin_{key}.BP_Berlin_{key}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 0))
        log(f'placed {key} at ({x}, 0, {z:.0f})')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        for key in PIECES:
            if actor.get_class().get_name() == f'BP_Berlin_{key}_C':
                S['actors'][key] = actor
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    log(f'runtime pieces {sorted(S["actors"])}')


def setup_info(key, actor):
    mesh = mesh_of(actor)
    holes = holes_of(actor)
    info = dict(health=actor.get_editor_property('health'), reduction=actor.get_editor_property('damage_reduction'),
                slots=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())],
                damaged=[(m.get_name() if m else None) for m in holes.get_editor_property('DamagedMaterials')],
                mobility=str(mesh.mobility), collision=str(mesh.get_collision_enabled()),
                object_type=str(mesh.get_collision_object_type()),
                nav_boxes=len([c for c in actor.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock_')]),
                sink_cm=actor.get_editor_property('SinkDeltaZ'), sink_s=actor.get_editor_property('SinkTime'),
                remnant=actor.get_editor_property('LeaveRemnant'), z=mesh.get_world_location().z)
    S['result'].setdefault('setup', {})[key] = info
    log(f'setup {key}: {json.dumps(info, default=str)}')


def look(eye_off, look_off, origin):
    cam = S['camera']
    eye, target = origin + unreal.Vector(*eye_off), origin + unreal.Vector(*look_off)
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_location(eye, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(cam, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False


def fire_hits(key):
    actor = S['actors'][key]
    origin = actor.get_actor_location()
    holes = holes_of(actor)
    out = []
    for label, p, d in HITS[key]:
        world_hit = origin + unreal.Vector(*p)
        removed = holes.debug_fire_test_hit(world_hit, unreal.Vector(*d), CALIBRE)
        out.append((label, round(removed, 3)))
    mesh = mesh_of(actor)
    info = dict(hits=out, hole_count=holes.get_hole_count(),
                slots_after=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())],
                health=actor.get_editor_property('health'))
    S['result'].setdefault('holes', {})[key] = info
    log(f'holes {key}: {json.dumps(info)}')


def sample(world):
    d = S['death']
    actor = S['actors'][d['key']]
    t = now(world) - d['t0']
    if alive(actor):
        mesh = mesh_of(actor)
        rot = actor.get_actor_rotation()
        d['samples'].append((round(t, 2), round(mesh.get_world_location().z, 1), round(rot.pitch, 2), round(rot.roll, 2),
                             round(rot.yaw, 2), mesh.is_visible()))
    else:
        d['samples'].append((round(t, 2), 'gone'))
    return t


def tick(delta):
    S['t'] += delta
    if S['busy'] or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'
            place_all()
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(editor_world, 'RebuildNavigation')
            S['phase'], S['mark'] = 'editor_nav', S['t']
        elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if nav_building(editor_world) and S['t'] - S['mark'] < 120.0:
                return
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                find_runtime(world)
                for key, actor in S['actors'].items():
                    setup_info(key, actor)
                S['queue'] = [('intact', k) for k in PIECES] + [('hits', k) for k in PIECES] + \
                             [('holes_game', k) for k in PIECES] + [('holes_close', k) for k in PIECES] + \
                             [('death', k) for k in PIECES]
                look(*GAME_EYE['BreweryHall'], S['actors']['BreweryHall'].get_actor_location())
                S['phase'], S['mark'] = 'warmup', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'warmup' and S['t'] - S['mark'] > SETTLE:
            picture('_warmup')
            S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'next' and S['t'] - S['mark'] > 2.0:
            if not S['queue']:
                finish()
                return
            what, key = S['queue'].pop(0)
            S['current'] = (what, key)
            actor = S['actors'][key]
            origin = actor.get_actor_location()
            if what == 'hits':
                fire_hits(key)
                S['mark'] = S['t']
                return
            if what == 'holes_close':
                look(*CLOSE[key], origin)
            else:
                look(*GAME_EYE[key], origin)
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > SETTLE:
            what, key = S['current']
            if what == 'death':
                picture(f'{key}_death_before')
                world = game_world()
                actor = S['actors'][key]
                S['death'] = dict(key=key, t0=now(world), samples=[], pics=list(PICTURE_TIMES),
                                  z0=mesh_of(actor).get_world_location().z, yaw0=actor.get_actor_rotation().yaw)
                unreal.GameplayStatics.apply_damage(actor, 1.0e7, None, None, unreal.DamageType)
                log(f'kill {key}')
                S['phase'] = 'dying'
                return
            picture(f'{key}_{what}')
            S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'dying':
            world = game_world()
            d = S['death']
            t = sample(world)
            if d['pics'] and t >= d['pics'][0]:
                picture(f'{d["key"]}_death_{d["pics"].pop(0):.1f}s')
                return
            if not d['pics']:
                actor = S['actors'][d['key']]
                res = dict(samples=d['samples'][::max(1, len(d['samples']) // 25)] + d['samples'][-1:])
                if alive(actor):
                    mesh = mesh_of(actor)
                    rot = actor.get_actor_rotation()
                    res.update(actor_valid=True, z_drop_cm=round(d['z0'] - mesh.get_world_location().z, 1),
                               pitch=round(rot.pitch, 2), roll=round(rot.roll, 2),
                               yaw_change=round((rot.yaw - d['yaw0'] + 180) % 360 - 180, 2),
                               mesh_visible=mesh.is_visible(), collision=str(mesh.get_collision_enabled()),
                               health=actor.get_editor_property('health'),
                               nav_boxes_kept=len([c for c in actor.get_components_by_class(unreal.BoxComponent)
                                                   if c.get_name().startswith('NavBlock_')]))
                    z_before = mesh.get_world_location().z
                    unreal.GameplayStatics.apply_damage(actor, 5000.0, None, None, unreal.DamageType)
                    res['second_damage_kept'] = alive(actor) and abs(mesh.get_world_location().z - z_before) < 1.0
                else:
                    res['actor_valid'] = False
                S['result'].setdefault('deaths', {})[d['key']] = res
                log(f'death {d["key"]}: {json.dumps(res, default=str)[:1500]}')
                S['phase'], S['mark'] = 'next', S['t']
    except Exception as error:  # noqa: BLE001
        import traceback
        log(f'ERROR {phase}: {error!r} {traceback.format_exc()[-600:]}')
        finish()
    if S['t'] > 600.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
