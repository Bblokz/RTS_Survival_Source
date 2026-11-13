"""Agent A, throwaway Development editor ONLY (agentA_run_headless.sh adds -LogCmds="LogWallHoleDamage Verbose").
Reads Saved/WallChunks/Pits2/agentA_test_config.json: {"run": name, "calm": [keys], "kill": [keys], "fire": [keys],
"fire_seconds": s}. All actors are placed in the editor world before PIE (map never saved):
- calm: player pits (owner 1) behind UT_VehicleObstacle's south wall: runtime values, debug hits (37-105 mm) at real
  BuildingMeshComponent collision points on the camera-facing rim, pictures.
- kill: extra player pits killed with ApplyDamage; actor validity / height / mesh visibility logged for 12 s.
- fire: ENEMY pits (owner 2) north of the map's player tanks; each projectile that goes dormant near one is traced over
  its last segment against BuildingMeshComponent and PitWeaponHitCollider (first hit logged), plus pictures.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
CFG = json.load(open(ROOT + 'agentA_test_config.json'))
OUT = ROOT + 'agentA_%s_result.json' % CFG['run']
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/'
# key: class path, ring radius (cm), rim test height, debug shot angles (deg, direction FROM the pit to the shooter)
PITS = {
    'TwinMG42': (BXP + 'GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42.Ger_BXP_GerPit_TwinMG42_C', 450, 50, [0, -20, 20, -75]),
    'BS3': (BXP + 'RusBS3Pit/Rus_BXP_BS3Pit.Rus_BXP_BS3Pit_C', 560, 40, [0, -60, -90, -120]),
    'Mortar120': (BXP + 'RusMortarPit/Rus_BXP_120MortarPit.Rus_BXP_120MortarPit_C', 460, 65, [0, -45, -90, -135]),
    'B4Stepped': (TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerSteppedSquarePit.Rus_BXP_B4HowitzerSteppedSquarePit_C', 800, 40,
                  [0, -45, -80, -110]),
    'ML20Twin': (TW + 'ML20_152MM/Pit/Rus_BXP_ML20TwinShouldersPit.Rus_BXP_ML20TwinShouldersPit_C', 760, 35,
                 [0, -45, -90, -135]),
}
CALM_Y = -7000.0
KILL_Y = -9800.0
FIRE_SPOTS = [tuple(spot) for spot in CFG.get('fire_spots', [(15500, 2600), (18000, 3000), (13000, 3000)])]
CALIBRES = [37, 75, 105, 50, 88]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'impacts': [], 'proj': {}, 'pics': [], 'placed': [],
     'kills': {}, 'hp': {}, 'kill_checks': [1.0, 4.0, 8.0, 12.0]}


def log(message):
    S['log'].append(message)
    unreal.log('AGENTA_TEST ' + message)


def finish():
    try:
        json.dump({'log': S['log'], 'impacts': S['impacts']}, open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 100.0


def comp(actor, name):
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        if component.get_name() == name:
            return component
    return None


def tagged(world):
    """(key, role, actor) for every test pit in the PIE world, matched by class and spawn location."""
    result = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BuildingExpansion):
        location = actor.get_actor_location()
        for key, role, x, y in S['placed']:
            if actor.get_class().get_name() == PITS[key][0].split('.')[-1] and abs(location.x - x) < 50 \
                    and abs(location.y - y) < 50:
                result.append((key, role, actor))
    return result


def report_runtime(key, role, actor):
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    health = actor.get_component_by_class(unreal.HealthComponent)
    mesh = comp(actor, 'BuildingMeshComponent')
    root = actor.root_component
    rel = mesh.get_editor_property('relative_location') if mesh else None
    log(f'{role} {key} owner={actor.get_component_by_class(unreal.RTSComponent).get_owning_player()} '
        f'hp={health.get_current_health():.0f}/{health.get_max_health():.0f} holeComp={holes is not None} '
        f'root={root.get_name()} mesh={mesh.static_mesh.get_name() if mesh else None} rel={rel} '
        f'scale={mesh.get_editor_property("relative_scale3d") if mesh else None}')
    if holes is not None:
        log(f'  {key} wallMesh={holes.get_editor_property("WallMeshComponentName")} damaged='
            f'{[m.get_name() if m else None for m in holes.get_editor_property("DamagedMaterials")]} collapse='
            f'{holes.get_editor_property("CollapseSettings").get_editor_property("collapse_into_chunks_on_death")}'
            f' debris={len(holes.get_editor_property("DebrisMeshes"))} holes={holes.get_hole_count()}')
        if role in ('calm', 'info'):
            for name in ('HoleSettings', 'GridSettings', 'CollapseSettings'):
                log(f'  {key} {name} {holes.get_editor_property(name).export_text()}')
    log(f'  {key} aim={[(round(v.x), round(v.y), round(v.z)) for v in actor.get_editor_property("AimOffsetPoints")]}'
        f' mats={[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())] if mesh else None}')
    box = comp(actor, 'PitWeaponHitCollider')
    if box is not None:
        loc = box.get_world_location() - actor.get_actor_location()
        ext = box.get_scaled_box_extent()
        log(f'  {key} PitWeaponHitCollider local=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) ext=({ext.x:.0f},{ext.y:.0f},{ext.z:.0f})')


def debug_hits(key, actor):
    _, ring, rim_z, angles = PITS[key]
    mesh = comp(actor, 'BuildingMeshComponent')
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    transform = mesh.get_world_transform()
    centre = actor.get_actor_location()
    shots = []
    if CFG.get('aim_debug'):
        # From outside the ring straight at each aim point (what a shooter on that side does).
        for point in actor.get_editor_property('AimOffsetPoints'):
            out = unreal.Vector(point.x, point.y, 0).normal()
            target = transform.transform_location(point)
            shots.append((target + out * (ring + 900), out * -1.0, '%d,%d' % (point.x, point.y)))
    else:
        for angle in angles:
            out = unreal.Vector(math.cos(math.radians(angle)), math.sin(math.radians(angle)), 0)
            shots.append((centre + out * (ring + 900) + unreal.Vector(0, 0, rim_z), out * -1.0, angle))
    for index, (start, direction, angle) in enumerate(shots):
        result = mesh.line_trace_component(start, start + direction * 4000, False, False, False)
        if not result:
            log(f'  {key} debug shot at {angle} deg missed the collision')
            continue
        local = transform.inverse_transform_location(result[0])
        calibre = CALIBRES[index % len(CALIBRES)]
        removed = holes.debug_fire_test_hit(result[0], direction, float(calibre))
        log(f'  {key} debug {calibre} mm from {angle} deg at local ({local.x:.0f},{local.y:.0f},{local.z:.0f}) '
            f'removed {removed:.2f} m2')
    log(f'  {key} debug holes={holes.get_hole_count()} '
        f'mats={[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())]}')


def kill(key, actor):
    health = actor.get_component_by_class(unreal.HealthComponent)
    unreal.GameplayStatics.apply_damage(actor, health.get_max_health() * 10.0, None, None, unreal.DamageType)
    S['kills'][actor.get_name()] = (key, actor, actor.get_actor_location().z, S['t'])
    log(f'kill {key} {actor.get_name()} hp after={health.get_current_health():.0f}')


def check_kills():
    if not S['kill_checks'] or not S['kills']:
        return
    due = S['kill_checks'][0]
    first = min(v[3] for v in S['kills'].values())
    if S['t'] - first < due:
        return
    S['kill_checks'].pop(0)
    wrecks = [a.get_class().get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.Actor)
              if 'Destroyed' in a.get_class().get_name() and ('B4' in a.get_class().get_name()
                                                              or 'ML20' in a.get_class().get_name())]
    log(f'  +{due:.0f}s B4/ML20 wreck actors in world: {wrecks}')
    for name, (key, actor, z0, _) in S['kills'].items():
        try:
            valid = unreal.SystemLibrary.is_valid(actor) and not actor.is_actor_being_destroyed()
        except Exception:  # noqa: BLE001
            valid = False
        if not valid:
            log(f'  +{due:.0f}s {key} actor gone')
            continue
        mesh = comp(actor, 'BuildingMeshComponent')
        holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
        log(f'  +{due:.0f}s {key} valid dz={actor.get_actor_location().z - z0:.0f} meshVisible='
            f'{mesh.is_visible() if mesh else None} hidden={actor.get_editor_property("hidden")} '
            f'holes={holes.get_hole_count() if holes else None} lifespan={actor.get_life_span():.1f}')


def first_component_hit(actor, start, end):
    best = None
    for name in ('BuildingMeshComponent', 'PitWeaponHitCollider'):
        component = comp(actor, name)
        if component is None:
            continue
        result = component.line_trace_component(start, end, False, False, False)
        if not result:
            continue
        distance = (result[0] - start).length()
        if best is None or distance < best[0]:
            best = (distance, name, result[0])
    return best


def track_projectiles(world, enemies, delta):
    for projectile in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Projectile):
        key = projectile.get_name()
        hidden = projectile.get_editor_property('hidden')
        location = projectile.get_actor_location()
        velocity = projectile.get_velocity()
        previous = S['proj'].get(key)
        S['proj'][key] = (hidden, location, velocity)
        if previous is None or previous[0] or not hidden:
            continue
        prev_location, prev_velocity = previous[1], previous[2]
        if prev_velocity.length() < 1:
            continue
        for pit_key, enemy in enemies:
            if (prev_location - enemy.get_actor_location()).length() > 4000:
                continue
            transform = comp(enemy, 'BuildingMeshComponent').get_world_transform()
            direction = prev_velocity.normal()
            step = prev_velocity.length() * max(delta, 0.016)
            moved = (location - prev_location).length()
            end = location + direction * 300 if moved < step * 4 else prev_location + direction * (step * 3 + 300)
            best = first_component_hit(enemy, prev_location - direction * 50, end)
            start_local = transform.inverse_transform_location(prev_location)
            record = {'pit': pit_key, 't': round(S['t'], 2), 'speed': round(prev_velocity.length()),
                      'start_local': [round(start_local.x), round(start_local.y), round(start_local.z)],
                      'own_gun': math.hypot(start_local.x, start_local.y) < PITS[pit_key][1] - 100}
            if best is None:
                end_local = transform.inverse_transform_location(location)
                record.update({'comp': None, 'end_local': [round(end_local.x), round(end_local.y), round(end_local.z)]})
            else:
                hit_local = transform.inverse_transform_location(best[2])
                record.update({'comp': best[1], 'hit_local': [round(hit_local.x), round(hit_local.y), round(hit_local.z)]})
            S['impacts'].append(record)


def note_health(enemies):
    for key, enemy in enemies:
        health = enemy.get_component_by_class(unreal.HealthComponent)
        holes = enemy.get_component_by_class(unreal.WallHoleDamageComponent)
        current = (round(health.get_current_health()), holes.get_hole_count() if holes else -1)
        if S['hp'].get(key) != current:
            S['hp'][key] = current
            log(f't={S["t"]:.1f} enemy {key} hp={current[0]} holes={current[1]}')


def aim_camera(target, eye_offset, look_offset):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    look = target.get_actor_location() + unreal.Vector(*look_offset)
    eye = target.get_actor_location() + unreal.Vector(*eye_offset)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    # The RTS camera pawn takes the view back between frames; the screenshot renders from this camera anyway.
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + name + '.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def enemies_alive(world):
    return [(key, actor) for key, role, actor in tagged(world) if role == 'fire']


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    plan = []
    for index, key in enumerate(CFG.get('calm', [])):
        spots = CFG.get('calm_spots')
        x, y = (spots[index][0], spots[index][1]) if spots else (7500 + index * 2100, CALM_Y)
        plan.append((key, 'calm', x, y, 1))
    for index, key in enumerate(CFG.get('kill', [])):
        plan.append((key, 'kill', 9000 + index * 2600, KILL_Y, 1))
    for index, key in enumerate(CFG.get('info', [])):
        plan.append((key, 'info', 6000 + index * 2600, KILL_Y - 2500, 1))
    for index, key in enumerate(CFG.get('fire', [])):
        plan.append((key, 'fire', FIRE_SPOTS[index][0], FIRE_SPOTS[index][1], 2))
    for key, role, x, y, owner in plan:
        spot = unreal.Vector(x, y, ground_z(world, x, y))
        placed = actors.spawn_actor_from_class(unreal.load_class(None, PITS[key][0]), spot)
        placed.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', owner)
        S['placed'].append((key, role, x, y))
        log(f'placed {key} {role} owner {owner} at ({spot.x:.0f},{spot.y:.0f},{spot.z:.0f})')
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(9000, CALM_Y - 3000, 1500))


def start_pie_checks():
    world = game_world()
    unreal.GameplayStatics.set_game_paused(world, False)
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(9000, CALM_Y - 3000, 1500)).length())
    found = tagged(world)
    log(f'pie world {world.get_name()} test pits={len(found)} of {len(S["placed"])}')
    for unit in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        rts = unit.get_component_by_class(unreal.RTSComponent)
        location = unit.get_actor_location()
        log(f'tank {unit.get_class().get_name()} owner={rts.get_owning_player() if rts else None} '
            f'at ({location.x:.0f},{location.y:.0f})')
    for key, role, actor in found:
        report_runtime(key, role, actor)
    for key, role, actor in found:
        if role == 'calm':
            debug_hits(key, actor)
    for key, role, actor in found:
        if role == 'kill':
            kill(key, actor)
    for key, role, actor in found:
        if role == 'calm':
            ring, rim_z = PITS[key][1], PITS[key][2]
            S['pics'].append(('%s/agentA_debug_wide' % key, actor, (ring * 0.9, -ring * 2.6, ring * 1.3),
                              (0, -ring * 0.4, rim_z)))
            S['pics'].append(('%s/agentA_debug_close' % key, actor, (ring * 0.6, -ring * 1.9, rim_z + 250),
                              (ring * 0.2, -ring, rim_z)))


def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            if game_world() is not None and S['t'] - S['mark'] > 12.0:
                start_pie_checks()
                S['phase'], S['mark'] = 'pics', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'pics':
            check_kills()
            if S['t'] - S['mark'] < 2.5:
                return
            if S.get('pending'):
                name = S.pop('pending')
                S['mark'] = S['t']
                picture(name)
                return
            if S['pics']:
                name, actor, eye, look = S['pics'].pop(0)
                aim_camera(actor, eye, look)
                S['pending'], S['mark'] = name, S['t']
                return
            if S['kill_checks'] and S['kills']:
                return
            S['phase'], S['mark'], S['last_pic'], S['fire_index'] = 'real_fire', S['t'], S['t'], 0
            if not CFG.get('fire'):
                finish()
        elif phase == 'real_fire':
            world = game_world()
            enemies = enemies_alive(world)
            track_projectiles(world, enemies, delta)
            note_health(enemies)
            if S['t'] - S['mark'] > CFG.get('fire_seconds', 150):
                for key, role, actor in tagged(world):
                    if role == 'fire':
                        report_runtime(key, 'end', actor)
                finish()
                return
            if enemies and S['t'] - S['last_pic'] > 35.0:
                S['fire_index'] += 1
                key, enemy = enemies[S['fire_index'] % len(enemies)]
                ring, rim_z = PITS[key][1], PITS[key][2]
                aim_camera(enemy, (-ring * 0.8, -ring * 2.6, ring * 1.4), (0, 0, rim_z))
                S['phase'], S['mark2'] = 'fire_pic', S['t']
                S['pic_name'] = '%s/agentA_realfire_%d' % (key, S['fire_index'])
        elif phase == 'fire_pic':
            world = game_world()
            enemies = enemies_alive(world)
            track_projectiles(world, enemies, delta)
            note_health(enemies)
            if S['t'] - S['mark2'] > 1.0:
                S['phase'], S['last_pic'] = 'real_fire', S['t']
                picture(S['pic_name'])
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 340.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
