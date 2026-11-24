"""Painted-steel atlas for the Soviet wall gates (system python, numpy + Pillow).

python make_gate_atlas.py <out.png>

The gates' steel slot uses the wall concrete master (M_RusBunker_Concrete) with this texture as ConcreteDiffuse, so
the gates need only the one master pair (plus its _WallHoles twin) for VAT and holes. Regions are in Blender UV space
(v = 0 at the bottom); build_gates.py maps faces into them by name (REGIONS must match).
"""
import math
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SIZE = 2048
REGIONS = {  # name: (u0, v0, u1, v1)
    'plate': (0.0, 0.5, 0.5, 1.0),       # green painted plate, panel seams, rust runs
    'roundel': (0.5, 0.75, 0.75, 1.0),   # white disc, red star, dark rim
    'dark': (0.75, 0.75, 1.0, 1.0),      # bare dark steel, oily
    'tube': (0.5, 0.5, 0.75, 0.75),      # green paint, heavy edge wear (frames, rails)
    'lamp': (0.75, 0.625, 0.875, 0.75),  # warm glass
    'rust': (0.875, 0.625, 1.0, 0.75),   # rusty steel (counterweight sliders, chains)
    'goldroundel': (0.75, 0.5, 1.0, 0.625),  # unused spare strip
}
rng = np.random.default_rng(7)


def noise(h, w, scale, octaves=4):
    """Value noise in 0..1 from blurred random layers."""
    acc = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        s = max(2, int(scale * (2 ** o)))
        small = rng.random((max(2, h * s // w), s)).astype(np.float32)
        img = Image.fromarray((small * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
        acc += amp * np.asarray(img, np.float32) / 255.0
        total += amp
        amp *= 0.5
    return acc / total


def px_rect(name):
    u0, v0, u1, v1 = REGIONS[name]
    return int(u0 * SIZE), int((1 - v1) * SIZE), int(u1 * SIZE), int((1 - v0) * SIZE)


def paint(base_rgb, h, w, grime=0.25, wear=0.0, scale=6):
    n = noise(h, w, scale)
    fine = noise(h, w, scale * 8, 2)
    col = np.array(base_rgb, np.float32)[None, None, :] * (0.82 + 0.3 * n[..., None]) * (0.92 + 0.12 * fine[..., None])
    # vertical grime / rust runs
    runs = rng.random((max(2, h // 64), max(2, w // 3))).astype(np.float32)
    runs = np.asarray(Image.fromarray((runs * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
                      .filter(ImageFilter.GaussianBlur(2)), np.float32) / 255
    streak = np.clip((runs - 0.55) * 3.0, 0, 1) * grime
    rust = np.array([0.42, 0.25, 0.13], np.float32)
    col = col * (1 - streak[..., None]) + rust * streak[..., None]
    if wear:
        chips = np.clip((noise(h, w, scale * 4, 3) - (1 - wear)) * 6, 0, 1)
        steel = np.array([0.45, 0.44, 0.41], np.float32)
        col = col * (1 - chips[..., None]) + steel * chips[..., None]
    return np.clip(col, 0, 1)


def put(atlas, name, arr):
    x0, y0, x1, y1 = px_rect(name)
    atlas[y0:y1, x0:x1] = arr[: y1 - y0, : x1 - x0]


def star_polygon(cx, cy, r, r_in, rot=-90):
    pts = []
    for i in range(10):
        a = math.radians(rot + i * 36)
        rr = r if i % 2 == 0 else r_in
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return pts


def main():
    out = sys.argv[1]
    atlas = np.zeros((SIZE, SIZE, 3), np.float32) + 0.35
    green = (0.27, 0.32, 0.21)  # sRGB Soviet olive green

    # plate: green, faint horizontal panel seams every quarter, weld beads, rust runs
    x0, y0, x1, y1 = px_rect('plate')
    h, w = y1 - y0, x1 - x0
    plate = paint(green, h, w, grime=0.45, wear=0.08)
    for k in range(1, 4):
        y = int(h * k / 4)
        plate[y - 3:y + 3] *= 0.55
        plate[y + 3:y + 6] *= 1.12
    plate[:, :10] *= 0.6
    plate[:, -10:] *= 0.6
    put(atlas, 'plate', plate)

    # tube: green with heavy wear on the edges of the region (faces are stretched onto it)
    x0, y0, x1, y1 = px_rect('tube')
    h, w = y1 - y0, x1 - x0
    tube = paint(green, h, w, grime=0.3, wear=0.25, scale=4)
    put(atlas, 'tube', tube)

    # dark bare steel
    x0, y0, x1, y1 = px_rect('dark')
    h, w = y1 - y0, x1 - x0
    put(atlas, 'dark', paint((0.22, 0.22, 0.21), h, w, grime=0.35, scale=5))

    # rust
    x0, y0, x1, y1 = px_rect('rust')
    h, w = y1 - y0, x1 - x0
    put(atlas, 'rust', paint((0.45, 0.27, 0.14), h, w, grime=0.2, wear=0.2, scale=5))

    # lamp glass: warm, slightly dirty
    x0, y0, x1, y1 = px_rect('lamp')
    h, w = y1 - y0, x1 - x0
    put(atlas, 'lamp', paint((0.95, 0.80, 0.45), h, w, grime=0.15, scale=3))

    # spare strip: plain green
    x0, y0, x1, y1 = px_rect('goldroundel')
    h, w = y1 - y0, x1 - x0
    put(atlas, 'goldroundel', paint(green, h, w, grime=0.2, scale=4))

    # roundel: weathered white disc, dark rim, red star; corners green (the disc mesh is round)
    x0, y0, x1, y1 = px_rect('roundel')
    h, w = y1 - y0, x1 - x0
    base = paint((0.86, 0.84, 0.78), h, w, grime=0.25, wear=0.05, scale=4)
    red = paint((0.66, 0.10, 0.07), h, w, grime=0.15, wear=0.06, scale=4)
    rim = paint((0.27, 0.32, 0.21), h, w, grime=0.2, wear=0.1, scale=4)
    mask_star = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(mask_star)
    d.polygon(star_polygon(w / 2, h / 2, w * 0.40, w * 0.40 * 0.42), fill=255)
    mask_star = np.asarray(mask_star.filter(ImageFilter.GaussianBlur(1.2)), np.float32)[..., None] / 255
    yy, xx = np.mgrid[0:h, 0:w]
    rr = np.hypot(xx - w / 2 + 0.5, yy - h / 2 + 0.5) / (w / 2)
    mask_rim = np.clip((rr - 0.90) * 40, 0, 1)[..., None]
    rnd = base * (1 - mask_star) + red * mask_star
    rnd = rnd * (1 - mask_rim) + rim * mask_rim
    put(atlas, 'roundel', rnd)

    Image.fromarray((np.clip(atlas, 0, 1) * 255).astype(np.uint8)).save(out)
    print('wrote', out)


if __name__ == '__main__':
    main()
