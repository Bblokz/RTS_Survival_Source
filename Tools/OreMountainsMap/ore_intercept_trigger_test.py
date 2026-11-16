"""Play test of the intercepted-radio triggers (ORE_Intercept_*) on Ger_2ndOreMountains (throwaway editor).

Slides one start-force tank into each trigger sphere in turn and checks that the trigger fires near its radius and
removes its sphere. With a line set it also takes a UI screenshot and waits for the line to end. Nothing is saved.
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
SCRIPTED = unreal.load_class(None, "/Script/RTS_Survival.ScriptedTriggerActor")
AREA = unreal.load_class(None, "/Script/RTS_Survival.TriggerArea")
TRIGGERS = ore_clues.intercept_triggers(ore_clues.INTERCEPT_PICKS)
_s = {"n": 0, "t": time.time(), "stage": "boot", "i": 0, "ok": []}


def _check(name, ok, detail=""):
    print("%s %s %s" % ("PASS" if ok else "FAIL", name, detail))
    _s["ok"].append(ok)


def _trigger(gw, label):
    return next((a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, SCRIPTED) if a.get_actor_label() == label), None)


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    now = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == "boot":
        if not _s.get("started"):
            # the map boots behind the "Press To Start" widget, which hides the portrait in the UI shots: press it
            start_cls = unreal.load_class(None, "/Script/RTS_Survival.W_StartGameWidget")
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(gw, start_cls, False):
                try:
                    w.call_method("OnStartGameButtonClicked")
                    _s["started"] = True
                    print("pressed start on", w.get_name())
                except Exception as error:
                    print("start press failed:", error)
        if now < 10.0:
            return False
        mine = [a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, AREA)
                if a.get_owner() and a.get_owner().get_actor_label().startswith("ORE_Intercept_")]
        _check("intercept spheres", len(mine) == len(TRIGGERS), "found %d of %d" % (len(mine), len(TRIGGERS)))
        tanks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.Pawn) if a.get_class().get_name().startswith("Ger_Vehicle_")]
        if not tanks:
            _check("a player tank", False)
            _les.editor_request_end_play()
            return True
        _s["tank"] = tanks[0]
        tl = _s["tank"].get_actor_location()
        _s["lift"] = tl.z - omb.ground_z(tl.x, tl.y)
        _s["stage"] = "place"
        return False
    if _s["stage"] == "place":
        if _s["i"] >= len(TRIGGERS):
            print("SUMMARY %d/%d passed" % (sum(_s["ok"]), len(_s["ok"])))
            _les.editor_request_end_play()
            return True
        label, cx, cy, radius, _line = TRIGGERS[_s["i"]]
        _s["path"] = [(cx + (radius + 1200.0) * (1.0 - k / 40.0), cy) for k in range(41)]
        _s["step"], _s["stage"] = 0, "drive"
        return False
    label, cx, cy, radius, _line = TRIGGERS[_s["i"]]
    spot = label[len("ORE_Intercept_"):]
    if _s["stage"] == "drive":
        x, y = _s["path"][_s["step"]]
        _s["tank"].set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + _s["lift"]), False, False)
        unreal.GameplayStatics.get_player_pawn(gw, 0).set_actor_location(unreal.Vector(cx + 900.0, cy, omb.ground_z(cx, cy) + 43.0), False, False)
        t = _trigger(gw, label)
        if t is not None and t.get_editor_property("bLinePlayed"):
            _s["at"], _s["mark"], _s["stage"] = math.hypot(x - cx, y - cy), now, "fired"
            _s["line"] = t.get_editor_property("VoiceLine")
            _check(spot + " line set", (_s["line"] is not None) == bool(_line), str(_s["line"].get_name() if _s["line"] else None))
            return False
        _s["step"] += 1
        if _s["step"] >= len(_s["path"]):
            _check(spot + " fired", False, "reached the centre without firing")
            _s["i"] += 1
            _s["stage"] = "place"
        return False
    if _s["stage"] == "fired":
        if now - _s["mark"] < 1.0:
            return False
        _check(spot + " fired", _s["at"] <= radius + 150.0, "at %.0f cm (radius %.0f)" % (_s["at"], radius))
        left = [a for a in unreal.GameplayStatics.get_all_actors_of_class(gw, AREA) if a.get_owner() and a.get_owner().get_actor_label() == label]
        _check(spot + " sphere removed", not left)
        if _s["line"] is not None:
            unreal.SystemLibrary.execute_console_command(gw, "shot showui")
            _s["wait"] = _s["line"].get_editor_property("duration") + 1.5
        else:
            _s["wait"] = 0.0
        _s["mark"], _s["stage"] = now, "wait"
        return False
    if _s["stage"] == "wait":
        if now - _s["mark"] < _s["wait"]:
            return False
        _s["i"] += 1
        _s["stage"] = "place"
    return False
