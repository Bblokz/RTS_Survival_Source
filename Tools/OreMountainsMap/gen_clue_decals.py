"""Textures for the miners' shelter (clue B) decals: painted words and the miners' tally marks.

python gen_clue_decals.py  ->  Saved/OreMountains/clues/decals/T_OreClue_<Name>_BCA.png (+ _RMAO.png)

BCA = colour in RGB, paint coverage in A (the cliff decal material M_OreCliffDecal reads it that way). Brush-painted
1946 look: off-white lime paint, uneven coverage, drips, chipped where the 30 years of weather took it off.
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/clues/decals"
FONT = "C:/Windows/Fonts/bahnschrift.ttf"
PAINT = (226, 219, 198)


def noise(size, scale, seed):
    """Blurred value noise 0..255 (L image), the size of size, features about `scale` px."""
    rng = random.Random(seed)
    w, h = max(2, size[0] // scale), max(2, size[1] // scale)
    small = Image.new("L", (w, h))
    small.putdata([rng.randint(0, 255) for _ in range(w * h)])
    return small.resize(size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(scale * 0.3))


def weather(mask, seed, keep=0.78):
    """Chip and thin the paint: coverage drops where coarse + fine noise is low; then add short drips."""
    w, h = mask.size
    coarse, fine = noise(mask.size, 60, seed), noise(mask.size, 9, seed + 1)
    out = Image.new("L", mask.size)
    m, c, f = mask.load(), coarse.load(), fine.load()
    o = out.load()
    for y in range(h):
        for x in range(w):
            if m[x, y] == 0:
                continue
            v = (c[x, y] * 0.6 + f[x, y] * 0.4) / 255.0
            cover = 0.0 if v < 1.0 - keep else min(1.0, (v - (1.0 - keep)) * 3.0)
            o[x, y] = int(m[x, y] * (0.55 + 0.45 * cover) * (1.0 if v > 0.18 else 0.0))
    rng = random.Random(seed + 2)
    draw = ImageDraw.Draw(out)
    bbox = mask.getbbox() or (0, 0, w, h)
    for _ in range(int((bbox[2] - bbox[0]) / 28)):
        x = rng.randint(bbox[0], bbox[2] - 1)
        ys = [y for y in range(bbox[1], bbox[3]) if m[x, y] > 128]
        if not ys:
            continue
        y0 = max(ys)
        length = rng.randint(8, 60)
        draw.line([(x, y0), (x + rng.randint(-2, 2), y0 + length)], fill=rng.randint(120, 220), width=rng.randint(2, 4))
    return out.filter(ImageFilter.GaussianBlur(1.1))


def save(name, alpha):
    w, h = alpha.size
    tone = noise((w, h), 30, 7)
    rgb = Image.new("RGB", (w, h), PAINT)
    dark = Image.new("RGB", (w, h), (150, 140, 120))
    rgb = Image.composite(rgb, dark, tone.point(lambda v: 150 + v // 3))
    bca = rgb.copy()
    bca.putalpha(alpha)
    bca.save(os.path.join(OUT, "T_OreClue_%s_BCA.png" % name))
    # RMAO: rough paint (R 0.85), no metal, AO 1
    Image.merge("RGB", (Image.new("L", (w, h), 217), Image.new("L", (w, h), 0), Image.new("L", (w, h), 255))).save(
        os.path.join(OUT, "T_OreClue_%s_RMAO.png" % name))
    print("wrote", name, alpha.getbbox())


def words():
    """GLÜCK AUF over SCHUTZRAUM 1946, slightly crooked brushwork (2:1, placed 6 x 3 m)."""
    w, h = 2048, 1024
    mask = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(mask)
    big = ImageFont.truetype(FONT, 330)
    small = ImageFont.truetype(FONT, 190)
    draw.text((w // 2, 300), "GLÜCK AUF", font=big, fill=255, anchor="mm", stroke_width=6, stroke_fill=255)
    draw.text((w // 2, 690), "SCHUTZRAUM 1946", font=small, fill=255, anchor="mm", stroke_width=4, stroke_fill=255)
    # underline stroke of a wide brush
    draw.line([(330, 860), (1720, 875)], fill=255, width=22)
    mask = mask.rotate(-1.6, resample=Image.BICUBIC, center=(w // 2, h // 2))
    save("Words", weather(mask, 11))


def tally():
    """Rows of five-bar tally marks (days below ground), 18 weeks and a few, ragged chalk-like paint (1:1, 3 x 3 m)."""
    w = h = 1024
    mask = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(mask)
    rng = random.Random(46)
    groups, x0, y0 = 26, 80, 90
    for g in range(groups):
        row, col = divmod(g, 6)
        gx, gy = x0 + col * 150, y0 + row * 175
        bars = 5 if g < groups - 1 else 3
        for b in range(min(bars, 4)):
            bx = gx + b * 24 + rng.randint(-3, 3)
            draw.line([(bx, gy + rng.randint(-4, 4)), (bx + rng.randint(-5, 5), gy + 120 + rng.randint(-6, 6))],
                      fill=255, width=rng.randint(7, 10))
        if bars == 5:
            draw.line([(gx - 14, gy + 100), (gx + 94, gy + 20)], fill=255, width=9)
    save("Tally", weather(mask, 23, keep=0.84))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    words()
    tally()
