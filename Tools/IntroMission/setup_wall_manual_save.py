# Sent to the throwaway editor (rear_editor_server + rear_cmd.py) with Ger_IntroMission open, 2026-11-25: turns on
# FSecureBaseAssaultResponse::bShowWallBreachManualAfterIntelSquad (the crushing field manual right after the Okhotnik
# squad intel) on the Mission_Intro_ClearSovietRig class defaults and on the map's mission instance(s), touching no
# other value, saves the Blueprint, then waits for the nav build and non-zero RecastNavMesh bounds before saving the
# map. Needs a DLL with the new field (DebugGame built 2026-11-25). Uses the server's _defer hook.
import sys
import time

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission')
import setup_ju87_reserve as reserve  # noqa: E402

_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert _w.get_path_name().split('.')[0] == '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission', _w.get_path_name()
_FLAG = 'show_wall_breach_manual_after_intel_squad'


def _set_flag(mission):
    resp = mission.get_editor_property('assault_response')
    resp.set_editor_property(_FLAG, reserve.WALL_MANUAL_AFTER_INTEL)
    mission.set_editor_property('assault_response', resp)
    after = mission.get_editor_property('assault_response')
    print(mission.get_path_name(), _FLAG, after.get_editor_property(_FLAG),
          'intel', after.get_editor_property('intel_squad_library_entry'),
          'aircraft', after.get_editor_property('air_support_aircraft_count'))


_bp = unreal.load_asset(reserve.MISSION_BP)
_set_flag(unreal.get_default_object(_bp.generated_class()))
unreal.BlueprintEditorLibrary.compile_blueprint(_bp)
print('bp saved', unreal.EditorAssetLibrary.save_loaded_asset(_bp, False))

_managers = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.MissionManager)
assert len(_managers) == 1, len(_managers)
_clears = reserve.map_clear_missions(_managers[0])
assert _clears, 'no Mission_Intro_ClearSovietRig in the map mission chain'
for _clear in _clears:
    _set_flag(_clear)
_managers[0].modify()

unreal.SystemLibrary.execute_console_command(_w, 'RebuildNavigation')
_t0 = time.time()
_state = {'quiet': 0}


def _nav_ready():
    if unreal.NavigationSystemV1.get_navigation_system(_w) is None:
        return False
    if unreal.NavigationSystemV1.is_navigation_being_built(_w):
        _state['quiet'] = 0
        return False
    _state['quiet'] += 1
    if _state['quiet'] < 30:
        return False
    navs = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)
    ext = [n.get_actor_bounds(False)[1] for n in navs]
    return bool(ext) and all(e.x > 1000 for e in ext)


def _defer():
    if time.time() - _t0 > 1200:
        raise RuntimeError('nav build still running after 20 min')
    if not _nav_ready():
        return False
    navs = unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)
    print('nav ready after %.0f s: %s' % (time.time() - _t0,
                                          [(n.get_name(), round(n.get_actor_bounds(False)[1].x)) for n in navs]))
    print('map saved', unreal.EditorLoadingAndSavingUtils.save_packages([_w.get_outermost()], False))
    return True
