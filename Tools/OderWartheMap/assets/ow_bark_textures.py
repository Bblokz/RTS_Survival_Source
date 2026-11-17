"""Tileable PBR detail maps for the Oder-Warthe dead pines (plain python: numpy + Pillow, no Blender).

    python ow_bark_textures.py [--preview]
Writes to D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Textures/:
  T_OW_BarkPlates_N.png   tangent normal (DirectX / UE convention, G = -dh/drow), Scots pine plate bark
  T_OW_BarkPlates_M.png   linear mask: R roughness, G ambient occlusion, B height, A lichen/patch noise
  T_OW_DeadWood_N.png     tangent normal, silver dead wood: long grain, checking cracks, knots
  T_OW_DeadWood_M.png     linear mask, same packing
All maps are 1024^2 and tile in both axes (noise built in the frequency domain, Worley cells with wrapped
distances). Image row 0 = top; in UE +V runs down the image and up the trunk (MI_OW_PineBark strip UVs), plates
and grain are elongated along V. The material samples them at UV0 * (DetailU, DetailV); see M_OW_PineBark.
Own RNG, so T_OW_PineBark_D (ow_textures.py) is not touched.
"""
import os
import sys

import numpy as np
from PIL import Image

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Textures/'
N = 1024
R = np.random.default_rng(4512)


def noise(beta=2.0, sx=1.0, sy=1.0, n=N):
    """Tileable fractal noise 0..1; sy > 1 elongates features along y (rows = V), sx > 1 along x."""
    white = R.standard_normal((n, n))
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.fftfreq(n)[None, :] * n
    k = np.sqrt((fx * sx) ** 2 + (fy * sy) ** 2)
    k[0, 0] = 1.0
    filt = 1.0 / np.power(k, beta)
    filt[0, 0] = 0.0
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * filt))
    out -= out.min()
    return out / max(out.max(), 1e-9)


def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def warp(field, dx, dy):
    """Sample field at (row + dy, col + dx) with wrap (bilinear)."""
    n = field.shape[0]
    rows, cols = np.mgrid[0:n, 0:n].astype(np.float64)
    y = (rows + dy) % n
    x = (cols + dx) % n
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    fy, fx = y - y0, x - x0
    y1, x1 = (y0 + 1) % n, (x0 + 1) % n
    return (field[y0, x0] * (1 - fx) * (1 - fy) + field[y0, x1] * fx * (1 - fy) +
            field[y1, x0] * (1 - fx) * fy + field[y1, x1] * fx * fy)


def worley(cx, cy, jitter=0.9, warp_px=0.0, n=N):
    """Tileable Worley on a cx x cy jittered grid (cells taller than wide when cy < cx). Returns F1, F2 (in px of the
    narrow axis), the cell id and a per-cell random 0..1."""
    cw, ch = n / cx, n / cy
    px = (np.arange(cx)[None, :] + 0.5 + jitter * (R.random((cy, cx)) - 0.5)) * cw
    py = (np.arange(cy)[:, None] + 0.5 + jitter * (R.random((cy, cx)) - 0.5)) * ch
    rnd = R.random((cy, cx))
    rows, cols = np.mgrid[0:n, 0:n].astype(np.float64)
    if warp_px:
        rows = rows + warp_px * (noise(2.2) - 0.5) * 2
        cols = cols + warp_px * (noise(2.2) - 0.5) * 2
    ci = np.floor(cols / cw).astype(int)
    cj = np.floor(rows / ch).astype(int)
    f1 = np.full((n, n), 1e9)
    f2 = np.full((n, n), 1e9)
    cid = np.zeros((n, n), dtype=int)
    aspect = ch / cw   # measure distance in "cell widths" so tall cells stay tall
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            j = cj + dj
            i = ci + di
            jw, iw = j % cy, i % cx
            fx = px[jw, iw] + (i - iw) * cw
            fy = py[jw, iw] + (j - jw) * ch
            d = np.sqrt((cols - fx) ** 2 + ((rows - fy) / aspect) ** 2)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            cid = np.where(closer, jw * cx + iw, cid)
            f1 = np.where(closer, d, f1)
    return f1, f2, cid, rnd.ravel()[cid]


def normal_from_height(h, depth_px):
    """DirectX tangent normal from a 0..1 height field; depth_px = height range in pixels."""
    hp = h * depth_px
    dx = (np.roll(hp, -1, 1) - np.roll(hp, 1, 1)) * 0.5
    dy = (np.roll(hp, -1, 0) - np.roll(hp, 1, 0)) * 0.5   # rows top -> bottom
    # UE (DirectX): green = +Y = image DOWN. A slope rising downwards (dy > 0) faces up the image -> green < 0.
    nx, ny, nz = -dx, -dy, np.ones_like(hp)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / ln, ny / ln, nz / ln], -1)


def ao_from_height(h, radius=(4, 10, 24)):
    """Cheap cavity/AO: how far below the local average height a pixel sits (tileable box blurs)."""
    ao = np.ones_like(h)
    for r in radius:
        k = np.fft.fftfreq(h.shape[0])
        # gaussian blur in the frequency domain keeps it tileable
        g = np.exp(-0.5 * ((k[:, None] * 2 * np.pi * r) ** 2 + (k[None, :] * 2 * np.pi * r) ** 2))
        blur = np.real(np.fft.ifft2(np.fft.fft2(h) * g))
        ao *= 1.0 - np.clip((blur - h) * 3.0, 0, 0.6)
    return np.clip(ao, 0, 1)


def save_rgb(name, rgb):
    img = (np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(img, 'RGB' if img.shape[-1] == 3 else 'RGBA').save(OUT + name + '.png')
    print('saved', name, img.shape, 'mean', img.reshape(-1, img.shape[-1]).mean(0).round(1))


def save_normal(name, n):
    save_rgb(name, n * 0.5 + 0.5)


def bark_plates():
    # Tile ~1 m around x 1 m up the trunk at the base (UV0 * (1, 21)). Scots pine: thick plates 6-12 cm wide,
    # 15-35 cm tall, split by deep vertical fissures with cross breaks; each plate is built of thin flaky layers.
    f1, f2, cid, crnd = worley(11, 4, jitter=0.85, warp_px=28.0)
    edge = f2 - f1                                    # 0 on the fissure line
    fiss = 1.0 - ss(0.0, 26.0, edge)                  # wide soft fissure walls
    fiss_core = 1.0 - ss(0.0, 7.0, edge)
    plate_tilt = (crnd - 0.5) * 0.25                  # each plate sits at its own height
    layers = noise(1.4, sx=3.5, sy=1.0)               # flaky layers elongated across the trunk
    flakes = ss(0.45, 0.75, noise(1.1, sx=2.0, sy=1.0))
    fine = noise(0.9)
    h = 0.62 + plate_tilt
    h += 0.16 * (layers - 0.5) + 0.08 * flakes + 0.04 * (fine - 0.5)
    # small sub-plates: a finer Worley inside the big plates, shallow cracks
    s1, s2, _, srnd = worley(26, 11, jitter=0.9, warp_px=6.0)
    sub = 1.0 - ss(0.0, 5.0, s2 - s1)
    h -= 0.07 * sub * ss(0.35, 0.8, srnd)
    h -= 0.55 * fiss + 0.25 * fiss_core
    h = np.clip(h, 0, 1)
    h = (h - h.min()) / (h.max() - h.min())
    n = normal_from_height(h, depth_px=22.0)
    ao = ao_from_height(h)
    # roughness: dusty matte plate tops, rougher fibrous fissures, a few shinier worn plate faces
    rough = 0.80 + 0.12 * fiss + 0.06 * (fine - 0.5) - 0.12 * ss(0.75, 0.95, crnd) * (1 - fiss)
    rough -= 0.06 * flakes * (1 - fiss)
    lichen = ss(0.55, 0.72, noise(2.0) * 0.7 + noise(1.0) * 0.3)   # patch mask, the material decides where
    save_normal('T_OW_BarkPlates_N', n)
    save_rgb('T_OW_BarkPlates_M', np.stack([np.clip(rough, 0, 1), ao, h, lichen], -1))
    return h


def dead_wood():
    # Tile ~1 m along the branch (V) x one wrap around it (U). Silver weathered wood: deep grain running along V,
    # checking cracks along the grain, a few knots, fibrous end splits.
    grain = noise(1.3, sx=1.0, sy=14.0)              # long along V (the branch)
    grain2 = noise(0.8, sx=1.0, sy=6.0)
    ridges = 0.5 + 0.5 * np.sin(2 * np.pi * (np.arange(N)[None, :] / N * 48 + 3.0 * grain))
    h = 0.55 + 0.22 * (grain - 0.5) + 0.12 * (ridges - 0.5) + 0.08 * (grain2 - 0.5)
    # checking cracks: thin, long, along the grain
    crack_field = noise(1.6, sx=1.0, sy=22.0)
    cracks = (1.0 - ss(0.0, 0.012, np.abs(crack_field - 0.5))) * ss(0.45, 0.7, noise(2.2, sx=1.0, sy=1.0))
    h -= 0.45 * cracks
    # knots: dark raised rings
    rows, cols = np.mgrid[0:N, 0:N].astype(np.float64)
    knot = np.zeros((N, N))
    for _ in range(5):
        kx, ky, kr = R.uniform(0, N), R.uniform(0, N), R.uniform(10, 26)
        dxw = (cols - kx + N / 2) % N - N / 2
        dyw = (rows - ky + N / 2) % N - N / 2
        d = np.sqrt((dxw * 1.0) ** 2 + (dyw * 0.55) ** 2) / kr
        knot = np.maximum(knot, np.exp(-d * d * 2.0))
    h += 0.18 * knot - 0.25 * ss(0.85, 1.0, knot)
    h = np.clip(h, 0, 1)
    h = (h - h.min()) / (h.max() - h.min())
    n = normal_from_height(h, depth_px=14.0)
    ao = ao_from_height(h, radius=(3, 8))
    rough = 0.74 + 0.14 * cracks + 0.08 * (1 - ridges) + 0.05 * (noise(1.0) - 0.5)
    lichen = ss(0.58, 0.74, noise(2.0) * 0.7 + noise(1.0) * 0.3)
    save_normal('T_OW_DeadWood_N', n)
    save_rgb('T_OW_DeadWood_M', np.stack([np.clip(rough, 0, 1), ao, h, lichen], -1))
    return h


def preview(hp, hw):
    """Lit preview of the height fields (sun from the upper left), 2x2 tiled to prove the wrap."""
    out = []
    for h, d in ((hp, 22.0), (hw, 14.0)):
        n = normal_from_height(h, d)
        light = np.array([-0.5, 0.5, 0.7])
        light /= np.linalg.norm(light)
        # DirectX green points down the image: flip it to light in image space (x right, y up)
        lit = np.clip(n[..., 0] * light[0] - n[..., 1] * light[1] + n[..., 2] * light[2], 0, 1)
        out.append(np.tile(lit, (2, 2)))
    img = np.concatenate(out, 1)
    Image.fromarray((img * 255).astype(np.uint8), 'L').resize((1024, 512)).save(OUT + '_bark_preview.png')
    print('preview', OUT + '_bark_preview.png')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    hp = bark_plates()
    hw = dead_wood()
    if '--preview' in sys.argv:
        preview(hp, hw)
    print('OW_BARK_TEXTURES_DONE')
