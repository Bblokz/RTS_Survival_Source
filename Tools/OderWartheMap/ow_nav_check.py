"""Nav check for Ger_OderWarthe (editor, map open, navmesh built): import ow_nav_check; ow_nav_check.run()

Leg kinds:
  go    every agent must find a path
  inf   only the Character agent may find a path (boardwalks)
  wall  a path may exist, but not straight through (the length must be at least 1.6 x the straight line): lakes and
        the ridge escarpment between basins, the dragon's-teeth belt for vehicles
Legs stay under about 600 m (longer editor path queries fail on the search node limit)."""
import math

import unreal as u

import ow_map_build as omb

ORDER = ("Character", "LightTank", "MediumTank", "HeavyTank")


def run(verbose=True):
    world = omb.world()
    omb.load_layout()
    L = omb.LAYOUT
    nav_sys = u.NavigationSystemV1.get_navigation_system(world)
    if nav_sys.is_navigation_being_built(world):
        print("NAV STILL BUILDING")
        return None
    navs = {a.get_actor_label().split("-")[-1]: a for a in omb.all_actors() if isinstance(a, u.RecastNavMesh)}

    def P(p):
        return u.Vector(p[0], p[1], omb.ground_z(p[0], p[1]) + 20.0)

    filters = {}
    for agent, fname in (("Character", "RTS_NavFilter_Characters"), ("LightTank", "RTS_NavFilter_LightTanks"),
                         ("MediumTank", "RTS_NavFilter_MediumTanks"), ("HeavyTank", "RTS_NavFilter_HeavyTanks")):
        filters[agent] = u.EditorAssetLibrary.load_blueprint_class("/Game/RTS_Survival/Blueprints/Nav/Filters/" + fname)
    agent_of = {}

    def query(nav, a, b):
        # the agents' own query filters: the project blocks tanks with nav areas (e.g. RTS_NavArea_BlockAllTanks)
        path = u.NavigationSystemV1.find_path_to_location_synchronously(world, P(a), P(b), nav, filters.get(agent_of.get(nav.get_name())))
        if path is None or len(path.path_points) < 2 or path.is_partial():
            return None
        end = path.path_points[-1]
        if abs(end.x - b[0]) > 400.0 or abs(end.y - b[1]) > 400.0:
            return None
        return round(path.get_path_length() / 100.0)

    def f(fid, k=0):
        return tuple(next(x for x in L["features"] if x["id"] == fid)["points"][k])

    def road(rid, frac):
        pts = L["roads"][rid]["points"]
        p = pts[int(frac * (len(pts) - 1))]
        return (p[0], p[1])

    def werk(fid, lx, ly):
        w = L["werks"][fid]
        yaw = math.radians(w["yaw"])
        c = w["centre"]
        return (c[0] + lx * math.cos(yaw) - ly * math.sin(yaw), c[1] + lx * math.sin(yaw) + ly * math.cos(yaw))

    start = f("start")
    legs = [("start -> %s foot" % r, start, road(r, 0.97), "go") for r in ("ramp_north", "ramp_prow", "ramp_east")]
    legs += [
        ("north ramp foot -> DF1", road("ramp_north", 0.97), f("s1"), "go"),
        ("DF1 -> Packlitz dam far end", f("s1"), road("dam_packlitz", 0.97), "go"),
        ("Packlitz dam -> DF2", road("dam_packlitz", 0.97), f("s2"), "go"),
        ("east ramp foot -> Kalau causeway far end", road("ramp_east", 0.97), road("causeway_kalau", 0.97), "go"),
        ("Kalau causeway -> DF3", road("causeway_kalau", 0.97), f("s3"), "go"),
        ("prow ramp foot -> island causeway far end", road("ramp_prow", 0.97), road("causeway_island", 0.97), "go"),
        ("island causeway -> DF4", road("causeway_island", 0.97), f("s4"), "go"),
        ("DF2 -> ford west end", f("s2"), road("ford_hochwald", 0.05), "go"),
        ("ford -> DF5", road("ford_hochwald", 0.95), f("s5"), "go"),
        ("DF4 -> Lubow dam", f("s4"), road("dam_lubow", 0.05), "go"),
        ("Lubow dam -> DF5", road("dam_lubow", 0.95), f("s5"), "go"),
        ("boardwalk Nischlitz", road("boardwalk_nischlitz", 0.03), road("boardwalk_nischlitz", 0.97), "inf"),
        ("boardwalk Hochwald", road("boardwalk_hochwald", 0.03), road("boardwalk_hochwald", 0.97), "inf"),
        ("ridge top -> lowland straight over the escarpment", (-15000.0, -16500.0), (-9000.0, -9500.0), "wall"),
        ("across the Mittelsee (no causeway)", (-14500.0, 3000.0), (-14500.0, -6500.0), "wall"),
    ]
    for fid in sorted(L["werks"]):
        w = L["werks"][fid]
        # from the apron in front of the portal (the portal itself and the Werk block are solid) to the hill top
        legs.append(("%s tunnel apron -> hill top" % fid, werk(fid, -6300.0, 0.0), werk(fid, 300.0, -1900.0), "go"))
        # cross the belt where both ends are dry ground (some fronts look out over a lake)
        for bdeg in (-10.0, -30.0, 10.0, -45.0, 25.0, 0.0):
            br = math.radians(bdeg)
            outer = werk(fid, 4300.0 * math.cos(br), 4300.0 * math.sin(br))
            inner = werk(fid, 2400.0 * math.cos(br), 2400.0 * math.sin(br))
            if omb.ground_z(*outer) > 60.0 and omb.ground_z(*inner) > 60.0:
                break
        legs.append(("%s through the teeth (front, %+.0f deg)" % (fid, bdeg), outer, inner, "teeth"))
    for agent, nav in navs.items():
        agent_of[nav.get_name()] = agent
    station_ids = {tuple(x["points"][0]) for x in L["features"] if x["raw"].get("df") == "station"}

    def edge(p, towards):
        """A station's own structure fills its point: aim 15 m short of it, on the side the leg comes from."""
        if tuple(p) not in station_ids:
            return p
        d = math.hypot(towards[0] - p[0], towards[1] - p[1]) or 1.0
        return (p[0] + (towards[0] - p[0]) / d * 1500.0, p[1] + (towards[1] - p[1]) / d * 1500.0)

    legs = [(name, edge(a, b), edge(b, a), kind) for name, a, b, kind in legs]
    problems = []
    for name, a, b, kind in legs:
        row = []
        straight = math.hypot(a[0] - b[0], a[1] - b[1]) / 100.0
        for agent in ORDER:
            if agent not in navs:
                continue
            length = query(navs[agent], a, b)
            row.append("%s %s" % (agent[:2], length))
            if kind == "go" and length is None:
                problems.append("%s: no path for %s" % (name, agent))
            elif kind == "inf" and agent == "Character" and length is None:
                problems.append("%s: no path for infantry" % name)
            elif kind == "inf" and agent != "Character" and length is not None and length < straight * 1.6:
                problems.append("%s: %s can use it too (%s m)" % (name, agent, length))
            elif kind == "wall" and length is not None and length < straight * 1.6:
                problems.append("%s: %s crosses directly (%s m, straight %.0f m)" % (name, agent, length, straight))
            elif kind == "teeth":
                if agent == "Character" and (length is None or length > straight * 1.6):
                    problems.append("%s: infantry cannot pass the teeth (%s m, straight %.0f m)" % (name, length, straight))
                if agent != "Character" and length is not None and length < straight * 1.6:
                    problems.append("%s: %s drives straight through the teeth (%s m, straight %.0f m)" % (name, agent, length, straight))
        if verbose:
            print("%-48s %-5s straight %4.0f  %s" % (name, kind, straight, "  ".join(row)))
    print("PROBLEMS %d" % len(problems))
    for p in problems:
        print("  " + p)
    return problems
