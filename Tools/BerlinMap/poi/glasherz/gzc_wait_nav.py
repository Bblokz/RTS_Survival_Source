# Wait until the navmesh is built (bounds non-zero, not building) and stays so for 20 s of ticks.
import time
import unreal

_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
_st = {"t0": time.time(), "ok_since": None}


def _nav_ready():
    NS = unreal.NavigationSystemV1
    navs = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)
    building = NS.is_navigation_being_built(_w)
    o, e = (navs[0].get_actor_bounds(False) if navs else (None, None))
    filled = bool(navs) and e is not None and (e.x > 100 or e.y > 100)
    now = time.time()
    if building or not filled:
        _st["ok_since"] = None
    elif _st["ok_since"] is None:
        _st["ok_since"] = now
    if _st["ok_since"] is not None and now - _st["ok_since"] > 20:
        print("nav ready after %.0f s; navs %d; building %s" % (now - _st["t0"], len(navs), building))
        return True
    if now - _st["t0"] > 1500:
        raise RuntimeError("nav never finished")
    return False


_defer = _nav_ready
