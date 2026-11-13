"""Nav check for Ger_2ndOreMountains (editor, map open, navmesh built): import ore_nav_check; ore_nav_check.run()

Legs stay under 600 m (longer editor path queries fail on the search node limit). A ramp leg must exist for every
agent; a "wall" leg must come back much longer than the straight line (the path has to go round by a ramp)."""
import math

import unreal as u

import ore_map_build as omb

ORDER = ("Character", "LightTank", "MediumTank", "HeavyTank")


def run():
    world = omb.world()
    omb.load_layout()
    nav_sys = u.NavigationSystemV1.get_navigation_system(world)
    if nav_sys.is_navigation_being_built(world):
        print("NAV STILL BUILDING")
        return None
    navs = {a.get_actor_label().split("-")[-1]: a for a in omb.all_actors() if isinstance(a, u.RecastNavMesh)}

    def P(x, y):
        return u.Vector(x, y, omb.ground_z(x, y) + 20.0)

    def query(nav, a, b):
        path = u.NavigationSystemV1.find_path_to_location_synchronously(world, P(*a), P(*b), nav, None)
        if path is None or len(path.path_points) < 2 or path.is_partial():
            return None
        return round(path.get_path_length() / 100.0)

    def ramp_ends(fid):
        line = omb.feature(fid)["centreline"]
        (x0, y0), (x1, y1) = line[0], line[-1]
        # step 12 m past each end, along the ramp
        d0 = math.hypot(line[1][0] - x0, line[1][1] - y0)
        d1 = math.hypot(x1 - line[-2][0], y1 - line[-2][1])
        a = (x0 - (line[1][0] - x0) / d0 * 1200.0, y0 - (line[1][1] - y0) / d0 * 1200.0)
        b = (x1 + (x1 - line[-2][0]) / d1 * 1200.0, y1 + (y1 - line[-2][1]) / d1 * 1200.0)
        return a, b

    hq = tuple(omb.feature("hq")["points"][0])
    knoll = tuple(omb.region("base_b_knoll")["centre"])
    problems = []
    legs = []
    for fid in ("ramp_east", "ramp_main", "ramp_sally", "ramp_west", "knoll_ramp_w", "knoll_ramp_ne"):
        a, b = ramp_ends(fid)
        legs.append((fid, a, b, "ramp"))
        legs.append((("hq->" if "knoll" not in fid else "knoll->") + fid + " top", hq if "knoll" not in fid else knoll, a, "ramp"))
    k = omb.LAYOUT.get("scale", 1.0)
    road = [tuple(p) for p in omb.path("valley_road")["points"]]
    mid, east, west = road[len(road) // 2], road[3], road[-4]
    base_a = tuple(omb.feature("base_a")["points"][0])
    battery = tuple(omb.feature("artillery_battery")["points"][0])
    fixed = [("road: middle->east end", mid, east, "ramp"), ("road: middle->west end", mid, west, "ramp"),
             ("base A->battery", base_a, battery, "ramp"),
             ("road: east edge->through the Base A yard", road[1], road[9], "ramp"),
             ("WALL bench edge->valley under the prow", (-16500.0, 6000.0), (-8500.0, 6000.0), "wall"),
             ("WALL bench edge->valley east", (-17500.0, 22500.0), (-11000.0, 22500.0), "wall"),
             ("WALL bench edge->valley west", (-18500.0, -10000.0), (-9500.0, -10000.0), "wall"),
             ("WALL knoll top->south foot", (15500.0, -9000.0), (11500.0, -9000.0), "wall"),
             ("WALL knoll top->east foot", (19000.0, -5500.0), (19000.0, -1500.0), "wall"),
             ("WALL bench->crest", (-33000.0, 0.0), (-39500.0, 0.0), "none")]
    for name, a, b, kind in fixed:
        if name.startswith("WALL"):
            a, b = (a[0] * k, a[1] * k), (b[0] * k, b[1] * k)      # drawn on the 850 m sketch
        legs.append((name, a, b, kind))
    for name, a, b, kind in legs:
        straight = math.hypot(a[0] - b[0], a[1] - b[1]) / 100.0
        row = {agent: query(navs[agent], a, b) for agent in ORDER}
        bad = False
        for agent in ORDER:
            got = row[agent]
            if kind == "ramp" and (got is None or got > straight * 1.6 + 30):
                bad = bad or not (fid_no_heavy(name) and agent == "HeavyTank")
            if kind == "wall" and got is not None and got < straight * 2.0:
                bad = True
            if kind == "none" and got is not None:
                bad = True
        print("%-4s %-42s straight %4.0f m  %s" % ("BAD" if bad else "ok", name, straight, row))
        if bad:
            problems.append(name)
    print("NAV PROBLEMS %d %s" % (len(problems), problems))
    return problems


def fid_no_heavy(name):
    return False
