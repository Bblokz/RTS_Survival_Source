"""Throwaway-editor PIE test: Ger_IntroMission fuel crane + ZIL fuel truck hit boxes and deaths.

    UnrealEditor-Cmd.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env CRANE_TEST_OUT = result json path

Chain cut like secure_line_headless_test.py (ClearSovietRig first, not saved). Once the truck parked and the crane
lifted a pallet:
  1. Hit-box coverage A/B with Visibility traces over each prop's side silhouette: truck with the original SM_Hull_Zil
     vs the SM_Hull_Zil_Hitbox the mission swaps in; crane with its hit boxes off vs on.
  2. Real fire: two player Pz IV G east of the wall attack the truck, then the crane (aim points above the wall).
     Any prop still alive after the fire window is killed through ApplyDamage so the death still gets checked.
  3. Death: wreck/crane sinks for ~8 game seconds then is gone; scorch decal(s) (+2 ember decals for the truck) and the
     destruction smoke at the spot; the crane stops unloading; the objective still counts 7 targets.
Pictures: Saved/CraneTruckDeath/*.png via HighResShot (camera pawn moved over the spot).
"""
import json
import math
import os
import time

import unreal

OUT = os.environ.get('CRANE_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/CraneTruckDeath/result.json')
CLEAR = 'Mission_Intro_ClearSovietRig_C'
BASE = 'Mission_Intro_BaseBuilding_C'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
PZIV = V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C'
HULL = V + 'Rus_ArmoredCar/ZilCargo/Meshes/SM_Hull_Zil'
HULL_HITBOX = HULL + '_Hitbox'
WRECK = 'Env_Dest_Zil_Fuel_C'
FIRE_POSITIONS = [(6700.0, -1300.0), (6700.0, -2300.0)]
FIRE_WINDOW = 60.0
SINK_EXPECT = 8.0
S = {'wall0': time.time(), 'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'shots': 0}


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('CRANETEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'log': S['log'],
               'samples': S.get('samples', {})}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def game_seconds(world):
    return unreal.GameplayStatics.get_time_seconds(world)


def actors_of(world, class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def mission(world, class_name):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == class_name and obj.get_world() == world:
            return obj
    return None


def alive(actor):
    """IsUnitAlive is not reflected: units read their health component, env actors their Health property."""
    try:
        if not exists(actor):
            return False
        health = actor.get_component_by_class(unreal.HealthComponent)
        if health is not None:
            return health.call_method('GetCurrentHealth') > 0.0
        return actor.get_editor_property('health') > 0.0
    except Exception:  # noqa: BLE001
        return False


def exists(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y, ignore=None):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore or [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def coverage(world, actor, centre, half_width, z0, z1, axis_yaw, cols=14, rows=8, reach=600.0):
    """Fraction of a grid of horizontal Visibility traces (fired at the actor's side from `reach` cm, inside the base
    wall) that hit the actor.
    axis_yaw: the direction the traces travel; the grid spans half_width across it and z0..z1 in height."""
    yaw = math.radians(axis_yaw)
    fwd = (math.cos(yaw), math.sin(yaw))
    side = (-fwd[1], fwd[0])
    # Only the prop's own silhouette counts: ignore every other actor near it (walls, bunkers, the truck).
    here = actor.get_actor_location()
    ignore = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
              if a != actor and math.hypot(a.get_actor_location().x - here.x, a.get_actor_location().y - here.y) < 3000]
    hits = 0
    total = 0
    for c in range(cols):
        u = -half_width + 2.0 * half_width * (c + 0.5) / cols
        for r in range(rows):
            z = z0 + (z1 - z0) * (r + 0.5) / rows
            px, py = centre[0] + side[0] * u, centre[1] + side[1] * u
            start = unreal.Vector(px - fwd[0] * reach, py - fwd[1] * reach, z)
            end = unreal.Vector(px + fwd[0] * reach, py + fwd[1] * reach, z)
            hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                         False, ignore, unreal.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            total += 1
            if t and t[0] and t[9] == actor:
                hits += 1
    return hits / max(1, total)


def find_component(actor, cls, name):
    for comp in actor.get_components_by_class(cls):
        if comp.get_name() == name:
            return comp
    return None


def decals_near(world, point, radius):
    found = []
    for comp in unreal.ObjectIterator(unreal.DecalComponent):
        try:
            if comp.get_world() != world:
                continue
            loc = comp.get_world_location()
            if math.hypot(loc.x - point.x, loc.y - point.y) < radius:
                mat = comp.get_decal_material()
                found.append(mat.get_name() if mat else 'None')
        except Exception:  # noqa: BLE001
            continue
    return found


def smoke_near(world, point, radius, system_name):
    for comp in unreal.ObjectIterator(unreal.NiagaraComponent):
        try:
            if comp.get_world() != world:
                continue
            asset = comp.get_asset()
            loc = comp.get_world_location()
            if asset is not None and asset.get_name() == system_name and \
                    math.hypot(loc.x - point.x, loc.y - point.y) < radius:
                return True
        except Exception:  # noqa: BLE001
            continue
    return False


def look_at(world, point):
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is not None:
        pawn.set_actor_location(unreal.Vector(point.x, point.y, pawn.get_actor_location().z), False, True)


def picture(world, label):
    os.makedirs('D:/UE5Projects/RTS_Survival/Saved/CraneTruckDeath', exist_ok=True)
    unreal.SystemLibrary.execute_console_command(
        world, f'HighResShot 1280x720 filename=D:/UE5Projects/RTS_Survival/Saved/CraneTruckDeath/{label}.png')
    log(f'picture {label}')


def spawn_parked_shooters(editor_world):
    """Editor world, before PIE (Python cannot spawn into PIE): two player Pz IV G parked 25 m off the player start,
    far from the base; they are teleported to FIRE_POSITIONS when the fire test starts."""
    # First spot with ground that is far from the base (out of weapon range of everything there).
    origin = None
    for cx, cy in [(12000.0, -1800.0), (11000.0, -6000.0), (11000.0, 3000.0), (9500.0, -1800.0), (8500.0, -8000.0),
                   (-9000.0, -9000.0), (6700.0, -9000.0)]:
        hit = unreal.SystemLibrary.line_trace_single(editor_world, unreal.Vector(cx, cy, 20000),
                                                     unreal.Vector(cx, cy, -20000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit and hit.to_tuple()[0]:
            origin = unreal.Vector(cx, cy, 0.0)
            break
    if origin is None:
        origin = unreal.Vector(FIRE_POSITIONS[0][0], FIRE_POSITIONS[0][1], 0.0)
    cls = unreal.load_class(None, PZIV)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for index in range(len(FIRE_POSITIONS)):
        x, y = origin.x, origin.y + 900.0 * index
        tank = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(editor_world, x, y) + 60.0),
                                             unreal.Rotator(0, 0, 180.0))
        comp = tank.get_component_by_class(unreal.RTSComponent) if tank else None
        if comp is not None:
            comp.set_editor_property('owning_player', 1)
        tank.set_actor_label(f'CRANETEST_Shooter{index}')
        log(f'parked shooter {index} at {tank.get_actor_location() if tank else None}')
    log(f'parked shooters around {origin}')


def place_shooters(world):
    shooters = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                if a.get_actor_label().startswith('CRANETEST_Shooter')] if hasattr(unreal.Actor, 'get_actor_label') else []
    if not shooters:
        shooters = [a for a in actors_of(world, 'Ger_Vehicle_PanzerIV_G_C')][-len(FIRE_POSITIONS):]
    if not shooters:
        tanks = [(a.get_class().get_name(), a.get_actor_location()) for a in
                 unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster)]
        log(f'no shooters in PIE; tanks in PIE: {tanks}')
    for tank, (x, y) in zip(shooters, FIRE_POSITIONS):
        tank.set_actor_location_and_rotation(unreal.Vector(x, y, ground_z(world, x, y, [tank]) + 60.0),
                                             unreal.Rotator(0, 0, 180.0), False, True)
    return shooters


def order_attack(target, label):
    results = []
    for shooter in S['shooters']:
        if alive(shooter):
            try:
                results.append(str(shooter.attack_actor(target, True)))
            except Exception as error:  # noqa: BLE001
                results.append(f'failed {error!r}'[:120])
    log(f'{label}: AttackActor -> {results}')
    return results


def start_death_watch(world, key, actor, point):
    S[key] = {'start': game_seconds(world), 'start_wall': time.time(), 'z0': None, 'point': point,
              'actor': actor, 'samples': []}


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world = game_world()

        if phase == 'wait_load' and S['t'] > 15.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            manager = actors_of(editor_world, 'BP_MissionManager_C')[0]
            nodes = {}
            for top in manager.get_editor_property('missions'):
                while top is not None:
                    nodes.setdefault(top.get_class().get_name(), top)
                    top = top.get_editor_property('next_mission')
            clear, base = nodes.get(CLEAR), nodes.get(BASE)
            if clear is None or base is None:
                check('chain_has_clear_and_base', False, list(nodes))
                finish()
                return
            clear.set_editor_property('mission_trigger', None)
            manager.set_editor_property('missions', [clear, base])
            log('chain cut to [ClearSovietRig, BaseBuilding] (editor world only)')
            spawn_parked_shooters(editor_world)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return

        if phase == 'wait_pie':
            if world is not None and elapsed > 8.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                if pc is not None:
                    pc.call_method('ClickedStartGameButton')
                    log('PIE up, start game clicked')
                    S['phase'], S['mark'] = 'wait_park', S['t']
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return

        m = mission(world, CLEAR) if world is not None else None

        if phase == 'wait_park':
            crane = m.call_method('GetFuelCrane') if m is not None else None
            parked = m is not None and m.call_method('GetHasFuelTruckParked')
            unloaded = crane.call_method('GetUnloadedCount') if crane is not None else 0
            tour_done = m is not None and m.call_method('GetHasTourFinished')
            if parked and unloaded >= 1 and tour_done:
                S['crane'] = crane
                S['truck'] = m.call_method('GetFuelTruck')
                check('crane_and_truck_present', crane is not None and S['truck'] is not None,
                      f'crane {crane} truck {S["truck"]}')
                check('crane_is_destructible', isinstance(crane, unreal.DestructableEnvActor),
                      crane.get_class().get_name() if crane else None)
                check('targets_still_seven', m.call_method('GetTargetCount') == 7, m.call_method('GetTargetCount'))
                S['phase'], S['mark'] = 'coverage', S['t']
            elif elapsed > 150.0:
                check('truck_parked_crane_unloading', False, f'parked {parked} unloaded {unloaded} tour {tour_done}')
                finish()
            return

        if phase == 'coverage':
            truck, crane = S['truck'], S['crane']
            hull = find_component(truck, unreal.StaticMeshComponent, 'Hull')
            mesh_now = hull.static_mesh.get_name() if hull and hull.static_mesh else None
            check('truck_hull_swapped', mesh_now == 'SM_Hull_Zil_Hitbox', mesh_now)
            tl = truck.get_actor_location()
            ty = truck.get_actor_rotation().yaw
            gz = ground_z(world, tl.x, tl.y, [truck])
            # Side-on (traces across the truck) and nose-on.
            if hull is not None:
                hull.set_static_mesh(unreal.load_asset(HULL))
            before_side = coverage(world, truck, (tl.x, tl.y), 330.0, gz + 40, gz + 270, ty + 90.0)
            before_end = coverage(world, truck, (tl.x, tl.y), 140.0, gz + 40, gz + 270, ty)
            if hull is not None:
                hull.set_static_mesh(unreal.load_asset(HULL_HITBOX))
            after_side = coverage(world, truck, (tl.x, tl.y), 330.0, gz + 40, gz + 270, ty + 90.0)
            after_end = coverage(world, truck, (tl.x, tl.y), 140.0, gz + 40, gz + 270, ty)
            check('truck_hit_coverage_up', after_side >= 0.95 and after_end >= 0.95
                  and after_side + after_end > before_side + before_end,
                  f'side {before_side:.2f}->{after_side:.2f}, nose {before_end:.2f}->{after_end:.2f}')
            cl = crane.get_actor_location()
            mast = find_component(crane, unreal.BoxComponent, 'MastHitBox')
            jib = find_component(crane, unreal.BoxComponent, 'JibHitBox')
            check('crane_hit_boxes_exist', mast is not None and jib is not None, f'mast {mast} jib {jib}')
            for box in (mast, jib):
                if box is not None:
                    box.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            mast_before = coverage(world, crane, (cl.x, cl.y), 200.0, cl.z + 50, cl.z + 900, 180.0, 10, 12, 380.0)
            for box in (mast, jib):
                if box is not None:
                    box.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
            mast_after = coverage(world, crane, (cl.x, cl.y), 200.0, cl.z + 50, cl.z + 900, 180.0, 10, 12, 380.0)
            check('crane_mast_coverage_up', mast_after >= 0.95 and mast_after > mast_before,
                  f'mast {mast_before:.2f}->{mast_after:.2f}')
            # Cursor: a click straight down through the jib must reach what is under it, not the crane.
            jc = jib.get_world_location() if jib is not None else cl
            down = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(jc.x, jc.y, jc.z + 3000),
                                                          unreal.Vector(jc.x, jc.y, jc.z - 3000),
                                                          unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                          unreal.DrawDebugTrace.NONE, True)
            under = down.to_tuple()[9] if down and down.to_tuple()[0] else None
            check('jib_does_not_block_cursor', under != crane,
                  f'click at jib centre ({round(jc.x)}, {round(jc.y)}) hits {under.get_name() if under else None}')
            top = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(cl.x + 60, cl.y + 60, cl.z + 3000),
                                                         unreal.Vector(cl.x + 60, cl.y + 60, cl.z - 500),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            top_hit = top.to_tuple()[9] if top and top.to_tuple()[0] else None
            check('mast_blocks_cursor', top_hit == crane, top_hit.get_name() if top_hit else None)
            health = crane.get_editor_property('health')
            check('crane_health_three_medium_shots', 300 < health < 2000, health)
            S['crane_health0'] = health
            S['crane_start_loc'] = cl
            S['truck_health0'] = truck.get_component_by_class(unreal.HealthComponent).call_method('GetCurrentHealth') \
                if truck.get_component_by_class(unreal.HealthComponent) else None
            S['phase'], S['mark'] = 'spawn_shooters', S['t']
            return

        if phase == 'spawn_shooters':
            S['shooters'] = place_shooters(world)
            log(f'shooters {[s.get_actor_location() if s else None for s in S["shooters"]]}')
            S['phase'], S['mark'] = 'fire_truck', S['t']
            S['ordered'] = False
            return

        if phase == 'fire_truck':
            truck = S['truck']
            if not S['ordered'] and elapsed > 3.0:
                S['ordered'] = True
                S['truck_fire_start'] = game_seconds(world)
                order_attack(truck, 'truck')
                look_at(world, truck.get_actor_location())
            if not alive(truck) and 'truck_last_loc' in S:
                S['truck_death_point'] = S.get('truck_last_loc', truck.get_actor_location() if exists(truck) else None)
                check('truck_killed_by_real_fire', True,
                      f"{round(game_seconds(world) - S.get('truck_fire_start', game_seconds(world)), 1)} game s after "
                      f"the attack order (0 = the placed tanks auto-engaged it before the order)")
                S['phase'], S['mark'] = 'truck_death', S['t']
                return
            if exists(truck):
                S['truck_last_loc'] = truck.get_actor_location()
            if S['ordered'] and game_seconds(world) - S['truck_fire_start'] > FIRE_WINDOW:
                check('truck_killed_by_real_fire', False, f'still alive after {FIRE_WINDOW} s; killing via damage')
                S['truck_death_point'] = truck.get_actor_location()
                unreal.GameplayStatics.apply_damage(truck, 1.0e7, None, None, unreal.DamageType)
                S['phase'], S['mark'] = 'truck_death', S['t']
            return

        if phase == 'truck_death':
            point = S['truck_death_point']
            wrecks = [w for w in actors_of(world, WRECK)
                      if math.hypot(w.get_actor_location().x - point.x, w.get_actor_location().y - point.y) < 600]
            if 'truck_w' not in S:
                if wrecks:
                    # The sink starts as the wreck spawns; the tanks may have killed the truck before this phase.
                    age = wrecks[0].get_game_time_since_creation()
                    S['truck_w'] = {'actor': wrecks[0], 'start': game_seconds(world) - age,
                                    'z0': S['truck_death_point'].z, 'samples': []}
                    log(f'wreck {wrecks[0].get_name()} at {wrecks[0].get_actor_location()}')
                elif elapsed > 15.0:
                    check('truck_wreck_spawned', False, 'no wreck within 15 s')
                    S['phase'], S['mark'] = 'fire_crane', S['t']
                    S['ordered'] = False
                return
            w = S['truck_w']
            since = game_seconds(world) - w['start']
            if exists(w['actor']):
                dz = w['z0'] - w['actor'].get_actor_location().z
                w['samples'].append([round(since, 2), round(dz)])
                if 1.8 < since and 'truck_pic_2s' not in S:
                    S['truck_pic_2s'] = True
                    look_at(world, point)
                    picture(world, 'truck_sinking_2s')
                if 3.8 < since < 4.5 and 'truck_mid_pic' not in S:
                    S['truck_mid_pic'] = True
                    S['truck_mid_dz'] = dz
                    check('truck_smoke_at_spot', smoke_near(world, point, 600, 'NS_DestructionSmoke_Large'))
                if 6.0 < since and 'truck_pic_6s' not in S:
                    S['truck_pic_6s'] = True
                    S['truck_smoke_6s'] = smoke_near(world, point, 600, 'NS_DestructionSmoke_Large')
                    picture(world, 'truck_sinking_6s')
            else:
                S.setdefault('samples', {})['truck'] = w['samples'][-12:]
                last = w['samples'][-1] if w['samples'] else [0, 0]
                check('truck_wreck_spawned', True, w['actor'])
                check('truck_smoke_still_at_6s', S.get('truck_smoke_6s', False))
                check('truck_wreck_sinks_8s', 7.0 < since < 10.5 and last[1] > 250 and S.get('truck_mid_dz', 0) > 60,
                      f'gone after {round(since, 2)} game s, last dz {last[1]}, dz at ~4 s {S.get("truck_mid_dz")}')
                found = decals_near(world, point, 500)
                scorch = [d for d in found if d.startswith('MI_Decal_Scorch_0')]
                embers = [d for d in found if 'Embers' in d]
                check('truck_scorch_and_ember_decals', len(scorch) >= 1 and len(embers) >= 2, found)
                look_at(world, point)
                S['truck_decal_pic_due'] = S['t'] + 1.0
                S['phase'], S['mark'] = 'fire_crane', S['t']
                S['ordered'] = False
                return
            if since > 20.0:
                check('truck_wreck_sinks_8s', False, f'wreck still there after {round(since, 1)} s')
                S['phase'], S['mark'] = 'fire_crane', S['t']
                S['ordered'] = False
            return

        if phase == 'fire_crane':
            if S.get('truck_decal_pic_due') and S['t'] > S['truck_decal_pic_due']:
                S['truck_decal_pic_due'] = None
                picture(world, 'truck_decals')
            crane = S['crane']
            if not S['ordered'] and elapsed > 3.0:
                S['ordered'] = True
                S['crane_fire_start'] = game_seconds(world)
                check('crane_halted_after_truck_died', True,
                      f"unloading {crane.call_method('GetIsUnloading')}")
                order_attack(crane, 'crane')
                look_at(world, crane.get_actor_location())
            if S['ordered'] and not alive(crane):
                hp = crane.get_editor_property('health') if exists(crane) else None
                check('crane_killed_by_real_fire', True,
                      f"{round(game_seconds(world) - S['crane_fire_start'], 1)} game s of fire, health {hp}")
                S['crane_death_point'] = S['crane_start_loc']
                S['crane_c'] = {'start': game_seconds(world), 'z0': S['crane_start_loc'].z, 'samples': []}
                check('crane_stopped_unloading', exists(crane) and not crane.call_method('GetIsUnloading'))
                S['phase'], S['mark'] = 'crane_death', S['t']
                return
            if exists(crane):
                S['crane_start_loc'] = crane.get_actor_location()
                if S['ordered'] and 'crane_hit_logged' not in S and crane.get_editor_property('health') < S['crane_health0']:
                    S['crane_hit_logged'] = True
                    log(f"crane took its first hit {round(game_seconds(world) - S['crane_fire_start'], 1)} s in")
            if S['ordered'] and game_seconds(world) - S['crane_fire_start'] > FIRE_WINDOW:
                check('crane_killed_by_real_fire', False,
                      f"health {crane.get_editor_property('health')} of {S['crane_health0']} after {FIRE_WINDOW} s; "
                      f"killing via damage")
                S['crane_start_loc'] = crane.get_actor_location()
                unreal.GameplayStatics.apply_damage(crane, 1.0e7, None, None, unreal.DamageType)
            return

        if phase == 'crane_death':
            crane = S['crane']
            c = S['crane_c']
            since = game_seconds(world) - c['start']
            point = S['crane_death_point']
            if exists(crane):
                dz = c['z0'] - crane.get_actor_location().z
                c['samples'].append([round(since, 2), round(dz)])
                if 1.8 < since and 'crane_pic_2s' not in S:
                    S['crane_pic_2s'] = True
                    look_at(world, point)
                    picture(world, 'crane_sinking_2s')
                if 3.8 < since < 4.5 and 'crane_mid_pic' not in S:
                    S['crane_mid_pic'] = True
                    S['crane_mid_dz'] = dz
                    check('crane_smoke_at_spot', smoke_near(world, point, 700, 'NS_DestructionSmoke_Large'))
                if 6.0 < since and 'crane_pic_6s' not in S:
                    S['crane_pic_6s'] = True
                    S['crane_smoke_6s'] = smoke_near(world, point, 700, 'NS_DestructionSmoke_Large')
                    picture(world, 'crane_sinking_6s')
                if since > 20.0:
                    check('crane_sinks_8s', False, f'crane still there after {round(since, 1)} s')
                    finish()
                return
            S.setdefault('samples', {})['crane'] = c['samples'][-12:]
            last = c['samples'][-1] if c['samples'] else [0, 0]
            check('crane_sinks_8s', 7.0 < since < 10.5 and last[1] > 800 and S.get('crane_mid_dz', 0) > 150,
                  f'gone after {round(since, 2)} game s, last dz {last[1]}, dz at ~4 s {S.get("crane_mid_dz")}')
            check('crane_smoke_still_at_6s', S.get('crane_smoke_6s', False))
            found = decals_near(world, point, 500)
            check('crane_scorch_decal', len([d for d in found if d.startswith('MI_Decal_Scorch_0')]) >= 1, found)
            check('targets_still_seven_after', m is None or m.call_method('GetTargetCount') == 7)
            look_at(world, point)
            S['phase'], S['mark'] = 'final_pic', S['t']
            return

        if phase == 'final_pic':
            if elapsed > 1.0 and 'final_pic' not in S:
                S['final_pic'] = True
                picture(world, 'crane_decal')
            if elapsed > 4.0:
                finish()
            return
    except Exception as error:  # report and stop instead of spinning
        check('exception', False, repr(error))
        finish()


unreal.register_slate_post_tick_callback(tick)
