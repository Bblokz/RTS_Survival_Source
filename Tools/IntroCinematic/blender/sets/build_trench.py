"""Intro cinematic - 1945 field trench set: SM_Intro_Trench, SM_Intro_TrenchProps and the mould meshes.

Run:  blender.exe --factory-startup -b -P build_trench.py      (after make_set_textures.py)

Frame (metres here, cm in the FBX): trench runs along Y, origin on the floor centreline at mid length,
floor z = 0, ground z = +1.9. Fire step on the +X (east) wall. Both ends turn 90 degrees into a short
closed stub (north end turns west, south end turns east).
The earth is one height field z = H(x, y) on a tensor grid; everything else is seated on that grid.
"""
import os
import sys
import math
import random
import numpy as np
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sets_common as C
from sets_common import sn, sstep, obox, frame, yaw_frame, wood_uv, revolve, tube, smooth_path, vnorm, vcross

DEPTH, BAT = 1.9, 0.30
MAIN = (-0.85, 0.85, -11.35, 11.35)
NST = (-3.9, 0.85, 9.65, 11.35)          # north stub (turns west)
SST = (-0.85, 3.9, -11.35, -9.65)        # south stub (turns east)
STEP = (0.85, 1.35, -8.0, 5.0)           # fire step footprint
SEXP = (0.85, 1.80, -8.0, 5.0)
STEP_H = 0.45
REVET_W = [(-9.0, -1.5), (5.6, 8.6)]     # west wall revetted stretches (y0, y1), posts every 1.5 m
REVET_E = [(-6.5, -0.5)]                 # east wall (behind the fire step)
LADDER_Y = -1.05
SCRAPES = [(5.5, -4.0, 1.4, 0.55), (-6.0, 3.0, 1.2, 0.5), (7.5, 8.0, 1.6, 0.6), (-8.0, -9.0, 1.5, 0.6),
           (-4.5, 14.5, 1.3, 0.5), (4.0, -15.5, 1.2, 0.45)]
VB0, VB1 = 520 / 2048.0, 2040 / 2048.0   # soil band of the earth atlas (see make_set_textures.py)
CRATE_A = (-0.52, 2.2, 94.0)             # x, y, yaw (closed crate)
CRATE_B = (-0.50, 3.0, 82.0)             # open crate
HELMETS = [(1.08, 0.9), (1.14, 1.5)]
rng = random.Random(1945)


def rd(x, y, x0, x1, y0, y1):
    dx = np.maximum(x0 - x, x - x1)
    dy = np.maximum(y0 - y, y - y1)
    return np.minimum(np.maximum(dx, dy), 0) + np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))


def revet_mask(x, y):
    m = 0.0
    for (a, b) in REVET_W:
        m = m + sstep(y, a - 0.25, a) * (1 - sstep(y, b, b + 0.25)) * (x < 0) * (np.abs(x) < 2.0)
    for (a, b) in REVET_E:
        m = m + sstep(y, a - 0.25, a) * (1 - sstep(y, b, b + 0.25)) * (x > 0) * (np.abs(x) < 2.5)
    return np.clip(m, 0, 1)


def UND(x, y):
    calm = sstep(y, 1.3, 1.8) * (1 - sstep(y, 3.5, 4.0)) * (x < 0)      # straighter wall behind the crates
    return (0.075 * sn(x, y, 101, 0.42, 2) + 0.03 * sn(x, y, 102, 1.3, 2)) * (1 - 0.75 * calm)


def H(x, y):
    """Analytic ground height (m)."""
    x = np.asarray(x, float)
    y = np.asarray(y, float)
    dF = np.minimum(np.minimum(rd(x, y, *MAIN), rd(x, y, *NST)), rd(x, y, *SST))
    dF2 = np.minimum(dF, rd(x, y, *STEP))
    rv = revet_mask(x, y)
    und = UND(x, y)
    dw = dF2 + und * (1 - 0.75 * rv) - 0.05 * rv
    t = np.clip(dw / BAT, 0, 1)
    p = np.clip(0.95 + 0.42 * sn(x, y, 103, 0.5, 2), 0.55, 1.7)
    p = p * (1 - rv) + 0.95 * rv
    wall = DEPTH * t ** p
    stepm = np.clip(1 - rd(x, y, *SEXP) / 0.05, 0, 1)
    base = STEP_H * np.clip((dF + 0.3 * und) / 0.1, 0, 1) * stepm
    floor = 0.02 * sn(x, y, 104, 0.8, 2) - 0.05 * np.exp(-(((x + 0.2) / 0.55) ** 2 + ((y + 3.2) / 1.4) ** 2))
    z = np.maximum(base, wall) + floor * (1 - t)
    da = np.maximum(dw - BAT, 0)
    spoil = np.clip(0.36 + 0.32 * sn(x, y, 105, 0.16, 2), 0.04, None) * np.exp(-((da - 2.3) / 1.0) ** 2)
    roll = 0.12 * sn(x, y, 106, 0.07, 2) * sstep(da, 0.9, 4.0)
    rough = 0.03 * sn(x, y, 107, 0.45, 2) * sstep(da, 0.7, 1.6)
    scr = 0.0
    for (sx, sy, R, dep) in SCRAPES:
        r = np.hypot(x - sx, y - sy)
        scr = scr - dep * np.exp(-(r / R) ** 2) + 0.16 * np.exp(-((r - 1.3 * R) / (0.4 * R)) ** 2)
    edge = 0.5 * sstep(np.maximum(np.abs(x) - 11.0, np.abs(y) - 17.0), 0.0, 3.0)
    z = z + (spoil * sstep(da, 0.8, 1.5) + roll + rough + scr * sstep(da, 0.9, 2.0) + edge) * (t >= 1.0)
    return z


def grid_lines():
    def ar(a, b, s):
        return list(np.round(np.arange(a, b + 1e-6, s), 4))
    coarse = [5.0, 5.9, 7.0, 8.2, 9.5, 10.9, 12.4, 14.0]
    xs = [-c for c in coarse] + ar(-4.25, -1.25, 0.2) + [-1.15, -1.05, -0.95] + ar(-0.85, 0.85, 0.17) \
        + ar(0.95, 1.85, 0.1) + ar(2.05, 4.45, 0.2) + coarse
    yc = [12.0, 12.3, 12.8, 13.8, 15.0, 16.5, 18.0, 20.0]
    stub = ar(9.25, 9.75, 0.1) + ar(9.95, 11.15, 0.2) + ar(11.25, 11.75, 0.1)
    ys = [-c for c in yc] + [-s for s in stub] + stub + yc + ar(-9.0, 9.0, 0.4)
    ys += ar(-8.3, -7.9, 0.1) + ar(4.9, 5.3, 0.1) + [-6.0, -3.0, 0.0, 3.0, 6.0]

    def clean(v):
        v = sorted(set(v))
        out = [v[0]]
        for a in v[1:]:
            if a - out[-1] > 0.045:
                out.append(a)
        return np.array(out)
    return clean(xs), clean(ys)


XS, YS = grid_lines()
XG, YG = np.meshgrid(XS, YS)
ZG = H(XG, YG)


def Hm(x, y):
    """Height of the actual mesh grid (bilinear), used to seat everything else."""
    x = np.asarray(x, float)
    y = np.asarray(y, float)
    i = np.clip(np.searchsorted(XS, x) - 1, 0, len(XS) - 2)
    j = np.clip(np.searchsorted(YS, y) - 1, 0, len(YS) - 2)
    tx = np.clip((x - XS[i]) / (XS[i + 1] - XS[i]), 0, 1)
    ty = np.clip((y - YS[j]) / (YS[j + 1] - YS[j]), 0, 1)
    return (ZG[j, i] * (1 - tx) + ZG[j, i + 1] * tx) * (1 - ty) + (ZG[j + 1, i] * (1 - tx) + ZG[j + 1, i + 1] * tx) * ty


def Hs(x, y):
    """Surface the mould creeps on: the earth plus the duckboard walkway."""
    x = np.asarray(x, float)
    y = np.asarray(y, float)
    duck = 0.095 * (1 - sstep(np.abs(x), 0.30, 0.37)) * (np.abs(y) < 9.0)
    return Hm(x, y) + duck


def surf_n(x, y, eps=0.04):
    gx = (Hs(x + eps, y) - Hs(x - eps, y)) / (2 * eps)
    gy = (Hs(x, y + eps) - Hs(x, y - eps)) / (2 * eps)
    n = np.array([-float(gx), -float(gy), 1.0])
    return n / np.linalg.norm(n)


# =============================================================================== trench mesh
def build_earth(acc):
    """Height-field grid, split into triangles here so that every triangle gets the planar
    projection of its OWN dominant axis (a per-quad choice left flat lip triangles with zero UV area)."""
    ny, nx = ZG.shape
    verts = [(float(XG[j, i]), float(YG[j, i]), float(ZG[j, i])) for j in range(ny) for i in range(nx)]
    faces, uvs = [], []
    kv = VB1 - VB0

    def tri_uv(ids, xq, yq):
        P = [np.array(verts[k]) for k in ids]
        n = np.cross(P[1] - P[0], P[2] - P[0])
        ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
        if az >= ax and az >= ay:
            if abs(xq) < 0.85 and -7.0 < yq < 1.0:                        # puddle strip (8 m x 2 m)
                return [((p[1] + 7.0) / 8.0, (p[0] + 1.0) / 2.0 * 0.25) for p in P]
            y0 = 3.0 * math.floor(yq / 3.0)
            return [(p[0] / 4.0, VB0 + (p[1] - y0) / 3.0 * kv) for p in P]
        if ax >= ay:
            return [(p[1] / 4.0, VB0 + min(max((p[2] + 0.2) / 3.0, 0.0), 1.0) * kv) for p in P]
        return [(p[0] / 4.0, VB0 + min(max((p[2] + 0.2) / 3.0, 0.0), 1.0) * kv) for p in P]
    for j in range(ny - 1):
        for i in range(nx - 1):
            a, b, c, d = j * nx + i, j * nx + i + 1, (j + 1) * nx + i + 1, (j + 1) * nx + i
            xq, yq = (XS[i] + XS[i + 1]) / 2, (YS[j] + YS[j + 1]) / 2
            if abs(verts[a][2] - verts[c][2]) <= abs(verts[b][2] - verts[d][2]):
                tris = ((a, b, c), (a, c, d))
            else:
                tris = ((a, b, d), (b, c, d))
            for t in tris:
                faces.append(t)
                uvs.append(tri_uv(t, xq, yq))
    acc.mesh(verts, faces, uvs, 'Earth')


def bag(acc, c, yaw, L=0.56, W=0.30, T=0.14):
    fr = yaw_frame(c, yaw + rng.uniform(-7, 7), rng.uniform(-3, 3), rng.uniform(-5, 5))
    ts = [-0.5, -0.33, 0.33, 0.5]
    sc = [(0.6, 0.42), (1.0, 1.0), (1.0, 1.0), (0.6, 0.42)]
    verts = []
    for t, (sw, st) in zip(ts, sc):
        for k in range(6):
            a = math.pi / 6 + k * math.pi / 3
            cy, sz = math.cos(a), math.sin(a)
            y = W / 2 * sw * math.copysign(abs(cy) ** 0.5, cy) + rng.uniform(-0.008, 0.008)
            z = T / 2 * st * math.copysign(abs(sz) ** 0.6, sz) - 0.02 * abs(t) + rng.uniform(-0.006, 0.006)
            verts.append(fr(t * L, y, z))
    u0, v0 = rng.random(), rng.random()
    faces, uvs = [], []
    for r in range(3):
        for k in range(6):
            k2 = (k + 1) % 6
            faces.append((r * 6 + k, r * 6 + k2, (r + 1) * 6 + k2, (r + 1) * 6 + k))
            ua, ub = u0 + ts[r] * L, u0 + ts[r + 1] * L
            va, vb = v0 + k * 0.14, v0 + (k + 1) * 0.14
            uvs.append([(ua, va), (ua, vb), (ub, vb), (ub, va)])
    for r, rev in ((0, True), (3, False)):
        ids = [r * 6 + k for k in range(6)]
        if rev:
            ids.reverse()
        faces.append(tuple(ids))
        uvs.append([(u0 + 0.1 * math.cos(k), v0 + 0.06 * math.sin(k)) for k in range(6)])
    acc.mesh(verts, faces, uvs, 'Sandbag')


def build_parapet(acc):
    o = 0.62
    lines = [
        ((-0.85 - o, -11.35 - o + 0.3), (-0.85 - o, 9.65 - o - 0.3), 'W'),
        ((-3.9 - o + 0.3, 9.65 - o), (-0.85 - o, 9.65 - o), ''),
        ((-3.9 - o, 9.65 - o), (-3.9 - o, 11.35 + o), ''),
        ((-3.9 - o + 0.3, 11.35 + o), (0.85 + o - 0.3, 11.35 + o), ''),
        ((0.85 + o, 11.35 + o), (0.85 + o, 5.0 + o + 0.25), ''),
        ((0.85 + o, 5.0 + o), (1.35 + o, 5.0 + o), ''),
        ((1.35 + o, 5.0 + o - 0.3), (1.35 + o, -8.0 - o + 0.3), ''),
        ((0.85 + o, -8.0 - o), (1.35 + o, -8.0 - o), ''),
        ((0.85 + o, -8.0 - o - 0.25), (0.85 + o, -9.65 + o + 0.3), ''),
        ((0.85 + o, -9.65 + o), (3.9 + o - 0.3, -9.65 + o), ''),
        ((3.9 + o, -9.65 + o), (3.9 + o, -11.35 - o), ''),
        ((3.9 + o - 0.3, -11.35 - o), (-0.85 - o + 0.3, -11.35 - o), ''),
    ]
    L = 0.56
    n = 0
    for (p0, p1, tag) in lines:
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        ln = math.hypot(dx, dy)
        ux, uy = dx / ln, dy / ln
        yaw = math.degrees(math.atan2(uy, ux))
        for row in range(3):
            s = (row % 2) * L / 2 + 0.28
            while s < ln - 0.2 + (row % 2) * 0.2:
                x, y = p0[0] + ux * s, p0[1] + uy * s
                s += L + rng.uniform(-0.01, 0.03)
                hi = float(sn(x, y, 201, 0.14, 2))                 # where the parapet is 3 bags high
                if row == 2 and hi < 0.15:
                    continue
                if row == 1 and hi < -1.35:
                    continue
                if tag == 'W' and abs(y - LADDER_Y) < 0.42:        # gap where the ladder goes over
                    continue
                px, py = x - uy * rng.uniform(-0.03, 0.03) - uy * 0.0, y + ux * rng.uniform(-0.03, 0.03)
                z = float(Hm(px, py)) + 0.062 + row * 0.122
                bag(acc, (px, py, z), yaw)
                n += 1
    return n


def build_revetment(acc):
    a = math.atan(BAT / DEPTH)

    def wall(xf, out, base, y0, y1):
        az = (out * math.sin(a), 0.0, math.cos(a))
        ax = (0.0, 1.0, 0.0)
        ay = vcross(az, ax)

        def wx(z):
            return xf + out * BAT * z / DEPTH
        nb = int(round((y1 - y0) / 1.5))
        for k in range(nb + 1):
            yp = y0 + 1.5 * k
            zb, zt = base - 0.22, 1.93 + rng.uniform(0.0, 0.28)
            zc = (zb + zt) / 2
            lean = rng.uniform(-0.012, 0.012)
            az2 = vnorm((az[0] + lean, rng.uniform(-0.02, 0.02), az[2]))
            ay2 = vnorm(vcross(az2, ax))
            ax2 = vcross(ay2, az2)
            obox(acc, frame((wx(zc) - out * 0.045, yp, zc), ax2, ay2, az2), (0.10, 0.10, (zt - zb) / math.cos(a)), 'Wood')
        for k in range(nb):
            ya = y0 + 1.5 * k
            row = 0
            z = base + 0.11
            while z < 1.78:
                top = z > 1.4
                r = rng.random()
                zz = z + rng.uniform(-0.006, 0.006)
                c = (wx(zz) + out * (0.0175 + rng.uniform(-0.004, 0.004)), ya + 0.75, zz)
                z += 0.165
                row += 1
                if r < (0.34 if top else 0.12):
                    continue                                        # missing plank
                if r < (0.5 if top else 0.24):                      # broken plank hanging from one post
                    d = rng.choice((-1, 1))
                    ln = rng.uniform(0.45, 1.05)
                    droop = math.tan(math.radians(rng.uniform(3, 11)))
                    axb = vnorm((-az[0] * droop, d * 1.0, -az[2] * droop))
                    azb = vnorm(vcross(axb, ay))
                    end = (c[0], ya + 0.75 - d * 0.8, c[2])
                    cc = (end[0] + axb[0] * ln / 2, end[1] + axb[1] * ln / 2, end[2] + axb[2] * ln / 2)
                    obox(acc, frame(cc, axb, ay, azb), (ln, 0.025, 0.15), 'Wood')
                    continue
                obox(acc, frame(c, ax, ay, az), (1.6, 0.025, 0.15), 'Wood')
    for (y0, y1) in REVET_W:
        wall(-0.85, -1.0, 0.0, y0, y1)
    for (y0, y1) in REVET_E:
        wall(1.35, 1.0, STEP_H, y0, y1)


def build_duckboards(acc):
    y = -9.0
    while y < 8.9 - 1.7:
        ln = 1.8
        yc = y + ln / 2
        y += ln + rng.uniform(0.04, 0.1)
        x0, yaw, dz, roll = rng.uniform(-0.02, 0.02), 90 + rng.uniform(-1.5, 1.5), 0.0, rng.uniform(-1.2, 1.2)
        if -4.6 < yc < -2.0:                                      # the section that slid into the puddle
            x0, yaw, dz, roll = 0.16, 90 + 9.0, -0.035, 5.0
        z = float(Hm(x0, yc)) + dz
        fr = yaw_frame((x0, yc, z), yaw, 0.0, roll)

        def sub(dx, dy, dzz):
            return lambda a, b, c: fr(a + dx, b + dy, c + dzz)
        for sy in (-0.2, 0.2):
            obox(acc, sub(0, sy, 0.03), (ln, 0.07, 0.06), 'Wood', skip='-z')
        s = -ln / 2 + 0.07
        while s < ln / 2 - 0.04:
            r = rng.random()
            sx = s
            s += 0.135
            if r < 0.09:
                continue                                           # missing slat
            if r < 0.17:                                           # snapped slat: one half left, tilted
                side = rng.choice((-1, 1))
                f2 = yaw_frame(fr(sx, side * 0.17, 0.071), yaw + 90 + rng.uniform(-8, 8), side * rng.uniform(4, 9), 0)
                obox(acc, f2, (0.27, 0.095, 0.022), 'Wood')
                continue
            f2 = yaw_frame(fr(sx, rng.uniform(-0.012, 0.012), 0.071), yaw + 90 + rng.uniform(-2.5, 2.5), 0, 0)
            obox(acc, f2, (0.62, 0.095, 0.022), 'Wood', skip='-z')


# =============================================================================== props
PAINT = (0.02, 0.02, 0.48, 0.98)
ENAMEL = (0.53, 0.53, 0.97, 0.97)


def crate_frame(spec):
    x, y, yaw = spec
    return yaw_frame((x, y, float(Hm(x, y)) - 0.008), yaw)


def build_crate(acc, spec, is_open):
    L, W, Hh, th = 0.60, 0.40, 0.35, 0.02
    fr = crate_frame(spec)

    def sub(dx, dy, dz):
        return lambda a, b, c: fr(a + dx, b + dy, c + dz)
    if not is_open:
        obox(acc, sub(0, 0, (Hh - th) / 2), (L, W, Hh - th), 'Wood')
        obox(acc, sub(0, 0, Hh - th / 2), (L + 0.024, W + 0.024, th), 'Wood')
    else:
        obox(acc, sub(0, 0, th / 2), (L, W, th), 'Wood')
        for s in (-1, 1):
            obox(acc, sub(0, s * (W / 2 - th / 2), (Hh + th) / 2), (L, th, Hh - th), 'Wood')
            obox(acc, sub(s * (L / 2 - th / 2), 0, (Hh + th) / 2), (th, W - 2 * th, Hh - th), 'Wood')
        # the lid, slid half off and tipped against the rim
        lf = yaw_frame(fr(0.16, -0.17, Hh + 0.035), spec[2] + 17, -4.0, 16.0)
        obox(acc, lf, (L + 0.024, W + 0.024, th), 'Wood')
    for s in (-1, 1):                                              # end cleats + carrying batten
        for q in (-1, 1):
            obox(acc, sub(s * (L / 2 + 0.011), q * (W / 2 - 0.05), Hh / 2 - 0.01), (0.022, 0.05, Hh - 0.05), 'Wood')
        obox(acc, sub(s * (L / 2 + 0.016), 0, Hh * 0.66), (0.032, W - 0.2, 0.035), 'Wood')


def build_helmet(acc, fr):
    outer = [(0, 0.158), (0.035, 0.154), (0.07, 0.140), (0.098, 0.112), (0.113, 0.072), (0.118, 0.036),
             (0.128, 0.014), (0.152, 0.0)]
    inner = [(0.151, -0.004), (0.125, 0.009), (0.114, 0.034), (0.109, 0.070), (0.094, 0.108), (0.067, 0.135),
             (0.033, 0.149), (0, 0.153)]
    revolve(acc, fr, outer + inner, 16, 'Steel', PAINT, sy=1.17)


def build_props():
    acc = C.Acc('SM_Intro_TrenchProps', ['Wood', 'Steel'])
    build_crate(acc, CRATE_A, False)
    build_crate(acc, CRATE_B, True)
    # helmets on the fire step: one upright, one tipped on its side
    (x, y) = HELMETS[0]
    build_helmet(acc, yaw_frame((x, y, float(Hm(x, y)) + 0.002), 25))
    (x, y) = HELMETS[1]
    build_helmet(acc, yaw_frame((x, y, float(Hm(x, y)) + 0.118), 70, 0, 78))
    # enamel mug on the closed crate
    fa = crate_frame(CRATE_A)
    mc = fa(-0.15, -0.10, 0.352)
    mf = yaw_frame(mc, 30)
    revolve(acc, mf, [(0, 0), (0.042, 0), (0.043, 0.085), (0.039, 0.085), (0.038, 0.006), (0, 0.006)], 12, 'Steel', ENAMEL)
    hp = [mf(0.041, 0, 0.07), mf(0.066, 0, 0.066), mf(0.072, 0, 0.045), mf(0.064, 0, 0.022), mf(0.041, 0, 0.017)]
    tube(acc, hp, [0.0045] * 5, 5, 'Steel', uscale=1.0, vspan=0.1, u0=0.6, v0=0.6)
    # coil of wire standing on the fire step, leaning on the wall
    cx, cy = 1.17, 3.6
    R = 0.2
    e1, e2 = np.array((0.0, 1.0, 0.0)), np.array((0.30, 0.0, 0.954))
    axn = np.cross(e1, e2)
    c0 = np.array((cx, cy, float(Hm(cx, cy)) + R * 0.954 + 0.006))
    pts = []
    turns, per = 7, 18
    for i in range(turns * per + 1):
        a = 2 * math.pi * i / per
        rr = R * (1 + 0.035 * math.sin(3.1 * a + 1.0) + 0.02 * math.sin(0.37 * a))
        pts.append(tuple(c0 + rr * (math.cos(a) * e1 + math.sin(a) * e2)
                         + axn * (0.011 * (a / (2 * math.pi) - turns / 2) + 0.006 * math.sin(2.3 * a))))
    tube(acc, pts, [0.006] * len(pts), 4, 'Steel', uscale=0.045, vspan=0.4, u0=0.53, v0=0.05)
    # ladder leaning on the west wall, top over the lip
    d = np.array(vnorm((-0.321, 0.0, 1.0)))
    B = np.array((-0.55, LADDER_Y, 0.02))
    ax, ay = tuple(d), (0.0, 1.0, 0.0)
    az = vcross(ax, ay)
    for s in (-0.21, 0.21):
        obox(acc, frame(tuple(B + d * 1.35 + np.array(ay) * s), ax, ay, az), (2.7, 0.045, 0.07), 'Wood')
    for k in range(9):
        if k == 5:
            continue
        obox(acc, frame(tuple(B + d * (0.3 + 0.28 * k)), ay, ax, az), (0.40, 0.032, 0.036), 'Wood')
    return acc.build()


# =============================================================================== mould
def wall_foot(x_in, dirx, y):
    """x where the wall actually leaves the floor, marching from x_in toward dirx (walls undulate)."""
    xs = x_in + dirx * np.linspace(0, 1.2, 241)
    zs = Hm(xs, np.full_like(xs, y))
    k = int(np.argmax(zs > zs[0] + 0.05))
    return float(xs[max(k - 1, 0)])


_SX = np.linspace(0, 2.4, 481)


def sec_point(x0, dirx, s, y):
    """Point at arc length s along the cross-section through y, starting at x0 (s > 0 goes toward dirx)."""
    sg = 1.0 if s >= 0 else -1.0
    xs = x0 + dirx * sg * _SX
    zs = Hs(xs, np.full_like(xs, y))
    arc = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(xs), np.diff(zs)))])
    return float(np.interp(abs(s), arc, xs)), float(y), float(np.interp(abs(s), arc, zs))


def build_mat(name, x0, dirx, y0, s_c, Rs, Rt, T, seed, lobes, bumps=(), corner=0.0):
    rings = [0.12, 0.25, 0.38, 0.5, 0.61, 0.71, 0.8, 0.87, 0.93, 0.975, 1.0]
    spokes = 64
    pivot = sec_point(x0, dirx, s_c, y0)
    verts, st, nrm = [], [], []

    def point(rho, th):
        rm = 1.0 + 0.15 * float(C.ang_noise(th, seed + 5, 2, 7))
        for (lt, la, lw) in lobes:
            dth = (th - lt + math.pi) % (2 * math.pi) - math.pi
            rm += la * math.exp(-(dth / lw) ** 2)
        s = s_c + rho * Rs * rm * math.cos(th)
        t = rho * Rt * rm * math.sin(th)
        P = np.array(sec_point(x0, dirx, s, y0 + t))
        n = surf_n(P[0], P[1], 0.10)
        prof = math.sqrt(max(1 - rho ** 6, 0.0)) * (1 + 0.25 * math.exp(-((rho - 0.82) / 0.12) ** 2))
        thk = T * prof * (1 + 0.3 * float(sn(s, t, seed, 2.2, 2)) + 0.12 * float(sn(s, t, seed + 1, 6.0, 1)))
        for (bs, bt, ba, br) in bumps:
            thk += ba * math.exp(-((math.hypot(s - bs, t - bt)) / br) ** 4) * min(prof, 1.0)
        thk += corner * math.exp(-((s) / 0.22) ** 2) * prof
        off = 0.006 if rho >= 1.0 else 0.02 + max(thk, 0.01)
        verts.append(tuple(P + n * off))
        st.append((s, t))
        nrm.append(n)
    point(0.0, 0.0)
    for rho in rings:
        for k in range(spokes):
            point(rho, 2 * math.pi * k / spokes)
    # relax the sheet (not its rim) so it drapes like a blanket instead of following every crease
    A = np.array(verts[1:]).reshape(len(rings), spokes, 3)
    c0 = np.array(verts[0])
    for _ in range(3):
        inner = np.concatenate([np.broadcast_to(c0, (1, spokes, 3)), A[:-1]], 0)
        outer = np.concatenate([A[1:], A[-1:]], 0)
        B = 0.4 * A + 0.15 * (np.roll(A, 1, 1) + np.roll(A, -1, 1) + inner + outer)
        A[:-2] = B[:-2]
        c0 = 0.5 * c0 + 0.5 * A[0].mean(0)
    verts = [tuple(c0)] + [tuple(p) for p in A.reshape(-1, 3)]
    faces = []
    for k in range(spokes):
        faces.append((0, 1 + k, 1 + (k + 1) % spokes))
    for r in range(len(rings) - 1):
        for k in range(spokes):
            k2 = (k + 1) % spokes
            a, b = 1 + r * spokes + k, 1 + r * spokes + k2
            faces.append((a, a + spokes, b + spokes, b))
    # orient so the faces look away from the trench surface
    V = np.array(verts)
    f0 = faces[0]
    fn = np.cross(V[f0[1]] - V[f0[0]], V[f0[2]] - V[f0[0]])
    if np.dot(fn, nrm[0]) < 0:
        faces = [tuple(reversed(f)) for f in faces]
    bad = 0
    for f in faces:
        fn = np.cross(V[f[1]] - V[f[0]], V[f[2]] - V[f[0]])
        if np.dot(fn, nrm[f[0]]) < 0:
            bad += 1
    print('MAT %s: %d faces, %d facing into the surface' % (name, len(faces), bad))
    acc = C.Acc(name, ['Mould'])
    acc.mesh(verts, faces, [[(st[i][0] + 0.5, st[i][1] + 0.5) for i in f] for f in faces], 'Mould')
    return acc.build(origin=pivot, smooth_angle=80.0)


def plan_vein(acc, ctrl, r0, r1, seed):
    P = smooth_path([(x, y, 0.0) for (x, y) in ctrl], 0.05, 5)
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    t = np.linspace(0, cum[-1], max(int(cum[-1] / 0.01), 4))
    x, y = np.interp(t, cum, P[:, 0]), np.interp(t, cum, P[:, 1])
    tx, ty = np.gradient(x), np.gradient(y)
    ln = np.hypot(tx, ty) + 1e-9
    w = 0.04 * sn(t, t * 0.0, seed, 0.9, 2) * np.clip(np.sin(np.pi * t / cum[-1]), 0, 1) ** 0.5
    x, y = x - ty / ln * w, y + tx / ln * w
    pts = []
    for i in range(len(t)):
        r = r0 + (r1 - r0) * t[i] / cum[-1]
        n = surf_n(x[i], y[i])
        pts.append(np.array((x[i], y[i], float(Hs(x[i], y[i])))) + n * r * 0.55)
    Q = smooth_path(pts, 0.06, 2)
    k = np.linspace(0, 1, len(Q))
    rad = (r0 + (r1 - r0) * k) * (1 + 0.14 * np.sin(k * cum[-1] * 9.0 + seed))
    tube(acc, Q, list(rad), 6, 'Mould', uscale=1.0, vspan=0.25, u0=seed * 0.137)


def build_veins():
    acc = C.Acc('SM_Intro_Mould_Veins', ['Mould'])
    root = (-0.6, 3.62)
    plan_vein(acc, [root, (-0.62, 4.6), (-0.55, 5.6), (-0.45, 7.4), (-0.6, 9.3)], 0.040, 0.022, 1)
    plan_vein(acc, [root, (-0.25, 3.45), (-0.17, 2.6), (-0.2, 1.6), (-0.55, 0.6), (-0.68, -1.6), (-0.6, -3.6)], 0.038, 0.02, 2)
    plan_vein(acc, [root, (-0.2, 4.1), (0.5, 4.5), (0.95, 4.6), (1.2, 5.25), (1.45, 5.5), (1.74, 5.75)], 0.036, 0.022, 3)
    plan_vein(acc, [(-0.6, 5.0), (-0.86, 5.08), (-1.02, 5.2), (-1.17, 5.3)], 0.032, 0.02, 4)
    plan_vein(acc, [(-0.18, 2.6), (0.45, 2.7), (0.95, 2.85), (1.3, 2.75), (1.55, 2.9), (1.76, 3.0)], 0.032, 0.02, 5)
    plan_vein(acc, [(-0.5, 0.7), (-0.84, 0.9), (-1.0, 1.05), (-1.16, 1.1)], 0.03, 0.02, 6)
    plan_vein(acc, [(-0.45, 7.4), (0.2, 7.9), (0.7, 8.6), (0.5, 9.8), (0.2, 10.6)], 0.03, 0.02, 7)
    # tight wraps around the two crates
    L, W = 0.60, 0.40
    r = 0.027
    ro = r * 1.15
    for spec, heights in ((CRATE_A, (0.12, 0.25)), (CRATE_B, (0.16,))):
        fr = crate_frame(spec)
        for zi, zr in enumerate(heights):
            hx, hy = L / 2 + ro, W / 2 + ro
            loop = [(hx, 0), (hx, hy), (0, hy), (-hx, hy), (-hx, 0), (-hx, -hy), (0, -hy), (hx, -hy), (hx, 0), (hx, hy * 0.5)]
            pts = [fr(a, b, zr + 0.03 * math.sin(i * 1.7 + zi)) for i, (a, b) in enumerate(loop)]
            Q = smooth_path(pts, 0.04, 2)
            tube(acc, Q, [r * (1 + 0.12 * math.sin(i * 0.5)) for i in range(len(Q))], 6, 'Mould', u0=0.3 * zi)
    fa, fb = crate_frame(CRATE_A), crate_frame(CRATE_B)
    hx = L / 2 + ro
    strap = [fa(-hx - 0.45, 0.10, r), fa(-hx, 0.06, r), fa(-hx, 0.05, 0.35 + ro), fa(0.0, 0.0, 0.35 + ro),
             fa(hx, -0.06, 0.35 + ro), fa(hx, -0.06, r),
             fb(-hx, 0.19, r), fb(-hx, 0.19, 0.35 + ro), fb(0.0, 0.195, 0.35 + ro * 0.9), fb(hx, 0.19, 0.35 + ro),
             fb(hx, 0.19, r), (root[0], root[1], float(Hm(*root)) + r)]
    Q = smooth_path(strap, 0.035, 2)
    tube(acc, Q, [0.03 * (1 + 0.1 * math.sin(i * 0.4)) for i in range(len(Q))], 6, 'Mould', u0=0.7)
    return acc.build(origin=(root[0], root[1], float(Hm(*root))), smooth_angle=80.0)


def pod(acc, c, n, r):
    n = np.array(n, float)
    n /= np.linalg.norm(n)
    a = np.cross(n, (0.0, 0.0, 1.0) if abs(n[2]) < 0.9 else (1.0, 0.0, 0.0))
    a /= np.linalg.norm(a)
    b = np.cross(n, a)
    q = rng.uniform(0.85, 1.25)
    prof = [(0, -0.5 * r), (0.72 * r, -0.3 * r), (r, 0.15 * r), (0.86 * r, 0.7 * r * q), (0.45 * r, 1.1 * r * q), (0, 1.27 * r * q)]
    u0, v0 = rng.random(), rng.random()
    revolve(acc, frame(tuple(c), tuple(a), tuple(b), tuple(n)), prof, 8, 'Mould', (u0, v0, u0 + 5.0 * r, v0 + 2.6 * r),
            sy=rng.uniform(0.85, 1.15))


def place_pods(acc, cands):
    placed = []
    for (P, n, r) in cands:
        P = np.array(P)
        if any(np.linalg.norm(P - Q) < 0.72 * (r + r2) for (Q, r2) in placed):
            continue
        placed.append((P, r))
        pod(acc, P + np.array(n) * r * 0.2, n, r)
    return len(placed)


def build_pustules_a():
    acc = C.Acc('SM_Intro_Mould_Pustules_A', ['Mould'])
    cx, cy = wall_foot(0.3, 1.0, 10.9) - 0.03, 0.0                 # north-east inside corner of the trench
    ys_ = 10.6 + np.linspace(0, 1.2, 241)
    zs_ = Hm(np.full_like(ys_, cx - 0.1), ys_)
    cy = float(ys_[max(int(np.argmax(zs_ > zs_[0] + 0.05)) - 1, 0)]) - 0.03
    cands = []
    for i in range(400):
        d = abs(rng.gauss(0, 0.26))
        a = rng.uniform(0, 2 * math.pi)
        x, y = cx + d * math.cos(a) + 0.04, cy + d * math.sin(a) + 0.04
        z = float(Hs(x, y))
        if z > 1.2:
            continue
        r = max(0.04, min(0.10, 0.10 - 0.09 * d + rng.uniform(-0.02, 0.02)))
        cands.append(((x, y, z), surf_n(x, y), r))
    n = place_pods(acc, cands)
    print('PUSTULES_A pods', n)
    return acc.build(origin=(cx, cy, float(Hm(cx, cy))), smooth_angle=80.0)


def build_pustules_b():
    acc = C.Acc('SM_Intro_Mould_Pustules_B', ['Mould'])
    yp = 7.1                                                       # revetment post on the west wall

    def xp(z):
        return -0.85 - BAT * z / DEPTH + 0.045
    cands = []
    for i in range(200):
        if i % 2 == 0:
            ph = math.radians(rng.uniform(-105, 105))
            d = rng.uniform(0.09, 0.36)
            x, y = xp(0) + d * math.cos(ph), yp + d * math.sin(ph)
            if x < -0.82:
                continue
            r = max(0.04, 0.10 - 0.15 * (d - 0.09) + rng.uniform(-0.015, 0.015))
            cands.append(((x, y, float(Hs(x, y))), surf_n(x, y), r))
        else:
            z = rng.uniform(0.04, 1.0) ** 1.3
            ph = math.radians(rng.uniform(-115, 115))
            n = vnorm((math.cos(ph), math.sin(ph), 0.25))
            r = max(0.04, 0.095 - 0.05 * z + rng.uniform(-0.015, 0.015))
            cands.append(((xp(z) + n[0] * 0.055, yp + n[1] * 0.055, z), n, r))
    n = place_pods(acc, cands)
    print('PUSTULES_B pods', n)
    return acc.build(origin=(xp(0) + 0.06, yp, float(Hm(xp(0) + 0.06, yp))), smooth_angle=80.0)


# =============================================================================== main
def main():
    C.setup_scene()
    for slot, prefix in (('Earth', 'T_Intro_Earth'), ('Wood', 'T_Intro_Wood'), ('Sandbag', 'T_Intro_Sandbag'),
                         ('Steel', 'T_Intro_Steel'), ('Mould', 'T_Intro_Mould')):
        C.make_material(slot, prefix)
    acc = C.Acc('SM_Intro_Trench', ['Earth', 'Wood', 'Sandbag'])
    build_earth(acc)
    n_earth = len(acc.f)
    nb = build_parapet(acc)
    build_revetment(acc)
    build_duckboards(acc)
    print('TRENCH grid %d x %d, earth tris %d, sandbags %d' % (len(XS), len(YS), n_earth, nb))
    trench = acc.build(smooth_angle=62.0)
    # earth is a height field: every earth triangle must face up
    me = trench.data
    down = sum(1 for p in me.polygons if p.material_index == 0 and p.normal.z <= 0)
    print('CHECK earth triangles facing down: %d' % down)
    assert down == 0
    obs = [trench, build_props()]
    obs.append(build_mat('SM_Intro_Mould_Mat_A', wall_foot(-0.4, -1.0, 4.4), -1.0, 4.4, 0.30, 0.95, 1.10, 0.080, 301,
                         [(2.7, 0.35, 0.14), (3.5, 0.45, 0.12), (1.4, 0.22, 0.2), (4.9, 0.25, 0.2)]))
    obs.append(build_mat('SM_Intro_Mould_Mat_B', 1.10, 1.0, 1.2, -0.08, 0.80, 1.15, 0.075, 311,
                         [(3.0, 0.40, 0.13), (3.6, 0.3, 0.10), (2.5, 0.35, 0.12), (1.3, 0.2, 0.2), (4.9, 0.2, 0.2)],
                         bumps=[(HELMETS[0][0] - 1.10, HELMETS[0][1] - 1.2, 0.30, 0.25),
                                (HELMETS[1][0] - 1.10, HELMETS[1][1] - 1.2, 0.44, 0.27)]))
    obs.append(build_mat('SM_Intro_Mould_Mat_C', wall_foot(0.4, 1.0, 7.6), 1.0, 7.6, 0.05, 0.5, 2.0, 0.085, 321,
                         [(2.2, 0.5, 0.10), (3.2, 0.6, 0.08), (4.0, 0.5, 0.10), (0.8, 0.4, 0.10), (5.6, 0.45, 0.10)],
                         corner=0.05))
    obs += [build_veins(), build_pustules_a(), build_pustules_b()]
    stats = {}
    for ob in obs:
        st = C.ob_stats(ob)
        assert st['uv_layers'] == 1
        stats[ob.name] = st
        C.export_fbx(ob)
        st['fbx_bytes'] = os.path.getsize(os.path.join(C.OUT, ob.name + '.fbx'))
        print('STAT %-28s tris %6d  pivot_cm %s  min %s max %s' % (ob.name, st['tris'], st['pivot_cm'], st['min_cm'], st['max_cm']))
    assert stats['SM_Intro_Trench']['tris'] <= 40000, 'trench over budget'
    C.write_json('trench_stats.json', stats)
    os.makedirs(C.WORK, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(C.WORK, 'IntroTrench.blend'))
    print('TRENCH DONE')


main()
