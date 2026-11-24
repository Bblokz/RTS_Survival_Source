"""Offline check after applying: the re-exported landscape against the survey export and the plan, the void grids and
the nav grids.

    python berlin_skirt_verify.py [after_tag]      (default 'after'; use 'fresh' for the fresh-load export)
"""
import json
import sys

import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/'
TAG = sys.argv[1] if len(sys.argv) > 1 else 'after'
lines = open(ROOT + 'grid.txt').read().split('\n')
MINX, MINY, MAXX, MAXY, COMP = [int(v) for v in lines[0].split()]
LAYERS = lines[3].split()
SX, SY = MAXX - MINX + 1, MAXY - MINY + 1
II, JJ = np.meshgrid(np.arange(SX), np.arange(SY))
D = np.minimum(np.minimum(II, SX - 1 - II), np.minimum(JJ, SY - 1 - JJ))


def load(tag, name, dt):
    return np.fromfile(ROOT + '%s_%s' % (tag, name), dt).reshape(SY, SX)


h0, h1, hp = load('full', 'height.r16', np.uint16), load(TAG, 'height.r16', np.uint16), load('planned', 'height.r16', np.uint16)
ch = h0 != h1
print('heights: %d vertices differ from the survey export; farthest from the rim %d m; differ from the plan: %d' % (
    int(ch.sum()), int(D[ch].max()) if ch.any() else -1, int((h1 != hp).sum())))
print('   changed vertices deeper than 70 m from the rim: %d' % int((ch & (D > 70)).sum()))
pc_total, plan_diff = np.zeros((SY, SX), bool), 0
for n in LAYERS:
    a, b, p = load('full', n + '.r8', np.uint8), load(TAG, n + '.r8', np.uint8), load('planned', n + '.r8', np.uint8)
    pc_total |= a != b
    plan_diff += int((b != p).sum())
print('paint: %d vertices differ from the survey export; farthest from the rim %d m; layer values off the plan: %d' % (
    int(pc_total.sum()), int(D[pc_total].max()) if pc_total.any() else -1, plan_diff))
print('   changed paint deeper than 70 m from the rim: %d' % int((pc_total & (D > 70)).sum()))
rim = D == 0
z = (h1.astype(float) - 32768) * 100 / 128
print('rim line height: min %.2f max %.2f; flat band (<=10 m) min %.2f max %.2f' % (z[rim].min(), z[rim].max(), z[D <= 10].min(), z[D <= 10].max()))

for tag in ('before', TAG):
    try:
        v = json.load(open(ROOT + 'void_%s.json' % tag))
    except OSError:
        continue
    n, x0, st = v['n'], v['x0'], v['step']
    tot = {'V': 0, 'L': 0, 'M': 0, 'S': 0}
    near = {'V': 0, 'L': 0, 'M': 0, 'S': 0}       # within 750 m of the centre: 250 m past the camera square
    for i in range(n):
        x = x0 + i * st
        for j in range(n):
            y = x0 + j * st
            c = v['rows'][i][j]
            tot[c] += 1
            if abs(x) <= 75000 and abs(y) <= 75000:
                near[c] += 1
    print('void grid %-7s 10 m cells over +-1500 m: %s | within +-750 m: %s' % (tag, tot, near))

try:
    a, b = json.load(open(ROOT + 'nav_before.json')), json.load(open(ROOT + 'nav_%s.json' % TAG))
    for k in a['agents']:
        ra, rb = a['agents'][k]['rows'], b['agents'][k]['rows']
        diff = sum(1 for r1, r2 in zip(ra, rb) for c1, c2 in zip(r1, r2) if c1 != c2)
        print('nav %-26s on-nav points before %d, %s %d, differing grid points %d, bounds before %s after %s' % (
            k, a['agents'][k]['on_nav'], TAG, b['agents'][k]['on_nav'], diff,
            [round(x) for x in a['agents'][k]['bounds']], [round(x) for x in b['agents'][k]['bounds']]))
except OSError as ex:
    print('nav grids: missing', ex)
try:
    c0, c1 = json.load(open(ROOT + 'counts_before.json')), json.load(open(ROOT + 'counts_%s.json' % TAG))
    print('actors without SKIRT_: before %d, %s %d; SKIRT_ actors %s' % (c0['total_non_skirt'], TAG, c1['total_non_skirt'], c1['skirt']))
    keys = set(c0['by_class']) | set(c1['by_class'])
    d = {k: (c0['by_class'].get(k, 0), c1['by_class'].get(k, 0)) for k in keys if c0['by_class'].get(k, 0) != c1['by_class'].get(k, 0)}
    print('   classes whose count differs:', d)
except OSError as ex:
    print('counts: missing', ex)
