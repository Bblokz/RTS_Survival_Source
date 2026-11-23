"""Ger_IntroMission ground variety pass (plain python + numpy, no Unreal). 2026-11-19.

User: "add more details to the landscape; right now many places still use the same sand shader; blend some of the other
layers of the landscape in there."

Only cells that are pure Sand today (Sand >= 230, every other layer <= 25) change, so everything the user or earlier
passes painted stays as it is. Layers blended in (see memory intro-mission-detail-pass for what each one looks like):
    DesertGravel      subtle red gravel   broad organic patches over the open ground
    DesertRockDark    pale pebbles        on slopes, at the foot of steep faces and cliffs, small scattered patches
    Weedy             dark cracked mud    in local hollows (lower than the ground around them) and some flat lows
ScorchSandyStone / ScorchConcrete (charcoal, slabs) are not used: they read as burns and concrete.
Masks are multi-octave value noise with domain warping, so the patches are organic, never dots or stripes.

    python paint_gen.py   -> Saved/IntroPaint/paint_full_<layer>.r8 (full grid) + preview_before/after.png + stats
Input: Saved/IntroRear/terrain/now_full_<layer>.r8 (paint on the map; heights from ridge_full_height.r16).
"""
import os

import numpy as np

GRID = (105, 75, 825, 585)
ORIGIN = (-51600.0, -32701.0)
SY, SX = GRID[3] - GRID[1] + 1, GRID[2] - GRID[0] + 1
LAYERS = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']
T = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/IntroPaint/'
RIM = 6                          # outer vertices (eased skirt rim) stay as they are
ALBEDO = {'Sand': (196, 120, 72), 'DesertRockDark': (214, 196, 170), 'Weedy': (88, 62, 46),
          'DesertGravel': (158, 80, 52), 'ScorchSandyStone': (52, 46, 40), 'ScorchConcrete': (72, 72, 72)}


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def value_noise(shape, cell, rng):
    """Smooth value noise with features about `cell` vertices (metres) across, 0..1."""
    gy, gx = shape[0] // cell + 3, shape[1] // cell + 3
    base = rng.random((gy, gx))
    y = np.arange(shape[0]) / cell
    x = np.arange(shape[1]) / cell
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    ty, tx = smooth(y - y0)[:, None], smooth(x - x0)[None, :]
    a = base[y0][:, x0]
    b = base[y0][:, x0 + 1]
    c = base[y0 + 1][:, x0]
    d = base[y0 + 1][:, x0 + 1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def fbm(shape, cells, seed, warp=0.0):
    rng = np.random.default_rng(seed)
    total, amp, norm = np.zeros(shape), 1.0, 0.0
    for cell in cells:
        total += amp * value_noise(shape, cell, rng)
        norm += amp
        amp *= 0.55
    n = total / norm
    if warp > 0:
        # domain warp: resample through a second noise field so the patches curl instead of looking like blobs
        wy = (value_noise(shape, cells[0], rng) - 0.5) * warp
        wx = (value_noise(shape, cells[0], rng) - 0.5) * warp
        yy = np.clip(np.arange(shape[0])[:, None] + wy, 0, shape[0] - 1).astype(int)
        xx = np.clip(np.arange(shape[1])[None, :] + wx, 0, shape[1] - 1).astype(int)
        n = n[yy, xx]
    # stretch to 0..1 by percentile so thresholds mean coverage
    lo, hi = np.percentile(n, 1), np.percentile(n, 99)
    return np.clip((n - lo) / (hi - lo), 0, 1)


def box_mean(a, r):
    c = np.cumsum(np.cumsum(np.pad(a, ((r + 1, r), (r + 1, r)), mode='edge'), axis=0), axis=1)
    s = c[2 * r + 1:, 2 * r + 1:] - c[:-2 * r - 1, 2 * r + 1:] - c[2 * r + 1:, :-2 * r - 1] + c[:-2 * r - 1, :-2 * r - 1]
    return s / (2 * r + 1) ** 2


def dilate(m, it):
    out = m.copy()
    for _ in range(it):
        o = out.copy()
        o[1:, :] |= out[:-1, :]
        o[:-1, :] |= out[1:, :]
        o[:, 1:] |= out[:, :-1]
        o[:, :-1] |= out[:, 1:]
        out = o
    return out


def preview(W, path):
    from PIL import Image
    img = np.zeros((SY, SX, 3))
    for n in LAYERS:
        img += W[n][..., None].astype(float) / 255.0 * np.array(ALBEDO[n])[None, None, :]
    Image.fromarray(np.clip(img[::-1], 0, 255).astype(np.uint8)).resize((SX * 2, SY * 2), Image.NEAREST).save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    W = {n: np.fromfile(T + 'now_full_%s.r8' % n, np.uint8).reshape(SY, SX) for n in LAYERS}
    Z = (np.fromfile(T + 'ridge_full_height.r16', np.uint16).reshape(SY, SX).astype(float) - 32768.0) * 100.0 / 128.0
    shape = (SY, SX)
    pure = W['Sand'] >= 230
    for n in LAYERS[1:]:
        pure &= W[n] <= 25
    pure[:RIM, :] = pure[-RIM:, :] = pure[:, :RIM] = pure[:, -RIM:] = False
    # fade the new paint out over 3 m next to anything already painted, so no hard seam forms against it
    near_paint = dilate(~pure, 3) & pure
    fade = np.where(near_paint, 0.45, 1.0)

    gy, gx = np.gradient(Z, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    hollow = box_mean(Z, 20) - Z                       # cm the ground lies below its 41 m surroundings

    n_gravel = fbm(shape, [70, 34, 16, 7], 11, warp=18)
    n_gravel2 = fbm(shape, [22, 10, 5], 12)
    n_pebble = fbm(shape, [28, 12, 5], 21, warp=8)
    n_mud = fbm(shape, [36, 16, 7, 3], 31, warp=10)
    n_break = fbm(shape, [9, 4, 2], 41)                # fine breakup inside every patch

    gravel = smooth((n_gravel - 0.52) / 0.16) * (0.45 + 0.35 * n_gravel2)
    gravel *= 0.75 + 0.5 * (n_break - 0.5)

    steep = slope > 40.0
    cliff_foot = dilate(steep, 5) & ~steep
    pebble = np.maximum(smooth((slope - 12.0) / 14.0) * smooth((n_pebble - 0.35) / 0.25) * 0.75,
                        cliff_foot * (0.55 + 0.35 * n_pebble))
    pebble = np.maximum(pebble, smooth((n_pebble - 0.86) / 0.06) * 0.6)          # a few small scattered patches
    pebble *= 0.8 + 0.4 * (n_break - 0.5)

    mud = smooth((hollow - 12.0) / 30.0) * smooth((n_mud - 0.30) / 0.30) * 0.85
    mud = np.maximum(mud, smooth((n_mud - 0.90) / 0.05) * smooth((12.0 - slope) / 6.0) * 0.55)
    mud *= 0.8 + 0.4 * (n_break - 0.5)

    add = {'DesertGravel': gravel, 'DesertRockDark': pebble, 'Weedy': mud}
    total = sum(add.values())
    scale = np.where(total > 0.85, 0.85 / np.maximum(total, 1e-6), 1.0)
    N = {n: W[n].copy() for n in LAYERS}
    keep_sand = np.ones(shape)
    for n, w in add.items():
        wv = np.where(pure, w * scale * fade, 0.0)
        N[n] = np.clip(np.round(W[n] + wv * 255.0), 0, 255).astype(np.uint8)
        keep_sand -= wv
    # weights must add up to 255: Sand takes whatever is left
    others = sum(N[n].astype(int) for n in LAYERS[1:])
    N['Sand'] = np.where(pure, np.clip(255 - others, 0, 255), W['Sand']).astype(np.uint8)
    for n in LAYERS:
        N[n].tofile(OUT + 'paint_full_%s.r8' % n)
    changed = pure & (np.abs(N['Sand'].astype(int) - W['Sand'].astype(int)) > 10)
    tot = SY * SX
    print('pure sand before %.1f%%, cells changed %.1f%%' % (100 * pure.sum() / tot, 100 * changed.sum() / tot))
    for n in LAYERS:
        print('%-16s mean %5.1f -> %5.1f, >128: %4.1f%% -> %4.1f%%' % (
            n, W[n].mean(), N[n].mean(), 100 * (W[n] > 128).sum() / tot, 100 * (N[n] > 128).sum() / tot))
    preview(W, OUT + 'preview_before.png')
    preview(N, OUT + 'preview_after.png')


if __name__ == '__main__':
    main()
