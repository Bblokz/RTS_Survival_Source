"""Game-camera-like editor viewport pictures of Ger_Berlin for the detail pass (exec through berlin_editor_server).

Set before exec (optional): SHOT_TAG = "r0" (sub folder), SHOT_ONLY = ["reichstag_front", ...].
Writes Saved/Berlin/detail/shots/<tag>/<name>.png. The RTS camera looks west from high up, so every view puts the
camera east of its target, PITCH degrees down, HEIGHT cm above the target. Close views (prefix "close_") are lower.
"""
import math
import os

import unreal

SHOT_TAG = globals().get("SHOT_TAG", "r0")
OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/shots/%s/" % SHOT_TAG
os.makedirs(OUT, exist_ok=True)
PITCH, HEIGHT = globals().get("SHOT_PITCH", 62.0), globals().get("SHOT_HEIGHT", 4200.0)
# name, target x, y (+ optional height, yaw of the view: 180 = looking west, i.e. camera east of the target)
VIEWS = [
    ("reichstag_front", 3500, -15800), ("reichstag_east", 8000, -12000), ("reichstag_west", 8000, -20500),
    ("reichstag_north", 12500, -16000), ("koenigsplatz", 1500, -14000), ("gate", 0, -8300),
    ("forge_crater", -3000, -2500), ("zoo_tower", -17000, -42000), ("tiergarten_mid", -4000, -30000),
    ("tiergarten_north", 8000, -32000), ("hbf_nw", 18000, -33000), ("spree_bank", 18500, -10000),
    ("mitte_north", 15000, 5000), ("mitte_mid", 4000, 8000), ("alexanderplatz", 7000, 28000),
    ("potsdamer", -14000, -2000), ("south_lane_mid", -12000, 14000), ("rail_yard", -6000, 22000),
    ("friedrichshain_start", 14000, 43000), ("brewery", -9500, 35000), ("east_south", -20000, 40000),
    ("north_lane_mid", 20000, 17000), ("south_west", -22000, -25000), ("central_lane", 2000, 18000),
    ("rt_courtyard", 8100, -16000), ("rt_south_camp", 8200, -23500), ("rt_east_park", 6500, -6700),
]
SHOT_ONLY = globals().get("SHOT_ONLY")
JOBS = [v for v in VIEWS if not SHOT_ONLY or v[0] in SHOT_ONLY]
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_w = _ues.get_editor_world()
_s = {"i": 0, "n": 0}


def _z(x, y):
    ignore = []
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(_w, unreal.Vector(x, y, 40000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if "Landscape" in t[9].get_class().get_name():
            return t[4].z
        ignore.append(t[9])
    return 0.0


def _defer():
    if _s["i"] >= len(JOBS):
        print("shots", len(JOBS), OUT)
        return True
    v = JOBS[_s["i"]]
    name, x, y = v[0], v[1], v[2]
    if _s["n"] == 0:
        h = HEIGHT * (0.45 if name.startswith("close_") else 1.0)
        back = h / math.tan(math.radians(PITCH))
        gz = _z(x, y)
        cam = unreal.Vector(x, y + back, gz + h)
        _ues.set_level_viewport_camera_info(cam, unreal.Rotator(roll=0.0, pitch=-PITCH, yaw=-90.0))   # looking -Y (west); Rotator positional order is (roll, pitch, yaw)
    if _s["n"] == 70:
        unreal.AutomationLibrary.finish_loading_before_screenshot()
    if _s["n"] == 100:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + name + ".png")
    _s["n"] += 1
    if _s["n"] > 125:
        _s["n"] = 0
        _s["i"] += 1
    return False
