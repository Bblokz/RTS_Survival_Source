"""Agent B, throwaway Development editor ONLY: several two-mesh pits in one run (holes on `Sandbags`).

  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
    -ExecCmds="py <this file>" -LogCmds="LogWallHoleDamage Verbose" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Reads Saved/WallChunks/Pits2/agentB_multi_config.json: {"pits": {key: {bp, ring_radius, rim_z, debug_angles,
eye, look}}, "calm": [keys], "enemy": [keys]}. Nothing is saved: pits are spawned into the editor world before PIE.
1. Runtime readback per pit (hole component, materials, collapse flag, aim points, Sandbags / BuildingMesh collision).
2. Debug hits on each calm (owner 1) pit's -Y rim at real Sandbags collision points; a picture each.
3. Real fire on the enemy (owner 2) pits: each pooled projectile that goes dormant within 1500 cm of one is traced
   over its last segment against that pit's primitives (first hit + Sandbags-space point, engine frame for the join
   with LogWallHoleDamage). Projectiles that appear within 1000 cm of an enemy pit are logged as its own launches.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
CFG = json.load(open(ROOT + 'agentB_multi_config.json'))
OUT = ROOT + CFG.get('result', 'agentB_multi_result.json')
CALM_SPOTS = [unreal.Vector(10000, -7000, 0), unreal.Vector(12500, -7000, 0), unreal.Vector(15000, -7000, 0)]
ENEMY_SPOTS = [unreal.Vector(14000, 2800, 0), unreal.Vector(17000, 2800, 0)]
REAL_FIRE_SECONDS = 170.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'impacts': [], 'launches': [], 'proj': {},
     'health': {}, 'last_pic': 0.0, 'pic_index': 0, 'placed': []}


def log(message):
    S['log'].append(message)
    unreal.log('AGENTB_TEST ' + message)


def finish():
    try:
        json.dump({'log': S['log'], 'impacts': S['impacts'], 'launches': S['launches']}, open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, spot):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(spot.x, spot.y, 20000),
                                                 unreal.Vector(spot.x, spot.y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 100.0


def comp(actor, name):
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        if component.get_name() == name:
            return component
    return None


def owner_of(actor):
    return actor.get_component_by_class(unreal.RTSComponent).get_editor_property('owning_player')


def find_pit(world, key, owner):
    class_name = CFG['pits'][key]['bp'].split('.')[-1]
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name() == class_name and owner_of(actor) == owner:
            return actor
    return None


def short(value):
    return str(value).split('.')[-1].rstrip('>').split(':')[0]


def responses(component):
    if component is None:
        return None
    ch = unreal.CollisionChannel
    return {'enabled': short(component.get_collision_enabled()),
            'tracePlayer': short(component.get_collision_response_to_channel(ch.ECC_PLAYER)),
            'traceEnemy': short(component.get_collision_response_to_channel(ch.ECC_ENEMY)),
            'visibility': short(component.get_collision_response_to_channel(ch.ECC_VISIBILITY)),
            'placement': short(component.get_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT))}


def report_runtime(actor, tag):
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    health = actor.get_component_by_class(unreal.HealthComponent)
    sand = comp(actor, 'Sandbags')
    log(f'{tag} {actor.get_name()} owner={owner_of(actor)} hp={health.get_max_health():.0f} holeComp={holes is not None}'
        f' mesh={holes.get_editor_property("WallMeshComponentName") if holes else None}'
        f' mats={[m.get_name() if m else None for m in holes.get_editor_property("DamagedMaterials")] if holes else None}'
        f' collapse={holes.get_editor_property("CollapseSettings").get_editor_property("collapse_into_chunks_on_death") if holes else None}'
        f' sandbagMesh={sand.get_editor_property("static_mesh").get_name() if sand else None}')
    log(f'{tag}  aim={[(round(v.x), round(v.y), round(v.z)) for v in actor.get_editor_property("AimOffsetPoints")]}')
    log(f'{tag}  Sandbags {responses(sand)}  BuildingMesh {responses(comp(actor, "BuildingMeshComponent"))}')


def debug_hits(key, actor):
    cfg = CFG['pits'][key]
    sand = comp(actor, 'Sandbags')
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    calibres = [37, 75, 105, 50]
    centre = actor.get_actor_location()
    for index, angle in enumerate(cfg['debug_angles']):
        radians = math.radians(angle)
        out = unreal.Vector(math.cos(radians), math.sin(radians), 0)
        start = centre + out * (cfg['ring_radius'] + 900) + unreal.Vector(0, 0, cfg['rim_z'])
        direction = out * -1.0
        result = sand.line_trace_component(start, start + direction * 4000, False, False, False)
        if not result:
            log(f'debug {key} angle {angle} missed the Sandbags collision')
            continue
        local = sand.get_world_transform().inverse_transform_location(result[0])
        removed = holes.debug_fire_test_hit(result[0], direction, float(calibres[index % 4]))
        log(f'debug {key} {calibres[index % 4]} mm at sandbag-local ({local.x:.0f},{local.y:.0f},{local.z:.0f}) '
            f'removed {removed:.2f} m2')
    log(f'debug {key} holes={holes.get_hole_count()} mat0={sand.get_material(0).get_name()}')


def pit_primitives(actor):
    found = []
    actors = [actor]
    for child_component in actor.get_components_by_class(unreal.ChildActorComponent):
        child = child_component.get_editor_property('child_actor')
        if child is not None:
            actors.append(child)
    for owner in actors:
        for component in owner.get_components_by_class(unreal.PrimitiveComponent):
            if component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                continue
            if component.get_name() == 'bpSelectionArea':
                continue
            name = component.get_name() if owner == actor else owner.get_class().get_name() + '.' + component.get_name()
            found.append((name, component))
    return found


def first_component_hit(actor, start, end):
    best = None
    for name, component in pit_primitives(actor):
        result = component.line_trace_component(start, end, False, False, False)
        if not result:
            continue
        distance = (result[0] - start).length()
        if best is None or distance < best[0]:
            best = (distance, name, result[0])
    return best


def track_projectiles(world, enemies, delta):
    step_time = max(delta, 0.016)
    frame = unreal.SystemLibrary.get_frame_count()
    for projectile in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Projectile):
        key = projectile.get_name()
        hidden = projectile.get_editor_property('hidden')
        location = projectile.get_actor_location()
        velocity = projectile.get_velocity()
        previous = S['proj'].get(key)
        S['proj'][key] = (hidden, location, velocity)
        if not hidden and (previous is None or previous[0]):
            for pit_key, enemy in enemies.items():
                offset = location - enemy.get_actor_location()
                if math.hypot(offset.x, offset.y) < 1000:
                    S['launches'].append({'pit': pit_key, 't': round(S['t'], 2), 'frame': frame,
                                          'local': [round(offset.x), round(offset.y), round(offset.z)],
                                          'vel': [round(velocity.x), round(velocity.y), round(velocity.z)]})
            continue
        if previous is None or previous[0]:
            continue
        prev_location, prev_velocity = previous[1], previous[2]
        if prev_velocity.length() < 1:
            continue
        expected = prev_location + prev_velocity * step_time
        reused = not hidden and (location - expected).length() > prev_velocity.length() * step_time * 2 + 300
        if not hidden and not reused:
            continue
        for pit_key, enemy in enemies.items():
            centre = enemy.get_actor_location()
            if (prev_location - centre).length() > 1500:
                continue
            transform = comp(enemy, 'Sandbags').get_world_transform()
            direction = prev_velocity.normal()
            step = prev_velocity.length() * step_time
            moved = (location - prev_location).length()
            end = location + direction * 300 if (moved < step * 4 and not reused) else prev_location + direction * (step * 3 + 300)
            best = first_component_hit(enemy, prev_location - direction * 50, end)
            start_local = transform.inverse_transform_location(prev_location)
            record = {'pit': pit_key, 't': round(S['t'], 2), 'frame': frame, 'reused': reused,
                      'start_local': [round(start_local.x), round(start_local.y), round(start_local.z)],
                      'speed': round(prev_velocity.length())}
            if best is None:
                end_local = transform.inverse_transform_location(location)
                record.update({'comp': None, 'end_local': [round(end_local.x), round(end_local.y), round(end_local.z)]})
            else:
                hit_local = transform.inverse_transform_location(best[2])
                record.update({'comp': best[1], 'hit_local': [round(hit_local.x), round(hit_local.y), round(hit_local.z)]})
            S['impacts'].append(record)


def note_health(enemies):
    for pit_key, enemy in enemies.items():
        health = enemy.get_component_by_class(unreal.HealthComponent)
        holes = enemy.get_component_by_class(unreal.WallHoleDamageComponent)
        current = (round(health.get_current_health(), 1), holes.get_hole_count())
        if current != S['health'].get(pit_key):
            S['health'][pit_key] = current
            log(f't={S["t"]:.2f} frame={unreal.SystemLibrary.get_frame_count()} {pit_key} hp={current[0]} '
                f'holes={current[1]}')


def aim_camera(world, key, owner):
    # Moving a placed camera at runtime lagged a view behind; each view has its own camera placed before PIE.
    eye = S['views'][(key, owner)]
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - eye).length())
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(S['camera'], 0.0)


def picture(path):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, path, camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {path}')


def live_enemies(world):
    enemies = {}
    for key in CFG['enemy']:
        actor = find_pit(world, key, 2)
        if actor is not None:
            enemies[key] = actor
    return enemies


def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            plan = [(key, 1, CALM_SPOTS[i]) for i, key in enumerate(CFG['calm'])]
            plan += [(key, 2, ENEMY_SPOTS[i]) for i, key in enumerate(CFG['enemy'])]
            for key, owner, spot in plan:
                spot.z = ground_z(world, spot)
                placed = actors.spawn_actor_from_class(unreal.load_class(None, CFG['pits'][key]['bp']), spot)
                placed.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', owner)
                log(f'placed {key} {placed.get_name()} owner {owner} at ({spot.x:.0f},{spot.y:.0f},{spot.z:.0f})')
            S['views'] = {}
            for key, owner, spot in plan:
                cfg = CFG['pits'][key]
                eye_offset = cfg['eye'] if owner == 1 else cfg['enemy_eye']
                eye = spot + unreal.Vector(*eye_offset)
                look = spot + unreal.Vector(*cfg['look'])
                camera = actors.spawn_actor_from_class(unreal.CameraActor, eye,
                                                       unreal.MathLibrary.find_look_at_rotation(eye, look))
                S['views'][(key, owner)] = eye
            actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(12500, -10000, 1500))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(12500, -10000, 1500)).length())
                for key in CFG['calm']:
                    report_runtime(find_pit(world, key, 1), 'calm ' + key)
                for key in CFG['enemy']:
                    report_runtime(find_pit(world, key, 2), 'enemy ' + key)
                for key in CFG['calm']:
                    debug_hits(key, find_pit(world, key, 1))
                S['calm_queue'] = list(CFG['calm'])
                S['phase'], S['mark'] = 'calm_aim', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'calm_aim':
            world = game_world()
            if not S['calm_queue']:
                S['phase'], S['fire_start'], S['last_pic'] = 'real_fire', S['t'], S['t']
                return
            key = S['calm_queue'][0]
            cfg = CFG['pits'][key]
            aim_camera(world, key, 1)
            S['phase'], S['mark'] = 'calm_pic', S['t']
        elif phase == 'calm_pic' and S['t'] - S['mark'] > 2.5:
            key = S['calm_queue'].pop(0)
            S['phase'] = 'calm_aim'
            picture(f'{ROOT}{key}/debug_holes.png')
        elif phase in ('real_fire', 'enemy_pic'):
            world = game_world()
            enemies = live_enemies(world)
            track_projectiles(world, enemies, delta)
            note_health(enemies)
            if phase == 'enemy_pic':
                if S['t'] - S['pic_mark'] > 1.0:
                    S['phase'], S['last_pic'] = 'real_fire', S['t']
                    picture(f'{ROOT}{S["pic_key"]}/real_fire_{S["pic_index"]}.png')
                return
            if not enemies or S['t'] - S['fire_start'] > REAL_FIRE_SECONDS:
                log(f't={S["t"]:.1f} end of real fire, enemies left {list(enemies)}')
                for key, enemy in enemies.items():
                    report_runtime(enemy, 'end ' + key)
                finish()
                return
            if S['t'] - S['last_pic'] > 25.0:
                S['pic_index'] += 1
                keys = sorted(enemies)
                S['pic_key'] = keys[S['pic_index'] % len(keys)]
                cfg = CFG['pits'][S['pic_key']]
                aim_camera(world, S['pic_key'], 2)
                S['phase'], S['pic_mark'] = 'enemy_pic', S['t']
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 340.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
