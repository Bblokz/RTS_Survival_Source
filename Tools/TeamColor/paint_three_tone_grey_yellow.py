"""Repaints a three-tone camo (sand, olive, red-brown) as Panzergrau with dark-yellow blotches, like the SdKfz 250.

Usage: python paint_three_tone_grey_yellow.py <camo png> <output png> [--preview <png>]
Used for the Puma (SdKfz_234_Puma_D_Camo_4) and the SdKfz 234/3 (SdKfz234-3_Camo_2_D): their olive blotches become
the dark-yellow blotches (the only band the team colour keys); the sand base and the red-brown band both become the
Panzergrau base, and the dark interior and underside panels (the darker end of the brown) dark grey. Shapes, wear,
grime, rust, markings and alpha carry over: the olive takes a per-channel gain to the yellow (paint_sdkfz250_grey_yellow.py);
the base classes become a neutral Panzergrau whose brightness follows the pixel's brightness relative to its class
paint (a per-channel gain from warm brown overshoots into blue on the darker panels). Pixels that do not look like
their class paint keep their colour. The old paint colours are measured from the texture.
"""
import argparse

import numpy as np
from PIL import Image, ImageFilter

from paint_sdkfz250_grey_yellow import LUMA, NEW_GREY, NEW_YELLOW, blur, chromaticity, smoothstep

# HSV hue (degrees) of the olive blotches; below it lie the sand (pale) and the red-brown (darker, more saturated).
OLIVE_HUE_EDGE = (50.0, 56.0)
# The SdKfz 234/3 sand is greenish (hue 48-54) but light, so light pixels need a greener hue to count as olive
# (both olives sit at 56 degrees and up when light).
OLIVE_LIGHT_HUE_SHIFT = 6.0
OLIVE_LIGHT_LUMINANCE = (0.30, 0.38)
OLIVE_HUE_TOP = (95.0, 105.0)
# Sand and red-brown share hues; the sand is the bright one (smoothed luminance).
SAND_LUMINANCE = (0.30, 0.38)
# Neutral pixels (bare metal, black gaps, white and black markings) are no paint and keep their colour.
PAINT_SATURATION = (0.08, 0.15)
CLASS_MEDIAN_SIZE = 9
CLASS_BLUR_RADIUS = 3.0
# Class cores for measuring the old paint colours.
CORE_WEIGHT = 0.9
CORE_MIN_LUMINANCE = 0.12
# Off-paint pixels (rust, scratches) keep their colour: chromaticity distance to the class paint.
PAINT_MATCH_RANGE = (0.05, 0.10)
GAP_LUMINANCE = 0.05


def get_hue_saturation(srgb):
    maximum = srgb.max(axis=-1)
    delta = maximum - srgb.min(axis=-1) + 1e-6
    red, green, blue = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.where(maximum == red, ((green - blue) / delta) % 6.0,
                   np.where(maximum == green, (blue - red) / delta + 2.0, (red - green) / delta + 4.0)) * 60.0
    return hue, delta / (maximum + 1e-6)


def get_class_weights(rgb_picture):
    """@return Soft weights (olive, sand, brown) per pixel; whatever is left is neutral and keeps its colour."""
    smooth = np.asarray(rgb_picture.filter(ImageFilter.MedianFilter(CLASS_MEDIAN_SIZE)), dtype=np.float32) / 255.0
    hue, saturation = get_hue_saturation(smooth)
    luminance = smooth @ LUMA
    paint = smoothstep(PAINT_SATURATION[0], PAINT_SATURATION[1], saturation)
    paint *= smoothstep(GAP_LUMINANCE * 0.5, GAP_LUMINANCE, luminance)
    shift = OLIVE_LIGHT_HUE_SHIFT * smoothstep(OLIVE_LIGHT_LUMINANCE[0], OLIVE_LIGHT_LUMINANCE[1], luminance)
    olive = smoothstep(OLIVE_HUE_EDGE[0] + shift, OLIVE_HUE_EDGE[1] + shift, hue) * (1.0 - smoothstep(*OLIVE_HUE_TOP, hue))
    bright = smoothstep(SAND_LUMINANCE[0], SAND_LUMINANCE[1], luminance)
    weights = np.stack([olive, (1.0 - olive) * bright, (1.0 - olive) * (1.0 - bright)], axis=-1) * paint[..., None]
    return np.stack([blur(weights[..., index], CLASS_BLUR_RADIUS) for index in range(3)], axis=-1), luminance


def measure_paint(rgb, weights, luminance):
    """@return Median sRGB of each class's core pixels (olive, sand, brown)."""
    colours = []
    for index in range(3):
        core = (weights[..., index] > CORE_WEIGHT) & (luminance > CORE_MIN_LUMINANCE)
        colours.append(np.median(rgb[core], axis=0))
    return np.stack(colours).astype(np.float32)


def recolour(rgba):
    """@return The repainted RGBA (alpha untouched), the class weights and the measured old paint colours."""
    rgb = rgba[..., :3].astype(np.float32) / 255.0
    weights, luminance = get_class_weights(Image.fromarray(rgba[..., :3]))
    old = measure_paint(rgb, weights, luminance)
    painted = np.clip(weights.sum(axis=-1, keepdims=True), 1e-4, None)
    old_paint = (weights @ old) / painted
    yellow = rgb * (NEW_YELLOW / old[0])
    # Neutral grey at the pixel's brightness relative to its class paint, so sand and brown become one even base.
    relative = (rgb @ LUMA)[..., None] / np.clip((weights[..., 1:] @ (old[1:] @ LUMA))[..., None]
                                                  / np.clip(weights[..., 1:].sum(axis=-1, keepdims=True), 1e-4, None),
                                                  0.02, None)
    grey = relative * NEW_GREY
    target = (weights[..., 0:1] * yellow + weights[..., 1:].sum(axis=-1, keepdims=True) * grey
              + (1.0 - painted.clip(0.0, 1.0)) * rgb) / np.maximum(painted.clip(0.0, 1.0)
                                                                   + (1.0 - painted.clip(0.0, 1.0)), 1e-4)
    distance = np.linalg.norm(chromaticity(rgb) - chromaticity(old_paint), axis=-1)
    match = (1.0 - smoothstep(PAINT_MATCH_RANGE[0], PAINT_MATCH_RANGE[1], distance))[..., None]
    repainted = rgb + (target - rgb) * match
    out = rgba.copy()
    out[..., :3] = np.clip(np.round(repainted * 255.0), 0, 255).astype(np.uint8)
    return out, weights, old


def save_class_preview(weights, path):
    """Yellow = olive (becomes the blotches), pale = sand, brown = red-brown and interior; black = neutral."""
    colours = np.array([[1.00, 0.80, 0.10], [0.75, 0.70, 0.50], [0.50, 0.25, 0.10]], dtype=np.float32)
    preview = Image.fromarray((np.clip(weights @ colours, 0, 1) * 255).astype(np.uint8))
    preview.resize((1024, 1024), Image.BILINEAR).save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("camo_png")
    parser.add_argument("output_png")
    parser.add_argument("--preview")
    arguments = parser.parse_args()
    rgba = np.asarray(Image.open(arguments.camo_png).convert("RGBA"))
    repainted, weights, old = recolour(rgba)
    Image.fromarray(repainted, "RGBA").save(arguments.output_png)
    if arguments.preview:
        save_class_preview(weights, arguments.preview)
    paint = weights.sum(axis=-1)
    print(f"old paint olive {old[0].round(3)} sand {old[1].round(3)} brown {old[2].round(3)}; "
          f"yellow covers {weights[..., 0].sum() / max(paint.sum(), 1.0):.1%} of the paint")


if __name__ == "__main__":
    main()
