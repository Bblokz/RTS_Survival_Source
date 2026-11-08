"""Summarises ProfileGPU dumps in a PerfProbe log: top passes and the most expensive material draws.

Usage: python profilegpu_report.py <probe name> [top N]
Needs r.ShowMaterialDrawEvents 1 / r.EmitMeshDrawEvents 1 (run_gpu_probe.sh sets them) for per-material lines.
Per dump: the frame total, the main passes, and leaf draw events aggregated by name (material / mesh), sorted by ms.
"""
import collections
import re
import sys

LOG = "D:/UE5Projects/RTS_Survival/Saved/Logs/PerfProbe_%s.log"
LINE = re.compile(r"LogRHI:\s*([\d.]+)%\s*([\d.]+)ms\s+(.*?)\s*(?:(\d+) draws?\s+(\d+) prims?.*)?$")


def read_dumps(name):
    dumps, cur, ctx = [], None, None
    with open(LOG % name, encoding="utf-8", errors="ignore") as fh:
        for raw in fh:
            if "RTS_PERF_PROBE profilegpu" in raw:
                ctx = raw.split("profilegpu", 1)[1].strip()
            if "Perf marker hierarchy, total GPU time" in raw:
                cur = {"ctx": ctx, "rows": []}
                dumps.append(cur)
                continue
            if cur is None:
                continue
            if "LogRHI:" not in raw:
                cur = None
                continue
            body = raw.split("LogRHI:", 1)[1].rstrip("\n")
            m = re.match(r"(\s*)([\d.]+)%\s*([\d.]+)ms\s+(.*)$", body)
            if not m:
                continue
            depth = len(m.group(1))
            label = m.group(4)
            draws = re.search(r"(\d+) draws?", label)
            label_clean = re.sub(r"\s+\d+ draws?.*$", "", label).strip()
            label_clean = re.sub(r"\s+\d+ dispatch(es)?.*$", "", label_clean).strip()
            cur["rows"].append((depth, float(m.group(3)), label_clean, int(draws.group(1)) if draws else 0))
    return dumps


def report(name, top):
    dumps = read_dumps(name)
    print("%s: %d dumps" % (name, len(dumps)))
    for d in dumps:
        rows = d["rows"]
        if not rows:
            continue
        frame = rows[0][1]
        print("\n== dump %s  frame %.2f ms" % (d["ctx"], frame))
        # top-level passes under FRDGBuilder::Execute (depth of its children)
        # children of FRDGBuilder::Execute and of its "Scene" child
        for parent in ("FRDGBuilder::Execute", "Scene"):
            pdepth = None
            out = []
            for i, (depth, ms, label, draws) in enumerate(rows):
                if pdepth is None:
                    if label == parent or label.startswith(parent + " "):
                        pdepth = depth
                        cdepth = None
                    continue
                if depth <= pdepth:
                    break
                if cdepth is None:
                    cdepth = depth
                if depth == cdepth:
                    out.append((ms, label, draws))
            for ms, label, draws in sorted(out, reverse=True)[:16]:
                if ms >= 0.03:
                    print("   %-8s %7.3f ms  %-70s %d draws" % (parent[:8], ms, label[:70], draws))
        # leaf aggregation: rows with no deeper child following
        agg = collections.defaultdict(lambda: [0.0, 0])
        for i, (depth, ms, label, draws) in enumerate(rows):
            nxt = rows[i + 1][0] if i + 1 < len(rows) else -1
            if nxt <= depth:
                agg[label][0] += ms
                agg[label][1] += 1
        print("   -- top leaf events by summed ms")
        for label, (ms, n) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:top]:
            print("   %7.3f ms  x%-4d %s" % (ms, n, label[:110]))


if __name__ == "__main__":
    report(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 40)
