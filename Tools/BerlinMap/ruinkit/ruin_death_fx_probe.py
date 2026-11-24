"""Throwaway-editor look at dust / collapse effect candidates for the ruin death (not a test; a contact sheet).
Launch: ruin_death_run.sh <logname> [timeout_s] fx      (the third argument picks this script)
Spawns the variants below in PIE on RTS_ToCopySmall in groups of three, 10 m apart, behind a one-storey slab
(B1_Facade1St_A, 7 m wide, 5 m tall) for scale, and takes pictures 0.4 / 1.2 / 2.5 / 4 s after the spawn from the game
camera height (28 m, the framing of the death test). Logs which components are still active 8 s after the spawn (a
looping system never ends: unusable as a one-shot death effect).
A variant is (label, system, component scale, {user parameter: float or (r, g, b, a)}).
Pictures: Saved/Berlin/ruin_death_test/fx_<group>_<t>s.png, order left to right = the logged order.
First sheet of 2026-11-13 (twelve systems at scale 1.5): the white NS_DestructionSmoke(_Large) reads as steam, the
NS_RTS_GroundDust* / NS_RTS_DustSmoke_07 / NS_RTS_StaticDust_22 / NS_DebriDust / NS_DustSmoke_03 systems loop, the
NS_Deploy_* dusts are too faint.
"""
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test/'
STONE = '/Game/RTS_Survival/ImpactSfxVfx/'
HUGE = STONE + '4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge'
MEDIUM = STONE + '2MediumCalibre/Stone/NS_MediumCaliber_StoneHit'
HEAVY = STONE + '3HeavyCalibre/Stone/NS_HeavyCaliber_StoneImpact'
SUPER = STONE + '4SuperHeavyCalibre/Stone/NS_SuperHeavyCaliber_StoneImpact'
ARTY = STONE + '5Artillery/Stone/NS_Artillery_StoneImpact'
DUSTY = (0.30, 0.27, 0.23, 1.0)
TINT = {'Color Smoke': DUSTY, 'GroundSmokeColor': DUSTY, 'Color': DUSTY}
VARIANTS = [
    ('artillery', ARTY, 1.0, {}),
    ('artillery Scale 2', ARTY, 1.0, {'Scale': 2.0}),
    ('artillery Scale 3 life 2', ARTY, 1.0, {'Scale': 3.0, 'LifetimeMlt': 2.0, 'LifetimeMult': 2.0}),
    ('artillery component x2.5', ARTY, 2.5, {}),
    ('artillery Scale 2 count 0.3 tint', ARTY, 1.0, dict(TINT, Scale=2.0, **{'Spawn Count': 0.3})),
    ('artillery Scale 2 velocity 0.4 life 2', ARTY, 1.0, {'Scale': 2.0, 'Velocity Speed Mlt': 0.4, 'LifetimeMlt': 2.0, 'LifetimeMult': 2.0}),
    ('huge 0.5 tint velocity 0.3', HUGE, 0.5, dict(TINT, **{'Velocity Speed Mlt': 0.3})),
    ('huge 0.5 tint velocity 0.1', HUGE, 0.5, dict(TINT, **{'Velocity Speed Mlt': 0.1})),
    ('huge 1.0 tint velocity 0.4', HUGE, 1.0, dict(TINT, **{'Velocity Speed Mlt': 0.4})),
    ('huge 1.0 tint calibre 0.5', HUGE, 1.0, dict(TINT, NormalizedWeaponCalibre=0.5)),
    ('huge 1.0 tint calibre 2', HUGE, 1.0, dict(TINT, NormalizedWeaponCalibre=2.0)),
    ('huge 0.7 tint life 1.5 velocity 0.3', HUGE, 0.7, dict(TINT, LifetimeMlt=1.5, **{'Velocity Speed Mlt': 0.3})),
]
SPACING = 1000.0
TIMES = [0.4, 1.2, 2.5, 4.0]
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
                                          'BP_BerlinRuin_B1_Facade1St_A.BP_BerlinRuin_B1_Facade1St_A_C')
            actors.spawn_actor_from_class(cls, unreal.Vector(0, -700, S['z']), unreal.Rotator(0, 0, 90))
            actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 6000, 2000))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif S['phase'] == 'wait_pie' and game_world() is not None and S['t'] - S['mark'] > 12.0:
            world = game_world()
            unreal.GameplayStatics.set_game_paused(world, False)
            unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
            cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
            camera = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 6000, 2000)).length())
            eye = unreal.Vector(200, 1900, S['z'] + 2800)
            look = unreal.Vector(0, 0, S['z'] + 200)
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
            group = VARIANTS[S['group'] * 3:S['group'] * 3 + 3]
            if not group:
                unreal.SystemLibrary.quit_editor()
                S['phase'] = 'done'
                return
            S['comps'] = []
            for index, (label, path, scale, params) in enumerate(group):
                system = unreal.load_asset(path)
                location = unreal.Vector(-SPACING + index * SPACING, 0, S['z'] + 60)
                comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(
                    world, system, location, unreal.Rotator(0, 0, 0), unreal.Vector(scale, scale, scale), True, True) if system else None
                if comp is not None:
                    for name, value in params.items():
                        if isinstance(value, tuple):
                            comp.set_variable_linear_color(name, unreal.LinearColor(*value))
                        else:
                            comp.set_variable_float(name, float(value))
                S['comps'].append((label, comp))
            log(f'group {S["group"]} left to right: {[n for n, _ in S["comps"]]} (missing: {[n for n, c in S["comps"] if c is None]})')
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
            elif not S['times'] and t > 8.0:
                state = []
                for name, comp in S['comps']:
                    try:
                        state.append((name, bool(comp is not None and unreal.SystemLibrary.is_valid(comp) and comp.is_active())))
                    except Exception:  # noqa: BLE001
                        state.append((name, False))
                log(f'group {S["group"]} still active after 8 s: {state}')
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
    if S['t'] > 220.0 and S['phase'] != 'done':
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
