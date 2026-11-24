"""Offline planner for the Ger_Berlin horizon skirt (numpy + PIL), from berlin_skirt_survey.py's export.

    python berlin_skirt_plan.py

Method: simple rim easing (the landscape is a complete 1197 m square whose rim is one rubble plateau, 2.2-6.4 m high;
nothing is missing, so no ring of new sections is needed). Unlike MapSkirt/skirt_plan.py the rim is NOT eased down to
its lowest point (that is the Spree bed, -5 m): the skirt planes sit at the plateau's own height and the outer band is
flattened to it, raised where it is lower, lowered where it is higher.

  flat band   outer FLAT m: exactly the skirt height T (so the planes, tucked under it, never show a step or a gap,
              whatever landscape LOD is drawn)
  ease band   FLAT..EASE m from the rim: smoothstep between the untouched ground and T
  river       where the Spree trench crosses the band the bed rises to T over TRENCH m instead (the channel silts up
              with rubble and ends before the rim; the water plane stays as it is)
  paint       fades to pure BerlinRubble with the same weight (the skirt material is the rubble layer)
  guards      a vertex within ACTOR_CLEAR of any actor bounds, instanced mesh or mesh-hit trace cell keeps its ground
              (weight fades in over ACTOR_FADE beyond that); the play area, blockers and nav bounds are far inside

Writes Saved/MapSkirt/Ger_Berlin/: plan.json, rim_<side>_height.r16 + rim_<side>_<Layer>.r8 for the four border
strips (the only rectangles imported), planned_height.r16 (full grid, for the check after applying), plan.png.
"""
import json
import struct

import numpy as np
from PIL import Image

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/'
FLAT, EASE, TRENCH = 10, 30, 70          # metres (= vertices) from the rim
STRIP = 90                               # imported strip width in vertices
ACTOR_CLEAR, ACTOR_FADE = 800.0, 1000.0  # cm
TRENCH_Z = 150.0                         # ground below this inside the band is the river trench
BASE = 'BerlinRubble'
REACH, TUCK, DROP = 150000.0, 300.0, 3.0

lines = open(ROOT + 'grid.txt').read().split('\n')
MINX, MINY, MAXX, MAXY, COMP = [int(v) for v in lines[0].split()]
loc = [float(v) for v in lines[1].split()]
scl = [float(v) for v in lines[2].split()]
LAYERS = lines[3].split()
SX, SY = MAXX - MINX + 1, MAXY - MINY + 1
H = np.fromfile(ROOT + 'full_height.r16', np.uint16).reshape(SY, SX)
UNIT = scl[2] / 128.0
Z = (H.astype(np.float64) - 32768.0) * UNIT + loc[2]
W = {n: np.fromfile(ROOT + 'full_%s.r8' % n, np.uint8).reshape(SY, SX) for n in LAYERS}
II, JJ = np.meshgrid(np.arange(SX), np.arange(SY))
X = loc[0] + (II + MINX) * scl[0]
Y = loc[1] + (JJ + MINY) * scl[1]
D = np.minimum(np.minimum(II, SX - 1 - II), np.minimum(JJ, SY - 1 - JJ)).astype(np.float64)   # metres from the rim


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def box_blur(a, r):
    out = a.astype(np.float64)
    for axis in (0, 1):
        c = np.cumsum(np.pad(out, [(r + 1, r) if k == axis else (0, 0) for k in (0, 1)], mode='edge'), axis=axis)
        n = out.shape[axis]
        hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
        lo = np.take(c, np.arange(0, n), axis=axis)
        out = (hi - lo) / (2 * r + 1)
    return out


band = D <= TRENCH + 10
trench0 = band & (Z < TRENCH_Z)
trench = np.clip(box_blur(box_blur(trench0, 3), 3) * 1.6, 0, 1)        # soft, a little wider than the trench itself
w_reg = 1.0 - smooth((D - FLAT) / float(EASE - FLAT))
w_tr = 1.0 - smooth((D - FLAT) / float(TRENCH - FLAT))
w = np.maximum(w_reg, trench * w_tr)
w[D > TRENCH] = 0.0

# ---- guards: distance of band vertices to everything that stands out there ----------------------------------------
rects = []            # (x0, x1, y0, y1) footprints
SKIP = ('Landscape', 'RecastNavMesh', 'BlockingVolume', 'NavMeshBoundsVolume', 'RuntimeVirtualTextureVolume',
        'WorldSettings', 'SkyLight', 'DirectionalLight', 'PostProcessVolume', 'BP_FogOfWar_Manager_C', 'Brush',
        'DefaultPhysicsVolume')
edge = -loc[0]
for row in open(ROOT + 'actors.tsv', encoding='utf-8'):
    p = row.rstrip('\n').split('\t')
    if len(p) < 13 or p[1] in SKIP:
        continue
    ox, oy, ex, ey = float(p[5]), float(p[6]), float(p[8]), float(p[9])
    if ex > 20000 or ey > 20000:          # map-wide containers (instanced rubble, rails, the water plane): see below
        continue
    if max(abs(ox) + ex, abs(oy) + ey) < edge - (TRENCH + 30) * 100:
        continue
    rects.append((ox - ex, ox + ex, oy - ey, oy + ey))
n_act = len(rects)
for row in open(ROOT + 'instances.tsv', encoding='utf-8'):
    p = row.rstrip('\n').split('\t')
    if len(p) < 7:
        continue
    x, y, r = float(p[2]), float(p[3]), float(p[5])
    if max(abs(x), abs(y)) + r < edge - (TRENCH + 30) * 100:
        continue
    rects.append((x - r, x + r, y - r, y + r))
n_inst = len(rects) - n_act
void = json.load(open(ROOT + 'void_before.json'))
nv = void['n']
zs = np.array(struct.unpack('%df' % (nv * nv), open(ROOT + 'void_before_z.f32', 'rb').read())).reshape(nv, nv)
n_mesh = 0
for i in range(nv):
    for j in range(nv):
        if void['rows'][i][j] != 'M':
            continue
        x, y = void['x0'] + i * void['step'], void['y0'] + j * void['step']
        if max(abs(x), abs(y)) < edge - (TRENCH + 30) * 100 or abs(zs[i, j] + 190.0) < 2.0:   # the water plane is no guard
            continue
        rects.append((x - 500, x + 500, y - 500, y + 500))
        n_mesh += 1
gdist = np.full(Z.shape, 1e9)
idx = np.argwhere(D <= TRENCH + 1)
bx, by = X[idx[:, 0], idx[:, 1]], Y[idx[:, 0], idx[:, 1]]
best = np.full(len(idx), 1e9)
for (x0, x1, y0, y1) in rects:
    dx = np.maximum(np.maximum(x0 - bx, bx - x1), 0)
    dy = np.maximum(np.maximum(y0 - by, by - y1), 0)
    best = np.minimum(best, np.hypot(dx, dy))
gdist[idx[:, 0], idx[:, 1]] = best
guard = smooth((gdist - ACTOR_CLEAR) / ACTOR_FADE)
flat_hit = int(((D <= FLAT) & (guard < 1.0)).sum())
assert flat_hit == 0, 'something stands within %d cm of the flat band: %d vertices' % (ACTOR_CLEAR + ACTOR_FADE, flat_hit)
w = w * guard

# ---- skirt height: the plateau's median over the regular band, on the height grid --------------------------------
T_raw = float(np.median(Z[(D <= EASE) & ~trench0]))
H_T = int(round((T_raw - loc[2]) / UNIT + 32768.0))
T = (H_T - 32768.0) * UNIT + loc[2]

Zn = Z + (T - Z) * w
Hn = np.clip(np.round((Zn - loc[2]) / UNIT + 32768.0), 0, 65535).astype(np.uint16)
Hn[w <= 0.0] = H[w <= 0.0]
Hn[D <= FLAT] = H_T
stack = np.stack([W[n].astype(np.float64) for n in LAYERS])
base = np.array([255.0 if n == BASE else 0.0 for n in LAYERS])[:, None, None]
new = stack * (1 - w)[None] + base * w[None]
new = np.clip(np.round(new), 0, 255)
diff = 255 - new.sum(0)
bi = LAYERS.index(BASE)
touched = w > 0.0
new[bi] += np.where(touched, diff, 0)                # rounding remainder goes to the base layer
new = np.clip(new, 0, 255)
Wn = {n: np.where(touched, new[k], stack[k]).astype(np.uint8) for k, n in enumerate(LAYERS)}

# ---- strips to import ---------------------------------------------------------------------------------------------
strips = {'S': (0, 0, STRIP - 1, SY - 1), 'N': (SX - STRIP, 0, SX - 1, SY - 1),
          'W': (0, 0, SX - 1, STRIP - 1), 'E': (0, SY - STRIP, SX - 1, SY - 1)}
outside = np.ones(Z.shape, bool)
for name, (i0, j0, i1, j1) in strips.items():
    Hn[j0:j1 + 1, i0:i1 + 1].tofile(ROOT + 'rim_%s_height.r16' % name)
    for n in LAYERS:
        Wn[n][j0:j1 + 1, i0:i1 + 1].tofile(ROOT + 'rim_%s_%s.r8' % (name, n))
    outside[j0:j1 + 1, i0:i1 + 1] = False
assert not (touched & outside).any(), 'a changed vertex lies outside the imported strips'
Hn.tofile(ROOT + 'planned_height.r16')
for n in LAYERS:
    Wn[n].tofile(ROOT + 'planned_%s.r8' % n)

dz = (Hn.astype(np.float64) - H) * UNIT
changed = np.abs(dz) > 0.4
paint_changed = np.zeros(Z.shape, bool)
for n in LAYERS:
    paint_changed |= Wn[n] != W[n]


def max_slope(a, m):
    gx = np.abs(np.diff(a, axis=1)) / scl[0]
    gy = np.abs(np.diff(a, axis=0)) / scl[1]
    return float(np.degrees(np.arctan(max(gx[m[:, 1:]].max(), gy[m[1:, :]].max()))))


bandm = D <= TRENCH
Znq = (Hn.astype(np.float64) - 32768.0) * UNIT + loc[2]
rect = [loc[0] + MINX * scl[0], loc[1] + MINY * scl[1], loc[0] + MAXX * scl[0], loc[1] + MAXY * scl[1]]
plan = {'map': 'Ger_Berlin', 'method': 'rim easing to the plateau height', 'T': T, 'H_T': H_T, 'plane_z': T - DROP,
        'rect': rect, 'reach': REACH, 'tuck': TUCK, 'layers': LAYERS,
        'strips': {k: list(v) for k, v in strips.items()},
        'stats': {'height_vertices_changed': int(changed.sum()), 'paint_vertices_changed': int(paint_changed.sum()),
                  'max_raise_cm': float(dz.max()), 'max_lower_cm': float(dz.min()),
                  'farthest_change_from_rim_m': float(D[changed | paint_changed].max()),
                  'trench_vertices': int(trench0.sum()), 'guards': {'actors': n_act, 'instances': n_inst, 'mesh_cells': n_mesh},
                  'vertices_held_back_by_guards': int(((guard < 1.0) & (D <= TRENCH) & ((w_reg > 0) | (trench * w_tr > 0))).sum()),
                  'max_slope_band_before_deg': max_slope(Z, bandm), 'max_slope_band_after_deg': max_slope(Znq, bandm),
                  'max_slope_regular_after_deg': max_slope(Znq, bandm & (trench < 0.01))}}
json.dump(plan, open(ROOT + 'plan.json', 'w'), indent=1)
print(json.dumps(plan, indent=1))
print('closest changed vertex to the play area: |x| %.0f, |y| %.0f (play edge 26000 / 50000, blockers 26000-27200 / 50000-51200)' % (
    np.abs(X[changed | paint_changed]).min(), np.abs(Y[changed | paint_changed]).min()))
inner = (np.abs(X) <= 27200) & (np.abs(Y) <= 51200)
print('changed vertices inside the blockers rectangle: %d' % int(((changed | paint_changed) & inner).sum()))
print('rim line after: min %.1f max %.1f (T %.1f)' % (Znq[D == 0].min(), Znq[D == 0].max(), T))

img = np.zeros((SY, SX, 3), np.uint8)
g = np.clip((Z + 500) / 1400.0, 0, 1) * 255
img[..., 0] = img[..., 1] = img[..., 2] = g
up, down = dz > 0.4, dz < -0.4
img[up] = np.stack([np.clip(60 + dz[up] / 2, 0, 255), np.full(up.sum(), 200), np.full(up.sum(), 60)], -1)
img[down] = np.stack([np.full(down.sum(), 230), np.clip(160 + dz[down] / 2, 0, 255), np.full(down.sum(), 40)], -1)
img[(guard < 1.0) & (D <= TRENCH)] = (255, 0, 255)
Image.fromarray(img[:, ::-1].transpose(1, 0, 2)).save(ROOT + 'plan.png')
