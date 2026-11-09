"""Procedural 4x4 atlas of BOMB craters (aircraft bombs, bomb rockets, missiles): 4 weight bands x 4 variants.

Run:  python gen_bomb_atlas.py --out D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/bomb_r1

Writes T_BombImpact_BCO.png / T_BombImpact_NRS.png with the same channel packing and import settings as the shell
atlas (see gen_atlas.py / ue_build_material.py), plus atlas_meta.json and height_cells.npy. Same master material
(M_LandscapeImpactDecal) through MI_BombImpactDecal, which only swaps the two textures.

Cell i (0..15) is at column i % 4, row i // 4. Band k uses cells 4k..4k+3 (one row per band):
  v0 round       straight-down burst: deep bowl, heavy continuous rim, lobed thinning blanket
  v1 lopsided    oblique fall from -U: elliptic bowl, deepest uprange, low narrow uprange lip, blanket thrown downrange
  v2 double      main crater + a younger overlapping crater cut through its rim (stick of bombs)
  v3 broken      irregular outline, rim sector(s) collapsed into a lighter slump tongue, wet floor patch

Bombs bury before they go off, so compared with shells: a deep bowl, a heavy raised rim of upturned earth with blocks
on the crest, a wide lumpy blanket of darker, warmer subsoil that thins out in noisy patches and broad lobes, big clods
and only a little soot. Radial layout per band in fractions of the decal box D: rim crest ~0.25 D, blanket to 0.45 D
(lobes reach it, gaps stop near 0.33 D). Bowl, rim and blanket heights are in cm at that D; clods and blocks have
ABSOLUTE sizes (80 % 15-40 cm, 20 % 50-100 cm, blocks 50-110 cm) for every band, the count grows with the area, so a
2500 kg crater gets more clods, not boulders. Noise that must stay crisp is specified in pixels (1.8-4.3 cm/px).
Alpha is hard zero beyond r = 0.45 D, like the shell atlas.
"""
import argparse
import json
import math
import os

import cv2
import numpy as np

from gen_atlas import CELL, GRID, NEUTRAL, Polar, fbm, fft_noise, height_to_normal, smoothstep

# sRGB 0..1. Warmer than the neutral shell soil (bombs dig up subsoil), value kept below typical ground values.
LOAM = np.array([0.242, 0.190, 0.142])       # thrown subsoil blanket (dark moist umber)
OCHRE = np.array([0.290, 0.222, 0.150])      # clay patches in the blanket
MARL = np.array([0.238, 0.205, 0.172])       # marl patches (kept warm: no grey in the bomb cells)
DRY = np.array([0.262, 0.212, 0.165])        # outer blanket, drier
CREST = np.array([0.315, 0.258, 0.200])      # dry crust on the rim crest
WALL = np.array([0.205, 0.160, 0.120])       # freshly cut wall (in shade of the bowl, ~x0.65 of the crest)
WET = np.array([0.150, 0.115, 0.086])        # wet floor (~x0.45 of the crest)
TONGUE = np.array([0.305, 0.248, 0.190])     # fallen rim (broken variant)
CLOD = LOAM * 0.80                           # clods in the subsoil hue: shaded base ...
CLOD_TOP = LOAM * 1.04                       # ... lit flat top
CLAY = np.array([0.315, 0.250, 0.180])       # lighter upturned clay blocks
PUDDLE = np.array([0.105, 0.092, 0.082])     # standing water / slurry
SPECK = np.array([0.205, 0.162, 0.122])      # small thrown lumps
FLOOR_MIN = 0.11

VARIANTS = ['round', 'lopsided', 'double', 'broken']

# Rc = rim crest radius (cm), depth = bowl depth, rimH = crest height above ground, clods = count (absolute sizes),
# blocks = count, lobes = (min, max) count of blanket lobes, terrace = wall benches (x depth), double = (secondary
# radius x Rc, angle offset), slumps = collapsed rim sectors in the broken variant, scorch = soot strength (low).
BANDS = [
    dict(name='K0', kg='<= 300 kg', design_kg=250, D=900, depth_box=250, Rc=232.0, depth=135.0, rimH=55.0,
         clods=240, blocks=20, lobes=(6, 8), terrace=0.0, double=(0.55, 0.3), slumps=1, scorch=0.18),
    dict(name='K1', kg='300-600 kg', design_kg=500, D=1200, depth_box=300, Rc=308.0, depth=180.0, rimH=70.0,
         clods=400, blocks=26, lobes=(5, 7), terrace=0.0, double=(0.62, -0.5), slumps=2, scorch=0.16),
    dict(name='K2', kg='600-1200 kg', design_kg=1000, D=1600, depth_box=350, Rc=410.0, depth=240.0, rimH=92.0,
         clods=650, blocks=34, lobes=(7, 9), terrace=0.08, double=(0.47, 0.9), slumps=1, scorch=0.14),
    dict(name='K3', kg='> 1200 kg', design_kg=2500, D=2200, depth_box=450, Rc=560.0, depth=325.0, rimH=124.0,
         clods=1000, blocks=44, lobes=(6, 9), terrace=0.10, double=(0.58, -1.2), slumps=2, scorch=0.12),
]
REACH_MAX = 1.75   # blanket lobes reach this many Rc (~0.445 D); gaps end near REACH_MIN
REACH_MIN = 1.45   # continuous apron to ~0.37 D everywhere


def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def stamp_lumps(h_field, mask, color_field, color_w, items, x0, s, rng, height_ratio, col_a, col_b, on_ground=True):
    """Lumpy flattish domes (upturned earth). items: (cx_cm, cy_cm, size_cm, angle, elong). on_ground: sits on
    max(surface, 0) (thrown clods); otherwise on the local surface (fallback in the bowl)."""
    H, W = h_field.shape
    for (cx, cy, size, ang, elong) in items:
        a = size * 0.5 * rng.uniform(0.95, 1.15) * elong
        b = size * 0.5 * rng.uniform(0.6, 0.9)
        ext = int(math.ceil(max(a, b) * 1.3 / s)) + 2
        px, py = int(round((cx - x0) / s)), int(round((cy - x0) / s))
        xa, xb = max(px - ext, 0), min(px + ext + 1, W)
        ya, yb = max(py - ext, 0), min(py + ext + 1, H)
        if xa >= xb or ya >= yb:
            continue
        xs = (np.arange(xa, xb) + 0.5) * s + x0 - cx
        ys = (np.arange(ya, yb) + 0.5) * s + x0 - cy
        Xg, Yg = np.meshgrid(xs, ys)
        ca, sa = math.cos(ang), math.sin(ang)
        u = (Xg * ca + Yg * sa) / a
        v = (-Xg * sa + Yg * ca) / b
        t = np.arctan2(v, u)
        # Angular outline (3/4-sided facets) and a flat top: broken sods, not pebbles.
        lump = (1.0 + 0.10 * np.sin(2 * t + rng.uniform(0, 6.3)) + 0.12 * np.abs(np.sin(1.5 * t + rng.uniform(0, 6.3)))
                + 0.08 * np.sin(4 * t + rng.uniform(0, 6.3)) - 0.06)
        d2 = (u * u + v * v) / (lump * lump)
        dome = np.clip(1.0 - d2 ** 2.5, 0.0, 1.0) ** 0.5
        tilt = 1.0 + 0.2 * (u * rng.uniform(-1, 1) + v * rng.uniform(-1, 1))
        edge = smoothstep(1.0, 0.55, d2)
        hh = dome * tilt * size * height_ratio * rng.uniform(0.75, 1.1)
        sub = h_field[ya:yb, xa:xb]
        base = np.maximum(sub, 0) if on_ground else sub
        np.maximum(sub, sub * (1 - edge) + (base + hh) * edge, out=sub)
        m = mask[ya:yb, xa:xb]
        np.maximum(m, edge, out=m)
        k = rng.uniform(0.0, 0.4)
        base_c = col_a * (1 - k) + col_b * k
        c = base_c[None, None, :] * (1 - dome[..., None] * 0.6) + col_b[None, None, :] * (dome[..., None] * 0.6)
        cw = color_w[ya:yb, xa:xb]
        sel = edge > cw
        color_field[ya:yb, xa:xb][sel] = c[sel]
        cw[sel] = edge[sel]


def lobe_field(th, rng, n, centre=None, spread=None):
    """0..1 angular field of broad blanket lobes. centre/spread: lobes clustered around an angle (oblique fall)."""
    f = np.zeros(th.shape)
    base = rng.uniform(0, 2 * math.pi)
    for i in range(n):
        if centre is None:
            a = base + (i + rng.uniform(-0.3, 0.3)) * 2 * math.pi / n
        else:
            a = centre + spread * ((i + 0.5) / n * 2 - 1) + rng.normal(0, 0.12)
        w = rng.uniform(0.22, 0.42) * (6.0 / max(n, 3)) ** 0.5
        f = np.maximum(f, rng.uniform(0.7, 1.0) * np.exp(-(wrap_arr(th - a) / w) ** 2))
    return f


def wrap_arr(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def crater(band, X, Y, rng, shape, s, Rc, depth, rimH, ellipse=1.0, deep_shift=0.0, lip=None, outline=1.0,
           gaps=(), slope_w=None):
    """One bomb crater centred at X = Y = 0 (cm), radial profile in units of its rim crest radius Rc.
    ellipse: stretch along X; deep_shift: deepest point offset along X (x Rc); lip(th) -> rim height multiplier;
    gaps: (angle, half width) rim sectors collapsed. Returns heights and rn (1 = crest)."""
    th = np.arctan2(Y, X)
    re = np.sqrt((X / ellipse) ** 2 + Y * Y)
    lobes = np.zeros(shape)
    for k, amp in ((2, 0.030), (3, 0.040), (5, 0.030), (7, 0.018)):
        lobes += amp * outline * np.sin(k * th + rng.uniform(0, 2 * math.pi))
    rn = re / Rc * (1.0 + lobes + 0.035 * fbm(rng, shape, max(Rc * 0.2 / s, 3.0), 3))
    # Bowl: steep inner wall (rn 0.25..1), small fallback floor.
    xs = X / Rc - deep_shift
    rd = np.sqrt((xs / ellipse) ** 2 + (Y / Rc) ** 2) * (1.0 + lobes)
    bowl_r = np.minimum(rn, rd * 0.5 + rn * 0.5) if deep_shift else rn
    # Cone-like (parabolic) bowl: flat floor only ~0.15 Rc, walls steepest near the rim (soft-capped at 55 deg).
    pr = np.clip(bowl_r, 0, 1)
    bowl = -depth * (1.0 - smoothstep(0.0, 1.0, (pr - 0.12) / 0.88) * 0.25 - np.clip((pr - 0.12) / 0.88, 0, 1) ** 1.6 * 0.75)
    bowl *= smoothstep(1.06, 0.96, bowl_r)
    bowl = cv2.GaussianBlur(bowl, (0, 0), max(Rc * 0.03 / s, 1.0))
    # Rim: crest at rn = 1, short steep inner face, long gentle outer slope (~10 deg) that carries the blanket.
    lipm = lip(th) if lip is not None else 1.0
    gapm = np.ones(shape)
    for a0, hw in gaps:
        gapm *= 1.0 - 0.85 * smoothstep(hw, hw * 0.4, np.abs(wrap_arr(th - a0)))
    big = 1.0 + 0.22 * fbm(rng, shape, max(Rc * 0.35 / s, 4.0), 2)          # low-frequency crest undulation
    amp = rimH * lipm * gapm * big
    dr = rn - 1.0
    inner = np.exp(-(dr / 0.13) ** 2)
    sw = slope_w(th) if slope_w is not None else 1.0
    outer = np.clip(1.0 - dr / (0.75 * sw), 0, 1) ** 1.5
    rim = amp * np.where(dr < 0, inner, outer)
    # Inner-wall gullies (8-14) and slump benches on the large bands.
    walls = smoothstep(0.3, 0.6, rn) * smoothstep(1.02, 0.85, rn)
    return dict(bowl=bowl, rim=rim, rn=rn, th=th, walls=walls, gapm=gapm)


def make_cell(band, variant, seed):
    rng = np.random.default_rng(seed)
    D, Rc0 = band['D'], band['Rc']
    s = D / CELL
    x0 = -D * 0.5
    coords = (np.arange(CELL) + 0.5) * s + x0
    X, Y = np.meshgrid(coords, coords)
    shape = (CELL, CELL)
    r = np.sqrt(X * X + Y * Y)
    th = np.arctan2(Y, X)
    polar = Polar(r, th, D * 0.75, s)
    vname = VARIANTS[variant]
    depth, rimH = band['depth'], band['rimH']
    tongue = np.zeros(shape)
    wet_patch = 0.0
    rims = None          # (cx, cy, Rc, share) of every crest that carries blocks
    spill = np.zeros(shape)
    puddle_c = None      # (cx, cy, Rc) of the crater holding standing water
    cx_c, cy_c = 0.0, 0.0       # main crater centre (clod placement)
    lob_n = int(rng.integers(band['lobes'][0], band['lobes'][1] + 1))

    if vname == 'round':
        a0 = rng.uniform(0, 2 * math.pi)
        cx_c, cy_c = Rc0 * 0.04 * math.cos(a0), Rc0 * 0.04 * math.sin(a0)
        c = crater(band, X - cx_c, Y - cy_c, rng, shape, s, Rc0, depth, rimH, outline=1.9)
        rn, cth = c['rn'], c['th']
        h = c['bowl'] + c['rim']
        lobes = lobe_field(cth, rng, lob_n)
        reach = REACH_MIN + (REACH_MAX - REACH_MIN) * lobes ** 0.7
        walls = c['walls']
    elif vname == 'lopsided':
        Rc = Rc0 * 0.92
        cx_c = -0.05 * D
        Xl, Yl = X - cx_c, Y
        down_f = lambda t: smoothstep(-0.2, 0.9, np.cos(t))
        c = crater(band, Xl, Yl, rng, shape, s, Rc, depth * 1.05, rimH * 1.1, ellipse=1.42, deep_shift=-0.25,
                   lip=lambda t: 0.4 + 1.0 * down_f(t), slope_w=lambda t: 0.6 + 1.0 * down_f(t))
        rn, cth = c['rn'], c['th']
        h = c['bowl'] + c['rim']
        lobes = lobe_field(cth, rng, int(rng.integers(3, 5)), centre=0.0, spread=1.0)
        dn = down_f(cth)
        reach = (1.12 + 0.5 * dn) + (0.75 * dn) * lobes ** 0.7     # uprange ~1.1 Rc, downrange up to ~2.4 Rc
        walls = c['walls']
    elif vname == 'double':
        Rc1 = Rc0 * 0.86
        ratio, aoff = band['double']
        a1 = rng.uniform(0, 2 * math.pi)
        c1x, c1y = -0.28 * Rc1 * math.cos(a1), -0.28 * Rc1 * math.sin(a1)
        c1 = crater(band, X - c1x, Y - c1y, rng, shape, s, Rc1, depth * 0.95, rimH * 0.95)
        Rc2 = Rc0 * ratio
        d2 = Rc1 * 0.98 + Rc2 * 0.25
        c2x, c2y = c1x + d2 * math.cos(a1 + aoff * 0.4), c1y + d2 * math.sin(a1 + aoff * 0.4)
        c2 = crater(band, X - c2x, Y - c2y, rng, shape, s, Rc2, depth * 0.9, rimH * 0.8)
        outer = c1['bowl'] + c1['rim'] + c2['rim']
        w = smoothstep(1.0, 0.82, c2['rn'])
        carve = np.minimum(outer, c2['bowl'] + 0.3 * c2['rim'])
        h = outer * (1 - w) + carve * w
        cx_c, cy_c = c1x, c1y
        rn = np.minimum(c1['rn'], c2['rn'])
        cth = c1['th']
        lobes = np.maximum(lobe_field(c1['th'], rng, lob_n), 0.8 * lobe_field(c2['th'], rng, 4))
        reach = REACH_MIN * 0.95 + (REACH_MAX - REACH_MIN * 0.95) * lobes ** 0.7
        walls = np.maximum(c1['walls'], c2['walls'])
        rims = [(c1x, c1y, Rc1, 0.6), (c2x, c2y, Rc2, 0.4)]
        # Fresh ejecta of the young crater lying in the old bowl: a dark fan, a little relief.
        d2c = np.sqrt((X - c2x) ** 2 + (Y - c2y) ** 2) / Rc2
        spill = smoothstep(2.1, 1.05, d2c * (1 + 0.2 * fbm(rng, shape, max(Rc2 * 0.2 / s, 3.0), 2))) * smoothstep(0.95, 1.08, d2c)
        spill *= smoothstep(1.05, 0.8, c1['rn'])
        h = h + spill * rimH * 0.25
        puddle_c = (c1x, c1y, Rc1)
    else:  # broken
        n_sl = band['slumps']
        a0 = rng.uniform(0, 2 * math.pi)
        gaps = [(a0, 0.78)] + ([(a0 + rng.uniform(2.2, 3.4), 0.40)] if n_sl > 1 else [])
        cx_c, cy_c = -0.04 * D * math.cos(a0), -0.04 * D * math.sin(a0)    # room for the spill on the gap side
        c = crater(band, X - cx_c, Y - cy_c, rng, shape, s, Rc0 * 0.93, depth * 0.92, rimH * 1.05, outline=1.1,
                   gaps=gaps)
        rn, cth = c['rn'], c['th']
        h = c['bowl'] + c['rim']
        Rc = Rc0 * 0.93
        for k_, (ga, hw) in enumerate(gaps):
            rr_ = np.sqrt((X - cx_c) ** 2 + (Y - cy_c) ** 2)
            dth = wrap_arr(np.arctan2(Y - cy_c, X - cx_c) - ga)
            along = rr_ * np.cos(dth)
            perp = rr_ * np.abs(np.sin(dth))
            reach_in = 0.22 if k_ == 0 else 0.55
            t = smoothstep(Rc * 1.02, Rc * 0.8, along) * smoothstep(Rc * (reach_in - 0.05), Rc * (reach_in + 0.2), along)
            wid = Rc * (0.30 + 0.25 * smoothstep(Rc * 0.9, Rc * reach_in, along)) * hw / 0.62
            t *= np.exp(-(perp / wid) ** 2)
            t = cv2.GaussianBlur(t, (0, 0), max(Rc * 0.06 / s, 2.0))           # soft lobe, no contour line
            # The tongue fills the bowl up to ~55 % of its depth (a smooth spill of fallen rim).
            h = h * (1 - t * 0.62)
            tongue = np.maximum(tongue, t)
        lobes = lobe_field(cth, rng, lob_n)
        gap_side = np.exp(-(wrap_arr(cth - a0) / 0.7) ** 2)
        reach = REACH_MIN + (REACH_MAX - REACH_MIN) * lobes ** 0.7 + 0.55 * gap_side   # ejecta thrown out past the gap
        walls = c['walls'] * (1 - tongue)
        wet_patch = 1.0
        puddle_c = (cx_c, cy_c, Rc)

    Rc = Rc0
    rc_local = {'lopsided': Rc0 * 0.92, 'broken': Rc0 * 0.93}.get(vname, Rc0)
    if vname == 'double':
        rc_local = Rc0 * 0.86
    # Blanket: thickness on the outer slope, thinning to the lobed edge, low-relief radial streaks inside it.
    t_bl = np.clip((rn - 1.0) / np.maximum(reach - 1.0, 0.05), 0, 2)
    streak = polar(rng, Rc * 0.3, 0.035, Rc * 0.9)
    lumpy = fbm(rng, shape, max(40.0 / s, 3.0), 3)                           # ~0.4 m lumps in the blanket
    blanket_h = rimH * 0.35 * (1 - smoothstep(0.0, 1.0, t_bl)) * smoothstep(0.98, 1.1, rn) * (1 + 0.05 * streak + 0.35 * lumpy)
    h += blanket_h
    # Wall gullies and benches.
    gul = polar(rng, Rc * 0.5, 0.22, Rc * 0.5)
    h += walls * depth * (0.022 * gul + 0.025 * fbm(rng, shape, max(Rc * 0.08 / s, 3.0), 2))
    if vname != 'broken':
        brk = smoothstep(-0.4, 0.4, fbm(rng, shape, max(Rc * 0.25 / s, 3.0), 2))
        h += depth * max(band['terrace'], 0.05) * (np.exp(-((rn - 0.85) / 0.06) ** 2) + 0.8 * np.exp(-((rn - 0.6) / 0.07) ** 2)) * 0.8 * brk * (1 - tongue)
    floor = smoothstep(0.55, 0.2, rn) * (1 - tongue)
    warp = rn + 0.03 * fbm(rng, shape, max(Rc * 0.15 / s, 3.0), 2)
    strata = (np.exp(-((warp - 0.66) / 0.03) ** 2) + np.exp(-((warp - 0.84) / 0.025) ** 2) * 0.8) * walls
    # Fallback debris on the floor and lower wall: mid-frequency lumps 10-30 cm (K0) .. 20-60 cm (K3), low relief.
    deb_cm = 20.0 * (D / 900.0) ** 0.5
    deb = np.clip(fbm(rng, shape, max(deb_cm / s, 2.0), 2) - 0.2, 0, None)
    debris_zone = smoothstep(0.75, 0.5, rn) * (1 - tongue)
    h += debris_zone * deb * deb_cm * 0.5

    # ---- clods (absolute sizes) and crest blocks -----------------------------------------------------------------
    clod_mask = np.zeros(shape)
    clod_col = np.zeros(shape + (3,))
    clod_w = np.zeros(shape)
    r_out = D * 0.44
    lob_at = lambda px, py: lobes[py, px]

    def cell_px(cx, cy):
        return int(np.clip((cx - x0) / s, 0, CELL - 1)), int(np.clip((cy - x0) / s, 0, CELL - 1))

    def clod_size():
        return 10.0 + 60.0 * rng.uniform() ** 3.6        # many 10-20 cm, a few 50-70 cm

    items = []
    for _ in range(band['clods']):
        for _try in range(80):
            u = rng.uniform()
            a = rng.uniform(-math.pi, math.pi)
            if u < 0.5:
                rr = rc_local * rng.uniform(1.02, 1.35)                       # densest just outside the crest
            elif u < 0.995:
                rr = rc_local * 1.2 + (r_out - rc_local * 1.2) * rng.uniform() ** 1.8   # thinning to ~0.1 at 0.45 D
            else:
                rr = rc_local * rng.uniform(1.0, 1.1)
            cx, cy = cx_c + rr * math.cos(a), cy_c + rr * math.sin(a)
            if math.hypot(cx, cy) >= r_out:
                continue
            px, py = cell_px(cx, cy)
            if 0.5 < rn[py, px] < 0.98 or tongue[py, px] > 0.3:
                continue
            # Beyond the crest zone keep clods inside the blanket, mostly in its lobes (islands beyond).
            if rn[py, px] > 1.25 and rng.uniform() > 0.15 + 0.85 * lob_at(px, py) * (t_bl[py, px] < 1.1):
                continue
            break
        else:
            continue
        sz = clod_size()
        it = (cx, cy, sz, rng.uniform(0, math.pi), rng.uniform(1.0, 1.3))
        if rn[py, px] >= 0.5:
            items.append(it)
    for _ in range(int(rng.integers(3, 7))):
        a = rng.uniform(-math.pi, math.pi)
        rr = rc_local * rng.uniform(1.03, 1.3)
        items.append((cx_c + rr * math.cos(a), cy_c + rr * math.sin(a), min(rng.uniform(0.04, 0.06) * D, 130.0),
                      rng.uniform(0, math.pi), rng.uniform(1.0, 1.25)))
    stamp_lumps(h, clod_mask, clod_col, clod_w, items, x0, s, rng, 0.24, CLOD, CLOD_TOP, on_ground=True)
    blocks_dark, blocks_clay = [], []
    crests = rims or [(cx_c, cy_c, rc_local, 1.0)]
    for _ in range(band['blocks']):
        bx, by, brc, _sh = crests[rng.choice(len(crests), p=np.array([c_[3] for c_ in crests]) / sum(c_[3] for c_ in crests))]
        for _try in range(40):
            a = rng.uniform(-math.pi, math.pi)
            rr = brc * rng.uniform(0.98, 1.2)
            cx, cy = bx + rr * math.cos(a), by + rr * math.sin(a)
            px, py = cell_px(cx, cy)
            if math.hypot(cx, cy) < r_out and rn[py, px] > 0.97 and tongue[py, px] < 0.2:
                break
        size = 40.0 + 40.0 * rng.uniform() ** 1.5
        it = (cx, cy, size, rng.uniform(0, math.pi), rng.uniform(1.0, 1.3))     # random orientation, not radial
        (blocks_clay if rng.uniform() < 0.25 else blocks_dark).append(it)
    stamp_lumps(h, clod_mask, clod_col, clod_w, blocks_dark, x0, s, rng, 0.22, CLOD * 1.05, CLOD_TOP, on_ground=True)
    stamp_lumps(h, clod_mask, clod_col, clod_w, blocks_clay, x0, s, rng, 0.22, CLAY * 0.8, CLAY, on_ground=True)

    # ---- window -------------------------------------------------------------------------------------------------------
    win_noise = fbm(rng, shape, max(D * 0.05 / s, 2.0), 3)
    window = smoothstep(D * 0.448, D * 0.41, r * (1.0 + 0.04 * win_noise))
    hard = (r < D * 0.45).astype(np.float64)
    h = cv2.GaussianBlur(h, (0, 0), 1.0)
    h *= window

    # ---- opacity masks ----------------------------------------------------------------------------------------------
    core = smoothstep(1.06, 0.94, rn)                     # bowl + inner face
    patch = fbm(rng, shape, max(100.0 / s, 4.0), 4) * 0.75 + 0.25 * fft_noise(rng, shape, max(25.0 / s, 2.0))
    # 0.95 at the crest, ~0.55 by t ~0.5, 0 at the lobed edge; noise-thresholded so ground shows through in patches.
    # Continuous to ~0.55 of the apron, then breaking up: the noise decides solid soil (~0.88) or bare ground.
    keep = smoothstep(0.0, 0.08, patch - (2.2 * np.clip(t_bl - 0.5, 0, 1) - 0.25)) * smoothstep(1.02, 0.92, t_bl)
    blanket_op = np.maximum(keep, smoothstep(0.6, 0.45, t_bl)) * 0.9
    blanket_op = np.where(rn < 1.0, 0.96, blanket_op)
    sp_t = np.clip((rn - 1.1) / np.maximum(0.445 * D / rc_local - 1.1, 0.1), 0, 1)
    spn = fft_noise(rng, shape, 2.4) * 0.7 + 0.3 * polar(rng, Rc * 0.08, 0.012, Rc * 1.2)
    sp_th = 0.9 + 1.6 * sp_t - 0.8 * lobes
    specks = smoothstep(sp_th, sp_th + 0.2, spn) * smoothstep(1.0, 0.8, sp_t) * (rn > 1.05)

    # ---- albedo -----------------------------------------------------------------------------------------------------
    grain = fbm(rng, shape, 1.2, 3)
    bl1 = fbm(rng, shape, max(160.0 / s, 4.0), 3)        # 1-3 m subsoil tone blotches
    bl2 = fbm(rng, shape, max(220.0 / s, 4.0), 3)
    och = smoothstep(0.2, 0.9, bl1)[..., None]
    mar = smoothstep(0.3, 1.0, bl2)[..., None] * (1 - och)
    alb = LOAM * (1 - och - mar) + OCHRE * och * 0.7 + LOAM * och * 0.3 + MARL * mar
    thin = smoothstep(0.45, 1.0, t_bl)[..., None]
    alb = alb * (1 - thin * 0.55) + DRY * (thin * 0.55)
    alb = alb * 0.9                                     # apron a little darker than the crest
    # Bowl: lighter fresh-cut walls, dark wet floor, AO by depth.
    wallm = smoothstep(0.35, 0.75, rn) * core
    floorm = smoothstep(0.55, 0.15, rn)
    alb = alb * (1 - core[..., None]) + (WALL * wallm[..., None] + WET * (1 - wallm[..., None])) * core[..., None]
    alb = alb * (1 - floorm[..., None] * 0.7) + WET * (floorm[..., None] * 0.7)
    alb *= 1.0 + 0.10 * strata[..., None]                # faint lighter strata bands on the walls
    crest = np.clip(h / max(rimH, 0.2), 0, 1.2) ** 1.2 * smoothstep(0.95, 1.05, rn) * smoothstep(1.5, 1.1, rn)
    crest = np.clip(crest, 0, 1)
    alb = alb * (1 - crest[..., None] * 0.55) + CREST * (crest[..., None] * 0.55)
    deep = np.clip(-h / max(depth, 0.1), 0, 1)
    alb *= (1.0 - 0.30 * deep[..., None] ** 0.9)
    gul_c = np.clip(0.5 - gul * 0.5, 0, 1) * walls        # darker damp streaks in the gullies
    alb *= 1.0 - 0.12 * gul_c[..., None]
    alb *= 1.0 - 0.25 * spill[..., None]
    tg = tongue[..., None]
    alb = alb * (1 - tg * 0.8) + TONGUE * (tg * 0.8)
    alb *= 1.0 + 0.08 * grain[..., None] + 0.06 * fbm(rng, shape, max(30.0 / s, 2.0), 3)[..., None]
    alb = np.maximum(alb, FLOOR_MIN * core[..., None])
    sp = specks[..., None] * (1 - core[..., None])
    alb = alb * (1 - sp * 0.75) + SPECK * (sp * 0.75)
    cm = np.clip(clod_w, 0, 1)[..., None]
    alb = alb * (1 - cm) + clod_col * cm
    wet_floor = np.zeros(shape)
    if puddle_c is not None:
        # Standing water / slurry in the floor: 10-25 % of the bowl radius, irregular, dark, smoother.
        pcx, pcy, prc = puddle_c
        pr_ = np.sqrt((X - pcx - prc * 0.08) ** 2 + (Y - pcy) ** 2) / prc
        pr_ *= 1.0 + 0.25 * fbm(rng, shape, max(prc * 0.06 / s, 2.0), 3)
        wet_floor = smoothstep(0.2, 0.13, pr_) * (1 - tongue)
        alb = alb * (1 - wet_floor[..., None]) + PUDDLE * wet_floor[..., None]
    alb = np.clip(alb, 0.02, 0.9)

    # ---- opacity, roughness, soot ------------------------------------------------------------------------------------
    op = np.maximum.reduce([core * 0.98, blanket_op, specks * 0.5 * smoothstep(0.3, 0.6, specks), clod_mask * 0.97])
    op *= window * hard
    rough = 0.84 + 0.11 * core - 0.06 * floorm - 0.10 * cm[..., 0] - 0.22 * wet_floor + 0.02 * grain
    rough = np.clip(rough, 0.62, 0.97)
    wedge = np.clip(0.2 + polar(rng, Rc * 0.5, 0.08, Rc * 0.6), 0, 1.3)
    scorch = np.exp(-(rn / 0.9) ** 2 * 1.5) * wedge * band['scorch'] * (1 - 0.4 * floorm)
    scorch_mask = np.clip(scorch / 0.5, 0, 1) * window

    g = hard * window
    alb = alb * g[..., None] + NEUTRAL * (1 - g[..., None])
    return dict(height=h, albedo=alb, opacity=op, rough=rough, scorch=scorch_mask, s=s)


def build(out_dir, seed=4321):
    os.makedirs(out_dir, exist_ok=True)
    W = CELL * GRID
    bco = np.zeros((W, W, 4), np.float64)
    nrs = np.zeros((W, W, 4), np.float64)
    heights = np.zeros((16, CELL, CELL), np.float32)
    meta = []
    for k, band in enumerate(BANDS):
        for v in range(4):
            i = 4 * k + v
            c = make_cell(band, v, seed + i * 7919)
            nx, ny, nz = height_to_normal(c['height'], c['s'])
            col, row = i % GRID, i // GRID
            ys, xs = row * CELL, col * CELL
            bco[ys:ys + CELL, xs:xs + CELL, :3] = c['albedo']
            bco[ys:ys + CELL, xs:xs + CELL, 3] = c['opacity']
            nrs[ys:ys + CELL, xs:xs + CELL, 0] = nx * 0.5 + 0.5
            nrs[ys:ys + CELL, xs:xs + CELL, 1] = ny * 0.5 + 0.5
            nrs[ys:ys + CELL, xs:xs + CELL, 2] = c['rough']
            nrs[ys:ys + CELL, xs:xs + CELL, 3] = c['scorch']
            heights[i] = c['height']
            meta.append(dict(cell=i, band=band['name'], variant=VARIANTS[v], D=band['D'], kg=band['kg'],
                             design_kg=band['design_kg'], projection_depth=band['depth_box']))
            print('cell', i, band['name'], VARIANTS[v], 'h[min,max]=%.1f,%.1f' % (c['height'].min(), c['height'].max()))

    rng = np.random.default_rng(seed)

    def save_rgba(path, a):
        a = a + (rng.uniform(-0.5, 0.5, a.shape) / 255.0)
        a8 = np.clip(np.round(a * 255), 0, 255).astype(np.uint8)
        cv2.imwrite(path, cv2.cvtColor(a8, cv2.COLOR_RGBA2BGRA))

    save_rgba(os.path.join(out_dir, 'T_BombImpact_BCO.png'), bco)
    save_rgba(os.path.join(out_dir, 'T_BombImpact_NRS.png'), nrs)
    np.save(os.path.join(out_dir, 'height_cells.npy'), heights)
    with open(os.path.join(out_dir, 'atlas_meta.json'), 'w') as f:
        json.dump(dict(set='bomb', cell_rule='band k = cells 4k..4k+3, variant v = cell 4k+v; DecalColor.R = (cell+0.5)/16',
                       variants=VARIANTS, clod_sizes_cm='80% 15-40, 20% 50-100 (cap 120); crest blocks 50-110',
                       cells=meta, bands=BANDS), f, indent=1)
    print('wrote', out_dir)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/bomb_r1')
    ap.add_argument('--seed', type=int, default=4321)
    a = ap.parse_args()
    build(a.out, a.seed)
