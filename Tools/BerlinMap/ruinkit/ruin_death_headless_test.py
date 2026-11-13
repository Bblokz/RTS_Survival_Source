"""Throwaway-editor test of the Berlin ruin death (graph in BP_BerlinRuin_Base, data from ruin_blueprints_build.py).
Never on Ger_Berlin, never in the shared editor. Launch with ruin_death_run.sh <logname> [timeout_s] (same folder):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -LogCmds="LogWallHoleDamage Verbose, LogRTSCoverFinder Verbose" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
The map is never saved; the editor quits at the end.

Placed before PIE along world X (y = 0, yaw 90 so the street front faces the sunlit +Y), then RebuildNavigation:
  E1_CornerTower (hero, one large heap), E7_HalfHouse_A (filler with a skirt, small heap at 0.8),
  E3_CoreRow (wall core, two large heaps), B3_Facade4St (plot slab, no heap), E4_StubRing_A (stub, small heap; a
  player Pz IV G shoots it dead, apply_damage only if it survives STUB_FIRE_SECONDS),
  B2_Facade2St_B dressed through the ambient module (curtain, lamp, crow group; tags BerlinAmbient / BerlinCrowPerch),
  and a control BP_BerlinCoverHeap_Collapse_S placed like a map heap (its cover registers at map start).
E1, E3 and B3 are large: they must end as a sunken, tilted remnant that stays (dead: more damage, point damage and
TriggerDestroyActor are thrown at it; its kept nav boxes must still block). The others must sink away and be destroyed.
Second set (third launcher argument "large2"): B7, E2, B5 and B4, the other large pieces, pictures of heap and remnant.
Each ruin is killed in turn (apply_damage, i.e. the TakeDamage path). Sampled every tick with the world clock:
mesh z, actor tilt, nav boxes left, dressing still visible, Niagara components of the death dust, heap actors.
Pictures from an RTS-like camera before and 0.5 / 2 / 5 s after the death and once the dust thins.
After each death: actor gone, heap transforms against the footprint table and the transform before the death,
navmesh (ruin boxes freed, heap boxes blocked, path across before / after), published cover points at the heap's
sockets (and at the control heap's). Results: Saved/Berlin/ruin_death_test/result.json + PNGs.
"""
import json
import math
import os
import sys

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test/'
OUT = ROOT + 'result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'
HEAP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/'
FOOTPRINTS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/footprint_table.json'
TANK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C'
# key: (x position, camera eye offset, look offset). The game camera is about 28 m up; the two 20 m pieces are shot
# from a little further so the whole event is in frame.
RUINS = {
    'E1_CornerTower': (-9500, (300, 3000, 3600), (150, -150, 500)),
    'E7_HalfHouse_A': (-5500, (200, 1900, 2800), (0, 0, 200)),
    'E3_CoreRow': (-1500, (300, 3000, 3600), (0, 0, 300)),
    'B3_Facade4St': (2500, (200, 1900, 2800), (0, 0, 300)),
    'E4_StubRing_A': (5500, (200, 1900, 2800), (0, 0, 100)),
    'B2_Facade2St_B': (8500, (200, 1900, 2800), (0, 0, 300)),
}
KILL_ORDER = ['B3_Facade4St', 'E7_HalfHouse_A', 'E3_CoreRow', 'E1_CornerTower', 'B2_Facade2St_B', 'E4_StubRing_A']
TANK_KILL = 'E4_StubRing_A'
DRESSED = 'B2_Facade2St_B'
CONTROL_HEAP_X = 11500
TANK_Y = 3500.0
STUB_FIRE_SECONDS = 70.0
# Second set (ruin_death_run.sh <log> <timeout> large2): the other four large pieces, to see heap against remnant.
# No tank and no dressing; results go to result_large2.json and the pictures get the same names.
if os.environ.get('RUIN_DEATH_SET') == 'large2':
    RUINS = {
        'B7_WallCore': (-8000, (200, 2300, 3000), (0, 0, 200)),
        'E2_Courtyard': (-3500, (300, 3000, 3600), (75, 0, 300)),
        'B5_CornerL': (1500, (200, 1900, 2800), (0, 0, 200)),
        'B4_Firewall': (5500, (200, 1900, 2800), (0, 0, 300)),
    }
    KILL_ORDER = list(RUINS)
    TANK_KILL = None
    DRESSED = None
    OUT = ROOT + 'result_large2.json'
PICTURE_TIMES = [0.5, 2.0, 5.0]
CAMERA_SETTLE = 3.0
EFFECTS = ('NS_RTSDebrisExplosion_Huge',)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'ruins': {}, 'tanks': {}, 'result': {}, 'pre': {},
     'known_heaps': set()}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('RUINDEATHTEST ' + message)


def finish():
    import builtins
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT, 'w'), indent=1, default=str)
    finally:
        S['phase'] = 'done'
        if getattr(builtins, 'RUIN_DEATH_KEEP_EDITOR', False):
            # Run from an editor that holds unsaved work (graph authoring session): only leave PIE.
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            log('finished; editor kept open')
        else:
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
    """The ruin's own mesh is the component named StaticMesh (dressing adds other StaticMeshComponents)."""
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def heap_mesh(actor):
    named = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'MainMesh']
    return named[0] if named else actor.get_component_by_class(unreal.StaticMeshComponent)


def nav_boxes_of(actor):
    return [c for c in actor.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('NavBlock_')]


def dressing_of(actor):
    out = []
    for comp in actor.get_components_by_class(unreal.SceneComponent):
        tags = [str(t) for t in comp.get_editor_property('component_tags')]
        if 'BerlinAmbient' in tags or 'BerlinCrowPerch' in tags:
            out.append(comp)
    return out


# ---------------------------------------------------------------- setup

def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed = {}
    for key, (x, _, _) in RUINS.items():
        z = ground_z(world, x, 0)
        cls = unreal.load_class(None, f'{BP_DIR}BP_BerlinRuin_{key}.BP_BerlinRuin_{key}_C')
        placed[key] = actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 90))
        log(f'placed {key} at ({x},0,{z:.0f})')
    if TANK_KILL:
        tz = ground_z(world, RUINS[TANK_KILL][0], TANK_Y)
        tank = actors.spawn_actor_from_class(unreal.load_class(None, TANK), unreal.Vector(RUINS[TANK_KILL][0], TANK_Y, tz + 50),
                                             unreal.Rotator(0, 0, -90))
        rts = tank.get_component_by_class(unreal.RTSComponent) if tank else None
        if rts is not None:
            rts.set_editor_property('owning_player', 1)
    # Dressing through the ambient module, exactly as on the map: components of the ruin actor with the two tags.
    sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient')
    try:
        if not DRESSED:
            raise RuntimeError('no dressed ruin in this set')
        import ambient_place
        lookup = {0: placed[DRESSED]}
        items = [(0, 'SM_BerlinAmb_Curtain_A', 40.0, -150.0, 420.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0),
                 (0, 'SM_BerlinAmb_Lamp', 40.0, 200.0, 500.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)]
        done = ambient_place.place_dressing(items, lookup)
        birds = ambient_place.place_crows([{'ruin': 0, 'birds': [(0.0, -100.0 + 60.0 * i, 760.0, 30.0 * i, 1.0, 0.3 * i, 1.0)
                                                                    for i in range(4)]}], lookup)
        log(f'dressing through ambient_place: {done} pieces placed/skipped, {birds} crows; components on the actor '
            f'{[(c.get_name(), [str(t) for t in c.get_editor_property("component_tags")]) for c in dressing_of(placed[DRESSED])]}')
    except Exception as error:  # noqa: BLE001
        log(f'ambient module failed ({error!r}); the dressed-ruin check will show 0 components')
    z = ground_z(world, CONTROL_HEAP_X, 0)
    heap_cls = unreal.load_class(None, f'{HEAP_DIR}BP_BerlinCoverHeap_Collapse_S.BP_BerlinCoverHeap_Collapse_S_C')
    actors.spawn_actor_from_class(heap_cls, unreal.Vector(CONTROL_HEAP_X, 0, z), unreal.Rotator(0, 0, 90))
    log(f'placed the control heap (Collapse_S) at ({CONTROL_HEAP_X},0,{z:.0f})')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))


def heaps_in(world):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name().startswith('BP_BerlinCoverHeap_Collapse')]


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        for key in RUINS:
            if name == f'BP_BerlinRuin_{key}_C':
                S['ruins'][key] = actor
    tanks = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster)
    near = [t for t in tanks if TANK_KILL and abs(t.get_actor_location().x - RUINS[TANK_KILL][0]) < 300
            and abs(t.get_actor_location().y - TANK_Y) < 500]
    S['tank'] = near[0] if near else None
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    heaps = heaps_in(world)
    S['control_heap'] = heaps[0] if heaps else None
    S['known_heaps'] = set(h.get_name() for h in heaps)
    log(f'runtime ruins {sorted(S["ruins"])} tank {S["tank"] is not None} control heap {S["control_heap"] is not None}')


def report_setup(key, actor):
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    values = {}
    for name in ('SinkOnDeath', 'LeaveRemnant', 'SinkDeltaZ', 'SinkTime', 'SinkMinTilt', 'SinkMaxTilt', 'SinkMaxYaw', 'HeapDustDelay',
                 'HeapDustLead', 'DustPulseInterval', 'DustSpreadX', 'DustSpreadY', 'DeathVfxOffset', 'DeathDustScale',
                 'DeathHeapDustScale', 'DeathDustVfx', 'DeathSound'):
        value = actor.get_editor_property(name)
        values[name] = value.get_name() if isinstance(value, unreal.Object) else (
            [round(value.x, 2), round(value.y, 2), round(value.z, 2)] if isinstance(value, unreal.Vector) else value)
    heaps = {n: [(round(t.translation.x), round(t.translation.y), round(t.rotation.rotator().yaw), round(t.scale3d.x, 2))
                 for t in actor.get_editor_property(n)] for n in ('DeathHeapsSmall', 'DeathHeapsLarge')}
    ticking = [c.get_name() for c in actor.get_components_by_class(unreal.ActorComponent) if c.is_component_tick_enabled()]
    info = dict(health=actor.get_editor_property('Health'), death=values, heaps=heaps,
                chunk_death=('bCollapseIntoChunksOnDeath=True' in holes.get_editor_property('CollapseSettings').export_text()),
                actor_tick=actor.is_actor_tick_enabled(), ticking_components=ticking,
                nav_boxes=len(nav_boxes_of(actor)), dressing=len(dressing_of(actor)))
    S['result'].setdefault('setup', {})[key] = info
    log(f'{key} setup {json.dumps(info, default=str)}')


# ---------------------------------------------------------------- navigation

def on_navmesh(world, point, extent=(20.0, 20.0, 80.0)):
    nav = unreal.NavigationSystemV1
    fn = getattr(nav, 'k2_project_point_to_navigation', None) or getattr(nav, 'project_point_to_navigation', None)
    if fn is None:
        return 'ERR no project_point_to_navigation'
    try:
        result = fn(world, point, None, None, unreal.Vector(*extent))
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'
    if isinstance(result, tuple):
        return bool(result[0])
    if isinstance(result, bool):
        return result
    return result is not None


def box_samples(actor):
    return box_samples_at(actor, actor.get_actor_location().z + 20.0)


def box_samples_at(actor, ground):
    """Per nav box of the actor: (world rectangle description, world sample points along its long axis at ground height)."""
    out = []
    transform = actor.get_actor_transform()
    for box in nav_boxes_of(actor):
        centre = box.get_world_location()
        extent = box.get_scaled_box_extent()
        rotation = box.get_world_rotation()
        long_x = extent.x >= extent.y
        half = max(extent.x, extent.y) - 25.0
        count = max(1, int(half * 2 // 150))
        offsets = [0.0] + [(-half + i * (2 * half) / count) for i in range(count + 1)] if half > 0 else [0.0]
        points = []
        for offset in offsets:
            local = unreal.Vector(offset if long_x else 0.0, 0.0 if long_x else offset, 0.0)
            world_point = centre + unreal.MathLibrary.greater_greater_vector_rotator(local, rotation)
            points.append(unreal.Vector(world_point.x, world_point.y, ground))
        out.append(dict(centre=centre, extent=extent, yaw=rotation.yaw, points=points))
    del transform
    return out


def inside_any(point, boxes):
    for box in boxes:
        local = unreal.MathLibrary.less_less_vector_rotator(point - box['centre'], unreal.Rotator(0, 0, box['yaw']))
        if abs(local.x) <= box['extent'].x + 30.0 and abs(local.y) <= box['extent'].y + 30.0:
            return True
    return False


def heavy_navmesh(world):
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        if 'HeavyTank' in nav.get_name():
            return nav
    return None


def path_across(world, ends, nav_data=None):
    a, b = ends
    try:
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data)
        points = list(path.get_editor_property('path_points')) if path else []
    except Exception as error:  # noqa: BLE001
        return f'ERR {error!r}'[:80]
    if not points:
        return 'no path'
    length = sum((points[i + 1] - points[i]).length() for i in range(len(points) - 1))
    return f'{length:.0f} cm (straight {(b - a).length():.0f}, partial {path.is_partial()})'


def across_ends(actor):
    transform = actor.get_actor_transform()
    boxes = nav_boxes_of(actor)
    reach = max([abs(transform.inverse_transform_location(b.get_world_location()).x) + b.get_unscaled_box_extent().x
                 for b in boxes] or [500.0]) + 400.0
    return (transform.transform_location(unreal.Vector(reach, 0, 20)), transform.transform_location(unreal.Vector(-reach, 0, 20)))


def nav_alive(world, key, actor):
    boxes = box_samples(actor)
    on = sum(1 for b in boxes for p in b['points'] if on_navmesh(world, p) is True)
    total = sum(len(b['points']) for b in boxes)
    ends = across_ends(actor)
    info = dict(box_points_on_navmesh=f'{on}/{total}', open_ends_on_navmesh=[on_navmesh(world, e) for e in ends],
                path=path_across(world, ends), heavy_path=path_across(world, ends, heavy_navmesh(world)))
    S['pre'][key] = dict(boxes=boxes, ends=ends, transform=actor.get_actor_transform(), nav=info)
    S['result'].setdefault('nav_alive', {})[key] = info
    log(f'nav alive {key}: ruin box points on navmesh {on}/{total}; the two path ends (open ground) on navmesh '
        f'{info["open_ends_on_navmesh"]}; path across {info["path"]}; HeavyTank {info["heavy_path"]}')


def nav_dead(world, key, heaps, label):
    pre = S['pre'][key]
    heap_boxes = [b for h in heaps if alive(h) for b in box_samples(h)]
    ruin = S['ruins'][key]
    # A remnant keeps the boxes of the walls that still stand; they stay where they were (detached from the sinking actor).
    kept = box_samples_at(ruin, pre['transform'].translation.z + 20.0) if alive(ruin) else []
    kept_on = sum(1 for b in kept for p in b['points'] if on_navmesh(world, p) is True)
    kept_total = sum(len(b['points']) for b in kept)
    kept_z = sorted(set(round(b['centre'].z) for b in kept))
    heap_boxes = heap_boxes + kept
    freed, still, under_heap = 0, 0, 0
    for box in pre['boxes']:
        for point in box['points']:
            if inside_any(point, heap_boxes):
                under_heap += 1
            elif on_navmesh(world, point) is True:
                freed += 1
            else:
                still += 1
    heap_on = sum(1 for b in heap_boxes for p in b['points'] if on_navmesh(world, p) is True)
    heap_total = sum(len(b['points']) for b in heap_boxes)
    info = dict(ruin_box_points_free=freed, ruin_box_points_still_blocked=still, ruin_box_points_under_heap_boxes=under_heap,
                heap_and_remnant_nav_boxes=len(heap_boxes), heap_and_remnant_box_points_on_navmesh=f'{heap_on}/{heap_total}',
                remnant_nav_boxes=len(kept), remnant_box_points_on_navmesh=f'{kept_on}/{kept_total}', remnant_box_centre_z=kept_z,
                open_ends_on_navmesh=[on_navmesh(world, e) for e in pre['ends']],
                path=path_across(world, pre['ends']), heavy_path=path_across(world, pre['ends'], heavy_navmesh(world)),
                nav_building=nav_building(world))
    S['result'].setdefault('nav_' + label, {})[key] = info
    log(f'nav {label} {key}: {json.dumps(info)}')


# ---------------------------------------------------------------- cover

def cover_subsystem(world):
    if S.get('cover_sub') is not None and alive(S['cover_sub']):
        return S['cover_sub']
    for sub in unreal.ObjectIterator(unreal.RTSCoverFinderWorldSubsystem):
        try:
            if sub.get_outer() == world or sub.get_world() == world:
                S['cover_sub'] = sub
                return sub
        except Exception:  # noqa: BLE001
            continue
    return None


def cover_of(world, heap):
    """(sockets on the heap mesh, sockets with a published prone point within 40 cm, all points within 9 m)."""
    sub = cover_subsystem(world)
    if sub is None or not alive(heap):
        return None
    mesh = heap_mesh(heap)
    sockets = [mesh.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_WORLD).translation
               for n in mesh.get_all_socket_names() if 'prone' in str(n)]
    points = list(sub.find_cover_points_in_radius(heap.get_actor_location(), 900.0))
    matched = 0
    for socket in sockets:
        for point in points:
            loc = point.get_editor_property('location')
            if 'PRONE' in str(point.get_editor_property('cover_type')).upper() and math.hypot(loc.x - socket.x, loc.y - socket.y) <= 40.0:
                matched += 1
                break
    return dict(sockets=len(sockets), published_at_sockets=matched, points_within_9m=len(points))


# ---------------------------------------------------------------- pictures

def place_camera(key, origin):
    _, eye, look = RUINS[key]
    camera = S['camera']
    eye_world, look_world = origin + unreal.Vector(*eye), origin + unreal.Vector(*look)
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye_world, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye_world, look_world), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False


# ---------------------------------------------------------------- one death

def expected_heaps(key, transform):
    with open(FOOTPRINTS, 'r') as handle:
        table = json.load(handle)['pieces']
    out = []
    for entry in table.get(key, {}).get('dead', []):
        location = transform.transform_location(unreal.Vector(entry['x'], entry['y'], 0.0))
        out.append(dict(size=entry['mesh'].rsplit('_', 1)[1], location=location,
                        yaw=(transform.rotation.rotator().yaw + entry['yaw']) % 360.0,
                        scale=float(entry.get('scale', 1.0)) * transform.scale3d.x))
    return out


def effects_alive(world):
    found = {}
    for comp in unreal.ObjectIterator(unreal.NiagaraComponent):
        try:
            if comp.get_world() != world:
                continue
            asset = comp.get_asset()
            name = asset.get_name() if asset else ''
            if name in EFFECTS and comp.is_active():
                found[name] = found.get(name, 0) + 1
        except Exception:  # noqa: BLE001
            continue
    return found


def prepare_death(key):
    """What the ruin looks like while alive; read before the damage (the death hides and destroys things at once)."""
    ruin = S['ruins'][key]
    dressing = dressing_of(ruin)
    S['before'] = dict(key=key, z0=mesh_of(ruin).get_world_location().z, dressing=dressing,
                       dressing_visible=sum(1 for c in dressing if c.is_visible()), nav_boxes=len(nav_boxes_of(ruin)),
                       sink_time=ruin.get_editor_property('SinkTime'), sink_cm=ruin.get_editor_property('SinkDeltaZ'),
                       remnant=bool(ruin.get_editor_property('LeaveRemnant')), yaw0=ruin.get_actor_rotation().yaw,
                       health0=ruin.get_editor_property('Health'),
                       heap_delay=ruin.get_editor_property('HeapDustDelay') + ruin.get_editor_property('HeapDustLead'))


def start_death(world, key, how):
    b = S['before']
    d = dict(key=key, how=how, t0=now(world), samples=[], pictures=list(PICTURE_TIMES), heaps=[], heap_seen_at=None,
             gone_at=None, effects_at=None, effects_max=0, dressing_hidden_at=None, boxes_gone_at=None, after_done=False,
             sink_time=b['sink_time'], heap_delay=b['heap_delay'], z0=b['z0'], dressing=b['dressing'], listed=False,
             dressing_visible_before=b['dressing_visible'], nav_boxes_before=b['nav_boxes'], remnant=b['remnant'],
             sink_cm=b['sink_cm'], yaw0=b['yaw0'], plus10_done=False, second=None)
    S['death'] = d
    log(f'death {key} ({how}) at world {d["t0"]:.2f}: sink {d["sink_time"]} s by {b["sink_cm"]:.0f} cm, heaps at '
        f'{d["heap_delay"]:.2f} s, nav boxes {b["nav_boxes"]}, dressing visible {d["dressing_visible_before"]}/{len(d["dressing"])}')


def tick_death(world):
    d = S['death']
    key = d['key']
    ruin = S['ruins'][key]
    t = now(world) - d['t0']
    valid = alive(ruin)
    if valid:
        mesh = mesh_of(ruin)
        rotation = ruin.get_actor_rotation()
        boxes = len(nav_boxes_of(ruin))
        visible = sum(1 for c in d['dressing'] if alive(c) and c.is_visible())
        d['samples'].append((round(t, 2), round(mesh.get_world_location().z - d['z0'], 1), round(rotation.pitch, 2),
                             round(rotation.roll, 2), boxes, visible, mesh.is_visible()))
        if boxes == 0 and d['boxes_gone_at'] is None:
            d['boxes_gone_at'] = round(t, 2)
        if d['dressing'] and visible == 0 and d['dressing_hidden_at'] is None:
            d['dressing_hidden_at'] = round(t, 2)
    elif d['gone_at'] is None:
        d['gone_at'] = round(t, 2)
        log(f'{key}: ruin actor gone at {t:.2f} s (sink time {d["sink_time"]})')
    found = effects_alive(world)
    d['effects_max'] = max(d['effects_max'], sum(found.values()))
    if d['effects_at'] is None and (found or t > 1.0):
        d['effects_at'] = (round(t, 2), found)
        log(f'{key}: death effects active at {t:.2f} s: {found}')
    if not d['listed'] and t >= 0.25:
        d['listed'] = True
        log(f'{key}: ListWaves at {t:.2f} s (see the lines after AUDIOPROBE {key} in the log)')
        unreal.log(f'AUDIOPROBE {key} begin')
        unreal.SystemLibrary.execute_console_command(world, 'ListWaves')
        unreal.log(f'AUDIOPROBE {key} end')
    new_heaps = [h for h in heaps_in(world) if h.get_name() not in S['known_heaps']]
    if new_heaps:
        for heap in new_heaps:
            S['known_heaps'].add(heap.get_name())
            d['heaps'].append(heap)
        if d['heap_seen_at'] is None:
            d['heap_seen_at'] = round(t, 2)
        log(f'{key}: {len(new_heaps)} heap actor(s) appeared at {t:.2f} s (delay {d["heap_delay"]})')
    if d['pictures'] and t >= d['pictures'][0]:
        at = d['pictures'].pop(0)
        picture(f'death_{key}_{str(at).replace(".", "_")}s')
        log(f'{key}: picture {at} s requested at {t:.2f} s')
    after_at = max(d['sink_time'] + 3.5, 8.5)
    if not d['after_done'] and t >= after_at:
        d['after_done'] = True
        picture(f'death_{key}_after')
        log(f'{key}: picture after requested at {t:.2f} s')
    if d['remnant'] and valid and d['second'] is None and t >= after_at + 4.0:
        d['second'] = second_damage(world, ruin)
    if d['remnant'] and not d['plus10_done'] and t >= after_at + 10.0:
        d['plus10_done'] = True
        picture(f'death_{key}_remnant_plus10s')
        log(f'{key}: picture remnant +10 s requested at {t:.2f} s')
    if t >= after_at + (13.5 if d['remnant'] else 4.0):
        end_death(world, t)
        return True
    return False


def second_damage(world, ruin):
    """A remnant is dead: more damage must not start a second death, cut holes or move it."""
    holes = ruin.get_component_by_class(unreal.WallHoleDamageComponent)
    before = dict(z=mesh_of(ruin).get_world_location().z, holes=holes.get_hole_count(), heaps=len(heaps_in(world)),
                  dust=sum(effects_alive(world).values()), health=ruin.get_editor_property('Health'))
    aim = ruin.get_actor_transform().transform_location(ruin.get_editor_property('AimOffsetPoints')[0])
    unreal.GameplayStatics.apply_damage(ruin, 5000.0, None, None, unreal.DamageType)
    unreal.GameplayStatics.apply_point_damage(ruin, 5000.0, unreal.Vector(0, -1, 0), unreal.HitResult(), None, None, unreal.DamageType)
    try:
        ruin.trigger_destroy_actor(unreal.RTSDeathType.KINETIC)
        trigger = 'called'
    except Exception as error:  # noqa: BLE001
        trigger = repr(error)[:80]
    try:
        order = str(S['tank'].attack_actor(ruin, True)) if S.get('tank') is not None and alive(S['tank']) else 'no tank'
        S['tank'].stop_command()
    except Exception as error:  # noqa: BLE001
        order = repr(error)[:80]
    after = dict(z=mesh_of(ruin).get_world_location().z, holes=holes.get_hole_count(), heaps=len(heaps_in(world)),
                 dust=sum(effects_alive(world).values()), health=ruin.get_editor_property('Health'))
    del aim
    out = dict(before=before, after=after, trigger_destroy=trigger, tank_attack_order=order, valid=alive(ruin))
    log(f'second damage on the remnant: {json.dumps(out, default=str)}')
    return out


def end_death(world, t):
    d = S['death']
    key = d['key']
    ruin = S['ruins'][key]
    pre = S['pre'][key]
    samples = d['samples']
    thin = [s for i, s in enumerate(samples) if i % max(1, len(samples) // 14) == 0] + samples[-1:]
    heaps = []
    for expected in expected_heaps(key, pre['transform']):
        best = None
        for heap in d['heaps']:
            if not alive(heap) or not heap.get_class().get_name().endswith(f'Collapse_{expected["size"]}_C'):
                continue
            dist = (heap.get_actor_location() - expected['location']).length()
            if best is None or dist < best[0]:
                best = (dist, heap)
        if best is None:
            heaps.append(dict(expected=expected['size'], found=False))
            continue
        heap = best[1]
        yaw_error = (heap.get_actor_rotation().yaw - expected['yaw'] + 180.0) % 360.0 - 180.0
        heaps.append(dict(expected=expected['size'], found=True, cls=heap.get_class().get_name(),
                          location_error_cm=round(best[0], 2), yaw_error_deg=round(yaw_error, 2),
                          scale=round(heap.get_actor_scale3d().x, 3), expected_scale=round(expected['scale'], 3),
                          location=[round(v) for v in heap.get_actor_location().to_tuple()],
                          nav_boxes=len(nav_boxes_of(heap)), cover=cover_of(world, heap)))
    remnant = None
    if d['remnant']:
        if alive(ruin):
            rotation = ruin.get_actor_rotation()
            mesh = mesh_of(ruin)
            late = [s for s in samples if s[0] >= d['sink_time'] + 0.3]
            remnant = dict(actor_valid=True, z_drop_cm=round(d['z0'] - mesh.get_world_location().z, 1), sink_cm=d['sink_cm'],
                           pitch=round(rotation.pitch, 2), roll=round(rotation.roll, 2),
                           yaw_change=round((rotation.yaw - d['yaw0'] + 180.0) % 360.0 - 180.0, 2),
                           z_still_after_sink=len(set(s[1] for s in late)) <= 1, samples_after_sink=len(late),
                           mesh_visible=mesh.is_visible(), mesh_collision=str(mesh.get_collision_enabled()),
                           actor_tick=ruin.is_actor_tick_enabled(),
                           ticking_components=[c.get_name() for c in ruin.get_components_by_class(unreal.ActorComponent)
                                               if c.is_component_tick_enabled()],
                           nav_boxes_kept=len(nav_boxes_of(ruin)), health=ruin.get_editor_property('Health'),
                           dust_alive_at_end=sum(effects_alive(world).values()), heaps_now=len([h for h in d['heaps'] if alive(h)]),
                           second_damage=d['second'])
        else:
            remnant = dict(actor_valid=False)
    info = dict(how=d['how'], remnant=remnant, actor_valid_after=alive(ruin), gone_at_s=d['gone_at'], sink_time=d['sink_time'],
                final_sample=samples[-1] if samples else None, samples_t_z_pitch_roll_boxes_dressing_meshvisible=thin,
                nav_boxes_before=d['nav_boxes_before'], nav_boxes_gone_at_s=d['boxes_gone_at'], effects_first=d['effects_at'],
                dust_clouds_alive_at_once_max=d['effects_max'], heap_seen_at_s=d['heap_seen_at'],
                heap_delay=d['heap_delay'], heaps_spawned=len(d['heaps']), heaps=heaps,
                dressing=dict(components=len(d['dressing']), visible_before=d['dressing_visible_before'],
                              all_hidden_at_s=d['dressing_hidden_at']))
    S['result'].setdefault('deaths', {})[key] = info
    S.setdefault('death_heaps', {})[key] = d['heaps']
    log(f'death result {key}: {json.dumps(info, default=str)}')
    nav_dead(world, key, d['heaps'], 'after')


# ---------------------------------------------------------------- main loop

def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            # Spawning pumps Slate, which re-enters this callback: leave the phase first.
            S['phase'] = 'placing'
            place_all()
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(editor_world, 'RebuildNavigation')
            S['phase'], S['mark'] = 'editor_nav', S['t']
        elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if nav_building(editor_world) and S['t'] - S['mark'] < 120.0:
                return
            log(f'editor navmesh rebuilt in {S["t"] - S["mark"]:.1f} s')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                find_runtime(world)
                for key, ruin in S['ruins'].items():
                    report_setup(key, ruin)
                S['phase'], S['mark'] = 'nav_wait', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'nav_wait':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            for key, ruin in S['ruins'].items():
                nav_alive(world, key, ruin)
            S['result']['control_heap_cover_start'] = cover_of(world, S['control_heap'])
            log(f'control heap cover at start: {S["result"]["control_heap_cover_start"]}')
            # A throwaway first picture: the first screenshot of a session can show the previous view.
            place_camera(KILL_ORDER[0], S['ruins'][KILL_ORDER[0]].get_actor_location())
            S['queue'] = [k for k in KILL_ORDER if k in S['ruins']]
            S['phase'], S['mark'] = 'warmup', S['t']
        elif phase == 'warmup' and S['t'] - S['mark'] > CAMERA_SETTLE:
            picture('_warmup')
            S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'next' and S['t'] - S['mark'] > 2.5:
            if not S['queue']:
                S['phase'], S['mark'] = 'final', S['t']
                return
            key = S['queue'].pop(0)
            S['current'] = key
            place_camera(key, S['ruins'][key].get_actor_location())
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > CAMERA_SETTLE:
            picture(f'death_{S["current"]}_before')
            S['phase'], S['mark'] = 'before_shot', S['t']
        elif phase == 'before_shot' and S['t'] - S['mark'] > 2.5:
            world = game_world()
            key = S['current']
            ruin = S['ruins'][key]
            if key == TANK_KILL and S.get('tank') is not None:
                try:
                    result = S['tank'].attack_actor(ruin, True)
                except Exception as error:  # noqa: BLE001
                    result = repr(error)[:100]
                log(f'{key}: tank ordered AttackActor -> {result}; waiting for the shells to kill it')
                S['phase'], S['mark'] = 'tank_fire', S['t']
                S['fire_t0'] = now(world)
                return
            prepare_death(key)
            unreal.GameplayStatics.apply_damage(ruin, 1.0e7, None, None, unreal.DamageType)
            start_death(world, key, 'apply_damage')
            S['phase'] = 'dying'
        elif phase == 'tank_fire':
            world = game_world()
            key = S['current']
            ruin = S['ruins'][key]
            health = ruin.get_editor_property('Health') if alive(ruin) else -1
            if health > 0:
                prepare_death(key)      # refreshed every tick while it lives: the shells decide when it dies
            if S.get('last_health') != round(health):
                S['last_health'] = round(health)
                log(f'{key}: health {health:.0f} after {now(world) - S["fire_t0"]:.1f} s of fire')
            if health <= 0:
                start_death(world, key, 'tank shells')
                S['phase'] = 'dying'
            elif now(world) - S['fire_t0'] > STUB_FIRE_SECONDS:
                log(f'{key}: still {health:.0f} health after {STUB_FIRE_SECONDS:.0f} s of fire -> apply_damage')
                prepare_death(key)
                unreal.GameplayStatics.apply_damage(ruin, 1.0e7, None, None, unreal.DamageType)
                start_death(world, key, 'apply_damage after tank fire')
                S['phase'] = 'dying'
        elif phase == 'dying':
            if tick_death(game_world()):
                S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'final' and S['t'] - S['mark'] > 6.0:
            world = game_world()
            # Late re-check: navigation tiles and cover registration get a few more seconds.
            for key, heaps in S.get('death_heaps', {}).items():
                nav_dead(world, key, heaps, 'final')
                S['result'].setdefault('cover_final', {})[key] = [cover_of(world, h) for h in heaps]
            S['result']['control_heap_cover_final'] = cover_of(world, S['control_heap'])
            log(f'cover final: spawned {json.dumps(S["result"].get("cover_final"))} control {S["result"]["control_heap_cover_final"]}')
            leftovers = [(a.get_name(), bool(a.get_editor_property('LeaveRemnant')), round(a.get_actor_location().z))
                         for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                         if a.get_class().get_name().startswith('BP_BerlinRuin_')]
            S['result']['ruin_actors_left'] = leftovers
            log(f'ruin actors left in the world: {leftovers}')
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        import traceback
        log(f'ERROR {phase}: {error!r} {traceback.format_exc()[-600:]}')
        finish()
    if S['t'] > 640.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
