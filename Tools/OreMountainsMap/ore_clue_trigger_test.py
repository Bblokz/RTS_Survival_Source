"""Play test of the two Act 1 clue triggers on Ger_2ndOreMountains (throwaway editor, sent with ore_cmd.py).

Starts a play session, checks the trigger spheres (count, radius, owner), then drives one tank of the default start
force into fossil-root gully 3 and afterwards onto the miners' shelter apron, sliding it in small steps (kinematic
moves, so overlaps fire as in play). After each entry it checks that the trigger actor flagged its line as played,
that its spheres are gone, and takes a UI screenshot (portrait + subtitle) to Saved/Screenshots/WindowsEditor.
Nothing is saved. Prints PASS / FAIL lines; the last line of the result says OK/FAIL for the request itself.
"""
import math
import time

import unreal

import ore_clues
import ore_map_build as omb

omb.load_layout()
unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
_f = ore_clues.adit_frame()
RUNS = [("A", "ORE_ClueA_Trigger_FossilRoots", ore_clues.ROOT_SPOTS[2], 1500.0),
        ("B", "ORE_ClueB_Trigger_MinersShelter", (_f["x"] + _f["out"][0] * 700.0, _f["y"] + _f["out"][1] * 700.0), 1300.0)]
_s = {"n": 0, "t": time.time(), "stage": "boot", "run": 0, "results": []}
AREA_CLASS = unreal.load_class(None, "/Script/RTS_Survival.TriggerArea")


def _areas(gw):
    return unreal.GameplayStatics.get_all_actors_of_class(gw, AREA_CLASS)


SCRIPTED_CLASS = unreal.load_class(None, "/Script/RTS_Survival.ScriptedTriggerActor")


def _trigger(gw, label):
    found = unreal.GameplayStatics.get_all_actors_of_class(gw, SCRIPTED_CLASS)
    for a in found:
        if a.get_actor_label() == label:
            return a
    if _s["n"] % 200 == 0:
        print("  (no %s among %s)" % (label, [x.get_actor_label() for x in found]))
    return None


def _check(name, ok, detail=""):
    print("%s %s %s" % ("PASS" if ok else "FAIL", name, detail))
    _s["results"].append(ok)


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    now = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == "boot":
        if now < 10.0:
            return False
        areas = _areas(gw)
        radii = sorted(round(a.get_component_by_class(unreal.SphereComponent).get_scaled_sphere_radius()) for a in areas)
        _check("six trigger spheres", len(areas) == 6, "found %d" % len(areas))
        _check("radii 5 x 1500 + 1300", radii == [1300] + [1500] * 5, str(radii))
        for _, label, _, _ in RUNS:
            t = _trigger(gw, label)
            _check(label + " has a line", t is not None and t.get_editor_property("VoiceLine") is not None,
                   str(t.get_editor_property("VoiceLine").get_name()) if t and t.get_editor_property("VoiceLine") else "none")
        tanks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.Pawn)
                 if a.get_class().get_name().startswith("Ger_Vehicle_")]
        _check("a player tank to drive", bool(tanks), "%d tanks" % len(tanks))
        if not tanks:
            _les.editor_request_end_play()
            return True
        _s["tank"] = tanks[0]
        tl = _s["tank"].get_actor_location()
        _s["lift"] = tl.z - omb.ground_z(tl.x, tl.y)
        _s["stage"] = "place"
        return False
    if _s["stage"] == "place":
        if _s["run"] >= len(RUNS):
            print("SUMMARY %d/%d passed" % (sum(_s["results"]), len(_s["results"])))
            _les.editor_request_end_play()
            return True
        key, label, (cx, cy), radius = RUNS[_s["run"]]
        _s["path"] = [(cx + (radius + 1500.0) * (1.0 - k / 40.0), cy) for k in range(41)]
        _s["step"], _s["stage"], _s["played_at"] = 0, "drive", None
        return False
    if _s["stage"] == "drive":
        key, label, (cx, cy), radius = RUNS[_s["run"]]
        x, y = _s["path"][_s["step"]]
        _s["tank"].set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + _s["lift"]), False, False)
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        pawn.set_actor_location(unreal.Vector(cx + 900.0, cy, omb.ground_z(cx, cy) + 43.0), False, False)
        t = _trigger(gw, label)
        if t is not None and t.get_editor_property("bLinePlayed") and _s["played_at"] is None:
            _s["played_at"] = math.hypot(x - cx, y - cy)
            _s["mark"], _s["stage"] = now, "heard"
            return False
        _s["step"] += 1
        if _s["step"] >= len(_s["path"]):
            _check(key + " trigger fired", False, "tank reached the centre, bLinePlayed still False")
            _s["run"] += 1
            _s["stage"] = "place"
        return False
    if _s["stage"] == "heard":
        key, label, _, radius = RUNS[_s["run"]]
        if now - _s["mark"] < 1.5:
            return False
        _check(key + " trigger fired", True, "at %.0f cm from the centre (radius %.0f)" % (_s["played_at"], radius))
        left = [a for a in _areas(gw) if a.get_owner() and a.get_owner().get_actor_label() == label]
        _check(key + " spheres removed after the line", not left, "%d left" % len(left))
        unreal.SystemLibrary.execute_console_command(gw, "shot showui")
        _s["mark"], _s["stage"] = now, "wait_line"
        return False
    if _s["stage"] == "wait_line":
        # let the line play out (shelter 12.9 s) before the next run so the portrait queue is empty
        if now - _s["mark"] < 14.0:
            return False
        _s["run"] += 1
        _s["stage"] = "place"
    return False
