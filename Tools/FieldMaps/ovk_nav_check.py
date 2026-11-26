"""Per-agent navmesh checks for Field_OreViadukt (run in the build editor after RebuildNavigation).

    import ovk_nav_check; ovk_nav_check.run()

For each RecastNavMesh agent (Character, LightTank, MediumTank, HeavyTank) it finds paths with that nav data as the
pathfinding context and reports length and whether the path is complete:
- HQ -> fuel depot (across the gorge: viaduct for vehicles),
- trestle NE end -> SW end (infantry + light vehicles only; medium/heavy must detour),
- ford: NE rim -> SW rim near the ford (down the tracks, across, up),
- HQ -> gorge floor, HQ -> shaft 7, director -> route exit.
"""
import math

import unreal

import ovk_map_build as ob


def _nav_datas():
    out = {}
    for actor in ob.actors_sub().get_all_level_actors():
        if isinstance(actor, unreal.RecastNavMesh):
            out[actor.get_actor_label().replace("RecastNavMesh-", "")] = actor
    return out


def _v(p, lift=60.0):
    return unreal.Vector(p[0], p[1], ob.ground_or_deck(p[0], p[1]) + lift)


def _path(nav, a, b):
    world = ob.W.world()
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav)
    if path is None:
        return None, False
    pts = list(path.get_editor_property("path_points"))
    length = sum(math.dist((p.x, p.y, p.z), (q.x, q.y, q.z)) for p, q in zip(pts, pts[1:]))
    end_ok = bool(pts) and math.hypot(pts[-1].x - b.x, pts[-1].y - b.y) < 400.0
    return length / 100.0, end_ok and not path.is_partial()


def run():
    navs = _nav_datas()
    t_sw, t_ne = ob.crossing_ends("trestle")
    fsw = ob.feat("ramp_ford_sw").get("centreline") or ob.feat("ramp_ford_sw")["points"]
    fne = ob.feat("ramp_ford_ne").get("centreline") or ob.feat("ramp_ford_ne")["points"]
    g = ob.L()["gorge"]
    floor = (g["ford_centre"][0] + g["ford_dir"][0] * 6000.0, g["ford_centre"][1] + g["ford_dir"][1] * 6000.0)
    floor = (floor[0] + 2000.0 * g["ford_dir"][1], floor[1] - 2000.0 * g["ford_dir"][0])
    tests = [
        ("hq -> fuel depot", ob.xy("hq"), ob.xy("b6_depot")),
        ("trestle NE -> SW", (t_ne[0] + 300.0 * math.cos(math.radians(46.7)), t_ne[1] + 300.0 * math.sin(math.radians(46.7))),
         (t_sw[0] - 300.0 * math.cos(math.radians(46.7)), t_sw[1] - 300.0 * math.sin(math.radians(46.7)))),
        ("ford NE rim -> SW rim", fne[0], fsw[-1]),
        ("hq -> gorge floor", ob.xy("hq"), floor),
        ("hq -> shaft 7", ob.xy("hq"), ob.xy("branch_dest")),
        ("director -> exit", ob.xy("director"), ob.xy("convoy_exit")),
    ]
    lines = []
    for name, a, b in tests:
        row = []
        for agent in ("Character", "LightTank", "MediumTank", "HeavyTank"):
            nav = navs.get(agent)
            if nav is None:
                row.append("%s: no navdata" % agent)
                continue
            length, ok = _path(nav, _v(a), _v(b))
            row.append("%s %s m%s" % (agent, "-" if length is None else "%.0f" % length, "" if ok else " PARTIAL"))
        lines.append("%-22s %s" % (name, " | ".join(row)))
    for line in lines:
        ob.log("nav " + line)
    return lines
