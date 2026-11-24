"""Throwaway-editor test of the Berlin furniture crush sounds. Never on Ger_Berlin, never in the shared editor.
Launch with furniture_sound_run.sh <logname> [timeout_s] (same folder). The map (RTS_ToCopySmall) is never saved.

Row 1 (y = 0): all 16 pieces, a Pz II Luchs 20 m in front of each. Row 2 (y = ROW2_Y): all 16 pieces again.
1. DEFAULTS  every spawned piece (both rows) must hold its cue from furniture_sound_table.py in CrushSound and in
             WallHoleDamage.CollapseSettings.CollapseSound; every cue must be Modulator -> Random -> 3 waves.
2. CRUSH     the tanks drive through row 1, eight at a time with the camera (= the audio listener) over them.
3. DEATH     the row 2 pieces take lethal damage one after another (CollapseIntoPieces path).
4. LOG       the editor runs with LogAudio VeryVerbose; the audio device then logs
             "New ActiveSound <cue> Comp: .. Loc: .." for every sound that really started (after the audible-range and
             concurrency checks). For each piece the test wants that line with the piece's own cue at the piece's place,
             once for the crush and once for the death, and no furniture piece may start any other cue.
Result: Saved/Berlin/furniture/sounds/sound_test_result.json. This proves the right cue started as an active sound at
the right place; it does not prove how it sounds.
"""
import json
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import furniture_sound_table as table  # noqa: E402

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds/sound_test_result.json'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Blueprints/'
LIGHT = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII_Luchs/'
         'Ger_Vehicle_PanzerII_Luchs.Ger_Vehicle_PanzerII_Luchs_C')
NAMES = sorted(table.PIECE_CUE)
X = {name: -9000 + index * 1200 for index, name in enumerate(NAMES)}
GROUPS = [NAMES[:8], NAMES[8:]]
CRUSH_Y = 2000.0
ROW2_Y = 4200.0
CRUSH_SECONDS = 16.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'row1': {}, 'row2': {}, 'tanks': {}, 'result': {}, 'group': 0,
     'crush_seen': {}}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('FURNSOUND ' + message)


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
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else None


def name_of(obj):
    return obj.get_name() if obj else None


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 2100, 1800))
    for name, x in X.items():
        cls = unreal.load_class(None, f'{BP_DIR}BP_BerlinFurn_{name}.BP_BerlinFurn_{name}_C')
        for y in (0.0, ROW2_Y):
            z = ground_z(world, x, y)
            if z is None:
                log(f'{name}: no ground at y={y}')
                z = 0.0
            actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
        tank = actors.spawn_actor_from_class(unreal.load_class(None, LIGHT),
                                             unreal.Vector(x, CRUSH_Y, (ground_z(world, x, CRUSH_Y) or 0.0) + 50),
                                             unreal.Rotator(0, 0, -90))
        comp = tank.get_component_by_class(unreal.RTSComponent) if tank else None
        if comp is not None:
            comp.set_editor_property('owning_player', 1)


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
        cls = actor.get_class().get_name()
        for name in NAMES:
            if cls == f'BP_BerlinFurn_{name}_C':
                S['row1' if abs(actor.get_actor_location().y) < 500 else 'row2'][name] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for name in NAMES:
            if abs(loc.x - X[name]) < 300 and abs(loc.y - CRUSH_Y) < 400:
                S['tanks'][name] = tank
    log(f'runtime row1 {len(S["row1"])} row2 {len(S["row2"])} tanks {len(S["tanks"])}')


def move_listener(group):
    """Camera over the middle of the group: the farthest piece of either row is about 50 m away (audible to 60 m)."""
    world = game_world()
    if S.get('camera') is None:
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 2100, 1800)).length())
    centre = sum(X[n] for n in group) / len(group)
    eye = unreal.Vector(centre, 2100, 1800)
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, unreal.Vector(centre, 2100, 0)), True)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    controller.set_view_target_with_blend(camera, 0.0)
    pawn = controller.get_controlled_pawn() if hasattr(controller, 'get_controlled_pawn') else None
    if pawn is not None:
        pawn.set_actor_location(eye, False, True)
    log(f'listener over x={centre:.0f} (camera {eye.x:.0f},{eye.y:.0f},{eye.z:.0f}; pawn moved {pawn is not None})')


def cue_shape(key):
    cue = unreal.load_asset(table.cue_path(key))
    if cue is None:
        return dict(missing=True, ok=False)
    modulator = cue.get_editor_property('first_node')
    random = list(modulator.get_editor_property('child_nodes'))[0] if isinstance(modulator, unreal.SoundNodeModulator) else None
    players = list(random.get_editor_property('child_nodes')) if isinstance(random, unreal.SoundNodeRandom) else []
    waves = [name_of(p.get_editor_property('sound_wave')) for p in players if isinstance(p, unreal.SoundNodeWavePlayer)]
    wanted = [table.wave_name(key, v) for v in range(3)]
    info = dict(waves=waves, volume=round(cue.get_editor_property('volume_multiplier'), 3),
                duration=round(cue.get_editor_property('duration'), 2),
                pitch=[round(modulator.get_editor_property('pitch_min'), 3), round(modulator.get_editor_property('pitch_max'), 3)]
                if random else None, sound_class=name_of(cue.get_editor_property('sound_class_object')))
    info['ok'] = bool(waves == wanted and 0.6 <= info['duration'] <= 1.8 and abs(info['volume'] - table.CUE_VOLUME) < 0.001)
    return info


def defaults():
    out = {}
    for name in NAMES:
        wanted = table.cue_name(table.PIECE_CUE[name])
        row = dict(wanted=wanted)
        for key in ('row1', 'row2'):
            piece = S[key].get(name)
            if piece is None:
                row[key] = None
                continue
            holes = piece.get_component_by_class(unreal.WallHoleDamageComponent)
            row[key] = dict(crush=name_of(piece.get_editor_property('CrushSound')),
                            death=name_of(holes.get_editor_property('CollapseSettings').get_editor_property('collapse_sound')))
        cdo = unreal.get_default_object(unreal.load_class(None, f'{BP_DIR}BP_BerlinFurn_{name}.BP_BerlinFurn_{name}_C'))
        row['class_default'] = name_of(cdo.get_editor_property('CrushSound'))
        row['ok'] = bool(row['class_default'] == wanted and all(
            row[key] is not None and row[key]['crush'] == wanted and row[key]['death'] == wanted for key in ('row1', 'row2')))
        out[name] = row
    return out


def watch_crush(group):
    for name in group:
        piece = S['row1'].get(name)
        if name in S['crush_seen'] or not alive(piece):
            continue
        origin = S['origins'][name]
        mesh = [c for c in piece.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'MainMesh'][0]
        rot = mesh.get_world_rotation()
        if piece.get_actor_location().z != origin.z or abs(rot.pitch) > 0.5 or abs(rot.roll) > 0.5:
            S['crush_seen'][name] = round(S['t'] - S['mark'], 1)
            log(f'{name}: crushed {S["crush_seen"][name]} s after the order')


def read_audio_log():
    """-> [(cue, x, y)] of every 'New ActiveSound' line of a furniture cue, plus the count of rejected starts."""
    match = re.search(r'-abslog=(\S+)', unreal.SystemLibrary.get_command_line(), re.IGNORECASE)
    if not match:
        return None, None, 'no -abslog on the command line'
    path = match.group(1).strip('"')
    started, rejected = [], []
    with open(path, 'r', errors='ignore') as handle:
        for line in handle:
            if 'Cue_BerlinFurn_Crush_' not in line:
                continue
            hit = re.search(r'New ActiveSound (\S+) Comp: .*Loc: X=([-\d.]+) Y=([-\d.]+)', line)
            if hit:
                started.append((hit.group(1), float(hit.group(2)), float(hit.group(3))))
            elif 'Virtualizing' in line or 'Failed to pass' in line:
                rejected.append(line.strip()[-160:])
    return started, rejected, path


def verdict():
    started, rejected, path = read_audio_log()
    result = S['result']
    result['audio_log'] = path
    if started is None:
        result['audio_lines'] = None
        result['all_passed'] = False
        return
    result['audio_lines'] = len(started)
    result['rejected_starts'] = rejected
    pieces, strays = {}, []
    for cue, x, y in started:
        owner = [n for n in NAMES if abs(x - X[n]) < 400]
        row = 'crush' if abs(y) < 600 else ('death' if abs(y - ROW2_Y) < 600 else None)
        if not owner or row is None:
            strays.append((cue, x, y))
            continue
        pieces.setdefault(owner[0], {'crush': [], 'death': []})[row].append(cue)
    result['stray_lines'] = strays
    out = {}
    for name in NAMES:
        wanted = table.cue_name(table.PIECE_CUE[name])
        got = pieces.get(name, {'crush': [], 'death': []})
        out[name] = dict(wanted=wanted, crush_started=got['crush'], death_started=got['death'],
                         crushed_after_s=S['crush_seen'].get(name), row1_gone=not alive(S['row1'].get(name)),
                         row2_gone=not alive(S['row2'].get(name)),
                         crush_ok=got['crush'] == [wanted], death_ok=got['death'] == [wanted])
        log(f'{name} sound verdict {json.dumps(out[name])}')
    result['pieces'] = out
    result['crush_passed'] = all(r['crush_ok'] for r in out.values())
    result['death_passed'] = all(r['death_ok'] for r in out.values())
    result['defaults_passed'] = all(r['ok'] for r in result['defaults'].values())
    result['cues_passed'] = all(r['ok'] for r in result['cues'].values())
    result['all_passed'] = bool(result['crush_passed'] and result['death_passed'] and result['defaults_passed'] and
                                result['cues_passed'] and not strays)
    log(f'DEFAULTS {result["defaults_passed"]} CUES {result["cues_passed"]} CRUSH {result["crush_passed"]} '
        f'DEATH {result["death_passed"]} ALL PASSED {result["all_passed"]}')


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT, 'w'), indent=1, default=str)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
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
                S['origins'] = {k: p.get_actor_location() for k, p in S['row1'].items()}
                S['phase'], S['mark'] = 'registered', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'registered':
            world = game_world()
            if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 4.0:
                return
            S['result']['defaults'] = defaults()
            S['result']['cues'] = {table.cue_name(c['key']): cue_shape(c['key']) for c in table.CUES}
            for name in NAMES:
                log(f'{name} defaults {json.dumps(S["result"]["defaults"][name])}')
            for key, info in S['result']['cues'].items():
                log(f'{key} {json.dumps(info)}')
            S['phase'] = 'listen'
        elif phase == 'listen':
            move_listener(GROUPS[S['group']])
            S['phase'], S['mark'] = 'order', S['t']
        elif phase == 'order' and S['t'] - S['mark'] > 1.5:
            S['pending'] = list(GROUPS[S['group']])
            S['phase'], S['mark'], S['next'] = 'crush', S['t'], S['t']
        elif phase == 'crush':
            group = GROUPS[S['group']]
            if S['pending'] and S['t'] >= S['next']:      # one tank every 0.5 s: the crushes do not all land at once
                name = S['pending'].pop(0)
                tank = S['tanks'].get(name)
                target = unreal.Vector(X[name], -CRUSH_Y, S['origins'][name].z + 20)
                ok = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90), False) if tank else None
                log(f'{name}: light tank MoveToLocation -> {ok}')
                S['next'] = S['t'] + 0.5
            watch_crush(group)
            if S['t'] - S['mark'] > CRUSH_SECONDS:
                S['pending'] = list(group)
                S['phase'], S['mark'], S['next'] = 'kill', S['t'], S['t']
        elif phase == 'kill':
            if S['pending'] and S['t'] >= S['next']:
                name = S['pending'].pop(0)
                piece = S['row2'].get(name)
                if alive(piece):
                    unreal.GameplayStatics.apply_damage(piece, 1.0e6, None, None, unreal.DamageType)
                    log(f'{name}: row 2 piece killed by damage, life span {piece.get_life_span():.1f}')
                S['next'] = S['t'] + 0.6
            if not S['pending'] and S['t'] - S['next'] > 3.0:
                S['group'] += 1
                S['phase'] = 'listen' if S['group'] < len(GROUPS) else 'settle'
                S['mark'] = S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 4.0:
            unreal.SystemLibrary.execute_console_command(game_world(), 'log flush')
            S['phase'], S['mark'] = 'verdict', S['t']
        elif phase == 'verdict' and S['t'] - S['mark'] > 2.0:
            verdict()
            finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 300.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
