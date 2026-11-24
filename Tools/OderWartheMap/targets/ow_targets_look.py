"""Throwaway-editor look check of the Werk targets in Unreal (lit and base colour, intact and destroyed).
Launch: TARGET_SCRIPT=ow_targets_look.py sh run_targets_test.sh <run>
The porch stands on a real SM_OW_Werk_Kasemattennase Entrance, the rail gate inside a tunnel mouth, the jammer alone;
each is killed with plain damage after the intact pictures. Pictures: Saved/OderWarthe/targets/test/<run>/look_*.png.
"""
import os

import unreal

RUN = os.environ.get('TARGET_RUN', 'look')
ROOT = f'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/test/{RUN}/'
BPS = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets/Blueprints/'
WERK = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Werke/SM_OW_Werk_Kasemattennase'
E = (1580.0, -120.0, 5.0)
X = {'porch': 0.0, 'gate': 9000.0, 'jammer': 18000.0}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'a': {}}


def log(m):
    unreal.log('OWTARGETTEST ' + m)


def ground(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def cls(n):
    return unreal.load_class(None, f'{BPS}{n}.{n}_C')


def place():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    act = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    z = {k: ground(world, x, 6000) for k, x in X.items()}        # open ground, before anything is placed
    S['z'] = z
    w = act.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(X['porch'], 0, z['porch']), unreal.Rotator(0, 0, 0))
    w.static_mesh_component.set_static_mesh(unreal.load_asset(WERK))
    act.spawn_actor_from_class(cls('BP_OW_Target_CommandPorch'),
                               unreal.Vector(X['porch'] + E[0], E[1], z['porch'] + E[2]), unreal.Rotator(0, 0, 0))
    act.spawn_actor_from_class(cls('BP_OW_TunnelMouth'), unreal.Vector(X['gate'], 0, z['gate']), unreal.Rotator(0, 0, 90))
    act.spawn_actor_from_class(cls('BP_OW_Target_RailGate'), unreal.Vector(X['gate'], 0, z['gate']), unreal.Rotator(0, 0, 90))
    act.spawn_actor_from_class(cls('BP_OW_Target_Jammer'), unreal.Vector(X['jammer'], 0, z['jammer']), unreal.Rotator(0, 0, 0))
    sun = act.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(-40, 35, 0))
    sun.light_component.set_editor_property('intensity', 7.0)
    act.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 12100, 2000))


def views():
    z = S['z']
    px, gx, jx = X['porch'] + E[0], X['gate'], X['jammer']
    return {
        'porch': (unreal.Vector(px + 90, E[1] - 60, z['porch'] + 180), unreal.Vector(px + 760, E[1] - 820, z['porch'] + 900)),
        'gate': (unreal.Vector(gx, 30, z['gate'] + 220), unreal.Vector(gx + 260, 1600, z['gate'] + 450)),
        'jammer': (unreal.Vector(jx, 220, z['jammer'] + 350), unreal.Vector(jx + 1100, 1900, z['jammer'] + 1300)),
    }


def shoot(tag):
    cap = S['cap']
    comp = cap.capture_component2d
    for name, (look, eye) in views().items():
        cap.set_actor_location(eye, False, True)
        cap.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
        for kind, src in (('lit', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR),
                          ('basecolor', unreal.SceneCaptureSource.SCS_BASE_COLOR)):
            comp.set_editor_property('capture_source', src)
            comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(S['world'], S['rt'], ROOT.rstrip('/'), f'look_{name}_{tag}_{kind}.png')
    log('pictures ' + tag)


def tick(dt):
    S['t'] += dt
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        step()
    except Exception as e:  # noqa: BLE001
        log(f'ERROR {S["phase"]}: {e!r}')
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        S['busy'] = False


def step():
    p = S['phase']
    if p == 'wait_load' and S['t'] > 15:
        place()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'pie', S['t']
    elif p == 'pie' and S['t'] - S['mark'] > 12:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        S['world'] = world
        unreal.GameplayStatics.set_game_paused(world, False)
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
            if actor.get_class().get_name().startswith('BP_FogOfWar_Manager'):
                for c in actor.get_components_by_class(unreal.PostProcessComponent):
                    c.set_editor_property('enabled', False)
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DestructableEnvActor):
            n = a.get_class().get_name()
            if 'Porch' in n or 'RailGate' in n or 'Jammer' in n:
                S['a'][n] = a
        cap = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
        S['rt'] = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900,
                                                                 unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp = cap.capture_component2d
        comp.set_editor_property('texture_target', S['rt'])
        comp.set_editor_property('capture_every_frame', False)
        comp.set_editor_property('capture_on_movement', False)
        comp.set_editor_property('fov_angle', 55.0)
        cap.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        S['cap'] = cap
        S['phase'], S['mark'] = 'intact', S['t']
    elif p == 'intact' and S['t'] - S['mark'] > 4:
        shoot('intact')
        for a in S['a'].values():
            unreal.GameplayStatics.apply_damage(a, 99999.0, None, None, unreal.DamageType)
        S['phase'], S['mark'] = 'dead', S['t']
    elif p == 'dead' and S['t'] - S['mark'] > 8:
        shoot('destroyed')
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    if S['t'] > 240 and S['phase'] != 'done':
        log('timeout')
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log('look script loaded')
