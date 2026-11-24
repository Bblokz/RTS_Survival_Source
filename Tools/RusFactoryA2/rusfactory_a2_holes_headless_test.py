"""Throwaway-editor HOLES test for Rus_Building_Factory_A2 (procedural wall holes, no damage meshes) (never the shared editor). Launch:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Spawns the A2 factory, plays, logs the building mesh, the crane animation, the turret attachments (socket names and world
heading vs the expected yaw) and takes RTS-angle screenshots spread over the 30 s work loop to Saved/RusFactoryA2/.
"""
import json
import os

import unreal

OUT = unreal.Paths.project_saved_dir() + 'RusFactoryA2/Holes/'
BP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Factory_A2.Rus_Building_Factory_A2_C'
SPOT = unreal.Vector(0, 0, 0)
# concept front-right camera (as the sheets) in Unreal axes: eye offset direction from the look-at point
EYE_DIR = unreal.Vector(-0.494, -0.685, 0.536)
SHOTS = [('holes_wide', 4600, (-260, 180, 350)), ('holes_front', 2200, (0, -1300, 300)),
         ('holes_east', 2200, (-650, 500, 350)), ('health_60', 4600, (-260, 180, 350)),
         ('health_30', 4600, (-260, 180, 350))]
# debug hits in mesh space: start, direction, calibre (mm). UE x = -concept x, UE y = concept y.
HITS = [((-200, -2600, 250), (0, 1, 0), 76), ((300, -2400, 450), (0, 1, 0), 122), ((-600, -2400, 200), (0, 1, 0), 152),
        ((-1800, 300, 300), (1, 0, 0), 85), ((-1800, 750, 450), (1, 0, 0), 152), ((-1800, -200, 150), (1, 0, 0), 100),
        ((200, 2600, 300), (0, -1, 0), 76), ((-300, 900, 2500), (0, 0, -1), 100)]
GAP = 5.5  # seconds between shots, so the wide shots fall in different parts of the loop
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': list(SHOTS)}


def log(message):
    S['log'].append(message)
    unreal.log('RUSA2TEST ' + message)


def finish():
    os.makedirs(OUT, exist_ok=True)
    with open(OUT + 'headless_result.json', 'w') as f:
        json.dump(S['log'], f, indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def factory(world):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == 'Rus_Building_Factory_A2_C'][0]


def comp(actor, name):
    for c in actor.get_components_by_class(unreal.SceneComponent):
        if c.get_name() == name:
            return c
    return None


def inspect(world):
    a = factory(world)
    mesh = comp(a, 'BuildingMeshComponent')
    log(f'building mesh {mesh.static_mesh.get_name() if mesh.static_mesh else None} at {a.get_actor_location()} yaw {a.get_actor_rotation().yaw:.1f}')
    crane = comp(a, 'A2_Crane')
    log(f'crane mesh {crane.get_skinned_asset().get_name() if crane.get_skinned_asset() else None} playing {crane.is_playing()} '
        f'position {crane.get_position():.2f} visible {crane.is_visible()}')
    base = mesh.get_world_transform()
    for name, socket, yaw in (('A2_TurretHook', 'TurretHook', 180.0), ('A2_TurretLorry', 'TurretLorry', -90.0),
                              ('A2_GunHook', 'TurretHook', 180.0), ('A2_Lorry', '', 90.0)):
        c = comp(a, name)
        if c is None:
            log(f'{name} MISSING')
            continue
        rel = unreal.MathLibrary.make_relative_transform(c.get_world_transform(), base)
        r = rel.rotation.rotator()
        log(f'{name} parent {c.get_attach_parent().get_name() if c.get_attach_parent() else None} socket '
            f'{c.get_attach_socket_name()} (want {socket or "-"}) mesh-space loc '
            f'({rel.translation.x:.0f},{rel.translation.y:.0f},{rel.translation.z:.0f}) yaw {r.yaw:.1f} pitch {r.pitch:.1f} '
            f'roll {r.roll:.1f} visible {c.is_visible()}')


def hits(world):
    a = factory(world)
    mesh = comp(a, 'BuildingMeshComponent')
    holes = a.get_component_by_class(unreal.WallHoleDamageComponent)
    xf = mesh.get_world_transform()
    log(f'slot0 before {mesh.get_material(0).get_name()} holes comp {holes is not None}')
    for start, d, cal in HITS:
        ws = xf.transform_location(unreal.Vector(*start))
        we = xf.transform_location(unreal.Vector(start[0] + d[0] * 4000, start[1] + d[1] * 4000, start[2] + d[2] * 4000))
        r = mesh.line_trace_component(ws, we, False, False, False)
        if not r:
            log(f'hit {start} missed the collision')
            continue
        local = xf.inverse_transform_location(r[0])
        removed = holes.debug_fire_test_hit(r[0], unreal.Vector(*d), float(cal))
        log(f'hit {start} {cal} mm at local ({local.x:.0f},{local.y:.0f},{local.z:.0f}) removed {removed:.2f} m2')
    log(f'holes {holes.get_hole_count()} slot0 after {mesh.get_material(0).get_name()} slots '
        f'{[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())]}')


def set_health(world, share):
    a = factory(world)
    h = a.get_component_by_class(unreal.HealthComponent)
    h.set_current_health(h.get_max_health() * share)
    mesh = comp(a, 'BuildingMeshComponent')
    log(f'health {share:.0%}: mesh {mesh.static_mesh.get_name()} slot0 {mesh.get_material(0).get_name()} '
        f'machinery visible {comp(a, "A2_Crane").is_visible()}')


def aim(world, distance, look_local):
    a = factory(world)
    mesh = comp(a, 'BuildingMeshComponent')
    look = mesh.get_world_transform().transform_location(unreal.Vector(*look_local))
    eye = look + EYE_DIR * distance
    cam = S['camera']
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_location(eye, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0.0)


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(0, 0, 20000), unreal.Vector(0, 0, -20000),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            if hit:
                SPOT.z = hit.to_tuple()[4].z
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            a = actors.spawn_actor_from_class(unreal.load_class(None, BP), SPOT)
            rts = a.get_component_by_class(unreal.RTSComponent)
            if rts:
                rts.set_editor_property('owning_player', 1)
            actors.spawn_actor_from_class(unreal.CameraActor, SPOT + unreal.Vector(-4000, -4000, 3000))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 10.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                cams = [c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)]
                S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - (SPOT + unreal.Vector(-4000, -4000, 3000))).length())
                inspect(world)
                hits(world)
                S['phase'], S['mark'] = 'shots', S['t'] - GAP
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'shots':
            if not S['shots']:
                finish()
                return
            if S['t'] - S['mark'] > GAP:
                world = game_world()
                name, distance, look = S['shots'][0]
                if name == 'health_60':
                    set_health(world, 0.60)
                elif name == 'health_30':
                    set_health(world, 0.30)
                aim(world, distance, look)
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 1.5:
            world = game_world()
            name, distance, look = S['shots'].pop(0)
            crane = comp(factory(world), 'A2_Crane')
            S['phase'], S['mark'] = 'shots', S['t']
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, f'{OUT}{name}.png', camera=S['camera'])
            finally:
                S['busy'] = False
            log(f'shot {name} at anim {crane.get_position():.2f} s')
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
