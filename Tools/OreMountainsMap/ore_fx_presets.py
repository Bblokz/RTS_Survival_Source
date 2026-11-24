"""Try Niagara presets in a play session on Ger_2ndOreMountains and screenshot each (throwaway editor, ore_cmd.py).

Define PRESETS before this body runs: a list of dicts
    {"name": str, "system": "/Game/...", "at": (x, y, dz), "cam": (x, y, yaw), "scale": 1.0, "yaw": 0.0, "warm": 9.0,
     "floats": {"Size": 2.0}, "colors": {"Color_A": (r, g, b, a)}, "vectors": {"Velocity": (x, y, z)}}
Each is spawned alone, given WARM seconds of game time, shot to Saved/OreMountains/fx/<PREFIX><name>.png (fog of war
off) and removed. apply_preset() is the same code the map build uses for placed effects.
"""
import os
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb

omb.load_layout()
FX_OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/fx/"
os.makedirs(FX_OUT, exist_ok=True)
PREFIX = globals().get("PREFIX", "p_")
SHOT = globals().get("SHOT", "1280x720")


def apply_preset(comp, preset):
    for key, value in preset.get("floats", {}).items():
        comp.set_niagara_variable_float(key, float(value))
    for key, value in preset.get("colors", {}).items():
        comp.set_niagara_variable_linear_color(key, unreal.LinearColor(*value))
    for key, value in preset.get("vectors", {}).items():
        comp.set_niagara_variable_vec3(key, unreal.Vector(*value))


unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 1, "i": 0, "comp": None}


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 120
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 1 and game_t > 8.0:
        unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
        _s["stage"] = 2
        return False
    if _s["stage"] == 2:
        if _s["i"] >= len(PRESETS):
            _les.editor_request_end_play()
            return True
        preset = PRESETS[_s["i"]]
        if _s["comp"] is None:
            cam = preset["cam"]
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(cam[0], cam[1], omb.ground_z(cam[0], cam[1]) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, cam[2]), False)
            x, y, dz = preset["at"]
            scale = preset.get("scale", 1.0)
            comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(
                gw, unreal.load_asset(preset["system"]), unreal.Vector(x, y, dz if preset.get("abs_z") else omb.ground_z(x, y) + dz),
                unreal.Rotator(0.0, 0.0, preset.get("yaw", 0.0)), unreal.Vector(scale, scale, scale), False, True)
            apply_preset(comp, preset)
            _s["comp"], _s["mark"], _s["shot"] = comp, game_t, False
            return False
        if not _s["shot"] and game_t - _s["mark"] > preset.get("warm", 9.0):
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot " + SHOT + " filename=" + FX_OUT + PREFIX + preset["name"] + ".png")
            _s["shot"], _s["mark"] = True, game_t
            return False
        if _s["shot"] and game_t - _s["mark"] > 0.8:
            _s["comp"].destroy_component(_s["comp"])
            _s["comp"] = None
            _s["i"] += 1
    return False
