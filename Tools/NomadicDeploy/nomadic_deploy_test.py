"""Throwaway-editor PIE test of one nomadic building's real deploy + pack-up (never run in the shared editor).
Launched by run_deploy_test.py, which passes the building work dir in Saved/NomadicDeploy/test_run.json.
In the editor world (the unit-test startup map, not saved): the truck blueprint with bStartAsConvertedToBuilding,
a sun + sky and a scene capture. In PIE (fixed 30 fps): the truck plays its montage, then the VAT deploy with the
smoke screen; once the building stands it is told to pack up (ConvertToVehicle). Every 2nd frame is captured (15 fps
video at real speed) and the deploy / building mesh visibility is logged every second.
"""
import json

import unreal

RUN = json.load(open('D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/test_run.json'))
WORK = RUN['work']
CFG = json.load(open(WORK + '/config.json'))
OUT = WORK + '/test_frames/'
SPOT = unreal.Vector(0, 0, 0)
CAPTURE_EVERY = 2
HOLD_BUILDING_SECONDS = 4.0
MAX_SECONDS = RUN.get('max_seconds', 150.0)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'frame': 0, 'shot': 0}


def log(message):
    S['log'].append(message)
    unreal.log('NOMADICTEST ' + message)


def finish():
    json.dump(S['log'], open(WORK + '/test_log.json', 'w'), indent=1)
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 1')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def pie_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def setup_editor_world():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    SPOT.z = ground_z(w, 0, 0) + 30
    truck = actors.spawn_actor_from_class(unreal.load_class(None, CFG['truck_bp'] + '.' + CFG['truck_bp'].rsplit('/', 1)[1] + '_C'),
                                          SPOT, unreal.Rotator(0, 0, 0))
    truck.set_actor_label('NomadicTest_truck')
    truck.set_editor_property('start_as_converted_to_building', True)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, SPOT + unreal.Vector(0, 0, 2000),
                                        unreal.Rotator(roll=0, pitch=-50, yaw=150))
    sun.light_component.set_intensity(6.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, SPOT + unreal.Vector(0, 0, 2000))
    sky.light_component.set_intensity(1.5)
    cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, SPOT + unreal.Vector(0, 0, 3000))
    cam.set_actor_label('NomadicTest_capture')
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    view = actors.spawn_actor_from_class(unreal.CameraActor, SPOT + unreal.Vector(-3000, 0, 3000))
    view.set_actor_label('NomadicTest_view')
    view.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    log('editor world ' + w.get_path_name() + ' truck ' + truck.get_class().get_name())


def find(label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world(), unreal.Actor):
        if a.get_actor_label() == label:
            return a
    return None


def setup_pie():
    w = pie_world()
    unreal.GameplayStatics.set_game_paused(w, False)
    S['truck'] = find('NomadicTest_truck')
    S['cam'] = find('NomadicTest_capture')
    comp = S['cam'].capture_component2d
    comp.texture_target = unreal.RenderingLibrary.create_render_target2d(w, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp.fov_angle = 40.0
    comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    comp.set_editor_property('capture_every_frame', False)
    unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
    deploy = unreal.load_asset(RUN['deploy_mesh'])
    ext = deploy.get_bounds().box_extent
    radius = max(ext.x, ext.y, ext.z)
    # frame the deploy mesh's own bounds centre (pieces can sit far from the truck root), not the actor origin
    deploy_comp = component(S['truck'], 'DeployAnimationMesh')
    if deploy_comp:
        centre = deploy_comp.get_world_transform().transform_location(deploy.get_bounds().origin)
        target = unreal.Vector(centre.x, centre.y, S['truck'].get_actor_location().z + ext.z * 0.7)
    else:
        target = S['truck'].get_actor_location() + unreal.Vector(0, 0, ext.z * 0.7)
    eye = target + unreal.Vector(1.45, -1.05, 1.7) * radius * 2.1  # wide and high enough for elongated layouts and tall chimneys
    S['cam'].set_actor_location(eye, False, True)
    S['cam'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
    # skeletal meshes only tick when the main view renders them; the truck montage must finish for the deploy to start
    for sk in S['truck'].get_components_by_class(unreal.SkeletalMeshComponent):
        sk.set_editor_property('visibility_based_anim_tick_option',
                               unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    viewer = find('NomadicTest_view')
    if pc and viewer:
        viewer.set_actor_location(eye, False, True)
        viewer.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
        pc.set_view_target_with_blend(viewer, 0.0)
    log('pie ready truck=%s cam=%s view=%s' % (S['truck'] is not None, S['cam'] is not None, viewer is not None))


def component(actor, name):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == name:
            return c
    return None


def state():
    t = S['truck']
    deploy = component(t, 'DeployAnimationMesh')
    building = t.get_building_mesh_component()
    return (deploy.is_visible() if deploy else None, building.is_visible() if building else None, None)


def tick(delta):
    if S.get('in_tick'):
        return
    S['in_tick'] = True
    try:
        step(delta)
    except Exception as exc:
        log('ERROR ' + repr(exc))
        finish()
    finally:
        S['in_tick'] = False


def step(delta):
    S['t'] += delta
    phase = S['phase']
    if phase == 'wait_load' and S['t'] > 15.0:
        setup_editor_world()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'wait_pie', S['t']
    elif phase == 'wait_pie':
        if pie_world() is not None and S['t'] - S['mark'] > 8.0:
            setup_pie()
            S['phase'], S['game0'] = 'record', unreal.GameplayStatics.get_time_seconds(pie_world())
        elif S['t'] - S['mark'] > 150.0:
            log('pie never started')
            finish()
    elif phase == 'record':
        record()


def record():
    w = pie_world()
    now = unreal.GameplayStatics.get_time_seconds(w) - S['game0']
    S['frame'] += 1
    if S['frame'] % CAPTURE_EVERY == 0:
        S['cam'].capture_component2d.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, S['cam'].capture_component2d.texture_target, OUT,
                                                     'f_%05d.png' % S['shot'])
        S['shot'] += 1
    deploy_visible, building_visible, progress = state()
    second = int(now)
    if second != S.get('last_second'):
        S['last_second'] = second
        log('t=%d deploy_mesh_visible=%s building_visible=%s phase_flag=%s' % (second, deploy_visible, building_visible,
                                                                             S.get('stage', 'deploying')))
    stage = S.get('stage', 'deploying')
    if stage == 'deploying' and building_visible and not deploy_visible and now > 5.0:
        S['stage'], S['built_at'] = 'built', now
        log('building stands at %.1f s' % now)
    elif stage == 'built' and now - S['built_at'] > HOLD_BUILDING_SECONDS:
        result = S['truck'].convert_to_vehicle(True)
        S['stage'], S['packup_at'] = 'packing', now
        log('convert_to_vehicle -> %s at %.1f s' % (result, now))
    elif stage == 'packing' and not deploy_visible and not building_visible and now - S['packup_at'] > 2.0:
        S['stage'], S['truck_at'] = 'truck', now
        log('truck again at %.1f s (pack-up %.1f s)' % (now, now - S['packup_at']))
    elif stage == 'truck' and now - S['truck_at'] > 3.0:
        log('done, %d frames' % S['shot'])
        finish()
    if now > MAX_SECONDS:
        log('TIMEOUT in stage ' + stage)
        finish()


unreal.register_slate_post_tick_callback(tick)
