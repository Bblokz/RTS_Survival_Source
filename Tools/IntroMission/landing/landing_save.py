# Save Ger_IntroMission from the throwaway editor once the navmesh has finished building (an early save writes an empty
# navmesh). Refuses if temporary test/kit actors are still in the level.
import time
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
require_map()
bad = [a.get_actor_label() for a in all_actors() if a.get_actor_label().startswith(('LANDTEST_', 'KIT_'))
       or a.get_actor_location().z < -150000]
if bad:
    raise RuntimeError('temporary actors still in the level: %s' % bad)
_s = {'t': time.time()}


def _nav_bounds_ok():
    for a in all_actors():
        if a.get_class().get_name() == 'RecastNavMesh':
            o, e = a.get_actor_bounds(False)
            if e.x <= 0:
                return False
    return True


def _defer():
    if time.time() - _s['t'] < 10 or not nav_ready() or not _nav_bounds_ok():
        return time.time() - _s['t'] > 900 and print('nav wait timeout') is None and False
    import os
    path = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_IntroMission.umap'
    before = os.path.getmtime(path)
    ok = save_level()
    after = os.path.getmtime(path)
    print('saved %s, mtime changed %s, size %.1f MB' % (ok, after != before, os.path.getsize(path) / 1e6))
    return True
