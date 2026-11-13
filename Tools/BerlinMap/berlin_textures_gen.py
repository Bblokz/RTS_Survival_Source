"""Procedural tileable landscape ground textures for the Ger_Berlin mission map.

Six paint layers, each written as two 2048x2048 PNGs to <project>/Saved/Berlin/textures/:
  T_Berlin_<Layer>_D.png  RGBA8: RGB albedo (sRGB), A roughness (linear, 0 smooth .. 255 rough)
  T_Berlin_<Layer>_N.png  RGB8 tangent-space normal, DirectX convention (green down, as Unreal expects)

Every map of a layer is derived from one periodic height field plus shared masks, so albedo,
roughness and normal line up. All noise is FFT-shaped or built on a wrapped Voronoi grid, and
every stamp writes with modular indices, so all textures tile seamlessly on both axes.
One tile covers ~5 m x 5 m of world (1 px ~ 2.44 mm).

Usage (numpy + Pillow only, deterministic, fixed seeds):
  python berlin_textures_gen.py                       # all six layers + previews
  python berlin_textures_gen.py --layers BerlinCobble # just one (previews too)
  python berlin_textures_gen.py --no-preview --jobs 1
"""
import argparse
import os
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image

N = 2048
TILE_M = 5.0
PX = TILE_M / N  # metres per pixel
F32 = np.float32

PROJECT = Path(__file__).resolve().parents[3]
OUT_DIR = PROJECT / "Saved" / "Berlin" / "textures"

LAYERS = ["BerlinRubble", "BerlinAsh", "BerlinCobble", "BerlinGlass", "BerlinScrub", "BerlinMud"]
SEEDS = {"BerlinRubble": 1101, "BerlinAsh": 2202, "BerlinCobble": 3303,
         "BerlinGlass": 4404, "BerlinScrub": 5505, "BerlinMud": 6606}

# Shared palette (sRGB 0..1) so all six layers belong together.
DUST = np.array([0.47, 0.445, 0.41], F32)       # settled rubble dust
ASH = np.array([0.58, 0.565, 0.54], F32)        # pale ash
SOIL = np.array([0.27, 0.235, 0.195], F32)      # dark earth
BRICK = np.array([0.46, 0.32, 0.27], F32)       # weathered red brick, desaturated
CONCRETE = np.array([0.55, 0.54, 0.515], F32)
PLASTER = np.array([0.66, 0.64, 0.60], F32)
CHAR = np.array([0.22, 0.21, 0.20], F32)

# ----------------------------------------------------------------------------- toolkit

_YY, _XX = np.mgrid[0:N, 0:N].astype(F32)
_YY += 0.5
_XX += 0.5


def _freqs(n=N):
    fy = (np.fft.fftfreq(n) * n).astype(F32)
    fx = (np.fft.rfftfreq(n) * n).astype(F32)
    return fy[:, None], fx[None, :]


_FY, _FX = _freqs()
_FR = np.sqrt(_FY ** 2 + _FX ** 2)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _norm(a):
    a = a - a.mean()
    s = a.std()
    return (a / (s + 1e-12)).astype(F32)


def fnoise(rng, fmin, fmax, alpha=1.0, aniso=1.0, angle=0.0):
    """Periodic band-limited noise, std 1. Frequencies in cycles per tile.
    alpha: amplitude ~ f^-alpha. aniso > 1 stretches features along `angle` (radians)."""
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
    """Periodic gaussian blur (sigma in px)."""
    g = np.exp(-2.0 * (np.pi ** 2) * (sigma ** 2) * ((_FR / N) ** 2))
    if img.ndim == 2:
        return np.fft.irfft2(np.fft.rfft2(img) * g, s=(N, N)).astype(F32)
    return np.stack([blur(img[..., c], sigma) for c in range(img.shape[-1])], -1)


def highpass(img, fcut):
    """Remove energy below fcut cycles per tile (kills 5 m tiling landmarks)."""
    k = smoothstep(fcut * 0.6, fcut, _FR)
    return np.fft.irfft2(np.fft.rfft2(img) * k, s=(N, N)).astype(F32) + img.mean()


def sample_wrap(img, x, y):
    """Bilinear sample with wrap. x, y in px."""
    x = x - 0.5
    y = y - 0.5
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    fx = (x - x0).astype(F32)
    fy = (y - y0).astype(F32)
    x0 %= N
    y0 %= N
    x1 = (x0 + 1) % N
    y1 = (y0 + 1) % N
    a = img[y0, x0] * (1 - fx) + img[y0, x1] * fx
    b = img[y1, x0] * (1 - fx) + img[y1, x1] * fx
    return (a * (1 - fy) + b * fy).astype(F32)


def worley(rng, g, jitter=0.9, x=None, y=None):
    """Wrapped Voronoi on a g x g jittered grid. Returns F1, F2 (px), cell id, offset to nearest point."""
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
            wx = ncx % g
            wy = ncy % g
            px = (ncx + pts[wy, wx, 0]) * cs
            py = (ncy + pts[wy, wx, 1]) * cs
            ddx = x - px
            ddy = y - py
            d = np.sqrt(ddx * ddx + ddy * ddy)
            closer = d < F1
            F2 = np.where(closer, F1, np.minimum(F2, d))
            F1 = np.where(closer, d, F1)
            cid = np.where(closer, wy * g + wx, cid)
            ox = np.where(closer, ddx, ox)
            oy = np.where(closer, ddy, oy)
    centres = np.stack([(np.arange(g)[None, :] + pts[..., 0]) * cs,
                        (np.arange(g)[:, None] + pts[..., 1]) * cs], -1).reshape(-1, 2) % N
    return F1, F2, cid, ox, oy, centres


def zstamp(H, A, R, x0, y0, ph, pa, pr, pm):
    """Write a patch into the height/albedo/roughness stacks where it is higher (z-buffer), wrapping."""
    h, w = ph.shape
    rows = (np.arange(h) + y0) % N
    cols = (np.arange(w) + x0) % N
    ix = np.ix_(rows, cols)
    cur = H[ix]
    m = pm & (ph > cur)
    if not m.any():
        return
    H[ix] = np.where(m, ph, cur)
    A[ix] = np.where(m[..., None], pa, A[ix])
    R[ix] = np.where(m, pr, R[ix])


def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    t = np.clip(((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy + 1e-9), 0, 1)
    dx = px - (ax + t * vx)
    dy = py - (ay + t * vy)
    return np.sqrt(dx * dx + dy * dy), t


def height_to_normal(H, strength=1.0):
    """DirectX tangent-space normal (green = -dh/drow, i.e. green down) from a height in metres."""
    dhdx = (np.roll(H, -1, 1) - np.roll(H, 1, 1)) / (2 * PX) * strength
    dhdy = (np.roll(H, -1, 0) - np.roll(H, 1, 0)) / (2 * PX) * strength
    n = np.stack([-dhdx, -dhdy, np.ones_like(H)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n.astype(F32)


def cavity(H, sigma, scale):
    """>0 in local hollows (metres * scale), 0 on tops."""
    return np.clip((blur(H, sigma) - H) * scale, 0, 1)


def colour_jitter(rng, k, lum=0.1, hue=0.03):
    """k random per-element RGB multipliers."""
    l = 1.0 + rng.uniform(-lum, lum, (k, 1))
    h = 1.0 + rng.uniform(-hue, hue, (k, 3))
    return (l * h).astype(F32)


def fragment_layer(rng, g, density, height_m, gap_px, bevel_px, palette, weights, tilt=0.4,
                   lum=0.12, warp=None):
    """Scattered angular fragments from wrapped Voronoi cells.
    density: scalar or full-res map (0..1) sampled at each cell centre.
    Returns height (m, -inf where empty), albedo, mask."""
    if warp is not None:
        F1, F2, cid, ox, oy, cen = worley(rng, g, 0.95, _XX + warp[0], _YY + warp[1])
    else:
        F1, F2, cid, ox, oy, cen = worley(rng, g, 0.95)
    k = g * g
    if np.ndim(density) == 2:
        ci = cen.astype(np.int32) % N
        dens = density[ci[:, 1], ci[:, 0]]
    else:
        dens = np.full(k, density, F32)
    present = rng.random(k) < dens
    mat = rng.choice(len(palette), size=k, p=np.array(weights) / np.sum(weights))
    cols = np.array(palette, F32)[mat] * colour_jitter(rng, k, lum, 0.03)
    hcell = (height_m * rng.uniform(0.5, 1.3, k)).astype(F32)
    sx = rng.uniform(-tilt, tilt, k).astype(F32) * height_m / (N / g)
    sy = rng.uniform(-tilt, tilt, k).astype(F32) * height_m / (N / g)
    gapc = (gap_px * rng.uniform(0.6, 1.8, k)).astype(F32)
    edge = (F2 - F1) - gapc[cid]
    m = present[cid] & (edge > 0)
    prof = np.clip(edge / bevel_px, 0, 1) ** 0.6
    h = hcell[cid] * prof + sx[cid] * ox + sy[cid] * oy
    h = np.where(m, h, -np.inf).astype(F32)
    surf = fnoise(rng, 60, 700, 0.7)
    alb = cols[cid] * (1 + 0.07 * surf[..., None]) * (0.86 + 0.14 * prof)[..., None]
    return h, alb, m, mat[cid]


def stamp_chunks(H, A, R, rng, count, dens, size_px, hm, palette, weights, rough, aspect=(0.35, 0.75),
                 sink=0.35, bevel=4.0):
    """Angular broken chunks (brick bats, concrete lumps): rotated boxes with 1-3 random chip cuts,
    bevelled, tilted, partly buried relative to the local ground. z-buffered and wrapped."""
    pal = np.array(palette, F32)
    w = np.array(weights, np.float64)
    w /= w.sum()
    surf = _SURF if _SURF is not None else None
    xs = rng.uniform(0, N, count * 4)
    ys = rng.uniform(0, N, count * 4)
    d_at = dens[ys.astype(int) % N, xs.astype(int) % N] if np.ndim(dens) == 2 else np.full(xs.shape, dens)
    keep = rng.random(xs.shape) < d_at
    xs, ys = xs[keep][:count], ys[keep][:count]
    for x, y in zip(xs, ys):
        L = rng.uniform(*size_px)
        W = L * rng.uniform(*aspect)
        ang = rng.uniform(0, np.pi)
        s = int(L * 0.75) + 3
        x0, y0 = int(x) - s, int(y) - s
        yy, xx = np.mgrid[0:2 * s + 1, 0:2 * s + 1].astype(F32)
        dx = xx + x0 + 0.5 - x
        dy = yy + y0 + 0.5 - y
        ca, sa = np.cos(ang), np.sin(ang)
        a = ca * dx + sa * dy
        b = -sa * dx + ca * dy
        d = np.maximum(np.abs(a) - L / 2, np.abs(b) - W / 2)
        for _ in range(rng.integers(1, 4)):
            th = rng.uniform(0, 2 * np.pi)
            d = np.maximum(d, np.cos(th) * a + np.sin(th) * b - rng.uniform(0.18, 0.42) * L)
        rows = (np.arange(2 * s + 1) + y0) % N
        cols = (np.arange(2 * s + 1) + x0) % N
        sn = surf[np.ix_(rows, cols)]
        d = d + sn * 0.8
        inside = -d
        m = inside > 0
        if not m.any():
            continue
        prof = np.clip(inside / bevel, 0, 1) ** 0.5
        h_here = hm * rng.uniform(0.6, 1.3)
        base = H[int(y) % N, int(x) % N]
        ph = (base + h_here * (prof - sink) + rng.normal(0, 0.25) * h_here * a / L
              + rng.normal(0, 0.25) * h_here * b / max(W, 1) + sn * 0.0006).astype(F32)
        col = pal[rng.choice(len(pal), p=w)] * rng.uniform(0.85, 1.12) * (1 + rng.uniform(-0.03, 0.03, 3))
        pa = col * (1 + 0.08 * sn[..., None]) * (0.82 + 0.18 * prof)[..., None]
        zstamp(H, A, R, x0, y0, ph, pa.astype(F32), np.full(ph.shape, rough, F32), m)


_SURF = None


def eq_lowfreq(A, R, sigma=50.0, keep=0.5):
    """Compress albedo-luminance and roughness variation below ~0.19*N/sigma cycles per tile.
    Mid-scale blotches are what turns into a visible 5 m grid when the tile repeats from the
    RTS camera; small-scale detail is untouched. Ratio-based so hue is kept."""
    lum = A.mean(-1)
    lb = blur(lum, sigma)
    m = lum.mean()
    A = A * ((m + (lb - m) * keep) / np.maximum(lb, 1e-3))[..., None]
    rb = blur(R, sigma)
    R = R - (rb - R.mean()) * (1 - keep)
    return A, R


def finish(H, A, R, keep=0.5):
    A, R = eq_lowfreq(A, R, 50.0, keep)
    A = np.clip(A, 0.02, 1.0).astype(F32)
    R = np.clip(R, 0.04, 1.0).astype(F32)   # never near alpha 0 (importer may treat as transparent)
    return H.astype(F32), A, R


# ----------------------------------------------------------------------------- layers

def layer_rubble(rng):
    # Mid-scale clumping: denser rubble heaps vs dusty flats, 6-20 cycles (25-80 cm), no tile landmark.
    clump = fnoise(rng, 6, 20, 0.8)
    clump2 = fnoise(rng, 14, 50, 0.8)
    dens_base = smoothstep(-1.0, 1.4, clump + 0.45 * clump2)
    H = (fnoise(rng, 5, 64, 1.0) * 0.010 + fnoise(rng, 40, 400, 1.0) * 0.0006
         + fnoise(rng, 300, 1000, 0.5) * 0.0004 + dens_base * 0.02).astype(F32)
    A = np.empty((N, N, 3), F32)
    tint = fnoise(rng, 6, 40, 0.8)
    A[:] = DUST
    A *= (1.0 + 0.06 * tint)[..., None]
    A += (BRICK - DUST) * (0.18 * smoothstep(-0.5, 1.5, fnoise(rng, 8, 60, 0.8)))[..., None]
    R = np.full((N, N), 0.92, F32)

    pal = [BRICK, BRICK * np.array([0.92, 0.95, 1.0], F32), CONCRETE, PLASTER, CHAR,
           np.array([0.40, 0.38, 0.35], F32)]
    wts = [0.30, 0.14, 0.26, 0.04, 0.06, 0.20]
    rough_of = np.array([0.88, 0.9, 0.86, 0.85, 0.93, 0.9], F32)
    wx = fnoise(rng, 10, 80) * 6
    wy = fnoise(rng, 10, 80) * 6
    # (cells per tile, density scale, height, gap, bevel)
    for g, dsc, hm, gap, bev in [(34, 0.55, 0.045, 5, 16), (70, 0.55, 0.026, 3, 9),
                                 (150, 0.6, 0.012, 2, 5), (300, 0.8, 0.006, 1.2, 3)]:
        h, alb, m, mat = fragment_layer(rng, g, np.clip(dens_base * dsc * 1.6, 0.06, 0.95), hm, gap, bev,
                                        pal, wts, tilt=0.5, warp=(wx * 0.4, wy * 0.4))
        # partially embedded: sink by local dust level
        h = h - hm * 0.45
        up = h > H
        H = np.where(up, h, H)
        A = np.where(up[..., None], alb, A)
        R = np.where(up, rough_of[mat], R)

    # broken brick bats and concrete lumps on top
    global _SURF
    _SURF = fnoise(rng, 60, 500, 0.8)
    stamp_chunks(H, A, R, rng, 520, np.clip(dens_base * 0.9, 0.05, 1), (34, 100), 0.04,
                 [BRICK, BRICK * 0.9, CONCRETE, PLASTER * 0.9], [0.45, 0.2, 0.28, 0.07], 0.88)
    stamp_chunks(H, A, R, rng, 1400, np.clip(dens_base, 0.08, 1), (14, 38), 0.016,
                 [BRICK, CONCRETE, CHAR, np.array([0.40, 0.38, 0.35], F32)], [0.45, 0.35, 0.08, 0.12], 0.9,
                 bevel=2.5)
    # dust settles into hollows and veils fragments in places
    cav = cavity(H, 12, 60)
    veil = smoothstep(-0.2, 1.2, fnoise(rng, 8, 60, 0.9) + 0.6 * fnoise(rng, 60, 300, 0.7))
    dmix = np.clip(0.22 + 0.45 * cav + 0.4 * veil, 0, 0.75)
    A = A * (1 - dmix[..., None]) + (DUST * (1 + 0.04 * tint[..., None])) * dmix[..., None]
    A *= (1.0 - 0.18 * cavity(H, 3, 80))[..., None]
    A *= (1.0 + 0.02 * fnoise(rng, 200, 900, 0.3))[..., None]
    R = R * (1 - dmix) + 0.94 * dmix
    R += 0.02 * fnoise(rng, 20, 200)
    return finish(H, A, R) + (1.0,)


def layer_ash(rng):
    # Drift ripples: integer wave vector keeps them periodic, phase warped by periodic noise.
    phase = fnoise(rng, 3, 10, 1.3) * 0.45 + fnoise(rng, 10, 30, 1.0) * 0.06
    k1 = (22, 8)
    t = 2 * np.pi * (k1[0] * _XX + k1[1] * _YY) / N + phase * 2 * np.pi * 0.5
    rip = np.sin(t) + 0.35 * np.sin(2 * t + 0.8)
    k2 = (6, 25)
    t2 = 2 * np.pi * (k2[0] * _XX + k2[1] * _YY) / N + fnoise(rng, 3, 10, 1.3) * 2 * np.pi * 0.2
    rip2 = np.sin(t2) + 0.3 * np.sin(2 * t2 + 0.5)
    sel = smoothstep(0.6, 1.6, fnoise(rng, 4, 12, 0.6))          # rare cross-set patches
    ripamp = 0.25 + 0.75 * smoothstep(-1.0, 1.0, fnoise(rng, 4, 16, 0.6))
    ripples = (rip * (1 - sel) + 0.6 * rip2 * sel) * ripamp
    H = (ripples * 0.0065 + fnoise(rng, 4, 32, 1.2) * 0.006 + fnoise(rng, 40, 300, 1.0) * 0.00015
         + fnoise(rng, 300, 1000, 0.5) * 0.00003).astype(F32)

    A = np.empty((N, N, 3), F32)
    A[:] = ASH
    tone = fnoise(rng, 5, 40, 0.9)
    A *= (1.0 + 0.045 * tone)[..., None]
    # darker sooty / damp patches and old brick-dust stains, mid-scale
    soot = smoothstep(0.4, 1.8, fnoise(rng, 6, 30, 0.8) + 0.4 * fnoise(rng, 30, 120, 0.8))
    A = A * (1 - 0.3 * soot[..., None]) + (DUST * 0.8) * (0.3 * soot[..., None])
    stain = smoothstep(1.0, 2.2, fnoise(rng, 6, 40, 0.9))
    A = A * (1 - 0.18 * stain[..., None]) + BRICK * 1.1 * (0.18 * stain[..., None])
    # ripple crests sorted lighter, troughs collect darker grit
    A *= (1.0 + 0.07 * np.clip(ripples, -1.5, 1.5))[..., None]
    R = np.full((N, N), 0.93, F32) - 0.03 * np.clip(ripples, -1, 1) * 0.5

    # scattered grit and small pebbles, more in ripple troughs
    gdens = np.clip(0.10 + 0.10 * smoothstep(0, 1.5, -ripples) + 0.1 * soot, 0, 0.5)
    pal = [CHAR, BRICK, CONCRETE, np.array([0.33, 0.31, 0.29], F32)]
    for g, dsc, hm, gap, bev in [(260, 0.6, 0.004, 1.2, 2.5), (110, 0.25, 0.008, 2.0, 4)]:
        h, alb, m, mat = fragment_layer(rng, g, gdens * dsc, hm, gap, bev, pal, [0.35, 0.3, 0.25, 0.1],
                                        tilt=0.4)
        h = h - hm * 0.5
        up = h > H
        H = np.where(up, h, H)
        A = np.where(up[..., None], alb * 0.95, A)
        R = np.where(up, 0.88, R)
    A *= (1.0 - 0.12 * cavity(H, 3, 150))[..., None]
    A *= (1.0 + 0.035 * fnoise(rng, 200, 900, 0.3))[..., None]
    return finish(H, A, R) + (1.0,)


def layer_cobble(rng):
    nrows = 36
    rh = N / nrows                      # ~14 cm rows
    wxn = fnoise(rng, 4, 24, 1.0) * 5.0
    wyn = fnoise(rng, 4, 24, 1.0) * 3.5
    X = _XX + wxn
    Y = _YY + wyn
    rowf = Y / rh
    row = (np.floor(rowf).astype(np.int32)) % nrows
    v = (rowf - np.floor(rowf)) * rh
    u = np.zeros((N, N), F32)
    L = np.zeros((N, N), F32)
    sid = np.zeros((N, N), np.int32)
    centres = []
    base = 0
    for r in range(nrows):
        lens = []
        while sum(lens) < N:
            lens.append(rng.uniform(40, 66))
        lens = np.array(lens, F32)
        lens *= N / lens.sum()
        b = np.concatenate([[0.0], np.cumsum(lens)]).astype(F32)
        off = rng.uniform(0, N)
        m = row == r
        xr = (X[m] - off) % N
        k = np.clip(np.searchsorted(b, xr, side="right") - 1, 0, len(lens) - 1)
        u[m] = xr - b[k]
        L[m] = lens[k]
        sid[m] = base + k
        for j in range(len(lens)):
            centres.append(((off + b[j] + lens[j] / 2) % N, (r + 0.5) * rh))
        base += len(lens)
    ns = base
    centres = np.array(centres, F32)

    # per-stone properties
    gap = rng.uniform(3.0, 6.5, ns).astype(F32)
    rad = rng.uniform(6, 14, ns).astype(F32)
    cap = rng.uniform(0.016, 0.026, ns).astype(F32)
    sink = rng.normal(0, 0.006, ns).astype(F32)
    tx = rng.normal(0, 0.006, ns).astype(F32)
    ty = rng.normal(0, 0.006, ns).astype(F32)
    # granite tones: neutral grey, pinkish, bluish, brownish, a few dark basalt-like
    tones = np.array([[1, 1, 1], [1.05, 0.985, 0.955], [0.97, 0.99, 1.03], [1.04, 1.0, 0.93], [0.8, 0.8, 0.82]], F32)
    tsel = rng.choice(5, ns, p=[0.50, 0.15, 0.12, 0.15, 0.08])
    lum = rng.uniform(0.40, 0.56, ns).astype(F32)
    scol = (tones[tsel] * lum[:, None] * colour_jitter(rng, ns, 0.06, 0.02)).astype(F32)

    # missing patches decided per stone from a mid-scale mask (several patches, no hero hole)
    pmask = fnoise(rng, 5, 11, 0.4) + 0.35 * fnoise(rng, 12, 30, 0.6)
    ci = centres.astype(np.int32) % N
    pc = pmask[ci[:, 1], ci[:, 0]]
    thr = np.quantile(pc, 0.86)
    missing = (pc > thr) | (rng.random(ns) < 0.03)
    # loose / sunken stones near holes
    near = (pc > np.quantile(pc, 0.76)) & ~missing
    sink = np.where(near, sink - rng.uniform(0.004, 0.014, ns), sink).astype(F32)
    tx = np.where(near, tx * 2.5, tx)
    ty = np.where(near, ty * 2.5, ty)

    px_ = u - L / 2
    py_ = v - rh / 2
    hx = L / 2 - gap[sid] / 2
    hy = rh / 2 - gap[sid] / 2
    r_ = rad[sid]
    qx = np.abs(px_) - (hx - r_)
    qy = np.abs(py_) - (hy - r_)
    sdf = (np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2) + np.minimum(np.maximum(qx, qy), 0) - r_)
    sdf += fnoise(rng, 40, 220, 0.8) * 1.3        # chipped, irregular edges
    inside = -sdf
    nx = px_ / np.maximum(hx, 1)
    ny = py_ / np.maximum(hy, 1)
    crown = np.clip((1 - nx * nx) * (1 - ny * ny), 0, 1)
    prof = smoothstep(0, 9, inside)
    grain = fnoise(rng, 150, 900, 0.7) * 0.0006 + fnoise(rng, 30, 150, 1.0) * 0.0012
    Hs = (cap[sid] * (0.55 * prof + 0.45 * prof * crown ** 0.5) + sink[sid]
          + tx[sid] * nx + ty[sid] * ny + grain)

    # cracks through ~18% of stones; halves offset slightly
    cracked = rng.random(ns) < 0.18
    ca = rng.uniform(0, np.pi, ns).astype(F32)
    cc = rng.uniform(-8, 8, ns).astype(F32)
    cn = fnoise(rng, 80, 400, 0.8) * 1.2
    cd = np.cos(ca[sid]) * px_ + np.sin(ca[sid]) * py_ - cc[sid] + cn
    crack = cracked[sid] & (np.abs(cd) < 1.1) & (inside > 2)
    Hs = Hs + np.where(cracked[sid] & (cd > 0), 0.0025, 0.0)
    Hs = np.where(crack, Hs - 0.008, Hs)

    # joints and holes: dirt
    dirtn = fnoise(rng, 20, 300, 0.9)
    Hj = (-0.004 + dirtn * 0.0015).astype(F32)
    stone = (inside > 0) & ~missing[sid]
    H = np.where(stone, np.maximum(Hs, Hj), Hj)
    holes = missing[sid]
    soft_hole = blur(holes.astype(F32), 10)
    H = H - 0.022 * soft_hole

    dirtcol = (SOIL * 0.55 + DUST * 0.45) * (1 + 0.08 * dirtn[..., None])
    A = np.empty((N, N, 3), F32)
    A[:] = dirtcol
    speck = fnoise(rng, 400, 1000, 0.2)
    scolp = scol[sid] * (1 + 0.04 * speck[..., None]) * (1 + 0.07 * fnoise(rng, 30, 200, 0.9)[..., None])
    scolp = np.where((speck < -2.2)[..., None], scolp * 0.75, scolp)
    A = np.where(stone[..., None], scolp, A)
    A = np.where(crack[..., None], dirtcol * 0.8, A)
    R = np.where(stone, 0.80 - 0.12 * crown, 0.93).astype(F32)
    R = np.where(crack, 0.93, R)

    # rubble fragments lying in holes and spilling a little over the paving
    hdens = np.clip(soft_hole * 0.8 + 0.006, 0, 0.8)
    pal = [BRICK, CONCRETE, scol.mean(0), CHAR]
    for g, hm, gap_, bev in [(90, 0.02, 2.5, 7), (200, 0.01, 1.5, 4)]:
        h, alb, m, mat = fragment_layer(rng, g, hdens, hm, gap_, bev, pal, [0.25, 0.35, 0.3, 0.1], tilt=0.5)
        h = h - 0.025 * soft_hole - hm * 0.3
        up = h > H
        H = np.where(up, h, H)
        A = np.where(up[..., None], alb, A)
        R = np.where(up, 0.9, R)

    # settled dust: shared palette tie-in, mid-scale patches + hollows
    dust = smoothstep(0.0, 1.6, fnoise(rng, 6, 40, 0.9) + 0.5 * fnoise(rng, 40, 200, 0.8))
    cav = cavity(H, 6, 120)
    dmix = np.clip(0.15 + 0.4 * dust + 0.5 * cav + 0.5 * soft_hole, 0, 0.75)
    A = A * (1 - dmix[..., None]) + DUST * dmix[..., None]
    R = R * (1 - dmix) + 0.93 * dmix
    A *= (1.0 - 0.15 * cavity(H, 2.5, 150))[..., None]
    return finish(H, A, R) + (1.0,)


def layer_glass(rng):
    # melted flow surface: domain-warped smooth noise
    w1 = fnoise(rng, 4, 20, 1.0) * 60
    w2 = fnoise(rng, 4, 20, 1.0) * 60
    flow = sample_wrap(fnoise(rng, 4, 24, 1.3), _XX + w1, _YY + w2)
    ridge = 1.0 - np.abs(sample_wrap(fnoise(rng, 8, 60, 1.0), _XX + w2 * 0.7, _YY + w1 * 0.7))
    H = (flow * 0.008 + ridge * 0.0012).astype(F32)

    # crazing: warped Voronoi plates at two scales
    cx = fnoise(rng, 10, 80, 1.0) * 10
    cy = fnoise(rng, 10, 80, 1.0) * 10
    F1, F2, cid, ox, oy, _ = worley(rng, 22, 0.95, _XX + cx, _YY + cy)
    e_big = F2 - F1
    k = 22 * 22
    poff = rng.normal(0, 0.003, k).astype(F32)
    ptx = rng.normal(0, 0.00012, k).astype(F32)
    pty = rng.normal(0, 0.00012, k).astype(F32)
    H = H + poff[cid] + ptx[cid] * ox + pty[cid] * oy
    F1s, F2s, cids, _, _, _ = worley(rng, 70, 0.95, _XX + cy * 0.6, _YY - cx * 0.6)
    e_small = F2s - F1s
    craze_region = smoothstep(-0.3, 0.8, fnoise(rng, 6, 30, 0.8))
    wbig = 2.4 + 1.6 * smoothstep(-1, 1.5, fnoise(rng, 20, 100))
    crack_big = 1.0 - smoothstep(wbig * 0.5, wbig, e_big)
    crack_small = (1.0 - smoothstep(0.4, 1.0, e_small)) * craze_region
    crack = np.maximum(crack_big, crack_small * 0.85)
    H = H - crack_big * 0.005 - crack_small * 0.0015

    # bubbles / pits: wrapped dimples with slight rims
    Hb = np.zeros((N, N), F32)
    nb = 2600
    bx = rng.uniform(0, N, nb)
    by = rng.uniform(0, N, nb)
    br = rng.uniform(1.0, 1.5, nb) * np.exp(rng.normal(0.6, 0.6, nb)) + 1.0
    for i in range(nb):
        r = br[i]
        s = int(np.ceil(r * 1.6)) + 1
        x0 = int(bx[i]) - s
        y0 = int(by[i]) - s
        yy, xx = np.mgrid[0:2 * s + 1, 0:2 * s + 1].astype(F32)
        d = np.sqrt((xx + x0 + 0.5 - bx[i]) ** 2 + (yy + y0 + 0.5 - by[i]) ** 2) / r
        prof = np.where(d < 1, -(1 - d * d), 0.25 * np.exp(-((d - 1.15) / 0.2) ** 2))
        rows = (np.arange(2 * s + 1) + y0) % N
        cols = (np.arange(2 * s + 1) + x0) % N
        Hb[np.ix_(rows, cols)] += (prof * r * 0.0004).astype(F32)
    H = H + Hb
    pits = np.clip(-Hb / 0.002, 0, 1)

    # colour: obsidian green-black with olive and rust-brown inclusions, lighter green flow veins
    tone = fnoise(rng, 5, 40, 0.9)
    gA = np.array([0.125, 0.150, 0.130], F32)
    gB = np.array([0.175, 0.190, 0.150], F32)
    gC = np.array([0.215, 0.185, 0.150], F32)
    A = gA + (gB - gA) * smoothstep(-1, 1, tone)[..., None]
    A = A + (gC - A) * (0.6 * smoothstep(0.9, 2.0, fnoise(rng, 6, 40, 0.8)))[..., None]
    ptone = rng.uniform(0.82, 1.18, k).astype(F32)
    A = A * ptone[cid][..., None]
    vein = smoothstep(0.82, 0.97, ridge)
    A = A + (np.array([0.24, 0.29, 0.24], F32) - A) * (0.5 * vein)[..., None]
    A *= (1 + 0.05 * fnoise(rng, 60, 400, 0.6))[..., None]
    R = (0.14 + 0.05 * smoothstep(-1, 2, fnoise(rng, 10, 120, 0.8)) + 0.03 * vein).astype(F32)

    # dust: patches on the low ground + fine veil + dust packed into cracks and pits
    lowness = -flow
    dcov = smoothstep(0.95, 1.6, 0.7 * fnoise(rng, 6, 30, 0.8) + 0.45 * lowness
                      + 0.35 * fnoise(rng, 30, 200, 0.8))
    veil = smoothstep(0.0, 2.0, fnoise(rng, 8, 60, 0.9) + 0.5 * lowness) * 0.25
    fill = np.clip(crack + pits * 0.35, 0, 1)
    dustcol = (DUST * 0.65 + ASH * 0.35) * (1 + 0.05 * fnoise(rng, 20, 200)[..., None])
    dm = np.clip(np.maximum(dcov * 0.7, fill * 0.8) + veil, 0, 0.85)
    A = A * (1 - dm[..., None]) + dustcol * dm[..., None]
    R = R * (1 - dm) + 0.92 * dm
    # dust softens the glass relief where it lies thick
    H = H * (1 - 0.6 * dcov) + blur(H, 6) * (0.6 * dcov)
    H = H + dcov * 0.0008 * fnoise(rng, 100, 600, 0.6)
    A *= (1.0 - 0.1 * cavity(H, 2.5, 200))[..., None]
    return finish(H, A, R) + (1.0,)


def layer_scrub(rng):
    # soil base
    clod = fnoise(rng, 8, 120, 1.0)
    H = (fnoise(rng, 4, 40, 1.1) * 0.012 + clod * 0.004 + fnoise(rng, 120, 800, 0.8) * 0.0008).astype(F32)
    A = np.empty((N, N, 3), F32)
    A[:] = SOIL
    A *= (1 + 0.10 * fnoise(rng, 6, 60, 0.9))[..., None]
    A = A * (1 - 0.3 * smoothstep(0.5, 1.8, fnoise(rng, 6, 40))[..., None]) + DUST * (
        0.3 * smoothstep(0.5, 1.8, fnoise(rng, 6, 40)))[..., None]
    A *= (1 + 0.06 * clod)[..., None]
    R = np.full((N, N), 0.9, F32)

    # small stones in the soil
    h, alb, m, mat = fragment_layer(rng, 160, 0.10, 0.006, 2, 4, [CONCRETE, BRICK, CHAR, DUST],
                                    [0.35, 0.25, 0.15, 0.25], tilt=0.3)
    h -= 0.003
    up = h > H
    H = np.where(up, h, H)
    A = np.where(up[..., None], alb * 0.9, A)
    R = np.where(up, 0.88, R)

    # placement fields: tufts clustered at mid scale (two bands so no single row/hero clump),
    # a smooth wind direction so blades lie combed over, litter where grass is thin
    tdens = smoothstep(-0.3, 1.6, fnoise(rng, 6, 24, 0.7) + 0.6 * fnoise(rng, 24, 80, 0.7))
    litter = smoothstep(-0.8, 1.0, -0.7 * tdens + fnoise(rng, 6, 40, 0.8))
    wind = 0.9 + fnoise(rng, 3, 10, 1.0) * 0.8           # radians, slowly varying

    # leaf litter: flat curled dead leaves, grey-brown
    leafcols = np.array([[0.38, 0.30, 0.22], [0.31, 0.25, 0.19], [0.42, 0.37, 0.29],
                         [0.30, 0.27, 0.23], [0.46, 0.40, 0.31]], F32)
    nleaf = 6000
    lx = rng.uniform(0, N, nleaf)
    ly = rng.uniform(0, N, nleaf)
    keep = rng.random(nleaf) < (0.1 + 0.9 * litter[ly.astype(int) % N, lx.astype(int) % N])
    for i in np.nonzero(keep)[0]:
        ln = rng.uniform(10, 26)
        wd = ln * rng.uniform(0.35, 0.6)
        ang = rng.uniform(0, np.pi)
        s = int(ln) + 2
        x0 = int(lx[i]) - s
        y0 = int(ly[i]) - s
        yy, xx = np.mgrid[0:2 * s + 1, 0:2 * s + 1].astype(F32)
        dx = xx + x0 + 0.5 - lx[i]
        dy = yy + y0 + 0.5 - ly[i]
        a = np.cos(ang) * dx + np.sin(ang) * dy
        b = -np.sin(ang) * dx + np.cos(ang) * dy
        bb = b + 0.12 * a * a / ln * rng.uniform(-1, 1)     # slight curve
        e = (a / (ln / 2)) ** 2 + (bb / (wd / 2)) ** 2 * (1 + 0.6 * np.clip(a / (ln / 2), 0, 1))
        m = e < 1
        curl = rng.uniform(0.2, 1.0)
        ph = (0.003 + 0.002 * curl * (np.abs(bb) / (wd / 2)) ** 2 + 0.001 * (1 - e)).astype(F32) +             H[int(ly[i]) % N, int(lx[i]) % N] + 0.001
        c = leafcols[rng.integers(0, len(leafcols))] * rng.uniform(0.85, 1.1)
        rib = (np.abs(bb) < 0.7)[..., None]
        pa = np.where(rib, c * 0.82, c) * (0.88 + 0.12 * (1 - e))[..., None]
        zstamp(H, A, R, x0, y0, ph, pa.astype(F32), np.full(ph.shape, 0.82, F32), m)

    # twigs
    for i in range(420):
        x = rng.uniform(0, N)
        y = rng.uniform(0, N)
        ln = rng.uniform(25, 110)
        ang = rng.uniform(0, 2 * np.pi)
        bend = rng.uniform(-0.4, 0.4)
        wid = rng.uniform(0.8, 2.4)
        mx, my = x + np.cos(ang) * ln / 2, y + np.sin(ang) * ln / 2
        ex, ey = mx + np.cos(ang + bend) * ln / 2, my + np.sin(ang + bend) * ln / 2
        s = int(ln) + 4
        x0, y0 = int(x) - s, int(y) - s
        yy, xx = np.mgrid[0:2 * s + 1, 0:2 * s + 1].astype(F32)
        gx, gy = xx + x0 + 0.5, yy + y0 + 0.5
        d1, t1 = seg_dist(gx, gy, x, y, mx, my)
        d2, t2 = seg_dist(gx, gy, mx, my, ex, ey)
        d = np.minimum(d1, d2)
        m = d < wid
        ph = (H[int(y) % N, int(x) % N] + 0.006 + 0.003 * np.sqrt(np.clip(1 - (d / wid) ** 2, 0, 1))).astype(F32)
        c = np.array([0.29, 0.25, 0.21], F32) * rng.uniform(0.75, 1.25)
        pa = np.broadcast_to(c, ph.shape + (3,)) * (0.85 + 0.15 * np.clip(1 - d / wid, 0, 1))[..., None]
        zstamp(H, A, R, x0, y0, ph, pa.astype(F32), np.full(ph.shape, 0.8, F32), m)

    grass = np.array([[0.55, 0.50, 0.39], [0.49, 0.45, 0.37], [0.60, 0.56, 0.45], [0.44, 0.41, 0.35],
                      [0.52, 0.50, 0.44]], F32)

    def blade(sx_, sy_, dirx, diry, ln, curve, h0, h_root, col, wid0=1.3):
        mx, my = sx_ + dirx * ln * 0.5, sy_ + diry * ln * 0.5
        ca, sa = np.cos(curve), np.sin(curve)
        ex = mx + (dirx * ca - diry * sa) * ln * 0.5
        ey = my + (dirx * sa + diry * ca) * ln * 0.5
        minx = int(min(sx_, mx, ex)) - 3
        miny = int(min(sy_, my, ey)) - 3
        maxx = int(max(sx_, mx, ex)) + 4
        maxy = int(max(sy_, my, ey)) + 4
        yy, xx = np.mgrid[miny:maxy, minx:maxx].astype(F32)
        gx, gy = xx + 0.5, yy + 0.5
        d1, t1 = seg_dist(gx, gy, sx_, sy_, mx, my)
        d2, t2 = seg_dist(gx, gy, mx, my, ex, ey)
        t = np.where(d1 < d2, t1 * 0.5, 0.5 + t2 * 0.5)
        d = np.minimum(d1, d2)
        m = d < wid0 * (1 - 0.55 * t)
        ph = (h0 + h_root * (1 - t) ** 1.5 + 0.003 + rng.uniform(0, 0.004)).astype(F32)
        c = col * (0.72 + 0.38 * t)[..., None]
        zstamp(H, A, R, minx, miny, ph, np.clip(c, 0, 1).astype(F32), np.full(ph.shape, 0.74, F32), m)

    # flattened dry grass mat: loose blades combed by the wind field, where tufts are dense
    for i in range(7000):
        x = rng.uniform(0, N)
        y = rng.uniform(0, N)
        td = tdens[int(y) % N, int(x) % N]
        if rng.random() > 0.15 + 0.85 * td:
            continue
        a = wind[int(y) % N, int(x) % N] + rng.normal(0, 0.45)
        col = grass[rng.integers(0, len(grass))] * rng.uniform(0.7, 1.05)
        blade(x, y, np.cos(a), np.sin(a), rng.uniform(25, 70), rng.uniform(-0.3, 0.3),
              H[int(y) % N, int(x) % N], 0.004, col, 1.1)

    # dead grass tufts: dense clumps, blades lying over mostly downwind, uneven radius
    nt = 420
    tx = rng.uniform(0, N, nt * 4)
    ty = rng.uniform(0, N, nt * 4)
    ok = rng.random(nt * 4) < tdens[ty.astype(int) % N, tx.astype(int) % N] ** 1.5
    tx, ty = tx[ok][:nt], ty[ok][:nt]
    for j in range(len(tx)):
        size = rng.uniform(18, 60) * (1 + 0.6 * tdens[int(ty[j]) % N, int(tx[j]) % N])
        lean = wind[int(ty[j]) % N, int(tx[j]) % N] + rng.normal(0, 0.5)
        spread = rng.uniform(0.45, 1.0)
        nbl = int(size * 2.4)
        base_h = H[int(ty[j]) % N, int(tx[j]) % N]
        tcol = grass[rng.integers(0, len(grass))]
        s = int(size * 0.3) + 2
        x0, y0 = int(tx[j]) - s, int(ty[j]) - s
        yy, xx = np.mgrid[0:2 * s + 1, 0:2 * s + 1].astype(F32)
        d = np.sqrt((xx + x0 + 0.5 - tx[j]) ** 2 + (yy + y0 + 0.5 - ty[j]) ** 2) / (size * 0.3)
        zstamp(H, A, R, x0, y0, (base_h + 0.03 * (1 - d * d)).astype(F32),
               np.broadcast_to(tcol * 0.6, d.shape + (3,)).astype(F32), np.full(d.shape, 0.8, F32), d < 1)
        for k in range(nbl):
            a = lean + rng.normal(0, spread) if rng.random() < 0.8 else rng.uniform(0, 2 * np.pi)
            ln = size * rng.uniform(0.5, 1.25)
            col = tcol * rng.uniform(0.78, 1.1) * (1 + rng.uniform(-0.04, 0.04, 3))
            blade(tx[j] + rng.normal(0, size * 0.12), ty[j] + rng.normal(0, size * 0.12),
                  np.cos(a), np.sin(a), ln, rng.uniform(-0.4, 0.4), base_h, 0.035, col)

    A *= (1.0 - 0.2 * cavity(H, 3, 60))[..., None]
    return finish(H, A, R) + (1.0,)


def layer_mud(rng):
    ang = rng.uniform(0, np.pi)
    smear = fnoise(rng, 6, 60, 1.1, aniso=2.2, angle=ang)
    H = (fnoise(rng, 4, 48, 1.15) * 0.014 + smear * 0.004 + fnoise(rng, 40, 300, 1.0) * 0.0006
         + fnoise(rng, 300, 1000, 0.5) * 0.00015).astype(F32)
    # sliding / trampled ridges
    ridges = 1 - np.abs(fnoise(rng, 10, 60, 1.0, aniso=2.0, angle=ang + 0.6))
    H += smoothstep(0.8, 1.0, ridges) * 0.001

    pal = [np.array([0.30, 0.29, 0.27], F32), np.array([0.38, 0.36, 0.33], F32), BRICK * 0.8, CHAR]
    rough_stone = 0.55
    sdens = smoothstep(-1, 1.5, fnoise(rng, 6, 30, 0.8)) * 0.22
    A = np.empty((N, N, 3), F32)
    base = np.array([0.255, 0.225, 0.19], F32)
    A[:] = base
    tone = fnoise(rng, 5, 40, 0.9)
    A *= (1 + 0.08 * tone)[..., None]
    R = np.full((N, N), 0.5, F32)
    for g, hm in [(120, 0.009), (260, 0.004)]:
        h, alb, m, mat = fragment_layer(rng, g, sdens, hm, 3, 6, pal, [0.4, 0.3, 0.15, 0.15], tilt=0.3)
        h = h - hm * 0.55
        up = h > H
        H = np.where(up, h, H)
        A = np.where(up[..., None], alb, A)
        R = np.where(up, rough_stone, R)

    # moisture: high crests drier/lighter/rougher; low pockets hold standing water
    hb = blur(H, 4)
    dry = smoothstep(0.004, 0.02, hb)
    A = A * (1 - 0.45 * dry[..., None]) + (DUST * 0.78) * (0.45 * dry[..., None])
    R = R + 0.25 * dry
    level = np.quantile(hb, 0.09)
    water = smoothstep(level + 0.0008, level - 0.0012, H)
    wet = smoothstep(level + 0.006, level, hb)
    A *= (1 - 0.25 * wet)[..., None]
    A = A * (1 - 0.4 * water[..., None]) + np.array([0.12, 0.115, 0.10], F32) * (0.4 * water[..., None])
    R = R * (1 - wet * 0.35)
    R = R * (1 - water) + 0.07 * water
    H = H * (1 - water) + level * water
    A *= (1.0 - 0.15 * cavity(H, 3, 150))[..., None]
    R += 0.04 * fnoise(rng, 20, 200, 0.8)
    return finish(H, A, R) + (1.0,)


BUILDERS = {"BerlinRubble": layer_rubble, "BerlinAsh": layer_ash, "BerlinCobble": layer_cobble,
            "BerlinGlass": layer_glass, "BerlinScrub": layer_scrub, "BerlinMud": layer_mud}
NSTRENGTH = {"BerlinRubble": 1.0, "BerlinAsh": 1.0, "BerlinCobble": 1.0,
             "BerlinGlass": 1.0, "BerlinScrub": 1.0, "BerlinMud": 1.0}

# ----------------------------------------------------------------------------- output + previews


def srgb_to_lin(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(np.maximum(c, 0), 1 / 2.4) - 0.055)


def box_down(img, f):
    h, w = img.shape[:2]
    return img.reshape(h // f, f, w // f, f, *img.shape[2:]).mean(axis=(1, 3))


def shade(alb_srgb, n, rough=None, az=315.0, el=22.0):
    """Lambert + faint spec, low sun. n in DirectX tangent frame (y down the image)."""
    a, e = np.radians(az), np.radians(el)
    L = np.array([np.cos(e) * np.sin(a), -np.cos(e) * np.cos(a), np.sin(e)], F32)  # az 0 = sun at top
    n = n / np.linalg.norm(n, axis=-1, keepdims=True)
    ndl = np.clip((n * L).sum(-1), 0, None)
    lit = 0.30 + 0.70 * ndl / np.sin(e)
    out = srgb_to_lin(alb_srgb) * lit[..., None]
    if rough is not None:  # camera looking straight down: half vector between L and V=(0,0,1)
        Hv = L + np.array([0, 0, 1], F32)
        Hv /= np.linalg.norm(Hv)
        ndh = np.clip((n * Hv).sum(-1), 0, 1)
        a2 = np.maximum(rough, 0.05) ** 4
        d = a2 / (np.pi * ((ndh * ndh) * (a2 - 1) + 1) ** 2)
        out = out + (0.04 * d * ndl * 0.25)[..., None]
    return np.clip(lin_to_srgb(np.clip(out, 0, 1)), 0, 1)


def to_u8(x):
    return (np.clip(x, 0, 1) * 255 + 0.5).astype(np.uint8)


def save_layer(name, H, A, R, nstrength):
    n = height_to_normal(H, nstrength)
    d_png = np.concatenate([to_u8(A), np.clip(to_u8(R), 1, 255)[..., None]], -1)
    n_png = to_u8(n * 0.5 + 0.5)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    Image.fromarray(d_png, "RGBA").save(OUT_DIR / f"T_Berlin_{name}_D.png", optimize=False)
    Image.fromarray(n_png, "RGB").save(OUT_DIR / f"T_Berlin_{name}_N.png", optimize=False)
    return d_png, n_png


def decode(d_png, n_png):
    A = d_png[..., :3].astype(F32) / 255
    R = d_png[..., 3].astype(F32) / 255
    n = n_png.astype(F32) / 255 * 2 - 1
    return A, R, n


def seam_report(img):
    x = img.astype(F32)
    inner_c = np.abs(x[:, 1:] - x[:, :-1]).mean()
    edge_c = np.abs(x[:, 0] - x[:, -1]).mean()
    inner_r = np.abs(x[1:] - x[:-1]).mean()
    edge_r = np.abs(x[0] - x[-1]).mean()
    return edge_c / (inner_c + 1e-9), edge_r / (inner_r + 1e-9)


def stats(d_png):
    A = d_png[..., :3].astype(np.float64) / 255
    lin = srgb_to_lin(A).reshape(-1, 3).mean(0)
    avg = lin_to_srgb(lin) * 255
    r = d_png[..., 3].astype(np.float64)
    return avg, r.mean() / 255, r.mean()


def contact_sheet(name, d_png, n_png):
    A, R, n = decode(d_png, n_png)
    t = 192
    # 3x3 tiled panels from a mip-like box reduction (2048 -> 256) then resize to 192
    def mip(img, f):
        return box_down(img, f)
    A8, R8, n8 = mip(A, 8), mip(R, 8), mip(n, 8)
    def tile3(img):
        im = Image.fromarray(to_u8(img)).resize((t, t), Image.LANCZOS)
        a = np.asarray(im)
        return np.tile(a, (3, 3, 1) if a.ndim == 3 else (3, 3))
    p_alb = tile3(A8)
    p_lit = tile3(shade(A8, n8, R8))
    p_r = tile3(R8)
    p_r = np.repeat(p_r[..., None], 3, -1)
    S = 576
    crop = slice(700, 700 + S)
    lit_full = shade(A[crop, crop], n[crop, crop], R[crop, crop])
    c2 = slice(400, 400 + 2 * S)
    lit_half = box_down(shade(A[c2, c2], n[c2, c2], R[c2, c2]), 2)
    top = np.concatenate([p_alb, p_lit, p_r], 1)
    bot = np.concatenate([to_u8(A[crop, crop]), to_u8(lit_full), to_u8(lit_half)], 1)
    Image.fromarray(np.concatenate([top, bot], 0)).save(OUT_DIR / f"preview_{name}.png")


def build_one(name):
    rng = np.random.default_rng(SEEDS[name])
    H, A, R, _ = BUILDERS[name](rng)
    d_png, n_png = save_layer(name, H, A, R, NSTRENGTH[name])
    return name, d_png, n_png


def run_layer(name, preview=True):
    name, d_png, n_png = build_one(name)
    if preview:
        contact_sheet(name, d_png, n_png)
    avg, r01, r255 = stats(d_png)
    sd = seam_report(d_png)
    sn = seam_report(n_png)
    return (f"{name:13s} albedo sRGB avg (linear-mean) = ({avg[0]:.0f},{avg[1]:.0f},{avg[2]:.0f})  "
            f"roughness mean = {r01:.3f} ({r255:.0f}/255)  seam ratio D {sd[0]:.2f}/{sd[1]:.2f} "
            f"N {sn[0]:.2f}/{sn[1]:.2f}")


def preview_all(names):
    """Six layers side by side as from the RTS camera: 8x8 tiles (~40 m) per panel, mip-then-shade."""
    panels = []
    for name in names:
        d = np.asarray(Image.open(OUT_DIR / f"T_Berlin_{name}_D.png"))
        nn = np.asarray(Image.open(OUT_DIR / f"T_Berlin_{name}_N.png"))
        A, R, n = decode(d, nn)
        A, R, n = box_down(A, 16), box_down(R, 16), box_down(n, 16)  # 128 px per 5 m tile
        img = to_u8(shade(A, n, R))
        panels.append(np.tile(img, (8, 8, 1)))
    while len(panels) % 3:
        panels.append(np.zeros_like(panels[0]))
    rows = [np.concatenate(panels[i:i + 3], 1) for i in range(0, len(panels), 3)]
    Image.fromarray(np.concatenate(rows, 0)).save(OUT_DIR / "preview_all.png")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--layers", nargs="*", default=LAYERS, choices=LAYERS)
    ap.add_argument("--no-preview", action="store_true")
    ap.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 1))
    args = ap.parse_args(argv)
    prev = not args.no_preview
    if args.jobs > 1 and len(args.layers) > 1:
        with ProcessPoolExecutor(max_workers=args.jobs) as ex:
            lines = list(ex.map(run_layer, args.layers, [prev] * len(args.layers)))
    else:
        lines = [run_layer(n, prev) for n in args.layers]
    for line in lines:
        print(line)
    if prev and all((OUT_DIR / f"T_Berlin_{n}_D.png").exists() for n in LAYERS):
        preview_all(LAYERS)
        print("preview_all.png written")
    print(f"output: {OUT_DIR}")


if __name__ == "__main__":
    sys.exit(main())
