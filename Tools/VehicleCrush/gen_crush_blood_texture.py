"""Paints T_CrushBloodSmear: the blood a hull leaves where it ran a soldier over.

Usage: python gen_crush_blood_texture.py <out_png>

1024 x 512 RGBA. U (width) runs along the drive direction, as the decal's local Z (along the drive) maps to U.
A noisy pool at the rear third, streaks dragged forward from it with track-link prints stamped across them, and
spatter around. RGB holds thickness-dependent red; alpha the coverage. Unlike the bullet-hit blood (a puff and a
radial splat) this is a long, printed smear on the ground.
"""
import sys

import numpy as np
from PIL import Image, ImageFilter

W, H = 1024, 512
RNG = np.random.default_rng(1945)


def fbm(width, height, octaves=5, base=8):
    out = np.zeros((height, width), np.float32)
    amp, total = 1.0, 0.0
    for octave in range(octaves):
        cells = base * (2 ** octave)
        grid = RNG.random((cells // 2 + 2, cells + 2)).astype(np.float32)
        img = Image.fromarray((grid * 255).astype(np.uint8)).resize((width, height), Image.BICUBIC)
        out += amp * (np.asarray(img, np.float32) / 255.0)
        total += amp
        amp *= 0.5
    return out / total


def ranked(values):
    """Spreads noise evenly over 0-1, so thresholds pick a known share of it."""
    flat = values.ravel()
    order = flat.argsort().argsort().astype(np.float32)
    return (order / max(len(flat) - 1, 1)).reshape(values.shape)


def main(out_png):
    u = np.linspace(0.0, 1.0, W, dtype=np.float32)[None, :].repeat(H, 0)
    v = np.linspace(0.0, 1.0, H, dtype=np.float32)[:, None].repeat(W, 1)
    noise = fbm(W, H)
    fine = fbm(W, H, octaves=4, base=32)

    # Pool where the body went under: rear third, small and ragged.
    pu, pv = 0.27, 0.5
    pool_d = np.sqrt(((u - pu) / 0.11) ** 2 + ((v - pv) / 0.27) ** 2)
    pool = np.clip((1.0 - pool_d + (noise - 0.5) * 1.3 + (fine - 0.5) * 0.5) * 3.0, 0.0, 1.0)

    # Thin streaks dragged forward by the tracks, from the pool towards the front, each its own length.
    rows = ranked(fbm(1, H, octaves=3, base=128)[:, 0])
    streak_mask = np.clip((rows - 0.45) * 6.0, 0.0, 1.0)[:, None].repeat(W, 1)
    band = np.clip(1.0 - np.abs(v - 0.5) / 0.40, 0.0, 1.0) ** 0.5
    forward = np.clip((u - pu) / (0.97 - pu), 0.0, 1.0)
    reach = (0.2 + 0.8 * ranked(fbm(1, H, octaves=3, base=48)[:, 0]))[:, None].repeat(W, 1)
    streak_len = np.clip((reach - forward) * 5.0, 0.0, 1.0)
    breaks = np.clip((ranked(fbm(W, H, octaves=3, base=24)) - 0.2) * 3.0, 0.0, 1.0)
    streaks = 0.8 * streak_mask * band * streak_len * breaks * (u > pu - 0.04)

    # Track links stamped across the smear: regular lighter bars where the pads lifted the blood.
    link_period = 1.0 / 18.0
    phase = (u % link_period) / link_period
    links = np.where((phase < 0.22) & (u > pu + 0.06), 0.35, 1.0).astype(np.float32)

    # Spatter: droplets thrown forward and sideways, stretched along the throw.
    spatter = np.zeros((H, W), np.float32)
    for _ in range(90):
        cu = RNG.normal(pu + 0.1, 0.16)
        cv = RNG.normal(0.5, 0.2)
        r = abs(RNG.normal(0.0, 0.006)) + 0.0015
        stretch = RNG.uniform(1.0, 3.0)
        d = np.sqrt(((u - cu) * 2.0 / stretch) ** 2 + (v - cv) ** 2)
        spatter = np.maximum(spatter, np.clip((r - d) / r * 3.0, 0.0, 1.0) * RNG.uniform(0.5, 0.95))

    alpha = np.maximum(pool, streaks) * links
    alpha = np.maximum(alpha, pool * 0.8)  # the pool itself is too deep to lift
    alpha = np.maximum(alpha, spatter)
    alpha = np.minimum(alpha, 0.92)
    # Soft fade at the decal edges so no rectangle shows.
    edge = np.clip(np.minimum.reduce([u, 1 - u, v, 1 - v]) / 0.06, 0.0, 1.0)
    alpha = np.clip(alpha * edge, 0.0, 1.0)

    # Thick blood is near black-red, thin films brighter.
    thickness = np.clip(pool * 0.8 + noise * 0.4, 0.0, 1.0)
    red = 0.42 - 0.28 * thickness
    rgb = np.stack([red, red * 0.06, red * 0.05], axis=-1)
    rgba = np.concatenate([rgb, alpha[..., None]], axis=-1)
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), "RGBA")
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    img.save(out_png)
    print("wrote", out_png, "coverage %.2f" % float(alpha.mean()))


if __name__ == "__main__":
    main(sys.argv[1])
