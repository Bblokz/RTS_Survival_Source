"""Survey of Niagara systems in a play session on Ger_2ndOreMountains (throwaway editor, sent with ore_cmd.py).

Set SYSTEMS (list of /Game/... paths), SPOT (x, y) for the camera, optional SPAWN (x, y, dz), PREFIX and optionally EXPORT_T3D before the file body runs, e.g.
    SYSTEMS = [...]; SPOT = (-17000.0, -9000.0); PREFIX = "fx_"; exec(open(".../ore_fx_survey.py").read())
For each system: spawns it at SPOT in the running game, waits WARM seconds of game time, writes
Saved/OreMountains/fx/<PREFIX><name>.png (fog of war off, default RTS camera), then removes it.
With EXPORT_T3D it first writes Saved/OreMountains/fx/t3d/<name>.t3d for offline reading of emitters and values.
"""
import os
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb

omb.load_layout()
FX_OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/fx/"
os.makedirs(FX_OUT + "t3d", exist_ok=True)
WARM = globals().get("WARM", 5.0)
YAW = globals().get("YAW", 0.0)
SCALE = globals().get("SCALE", 1.0)
SPAWN = globals().get("SPAWN") or (SPOT[0], SPOT[1], 10.0)     # where the effect goes: x, y, height above ground
SHOT = globals().get("SHOT", "960x540")
if globals().get("EXPORT_T3D"):
    for path in SYSTEMS:
        asset = unreal.load_asset(path)
        if asset is None:
            print("MISSING", path)
            continue
        task = unreal.AssetExportTask()
        task.object = asset
        task.exporter = unreal.ObjectExporterT3D()
        task.filename = FX_OUT + "t3d/" + path.split("/")[-1] + ".t3d"
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        unreal.Exporter.run_asset_export_task(task)
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
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        pawn.set_actor_location(unreal.Vector(SPOT[0], SPOT[1], omb.ground_z(SPOT[0], SPOT[1]) + 43.0), False, False)
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, YAW), False)
        _s["stage"], _s["mark"] = 2, game_t
        return False
    if _s["stage"] == 2 and game_t - _s["mark"] > 2.5:
        _s["stage"] = 3
    if _s["stage"] == 3:
        if _s["i"] >= len(SYSTEMS):
            _les.editor_request_end_play()
            return True
        path = SYSTEMS[_s["i"]]
        if _s["comp"] is None:
            system = unreal.load_asset(path)
            loc = unreal.Vector(SPAWN[0], SPAWN[1], omb.ground_z(SPAWN[0], SPAWN[1]) + SPAWN[2])
            _s["comp"] = unreal.NiagaraFunctionLibrary.spawn_system_at_location(gw, system, loc, unreal.Rotator(0.0, 0.0, 0.0),
                                                                                 unreal.Vector(SCALE, SCALE, SCALE), False, True)
            _s["mark"], _s["shot"] = game_t, False
            return False
        if not _s["shot"] and game_t - _s["mark"] > WARM:
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot " + SHOT + " filename=" + FX_OUT + PREFIX + path.split("/")[-1] + ".png")
            _s["shot"], _s["mark"] = True, game_t
            return False
        if _s["shot"] and game_t - _s["mark"] > 0.8:
            _s["comp"].destroy_component(_s["comp"])
            _s["comp"] = None
            _s["i"] += 1
    return False
