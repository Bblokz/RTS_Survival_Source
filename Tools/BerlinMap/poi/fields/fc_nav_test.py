# Read-only nav gate for the field craters (E1/E2/E5/E6), sent to crater_editor_server.py.
# Set FC_TAG ("before" / "after") in the namespace first. Reads Saved/BerlinCrater/fields/nav_spec.json (written by
# bfc_build.py: pieces with harvest radius, bowl centre, lanes with ring radius) and writes nav_<tag>.json.
#  - slots: piece centre + (inner + 100 cm) x 8, LightTank: on nav within 60 cm; game projection (850 cm) + full path
#    from the HQ approach
#  - routes: HQ -> bowl centre for LightTank / HeavyTank
#  - lanes: clear width across the lane at the ring radius (LightTank / HeavyTank, 25 cm steps), and a HeavyTank
#    path from 14 m outside the lane mouth to the bowl centre (ok + length)
import json
import math

import unreal

_TAG = globals().get("FC_TAG", "before")
_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
_NS = unreal.NavigationSystemV1
_HQ = unreal.Vector(13900, 43300, 0)
_navs = {n.get_name().replace("RecastNavMesh-", ""): n for n in unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.RecastNavMesh)}
_spec = json.load(open("D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/fields/nav_spec.json"))


def _ground(x, y):
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(_w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return 0.0


def _project(nav, p, ext):
    q = _NS.project_point_to_navigation(_w, p, nav, None, unreal.Vector(ext, ext, 400))
    if q is None or (q.x == 0 and q.y == 0 and q.z == 0):
        return None
    return q


def _path(nav, a, goal):
    p = _NS.find_path_to_location_synchronously(_w, a, goal, nav, None)
    pts = list(p.path_points) if p else []
    if not pts or p.is_partial():
        return False, 0.0
    end = pts[-1]
    ln = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100
    return math.hypot(end.x - goal.x, end.y - goal.y) < 80, round(ln, 1)


_res = {}
for key, site in _spec.items():
    r = {"slots": {}, "routes": {}, "lanes": {}}
    lt = _navs["LightTank"]
    for p in site["pieces"]:
        on = ok = 0
        for k in range(8):
            a = math.radians(45 * k)
            sx, sy = p["x"] + math.cos(a) * p["rr"], p["y"] + math.sin(a) * p["rr"]
            q0 = unreal.Vector(sx, sy, _ground(sx, sy) + 50)
            if _project(lt, q0, 60) is not None:
                on += 1
            qg = _project(lt, q0, 850)
            if qg is not None and _path(lt, _HQ, qg)[0]:
                ok += 1
        r["slots"][p["label"]] = [on, ok]
    cx, cy = site["centre"]
    for agent in ("LightTank", "HeavyTank"):
        g = _project(_navs[agent], unreal.Vector(cx, cy, _ground(cx, cy) + 50), 600)
        r["routes"][agent] = list(_path(_navs[agent], _HQ, g)) if g else [False, 0.0]
    for ln in site["lanes"]:
        br = math.radians(ln["bearing"])
        mx, my = cx + ln["ring_cm"] * math.cos(br), cy + ln["ring_cm"] * math.sin(br)
        tx, ty = -math.sin(br), math.cos(br)
        row = {}
        for agent in ("LightTank", "HeavyTank"):
            best = cur = 0
            for i in range(-100, 101):
                x, y = mx + tx * i * 25, my + ty * i * 25
                on = _project(_navs[agent], unreal.Vector(x, y, _ground(x, y) + 50), 30) is not None
                cur = cur + 1 if on else 0
                best = max(best, cur)
            row[agent] = best * 0.25
        ox, oy = cx + (ln["ring_cm"] + 1400) * math.cos(br), cy + (ln["ring_cm"] + 1400) * math.sin(br)
        hv = _navs["HeavyTank"]
        a0 = _project(hv, unreal.Vector(ox, oy, _ground(ox, oy) + 50), 600)
        g = _project(hv, unreal.Vector(cx, cy, _ground(cx, cy) + 50), 600)
        row["heavy_through"] = list(_path(hv, a0, g)) if (a0 and g) else [False, 0.0]
        r["lanes"][str(ln["bearing"])] = row
    _res[key] = r
    print(key, json.dumps(r))
json.dump(_res, open("D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/fields/nav_%s.json" % _TAG, "w"), indent=1)
print("wrote nav_%s.json" % _TAG)
