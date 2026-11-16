"""Offline generator for the Ger_IntroMission rear areas (run with plain python + numpy).

Reads the untouched landscape export Saved/IntroRear/terrain/orig_full_* (written by rear_lib.export_region on the full
grid), shapes the ground behind the Soviet base and writes:
  terrain/rear_height.r16 + rear_<layer>.r8   the import rectangle REGION (vertex indices)
  rear_plan.json                              roads, water, ridge, station and depot data for rear_build.py
  rear_plan.png                               top-down check picture (up = +X = map rear, right = +Y)

Layout (world cm; X runs from the base's rear wall at 4139 to the map edge at 24775; ground z 143):
  hub        just east of the fuel-truck parking spot by the long camo bunker (4640,-1580)
  ridge S    "station ridge": long raised spine along Y on the map's rear edge, 8 m high, flat top; the rail line runs
             along it from the north map edge and ends in the station on the top.
  ridge D    curved ridge between the base and the concrete depot; road D crosses it through a cut saddle.
  depot      large concrete platform (airport ground tiles) south-east, in the pocket behind ridge D.
  road E     curves east between the two ponds and fades out near the east map edge.
  ponds      W1 between road S and road E, W2 between road E and the depot pocket; steep 60 degree banks under the
             waterline so no nav agent walks in (Pripyat rule: >= 2.4 m drop, no verge).
Nothing west of x = 4400 or inside the keep-out boxes (truck lane, the two mounds behind the bunker) changes.
"""
import json
import math
import os

import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/'
T = ROOT + 'terrain/'
LAYERS = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']
ORIG_GRID = (105, 75, 765, 585)       # landscape extent before the rear extension (vertex indices)
EXTEND_X = 60                         # 4 new 15-quad component columns east of the old edge (x 24900 -> 30900)
GRID = (105, 75, 765 + EXTEND_X, 585)  # full extent after rear_build.extend_landscape() added the components
REGION = (556, 75, 765 + EXTEND_X, 585)  # rectangle that gets re-imported (x >= 3999)
ORIGIN = (-51600.0, -32701.0)
GROUND = 143.0
rng = np.random.default_rng(1976)

# ---------------------------------------------------------------------------------------------------------------------
HUB = (6500.0, -700.0)
TRUCK_APRON = (4950.0, -1250.0)

RIDGE_S = {   # station ridge spine
    'spine': [(20700, 3600), (21050, 8000), (21250, 13000), (21050, 19000), (20750, 26500)],
    'top_half': 1750.0, 'height': 800.0, 'flank_w': 1500.0, 'east_flank_w': 2600.0,
}
RIDGE_D = {   # ridge between base and depot
    'spine': [(7000, -25500), (8000, -20500), (9700, -15800), (11800, -12000), (13600, -9200), (15100, -7600)],
    'top_half': 380.0, 'height': 950.0, 'flank_w': 1250.0,
}
# dry wash: shallow meandering channel out of pond W1 towards the north-west (cracked mud bed)
WASH = [(14300, 4000), (13200, 6800), (12300, 9000), (10600, 11000), (9600, 13800), (8200, 16500), (7300, 19500)]

ROADS = {
    # (x, y) control points; the generator resamples a Catmull-Rom curve through them
    'D': [TRUCK_APRON, HUB, (8500, -1700), (10300, -3900), (11700, -6800), (12500, -9300), (13600, -11500),
          (15200, -12900), (16700, -13600)],
    'E': [(8400, -1500), (10000, -300), (12400, 200), (14600, -500), (17000, -1700), (19600, -1500), (22000, -800),
          (24300, -1300)],
    'S': [(10000, -300), (11300, 2200), (13000, 4800), (15400, 6300), (17500, 6500), (18700, 7400), (19400, 8900),
          (19750, 10100)],
}
ROAD_HALF = 330.0
SADDLE_D = (12550, -9500)
DEPOT = {'centre': (18300.0, -15700.0), 'yaw': 18.0, 'tiles': (3, 2), 'tile': 2400.0}
STATION = {'track_x': [(21550, 26500), (21600, 18000), (21700, 11000), (21650, 5200)],
           'siding_x': [(21000, 17500), (21000, 12000), (21000, 7600)]}
PONDS = {
    # lobes: (dx, dy, rx, ry) blobs in the pond's frame; the shoreline is their union, domain-warped
    'W1': {'centre': (15200.0, 2600.0), 'rx': 2300.0, 'ry': 1250.0, 'yaw': 20.0, 'water_z': 40.0,
           'lobes': [(0, 0, 1900, 1050), (1500, 500, 1000, 700), (-1500, -300, 900, 650), (-600, 900, 700, 500)]},
    'W2': {'centre': (17600.0, -5700.0), 'rx': 2000.0, 'ry': 1350.0, 'yaw': -25.0, 'water_z': 40.0,
           'lobes': [(0, 0, 1600, 1100), (1200, -700, 1000, 650), (-1300, 400, 800, 700), (300, 1100, 600, 420)]},
}
KEEP_OUT = [  # (xmin, xmax, ymin, ymax): never touched
    (3900, 5250, -6200, -1000),   # truck lane + parking along the east wall
    (4900, 8100, -6300, -2200),   # the two mounds behind the bunker
]


# ---------------------------------------------------------------------------------------------------------------------
def load():
    """Original export, padded east by EXTEND_X columns that repeat the old edge (the new components)."""
    sx, sy = ORIG_GRID[2] - ORIG_GRID[0] + 1, ORIG_GRID[3] - ORIG_GRID[1] + 1
    h = np.fromfile(T + 'orig_full_height.r16', np.uint16).reshape(sy, sx)
    w = {n: np.fromfile(T + 'orig_full_%s.r8' % n, np.uint8).reshape(sy, sx) for n in LAYERS}
    h = np.pad(h, ((0, 0), (0, EXTEND_X)), mode='edge')
    w = {n: np.pad(v, ((0, 0), (0, EXTEND_X)), mode='edge') for n, v in w.items()}
    return h, w


def to_z(h):
    return (h.astype(np.float64) - 32768.0) * 100.0 / 128.0 + 100.0


def to_h(z):
    return np.clip(np.round((z - 100.0) * 1.28 + 32768.0), 0, 65535).astype(np.uint16)


def catmull(points, step=100.0):
    p = [np.array(points[0]) * 2 - np.array(points[1])] + [np.array(q, float) for q in points] + \
        [np.array(points[-1]) * 2 - np.array(points[-2])]
    out = []
    for k in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[k - 1], p[k], p[k + 1], p[k + 2]
        n = max(2, int(np.linalg.norm(p2 - p1) / step))
        for t in np.linspace(0, 1, n, endpoint=False):
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(p[-2])
    return np.array(out)


def dist_to_poly(X, Y, poly):
    """Distance field + arc-length parameter of the nearest point (poly = Nx2 dense polyline)."""
    best = np.full(X.shape, 1e12)
    s_at = np.zeros(X.shape)
    acc = 0.0
    for a, b in zip(poly[:-1], poly[1:]):
        d = b - a
        L2 = float(d @ d)
        if L2 <= 0:
            continue
        t = np.clip(((X - a[0]) * d[0] + (Y - a[1]) * d[1]) / L2, 0, 1)
        px, py = a[0] + t * d[0], a[1] + t * d[1]
        dd = (X - px) ** 2 + (Y - py) ** 2
        m = dd < best
        best = np.where(m, dd, best)
        s_at = np.where(m, acc + t * math.sqrt(L2), s_at)
        acc += math.sqrt(L2)
    return np.sqrt(best), s_at


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def value_noise(X, Y, scale, seed):
    r = np.random.default_rng(seed)
    gx = np.arange(X.min() // scale - 2, X.max() // scale + 3)
    gy = np.arange(Y.min() // scale - 2, Y.max() // scale + 3)
    tab = r.random((len(gy), len(gx)))
    fx = X / scale - gx[0]
    fy = Y / scale - gy[0]
    ix, iy = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = smooth(fx - ix), smooth(fy - iy)
    v00, v10 = tab[iy, ix], tab[iy, ix + 1]
    v01, v11 = tab[iy + 1, ix], tab[iy + 1, ix + 1]
    return (v00 * (1 - tx) + v10 * tx) * (1 - ty) + (v01 * (1 - tx) + v11 * tx) * ty


def fbm(X, Y, scale, seed, octaves=4):
    v, a, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        v = v + a * value_noise(X, Y, scale / (2 ** o), seed + o)
        tot += a
        a *= 0.5
    return v / tot


def box_blur(a, r):
    if r <= 0:
        return a
    k = 2 * r + 1
    p = np.pad(a, r, mode='edge')
    c = p.cumsum(0)
    c = np.vstack([np.zeros((1, c.shape[1])), c])
    v = (c[k:] - c[:-k]) / k
    c = v.cumsum(1)
    c = np.hstack([np.zeros((c.shape[0], 1)), c])
    return (c[:, k:] - c[:, :-k]) / k


def pond_sd(X, Y, pond, seed):
    """Approximate signed distance (cm, negative inside) to a shoreline made of the pond's lobes, domain-warped."""
    wx = (fbm(X, Y, 1400.0, seed) - 0.5) * 700.0
    wy = (fbm(X, Y, 1400.0, seed + 7) - 0.5) * 700.0
    cx, cy = pond['centre']
    a = math.radians(pond['yaw'])
    dx, dy = X + wx - cx, Y + wy - cy
    u = dx * math.cos(a) + dy * math.sin(a)
    v = -dx * math.sin(a) + dy * math.cos(a)
    sd = np.full(X.shape, 1e9)
    for (lx, ly, rx, ry) in pond['lobes']:
        r = np.sqrt(((u - lx) / rx) ** 2 + ((v - ly) / ry) ** 2)
        sd = np.minimum(sd, (r - 1.0) * min(rx, ry))
    return sd


# ---------------------------------------------------------------------------------------------------------------------
def build():
    h, w = load()
    sy, sx = h.shape
    I, J = np.meshgrid(np.arange(sx) + GRID[0], np.arange(sy) + GRID[1])
    X = ORIGIN[0] + I * 100.0
    Y = ORIGIN[1] + J * 100.0
    Z0 = to_z(h)
    Z = Z0.copy()
    valid = sum(w[n].astype(int) for n in LAYERS) > 0
    in_region = (I >= REGION[0]) & (I <= REGION[2]) & (J >= REGION[1]) & (J <= REGION[3])
    keep = np.zeros_like(in_region)
    for (x0, x1, y0, y1) in KEEP_OUT:
        keep |= (X >= x0) & (X <= x1) & (Y >= y0) & (Y <= y1)
    work = in_region & ~keep & valid & (X >= 4400)
    # soft blend weight towards the keep-out boxes / west edge so nothing steps
    edge = np.ones_like(Z)
    for (x0, x1, y0, y1) in KEEP_OUT:
        dx = np.maximum(np.maximum(x0 - X, X - x1), 0)
        dy = np.maximum(np.maximum(y0 - Y, Y - y1), 0)
        edge = np.minimum(edge, smooth(np.hypot(dx, dy) / 800.0))
    edge = np.minimum(edge, smooth((X - 4400) / 900.0))

    warp1 = fbm(X, Y, 3000.0, 11)
    warp2 = fbm(X, Y, 900.0, 12)
    micro = fbm(X, Y, 1600.0, 13) - 0.5

    # ---- base: gentle undulation of the rear plain (+-35 cm), lower towards the ponds
    add = 70.0 * (fbm(X, Y, 5000.0, 21) - 0.5)
    # beyond the map-edge blocker (x 24775) the ground rises into low rolling hills, so the camera at the playable
    # edge looks onto terrain instead of the end of the landscape
    add = add + 700.0 * smooth((X - 25300.0) / 3800.0) * (0.55 + 0.7 * fbm(X, Y, 2600.0, 22))

    # ---- station ridge
    sp = catmull(RIDGE_S['spine'], 100.0)
    dS, sS = dist_to_poly(X, Y, sp)
    side = np.sign(X - np.interp(Y, sp[:, 1], sp[:, 0]))          # +1 east of spine (towards map edge)
    flank = np.where(side > 0, RIDGE_S['east_flank_w'], RIDGE_S['flank_w'])
    top = RIDGE_S['top_half'] * (1.0 + 0.18 * (warp1 - 0.5))
    prof_s = 1.0 - smooth((dS - top) / flank)
    # rounded south nose: fade the height over the first 1500 cm of the spine
    nose = smooth(sS / 1500.0)
    h_s = RIDGE_S['height'] * prof_s * (0.25 + 0.75 * nose) + 60.0 * micro * prof_s
    # ---- depot ridge
    dp = catmull(RIDGE_D['spine'], 100.0)
    dD, sD = dist_to_poly(X, Y, dp)
    lenD = sD.max()
    endfade = smooth((lenD - sD) / 2200.0)
    topd = RIDGE_D['top_half'] * (1.0 + 0.6 * (warp1 - 0.5))
    prof_d = 1.0 - smooth((dD - topd) / (RIDGE_D['flank_w'] * (1.0 + 0.35 * (warp2 - 0.5))))
    hgtD = RIDGE_D['height'] * (0.85 + 0.3 * warp1)
    h_d = hgtD * prof_d * endfade + 70.0 * micro * prof_d
    # saddle where road D crosses
    sad = np.hypot(X - SADDLE_D[0], Y - SADDLE_D[1])
    h_d = h_d * (1.0 - 0.62 * (1.0 - smooth((sad - 600.0) / 1800.0)))

    Zn = GROUND + add + np.maximum(h_s, h_d)

    # ---- depot pad: flatten to 150 under the tiles + 6 m apron
    dc = DEPOT['centre']
    a = math.radians(DEPOT['yaw'])
    u = (X - dc[0]) * math.cos(a) + (Y - dc[1]) * math.sin(a)
    v = -(X - dc[0]) * math.sin(a) + (Y - dc[1]) * math.cos(a)
    hu = DEPOT['tiles'][0] * DEPOT['tile'] / 2 + 600.0
    hv = DEPOT['tiles'][1] * DEPOT['tile'] / 2 + 600.0
    pad_d = np.hypot(np.maximum(np.abs(u) - hu, 0), np.maximum(np.abs(v) - hv, 0))
    pad_w = 1.0 - smooth(pad_d / 1500.0)
    Zn = Zn * (1 - pad_w) + 140.0 * pad_w   # tiles' top sits at 146

    # ---- station top: flatten the forecourt + track bed (z = ridge top)
    st_top = GROUND + RIDGE_S['height']
    bed = (1.0 - smooth((dS - (top - 200.0)) / 400.0)) * smooth((sS - 1200.0) / 900.0)
    Zn = Zn * (1 - bed) + (st_top + 0.0) * bed

    # ---- dry wash out of W1: 60 cm deep meandering channel
    wash = catmull(WASH, 100.0)
    dW, sW = dist_to_poly(X, Y, wash)
    wash_w = 1.0 - smooth((dW - 220.0 * (0.7 + 0.6 * warp2)) / 520.0)
    wash_depth = 65.0 * smooth(sW / 600.0) * smooth((sW.max() - sW) / 2500.0)
    Zn = Zn - wash_depth * wash_w

    # ---- ponds: natural basins (ground eases down ~1 m over 22 m to the shore), then a 60 deg underwater bank
    # dropping >= 2.6 m (Pripyat rule) so no nav agent walks in; floor 2.8 m under water
    pond_mask = {}
    for k, (name, pd) in enumerate(PONDS.items()):
        r_px = pond_sd(X, Y, pd, 41 + k * 13)
        wz = pd['water_z']
        bank = wz + 14.0 + (Zn - wz - 14.0) * smooth(r_px / 2200.0) ** 0.85
        basin = wz - 22.0 - np.clip(-r_px, 0, 160) * 1.73
        basin = np.maximum(basin, wz - 280.0)
        Zn = np.where(r_px > 0, np.minimum(Zn, bank), basin)
        pond_mask[name] = r_px

    # ---- roads: graded corridor (cut/fill to a smoothed centre profile)
    road_polys = {}
    for name, pts in ROADS.items():
        poly = catmull(pts, 100.0)
        road_polys[name] = poly
    # longitudinal profile from Zn sampled on the line, smoothed; the station road climbs evenly
    def sample(arr, px, py):
        fi = (px - ORIGIN[0]) / 100.0 - GRID[0]
        fj = (py - ORIGIN[1]) / 100.0 - GRID[1]
        return arr[np.clip(np.round(fj).astype(int), 0, sy - 1), np.clip(np.round(fi).astype(int), 0, sx - 1)]
    profiles = {}
    for name, poly in road_polys.items():
        zc = sample(Zn, poly[:, 0], poly[:, 1])
        k = 12
        zs = np.convolve(np.pad(zc, k, mode='edge'), np.ones(2 * k + 1) / (2 * k + 1), mode='valid')
        if name == 'S':
            seg = np.r_[0, np.cumsum(np.hypot(np.diff(poly[:, 0]), np.diff(poly[:, 1])))]
            # ramp: from the foot (s0) linearly to the station top at the end
            s_end = seg[-1]
            s0 = s_end - 4600.0
            ramp = np.clip((seg - s0) / (s_end - 400.0 - s0), 0, 1)
            zs = np.where(seg >= s0, zs[np.searchsorted(seg, s0)] * (1 - ramp) + st_top * ramp, zs)
        if name == 'D':
            zs[:3] = zc[:3]
        profiles[name] = zs
    for name, poly in road_polys.items():
        d, s = dist_to_poly(X, Y, poly)
        seg = np.r_[0, np.cumsum(np.hypot(np.diff(poly[:, 0]), np.diff(poly[:, 1])))]
        zr = np.interp(s, seg, profiles[name])
        core = 1.0 - smooth((d - ROAD_HALF) / 900.0)
        if name == 'S':   # the ramp cuts into the flank: wider falloff on the uphill side
            core = 1.0 - smooth((d - ROAD_HALF) / 1300.0)
        Zn = Zn * (1 - core) + zr * core

    # ---- final blend: only inside the work area, eased into the keep-outs / west edge
    wgt = np.where(work, edge, 0.0)
    Z = Z0 * (1 - wgt) + Zn * wgt
    Z = np.where(work, Z, Z0)

    # ===== paint ==================================================================================================
    W = {n: w[n].astype(np.float64) for n in LAYERS}
    gzx, gzy = np.gradient(Z, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gzx, gzy)))
    rise = Z - GROUND
    # sun from the light: approximate aspect term (south-west lit, see Sun actor); lit = slope facing -X,-Y
    lit = (-gzx * 0.6 - gzy * 0.8) / (np.hypot(gzx, gzy) + 1e-6)
    n1 = fbm(X, Y, 700.0, 31)
    n2 = fbm(X, Y, 2200.0, 32)
    tgt = {n: np.zeros_like(Z) for n in LAYERS}
    tgt['Sand'][:] = 1.0
    # ridges: pale pebbles on crests and lit flanks, red gravel on lee flanks
    crest = smooth((rise - 350.0) / 300.0) * smooth((n1 - 0.25) / 0.35)
    flankm = smooth((slope - 9.0) / 10.0)
    tgt['DesertRockDark'] += 0.85 * crest + 0.6 * flankm * smooth(lit * 2.0) * smooth((n2 - 0.3) / 0.3)
    tgt['DesertGravel'] += 0.75 * flankm * smooth(-lit * 2.0 + 0.4)
    tgt['ScorchSandyStone'] += 0.55 * smooth((slope - 24.0) / 10.0) * smooth((n1 - 0.45) / 0.3)
    # wet margins around the ponds (dark cracked mud) + bed
    for name, r_px in pond_mask.items():
        wet = 1.0 - smooth((r_px - 250.0 - 500.0 * n1) / (700.0 + 900.0 * n2))
        tgt['Weedy'] += 3.0 * wet
        tgt['DesertGravel'] += 0.35 * (1.0 - smooth((r_px - 1000.0) / 1400.0)) * smooth((n1 - 0.4) / 0.3)
    # dry wash bed: cracked mud with gravel banks
    tgt['Weedy'] += 2.2 * (1.0 - smooth((dW - 150.0) / 300.0)) * smooth(sW / 400.0)
    tgt['DesertGravel'] += 0.8 * (1.0 - smooth((dW - 350.0) / 400.0)) * smooth((n1 - 0.3) / 0.3)
    # road wear: gravel band along the roads (the two-track ribbons draw the ruts)
    for name, poly in road_polys.items():
        d, s = dist_to_poly(X, Y, poly)
        tgt['DesertGravel'] += 0.55 * (1.0 - smooth((d - 250.0) / 700.0)) * (0.6 + 0.4 * n1)
    # depot: concrete apron margin and spill
    tgt['ScorchConcrete'] += 4.0 * (1.0 - smooth((pad_d - 100.0) / 500.0)) + 1.2 * pad_w * smooth((n1 - 0.3) / 0.3)
    tgt['DesertRockDark'] += 0.6 * (1.0 - smooth(pad_d / 2500.0)) * smooth((n2 - 0.45) / 0.3)
    # station forecourt / track bed: ballast grey + concrete under the station
    tgt['DesertRockDark'] += 1.4 * bed
    tgt['ScorchSandyStone'] += 0.5 * bed * smooth((n1 - 0.55) / 0.25)
    # normalise targets to 255
    tot = sum(tgt[n] for n in LAYERS)
    newW = {n: tgt[n] / tot * 255.0 for n in LAYERS}
    # paint only where something changed (feature influence) so the user's existing paint stays elsewhere
    infl = np.clip(np.abs(Z - Z0) / 40.0, 0, 1)
    for poly in road_polys.values():
        d, s = dist_to_poly(X, Y, poly)
        infl = np.maximum(infl, 1.0 - smooth((d - 300.0) / 900.0))
    infl = np.maximum(infl, 1.0 - smooth(pad_d / 2600.0))
    infl = np.maximum(infl, 1.0 - smooth((dW - 400.0) / 600.0))
    for r_px in pond_mask.values():
        infl = np.maximum(infl, 1.0 - smooth((r_px - 1500.0) / 1200.0))
    infl = box_blur(infl, 4) * work
    outW = {n: W[n] * (1 - infl) + newW[n] * infl for n in LAYERS}
    # exact 255 sums
    stack = np.stack([outW[n] for n in LAYERS])
    stack = np.clip(np.round(stack), 0, 255)
    diff = 255 - stack.sum(0)
    dom = stack.argmax(0)
    for k in range(len(LAYERS)):
        stack[k] += np.where(dom == k, diff, 0)
    stack = np.where(valid[None], stack, 0)

    # ===== write region =============================================================================================
    i0, j0 = REGION[0] - GRID[0], REGION[1] - GRID[1]
    i1, j1 = REGION[2] - GRID[0], REGION[3] - GRID[1]
    to_h(Z)[j0:j1 + 1, i0:i1 + 1].tofile(T + 'rear_height.r16')
    for k, n in enumerate(LAYERS):
        stack[k].astype(np.uint8)[j0:j1 + 1, i0:i1 + 1].tofile(T + 'rear_%s.r8' % n)
    # full arrays for the build script's ground lookups
    to_h(Z).tofile(T + 'rear_full_height.r16')

    shores = {}
    for name, pd in PONDS.items():
        cx, cy = pd['centre']
        pts = []
        for k in range(72):
            ang = 2 * math.pi * k / 72
            last = (cx, cy)
            for rr in range(0, 4000, 50):
                px, py = cx + rr * math.cos(ang), cy + rr * math.sin(ang)
                i = int(round((px - ORIGIN[0]) / 100.0)) - GRID[0]
                j = int(round((py - ORIGIN[1]) / 100.0)) - GRID[1]
                if pond_mask[name][j, i] > 0:
                    break
                last = (px, py)
            pts.append([float(last[0]), float(last[1])])
        shores[name] = pts
    plan = {
        'shores': shores,
        'wash': [list(map(float, p)) for p in wash[::8]],
        'region': REGION, 'grid': GRID, 'hub': HUB,
        'roads': {k: [[float(a), float(b)] for a, b in v[::6]] + [[float(v[-1][0]), float(v[-1][1])]]
                  for k, v in road_polys.items()},
        'ponds': {k: dict(v) for k, v in PONDS.items()},
        'depot': DEPOT, 'station': STATION, 'station_top_z': st_top,
        'ridge_s_spine': [list(map(float, p)) for p in sp[::10]],
        'ridge_d_spine': [list(map(float, p)) for p in dp[::10]],
    }
    with open(ROOT + 'rear_plan.json', 'w') as f:
        json.dump(plan, f, indent=1)

    # ===== stats + picture ===========================================================================================
    changed = np.abs(to_h(Z).astype(int) - h.astype(int)) > 0
    outside = changed & ~in_region
    print('changed vertices', int(changed.sum()), 'outside region', int(outside.sum()),
          'in keep-out', int((changed & keep).sum()))
    print('z range in region %.0f .. %.0f' % (Z[in_region].min(), Z[in_region].max()))
    mx = slope[work & (Z > 60)].max()
    print('max slope (dry ground) %.1f deg' % mx)
    for name, poly in road_polys.items():
        zr = profiles[name]
        seg = np.hypot(np.diff(poly[:, 0]), np.diff(poly[:, 1]))
        g = np.degrees(np.arctan(np.abs(np.diff(zr)) / np.maximum(seg, 1)))
        print('road %s length %.0f m, max grade %.1f deg, z %.0f..%.0f' % (name, seg.sum() / 100, g.max(), zr.min(), zr.max()))
    from PIL import Image, ImageDraw
    cols = {'Sand': (205, 150, 110), 'DesertRockDark': (225, 215, 195), 'Weedy': (70, 58, 48),
            'DesertGravel': (165, 85, 65), 'ScorchSandyStone': (45, 45, 45), 'ScorchConcrete': (120, 120, 118)}
    rgb = sum((stack[k] / 255.0)[..., None] * np.array(cols[n]) for k, n in enumerate(LAYERS))
    lx = -gzx / np.sqrt(gzx ** 2 + gzy ** 2 + 1)
    ly = -gzy / np.sqrt(gzx ** 2 + gzy ** 2 + 1)
    shade = np.clip(0.75 + 1.6 * (lx * -0.5 + ly * -0.6), 0.35, 1.25)
    rgb = rgb * shade[..., None]
    for name, pd in PONDS.items():
        rgb[(pond_mask[name] <= 0)] = (40, 70, 95)
    crop = rgb[:, REGION[0] - GRID[0] - 30:]
    img = np.transpose(crop, (1, 0, 2))[::-1].clip(0, 255).astype(np.uint8)
    im = Image.fromarray(img).resize((img.shape[1] * 2, img.shape[0] * 2))
    d = ImageDraw.Draw(im)
    x_img0 = REGION[0] - GRID[0] - 30

    def P(x, y):
        i = (x - ORIGIN[0]) / 100 - GRID[0] - x_img0
        j = (y - ORIGIN[1]) / 100 - GRID[1]
        return j * 2, (img.shape[0] - 1 - i) * 2
    for name, poly in road_polys.items():
        d.line([P(*p) for p in poly], fill=(255, 240, 60), width=2)
        d.text(P(*poly[-1]), 'road ' + name, fill=(255, 255, 0))
    for seg_name in ('track_x', 'siding_x'):
        pts = [(a, b) for a, b in STATION[seg_name]]
        d.line([P(a, b) for a, b in pts], fill=(30, 30, 30), width=3)
    corners = []
    for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        uu, vv = su * (hu - 600), sv * (hv - 600)
        corners.append((dc[0] + uu * math.cos(a) - vv * math.sin(a), dc[1] + uu * math.sin(a) + vv * math.cos(a)))
    d.polygon([P(*c) for c in corners], outline=(255, 255, 255))
    for x in range(4000, 25001, 2000):
        d.text(P(x, -25000), str(x), fill=(255, 255, 255))
    for y in range(-24000, 25001, 4000):
        d.text(P(4100, y), str(y), fill=(120, 255, 255))
    im.save(ROOT + 'rear_plan.png')
    print('plan picture', im.size)


if __name__ == '__main__':
    build()
