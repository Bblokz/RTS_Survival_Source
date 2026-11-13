"""Fixed set of in-game landscape views of Ger_2ndOreMountains for the landscape critic rounds.

Set TAG (e.g. "r0") before this body runs (ore_cmd.py). Optional REBUILD = "paint" (landscape + fit only: the paint
or MI changed, heights did not) or "full" (every build step: heights changed). Writes
Saved/OreMountains/critic/<TAG>/<view>.png (play session, fog of war off, default RTS camera) and an editor top-down
overview top.png. Saves the map after a rebuild when the nav check passes.
"""
import importlib
import os
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb
import ore_nav_check

importlib.reload(omb)
importlib.reload(ore_nav_check)
omb.load_layout()
TAG = globals().get("TAG", "r0")
REBUILD = globals().get("REBUILD", "")
OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/critic/" + TAG + "/"
os.makedirs(OUT, exist_ok=True)
# name, x, y, camera yaw
VIEWS = [("01_valley_ridge_s", -4500.0, -13000.0, 0.0), ("02_hollow", -5500.0, -2500.0, 0.0), ("03_wash", 2300.0, 12500.0, 0.0),
         ("04_road_verge", 300.0, 2500.0, 0.0), ("05_checkpoint", 200.0, 7500.0, 0.0), ("06_radixite_edge", -5800.0, 12500.0, 0.0),
         ("07_bench_start", -17000.0, 8000.0, 0.0), ("08_bench_lip", -10800.0, -6000.0, 0.0), ("09_bench_back", -22500.0, -2000.0, 0.0),
         ("10_ramp_main_foot", -6200.0, 18500.0, 0.0), ("11_foothill_ridge", 11000.0, 9000.0, 0.0), ("12_woods_floor", 21500.0, 3500.0, 0.0),
         ("13_peat_bog", -2500.0, -24500.0, 0.0), ("14_spoil_heap", 3200.0, 6400.0, 0.0), ("15_gully", 5600.0, 10500.0, 0.0),
         ("16_knoll_top", 12700.0, -6000.0, 0.0), ("17_yard", 1000.0, 23800.0, 0.0), ("18_west_shoulder", -12000.0, -23500.0, 0.0),
         ("19_valley_open_w", 1500.0, -19000.0, 0.0), ("20_battery", 18700.0, 13400.0, 0.0)]
if REBUILD:
    omb.step_landscape()
    omb.step_fit_level_actors()
    if REBUILD == "full":
        omb.step_resources()
        omb.step_trees()
        omb.step_placeholders()
        omb.step_roads()
        omb.step_checkpoint()
        omb.step_props()
        omb.step_cliffs()
        omb.step_viaduct()
        omb.step_fx()
unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_s = {"n": 0, "t": time.time(), "stage": 0, "i": 0}


def _defer():
    _s["n"] += 1
    now = time.time()
    if _s["stage"] == 0:
        wait = 700 if REBUILD else 60
        if _s["n"] < wait or (REBUILD and not omb.nav_ready() and now - _s["t"] < 900):
            return False
        if REBUILD:
            problems = ore_nav_check.run()
            if problems != []:
                print("NOT SAVED: nav problems", problems)
                return True
            omb.save_level()
        omb.shot_top("top", size=2048)
        os.replace(omb.SHOTS + "top.png", OUT + "top.png")
        _les.editor_request_begin_play()
        _s["stage"], _s["t"], _s["n"] = 1, now, 0
        return False
    gw = _ues.get_game_world()
    if gw is None:
        return now - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 1 and game_t > 10.0:
        unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
        _s["stage"], _s["moved"] = 2, False
        return False
    if _s["stage"] == 2:
        i = _s["i"]
        if i >= len(VIEWS):
            _les.editor_request_end_play()
            print("shots", len(VIEWS), "in", OUT)
            return True
        name, x, y, yaw = VIEWS[i]
        if not _s["moved"]:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            _s["moved"], _s["mark"], _s["shot"] = True, game_t, False
            return False
        if game_t - _s["mark"] > 3.0 and not _s["shot"]:
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1280x720 filename=" + OUT + name + ".png")
            _s["shot"], _s["mark"] = True, game_t
            return False
        if _s["shot"] and game_t - _s["mark"] > 1.0:
            _s["i"] += 1
            _s["moved"] = False
    return False
