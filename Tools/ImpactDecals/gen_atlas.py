"""Procedural 4x4 atlas of landscape impact decals (8 calibre bands x 2 variants).

Run:  python gen_atlas.py --out D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/r1

Writes (sRGB/linear notes match the UE import settings in ue_build_material.py):
  T_LandscapeImpact_BCO.png  RGB = disturbed-soil albedo (sRGB), A = opacity           -> TC_Default (BC3), sRGB on
  T_LandscapeImpact_NRS.png  R = normal X, G = normal Y (DirectX, +G = +V/image down),
                             B = roughness, A = scorch (soot) mask                      -> TC_Masks (BC3), sRGB off
  height_cells.npy           per-cell height in cm (debug)

Cell i (0..15) is at column i % 4, row i // 4 (row 0 = top). Band b uses cells 2b (variant A) and 2b+1 (variant B).
The decal box diameter (design D) maps onto the full cell. Every band is designed in centimetres from the BANDS table
(crater radius, depth, rim, rays, clod sizes and counts), not scaled from another band. Alpha is hard zero beyond
r = 0.45 D (>= 26 px gutter at the cell edge midpoints), where colour is neutral and the normal flat, so mips don't
bleed between cells.

Variants: A = straight-down burst. B = B0/B1: grazing ricochet furrow; B2..B7: oblique impact (shell from -U):
shallow uprange entry, steep downrange wall, higher downrange rim, ejecta fan downrange, clean uprange.
"""
import argparse
import json
import math
import os

import cv2
import numpy as np

CELL = 512
GRID = 4

# Colours in sRGB 0..1. Low-saturation brown-grey so it reads on grass, dirt, sand and rock.
SOIL = np.array([0.300, 0.280, 0.255])       # churned topsoil apron (below typical ground values, low chroma)
SOIL_DAMP = np.array([0.225, 0.207, 0.188])  # crater floor / fresh subsoil (darker than the rim)
DUST = np.array([0.345, 0.325, 0.298])       # thin spray / rim crest
RAY = np.array([0.190, 0.175, 0.160])        # thrown damp soil in the rays
CLOD = np.array([0.165, 0.147, 0.128])       # damp clods: dark base ...
CLOD_TOP = np.array([0.265, 0.240, 0.212])   # ... lighter lit top
STONE = np.array([0.300, 0.290, 0.275])      # broken subsoil / stone (neutral grey)
NEUTRAL = np.array([0.29, 0.27, 0.25])       # gutter colour under zero alpha
FLOOR_MIN = 0.12                             # sRGB floor of the crater soil: the normal map makes the shadow
MAX_SLOPE = math.tan(math.radians(55.0))     # normal-map slope cap: no saturated black walls

# Lengths in cm. Rc crater/pit radius, depth bowl depth, prof bowl profile exponent (low = saucer, high = steep walls
# with a flat floor), rimH rim height, rimW rim width as a fraction of Rc, rays (count, reach), clods (count, min, max),
# spray reach of the speck field, scorch (radius, strength).


def BIG(ray_w_cm, spray_op=0.42, clod_r_out=0.40):
    """Shared B3-B7 (2026-11-09) look keys: absolute ray width, lighter clods, sparser spray, rounder oblique bowl."""
    return dict(ray_w_cm=ray_w_cm, clod_lift=1.5, spray_op=spray_op, oblique_narrow=0.0, oblique_stretch=1.35,
                outline_lobes=((2, 0.035), (3, 0.022), (5, 0.04), (7, 0.03), (11, 0.018)),
                core_edge=(1.0, 0.78), oblique_floor=0.3, ray_groups=(3, 5), soot_rim=0.1, clod_r_out=clod_r_out,
                scorch_mul=0.7, core_lift=(1.22, 1.17, 1.10), speck_arc=0.03)


BANDS = [
    # B0/B1 are exaggerated (~2x real) so they survive at 4-5 cm/px: bold dark scuff core + spray streaks.
    dict(name='B0', calibre='15-25 mm', D=40, kind='scuff', Rc=4.2, depth=1.5, prof=1.6, rimH=0.35, rimW=0.5,
         rays=(8, 15.0), clods=(12, 0.8, 2.6), spray=16.0, density=0.35, scorch=(8.5, 0.70),
         furrow=dict(L=21.0, w0=3.4, w1=9.5, depth=2.0, ridge=0.9, heap=1.4)),
    dict(name='B1', calibre='25-40 mm', D=70, kind='pit', Rc=10.0, depth=5.0, prof=1.4, rimH=1.6, rimW=0.45,
         rays=(9, 27.0), clods=(18, 1.2, 4.6), spray=28.0, density=0.3, scorch=(15.0, 0.70),
         furrow=dict(L=30.0, w0=6.5, w1=17.0, depth=3.8, ridge=1.8, heap=3.4)),
    # B2: shallow scorched scoop + soot fan, no rim.
    dict(name='B2', calibre='40-60 mm', D=110, kind='pit', Rc=17.0, depth=6.0, prof=1.2, rimH=0.8, rimW=0.38,
         rays=(10, 38.0), clods=(22, 1.5, 6.0), spray=42.0, density=0.15, scorch=(30.0, 0.75), fan=True),
    # B3-B7 re-designed 2026-11-09 at the larger decal boxes (D 240/330/450/600/850): crater, rim, rays and blanket
    # are genuinely bigger in the world (bowl ~0.85/1.2/1.7/2.6/4.1 m), while clods, stones, specks and soil mottle
    # keep physical sizes in cm (speck_cm, mottle_cm, erode_cm), so the art is not a stretch of the old cells.
    # B3: shallow saucer, thin rim begins, soot streaks.
    dict(name='B3', calibre='60-80 mm', D=240, kind='crater', Rc=46.0, depth=13.0, prof=1.0, rimH=5.0, rimW=0.24,
         rays=(16, 94.0), clods=(42, 2.5, 10.0), spray=96.0, density=0.1, scorch=(58.0, 0.70), fan=True,
         speck_cm=1.6, mottle_cm=8.0, erode_cm=3.5, **BIG(9.0)),
    # B4/B5: real rim + broken ray fan.
    dict(name='B4', calibre='80-100 mm', D=330, kind='crater', Rc=64.0, depth=19.0, prof=1.2, rimH=8.5, rimW=0.28,
         rays=(18, 138.0), clods=(56, 3.5, 14.0), spray=134.0, density=0.05, scorch=(79.0, 0.65),
         speck_cm=2.0, mottle_cm=10.0, erode_cm=4.5, **BIG(11.0)),
    dict(name='B5', calibre='100-130 mm', D=450, kind='crater', Rc=88.0, depth=35.0, prof=1.3, rimH=14.0, rimW=0.30,
         rays=(20, 190.0), clods=(78, 4.5, 20.0), spray=184.0, density=0.05, scorch=(106.0, 0.65),
         speck_cm=2.6, mottle_cm=13.0, erode_cm=6.0, **BIG(13.0)),
    # B6: real bowl + raised rim + strong radial ejecta rays.
    dict(name='B6', calibre='130-180 mm', D=600, kind='crater', Rc=130.0, depth=68.0, prof=1.4, rimH=23.0, rimW=0.32,
         rays=(24, 255.0), ray_boost=1.0, clods=(125, 7.0, 38.0), spray=250.0, density=0.0, scorch=(152.0, 0.65),
         stones=(14, 9.0, 22.0), speck_cm=3.4, mottle_cm=17.0, erode_cm=8.0, **BIG(16.0, 0.3, 0.43)),
    # B7: deep multi-lobed bowl, broad blanket, long rays to the cell edge.
    dict(name='B7', calibre='>180 mm', D=850, kind='crater', Rc=205.0, depth=110.0, prof=1.6, rimH=38.0, rimW=0.34,
         rays=(30, 362.0), ray_boost=1.0, lobed=True, clods=(190, 9.0, 56.0), spray=360.0, density=0.0,
         scorch=(226.0, 0.62), stones=(32, 12.0, 34.0), speck_cm=4.5, mottle_cm=22.0, erode_cm=10.0, **BIG(21.0, 0.3, 0.43)),
]


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def fft_noise(rng, shape, feature_px):
    """Periodic gaussian-filtered noise, mean 0 std 1. feature_px ~ blob size in pixels."""
    h, w = shape
    white = rng.standard_normal(shape)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    sigma_f = 1.0 / (math.pi * max(feature_px, 0.5))
    filt = np.exp(-(fx * fx + fy * fy) / (2.0 * sigma_f * sigma_f))
    n = np.real(np.fft.ifft2(np.fft.fft2(white) * filt))
    n -= n.mean()
    return n / (n.std() + 1e-9)


def fbm(rng, shape, feature_px, octaves=4, gain=0.5):
    out = np.zeros(shape)
    amp, f, tot = 1.0, feature_px, 0.0
    for _ in range(octaves):
        out += amp * fft_noise(rng, shape, f)
        tot += amp
        amp *= gain
        f *= 0.5
    return out / tot


class Polar:
    """Radially streaked noise, periodic in angle. Near the centre (r < r_iso) it blends into isotropic noise so the
    streaks never pinch into a pie-slice point."""

    def __init__(self, r, th, rmax, s):
        self.r, self.th, self.rmax, self.s = r, th, rmax, s

    def __call__(self, rng, r_feature_cm, arc_feature_rad, r_iso):
        na, nr = 1024, 256
        white = rng.standard_normal((nr, na))
        fy = np.fft.fftfreq(nr)[:, None]
        fx = np.fft.fftfreq(na)[None, :]
        sy = 1.0 / (math.pi * max(r_feature_cm / self.rmax * nr, 0.5))
        sx = 1.0 / (math.pi * max(arc_feature_rad / (2 * math.pi) * na, 0.5))
        filt = np.exp(-(fx * fx) / (2 * sx * sx) - (fy * fy) / (2 * sy * sy))
        n = np.real(np.fft.ifft2(np.fft.fft2(white) * filt))
        n = (n - n.mean()) / (n.std() + 1e-9)
        mx = ((self.th + math.pi) / (2 * math.pi) * na).astype(np.float32)
        my = np.clip(self.r / self.rmax * (nr - 1), 0, nr - 1).astype(np.float32)
        p = cv2.remap(n.astype(np.float32), mx, my, cv2.INTER_LINEAR, borderMode=cv2.BORDER_WRAP).astype(np.float64)
        iso = fft_noise(rng, self.r.shape, max(r_iso * 0.25 / self.s, 1.0))
        w = smoothstep(r_iso * 0.4, r_iso * 1.2, self.r)
        return p * w + iso * (1 - w)


def angle_wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def stamp_blobs(h_field, mask, color_field, color_w, items, x0, s, rng, height_ratio, col_a, col_b):
    """Stamps lumpy rounded domes. items: (cx_cm, cy_cm, size_cm, angle)."""
    H, W = h_field.shape
    for (cx, cy, size, ang) in items:
        a = size * 0.5 * rng.uniform(0.95, 1.2)
        b = size * 0.5 * rng.uniform(0.65, 0.95)
        ext = int(math.ceil(max(a, b) * 1.3 / s)) + 2
        px, py = int(round((cx - x0) / s)), int(round((cy - x0) / s))
        xa, xb = max(px - ext, 0), min(px + ext + 1, W)
        ya, yb = max(py - ext, 0), min(py + ext + 1, H)
        if xa >= xb or ya >= yb:
            continue
        xs = (np.arange(xa, xb) + 0.5) * s + x0 - cx
        ys = (np.arange(ya, yb) + 0.5) * s + x0 - cy
        X, Y = np.meshgrid(xs, ys)
        ca, sa = math.cos(ang), math.sin(ang)
        u = (X * ca + Y * sa) / a
        v = (-X * sa + Y * ca) / b
        t = np.arctan2(v, u)
        lump = (1.0 + 0.10 * np.sin(2 * t + rng.uniform(0, 6.3)) + 0.08 * np.sin(3 * t + rng.uniform(0, 6.3))
                + 0.05 * np.sin(7 * t + rng.uniform(0, 6.3)))
        d2 = (u * u + v * v) / (lump * lump)
        dome = np.clip(1.0 - d2, 0.0, 1.0) ** 0.8
        edge = smoothstep(1.0, 0.5, d2)
        hh = dome * size * height_ratio * rng.uniform(0.75, 1.15)
        sub = h_field[ya:yb, xa:xb]
        np.maximum(sub, sub * (1 - edge) + (np.maximum(sub, 0) + hh) * edge, out=sub)
        m = mask[ya:yb, xa:xb]
        np.maximum(m, edge, out=m)
        k = rng.uniform(0.0, 0.4)
        base = col_a * (1 - k) + col_b * k
        c = base[None, None, :] * (1 - dome[..., None] * 0.5) + col_b[None, None, :] * (dome[..., None] * 0.5)
        cw = color_w[ya:yb, xa:xb]
        sel = edge > cw
        color_field[ya:yb, xa:xb][sel] = c[sel]
        cw[sel] = edge[sel]


def crater_height(band, X, Y, r, th, oblique, rng, shape, s):
    """Bowl + rim + ejecta blanket height (cm) and the normalised radius rn used for masks."""
    Rc, depth, prof, rimH, rimW = band['Rc'], band['depth'], band['prof'], band['rimH'], band['rimW']
    # Organic outline: weak low-order lobes + mid-frequency noise; no single memorable corner.
    lobes = np.zeros(shape)
    lob = ((2, 0.06), (3, 0.08), (5, 0.05), (7, 0.03)) if band.get('lobed') else ((2, 0.03), (3, 0.045), (5, 0.04), (7, 0.03))
    lob = band.get('outline_lobes', lob)   # B3-B7: weaker k=3 term (no triangular bowls at the bigger sizes)
    for k, amp in lob:
        lobes += amp * np.sin(k * th + rng.uniform(0, 2 * math.pi))
    mid = fbm(rng, shape, max(Rc * 0.28 / s, 1.5), 3)
    Xe, Ye = X, Y
    if oblique:
        # Bowl stretched downrange (+X) with a steep far wall; uprange only a narrow entry gouge.
        st_ = band.get('oblique_stretch', 1.45)
        Xe = X / (st_ + (1.0 - st_) * smoothstep(-Rc * 0.4, Rc * 0.8, X)) - Rc * 0.25
        Ye = Y / (1.0 - band.get('oblique_narrow', 0.18) * smoothstep(0.0, -Rc * 1.8, X) ** 2)
    else:
        # Off-centre burst: the bowl sits a little to one side, so random yaw gives real variety.
        a0 = rng.uniform(0, 2 * math.pi)
        Xe, Ye = X - Rc * 0.12 * math.cos(a0), Y - Rc * 0.12 * math.sin(a0)
    re = np.sqrt(Xe * Xe + Ye * Ye)
    rn = re / Rc * (1.0 + lobes + 0.07 * mid)
    # Floor + wall: the wall spans rn = f0..1 (wider for saucers), so no knife-edge walls even on the deep bowls.
    f0 = min(0.12 + 0.1 * prof, 0.42)
    bowl = -depth * ((1.0 - smoothstep(f0, 1.0, rn)) * 0.82 + 0.18 * np.clip(1.0 - rn * rn, 0, 1))
    bowl = cv2.GaussianBlur(bowl, (0, 0), max(Rc * 0.05 / s, 0.8))  # no knife-edge wall lip
    dr = rn - 1.0
    w_in, w_out = rimW * 0.45, rimW * 1.4
    rim_amp = rimH * (1.0 + 0.45 * fbm(rng, shape, max(Rc * 0.35 / s, 1.5), 3))
    if band['kind'] == 'pit':
        # Broken lip: gaps where the noise is low.
        rim_amp *= smoothstep(-0.6, 0.3, fbm(rng, shape, max(Rc * 0.4 / s, 1.5), 2))
    if oblique:
        rim_amp *= 0.55 + 0.75 * smoothstep(-0.6, 0.8, np.cos(th))
    rim = rim_amp * np.where(dr < 0, np.exp(-(dr / w_in) ** 2), np.exp(-(dr / w_out) ** 2))
    # Continuous blanket ~ r^-3 beyond the rim.
    blanket = rimH * 0.35 * np.clip(rn, 1.0, None) ** -3.0 * smoothstep(0.95, 1.3, rn) * smoothstep(3.2, 1.6, rn)
    # Wall slumps only (mid frequency, strongest on the walls); the floor stays a smooth dish.
    slump = fbm(rng, shape, max(Rc * 0.22 / s, 1.5), 3) * depth * 0.045
    walls = smoothstep(0.35, 0.75, rn) * smoothstep(1.15, 0.9, rn)
    fine = fbm(rng, shape, max(Rc * 0.05 / s, 0.8), 3) * depth * 0.025 * smoothstep(1.3, 0.6, rn)
    h = bowl + rim + blanket + slump * walls + fine
    return h, rn


def furrow_height(band, X, Y, rng, shape, s):
    """Grazing hit: a tapering furrow along +X (entry thin at -X, widened/deepened exit), side ridges, heaped torn end."""
    f = band['furrow']
    L = f['L']
    x0 = -L * 0.55
    t = np.clip((X - x0) / L, 0, 1)
    wob = fbm(rng, shape, max(L * 0.1 / s, 1.5), 3) * 0.3
    width = (f['w0'] + (f['w1'] - f['w0']) * t ** 0.8) * (1 + wob)
    along = smoothstep(x0 - f['w0'], x0 + f['w0'], X) * smoothstep(x0 + L + f['w1'] * 0.6, x0 + L - f['w1'] * 0.3, X)
    q = Y / (width * 0.5)
    groove = -f['depth'] * (0.35 + 0.65 * t) * np.clip(1 - q * q, 0, 1) ** 1.3 * along
    ridge = f['ridge'] * (0.5 + 0.5 * t) * np.exp(-((np.abs(q) - 1.25) / 0.4) ** 2) * along
    endx = X - (x0 + L)
    torn = 1.0 + 0.5 * fbm(rng, shape, max(f['w1'] * 0.3 / s, 1.2), 3)
    heap = f['heap'] * torn * np.exp(-(endx / (f['w1'] * 0.7)) ** 2 - (Y / (f['w1'] * 0.8)) ** 2)
    core = np.maximum(smoothstep(1.6, 0.9, np.abs(q)) * along, smoothstep(f['w1'] * 1.1, f['w1'] * 0.4, np.sqrt(endx ** 2 + Y ** 2)))
    ridge_mask = np.exp(-((np.abs(q) - 1.25) / 0.45) ** 2) * along
    return groove + ridge + heap, core, ridge_mask, x0, L


def make_cell(band, variant, seed):
    rng = np.random.default_rng(seed)
    D, Rc = band['D'], band['Rc']
    s = D / CELL
    x0 = -D * 0.5
    coords = (np.arange(CELL) + 0.5) * s + x0
    X, Y = np.meshgrid(coords, coords)       # X = +U (right), Y = +V (down), cm
    shape = (CELL, CELL)
    r = np.sqrt(X * X + Y * Y)
    th = np.arctan2(Y, X)
    polar = Polar(r, th, D * 0.75, s)
    furrow = variant == 1 and 'furrow' in band
    oblique = variant == 1 and not furrow
    kind = band['kind']

    rim_mask = np.zeros(shape)
    rn = None
    if furrow:
        height, core, rim_mask, fx0, fL = furrow_height(band, X, Y, rng, shape, s)
        centre_r = np.sqrt((X - (fx0 + fL)) ** 2 + Y ** 2)   # spray thrown from the exit
    else:
        height, rn = crater_height(band, X, Y, r, th, oblique, rng, shape, s)
        ce = band.get('core_edge')   # B3-B7: dark core stops inside the crest, so a ring of earth reads
        core = smoothstep(ce[0], ce[1], rn) if ce else smoothstep(1.25, 0.85, rn)
        rim_mask = np.exp(-((rn - 1.08) / (band['rimW'] * 1.1)) ** 2) * (kind != 'scuff')
        centre_r = r
        if kind == 'scuff':
            # 20 mm: scuffed, darkened patch around the small pit, elongated a bit (exaggerated for readability).
            sc_n = fbm(rng, shape, max(1.5 / s, 1.0), 3)
            scuff = smoothstep(1.0, 0.45, np.sqrt((X / (Rc * 2.3)) ** 2 + (Y / (Rc * 1.6)) ** 2) * (1 + 0.3 * sc_n))
            core = np.maximum(core, scuff * 0.9)
            height += scuff * fbm(rng, shape, max(0.8 / s, 1.0), 2) * 0.3

    # Downrange weighting for the oblique and furrow variants (0 uprange, 1 downrange); A cells get a lopsided
    # splash towards a random direction instead, so random yaw at runtime gives real variety.
    if furrow or oblique:
        down = smoothstep(-0.55, 0.65, np.cos(th))
        if 'oblique_floor' in band:   # B3-B7: some apron uprange too (no teardrop apex)
            down = band['oblique_floor'] + (1 - band['oblique_floor']) * down
        splash_dir = 0.0
    else:
        splash_dir = rng.uniform(-math.pi, math.pi)
        down = 0.7 + 0.3 * np.cos(angle_wrap(th - splash_dir))

    # ---- rays of thrown soil: 3-5 dominant rays + weaker ones ------------------------------------------------------
    nrays, ray_len = band['rays']
    boost = band.get('ray_boost', 0.0)
    rays = np.zeros(shape)
    ray_angles = []
    ray_weights = []
    base_ang = rng.uniform(0, 2 * math.pi)
    start = Rc * 1.0 if not furrow else 2.0
    n_dom = int(rng.integers(3, 6))
    groups = band.get('ray_groups')   # B3-B7: rays clustered in 3-5 groups, varied length/width (no comb)
    if groups:
        g_ang = base_ang + np.cumsum(rng.uniform(0.6, 1.6, int(rng.integers(groups[0], groups[1] + 1))))
        g_ang = base_ang + (g_ang - base_ang) * 2 * math.pi / (g_ang[-1] - base_ang + rng.uniform(0.6, 1.6))
    for i in range(nrays):
        if furrow or oblique:
            a = float(np.clip(rng.normal(0.0, 0.6), -1.05, 1.05))   # ~120 deg downrange fan
        elif groups:
            a = float(g_ang[i % len(g_ang)] + rng.normal(0, 0.16))
        else:
            a = base_ang + (i + rng.uniform(-0.4, 0.4)) * 2 * math.pi / nrays
        dom = i % max(nrays // n_dom, 1) == 0
        if not (furrow or oblique):
            # Dominant rays lean towards the splash side.
            dom = dom or abs(angle_wrap(a - splash_dir)) < 0.5
        strength = rng.uniform(0.9, 1.0) if dom else rng.uniform(0.6, 0.75)
        length = ray_len * (rng.uniform(0.85, 1.0) if dom else rng.uniform(0.45, 0.75))
        ray_angles.append(a)
        ray_weights.append(3.0 if dom else 1.0)
        dth = angle_wrap(th - a)
        perp = centre_r * np.abs(np.sin(dth))
        along = centre_r * np.cos(dth)
        if 'ray_w_cm' in band:   # B3-B7: absolute ray width, more rays rather than fatter ones
            w0 = band['ray_w_cm'] * (1.25 if dom else 0.8) * rng.uniform(0.6, 1.7)
            length = min(length * rng.uniform(0.75, 1.35), ray_len)
        else:
            w0 = max(Rc * (0.32 + 0.08 * boost) * (1.25 if dom else 0.8), 1.0) if not furrow else 2.2
        prog = np.clip((along - start) / max(length - start, 1.0), 0, 1)
        wcm = w0 * rng.uniform(0.8, 1.2) * (1.0 - 0.7 * prog) + 0.3
        env = smoothstep(start * 0.85, start * 1.25, along) * np.clip(1 - (along - start) / max(length - start, 1), 0, 1) ** 0.6
        rays = np.maximum(rays, np.exp(-(perp / wcm) ** 2) * env * (np.cos(dth) > 0) * strength)
    # Clumpy along their length: thrown soil lands in lumps.
    rays *= np.clip(0.75 + 0.5 * polar(rng, max(ray_len * 0.05, 1.2), 0.03, Rc * 1.2), 0, 1)

    # ---- speck field: on/off coverage that thins out with distance (no translucent wash) ------------------------
    spray_r = band['spray']
    inner_r = Rc * 1.15 if not furrow else 2.0
    sr = centre_r / (0.35 + 0.65 * down) if (furrow or oblique) else centre_r / down
    t = np.clip((sr - inner_r) / max(spray_r - inner_r, 1.0), 0, 1)
    speck_cm = band.get('speck_cm', D * 0.009)   # physical speck size (B0-B2: the original D-relative value)
    speck = polar(rng, max(speck_cm * 2.2 if 'speck_cm' in band else D * 0.02, 0.5), 0.012, Rc * 1.5) if 'speck_arc' not in band else polar(rng, speck_cm * 2.2, band['speck_arc'], Rc * 1.5)
    speck = 0.55 * speck + 0.45 * fft_noise(rng, shape, max(speck_cm / s, 0.9))
    thresh = -0.4 - 1.2 * band['density'] + 2.2 * t ** 0.9
    spray = smoothstep(thresh, thresh + 0.25, speck) * smoothstep(1.0, 0.82, t) * (sr > inner_r * 0.8)
    near_ray = np.clip(cv2.GaussianBlur(rays, (0, 0), max(Rc * 0.25 / s, 2.0)) * 2.5, 0, 1)
    spray *= 0.25 + 0.75 * near_ray
    spray = np.maximum(spray, rays * smoothstep(thresh - 0.6, thresh - 0.2, speck))

    # ---- churned apron: opacity falls off along a curve, streaked along the rays (no flat-alpha plateau) ---------
    ap_lobes = sum(a_ * np.sin(k_ * th + rng.uniform(0, 6.3)) for k_, a_ in ((4, 0.12), (5, 0.08), (6, 0.06)))
    churn_r = (Rc * 1.45 if not furrow else 3.0) * (1 + ap_lobes + 0.1 * fbm(rng, shape, max(Rc * 0.3 / s, 1.5), 3))
    cr = sr if (furrow or oblique) else centre_r / down
    churn = 1.0 - smoothstep(churn_r * 0.85, churn_r * 1.9, cr)
    churn *= np.clip(0.85 + 0.35 * polar(rng, Rc * 0.5, 0.05, Rc * 1.2), 0, 1)
    if furrow:
        churn = np.maximum(churn, core)

    # ---- scorch: on the lip and rays, light in the bowl centre, radially streaked ---------------------------------
    sc_r, sc_k = band['scorch']
    sc_k *= band.get('scorch_mul', 1.0)   # B3-B7: less soot, so the bowl is not an ink blot
    scorch = np.exp(-(centre_r / sc_r) ** 2 * 1.4) if not furrow else np.exp(-(centre_r / sc_r) ** 2) * 0.35
    wedge = np.clip(0.25 + 1.0 * polar(rng, sc_r * 0.9, 0.07, Rc * 0.7), 0, 1.4)
    if band.get('fan'):
        scorch = np.exp(-(centre_r / (sc_r * 1.3)) ** 2 * 1.1) * wedge
    elif not furrow:
        scorch = np.exp(-(centre_r / (sc_r * 1.15)) ** 2 * 1.2) * (0.35 + 0.65 * wedge)
    else:
        scorch = np.maximum(scorch, core * 0.55)
    scorch = np.clip(scorch + rim_mask * band.get('soot_rim', 0.3) + rays * 0.35 * smoothstep(sc_r * 1.8, sc_r * 0.7, centre_r), 0, 1)
    if rn is not None and kind == 'crater':
        scorch *= 1.0 - 0.55 * smoothstep(0.75, 0.1, rn)
    if oblique:
        scorch *= 0.4 + 0.6 * down

    # ---- clods / stones: power-law sizes, big near the rim, mostly along the (dominant) rays, a few in the bowl ---
    clod_mask = np.zeros(shape)
    clod_col = np.zeros(shape + (3,))
    clod_w = np.zeros(shape)
    r_out = D * band.get('clod_r_out', 0.40)
    n_cl, cmin, cmax = band['clods']
    wsum = np.array(ray_weights) / max(sum(ray_weights), 1e-6) if ray_weights else None

    def sample_pos():
        for _ in range(100):
            u = rng.uniform()
            if u < 0.7 and ray_angles:
                a = ray_angles[rng.choice(len(ray_angles), p=wsum)] + rng.normal(0, 0.06)
                rr = start + (ray_len - start) * rng.uniform() ** 1.4
            elif u < 0.82:
                a = (rng.uniform(-math.pi, math.pi) if not (oblique or furrow) else rng.normal(0, 0.7))
                rr = (Rc if not furrow else 2.0) * rng.uniform(1.0, 2.2)
            elif u < 0.97:
                a = rng.uniform(-math.pi, math.pi) if not (oblique or furrow) else rng.normal(0, 0.8)
                rr = inner_r + (r_out - inner_r) * rng.uniform() ** 2.0     # density falls off with r
            else:
                a = rng.uniform(-math.pi, math.pi) if not (oblique or furrow) else rng.normal(0, 0.7)
                rr = Rc * rng.uniform(0.75, 1.0)    # a few low on the walls, none on the floor
            cx, cy = rr * math.cos(a), rr * math.sin(a)
            if furrow:
                cx += band['furrow']['L'] * 0.45
            if math.hypot(cx, cy) < r_out:
                return cx, cy
        return 0.0, 0.0

    def size_at(cx, cy, smin, smax):
        dn = np.clip((math.hypot(cx, cy) - Rc) / max(r_out - Rc, 1.0), 0, 1)
        return smin + (smax - smin) * (1 - dn) ** 1.3 * rng.uniform() ** 3.0

    items = []
    for _ in range(n_cl):
        cx, cy = sample_pos()
        items.append((cx, cy, size_at(cx, cy, cmin, cmax), rng.uniform(0, math.pi)))
    lift = band.get('clod_lift')   # B3-B7: lighter subsoil clods (lit brown, never near black)
    cl_a, cl_b = (CLOD * lift, CLOD_TOP * lift) if lift else (CLOD, CLOD_TOP)
    stamp_blobs(height, clod_mask, clod_col, clod_w, items, x0, s, rng, 0.36, cl_a, cl_b)
    if 'stones' in band:
        n_st, smin, smax = band['stones']
        st = []
        for _ in range(n_st):
            cx, cy = sample_pos()
            st.append((cx, cy, size_at(cx, cy, smin, smax), rng.uniform(0, math.pi)))
        stamp_blobs(height, clod_mask, clod_col, clod_w, st, x0, s, rng, 0.25, STONE * 0.8, STONE)

    # ---- window ----------------------------------------------------------------------------------------------------
    win_noise = fbm(rng, shape, max(D * 0.05 / s, 2.0), 3)
    window = smoothstep(D * 0.445, D * 0.375, r * (1.0 + 0.05 * win_noise))
    hard = (r < D * 0.45).astype(np.float64)

    height = cv2.GaussianBlur(height, (0, 0), 1.2)
    height *= window

    # ---- albedo ------------------------------------------------------------------------------------------------------
    grain = fbm(rng, shape, max(1.0 / s, 0.8), 3)
    mottle = fbm(rng, shape, max(band.get('mottle_cm', D * 0.045) / s, 2.0), 3)
    alb = np.ones(shape + (3,)) * SOIL
    alb = alb * (1 - spray[..., None] * 0.45) + DUST * (spray[..., None] * 0.45)
    alb = alb * (1 - rays[..., None] * 0.85) + RAY * (rays[..., None] * 0.85)
    damp = SOIL_DAMP * np.array(band['core_lift']) if 'core_lift' in band else SOIL_DAMP   # B3-B7: lighter, warmer bowl soil
    alb = alb * (1 - core[..., None]) + damp * core[..., None]
    if band.get('core_edge') and rn is not None:   # B3-B7: the ring of earth is fresh brown soil, not grey haze
        ringm = (smoothstep(1.4, 1.1, rn) * (1 - core) * 0.65)[..., None]
        alb = alb * (1 - ringm) + damp * 1.12 * ringm
    # Fresh dry soil on the rim crest: lighter.
    crest = np.clip(height / max(band['rimH'], 0.2), 0, 1) * (1 - core * 0.4) * (1 - rays)
    alb = alb * (1 - crest[..., None] * 0.55) + DUST * (crest[..., None] * 0.55)
    # Non-directional AO: floor of the bowl at ~45-55 % of the surrounding soil, fading up the walls; cavity term.
    deep = np.clip(-height / max(band['depth'], 0.1), 0, 1)
    blur_big = cv2.GaussianBlur(height, (0, 0), max(Rc * 0.3 / s, 2.0))
    cav = np.clip((blur_big - height) / max(band['depth'] * 0.2, 0.3), 0, 1)
    alb *= (1.0 - 0.18 * deep[..., None] ** 0.8) * (1.0 - 0.08 * cav[..., None])
    alb *= 1.0 + 0.12 * fbm(rng, shape, max(Rc * 0.12 / s, 1.2), 3)[..., None] * core[..., None]   # mottled floor
    alb *= 1.0 + 0.08 * grain[..., None] + 0.06 * mottle[..., None]
    alb = np.maximum(alb, FLOOR_MIN * core[..., None])
    cm = np.clip(clod_w, 0, 1)[..., None]
    alb = alb * (1 - cm) + clod_col * cm
    alb = np.clip(alb, 0.02, 0.9)

    # ---- opacity -----------------------------------------------------------------------------------------------------
    erode = fbm(rng, shape, max(band.get('erode_cm', D * 0.02) / s, 1.2), 3)
    op_churn = np.clip(churn * (0.9 + 0.2 * erode), 0, 0.85)
    op = np.maximum.reduce([core * 0.97, op_churn, rays * 0.88, spray * band.get('spray_op', 0.6), scorch * sc_k * 0.6,
                            clod_mask * 0.98])
    if band.get('core_edge') and rn is not None:   # B3-B7: the ring of earth around the bowl is solid, not haze
        op = np.maximum(op, 0.94 * smoothstep(1.38, 1.12, rn) * (0.9 + 0.1 * erode))
    op *= window * hard

    # ---- roughness ---------------------------------------------------------------------------------------------------
    rough = 0.92 - 0.04 * core - 0.20 * cm[..., 0] + 0.02 * spray + 0.02 * grain
    rough = np.clip(rough, 0.68, 0.97)
    scorch_mask = np.clip(scorch * sc_k / 0.5, 0, 1) * window

    g = hard * window
    alb = alb * g[..., None] + NEUTRAL * (1 - g[..., None])
    return dict(height=height, albedo=alb, opacity=op, rough=rough, scorch=scorch_mask, s=s)


def height_to_normal(height, s, strength=1.0):
    gy, gx = np.gradient(height, s)          # dh/dV (rows, down), dh/dU (cols, right)
    g = np.sqrt(gx * gx + gy * gy)
    k = 1.0 / np.sqrt(1.0 + (g / MAX_SLOPE) ** 2)    # soft cap: slopes saturate towards MAX_SLOPE
    gx, gy = gx * k, gy * k
    nx, ny, nz = -gx * strength, -gy * strength, np.ones_like(height)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    return nx / l, ny / l, nz / l


def build(out_dir, seed=1234):
    os.makedirs(out_dir, exist_ok=True)
    W = CELL * GRID
    bco = np.zeros((W, W, 4), np.float64)
    nrs = np.zeros((W, W, 4), np.float64)
    heights = np.zeros((16, CELL, CELL), np.float32)
    meta = []
    for b, band in enumerate(BANDS):
        for v in range(2):
            i = 2 * b + v
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
            meta.append(dict(cell=i, band=band['name'], variant='AB'[v], D=band['D'], calibre=band['calibre']))
            print('cell', i, band['name'], 'AB'[v], 'h[min,max]=%.1f,%.1f' % (c['height'].min(), c['height'].max()))

    rng = np.random.default_rng(seed)

    def save_rgba(path, a, dither=False):
        if dither:
            a = a + (rng.uniform(-0.5, 0.5, a.shape) / 255.0)
        a8 = np.clip(np.round(a * 255), 0, 255).astype(np.uint8)
        cv2.imwrite(path, cv2.cvtColor(a8, cv2.COLOR_RGBA2BGRA))

    save_rgba(os.path.join(out_dir, 'T_LandscapeImpact_BCO.png'), bco, dither=True)
    save_rgba(os.path.join(out_dir, 'T_LandscapeImpact_NRS.png'), nrs, dither=True)
    np.save(os.path.join(out_dir, 'height_cells.npy'), heights)
    with open(os.path.join(out_dir, 'atlas_meta.json'), 'w') as f:
        json.dump(dict(cells=meta, bands=BANDS), f, indent=1)
    print('wrote', out_dir)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/r1')
    ap.add_argument('--seed', type=int, default=1234)
    a = ap.parse_args()
    build(a.out, a.seed)
