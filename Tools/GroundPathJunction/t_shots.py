"""Viewport screenshots of the test scene (t_scene.py). Set SHOT_TAG and optionally SHOT_VIEWS in the namespace first."""
import math
import os
import time

OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/junction/"
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = ues.get_editor_world()
CX, CY = globals().get("JT", {}).get("centre", (2000.0, 2000.0))
GZ = 143.7
# name: (distance from the crossing, camera pitch, yaw of the view direction)
VIEWS = globals().get("SHOT_VIEWS") or {
    "top": (2600.0, -89.0, 0.0), "rts": (3000.0, -58.0, 20.0), "low": (1700.0, -11.0, 35.0), "close": (1100.0, -89.0, 0.0),
}
TAG = globals().get("SHOT_TAG", "v00")
les.editor_set_game_view(True)
for cmd in ("r.TextureStreaming 0", "t.IdleWhenNotForeground 0", "r.ScreenPercentage 100"):
    unreal.SystemLibrary.execute_console_command(world, cmd)
_q = {"views": list(VIEWS.items()), "stage": "move", "mark": time.time(), "files": []}


def _defer():
    now = time.time()
    if not _q["views"]:
        if now - _q["mark"] < 2.0:
            return False
        unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 1")
        print("shots", _q["files"])
        return True
    name, (dist, pitch, yaw) = _q["views"][0]
    if _q["stage"] == "move":
        p, y = math.radians(pitch), math.radians(yaw)
        direction = (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))
        loc = unreal.Vector(CX - direction[0] * dist, CY - direction[1] * dist, GZ + 20.0 - direction[2] * dist)
        ues.set_level_viewport_camera_info(loc, unreal.Rotator(0.0, pitch, yaw))
        les.editor_invalidate_viewports()
        _q["stage"], _q["mark"], _q["n"] = "settle", now, 0
        return False
    if _q["stage"] == "settle":
        # the editor viewport is not realtime: keep redrawing so temporal AA resolves the dither
        les.editor_invalidate_viewports()
        _q["n"] += 1
        if now - _q["mark"] > 4.0 and _q["n"] > 60:
            path = "%s%s_%s.png" % (OUT, TAG, name)
            if os.path.exists(path):
                os.remove(path)
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, path)
            _q["files"].append(path)
            _q["stage"], _q["mark"] = "shot", now
        return False
    if _q["stage"] == "shot":
        les.editor_invalidate_viewports()
        if now - _q["mark"] > 3.0:
            _q["views"].pop(0)
            _q["stage"] = "move"
    return False
