"""Repaints the SdKfz 250 hull camo as Panzergrau with dark-yellow blotches, so the team colour can key the yellow alone.

Usage: python paint_sdkfz250_grey_yellow.py <hull png> <ambulance png> <output folder>
The inputs are the exports of T_Sdkfz_250_D and T_Sdkfz250_Ambulance_D (export_team_color_diffuse.py). Writes
T_Sdkfz_250_GreyYellow_D.png, T_Sdkfz250_Ambulance_GreyYellow_D.png and a class preview.

The original camo is sand blotches on a blue-grey coat, with dark-brown interior and underside panels. Both camos have
low chroma, so the key could not tell them apart and the vehicle only got a whole-paint tint. The blotch shapes are kept
(they already follow the UV islands and the wear is painted along them), shrunk so the yellow covers about a third of
the camo panels instead of half: sand -> dark yellow (or Panzergrau where the blotch shrank), blue-grey -> Panzergrau,
dark brown -> dark grey. Each pixel is multiplied by a per-channel gain
(new paint / old paint) taken from smoothed class weights, so wear, grime, rust and shading carry over as ratios.
Pixels whose colour is not the paint (rust, bare metal, markings) get less of the gain.
The ambulance keeps every pixel that differs from the plain hull texture (red crosses, tarp) as it was.
Alpha (each texture's own) is copied unchanged.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

# Old paint colours (sRGB medians measured on T_Sdkfz_250_D) and their replacements.
OLD_GREY = np.array([0.223, 0.239, 0.243], dtype=np.float32)
OLD_SAND = np.array([0.420, 0.375, 0.280], dtype=np.float32)
OLD_DARK = np.array([0.149, 0.133, 0.114], dtype=np.float32)
# Panzergrau: the old blue-grey already is one (max - min 0.02, below the team colour paint mask), so it stays.
NEW_GREY = OLD_GREY
# Dark yellow at the brightness of the fleet's dunkelgelb (median about 0.40, 0.355, 0.245) but more saturated, so its
# chromaticity sits about 0.18 from the grey and from the canvas tarp of the ambulance.
NEW_YELLOW = np.array([0.420, 0.360, 0.200], dtype=np.float32)
# Interior and underside panels: grey at the old dark-brown brightness.
NEW_DARK = OLD_GREY * float(OLD_DARK @ np.array([0.299, 0.587, 0.114]) / (OLD_GREY @ np.array([0.299, 0.587, 0.114])))
# Warmth (r - b) / (r + g + b) separates sand and dark brown (warm) from the blue-grey (cool).
WARM_RANGE = (0.03, 0.08)
# Large-scale luminance separates the sand (about 0.3-0.6) from the dark-brown panels (about 0.1-0.2).
BRIGHT_RANGE = (0.20, 0.27)
# Below this smoothed luminance the pixel is the black gap between UV islands or a black marking.
GAP_LUMINANCE = 0.05
CLASS_MEDIAN_SIZE = 9
CLASS_BLUR_RADIUS = 3.0
# Pixels farther than this from their class paint in chromaticity (rust, bare metal, white markings) keep their colour.
PAINT_MATCH_RANGE = (0.05, 0.10)
# The sand covers about half of the camo panels; the yellow keeps the same blotches, shrunk to this share, so the team
# colour reads as a marking. The shrink blurs the sand only over paint (normalised), so island edges do not eat it.
YELLOW_SHARE = 0.33
SHRINK_BLUR_RADIUS = 14.0
SHRINK_SOFTNESS = 0.06
# Grey paint (not metal edges or the gaps) that counts toward the camo panels, by smoothed luminance.
GREY_PAINT_LUMINANCE = (0.12, 0.50)
CAMO_PANEL_REACH = 25.0
CAMO_PANEL_MIN_SAND = 0.08
# Sand that turns grey takes the grey of its neighbourhood (baked shading makes lit plates brighter than the global
# reference); with the global colours a former blotch would show as a darker ghost of the old pattern.
NEIGHBOURHOOD_RADIUS = 32.0
NEIGHBOURHOOD_DOWNSAMPLE = 4
NEIGHBOURHOOD_MIN_WEIGHT = 0.05
AMBULANCE_DIFF = 0.03
AMBULANCE_GROW = 9
AMBULANCE_FEATHER = 3.0
LUMA = np.array([0.299, 0.587, 0.114], dtype=np.float32)


def smoothstep(edge0, edge1, value):
    t = np.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def chromaticity(srgb):
    return srgb / (srgb.sum(axis=-1, keepdims=True) + 0.001)


def blur(array, radius):
    """Separable Gaussian blur in float (PIL only blurs 8-bit pictures), edges clamped."""
    reach = int(np.ceil(radius * 3.0))
    taps = np.exp(-0.5 * (np.arange(-reach, reach + 1) / radius) ** 2)
    taps /= taps.sum()
    result = array.astype(np.float32)
    for axis in (0, 1):
        padded = np.pad(result, [(reach, reach) if index == axis else (0, 0) for index in range(2)], mode="edge")
        size = result.shape[axis]
        result = sum(tap * np.take(padded, np.arange(offset, offset + size), axis=axis)
                     for offset, tap in enumerate(taps)).astype(np.float32)
    return result


def get_class_weights(rgb_picture):
    """@return Soft weights (grey, sand, dark) per pixel, from a median-filtered copy so wear does not flip classes."""
    smooth = np.asarray(rgb_picture.filter(ImageFilter.MedianFilter(CLASS_MEDIAN_SIZE)), dtype=np.float32) / 255.0
    luminance = smooth @ LUMA
    warmth = (smooth[..., 0] - smooth[..., 2]) / (smooth.sum(axis=-1) + 0.001)
    warm = blur(smoothstep(WARM_RANGE[0], WARM_RANGE[1], warmth), CLASS_BLUR_RADIUS)
    bright = blur(smoothstep(BRIGHT_RANGE[0], BRIGHT_RANGE[1], luminance), CLASS_BLUR_RADIUS)
    return np.stack([1.0 - warm, warm * bright, warm * (1.0 - bright)], axis=-1), luminance


def get_grey_paint(weights, luminance):
    return weights[..., 0] * ((luminance > GREY_PAINT_LUMINANCE[0]) & (luminance < GREY_PAINT_LUMINANCE[1]))


def get_camo_panels(weights):
    """@return Where the camo panels are: paint within reach of a sand blotch (not the one-tone dark panels)."""
    return blur(weights[..., 1], CAMO_PANEL_REACH) > CAMO_PANEL_MIN_SAND


def get_yellow_share(yellow, weights, grey_paint, panels):
    """@return The share of the camo panels' paint that the yellow covers."""
    paint = ((weights[..., 1] + grey_paint) * panels).sum()
    return float((yellow * panels).sum() / max(paint, 1.0))


def get_yellow_mask(weights, luminance):
    """@return Per pixel how yellow the paint becomes: the sand blotches shrunk to YELLOW_SHARE of the camo panels."""
    grey_paint = get_grey_paint(weights, luminance)
    panels = get_camo_panels(weights)
    paint = weights[..., 1] + grey_paint
    spread = blur(weights[..., 1], SHRINK_BLUR_RADIUS) / np.maximum(blur(paint, SHRINK_BLUR_RADIUS), 0.05)
    low, high = 0.5, 0.98
    for _ in range(14):
        threshold = 0.5 * (low + high)
        yellow = weights[..., 1] * smoothstep(threshold - SHRINK_SOFTNESS, threshold + SHRINK_SOFTNESS, spread)
        if get_yellow_share(yellow, weights, grey_paint, panels) > YELLOW_SHARE:
            low = threshold
        else:
            high = threshold
    return yellow


def recolour(rgba, yellow=None):
    """@return The repainted RGBA (alpha untouched), the class weights and the yellow mask.
    @param yellow The yellow mask to reuse (the ambulance takes the hull's, so both get the same blotches)."""
    rgb = rgba[..., :3].astype(np.float32) / 255.0
    weights, luminance = get_class_weights(Image.fromarray(rgba[..., :3]))
    if yellow is None:
        yellow = get_yellow_mask(weights, luminance)
    old = np.stack([OLD_GREY, OLD_SAND, OLD_DARK])
    old_paint = weights @ old
    # Sand outside the shrunk blotches turns Panzergrau.
    sand_gain = (yellow[..., None] * (NEW_YELLOW / OLD_SAND)
                 + (weights[..., 1:2] - yellow[..., None]) * get_sand_to_grey_gain(rgb, weights, luminance))
    gain = weights[..., 0:1] * (NEW_GREY / OLD_GREY) + sand_gain + weights[..., 2:3] * (NEW_DARK / OLD_DARK)
    # Off-paint pixels keep their colour: compare each pixel's chromaticity with the paint its class expects.
    paint_distance = np.linalg.norm(chromaticity(rgb) - chromaticity(old_paint), axis=-1)
    match = 1.0 - smoothstep(PAINT_MATCH_RANGE[0], PAINT_MATCH_RANGE[1], paint_distance)
    match *= smoothstep(GAP_LUMINANCE * 0.5, GAP_LUMINANCE, luminance)
    repainted = rgb * (1.0 + (gain - 1.0) * match[..., None])
    out = rgba.copy()
    out[..., :3] = np.clip(np.round(repainted * 255.0), 0, 255).astype(np.uint8)
    return out, weights, yellow


def get_neighbourhood_mean(rgb, weight):
    """@return Weighted mean colour around each pixel and how much weight backs it (computed at reduced size)."""
    step = NEIGHBOURHOOD_DOWNSAMPLE
    height, width = weight.shape
    small_weight = weight.reshape(height // step, step, width // step, step).mean(axis=(1, 3))
    small_colour = (rgb * weight[..., None]).reshape(height // step, step, width // step, step, 3).mean(axis=(1, 3))
    radius = NEIGHBOURHOOD_RADIUS / step
    blurred_weight = blur(small_weight, radius)
    channels = [blur(small_colour[..., channel], radius) / np.maximum(blurred_weight, 1e-4) for channel in range(3)]

    def upsample(array):
        picture = Image.fromarray(array.astype(np.float32), mode="F").resize((width, height), Image.BILINEAR)
        return np.asarray(picture, dtype=np.float32)

    return np.stack([upsample(channel) for channel in channels], axis=-1), upsample(blurred_weight)


def get_sand_to_grey_gain(rgb, weights, luminance):
    """@return Per-channel gain that turns sand into the grey next to it (global colours where no grey is near)."""
    grey_colour, grey_weight = get_neighbourhood_mean(rgb, get_grey_paint(weights, luminance))
    sand_colour, sand_weight = get_neighbourhood_mean(rgb, weights[..., 1])
    trust = (smoothstep(0.0, NEIGHBOURHOOD_MIN_WEIGHT, grey_weight)
             * smoothstep(0.0, NEIGHBOURHOOD_MIN_WEIGHT, sand_weight))[..., None]
    local_gain = grey_colour / np.maximum(sand_colour, 0.02)
    return trust * local_gain + (1.0 - trust) * (NEW_GREY / OLD_SAND)


def keep_ambulance_markings(repainted, ambulance, hull):
    """@return The repainted ambulance with its own pixels wherever it differs from the plain hull texture."""
    differs = (np.abs(ambulance[..., :3].astype(np.int16) - hull[..., :3].astype(np.int16)).max(axis=-1)
               > AMBULANCE_DIFF * 255).astype(np.uint8) * 255
    grown = Image.fromarray(differs).filter(ImageFilter.MaxFilter(AMBULANCE_GROW))
    keep = np.asarray(grown.filter(ImageFilter.GaussianBlur(AMBULANCE_FEATHER)), dtype=np.float32)[..., None] / 255.0
    out = repainted.copy()
    blended = repainted[..., :3] * (1.0 - keep) + ambulance[..., :3] * keep
    out[..., :3] = np.clip(np.round(blended), 0, 255).astype(np.uint8)
    return out, keep[..., 0]


def save_class_preview(weights, yellow, path):
    """Blue = grey paint, brown = dark panels, pale yellow = sand turned grey, strong yellow = the new blotches."""
    colours = np.array([[0.30, 0.50, 0.90], [0.75, 0.70, 0.50], [0.50, 0.25, 0.10]], dtype=np.float32)
    shown = weights @ colours + yellow[..., None] * (np.array([1.00, 0.80, 0.10]) - colours[1])
    preview = Image.fromarray((np.clip(shown, 0, 1) * 255).astype(np.uint8))
    preview.resize((1024, 1024), Image.BILINEAR).save(path)


def main():
    hull_png, ambulance_png, output = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(output, exist_ok=True)
    hull = np.asarray(Image.open(hull_png).convert("RGBA"))
    ambulance = np.asarray(Image.open(ambulance_png).convert("RGBA"))
    hull_new, weights, yellow = recolour(hull)
    Image.fromarray(hull_new, "RGBA").save(os.path.join(output, "T_Sdkfz_250_GreyYellow_D.png"))
    save_class_preview(weights, yellow, os.path.join(output, "classes_preview.png"))
    ambulance_repainted, _, _ = recolour(ambulance, yellow)
    ambulance_new, keep = keep_ambulance_markings(ambulance_repainted, ambulance, hull)
    Image.fromarray(ambulance_new, "RGBA").save(os.path.join(output, "T_Sdkfz250_Ambulance_GreyYellow_D.png"))
    _, luminance = get_class_weights(Image.fromarray(hull[..., :3]))
    grey_paint, panels = get_grey_paint(weights, luminance), get_camo_panels(weights)
    print(f"yellow: {get_yellow_share(yellow, weights, grey_paint, panels):.1%} of the camo panels (sand was "
          f"{get_yellow_share(weights[..., 1], weights, grey_paint, panels):.1%}); "
          f"ambulance kept {(keep > 0.5).mean():.1%}")


if __name__ == "__main__":
    main()
