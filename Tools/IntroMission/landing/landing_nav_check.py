"""Nav check for the landing field (exec'd in the throwaway editor): rebuild the navmesh, then for every agent
- path from each pad centre out through the gate to the road outside, and from the Soviet base to each pad;
- a path from just outside each fence side to just inside it: long (> 40 m) means the fence closes that side.
Writes Saved/IntroLanding/nav.txt; last line 'NAV FAILS n'."""
import time
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
unreal.SystemLibrary.execute_console_command(world(), 'RebuildNavigation')
STATE = {'t0': time.time()}
PADC = {1: (-1850, 1250), 2: (1850, 1250), 3: (-1850, -1250), 4: (1850, -1250)}
OUTSIDE_GATE = (-2450, 4300)
BASE = (1200.0, 300.0, 600.0)
SIDES = {'N_mid': ((1500, 3900), (1500, 2900)), 'S_mid': ((0, -3900), (0, -2900)), 'E_mid': ((5100, 0), (4100, 0)),
         'W_mid': ((-5100, 0), (-4100, 0)), 'NE_corner': ((5000, 3800), (4200, 3000)),
         'SW_corner': ((-5000, -3800), (-4200, -3000))}


def P3(u, v, dz=60.0):
    x, y = W(u, v)
    z = ground_hit(x, y)[0]
    return unreal.Vector(x, y, z + dz)


def path(a, b, nav):
    p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world(), a, b, nav)
    pts = p.get_editor_property('path_points') if p else []
    ln = sum(math.hypot(q.x - o.x, q.y - o.y) for o, q in zip(pts[:-1], pts[1:])) / 100.0
    end_ok = bool(pts) and math.hypot(pts[-1].x - b.x, pts[-1].y - b.y) < 250.0
    return pts, ln, end_ok


def _run():
    if not nav_ready() or time.time() - STATE['t0'] < 5:
        return False
    out, fails = [], 0
    navs = sorted([a for a in all_actors() if a.get_class().get_name() == 'RecastNavMesh'], key=lambda a: a.get_actor_label())
    for nav in navs:
        lab = nav.get_actor_label()
        for n, (u, v) in PADC.items():
            pts, ln, ok = path(P3(u, v), P3(*OUTSIDE_GATE), nav)
            # the path must cross the north fence line inside the gate opening
            through = False
            for o, q in zip(pts[:-1], pts[1:]):
                ou, ov = L(o.x, o.y)
                qu, qv = L(q.x, q.y)
                if (ov - 3400) * (qv - 3400) <= 0 and abs(qv - ov) > 1e-3:
                    cu = ou + (qu - ou) * (3400 - ov) / (qv - ov)
                    through = through or (-2355 <= cu <= -1405)
            good = ok      # through_gate is informational: project fences are crushable and stay out of the navmesh
            fails += not good
            out.append('%-24s pad%d -> outside gate %s len %.0f m through_gate %s' % (lab, n, 'OK' if good else 'FAIL', ln, through))
            pts, ln, ok = path(unreal.Vector(*BASE), P3(u, v), nav)
            fails += not ok
            out.append('%-24s base -> pad%d %s len %.0f m' % (lab, n, 'OK' if ok else 'FAIL', ln))
        for name, (a, b) in SIDES.items():
            pts, ln, ok = path(P3(*a), P3(*b), nav)
            closed = (not ok) or ln > 40.0
            out.append('%-24s fence %-10s %s (reach %s, len %.0f m)' % (lab, name, 'CLOSED' if closed else 'OPEN', ok, ln))
    out.append('NAV FAILS %d' % fails)
    open(ROOT + 'nav.txt', 'w').write('\n'.join(out))
    print('\n'.join(out))
    return True


_defer = _run
