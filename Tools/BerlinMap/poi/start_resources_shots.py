# Editor-viewport pictures of the start-base resource fields (sent through berlin_editor_server; nothing saved).
# Writes Saved/Berlin/start_resources/<name>.png.
import math
import os

import unreal

SHOTS = globals().get("SHOT_DIR") or "D:/UE5Projects/RTS_Survival/Saved/Berlin/start_resources/"
os.makedirs(SHOTS, exist_ok=True)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_w = _ues.get_editor_world()
# (name, camera xyz, look-at xyz)
JOBS = globals().get("SHOT_JOBS") or [
    ("game_view_west", (15200, 52500, 5200), (15200, 45800, 0)),
    ("top_crater", (17300, 47300, 5200), (17300, 47301, 0)),
    ("top_depot", (12750, 48100, 5200), (12750, 48101, 0)),
    ("low_crater", (14800, 49200, 700), (17300, 47300, 150)),
    ("low_depot", (12700, 45000, 600), (12700, 48300, 120)),
    ("overview_tower", (8000, 55000, 9000), (15500, 45000, 0)),
]
_s = {"i": 0, "n": 0}


def _defer():
    if _s["i"] >= len(JOBS):
        print("shots", len(JOBS), SHOTS)
        return True
    name, cam, tgt = JOBS[_s["i"]]
    if _s["n"] == 0:
        look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*cam), unreal.Vector(*tgt))
        _ues.set_level_viewport_camera_info(unreal.Vector(*cam), look)
    if _s["n"] == 40:
        unreal.AutomationLibrary.finish_loading_before_screenshot()
    if _s["n"] == 60:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, SHOTS + name + ".png")
    _s["n"] += 1
    if _s["n"] > 80:
        _s["n"] = 0
        _s["i"] += 1
    return False
