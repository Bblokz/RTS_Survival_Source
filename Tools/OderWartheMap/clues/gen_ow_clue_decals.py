"""Painted stencils and notices for the Ger_OderWarthe Act 1 clue spots (quarantine wagon, algae sampling camp).

python -B gen_ow_clue_decals.py [out_dir]  ->  T_OWClue_<Name>_BCA.png (+ _RMAO.png) and preview_<Name>.jpg

Same convention as the Ore Mountains clue decals (Source/Tools/OreMountainsMap/gen_clue_decals.py): BCA = paint colour
in RGB, paint coverage in A; RMAO = rough paint, no metal, AO 1. 1946 brush/stencil paint, weathered 30 years.

Russian texts (what they say):
  Quarantine   КАРАНТИН / СЕКТОР ЛЕБУС                      quarantine / sector Lebus
  Grain        ЗЕРНО ЗАРАЖЕНО / «ФИТОФАГ» / НЕ ВСКРЫВАТЬ    grain contaminated / "Fitofag" / do not open
  WagonNo      НКПС 41-7719 / 20 т / 1946                   wagon number (People's Commissariat of Railways), load
  RoofX        a big painted cross + КАРАНТИН (read from the RTS camera above)
  Burn         СЖЕЧЬ / ПРИКАЗ 46/112                        burn / order 46/112
  Notice       paper order nailed on the door (typed), Fitofag in the wheat, sector Lebus, burn the grain
  WaterWarn    ВНИМАНИЕ! / ВОДОЁМ ЗАРАЖЁН                     attention! / water body contaminated
  NoWater      КУПАТЬСЯ И БРАТЬ ВОДУ ЗАПРЕЩЕНО               bathing and taking water prohibited
  Station      ПУНКТ ОТБОРА ПРОБ № 3                        sampling point no. 3
  Crate        ОСТОРОЖНО / СТЕКЛО / ПРОБЫ                    careful / glass / samples
"""
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else "D:/UE5Projects/RTS_Survival/Saved/OderWarthe/clues/decals"
FONTS = "C:/Windows/Fonts/"
LIME = (228, 222, 204)
RED = (156, 38, 28)
STENCIL_RED = (214, 72, 46)        # brighter red for the wagon stencil (reads on the oxide-red planks)
BLACK = (34, 30, 28)
SOOT = (40, 36, 34)


def font(size, condensed=True, weight="Bold"):
    f = ImageFont.truetype(FONTS + "bahnschrift.ttf", size)
    try:
        f.set_variation_by_name(("%s Condensed" % weight) if condensed else weight)
    except Exception:
        pass
    return f


def noise(size, scale, seed):
    rng = np.random.default_rng(seed)
    w, h = max(2, size[0] // scale), max(2, size[1] // scale)
    small = Image.fromarray((rng.random((h, w)) * 255).astype("uint8"), "L")
    img = small.resize(size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(scale * 0.3))
    return np.asarray(img, dtype=np.float32) / 255.0


def weather(mask, seed, keep=0.8, drips=True):
    """Chip and thin the paint where coarse + fine noise is low; short drips under strokes."""
    m = np.asarray(mask, dtype=np.float32) / 255.0
    v = noise(mask.size, 60, seed) * 0.6 + noise(mask.size, 9, seed + 1) * 0.4
    v = (v - v.min()) / max(1e-6, v.max() - v.min())
    cover = np.clip((v - (1.0 - keep)) * 3.0, 0.0, 1.0)
    out = m * (0.55 + 0.45 * cover) * (v > 0.16)
    img = Image.fromarray((out * 255).astype("uint8"), "L")
    if drips:
        rng = random.Random(seed + 2)
        d = ImageDraw.Draw(img)
        mm = m > 0.5
        bbox = mask.getbbox()
        if bbox:
            for _ in range(int((bbox[2] - bbox[0]) / 30)):
                x = rng.randint(bbox[0], bbox[2] - 1)
                ys = np.nonzero(mm[:, x])[0]
                if len(ys) == 0:
                    continue
                y0 = int(ys.max())
                d.line([(x, y0), (x + rng.randint(-2, 2), y0 + rng.randint(8, 50))], fill=rng.randint(110, 210),
                       width=rng.randint(2, 4))
    return img.filter(ImageFilter.GaussianBlur(1.0))


def save(name, alpha, colour, bg=None, rough=217):
    """bg: an opaque board colour under the paint (signs); None = decal (paint only)."""
    w, h = alpha.size
    tone = noise((w, h), 30, 7)[..., None]
    paint = np.array(colour, np.float32)[None, None, :] * (0.78 + 0.22 * tone)
    a = np.asarray(alpha, np.float32)[..., None] / 255.0
    if bg is not None:
        grain = noise((w, h), 6, 3)[..., None] * 0.5 + noise((w, h), 80, 4)[..., None] * 0.5
        board = np.array(bg, np.float32)[None, None, :] * (0.72 + 0.4 * grain)
        rgb = board * (1 - a) + paint * a
        alpha_out = np.full((h, w), 255, np.uint8)
    else:
        rgb = paint
        alpha_out = (a[..., 0] * 255).astype(np.uint8)
    bca = Image.fromarray(np.clip(rgb, 0, 255).astype("uint8"), "RGB")
    bca.putalpha(Image.fromarray(alpha_out, "L"))
    bca.save(os.path.join(OUT, "T_OWClue_%s_BCA.png" % name))
    Image.merge("RGB", (Image.new("L", (w, h), rough), Image.new("L", (w, h), 0), Image.new("L", (w, h), 255))).save(
        os.path.join(OUT, "T_OWClue_%s_RMAO.png" % name))
    prev = Image.new("RGB", (w, h), (70, 58, 48))
    prev.paste(bca, (0, 0), bca)
    prev.convert("RGB").resize((w // 2, h // 2)).save(os.path.join(OUT, "preview_%s.jpg" % name), quality=85)
    print("wrote", name, alpha.getbbox())


def text_mask(w, h, lines, rot=0.0):
    """lines: [(text, y, size, condensed)]"""
    mask = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(mask)
    for text, y, size, cond in lines:
        d.text((w // 2, y), text, font=font(size, cond), fill=255, anchor="mm", stroke_width=max(2, size // 60),
               stroke_fill=255)
    if rot:
        mask = mask.rotate(rot, resample=Image.BICUBIC, center=(w // 2, h // 2))
    return mask


def quarantine():
    """КАРАНТИН over СЕКТОР ЛЕБУС, lime paint, wagon side (2:1, about 4 x 2 m)."""
    m = text_mask(2048, 1024, [("КАРАНТИН", 340, 400, True), ("СЕКТОР  ЛЕБУС", 760, 230, True)], rot=-1.2)
    d = ImageDraw.Draw(m)
    d.line([(260, 560), (1790, 548)], fill=255, width=20)
    save("Quarantine", weather(m, 11), LIME)


def grain():
    """ЗЕРНО ЗАРАЖЕНО / «ФИТОФАГ» / НЕ ВСКРЫВАТЬ: red, between two bars (2:1)."""
    m = text_mask(2048, 1024, [("ЗЕРНО  ЗАРАЖЕНО", 250, 210, True), ("«ФИТОФАГ»", 530, 300, True),
                               ("НЕ  ВСКРЫВАТЬ", 820, 180, True)], rot=0.8)
    d = ImageDraw.Draw(m)
    d.rectangle([(180, 90), (1870, 112)], fill=255)
    d.rectangle([(180, 940), (1870, 962)], fill=255)
    save("Grain", weather(m, 21, keep=0.8), STENCIL_RED)


def wagon_no():
    """Railway stencils: НКПС 41-7719, 20 т, 1946 (2:1, about 1.6 x 0.8 m), lime."""
    w, h = 1024, 512
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    d.text((40, 60), "НКПС", font=font(150, False), fill=255)
    d.text((40, 230), "41-7719", font=font(150, True), fill=255)
    d.text((640, 70), "20 т", font=font(120, True), fill=255)
    d.text((640, 260), "1946", font=font(120, True), fill=255)
    d.line([(600, 60), (600, 420)], fill=255, width=10)
    save("WagonNo", weather(m, 31, keep=0.7, drips=False), LIME)


def roof_x():
    """A big brushed cross with КАРАНТИН along it, for the roof (1:1)."""
    w = h = 1024
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    d.line([(90, 120), (930, 900)], fill=255, width=95)
    d.line([(930, 110), (90, 910)], fill=255, width=95)
    save("RoofX", weather(m, 41, keep=0.72, drips=False), LIME)


def burn():
    """СЖЕЧЬ / ПРИКАЗ 46/112, red-brown, crooked (2:1)."""
    m = text_mask(2048, 1024, [("СЖЕЧЬ", 380, 470, False), ("ПРИКАЗ  46/112", 790, 180, True)], rot=2.5)
    save("Burn", weather(m, 51, keep=0.7), RED)


def notice():
    """Typed paper order nailed to the door (3:4): yellowed paper, red star stamp, a signature."""
    w, h = 768, 1024
    rng = np.random.default_rng(5)
    paper = np.array((214, 204, 170), np.float32)[None, None, :] * (0.8 + 0.25 * noise((w, h), 40, 6)[..., None])
    stain = noise((w, h), 120, 9)[..., None]
    paper = paper * (1 - 0.35 * np.clip(stain - 0.55, 0, 1) * 2.5)
    img = Image.fromarray(np.clip(paper, 0, 255).astype("uint8"), "RGB")
    d = ImageDraw.Draw(img)
    tf = ImageFont.truetype(FONTS + "courbd.ttf", 30)
    hf = ImageFont.truetype(FONTS + "courbd.ttf", 44)
    d.text((w // 2, 90), "ПРИКАЗ № 46/112", font=hf, fill=(40, 34, 30), anchor="mm")
    lines = ["Сектор Лебус.", "Вагон опечатан.", "Груз: пшеница,", "урожай 1945 г.", "Заражение:", "«ФИТОФАГ».",
             "Людям опасности", "нет. Зерно -", "уничтожить.", "Вскрывать", "запрещено.", "", "Полковник"]
    for i, t in enumerate(lines):
        d.text((70, 170 + i * 52), t, font=tf, fill=(46, 40, 36))
    d.line([(330, 840), (600, 820), (520, 860), (690, 835)], fill=(40, 40, 90), width=4)
    d.ellipse([(500, 640), (700, 840)], outline=(160, 40, 34), width=8)
    d.text((600, 740), "★", font=ImageFont.truetype(FONTS + "seguisym.ttf", 90), fill=(160, 40, 34), anchor="mm")
    a = np.full((h, w), 255, np.uint8)
    edge = noise((w, h), 14, 8)
    yy, xx = np.mgrid[0:h, 0:w]
    border = np.minimum(np.minimum(xx, w - 1 - xx), np.minimum(yy, h - 1 - yy))
    a[(border < 18 + edge * 22)] = 0
    img.putalpha(Image.fromarray(a, "L"))
    img.save(os.path.join(OUT, "T_OWClue_Notice_BCA.png"))
    Image.merge("RGB", (Image.new("L", (w, h), 235), Image.new("L", (w, h), 0), Image.new("L", (w, h), 255))).save(
        os.path.join(OUT, "T_OWClue_Notice_RMAO.png"))
    prev = Image.new("RGB", (w, h), (70, 58, 48))
    prev.paste(img, (0, 0), img)
    prev.resize((w // 2, h // 2)).save(os.path.join(OUT, "preview_Notice.jpg"), quality=85)
    print("wrote Notice")


def sign(name, lines, bg, colour, border=None, ratio=(2048, 1024), seed=61):
    w, h = ratio
    m = text_mask(w, h, lines)
    if border:
        d = ImageDraw.Draw(m)
        d.rectangle([(40, 40), (w - 40, h - 40)], outline=255, width=border)
    save(name, weather(m, seed, keep=0.86, drips=False), colour, bg=bg, rough=200)


def crate():
    m = text_mask(1024, 512, [("ОСТОРОЖНО", 130, 130, True), ("СТЕКЛО · ПРОБЫ", 330, 120, True)])
    d = ImageDraw.Draw(m)
    d.polygon([(880, 380), (960, 470), (800, 470)], fill=255)
    save("Crate", weather(m, 71, keep=0.75, drips=False), BLACK)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    quarantine()
    grain()
    wagon_no()
    roof_x()
    burn()
    notice()
    sign("WaterWarn", [("ВНИМАНИЕ!", 300, 330, True), ("ВОДОЁМ  ЗАРАЖЁН", 700, 250, True)], (214, 206, 186), RED,
         border=40)
    sign("NoWater", [("КУПАТЬСЯ  И  БРАТЬ", 300, 200, True), ("ВОДУ  ЗАПРЕЩЕНО", 640, 260, True)], (196, 160, 52),
         BLACK, border=30, seed=63)
    sign("Station", [("ПУНКТ  ОТБОРА  ПРОБ", 380, 230, True), ("№ 3", 720, 300, False)], (70, 84, 66), LIME, seed=65)
    crate()
