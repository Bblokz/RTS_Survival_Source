"""Sums per-mesh draw ms inside a named pass subtree (e.g. ShadowDepths, Translucency, BasePass) across ProfileGPU dumps.
Usage: python subtree_report.py <probe name> <pass label prefix> [top N]"""
import re, sys, collections
from profilegpu_report import read_dumps

name, prefix = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
agg = collections.Counter(); prims = collections.Counter(); n = 0
for d in read_dumps(name):
    rows = d["rows"]; i = 0; n += 1
    while i < len(rows):
        depth, ms, label, draws = rows[i]
        if label.startswith(prefix):
            j = i + 1
            while j < len(rows) and rows[j][0] > depth:
                m = re.match(r"(\S+) (\S+) \((\d+) instances?\)", rows[j][2])
                if m:
                    agg[m.group(2)] += rows[j][1]
                j += 1
            i = j
        else:
            i += 1
print("%s %s: %d dumps" % (name, prefix, n))
for k, v in agg.most_common(top):
    print("  %7.3f ms/dump  %s" % (v / max(n, 1), k))
