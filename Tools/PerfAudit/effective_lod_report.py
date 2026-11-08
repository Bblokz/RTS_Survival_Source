"""Triangles actually drawn at the RTS camera distance, from inventory_<map>.json (LOD screen sizes + bounds).
UE screen size ~ 2 * 0.889 * radius / distance (90 deg FOV, 16:9). LOD i is used once size < screen_sizes[i].
Usage: python effective_lod_report.py <dist cm> <MapName> [...]"""
import json
import sys

dist = float(sys.argv[1])
rows = []
for name in sys.argv[2:]:
    d = json.load(open("D:/UE5Projects/RTS_Survival/Saved/PerfAudit/inventory_%s.json" % name))
    for k, v in d["meshes"].items():
        ss = v.get("screen_sizes")
        r = v.get("radius") or 0
        if not isinstance(ss, list) or not v["tris"]:
            continue
        size = 1.778 * r / dist
        lod = 0
        for i in range(1, len(ss)):
            if size < ss[i]:
                lod = i
        lod = max(lod, v.get("min_lod") or 0)
        lod = min(lod, len(v["tris"]) - 1)
        tris = v["tris"][lod]
        vis = v["instances"] - v["hidden"]
        rows.append((tris * vis, tris, lod, vis, size, r, v["tris"][0], ss, name, k.split(".")[0]))
rows.sort(reverse=True)
print("drawn_total  tris@lod lod  inst  scrsize  radius  lod0  map  mesh  screen_sizes")
for row in rows[:45]:
    print("%10d %8d %3d %5d %7.3f %7.0f %7d %-22s %s %s" % (row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[8][:22], row[9], row[7]))
print("\n-- per-instance tris drawn vs size (tris per screen-size unit, high = overdetailed for its size)")
dens = sorted(rows, key=lambda r: -(r[1] / max(r[4], 0.01)))
for row in dens[:30]:
    print("%8d tris at size %.3f (r=%.0f) lod=%d inst=%d %s %s" % (row[1], row[4], row[5], row[2], row[3], row[8][:22], row[9]))
