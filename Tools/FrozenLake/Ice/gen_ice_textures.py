"""Generate the source PNGs for the Frozen Lake ice shader (run with plain python 3 + numpy + Pillow, outside Unreal).

  python gen_ice_textures.py            -> textures/T_FrozenLakeIce_Noise.png, T_FrozenLakeIce_StateNeutral.png,
                                           T_FrozenLakeIce_TestState.png (+ preview PNGs in textures/preview/)

T_FrozenLakeIce_Noise (1024^2, tiles seamlessly, linear):
  R  fbm clouds, rank-equalised to a uniform 0..1 (thresholds behave like percentiles)
  G  crack network: distance to the nearest Voronoi cell edge (domain-warped), / 0.25 cell, clamped. 0 = on a crack.
  B  root network: ridged, domain-warped noise (1 on a root, falls off to 0). Shader takes pow() for thin roots.
  A  bubbles / frost specks: soft dots of random size on a jittered grid.

T_FrozenLakeIce_TestState (1024^2, linear, clamp): the state mask for the test lake (see build_ice_material.py),
  laid out over TEST_LAKE_SIZE_M metres. R thickness (1 heavy, 0.5 medium, 0 infantry/black), G warn glow,
  B open water (holes / melted), A vein glow under black ice.
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'textures')
PREVIEW = os.path.join(OUT, 'preview')
SIZE = 1024
TEST_LAKE_SIZE_M = 586.0
rng = np.random.default_rng(1976)


# ---------------------------------------------------------------- periodic helpers
def spectral_noise(size, beta, low_cut=1.0, high_cut=None, seed=None):
    """Periodic fbm by spectral synthesis: white noise shaped by 1/f^beta (tiles perfectly)."""
    r = np.random.default_rng(seed)
    white = r.standard_normal((size, size))
    f = np.fft.fftfreq(size) * size
    fx, fy = np.meshgrid(f, f)
    freq = np.sqrt(fx * fx + fy * fy)
    amp = np.zeros_like(freq)
    ok = freq >= low_cut
    amp[ok] = 1.0 / freq[ok] ** beta
    if high_cut:
        amp[freq > high_cut] = 0.0
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * amp))
    return out


def rank01(a):
    """Rank-equalise to a uniform 0..1 distribution."""
    flat = a.ravel()
    order = flat.argsort()
    ranks = np.empty_like(order)
    ranks[order] = np.arange(flat.size)
    return (ranks / (flat.size - 1)).reshape(a.shape).astype(np.float32)


def norm01(a):
    return ((a - a.min()) / (a.max() - a.min())).astype(np.float32)


def bilinear_wrap(field, u, v):
    """Sample a periodic field at fractional pixel coords (u, v) with wrap."""
    n = field.shape[0]
    u0 = np.floor(u).astype(int)
    v0 = np.floor(v).astype(int)
    fu = u - u0
    fv = v - v0
    u0 %= n
    v0 %= n
    u1 = (u0 + 1) % n
    v1 = (v0 + 1) % n
    a = field[v0, u0] * (1 - fu) + field[v0, u1] * fu
    b = field[v1, u0] * (1 - fu) + field[v1, u1] * fu
    return a * (1 - fv) + b * fv


def voronoi_f1f2(size, cells, seed, warp_u=None, warp_v=None):
    """Periodic jittered-grid Voronoi: F1, F2 in cell units for every pixel."""
    r = np.random.default_rng(seed)
    jitter = r.random((cells, cells, 2)) * 0.9 + 0.05
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    px = xs / size * cells
    py = ys / size * cells
    if warp_u is not None:
        px = px + warp_u
        py = py + warp_v
    cx = np.floor(px).astype(int)
    cy = np.floor(py).astype(int)
    f1 = np.full((size, size), 9.0, np.float32)
    f2 = np.full((size, size), 9.0, np.float32)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            gx = cx + ox
            gy = cy + oy
            j = jitter[gy % cells, gx % cells]
            dx = gx + j[..., 0] - px
            dy = gy + j[..., 1] - py
            d = np.sqrt(dx * dx + dy * dy)
            f2 = np.where(d < f1, f1, np.minimum(f2, d))
            f1 = np.minimum(f1, d)
    return f1, f2


def bubbles(size, cells, seed):
    r = np.random.default_rng(seed)
    jitter = r.random((cells, cells, 2)) * 0.8 + 0.1
    radius = r.random((cells, cells)) * 0.28 + 0.07
    present = (r.random((cells, cells)) < 0.55).astype(np.float32)
    bright = r.random((cells, cells)) * 0.5 + 0.5
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    px = xs / size * cells
    py = ys / size * cells
    cx = np.floor(px).astype(int)
    cy = np.floor(py).astype(int)
    out = np.zeros((size, size), np.float32)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            gx = (cx + ox) % cells
            gy = (cy + oy) % cells
            j = jitter[gy, gx]
            dx = cx + ox + j[..., 0] - px
            dy = cy + oy + j[..., 1] - py
            d = np.sqrt(dx * dx + dy * dy)
            rad = radius[gy, gx]
            dot = np.clip(1.0 - (d - rad * 0.55) / (rad * 0.45), 0.0, 1.0) * present[gy, gx] * bright[gy, gx]
            out = np.maximum(out, dot)
    return out


# ---------------------------------------------------------------- noise texture
def make_noise():
    n = SIZE
    clouds = rank01(spectral_noise(n, 1.55, seed=11))

    # cracks: warped Voronoi (long, slightly wavy cracks), 9 cells per tile
    wu = spectral_noise(n, 1.8, seed=21)
    wv = spectral_noise(n, 1.8, seed=22)
    wu = (norm01(wu) - 0.5) * 0.55
    wv = (norm01(wv) - 0.5) * 0.55
    f1, f2 = voronoi_f1f2(n, 9, seed=31, warp_u=wu, warp_v=wv)
    edge = f2 - f1
    # second, finer generation of cracks that only branch off the big ones' cells (ice fractures in two orders)
    f1b, f2b = voronoi_f1f2(n, 23, seed=32, warp_u=wu * 2.2, warp_v=wv * 2.2)
    edge_b = (f2b - f1b) / (23.0 / 9.0)  # into big-cell units
    # the small cracks only in some places; fade them out softly (a hard keep-mask leaves cut-off crack stubs)
    keep = np.clip((rank01(spectral_noise(n, 2.0, seed=33)) - 0.45) / 0.2, 0.0, 1.0)
    edge_all = np.minimum(edge, edge_b * 1.6 + (1.0 - keep) * 0.6)
    cracks = np.clip(edge_all / 0.25, 0.0, 1.0)

    # roots: ridged, domain-warped noise (main roots) + finer ridges that live near the main roots
    rw1 = norm01(spectral_noise(n, 1.7, seed=41)) - 0.5
    rw2 = norm01(spectral_noise(n, 1.7, seed=42)) - 0.5
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    base = rank01(spectral_noise(n, 2.1, low_cut=2.0, seed=43))
    main = bilinear_wrap(base, xs + rw1 * 90.0, ys + rw2 * 90.0)
    ridge1 = 1.0 - np.abs(main * 2.0 - 1.0)
    fine = rank01(spectral_noise(n, 1.7, low_cut=6.0, seed=44))
    fine = bilinear_wrap(fine, xs + rw2 * 40.0, ys + rw1 * 40.0)
    ridge2 = 1.0 - np.abs(fine * 2.0 - 1.0)
    roots = np.maximum(ridge1 ** 3, (ridge2 ** 5) * np.clip(ridge1 * 1.6 - 0.35, 0, 1))
    roots = np.clip(roots, 0, 1)

    specks = bubbles(n, 48, seed=51)

    rgba = np.stack([clouds, cracks, roots, specks], axis=-1)
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA')
    img.save(os.path.join(OUT, 'T_FrozenLakeIce_Noise.png'))
    for i, name in enumerate('RGBA'):
        Image.fromarray((np.clip(rgba[..., i], 0, 1) * 255).astype(np.uint8), 'L').save(
            os.path.join(PREVIEW, 'noise_%s.png' % name))
    # 2x2 tile preview of the roots at shader sharpness, to check seams
    rt = np.tile(np.clip(roots, 0, 1) ** 6, (2, 2))
    Image.fromarray((rt * 255).astype(np.uint8), 'L').resize((1024, 1024)).save(os.path.join(PREVIEW, 'roots_tiled.png'))


# ---------------------------------------------------------------- test state mask
def blur(a, radius_px):
    """Separable box blur x3 (≈ gaussian), clamp edges."""
    r = max(1, int(radius_px))
    out = a.astype(np.float32)
    for _ in range(3):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r, r)
            p = np.pad(out, pad, mode='edge')
            c = np.cumsum(p, axis=axis, dtype=np.float64)
            c = np.concatenate([np.zeros_like(c.take([0], axis=axis)), c], axis=axis)
            hi = c.take(np.arange(2 * r + 1, c.shape[axis]), axis=axis)
            lo = c.take(np.arange(0, c.shape[axis] - 2 * r - 1), axis=axis)
            out = ((hi - lo) / (2 * r + 1)).astype(np.float32)
    return out


def make_test_state():
    """World layout in metres from the lake's min corner (LakeOrigin), X = north, Y = east.

    The material maps uv = (WP.xy - LakeOrigin.xy) / LakeSize, so u follows world X and v follows world Y.
    UE puts u across image columns and v down image rows: image column = X, image row = Y
    (so in the PNG north is to the RIGHT and east is DOWN). Built as [x, y] arrays, saved transposed.
    """
    n = SIZE
    m_per_px = TEST_LAKE_SIZE_M / n
    xs = (np.arange(n) + 0.5) * m_per_px
    X, Y = np.meshgrid(xs, xs, indexing='ij')  # X[i, j] = x of i, Y[i, j] = y of j
    T = np.full((n, n), 0.5, np.float32)       # medium everywhere
    warn = np.zeros((n, n), np.float32)
    open_ = np.zeros((n, n), np.float32)
    vein = np.zeros((n, n), np.float32)
    wob = (norm01(spectral_noise(n, 1.9, seed=61)) - 0.5)  # organic wobble for blob edges

    # Station A (types side by side): centre (150, 150) m, bands across Y, 100 m long in X.
    ax, ay = 150.0, 150.0
    in_a = np.abs(X - ax) < 55.0
    T = np.where(in_a & (Y < ay - 14.0), 1.0, T)
    T = np.where(in_a & (Y > ay + 14.0) & (Y < ay + 60.0), 0.0, T)

    # Heavy ice road: 10 m wide, sinuous, from the south-west shore to the north-east.
    t = np.linspace(0, 1, 400)
    rx = 60 + t * 470
    ry = 300 + 120 * np.sin(t * 5.0) + t * 120
    road = np.zeros((n, n), bool)
    for x0, y0 in zip(rx, ry):
        road |= (X - x0) ** 2 + (Y - y0) ** 2 < 5.0 ** 2
    T = np.where(road, 1.0, T)

    # A few organic heavy and black patches for the wide view.
    for cx, cy, rad, val in ((420, 120, 45, 1.0), (480, 470, 60, 0.0), (300, 520, 35, 1.0), (90, 470, 40, 0.0)):
        d = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2) / rad + wob * 0.6
        T = np.where(d < 1.0, val, T)

    # Station B (reveal): black ice over the vein, centre (380, 300).
    bx, by = 380.0, 300.0
    d = np.sqrt((X - bx) ** 2 + ((Y - by) * 0.8) ** 2) / 48.0 + wob * 0.5
    T = np.where(d < 1.0, 0.0, T)
    vd = np.sqrt((X - bx) ** 2 + (Y - by) ** 2) / 75.0 + wob * 0.4
    vein = np.clip(1.4 - vd, 0, 1)  # the vein reaches under the surrounding medium ice too (hidden there)

    # Station C (hazards), centre (250, 400): pump-zone melt front + warn glow + shell holes.
    cx, cy = 250.0, 420.0
    r = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2)
    front = 22.0  # metres the front has travelled
    open_ = np.maximum(open_, (r < front).astype(np.float32))
    ring = np.clip((r - front) / 14.0, 0, 1)  # thinning band ahead of the front: black -> medium over 14 m
    T = np.where(r < front + 14.0, np.minimum(T, ring * 0.5), T)
    warn = np.maximum(warn, np.clip(1.0 - (r - front - 4.0) / 30.0, 0, 1) * (r > front - 2))
    # a separate warn-only patch (pump about to blow, heavy ice still intact), west of the front
    wd = np.sqrt((X - 250.0) ** 2 + (Y - 345.0) ** 2) / 26.0 + wob * 0.3
    T = np.where(wd < 1.0, 1.0, T)
    warn = np.maximum(warn, np.clip(1.6 - wd * 1.2, 0, 1))
    # shell holes in heavy and medium ice near station A's east side and station C
    for hx, hy, hr in ((175, 135, 3.0), (120, 128, 2.2), (150, 185, 2.6), (205, 395, 3.5), (215, 455, 2.5),
                       (190, 430, 4.0), (165, 150, 1.8)):
        open_ = np.maximum(open_, ((X - hx) ** 2 + (Y - hy) ** 2 < hr * hr).astype(np.float32))

    # Soft painted borders: blur thickness ~2.5 m, warn ~4 m, holes ~0.6 m, vein ~6 m.
    T = blur(T, 2.5 / m_per_px)
    warn = blur(warn, 4.0 / m_per_px)
    open_ = blur(open_, 0.6 / m_per_px)
    vein = blur(vein, 6.0 / m_per_px)

    rgba = np.stack([T, warn, open_, vein], axis=-1)
    rgba = np.transpose(rgba, (1, 0, 2))  # row = y (v), col = x (u)
    Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(
        os.path.join(OUT, 'T_FrozenLakeIce_TestState.png'))
    prev = np.stack([rgba[..., 0], rgba[..., 1], rgba[..., 2]], axis=-1)
    Image.fromarray((np.clip(prev, 0, 1) * 255).astype(np.uint8), 'RGB').save(os.path.join(PREVIEW, 'test_state_rgb.png'))
    Image.fromarray((np.clip(rgba[..., 3], 0, 1) * 255).astype(np.uint8), 'L').save(
        os.path.join(PREVIEW, 'test_state_vein.png'))


def make_neutral():
    a = np.zeros((4, 4, 4), np.uint8)
    a[..., 0] = 128  # medium thickness, no warn, no open water, no vein
    Image.fromarray(a, 'RGBA').save(os.path.join(OUT, 'T_FrozenLakeIce_StateNeutral.png'))


if __name__ == '__main__':
    os.makedirs(PREVIEW, exist_ok=True)
    make_noise()
    make_test_state()
    make_neutral()
    print('written to', OUT)
