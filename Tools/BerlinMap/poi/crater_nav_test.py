# Read-only nav test of the start crater on the saved Ger_Berlin (sent to crater_editor_server.py after the save).
# 1) harvest slots: piece centre + (inner radius + 100 cm) in 8 directions (inner radius as the concept assumed:
#    C++ default 200, Large / Crater0 their half size). Per agent: is the slot itself on nav (within 60 cm), and does the
#    game's projection (850 cm extent) + a full path from the HQ approach succeed?
# 2) breaches: clear width of each breach for every agent, sampled across the mouth at the heap ring.
# 3) rim: heap centres must be off the tank navmeshes (the NavArea_Null boxes work).
import json
import math

import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
NS = unreal.NavigationSystemV1
HQ = unreal.Vector(13900, 43300, 0)
OFF = (17400.0, 46800.0)                      # bowl centre (C + (1, -5) m)
PIECES = [("Crater0", 17300, 47300, 500), ("Large", 18250, 46400, 420), ("Point", 16500, 47900, 200),
          ("Crystal 1", 18050, 48150, 220), ("Crystal 2", 16450, 46700, 220), ("Crystal 3", 17300, 48450, 220),
          ("Small 1", 18450, 47450, 200), ("Small 2", 16150, 47350, 200)]
BREACHES = [("W-SW", 250.0, 1750.0), ("N", 8.0, 1950.0), ("S", 183.0, 1750.0)]
navs = {n.get_name().replace("RecastNavMesh-", ""): n for n in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)}
order = ["Character", "LightTank", "MediumTank", "HeavyTank"]


def ground(x, y):
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return 0.0


def project(nav, p, ext):
    q = NS.project_point_to_navigation(w, p, nav, None, unreal.Vector(ext, ext, 400))
    if q is None or (q.x == 0 and q.y == 0 and q.z == 0):
        return None
    return q


def path_ok(nav, goal):
    p = NS.find_path_to_location_synchronously(w, HQ, goal, nav, None)
    pts = list(p.path_points) if p else []
    if not pts or p.is_partial():
        return False, 0.0
    end = pts[-1]
    ln = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100
    return math.hypot(end.x - goal.x, end.y - goal.y) < 80, ln


res = {"slots": {}, "breaches": {}, "rim": {}}
for agent in order:
    nav = navs[agent]
    on_slot = proj_ok = 0
    worst = {}
    for name, x, y, inner in PIECES:
        n_on = 0
        for k in range(8):
            a = math.radians(45 * k)
            sx, sy = x + math.cos(a) * (inner + 100), y + math.sin(a) * (inner + 100)
            p = unreal.Vector(sx, sy, ground(sx, sy) + 50)
            q = project(nav, p, 60)
            if q is not None:
                on_slot += 1
                n_on += 1
            qg = project(nav, p, 850)
            if qg is not None and path_ok(nav, qg)[0]:
                proj_ok += 1
        worst[name] = n_on
    res["slots"][agent] = dict(on_slot=on_slot, game_projection_path_ok=proj_ok, per_piece_on_slot=worst)
    print("%-10s slots on nav %2d/64, reachable via the game's projection %2d/64, per piece %s" % (
        agent, on_slot, proj_ok, worst))
# HQ route to the crater centre floor
for agent in ("LightTank", "HeavyTank"):
    goal = project(navs[agent], unreal.Vector(OFF[0], OFF[1], ground(*OFF) + 50), 300)
    ok, ln = path_ok(navs[agent], goal) if goal else (False, 0)
    print("HQ -> bowl floor %-9s %s, %.0f m" % (agent, "OK" if ok else "FAIL", ln))
    res["route_" + agent] = [ok, ln]
# breach widths: walk across the mouth (perpendicular to the bearing) at the ring radius, 25 cm steps
for name, b, r in BREACHES:
    br = math.radians(b)
    cx, cy = OFF[0] + r * math.cos(br), OFF[1] + r * math.sin(br)
    tx, ty = -math.sin(br), math.cos(br)
    row = {}
    for agent in order:
        best = cur = 0
        for i in range(-120, 121):
            x, y = cx + tx * i * 25, cy + ty * i * 25
            on = project(navs[agent], unreal.Vector(x, y, ground(x, y) + 50), 30) is not None
            cur = cur + 1 if on else 0
            best = max(best, cur)
        row[agent] = best * 0.25
    res["breaches"][name] = row
    print("breach %-5s clear width on nav (m): %s" % (name, row))
# rim heaps blocked
kit = json.load(open("D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinStartCrater/Kit/kit.json"))
for agent in order:
    on = tot = 0
    for m in kit["meshes"].values():
        for bx in m["nav_boxes"]:
            x, y = 17300 + bx["cx"], 47300 + bx["cy"]
            tot += 1
            if project(navs[agent], unreal.Vector(x, y, ground(x, y) + 50), 30) is not None:
                on += 1
    res["rim"][agent] = [on, tot]
    print("rim block centres on %-10s nav: %d of %d" % (agent, on, tot))
json.dump(res, open("D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/nav_test.json", "w"), indent=1)
