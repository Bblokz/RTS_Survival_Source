"""Throwaway-editor run for the deploy-animation prototypes (PROTOTYPE, delete with the folder).
Never run in the shared editor. Launch (EditorStartupMap override opens the demo map):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/_Prototypes/DeployAnim/UT_DeployAnimProto
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
1. Places the original SD HQ mesh, prototype 1 (procedural WPO) and prototype 2 (baked VAT) side by side with
   labels, both prototypes looping (custom primitive data mode 0, 12 s), and saves the demo map.
2. Waits for shaders, then captures each prototype at scrubbed progress 0..1 (CPD mode 1) from an RTS camera,
   plus the original for the progress-1 comparison. PNGs go to Saved/DeployAnimProto/captures.
"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/DeployAnimProto/captures'
BASE = '/Game/RTS_Survival/_Prototypes/DeployAnim'
MESHES = {
    'original': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/BuildingMeshes/HQ_BuildingMesh',
    'wpo': BASE + '/WPO/SM_HQDeploy_WPO',
    'vat': BASE + '/VAT/SM_HQDeploy_VAT',
}
LABELS = {'original': 'Original SD HQ (static)', 'wpo': 'Prototype 1: procedural WPO', 'vat': 'Prototype 2: baked VAT'}
SPACING = 3200.0
CLIP_STEPS = 90
LOOP_SECONDS = 12.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'actors': {}, 'queue': [], 'pending': None}


def log(message):
    S['log'].append(message)
    unreal.log('DEPLOYPROTO ' + message)


def finish():
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 1')
    json.dump(S['log'], open(OUT + '/headless_log.json', 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground_z(w, x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def place():
    w = world()
    log('editor map ' + w.get_path_name())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actors.get_all_level_actors():
        if a.get_actor_label().startswith('DeployProto_'):
            actors.destroy_actor(a)
    for k, key in enumerate(('original', 'wpo', 'vat')):
        x, y = 0.0, (k - 1) * SPACING
        loc = unreal.Vector(x, y, ground_z(w, x, y))
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc)
        a.set_actor_label('DeployProto_' + key)
        comp = a.static_mesh_component
        comp.set_static_mesh(unreal.load_asset(MESHES[key]))
        if key != 'original':
            comp.set_editor_property('bounds_scale', 3.0)  # pieces fly up to ~12 m from their rest place
            comp.set_default_custom_primitive_data_float(0, 0.0)  # loop demo
            comp.set_default_custom_primitive_data_float(2, LOOP_SECONDS)
        label = actors.spawn_actor_from_class(unreal.TextRenderActor, loc + unreal.Vector(-200, -50, 1500),
                                              unreal.Rotator(0, 0, 180))
        label.set_actor_label('DeployProto_label_' + key)
        text = label.text_render
        text.set_text(LABELS[key])
        text.set_editor_property('world_size', 120.0)
        text.set_editor_property('horizontal_alignment', unreal.HorizTextAligment.EHTA_CENTER)
        text.set_text_render_color(unreal.Color(255, 230, 160, 255))
        S['actors'][key] = a
        log(f'placed {key} at {loc}')
    # the copied unit-test map is lit dark red; a plain sun + sky makes the demo readable
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000),
                                        unreal.Rotator(roll=0, pitch=-50, yaw=150))
    sun.set_actor_label('DeployProto_light_sun')
    sun.light_component.set_intensity(6.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000))
    sky.set_actor_label('DeployProto_light_sky')
    sky.light_component.set_intensity(1.5)
    sky.light_component.recapture_sky()
    cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 3000))
    cam.set_actor_label('DeployProto_capture_tmp')
    S['camera'] = cam
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log(f'level saved {saved}')


def setup_capture():
    w = world()
    rt = unreal.RenderingLibrary.create_render_target2d(w, 1280, 800, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp = S['camera'].capture_component2d
    comp.texture_target = rt
    comp.fov_angle = 40.0
    comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    S['rt'] = rt
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 0')
    # queue: warm-up renders, then the clips and the original reference
    for _ in range(3):
        S['queue'].append(('wpo', 0.55, None))
        S['queue'].append(('vat', 0.55, None))
    for key in ('wpo', 'vat'):
        for i in range(CLIP_STEPS + 1):
            S['queue'].append((key, i / CLIP_STEPS, '%s_%03d.png' % (key, i)))
    S['queue'].append(('original', 1.0, 'original.png'))


def aim(key):
    a = S['actors'][key]
    target = a.get_actor_location() + unreal.Vector(-148, -49, 260)
    eye = target + unreal.Vector(2350, -1700, 2700)
    S['camera'].set_actor_location(eye, False, False)
    S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)


def set_progress(key, p):
    if key == 'original':
        return
    for k in ('wpo', 'vat'):  # the other prototype collapses so it never photobombs
        comp = S['actors'][k].static_mesh_component
        comp.set_custom_primitive_data_float(0, 1.0)
        comp.set_custom_primitive_data_float(1, p if k == key else 1.0)


def tick(delta):
    # saving the level ticks Slate, which would re-enter this callback mid-step
    if S.get('in_tick'):
        return
    S['in_tick'] = True
    try:
        tick_step(delta)
    finally:
        S['in_tick'] = False


def tick_step(delta):
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place()
            setup_capture()
            S['phase'], S['mark'] = 'shaders', S['t']
        elif phase == 'shaders' and S['t'] - S['mark'] > 20.0:
            if not unreal.ShaderCompilationScreenBPLibrary.are_shaders_compiling() or S['t'] - S['mark'] > 600.0:
                log('shaders ready after %.0f s' % (S['t'] - S['mark']))
                S['phase'] = 'capture'
        elif phase == 'capture':
            if S['pending'] is not None:
                key, p, name = S['pending']
                S['pending'] = None
                S['camera'].capture_component2d.capture_scene()
                if name:
                    unreal.RenderingLibrary.export_render_target(world(), S['rt'], OUT, name)
                return
            if not S['queue']:
                log('captures done')
                unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(S['camera'])
                for a in S['actors'].values():  # back to the looping demo before the final save
                    if a.static_mesh_component.static_mesh.get_name() != 'HQ_BuildingMesh':
                        a.static_mesh_component.set_custom_primitive_data_float(0, 0.0)
                log('level saved again %s' % unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
                finish()
                return
            key, p, name = S['queue'].pop(0)
            aim(key)
            set_progress(key, p)
            S['pending'] = (key, p, name)
    except Exception as exc:  # never leave a throwaway editor hanging
        log('ERROR ' + repr(exc))
        finish()


unreal.register_slate_post_tick_callback(tick)
