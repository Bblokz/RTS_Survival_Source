"""Throwaway-editor PIE test of the Oder-Warthe Werk targets, loaded from disk (never the shared editor or a mission map).
Launch: sh run_targets_test.sh <run> [timeout_s]   (RTS_ToCopySmall)

Lanes (x), target at y 0 turned to face +Y, a player tank 24 m away at +Y ordered to AttackActor it:
  tunnel_50  BP_OW_TunnelMouth  vs Pz III J (50 mm)   -> no damage (flat 150 per hit); then it attacks a control
             jammer beside it, which must take damage (proves the Pz III was firing)
  tunnel_75  BP_OW_TunnelMouth  vs Pz IV G (KwK 40)   -> 221.4 - 150 = 71.4 per shell
  tunnel_88  BP_OW_TunnelMouth  vs Tiger H1 (KwK 36)  -> 301.8 - 150 = 151.8 per shell, fires until it collapses
  porch      BP_OW_Target_CommandPorch vs Pz IV G     -> dies (1500 HP, 20 off per hit)
  jammer     BP_OW_Target_Jammer       vs Pz IV G     -> dies (900 HP)
  railgate   BP_OW_Target_RailGate     vs Pz IV G     -> dies (1500 HP, 20 off per hit)
Every death must swap the StaticMesh to the *_Destroyed / _Collapsed mesh and leave the actor in the world.
Results: Saved/OderWarthe/targets/test/<run>/result.json + PNG pictures (intact and destroyed, real materials).
"""
import json
import os

import unreal

RUN = os.environ.get('TARGET_RUN', 'run')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/test/{RUN}/'
BPS = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets/Blueprints/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
VEHICLES = {
    'PzIII_J': V + 'Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J.Ger_Vehicle_PanzerIII_J_C',
    'PzIV_G': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C',
    'TigerH1': V + 'Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C',
}
# key: (lane x, Blueprint, target yaw, vehicle, destroyed mesh expected, picture distance)
LANES = {
    'tunnel_50': (0, 'BP_OW_TunnelMouth', 90.0, 'PzIII_J', 'SM_OW_TunnelPortal_Collapsed', 3200),
    'tunnel_75': (5000, 'BP_OW_TunnelMouth', 90.0, 'PzIV_G', 'SM_OW_TunnelPortal_Collapsed', 3200),
    'tunnel_88': (10000, 'BP_OW_TunnelMouth', 90.0, 'TigerH1', 'SM_OW_TunnelPortal_Collapsed', 3200),
    'porch': (15000, 'BP_OW_Target_CommandPorch', 90.0, 'PzIV_G', 'SM_OW_Target_CommandPorch_Destroyed', 1500),
    'jammer': (19000, 'BP_OW_Target_Jammer', 0.0, 'PzIV_G', 'SM_OW_Target_Jammer_Destroyed', 2200),
    'railgate': (23000, 'BP_OW_Target_RailGate', 90.0, 'PzIV_G', 'SM_OW_Target_RailGate_Destroyed', 1500),
}
CONTROL = (-1500, 'BP_OW_Target_Jammer', 0.0)        # the Pz III's second target (tunnel_50 lane)
TANK_Y = 2400.0
TUNNEL_SECONDS = 50.0       # Pz III on the tunnel before it switches to the control jammer
FIGHT_SECONDS = 170.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'targets': {}, 'tanks': {}, 'hp': {}, 'hits': {},
     'result': {}, 'pics': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('OWTARGETTEST ' + message)


def finish():
    try:
        os.makedirs(ROOT, exist_ok=True)
        json.dump(dict(log=S['log'], result=S['result'], hits=S['hits']), open(ROOT + 'result.json', 'w'), indent=1,
                  default=str)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def alive(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def bp_class(name):
    return unreal.load_class(None, f'{BPS}{name}.{name}_C')


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f'editor map {world.get_path_name()}')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    spots = [(k, x, bp, yaw) for k, (x, bp, yaw, _, _, _) in LANES.items()] + [('control',) + CONTROL]
    for key, x, bp, yaw in spots:
        z = ground_z(world, x, 0)
        a = actors.spawn_actor_from_class(bp_class(bp), unreal.Vector(x, 0, z), unreal.Rotator(0, 0, yaw))
        log(f'{key}: placed {bp} at z {z:.0f}: {a is not None}')
    for key, (x, _, _, vehicle, _, _) in LANES.items():
        tank = actors.spawn_actor_from_class(unreal.load_class(None, VEHICLES[vehicle]),
                                             unreal.Vector(x, TANK_Y, ground_z(world, x, TANK_Y) + 50),
                                             unreal.Rotator(0, 0, -90))
        log(f'{key}: {vehicle} {tank is not None}')
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
        loc = actor.get_actor_location()
        for key, x in [(k, v[0]) for k, v in LANES.items()] + [('control', CONTROL[0])]:
            if abs(loc.x - x) < 30 and abs(loc.y) < 30:
                S['targets'][key] = actor
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for key, x in [(k, v[0]) for k, v in LANES.items()]:
            if abs(loc.x - x) < 300 and abs(loc.y - TANK_Y) < 500:
                S['tanks'][key] = tank
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    capture = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    comp = capture.capture_component2d
    S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                             unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property('texture_target', S['rt'])
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    comp.set_editor_property('fov_angle', 60.0)
    capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    S['capture'] = capture
    log(f'runtime targets {sorted(S["targets"])} tanks {sorted(S["tanks"])}')


def mesh_comp(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]


def state(actor):
    if not alive(actor):
        return dict(gone=True)
    m = mesh_comp(actor)
    out = dict(health=round(actor.get_editor_property('health'), 1),
               reduction=actor.get_editor_property('damage_reduction'),
               mesh=m.static_mesh.get_name() if m.static_mesh else None,
               collision=str(m.get_collision_enabled()).split('.')[-1])
    try:
        out['unit_alive'] = bool(actor.is_unit_alive())
    except Exception:  # noqa: BLE001
        out['unit_alive'] = None
    nav = actor.get_component_by_class(unreal.RTSNavCollisionEnvObstacle)
    if nav is not None:
        out['nav_area'] = nav.get_editor_property('area_class').get_name()
    return out


def picture(name, key):
    x = LANES[key][0] if key in LANES else CONTROL[0]
    dist = LANES[key][5] if key in LANES else 2200
    look = unreal.Vector(x, 0, S['ground'].get(key, 0.0) + 200)
    eye = look + unreal.Vector(dist * 0.55, dist * 0.75, dist * 0.65)
    capture = S['capture']
    capture.set_actor_location(eye, False, True)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    capture.capture_component2d.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), name + '.png')


def sample():
    for key, actor in S['targets'].items():
        if not alive(actor):
            continue
        hp = actor.get_editor_property('health')
        last = S['hp'].get(key)
        if last is not None and abs(hp - last) > 0.01:
            S['hits'].setdefault(key, []).append([round(S['t'] - S['fight'], 1), round(last - hp, 1)])
            log(f'{key} hit {last - hp:.1f} -> {hp:.1f}')
        if last is not None and last > 0 and hp <= 0:
            S.setdefault('died', {})[key] = S['t']
        S['hp'][key] = hp


def order(tank_key, target_key):
    tank, target = S['tanks'].get(tank_key), S['targets'].get(target_key)
    try:
        result = tank.attack_actor(target, True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'[:140]
    log(f'{tank_key} AttackActor {target_key} -> {result}')


def verdict():
    hits = S['hits']
    out = {}
    t50 = hits.get('tunnel_50', [])
    ctrl = hits.get('control', [])
    out['tunnel_50mm_ignored'] = dict(passed=not t50 and bool(ctrl), tunnel_hits=t50, control_hits=len(ctrl))
    for key, per in (('tunnel_75', 71.4), ('tunnel_88', 151.8)):
        h = [d for _, d in hits.get(key, [])]
        ok = bool(h) and all(abs(d - per) < 2.0 or d < per for d in h) and any(abs(d - per) < 2.0 for d in h)
        out[key] = dict(passed=ok, expected_per_shell=per, hits=h)
    for key, (x, bp, yaw, veh, dmesh, _) in LANES.items():
        if key in ('tunnel_50', 'tunnel_75'):
            continue
        st = state(S['targets'].get(key))
        died = key in S.get('died', {})
        ok = died and not st.get('gone') and st.get('mesh') == dmesh
        out[key + '_death'] = dict(passed=bool(ok), died=died, end_state=st, shots=len(hits.get(key, [])))
    intact = {'BP_OW_TunnelMouth': 'SM_OW_TunnelPortal', 'BP_OW_Target_CommandPorch': 'SM_OW_Target_CommandPorch',
              'BP_OW_Target_Jammer': 'SM_OW_Target_Jammer', 'BP_OW_Target_RailGate': 'SM_OW_Target_RailGate'}
    setup = S['result'].get('setup', {})
    wrong = {k: v.get('mesh') for k, v in setup.items()
             if v.get('mesh') != intact[LANES[k][1] if k in LANES else CONTROL[1]]}
    out['intact_meshes'] = dict(passed=not wrong and len(setup) == len(LANES) + 1, wrong=wrong)
    t75 = state(S['targets'].get('tunnel_75'))
    out['tunnel_nav_noop'] = dict(passed=t75.get('nav_area') == 'NavArea_Default', nav_area=t75.get('nav_area'))
    return out


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        step()
    finally:
        S['busy'] = False


def step():
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
                find_runtime(world)
                S['ground'] = {k: ground_z(world, v[0], 0) for k, v in LANES.items()}
                S['ground']['control'] = ground_z(world, CONTROL[0], 0)
                S['result']['setup'] = {k: state(a) for k, a in S['targets'].items()}
                log(f'setup {json.dumps(S["result"]["setup"])}')
                S['phase'], S['mark'] = 'pictures_intact', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'pictures_intact' and S['t'] - S['mark'] > 4.0:
            for key in LANES:
                if key in ('tunnel_50', 'tunnel_88', 'porch', 'jammer', 'railgate'):
                    picture(f'{key}_intact', key)
            S['fight'] = S['t']
            for key in LANES:
                order(key, key)
            sample()
            S['phase'], S['mark'] = 'fight', S['t']
        elif phase == 'fight':
            sample()
            elapsed = S['t'] - S['mark']
            if elapsed > TUNNEL_SECONDS and not S.get('switched'):
                S['switched'] = True
                order('tunnel_50', 'control')
            for key, at in list(S.get('died', {}).items()):
                if S['t'] - at > 6.0 and key not in S.get('pictured', set()):
                    S.setdefault('pictured', set()).add(key)
                    if key in LANES:
                        picture(f'{key}_destroyed', key)
            needed = {'tunnel_88', 'porch', 'jammer', 'railgate'}
            done = needed <= set(S.get('pictured', set())) and S.get('switched') and S['hits'].get('control')
            if done or elapsed > FIGHT_SECONDS:
                for key in needed - set(S.get('pictured', set())):
                    picture(f'{key}_end', key)
                S['result']['end'] = {k: state(a) for k, a in S['targets'].items()}
                S['result']['verdict'] = v = verdict()
                S['result']['all_passed'] = all(r['passed'] for r in v.values())
                for k, r in v.items():
                    log(f'VERDICT {k} {json.dumps(r, default=str)}')
                log(f'ALL PASSED {S["result"]["all_passed"]}')
                finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 420.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
