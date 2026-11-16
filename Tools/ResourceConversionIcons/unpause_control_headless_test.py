"""Control for conversion_progress_headless_test.py: starts PIE on the same map, does nothing but unpause the
self-paused mission and wait 8 s. If the tank path-following assert still fires, it is a map/AI issue, not the
converter progress bar or the test's selection/capture code.
Run: run_progress_test.ps1 -Script unpause_control_headless_test.py -Log <log>
"""
import os

import unreal

S = {"phase": "wait_load", "t": 0.0, "mark": 0.0}


def log(message):
    unreal.log("CONVPROG " + message)


def tick(delta):
    S["t"] += delta
    elapsed = S["t"] - S["mark"]
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if S["phase"] == "wait_load" and S["t"] > 15.0:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S["phase"], S["mark"] = "wait_pie", S["t"]
    elif S["phase"] == "wait_pie" and world is not None and elapsed > 40.0:
        if os.environ.get("CONVPROG_CONTROL_SELECT") == "1":
            # Bisect: select the unit the progress test uses, nothing else.
            unit = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                    if a.get_name() == "Rus_Vehicle_Su122_C_1"][0]
            unreal.GameplayStatics.get_player_controller(world, 0).select_single_unit_from_ui(unit)
            log("control: selected %s" % unit.get_name())
        log("control: paused at start %s; unpausing" % unreal.GameplayStatics.is_game_paused(world))
        unreal.GameplayStatics.set_game_paused(world, False)
        S["phase"], S["mark"] = "running", S["t"]
    elif S["phase"] == "running" and elapsed > 8.0:
        log("control: survived 8 s unpaused")
        S["phase"] = "done"
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log("control script loaded")
