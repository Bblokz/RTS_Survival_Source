"""Procedural tileable textures for the Berlin ruin kit material (M_BerlinRuin), plus a preview sheet.

Writes to <project>/Saved/Berlin/ruinkit_textures/ (2048 x 2048, every map tiles on both axes, 1 tile = 4 m):
  T_BerlinRuin_Plaster_D.png  RGB weathered grey-beige lime plaster (sRGB), A = chip height (linear, uniform 0..1:
                              the material thresholds it against vertex G, so plaster is lost where A < G)
  T_BerlinRuin_Brick_D.png    RGB dull red-brown Berlin brick in lime mortar, Blockverband, 52 courses (7.7 cm incl.
                              joint) x 16 stretchers per tile, A = cavity (1 brick face .. 0 deep joint)
  T_BerlinRuin_N.png          RG brick normal XY, BA plaster normal XY (linear, DirectX green, 128 = flat)
  T_BerlinRuin_Rubble_D.png   RGB brick/plaster/mortar rubble with charred timber (sRGB), A = height (uniform 0..1)
  ruinkit_preview.png         3 x 3 tiles of each map + a simulated wall panel (same maths as the material) at RTS
                              distance (4 cm/px) and close (1 cm/px)

numpy + Pillow only, deterministic. Usage:  python ruin_textures_gen.py [--no-preview] [--preview-only]
Docs: Source/Docs/Environment/BerlinRuinKitMaterial.md
"""
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ruin_params as RP  # noqa: E402

N = 2048
PX = RP.TILE_M / N  # metres per pixel (1.95 mm)
F32 = np.float32
PROJECT = Path(__file__).resolve().parents[4]
OUT_DIR = PROJECT / 'Saved' / 'Berlin' / 'ruinkit_textures'

_YY, _XX = np.mgrid[0:N, 0:N].astype(F32)
_YY += 0.5
_XX += 0.5
_FY = (np.fft.fftfreq(N) * N).astype(F32)[:, None]
_FX = (np.fft.rfftfreq(N) * N).astype(F32)[None, :]
_FR = np.sqrt(_FY ** 2 + _FX ** 2)


# ----------------------------------------------------------------------------- toolkit

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _norm(a):
    a = a - a.mean()
    return (a / (a.std() + 1e-12)).astype(F32)


def fnoise(rng, fmin, fmax, alpha=1.0, aniso=1.0, angle=0.0):
    """Periodic band-limited noise, std 1. Frequencies in cycles per tile. aniso > 1 stretches along `angle`."""
    w = rng.standard_normal((N, N)).astype(F32)
    F = np.fft.rfft2(w)
    if aniso != 1.0:
        c, s = np.cos(angle), np.sin(angle)
        fu = _FX * c + _FY * s
        fv = -_FX * s + _FY * c
        f = np.sqrt((fu * aniso) ** 2 + (fv / aniso) ** 2)
    else:
        f = _FR
    f = np.maximum(f, 1e-3)
    amp = f ** (-alpha)
    amp *= smoothstep(fmin * 0.7, fmin, f) * (1.0 - smoothstep(fmax, fmax * 1.4, f))
    amp[0, 0] = 0.0
    return _norm(np.fft.irfft2(F * amp, s=(N, N)))


def blur(img, sigma):
    g = np.exp(-2.0 * (np.pi ** 2) * (sigma ** 2) * ((_FR / N) ** 2))
    if img.ndim == 2:
        return np.fft.irfft2(np.fft.rfft2(img) * g, s=(N, N)).astype(F32)
    return np.stack([blur(img[..., c], sigma) for c in range(img.shape[-1])], -1)


def worley(rng, g, jitter=0.9, x=None, y=None):
    """Wrapped Voronoi on a g x g jittered grid: F1, F2 (px), cell id, offset to the nearest point."""
    cs = N / g
    pts = (rng.random((g, g, 2)) * jitter + (1 - jitter) / 2).astype(F32)
    if x is None:
        x, y = _XX, _YY
    cx = np.floor(x / cs).astype(np.int32)
    cy = np.floor(y / cs).astype(np.int32)
    F1 = np.full(x.shape, 1e9, F32)
    F2 = np.full(x.shape, 1e9, F32)
    cid = np.zeros(x.shape, np.int32)
    ox = np.zeros(x.shape, F32)
    oy = np.zeros(x.shape, F32)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ncx = cx + dx
            ncy = cy + dy
            px = (ncx + pts[ncy % g, ncx % g, 0]) * cs
            py = (ncy + pts[ncy % g, ncx % g, 1]) * cs
            ddx = x - px
            ddy = y - py
            d = np.sqrt(ddx * ddx + ddy * ddy)
            closer = d < F1
            F2 = np.where(closer, F1, np.minimum(F2, d))
            F1 = np.where(closer, d, F1)
            cid = np.where(closer, (ncy % g) * g + (ncx % g), cid)
            ox = np.where(closer, ddx, ox)
            oy = np.where(closer, ddy, oy)
    return F1, F2, cid, ox, oy


def equalize(a):
    """Rank-map to a uniform 0..1 distribution (the material thresholds these against vertex colour)."""
    flat = a.ravel()
    order = np.argsort(flat, kind='stable')
    out = np.empty(flat.size, F32)
    out[order] = np.linspace(0.0, 1.0, flat.size, dtype=F32)
    return out.reshape(a.shape)


def height_to_normal(H, strength=1.0):
    """DirectX tangent-space normal (green = -dh/drow) from a height in metres (same as berlin_textures_gen)."""
    dhdx = (np.roll(H, -1, 1) - np.roll(H, 1, 1)) / (2 * PX) * strength
    dhdy = (np.roll(H, -1, 0) - np.roll(H, 1, 0)) / (2 * PX) * strength
    n = np.stack([-dhdx, -dhdy, np.ones_like(H)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n.astype(F32)


def srgb(c):
    return np.array(c, F32)


def to_u8(x, lo=0):
    return np.clip(np.clip(x, 0, 1) * 255 + 0.5, lo, 255).astype(np.uint8)


def srgb_to_lin(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(F32)


def lin_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055).astype(F32)


def stamp_rect(H, A, cx, cy, half_len, half_wid, angle, height, bevel_px, color_fn, base_h):
    """Z-buffered rotated box (wrapping): height plateau with a bevel; color_fn(u, v, mask) -> RGB patch."""
    r = int(np.ceil(np.hypot(half_len, half_wid))) + 2
    ys = np.arange(-r, r + 1, dtype=F32)[:, None] + (cy - np.floor(cy))
    xs = np.arange(-r, r + 1, dtype=F32)[None, :] + (cx - np.floor(cx))
    c, s = np.cos(angle), np.sin(angle)
    u = xs * c + ys * s
    v = -xs * s + ys * c
    e = np.minimum(half_len - np.abs(u), half_wid - np.abs(v))
    m = e > 0
    if not m.any():
        return
    ph = base_h + height * smoothstep(0, bevel_px, e)
    rows = (np.arange(-r, r + 1) + int(np.floor(cy))) % N
    cols = (np.arange(-r, r + 1) + int(np.floor(cx))) % N
    ix = np.ix_(rows, cols)
    cur = H[ix]
    w = m & (ph > cur)
    if not w.any():
        return
    H[ix] = np.where(w, ph, cur)
    A[ix] = np.where(w[..., None], color_fn(u, v, e), A[ix])


# ----------------------------------------------------------------------------- plaster

def make_plaster(seed=7101):
    rng = np.random.default_rng(seed)
    # Chip height: big loss blotches + medium + angular flakes + cracks, equalised to uniform 0..1.
    big = fnoise(rng, 1.5, 8, 1.2)
    mid = fnoise(rng, 8, 50, 0.9)
    fine = fnoise(rng, 50, 420, 0.6)
    F1, F2, cid, _, _ = worley(rng, 36, 0.95, _XX + 9 * fnoise(rng, 10, 60), _YY + 9 * fnoise(rng, 10, 60))
    flake = rng.random(36 * 36).astype(F32)[cid] - 0.5          # per-flake step: angular chip edges
    Fc1, Fc2, _, _, _ = worley(rng, 9, 0.9, _XX + 30 * fnoise(rng, 4, 30), _YY + 30 * fnoise(rng, 4, 30))
    crack_line = np.exp(-(Fc2 - Fc1) / 2.2)
    crack_on = smoothstep(0.2, 0.9, fnoise(rng, 2, 10))           # only some cell borders are cracked
    crack = crack_line * crack_on
    chip = 0.95 * big + 0.55 * mid + 0.22 * fine + 0.55 * flake - 1.0 * crack
    Hc = equalize(chip)

    # Albedo.
    base = srgb((0.630, 0.612, 0.572))
    lum = (1.0 + 0.065 * fnoise(rng, 1.5, 10) + 0.045 * fnoise(rng, 10, 60) + 0.03 * fnoise(rng, 150, 700, 0.3))
    streak = fnoise(rng, 3, 60, 0.8, aniso=6.0, angle=np.pi / 2)  # vertical run-off streaks
    lum *= 1.0 - 0.06 * np.clip(streak, 0, None)
    lum *= 0.93 + 0.07 * smoothstep(0.0, 0.5, Hc)                  # thin (first to fall) plaster is dirtier
    lum *= 1.0 - 0.28 * crack_line * smoothstep(0.0, 0.6, crack_on)
    warm = fnoise(rng, 1, 6)
    hue = np.stack([1 + 0.012 * warm, 1 + 0.004 * warm, 1 - 0.016 * warm], -1)
    A = base[None, None, :] * lum[..., None] * hue
    # Repair patches: rectangles of a slightly different plaster, flat edges.
    for _ in range(9):
        cx, cy = rng.uniform(0, N, 2)
        hw, hh = rng.uniform(60, 260), rng.uniform(50, 200)
        dx = np.abs(((_XX - cx + N / 2) % N) - N / 2)
        dy = np.abs(((_YY - cy + N / 2) % N) - N / 2)
        m = smoothstep(1.5, -1.5, np.maximum(dx - hw, dy - hh))
        tone = srgb((1.0, 1.0, 1.0)) * rng.uniform(0.9, 1.08) * np.array([1, 1, rng.uniform(0.96, 1.03)], F32)
        A = A * (1 + m[..., None] * (tone - 1))
    A = np.clip(A, 0, 1)

    # Relief (metres): Rauputz grain + undulation + cracks + patch rims.
    Hr = (0.00045 * fnoise(rng, 180, 900, 0.2) + 0.0018 * fnoise(rng, 4, 40, 1.0)
          - 0.0018 * crack_line * crack_on)
    return A, Hc, Hr


# ----------------------------------------------------------------------------- brick

BRICK_PALETTE = [  # sRGB, weight
    ((0.470, 0.300, 0.240), 0.34),   # dull red-brown
    ((0.410, 0.255, 0.210), 0.26),   # darker red
    ((0.520, 0.345, 0.265), 0.14),   # orange-red
    ((0.315, 0.215, 0.195), 0.14),   # overburnt clinker
    ((0.440, 0.330, 0.280), 0.08),   # pale brown
    ((0.600, 0.505, 0.380), 0.04),   # yellow Berlin brick
]


def make_brick(seed=7202):
    rng = np.random.default_rng(seed)
    courses = RP.BRICK_COURSES_PER_TILE
    ch = N / courses
    c = np.floor(_YY / ch).astype(np.int32)
    vy = _YY - c * ch
    header = (c % 2) == 1
    L = np.where(header, N / (2 * RP.BRICK_STRETCHERS_PER_TILE), N / RP.BRICK_STRETCHERS_PER_TILE).astype(F32)
    off = np.where(header, N / (4 * RP.BRICK_STRETCHERS_PER_TILE), 0.0).astype(F32)
    xs = _XX - off
    i = np.floor(xs / L).astype(np.int32)
    ux = xs - i * L
    count = np.where(header, 2 * RP.BRICK_STRETCHERS_PER_TILE, RP.BRICK_STRETCHERS_PER_TILE)
    i = i % count
    bid = c * 64 + i
    nb = courses * 64

    joint_half = 0.006 / PX                                      # 1.2 cm joint
    jitter = rng.uniform(-0.8, 0.8, (nb, 4)).astype(F32)         # per-brick edge jitter (px): left right top bottom
    dl = ux + jitter[bid, 0]
    dr = L - ux + jitter[bid, 1]
    dt = vy + jitter[bid, 2]
    db = ch - vy + jitter[bid, 3]
    edge = np.minimum(np.minimum(dl, dr), np.minimum(dt, db)) - joint_half
    chip = np.clip(1.3 * fnoise(rng, 60, 400, 0.4) + 0.2, 0, None) * 1.6 + \
        np.clip(fnoise(rng, 8, 60) - 1.2, 0, None) * 4.0        # chipped arrises, a few big bites
    e = edge - chip
    face = smoothstep(-0.6, 0.8, e)

    # Per-brick colour.
    cols = np.array([p for p, _ in BRICK_PALETTE], F32)
    wts = np.array([w for _, w in BRICK_PALETTE], np.float64)
    pick = rng.choice(len(cols), nb, p=wts / wts.sum())
    bcol = cols[pick] * rng.uniform(0.88, 1.10, (nb, 1)).astype(F32) * \
        (1 + rng.uniform(-0.03, 0.03, (nb, 3))).astype(F32)
    Ab = bcol[bid]
    mott = 1 + 0.06 * fnoise(rng, 40, 260, 0.6) + 0.035 * fnoise(rng, 300, 900, 0.2)
    flash = rng.uniform(-0.12, 0.12, nb).astype(F32)[bid] * (ux / L - 0.5) * 2   # darker burnt end
    Ab = Ab * (mott * (1 + flash))[..., None]
    # Mortar / old plaster residue on brick faces.
    resid = smoothstep(1.25, 1.9, fnoise(rng, 12, 120, 0.7)) * 0.75
    mortar_c = srgb((0.545, 0.525, 0.485))
    Ab = Ab * (1 - resid[..., None]) + mortar_c * resid[..., None]

    # Mortar.
    mlum = 1 + 0.07 * fnoise(rng, 20, 300, 0.5)
    deep = smoothstep(0.3, 1.6, fnoise(rng, 10, 80))             # eroded, deeper joints (darker)
    # Dirty, darker joints: mortar close to the brick mean keeps the 7.7 cm course frequency weak in the mips (no shimmer).
    Am = mortar_c[None, None, :] * 0.80 * (mlum * (1 - 0.22 * deep))[..., None]

    A = Am * (1 - face[..., None]) + Ab * face[..., None]
    # Wall-scale grime and colour drift so 4 m tiles do not repeat as a flat pattern.
    grime = 1 + 0.07 * fnoise(rng, 1.5, 10) + 0.03 * fnoise(rng, 10, 40)
    warm = fnoise(rng, 1, 6)
    A = A * grime[..., None] * np.stack([1 + 0.02 * warm, 1 + 0.0 * warm, 1 - 0.02 * warm], -1)
    A = np.clip(A, 0, 1)

    # Height (m): brick face with per-brick set-back and tilt, rounded arris, recessed joint.
    setback = rng.uniform(-0.0007, 0.0007, nb).astype(F32)[bid]
    tilt = rng.uniform(-0.00002, 0.00002, (nb, 2)).astype(F32)
    hb = setback + tilt[bid, 0] * (ux - L / 2) + tilt[bid, 1] * (vy - ch / 2) + 0.0003 * fnoise(rng, 60, 500, 0.4)
    hb -= 0.0025 * (1 - smoothstep(0, 4.0, e))
    hm = -0.0055 - 0.003 * deep + 0.0006 * fnoise(rng, 80, 600, 0.3)
    H = hm + (hb - hm) * smoothstep(-1.0, 2.0, e) + 0.0012 * resid
    cav = np.clip(0.38 * (1 - face) * (1 - 0.6 * deep) + face * (0.85 + 0.15 * smoothstep(0, 5, e)), 0, 1)
    cav = np.clip(cav - 0.25 * resid * 0, 0, 1)
    return A, cav, H


# ----------------------------------------------------------------------------- rubble

RUBBLE_TYPES = [  # name, sRGB, weight
    ('brick', (0.455, 0.290, 0.235), 0.42),
    ('brick_dark', (0.345, 0.225, 0.195), 0.12),
    ('plaster', (0.600, 0.580, 0.540), 0.20),
    ('mortar', (0.520, 0.500, 0.460), 0.14),
    ('char', (0.085, 0.075, 0.068), 0.07),
    ('stone', (0.560, 0.545, 0.510), 0.05),
]


def make_rubble(seed=7303):
    rng = np.random.default_rng(seed)
    cols = np.array([c for _, c, _ in RUBBLE_TYPES], F32)
    wts = np.array([w for _, _, w in RUBBLE_TYPES], np.float64)
    dust_c = srgb((0.50, 0.475, 0.435))
    # Ground: dust and grit.
    H = 0.004 * fnoise(rng, 4, 60) + 0.0015 * fnoise(rng, 100, 700, 0.3) - 0.02
    A = np.broadcast_to(dust_c, (N, N, 3)).copy() * (1 + 0.06 * fnoise(rng, 20, 400, 0.5))[..., None]
    # Fragment layers on wrapped Voronoi cells (small grit .. plaster slabs).
    warp = (6 * fnoise(rng, 20, 120), 6 * fnoise(rng, 20, 120))
    for g, dens, hmax, bevel, gap in ((70, 0.75, 0.018, 2.5, 1.6), (34, 0.70, 0.035, 4.0, 2.2),
                                      (16, 0.45, 0.05, 6.0, 3.0), (8, 0.12, 0.05, 9.0, 4.0)):
        F1, F2, cid, ox, oy = worley(rng, g, 0.95, _XX + warp[0], _YY + warp[1])
        k = g * g
        present = rng.random(k) < dens
        typ = rng.choice(len(cols), k, p=wts / wts.sum())
        lumj = rng.uniform(0.85, 1.12, k).astype(F32)
        base = rng.uniform(0.2, 1.0, k).astype(F32) * hmax
        tx, ty = (rng.uniform(-1, 1, (2, k)) * hmax / (N / g) * 0.6).astype(F32)
        e = (F2 - F1) * 0.5 - gap
        ph = base[cid] + tx[cid] * ox + ty[cid] * oy - hmax * 0.6 * (1 - smoothstep(0, bevel, e))
        m = present[cid] & (e > 0)
        w = m & (ph > H)
        col = cols[typ][cid] * lumj[cid][..., None] * (1 + 0.05 * fnoise(rng, 80, 500, 0.4))[..., None]
        H = np.where(w, ph, H)
        A = np.where(w[..., None], col, A)
    # Whole and half bricks.
    for _ in range(110):
        cx, cy = rng.uniform(0, N, 2)
        half_len = (0.125 if rng.random() < 0.6 else 0.07) / PX
        colr = cols[rng.choice([0, 0, 0, 1])] * rng.uniform(0.85, 1.1)
        stamp_rect(H, A, cx, cy, half_len, 0.06 / PX, rng.uniform(0, np.pi), 0.06, 4.0,
                   lambda u, v, e, colr=colr: np.broadcast_to(colr, u.shape + (3,)) *
                   (1 - 0.25 * (1 - smoothstep(0, 3, e)))[..., None], rng.uniform(0.0, 0.03))
    # Charred timber: joists and boards, cracked char.
    for _ in range(6):
        cx, cy = rng.uniform(0, N, 2)
        half_len = rng.uniform(0.18, 0.5) / PX
        half_wid = rng.uniform(0.025, 0.06) / PX
        burnt = rng.random() < 0.75
        charc = srgb((0.075, 0.066, 0.060)) if burnt else srgb((0.30, 0.22, 0.15))

        def timber(u, v, e, charc=charc, burnt=burnt):
            grain = 1 + 0.25 * np.sin(v * 0.9 + 0.3 * np.sin(u * 0.05)) * (0.3 if burnt else 1.0)
            alligator = (np.sin(u * 0.35) * np.sin(v * 0.8 + u * 0.02) > 0.85) * (0.6 if burnt else 0.0)
            return np.broadcast_to(charc, u.shape + (3,)) * (grain * (1 + alligator))[..., None]

        stamp_rect(H, A, cx, cy, half_len, half_wid, rng.uniform(0, np.pi), 0.045, 3.0, timber, rng.uniform(0.0, 0.02))
    # Dust settles in the gaps, cavity shading baked into the albedo (no normal map on rubble).
    cav = np.clip((blur(H, 5.0) - H) * 60.0, 0, 1)
    dusty = smoothstep(0.0, 1.4, fnoise(rng, 3, 30)) * 0.45
    A = A * (1 - 0.45 * cav[..., None])
    A = A * (1 - dusty[..., None]) + dust_c * dusty[..., None]
    lit = np.clip(1 + (np.roll(H, 1, 0) - np.roll(H, -1, 0) + np.roll(H, 1, 1) - np.roll(H, -1, 1)) * 25.0, 0.75, 1.25)
    A = A * lit[..., None]
    A = A * 0.82 + dust_c * 0.18                                  # settled dust ties the fragments together
    return np.clip(A, 0, 1), equalize(H)


# ----------------------------------------------------------------------------- material simulation (preview)

def _down(img, f):
    """Box downsample by an integer factor (what the mip chain does)."""
    h, w = img.shape[:2]
    return img.reshape(h // f, f, w // f, f, *img.shape[2:]).mean((1, 3))


def _tile(img, tw, th):
    reps = (int(np.ceil(th / img.shape[0])), int(np.ceil(tw / img.shape[1]))) + (1,) * (img.ndim - 2)
    return np.tile(img, reps)[:th, :tw]


def wall_vertex_colours(w, h, res):
    """A fake painted ruin facade (metres -> px at `res` m/px): openings, ragged top, R / G / B fields."""
    rng = np.random.default_rng(99)
    y, x = np.mgrid[0:h, 0:w].astype(F32) * res
    hm = h * res
    top = hm - 0.6 - 1.4 * np.abs(np.sin(x * 0.45)) - 0.5 * np.sin(x * 1.7) ** 2   # ragged broken top (m)
    zz = hm - y                                                                       # height above ground
    solid = zz < top
    windows = np.zeros_like(solid)
    soot_src = np.zeros_like(x)
    for storey in range(3):
        for col in range(5):
            wx0 = 1.0 + col * 2.4
            wz0 = 1.0 + storey * 3.6
            win = (x > wx0) & (x < wx0 + 1.2) & (zz > wz0) & (zz < wz0 + 1.9)
            windows |= win
            # soot plume above the opening
            dx = (x - (wx0 + 0.6)) / 0.9
            dz = zz - (wz0 + 1.9)
            soot_src = np.maximum(soot_src, np.exp(-dx * dx) * np.clip(1 - dz / 2.2, 0, 1) * (dz > -0.1) *
                                  (0.6 + 0.4 * ((storey + col) % 2)))
    solid &= ~windows
    # G: plaster loss near the broken top, around openings (smooth: it is vertex-interpolated), lower wall.
    near_top = np.clip(1 - (top - zz) / 2.5, 0, 1)
    from_win = np.zeros_like(x)
    for storey in range(3):
        for col in range(5):
            wx0 = 1.0 + col * 2.4
            wz0 = 1.0 + storey * 3.6
            ddx = np.maximum(np.maximum(wx0 - x, x - (wx0 + 1.2)), 0)
            ddz = np.maximum(np.maximum(wz0 - zz, zz - (wz0 + 1.9)), 0)
            from_win = np.maximum(from_win, np.clip(1 - np.hypot(ddx, ddz) / 0.9, 0, 1) * 0.75)
    low = np.clip(1 - zz / 1.2, 0, 1) * 0.55
    G = np.clip(np.maximum.reduce([near_top, from_win, low]) + 0.25 * np.sin(x * 0.6 + 1) * np.sin(zz * 0.5), 0, 1)
    G[:, int(w * 0.62):int(w * 0.8)] = np.clip(G[:, int(w * 0.62):int(w * 0.8)] + 0.45, 0, 1)  # a stripped bay
    R = np.clip(soot_src + near_top * 0.35, 0, 1)
    B = np.clip(1 - zz / 1.3, 0, 1) ** 1.3
    return solid, R.astype(F32), G.astype(F32), B.astype(F32)


def shade_wall(maps, f, w_m=12.0, h_m=9.0, soot_amount=1.0, tint=(1, 1, 1)):
    """Evaluate the M_BerlinRuin wall maths (see ruin_material_build.py) on a facade, then light it."""
    S, V = RP.SCALARS, RP.VECTORS
    tex_px_m = N / RP.TILE_M
    P = _down(maps['plaster'], f)
    Hc = _down(maps['chip'][..., None], f)[..., 0]
    Bc = _down(maps['brick'], f)
    Cv = _down(maps['cav'][..., None], f)[..., 0]
    Nn = _down(maps['n'], f)
    w, h = int(w_m * tex_px_m / f), int(h_m * tex_px_m / f)
    P, Hc, Bc, Cv, Nn = (_tile(a, w, h) for a in (P, Hc, Bc, Cv, Nn))
    solid, R, G, B = wall_vertex_colours(w, h, f / tex_px_m)
    lin = srgb_to_lin
    g = np.clip(G + S['RuinPlasterLoss'], 0, 1)
    ew = S['RuinPlasterEdgeWidth']
    t = g * (1 + 2 * ew) - ew
    d = (Hc - t) / ew
    plaster = np.clip(d, 0, 1)
    edge = np.clip(2 - d, 0, 1) * plaster
    shadow = np.clip(1 + d * 0.5, 0, 1) * (1 - plaster)
    pc = lin(P) * np.array(V['RuinPlasterColor'], F32) * (1 + edge * S['RuinPlasterEdgeLight'])[..., None]
    bc = lin(Bc) * np.array(V['RuinBrickColor'], F32) * (1 - shadow * S['RuinPlasterEdgeShadow'])[..., None]
    base = bc + (pc - bc) * plaster[..., None]
    rough_b = S['RuinBrickRoughJoint'] + (S['RuinBrickRoughFace'] - S['RuinBrickRoughJoint']) * Cv
    rough = rough_b + (S['RuinPlasterRoughness'] - rough_b) * plaster
    nxy = (Nn[..., 0:2] + (Nn[..., 2:4] - Nn[..., 0:2]) * plaster[..., None]) * 2 - 1
    nrm = np.concatenate([nxy * S['RuinNormalStrength'], np.ones_like(nxy[..., :1])], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    noise = Cv + (Hc - Cv) * plaster
    base = base * np.array(tint, F32)
    soot = np.clip(R * soot_amount * (S['RuinSootStrength'] + S['RuinSootBreakup'] * (noise - 0.5)), 0, 1)
    base = base + (base * np.array(V['RuinSootColor'], F32) - base) * soot[..., None]
    dust = np.clip(B * (S['RuinDustStrength'] + S['RuinDustBreakup'] * (0.5 - noise)), 0, 1) * S['RuinDustOpacity']
    base = base + (np.array(V['RuinDustColor'], F32) - base) * dust[..., None]
    rough = rough + (S['RuinMatteRoughness'] - rough) * np.maximum(soot, dust)
    # Light: low sun from upper left, slightly grazing; camera frontal-ish.
    L = np.array([-0.45, -0.55, 0.70], F32)   # DirectX tangent frame, y down the image
    L /= np.linalg.norm(L)
    ndl = np.clip((nrm * L).sum(-1), 0, None)
    lit = 0.35 + 0.9 * ndl
    out = base * lit[..., None]
    out = np.where(solid[..., None], out, lin(np.array([0.05, 0.05, 0.055], F32)))
    return lin_to_srgb(np.clip(out * 1.15, 0, 1))


def preview(maps):
    from PIL import ImageDraw
    tiles = []
    labels = []
    def add(img, label):
        tiles.append(np.clip(img, 0, 1))
        labels.append(label)
    small = lambda a: _down(a, 8)  # 256 px per tile -> 3x3 = 768 px
    add(np.tile(small(maps['plaster']), (3, 3, 1)), 'Plaster_D RGB (12 m)')
    add(np.tile(small(np.repeat(maps['chip'][..., None], 3, -1)), (3, 3, 1)), 'Plaster_D A chip height')
    add(np.tile(small(maps['brick']), (3, 3, 1)), 'Brick_D RGB (12 m)')
    add(np.tile(small(np.repeat(maps['cav'][..., None], 3, -1)), (3, 3, 1)), 'Brick_D A cavity')
    nb = np.concatenate([maps['n'][..., 0:2], np.ones_like(maps['n'][..., :1])], -1)
    npl = np.concatenate([maps['n'][..., 2:4], np.ones_like(maps['n'][..., :1])], -1)
    add(np.tile(small(nb), (3, 3, 1)), 'N RG brick normal')
    add(np.tile(small(npl), (3, 3, 1)), 'N BA plaster normal')
    add(np.tile(small(maps['rubble']), (3, 3, 1)), 'Rubble_D RGB (12 m)')
    add(np.tile(small(np.repeat(maps['rh'][..., None], 3, -1)), (3, 3, 1)), 'Rubble_D A height')
    cell = 768
    sheet_w = cell * 4 + 50
    rts = shade_wall(maps, 16)              # 12 x 9 m at 3.1 cm/px (box-filtered like a mip) -> 384 x 288
    rts_soot0 = shade_wall(maps, 16, soot_amount=0.0, tint=(1.08, 1.0, 0.88))
    close = shade_wall(maps, 4, w_m=7.5, h_m=6.0)
    rts_big = np.repeat(np.repeat(rts, 3, 0), 3, 1)
    rts0_big = np.repeat(np.repeat(rts_soot0, 3, 0), 3, 1)
    rows_h = cell * 2 + 40 + max(rts_big.shape[0], close.shape[0]) + 40
    sheet = np.full((rows_h, sheet_w, 3), 0.16, F32)
    for k, img in enumerate(tiles):
        r, c = divmod(k, 4)
        y0, x0 = 20 + r * (cell + 10), 10 + c * (cell + 10)
        sheet[y0:y0 + cell, x0:x0 + cell] = img[:cell, :cell]
    y0 = 20 + 2 * (cell + 10) + 20
    sheet[y0:y0 + rts_big.shape[0], 10:10 + rts_big.shape[1]] = rts_big
    x1 = 10 + rts_big.shape[1] + 10
    sheet[y0:y0 + rts0_big.shape[0], x1:x1 + rts0_big.shape[1]] = rts0_big
    x2 = x1 + rts0_big.shape[1] + 10
    cw = min(close.shape[1], sheet_w - x2 - 10)
    sheet[y0:y0 + close.shape[0], x2:x2 + cw] = close[:, :cw]
    im = Image.fromarray(to_u8(sheet))
    dr = ImageDraw.Draw(im)
    for k, lab in enumerate(labels):
        r, c = divmod(k, 4)
        dr.text((10 + c * (cell + 10) + 4, 20 + r * (cell + 10) + 4), lab, fill=(255, 255, 0))
    dr.text((14, y0 + 4), 'RTS view 3.1 cm/px (x3 nearest), soot 1', fill=(255, 255, 0))
    dr.text((x1 + 4, y0 + 4), 'RTS view, CPD soot 0 + warm tint', fill=(255, 255, 0))
    dr.text((x2 + 4, y0 + 4), 'close 0.78 cm/px', fill=(255, 255, 0))
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    im.save(OUT_DIR / 'ruinkit_preview.png')
    # Also a pure RTS-scale image (no upscale) to judge shimmer / mush.
    Image.fromarray(to_u8(rts)).save(OUT_DIR / 'ruinkit_preview_rts_native.png')


# ----------------------------------------------------------------------------- main

def save_rgba(name, rgb, a):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    img = np.concatenate([to_u8(rgb), to_u8(a, 1)[..., None]], -1)   # alpha >= 1: zero alpha can alter RGB on import
    Image.fromarray(img, 'RGBA').save(OUT_DIR / f'{name}.png', optimize=False)


def load_maps():
    def rd(name):
        return np.asarray(Image.open(OUT_DIR / f'{name}.png')).astype(F32) / 255
    p, b, n, r = (rd('T_BerlinRuin_Plaster_D'), rd('T_BerlinRuin_Brick_D'), rd('T_BerlinRuin_N'),
                  rd('T_BerlinRuin_Rubble_D'))
    return {'plaster': p[..., :3], 'chip': p[..., 3], 'brick': b[..., :3], 'cav': b[..., 3], 'n': n,
            'rubble': r[..., :3], 'rh': r[..., 3]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--no-preview', action='store_true')
    ap.add_argument('--preview-only', action='store_true')
    args = ap.parse_args()
    if not args.preview_only:
        Ap, Hc, Hr = make_plaster()
        save_rgba('T_BerlinRuin_Plaster_D', Ap, Hc)
        print('plaster done')
        Ab, cav, Hb = make_brick()
        save_rgba('T_BerlinRuin_Brick_D', Ab, cav)
        print('brick done')
        nb = height_to_normal(Hb, 1.0)
        npl = height_to_normal(Hr, 1.0)
        npack = np.concatenate([nb[..., 0:2], npl[..., 0:2]], -1) * 0.5 + 0.5
        Image.fromarray(np.clip(npack * 255 + 0.5, 1, 255).astype(np.uint8), 'RGBA').save(
            OUT_DIR / 'T_BerlinRuin_N.png', optimize=False)
        print('normal done')
        Ar, Hrub = make_rubble()
        save_rgba('T_BerlinRuin_Rubble_D', Ar, Hrub)
        print('rubble done')
    if not args.no_preview:
        preview(load_maps())
        print('preview done ->', OUT_DIR / 'ruinkit_preview.png')


if __name__ == '__main__':
    main()
