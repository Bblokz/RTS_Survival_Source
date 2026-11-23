"""Throwaway-editor test of the Ore mineral flora bushes, rev2 (clump Blueprints + two bramble hedges).
Never on Ger_2ndOreMountains, never in a shared editor. Launch with run_engine_test.sh <logname> [timeout_s].
The map (/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall, orange desert sand) is never saved; the editor quits at the end.
(rev1 version: bush_engine_test_rev1.py.bak)

1. LOOK   all 9 Blueprints in three rows: on the map's own ground, on an IronOxide-tinted swatch, on an Ochre-tinted swatch,
          with a parked Pz IV G and BP_OreDeadSpruce_Survivor_9m. Pictures: the reviewer's RTS view (22 m back, 34 m up)
          and 45 m up, each also WITHOUT the bushes (bg_*) for the pixel-footprint measure (bush_rev2_measure.py), a
          straight top-down view, and close-ups of dogwood, both hedges, bilberry mat and broom.
2. PATHS  per lane piece 12 m paths across it per agent with that agent's RTS_NavFilter_*; across the hedges five
          crossings along the hedge (gap probe): a light vehicle must find no gap between standing clumps.
3. CRUSH  lanes along world Y; each vehicle starts 24 m before its piece and is ordered 24 m past it:
            A RustHeath clump, Luchs driving over ONE column (dx +115)  -> some plants crushed, the others standing
            B BrambleThicket_Rust + Luchs, B2 BrambleThicket_Ochre + Luchs -> path round, all clumps standing
            C BrambleThicket_Ochre + Pz IV G, C2 BrambleThicket_Rust + Pz IV G -> drive through, clumps under it crushed
            D OchreBroom clump + Pz IV G -> crushed
          then both lane-B Luchs are teleported onto their hedge: standing count must not change.
4. SHELLS line traces on every trace channel and object type through the pieces at 30-120 cm: nothing may hit a bush.
Results: Saved/OreMountains/flora/bushes/rev2/test/result.json and PNGs in Saved/OreMountains/flora/bushes/rev2/.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/rev2/'
OUT = ROOT + 'test/result.json'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Bushes/'
SPRUCE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_Survivor_9m'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIV_G(Medium)': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'Luchs(Light)': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C',
}
# key: (lane x, Blueprint, vehicle, expected, vehicle x offset from the piece)
LANES = {
    'A_clump_light': (-10000, 'RustHeath', 'Luchs(Light)', 'partial', 115.0),
    'B_hedgeRust_light': (-7500, 'BrambleThicket_Rust', 'Luchs(Light)', 'detour', 0.0),
    'B2_hedgeOchre_light': (-5000, 'BrambleThicket_Ochre', 'Luchs(Light)', 'detour', 0.0),
    'C_hedgeOchre_medium': (-2500, 'BrambleThicket_Ochre', 'PzIV_G(Medium)', 'crush', 0.0),
    'C2_hedgeRust_medium': (0, 'BrambleThicket_Rust', 'PzIV_G(Medium)', 'crush', 0.0),
    'D_clump_medium': (2500, 'OchreBroom', 'PzIV_G(Medium)', 'crush', 0.0),
}
NAMES = ['RustHeath', 'RedDogwood', 'RedBilberryMat', 'RustRowan', 'BrambleThicket_Rust', 'BrambleThicket_Ochre',
         'OchreBroom', 'OchreBracken', 'OchreTussock']
LOOK_X = {'RustHeath': -3300, 'RedDogwood': -2550, 'RedBilberryMat': -1850, 'RustRowan': -1150, 'BrambleThicket_Rust': -250,
          'BrambleThicket_Ochre': 650, 'OchreBroom': 1550, 'OchreBracken': 2300, 'OchreTussock': 3050}
LOOK_Y = 6000.0
ROW_DY = 1300.0                 # rows: map ground, IronOxide swatch, Ochre swatch
SWATCH = {'iron': (0.32, 0.05, 0.025), 'ochre': (0.30, 0.10, 0.025)}   # BasicShapeMaterial washes colour out: oversaturated
START_Y = 2400.0
MOVE_SECONDS = 110.0
HEDGE_PROBE_DX = (-230.0, -115.0, 0.0, 115.0, 230.0)
FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'
           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),
                        ('HeavyTank', 'HeavyTanks'))}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'tanks': {}, 'result': {}, 'track': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('BUSHTEST ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT, 'w'), indent=1, default=str)
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


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def bp_class(name):
    return unreal.load_class(None, f'{HOME}BP_OreFlora_{name}.BP_OreFlora_{name}_C')


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, (x, name, vehicle, _, dx) in LANES.items():
        actors.spawn_actor_from_class(bp_class(name), unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 0))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x + dx, START_Y, ground_z(world, x + dx, START_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: placed {name} at x {x} and {vehicle} at dx {dx}, owner set {set_owner(tank, 1)}')
    plane = unreal.load_asset('/Engine/BasicShapes/Plane')
    for row, swatch in ((1, 'iron'), (2, 'ochre')):
        y = LOOK_Y + row * ROW_DY
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, y, ground_z(world, 0, y) + 1.5))
        a.static_mesh_component.set_static_mesh(plane)
        a.set_actor_scale3d(unreal.Vector(76.0, 10.0, 1.0))
        a.set_actor_label('SWATCH_' + swatch)
    for row in range(3):
        y = LOOK_Y + row * ROW_DY
        for i, name in enumerate(NAMES):
            x = LOOK_X[name]
            yaw = 0.0 if name.startswith('Bramble') else 37.0 * i + 11 * row
            actors.spawn_actor_from_class(bp_class(name), unreal.Vector(x, y, ground_z(world, x, y)), unreal.Rotator(0, 0, yaw))
    sx, sy = 3900.0, LOOK_Y - 150
    actors.spawn_actor_from_class(unreal.load_class(None, SPRUCE + '.BP_OreDeadSpruce_Survivor_9m_C'),
                                  unreal.Vector(sx, sy, ground_z(world, sx, sy)), unreal.Rotator(0, 0, 0))
    for vehicle, x, y, yaw in (('PzIV_G(Medium)', -4200, LOOK_Y - 100, 90), ('Luchs(Light)', 4300, LOOK_Y + 1700, 0),
                               ('Luchs(Light)', -4300, LOOK_Y + 2400, 0)):
        scout = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                              unreal.Vector(x, y, ground_z(world, x, y) + 50), unreal.Rotator(0, 0, yaw))
        log(f'look scout {vehicle} owner set {set_owner(scout, 1)}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CrushableCropFieldActor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        for key, (x, bp, _, _, _) in LANES.items():
            if name == f'BP_OreFlora_{bp}_C' and abs(loc.x - x) < 200 and abs(loc.y) < 200:
                S['pieces'][key] = actor
        if abs(loc.y - LOOK_Y) < 2.6 * ROW_DY:
            S.setdefault('look', []).append(actor)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, (x, _, _, _, dx) in LANES.items():
            if abs(loc.x - (x + dx)) < 300 and abs(loc.y - START_Y) < 500:
                S['tanks'][key] = tank
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        loc = a.get_actor_location()
        for row, swatch in ((1, 'iron'), (2, 'ochre')):
            if abs(loc.y - (LOOK_Y + row * ROW_DY)) < 5 and abs(loc.x) < 5:
                c = SWATCH[swatch]
                mid = a.static_mesh_component.create_dynamic_material_instance(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
                mid.set_vector_parameter_value('Color', unreal.LinearColor(c[0], c[1], c[2], 1.0))
                S.setdefault('swatches', {})[swatch] = a
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_class().get_name() == 'BP_OreDeadSpruce_Survivor_9m_C':
            S['spruce'] = a
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 2000)).length())
    log(f'runtime pieces {sorted(S["pieces"])} tanks {sorted(S["tanks"])} look {len(S.get("look", []))} '
        f'swatches {sorted(S.get("swatches", {}))} spruce {"spruce" in S}')


def navmeshes(world):
    out = {}
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh) if n not in out.values()]
    if rest:
        out['Character'] = rest[0]
    log(f'navmeshes {dict((k, v.get_name()) for k, v in out.items())}')
    return out


def path(world, a, b, nav_data, filter_class):
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data, filter_class)
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:100])
    if not pts:
        return dict(length=None)
    length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))
    side = max(abs(pt.x - a.x) for pt in pts)
    return dict(length=round(length), straight=round((b - a).length()), partial=bool(p.is_partial()),
                side_cm=round(side), through=bool(side < 30 and not p.is_partial()))


def across_paths(world, key):
    o = S['origins'][key]
    out = {}
    for agent, nav in S['nav'].items():
        fc = unreal.load_class(None, FILTERS[agent])
        out[agent] = path(world, unreal.Vector(o.x, 600, o.z + 20), unreal.Vector(o.x, -600, o.z + 20), nav, fc)
    if 'Thicket' in LANES[key][1]:      # gap probe: crossings along the hedge, light agent only
        fc = unreal.load_class(None, FILTERS['LightTank'])
        out['LightTank_gap_probe'] = {str(int(dx)): path(world, unreal.Vector(o.x + dx, 600, o.z + 20),
                                                         unreal.Vector(o.x + dx, -600, o.z + 20), S['nav']['LightTank'], fc)
                                      for dx in HEDGE_PROBE_DX}
    return out


def standing(key):
    piece = S['pieces'].get(key)
    return piece.get_num_standing_plants() if alive(piece) else None


def plant_points(actor):
    """World XY of every live (not hidden) standing instance."""
    pts = []
    for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if not c.get_name().startswith('StandingPlants'):
            continue
        for k in range(c.get_instance_count()):
            t = c.get_instance_transform(k, True)
            t = t[-1] if isinstance(t, tuple) else t
            if t.scale3d.x > 0.01:
                pts.append([round(t.translation.x), round(t.translation.y), round(t.scale3d.x, 2)])
    return pts


def setup_report(key):
    piece = S['pieces'][key]
    comps = []
    for c in piece.get_components_by_class(unreal.InstancedStaticMeshComponent):
        sm = c.get_editor_property('static_mesh')
        mats = c.get_materials()
        comps.append(dict(mesh=sm.get_name() if sm else None, instances=c.get_instance_count(),
                          collision=str(c.get_collision_enabled()).split('.')[-1].split(':')[0],
                          material=mats[0].get_name() if mats and mats[0] else None, shadow=bool(c.get_editor_property('cast_shadow'))))
    o, ext = piece.get_actor_bounds(False)
    return dict(cls=piece.get_class().get_name(), plants=piece.get_num_plants(), standing=piece.get_num_standing_plants(),
                crush=str(piece.get_editor_property('crush_destruction_type')),
                nav_area=str(piece.get_editor_property('standing_plants_nav_area')), components=comps,
                actor_bounds_extent=[round(ext.x), round(ext.y), round(ext.z)], plants_xy=plant_points(piece))


def hits_of(res):
    if res is None:
        return []
    if isinstance(res, tuple) and res and isinstance(res[-1], (list, unreal.Array)):
        return list(res[-1])
    return list(res)


def shell_traces(world):
    targets = [p for p in S['pieces'].values() if alive(p)] + list(S.get('look', [])[:9])
    out = []
    obj_types = [getattr(unreal.ObjectTypeQuery, f'OBJECT_TYPE_QUERY{i}') for i in range(1, 17)
                 if hasattr(unreal.ObjectTypeQuery, f'OBJECT_TYPE_QUERY{i}')]
    for t in targets:
        o = t.get_actor_location()
        for hgt in (30.0, 70.0, 120.0):
            a, b = o + unreal.Vector(-700, -150, hgt), o + unreal.Vector(700, 150, hgt)
            hits = set()
            for i in range(1, 7):
                ch = getattr(unreal.TraceTypeQuery, f'TRACE_TYPE_QUERY{i}', None)
                if ch is None:
                    continue
                for h in hits_of(unreal.SystemLibrary.line_trace_multi(world, a, b, ch, True, [], unreal.DrawDebugTrace.NONE, True)):
                    actor = h.to_tuple()[9]
                    hits.add(actor.get_class().get_name() if actor else 'None')
            for h in hits_of(unreal.SystemLibrary.line_trace_multi_for_objects(world, a, b, obj_types, True, [], unreal.DrawDebugTrace.NONE, True)):
                actor = h.to_tuple()[9]
                hits.add(actor.get_class().get_name() if actor else 'None')
            out.append(dict(target=t.get_class().get_name(), height=hgt, hits=sorted(hits),
                            bush_hit=any(h.startswith('BP_OreFlora_') for h in hits)))
    return out


def sample():
    for key, tank in S['tanks'].items():
        x = LANES[key][0] + LANES[key][4]
        track = S['track'].setdefault(key, dict(max_side_near=0.0, cross_side=None, crush=None, last=None))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - x), round(loc.y))
            if abs(loc.y) < 450:
                track['max_side_near'] = max(track['max_side_near'], abs(loc.x - x))
            if track['cross_side'] is None and loc.y < 0:
                track['cross_side'] = round(loc.x - x)
        st = standing(key)
        if st is not None and st < S['standing_before'][key] and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], standing=st)
            log(f'{key}: first crush {json.dumps(track["crush"])}')


def aim(eye, look):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def probes(name, eye, look):
    """Camera pose plus, per look piece, its centre and its plant positions (world), for bush_rev2_measure.py."""
    pts = {}
    for a in S.get('look', []):
        o = a.get_actor_location()
        row = int(round((o.y - LOOK_Y) / ROW_DY))
        nm = a.get_class().get_name().replace('BP_OreFlora_', '').replace('_C', '')
        pts[f'{nm}@{row}'] = dict(centre=[o.x, o.y, o.z], plants=[[p[0], p[1], o.z] for p in plant_points(a)])
    sw = {k: [a.get_actor_location().x + 300, a.get_actor_location().y - 300, a.get_actor_location().z] for k, a in S.get('swatches', {}).items()}
    spr = None
    if alive(S.get('spruce')):
        o = S['spruce'].get_actor_location()
        spr = [o.x, o.y, o.z]
    fov = S['camera'].camera_component.get_editor_property('field_of_view')
    S['result'].setdefault('probes', {})[name] = dict(eye=[eye.x, eye.y, eye.z], look=[look.x, look.y, look.z], fov=fov,
                                                      pieces=pts, swatches=sw, spruce=spr, ground_z=S['look_z'])


def shoot(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    S['shot_at'] = S['t']
    log(f'picture {name}')


def set_look_hidden(hidden):
    for a in S.get('look', []):
        a.set_actor_hidden_in_game(hidden)


def look_shots():
    gz = S['look_z']
    mid_y = LOOK_Y + ROW_DY

    def rts(tx, ty, d):
        return unreal.Vector(tx, ty - d * math.cos(math.radians(55)), gz + d * math.sin(math.radians(55))), unreal.Vector(tx, ty, gz)

    # the reviewer's game view: 22 m back, 34 m up (pitch about -57); the far view 45 m up
    v34 = (unreal.Vector(0, LOOK_Y - 2200 + 600, gz + 3400), unreal.Vector(0, LOOK_Y + 600, gz))
    v45 = (unreal.Vector(0, LOOK_Y - 2900 + 900, gz + 4500), unreal.Vector(0, LOOK_Y + 900, gz))
    top = (unreal.Vector(0, mid_y, gz + 4200), unreal.Vector(0, mid_y + 1, gz))
    shots = [('rts34', *v34, False), ('bg_rts34', *v34, True), ('rts45', *v45, False), ('bg_rts45', *v45, True),
             ('topdown', *top, False)]
    for nm in ('RedDogwood', 'BrambleThicket_Rust', 'BrambleThicket_Ochre', 'RedBilberryMat', 'OchreBroom'):
        shots.append(('close_' + nm, *rts(LOOK_X[nm], LOOK_Y, 1300), False))
    shots += [('close_reds_on_iron', *rts(-2200, LOOK_Y + ROW_DY, 2000), False),
              ('close_yellows_on_ochre', *rts(2000, LOOK_Y + 2 * ROW_DY, 2000), False)]
    return shots


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
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
                unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
                find_runtime(world)
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
                S['standing_before'] = {k: standing(k) for k in S['pieces']}
                S['look_z'] = ground_z(world, 0, LOOK_Y)
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 5.0:
                return
            S['nav'] = navmeshes(world)
            S['result']['setup'] = {k: setup_report(k) for k in S['pieces']}
            S['result']['look_setup'] = {}
            for a in S.get('look', []):
                o = a.get_actor_location()
                if abs(o.y - LOOK_Y) < 10:
                    nm = a.get_class().get_name()
                    b_o, b_e = a.get_actor_bounds(False)
                    S['result']['look_setup'][nm] = dict(plants=a.get_num_plants(), plants_xy=plant_points(a),
                                                         actor_bounds_extent=[round(b_e.x), round(b_e.y), round(b_e.z)])
            S['result']['standing_before'] = S['standing_before']
            S['result']['paths_before'] = {k: across_paths(world, k) for k in S['pieces']}
            for key in S['pieces']:
                log(f'{key} setup plants {S["result"]["setup"][key]["plants"]} bounds {S["result"]["setup"][key]["actor_bounds_extent"]}')
                log(f'{key} paths before {json.dumps(S["result"]["paths_before"][key])}')
            for nm, v in S['result']['look_setup'].items():
                log(f'look {nm} plants {v["plants"]} xy {v["plants_xy"]}')
            S['result']['shells'] = shell_traces(world)
            log(f'shell traces: {sum(1 for s in S["result"]["shells"] if s["bush_hit"])} of {len(S["result"]["shells"])} hit a bush')
            S['shots'] = look_shots()
            S['phase'], S['mark'] = 'look', S['t']
        elif phase == 'look':
            if S['t'] - S.get('shot_at', -10.0) < 1.5:      # the high-res shot renders a frame later
                return
            if not S['shots']:
                set_look_hidden(False)
                S['phase'] = 'orders'
                return
            name, eye, look, hide = S['shots'][0]
            if S.get('aimed') != name:
                set_look_hidden(hide)
                aim(eye, look)
                S['aimed'], S['mark'] = name, S['t']
            elif S['t'] - S['mark'] > 2.5:
                probes(name, eye, look)
                shoot(name)
                S['shots'].pop(0)
        elif phase == 'orders':
            if S['t'] - S.get('shot_at', -10.0) < 1.5:
                return
            S['order_time'] = S['t']
            S['result']['plants_before_xy'] = {k: plant_points(p) for k, p in S['pieces'].items()}
            for key, tank in S['tanks'].items():
                x = LANES[key][0] + LANES[key][4]
                target = unreal.Vector(x, -START_Y, S['origins'][key].z + 20)
                try:
                    result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)
                except Exception as error:  # noqa: BLE001
                    result = f'failed {error!r}'[:140]
                log(f'{key}: MoveToLocation {target} -> {result}')
            S['phase'], S['mark'] = 'moving', S['t']
        elif phase == 'moving':
            sample()
            elapsed = S['t'] - S['mark']
            arrived = all(isinstance(t.get('last'), tuple) and t['last'][1] < -START_Y + 400
                          for t in S['track'].values()) and len(S['track']) == len(S['tanks'])
            if elapsed > MOVE_SECONDS or (arrived and elapsed > 20.0):
                S['phase'], S['mark'] = 'settle', S['t']
                log(f'movement over after {elapsed:.0f} s, all arrived {arrived}')
        elif phase == 'settle':
            sample()
            if S['t'] - S['mark'] > 4.0:
                out = {}
                for key, (x, bp, vehicle, expected, dx) in LANES.items():
                    track = S['track'].get(key, {})
                    before, after = S['standing_before'][key], standing(key)
                    crossed = track.get('cross_side') is not None
                    if expected == 'crush':
                        ok = after is not None and after < before and crossed and abs(track['cross_side']) < 150
                    elif expected == 'partial':
                        ok = after is not None and 0 < after < before and crossed and abs(track['cross_side']) < 150
                    else:
                        ok = after == before and crossed and track.get('max_side_near', 0) > 100
                    out[key] = dict(piece=bp, vehicle=vehicle, expected=expected, passed=bool(ok), standing_before=before,
                                    standing_after=after, vehicle_dx=dx, vehicle_crossed_y0_at=track.get('cross_side'),
                                    vehicle_max_side_offset_near_piece=round(track.get('max_side_near', 0)),
                                    crush=track.get('crush'), plants_after_xy=plant_points(S['pieces'][key]))
                S['result']['lanes'] = out
                for key, row in out.items():
                    log(f'{key} verdict {json.dumps(row)}')
                S['remains'] = ['A_clump_light', 'C_hedgeOchre_medium', 'C2_hedgeRust_medium', 'D_clump_medium']
                S['phase'], S['mark'] = 'remains', S['t']
        elif phase == 'remains':
            if S['t'] - S.get('shot_at', -10.0) < 1.5:
                return
            if not S['remains']:
                S['phase'], S['mark'] = 'nav_after', S['t']
                return
            key = S['remains'][0]
            if S.get('aimed') != key:
                o = S['origins'][key]
                eye = o + unreal.Vector(0, -900, 1300)
                S['camera'].set_actor_location(eye, False, True)
                S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, o + unreal.Vector(0, 0, 10)), True)
                S['aimed'], S['mark'] = key, S['t']
            elif S['t'] - S['mark'] > 2.0:
                shoot('remains_' + key)
                S['remains'].pop(0)
        elif phase == 'nav_after' and S['t'] - S['mark'] > 1.0:
            world = game_world()
            S['result']['paths_after'] = {k: across_paths(world, k) for k in LANES}
            for key in LANES:
                log(f'{key} paths after {json.dumps(S["result"]["paths_after"][key])}')
            S['tp'] = {}
            for key in ('B_hedgeRust_light', 'B2_hedgeOchre_light'):
                tank, piece = S['tanks'].get(key), S['pieces'].get(key)
                if alive(tank) and alive(piece):
                    tank.set_actor_location(piece.get_actor_location() + unreal.Vector(0, 0, 60), False, True)
                    S['tp'][key] = piece.get_num_standing_plants()
            log(f'teleported the lane-B Luchs onto their hedges, standing {S["tp"]}')
            S['phase'], S['mark'] = 'teleport_wait', S['t']
        elif phase == 'teleport_wait' and S['t'] - S['mark'] > 2.5:
            res = {}
            for key, before in S['tp'].items():
                piece = S['pieces'].get(key)
                after = piece.get_num_standing_plants() if alive(piece) else None
                res[key] = dict(before=before, after=after, passed=before == after)
            S['result']['teleport_light_onto_hedge'] = res
            log(f'teleport check {json.dumps(res)}')
            lanes_ok = all(r['passed'] for r in S['result']['lanes'].values())
            tp_ok = len(res) == 2 and all(r['passed'] for r in res.values())
            shells_ok = not any(s['bush_hit'] for s in S['result'].get('shells', []))
            gap_ok = all(not p.get('through', True)
                         for k in ('B_hedgeRust_light', 'B2_hedgeOchre_light')
                         for p in S['result']['paths_before'][k]['LightTank_gap_probe'].values())
            S['result']['all_passed'] = bool(lanes_ok and tp_ok and shells_ok and gap_ok)
            log(f'ALL PASSED {S["result"]["all_passed"]} (lanes {lanes_ok}, teleport {tp_ok}, shells {shells_ok}, hedge gap probe {gap_ok})')
            S['phase'], S['mark'] = 'final_hold', S['t']
        elif phase == 'final_hold' and S['t'] - S['mark'] > 1.0:
            finish()
    except Exception as error:  # noqa: BLE001
        import traceback
        log(f'ERROR {phase}: {error!r} {traceback.format_exc()[-600:]}')
        finish()
    if S['t'] > 640.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
