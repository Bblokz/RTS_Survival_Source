"""Throwaway-editor PIE test of one building expansion's VAT construction + pack-up (never run in the shared editor).
Launched by run_deploy_test.py for work dirs with config "mode": "bxp" (test settings in Saved/NomadicDeploy/test_run.json).
Editor world (unit-test startup map, not saved): the host nomadic truck (config "host_truck_bp", default the SD HQ)
with bStartAsConvertedToBuilding, the expansion blueprint, a sun + sky and a scene capture.
PIE (fixed 30 fps): the host deploys; once it stands the expansion is built where its owner would put it (config
"host_socket" = socket on the host building mesh, else the host origin) through the UNomadicDeployEditorLibrary test
hooks, without an owner. Then it holds, packs up over config "pack_up_seconds" and is finished like its owner does.
With "states" in test_run.json the run also packs up mid-construction and cancels (construction resumes) and cancels
a pack-up of the built expansion (it snaps back), before the final pack-up.
Every 2nd frame is captured (15 fps video at real speed); status and visibility are logged every second.
"""
import json

import unreal

RUN = json.load(open('D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/test_run.json'))
WORK = RUN['work']
CFG = json.load(open(WORK + '/config.json'))
OUT = WORK + '/test_frames/'
HOST_BP = CFG.get('host_truck_bp', '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Ger_HQ_Truck')
PACK_UP_SECONDS = float(CFG.get('pack_up_seconds', 20.0))
STATES = bool(RUN.get('states'))
SPOT = unreal.Vector(0, 0, 0)
CAPTURE_EVERY = 2
HOLD_BUILT_SECONDS = 3.0
MAX_SECONDS = RUN.get('max_seconds', 240.0)
LIB = unreal.NomadicDeployEditorLibrary
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'frame': 0, 'shot': 0, 'stage': 'host_deploying'}


def log(message):
    S['log'].append(message)
    unreal.log('BXPTEST ' + message)


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


def bp_class(path):
    return unreal.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')


def setup_editor_world():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    SPOT.z = ground_z(w, 0, 0) + 30
    host = actors.spawn_actor_from_class(bp_class(HOST_BP), SPOT, unreal.Rotator(0, 0, 0))
    host.set_actor_label('BxpTest_host')
    host.set_editor_property('start_as_converted_to_building', True)
    bxp = actors.spawn_actor_from_class(bp_class(CFG['bxp_bp']), SPOT + unreal.Vector(0, 0, -5000), unreal.Rotator(0, 0, 0))
    bxp.set_actor_label('BxpTest_bxp')
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, SPOT + unreal.Vector(0, 0, 2000),
                                        unreal.Rotator(roll=0, pitch=-50, yaw=150))
    sun.light_component.set_intensity(7.0)
    sun.set_actor_label('BxpTest_sun')
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, SPOT + unreal.Vector(0, 0, 2000))
    sky.light_component.set_intensity(2.5)
    cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, SPOT + unreal.Vector(0, 0, 3000))
    cam.set_actor_label('BxpTest_capture')
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    view = actors.spawn_actor_from_class(unreal.CameraActor, SPOT + unreal.Vector(-3000, 0, 3000))
    view.set_actor_label('BxpTest_view')
    view.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    log('editor world ' + w.get_path_name() + ' host ' + host.get_class().get_name() + ' bxp ' + bxp.get_class().get_name())


def find(label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world(), unreal.Actor):
        if a.get_actor_label() == label:
            return a
    return None


def component(actor, name):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == name:
            return c
    return None


def aim_camera(target, radius, outward):
    """Eye outside the host (along `outward`, a bit to the side) and high, so the host never covers the expansion."""
    side = unreal.Vector(-outward.y, outward.x, 0.0)
    eye = target + (outward * 1.35 + side * 0.55 + unreal.Vector(0, 0, 1.15)) * radius
    for actor in (S['cam'], S.get('view')):
        if actor:
            actor.set_actor_location(eye, False, True)
            actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)


def setup_pie():
    w = pie_world()
    unreal.GameplayStatics.set_game_paused(w, False)
    S['host'] = find('BxpTest_host')
    S['bxp'] = find('BxpTest_bxp')
    S['cam'] = find('BxpTest_capture')
    S['view'] = find('BxpTest_view')
    S['bxp'].set_actor_hidden_in_game(True)
    comp = S['cam'].capture_component2d
    comp.texture_target = unreal.RenderingLibrary.create_render_target2d(w, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp.fov_angle = 40.0
    comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    comp.set_editor_property('capture_every_frame', False)
    unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
    for sk in S['host'].get_components_by_class(unreal.SkeletalMeshComponent):
        sk.set_editor_property('visibility_based_anim_tick_option',
                               unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if pc and S['view']:
        pc.set_view_target_with_blend(S['view'], 0.0)
    log('pie ready host=%s bxp=%s cam=%s' % (S['host'] is not None, S['bxp'] is not None, S['cam'] is not None))


def placement():
    """Where the owner would build the expansion: on the host socket, or at the host building origin."""
    building = S['host'].get_building_mesh_component()
    socket = CFG.get('host_socket')
    if socket:
        t = building.get_socket_transform(socket, unreal.RelativeTransformSpace.RTS_WORLD)
        return t.translation, t.rotation.rotator()
    return building.get_world_location(), building.get_world_rotation()


def light_from_camera_side(outward):
    """Sun from behind the camera, slightly to the side: the faces the camera sees are lit (the fixed test sun left
    expansions on the HQ's shadow side near black)."""
    sun = find('BxpTest_sun')
    if not sun:
        return
    toward_expansion = unreal.MathLibrary.rotate_angle_axis(outward * -1.0, 35.0, unreal.Vector(0, 0, 1))
    yaw = unreal.MathLibrary.make_rot_from_x(toward_expansion).yaw
    sun.set_actor_rotation(unreal.Rotator(roll=0, pitch=-48, yaw=yaw), True)


def frame_expansion():
    """Frames the expansion where it now stands (its building mesh component's world bounds; the deploy mesh's
    bounds are scaled up for culling), from outside the host so the host never covers it."""
    building = component(S['bxp'], 'BuildingMeshComponent')
    # the deploy mesh's own bounds: component bounds can include attached child meshes (the radar dish)
    mesh_bounds = unreal.load_asset(RUN['deploy_mesh']).get_bounds()
    origin = building.get_world_transform().transform_location(mesh_bounds.origin)
    extent = mesh_bounds.box_extent
    host = S['host'].get_actor_location()
    outward = unreal.Vector(origin.x - host.x, origin.y - host.y, 0.0)
    if outward.length() < 100.0:
        outward = unreal.Vector(1.45, -1.05, 0.0)
    outward = outward.normal()
    # config "test_camera_yaw": extra yaw (degrees) of the view around the expansion, e.g. to look past a host part
    outward = unreal.MathLibrary.rotate_angle_axis(outward, float(CFG.get('test_camera_yaw', 0.0)), unreal.Vector(0, 0, 1))
    radius = max(extent.x, extent.y, extent.z, 250.0) * 3.2 * float(CFG.get('test_camera_distance', 1.0))
    target = unreal.Vector(origin.x, origin.y, origin.z)
    aim_camera(target, radius, outward)
    light_from_camera_side(outward)
    log('framing bxp bounds origin %s extent %s host %s' % (origin, extent, host))


def state():
    host, bxp = S['host'], S['bxp']
    host_deploy = component(host, 'DeployAnimationMesh')
    host_building = host.get_building_mesh_component()
    host_built = host_building.is_visible() and not (host_deploy.is_visible() if host_deploy else False)
    bxp_deploy = component(bxp, 'DeployAnimationMesh') if unreal.SystemLibrary.is_valid(bxp) else None
    bxp_building = component(bxp, 'BuildingMeshComponent') if unreal.SystemLibrary.is_valid(bxp) else None
    return {
        'host_built': host_built,
        'deploy': bxp_deploy.is_visible() if bxp_deploy else None,
        'building': bxp_building.is_visible() if bxp_building else None,
        'hidden': bxp.get_editor_property('hidden') if unreal.SystemLibrary.is_valid(bxp) else None,
        'status': LIB.test_get_bxp_status(bxp) if unreal.SystemLibrary.is_valid(bxp) else 'destroyed',
    }


def log_child_visibility(since):
    """First second of a pack-up: when the blueprint hides its turret / extra meshes, relative to the cover smoke."""
    bxp = S['bxp']
    if not unreal.SystemLibrary.is_valid(bxp):
        return
    shown = []
    for comp in bxp.get_components_by_class(unreal.SceneComponent):
        if isinstance(comp, unreal.ChildActorComponent) or comp.get_name() in ('Radar', 'Radar_GEN_VARIABLE'):
            shown.append('%s=%s' % (comp.get_name(), comp.is_visible()))
    if shown:
        log('pack-up +%.3f s child visibility %s' % (since, ' '.join(shown)))


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


def go(stage, now, message):
    S['stage'], S['at'] = stage, now
    log('%s at %.1f s' % (message, now))


def record():
    w = pie_world()
    now = unreal.GameplayStatics.get_time_seconds(w) - S['game0']
    st = state()
    stage = S['stage']
    S['frame'] += 1
    if stage != 'host_deploying' and S['frame'] % CAPTURE_EVERY == 0:
        S['cam'].capture_component2d.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, S['cam'].capture_component2d.texture_target, OUT,
                                                     'f_%05d.png' % S['shot'])
        S['shot'] += 1
    second = int(now)
    if second != S.get('last_second'):
        S['last_second'] = second
        log('t=%d stage=%s %s' % (second, stage, st))
    since = now - S.get('at', 0.0)
    bxp = S['bxp']
    if stage in ('packing', 'built_packing') and since < 1.0:
        log_child_visibility(since)
    if stage == 'host_deploying' and st['host_built'] and now > 5.0:
        location, rotation = placement()
        LIB.test_start_bxp_construction(bxp, location, rotation)
        frame_expansion()
        go('building', now, 'host stands; construction started (status %s)' % LIB.test_get_bxp_status(bxp))
    elif stage == 'building' and STATES and not S.get('mid_done') and since > float(CFG.get('test_midbuild_seconds', 6.0)):
        S['mid_done'] = True
        LIB.test_start_bxp_pack_up(bxp, PACK_UP_SECONDS)
        go('mid_packing', now, 'pack-up during construction (status %s)' % LIB.test_get_bxp_status(bxp))
    elif stage == 'mid_packing' and since > 2.0:
        LIB.test_cancel_bxp_pack_up(bxp)
        go('building', now, 'pack-up cancelled, construction resumes (status %s)' % LIB.test_get_bxp_status(bxp))
    elif stage == 'building' and st['status'] == 'BXS_Built' and st['building'] and not st['deploy']:
        go('built', now, 'expansion stands')
    elif stage == 'built' and since > HOLD_BUILT_SECONDS:
        LIB.test_start_bxp_pack_up(bxp, PACK_UP_SECONDS)
        if STATES and not S.get('built_cancel_done'):
            S['built_cancel_done'] = True
            go('built_packing', now, 'pack-up of the built expansion (to be cancelled)')
        else:
            go('packing', now, 'pack-up started (%.0f s)' % PACK_UP_SECONDS)
    elif stage == 'built_packing' and since > min(3.0, PACK_UP_SECONDS * 0.3):
        LIB.test_cancel_bxp_pack_up(bxp)
        go('built', now, 'pack-up cancelled (status %s, building visible %s)' % (LIB.test_get_bxp_status(bxp),
                                                                               state()['building']))
    elif stage == 'packing' and since > PACK_UP_SECONDS + 0.3:
        log('before finish: %s' % st)
        LIB.test_finish_bxp_pack_up(bxp)
        go('packed', now, 'pack-up finished')
    elif stage == 'packed' and since > 2.0:
        log('done, %d frames' % S['shot'])
        finish()
    if now > MAX_SECONDS:
        log('TIMEOUT in stage ' + stage)
        finish()


unreal.register_slate_post_tick_callback(tick)
