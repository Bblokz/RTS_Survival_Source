"""Throwaway-editor test for the procedural holes on the 7 BP_Dest_PostApo_* ruins (WallChunkDamage.md).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

1. Spawns the seven ruins in a row along X (shooters stand at -Y).
2. Per ruin: traces SIMPLE collision from -Y (and a skewed angle) toward each aim point, the way shells do, then
   Debug_FireTestHit at the collision point (75 / 88 / 105 mm). Logs hole count, the slot material and how far the
   collision hit sits in front of the render surface (complex trace).
3. One real damage event (ApplyPointDamage) on HighRise1, so the TakeDamage forwarding path is covered.
4. Pictures per ruin, then kills Nuclear and HighRise2 (pieces fly while they sink) and takes pictures of that.
Results: Saved/WallChunks/PostApo/headless_result.json + PNGs.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/PostApo/'
OUT = ROOT + 'headless_result.json'
PA = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_'
NAMES = ['Nuclear', 'Construction', 'HighRise1', 'HighRise2', 'DestroyedChimney', 'Crane', 'Crane2_Damaged']
SPACING = 3000.0
CALIBRES = [75, 88, 105, 75, 88]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'actors': {}, 'views': [], 'origin': None}


def log(message):
    S['log'].append(message)
    unreal.log('POSTAPOTEST ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def mesh_of(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def runtime_actors(world):
    out = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        cls = actor.get_class().get_name()
        for name in NAMES:
            if cls == 'BP_Dest_PostApo_%s_C' % name:
                out[name] = actor
    return out


def shoot_ruin(world, name, actor):
    mesh = mesh_of(actor)
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    if holes is None:
        log(f'{name}: NO WallHoleDamage component')
        return
    before = mesh.get_material(0).get_name()
    aims = list(actor.get_editor_property('AimOffsetPoints'))
    log(f'{name}: health {actor.get_editor_property("Health"):.0f} aims {len(aims)} slot0 {before}')
    transform = actor.get_actor_transform()
    for index, aim in enumerate(aims):
        target = transform.transform_location(aim)
        # Shooters at +Y (the sunlit side on RTS_ToCopySmall), some from a skewed angle, slightly from above (tank barrels ~2 m up, 30 m away).
        yaw = math.radians([90, 60, 120, 75, 105][index % 5])
        start = target + unreal.Vector(math.cos(yaw) * 3000, math.sin(yaw) * 3000, 200 - aim.z * 0.2)
        direction = (target - start).normal()
        end = start + direction * 6000
        simple = mesh.line_trace_component(start, end, False, False, False)
        if not simple:
            log(f'{name} aim {index}: shot missed the collision')
            continue
        complex_ = mesh.line_trace_component(start, end, True, False, False)
        gap = ((complex_[0] - start).length() - (simple[0] - start).length()) if complex_ else -1
        removed = holes.debug_fire_test_hit(simple[0], direction, float(CALIBRES[index % len(CALIBRES)]))
        local = transform.inverse_transform_location(simple[0])
        log(f'{name} aim {index}: hit ({local.x:.0f},{local.y:.0f},{local.z:.0f}) gap {gap:.0f} cm '
            f'calibre {CALIBRES[index % len(CALIBRES)]} removed {removed:.2f} m2')
    log(f'{name}: holes {holes.get_hole_count()} slot0 {mesh.get_material(0).get_name()}'
        + (f' slot1 {mesh.get_material(1).get_name()}' if mesh.get_num_materials() > 1 else ''))


def real_damage(world, actor):
    mesh = mesh_of(actor)
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    before = holes.get_hole_count()
    start = actor.get_actor_location() + unreal.Vector(150, 3000, 400)
    target = actor.get_actor_location() + unreal.Vector(150, 0, 450)
    hit = mesh.line_trace_component(start, target + (target - start), False, False, False)
    if not hit:
        log('real damage: no collision hit')
        return
    health_before = actor.get_editor_property('Health')
    # Python's HitResult maker takes no component/actor; the location is what the hole component reads.
    hit_result = unreal.HitResult(blocking_hit=True, location=hit[0], impact_point=hit[0],
                                  trace_start=start, trace_end=target)
    unreal.GameplayStatics.apply_point_damage(actor, 400.0, (target - start).normal(), hit_result, None, None,
                                              unreal.DamageType)
    log(f'real damage HighRise1: health {health_before:.0f} -> {actor.get_editor_property("Health"):.0f}, '
        f'holes {before} -> {holes.get_hole_count()}')


def place_camera(world, actor, eye_offset, look_offset):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    eye = actor.get_actor_location() + unreal.Vector(*eye_offset)
    centre = actor.get_actor_location() + unreal.Vector(*look_offset)
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


VIEWS = {
    'DestroyedChimney': ((300, 1700, 700), (0, 0, 550)),
    'Crane2_Damaged': ((1200, 2000, 900), (0, 300, 200)),
    'Crane': ((300, 1400, 600), (0, 0, 400)),
    'Nuclear': ((600, 3000, 900), (0, 0, 650)),
    'Construction': ((400, 2400, 800), (0, 0, 550)),
    'HighRise1': ((300, 1900, 700), (0, 0, 450)),
    'HighRise2': ((300, 1900, 700), (0, 0, 450)),
}


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log(f'editor map {world.get_path_name()}')
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for index, name in enumerate(NAMES):
                x = (index - 3) * SPACING
                ground = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, 0, 20000),
                                                                unreal.Vector(x, 0, -20000),
                                                                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                                unreal.DrawDebugTrace.NONE, True)
                z = ground.to_tuple()[4].z if ground and ground.to_tuple()[0] else 0.0
                actors.spawn_actor_from_class(unreal.load_class(None, PA + name + '.BP_Dest_PostApo_' + name + '_C'),
                                              unreal.Vector(x, 0, z))
            actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, -5000, 1500))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                # Fog of war blackens unexplored views; the ruins have no owner to reveal them.
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                S['actors'] = runtime_actors(world)
                log(f'runtime ruins {sorted(S["actors"])}')
                cams = [c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)]
                S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, -5000, 1500)).length())
                for name, actor in S['actors'].items():
                    shoot_ruin(world, name, actor)
                if 'HighRise1' in S['actors']:
                    try:
                        real_damage(world, S['actors']['HighRise1'])
                    except Exception as error:  # noqa: BLE001 - keep going to the pictures
                        log(f'real damage failed: {error!r}')
                # Pictures soon after PIE starts: later frames went black on this map (its own units raise error dialogs).
                S['views'] = [n for n in VIEWS if n in S['actors']]
                S['phase'], S['mark'] = 'views', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'views' and S['t'] - S['mark'] > 0.5:
            world = game_world()
            if not S['views']:
                S['phase'], S['mark'] = 'kill', S['t']
                return
            name = S['views'].pop(0)
            eye, look = VIEWS[name]
            place_camera(world, S['actors'][name], eye, look)
            S['pending'] = name
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 1.0:
            name = S.pop('pending')
            S['phase'], S['mark'] = 'views', S['t']
            picture('holes_' + name)
        elif phase == 'kill' and S['t'] - S['mark'] > 1.0:
            world = game_world()
            victim = S['actors'].get('HighRise2')
            place_camera(world, victim, (600, 3200, 900), (0, 0, 450))
            unreal.GameplayStatics.apply_damage(victim, 1.0e6, None, None, unreal.DamageType)
            log('killed HighRise2')
            S['phase'], S['mark'] = 'death_pic', S['t']
        elif phase == 'death_pic' and S['t'] - S['mark'] > 0.6:
            S['phase'], S['mark'] = 'death_pic2', S['t']
            picture('death_HighRise2_0_6s')
        elif phase == 'death_pic2' and S['t'] - S['mark'] > 1.5:
            S['phase'], S['mark'] = 'end', S['t']
            picture('death_HighRise2_2s')
        elif phase == 'end' and S['t'] - S['mark'] > 2.0:
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
