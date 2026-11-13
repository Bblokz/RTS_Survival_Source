"""Navigation probe for Ger_Berlin: every lane leg and every gap, per nav agent (run() in an editor with the map open
and the navmesh built). Writes Saved/Berlin/logs/nav_report.json."""
import json
import math

import unreal as u

import berlin_map_build as bmb
W = None
F = "/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_"
AGENTS = [("Character", "Characters"), ("LightTank", "LightTanks"), ("MediumTank", "MediumTanks"), ("HeavyTank", "HeavyTanks")]
def navs():
    out = {}
    for a in bmb.all_actors():
        if isinstance(a, u.RecastNavMesh):
            out[a.get_actor_label().replace("RecastNavMesh-", "")] = a
    return out
def P(x, y):
    return u.Vector(x, y, bmb.ground_z(x, y) + 30.0)
def query(nav, filt, a, b):
    path = u.NavigationSystemV1.find_path_to_location_synchronously(W, P(*a), P(*b), nav, filt)
    if path is None or not path.is_valid():
        return None
    pts = path.path_points
    if len(pts) < 2 or path.is_partial():
        return None
    end = pts[-1]
    if math.hypot(end.x - b[0], end.y - b[1]) > 400.0:
        return None
    return round(path.get_path_length() / 100.0)
def run():
    global W
    bmb.load_layout()
    W = bmb.world()
    N = navs()
    res = {}
    FC = {ag: u.EditorAssetLibrary.load_blueprint_class(F + fl) for ag, fl in AGENTS}
    feats = {f["id"]: f for f in bmb.LAYOUT["features"]}
    for p in bmb.LAYOUT["paths"]:
        if p["kind"] != "lane":
            continue
        pts = p["points"]
        bad = []
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            straight = math.hypot(a[0] - b[0], a[1] - b[1]) / 100.0
            row = {}
            for ag, fl in AGENTS:
                q = query(N[ag], FC[ag], a, b)
                row[ag] = "X" if q is None else ("ok" if q < straight * 1.5 + 15 else "detour %d/%d" % (q, straight))
            if any(v != "ok" for v in row.values()):
                bad.append(("%d (%d,%d)->(%d,%d)" % (i, a[0], a[1], b[0], b[1]), row))
        res["lane " + p["id"]] = bad if bad else "all %d segments ok for all classes" % (len(pts) - 1)
    extra = {"zoo->exit_west": ((-11375, -39444), (-15167, -47800)), "zoo->exit_sw": ((-14625, -38889), (-24300, -30000)),
             "hbf->exit_nw": ((22400, -15500), (25300, -15500)), "south_lane->demeter": ((-12458, -3333), feats["demeter_pod"]["points"][0]),
             "sink_all_crossing": ((-23292, -11500), (-23600, -6900)), "zoo_approach->bunker": ((-11375, -39444), (-16575, -40500))}
    for name, (a, b) in extra.items():
        res[name] = {ag: query(N[ag], FC[ag], a, b) for ag, fl in AGENTS}
    for f in bmb.LAYOUT["features"]:
        if f.get("cut_ends"):
            a, b = f["cut_ends"]
            d = f["dir"]
            a = (a[0] - d[0] * 900.0, a[1] - d[1] * 900.0); b = (b[0] + d[0] * 900.0, b[1] + d[1] * 900.0)
            row = {}
            for ag, fl in AGENTS:
                q = query(N[ag], FC[ag], a, b)
                straight = math.hypot(a[0] - b[0], a[1] - b[1]) / 100.0
                row[ag] = "X" if q is None else ("direct" if q < straight * 1.6 + 8 else "detour %d m" % q)
            res["gap " + f["id"] + " (" + str(f["raw"].get("vehicle_class", "all")) + ")"] = row
    json.dump(res, open("D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/nav_report.json", "w"), indent=1)
    for k, v in res.items():
        print(k, v)
    return res
