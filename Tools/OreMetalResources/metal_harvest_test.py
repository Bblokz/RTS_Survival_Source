"""Harvest-to-empty test (and pictures) of the Ore metal resources in a throwaway DebugGame editor on
Maps/UnitTests/RTS_ToCopySmall. The map is never saved and the editor quits at the end.
Launch: powershell -File run_metal_test.ps1 [-NullRhi] [-Resources "Blutbarren,Gelbguss"]   (takes the flora lock)

Each station is one resource and one Pz I harvester of player 1; the stock Resource_Metal_Barrels is the control.
The test reads what the construction script made of the resource (mesh, collection, amount, scale), then keeps two
harvesters on it until the full amount is gone. The test map has no drop-off building, so a harvester with a full cargo
(75 metal) is removed and replaced; without a renderer the harvest montage notify never fires, so the test calls
UHarvester::OnHarvestAnimationFinished in its place. Then it watches the storage-empty collapse (mesh hidden, collection
set and broken) and the actor destroying itself. With ORE_METAL_RENDER=1 it also takes pictures of every new resource
next to the stock barrels (shots/). Results: Saved/OreMountains/metal/test/result.json
"""
import json
import math
import os

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/test/'
SHOTS = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/shots/'
R = '/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/'
HARVESTER = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer1/Ger_Harvester_Panzer1.Ger_Harvester_Panzer1_C'
RENDER = os.environ.get('ORE_METAL_RENDER', '0') == '1'
RUN = os.environ.get('ORE_METAL_RUN', 'run')
LOOK_ONLY = os.environ.get('ORE_METAL_LOOK_ONLY', '0') == '1'
NAMES = [n for n in os.environ.get('ORE_METAL_RESOURCES', 'Blutbarren').split(',') if n]
STATION_DX = 3200.0
LINE_Y = 6000.0
STOCK = 'Resource_Metal_Barrels'
STATIONS = [('Stock_Barrels', R + STOCK)] + [(n, R + 'OreMountains/Resource_Metal_Ore_' + n) for n in NAMES]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'res': {}, 'slots': {}, 'line': {}, 'result': {}, 'track': {}, 'breaks': {}}


def log(message):
    S['log'].append('%.1f %s' % (S['t'], message))
    unreal.log('METALTEST ' + message)


def finish():
    try:
        os.makedirs(OUT_DIR, exist_ok=True)
        json.dump(dict(log=S['log'], result=S['result']), open(OUT_DIR + 'result_%s.json' % RUN, 'w'), indent=1, default=str)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def alive(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    return t[4].z if t and t[0] else 0.0


def bp_class(path):
    return unreal.load_class(None, '%s.%s_C' % (path, path.split('/')[-1]))


def station_x(i):
    return (i - (len(STATIONS) - 1) / 2.0) * STATION_DX


def prop(obj, *names):
    for n in names:
        try:
            return obj.get_editor_property(n)
        except Exception:  # noqa: BLE001
            continue
    return None


def spawn(world, cls, loc, rot=None, owner=None):
    """Runtime spawn in the PIE world through the project's map-test helper (python has no spawn of its own there, and
    the editor-world spawn needs a real viewport: it dies under -nullrhi)."""
    actor = unreal.MissionMapTestLibrary.map_test_spawn_actor(world, cls, loc, rot or unreal.Rotator(0, 0, 0))
    if actor is not None and owner is not None:
        comp = actor.get_component_by_class(unreal.RTSComponent)
        if comp is not None and prop(comp, 'owning_player') != owner:
            comp.set_editor_property('owning_player', owner)
    return actor


def spawn_all(world):
    log('game map %s render=%s stations=%s' % (world.get_path_name(), RENDER, [k for k, _ in STATIONS]))
    for i, (key, path) in enumerate(STATIONS):
        x = station_x(i)
        S['res'][key] = spawn(world, bp_class(path), unreal.Vector(x, 0, ground_z(world, x, 0)))
        log('%s: resource spawned %s' % (key, S['res'][key] is not None))
    if RENDER:
        # picture pairs: stock barrels left, the new resource right
        for i, name in enumerate(NAMES):
            x = (i - (len(NAMES) - 1) / 2.0) * 4200.0
            for dx, path in ((-550.0, R + STOCK), (450.0, R + 'OreMountains/Resource_Metal_Ore_' + name)):
                spawn(world, bp_class(path), unreal.Vector(x + dx, LINE_Y, ground_z(world, x + dx, LINE_Y)))
            S['line'][name] = unreal.Vector(x, LINE_Y, ground_z(world, x, LINE_Y))
    S['camera'] = spawn(world, unreal.CameraActor, unreal.Vector(0, 9000, 2000))
    S['res'] = {k: v for k, v in S['res'].items() if v is not None}


def comps(actor):
    mesh = None
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name().startswith('Barrels'):
            mesh = c
    gcs = actor.get_components_by_class(unreal.GeometryCollectionComponent)
    return mesh, (gcs[0] if gcs else None), actor.get_component_by_class(unreal.ResourceComponent)


def bounds(comp):
    try:
        o, e, _ = unreal.SystemLibrary.get_component_bounds(comp)
        return [round(o.x), round(o.y), round(o.z), round(e.x), round(e.y), round(e.z)]
    except Exception:  # noqa: BLE001
        return None


def amount(rc):
    v = prop(rc, 'TotalAmount', 'total_amount')
    return int(v) if v is not None else None


def safe(fn):
    try:
        return fn()
    except Exception as error:  # noqa: BLE001
        return 'ERR %r' % (error,)


def setup_report(key):
    actor = S['res'][key]
    mesh, gc, rc = comps(actor)
    nav = actor.get_component_by_class(unreal.NavModifierComponent)
    return dict(cls=safe(lambda: actor.get_class().get_name()), mesh=safe(lambda: mesh.get_editor_property('static_mesh').get_name()),
                material=safe(lambda: [m.get_name() for m in mesh.get_materials()]),
                mesh_scale=safe(lambda: str(mesh.get_editor_property('relative_scale3d'))), mesh_bounds=bounds(mesh),
                mesh_visible=safe(lambda: mesh.is_visible()), gc_rel_location=safe(lambda: str(gc.get_editor_property('relative_location'))),
                gc_visible=safe(lambda: gc.is_visible()), gc_rest_at_start=safe(lambda: str(prop(gc, 'rest_collection'))),
                geo_collection_var=safe(lambda: str(prop(actor, 'Geo Collection', 'geo_collection'))), worth_mlt=prop(actor, 'WorthMlt'),
                base_capacity=prop(actor, 'BaseCapacity'), constructed_amount=amount(rc), amount_per_harvest=prop(rc, 'AmountPerHarvest'),
                resource_type=str(prop(rc, 'ResourceType')), nav_extent=safe(lambda: str(prop(nav, 'failsafe_extent'))),
                health=prop(actor, 'Health'))


def watch_breaks(key):
    """No Chaos break-event callback: a Python callable on OnChaosBreakEvent crashed the editor (access violation in the
    delegate thunk) the first time an event fired, 2026-11-22. The collapse is read from IsRootBroken and the bounds."""
    S['breaks'][key] = None


def state(key):
    actor = S['res'][key]
    if not alive(actor):
        return dict(alive=False)
    mesh, gc, rc = comps(actor)
    d = dict(alive=True, amount=amount(rc), mesh_visible=safe(lambda: mesh.is_visible()))
    if gc is not None and alive(gc):
        rest = prop(gc, 'rest_collection')
        d.update(gc_visible=safe(lambda: gc.is_visible()), gc_rest=safe(lambda: rest.get_name()) if rest else None, gc_bounds=bounds(gc),
                 breaks=S['breaks'].get(key))
        fn = getattr(gc, 'is_root_broken', None)
        if fn:
            try:
                d['root_broken'] = bool(fn())
            except Exception:  # noqa: BLE001
                pass
    else:
        d['gc_component'] = 'gone'
    return d


def sample():
    for key in S['res']:
        tr = S['track'].setdefault(key, dict(first_amount=None, harvest_times=[], empty_at=None, mesh_hidden_at=None, rest_set_at=None, broken_at=None,
                                             gone_at=None, max_gc_extent=None, start_gc_extent=None, nudges=0, last_nudge=0.0, states=[]))
        st = state(key)
        now = round(S['t'] - S['order_time'], 1)
        if not st['alive']:
            if tr['gone_at'] is None:
                tr['gone_at'] = now
                log('%s: actor destroyed at %.1f s' % (key, now))
            continue
        if tr['first_amount'] is None:
            tr['first_amount'] = st['amount']
        if st['amount'] is not None and (not tr['harvest_times'] or tr['harvest_times'][-1][1] != st['amount']) and st['amount'] != tr['first_amount']:
            tr['harvest_times'].append((now, st['amount']))
            if st['amount'] == 0 or len(tr['harvest_times']) % 10 == 1:
                log('%s: amount %s at %.1f s' % (key, st['amount'], now))
        if st['amount'] == 0 and tr['empty_at'] is None:
            tr['empty_at'] = now
            release_harvesters(key)
        if tr['empty_at'] is not None:
            if st.get('mesh_visible') is False and tr['mesh_hidden_at'] is None:
                tr['mesh_hidden_at'] = now
            if st.get('gc_rest') and tr['rest_set_at'] is None:
                tr['rest_set_at'], tr['rest_name'], tr['start_gc_extent'] = now, st['gc_rest'], (st.get('gc_bounds') or [0] * 6)[3:]
                log('%s: collapse collection %s set at %.1f s, bounds %s' % (key, st['gc_rest'], now, st.get('gc_bounds')))
            if (st.get('root_broken') or (st.get('breaks') or 0) > 0) and tr['broken_at'] is None:
                tr['broken_at'] = now
                log('%s: collection broke at %.1f s (root_broken=%s breaks=%s)' % (key, now, st.get('root_broken'), st.get('breaks')))
            if st.get('gc_bounds'):
                ext = st['gc_bounds'][3:]
                low, high = st['gc_bounds'][2] - ext[2], st['gc_bounds'][2] + ext[2]
                if tr.get('first_low') is None:
                    tr['first_low'], tr['first_high'] = low, high
                tr['lowest'] = min(tr.get('lowest', low), low)
                tr['highest'] = max(tr.get('highest', high), high)
                if low < tr['first_low'] - 100 and tr.get('fall_seen_at') is None:
                    tr['fall_seen_at'] = round(now - tr['empty_at'], 1)
                if tr['max_gc_extent'] is None or ext[0] + ext[1] > tr['max_gc_extent'][0] + tr['max_gc_extent'][1]:
                    tr['max_gc_extent'] = ext
            since = now - tr['empty_at']
            if since >= len(tr['states']) * 0.5:
                st['root_transform'] = safe(lambda: str(comps(S['res'][key])[1].get_root_current_transform().translation))
                tr['states'].append((now, st))
            for n, at in enumerate(DEBUG_DUMP_SECONDS):
                if since >= at and n not in tr.setdefault('dumps', set()) and st.get('gc_rest'):
                    tr['dumps'].add(n)
                    dump_collection(key, n, now)
        elif tr['empty_at'] is None:
            drive_harvesters(key, tr, now)


CARGO_HARVESTS = 2          # a Pz I harvester carries 75 metal: one harvest of 50 and one of 25 fill it
NUDGE_SECONDS = 2.0
SLOTS = ((0.0, 950.0, -90.0), (0.0, -950.0, 90.0))


def drive_harvesters(key, tr, now):
    """Keeps two harvesters working on the resource until it is empty.
    The test map has no drop-off building, so a harvester with a full cargo is removed and a fresh one takes its place.
    Without a renderer the harvest montage never fires its notify (seen in the first runs: nothing in 25 s), so once a
    harvester stands at the resource the test calls UHarvester::OnHarvestAnimationFinished, as the notify would."""
    actor = S['res'][key]
    world = game_world()
    slots = S['slots'].setdefault(key, [dict(tank=None, arrived=None, nudges=0, last=0.0) for _ in SLOTS])
    origin = actor.get_actor_location()
    for slot, (dx, dy, yaw) in zip(slots, SLOTS):
        tank = slot['tank']
        if not alive(tank):
            loc = unreal.Vector(origin.x + dx, origin.y + dy, ground_z(world, origin.x + dx, origin.y + dy) + 60)
            tank = spawn(world, unreal.load_class(None, HARVESTER), loc, unreal.Rotator(0, 0, yaw), owner=1)
            if tank is None:
                continue
            result = safe(lambda: str(tank.harvest_resource(actor, True)))
            tr['harvesters_used'] = tr.get('harvesters_used', 0) + 1
            if tr['harvesters_used'] <= 2:
                log('%s: harvester %d ordered, HarvestResource -> %s' % (key, tr['harvesters_used'], result))
            slot.update(tank=tank, arrived=None, nudges=0, last=now)
            continue
        dist = (tank.get_actor_location() - origin).length()
        if slot['arrived'] is None:
            if dist < 900 and tank.get_velocity().length() < 5 and now - slot['last'] > 1.0:
                slot['arrived'], slot['last'] = now, now
            elif now - slot['last'] > 40.0:
                tr['harvesters_stuck'] = tr.get('harvesters_stuck', 0) + 1
                tank.destroy_actor()
                slot['tank'] = None
            continue
        if now - slot['last'] < NUDGE_SECONDS:
            continue
        if slot['nudges'] >= CARGO_HARVESTS:
            tank.destroy_actor()
            slot['tank'] = None
            continue
        harvester = tank.get_component_by_class(unreal.Harvester)
        if harvester is not None:
            harvester.on_harvest_animation_finished()
            tr['nudges'] += 1
        slot['nudges'] += 1
        slot['last'] = now


def release_harvesters(key):
    """An idle harvester looks for the next metal resource by itself; left alone, the harvesters of a finished station
    drive to a neighbouring station and take its four harvest places (this stalled Glaskopf in run 6)."""
    for slot in S['slots'].get(key, []):
        if alive(slot['tank']):
            slot['tank'].destroy_actor()
        slot['tank'] = None


DEBUG_DUMP_SECONDS = ()


def dump_collection(key, n, now):
    """The dynamic collection as text. In 5.5 the text only lists attribute sizes, so nothing is dumped by default."""
    _, gc, _ = comps(S['res'][key])
    try:
        text = gc.get_debug_info()
        dynamic = text[text.rfind('DynamicCollection - '):]
        os.makedirs(OUT_DIR, exist_ok=True)
        open('%s%s_dynamic_%d.txt' % (OUT_DIR, key, n), 'w').write('t=%.1f %s' % (now, dynamic))
    except Exception as error:  # noqa: BLE001
        log('%s: no debug info (%r)' % (key, error))


def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def shoot(name, w=1600, h=900):
    S['busy'] = True
    try:
        os.makedirs(SHOTS, exist_ok=True)
        unreal.AutomationLibrary.take_high_res_screenshot(w, h, SHOTS + name + '.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log('picture ' + name)


def fog_off():
    """The fog of war post process blacks out everything outside the player's vision, so the pictures turn it off:
    every post process component of the fog manager, and every fog blendable on a post process volume."""
    world = game_world()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FowManager):
        actor.set_actor_tick_enabled(False)
        actor.set_actor_hidden_in_game(True)
        for comp in actor.get_components_by_class(unreal.PostProcessComponent):
            comp.set_editor_property('enabled', False)
            log('fog of war post process off on %s' % actor.get_name())
    for volume in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume):
        settings = volume.get_editor_property('settings')
        blendables = settings.get_editor_property('weighted_blendables')
        rows = list(blendables.get_editor_property('array'))
        keep = [r for r in rows if r.get_editor_property('object') is None
                or 'fog' not in r.get_editor_property('object').get_path_name().lower()
                and 'fow' not in r.get_editor_property('object').get_path_name().lower()]
        log('volume %s blendables %s' % (volume.get_name(), [r.get_editor_property('object').get_path_name() if r.get_editor_property('object') else None for r in rows]))
        if len(keep) != len(rows):
            blendables.set_editor_property('array', keep)
            settings.set_editor_property('weighted_blendables', blendables)
            volume.set_editor_property('settings', settings)
            log('fog blendable removed from %s' % volume.get_name())


def build_shots():
    shots = []
    for name, target in S['line'].items():
        t = target + unreal.Vector(0, 0, 60)
        a = math.radians(55.0)
        shots.append((name + '_game', t + unreal.Vector(0, math.cos(a) * 2400, math.sin(a) * 2400), t))
        shots.append((name + '_close', t + unreal.Vector(-50, 1150, 330), t + unreal.Vector(-50, 0, 60)))
        shots.append((name + '_detail', t + unreal.Vector(450, 620, 260), t + unreal.Vector(450, 0, 90)))
    return shots


COLLAPSE_SHOT_SECONDS = (0.5, 2.0, 5.0, 10.0)


def collapse_shots():
    """Rendered runs only: pictures of every collapsing station at fixed times after its collection broke. One camera,
    so a picture is aimed in one tick and taken 0.4 s later; the real offset is in the log."""
    pending = S.get('pending')
    if pending is not None:
        if S['t'] >= pending[1]:
            S['pending'] = None
            shoot(pending[0])
        return
    keys = list(S['res'])
    for key in keys:
        tr = S['track'].get(key) or {}
        if tr.get('broken_at') is None or tr.get('gone_at') is not None:
            continue
        since = (S['t'] - S['order_time']) - tr['broken_at']
        taken = tr.setdefault('collapse_taken', 0)
        if taken < len(COLLAPSE_SHOT_SECONDS) and since >= COLLAPSE_SHOT_SECONDS[taken]:
            tr['collapse_taken'] = taken + 1
            t = unreal.Vector(station_x([k for k, _ in STATIONS].index(key)), 0, S['res'][key].get_actor_location().z + 60)
            aim(t + unreal.Vector(0, 1500, 1500), t)
            S['pending'] = ('%s_collapse_%d_at%.1fs' % (key, taken, since), S['t'] + 0.4)
            return


def verdict(key):
    tr = S['track'].get(key, {})
    grew = None
    if tr.get('start_gc_extent') and tr.get('max_gc_extent'):
        grew = [tr['max_gc_extent'][i] - tr['start_gc_extent'][i] for i in range(3)]
    ok = bool(tr.get('harvest_times')) and tr.get('empty_at') is not None and tr.get('mesh_hidden_at') is not None \
        and tr.get('rest_set_at') is not None and tr.get('broken_at') is not None and tr.get('gone_at') is not None
    return dict(passed=ok, harvested=tr.get('harvest_times'), empty_at=tr.get('empty_at'), mesh_hidden_at=tr.get('mesh_hidden_at'),
                collection=tr.get('rest_name'), collection_set_at=tr.get('rest_set_at'), broken_at=tr.get('broken_at'),
                break_events=S['breaks'].get(key), gc_extent_growth=grew, bottom_z_first=tr.get('first_low'), bottom_z_lowest=tr.get('lowest'),
                top_z_first=tr.get('first_high'), top_z_highest=tr.get('highest'), fall_seen_s_after_empty=tr.get('fall_seen_at'), actor_destroyed_at=tr.get('gone_at'),
                harvest_notify_called_by_hand=tr.get('nudges'), harvesters_used=tr.get('harvesters_used'),
                harvesters_stuck=tr.get('harvesters_stuck'), start_amount=tr.get('first_amount'), states=tr.get('states'))


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                spawn_all(world)
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered' and S['t'] - S['mark'] > 6.0:
            S['result']['setup'] = {k: setup_report(k) for k in S['res']}
            for key in S['res']:
                log('%s setup %s' % (key, json.dumps(S['result']['setup'][key], default=str)))
            S['shots'] = build_shots() if RENDER and S['camera'] is not None else []
            if S['shots']:
                fog_off()
                for cmd in ('r.TextureStreaming 0', 'r.ForceLOD 0'):
                    unreal.SystemLibrary.execute_console_command(game_world(), cmd)
                aim(S['shots'][0][1], S['shots'][0][2])
            S['phase'], S['mark'] = 'pictures', S['t']
        elif phase == 'pictures' and S['t'] - S['mark'] > 2.5:
            if S['shots']:
                name, _, _ = S['shots'].pop(0)
                shoot(name)
                if S['shots']:
                    aim(S['shots'][0][1], S['shots'][0][2])
                S['mark'] = S['t']
            elif LOOK_ONLY:
                log('look only, done')
                finish()
            else:
                S['phase'] = 'orders'
        elif phase == 'orders':
            S['order_time'] = S['t']
            for key in S['res']:
                watch_breaks(key)
            S['phase'], S['mark'] = 'harvesting', S['t']
        elif phase == 'harvesting':
            sample()
            if RENDER and S.get('camera') is not None:
                collapse_shots()
            elapsed = S['t'] - S['mark']
            done = all(S['track'].get(k, {}).get('gone_at') is not None for k in S['res'])
            if done or elapsed > 400.0:
                S['result']['stations'] = {k: verdict(k) for k in S['res']}
                for key, row in S['result']['stations'].items():
                    log('%s verdict %s' % (key, json.dumps({k: v for k, v in row.items() if k != 'states'}, default=str)))
                S['result']['all_passed'] = all(r['passed'] for r in S['result']['stations'].values()) and len(S['res']) == len(STATIONS)
                log('ALL PASSED %s' % S['result']['all_passed'])
                finish()
    except Exception as error:  # noqa: BLE001
        log('ERROR %s: %r' % (phase, error))
        finish()
    if S['t'] > 520.0 and S['phase'] != 'done':
        log('timeout in phase ' + S['phase'])
        S['result']['stations'] = {k: verdict(k) for k in S['res']}
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
