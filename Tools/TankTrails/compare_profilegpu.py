"""Compares the ProfileGPU dumps of TankTrailBench runs (run_bench.sh <name> -TankTrailBenchProfileGPU).

Usage: python compare_profilegpu.py <run name> [<run name> ...]
Per run: median of the whole GPU frame and of the main view's passes (the BasePass with the most draws in a dump is
the game view; scene captures such as the minimap add smaller ones), plus the main BasePass primitive count.
"""
import re
import statistics
import sys

LOG_DIR = "D:/UE5Projects/RTS_Survival/Saved/Logs/TankTrailBench_%s.log"
PASSES = ["PrePass", "BasePass", "ShadowDepths", "Lights", "Lumen", "Translucency", "PostProcessing"]
LINE = re.compile(r"LogRHI:\s*([\d.]+)%\s*([\d.]+)ms\s+(\S+)(?:\s+(\d+) draws\s+(\d+) prims)?")


def read_dumps(run):
    dumps = []
    current = None
    with open(LOG_DIR % run, encoding="utf-8", errors="ignore") as log:
        for line in log:
            if "Perf marker hierarchy, total GPU time" in line:
                current = {"frame": None, "passes": {}}
                dumps.append(current)
                continue
            if current is None or "LogRHI:" not in line:
                if current is not None and "LogRHI" not in line:
                    current = None
                continue
            match = LINE.search(line)
            if not match:
                continue
            ms, label = float(match.group(2)), match.group(3)
            draws = int(match.group(4)) if match.group(4) else 0
            prims = int(match.group(5)) if match.group(5) else 0
            if label == "Frame" and current["frame"] is None:
                current["frame"] = ms
            if label in PASSES:
                best = current["passes"].get(label)
                # The main view is the instance of the pass with the most draws.
                if best is None or draws > best[1]:
                    current["passes"][label] = (ms, draws, prims)
    return [dump for dump in dumps if dump["frame"] is not None]


def report(run):
    dumps = read_dumps(run)
    if not dumps:
        print("%s: no ProfileGPU dumps (run without -TankTrailBenchProfileGPU)" % run)
        return
    print("%s: %d dumps, GPU frame median %.2f ms" % (run, len(dumps),
                                                       statistics.median(d["frame"] for d in dumps)))
    for name in PASSES:
        values = [d["passes"][name] for d in dumps if name in d["passes"]]
        if values:
            print("  %-15s median %.3f ms  draws %d  prims %d" % (
                name, statistics.median(v[0] for v in values), int(statistics.median(v[1] for v in values)),
                int(statistics.median(v[2] for v in values))))


if __name__ == "__main__":
    for run_name in sys.argv[1:]:
        report(run_name)
