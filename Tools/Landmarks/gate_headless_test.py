"""Throwaway-editor test of BP_BrandenburgGateRuin (holes, health, damage reduction, sinking death with smoke).
Run in a throwaway editor (no UECP endpoint), never the shared one:
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

1. Spawns the gate at the map centre, front (+Y) toward the sunlit side, and starts PIE.
2. Real damage events: 15 (below the reduction: no change), 100 (80 taken).
3. Debug_FireTestHit at each aim point from +Y (75-152 mm), traced on SIMPLE collision as shells are; logs the gap to
   the render surface, the hole count and the slot materials.
4. Pictures of the holes, then kills the gate and takes pictures at 0.6 / 3 / 6 / 10 s of the sink and smoke, and checks
   the actor is gone after the 12 s sink.
Results: Saved/BG_Gate_Check/destructible/result.json + PNGs.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/BG_Gate_Check/destructible/'
OUT = ROOT + 'result.json'
BP = '/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/BP_BrandenburgGateRuin'
CALIBRES = [75, 88, 105, 122, 152, 75, 88, 105, 122]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'gate': None, 'shots': []}
VIEWS = [('holes_front', (1500, 7600, 2600), (0, 0, 900)),
         ('holes_leftbay', (-1200, 3300, 1300), (-1100, 0, 800)),
         ('holes_rightbay', (1300, 3300, 1100), (1100, 0, 700))]
DEATH_SHOTS = [(0.6, 'death_00_6s'), (3.0, 'death_03s'), (6.0, 'death_06s'), (9.0, 'death_09s'), (11.5, 'death_11_5s')]
DEATH_VIEW = ((2500, 10000, 3800), (0, 0, 900))


def log(message):
    S['log'].append(message)
    unreal.log('GATETEST ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def gate_mesh(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def health(actor):
    return actor.get_editor_property('Health')


def slot_names(mesh):
    return [mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())]


def real_damage(actor, amount):
    mesh = gate_mesh(actor)
    start = actor.get_actor_location() + unreal.Vector(300, 4000, 600)
    target = actor.get_actor_location() + unreal.Vector(300, 0, 600)
    hit = mesh.line_trace_component(start, target + (target - start), False, False, False)
    if not hit:
        log('real damage: no collision hit')
        return
    before = health(actor)
    hit_result = unreal.HitResult(blocking_hit=True, location=hit[0], impact_point=hit[0], trace_start=start,
                                  trace_end=target)
    unreal.GameplayStatics.apply_point_damage(actor, float(amount), (target - start).normal(), hit_result, None, None,
                                              unreal.DamageType)
    log(f'real damage {amount}: health {before:.0f} -> {health(actor):.0f} (reduction '
        f'{actor.get_editor_property("DamageReduction"):.0f})')


def shoot(actor):
    mesh = gate_mesh(actor)
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    transform = actor.get_actor_transform()
    aims = list(actor.get_editor_property('AimOffsetPoints'))
    log(f'aims {len(aims)} slots before {slot_names(mesh)}')
    for index, aim in enumerate(aims):
        target = transform.transform_location(aim)
        yaw = math.radians([90, 70, 110, 80, 100][index % 5])
        start = target + unreal.Vector(math.cos(yaw) * 3500, math.sin(yaw) * 3500, 200 - aim.z * 0.2)
        direction = (target - start).normal()
        end = start + direction * 7000
        simple = mesh.line_trace_component(start, end, False, False, False)
        if not simple:
            log(f'aim {index} {aim}: shot missed the collision')
            continue
        complex_ = mesh.line_trace_component(start, end, True, False, False)
        gap = ((complex_[0] - start).length() - (simple[0] - start).length()) if complex_ else -1
        removed = holes.debug_fire_test_hit(simple[0], direction, float(CALIBRES[index]))
        local = transform.inverse_transform_location(simple[0])
        log(f'aim {index}: hit ({local.x:.0f},{local.y:.0f},{local.z:.0f}) gap {gap:.0f} cm calibre '
            f'{CALIBRES[index]} removed {removed:.2f} m2')
    log(f'holes {holes.get_hole_count()} slots after {slot_names(mesh)}')


def place_camera(world, eye_offset, look_offset):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    origin = S['origin']
    eye = origin + unreal.Vector(*eye_offset)
    centre = origin + unreal.Vector(*look_offset)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, centre), True)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, f'{ROOT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False
    log(f'picture {name}')


def start_pie():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    ground = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(0, 0, 20000), unreal.Vector(0, 0, -20000),
                                                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                    unreal.DrawDebugTrace.NONE, True)
    z = ground.to_tuple()[4].z if ground and ground.to_tuple()[0] else 0.0
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors.spawn_actor_from_class(unreal.load_class(None, BP + '.BP_BrandenburgGateRuin_C'), unreal.Vector(0, 0, z))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 8000, 2500))
    log(f'spawned at z {z:.0f}')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()


def on_pie_ready(world):
    unreal.GameplayStatics.set_game_paused(world, False)
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
    gates = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
             if a.get_class().get_name() == 'BP_BrandenburgGateRuin_C']
    if not gates:
        log('no gate in PIE')
        finish()
        return
    gate = gates[0]
    S['gate'], S['origin'] = gate, gate.get_actor_location()
    log(f'runtime health {health(gate):.0f} reduction {gate.get_editor_property("DamageReduction"):.0f}')
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 8000, 2500)).length())
    real_damage(gate, 15)
    real_damage(gate, 100)
    shoot(gate)
    S['views'] = list(VIEWS)
    S['phase'], S['mark'] = 'views', S['t']


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            start_pie()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                on_pie_ready(world)
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'views' and S['t'] - S['mark'] > 0.5:
            if not S['views']:
                S['phase'], S['mark'] = 'kill', S['t']
                return
            name, eye, look = S['views'].pop(0)
            place_camera(game_world(), eye, look)
            S['pending'] = name
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 1.0:
            name = S.pop('pending')
            S['phase'], S['mark'] = 'views', S['t']
            picture(name)
        elif phase == 'kill' and S['t'] - S['mark'] > 1.0:
            world = game_world()
            place_camera(world, *DEATH_VIEW)
            unreal.GameplayStatics.apply_damage(S['gate'], 1.0e6, None, None, unreal.DamageType)
            log(f'killed: health {health(S["gate"]):.0f}')
            S['death_shots'] = list(DEATH_SHOTS)
            S['phase'], S['mark'] = 'death', S['t']
        elif phase == 'death':
            if S['death_shots'] and S['t'] - S['mark'] > S['death_shots'][0][0]:
                at, name = S['death_shots'].pop(0)
                gate = S['gate']
                if unreal.SystemLibrary.is_valid(gate):
                    sink = S['origin'].z - gate.get_actor_location().z
                    log(f'{at:.1f}s sunk {sink:.0f} cm')
                picture(name)
            elif not S['death_shots'] and S['t'] - S['mark'] > 14.0:
                log(f'after 14 s gate valid: {unreal.SystemLibrary.is_valid(S["gate"])}')
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
