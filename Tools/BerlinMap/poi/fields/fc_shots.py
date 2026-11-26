# Viewport pictures of the four field craters after the build (same cameras as the "today" game_view_west shots of
# Saved/Berlin/extra_radixite). Writes Saved/BerlinCrater/shots/fields/<k>_{game_view_west,top}.png. Nothing saved.
import json
import os

import unreal

_OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/shots/fields/"
os.makedirs(_OUT, exist_ok=True)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_w = _ues.get_editor_world()


def _gz(x, y):
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(_w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return 0.0


_jobs = []
for _f in json.load(open("D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/sites/fields.json")):
    if _f["key"] not in ("E1", "E2", "E5", "E6"):
        continue
    xs = [q["x"] for q in _f["pieces"]]
    ys = [q["y"] for q in _f["pieces"]]
    fx, fy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    gz = _gz(fx, fy)
    _jobs += [("%s_game_view_west" % _f["key"], (fx, fy + 4200, gz + 3600), (fx, fy - 200, gz)),
              ("%s_top" % _f["key"], (fx, fy, gz + 4200), (fx, fy + 1, gz))]
_s = {"i": 0, "n": 0}


def _shots():
    if _s["i"] >= len(_jobs):
        print("shots", len(_jobs), _OUT)
        return True
    name, cam, tgt = _jobs[_s["i"]]
    if _s["n"] == 0:
        look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*cam), unreal.Vector(*tgt))
        _ues.set_level_viewport_camera_info(unreal.Vector(*cam), look)
    if _s["n"] == 40:
        unreal.AutomationLibrary.finish_loading_before_screenshot()
    if _s["n"] == 70:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, _OUT + name + ".png")
    _s["n"] += 1
    if _s["n"] > 100:
        _s["n"] = 0
        _s["i"] += 1
    return False


_defer = _shots
