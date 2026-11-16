"""Nav check for the rear areas (exec'd in the throwaway editor after rear_lib + rear_build): rebuilds the navmesh,
then paths every agent from inside the Soviet base to the station, depot and east road end, and across each pond
(no path point may lie in the water)."""
import time
unreal.SystemLibrary.execute_console_command(world(), 'RebuildNavigation')
STATE = {'t0': time.time()}


def _navs():
    return {a.get_actor_label(): a for a in all_actors() if a.get_class().get_name() == 'RecastNavMesh'}


TARGETS = {
    'station_forecourt': (19650.0, 10200.0), 'station_north': (20300.0, 20000.0), 'depot': (18300.0, -15700.0),
    'road_east_end': (23500.0, -1300.0), 'hub': (6500.0, -700.0),
}
BASE = (1200.0, 300.0, 600.0)
OUTSIDE_WEST = (-6500.0, 300.0)      # player side, outside the base walls: rear reachable around the base
# (start, target) pairs that must NOT be reachable: across the rear map edges, incl. over both ridges
FORBIDDEN = [((22500.0, 12000.0), (27500.0, 12000.0)), ((23500.0, -1300.0), (27500.0, -1300.0)),
             ((21000.0, 20000.0), (21000.0, 25600.0)), ((7800.0, -22000.0), (7800.0, -25000.0)),
             ((20000.0, -20000.0), (27500.0, -20000.0))]


def _wet(pts):
    """Path samples every metre that lie in a pond (more than 1.5 m inside the shoreline)."""
    out = []
    for a, b in zip(pts[:-1], pts[1:]):
        n = max(1, int(math.hypot(b.x - a.x, b.y - a.y) / 100.0))
        for k in range(n + 1):
            x, y = a.x + (b.x - a.x) * k / n, a.y + (b.y - a.y) * k / n
            if _in_pond(x, y, -150.0):
                out.append((x, y))
    return out


def _run():
    if not nav_ready():
        return False
    if time.time() - STATE['t0'] < 5:
        return False
    fails = 0
    for label, nav in sorted(_navs().items()):
        for name, (x, y) in TARGETS.items():
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world(), unreal.Vector(*BASE), unreal.Vector(x, y, gz(x, y) + 50.0), nav)
            pts = p.get_editor_property('path_points') if p else []
            ok = bool(pts) and math.hypot(pts[-1].x - x, pts[-1].y - y) < 600.0
            wet = _wet(pts)
            length = sum(math.hypot(b.x - a.x, b.y - a.y) for a, b in zip(pts[:-1], pts[1:])) / 100.0
            status = 'OK' if ok and not wet else 'FAIL'
            fails += status == 'FAIL'
            print('%-26s %-18s %s len %.0f m, end %s, points in water %d' % (
                label, name, status, length, ('(%.0f,%.0f)' % (pts[-1].x, pts[-1].y)) if pts else '-', len(wet)))
        for name, (x, y) in TARGETS.items():
            if name == 'hub':
                continue
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world(), unreal.Vector(OUTSIDE_WEST[0], OUTSIDE_WEST[1], gz(*OUTSIDE_WEST) + 50.0),
                unreal.Vector(x, y, gz(x, y) + 50.0), nav)
            pts = p.get_editor_property('path_points') if p else []
            ok = bool(pts) and math.hypot(pts[-1].x - x, pts[-1].y - y) < 600.0 and not _wet(pts)
            fails += not ok
            print('%-26s west->%-17s %s' % (label, name, 'OK' if ok else 'FAIL'))
        for (s0, t0) in FORBIDDEN:
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world(), unreal.Vector(s0[0], s0[1], gz(*s0) + 50.0), unreal.Vector(t0[0], t0[1], gz(*t0) + 50.0), nav)
            pts = p.get_editor_property('path_points') if p else []
            leaked = bool(pts) and math.hypot(pts[-1].x - t0[0], pts[-1].y - t0[1]) < 600.0
            fails += leaked
            print('%-26s edge %s->%s %s' % (label, s0, t0, 'LEAK' if leaked else 'OK (blocked)'))
        for pname, pd in PLAN['ponds'].items():
            cx, cy = pd['centre']
            a = math.radians(pd['yaw'])
            s = (cx - math.cos(a) * 3500, cy - math.sin(a) * 3500)
            e = (cx + math.cos(a) * 3500, cy + math.sin(a) * 3500)
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world(), unreal.Vector(s[0], s[1], gz(*s) + 50), unreal.Vector(e[0], e[1], gz(*e) + 50), nav)
            pts = p.get_editor_property('path_points') if p else []
            wet = _wet(pts)
            status = 'OK' if pts and not wet else 'FAIL'
            fails += status == 'FAIL'
            print('%-26s across %-11s %s points %d, in water %d' % (label, pname, status, len(pts), len(wet)))
    print('NAV FAILS %d' % fails)
    return True


_defer = _run
