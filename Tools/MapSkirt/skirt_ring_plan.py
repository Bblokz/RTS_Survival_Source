"""Offline planner: transition ring + horizon skirt planes for maps whose landscape boundary is irregular and uneven
(Ger_FirstMission). Plain python + numpy, from skirt_export.py's files and the void trace grid (void.json).

    python skirt_ring_plan.py <MapName>

The survival map's end-of-map planes only look right when the landscape rim falls to the planes' height. On maps
whose edge runs 1-21 m high along a staircase of missing sections, the existing terrain is NOT edited; instead:
  ring    new landscape sections in currently empty places the camera can see, at most RING_COMPS sections from the
          existing landscape. Heights continue the nearest boundary height and ease down to SKIRT_Z over RING_FALL cm;
          paint continues the nearest boundary paint and fades to the base layer. Shared edge vertices keep their
          existing values, so nothing that exists changes.
  planes  flat skirt strips (SKIRT_Z - 2, tucked under the ring) around the extended landscape out to 1.5 km, plus
          fills for the empty places the camera can see that the ring does not reach.
  guards  ring sections on the playable side of the map's blocking volumes get blockers, so the playable area stays.
Sections the camera sees covered by water or meshes (lake-bed holes, mountains) are left alone: only places where
the trace grid (void.json) hits nothing get ring or planes.

Writes Saved/MapSkirt/<MapName>/ring_plan.json, ring_height.r16, ring_<layer>.r8 (extended-grid rectangle),
ring_plan.png.
"""
import json
import math
import sys

import numpy as np

NAME = sys.argv[1]
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/%s/' % NAME
RING_COMPS = 3
RING_FALL = 4000.0
SKIRT_Z = 144.0
BASE_LAYER = 'Sand'

lines = open(ROOT + 'grid.txt').read().split('\n')
MINX, MINY, MAXX, MAXY, COMP = [int(v) for v in lines[0].split()]
loc = [float(v) for v in lines[1].split()]
scl = [float(v) for v in lines[2].split()]
LAYERS = lines[3].split()
SX0, SY0 = MAXX - MINX + 1, MAXY - MINY + 1
H0 = np.fromfile(ROOT + 'full_height.r16', np.uint16).reshape(SY0, SX0)
W0 = {n: np.fromfile(ROOT + 'full_%s.r8' % n, np.uint8).reshape(SY0, SX0) for n in LAYERS}
void = json.load(open(ROOT + 'void.json'))


def wx(i):
    return loc[0] + i * scl[0]


def wy(j):
    return loc[1] + j * scl[1]


# ---- extended grid: component indices covering the camera-visible trace area -----------------------------------------
vx0, vy0, vs = void['x0'], void['y0'], void['step']
nvx, nvy = len(void['rows']), len(void['rows'][0])
vx1, vy1 = vx0 + (nvx - 1) * vs, vy0 + (nvy - 1) * vs
c_i0 = min(MINX // COMP, int(math.floor(((vx0 - loc[0]) / scl[0]) / COMP)))
c_i1 = max((MAXX - 1) // COMP, int(math.floor(((vx1 - loc[0]) / scl[0]) / COMP)))
c_j0 = min(MINY // COMP, int(math.floor(((vy0 - loc[1]) / scl[1]) / COMP)))
c_j1 = max((MAXY - 1) // COMP, int(math.floor(((vy1 - loc[1]) / scl[1]) / COMP)))
EMINX, EMINY = c_i0 * COMP, c_j0 * COMP
EMAXX, EMAXY = (c_i1 + 1) * COMP, (c_j1 + 1) * COMP
ESX, ESY = EMAXX - EMINX + 1, EMAXY - EMINY + 1
oi, oj = MINX - EMINX, MINY - EMINY        # offset of the original grid inside the extended one

valid = np.zeros((ESY, ESX), bool)
Z = np.zeros((ESY, ESX))
Wt = {n: np.zeros((ESY, ESX)) for n in LAYERS}
orig_valid = sum(W0[n].astype(np.int32) for n in LAYERS) > 0
valid[oj:oj + SY0, oi:oi + SX0] = orig_valid
Z[oj:oj + SY0, oi:oi + SX0] = (H0.astype(np.float64) - 32768.0) * scl[2] / 128.0 + loc[2]
for n in LAYERS:
    Wt[n][oj:oj + SY0, oi:oi + SX0] = W0[n]

NCX, NCY = c_i1 - c_i0 + 1, c_j1 - c_j0 + 1
comp_valid = np.zeros((NCY, NCX), bool)
for cj in range(NCY):
    for ci in range(NCX):
        comp_valid[cj, ci] = valid[cj * COMP + COMP // 2, ci * COMP + COMP // 2]

# ---- which missing components the camera sees as void, which lie under water --------------------------------------
waters = [w for w in void['waters']]


def comp_rect(ci, cj):
    return wx(EMINX + ci * COMP), wx(EMINX + (ci + 1) * COMP), wy(EMINY + cj * COMP), wy(EMINY + (cj + 1) * COMP)


def under_water(ci, cj):
    x0, x1, y0, y1 = comp_rect(ci, cj)
    for (_lab, _mat, ox, oy, oz, ex, ey, _col) in waters:
        if x0 < ox + ex and x1 > ox - ex and y0 < oy + ey and y1 > oy - ey:
            return True
    return False


def sees_void(ci, cj):
    x0, x1, y0, y1 = comp_rect(ci, cj)
    for a in np.arange(x0, x1 + 1, vs / 2):
        for b in np.arange(y0, y1 + 1, vs / 2):
            i, j = int(round((a - vx0) / vs)), int(round((b - vy0) / vs))
            if 0 <= i < nvx and 0 <= j < nvy and void['rows'][i][j] == 'V':
                return True
    return False


# chebyshev distance (components) from the existing landscape
cd = np.full((NCY, NCX), 99, np.int32)
cd[comp_valid] = 0
cur = comp_valid.copy()
for step in range(1, RING_COMPS + 1):
    p = np.pad(cur, 1, constant_values=False)
    grow = np.zeros_like(cur)
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            grow |= p[1 + dj:1 + dj + NCY, 1 + di:1 + di + NCX]
    nxt = grow & (cd == 99)
    cd[nxt] = step
    cur = cur | nxt

ring = []
far_void = []
for cj in range(NCY):
    for ci in range(NCX):
        # holes under water (lake beds) never see void: the water plane is hit by the trace, so sees_void excludes them
        if comp_valid[cj, ci]:
            continue
        if not sees_void(ci, cj):
            continue
        (ring if cd[cj, ci] <= RING_COMPS else far_void).append((ci, cj))
ring_set = set(ring)

# ---- heights + paint for the ring: nearest-boundary propagation, eased to the skirt height ------------------------
new_v = np.zeros((ESY, ESX), bool)
for (ci, cj) in ring:
    new_v[cj * COMP:(cj + 1) * COMP + 1, ci * COMP:(ci + 1) * COMP + 1] = True
new_v &= ~valid
known = valid.copy()
dist = np.where(valid, 0.0, np.inf)
near_z = np.where(valid, Z, 0.0)
near_w = {n: np.where(valid, Wt[n], 0.0) for n in LAYERS}
target = known | new_v
steps = 0
while True:
    frontier = target & ~known
    if not frontier.any():
        break
    steps += 1
    gained = np.zeros_like(known)
    acc_z = np.zeros_like(Z)
    acc_w = {n: np.zeros_like(Z) for n in LAYERS}
    cnt = np.zeros_like(Z)
    pk = np.pad(known, 1, constant_values=False)
    pz = np.pad(near_z, 1)
    pw = {n: np.pad(near_w[n], 1) for n in LAYERS}
    for dj, di in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        nk = pk[1 + dj:1 + dj + ESY, 1 + di:1 + di + ESX]
        m = frontier & nk
        acc_z[m] += pz[1 + dj:1 + dj + ESY, 1 + di:1 + di + ESX][m]
        for n in LAYERS:
            acc_w[n][m] += pw[n][1 + dj:1 + dj + ESY, 1 + di:1 + di + ESX][m]
        cnt[m] += 1
        gained |= m
    if not gained.any():
        break
    near_z[gained] = acc_z[gained] / cnt[gained]
    for n in LAYERS:
        near_w[n][gained] = acc_w[n][gained] / cnt[gained]
    dist[gained] = steps * scl[0]
    known |= gained


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


fall = smooth(dist / RING_FALL)
Zn = np.where(new_v, near_z + (SKIRT_Z - near_z) * fall, Z)
paint_fade = smooth(dist / 1200.0)
for n in LAYERS:
    base = 255.0 if n == BASE_LAYER else 0.0
    Wt[n] = np.where(new_v, near_w[n] * (1 - paint_fade) + base * paint_fade, Wt[n])
stack = np.stack([Wt[n] for n in LAYERS])
tot = stack.sum(0)
stack = np.where(new_v[None], stack / np.maximum(tot, 1e-6) * 255.0, stack)
stack = np.clip(np.round(stack), 0, 255)
diff = 255 - stack.sum(0)
dom = stack.argmax(0)
for k in range(len(LAYERS)):
    stack[k] += np.where(new_v & (dom == k), diff, 0)

# write the extended rectangle (existing vertices carry their own values: unchanged)
Hn = np.clip(np.round((Zn - loc[2]) * 128.0 / scl[2] + 32768.0), 0, 65535).astype(np.uint16)
H_ext = np.where(valid, np.pad(H0, ((oj, ESY - SY0 - oj), (oi, ESX - SX0 - oi))), Hn)
H_ext.astype(np.uint16).tofile(ROOT + 'ring_height.r16')
for k, n in enumerate(LAYERS):
    stack[k].astype(np.uint8).tofile(ROOT + 'ring_%s.r8' % n)

# ---- guards: ring sections on the playable side of the blockers ------------------------------------------------
fow = None
for row in open(ROOT + 'actors.tsv', encoding='utf-8'):
    parts = row.rstrip('\n').split('\t')
    if len(parts) >= 7 and parts[1].startswith('BP_FogOfWar_Manager'):
        fow = (float(parts[2]), float(parts[3]))
boxes = []
for row in open(ROOT + 'blockers.tsv', encoding='utf-8'):
    lab, ox, oy, oz, ex, ey, ez = row.rstrip('\n').split('\t')
    boxes.append((float(ox) - float(ex), float(ox) + float(ex), float(oy) - float(ey), float(oy) + float(ey)))


def crosses_blocker(x, y):
    for (bx0, bx1, by0, by1) in boxes:
        t0, t1, ok = 0.0, 1.0, True
        for (p0, d, lo, hi) in ((fow[0], x - fow[0], bx0, bx1), (fow[1], y - fow[1], by0, by1)):
            if abs(d) < 1e-6:
                if p0 < lo or p0 > hi:
                    ok = False
                    break
                continue
            a, b = (lo - p0) / d, (hi - p0) / d
            t0, t1 = max(t0, min(a, b)), min(t1, max(a, b))
            if t0 > t1:
                ok = False
                break
        if ok:
            return True
    return False


guards = []
for (ci, cj) in ring:
    x0, x1, y0, y1 = comp_rect(ci, cj)
    if not crosses_blocker((x0 + x1) / 2, (y0 + y1) / 2):
        sub = Zn[cj * COMP:(cj + 1) * COMP + 1, ci * COMP:(ci + 1) * COMP + 1]
        guards.append({'x0': x0, 'x1': x1, 'y0': y0, 'y1': y1, 'zmax': float(sub.max())})

# ---- planes -----------------------------------------------------------------------------------------------------
ext_rect = [wx(EMINX), wy(EMINY), wx(EMAXX), wy(EMAXY)]
fills = []
for cj in range(NCY):
    ci = 0
    while ci < NCX:
        if (ci, cj) not in set(far_void):
            ci += 1
            continue
        c0 = ci
        while ci < NCX and (ci, cj) in set(far_void):
            ci += 1
        a0, a1, b0, _ = comp_rect(c0, cj)
        _, a2, _, b1 = comp_rect(ci - 1, cj)
        fills.append({'x0': a0, 'x1': a2, 'y0': b0, 'y1': b1})
runs = [(ci, cj) for (ci, cj) in ring]
plan = {'map': NAME, 'skirt_z': SKIRT_Z - 2.0, 'ext_rect': ext_rect, 'ext_grid': [EMINX, EMINY, EMAXX, EMAXY],
        'ring': [{'ci': c_i0 + ci, 'cj': c_j0 + cj} for (ci, cj) in ring], 'fills': fills, 'guards': guards,
        'layers': LAYERS}
json.dump(plan, open(ROOT + 'ring_plan.json', 'w'), indent=1)
print('extended grid', plan['ext_grid'], 'vertices', ESX, 'x', ESY)
print('ring sections', len(ring), '(existing', int(comp_valid.sum()), ')  far void sections', len(far_void),
      'fills', len(fills), 'guards', len(guards))
lowest = Zn[new_v].min() if new_v.any() else None
print('ring z range %.0f .. %.0f' % (Zn[new_v].min(), Zn[new_v].max()) if new_v.any() else 'no ring')

from PIL import Image, ImageDraw
img = np.zeros((NCY, NCX, 3), np.uint8)
img[comp_valid] = (130, 100, 75)
for (ci, cj) in ring:
    img[cj, ci] = (230, 200, 80)
for (ci, cj) in far_void:
    img[cj, ci] = (90, 160, 230)
for g in guards:
    ci = int((((g['x0'] + g['x1']) / 2 - loc[0]) / scl[0] - EMINX) // COMP)
    cj = int((((g['y0'] + g['y1']) / 2 - loc[1]) / scl[1] - EMINY) // COMP)
    img[cj, ci] = (230, 60, 60)
Image.fromarray(img[::-1]).resize((NCX * 8, NCY * 8), Image.NEAREST).save(ROOT + 'ring_plan.png')
