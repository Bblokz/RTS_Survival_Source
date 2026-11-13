"""Composes the Vultures' AT stick grenade card icons (T_AB_ATStickGrenade / T_AB_CancelATStickGrenade) on the red hex
plate of the existing grenade icons, like Source/Tools/SovietGrenades/make_soviet_grenade_icons.py.

Usage: python make_vultures_at_icons.py <refs_dir with exported T_AB_*.png> <render png> <out_dir>
The render comes from D:/Blender/BlenderProjects/BlenderTanks/Infantry/Infweapons/VulturesATStick/render_at_stick_icon.py
(M24 stick grenade stuck on a riveted armour plate). Refs: Saved/MolotovWork/refs.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

REFS, RENDER, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
RED_PLATE_ICONS = ["T_AB_BundleGrenade", "T_AB_HEATGrenade", "T_AB_SmallGrenade", "T_AB_StickGrenade"]
ICON_NAME = "ATStickGrenade"
# Longest side of the render inside the hex.
FIT = 0.74


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


# Blank plate = per hex-ring median over the red-plate icons (the grenade differs per icon, the plate does not).
HEX_BINS = 400
bins = np.clip((hex_distance_map(size) / 1.25 * HEX_BINS).astype(int), 0, HEX_BINS - 1)
profile = np.zeros((HEX_BINS, 4), np.float32)
flat = stack.reshape(len(RED_PLATE_ICONS), -1, 4)
for b in range(HEX_BINS):
    sel = bins.ravel() == b
    if sel.any():
        profile[b] = np.median(flat[:, sel, :].reshape(-1, 4), axis=0)
CLEAN_RING = 0.70
clean_bin = int(CLEAN_RING / 1.25 * HEX_BINS)
for b in range(clean_bin):
    centre_weight = 1.0 - b / clean_bin
    profile[b] = profile[clean_bin]
    profile[b, :3] = np.minimum(255.0, profile[clean_bin, :3] * (1.0 + 0.18 * centre_weight))
plate = profile[bins]

# Cancel X mask lifted from the small grenade pair.
cancel = load("T_AB_CancelSmallGrenade")
plain = load("T_AB_SmallGrenade")
diff = np.abs(cancel[..., :3] - plain[..., :3]).sum(-1)
is_red = (cancel[..., 0] > 170) & (cancel[..., 1] < 90) & (cancel[..., 2] < 90)
x_mask = ((diff > 60) & is_red).astype(np.float32)
x_mask = np.asarray(Image.fromarray((x_mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
                    .filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32) / 255.0

render = Image.open(RENDER).convert("RGBA")
render = render.crop(render.getbbox())
rgb = ImageEnhance.Color(render.convert("RGB")).enhance(1.4)
rgb = ImageEnhance.Contrast(rgb).enhance(1.2)
render = Image.merge("RGBA", (*rgb.split(), render.getchannel("A")))
scale = int(size * FIT) / max(render.size)
render = render.resize((max(1, int(render.width * scale)), max(1, int(render.height * scale))), Image.LANCZOS)
# Soft dark drop shadow so the dark armour plate separates from the dark-red plate centre.
shadow_alpha = render.getchannel("A").filter(ImageFilter.GaussianBlur(3)).point(lambda a: int(a * 0.55))
shadow = Image.new("RGBA", render.size, (0, 0, 0, 0))
shadow.putalpha(shadow_alpha)

os.makedirs(OUT, exist_ok=True)
icon = Image.fromarray(plate.astype(np.uint8), "RGBA")
origin = ((size - render.width) // 2, (size - render.height) // 2)
icon.alpha_composite(shadow, (origin[0] + 2, origin[1] + 3))
icon.alpha_composite(render, origin)
arr = np.asarray(icon).astype(np.float32)
arr[..., 3:4] = np.minimum(arr[..., 3:4], plate[..., 3:4])
Image.fromarray(arr.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_AB_" + ICON_NAME + ".png"))
arr[..., :3] = arr[..., :3] * (1 - x_mask[..., None]) + cancel[..., :3] * x_mask[..., None]
Image.fromarray(arr.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_AB_Cancel" + ICON_NAME + ".png"))
print("icons", ICON_NAME, size)
