"""Look check for the SandDrift round (NS_SandPush round 3) - request for the private throwaway editor server
(Saved/OreMountains/sandstreak/sand_server.py on Ger_2ndOreMountains; client sand_cmd.py). Saves nothing.

Per view (valley, road, bench): lit shots with MI_SandDrift_A as saved and with the BRACKETS (MI parameters set in
memory only; a tuple value is a vector parameter), a shader-complexity shot, and MOTION_FRAMES frames MOTION_STEP s of
game time apart (scroll / grow-shrink check). The side camera step needs a spawn the python API lacks (unused).
Shots: Saved/OreMountains/sandstreak/drift/.
"""
import importlib
import json
import sys
import time

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap')
import ore_map_build as omb
importlib.reload(omb)
omb.load_layout()

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/dither/'
MI = unreal.load_asset('/Game/RTS_Survival/SpecialEffects/Dust/SandStreak/MI_SandDrift_A')
MEL = unreal.MaterialEditingLibrary
data = json.load(open(omb.FX_FILE))
streaks = [i for i in data['items'] if i[0] == 'SandStreak']
VIEWS = [('valley', streaks[1][2], streaks[1][3]), ('road', streaks[2][2], streaks[2][3]), ('bench', streaks[15][2], streaks[15][3])]
# dither pass 2026-11-16 (DriftDither node: coverage ramps over EdgeSoft up to Opacity, screen-door vs IGN noise)
BRACKETS = [('base', {}), ('op45', {'Opacity': 0.45}), ('op80', {'Opacity': 0.8, 'EdgeSoft': 0.5}),
            ('static', {'TemporalDither': 0.0})]
BASE = {'ColourGain': 0.5, 'ShadeContrast': 0.95, 'NormalStrength': 1.7, 'Opacity': 0.6, 'EdgeSoft': 0.35,
        'TemporalDither': 1.0, 'SandTint': (0.66, 0.48, 0.29)}   # the saved MI / master values
MOTION_FRAMES, MOTION_STEP = 4, 0.75
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.SystemLibrary.execute_console_command(omb.world(), 't.IdleWhenNotForeground 0')
les.editor_request_begin_play()

STEPS = []
for view, x, y in VIEWS:
    STEPS.append(('move', (x, y), 6.0))
    for tag, params in BRACKETS:
        STEPS.append(('mi', params, 1.0))
        STEPS.append(('shot', '%s_%s' % (view, tag), 2.0))
    STEPS.append(('mi', {}, 0.5))
    for k in range(MOTION_FRAMES):
        STEPS.append(('frame', '%s_m%d' % (view, k), MOTION_STEP))
    STEPS.append(('console', 'viewmode shadercomplexity', 1.0))
    STEPS.append(('shot', '%s_complexity' % view, 1.5))
    STEPS.append(('console', 'viewmode lit', 0.5))
S = {'i': 0, 'n': 0, 't0': time.time(), 'ready': False}


def set_mi(params):
    for key, base in BASE.items():
        value = params.get(key, base)
        if isinstance(value, tuple):
            MEL.set_material_instance_vector_parameter_value(MI, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            MEL.set_material_instance_scalar_parameter_value(MI, key, value)
    MEL.update_material_instance(MI)


def run_step(gw, kind, arg):
    if kind == 'move':
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        x, y = arg
        pawn.set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + 43.0), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    elif kind == 'mi':
        set_mi(arg)
    elif kind == 'console':
        unreal.SystemLibrary.execute_console_command(gw, arg)
    elif kind == 'shot':
        # viewport size (multiplier 1): a different size resets the TSR history and the dither shows raw
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1 filename=' + OUT + arg + '.png')
    elif kind == 'frame':
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1 filename=' + OUT + arg + '.png')
    elif kind == 'side':
        x, y = arg
        # 35 m across the wind from the streak, 4 m up, looking back at it (wind yaw 35 deg)
        side = unreal.Vector(x - 3500.0 * 0.574, y + 3500.0 * 0.819, omb.ground_z(x, y) + 400.0)
        cam = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(gw, unreal.CameraActor, unreal.Transform(side))
        unreal.GameplayStatics.finish_spawning_actor(cam, unreal.Transform(side))
        look = unreal.MathLibrary.find_look_at_rotation(side, unreal.Vector(x, y, omb.ground_z(x, y) + 100.0))
        cam.set_actor_rotation(look, False)
        S['cam'] = cam
        unreal.GameplayStatics.get_player_controller(gw, 0).set_view_target_with_blend(cam, 0.0)
    elif kind == 'side_end':
        set_mi({})


def _defer():
    S['n'] += 1
    gw = ues.get_game_world()
    if gw is None:
        return time.time() - S['t0'] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and S['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if not S['ready']:
        if game_t < 20.0:
            return False
        for cmd in ('ShowFlag.PostProcessMaterial 0', 'rts.AmbientFx 3'):
            unreal.SystemLibrary.execute_console_command(gw, cmd)
        S['ready'], S['mark'] = True, game_t
        return False
    if S['i'] >= len(STEPS):
        set_mi({})
        les.editor_request_end_play()
        return True
    kind, arg, wait = STEPS[S['i']]
    if S.get('done_i') != S['i']:
        try:
            run_step(gw, kind, arg)
        except Exception as exc:
            print('step %s failed: %r' % (kind, exc))
        S['done_i'], S['mark'] = S['i'], game_t
        return False
    if game_t - S['mark'] >= wait:
        S['i'] += 1
    return False
