"""Crush test for the dead-spruce kit on Ger_2ndOreMountains (throwaway editor play session, sent with ore_cmd.py).

Takes a player tank from the default start force, puts it 14 m in front of a BP_OreDeadSpruce tree and slides it through
the tree and 14 m beyond over a few seconds (kinematic moves, so the tree's crush overlap fires as it does in play).
Does it once with a medium tank (must crush) and once with a light one (must not: the trees' TankDestructionType is
MediumOrHeavyTank). Prints the tree's Tree-component rotation and validity before and after, and writes screenshots to
Saved/OreMountains/shots/crush_*.png. Nothing is saved.
"""
import math
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb

omb.load_layout()
OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/shots/"
RUNS = [("medium", "Ger_Vehicle_PanzerIII_M_C"), ("light", "Ger_Vehicle_Panzer38T")]
unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 0, "run": 0}


def tree_state(tree):
    if tree is None or not unreal.SystemLibrary.is_valid(tree):
        return "destroyed"
    for comp in tree.get_components_by_class(unreal.StaticMeshComponent):
        if comp.get_name() == "Tree":
            r = comp.get_world_rotation()
            return "Tree comp world rot pitch %.0f roll %.0f yaw %.0f, visible %s, collision %s" % (
                r.pitch, r.roll, r.yaw, comp.is_visible(), str(comp.get_collision_enabled()).split(".")[-1].split(":")[0])
    return "no Tree component"


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 120
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 0 and game_t > 10.0:
        unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
        actors = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.Actor)
        _s["trees"] = [a for a in actors if a.get_class().get_name().startswith("BP_OreDeadSpruce_Skeleton")]
        _s["actors"] = actors
        print("kit skeleton trees in the play session:", len(_s["trees"]))
        _s["stage"] = 1
        return False
    if _s["stage"] == 1:
        if _s["run"] >= len(RUNS):
            _les.editor_request_end_play()
            return True
        name, class_prefix = RUNS[_s["run"]]
        tanks = [a for a in _s["actors"] if unreal.SystemLibrary.is_valid(a) and a.get_class().get_name().startswith(class_prefix)]
        if not tanks:
            print(name, "no unit of class", class_prefix)
            _s["run"] += 1
            return False
        tank = tanks[0]
        # a tree on open ground: the one with the most room round it
        best, room = None, 0.0
        cand = [t for t in _s["trees"] if unreal.SystemLibrary.is_valid(t) and t not in _s.get("used", [])]
        locs = [t.get_actor_location() for t in cand]
        for i, t in enumerate(cand[:400]):
            near = min((math.hypot(locs[i].x - o.x, locs[i].y - o.y) for j, o in enumerate(locs) if j != i), default=1e9)
            if near > room and near < 4000.0 and abs(locs[i].x) < 24000.0 and abs(locs[i].y) < 24000.0:
                best, room = t, near
        _s.setdefault("used", []).append(best)
        loc = best.get_actor_location()
        tl = tank.get_actor_location()
        _s.update({"tree": best, "tank": tank, "name": name, "lift": tl.z - omb.ground_z(tl.x, tl.y), "x": loc.x, "y": loc.y, "mark": game_t})
        tank.set_actor_location(unreal.Vector(loc.x - 1400.0, loc.y, omb.ground_z(loc.x - 1400.0, loc.y) + _s["lift"]), False, True)
        tank.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), True)
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        pawn.set_actor_location(unreal.Vector(loc.x - 300.0, loc.y, omb.ground_z(loc.x, loc.y) + 43.0), False, False)
        print("%s: tank %s, tree %s at (%.0f, %.0f), nearest other tree %.1f m" % (name, tank.get_class().get_name(), best.get_class().get_name(), loc.x, loc.y, room / 100.0))
        print("   before:", tree_state(best))
        _s["stage"], _s["shot"] = 2, False
        return False
    if _s["stage"] == 2:
        dt = game_t - _s["mark"]
        if dt > 2.0 and not _s["shot"]:
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1280x720 filename=" + OUT + "crush_" + _s["name"] + "_before.png")
            _s["shot"] = True
        if dt > 3.0:
            _s["stage"], _s["mark"] = 3, game_t
        return False
    if _s["stage"] == 3:
        dt = game_t - _s["mark"]
        x = _s["x"] - 1400.0 + min(dt / 5.0, 1.0) * 2800.0
        tank = _s["tank"]
        if unreal.SystemLibrary.is_valid(tank):
            tank.set_actor_location(unreal.Vector(x, _s["y"], omb.ground_z(x, _s["y"]) + _s["lift"]), False, True)
        if dt > 9.0:
            print("   after: ", tree_state(_s["tree"]))
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1280x720 filename=" + OUT + "crush_" + _s["name"] + "_after.png")
            _s["stage"], _s["mark"] = 4, game_t
        return False
    if _s["stage"] == 4 and game_t - _s["mark"] > 1.5:
        _s["run"] += 1
        _s["stage"] = 1
    return False
