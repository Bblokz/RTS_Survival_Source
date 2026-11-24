"""One A/B verification run for the masked SandStreak rework (ore_cmd request for the throwaway editor server).

    (throwaway editor on Ger_2ndOreMountains running ore_editor_server.py)
    python Source/Tools/OreMountainsMap/ore_cmd.py Source/Tools/OreMountainsMap/ore_sandstreak_ab.py --timeout 1500
    python Source/Tools/OreMountainsMap/ore_sandstreak_ab.py --report <editor log>

States, in this order, all in one play session: AFTER (NS_SandPush as saved, masked), BEFORE (every streak actor
switched to the untouched copy Dev/NS_SandPush_BeforeMasked, actor overrides kept), OFF (streaks hidden).
Per view and state: a lit HighResShot, a shader-complexity HighResShot, 6 consecutive plain frames (crawl check).
At the valley view: 3 ProfileGPU dumps per state, marked SAND_PROFILE_BEGIN <state> in the log. CAUTION
(2026-11-16): in a throwaway editor some dumps profile the level editor viewport, not the play view (no NS_SandPush
draws in them), so the dumps did not give a usable A/B; the shader-complexity shots are the evidence.
Shots: Saved/OreMountains/sandstreak/ab/.
"""
import sys

if __name__ == '__main__' and '--report' in sys.argv:
    import re
    import statistics
    log = sys.argv[sys.argv.index('--report') + 1]
    passes = ('Scene', 'PrePass', 'BasePass', 'Translucency', 'Velocity', 'NiagaraGPUSimulation', 'ShadowDepths')
    line = re.compile(r'LogRHI:\s+[\d.]+%\s*([\d.]+)ms\s+(.+?)(?:\s+(\d+) draws?\b.*)?$')
    dumps, current = [], None
    for raw in open(log, 'r', encoding='utf-8', errors='replace'):
        if 'SAND_PROFILE_BEGIN' in raw:
            current = {'state': raw.strip().rsplit(' ', 1)[1]}
            dumps.append(current)
            continue
        if current is None or 'LogRHI:' not in raw:
            continue
        m = line.search(raw.rstrip())
        if not m:
            continue
        key = m.group(2).strip().split(' ')[0].split('(')[0]
        if key in passes and key not in current:
            current[key] = (float(m.group(1)), int(m.group(3) or 0))
    for state in ('AFTER', 'BEFORE', 'OFF'):
        runs = [d for d in dumps if d['state'] == state]
        cells = []
        for key in passes:
            vals = [r[key] for r in runs if key in r]
            cells.append('%s %.2f ms/%d' % (key, statistics.median(v[0] for v in vals), statistics.median(v[1] for v in vals)) if vals else key + ' -')
        print(state, '(n=%d)' % len(runs), ' | '.join(cells))
    sys.exit(0)

import importlib
import json
import time

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap')
import ore_map_build as omb
importlib.reload(omb)
omb.load_layout()

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/ab/'
# what was saved: T3D of NS_SandPush as loaded from disk (spawn literals, sort modes, renderer materials)
_task = unreal.AssetExportTask()
_task.object = unreal.load_asset('/Game/DustAndSandStorm/SandStorm/Niagara/NS_SandPush')
_task.filename = OUT + 'NS_SandPush_after.t3d'
_task.automated, _task.replace_identical, _task.prompt = True, True, False
_task.exporter = unreal.ObjectExporterT3D()
print('t3d export', unreal.Exporter.run_asset_export_task(_task))
BEFORE_SYS = unreal.load_asset('/Game/RTS_Survival/SpecialEffects/Dust/SandStreak/Dev/NS_SandPush_BeforeMasked')
data = json.load(open(omb.FX_FILE))
streaks = [i for i in data['items'] if i[0] == 'SandStreak']
VIEWS = [('valley', streaks[1][2], streaks[1][3]), ('road', streaks[2][2], streaks[2][3]),
         ('bench', streaks[15][2], streaks[15][3])]
# A request may set AB_STATES / AB_PROFILE before this code (e.g. 'AB_STATES = ["AFTER"]' for a look-only rerun).
STATES = globals().get('AB_STATES', ['AFTER', 'BEFORE', 'OFF'])
PROFILE = globals().get('AB_PROFILE', True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.SystemLibrary.execute_console_command(omb.world(), 't.IdleWhenNotForeground 0')
les.editor_request_begin_play()

# Step list built up front: each entry is (kind, arg, wait seconds of game time before the next step).
STEPS = []
for state in STATES:
    STEPS.append(('state', state, 8.0))
    for view, x, y in VIEWS:
        STEPS.append(('move', (x, y), 5.0))
        STEPS.append(('console', 'viewmode lit', 0.5))
        STEPS.append(('shot', '%s_%s_lit' % (state, view), 1.5))
        if view == 'valley' and PROFILE:
            for k in range(3):
                STEPS.append(('profile', state, 2.5))
        if state != 'OFF':
            for k in range(6):
                STEPS.append(('frame', '%s_%s_f%d' % (state, view, k), 0.05))
            STEPS.append(('console', 'viewmode shadercomplexity', 1.0))
            STEPS.append(('shot', '%s_%s_complexity' % (state, view), 1.5))
            STEPS.append(('console', 'viewmode lit', 0.5))
S = {'i': 0, 'mark': None, 'n': 0, 't0': time.time(), 'ready': False}


def streak_comps(gw):
    out = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.NiagaraActor):
        if 'SandStreak' in actor.get_actor_label():
            out.append(actor.niagara_component)
    return out


def apply_state(gw, state):
    comps = streak_comps(gw)
    for comp in comps:
        if state == 'BEFORE':
            comp.set_asset(BEFORE_SYS, False)
            comp.activate(True)
        if state == 'OFF':
            comp.set_visibility(False, True)
            comp.deactivate()
    print('state', state, 'streak components', len(comps), 'asset', comps[0].get_asset().get_name() if comps else None)


def run_step(gw, kind, arg):
    if kind == 'state':
        apply_state(gw, arg)
    elif kind == 'move':
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        x, y = arg
        pawn.set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + 43.0), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    elif kind == 'console':
        unreal.SystemLibrary.execute_console_command(gw, arg)
    elif kind == 'shot':
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1600x900 filename=' + OUT + arg + '.png')
    elif kind == 'frame':
        # viewport-size capture (multiplier 1 keeps the view size, so the TSR history stays valid)
        unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1 filename=' + OUT + arg + '.png')
    elif kind == 'profile':
        unreal.log('SAND_PROFILE_BEGIN ' + arg)
        unreal.SystemLibrary.execute_console_command(gw, 'ProfileGPU')


def _defer():
    S['n'] += 1
    gw = ues.get_game_world()
    if gw is None:
        return time.time() - S['t0'] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and S['n'] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if not S['ready']:
        if game_t < 20.0:           # fog of war off, masked shaders compiled, effects built up
            return False
        for cmd in ('ShowFlag.PostProcessMaterial 0', 'rts.AmbientFx 3', 'r.ProfileGPU.ShowUI 0'):
            unreal.SystemLibrary.execute_console_command(gw, cmd)
        S['ready'], S['mark'] = True, game_t
        return False
    if S['i'] >= len(STEPS):
        les.editor_request_end_play()
        return True
    kind, arg, wait = STEPS[S['i']]
    if S.get('done_i') != S['i']:
        run_step(gw, kind, arg)
        S['done_i'], S['mark'] = S['i'], game_t
        return False
    if game_t - S['mark'] >= wait:
        S['i'] += 1
    return False
