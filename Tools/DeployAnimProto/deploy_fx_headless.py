"""Throwaway-editor PIE run for the deploy-effect setups (PROTOTYPE, delete with the folder). Never in the shared editor.
Launch (fixed 30 fps so the world clock is deterministic):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/_Prototypes/DeployAnim/UT_DeployAnimProto
    -ini:Engine:[/Script/Engine.Engine]:bUseFixedFrameRate=True -ini:Engine:[/Script/Engine.Engine]:FixedFrameRate=30
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Reads Saved/DeployAnimProto/fx_run.json:
  {"mode": "gallery"}  every candidate system at a grid spot, captured at fixed ages
  {"mode": "setups", "setups": [...names...]}  per setup: one warm-up pass, then a recorded pass; each tick the
      progress comes from the frame counter, is written as the VAT scrub value (CPD 0 = 1, CPD 1 = progress) and the
      setup's effects fire for every baked landing event whose time has passed (setups in deploy_fx_setups.py).
"""
import json
import math

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/'
TOOLS = ROOT + 'Source/Tools/DeployAnimProto/'
OUT = ROOT + 'Saved/DeployAnimProto/fx/'
RUN = json.load(open(ROOT + 'Saved/DeployAnimProto/fx_run.json'))
exec(open(TOOLS + 'deploy_fx_setups.py').read())  # SETUPS, GALLERY
EVENTS = json.load(open(ROOT + 'Saved/DeployAnimProto/deploy_events.json'))['events']
FPS = 30
DEPLOY_SECONDS = 12.0
TAIL_SECONDS = 3.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'frame': 0, 'queue': []}


def log(message):
    S['log'].append(message)
    unreal.log('DEPLOYFX ' + message)


def finish():
    json.dump(S['log'], open(OUT + 'run_log.json', 'w'), indent=1)
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 1')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def pie_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def pie_actor(label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world(), unreal.Actor):
        if a.get_actor_label() == label:
            return a
    return None


def setup_pie():
    w = pie_world()
    unreal.GameplayStatics.set_game_paused(w, False)
    for label in ('DeployProto_original', 'DeployProto_wpo', 'DeployProto_label_original', 'DeployProto_label_wpo',
                  'DeployProto_label_vat'):
        a = pie_actor(label)
        if a:
            a.set_actor_hidden_in_game(True)
            a.set_actor_location(unreal.Vector(0, 0, -100000), False, True)
    S['vat'] = pie_actor('DeployProto_vat')
    S['cam'] = pie_actor('DeployFx_capture')
    rt = unreal.RenderingLibrary.create_render_target2d(w, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp = S['cam'].capture_component2d
    comp.texture_target = rt
    comp.fov_angle = 40.0
    comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    comp.set_editor_property('capture_every_frame', False)
    S['rt'] = rt
    unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
    log('pie ready vat=%s cam=%s' % (S['vat'] is not None, S['cam'] is not None))


def aim(target, offset):
    eye = target + unreal.Vector(*offset)
    S['cam'].set_actor_location(eye, False, True)
    S['cam'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)


def capture(name):
    S['cam'].capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(pie_world(), S['rt'], OUT, name)


def spawn(path, location, scale):
    system = unreal.load_asset(path)
    comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(pie_world(), system, location, unreal.Rotator(0, 0, 0),
                                                                   unreal.Vector(scale, scale, scale), True)
    if comp is not None:
        S.setdefault('live', []).append(comp)
    if comp is not None and '/DeployAnim/' in path:
        # stateless sprite sizes ignore the component scale (see Source/Docs/VFX/HowitzerImpacts.md)
        comp.set_variable_vec2('ImpactSpriteScale', unreal.Vector2D(scale, scale))
    return comp


# ---------------------------------------------------------------- gallery ------------------------------------------------
GALLERY_ORIGIN = unreal.Vector(9000, -6000, 0)
GALLERY_AGES = (0.3, 1.0, 2.5)


def gallery_spot(i, warm=False):
    # the warm-up set plays far away so the pictured set starts clean once shaders are compiled
    return GALLERY_ORIGIN + unreal.Vector((i % 4) * 2500 + (14000 if warm else 0), (i // 4) * 2500, 0)


def gallery_tick():
    w = pie_world()
    now = unreal.GameplayStatics.get_time_seconds(w)
    if 'g_warm' not in S:
        for i, (name, path, scale) in enumerate(GALLERY):
            spot = gallery_spot(i, True)
            spot.z = ground_z(spot)
            spawn(path, spot, scale)
        S['g_warm'] = now
        return
    if 'g_start' not in S:
        if now - S['g_warm'] < 20.0 or (unreal.ShaderCompilationScreenBPLibrary.are_shaders_compiling()
                                        and now - S['g_warm'] < 300.0):
            return
        log('gallery warm-up done after %.0f s' % (now - S['g_warm']))
        for i, (name, path, scale) in enumerate(GALLERY):
            spot = gallery_spot(i)
            spot.z = ground_z(spot)
            spawn(path, spot, scale)
        S['g_start'], S['g_age'] = now, 0
        return
    if S['g_age'] < len(GALLERY_AGES) and now - S['g_start'] >= GALLERY_AGES[S['g_age']]:
        for i, (name, path, scale) in enumerate(GALLERY):
            spot = gallery_spot(i)
            spot.z = ground_z(spot)
            aim(spot + unreal.Vector(0, 0, 150), (1300, -950, 1500))
            capture('gallery_%02d_%s_%03d.png' % (i, name, int(GALLERY_AGES[S['g_age']] * 100)))
        log('gallery age %.2f captured at %.3f' % (GALLERY_AGES[S['g_age']], now - S['g_start']))
        S['g_age'] += 1
    if S['g_age'] >= len(GALLERY_AGES):
        finish()


def ground_z(spot):
    hit = unreal.SystemLibrary.line_trace_single(pie_world(), unreal.Vector(spot.x, spot.y, 50000),
                                                 unreal.Vector(spot.x, spot.y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


# ---------------------------------------------------------------- setups -------------------------------------------------
def to_world(local):
    return S['vat'].get_actor_transform().transform_location(unreal.Vector(*local))


def set_progress(p):
    comp = S['vat'].static_mesh_component
    comp.set_custom_primitive_data_float(0, 1.0)
    comp.set_custom_primitive_data_float(1, p)


def start_pass(setup, record):
    S['pass'] = {'setup': setup, 'record': record, 'frame0': S['frame'], 'next_event': 0, 'spawned': 0,
                 'last_time': None, 'deltas': [], 'periodic_last': {}, 'once_done': set()}
    set_progress(0.0)
    log('pass %s record=%s' % (setup, record))


def fire_effects(setup, event):
    for rule in SETUPS[setup]['events'].get(event['cat'], []):
        size = event['size'] if rule.get('by_size') else 300.0
        scale = rule['scale'] * min(max(size / 300.0, rule.get('min', 0.6)), rule.get('max', 1.8))
        points = [event['centre']]
        if rule.get('spread') and event['count'] > 2:  # long rows / wall runs: one puff per end and middle
            ex, ey = event['extent'][0], event['extent'][1]
            c = event['centre']
            axis = (ex / 2.6, 0) if ex >= ey else (0, ey / 2.6)
            points = [c, [c[0] - axis[0], c[1] - axis[1], c[2]], [c[0] + axis[0], c[1] + axis[1], c[2]]]
        for pt in points:
            loc = to_world([pt[0], pt[1], pt[2] + rule.get('z', 0.0)])
            spawn(rule['system'], loc, scale)
            S['pass']['spawned'] += 1


def update_timed(setup, p, seconds):
    """'periodic' rules re-fire every N seconds inside their progress window; 'once' rules fire at a progress."""
    ps = S['pass']
    for i, rule in enumerate(SETUPS[setup].get('periodic', [])):
        if not rule['from'] <= p < rule['to']:
            continue
        last = ps['periodic_last'].get(i)
        if last is None or seconds - last >= rule['every']:
            ps['periodic_last'][i] = seconds
            for point in rule['at']:
                spawn(rule['system'], to_world(point), rule['scale'])
                ps['spawned'] += 1
    for i, rule in enumerate(SETUPS[setup].get('once', [])):
        if i not in ps['once_done'] and p >= rule['progress']:
            ps['once_done'].add(i)
            for point in rule['at']:
                spawn(rule['system'], to_world(point), rule['scale'])
                ps['spawned'] += 1


def setups_tick():
    w = pie_world()
    now = unreal.GameplayStatics.get_time_seconds(w)
    if 'pass' not in S:
        S['queue'] = []
        for name in RUN['setups']:
            S['queue'] += [(name, False), (name, True)]
        start_pass(*S['queue'].pop(0))
        return
    ps = S['pass']
    if ps['last_time'] is not None:
        ps['deltas'].append(now - ps['last_time'])
    ps['last_time'] = now
    k = S['frame'] - ps['frame0']
    p = min(k / (DEPLOY_SECONDS * FPS), 1.0)
    set_progress(p)
    while ps['next_event'] < len(EVENTS) and EVENTS[ps['next_event']]['t'] <= p:
        fire_effects(ps['setup'], EVENTS[ps['next_event']])
        ps['next_event'] += 1
    update_timed(ps['setup'], p, k / FPS)
    if ps['record']:
        aim(to_world([-148, -49, 220]), (2350, -1700, 2700))
        capture('%s_%04d.png' % (ps['setup'], k))
    if k >= (DEPLOY_SECONDS + TAIL_SECONDS) * FPS:
        d = ps['deltas']
        log('pass %s done: frames %d, spawned %d, delta min %.4f max %.4f' % (ps['setup'], k, ps['spawned'],
                                                                            min(d), max(d)))
        for comp in S.get('live', []):  # nothing may carry over into the next setup's clip
            try:
                comp.destroy_component(comp)
            except Exception:
                pass
        S['live'] = []
        if not S['queue']:
            finish()
            return
        S['pending_start'] = S['queue'].pop(0)
        S['pass'] = dict(ps, setup=None)
        S['phase'], S['mark'] = 'between', S['t']


# ---------------------------------------------------------------- main tick ----------------------------------------------
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
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        for a in actors.get_all_level_actors():
            if a.get_actor_label() == 'DeployFx_capture':
                actors.destroy_actor(a)
        cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 3000))
        cam.set_actor_label('DeployFx_capture')
        cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'wait_pie', S['t']
    elif phase == 'wait_pie':
        if pie_world() is not None and S['t'] - S['mark'] > 10.0:
            setup_pie()
            S['phase'] = RUN['mode']
        elif S['t'] - S['mark'] > 150.0:
            log('pie never started')
            finish()
    elif phase == 'gallery':
        gallery_tick()
    elif phase == 'setups':
        S['frame'] += 1
        setups_tick()
    elif phase == 'between':
        if S['t'] - S['mark'] > 2.0:
            S['phase'] = 'setups'
            start_pass(*S['pending_start'])


unreal.register_slate_post_tick_callback(tick)
