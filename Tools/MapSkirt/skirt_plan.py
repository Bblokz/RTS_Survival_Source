"""Offline planner for horizon skirt planes on any map (plain python + numpy), from skirt_export.py's files.

    python skirt_plan.py <MapName> [--picture]

Writes Saved/MapSkirt/<MapName>/:
  plan.json      landscape rectangle, skirt height, eased-rim stats, fill planes (one per row run of missing sections)
  rim_height.r16 full-grid heights with the landscape rim eased down to the skirt height where that is safe
  plan.png       valid landscape, playable area (camera square), blockers, rim, fills

Rules (same as the Ger_IntroMission skirt):
- Skirt height = just under the lowest landscape boundary height, so the planes never cover landscape.
- The outer 5 m of the landscape boundary eases down to the skirt height, but only where no actor stands within 8 m
  (the user's dressing is never lifted off the ground or buried) and the boundary vertex lies outside every
  blocking volume's playable side... approximated by: outside the camera square minus 10 m, OR beyond a blocker.
  Boundary stretches that cannot be eased keep their height and the fill/strip there is raised to just under them.
- Fill planes cover every missing landscape section inside the landscape rectangle; each run's plane sits just
  under the lowest landscape height along that run's border (after easing).
"""
import json
import math
import os
import sys

import numpy as np

NAME = sys.argv[1]
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/%s/' % NAME
RIM = 5            # eased rim width in vertices (1 m quads)
ACTOR_CLEAR = 800.0
SKIRT_DROP = 2.0   # skirt / fill planes sit this far under the border they meet

lines = open(ROOT + 'grid.txt').read().split('\n')
g = [int(v) for v in lines[0].split()]
loc = [float(v) for v in lines[1].split()]
scale = [float(v) for v in lines[2].split()]
layers = lines[3].split()
MINX, MINY, MAXX, MAXY, COMP = g
SX, SY = MAXX - MINX + 1, MAXY - MINY + 1
H = np.fromfile(ROOT + 'full_height.r16', np.uint16).reshape(SY, SX)
Z = (H.astype(np.float64) - 32768.0) * scale[2] / 128.0 + loc[2]
W = sum(np.fromfile(ROOT + 'full_%s.r8' % n, np.uint8).reshape(SY, SX).astype(np.int32) for n in layers)
valid = W > 0
II, JJ = np.meshgrid(np.arange(SX), np.arange(SY))
X = loc[0] + (II + MINX) * scale[0]
Y = loc[1] + (JJ + MINY) * scale[1]

# component validity (the centre vertex of each component)
NCX, NCY = (SX - 1) // COMP, (SY - 1) // COMP
comp_valid = np.array([[valid[cj * COMP + COMP // 2, ci * COMP + COMP // 2] for ci in range(NCX)] for cj in range(NCY)])

# boundary distance (vertices) inside the valid region: 0 on vertices next to a missing vertex or the array edge
pad = np.pad(valid, 1, constant_values=False)
frontier = valid & ~(pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:])
dist = np.full(valid.shape, 99, np.int32)
dist[frontier] = 0
cur = frontier
for step in range(1, RIM + 1):
    p = np.pad(cur, 1, constant_values=False)
    nxt = (p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]) & valid & (dist == 99)
    dist[nxt] = step
    cur = nxt
rim = dist <= RIM

# actors near the rim: never ease where something stands
acts = []
for row in open(ROOT + 'actors.tsv', encoding='utf-8'):
    parts = row.rstrip('\n').split('\t')
    if len(parts) < 7:
        continue
    label, cls, ax, ay, az, ex, ey = parts[0], parts[1], *map(float, parts[2:])
    if cls in ('Landscape', 'RecastNavMesh', 'BlockingVolume', 'NavMeshBoundsVolume', 'RuntimeVirtualTextureVolume',
               'WorldSettings', 'SkyLight', 'DirectionalLight', 'ExponentialHeightFog', 'PostProcessVolume',
               'SkyAtmosphere', 'VolumetricCloud', 'BP_FogOfWar_Manager_C', 'Brush', 'DefaultPhysicsVolume'):
        continue
    if ex > 20000 or ey > 20000:      # map-wide managers report huge bounds
        continue
    acts.append((ax, ay, max(ex, ey) if max(ex, ey) < 6000 else 0.0))
blocked = np.zeros(valid.shape, bool)
rim_idx = np.argwhere(rim)
if acts:
    A = np.array(acts)
    for (j, i) in rim_idx:
        d = np.hypot(A[:, 0] - X[j, i], A[:, 1] - Y[j, i]) - A[:, 2]
        if d.min() < ACTOR_CLEAR:
            blocked[j, i] = True

# playable side: a rim vertex counts as outside the playable area when the straight line from the camera centre (the
# fog-of-war manager) to it crosses a blocking volume. Inside rim vertices are never eased (units drive there).
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
        # slab test of segment fow -> (x, y) against the box
        t0, t1 = 0.0, 1.0
        ok = True
        for (p0, d, lo, hi) in ((fow[0], x - fow[0], bx0, bx1), (fow[1], y - fow[1], by0, by1)):
            if abs(d) < 1e-6:
                if p0 < lo or p0 > hi:
                    ok = False
                    break
                continue
            a, b = (lo - p0) / d, (hi - p0) / d
            a, b = min(a, b), max(a, b)
            t0, t1 = max(t0, a), min(t1, b)
            if t0 > t1:
                ok = False
                break
        if ok:
            return True
    return False


inside = np.zeros(valid.shape, bool)
if fow is not None and boxes:
    for (j, i) in rim_idx:
        inside[j, i] = not crosses_blocker(X[j, i], Y[j, i])
blocked |= inside

# skirt height: just under the lowest height on the outer rectangle boundary after easing
outer = np.zeros(valid.shape, bool)
outer[0, :] = outer[-1, :] = True
outer[:, 0] = outer[:, -1] = True
easable = rim & ~blocked
skirt_z = None
z_border_fixed = Z[frontier & blocked]
z_border_all = Z[frontier]
base = float(np.percentile(z_border_all, 5))
skirt_z = math.floor(min(base, float(z_border_fixed.min()) if z_border_fixed.size else base) - SKIRT_DROP)
skirt_z = max(skirt_z, float(z_border_all.min()) - SKIRT_DROP)


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


Zr = Z.copy()
w = np.where(easable, smooth(dist / float(RIM)), 1.0)
target = skirt_z + SKIRT_DROP
Zr = np.where(easable & (Z > target), target + (Z - target) * w, Z)
Hr = np.clip(np.round((Zr - loc[2]) * 128.0 / scale[2] + 32768.0), 0, 65535).astype(np.uint16)
Hr.tofile(ROOT + 'rim_height.r16')

# fills: row runs of missing components inside the rectangle, each at just under its own border's lowest height
fills = []
for cj in range(NCY):
    ci = 0
    while ci < NCX:
        if comp_valid[cj, ci]:
            ci += 1
            continue
        c0 = ci
        while ci < NCX and not comp_valid[cj, ci]:
            ci += 1
        c1 = ci
        x0 = loc[0] + (MINX + c0 * COMP) * scale[0]
        x1 = loc[0] + (MINX + c1 * COMP) * scale[0]
        y0 = loc[1] + (MINY + cj * COMP) * scale[1]
        y1 = loc[1] + (MINY + (cj + 1) * COMP) * scale[1]
        j0, j1 = max(cj * COMP - 6, 0), min((cj + 1) * COMP + 6, SY - 1)
        i0, i1 = max(c0 * COMP - 6, 0), min(c1 * COMP + 6, SX - 1)
        sub = Zr[j0:j1 + 1, i0:i1 + 1][valid[j0:j1 + 1, i0:i1 + 1]]
        z = float(sub.min()) - SKIRT_DROP if sub.size else skirt_z
        fills.append({'x0': x0, 'x1': x1, 'y0': y0, 'y1': y1, 'z': round(min(max(z, skirt_z), 100000.0), 1)})

rect = [loc[0] + MINX * scale[0], loc[1] + MINY * scale[1], loc[0] + MAXX * scale[0], loc[1] + MAXY * scale[1]]
# strips under each outer side: at just under that side's lowest (eased) boundary height
sides = {'NX': Zr[:, 0][valid[:, 0]], 'PX': Zr[:, -1][valid[:, -1]], 'NY': Zr[0, :][valid[0, :]], 'PY': Zr[-1, :][valid[-1, :]]}
side_z = {k: (round(float(v.min()) - SKIRT_DROP, 1) if v.size else skirt_z) for k, v in sides.items()}
plan = {'map': NAME, 'rect': rect, 'skirt_z': skirt_z, 'side_z': side_z, 'fills': fills,
        'rim_eased': int((easable & (np.abs(Zr - Z) > 0.4)).sum()), 'rim_blocked': int((rim & blocked).sum()),
        'grid': g}
json.dump(plan, open(ROOT + 'plan.json', 'w'), indent=1)
print('rect', rect, 'skirt_z', skirt_z, 'side_z', side_z)
print('fills', len(fills), 'missing comps', int((~comp_valid).sum()), 'of', comp_valid.size)
print('rim vertices inside the playable area (kept)', int((rim & inside).sum()))
print('rim vertices eased', plan['rim_eased'], 'rim vertices kept (actors within %.0f cm)' % ACTOR_CLEAR, plan['rim_blocked'])
print('fill z range', min(f['z'] for f in fills) if fills else None, max(f['z'] for f in fills) if fills else None)

if '--picture' in sys.argv:
    from PIL import Image, ImageDraw
    img = np.zeros((SY, SX, 3), np.uint8)
    img[valid] = (120, 95, 70)
    img[rim & ~blocked & valid] = (230, 200, 80)
    img[rim & blocked & ~inside] = (220, 60, 60)
    img[rim & inside] = (60, 220, 90)
    im = Image.fromarray(img[::-1]).resize((SX, SY))
    d = ImageDraw.Draw(im)

    def P(x, y):
        return ((x - rect[0]) / scale[0], SY - 1 - (y - rect[1]) / scale[1])
    for row in open(ROOT + 'blockers.tsv', encoding='utf-8'):
        lab, ox, oy, oz, ex, ey, ez = row.rstrip('\n').split('\t')
        ox, oy, ex, ey = float(ox), float(oy), float(ex), float(ey)
        d.rectangle([P(ox - ex, oy + ey), P(ox + ex, oy - ey)], outline=(80, 200, 255))
    im.save(ROOT + 'plan.png')
    print('picture', ROOT + 'plan.png')
