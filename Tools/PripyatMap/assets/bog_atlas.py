"""Paints T_Prypyat_BogFoliage.png (2048 RGBA): the one atlas of the Pripyat bog foliage kit.

Regions (UV, v up as in Blender):
  A u 0.0-0.5 v 0.5-1.0  reed card: dead straw Phragmites stems, blades and grey-brown plumes (alpha)
  B u 0.5-1.0 v 0.5-1.0  cattail card: strap leaves, a few brown heads (alpha)
  C u 0.0-0.5 v 0.0-0.5  sedge card: dense arching olive/straw blades (alpha)
  D u 0.5-1.0 v 0.25-0.5 opaque straw/olive thatch (tussock domes, clump bases)
  E u 0.5-1.0 v 0.0-0.25 opaque brown (cattail heads)
Blades are drawn thick and dense on purpose: masked cards seen from 25-40 m use small mips, where thin blades vanish.

python bog_atlas.py <out.png>
"""
import math
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

S = 2048
H = S // 2
rng = random.Random(7)


def col(base, var=0.08):
    # Brightness variation plus a small hue jitter: independent per-channel noise turned straw pink.
    k = 1.0 + rng.uniform(-var, var) * 2.0
    return tuple(int(255 * max(0.0, min(1.0, c * k + rng.uniform(-0.015, 0.015)))) for c in base) + (255,)


def blade(draw, x0, y0, x1, y1, w0, w1, colour, bend=0.0, steps=8):
    """Tapered curved blade from (x0,y0) (base, width w0) to (x1,y1) (tip, width w1), in pixel space (y down)."""
    left, right = [], []
    for i in range(steps + 1):
        t = i / steps
        x = x0 + (x1 - x0) * t + bend * math.sin(t * math.pi * 0.5) * t
        y = y0 + (y1 - y0) * t
        w = (w0 + (w1 - w0) * t) / 2
        dx, dy = (x1 - x0), (y1 - y0)
        n = math.hypot(dx, dy) or 1
        nx, ny = -dy / n, dx / n
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    draw.polygon(left + right[::-1], fill=colour)


def region_reed(img):
    d = ImageDraw.Draw(img)
    ox, oy = 0, 0                       # top-left quadrant in image space (v 0.5-1)
    for _ in range(70):
        x = ox + rng.uniform(40, H - 40)
        top = oy + rng.uniform(20, 260)
        lean = rng.uniform(-90, 90)
        c = col((0.74, 0.64, 0.40), 0.09)
        blade(d, x, oy + H, x + lean, top, rng.uniform(10, 16), rng.uniform(5, 8), c, bend=rng.uniform(-30, 30))
        for _ in range(rng.randint(1, 3)):     # leaves off the stem
            yy = rng.uniform(top + 150, oy + H - 80)
            xx = x + lean * (oy + H - yy) / (oy + H - top)
            blade(d, xx, yy, xx + rng.choice((-1, 1)) * rng.uniform(80, 180), yy - rng.uniform(40, 160),
                  rng.uniform(12, 18), 3, col((0.70, 0.60, 0.36), 0.08), bend=rng.uniform(-20, 20))
        # plume: dense grey-brown ellipse cluster at the tip
        px, py = x + lean, top
        pc = col((0.52, 0.45, 0.40), 0.07)
        for _ in range(9):
            ex, ey = px + rng.uniform(-18, 18), py + rng.uniform(0, 110)
            d.ellipse([ex - rng.uniform(12, 22), ey - rng.uniform(30, 55), ex + rng.uniform(12, 22), ey + rng.uniform(30, 55)],
                      fill=pc)


def region_cattail(img):
    d = ImageDraw.Draw(img)
    ox, oy = H, 0
    for _ in range(46):
        x = ox + rng.uniform(80, H - 80)
        tipx = x + rng.uniform(-260, 260)
        tipy = oy + rng.uniform(30, 380)
        c = col((0.50, 0.48, 0.28), 0.08) if rng.random() < 0.6 else col((0.68, 0.60, 0.38), 0.08)
        blade(d, x, oy + H, tipx, tipy, rng.uniform(20, 30), 4, c, bend=rng.uniform(-60, 60))
    for _ in range(9):                  # brown heads on stalks
        x = ox + rng.uniform(120, H - 120)
        top = oy + rng.uniform(80, 300)
        blade(d, x, oy + H, x + rng.uniform(-40, 40), top, 9, 6, col((0.48, 0.44, 0.26), 0.05))
        hx = x
        d.rounded_rectangle([hx - 20, top + 30, hx + 20, top + 190], radius=18, fill=col((0.30, 0.18, 0.09), 0.04))


def region_sedge(img):
    d = ImageDraw.Draw(img)
    ox, oy = 0, H
    for _ in range(230):
        x = ox + H / 2 + rng.gauss(0, H * 0.18)
        ang = rng.uniform(-1.25, 1.25)
        length = rng.uniform(420, 900)
        tipx = x + math.sin(ang) * length
        tipy = oy + H - math.cos(ang) * length * rng.uniform(0.55, 1.0)
        c = col((0.46, 0.47, 0.27), 0.08) if rng.random() < 0.55 else col((0.72, 0.64, 0.42), 0.08)
        blade(d, x, oy + H, tipx, tipy, rng.uniform(9, 15), 3, c, bend=math.sin(ang) * rng.uniform(40, 160))


def region_opaque(arr):
    yy, xx = np.mgrid[0:H // 2, 0:H].astype(np.float32)
    noise = np.zeros_like(xx)
    for f, a in ((0.02, 0.5), (0.06, 0.3), (0.17, 0.2)):
        noise += a * (np.sin(xx * f + rng.uniform(0, 6)) * np.cos(yy * f * 1.7 + rng.uniform(0, 6)))
    streak = np.sin(xx * 0.9 + yy * 0.15) * 0.5 + 0.5
    t = np.clip(0.5 + 0.35 * noise, 0, 1)
    straw = np.array([0.66, 0.58, 0.37]) * (0.8 + 0.25 * streak[..., None])
    olive = np.array([0.42, 0.43, 0.25]) * (0.8 + 0.25 * streak[..., None])
    thatch = straw * t[..., None] + olive * (1 - t[..., None])
    brown = np.array([0.30, 0.18, 0.09]) * (0.85 + 0.3 * t[..., None])
    rgb_d = np.clip(thatch, 0, 1)
    rgb_e = np.clip(brown, 0, 1)
    # image rows: v 0.25-0.5 is rows H .. H + H/2; v 0-0.25 is rows H + H/2 .. S
    arr[H:H + H // 2, H:S, :3] = (rgb_d * 255).astype(np.uint8)
    arr[H + H // 2:S, H:S, :3] = (rgb_e * 255).astype(np.uint8)
    arr[H:S, H:S, 3] = 255


def main(out):
    img = Image.new('RGBA', (S, S), (110, 100, 70, 0))
    region_reed(img)
    region_cattail(img)
    region_sedge(img)
    # Bleed colour under transparent pixels (mips average colour with alpha 0 neighbours).
    arr = np.array(img)
    alpha = arr[..., 3:4].astype(np.float32) / 255.0
    blur = np.array(img.filter(ImageFilter.GaussianBlur(12))).astype(np.float32)
    ablur = blur[..., 3:4] / 255.0
    bleed = np.where(ablur > 0.01, blur[..., :3] / np.maximum(ablur, 0.01), np.array([120, 105, 70], np.float32))
    arr[..., :3] = (arr[..., :3] * alpha + np.clip(bleed, 0, 255) * (1 - alpha)).astype(np.uint8)
    region_opaque(arr)
    Image.fromarray(arr, 'RGBA').save(out)
    cover = (arr[:H, :H, 3] > 127).mean(), (arr[:H, H:, 3] > 127).mean(), (arr[H:, :H, 3] > 127).mean()
    print('atlas', out, 'coverage reed %.2f cattail %.2f sedge %.2f' % cover)


if __name__ == '__main__':
    main(sys.argv[1])
