"""Picks the team colour keys (camo bands to repaint) and strength of every target from its camo texture.

Usage: python pick_team_color_keys.py <Saved/TeamColorTest folder>
Reads team_color_diffuse.json, writes team_color_keys.json and keys_sheet.png (each camo, its swap mask and a red team
colour preview made with the same maths as MF_TeamColor_CamoSwap) so the picks can be checked by eye.

Rule: the German schemes share two dark bands, red-brown and dark green, over a dunkelgelb base coat. Both dark bands
are repainted (TC_KeyColor and TC_KeyColor2) so the base coat keeps the camo look. A camo without them may have pale
blotches close to the base coat (Brummbar, Puma): those are keyed with a tight radius (TC_KeyMin / TC_KeyMax). A camo
with neither is one-tone: its whole paint gets a light tint, so no vehicle changes clearly more than the others.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# Mirrors the material function defaults.
PAINT_MIN, PAINT_MAX = 0.035, 0.10
KEY_MIN, KEY_MAX = 0.08, 0.15
LIFT = 0.45
STRENGTH_BAND = 0.95
# One-tone camo has no band to repaint; the whole paint gets a light tint instead. Three-tone vehicles repaint about
# 10-25% of their paint at 0.95; 0.3 over all paint already read stronger than that on colourful camo (Jagdpanzer
# IV/70, Tiger I), 0.2 does not.
STRENGTH_ONE_TONE = 0.2
# Reference bands (sRGB): the defaults of the hull masters, whose instances each get measured keys.
RED_BROWN_SRGB = (0.19, 0.085, 0.002)
DARK_GREEN_SRGB = (0.07, 0.10, 0.005)
# HSV hue ranges (degrees) of the bands; dunkelgelb sits between them. The bands are saturated paint (blue near 0),
# the base coat and its wear are not.
RED_BROWN_HUE = (0.0, 32.0)
GREEN_HUE = (58.0, 160.0)
BAND_MIN_SATURATION = 0.7
# Share of the painted pixels a band needs to count as part of the camo rather than a stray marking.
MIN_BAND_SHARE = 0.02
# Most of the base coat must stay untouched, or the camo turns into a one-colour vehicle.
MAX_BASE_LEAK = 0.15
BASE_HUE = (32.0, 58.0)
ANALYSIS_SIZE = 256
CLUSTER_COUNT = 5
MERGE_DISTANCE = 0.06
# Textures that are not the camo the material shows (picked because the root has no better candidate); never swap.
NO_SWAP_TEXTURES = ["T_Ger_InfWeapons_MetRouAO_Atlas"]
# Parts whose own texture has a few band or blotch spots while the rest of the vehicle is one-tone; tinted like the
# hull so one vehicle never mixes the two looks (Panther G green scheme, SdKfz 250 turret).
ONE_TONE_TEXTURES = ["T_PantherG_Turret_Green", "T_PantherG_Engine_Green", "Sdkfz_250_9_D_camo2"]
KEY_RADIUS = (KEY_MIN, KEY_MAX)
# Pale camo whose blotches lie close to the base coat in chromaticity (about 0.06 away): tried when no band is found,
# with a tighter radius (TC_KeyMin / TC_KeyMax) and less saturated band pixels. Kept only when the blotches cover a
# camo-like share of the paint; more means the key caught a reddish base coat (SdKfz 231, Panzer II turret).
PALE_KEY_RADIUS = (0.015, 0.03)
PALE_BAND_MIN_SATURATION = 0.35
PALE_COVERAGE = (0.05, 0.35)
# Parts keyed like the pale camo of the rest of their vehicle although the default radius finds a band on them
# (Brummbar side plates: its hull and gun are pale camo).
PALE_FIRST_TEXTURES = ["Brummbar_Plates_Yellow_Camo"]
# Vehicles that repaint one band of their camo instead of the one-tone tint, by material name: sRGB key and key radius.
# Tiger E: MM_Ger_PostApoTank shows lerp(TankDiffuse * DiffuseTint, Camo, CamoAlphaStrenght), so the camo on screen is
# that blend, not the Camo texture; the keys are its red-brown (desert) and brown (forest) band measured on the blend.
# Its two turret materials share one diffuse but not the overlay, so this goes by material, not texture.
# Panther G: the olive-green blotches (hue 52-90) of its green scheme; the generic green band needs saturation 0.7.
TARGET_KEY_OVERRIDES = {
    "MI_Tiger1_PostApo_HullDesert": ((0.218, 0.152, 0.109), (0.03, 0.06), "Tiger E overlay red-brown"),
    "MI_Tiger1_PostApo_TurretDesert": ((0.218, 0.152, 0.109), (0.03, 0.06), "Tiger E overlay red-brown"),
    "MI_Tiger1_PostApo_TurretForest": ((0.286, 0.238, 0.169), (0.015, 0.03), "Tiger E forest overlay brown"),
    "MI_PantherG_Hull_Green": ((0.297, 0.292, 0.211), (0.015, 0.03), "Panther G olive blotches"),
    "MI_PantherG_Turret_Green": ((0.297, 0.292, 0.211), (0.015, 0.03), "Panther G olive blotches"),
    "MI_PantherG_Engine_Green": ((0.297, 0.292, 0.211), (0.015, 0.03), "Panther G olive blotches"),
}
PREVIEW_TEAM_SRGB = (0.85, 0.12, 0.08)
CELL = 200


def smoothstep(edge0, edge1, value):
    t = np.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def load_srgb(png_path, size):
    picture = Image.open(png_path).convert("RGB").resize((size, size), Image.BILINEAR)
    return np.asarray(picture, dtype=np.float32) / 255.0


def chromaticity(srgb):
    return srgb / (srgb.sum(axis=-1, keepdims=True) + 0.001)


def paint_mask(srgb):
    chroma = srgb.max(axis=-1) - srgb.min(axis=-1)
    return smoothstep(PAINT_MIN, PAINT_MAX, chroma)


def get_hue_saturation(srgb):
    """@return HSV hue in degrees and saturation, per pixel."""
    maximum = srgb.max(axis=-1)
    minimum = srgb.min(axis=-1)
    delta = maximum - minimum + 1e-6
    red, green, blue = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.where(maximum == red, ((green - blue) / delta) % 6.0,
                   np.where(maximum == green, (blue - red) / delta + 2.0, (red - green) / delta + 4.0)) * 60.0
    return hue, delta / (maximum + 1e-6)


def get_key_mask(srgb, key_srgb, radius=KEY_RADIUS):
    key_distance = np.linalg.norm(chromaticity(srgb) - chromaticity(np.array(key_srgb, dtype=np.float32)), axis=-1)
    return (1.0 - smoothstep(radius[0], radius[1], key_distance)) * paint_mask(srgb)


def swap_preview(srgb, keys, strength, team_srgb, radius=KEY_RADIUS):
    """Same maths as MF_TeamColor_CamoSwap, in sRGB. @return The repainted texture and the swap mask."""
    mask = np.zeros(srgb.shape[:2], dtype=np.float32)
    for key in keys:
        mask = np.maximum(mask, get_key_mask(srgb, key, radius))
    luminance = srgb @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    lifted = (luminance + (0.5 - luminance) * LIFT)[..., None]
    team = np.array(team_srgb, dtype=np.float32)
    painted = np.where(lifted < 0.5, 2.0 * lifted * team, 1.0 - 2.0 * (1.0 - lifted) * (1.0 - team))
    weight = np.clip(strength * mask, 0.0, 1.0)[..., None]
    return srgb + (painted - srgb) * weight, mask


def measure_band(srgb, hue_range, radius=KEY_RADIUS, min_saturation=BAND_MIN_SATURATION):
    """@return Mean sRGB of the camo band in the hue range, or None when the camo has (almost) none of it."""
    hue, saturation = get_hue_saturation(srgb)
    painted = paint_mask(srgb) > 0.5
    band = painted & (hue >= hue_range[0]) & (hue < hue_range[1]) & (saturation >= min_saturation)
    if band.sum() < 50:
        return None
    # Median chromaticity: wear and soot inside the band pull a mean towards grey.
    chroma = chromaticity(srgb[band])
    nearest = np.linalg.norm(chroma - np.median(chroma, axis=0), axis=-1) < radius[0]
    if not nearest.any():
        return None
    key = tuple(float(v) for v in srgb[band][nearest].mean(axis=0))
    share = float(get_key_mask(srgb, key, radius).sum() / max(paint_mask(srgb).sum(), 1.0))
    if share < MIN_BAND_SHARE or get_base_leak(srgb, key, radius, min_saturation) > MAX_BASE_LEAK:
        return None
    return key


def get_base_leak(srgb, key_srgb, radius=KEY_RADIUS, min_saturation=BAND_MIN_SATURATION):
    """@return The share of the dunkelgelb base coat the key would repaint too."""
    hue, saturation = get_hue_saturation(srgb)
    base = (paint_mask(srgb) > 0.5) & (hue >= BASE_HUE[0]) & (hue < BASE_HUE[1]) & (saturation < min_saturation)
    return float((get_key_mask(srgb, key_srgb, radius)[base] > 0.5).mean()) if base.any() else 0.0


def find_dominant_paint(srgb):
    """@return Mean sRGB of the largest chromaticity cluster of the paint, or None without paint."""
    pixels = srgb.reshape(-1, 3)
    weights = paint_mask(srgb).reshape(-1)
    painted = weights > 0.5
    if painted.sum() < 50:
        return None
    points = chromaticity(pixels[painted])
    generator = np.random.default_rng(7)
    centres = points[generator.choice(len(points), size=min(CLUSTER_COUNT, len(points)), replace=False)]
    for _ in range(25):
        labels = np.argmin(((points[:, None, :] - centres[None]) ** 2).sum(-1), axis=1)
        for index in range(len(centres)):
            if (labels == index).any():
                centres[index] = points[labels == index].mean(axis=0)
    counts = []
    for index, centre in enumerate(centres):
        # Clusters closer than MERGE_DISTANCE are one band split by the clustering.
        close = np.linalg.norm(centres - centre, axis=-1) < MERGE_DISTANCE
        counts.append((np.isin(labels, np.nonzero(close)[0]).sum(), index))
    best = max(counts)[1]
    close = np.linalg.norm(centres - centres[best], axis=-1) < MERGE_DISTANCE
    return tuple(float(v) for v in pixels[painted][np.isin(labels, np.nonzero(close)[0])].mean(axis=0))


def pick_keys(srgb, texture_name):
    """@return (keys sRGB list of 0-2, strength, reason, key radius); no keys means nothing swaps."""
    if texture_name in NO_SWAP_TEXTURES:
        return [], 0.0, "not the camo texture: off", KEY_RADIUS
    if texture_name not in ONE_TONE_TEXTURES:
        found, coverage = ([], 0.0) if texture_name in PALE_FIRST_TEXTURES else find_bands_at_radius(
            srgb, KEY_RADIUS, BAND_MIN_SATURATION)
        if found:
            names = " + ".join(name for name, _ in found)
            return [key for _, key in found], STRENGTH_BAND, f"{names} ({coverage:.0%} of paint)", KEY_RADIUS
        found, coverage = find_bands_at_radius(srgb, PALE_KEY_RADIUS, PALE_BAND_MIN_SATURATION)
        if found and PALE_COVERAGE[0] <= coverage <= PALE_COVERAGE[1]:
            names = " + ".join(name for name, _ in found)
            return ([key for _, key in found], STRENGTH_BAND, f"pale {names}, tight key ({coverage:.0%} of paint)",
                    PALE_KEY_RADIUS)
    base = find_dominant_paint(srgb)
    if base is None:
        return [], 0.0, "no paint: off", KEY_RADIUS
    return [base], STRENGTH_ONE_TONE, "one-tone: light tint", KEY_RADIUS


def find_bands_at_radius(srgb, radius, min_saturation):
    """@return The red-brown and green bands found with this key radius, and the share of the paint they cover."""
    found = []
    for name, hue_range in (("red-brown", RED_BROWN_HUE), ("green", GREEN_HUE)):
        key = measure_band(srgb, hue_range, radius, min_saturation)
        if key is not None:
            found.append((name, key))
    if not found:
        return [], 0.0
    mask = get_key_mask(srgb, found[0][1], radius)
    for _, key in found[1:]:
        mask = np.maximum(mask, get_key_mask(srgb, key, radius))
    return found, float(mask.sum() / max(paint_mask(srgb).sum(), 1.0))


def to_linear(srgb_key):
    return [value ** 2.2 for value in srgb_key]


def main():
    folder = sys.argv[1]
    with open(os.path.join(folder, "team_color_diffuse.json"), encoding="utf-8") as handle:
        targets = json.load(handle)
    by_png = {}
    keys = {}
    for target, info in targets.items():
        override = TARGET_KEY_OVERRIDES.get(target.split("/")[-1])
        if override:
            key, radius, reason = override
            keys[target] = {"kind": info["kind"], "texture": os.path.basename(info["png"]), "keys": [to_linear(key)],
                            "strength": STRENGTH_BAND, "key_radius": list(radius), "reason": reason}
            continue
        # Not the raw camo overlay of MM_Ger_PostApoTank (overlay_png): blended with the diffuse its colours shift
        # (about 0.3 in chromaticity), so keys measured on it miss on screen; see TARGET_KEY_OVERRIDES for the Tiger E.
        png =os.path.join(folder, "diffuse", os.path.basename(info["png"])) if info["png"] else ""
        if info["kind"] == "master":
            # Master defaults only show on an instance without its own camo; every listed instance has one.
            keys[target] = {"kind": "master", "keys": [to_linear(RED_BROWN_SRGB), to_linear(DARK_GREEN_SRGB)],
                            "strength": STRENGTH_BAND, "key_radius": list(KEY_RADIUS), "reason": "master default"}
            continue
        if not png or not os.path.exists(png):
            keys[target] = {"kind": info["kind"], "inherit": True, "reason": "no camo texture of its own"}
            continue
        if png not in by_png:
            srgb = load_srgb(png, ANALYSIS_SIZE)
            texture_name = os.path.basename(png)[:-4].split("__")[-1]
            picked, strength, reason, radius = pick_keys(srgb, texture_name)
            by_png[png] = {"keys": picked, "strength": strength, "reason": reason, "radius": radius, "srgb": srgb}
        pick = by_png[png]
        keys[target] = {"kind": info["kind"], "texture": os.path.basename(png),
                        "keys": [to_linear(key) for key in pick["keys"]], "strength": pick["strength"],
                        "key_radius": list(pick["radius"]), "reason": pick["reason"]}
    with open(os.path.join(folder, "team_color_keys.json"), "w", encoding="utf-8") as handle:
        json.dump(keys, handle, indent=1)
    write_sheet(folder, by_png)


def write_sheet(folder, by_png):
    rows = sorted(by_png.items(), key=lambda item: os.path.basename(item[0]).split("__")[-1])
    sheet = Image.new("RGB", (CELL * 3 + 300, CELL * len(rows)), (24, 24, 26))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 14)
    except OSError:
        font = ImageFont.load_default()
    for row, (png, pick) in enumerate(rows):
        preview, mask = swap_preview(pick["srgb"], pick["keys"], pick["strength"], PREVIEW_TEAM_SRGB, pick["radius"])
        top = row * CELL
        for column, picture in enumerate([pick["srgb"], np.repeat(mask[..., None], 3, axis=-1), preview]):
            sheet.paste(Image.fromarray((np.clip(picture, 0, 1) * 255).astype(np.uint8)).resize((CELL, CELL)),
                        (CELL * column, top))
        draw.text((CELL * 3 + 8, top + 8), os.path.basename(png)[:-4].split("__")[-1][:34], fill=(235, 235, 235),
                  font=font)
        draw.text((CELL * 3 + 8, top + 30), pick["reason"][:40], fill=(200, 200, 200), font=font)
        for index, key in enumerate(pick["keys"]):
            draw.rectangle([CELL * 3 + 8 + index * 48, top + 60, CELL * 3 + 48 + index * 48, top + 100],
                           fill=tuple(int(v * 255) for v in key))
        draw.text((CELL * 3 + 8, top + 110), f"strength {pick['strength']}", fill=(200, 200, 200), font=font)
    sheet.save(os.path.join(folder, "keys_sheet.png"))
    print(f"{len(rows)} camo textures -> {os.path.join(folder, 'keys_sheet.png')}")


if __name__ == "__main__":
    main()
