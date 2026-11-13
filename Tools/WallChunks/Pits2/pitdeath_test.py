"""Pit death check (2026-11-12), throwaway editor ONLY (run with pitdeath_run_headless.sh).
Places every UPitDefenseComponent pit in UT_VehicleObstacle's editor world (map never saved), starts PIE, kills them all
with ApplyDamage and logs per pit at +0.5..12 s: actor alive / sink depth / weapon-hit collider state, and the wreck
actors that spawned at the pit (class, XY offset, Z offset, yaw offset). Writes Saved/WallChunks/Pits2/pitdeath_result.json.
Expected: 'wreck' pits are gone by ~2 s with their two wreck actors at the pit's XY/Z/yaw and no sink;
'sink' pits sink (dz < 0) and are gone after the 3 s sink, with no wreck spawned.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
OUT = ROOT + 'pitdeath_result.json'
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/'
B4_WRECK = ['BP_Obstacle_B4Pit_Destroyed_C', 'DestroyedTW_B4_C']
# key: (class path, expected death, expected wreck classes)
PITS = {
    'Pak40': (BXP + 'GerPits/Ger_BXP_GerPit_Pak40', 'sink', []),
    'Flak37': (BXP + 'GerPits/Ger_BXP_GerPit_Flak37', 'sink', []),
    'LeFH18': (BXP + 'GerPits/Ger_BXP_GerPit_LeFH18', 'sink', []),
    'Panzerschreck': (BXP + 'GerPits/Ger_BXP_GerPit_Panzerschreck', 'sink', []),
    'Puppchen': (BXP + 'GerPits/Ger_BXP_GerPit_Puppchen', 'sink', []),
    'SFH18': (BXP + 'GerPits/Ger_BXP_GerPit_SFH18', 'sink', []),
    'TwinMG42': (BXP + 'GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42', 'sink', []),
    '53K': (BXP + 'Rus53KPit/Rus_BXP_53KPit', 'sink', []),
    '53KSector': (BXP + 'Rus53KPit/Rus_BXP_53KSectorPit', 'sink', []),
    'Ampulomet': (BXP + 'RusAmpulometPit/Rus_BXP_AmpulometPit', 'sink', []),
    'DShK': (BXP + 'RusDShKPit/Rus_BXP_DShKPit', 'sink', []),
    'BS3': (BXP + 'RusBS3Pit/Rus_BXP_BS3Pit', 'sink', []),
    'AssemblyYard': (BXP + 'RusBuildings/AssemblyYard/Rus_Building_AssemblyYard', 'sink', []),
    'Mortar120': (BXP + 'RusMortarPit/Rus_BXP_120MortarPit', 'sink', []),
    # Never died until PitDefense was added (2026-11-12).
    'GerFlak88': (BXP + 'GerPits/Ger_BXP_GerPit_Flak88', 'sink', []),
    'GerMortar': (BXP + 'GerPits/Ger_BXP_GerPit_Mortar', 'sink', []),
    'B4': (TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerPit', 'wreck', B4_WRECK),
    'B4Captured': (TW + 'B4_203MM/Pit/Ger_BXP_CapturedB4Pit', 'wreck', B4_WRECK),
    'B4Concrete': (TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit', 'wreck', B4_WRECK),
    'B4Stepped': (TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerSteppedSquarePit', 'wreck',
                  ['BP_Obstacle_B4Pit_SteppedSquare_Destroyed_C', 'DestroyedTW_B4_C']),
    'ML20': (TW + 'ML20_152MM/Pit/Rus_BXP_ML20Pit', 'wreck', ['BP_Obstacle_B4Pit_Destroyed_C', 'DestroyedTW_ML20_C']),
    'ML20Concrete': (TW + 'ML20_152MM/Pit/Rus_BXP_ML20ConcretePit', 'wreck',
                     ['BP_Obstacle_B4Pit_Destroyed_C', 'DestroyedTW_ML20_C']),
    'ML20Twin': (TW + 'ML20_152MM/Pit/Rus_BXP_ML20TwinShouldersPit', 'wreck',
                 ['BP_Obstacle_ML20Pit_TwinShoulders_Destroyed_C', 'DestroyedTW_ML20_C']),
}
GRID_X0, GRID_Y0, GRID_STEP, GRID_COLUMNS = 6000.0, -7000.0, 2600.0, 7
PIT_YAW = 30.0
CHECK_TIMES = [0.5, 2.0, 3.0, 4.5, 8.0, 12.0]
# Off: on 2026-11-12 the shots framed the map's tank park instead of the pit (camera placement needs fixing first).
PICTURE_PIT, PICTURE_TIMES = 'B4Stepped', ()
WRECK_RADIUS = 600.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'placed': [], 'kills': {}, 'checks': list(CHECK_TIMES),
     'result': {}}


def log(message):
    S['log'].append(message)
    unreal.log('PITDEATH_TEST ' + message)


def finish():
    try:
        json.dump({'log': S['log'], 'result': S['result']}, open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return (hit.to_tuple()[4].z, True) if hit else (100.0, False)


def class_path(key):
    path = PITS[key][0]
    return '%s.%s_C' % (path, path.split('/')[-1])


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for index, key in enumerate(PITS):
        x = GRID_X0 + (index % GRID_COLUMNS) * GRID_STEP
        y = GRID_Y0 - (index // GRID_COLUMNS) * GRID_STEP
        z, grounded = ground_z(world, x, y)
        placed = actors.spawn_actor_from_class(unreal.load_class(None, class_path(key)), unreal.Vector(x, y, z),
                                               unreal.Rotator(0, 0, PIT_YAW))
        placed.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', 2)
        S['placed'].append((key, x, y, z))
        log(f'placed {key} at ({x:.0f},{y:.0f},{z:.0f}) grounded={grounded} yaw={placed.get_actor_rotation().yaw:.0f}')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(GRID_X0, GRID_Y0 - 9000, 1500))


def find_pits(world):
    found = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BuildingExpansion):
        location = actor.get_actor_location()
        for key, x, y, _ in S['placed']:
            if actor.get_class().get_name() == class_path(key).split('.')[-1] and abs(location.x - x) < 50 \
                    and abs(location.y - y) < 50:
                found[key] = actor
    return found


def collider_state(actor):
    states = []
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        if component.get_name().startswith('PitWeaponHitCollider'):
            states.append('%s:%s' % (component.get_name(), str(component.get_collision_enabled()).split('.')[-1]
                                     .split(':')[0]))
    return ','.join(states) if states else 'none'


def kill_all():
    found = find_pits(game_world())
    log(f'pie test pits {len(found)} of {len(S["placed"])}')
    for key, x, y, z in S['placed']:
        actor = found.get(key)
        if actor is None:
            log(f'{key} MISSING in PIE')
            continue
        defenses = actor.get_components_by_class(unreal.PitDefenseComponent)
        holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
        collapse = holes.get_editor_property('CollapseSettings').get_editor_property('collapse_into_chunks_on_death') \
            if holes else None
        health = actor.get_component_by_class(unreal.HealthComponent)
        log(f'{key} before: pitDefense={len(defenses)} collider={collider_state(actor)} collapseIntoChunks={collapse} '
            f'hp={health.get_current_health():.0f}')
        S['kills'][key] = (actor, actor.get_actor_location(), actor.get_actor_rotation().yaw)
        S['result'][key] = {'expected': PITS[key][1], 'pitDefense': len(defenses), 'collapseIntoChunks': collapse,
                            'checks': {}}
    for key, (actor, _, _) in S['kills'].items():
        health = actor.get_component_by_class(unreal.HealthComponent)
        unreal.GameplayStatics.apply_damage(actor, health.get_max_health() * 10.0, None, None, unreal.DamageType)
        log(f'{key} right after kill: hp={health.get_current_health():.0f} collider={collider_state(actor)}')


def wrecks_near(world, location, yaw):
    result = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        if 'Destroyed' not in name:
            continue
        offset = actor.get_actor_location() - location
        if math.hypot(offset.x, offset.y) > WRECK_RADIUS:
            continue
        result.append({'class': name, 'dxy': round(math.hypot(offset.x, offset.y)), 'dz': round(offset.z),
                       'dyaw': round((actor.get_actor_rotation().yaw - yaw + 180) % 360 - 180)})
    return result


def picture(due):
    actor, location, yaw = S['kills'][PICTURE_PIT]
    camera = S.get('camera')
    if camera is None:
        return
    eye = location + unreal.Vector(-900, -1700, 1100)
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, location + unreal.Vector(0, 0, 100)), True)
    name = ROOT + 'pitdeath_%s_%ss.png' % (PICTURE_PIT, str(due).replace('.', '_'))
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, name, camera=camera)
    log(f'picture {name}')


def check(due):
    world = game_world()
    if due in PICTURE_TIMES:
        picture(due)
    for key, (actor, location, yaw) in S['kills'].items():
        try:
            alive = unreal.SystemLibrary.is_valid(actor) and not actor.is_actor_being_destroyed()
        except Exception:  # noqa: BLE001
            alive = False
        entry = {'alive': alive, 'wrecks': wrecks_near(world, location, yaw)}
        if alive:
            entry['dz'] = round(actor.get_actor_location().z - location.z)
            entry['collider'] = collider_state(actor)
        S['result'][key]['checks'][due] = entry
        log(f'+{due}s {key}: {entry}')


def verdicts():
    for key, data in S['result'].items():
        expected, wreck_classes = PITS[key][1], PITS[key][2]
        last = data['checks'].get(CHECK_TIMES[-1], {})
        sank = any(c.get('dz', 0) < -50 for c in data['checks'].values())
        classes = sorted(w['class'] for w in last.get('wrecks', []))
        if expected == 'probe':
            data['verdict'] = f'OBSERVED alive_at_12s={last.get("alive")} sank={sank} wrecks={classes}'
            log(f'VERDICT {key} (probe): {data["verdict"]}')
            continue
        problems = []
        live_colliders = sorted({c['collider'] for c in data['checks'].values()
                                 if c.get('alive') and 'NO_COLLISION' not in c.get('collider', '')})
        if live_colliders:
            problems.append(f'collider still on after death {live_colliders}')
        if last.get('alive', True):
            problems.append('still alive')
        if expected == 'sink' and not sank:
            problems.append('never sank')
        if expected == 'wreck':
            if sank:
                problems.append('sank')
            if classes != sorted(wreck_classes):
                problems.append(f'wrecks {classes} != {sorted(wreck_classes)}')
            for wreck in last.get('wrecks', []):
                if wreck['dxy'] > 10 or abs(wreck['dz']) > 10 or wreck['dyaw'] != 0:
                    problems.append(f'{wreck["class"]} offset dxy={wreck["dxy"]} dz={wreck["dz"]} dyaw={wreck["dyaw"]}')
        elif classes:
            problems.append(f'unexpected wrecks {classes}')
        data['verdict'] = 'PASS' if not problems else 'FAIL: ' + '; '.join(problems)
        log(f'VERDICT {key} ({expected}): {data["verdict"]}')


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            if game_world() is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(game_world(), False)
                cams = unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.CameraActor)
                target = unreal.Vector(GRID_X0, GRID_Y0 - 9000, 1500)
                S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - target).length()) if cams else None
                kill_all()
                S['phase'], S['mark'] = 'watch', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'watch':
            if S['checks'] and S['t'] - S['mark'] >= S['checks'][0]:
                check(S['checks'].pop(0))
            if not S['checks']:
                verdicts()
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 300.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
