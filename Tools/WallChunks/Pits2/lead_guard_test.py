"""Lead copy for the hole guard test (2026-11-12): also fires mid-pit debug hits that must be skipped.
Agent B, throwaway Development editor ONLY: real tank fire + debug hits on a two-mesh pit (holes on `Sandbags`).

  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
    -ExecCmds="py <this file>" -LogCmds="LogWallHoleDamage Verbose" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Reads Saved/WallChunks/Pits2/agentB_test_config.json ({"key", "bp", "ring_radius", "rim_z", "square"}).
1. Spawns an ENEMY pit (owner 2) north of the map's player tanks and a calm player pit (owner 1) behind the south wall,
   both in the editor world before PIE (never saved).
2. Logs runtime values: hole component, DamagedMaterials, collapse flag, aim points, Sandbags/BuildingMesh collision.
3. Debug hits (37-105 mm) on the calm pit's shooter-facing rim, at real Sandbags collision points.
4. Real fire on the enemy pit: every pooled projectile that goes dormant near it is traced over its last segment
   against Sandbags, BuildingMeshComponent and PitWeaponHitCollider; the first component hit and its Sandbags-space
   point are logged (agentB_bagdistance.py measures how far each lies from the bags).
5. Pictures with a placed CameraActor (HighResShot).
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
CFG = json.load(open(ROOT + 'agentB_test_config.json'))
OUT_DIR = ROOT + CFG['key'] + '/guard/'
OUT = OUT_DIR + 'headless_result.json'
ENEMY_SPOT = unreal.Vector(15500, 2600, 0)
CALM_SPOT = unreal.Vector(12500, -7000, 0)
REAL_FIRE_SECONDS = 150.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'impacts': [], 'proj': {}, 'views': [],
     'last_pic': 0.0, 'pic_index': 0, 'health': None}


def log(message):
    S['log'].append(message)
    unreal.log('AGENTB_TEST ' + message)


def finish():
    try:
        json.dump({'log': S['log'], 'impacts': S['impacts']}, open(OUT, 'w'), indent=1)
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


def pits(world):
    class_name = CFG['bp'].split('.')[-1]
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def owner_of(actor):
    return actor.get_component_by_class(unreal.RTSComponent).get_editor_property('owning_player')


def pit_by_owner(world, owner):
    for actor in pits(world):
        if owner_of(actor) == owner:
            return actor
    return None


def responses(component):
    if component is None:
        return None
    ch = unreal.CollisionChannel
    return {'enabled': str(component.get_collision_enabled()).split('.')[-1],
            'obj': str(component.get_collision_object_type()).split('.')[-1],
            'tracePlayer': str(component.get_collision_response_to_channel(ch.ECC_PLAYER)).split('.')[-1],
            'traceEnemy': str(component.get_collision_response_to_channel(ch.ECC_ENEMY)).split('.')[-1],
            'visibility': str(component.get_collision_response_to_channel(ch.ECC_VISIBILITY)).split('.')[-1],
            'projectileObj': str(component.get_collision_response_to_channel(ch.ECC_PROJECTILE)).split('.')[-1],
            'placement': str(component.get_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT)).split('.')[-1]}


def report_runtime(actor, tag):
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    health = actor.get_component_by_class(unreal.HealthComponent)
    sand = comp(actor, 'Sandbags')
    log(f'{tag} {actor.get_name()} owner={owner_of(actor)} hp={health.get_current_health():.0f}/{health.get_max_health():.0f}'
        f' holeComp={holes is not None} sandbags={sand is not None}')
    if holes is not None:
        log(f'{tag}  mesh={holes.get_editor_property("WallMeshComponentName")} mats='
            f'{[m.get_name() if m else None for m in holes.get_editor_property("DamagedMaterials")]} collapse='
            f'{holes.get_editor_property("CollapseSettings").get_editor_property("collapse_into_chunks_on_death")}')
    log(f'{tag}  aim={[(round(v.x), round(v.y), round(v.z)) for v in actor.get_editor_property("AimOffsetPoints")]}')
    log(f'{tag}  Sandbags {responses(sand)} mats={[sand.get_material(i).get_name() for i in range(sand.get_num_materials())] if sand else None}')
    log(f'{tag}  BuildingMesh {responses(comp(actor, "BuildingMeshComponent"))}')
    box = comp(actor, 'PitWeaponHitCollider')
    if box is not None:
        loc = box.get_world_location() - actor.get_actor_location()
        ext = box.get_scaled_box_extent()
        log(f'{tag}  PitWeaponHitCollider local=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) ext=({ext.x:.0f},{ext.y:.0f},{ext.z:.0f})'
            f' {responses(box)}')


def rim_shot_starts(actor, angles_deg, heights):
    """World start + direction per shot, from outside the ring toward the pit centre."""
    centre = actor.get_actor_location()
    shots = []
    for angle in angles_deg:
        radians = math.radians(angle)
        out = unreal.Vector(math.cos(radians), math.sin(radians), 0)
        for height in heights:
            start = centre + out * (CFG['ring_radius'] + 900) + unreal.Vector(0, 0, height)
            shots.append((start, out * -1.0))
    return shots


def debug_hits(actor):
    sand = comp(actor, 'Sandbags')
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    calibres = [37, 75, 105, 50, 88]
    index = 0
    for start, direction in rim_shot_starts(actor, CFG['debug_angles'], [CFG['rim_z']]):
        result = sand.line_trace_component(start, start + direction * 4000, False, False, False)
        if not result:
            log(f'debug shot from {start} missed the Sandbags collision')
            continue
        local = sand.get_world_transform().inverse_transform_location(result[0])
        calibre = calibres[index % len(calibres)]
        index += 1
        removed = holes.debug_fire_test_hit(result[0], direction, float(calibre))
        log(f'debug {calibre} mm at sandbag-local ({local.x:.0f},{local.y:.0f},{local.z:.0f}) removed {removed:.2f} m2')
    log(f'debug holes={holes.get_hole_count()} mat0={sand.get_material(0).get_name()}')
    # Guard check: bursts in the open pit (on the gun, mid-air) must not cut a hole.
    before = holes.get_hole_count()
    centre = actor.get_actor_location()
    for offset in [(0, 0, 250), (300, 200, 200), (-250, 300, 300), (200, -350, 180)]:
        point = centre + unreal.Vector(*offset)
        removed = holes.debug_fire_test_hit(point, unreal.Vector(1, 0, -0.2), 30.0)
        log(f'guard mid-pit 30 mm at local {offset} removed {removed:.2f} m2')
    log(f'guard holes before={before} after={holes.get_hole_count()}')


def pit_primitives(actor):
    """Every primitive with collision on the pit and its child actors (gun turret, crew), named owner.component."""
    found = []
    actors = [actor]
    for child_component in actor.get_components_by_class(unreal.ChildActorComponent):
        child = child_component.get_editor_property('child_actor')
        if child is not None:
            actors.append(child)
    actors += [a for a in actor.get_attached_actors() if a not in actors]
    for owner in actors:
        for component in owner.get_components_by_class(unreal.PrimitiveComponent):
            if component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                continue
            name = component.get_name() if owner == actor else owner.get_class().get_name() + '.' + component.get_name()
            found.append((name, component))
    return found


def first_component_hit(actor, start, end):
    best = None
    for name, component in pit_primitives(actor):
        if name in ('bpSelectionArea',):
            continue
        result = component.line_trace_component(start, end, False, False, False)
        if not result:
            continue
        distance = (result[0] - start).length()
        if best is None or distance < best[0]:
            best = (distance, name, result[0])
    return best


def track_projectiles(world, enemy, delta):
    centre = enemy.get_actor_location()
    sand = comp(enemy, 'Sandbags')
    transform = sand.get_world_transform()
    for projectile in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Projectile):
        key = projectile.get_name()
        hidden = projectile.get_editor_property('hidden')
        location = projectile.get_actor_location()
        velocity = projectile.get_velocity()
        previous = S['proj'].get(key)
        S['proj'][key] = (hidden, location, velocity)
        if previous is None or previous[0]:
            continue
        prev_location, prev_velocity = previous[1], previous[2]
        expected = prev_location + prev_velocity * max(delta, 0.016)
        # A pooled shell can be re-fired in the same frame it went dormant: a jump off its path is an impact too.
        reused = not hidden and (location - expected).length() > prev_velocity.length() * max(delta, 0.016) * 2 + 300
        if not hidden and not reused:
            continue
        if (prev_location - centre).length() > 1500 or prev_velocity.length() < 1:
            continue
        direction = prev_velocity.normal()
        step = prev_velocity.length() * max(delta, 0.016)
        moved = (location - prev_location).length()
        end = location + direction * 300 if (moved < step * 4 and not reused) else prev_location + direction * (step * 3 + 300)
        best = first_component_hit(enemy, prev_location - direction * 50, end)
        start_local = transform.inverse_transform_location(prev_location)
        start_radius = math.hypot(start_local.x, start_local.y)
        # own_gun is only a hint: a shell one frame before hitting the far rim's inner face also starts inside.
        record = {'t': round(S['t'], 2), 'frame': unreal.SystemLibrary.get_frame_count(), 'reused': reused, 'start_local': [round(start_local.x), round(start_local.y), round(start_local.z)],
                  'own_gun': start_radius < CFG['ring_radius'] - 100 and prev_velocity.dot(prev_location - centre) > 0,
                  'speed': round(prev_velocity.length())}
        if best is None:
            end_local = transform.inverse_transform_location(location)
            record.update({'comp': None, 'end_local': [round(end_local.x), round(end_local.y), round(end_local.z)]})
        else:
            hit_local = transform.inverse_transform_location(best[2])
            record.update({'comp': best[1], 'hit_local': [round(hit_local.x), round(hit_local.y), round(hit_local.z)]})
        S['impacts'].append(record)


def note_health(enemy):
    health = enemy.get_component_by_class(unreal.HealthComponent)
    holes = enemy.get_component_by_class(unreal.WallHoleDamageComponent)
    current = (round(health.get_current_health(), 1), holes.get_hole_count() if holes else -1)
    if current != S['health']:
        S['health'] = current
        log(f't={S["t"]:.2f} frame={unreal.SystemLibrary.get_frame_count()} enemy hp={current[0]} holes={current[1]} '
            f'impacts_so_far={len(S["impacts"])}')


def aim_camera(world, target, eye_offset, look_offset):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    look = target.get_actor_location() + unreal.Vector(*look_offset)
    eye = target.get_actor_location() + unreal.Vector(*eye_offset)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    # The RTS camera pawn takes the view target back between frames; HighResShot also gets camera= below.
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT_DIR + name + '.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            pit_class = unreal.load_class(None, CFG['bp'])
            for spot, owner in ((ENEMY_SPOT, 2), (CALM_SPOT, 1)):
                spot.z = ground_z(world, spot)
                placed = actors.spawn_actor_from_class(pit_class, spot)
                placed.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', owner)
                log(f'placed {placed.get_name()} owner {owner} at {spot}')
            for extra_bp in CFG.get('extra_bps', []):
                spot = CALM_SPOT + unreal.Vector(2500, 0, 0)
                spot.z = ground_z(world, spot)
                extra = actors.spawn_actor_from_class(unreal.load_class(None, extra_bp), spot)
                extra.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', 1)
                log(f'placed {extra.get_name()} owner 1 at {spot}')
            actors.spawn_actor_from_class(unreal.CameraActor, CALM_SPOT + unreal.Vector(0, -3000, 1500))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            # Optional runtime aim-point trial (config 'aim_override'), applied on the first PIE frame so the tanks
            # load these offsets when they acquire the pit. Never saved.
            if world is not None and CFG.get('aim_override') and not S.get('aim_done'):
                for actor in pits(world):
                    actor.set_editor_property('AimOffsetPoints', [unreal.Vector(*v) for v in CFG['aim_override']])
                S['aim_done'] = True
                log(f'aim override {CFG["aim_override"]}')
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - (CALM_SPOT + unreal.Vector(0, -3000, 1500))).length())
                for actor in pits(world):
                    report_runtime(actor, 'start')
                for extra_bp in CFG.get('extra_bps', []):
                    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, extra_bp)):
                        if actor.get_class().get_path_name() == extra_bp:
                            report_runtime(actor, 'extra')
                enemy = pit_by_owner(world, 2)
                log('enemy primitives with collision: ' + ', '.join(n for n, _ in pit_primitives(enemy)))
                calm = pit_by_owner(world, 1)
                debug_hits(calm)
                aim_camera(world, calm, CFG['calm_eye'], (0, 0, CFG['rim_z'] * 0.5))
                S['phase'], S['mark'] = 'calm_pic', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'calm_pic' and S['t'] - S['mark'] > 2.5:
            S['phase'] = 'calm_pic2'
            picture('debug_holes')
            world = game_world()
            aim_camera(world, pit_by_owner(world, 1), CFG['calm_eye_close'], (0, -CFG['ring_radius'], CFG['rim_z']))
            S['mark'] = S['t']
        elif phase == 'calm_pic2' and S['t'] - S['mark'] > 2.5:
            S['phase'], S['fire_start'], S['last_pic'] = 'real_fire', S['t'], S['t']
            picture('debug_holes_close')
        elif phase == 'real_fire':
            world = game_world()
            enemy = pit_by_owner(world, 2)
            if enemy is None:
                log(f't={S["t"]:.1f} enemy pit gone')
                finish()
                return
            track_projectiles(world, enemy, delta)
            note_health(enemy)
            if S['t'] - S['last_pic'] > 30.0:
                S['pic_index'] += 1
                aim_camera(world, enemy, CFG['enemy_eye'], (0, 0, CFG['rim_z'] * 0.5))
                S['phase'], S['pic_mark'] = 'enemy_pic', S['t']
            elif S['t'] - S['fire_start'] > REAL_FIRE_SECONDS:
                report_runtime(enemy, 'end')
                finish()
        elif phase == 'enemy_pic':
            world = game_world()
            enemy = pit_by_owner(world, 2)
            if enemy is not None:
                track_projectiles(world, enemy, delta)
                note_health(enemy)
            if S['t'] - S['pic_mark'] > 1.0:
                S['phase'], S['last_pic'] = 'real_fire', S['t']
                picture('real_fire_%d' % S['pic_index'])
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 330.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
