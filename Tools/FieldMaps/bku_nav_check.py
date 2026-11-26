"""Per-agent navmesh checks for Field_BerlinKanal (run in the build editor after RebuildNavigation).

    import bku_nav_check; bku_nav_check.run()

For each RecastNavMesh agent (Character, LightTank, MediumTank, HeavyTank): paths from the HQ to every ambush spot,
the flak battery and the director, and across the lock footbridge (infantry only: tanks must detour over a bridge).
"""
import math

import unreal

import bku_map_build as bb


def _nav_datas():
    out = {}
    for actor in bb.actors_sub().get_all_level_actors():
        if isinstance(actor, unreal.RecastNavMesh):
            out[actor.get_actor_label().replace("RecastNavMesh-", "")] = actor
    return out


def _v(p, lift=60.0):
    return unreal.Vector(p[0], p[1], bb.gz(p[0], p[1]) + lift)


def _path(nav, a, b):
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(bb.W.world(), a, b, nav)
    if path is None:
        return None, False
    pts = list(path.get_editor_property("path_points"))
    length = sum(math.dist((p.x, p.y, p.z), (q.x, q.y, q.z)) for p, q in zip(pts, pts[1:]))
    end_ok = bool(pts) and math.hypot(pts[-1].x - b.x, pts[-1].y - b.y) < 400.0
    return length / 100.0, end_ok and not path.is_partial()


def run():
    navs = _nav_datas()
    d = bb.L()["dams"]["weir"]
    off = bb.L()["canal"]["half_cm"] + 900.0
    lock_s = (d["centre"][0] - d["dir"][0] * off, d["centre"][1] - d["dir"][1] * off)
    lock_n = (d["centre"][0] + d["dir"][0] * off, d["centre"][1] + d["dir"][1] * off)
    tests = [("hq -> " + f["id"], bb.xy("hq"), f["points"][0]) for f in bb.L()["features"]
             if f["raw"].get("convoy") == "ambush"]
    tests += [("hq -> battery", bb.xy("hq"), bb.xy("battery_dest")), ("hq -> director", bb.xy("hq"), bb.xy("director")),
              ("hq -> radio post", bb.xy("hq"), (bb.xy("radio_post")[0] - 600.0, bb.xy("radio_post")[1])),
              ("lock S -> N", lock_s, lock_n), ("director -> exit", bb.xy("director"), bb.xy("route_exit"))]
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
        bb.log("nav " + line)
    return lines
