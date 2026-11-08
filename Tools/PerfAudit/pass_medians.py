"""Median per pass / per material draw across ProfileGPU dumps of a probe log (robust to single-dump contention spikes).
Usage: python pass_medians.py <probe name> [variant index, default 0]"""
import re, sys, statistics, collections
from profilegpu_report import read_dumps

name = sys.argv[1]
variant = sys.argv[2] if len(sys.argv) > 2 else "0"
dumps = [d for d in read_dumps(name) if d["ctx"] and ("variant=%s" % variant) in d["ctx"]]
passes = collections.defaultdict(list)
mats = collections.defaultdict(list)
for d in dumps:
    rows = d["rows"]
    sdepth = None
    seen = collections.Counter()
    mseen = collections.Counter()
    for depth, ms, label, draws in rows:
        if sdepth is None and (label == "Scene" or label.startswith("Scene ")):
            sdepth = depth
            continue
        if sdepth is not None and depth == sdepth + 3:
            seen[label] += ms
        m = re.match(r"(\S+) (\S+) \((\d+) instances?\)", label)
        if m:
            mats[m.group(1).split(".")[-1]].append(ms)
    for k, v in seen.items():
        passes[k].append(v)
print("%s variant %s: %d dumps" % (name, variant, len(dumps)))
for k, v in sorted(passes.items(), key=lambda kv: -statistics.median(kv[1]))[:16]:
    print("  pass %-55s median %.3f ms (n=%d)" % (k[:55], statistics.median(v), len(v)))
agg = {k: sum(v) / max(1, len(dumps)) for k, v in mats.items()}
print("  -- materials, summed draw ms per dump (avg)")
for k, v in sorted(agg.items(), key=lambda kv: -kv[1])[:25]:
    print("  %7.3f ms  %s" % (v, k))
