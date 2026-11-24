"""Throwaway-editor lineup of all 16 desert rock Blueprints (ue_blueprints.py) for a look check.
Large rocks in a row at y 0, medium rocks in a row 14 m in front; PIE, then screenshots from an RTS-like angle and
one closer shot per row. Never in the shared editor; RTS_ToCopySmall is never saved; the editor quits at the end.
Launch: sh fire_test_run.sh <logname> 300 with ROCK_SCRIPT=rock_lineup_shot.py, or directly through lineup_run.sh.
Pictures: Saved/DesertRocks/lineup/*.png
"""
import json

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/lineup/'
BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'
ROCKS = json.load(open('D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/DesertRockKit/Model/out/rocks.json'))
GAP = 250.0
ROW_Y = {'large': 0.0, 'medium': 1400.0}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'pics': [], 'log': []}


def log(message):
    S['log'].append(message)
    unreal.log('DROCKLINE ' + message)


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def layout():
    rows = {'large': [], 'medium': []}
    for name, info in ROCKS.items():
        if name[0] in 'LM' and '_' in name:
            rows[info['class']].append((name, info['dims'][0] * 100.0))
    spots = {}
    for cls, items in rows.items():
        items.sort()
        width = sum(w for _, w in items) + GAP * (len(items) - 1)
        x = -width / 2
        for name, w in items:
            spots[name] = (x + w / 2, ROW_Y[cls])
            x += w + GAP
    return spots


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    S['spots'] = layout()
    for name, (x, y) in S['spots'].items():
        cls = unreal.load_class(None, f'{BP_DIR}BP_DesertRock_{name}.BP_DesertRock_{name}_C')
        if cls is None:
            log(f'missing BP {name}')
            continue
        actors.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(world, x, y)), unreal.Rotator(0, 0, 0))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 9000, 5000))
    log(f'placed {len(S["spots"])} rocks')


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def camera():
    cams = unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.CameraActor)
    return min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 9000, 5000)).length())


def queue():
    xs_l = [x for n, (x, y) in S['spots'].items() if n[0] == 'L']
    xs_m = [x for n, (x, y) in S['spots'].items() if n[0] == 'M']
    span = max(xs_l) - min(xs_l)
    S['pics'] = [
        ('_warmup', unreal.Vector(0, span * 0.75 + 1400, span * 0.5), unreal.Vector(0, 700, 0)),
        ('overview', unreal.Vector(0, span * 0.75 + 1400, span * 0.5), unreal.Vector(0, 700, 0)),
        ('rts_angle', unreal.Vector(0, 5200, 6500), unreal.Vector(0, 700, 0)),
        ('large_left', unreal.Vector(min(xs_l) * 0.5, 3600, 1600), unreal.Vector(min(xs_l) * 0.5, 0, 250)),
        ('large_right', unreal.Vector(max(xs_l) * 0.5, 3600, 1600), unreal.Vector(max(xs_l) * 0.5, 0, 250)),
        ('medium_row', unreal.Vector(0, 1400 + max(xs_m) * 1.3, 900), unreal.Vector(0, 1400, 80)),
    ]


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif S['phase'] == 'wait_pie' and game_world() is not None and S['t'] - S['mark'] > 15.0:
            world = game_world()
            unreal.GameplayStatics.set_game_paused(world, False)
            unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
            queue()
            S['cam'] = camera()
            unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(S['cam'], 0.0)
            S['phase'], S['mark'] = 'pics', S['t']
        elif S['phase'] == 'pics':
            if S['t'] - S['mark'] < (1.5 if S.get('pending') else 3.0):
                return
            if S.get('pending'):
                name = S.pop('pending')
                S['busy'] = True
                try:
                    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, f'{ROOT}{name}.png', camera=S['cam'])
                finally:
                    S['busy'] = False
                log('picture ' + name)
                S['mark'] = S['t']
            elif S['pics']:
                name, eye, look = S['pics'].pop(0)
                S['cam'].set_actor_location(eye, False, True)
                S['cam'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
                S['pending'], S['mark'] = name, S['t']
            else:
                S['phase'] = 'quit'
                S['mark'] = S['t']
        elif S['phase'] == 'quit' and S['t'] - S['mark'] > 3.0:
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
        elif S['t'] > 400.0:
            log('timeout in phase ' + S['phase'])
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'error {error!r}')
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
