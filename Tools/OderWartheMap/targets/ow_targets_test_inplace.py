"""Throwaway-editor PIE test of the Werk targets where they will stand (RTS_ToCopySmall, Blueprints from disk).
Launch: TARGET_SCRIPT=ow_targets_test_inplace.py sh run_targets_test.sh <run> [timeout_s]

  gate_live    BP_OW_TunnelMouth + BP_OW_Target_RailGate at one transform; a Pz IV attacks the gate:
               the gate takes the hits and dies, the live mouth takes none
  gate_buried  the same pair, the mouth collapsed first (it keeps blocking shells); the Pz IV attacks the gate:
               records whether the buried gate can still be hit (data for the mission's open question)
  porch_front  BP_OW_Target_CommandPorch on the Entrance socket of a real SM_OW_Werk_Kasemattennase, Pz IV in front
  porch_side   the same, Pz IV off the alcove's open end
  hide_reveal  BP_OW_Target_Jammer hidden with collision off and attacked for 25 s (no hits), then revealed (hits)
Pictures: lit final colour and base colour of each target. Results: Saved/OderWarthe/targets/test/<run>/.
"""
import json
import os

import unreal

RUN = os.environ.get('TARGET_RUN', 'inplace')
NO_PICTURES = os.environ.get('NO_PICTURES') == '1'      # -nullrhi runs: gameplay checks only
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/test/{RUN}/'
BPS = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets/Blueprints/'
WERK = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Werke/SM_OW_Werk_Kasemattennase'
PZIV = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/'
        'Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C')
ENTRANCE = (1580.0, -120.0, 5.0)
LANE_X = {'gate_live': 0, 'gate_buried': 9000, 'porch_front': 18000, 'porch_side': -10000, 'hide_reveal': -19000}
HIDDEN_SECONDS = 25.0
FIGHT_SECONDS = 150.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'actors': {}, 'tanks': {}, 'hp': {}, 'hits': {},
     'result': {}}


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


def tank_spots():
    """lane -> (tank x, tank y, yaw)"""
    e = ENTRANCE
    return {'gate_live': (0, 2400, -90), 'gate_buried': (9000, 2400, -90),
            'porch_front': (18000 + e[0] + 2600, e[1], 180), 'porch_side': (-10000 + e[0] + 150, e[1] - 2700, 90),
            'hide_reveal': (-19000, 2400, -90)}


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for lane in ('gate_live', 'gate_buried'):
        x = LANE_X[lane]
        z = ground_z(world, x, 0)
        actors.spawn_actor_from_class(bp_class('BP_OW_TunnelMouth'), unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 90))
        actors.spawn_actor_from_class(bp_class('BP_OW_Target_RailGate'), unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 90))
    werk = unreal.load_asset(WERK)
    for lane in ('porch_front', 'porch_side'):
        x = LANE_X[lane]
        z = ground_z(world, x, 0)
        w = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 0))
        w.static_mesh_component.set_static_mesh(werk)
        actors.spawn_actor_from_class(bp_class('BP_OW_Target_CommandPorch'),
                                      unreal.Vector(x + ENTRANCE[0], ENTRANCE[1], z + ENTRANCE[2]), unreal.Rotator(0, 0, 0))
    x = LANE_X['hide_reveal']
    actors.spawn_actor_from_class(bp_class('BP_OW_Target_Jammer'), unreal.Vector(x, 0, ground_z(world, x, 0)),
                                  unreal.Rotator(0, 0, 0))
    for lane, (tx, ty, yaw) in tank_spots().items():
        actors.spawn_actor_from_class(unreal.load_class(None, PZIV), unreal.Vector(tx, ty, ground_z(world, tx, ty) + 50),
                                      unreal.Rotator(0, 0, yaw))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(-35, 40, 0))
    sun.light_component.set_editor_property('intensity', 6.0)
    sun.light_component.set_editor_property('light_color', unreal.Color(255, 245, 230, 255))
    actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))
    log('placed')


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
        loc = actor.get_actor_location()
        name = actor.get_class().get_name()
        for lane, x in LANE_X.items():
            lx = x + (ENTRANCE[0] if lane.startswith('porch') else 0)
            ly = ENTRANCE[1] if lane.startswith('porch') else 0
            if abs(loc.x - lx) < 30 and abs(loc.y - ly) < 30:
                kind = ('mouth' if 'TunnelMouth' in name else 'gate' if 'RailGate' in name else
                        'porch' if 'Porch' in name else 'jammer')
                S['actors'][f'{lane}.{kind}'] = actor
    spots = tank_spots()
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        for lane, (tx, ty, _) in spots.items():
            if abs(loc.x - tx) < 300 and abs(loc.y - ty) < 300:
                S['tanks'][lane] = tank
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    if NO_PICTURES:
        log(f'runtime actors {sorted(S["actors"])} tanks {sorted(S["tanks"])}')
        return
    capture = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    comp = capture.capture_component2d
    S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                             unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property('texture_target', S['rt'])
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    comp.set_editor_property('fov_angle', 50.0)
    capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    S['capture'] = capture
    log(f'runtime actors {sorted(S["actors"])} tanks {sorted(S["tanks"])}')


def picture(name, look, eye):
    if NO_PICTURES:
        return
    capture = S['capture']
    capture.set_actor_location(eye, False, True)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    comp = capture.capture_component2d
    for tag, source in (('lit', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR),
                        ('basecolor', unreal.SceneCaptureSource.SCS_BASE_COLOR)):
        comp.set_editor_property('capture_source', source)
        comp.capture_scene()
        unreal.RenderingLibrary.export_render_target(game_world(), S['rt'], ROOT.rstrip('/'), f'{name}_{tag}.png')


def views():
    g = S['ground']
    e = ENTRANCE
    return {
        'porch': (unreal.Vector(18000 + e[0] + 80, e[1] - 60, g['porch_front'] + 250),
                  unreal.Vector(18000 + e[0] + 800, e[1] - 750, g['porch_front'] + 1100)),
        'gate': (unreal.Vector(0, 20, g['gate_live'] + 200), unreal.Vector(250, 1500, g['gate_live'] + 420)),
        'tunnel_collapsed': (unreal.Vector(9000, 0, g['gate_buried'] + 250),
                             unreal.Vector(9000 + 1800, 2600, g['gate_buried'] + 2200)),
        'jammer': (unreal.Vector(-19000, 200, g['hide_reveal'] + 400),
                   unreal.Vector(-19000 + 1300, 2300, g['hide_reveal'] + 1500)),
    }


def mesh_name(actor):
    m = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]
    return m.static_mesh.get_name() if m.static_mesh else None


def sample():
    for key, actor in S['actors'].items():
        if not alive(actor):
            continue
        hp = actor.get_editor_property('health')
        last = S['hp'].get(key)
        if last is not None and abs(hp - last) > 0.01:
            S['hits'].setdefault(key, []).append([round(S['t'] - S['fight'], 1), round(last - hp, 1)])
            log(f'{key} hit {last - hp:.1f} -> {hp:.1f}')
        S['hp'][key] = hp


def order(lane, key):
    try:
        result = S['tanks'][lane].attack_actor(S['actors'][key], True)
    except Exception as error:  # noqa: BLE001
        result = f'failed {error!r}'[:140]
    log(f'{lane} AttackActor {key} -> {result}')


def set_revealed(actor, revealed):
    actor.set_actor_hidden_in_game(not revealed)
    actor.set_actor_enable_collision(revealed)


def verdict():
    h = S['hits']
    a = S['actors']
    out = {}
    gate = h.get('gate_live.gate', [])
    out['gate_live'] = dict(passed=bool(gate) and not h.get('gate_live.mouth') and
                            mesh_name(a['gate_live.gate']) == 'SM_OW_Target_RailGate_Destroyed',
                            gate_hits=len(gate), mouth_hits=len(h.get('gate_live.mouth', [])))
    out['gate_buried'] = dict(passed=True, info=True, gate_hits=len(h.get('gate_buried.gate', [])),
                              collapsed_mouth_hits=len(h.get('gate_buried.mouth', [])),
                              mouth_mesh=mesh_name(a['gate_buried.mouth']),
                              gate_mesh=mesh_name(a['gate_buried.gate']))
    for lane in ('porch_front', 'porch_side'):
        hits = h.get(f'{lane}.porch', [])
        out[lane] = dict(passed=bool(hits) and mesh_name(a[f'{lane}.porch']) == 'SM_OW_Target_CommandPorch_Destroyed',
                         hits=len(hits), per_hit=sorted(set(d for _, d in hits)))
    hidden = [x for x in h.get('hide_reveal.jammer', []) if x[0] < S.get('revealed_at', 1e9) - S['fight']]
    after = [x for x in h.get('hide_reveal.jammer', []) if x[0] >= S.get('revealed_at', 1e9) - S['fight']]
    out['hide_reveal'] = dict(passed=not hidden and bool(after), hits_hidden=len(hidden), hits_revealed=len(after))
    out['tanks'] = dict(passed=True, info=True, at={k: [round(c) for c in (t.get_actor_location().x,
                        t.get_actor_location().y, t.get_actor_location().z)] for k, t in S['tanks'].items() if alive(t)},
                        ground=S['ground'])
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
                S['ground'] = {k: ground_z(world, x, 0) for k, x in LANE_X.items()}
                set_revealed(S['actors']['hide_reveal.jammer'], False)
                S['phase'], S['mark'] = 'pictures', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'pictures' and S['t'] - S['mark'] > 5.0:
            v = views()
            picture('porch_intact', *v['porch'])
            picture('gate_intact', *v['gate'])
            unreal.GameplayStatics.apply_damage(S['actors']['gate_buried.mouth'], 99999.0, None, None, unreal.DamageType)
            log('collapsed gate_buried.mouth')
            S['fight'] = S['t']
            sample()
            order('gate_live', 'gate_live.gate')
            order('gate_buried', 'gate_buried.gate')
            order('porch_front', 'porch_front.porch')
            order('porch_side', 'porch_side.porch')
            order('hide_reveal', 'hide_reveal.jammer')
            S['phase'], S['mark'] = 'fight', S['t']
        elif phase == 'fight':
            sample()
            elapsed = S['t'] - S['mark']
            if elapsed > 8.0 and not S.get('pic_tunnel'):
                S['pic_tunnel'] = True
                picture('tunnel_collapsed', *views()['tunnel_collapsed'])
            if elapsed > HIDDEN_SECONDS and not S.get('revealed_at'):
                S['revealed_at'] = S['t']
                set_revealed(S['actors']['hide_reveal.jammer'], True)
                picture('jammer_intact', *views()['jammer'])
                order('hide_reveal', 'hide_reveal.jammer')
                log('revealed the jammer')
            dead = [k for k in ('gate_live.gate', 'porch_front.porch', 'porch_side.porch', 'hide_reveal.jammer')
                    if S['hp'].get(k, 1) <= 0]
            if len(dead) == 4 and not S.get('all_dead_at'):
                S['all_dead_at'] = S['t']          # the destroyed mesh loads async: give the swap a moment
            settled = S.get('all_dead_at') and S['t'] - S['all_dead_at'] > 6.0
            if (settled and elapsed > HIDDEN_SECONDS + 10) or elapsed > FIGHT_SECONDS:
                v = views()
                picture('porch_destroyed', *v['porch'])
                picture('gate_destroyed', *v['gate'])
                picture('jammer_destroyed', *v['jammer'])
                S['result']['verdict'] = out = verdict()
                S['result']['all_passed'] = all(r['passed'] for r in out.values())
                for k, r in out.items():
                    log(f'VERDICT {k} {json.dumps(r, default=str)}')
                log(f'ALL PASSED {S["result"]["all_passed"]}')
                finish()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {phase}: {error!r}')
        finish()
    if S['t'] > 400.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
