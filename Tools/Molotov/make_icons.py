"""Composes T_AB_Molotov / T_AB_CancelMolotov in the style of the existing red-plate grenade icons.

Usage: python make_icons.py <refs_dir with exported T_AB_*.png> <icon_render.png> <out_dir>
The blank hex plate is the per-pixel median of the red-plate icons; the cancel X is lifted from
T_AB_CancelSmallGrenade (pixels that differ from T_AB_SmallGrenade and are saturated red).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REFS, RENDER, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
RED_PLATE_ICONS = ["T_AB_BundleGrenade", "T_AB_HEATGrenade", "T_AB_SmallGrenade", "T_AB_StickGrenade",
                   "T_AB_FireRockets", "T_AB_FireRockets_Small"]


def load(name):
    return np.asarray(Image.open(os.path.join(REFS, name + ".png")).convert("RGBA")).astype(np.float32)


stack = np.stack([load(n) for n in RED_PLATE_ICONS])
size = stack.shape[1]


def hex_distance_map(n):
    # Pointy left/right hexagon: distance 1 on the outline, rings are concentric hexagons.
    c = (n - 1) / 2.0
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    x, y = np.abs(xx - c) / c, np.abs(yy - c) / c
    return np.maximum(y / (np.sqrt(3) / 2), x + y / np.sqrt(3))


# The plate is concentric hex rings plus a radial gradient, so a per-ring median over every red-plate icon
# rebuilds it without any of the objects drawn on top.
HEX_BINS = 400
hex_dist = hex_distance_map(size)
bins = np.clip((hex_dist / 1.25 * HEX_BINS).astype(int), 0, HEX_BINS - 1)
profile = np.zeros((HEX_BINS, 4), np.float32)
flat = stack.reshape(len(RED_PLATE_ICONS), -1, 4)
for b in range(HEX_BINS):
    sel = (bins.ravel() == b)
    if sel.any():
        profile[b] = np.median(flat[:, sel, :].reshape(-1, 4), axis=0)
# Objects cover the centre of most icons; inside the inner border use a smooth gradient from the clean ring.
CLEAN_RING = 0.70
clean_bin = int(CLEAN_RING / 1.25 * HEX_BINS)
for b in range(clean_bin):
    centre_weight = 1.0 - b / clean_bin
    profile[b] = profile[clean_bin]
    profile[b, :3] = np.minimum(255.0, profile[clean_bin, :3] * (1.0 + 0.18 * centre_weight))
plate = profile[bins]

# Cancel X mask.
cancel = load("T_AB_CancelSmallGrenade")
plain = load("T_AB_SmallGrenade")
diff = np.abs(cancel[..., :3] - plain[..., :3]).sum(-1)
is_red = (cancel[..., 0] > 170) & (cancel[..., 1] < 90) & (cancel[..., 2] < 90)
x_mask = ((diff > 60) & is_red).astype(np.float32)
x_mask = np.asarray(Image.fromarray((x_mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
                    .filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32) / 255.0

# Object render, cropped to its alpha and fitted in the hex like the stick grenade.
render = Image.open(RENDER).convert("RGBA")
render = render.crop(render.getbbox())
fit = int(size * 0.58)
scale = fit / max(render.size)
render = render.resize((max(1, int(render.width * scale)), max(1, int(render.height * scale))), Image.LANCZOS)

# Painted flame on the rag tip: three blurred tongues per heat layer, hot core at the base.
FLAME_TONGUES = ((0.0, 1.0, 0.0), (-0.22, 0.62, -0.10), (0.20, 0.70, 0.12))


def flame_layer(w, h):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for color, shrink, blur in (((190, 35, 5, 190), 1.0, 3.0), ((255, 110, 10, 225), 0.75, 2.2),
                                ((255, 200, 70, 245), 0.48, 1.6), ((255, 245, 200, 255), 0.24, 1.0)):
        part = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(part)
        base = h * 0.90
        for offset, height, lean in FLAME_TONGUES:
            cx = w * (0.5 + offset * shrink)
            rw = w * 0.20 * shrink * (0.7 + 0.3 * height)
            rh = h * 0.82 * shrink * height
            top = (cx + w * lean * shrink, base - rh)
            mid_l = (cx - rw * 0.75 + w * lean * 0.4 * shrink, base - rh * 0.45)
            mid_r = (cx + rw * 0.85 + w * lean * 0.5 * shrink, base - rh * 0.40)
            bowl = [(cx + rw * np.cos(a), base + rw * 0.5 * np.sin(a)) for a in np.linspace(0, np.pi, 14)]
            d.polygon([(cx - rw, base), mid_l, top, mid_r, (cx + rw, base)] + bowl, fill=color)
        layer.alpha_composite(part.filter(ImageFilter.GaussianBlur(blur)))
    return layer


icon = Image.fromarray(plate.astype(np.uint8), "RGBA")
ox = (size - render.width) // 2 + 4
oy = (size - render.height) // 2 + 18
icon.alpha_composite(render, (ox, oy))

# The rag tip is the top-left-most opaque region of the tilted render.
alpha = np.asarray(render)[..., 3]
ys, xs = np.nonzero(alpha > 128)
tip_idx = np.argmin(ys + xs * 0.35)
tip = (ox + xs[tip_idx], oy + ys[tip_idx])
fw, fh = int(size * 0.30), int(size * 0.42)
flame = flame_layer(fw, fh)
icon.alpha_composite(flame, (int(tip[0] - fw * 0.42), int(tip[1] - fh * 0.80)))

# Keep everything inside the hex plate's alpha.
plate_alpha = plate[..., 3:4] / 255.0
arr = np.asarray(icon).astype(np.float32)
arr[..., 3:4] = np.minimum(arr[..., 3:4], plate[..., 3:4])
icon = Image.fromarray(arr.astype(np.uint8), "RGBA")
os.makedirs(OUT, exist_ok=True)
icon.save(os.path.join(OUT, "T_AB_Molotov.png"))

arr = np.asarray(icon).astype(np.float32)
x_col = cancel[..., :3]
arr[..., :3] = arr[..., :3] * (1 - x_mask[..., None]) + x_col * x_mask[..., None]
Image.fromarray(arr.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_AB_CancelMolotov.png"))
Image.fromarray(plate.astype(np.uint8), "RGBA").save(os.path.join(OUT, "plate.png"))
print("icons written", OUT)
