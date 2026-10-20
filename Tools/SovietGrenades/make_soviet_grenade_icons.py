"""Composes the card icons of the Soviet grenades (T_AB_<Name>Grenade / T_AB_Cancel<Name>Grenade) on the red hex plate
of the existing grenade icons, the same way Source/Tools/Molotov/make_icons.py made the molotov icons.

Usage: python make_soviet_grenade_icons.py <refs_dir with exported T_AB_*.png> <icon_render_dir> <out_dir>
<icon_render_dir> holds <Name>_icon_render.png from
D:/Blender/BlenderProjects/BlenderTanks/Infantry/Infweapons/SovietGrenades/render_icons.py.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

REFS, RENDERS, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
RED_PLATE_ICONS = ["T_AB_BundleGrenade", "T_AB_HEATGrenade", "T_AB_SmallGrenade", "T_AB_StickGrenade",
                   "T_AB_FireRockets", "T_AB_FireRockets_Small"]
# Render name -> (icon name, fit of the longest side in the hex, mirror so stick grenades point up-right).
GRENADES = {
    "RGD33": ("RGD33Grenade", 0.70, True),
    "F1": ("F1Grenade", 0.60, False),
    "RG42": ("RG42Grenade", 0.60, False),
    "RPG43": ("RPG43Grenade", 0.70, True),
}


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


# Blank plate = per hex-ring median over the red-plate icons (see make_icons.py).
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

os.makedirs(OUT, exist_ok=True)
for render_name, (icon_name, fit, mirror) in GRENADES.items():
    render = Image.open(os.path.join(RENDERS, render_name + "_icon_render.png")).convert("RGBA")
    render = render.crop(render.getbbox())
    if mirror:
        render = render.transpose(Image.FLIP_LEFT_RIGHT)
    # The studio light greys the olive paint; push its colour back so it reads against the red plate.
    rgb = ImageEnhance.Brightness(render.convert("RGB")).enhance(0.9)
    rgb = ImageEnhance.Color(rgb).enhance(2.0)
    rgb = ImageEnhance.Contrast(rgb).enhance(1.25)
    render = Image.merge("RGBA", (*rgb.split(), render.getchannel("A")))
    scale = int(size * fit) / max(render.size)
    render = render.resize((max(1, int(render.width * scale)), max(1, int(render.height * scale))), Image.LANCZOS)

    icon = Image.fromarray(plate.astype(np.uint8), "RGBA")
    icon.alpha_composite(render, ((size - render.width) // 2, (size - render.height) // 2))
    arr = np.asarray(icon).astype(np.float32)
    arr[..., 3:4] = np.minimum(arr[..., 3:4], plate[..., 3:4])
    Image.fromarray(arr.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_AB_" + icon_name + ".png"))
    arr[..., :3] = arr[..., :3] * (1 - x_mask[..., None]) + cancel[..., :3] * x_mask[..., None]
    Image.fromarray(arr.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_AB_Cancel" + icon_name + ".png"))
    print("icons", icon_name)
