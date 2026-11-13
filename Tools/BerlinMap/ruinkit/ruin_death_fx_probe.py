"""Throwaway-editor look at dust / collapse effect candidates for the ruin death (not a test; a contact sheet).
Launch: ruin_death_run.sh <logname> [timeout_s] fx      (the third argument picks this script)
Spawns every candidate Niagara system in PIE on RTS_ToCopySmall in groups of four, 16 m apart, next to a 10 m ruin
slab for scale, and takes pictures 0.4 / 1.5 / 3 / 5 / 8 s after the spawn from the RTS camera height. Logs which
components are still active 11 s after the spawn (a looping system never ends: unusable as a one-shot death effect).
Pictures: Saved/Berlin/ruin_death_test/fx_<group>_<t>s.png, order left to right = the logged order.
"""
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test/'
CANDIDATES = [
    '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large',
    '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke',
    '/Game/RTS_Survival/ImpactSfxVfx/4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge',
    '/Game/RTS_Survival/SpecialEffects/Dust/NS_RTS_DustSmoke_07',
    '/Game/RTS_Survival/SpecialEffects/Dust/NS_RTS_GroundDust16',
    '/Game/RTS_Survival/SpecialEffects/Dust/NS_RTS_GroundDust20',
    '/Game/RTS_Survival/SpecialEffects/Dust/NS_RTS_GroundDust_12',
    '/Game/RTS_Survival/SpecialEffects/Dust/StaticDust/NS_RTS_StaticDust_22',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_HeavyDust',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_LandDust',
    '/Game/DustAndSandStorm/SandStorm/Niagara/NS_DebriDust',
    '/Game/DustAndSandStorm/SandStorm/Niagara/NS_DustSmoke_03',
]
SCALE = 1.5
SPACING = 1600.0
TIMES = [0.4, 1.5, 3.0, 5.0, 8.0]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'group': 0}


def log(message):
    unreal.log('RUINFXPROBE ' + message)


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def tick(delta):
    S['t'] += delta
    if S.get('busy') or S['phase'] == 'done':
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            S['z'] = ground_z(world, 0, 0)
            cls = unreal.load_class(None, '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'
                                          'BP_BerlinRuin_B2_Facade2St_B.BP_BerlinRuin_B2_Facade2St_B_C')
            actors.spawn_actor_from_class(cls, unreal.Vector(-4000, 0, S['z']), unreal.Rotator(0, 0, 90))
            actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif S['phase'] == 'wait_pie' and game_world() is not None and S['t'] - S['mark'] > 12.0:
            world = game_world()
            unreal.GameplayStatics.set_game_paused(world, False)
            unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
            cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
            camera = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
            eye = unreal.Vector(-800, 2600, S['z'] + 2800)
            look = unreal.Vector(-800, 0, S['z'] + 100)
            camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            camera.set_actor_location(eye, False, True)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
            unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)
            S['camera'] = camera
            S['phase'], S['mark'] = 'settle', S['t']
        elif S['phase'] == 'settle' and S['t'] - S['mark'] > 4.0:
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + 'fx__warmup.png', camera=S['camera'])
            S['phase'], S['mark'] = 'spawn', S['t']
        elif S['phase'] == 'spawn' and S['t'] - S['mark'] > 2.5:
            world = game_world()
            group = CANDIDATES[S['group'] * 4:S['group'] * 4 + 4]
            if not group:
                unreal.SystemLibrary.quit_editor()
                S['phase'] = 'done'
                return
            S['comps'] = []
            for index, path in enumerate(group):
                system = unreal.load_asset(path)
                location = unreal.Vector(-2400 + index * SPACING, 0, S['z'] + 100)
                comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(
                    world, system, location, unreal.Rotator(0, 0, 0), unreal.Vector(SCALE, SCALE, SCALE), True, True) if system else None
                S['comps'].append((path.rsplit('/', 1)[1], comp))
            log(f'group {S["group"]} left to right at scale {SCALE}: {[n for n, _ in S["comps"]]} (missing: '
                f'{[n for n, c in S["comps"] if c is None]})')
            S['t0'] = unreal.GameplayStatics.get_time_seconds(world)
            S['times'] = list(TIMES)
            S['phase'] = 'shoot'
        elif S['phase'] == 'shoot':
            world = game_world()
            t = unreal.GameplayStatics.get_time_seconds(world) - S['t0']
            if S['times'] and t >= S['times'][0]:
                at = S['times'].pop(0)
                unreal.AutomationLibrary.take_high_res_screenshot(
                    1600, 900, f'{ROOT}fx_{S["group"]}_{str(at).replace(".", "_")}s.png', camera=S['camera'])
            elif not S['times'] and t > 11.0:
                state = []
                for name, comp in S['comps']:
                    try:
                        state.append((name, bool(comp is not None and unreal.SystemLibrary.is_valid(comp) and comp.is_active())))
                    except Exception:  # noqa: BLE001
                        state.append((name, False))
                log(f'group {S["group"]} still active after 11 s: {state}')
                for _, comp in S['comps']:
                    try:
                        comp.destroy_component(comp)
                    except Exception:  # noqa: BLE001
                        pass
                S['group'] += 1
                S['phase'], S['mark'] = 'spawn', S['t']
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {error!r}')
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'
    if S['t'] > 200.0 and S['phase'] != 'done':
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
