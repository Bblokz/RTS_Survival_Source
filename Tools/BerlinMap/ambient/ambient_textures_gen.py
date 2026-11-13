"""Textures for the Berlin ambient motion (plain Python + Pillow + numpy, no Unreal):

  T_BerlinAmbient_Litter.png   4x4 atlas, 64 px cells (256 x 256, RGBA).
      cells 0-11  one sheet of paper tumbling through half a turn (the two sides look alike, so the loop is seamless)
      cells 12-15 four ash / charred paper flakes
  T_BerlinAmbient_Cloth.png    256 x 256 RGB: curtain cloth (weave, vertical folds, burnt and torn lower edge in alpha)

Run: python ambient_textures_gen.py   -> Saved/Berlin/ambient/textures
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/textures"
CELL = 64
SS = 4          # supersampling


def paper_cell(index, rng):
    size = CELL * SS
    image = Image.new("RGBA", (size, size), (235, 230, 215, 0))
    draw = ImageDraw.Draw(image)
    angle = math.pi * index / 12.0
    fore = max(abs(math.cos(angle)), 0.10)          # never thinner than a tenth: an edge-on sheet must not vanish
    half_w, half_h = 0.30 * size, 0.42 * size * fore
    bow = 0.10 * size * math.sin(angle)             # the sheet bends while it turns
    shade = 0.62 + 0.38 * abs(math.cos(angle)) if index < 6 else 0.50 + 0.42 * abs(math.cos(angle))
    centre = size / 2.0
    points_top, points_bottom = [], []
    for step in range(13):
        u = step / 12.0
        x = centre - half_w + 2 * half_w * u
        curve = bow * math.sin(u * math.pi)
        points_top.append((x, centre - half_h + curve))
        points_bottom.append((x, centre + half_h + curve * 0.6))
    base = np.array([226, 220, 202]) * shade
    draw.polygon(points_top + points_bottom[::-1], fill=tuple(int(v) for v in base) + (255,))
    # a few lines of typing: only on the wider frames, where they read
    if fore > 0.35:
        ink = tuple(int(v * 0.72) for v in base) + (255,)
        rows = 6
        for row in range(rows):
            t = (row + 1.0) / (rows + 1.5)
            y0 = centre - half_h + 2 * half_h * t
            x0 = centre - half_w * 0.78
            x1 = centre + half_w * (0.78 - 0.5 * rng.random() * (row % 3 == 2))
            pts = []
            for step in range(9):
                u = step / 8.0
                x = x0 + (x1 - x0) * u
                rel = (x - (centre - half_w)) / (2 * half_w)
                pts.append((x, y0 + bow * math.sin(rel * math.pi) * (1.0 - 0.4 * t)))
            draw.line(pts, fill=ink, width=max(1, int(SS * 0.9 * fore)))
    return image.resize((CELL, CELL), Image.LANCZOS)


def flake_cell(index, rng):
    size = CELL * SS
    image = Image.new("RGBA", (size, size), (200, 200, 200, 0))
    draw = ImageDraw.Draw(image)
    centre = size / 2.0
    count = 7 + index
    radius = size * (0.26 + 0.05 * index)
    points = []
    for step in range(count):
        a = 2 * math.pi * step / count + rng.uniform(-0.25, 0.25)
        r = radius * rng.uniform(0.45, 1.0)
        points.append((centre + r * math.cos(a) * (1.0 if index % 2 else 0.7), centre + r * math.sin(a)))
    grey = 215 - 12 * index
    draw.polygon(points, fill=(grey, grey, grey - 4, 255))
    # a darker burnt rim on one side
    draw.line(points[: count // 2 + 1], fill=(grey - 90, grey - 92, grey - 95, 255), width=SS * 3)
    return image.resize((CELL, CELL), Image.LANCZOS)


def litter_atlas():
    rng = np.random.default_rng(1945)
    atlas = Image.new("RGBA", (CELL * 4, CELL * 4), (226, 220, 202, 0))
    for index in range(16):
        cell = paper_cell(index, rng) if index < 12 else flake_cell(index - 12, rng)
        atlas.paste(cell, ((index % 4) * CELL, (index // 4) * CELL))
    # bleed the colour under the transparent texels, so mips do not pull in a dark fringe
    data = np.array(atlas).astype(np.float32)
    alpha = data[..., 3:4] / 255.0
    blurred = np.array(atlas.convert("RGB").filter(ImageFilter.GaussianBlur(3))).astype(np.float32)
    weight = np.array(Image.fromarray((alpha[..., 0] * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))).astype(np.float32) / 255.0
    fill = np.where(weight[..., None] > 0.02, blurred / np.maximum(weight[..., None], 0.02), np.array([226, 220, 202]))
    data[..., :3] = np.where(alpha > 0.5, data[..., :3], np.clip(fill, 0, 255))
    Image.fromarray(data.astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_BerlinAmbient_Litter.png"))


def cloth_texture():
    """U runs across the curtain, V from the rail (0) down to the hem (1). Alpha cuts the torn hem and a few holes."""
    size = 256
    rng = np.random.default_rng(7)
    v, u = np.mgrid[0:size, 0:size].astype(np.float32) / size
    folds = 0.5 + 0.5 * np.sin(u * math.pi * 2 * 7 + 0.8 * np.sin(u * math.pi * 2 * 3))
    weave = 0.5 + 0.5 * np.sin(u * math.pi * 2 * 96) * np.sin(v * math.pi * 2 * 96)
    noise = np.array(Image.fromarray((rng.random((32, 32)) * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)).astype(np.float32) / 255.0
    noise2 = np.array(Image.fromarray((rng.random((8, 8)) * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)).astype(np.float32) / 255.0
    # a washed-out curtain: grey-cream cloth, darker in the folds, dirt and soot rising from the hem
    value = 0.50 + 0.22 * folds + 0.05 * weave + 0.10 * (noise - 0.5)
    soot = np.clip((v - 0.55) / 0.45, 0, 1) ** 1.5 * (0.55 + 0.6 * noise2)
    value = value * (1.0 - 0.75 * np.clip(soot, 0, 1))
    rgb = np.stack([value * 0.83, value * 0.80, value * 0.72], axis=-1)
    stripe = (np.sin(u * math.pi * 2 * 14) > 0.86).astype(np.float32)[..., None]       # faded woven stripes
    rgb = rgb * (1.0 - 0.22 * stripe) + np.array([0.20, 0.07, 0.05]) * 0.22 * stripe * value[..., None]
    # torn hem: the cut line wanders with two noise octaves; scattered burn holes above it
    hem = 0.80 + 0.16 * (noise2 - 0.5) * 2.0 + 0.05 * (noise - 0.5) * 2.0
    alpha = (v < hem).astype(np.float32)
    holes = (noise * (0.3 + 0.7 * np.clip((v - 0.3) / 0.7, 0, 1)) > 0.60).astype(np.float32)
    alpha *= 1.0 - holes
    out = np.concatenate([np.clip(rgb, 0, 1), alpha[..., None]], axis=-1)
    Image.fromarray((out * 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, "T_BerlinAmbient_Cloth.png"))


def main():
    os.makedirs(OUT, exist_ok=True)
    litter_atlas()
    cloth_texture()
    print("wrote", OUT)


if __name__ == "__main__":
    main()
