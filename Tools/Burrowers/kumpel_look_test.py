"""Throwaway-editor look check of SK_BurrowerKumpel (build_burrowers.py) next to the Scavenger, in PIE on RTS_ToCopySmall.
Never in the shared editor; the map is never saved; the editor quits at the end. Launch with kumpel_look_test_run.sh.

Places three Kumpels (idle rifle-hip, idle pistol, mesh ref pose rotated) and one Scavenger as SkeletalMeshActors with
single-node idle animations, starts PIE, takes pictures (close front, close back, game-like high camera) into
Saved/Burrowers/look/ and writes result.json (materials per slot, skeleton, bounds).
"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Burrowers/look/'
SK = '/Game/RTS_Survival/Blueprints/Squads/Ger/Burrowers/Mesh/SK_BurrowerKumpel'
SCAV = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier'
SCAV_MI = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Material/M_ScavengerNoCamo'
ANIMS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/Idle/'
CAM_HOME = unreal.Vector(0, 6000, 2000)
# (label, mesh, anim, offset cm, yaw)
FIGURES = [('kumpel_a', SK, 'Idle_Rifle_Hip', (0, -120, 0), 180), ('kumpel_b', SK, 'Idle_Pistol', (60, 0, 0), 200),
           ('kumpel_c', SK, 'Idle_Rifle_Hip_Break1', (-40, 120, 0), 160), ('scavenger', SCAV, 'Idle_Rifle_Hip', (40, 260, 0), 180)]
# (name, eye offset, look offset) around the group centre; the figures face -X (yaw 180), so the front is at -X.
SHOTS = [('front', (-520, 60, 170), (0, 60, 110)), ('front_close', (-220, -120, 170), (0, -120, 150)),
         ('back', (480, 0, 190), (0, 60, 110)), ('game', (-1500, 1500, 2300), (0, 60, 60))]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'result': {}, 'busy': False, 'queue': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('KUMPELTEST ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT + 'result.json', 'w'), indent=1, default=str)
    finally:
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    z = ground_z(world, 0, 60)
    S['centre'] = unreal.Vector(0, 60, z)
    for label, mesh, anim, (x, y, dz), yaw in FIGURES:
        a = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, z + dz), unreal.Rotator(0, 0, yaw))
        a.set_actor_label('KUMPELTEST_' + label)
        c = a.skeletal_mesh_component
        c.set_skeletal_mesh_asset(unreal.load_asset(mesh))
        if mesh == SCAV:
            c.set_material(0, unreal.load_asset(SCAV_MI))
        c.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        data = unreal.SingleAnimationPlayData()
        data.set_editor_property('anim_to_play', unreal.load_asset(ANIMS + anim))
        data.set_editor_property('saved_looping', True)
        data.set_editor_property('saved_playing', True)
        c.set_editor_property('animation_data', data)
        c.set_editor_property('visibility_based_anim_tick_option',
                              unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        log(f'placed {label} {mesh.split("/")[-1]} {anim}')
    cam = actors.spawn_actor_from_class(unreal.CameraActor, CAM_HOME)
    cam.camera_component.set_editor_property('field_of_view', 50.0)


def find_runtime(world):
    figs = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkeletalMeshActor):
        lab = a.get_actor_label() if hasattr(a, 'get_actor_label') else a.get_name()
        for label, *_ in FIGURES:
            if label in lab or label in a.get_name():
                figs[label] = a
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - CAM_HOME).length())
    info = {}
    for label, a in figs.items():
        c = a.skeletal_mesh_component
        origin, extent = a.get_actor_bounds(False)
        info[label] = dict(mesh=c.get_skeletal_mesh_asset().get_name() if c.get_skeletal_mesh_asset() else None,
                           mats=[c.get_material(i).get_name() if c.get_material(i) else None
                                 for i in range(c.get_num_materials())],
                           height_cm=round(extent.z * 2, 1))
    S['result']['figures'] = info
    log('runtime ' + json.dumps(info))


def look(eye_off, look_off):
    cam, o = S['camera'], S['centre']
    eye, target = o + unreal.Vector(*eye_off), o + unreal.Vector(*look_off)
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_location(eye, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(cam, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, f'{OUT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False


def tick(delta):
    S['t'] += delta
    if S['busy'] or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                unreal.SystemLibrary.execute_console_command(world, 'r.Streaming.FullyLoadUsedTextures 1')
                find_runtime(world)
                S['queue'] = list(SHOTS)
                look(*SHOTS[0][1:])
                S['phase'], S['mark'] = 'warmup', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'warmup' and S['t'] - S['mark'] > 6.0:
            picture('_warmup')
            S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'next' and S['t'] - S['mark'] > 2.0:
            if not S['queue']:
                finish()
                return
            S['current'] = S['queue'].pop(0)
            look(*S['current'][1:])
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 3.0:
            picture(S['current'][0])
            log('picture ' + S['current'][0])
            S['phase'], S['mark'] = 'next', S['t']
        elif S['t'] > 400.0:
            log('timeout in ' + phase)
            finish()
    except Exception as exc:  # noqa: BLE001
        log(f'ERROR in {phase}: {exc!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
