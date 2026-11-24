"""Play-session test of shell holes and full destruction (throwaway editor, pripyat_cmd.py). Nothing is saved.

For one actor of each class: WallHoleDamageComponent.debug_fire_test_hit a few times and get_hole_count before/after
(the procedural damage shader); then damage until the actor's health is gone and a look 12 s later (removed or not).
"""
import sys
import time

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap")
import pripyat_map_build as pmb

pmb.load_layout()
CLASSES = globals().get("HOLE_CLASSES") or ["BP_PolesieLogHouse_A", "BP_PolesieLogHouse_B", "BP_PolesiePartisanMemorial", "BP_PrypyatRidge_AA_Shell_North", "BP_PrypyatSandbags_Straight",
           "BP_Dest_PostApo_ConcreteFenceLong", "BP_Garrison_RusBunker_1", "BP_Dest_Military_ConcreteFence_1", "BP_Cover_RusSandbags_LongArc",
           "BP_Dest_PostApo_House", "BP_Airport_Dest_Building01", "BP_Military_Dest_Bunker05"]
unreal.SystemLibrary.execute_console_command(pmb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 0, "run": 0}


def valid(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:
        return False


def health(actor):
    try:
        return round(float(actor.get_editor_property("health")), 0)
    except Exception:
        comp = actor.get_component_by_class(unreal.HealthComponent) if valid(actor) else None
        for name in ("current_health", "health", "max_health"):
            try:
                return round(float(comp.get_editor_property(name)), 0)
            except Exception:
                pass
    return None


def _defer():
    _s["n"] += 1
    gw = _ues.get_game_world()
    if gw is None:
        return time.time() - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 0 and game_t > 10.0:
        _s["actors"] = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.Actor)
        try:
            print("debug_fire_test_hit doc:", (unreal.WallHoleDamageComponent.debug_fire_test_hit.__doc__ or "").strip().replace("\n", " ")[:300])
        except Exception as error:
            print("doc:", error)
        _s["stage"] = 1
        return False
    if _s["stage"] == 1:
        if _s["run"] >= len(CLASSES):
            _les.editor_request_end_play()
            return True
        prefix = CLASSES[_s["run"]]
        cand = [a for a in _s["actors"] if valid(a) and a.get_class().get_name().startswith(prefix)]
        if not cand:
            print("%-36s none on the map" % prefix)
            _s["run"] += 1
            return False
        target = cand[0]
        comp = target.get_component_by_class(unreal.WallHoleDamageComponent)
        line = "%-36s " % prefix
        if comp is None:
            line += "no WallHoleDamage component; "
        else:
            before = comp.get_hole_count()
            origin, extent = target.get_actor_bounds(True)
            err = ""
            shots = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 0), (0, 1))
            for k, (sx, sy) in enumerate(shots):
                hit = unreal.Vector(origin.x + sx * extent.x * 0.98 + sy * 60.0 * k, origin.y + sy * extent.y * 0.98 + sx * 60.0 * k, origin.z + extent.z * 0.1)
                try:
                    comp.debug_fire_test_hit(hit, unreal.Vector(-sx, -sy, 0.0), 75.0)
                except Exception as error:
                    err = " ".join(str(error).split())[:160]
                    break
            line += "holes %d -> %d after 6 test hits%s; " % (before, comp.get_hole_count(), (" (call failed: %s)" % err) if err else "")
        _s.update({"target": target, "line": line, "hits": 0, "mark": game_t, "h0": health(target), "stage": 2})
        return False
    if _s["stage"] == 2:
        target = _s["target"]
        if valid(target) and _s["hits"] < 60 and (health(target) is None or health(target) > 0):
            if game_t - _s["mark"] > 0.15:
                unreal.GameplayStatics.apply_damage(target, 1500.0, None, None, None)
                _s["hits"] += 1
                _s["mark"] = game_t
            return False
        _s["h1"] = health(target) if valid(target) else "gone"
        _s["stage"], _s["mark"] = 3, game_t - (11.0 if globals().get("HOLE_FAST") else 0.0)
        return False
    if _s["stage"] == 3 and game_t - _s["mark"] > 12.0:
        target = _s["target"]
        end = "removed" if not valid(target) else "still present (z %.0f, hidden %s)" % (target.get_actor_location().z, bool(target.get_editor_property("hidden")))
        print(_s["line"] + "health %s -> %s after %d hits of 1500; 12 s later: %s" % (_s["h0"], _s["h1"], _s["hits"], end))
        _s["run"] += 1
        _s["stage"] = 1
    return False
