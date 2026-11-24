"""Low oblique editor-viewport pictures at the gap spots (exec through gaps_editor_server; set SHOT_TAG first).
Writes Saved/Berlin/gaps/shots_<tag>/<name>_<view>.png."""
import math
import os

SHOT_TAG = globals().get('SHOT_TAG', 'before')
SHOTS = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/gaps/shots_%s/' % SHOT_TAG
os.makedirs(SHOTS, exist_ok=True)
SPOTS = globals().get('SHOT_SPOTS') or [
    ('gate', 73, -8263, 3600), ('gate_w', 73, -9900, 1500), ('gate_e', 73, -6700, 1500),
    ('slabA_south', 6454, -44809, 1700), ('stub_mid', -17452, 1816, 1700), ('slabB_east', 19169, 20714, 1700),
    ('heap_120', -342, -45184, 1100), ('corner_ten', -27033, 12074, 2200), ('court_2', 8893, 52267, 2400),
    ('slabB_c', -2278, 34028, 1700), ('slabA_n', 20302, -6186, 1700), ('power_mast', 15300, 39700, 1800),
]
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_jobs = []
for name, x, y, dist in SPOTS:
    for view, ang in (('a', 200.0), ('b', 70.0), ('c', -50.0)):
        _jobs.append((name + '_' + view, x, y, dist, ang))
_s = {'i': 0, 'n': 0}


def _z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 60000), unreal.Vector(x, y, -30000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit is not None and hit.to_tuple()[0] else 0.0


def _defer():
    if _s['i'] >= len(_jobs):
        print('shots', len(_jobs), SHOTS)
        return True
    name, x, y, dist, ang = _jobs[_s['i']]
    if _s['n'] == 0:
        a = math.radians(ang)
        cx, cy = x + math.cos(a) * dist, y + math.sin(a) * dist
        cz = max(_z(cx, cy), _z(x, y)) + dist * 0.22
        look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(cx, cy, cz), unreal.Vector(x, y, _z(x, y) + 60))
        _ues.set_level_viewport_camera_info(unreal.Vector(cx, cy, cz), look)
    if _s['n'] == 25:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, SHOTS + name + '.png')
    _s['n'] += 1
    if _s['n'] > 45:
        _s['n'] = 0
        _s['i'] += 1
    return False
