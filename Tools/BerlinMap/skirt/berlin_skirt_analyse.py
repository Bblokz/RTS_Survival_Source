"""Offline look at the Ger_Berlin export for the skirt (numpy + PIL): rim heights, what crosses the rim, a picture.

    python berlin_skirt_analyse.py        -> prints stats, writes Saved/MapSkirt/Ger_Berlin/analyse.png
"""
import json

import numpy as np
from PIL import Image, ImageDraw

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/'
lines = open(ROOT + 'grid.txt').read().split('\n')
MINX, MINY, MAXX, MAXY, COMP = [int(v) for v in lines[0].split()]
loc = [float(v) for v in lines[1].split()]
scl = [float(v) for v in lines[2].split()]
LAYERS = lines[3].split()
SX, SY = MAXX - MINX + 1, MAXY - MINY + 1
H = np.fromfile(ROOT + 'full_height.r16', np.uint16).reshape(SY, SX)
Z = (H.astype(np.float64) - 32768.0) * scl[2] / 128.0 + loc[2]          # Z[j, i]: j along world Y, i along world X
W = {n: np.fromfile(ROOT + 'full_%s.r8' % n, np.uint8).reshape(SY, SX) for n in LAYERS}
print('grid', SX, SY, 'z range %.0f .. %.0f' % (Z.min(), Z.max()), 'origin', loc[:2])


def wx(i):
    return loc[0] + i * scl[0]


sides = {'N (+X)': lambda d: Z[:, SX - 1 - d], 'S (-X)': lambda d: Z[:, d], 'E (+Y)': lambda d: Z[SY - 1 - d, :],
         'W (-Y)': lambda d: Z[d, :]}
for name, f in sides.items():
    print(name)
    for d in (0, 5, 10, 20, 40, 60, 90):
        v = f(d)
        print('   %3d m in: min %6.0f  p10 %6.0f  median %6.0f  p90 %6.0f  max %6.0f  below -100: %d m' % (
            d, v.min(), np.percentile(v, 10), np.median(v), np.percentile(v, 90), v.max(), int((v < -100).sum())))
    v = f(0)
    low = np.where(v < -60)[0]
    if low.size:
        runs, s = [], low[0]
        for a, b in zip(low[:-1], low[1:]):
            if b != a + 1:
                runs.append((s, a))
                s = b
        runs.append((s, low[-1]))
        print('   low stretches on the rim (along-edge world coord cm, min z):',
              [(int(wx(a)), int(wx(b)), int(v[a:b + 1].min())) for a, b in runs])

# paint on the outer band
band = np.zeros((SY, SX), bool)
band[:40, :] = band[-40:, :] = True
band[:, :40] = band[:, -40:] = True
tot = sum(W[n][band].astype(np.float64).sum() for n in LAYERS)
print('paint share on the outer 40 m:', {n: round(float(W[n][band].astype(np.float64).sum() / tot), 3) for n in LAYERS})
rim1 = np.zeros((SY, SX), bool)
rim1[0, :] = rim1[-1, :] = True
rim1[:, 0] = rim1[:, -1] = True
tot = sum(W[n][rim1].astype(np.float64).sum() for n in LAYERS)
print('paint share on the rim line:', {n: round(float(W[n][rim1].astype(np.float64).sum() / tot), 3) for n in LAYERS})

# actors and instances beyond the play area
acts = []
for row in open(ROOT + 'actors.tsv', encoding='utf-8'):
    p = row.rstrip('\n').split('\t')
    if len(p) < 13:
        continue
    label, cls = p[0], p[1]
    x, y, z, ox, oy, oz, ex, ey, ez = [float(v) for v in p[2:11]]
    acts.append((label, cls, x, y, z, ox, oy, oz, ex, ey, ez, p[12]))
outer = [a for a in acts if (abs(a[2]) > 26600 or abs(a[3]) > 50600) and a[8] < 20000 and a[9] < 20000]
far = sorted(outer, key=lambda a: -max(abs(a[5]) + a[8], abs(a[6]) + a[9]))
print('actors beyond the blockers (small bounds): %d' % len(outer))
by = {}
for a in outer:
    k = a[0].split('_')[0] + '/' + a[1][:28]
    by.setdefault(k, [0, 0.0, 0.0])
    by[k][0] += 1
    by[k][1] = max(by[k][1], abs(a[5]) + a[8])
    by[k][2] = max(by[k][2], abs(a[6]) + a[9])
for k, v in sorted(by.items(), key=lambda kv: -kv[1][0])[:40]:
    print('   %-50s n %4d  reach |x| %6.0f  |y| %6.0f' % (k, v[0], v[1], v[2]))
big = [a for a in acts if a[8] >= 20000 or a[9] >= 20000]
print('actors with map-wide bounds: %d' % len(big), sorted(set(a[0].split('_')[0] + '/' + a[1][:24] for a in big))[:40])
inst = []
for row in open(ROOT + 'instances.tsv', encoding='utf-8'):
    p = row.rstrip('\n').split('\t')
    if len(p) >= 7:
        inst.append((p[0], p[1], float(p[2]), float(p[3]), float(p[4]), float(p[5]), p[6]))
print('instances beyond play-10 m: %d; max |x| %.0f max |y| %.0f' % (
    len(inst), max(abs(i[2]) + i[5] for i in inst), max(abs(i[3]) + i[5] for i in inst)))

# distance from the landscape edge of the outermost thing per side (actor bounds or instance or mesh trace cell)
void = json.load(open(ROOT + 'void_before.json'))
n = void['n']
mesh_cells = [(void['x0'] + i * void['step'], void['y0'] + j * void['step']) for i in range(n) for j in range(n)
              if void['rows'][i][j] == 'M']
edge = 59850.0
m_out = [(x, y) for (x, y) in mesh_cells if abs(x) > 26600 or abs(y) > 50600]
print('mesh-hit trace cells beyond the blockers: %d; closest to the landscape edge: %.0f cm' % (
    len(m_out), min(edge - max(abs(x), abs(y)) for (x, y) in m_out) if m_out else -1))
ring_hist = {}
for (x, y) in m_out:
    d = int((edge - max(abs(x), abs(y))) // 1000) * 10
    ring_hist[d] = ring_hist.get(d, 0) + 1
print('   mesh cells by distance from the edge (m):', sorted(ring_hist.items())[:20])
near = [a for a in outer if edge - max(abs(a[5]) + a[8], abs(a[6]) + a[9]) < 6000]
print('actors whose bounds come within 60 m of the landscape edge: %d' % len(near))
for a in sorted(near, key=lambda a: edge - max(abs(a[5]) + a[8], abs(a[6]) + a[9]))[:30]:
    print('   %-34s %-30s o (%.0f,%.0f,%.0f) e (%.0f,%.0f) %s' % (a[0][:34], a[1][:30], a[5], a[6], a[7], a[8], a[9], a[11][:40]))

# picture: height shading, play rectangle, outer actors, mesh cells
zn = np.clip((Z + 500) / 1400.0, 0, 1)
img = (np.stack([zn, zn, zn], -1) * 255).astype(np.uint8)
img[Z < -150] = (40, 70, 140)
im = Image.fromarray(img[:, ::-1].transpose(1, 0, 2))      # picture: up = +X (north), right = +Y (east)
d = ImageDraw.Draw(im)


def P(x, y):
    return ((y - loc[1]) / scl[1], SX - 1 - (x - loc[0]) / scl[0])


d.rectangle([P(26000, -50000), P(-26000, 50000)], outline=(255, 220, 0))
d.rectangle([P(50000, -50000), P(-50000, 50000)], outline=(0, 255, 255))
for a in outer:
    d.rectangle([P(a[5] + a[8], a[6] - a[9]), P(a[5] - a[8], a[6] + a[9])], outline=(255, 60, 60))
for i in inst:
    px, py = P(i[2], i[3])
    d.ellipse([px - 2, py - 2, px + 2, py + 2], outline=(255, 140, 0))
im.save(ROOT + 'analyse.png')
print('picture', ROOT + 'analyse.png')
