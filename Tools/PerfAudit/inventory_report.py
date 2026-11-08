"""Prints the heaviest meshes and materials of Saved/PerfAudit/inventory_<map>.json files.
Usage: python inventory_report.py <MapName> [<MapName> ...]   (e.g. Ger_FirstMission)"""
import json
import sys

for name in sys.argv[1:]:
    d = json.load(open("D:/UE5Projects/RTS_Survival/Saved/PerfAudit/inventory_%s.json" % name))
    print("=" * 30, name, "actors", d["actors"])
    for l in d["landscape"]:
        print("landscape", l)
    m = d["meshes"]
    print("-- meshes by LOD0 triangles")
    for k, v in sorted(m.items(), key=lambda kv: -(kv[1]["tris"][0] if kv[1]["tris"] else 0))[:25]:
        print("%8d x%-5d lods=%d nan=%d r=%-7s %s" % (v["tris"][0], v["instances"], v["lods"], v["nanite"], v.get("radius"), k.split(".")[0]))
    print("-- meshes by total LOD0 triangles")
    for k, v in sorted(m.items(), key=lambda kv: -kv[1]["total_tris0"])[:20]:
        print("%10d = %7d x%-5d lods=%d nan=%d %s" % (v["total_tris0"], v["tris"][0], v["instances"], v["lods"], v["nanite"], k.split(".")[0]))
    print("-- materials by PS instructions")
    for k, v in sorted(d["materials"].items(), key=lambda kv: -(kv[1].get("ps") or 0))[:30]:
        print("ps=%4s vs=%4s smp=%2s tex=%-2s %-12s 2s=%d inst=%5d %s <- %s" % (
            v.get("ps"), v.get("vs"), v.get("samplers"), v.get("ps_tex"), (v.get("blend") or "")[6:17], v.get("two_sided", 0),
            v["instances"], k.split(".")[-1], (v.get("base") or "").split(".")[-1]))
    print("-- niagara", d["niagara"])
