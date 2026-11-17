"""Before/after shots of the dead-pine bark in a throwaway editor on Ger_OderWarthe (nothing is saved).

    UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/OderWartheMap/assets/ow_bark_shots.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
Env OW_BARK_SHOT_TAG (default 'ab') names the folder Saved/OderWarthe/shots/bark_<tag>/. Env OW_BARK_SKIP_BEFORE=1
skips the before pass. The after pass puts M_OW_PineBark (or MI_OW_PineBark when OW_BARK_AFTER=mi) on every
SM_OW_DeadPine component in memory. Quits the editor when done.
"""
import json
import math
import os
import time

import unreal

TAG = os.environ.get('OW_BARK_SHOT_TAG', 'ab')
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/shots/bark_%s/' % TAG
os.makedirs(OUT, exist_ok=True)
AFTER = os.environ.get('OW_BARK_AFTER', 'master')
MAT = ('/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/M_OW_PineBark' if AFTER == 'master'
       else '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/MI_OW_PineBark')
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
S = {'t0': time.time(), 'stage': 'warm', 'i': 0, 'mark': time.time(), 'shots': [], 'pass': 'before'}
LOG = []


def log(*a):
    line = ' '.join(str(x) for x in a)
    LOG.append(line)
    unreal.log_warning('OWSHOT ' + line)
    with open(OUT + 'log.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(LOG))


def world():
    return _ues.get_editor_world()


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world(), c)


def pines():
    out = []
    for a in _eas.get_all_level_actors():
        if 'OW_DeadPine' in a.get_class().get_name():
            out.append(a)
    return out


def ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return 0.0
    return hit.to_tuple()[4].z


def spawn_pines():
    """Unit-test map: spawn a small stand (01 in front, others behind) round the origin."""
    base = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Blueprints/BP_OW_DeadPine_'
    for key, x, y in (('01', 0, 0), ('03', 700, -450), ('05', 900, 500), ('02', 1500, -100), ('04', 400, 900)):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(base + key)
        a = _eas.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(x, y) - 8.0), unreal.Rotator(0, 0, x * 0.1))
        log('spawned', key, a.get_name() if a else None)


def pick():
    ps = pines()
    if not ps:
        spawn_pines()
        ps = pines()
    log('pines', len(ps))
    # a tall 01 nearest the map centre of the pines, so the shots are reproducible
    tall = [p for p in ps if 'DeadPine_01' in p.get_class().get_name()] or ps
    c = unreal.Vector(0, 0, 0)
    for p in tall:
        c += p.get_actor_location()
    c /= max(1, len(tall))
    tall.sort(key=lambda p: (p.get_actor_location() - c).length())
    return tall[0]


def look(cam, tgt):
    d = tgt - cam
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    return unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def spots(tree):
    p = tree.get_actor_location()
    yaw = math.radians(35.0)
    fwd = unreal.Vector(math.cos(yaw), math.sin(yaw), 0)
    side = unreal.Vector(-math.sin(yaw), math.cos(yaw), 0)
    return [
        ('crown', p - fwd * 380 + side * 140 + unreal.Vector(0, 0, 1700), p + side * 120 + unreal.Vector(0, 0, 1480)),
        ('branch', p - fwd * 160 + side * 260 + unreal.Vector(0, 0, 1560), p + side * 150 + unreal.Vector(0, 0, 1500)),
        ('trunk', p - fwd * 260 + side * 90 + unreal.Vector(0, 0, 260), p + unreal.Vector(0, 0, 200)),
        ('bark', p + fwd * 230 - side * 80 + unreal.Vector(0, 0, 560), p + unreal.Vector(0, 0, 520)),
        ('mid', p - fwd * 1300 + side * 400 + unreal.Vector(0, 0, 900), p + unreal.Vector(0, 0, 800)),
        ('rts', p - fwd * 2600 + unreal.Vector(0, 0, 3200), p + unreal.Vector(0, 0, 300)),
    ]


def swap():
    m = unreal.load_asset(MAT)
    n = 0
    for a in pines():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property('static_mesh')
            if sm and 'SM_OW_DeadPine' in sm.get_name():
                for k in range(c.get_num_materials()):
                    c.set_material(k, m)
                n += 1
    log('swapped', n, 'components to', MAT)


def tick(dt):
    now = time.time()
    try:
        if S['stage'] == 'warm':
            if now - S['mark'] < 45:
                return
            _les.editor_set_game_view(True)
            cmd('r.TextureStreaming 0')
            cmd('t.IdleWhenNotForeground 0')
            S['tree'] = pick()
            S['spots'] = spots(S['tree'])
            log('tree', S['tree'].get_name(), S['tree'].get_actor_location())
            if os.environ.get('OW_BARK_SKIP_BEFORE') == '1':
                swap()
                S['pass'] = 'after'
            S['stage'], S['mark'], S['i'] = 'move', now, 0
            return
        if S['stage'] == 'move':
            if now - S['mark'] < (60 if S['pass'] == 'after' and S['i'] == 0 else 4):
                return
            name, cam, tgt = S['spots'][S['i']]
            _ues.set_level_viewport_camera_info(cam, look(cam, tgt))
            S['stage'], S['mark'] = 'shoot', now
            return
        if S['stage'] == 'shoot':
            if now - S['mark'] < 4:
                return
            name = S['spots'][S['i']][0]
            f = OUT + '%s_%s.png' % (S['pass'], name)
            cmd('HighResShot 1600x900 filename=' + f)
            S['shots'].append(f)
            log('shot', f)
            S['i'] += 1
            S['stage'], S['mark'] = 'move', now
            if S['i'] >= len(S['spots']):
                if S['pass'] == 'before':
                    swap()
                    S['pass'], S['i'] = 'after', 0
                else:
                    S['stage'] = 'quit'
            return
        if S['stage'] == 'quit' and now - S['mark'] > 6:
            log('DONE')
            unreal.unregister_slate_post_tick_callback(S['h'])
            unreal.SystemLibrary.quit_editor()
    except Exception as e:  # noqa: BLE001
        log('ERROR', repr(e))
        unreal.unregister_slate_post_tick_callback(S['h'])
        unreal.SystemLibrary.quit_editor()


S['h'] = unreal.register_slate_post_tick_callback(tick)
log('registered', TAG, AFTER)
