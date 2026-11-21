"""Text textures for the intro cinematic's map table and props, drawn with PIL from the engine's own fonts
(no AI images). Output: Saved/IntroCinematic/tex/*.png (RGBA, text on transparent or on paper).

    python make_text_textures.py
"""
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/tex"
FONTS = "D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Content/Slate/Fonts/"
MONO = FONTS + "DroidSansMono.ttf"
ITALIC = FONTS + "Roboto-BoldItalic.ttf"
BOLD = FONTS + "Roboto-BoldCondensed.ttf"

INK = (38, 30, 24, 255)
RED_PENCIL = (168, 34, 28, 255)


def paper(size, tone=(214, 198, 160), seed=1):
    """Aged paper: flat tone with soft blotches and fine grain."""
    rng = random.Random(seed)
    image = Image.new("RGB", size, tone)
    blotch = Image.new("L", (size[0] // 16 + 1, size[1] // 16 + 1))
    blotch.putdata([rng.randint(96, 160) for _ in range(blotch.size[0] * blotch.size[1])])
    blotch = blotch.resize(size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(6))
    grain = Image.new("L", size)
    grain.putdata([rng.randint(112, 144) for _ in range(size[0] * size[1])])
    shade = Image.blend(blotch, grain, 0.35)
    dark = Image.new("RGB", size, (tone[0] - 70, tone[1] - 74, tone[2] - 80))
    light = Image.new("RGB", size, (min(tone[0] + 14, 255), min(tone[1] + 14, 255), min(tone[2] + 10, 255)))
    return Image.composite(light, dark, shade).convert("RGBA")


def rough(image, radius=0.6):
    """Slightly uneven strokes, as from a typewriter ribbon or a pencil."""
    alpha = image.split()[3].filter(ImageFilter.GaussianBlur(radius))
    noise = Image.effect_noise(image.size, 46).point(lambda v: 255 if v > 92 else 150)
    alpha = Image.composite(alpha, Image.new("L", image.size, 0), noise)
    image.putalpha(alpha)
    return image


def text_image(text, font_path, size, color, canvas, spacing=0, angle=0.0, jitter=0.0):
    image = Image.new("RGBA", canvas, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(font_path, size)
    box = draw.textbbox((0, 0), text, font=font, spacing=spacing)
    x = (canvas[0] - (box[2] - box[0])) // 2 - box[0]
    y = (canvas[1] - (box[3] - box[1])) // 2 - box[1]
    if jitter:
        rng = random.Random(len(text))
        cursor = x
        for character in text:
            draw.text((cursor, y + rng.uniform(-jitter, jitter)), character, font=font, fill=color)
            cursor += draw.textlength(character, font=font) + spacing
    else:
        draw.text((x, y), text, font=font, fill=color, spacing=spacing)
    if angle:
        image = image.rotate(angle, resample=Image.BICUBIC)
    return rough(image)


def tag(name, text):
    """Paper luggage tag face, 2:1, typed place name."""
    base = paper((1024, 512), seed=len(text))
    base.alpha_composite(text_image(text, MONO, 104, INK, (1024, 512), jitter=3.0))
    base.save("%s/T_Intro_Tag_%s.png" % (OUT, name))


def main():
    os.makedirs(OUT, exist_ok=True)
    tag("LosAlamos", "LOS ALAMOS")
    tag("OakRidge", "OAK RIDGE")
    tag("Hanford", "HANFORD")
    # Red pencil label on the map (shot 5.4).
    text_image("THE PURPLE DEATH", ITALIC, 150, RED_PENCIL, (2048, 256), angle=3.0, jitter=4.0).filter(
        ImageFilter.GaussianBlur(0.8)).save(OUT + "/T_Intro_PencilPurpleDeath.png")
    # Red pencil year written under the 1945 tick of the date line (shot 2.4a, 2026-11-21).
    pencil_1945()
    # Hand-drawn date line along the table edge (shots 1.2 and 9.2): a ruled line with year ticks, 1944 to 1976.
    line = Image.new("RGBA", (4096, 256), (0, 0, 0, 0))
    draw = ImageDraw.Draw(line)
    rng = random.Random(7)
    left, right, y = 150, 3946, 96
    points = [(x, y + rng.uniform(-2.5, 2.5)) for x in range(left, right + 1, 40)]
    draw.line(points, fill=INK, width=7)
    big = ImageFont.truetype(ITALIC, 84)
    small = ImageFont.truetype(ITALIC, 44)
    for year in range(1944, 1977):
        x = left + (right - left) * (year - 1944) / 32.0
        labelled = year in (1944, 1950, 1960, 1970, 1976)
        draw.line([(x + rng.uniform(-2, 2), y - (34 if labelled else 18)), (x, y + (34 if labelled else 18))], fill=INK, width=6)
        if labelled:
            font = big if year in (1944, 1976) else small
            width = draw.textlength(str(year), font=font)
            draw.text((x - width / 2, y + 44), str(year), font=font, fill=INK)
    rough(line, 0.9).save(OUT + "/T_Intro_DateLine.png")
    # Blueprint title block text for the V2 cutaway (shot 2.2).
    text_image("GEHEIME KOMMANDOSACHE", BOLD, 120, (226, 232, 240, 255), (2048, 256)).save(OUT + "/T_Intro_Geheim.png")
    paper((1024, 1024), seed=3).save(OUT + "/T_Intro_Paper.png")
    print("textures in", OUT, sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()


def blueprint():
    """V2 cutaway blueprint (shot 2.2): white line drawing on blue, drawn from scratch (hull, tanks, engine, fins)."""
    w, h = 2048, 1448
    image = Image.new("RGBA", (w, h), (24, 52, 104, 255))
    draw = ImageDraw.Draw(image)
    rng = random.Random(11)
    line = (214, 226, 240, 255)
    faint = (120, 150, 196, 255)
    for x in range(0, w, 64):
        draw.line([(x, 0), (x, h)], fill=(32, 64, 120, 255), width=1)
    for y in range(0, h, 64):
        draw.line([(0, y), (w, y)], fill=(32, 64, 120, 255), width=1)
    cy, x0, x1, radius = 640, 220, 1800, 150          # the rocket lies along X, nose to the right
    body_end = 1380
    nose = [(body_end + i * 4.2, radius * (1.0 - (i / 100.0) ** 1.7)) for i in range(101)]
    top = [(x0 + 120, cy - radius * 0.72), (x0 + 330, cy - radius)] + [(x, cy - r) for x, r in nose]
    bottom = [(x0 + 120, cy + radius * 0.72), (x0 + 330, cy + radius)] + [(x, cy + r) for x, r in nose]
    draw.line(top, fill=line, width=5)
    draw.line(bottom, fill=line, width=5)
    draw.line([top[0], bottom[0]], fill=line, width=5)
    for sign in (-1, 1):                                # fins
        fin = [(x0 + 330, cy + sign * radius), (x0 + 40, cy + sign * (radius + 250)), (x0 - 40, cy + sign * (radius + 250)),
               (x0 + 60, cy + sign * radius * 0.72)]
        draw.line(fin + [fin[0]], fill=line, width=5)
    for a, b in ((560, 900), (930, 1330)):              # alcohol and oxygen tanks
        draw.rounded_rectangle([a, cy - radius + 22, b, cy + radius - 22], radius=90, outline=line, width=4)
    draw.ellipse([420 - 60, cy - 78, 420 + 100, cy + 78], outline=line, width=4)        # combustion chamber
    draw.polygon([(x0 + 120, cy - 96), (360, cy - 40), (360, cy + 40), (x0 + 120, cy + 96)], outline=line)
    for x in (540, 915, 1345, 1500):                    # frames
        draw.line([(x, cy - radius), (x, cy + radius)], fill=faint, width=3)
    draw.line([(x0 - 80, cy), (x1 + 60, cy)], fill=faint, width=2)
    small = ImageFont.truetype(MONO, 30)
    for (lx, ly), (tx, ty), label in (((730, cy), (700, 300), "B-STOFF"), ((1130, cy), (1100, 240), "A-STOFF"),
                                      ((440, cy), (330, 990), "BRENNKAMMER"), ((1620, cy), (1560, 330), "SPRENGKOPF"),
                                      ((1430, cy), (1330, 1010), "STEUERUNG")):
        draw.line([(lx, ly), (tx + 60, ty + 40)], fill=faint, width=2)
        draw.text((tx, ty), label, font=small, fill=line)
    draw.text((70, 60), "GEHEIME KOMMANDOSACHE", font=ImageFont.truetype(BOLD, 96), fill=line)
    draw.rectangle([1180, 1150, 1980, 1380], outline=line, width=4)
    draw.text((1205, 1165), "GEHEIME KOMMANDOSACHE", font=ImageFont.truetype(BOLD, 62), fill=line)
    draw.text((1205, 1250), "AGGREGAT 4  -  SCHNITT", font=small, fill=line)
    draw.text((1205, 1300), "HVA PEENEMUENDE   BLATT 3", font=small, fill=line)
    stains = Image.effect_noise((w // 8, h // 8), 60).resize((w, h), Image.BICUBIC).filter(ImageFilter.GaussianBlur(20))
    image = Image.composite(image, Image.new("RGBA", (w, h), (16, 36, 76, 255)), stains.point(lambda v: 255 if v > 96 else 190))
    image.filter(ImageFilter.GaussianBlur(0.7)).save(OUT + "/T_Intro_Blueprint.png")


def pencil_1945():
    text_image("1945", ITALIC, 170, RED_PENCIL, (512, 256), angle=-4.0, jitter=3.0).filter(
        ImageFilter.GaussianBlur(0.8)).save(OUT + "/T_Intro_Pencil1945.png")


def date_strip():
    """The date line again, on a paper strip (ink does not show on the dark table)."""
    strip = paper((4096, 256), seed=5)
    strip.alpha_composite(Image.open(OUT + "/T_Intro_DateLine.png"))
    strip.save(OUT + "/T_Intro_DateStrip.png")


if __name__ == "__main__":
    blueprint()
    date_strip()
    print("extra textures done")
