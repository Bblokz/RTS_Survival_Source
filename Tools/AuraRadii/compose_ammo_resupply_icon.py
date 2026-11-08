"""Draws T_Beh_BeingResupplied (180x180), the rising icon over units inside an ammo resupply aura.

The plate is T_Beh_LifeRegen's hex with its cross painted out and its green recoloured to amber, so the icon matches
the other behaviour icons; three brass cartridges and a plus are drawn on top.
Usage: python compose_ammo_resupply_icon.py <exported T_Beh_LifeRegen.png> <output png>"""
import math
import sys

import numpy as np
from PIL import Image

SIZE = 180
SUPER = 4
CENTRE = (SIZE - 1) / 2.0


CLEAN_RADIUS = 58.0
PROFILE_START = 42.0
CENTRE_COLOUR = np.array([55.0, 225.0, 40.0, 215.0])


def paint_out_cross(rgba):
    """The plate gradient is radial: inside CLEAN_RADIUS every pixel takes the colour of the cross-free diagonal at its
    radius, and nearer the centre than the diagonal stays clean it brightens towards CENTRE_COLOUR."""
    out = rgba.astype(np.float32).copy()
    ys, xs = np.mgrid[0:SIZE, 0:SIZE]
    radii = np.hypot(xs - CENTRE, ys - CENTRE)
    for y, x in zip(*np.nonzero(radii < CLEAN_RADIUS)):
        radius = radii[y, x]
        sample_radius = max(radius, PROFILE_START)
        offset = sample_radius * math.cos(math.radians(45))
        colour = rgba[int(round(CENTRE + offset)), int(round(CENTRE + offset))].astype(np.float32)
        if radius < PROFILE_START:
            colour = colour + (CENTRE_COLOUR - colour) * (1.0 - radius / PROFILE_START)
        out[y, x] = colour
    return out


def recolour_amber(rgba):
    rgb = rgba[..., :3].astype(np.float32) / 255.0
    strength = rgb[..., 1]
    whiteness = np.minimum(rgb[..., 0], rgb[..., 2])
    dark = np.array([0.20, 0.07, 0.0])
    bright = np.array([1.0, 0.62, 0.12])
    colour = dark + (bright - dark) * strength[..., None]
    colour = colour + (np.array([1.0, 0.95, 0.82]) - colour) * np.clip(whiteness * 1.4, 0, 1)[..., None]
    out = rgba.astype(np.float32)
    out[..., :3] = np.clip(colour, 0, 1) * 255.0
    return out


def cartridge_layer(width, height):
    """Supersampled RGBA of three upright cartridges and a plus badge; coordinates are in final-icon pixels."""
    s = SUPER
    ys, xs = np.mgrid[0:height * s, 0:width * s].astype(np.float32) / s
    layer = np.zeros((height * s, width * s, 4), np.float32)

    def over(mask, colour):
        alpha = np.clip(mask, 0, 1)[..., None]
        layer[..., :3] = layer[..., :3] * (1 - alpha) + np.array(colour, np.float32) * alpha
        layer[..., 3:] = layer[..., 3:] + alpha * (1 - layer[..., 3:])

    def box(cx, top, bottom, half):
        return ((np.abs(xs - cx) <= half) & (ys >= top) & (ys <= bottom)).astype(np.float32)

    def draw_cartridge(cx, base, scale):
        case_half = 9.0 * scale
        case_top = base - 44.0 * scale
        neck_half = 6.2 * scale
        neck_top = case_top - 7.0 * scale
        tip = neck_top - 26.0 * scale
        outline = 2.2
        # Dark outline first, then the brass case, the copper bullet and the highlights.
        shoulder = (ys >= case_top - 4 * scale) & (ys < case_top) & (
            np.abs(xs - cx) <= neck_half + (case_half - neck_half) * (ys - (case_top - 4 * scale)) / (4 * scale))
        nose_t = np.clip((neck_top - ys) / (neck_top - tip), 0, 1)
        nose_half = neck_half * np.sqrt(np.clip(1 - nose_t ** 2.2, 0, 1))
        nose = (ys <= neck_top) & (ys >= tip) & (np.abs(xs - cx) <= nose_half)
        body = box(cx, case_top, base, case_half) + shoulder + box(cx, neck_top, case_top, neck_half)
        rim = box(cx, base, base + 5 * scale, case_half + 1.5 * scale)
        silhouette = np.clip(body + nose + rim, 0, 1)
        grown = silhouette.copy()
        for oy, ox in [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)]:
            grown = np.maximum(grown, np.roll(np.roll(silhouette, int(oy * outline * s), 0), int(ox * outline * s), 1))
        over(grown, (0.12, 0.05, 0.0))
        shade = np.clip(0.55 + 0.45 * np.cos((xs - cx + case_half * 0.35) / (case_half * 1.25) * 1.3), 0, 1)
        brass = np.stack([0.95 * shade + 0.05, 0.72 * shade + 0.04, 0.22 * shade + 0.02], -1)
        mask = np.clip(body + rim, 0, 1)[..., None]
        layer[..., :3] = layer[..., :3] * (1 - mask) + brass * mask
        copper = np.stack([0.85 * shade + 0.1, 0.42 * shade + 0.05, 0.18 * shade + 0.03], -1)
        nose_mask = nose.astype(np.float32)[..., None]
        layer[..., :3] = layer[..., :3] * (1 - nose_mask) + copper * nose_mask
        over(box(cx, base - 1.2 * scale, base + 0.6 * scale, case_half), (0.35, 0.2, 0.03))
        over(box(cx - case_half * 0.45, neck_top + 2, base - 4 * scale, 1.4 * scale) * 0.85, (1.0, 0.96, 0.78))

    draw_cartridge(60, 134, 0.95)
    draw_cartridge(120, 134, 0.95)
    draw_cartridge(90, 142, 1.12)

    # Plus badge, top right: the rounds are being handed out, not fired.
    px, py, arm, half = 134, 50, 15.0, 5.0
    plus = (((np.abs(xs - px) <= arm) & (np.abs(ys - py) <= half)) |
            ((np.abs(ys - py) <= arm) & (np.abs(xs - px) <= half))).astype(np.float32)
    rim_plus = (((np.abs(xs - px) <= arm + 2.6) & (np.abs(ys - py) <= half + 2.6)) |
                ((np.abs(ys - py) <= arm + 2.6) & (np.abs(xs - px) <= half + 2.6))).astype(np.float32)
    over(rim_plus, (0.08, 0.03, 0.0))
    over(plus, (1.0, 0.97, 0.86))

    small = layer.reshape(height, s, width, s, 4).mean(axis=(1, 3))
    return small


def main():
    source = np.array(Image.open(sys.argv[1]).convert("RGBA")).astype(np.int32)
    plate = recolour_amber(paint_out_cross(source))
    layer = cartridge_layer(SIZE, SIZE)
    alpha = layer[..., 3:4]
    plate[..., :3] = plate[..., :3] * (1 - alpha) + layer[..., :3] * 255.0 * alpha
    plate[..., 3:4] = plate[..., 3:4] + alpha * 255.0 * (1 - plate[..., 3:4] / 255.0)
    Image.fromarray(np.clip(plate, 0, 255).astype(np.uint8), "RGBA").save(sys.argv[2])
    print("wrote", sys.argv[2])


main()
