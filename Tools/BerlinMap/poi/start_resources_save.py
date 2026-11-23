# Sent to the throwaway editor: RebuildNavigation, wait until the nav build is done and the RecastNavMesh bounds are
# non-zero, then save Ger_Berlin. Uses the server's _defer hook.
import time
import unreal

_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert _w.get_path_name().split(".")[0] == "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin"
unreal.SystemLibrary.execute_console_command(_w, "RebuildNavigation")
_t0 = time.time()
_state = {"quiet": 0}


def _nav_ready():
    if unreal.NavigationSystemV1.get_navigation_system(_w) is None:
        return False
    if unreal.NavigationSystemV1.is_navigation_being_built(_w):
        _state["quiet"] = 0
        return False
    _state["quiet"] += 1
    if _state["quiet"] < 30:            # a few quiet ticks so a queued rebuild cannot start right after
        return False
    navs = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)
    ext = [n.get_actor_bounds(False)[1] for n in navs]
    return bool(ext) and all(e.x > 1000 for e in ext)


def _defer():
    if time.time() - _t0 > 1200:
        raise RuntimeError("nav build still running after 20 min")
    if not _nav_ready():
        return False
    navs = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)
    print("nav ready after %.0f s: %s" % (time.time() - _t0, [(n.get_name(), round(n.get_actor_bounds(False)[1].x)) for n in navs]))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([_w.get_outermost()], False)
    print("save_packages", ok)
    return True
