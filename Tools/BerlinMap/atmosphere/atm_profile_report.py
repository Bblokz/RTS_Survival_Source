"""Reads the ProfileGPU dumps that atm_shots.bench_end() asked for out of the throwaway editor's log and prints, per
state (layer hidden / shown), the GPU time of the scene and of the passes the atmosphere touches.

    python atm_profile_report.py [log]      default Saved/Berlin/atmosphere/logs/editor.log
"""
import re
import statistics
import sys

LOG = sys.argv[1] if len(sys.argv) > 1 else "D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/logs/editor.log"
PASSES = ("Scene", "Translucency", "Distortion", "Lights", "LightFunctionAtlas", "PostProcessing")
LINE = re.compile(r"LogRHI:\s+[\d.]+%\s*([\d.]+)ms\s+(.+?)(?:\s+(\d+) draws?\b.*)?$")

dumps, current = {}, None
for raw in open(LOG, "r", encoding="utf-8", errors="replace"):
    if "ATM_PROFILE_BEGIN" in raw:
        current = raw.strip().rsplit(" ", 1)[1]
        dumps[current] = {}
        continue
    if current is None or "LogRHI:" not in raw:
        continue
    match = LINE.search(raw.rstrip())
    if not match:
        continue
    ms, name, draws = float(match.group(1)), match.group(2).strip(), int(match.group(3) or 0)
    key = name.split(" ")[0].split("(")[0]
    if key in PASSES and key not in dumps[current]:
        dumps[current][key] = (ms, draws)
    if key == "Frame" and "Frame" not in dumps[current]:
        dumps[current]["Frame"] = (ms, draws)

groups = {}
for label, passes in dumps.items():
    district, state = label.split("_")[0], label.split("_")[1]
    groups.setdefault((district, state), []).append(passes)
print("%-8s %-4s %s" % ("view", "", "  ".join("%-22s" % p for p in ("Frame",) + PASSES)))
for (district, state), runs in sorted(groups.items()):
    cells = []
    for key in ("Frame",) + PASSES:
        values = [r[key] for r in runs if key in r]
        if values:
            cells.append("%5.2f ms %4d draws   " % (statistics.median(v[0] for v in values), statistics.median(v[1] for v in values)))
        else:
            cells.append("%-22s" % "-")
    print("%-8s %-4s %s  (n=%d)" % (district, state, "  ".join(cells), len(runs)))
