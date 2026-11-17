"""Places the rubble decals of rubble_placements.json on Ger_Berlin and saves the map (throwaway editor only).

Launch with rubble_headless_run.sh on /Game/RTS_Survival/Maps/GerMissions/Ger_Berlin while no other editor has the map
loaded. Re-runnable: it removes every actor labelled RUBDECAL_ first and touches nothing else (step_decals' DECAL_
actors, ruins, landscape stay as they are).

Per placement: z = the landscape under the centre (single traces that skip every non-landscape actor); placements with
a `check` trace horizontally 100 cm (then 45 cm) above that ground along each bearing and are dropped unless a
ruin/landmark wall is hit within the given distance (fans must touch their wall, corner piles both walls). The decal is
a plain DecalActor: rotation (roll 0, pitch -90, yaw), size (180, 200 s, 200 s), DecalColor (1, g, 0 if mirrored
else 1, a), sort order 3 (above the older ground decals), folder Berlin/Decals/Rubble/<NN_cell name>.

Save: like Saved/Berlin/cmd_r3_nav.py, it waits at least 240 s and until the navmesh is no longer being built (the
full editor rebuilds it on load; saving earlier stores an empty navmesh), runs berlin_nav_check and saves the map
package only when that reports at most one problem. Log lines start with RUBAPPLY; report: rubble_apply_report.json.
Environment: RUBBLE_APPLY_DRY=1 places and reports without saving; RUBBLE_APPLY_SHOTS=<dir> takes before/after
pictures (critic views + close-ups) while the navmesh builds, decals hidden for the before_ ones (shown again after).
"""
import json
import math
import os
import sys
import time
import traceback

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap')
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/'
FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/'
MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin'
PREFIX = 'RUBDECAL_'
WALL_CLASSES = ('BerlinRuin', 'ReichstagRuin', 'BrandenburgGate')
S = {'t': 0.0, 'phase': 'wait_load', 'report': {}}
DRY = os.environ.get('RUBBLE_APPLY_DRY') == '1'
SHOTS_DIR = os.environ.get('RUBBLE_APPLY_SHOTS')
VIEWS = [('01_central_lane_east', (5200, 36000, 4500), (-48, 0)), ('02_mitte_wallcore', (5000, 16000, 4200), (-48, 0)),
         ('10_hero_ruin', (-24963.3, -29272.7, 4300), (-48, 0)), ('11_side_street', (-1000, -28000, 4200), (-48, 90)),
         ('12_alexanderplatz', (6800, 26500, 4800), (-48, 0))]


def log(m):
    unreal.log('RUBAPPLY ' + str(m))


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def landscape_z(w, x, y):
    """Landscape height under (x, y). A trace by channel stops at the first blocking hit (heaps, ruins, wrecks), so
    every non-landscape actor hit is added to the ignore list until the landscape answers."""
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        if not t[0] or t[9] is None:
            return None
        if 'Landscape' in t[9].get_class().get_name():
            return t[4].z
        ignore.append(t[9])
    return None


def wall_hit(w, x, y, z, bearing, dist):
    b = math.radians(bearing)
    for dz in (100.0, 45.0):
        start = unreal.Vector(x, y, z + dz)
        end = unreal.Vector(x + math.cos(b) * dist, y + math.sin(b) * dist, z + dz)
        hit = unreal.SystemLibrary.line_trace_single(w, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            continue
        t = hit.to_tuple()
        if t[0] and t[9] is not None and any(k in t[9].get_class().get_name() for k in WALL_CLASSES):
            return True
    return False


def place():
    w = world()
    if not w.get_path_name().startswith(MAP):
        raise RuntimeError('wrong map ' + w.get_path_name())
    data = json.load(open(ROOT + 'rubble_placements.json'))
    names = data['names']
    removed = 0
    for a in actors().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            actors().destroy_actor(a)
            removed += 1
    mats = [unreal.load_asset(FOLDER + 'MI_BerlinRubbleDecal_%02d_%s' % (i, n)) for i, n in enumerate(names)]
    if any(m is None for m in mats):
        raise RuntimeError('missing decal material')
    import berlin_map_build as bmb
    bmb.load_layout()
    placed, no_ground, no_wall, worst = 0, 0, 0, 0.0
    per_cell = [0] * 16
    drops = {}
    S['placed'] = []
    for p in data['placements']:
        z = landscape_z(w, p['x'], p['y'])
        if z is None:
            no_ground += 1
            drops['no_ground ' + p['why']] = drops.get('no_ground ' + p['why'], 0) + 1
            continue
        if placed % 25 == 0:
            worst = max(worst, abs(z - bmb.ground_z(p['x'], p['y'])))
        chk = p.get('check')
        if chk:
            checks = chk if isinstance(chk[0], list) else [chk]
            if not all(wall_hit(w, p['x'], p['y'], z, c[0], c[1]) for c in checks):
                no_wall += 1
                drops['no_wall ' + p['why']] = drops.get('no_wall ' + p['why'], 0) + 1
                continue
        a = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(p['x'], p['y'], z + 30.0),
                                            unreal.Rotator(0.0, -90.0, p['yaw']))
        c = a.decal
        c.set_decal_material(mats[p['cell']])
        h = 200.0 * p['s']
        c.set_editor_property('decal_size', unreal.Vector(180.0, h, h))
        c.set_editor_property('decal_color', unreal.LinearColor(1.0, p['g'], 0.0 if p['mirror'] else 1.0, p['a']))
        c.set_editor_property('sort_order', 3)
        c.set_editor_property('fade_screen_size', 0.001)
        a.set_actor_label('%s%02d_%s_%04d' % (PREFIX, p['cell'], names[p['cell']], placed))
        a.set_folder_path('Berlin/Decals/Rubble/%02d_%s' % (p['cell'], names[p['cell']]))
        per_cell[p['cell']] += 1
        placed += 1
        S['placed'].append(dict(p, z=z))
    S['report'].update({'candidates': len(data['placements']), 'placed': placed, 'removed': removed,
                        'no_ground': no_ground, 'no_wall': no_wall, 'drops_by_rule': drops,
                        'ground_vs_heightfile_max_cm': round(worst, 1), 'per_cell': dict(zip(names, per_cell))})
    log('placed %d (removed %d, no ground %d, no wall %d) %s' % (placed, removed, no_ground, no_wall, drops))


def set_hidden(hidden):
    for a in actors().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            a.set_is_temporarily_hidden_in_editor(hidden)


def plan_shots():
    """Critic views plus close-ups (a mirrored corner pile, a facade fan, the Reichstag front); each view is taken
    with the rubble decals shown (after_) and hidden (before_)."""
    views = list(VIEWS)
    pl = S['placed']
    corner = [p for p in pl if p['cell'] == 3 and p['mirror']] or [p for p in pl if p['cell'] == 3]
    fan = [p for p in pl if p['why'] == 'facade_fan']
    for tag, lst in (('close_corner', corner), ('close_fan', fan)):
        if lst:
            p = lst[len(lst) // 2]
            views.append((tag, (p['x'] - 1400, p['y'], p['z'] + 1500), (-47, 0)))
            S['report'][tag] = p
    views.append(('close_reichstag', (8112 - 9000, -15994, 5200), (-40, 0)))
    os.makedirs(SHOTS_DIR, exist_ok=True)
    # a fresh editor builds textures asynchronously and samples the engine's black placeholder (black, alpha 1)
    # meanwhile: the decals draw as dark opaque squares. Force every pending asset build to finish first.
    # Shaders compile on demand, so a plain FinishAll runs before the decal shader is even submitted:
    # finish_loading_before_screenshot submits the remaining jobs of every loaded material, blocks until all
    # asset/shader compilation is done and streams all mips.
    t = time.time()
    # pictures only: the two atlas textures get all mips resident in memory (never saved: only the map package is
    # saved). Without it a fresh editor on this map kept sampling the black placeholder for many minutes.
    for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS'):
        unreal.load_asset(FOLDER + n).set_editor_property('never_stream', True)
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    S['report']['finish_loading_s'] = round(time.time() - t)
    log('finish_loading_before_screenshot took %.0f s' % (time.time() - t))
    # pairs back to back (the sun's cloud light function drifts): after_ with the decals, then before_ without
    # the first frame after a camera cut is not settled (streaming, temporal history: existing puddle decals drew
    # black there), so every view gets a throwaway warm-up shot first
    S['views'] = [(pre + v[0],) + v[1:] for v in views for pre in ('warm_', 'after_', 'before_')]
    S['shot_i'], S['cam'] = 0, None
    S['mark'] = time.time()
    S['log_off'] = os.path.getsize(LOGFILE[0]) if LOGFILE else 0
    S['last_busy'] = time.time()
    S['ready'] = False


LOGFILE = [a.split('=', 1)[1] for a in unreal.SystemLibrary.get_command_line().split() if a.lower().startswith('-abslog=')]


def scene_ready():
    """A fresh editor compiles every material of the map (minutes on this machine; meanwhile decals draw black or
    not at all): wait until this editor's own log reports an empty shader queue and stays quiet for 20 s, at least
    180 s after placing, plus the atlas at full size."""
    if S['ready']:
        return True
    if LOGFILE:
        with open(LOGFILE[0], 'rb') as f:
            f.seek(S['log_off'])
            new = f.read()
        S['log_off'] += len(new)
        for line in new.decode('utf-8', 'ignore').splitlines():
            if 'Shaders left to compile' in line:
                S['left'] = line.rsplit(' ', 1)[-1].strip()
                S['last_busy'] = time.time()
    el = time.time() - S['t0']
    sizes = [unreal.load_asset(FOLDER + n).blueprint_get_size_x() for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS')]
    quiet = S.get('left', '0') == '0' and time.time() - S['last_busy'] > 20.0
    if (el > 90.0 and quiet and min(sizes) >= 2048) or el > 720.0:
        S['ready'] = True
        S['report']['shots_after_s'] = round(el)
        log('scene ready after %.0f s (shaders left %s, atlas %s)' % (el, S.get('left'), sizes))
    return S['ready']


def probe_diff(on_png, off_png):
    """Mean colour difference in the middle third of two pictures (system python, numpy + cv2)."""
    import subprocess
    code = ('import cv2,numpy as np,sys;a=cv2.imread(sys.argv[1]).astype(float);b=cv2.imread(sys.argv[2]).astype(float);'
            'h,w=a.shape[:2];c=(slice(h//3,2*h//3),slice(w//3,2*w//3));'
            'print(np.abs(a[c]-b[c]).sum(-1).mean(), a[c].mean()/max(b[c].mean(),1.0))')
    out = subprocess.run(['python', '-c', code, on_png, off_png], capture_output=True, text=True, timeout=60)
    try:
        d, ratio = [float(v) for v in out.stdout.split()]
        return d, ratio
    except ValueError:
        return -1.0, 0.0


def probe_tick():
    """Proves the decals draw before any comparison picture: one placed decal on open ground, top-down, shown and
    hidden; the shader compile of a fresh editor can take minutes, until then the decals draw nothing."""
    pr = S.setdefault('probe', {'step': 0, 'tries': 0})
    if time.time() - S['mark'] < 5.0:
        return False
    if S['cam'] is not None:
        actors().destroy_actor(S['cam'])
        S['cam'] = None
    if 'spot' not in pr:
        cands = [p for p in S['placed'] if p['cell'] in (1, 11, 12, 15) and p['why'].startswith('street')]
        cands.sort(key=lambda p: -p['s'])
        pr['spot'] = cands[0] if cands else S['placed'][0]
    p = pr['spot']
    if pr['step'] in (0, 1):
        tag = 'probe_on' if pr['step'] == 0 else 'probe_off'
        set_hidden(pr['step'] == 1)
        pr['step'] += 1
        S['mark'] = time.time()
        cam = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(p['x'], p['y'], p['z'] + 1200.0),
                                              unreal.Rotator(0, -90, 0))
        cam.camera_component.set_field_of_view(60)
        cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
        S['cam'] = cam
        unreal.AutomationLibrary.take_high_res_screenshot(800, 450, '%s/%s.png' % (SHOTS_DIR, tag), cam)
        return False
    set_hidden(False)
    # until the decal's shaders and textures are built in this editor it draws as a dark opaque square (measured on
    # RTS_ToCopySmall: dark at 90 s, right at 300 s): it must change the picture AND not darken it wholesale
    d, ratio = probe_diff('%s/probe_on.png' % SHOTS_DIR, '%s/probe_off.png' % SHOTS_DIR)
    pr['tries'] += 1
    el = time.time() - S['t0']
    log('probe %d at %.0f s: diff %.1f brightness ratio %.2f at %s' % (pr['tries'], el, d, ratio,
                                                                       (p['x'], p['y'], NAMES_SHORT(p))))
    S['report'].setdefault('probe', []).append([round(el), round(d, 2), round(ratio, 3)])
    ok = d > 8.0 and ratio > 0.9
    if ok or el > 1200.0:
        S['visible'] = ok
        return True
    pr['step'] = 0
    S['mark'] = time.time() + 20.0
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    return False


def NAMES_SHORT(p):
    return '%02d %s' % (p['cell'], p['why'])


def shots_tick():
    if not scene_ready():
        return
    if not S.get('probed'):
        if not probe_tick():
            return
        S['probed'] = True
        S['mark'] = time.time()
    if time.time() - S['mark'] < 12.0:
        return
    if S['cam'] is not None:
        actors().destroy_actor(S['cam'])
        S['cam'] = None
    if S['shot_i'] >= len(S['views']):
        set_hidden(False)
        S['phase'] = 'wait_nav'
        return
    name, loc, (pitch, yaw) = S['views'][S['shot_i']]
    set_hidden(name.startswith('before_'))
    S['shot_i'] += 1
    S['mark'] = time.time()          # set before the screenshot: it pumps Slate and the tick re-enters
    cam = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*loc), unreal.Rotator(0, pitch, yaw))
    cam.camera_component.set_field_of_view(60)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
    S['cam'] = cam
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, '%s/%s.png' % (SHOTS_DIR, name), cam)
    log('shot ' + name)


def save_step():
    el = time.time() - S['t0']
    if el < 240.0:
        return
    if S['ns'].is_navigation_being_built(world()) and el < 900.0:
        return
    S['phase'] = 'saving'
    S['report']['nav_wait_s'] = round(el)
    set_hidden(False)
    import berlin_nav_check
    problems = berlin_nav_check.run()
    S['report']['nav_problems'] = None if problems is None else list(problems)
    log('nav problems %s' % S['report']['nav_problems'])
    if DRY:
        log('dry run, not saved')
    elif problems is not None and len(problems) <= 1:
        S['phase'] = 'save_retry'
        S['save_tries'] = 0
        S['next_try'] = 0.0
        return
    else:
        log('NOT saved: nav problems')
    finish()


def save_retry():
    """Other sessions' -game runs open the umap while they load it (the save then fails with 'Error moving file'):
    retry every 30 s for up to 10 minutes."""
    if time.time() < S['next_try']:
        return
    S['save_tries'] += 1
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([world().get_outermost()], False)
    log('save try %d: %s' % (S['save_tries'], ok))
    if ok or S['save_tries'] >= 20:
        S['report']['saved'] = bool(ok)
        S['report']['save_tries'] = S['save_tries']
        finish()
        return
    S['next_try'] = time.time() + 30.0


def finish():
    S['report'].pop('placed_list', None)
    json.dump(S['report'], open(ROOT + 'rubble_apply_report.json', 'w'), indent=1, default=str)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def _tick():
    if S['phase'] == 'wait_load' and S['t'] > 20.0:
        S['phase'] = 'placing'
        place()
        S['ns'] = unreal.NavigationSystemV1.get_navigation_system(world())
        S['t0'] = time.time()
        S['phase'] = 'shots' if SHOTS_DIR else 'wait_nav'
        if SHOTS_DIR:
            plan_shots()
    elif S['phase'] == 'shots':
        shots_tick()
    elif S['phase'] == 'wait_nav':
        save_step()
    elif S['phase'] == 'save_retry':
        save_retry()


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        _tick()
    except Exception:  # noqa: BLE001
        S['report']['error'] = traceback.format_exc()
        log('ERROR ' + traceback.format_exc())
        finish()
    finally:
        S['busy'] = False
    if S['t'] > 3000.0 and S['phase'] != 'done':
        S['report']['error'] = 'timeout in phase ' + S['phase']
        finish()


unreal.register_slate_post_tick_callback(tick)
log('loaded, dry=%s shots=%s' % (DRY, SHOTS_DIR))
