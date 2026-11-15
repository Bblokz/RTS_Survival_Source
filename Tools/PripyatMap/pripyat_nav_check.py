"""Nav check for Ger_PripyatMe410 (editor, map open, navmesh built): import pripyat_nav_check; pripyat_nav_check.run()

Legs stay under 600 m (longer editor path queries fail on the search node limit). "go" legs must exist for every
agent; "no" legs must not (bog, island, flank scarps); "inf" legs are for the Character agent only (the boardwalk)."""
import unreal as u

import pripyat_map_build as pmb

ORDER = ("Character", "LightTank", "MediumTank", "HeavyTank")


def run(verbose=True):
    world = pmb.world()
    pmb.load_layout()
    nav_sys = u.NavigationSystemV1.get_navigation_system(world)
    if nav_sys.is_navigation_being_built(world):
        print("NAV STILL BUILDING")
        return None
    navs = {a.get_actor_label().split("-")[-1]: a for a in pmb.all_actors() if isinstance(a, u.RecastNavMesh)}

    def P(p):
        return u.Vector(p[0], p[1], pmb.ground_z(p[0], p[1]) + 20.0)

    def query(nav, a, b):
        path = u.NavigationSystemV1.find_path_to_location_synchronously(world, P(a), P(b), nav, None)
        if path is None or len(path.path_points) < 2 or path.is_partial():
            return None
        end = path.path_points[-1]
        if abs(end.x - b[0]) > 400.0 or abs(end.y - b[1]) > 400.0:
            return None
        return round(path.get_path_length() / 100.0)

    def centre(rid):
        return tuple(pmb.region(rid)["centre"])

    b = pmb.LAYOUT["bounds"]

    def road(rid, frac):
        """A point along the part of the corridor that lies inside the play area (0 = at the edge, 1 = its end)."""
        pts = [p for p in pmb.LAYOUT["roads"][rid]["points"]
               if b["xmin"] + 600.0 < p[0] < b["xmax"] - 600.0 and b["ymin"] + 600.0 < p[1] < b["ymax"] - 600.0]
        p = pts[int(frac * (len(pts) - 1))]
        return (p[0], p[1])

    def aa(rid):
        """The hill-side half of an AA rise (its middle is taken by the gun rings)."""
        cx, cy = pmb.region(rid)["centre"]
        d = (cx * cx + cy * cy) ** 0.5
        return (cx - cx / d * 1000.0 - cy / d * 900.0, cy - cy / d * 1000.0 + cx / d * 900.0)

    start = centre("start_rise")
    ring_w, ring_n, ring_e, ring_s = road("road_west", 0.97), road("rail_north", 0.97), road("road_east", 0.97), road("causeway_south", 0.97)
    top = (-1800.0, 1800.0)
    legs = [
        ("start -> W road end (ring)", start, ring_w, "go"),
        ("ring W -> AA W", ring_w, aa("rise_aa_w"), "go"),
        ("ring W -> ring N", ring_w, ring_n, "go"),
        ("ring N -> AA N", ring_n, aa("rise_aa_n"), "go"),
        ("ring N -> ring E", ring_n, ring_e, "go"),
        ("ring E -> AA E", ring_e, aa("rise_aa_e"), "go"),
        ("ring E -> ring S", ring_e, ring_s, "go"),
        ("ring S -> AA S", ring_s, aa("rise_aa_s"), "go"),
        ("ring S -> ring W", ring_s, ring_w, "go"),
        ("ring W -> top via ramp W", ring_w, road("ramp_w", 0.98), "go"),
        ("ring N -> top via ramp N", ring_n, road("ramp_n", 0.98), "go"),
        ("ring S -> top via ramp S", ring_s, road("ramp_s", 0.98), "go"),
        ("ramp W top -> HQ site", road("ramp_w", 0.98), top, "go"),
        ("ramp N top -> HQ site", road("ramp_n", 0.98), top, "go"),
        ("ramp S top -> HQ site", road("ramp_s", 0.98), top, "go"),
        ("N entry -> hairpin", road("rail_north", 0.03), road("rail_north", 0.45), "go"),
        ("N hairpin -> ring", road("rail_north", 0.45), ring_n, "go"),
        ("E entry -> ford far side", road("road_east", 0.03), road("road_east", 0.45), "go"),
        ("E mid -> ring", road("road_east", 0.45), ring_e, "go"),
        ("S entry -> hairpin", road("causeway_south", 0.03), road("causeway_south", 0.45), "go"),
        ("S hairpin -> ring", road("causeway_south", 0.45), ring_s, "go"),
        ("N lane -> fuel rise", road("rail_north", 0.2), centre("rise_fuel"), "go"),
        ("S lane -> far radixite", road("causeway_south", 0.2), centre("rise_far_radixite"), "go"),
        ("N hairpin -> pilot N rise", road("rail_north", 0.45), (centre("rise_pilot_n")[0] - 600.0, centre("rise_pilot_n")[1] + 700.0), "go"),
        ("S hairpin -> pilot S rise", road("causeway_south", 0.45), (centre("rise_pilot_s")[0] - 600.0, centre("rise_pilot_s")[1] + 700.0), "go"),
        ("E mid -> khutor rise", road("road_east", 0.45), (pmb.LAYOUT["khutor"][0] + 200.0, pmb.LAYOUT["khutor"][1] - 1300.0), "go"),
        ("start -> Demeter pod (boardwalk)", start, centre("pod_hummock"), "inf"),
        ("start -> bog W", start, (5000.0, -24000.0), "no"),
        ("ring N -> bog NE", ring_n, (20000.0, 20000.0), "no"),
        ("ring S -> bog SW", ring_s, (-20000.0, -12000.0), "no"),
        ("ring W -> lower talus", ring_w, (20300.0, -6000.0), "no"),
        ("N lane -> island", road("rail_north", 0.2), centre("island"), "no"),
        ("AA N -> top over the scarp", centre("rise_aa_n"), (11000.0, 1000.0), "wall"),
        ("ring E -> top over the scarp", (0.0, 15200.0), (0.0, 11800.0), "wall"),
    ]
    problems = []
    for name, a, b, kind in legs:
        row = []
        straight = ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5 / 100.0
        for agent in ORDER:
            length = query(navs[agent], a, b)
            row.append("%s %s" % (agent[:2], length))
            if kind == "go" and length is None:
                problems.append("%s: no path for %s" % (name, agent))
            elif kind == "no" and length is not None:
                problems.append("%s: %s finds a path (%s m)" % (name, agent, length))
            elif kind == "wall" and length is not None and length < straight * 2.0:
                problems.append("%s: %s crosses directly (%s m, straight %.0f m)" % (name, agent, length, straight))
            elif kind == "inf" and agent == "Character" and length is None:
                problems.append("%s: no path for infantry" % name)
            elif kind == "inf" and agent != "Character" and length is not None:
                problems.append("NOTE %s: %s can use it too (%s m)" % (name, agent, length))
        if verbose:
            print("%-34s %-5s %s" % (name, kind, "  ".join(row)))
    print("PROBLEMS %d" % len(problems))
    for p in problems:
        print("  " + p)
    return problems
