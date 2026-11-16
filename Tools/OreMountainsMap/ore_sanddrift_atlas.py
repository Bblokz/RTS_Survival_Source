"""SandDrift atlas for NS_SandPush round 3 (2026-11-16): few large ground-lying drift sprites instead of many small pieces.

    python Source/Tools/OreMountainsMap/ore_sanddrift_atlas.py [--preview]

Owner after round 2: "too many small pieces, which makes it look very flaky and fake ... larger particles with more
detail ... efficiently using more of the UV space". So:
  * coverage is a plain clip of a SMOOTH authored density field (connected tongues with streaky fingers, features of
    tens of screen pixels), never a grain noise;
  * the detail is in shading: a height field gives a tangent-space normal (wind lineations, transverse ripples), lit by
    the engine on sprites lying flat on the ground;
  * each 1024 cell is filled: 2-4 tongues spread across the cell, 4 % margin.

T_SandDrift_Atlas 2048x2048, 2x2 cells, linear, BC7 (Saved/OreMountains/sandstreak/tex/):
  R = density D (material draws where D * particle alpha + jitter > Threshold), G = static body shade (0.5 neutral)
T_SandDrift_Detail 1024x1024, TILEABLE, linear, BC7: R, G = tangent-space normal X, Y (0.5 flat), B = lineation
  shade (0.5 neutral). The material scrolls it downwind (hardware wrap, no seams) so the sand flows inside the
  fixed outline of each sprite.
U runs along the wind (head of each tongue upwind at low U, frayed fingers downwind at high U).
"""
import os
import sys

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
OUT = os.path.join(ROOT, 'Saved', 'OreMountains', 'sandstreak', 'tex')
os.makedirs(OUT, exist_ok=True)
CELL = 1024
RNG = np.random.default_rng(1976)


def periodic_noise(n, stretch_x, cutoff, rng, power=1.6):
    """Periodic band-limited noise (FFT), anisotropic: features stretch_x times longer along x (U) than along y."""
    fy = np.fft.fftfreq(n)[:, None]
    fx = np.fft.fftfreq(n)[None, :]
    r = np.sqrt((fx * stretch_x) ** 2 + fy ** 2)
    amp = np.where(r > 0, 1.0 / np.maximum(r, 1e-4) ** power, 0.0) * np.exp(-(r / cutoff) ** 2)
    phase = np.exp(2j * np.pi * rng.random((n, n)))
    field = np.real(np.fft.ifft2(amp * phase))
    field -= field.mean()
    return field / (field.std() + 1e-8)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def make_cell(rng, sheet=False):
    """R: static streamer density. G/B/A: shade + normal detail that is PERIODIC in the cell (no body term), so the
    material can scroll it downwind (frac within the cell) while the outline stays put."""
    n = CELL
    y, x = (np.mgrid[0:n, 0:n].astype(np.float32) + 0.5) / n
    warp_y = periodic_noise(n, 3.0, 0.015, rng) * 0.025
    yw = y + warp_y
    lineation = periodic_noise(n, 10.0, 0.08, rng, power=1.2)       # long wind streaks along U
    lineation_fine = periodic_noise(n, 16.0, 0.22, rng, power=1.0)
    warp_x = periodic_noise(n, 1.0, 0.01, rng) * 0.05
    xw = x + warp_x
    filaments = periodic_noise(n, 18.0, 0.10, rng, power=1.1)      # thin long filaments the tails break into
    breakup = smoothstep(-0.9, 0.9, filaments)                         # 0..1, thin streaks along U
    body = np.zeros((n, n), np.float32)
    # 'sheet' cells: 2-3 wide overlapping tongues that merge into one sand sheet; else 3-4 separate streamers
    count = int(rng.integers(2, 4)) if sheet else int(rng.integers(3, 5))
    span = 0.5 if sheet else 0.84
    lanes = 0.5 - span / 2 + (np.arange(count) + rng.uniform(0.2, 0.8, count)) / count * span
    for cy in lanes:
        head = rng.uniform(0.03, 0.16)
        tail = rng.uniform(0.86, 0.95)
        width = rng.uniform(0.11, 0.16) if sheet else rng.uniform(0.055, 0.09)
        strength = rng.uniform(0.8, 1.0)
        along = np.clip((xw - head) / (tail - head), 0, 1)
        centre = cy + rng.uniform(-0.04, 0.04) * np.sin(np.pi * along) + 0.015 * periodic_noise(n, 1.0, 0.006, rng)
        w = width * (0.45 + 0.55 * smoothstep(0.0, 0.25, along)) * (1.0 + 0.9 * along)    # head narrow, tail fans out
        across = np.exp(-np.abs((yw - centre) / w) ** 1.6)
        rise = smoothstep(head - 0.01, head + 0.05, xw)
        fall = 1.0 - 0.6 * smoothstep(head + 0.3 * (tail - head), tail, xw)
        # fingers: every row across the wind ends at its own length (1-D noise across, ~1-2 % of the cell wide)
        rows = cv2.GaussianBlur(rng.random((n, 1)).astype(np.float32), (1, 0), sigmaX=0.1, sigmaY=rng.uniform(4, 9)).ravel()
        rows = (rows - rows.min()) / (np.ptp(rows) + 1e-6)
        row_tail = tail - 0.32 * (1.0 - rows[:, None]) ** 1.3
        end = 1.0 - smoothstep(row_tail - 0.05, row_tail, xw)
        body = np.maximum(body, strength * across * rise * end * (0.25 + 0.75 * fall))
    tail_weight = np.clip((x - 0.25) / 0.6, 0, 1)
    breakup = smoothstep(-0.9, 0.9, filaments)                         # 0..1, thin streaks along U
    dens = body * (1.0 + 0.2 * lineation) * ((1 - tail_weight) * (0.75 + 0.35 * breakup) + tail_weight * (0.35 + 1.0 * breakup))
    margin = smoothstep(0.0, 0.04, x) * smoothstep(0.0, 0.04, 1 - x) * smoothstep(0.0, 0.04, y) * smoothstep(0.0, 0.04, 1 - y)
    dens = np.clip(dens * margin * 1.1, 0, 1)

    # periodic detail: lineation ridges + faint transverse ripples (crests across the wind)
    ripple_phase = x * 32.0 + 1.2 * periodic_noise(n, 1.0, 0.02, rng)
    ripples = np.sin(2 * np.pi * ripple_phase)
    height = 0.6 * lineation + 0.3 * lineation_fine + 0.08 * ripples
    gy, gx = np.gradient(height * 2.0)
    nrm = np.stack([-gx, -gy, np.ones_like(gx)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    detail_shade = np.clip(0.5 + 0.18 * lineation + 0.10 * lineation_fine, 0, 1)
    thick = cv2.GaussianBlur(dens, (0, 0), 8)
    body_shade = np.clip(0.5 + 0.6 * (thick - cv2.GaussianBlur(thick, (0, 0), 24)) + 0.15 * (thick - 0.4), 0, 1)
    return dens, body_shade, nrm, detail_shade


def build():
    atlas = np.zeros((2 * CELL, 2 * CELL, 4), np.float32)
    cells = []
    detail = None
    for k in range(4):
        d, s, nrm, ds = make_cell(RNG, sheet=(k % 3 == 0))
        r, c = divmod(k, 2)
        sl = (slice(r * CELL, (r + 1) * CELL), slice(c * CELL, (c + 1) * CELL))
        atlas[sl + (0,)] = d
        atlas[sl + (1,)] = s
        atlas[sl + (3,)] = 1.0
        cells.append((d, ds, nrm))
        if detail is None:          # the first cell's periodic lineation/ripple field is the tileable detail map
            detail = np.stack([nrm[..., 0] * 0.5 + 0.5, nrm[..., 1] * 0.5 + 0.5, ds, np.ones_like(ds)], -1)
    Image.fromarray((np.clip(atlas, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_SandDrift_Atlas.png'))
    Image.fromarray((np.clip(detail, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_SandDrift_Detail.png'))
    return atlas, cells


def stats(cells, threshold):
    for k, (d, _, _) in enumerate(cells):
        m = (d > threshold).astype(np.uint8)
        count, labels, st, _ = cv2.connectedComponentsWithStats(m, 8)
        big = (st[1:, cv2.CC_STAT_AREA] > 400).sum()
        # minimum feature width ~ 2 x max distance-transform value along thin fingers; report the 10th percentile
        dist = cv2.distanceTransform(m, cv2.DIST_L2, 3)
        ridge = dist[(dist > 0) & (dist >= cv2.dilate(dist, np.ones((5, 5))) - 1e-3)]
        print('cell %d: coverage %.2f, pieces %d (>400 texels %d), finger width p10 %.0f texels' % (
            k, m.mean(), count - 1, big, 2 * np.percentile(ridge, 10) if ridge.size else 0))


def preview(cells, threshold=0.30):
    """Old translucent shot vs. ~7 drift sprites lying on the ground at 7.8 cm a pixel (sprite 13-26 m)."""
    bg = np.asarray(Image.open(os.path.join(ROOT, 'Saved', 'OreMountains', 'fx', 'y_streak_valley.png')).convert('RGB')).astype(np.float32) / 255
    canvas = bg.copy()
    colour = np.array([212, 197, 172], np.float32) / 255
    sun = np.array([0.35, -0.45, 0.82]); sun /= np.linalg.norm(sun)
    rr = np.random.default_rng(5)
    cx, cy, cm_px, wind = 640, 330, 7.8, -25.0
    for i in range(5):
        d, s, nrm = cells[rr.integers(0, 4)]
        fade = rr.choice([1.0, 1.0, 0.85, 0.6])
        px = int(rr.uniform(1300, 2600) / cm_px)
        dd = cv2.resize(d, (px, px), interpolation=cv2.INTER_AREA)
        ss = cv2.resize(s, (px, px), interpolation=cv2.INTER_AREA)
        nn = cv2.resize(nrm, (px, px), interpolation=cv2.INTER_AREA)
        light = np.clip((nn * sun).sum(-1) / sun[2], 0.85, 1.15)
        rgb = colour[None, None] * (0.9 + 0.2 * ss[..., None]) * (0.95 + 0.1 * dd[..., None]) * light[..., None]
        stack = np.concatenate([dd[..., None], rgb], -1)
        m = cv2.getRotationMatrix2D((px / 2, px / 2), wind + rr.uniform(-12, 12), 1.0)
        stack = cv2.warpAffine(stack, m, (px, px), flags=cv2.INTER_LINEAR)
        along, side = rr.uniform(-900, 900), rr.uniform(-250, 250)
        ang = np.radians(-wind)
        x0 = int(cx + (along * np.cos(ang) - side * np.sin(ang)) / cm_px - px / 2)
        y0 = int(cy + (along * np.sin(ang) + side * np.cos(ang)) / cm_px - px / 2)
        H, W = canvas.shape[:2]
        sx0, sy0 = max(0, -x0), max(0, -y0)
        ex, ey = min(px, W - x0), min(px, H - y0)
        if ex <= sx0 or ey <= sy0:
            continue
        sub = stack[sy0:ey, sx0:ex]
        dst = canvas[y0 + sy0:y0 + ey, x0 + sx0:x0 + ex]
        mask = sub[..., 0] * fade > threshold
        dst[mask] = sub[..., 1:][mask]
    sheet = np.concatenate([bg, canvas], 1)
    Image.fromarray((np.clip(sheet, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, 'preview_drift.png'))
    cell_view = np.concatenate([np.concatenate([c[0] for c in cells[:2]], 1), np.concatenate([c[0] for c in cells[2:]], 1)], 0)
    Image.fromarray(((cell_view > threshold) * 200 + cell_view * 55).astype(np.uint8)).resize((1024, 1024)).save(os.path.join(OUT, 'preview_cells.png'))
    print('preview written')


if __name__ == '__main__':
    atlas, cells = build()
    stats(cells, 0.30)
    if '--preview' in sys.argv:
        preview(cells)
