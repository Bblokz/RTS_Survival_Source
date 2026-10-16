"""Make a burnt-out wreck diffuse from a tank diffuse, in the style of IS_I_85_Destroyed: rust, soot and flaked paint
over a darkened copy whose panel detail still shows through.

Usage: python burn_texture.py <diffuse in> <diffuse out> [seed]
"""
import sys
import numpy as np
from PIL import Image


def value_noise(size, cells, rng):
    """Smooth noise in 0..1: a random grid upsampled bicubically."""
    grid = rng.random((cells, cells)).astype(np.float32)
    img = Image.fromarray((grid * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
    return np.asarray(img).astype(np.float32) / 255.0


def fractal(size, rng, octaves=((4, 0.5), (12, 0.3), (40, 0.15), (120, 0.05))):
    out = np.zeros((size, size), np.float32)
    for cells, weight in octaves:
        out += value_noise(size, cells, rng) * weight
    return out / sum(w for _, w in octaves)


def smoothstep(lo, hi, x):
    t = np.clip((x - lo) / (hi - lo), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def burn(src_path, dst_path, seed=7):
    src = Image.open(src_path).convert('RGB')
    size = max(src.size)
    base = np.asarray(src.resize((size, size), Image.LANCZOS)).astype(np.float32) / 255.0
    rng = np.random.default_rng(seed)

    grey = base.mean(axis=2, keepdims=True)
    # Burnt paint: mostly desaturated and darker, panel lines and rivets stay readable.
    darkened = (grey * 0.65 + base * 0.35) * 0.55

    rust_color = np.array([0.36, 0.20, 0.11], np.float32)
    rust_dark = np.array([0.16, 0.09, 0.06], np.float32)
    soot_color = np.array([0.04, 0.035, 0.03], np.float32)
    flake_color = np.array([0.55, 0.49, 0.40], np.float32)
    blue_heat = np.array([0.16, 0.17, 0.22], np.float32)

    rust_mask = smoothstep(0.35, 0.65, fractal(size, rng))[..., None]
    rust_var = fractal(size, rng)[..., None]
    rust = rust_dark + (rust_color - rust_dark) * rust_var
    out = darkened * (1 - rust_mask * 0.8) + rust * rust_mask * 0.8 + darkened * rust_mask * 0.2

    heat_mask = smoothstep(0.55, 0.75, fractal(size, rng))[..., None] * 0.5
    out = out * (1 - heat_mask) + blue_heat * heat_mask

    soot_mask = smoothstep(0.5, 0.8, fractal(size, rng))[..., None] * 0.85
    out = out * (1 - soot_mask) + soot_color * soot_mask

    # Flaked paint: hard-edged light patches like the IS-1 wreck, with a little of the panel detail inside.
    flake_noise = fractal(size, rng, ((10, 0.35), (40, 0.35), (160, 0.3)))
    flake_mask = smoothstep(0.66, 0.685, flake_noise)[..., None] * 0.85
    flake = flake_color * (0.75 + 0.5 * grey)
    out = out * (1 - flake_mask) + flake * flake_mask

    grain = (fractal(size, rng, ((256, 1.0),)) - 0.5)[..., None] * 0.06
    out = np.clip(out + grain, 0, 1)
    Image.fromarray((out * 255).astype(np.uint8)).save(dst_path)
    print('burnt', dst_path, size)


if __name__ == '__main__':
    burn(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 7)
