"""Play-session test of the Pripyat kit Blueprints (throwaway editor, sent with pripyat_cmd.py). Nothing is saved.

1. Crush: slides a tank of the default start force through one instance of each crushable kit class (kinematic moves
   with sweep off, so the crush overlap fires as in play) and prints the object's state before and after.
2. Damage: applies point-blank damage to one instance of each destructible kit class until it dies or 12 hits passed,
   and prints whether it was destroyed.
"""
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap")
import pripyat_map_build as pmb

pmb.load_layout()
CRUSH = ["BP_PolesieDeadSpruce_01", "BP_PolesieDeadSpruce_06", "BP_PolesieRoadSign", "BP_PolesieStorkNestPole", "BP_PolesieSweepWell",
         "BP_Prypyat_ServiceCrates", "BP_Prypyat_FuelDrums", "BP_Civilian_Dest_CrushablePole_Electric"]
DAMAGE = ["BP_PolesieLogHouse_A", "BP_PolesieLogHouse_B", "BP_PolesiePartisanMemorial", "BP_PrypyatRidge_AA_Shell_North", "BP_PrypyatSandbags_Straight",
          "BP_Dest_PostApo_ConcreteFenceLong", "BP_Garrison_RusBunker_1"]
unreal.SystemLibrary.execute_console_command(pmb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
CRUSH = globals().get("CRUSH_OVERRIDE", CRUSH)
DAMAGE = globals().get("DAMAGE_OVERRIDE", DAMAGE)
_s = {"n": 0, "t": time.time(), "stage": 0, "run": 0}


def valid(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:
        return False


def hidden(actor):
    try:
        return bool(actor.get_editor_property("hidden"))
    except Exception:
        return False


def state(actor):
    if not valid(actor):
        return "destroyed"
    comps = actor.get_components_by_class(unreal.StaticMeshComponent)
    vis = [c.is_visible() for c in comps]
    rot = comps[0].get_world_rotation() if comps else None
    col = [str(c.get_collision_enabled()).split(".")[-1].split(":")[0] for c in comps[:1]]
    return "alive, hidden %s, mesh visible %s, first mesh pitch %.0f roll %.0f, z %.0f, collision %s" % (
        hidden(actor), vis[:2], rot.pitch if rot else 0, rot.roll if rot else 0, actor.get_actor_location().z, col)


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 0 and game_t > 10.0:
        actors = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.Actor)
        _s["actors"] = actors
        start = pmb.region("start_rise")["centre"]
        near = [a for a in actors if abs(a.get_actor_location().x - start[0]) < 2500 and abs(a.get_actor_location().y - start[1]) < 2500
                and "Vehicle" in a.get_class().get_name()]
        print("start force vehicles:", sorted(set(a.get_class().get_name() for a in near)))
        tanks = [a for a in near if "PanzerIII" in a.get_class().get_name()] or near
        _s["tank"] = tanks[0] if tanks else None
        print("test tank:", _s["tank"].get_class().get_name() if _s["tank"] else None)
        _s["stage"] = 1 if _s["tank"] else 5
        return False
    if _s["stage"] == 1:
        if _s["run"] >= len(CRUSH):
            _s["stage"], _s["run"] = 5, 0
            return False
        prefix = CRUSH[_s["run"]]
        cand = [a for a in _s["actors"] if valid(a) and a.get_class().get_name().startswith(prefix)]
        if not cand:
            print("CRUSH %-40s none on the map" % prefix)
            _s["run"] += 1
            return False
        target = cand[0]
        loc = target.get_actor_location()
        tank = _s["tank"]
        tl = tank.get_actor_location()
        _s.update({"target": target, "lift": 60.0, "x": loc.x, "y": loc.y, "z": loc.z, "mark": game_t, "prefix": prefix, "before": state(target)})
        tank.set_actor_location(unreal.Vector(loc.x - 900.0, loc.y, loc.z + 60.0), False, True)
        tank.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), True)
        _s["stage"] = 3
        return False
    if _s["stage"] == 3:
        dt = game_t - _s["mark"]
        x = _s["x"] - 900.0 + min(max(dt - 1.0, 0.0) / 4.0, 1.0) * 1800.0
        tank = _s["tank"]
        if valid(tank):
            tank.set_actor_location(unreal.Vector(x, _s["y"], _s["z"] + 60.0), False, True)
        if dt > 8.5:
            print("CRUSH %-40s before: %s" % (_s["prefix"], _s["before"]))
            print("      %-40s after:  %s" % ("", state(_s["target"])))
            _s["run"] += 1
            _s["stage"] = 1
        return False
    if _s["stage"] == 5:
        if _s["run"] >= len(DAMAGE):
            _les.editor_request_end_play()
            return True
        prefix = DAMAGE[_s["run"]]
        cand = [a for a in _s["actors"] if valid(a) and a.get_class().get_name().startswith(prefix)]
        if not cand:
            print("DAMAGE %-40s none on the map" % prefix)
            _s["run"] += 1
            return False
        _s.update({"target": cand[0], "prefix": prefix, "hits": 0, "mark": game_t, "before": state(cand[0])})
        _s["stage"] = 6
        return False
    if _s["stage"] == 6:
        target = _s["target"]
        alive = valid(target) and not hidden(target)
        if alive and _s["hits"] < 12 and game_t - _s["mark"] > 0.4:
            unreal.GameplayStatics.apply_damage(target, 600.0, None, _s["tank"], None)
            _s["hits"] += 1
            _s["mark"] = game_t
            return False
        if (not alive and game_t - _s["mark"] > 1.0) or (_s["hits"] >= 12 and game_t - _s["mark"] > 9.0):
            print("DAMAGE %-40s %d hits of 600: before %s" % (_s["prefix"], _s["hits"], _s["before"]))
            print("       %-40s after: %s" % ("", state(target)))
            _s["run"] += 1
            _s["stage"] = 5
        return False
    return False
