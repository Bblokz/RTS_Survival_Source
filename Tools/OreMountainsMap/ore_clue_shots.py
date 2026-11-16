"""In-game screenshots of the Act 1 clue spots (fossil roots A, miners' shelter B), sent through ore_cmd.py.

Starts a play session in the throwaway editor, turns the fog-of-war post process off, moves the camera pawn to each
spot and writes HighResShot PNGs to Saved/OreMountains/clues/<tag>_<name>.png. Set CLUE_SHOT_TAG / CLUE_SHOT_SPOTS in
the persistent namespace before sending this file to change them (both are reset after the run).
"""
import importlib
import sys
import time

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb

importlib.reload(omb)
omb.load_layout()

OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/clues/"
# (name, x, y, camera yaw): pawn position is the camera focus point
DEFAULT_SPOTS = [
    ("rootsA1", -7102.7, 3953.0, 0.0), ("rootsA2", -7826.9, -6666.5, 0.0), ("rootsA3", 5513.4, 10519.0, 180.0),
    ("rootsA4", 5794.2, -9916.0, 180.0), ("rootsA5", -7035.0, -24790.0, 0.0),
    ("shelterB", -22600.0, 12650.0, 180.0), ("shelterB_wide", -20500.0, 12650.0, 180.0),
]
_tag = globals().pop("CLUE_SHOT_TAG", "now")
_spots = globals().pop("CLUE_SHOT_SPOTS", DEFAULT_SPOTS)

unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 1, "i": 0, "moved": False, "shot": False}


def _defer():
    _s["n"] += 1
    gw = ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 1:
        if game_t > 10.0:
            unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
            _s["stage"], _s["mark"] = 2, game_t
        return False
    if _s["i"] >= len(_spots):
        les.editor_request_end_play()
        return True
    name, x, y, yaw = _spots[_s["i"]]
    if not _s["moved"]:
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        pawn.set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + 43.0), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
        _s["moved"], _s["mark"] = True, game_t
        return False
    if not _s["shot"] and game_t - _s["mark"] > 3.0:
        unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1600x900 filename=%s%s_%s.png" % (OUT, _tag, name))
        _s["shot"], _s["mark"] = True, game_t
        return False
    if _s["shot"] and game_t - _s["mark"] > 1.5:
        print("shot", _tag, name)
        _s["i"] += 1
        _s["moved"], _s["shot"] = False, False
    return False
