"""Throwaway-editor test of the Berlin furniture Blueprints (furniture_blueprints_build.py), 2026-11-13.
Never on Ger_Berlin, never in the shared editor. Launch with furniture_run.sh <logname> [timeout_s] (same folder).
The map (RTS_ToCopySmall) is never saved; the editor quits at the end.

All 16 pieces stand in a row along world X at y = 0 (1200 cm apart).
1. NAV    for every piece a 10 m path across it on the Character navmesh with RTS_NavFilter_Characters (must detour)
          and on the LightTank navmesh with RTS_NavFilter_LightTanks (must be straight).
2. CRUSH  a light tank (Pz II Luchs, LightTank agent) starts 20 m in front of four pieces and gets MoveToLocation
          20 m behind them: it must drive through, the piece must die by the crush (lean / fall + sink, mesh visible
          meanwhile) and be gone afterwards.
3. SHELLS a Pz IV G gets AttackActor on three sturdy pieces: a real shell must hole at least one (hole count up, slot 0
          swapped to MI_BerlinFurniture_Holes). One more piece gets Debug_FireTestHit hits (holes without damage).
4. DEATH  two pieces take lethal damage (apply_damage): mesh hidden, collision off, life span set, actor gone later;
          the log has the RTS_WALL_HOLES collapse line with the number of pieces thrown.
Results: Saved/Berlin/furniture/test_result.json and close-up PNGs (piece_*.png before, after_*.png holed).

Batch 2 (pieces 17-32, 2026-11-14): furniture_run.sh <logname> <timeout_s> 2. The pieces and their roles are picked
from Saved/Berlin/furniture/build_report.json (two upright and two low pieces are crushed, the three sturdiest are
shelled, the next three get debug hits, the next two are killed). The run also reads its own log (LogAudio
VeryVerbose) and checks that each crushed or killed piece's own cue started at its place ("New ActiveSound <cue>").
Results: test_result_batch2.json, piece2_*.png, after2_*.png.
"""
import json
import math
import os
import re
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/sounds')
import furniture_sound_table as sounds  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/'
OUT = ROOT + 'test_result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Blueprints/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
LIGHT = V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C'
MEDIUM = V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C'
NAMES = ['01_Wardrobe', '02_IronBed', '03_KitchenTable', '04_ChairFallen', '05_Armchair', '06_Sofa', '07_Bathtub',
         '08_TiledStove', '09_Piano', '10_ChestOfDrawers', '11_BookshelfFallen', '12_KitchenDresser', '13_Desk',
         '14_Washstand', '15_ClockFallen', '16_Radiator']
X = {name: -9000 + index * 1200 for index, name in enumerate(NAMES)}
CRUSH = ['01_Wardrobe', '12_KitchenDresser', '11_BookshelfFallen', '14_Washstand']
SHELL = ['08_TiledStove', '07_Bathtub', '09_Piano']
DEBUG_HITS = ['02_IronBed']
KILL = ['05_Armchair', '13_Desk']
BATCH = int(os.environ.get('FURN_BATCH', '1'))
PIC = 'piece_' if BATCH == 1 else 'piece2_'
PIC_AFTER = 'after_' if BATCH == 1 else 'after2_'
SHOTS = ['01_Wardrobe', '06_Sofa', '09_Piano', '08_TiledStove', '02_IronBed']
if BATCH == 2:
    OUT = ROOT + 'test_result_batch2.json'
    _built = json.load(open(ROOT + 'build_report.json'))['pieces']
    _rows = sorted(((k[len('SM_BerlinFurn_'):], v) for k, v in _built.items()
                    if int(k[len('SM_BerlinFurn_'):].split('_')[0]) >= 17), key=lambda r: r[0])
    NAMES = [r[0] for r in _rows]
    X = {name: -9000 + index * 1200 for index, name in enumerate(NAMES)}
    # The three sturdiest are shelled (a 75 mm hit takes about 220 health: only they can survive one and show a hole).
    SHELL = [r[0] for r in sorted(_rows, key=lambda r: -r[1]['health'])[:3]]
    _up = [n for n, v in _rows if v['upright'] and n not in SHELL]
    _low = [n for n, v in _rows if not v['upright'] and n not in SHELL]
    CRUSH = (_up[:2] + _low[:2] + _up[2:] + _low[2:])[:4]
    _rest = sorted((r for r in _rows if r[0] not in CRUSH + SHELL), key=lambda r: -r[1]['health'])
    DEBUG_HITS = [r[0] for r in _rest[:3]]
    _rest = _rest[3:] + _rest[:3]
    KILL = [r[0] for r in _rest[:2]]
    # The pieces whose cue is checked stand together in the middle of the row, under the camera (the audio listener):
    # a sound farther than 60 m (Att_Collapse) is "out of range", and one of 1 s or less is then not started at all.
    _heard = CRUSH + KILL
    _others = [n for n in NAMES if n not in _heard]
    NAMES = _others[:5] + _heard + _others[5:]
    X = {name: -9000 + index * 1200 for index, name in enumerate(NAMES)}
    SHOTS = CRUSH + SHELL[:1]
CRUSH_Y = 2000.0
TANK_Y = 3200.0
ACTION_SECONDS = 50.0
FILTERS = '/Game/RTS_Survival/Blueprints/Nav/Filters/'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'pieces': {}, 'crushers': {}, 'shooters': {}, 'result': {},
     'track': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('FURNTEST ' + message)


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


def mesh_of(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'MainMesh'][0]


def holes_of(actor):
    return actor.get_component_by_class(unreal.WallHoleDamageComponent)


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))
    for name, x in X.items():
        cls = unreal.load_class(None, f'{BP_DIR}BP_BerlinFurn_{name}.BP_BerlinFurn_{name}_C')
        actors.spawn_actor_from_class(cls, unreal.Vector(x, 0, ground_z(world, x, 0)), unreal.Rotator(0, 0, 0))
        if name in CRUSH:
            tank = actors.spawn_actor_from_class(unreal.load_class(None, LIGHT),
                                                 unreal.Vector(x, CRUSH_Y, ground_z(world, x, CRUSH_Y) + 50),
                                                 unreal.Rotator(0, 0, -90))
            log(f'{name}: light tank placed, owner set {set_owner(tank, 1)}')
        if name in SHELL:
            tank = actors.spawn_actor_from_class(unreal.load_class(None, MEDIUM),
                                                 unreal.Vector(x, TANK_Y, ground_z(world, x, TANK_Y) + 50),
                                                 unreal.Rotator(0, 0, -90))
            log(f'{name}: Pz IV placed, owner set {set_owner(tank, 1)}')


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
        cls = actor.get_class().get_name()
        for name in NAMES:
            if cls == f'BP_BerlinFurn_{name}_C':
                S['pieces'][name] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for name in CRUSH:
            if abs(loc.x - X[name]) < 300 and abs(loc.y - CRUSH_Y) < 400:
                S['crushers'][name] = tank
        for name in SHELL:
            if abs(loc.x - X[name]) < 300 and abs(loc.y - TANK_Y) < 400:
                S['shooters'][name] = tank
    log(f'runtime pieces {len(S["pieces"])} crushers {sorted(S["crushers"])} shooters {sorted(S["shooters"])}')


def aim_camera(eye, look):
    world = game_world()
    if S.get('camera') is None:
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)


def picture(name):
    S['busy'] = True   # the screenshot pumps the tick callback
    try:
        unreal.SystemLibrary.execute_console_command(game_world(), 'ShowFlag.PostProcessMaterial 0')
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def close_up(name, prefix='piece_'):
    o = S['origins'][name]
    return prefix + name, o + unreal.Vector(260, 330, 260), o + unreal.Vector(0, 0, 70)


def run_shots():
    """One queued close-up per call sequence: aim, wait 1.6 s, shoot. True while shots are pending."""
    if not S.get('shots'):
        return False
    name, eye, look = S['shots'][0]
    if S.get('shot_mark') is None:
        aim_camera(eye, look)
        S['shot_mark'] = S['t']
    elif S['t'] - S['shot_mark'] > 1.6:
        S['shots'].pop(0)
        S['shot_mark'] = None
        picture(name)
    return True


def navmeshes(world):
    out = {}
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        for agent in ('Character', 'LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    return out


def path(world, a, b, nav_data, agent_filter):
    cls = unreal.load_class(None, f'{FILTERS}RTS_NavFilter_{agent_filter}.RTS_NavFilter_{agent_filter}_C')
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data, cls)
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:100])
    if not pts:
        return dict(length=None)
    length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))
    side = max(abs(pt.x - a.x) for pt in pts)
    return dict(length=round(length), side_cm=round(side), partial=bool(p.is_partial()),
                through=bool(side < 15 and not p.is_partial()))


def nav_checks(world):
    nav = navmeshes(world)
    out = {}
    for name in NAMES:
        z = S['origins'][name].z + 20
        a, b = unreal.Vector(X[name], 500, z), unreal.Vector(X[name], -500, z)
        out[name] = dict(character=path(world, a, b, nav.get('Character'), 'Characters'),
                         light_tank=path(world, a, b, nav.get('LightTank'), 'LightTanks'))
    return out


def setup_report(name):
    piece = S['pieces'][name]
    mesh = mesh_of(piece)
    ch = unreal.CollisionChannel
    resp = {n: str(mesh.get_collision_response_to_channel(c)).split('.')[-1].split(':')[0]
            for n, c in (('PlayerObj', ch.ECC_PLAYER_OBJ), ('EnemyObj', ch.ECC_ENEMY_OBJ), ('Projectile', ch.ECC_PROJECTILE),
                         ('TraceEnemy', ch.ECC_ENEMY), ('Pawn', ch.ECC_PAWN))}
    box = [c for c in piece.get_components_by_class(unreal.BoxComponent) if c.get_name() == 'NavBlock']
    holes = holes_of(piece)
    return dict(mesh=mesh.get_editor_property('static_mesh').get_name(), slot0=mesh.get_material(0).get_name(),
                health=piece.get_editor_property('Health'), reduction=piece.get_editor_property('DamageReduction'),
                object_type=str(mesh.get_collision_object_type()).split('.')[-1].split(':')[0], responses=resp,
                affects_nav=bool(mesh.get_editor_property('can_ever_affect_navigation')),
                nav_area=box[0].get_editor_property('area_class_override').get_name() if box else None,
                crush=str(piece.get_editor_property('CrushDestructionType')),
                tip=piece.get_editor_property('CrushTipSpeedScale'), sink=piece.get_editor_property('CrushSinkDeltaZ'),
                hole_mesh=str(holes.get_editor_property('WallMeshComponentName')) if holes else None,
                hole_material=[m.get_name() for m in holes.get_editor_property('DamagedMaterials')] if holes else None)


def piece_state(name):
    piece = S['pieces'][name]
    if not alive(piece):
        return None
    mesh = mesh_of(piece)
    rot = mesh.get_world_rotation()
    return dict(z=round(piece.get_actor_location().z - S['origins'][name].z, 1), pitch=round(rot.pitch, 1),
                roll=round(rot.roll, 1), visible=bool(mesh.is_visible()),
                collision=str(mesh.get_collision_enabled()).split('.')[-1].split(':')[0],
                life=round(piece.get_life_span(), 1), health=round(piece.get_editor_property('Health')),
                holes=holes_of(piece).get_hole_count(), slot0=mesh.get_material(0).get_name())


def sample_crush():
    for name, tank in S['crushers'].items():
        track = S['track'].setdefault(name, dict(cross_dx=None, crush=None, gone_at=None, samples=[], last=None,
                                                 sample_t=0.0))
        if alive(tank):
            loc = tank.get_actor_location()
            track['last'] = (round(loc.x - X[name]), round(loc.y))
            if track['cross_dx'] is None and loc.y < 0:
                track['cross_dx'] = round(loc.x - X[name])
        state = piece_state(name)
        if state is None:
            if track['gone_at'] is None:
                track['gone_at'] = round(S['t'] - S['order_time'], 1)
                log(f'{name}: actor gone {track["gone_at"]} s after the order')
            continue
        changed = state['z'] != 0 or abs(state['pitch']) > 0.5 or abs(state['roll']) > 0.5
        if changed and track['crush'] is None:
            track['crush'] = dict(time=round(S['t'] - S['order_time'], 1), vehicle_at=track['last'], first=state)
            log(f'{name}: crushed {json.dumps(track["crush"])}')
        if changed and len(track['samples']) < 30 and S['t'] - track['sample_t'] >= 0.25:
            track['sample_t'] = S['t']
            track['samples'].append((round(S['t'] - S['order_time'], 2), state['z'], state['pitch'], state['roll'],
                                     state['visible']))


def watch_shells():
    for name in SHELL:
        state = piece_state(name)
        key = None if state is None else (state['health'], state['holes'], state['slot0'])
        if S.setdefault('shell_seen', {}).get(name, 'start') != key:
            S['shell_seen'][name] = key
            S.setdefault('shell_log', {}).setdefault(name, []).append(
                (round(S['t'] - S['order_time'], 1), 'dead' if state is None else key))
            log(f'{name}: under fire {"dead" if state is None else key}')


def audio_starts():
    """[(cue, x, y)] of every 'New ActiveSound' line of a furniture cue in this run's own log."""
    match = re.search(r'-abslog=(\S+)', unreal.SystemLibrary.get_command_line(), re.IGNORECASE)
    if not match:
        return None
    started = []
    with open(match.group(1).strip('"'), 'r', errors='ignore') as handle:
        for line in handle:
            if 'Cue_BerlinFurn_Crush_' not in line:
                continue
            hit = re.search(r'New ActiveSound (\S+) Comp: .*Loc: X=([-\d.]+) Y=([-\d.]+)', line)
            if hit:
                started.append((hit.group(1), float(hit.group(2)), float(hit.group(3))))
    return started


def move_listener(names):
    """Camera and player pawn over the middle of these pieces, so their sounds are in range (audible to 60 m)."""
    centre = sum(X[n] for n in names) / len(names)
    z = S['origins'][names[0]].z
    aim_camera(unreal.Vector(centre, 900, z + 1500), unreal.Vector(centre, 0, z))
    controller = unreal.GameplayStatics.get_player_controller(game_world(), 0)
    pawn = controller.get_controlled_pawn() if hasattr(controller, 'get_controlled_pawn') else None
    if pawn is not None:
        pawn.set_actor_location(unreal.Vector(centre, 900, z + 1500), False, True)
    log(f'listener over x={centre:.0f} for {names} (pawn moved {pawn is not None})')


def cue_check(names):
    """Did each piece's own cue start within 4 m of where it stood? {name: dict(expected, started_here, ok)}."""
    started = audio_starts()
    out = {}
    for name in names:
        expected = sounds.cue_name(sounds.PIECE_CUE[name])
        if started is None:
            out[name] = dict(expected=expected, ok=None, note='no -abslog')
            continue
        here = sorted(set(c for c, x, y in started if math.hypot(x - X[name], y) < 400.0))
        out[name] = dict(expected=expected, started_here=here, ok=bool(here == [expected]))
    return out


def debug_hits(name):
    piece = S['pieces'][name]
    mesh, holes = mesh_of(piece), holes_of(piece)
    before = (holes.get_hole_count(), mesh.get_material(0).get_name())
    origin = piece.get_actor_location()
    removed = []
    top = mesh.get_editor_property('static_mesh').get_bounding_box().max.z
    for index, calibre in enumerate((75.0, 37.0, 88.0)):
        target = origin + unreal.Vector(-60 + 60 * index, 0, 45)
        if BATCH == 2:   # unknown shapes: aim at the body's middle height, 20 cm apart
            target = origin + unreal.Vector(-20 + 20 * index, 0, max(top * 0.5, 12.0))
        start = target + unreal.Vector(100 * (index - 1), 1500, 200)
        direction = (target - start).normal()
        hit = mesh.line_trace_component(start, start + direction * 4000, False, False, False)
        if not hit:
            removed.append('missed')
            continue
        removed.append(round(holes.debug_fire_test_hit(hit[0], direction, calibre), 3))
    after = (holes.get_hole_count(), mesh.get_material(0).get_name())
    return dict(before=before, after=after, removed_m2=removed, health=piece.get_editor_property('Health'))


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
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                find_runtime(world)
                S['origins'] = {k: p.get_actor_location() for k, p in S['pieces'].items()}
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            if S.get('shots') is None:
                S['shots'] = [close_up(n, PIC) for n in SHOTS]
            if run_shots():
                return
            S['result']['setup'] = {k: setup_report(k) for k in NAMES if k in S['pieces']}
            S['result']['nav'] = nav_checks(world)
            for name in NAMES:
                log(f'{name} setup {json.dumps(S["result"]["setup"].get(name), default=str)}')
                log(f'{name} nav {json.dumps(S["result"]["nav"].get(name))}')
            S['order_time'] = S['t']
            if BATCH == 2:
                move_listener(CRUSH + KILL)
            for name, tank in S['crushers'].items():
                target = unreal.Vector(X[name], -CRUSH_Y, S['origins'][name].z + 20)
                log(f'{name}: light tank MoveToLocation -> {tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False)}')
            for name, tank in S['shooters'].items():
                log(f'{name}: Pz IV AttackActor -> {tank.attack_actor(S["pieces"][name], True)}')
            S['result']['debug_hits'] = {name: debug_hits(name) for name in DEBUG_HITS}
            log(f'debug hits {json.dumps(S["result"]["debug_hits"])}')
            S['phase'], S['mark'] = 'action', S['t']
        elif phase == 'action':
            sample_crush()
            watch_shells()
            if S['t'] - S['mark'] > ACTION_SECONDS:
                for tank in S['shooters'].values():
                    try:
                        tank.stop_command()
                    except Exception:  # noqa: BLE001
                        pass
                crush = {}
                for name in CRUSH:
                    track = S['track'].get(name, {})
                    ok = track.get('crush') is not None and not alive(S['pieces'][name]) and \
                        track.get('cross_dx') is not None and abs(track['cross_dx']) < 100
                    crush[name] = dict(passed=bool(ok), crush=track.get('crush'), vehicle_crossed_at_dx=track.get('cross_dx'),
                                       actor_gone_s=track.get('gone_at'), vehicle_last=track.get('last'),
                                       samples=track.get('samples'))
                    log(f'{name} crush verdict {json.dumps(crush[name])}')
                S['result']['crush'] = crush
                shells = {}
                for name in SHELL:
                    state = piece_state(name)
                    history = S.get('shell_log', {}).get(name, [])
                    holed = any(h[1] != 'dead' and h[1][1] > 0 and h[1][2] == 'MI_BerlinFurniture_Holes' for h in history)
                    shells[name] = dict(end='dead' if state is None else state, history=history, holed=holed,
                                        died_under_fire=state is None)
                    log(f'{name} shell verdict {json.dumps(shells[name])}')
                S['result']['shells'] = shells
                # The main gun often misses pieces this small and MG rounds make no hole (under the minimum radius):
                # the check is that real shells holed at least one piece and that every hole swapped the material.
                S['result']['shells_passed'] = any(r['holed'] for r in shells.values())
                if BATCH == 2:
                    # The second set is light (150-260 health but for the range): a 75 mm hit usually kills outright
                    # and a killing hit leaves no hole. The requirement there is holes on three pieces by a shell or a
                    # debug hit; the real-shell result is reported, not required.
                    holed = [n for n, r in shells.items() if r['holed']] +                         [n for n, r in S['result']['debug_hits'].items() if r['after'][0] > 0]
                    S['result']['holed_pieces'] = holed
                    S['result']['real_shell_holed'] = S['result']['shells_passed']
                    S['result']['shells_passed'] = len(holed) >= 3
                kills = {}
                for name in KILL:
                    piece = S['pieces'][name]
                    unreal.GameplayStatics.apply_damage(piece, 1.0e6, None, None, unreal.DamageType)
                    kills[name] = dict(valid=alive(piece), life_span=round(piece.get_life_span(), 1),
                                       mesh_visible=bool(mesh_of(piece).is_visible()),
                                       collision=str(mesh_of(piece).get_collision_enabled()).split('.')[-1].split(':')[0],
                                       z=round(piece.get_actor_location().z - S['origins'][name].z, 1))
                    log(f'{name} killed {json.dumps(kills[name])}')
                S['kills'] = kills
                S['phase'], S['mark'] = 'after', S['t']
        elif phase == 'after' and S['t'] - S['mark'] > 6.0:
            world = game_world()
            if S.get('late_shots') is None:
                S['late_shots'] = True
                S['shots'] = [close_up(n, PIC_AFTER) for n in DEBUG_HITS + SHELL if alive(S['pieces'][n])]
            if run_shots():
                return
            for name in KILL:
                S['kills'][name]['actor_gone_6s_later'] = not alive(S['pieces'][name])
                S['kills'][name]['passed'] = bool(S['kills'][name]['actor_gone_6s_later'] and
                                                  not S['kills'][name]['mesh_visible'] and
                                                  S['kills'][name]['life_span'] > 0)
            S['result']['death_by_damage'] = S['kills']
            S['result']['nav_after'] = {name: nav_checks(world)[name] for name in CRUSH + KILL}
            S['result']['untouched_alive'] = {name: alive(S['pieces'][name]) for name in NAMES
                                              if name not in CRUSH + SHELL + KILL}
            log(f'death by damage {json.dumps(S["kills"])}')
            log(f'nav after {json.dumps(S["result"]["nav_after"])}')
            log(f'untouched alive {S["result"]["untouched_alive"]}')
            nav_ok = all(not r['character']['through'] and r['light_tank']['through'] for r in S['result']['nav'].values())
            S['result']['nav_passed'] = nav_ok
            cues = cue_check(CRUSH + KILL)
            S['result']['cues'] = cues
            # Batch 1 spreads its pieces over 156 m with no listener move: its cue result is informational only
            # (sounds/furniture_sound_test.py is the cue test of the first 16).
            S['result']['cues_passed'] = BATCH != 2 or all(c['ok'] is True for c in cues.values())
            S['result']['roles'] = dict(crush=CRUSH, shell=SHELL, debug_hits=DEBUG_HITS, kill=KILL)
            log(f'cues {json.dumps(cues)}')
            debug_ok = all(r['after'][0] > 0 and r['after'][1] == 'MI_BerlinFurniture_Holes'
                           for r in S['result']['debug_hits'].values())
            S['result']['debug_hits_passed'] = debug_ok
            S['result']['all_passed'] = bool(nav_ok and debug_ok and S['result']['cues_passed'] and all(r['passed'] for r in S['result']['crush'].values()) and
                                             S['result']['shells_passed'] and
                                             all(r['passed'] for r in S['kills'].values()))
            log(f'NAV PASSED {nav_ok} SHELLS PASSED {S["result"]["shells_passed"]} DEBUG HITS PASSED {debug_ok} '
                f'CUES PASSED {S["result"]["cues_passed"]} ALL PASSED {S["result"]["all_passed"]}')
            S['phase'], S['mark'] = 'closing', S['t']
        elif phase == 'closing' and S['t'] - S['mark'] > 3.0:   # lets the last screenshot reach the disk
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 330.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
