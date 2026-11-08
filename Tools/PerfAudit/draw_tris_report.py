"""Heaviest meshes actually drawn in ProfileGPU dumps (per-mesh draw events): triangles per instance per draw.
Usage: python draw_tris_report.py <probe name> [<probe name> ...]"""
import re
import sys

PAT = re.compile(r"LogRHI:\s+[\d.]+%\s+([\d.]+)ms\s+(\S+)\s+(\S+)\s+\((\d+) instances?\)\s+(\d+) draws?\s+(\d+) prims")
best = {}
for name in sys.argv[1:]:
    with open("D:/UE5Projects/RTS_Survival/Saved/Logs/PerfProbe_%s.log" % name, encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            m = PAT.search(line)
            if not m:
                continue
            ms, mat, mesh, inst, draws, prims = m.groups()
            inst, prims = int(inst), int(prims)
            per = prims // max(inst, 1)
            e = best.setdefault(mesh, {"per": 0, "mats": set(), "maps": set(), "ms": 0.0})
            e["per"] = max(e["per"], per)
            e["mats"].add(mat.split(".")[-1])
            e["maps"].add(name)
            e["ms"] += float(ms)
for mesh, e in sorted(best.items(), key=lambda kv: -kv[1]["per"])[:60]:
    print("%8d tris/inst  %6.2f ms(sum)  %-45s %s  %s" % (e["per"], e["ms"], mesh, ",".join(sorted(e["maps"])), ",".join(sorted(e["mats"]))[:80]))
