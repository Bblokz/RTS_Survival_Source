"""Composes the command card icons of the Radixite -> Vehicle Parts converter (180x180, like the other T_AB_ icons):
  T_AB_ConvertRadixiteToVehicleParts.png         green hex plate + radixite cluster -> red arrow -> vehicle parts, on the diagonal
  T_AB_CancelConvertRadixiteToVehicleParts.png   the same with the red cancel cross

Everything is lifted from the existing card icons or the game's own meshes, so the icons match the set:
  - plate:   per hex-ring median of the green-plate icons (as Source/Tools/Molotov/make_icons.py)
  - radixite: the crystal cluster in the middle of T_AB_ReturnCargo
  - arrow:   the glossy red arrow of T_AB_Harvest, turned to point down-right
  - parts:   SM_SavObj_VehicleParts3 cut-out (render_vehicle_parts.py -> Saved/GlobalAbilityIcons/renders/RCV_VehicleParts3_*),
             relit from its normals
  - cross:   the cancel cross of T_ActionUI_StopRepair (pixels that differ from T_AB_Repair)

Usage: python compose_conversion_icons.py <refs_dir with exported T_AB_*.png> <out_dir>
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter
import cv2

REFS, OUT = sys.argv[1], sys.argv[2]
RENDERS = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/renders/"
GREEN_PLATE_ICONS = ["T_AB_DigIn", "T_AB_Harvest", "T_AB_Reinforce", "T_AB_Repair", "T_AB_ReturnCargo"]
ICON_NAME = "T_AB_ConvertRadixiteToVehicleParts"
CANCEL_NAME = "T_AB_CancelConvertRadixiteToVehicleParts"


def load(name):
    return np.asarray(Image.open(os.path.join(REFS, name + ".png")).convert("RGBA")).astype(np.float32)


stack = np.stack([load(n) for n in GREEN_PLATE_ICONS])
SIZE = stack.shape[1]


def hex_distance_map(n):
    # Pointy left/right hexagon: distance 1 on the outline, rings are concentric hexagons.
    c = (n - 1) / 2.0
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    x, y = np.abs(xx - c) / c, np.abs(yy - c) / c
    return np.maximum(y / (np.sqrt(3) / 2), x + y / np.sqrt(3))


# ---- blank green plate -------------------------------------------------------------------------------------------
# Border and glow: per hex-ring median over the green-plate icons (as Source/Tools/Molotov/make_icons.py).
HEX_BINS = 400
hex_dist = hex_distance_map(SIZE)
bins = np.clip((hex_dist / 1.25 * HEX_BINS).astype(int), 0, HEX_BINS - 1)
profile = np.zeros((HEX_BINS, 4), np.float32)
flat = stack.reshape(len(GREEN_PLATE_ICONS), -1, 4)
for b in range(HEX_BINS):
    sel = bins.ravel() == b
    if sel.any():
        profile[b] = np.median(flat[:, sel, :].reshape(-1, 4), axis=0)
plate = profile[bins]
# Fill: alpha 179 and a green that rises linearly towards the centre with the hex distance. Objects cover most of
# it, so fit that ramp on the bare fill pixels of every reference (alpha 179, almost no red) instead of the median.
FILL_ALPHA = 179.0
fill_pixels = (np.abs(stack[..., 3] - FILL_ALPHA) <= 1.5) & (stack[..., 0] < 12)
fill_dist = np.broadcast_to(hex_dist, stack.shape[:3])[fill_pixels]
fill_rgb = stack[..., :3][fill_pixels]
design = np.stack([np.ones_like(fill_dist), fill_dist], axis=1)
ramp, *_ = np.linalg.lstsq(design, fill_rgb, rcond=None)
# The fill reaches out to the inner border line; take the outermost distance where most fill pixels still are.
FILL_EDGE = float(np.percentile(fill_dist, 99.5))
inside = hex_dist <= FILL_EDGE
plate[inside, :3] = ramp[0] + hex_dist[inside, None] * ramp[1]
plate[inside, 3] = FILL_ALPHA
# The plate fill has faint warm specks; keep a few so the empty plate does not look smoother than the others.
rng = np.random.default_rng(7)
specks = (rng.random((SIZE, SIZE)) > 0.994) & inside
plate[specks, :3] = plate[specks, :3] * 0.7 + np.array([90.0, 80.0, 30.0]) * 0.3


def lift_object(icon, box, keep_seed=None, threshold=30.0, soft=110.0, drop_pure_red=False):
    """Cut an object out of an existing icon. The green plate has almost no red while the radixite (gold) and the
    arrows (red with white edges) are all red-heavy, so alpha is keyed on the red channel; the colour is un-blended
    from the plate. Limited to box (x0, y0, x1, y1) and optionally to the connected region around keep_seed."""
    alpha = np.clip((icon[..., 0] - plate[..., 0] - threshold) / (soft - threshold), 0, 1)
    if drop_pure_red:
        # The ReturnCargo arrows run past the crystals: pure red (little green) is arrow, gold radixite is not.
        pure_red = (icon[..., 1] < icon[..., 0] * 0.42) & (icon[..., 0] > 120)
        alpha *= 1 - cv2.dilate(pure_red.astype(np.uint8), np.ones((3, 3), np.uint8))
    x0, y0, x1, y1 = box
    region = np.zeros_like(alpha)
    region[y0:y1, x0:x1] = 1
    alpha *= region
    if keep_seed is not None:
        _, labels = cv2.connectedComponents((alpha > 0.2).astype(np.uint8), connectivity=8)
        keep = (labels == labels[keep_seed[1], keep_seed[0]]).astype(np.uint8)
        keep = cv2.dilate(keep, np.ones((5, 5), np.uint8))
        alpha *= keep
    a = alpha[..., None]
    rgb = np.where(a > 0.02, (icon[..., :3] - plate[..., :3] * (1 - a)) / np.maximum(a, 0.02), 0)
    out = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8)
    img = Image.fromarray(out, "RGBA")
    return img.crop(img.getbbox())


# ---- radixite cluster (T_AB_ReturnCargo) and arrow (T_AB_Harvest) ---------------------------------------------------
return_cargo = load("T_AB_ReturnCargo")
crystals = lift_object(return_cargo, (40, 50, 134, 122), keep_seed=(90, 85), drop_pure_red=True)
harvest = load("T_AB_Harvest")
arrow = lift_object(harvest, (50, 14, 130, 82), keep_seed=(90, 40))
ARROW_TURN_DEG = 45.0
arrow = arrow.rotate(ARROW_TURN_DEG, expand=True, resample=Image.BICUBIC)   # down -> down-right (counter-clockwise)


# ---- vehicle parts render -----------------------------------------------------------------------------------------
def vehicle_parts_cutout():
    name = "RCV_VehicleParts3"
    color = np.asarray(Image.open(RENDERS + name + "_color.png").convert("RGB")).astype(np.float32) / 255
    # The mask is the HDR pass: its alpha is 1 - coverage (as compose_ability_icons.py reads it).
    alpha = 1 - np.asarray(Image.open(RENDERS + name + "_mask.png").convert("RGBA"))[..., 3].astype(np.float32) / 255
    points = json.load(open(RENDERS + name + "_points.json"))
    world = np.asarray(Image.open(RENDERS + name + "_normal.png").convert("RGB")).astype(np.float32) / 255 * 2 - 1
    right, up, forward = (np.array(points["camera"][k], np.float32) for k in ("right", "up", "forward"))
    normals = np.dstack([world @ right, world @ up, -(world @ forward)])
    normals /= np.linalg.norm(normals, axis=2, keepdims=True) + 1e-6
    # Card-icon light: key from the top left like the wrenches and the aircraft, a cool fill and a rim that keeps
    # the dark parts readable on the dark green plate.
    key = np.array([-0.5, 0.6, 0.62], np.float32)
    key /= np.linalg.norm(key)
    diffuse = np.clip(normals @ key, 0, 1)[..., None]
    half = key + np.array([0, 0, 1], np.float32)
    spec = np.clip(normals @ (half / np.linalg.norm(half)), 0, 1)[..., None] ** 14
    rim = (1 - np.clip(normals[..., 2:3], 0, 1)) ** 2.5
    # Neutral steel like the wrenches in T_AB_Repair: the render's olive cast is dropped (luminance only) and the
    # key light is warm, so the parts stand off the green plate.
    luminance = (color @ np.array([0.3, 0.55, 0.15], np.float32))[..., None]
    albedo = np.clip(luminance * 2.1, 0, 1) ** 0.85 * np.array([0.93, 0.95, 0.97], np.float32)
    lit = albedo * (0.38 + 1.0 * diffuse) + spec * np.array([1.0, 0.86, 0.66]) * 0.6         + rim * np.array([0.95, 0.75, 0.5]) * 0.22
    lit = lit / (1 + np.clip(lit - 0.85, 0, None))
    out = np.dstack([np.clip(lit, 0, 1) * 255, alpha * 255]).astype(np.uint8)
    img = Image.fromarray(out, "RGBA")
    return img.crop(img.getbbox())


parts = vehicle_parts_cutout()


def fit(img, longest):
    scale = longest / max(img.size)
    return img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)


def drop_shadow(img, offset=(2, 3), blur=2.5, strength=0.55):
    a = np.asarray(img)[..., 3].astype(np.float32) * strength
    shadow = Image.fromarray(np.dstack([np.zeros(a.shape + (3,)), a]).astype(np.uint8), "RGBA")
    pad = 8
    canvas = Image.new("RGBA", (img.width + 2 * pad, img.height + 2 * pad), (0, 0, 0, 0))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(blur)), (pad + offset[0], pad + offset[1]))
    canvas.alpha_composite(img, (pad, pad))
    return canvas, pad


def place(icon, img, centre):
    shaded, pad = drop_shadow(img)
    icon.alpha_composite(shaded, (round(centre[0] - img.width / 2 - pad), round(centre[1] - img.height / 2 - pad)))


# ---- layout: radixite (top left) -> arrow -> vehicle parts (bottom right) -----------------------------------------
# The diagonal gives both objects room in the hex (a row of three leaves the top and bottom empty).
LAYOUT = {
    "crystals": (fit(crystals, 80), (56, 62)),
    "parts": (fit(parts, 88), (122, 116)),
    "arrow": (fit(arrow, 52), (90, 88)),
}

icon = Image.fromarray(plate.astype(np.uint8), "RGBA")
for key in ("crystals", "parts", "arrow"):
    img, centre = LAYOUT[key]
    place(icon, img, centre)

arr = np.asarray(icon).astype(np.float32)
arr[..., 3] = np.minimum(arr[..., 3], plate[..., 3])
icon = Image.fromarray(arr.astype(np.uint8), "RGBA")

# ---- cancel cross lifted from the green repair pair ---------------------------------------------------------------
cancel = load("T_ActionUI_StopRepair")
plain = load("T_AB_Repair")
diff = np.abs(cancel[..., :3] - plain[..., :3]).sum(-1)
is_red = (cancel[..., 0] > 150) & (cancel[..., 1] < 110) & (cancel[..., 2] < 110)
x_mask = ((diff > 60) & is_red).astype(np.float32)
x_mask = np.asarray(Image.fromarray((x_mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
                    .filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32) / 255.0
# The glow around the cross: the reddening the cross adds to the plate around it.
glow = np.clip((cancel[..., 0] - plain[..., 0]) / 120.0, 0, 1) * (1 - x_mask)

os.makedirs(OUT, exist_ok=True)
icon.save(os.path.join(OUT, ICON_NAME + ".png"))
arr = np.asarray(icon).astype(np.float32)
arr[..., :3] = arr[..., :3] * (1 - glow[..., None] * 0.6) + np.array([255.0, 30.0, 40.0]) * glow[..., None] * 0.6
arr[..., :3] = arr[..., :3] * (1 - x_mask[..., None]) + cancel[..., :3] * x_mask[..., None]
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, CANCEL_NAME + ".png"))
Image.fromarray(plate.astype(np.uint8), "RGBA").save(os.path.join(OUT, "plate_green.png"))
crystals.save(os.path.join(OUT, "lift_crystals.png"))
arrow.save(os.path.join(OUT, "lift_arrow.png"))
parts.save(os.path.join(OUT, "cut_parts.png"))
print("icons written", OUT)
