"""Global ability icons on real in-game sky plates (2026-11-07 study).

The plates are SceneCapture renders of the game's own skies (sky_plates_render.py -> Saved/GlobalAbilityIcons/sky/plates).
This reuses compose_ability_icons.py unchanged and only swaps what it draws behind the units:
  * background(): a crop of a sky plate with a treatment (raw / graded to the faction / soft focus / ...)
  * the far-plane haze colour and the relight's sun + sky colours are measured from the plate, so the meshes are lit
    by the sky they sit in
  * optional light wrap: the sky behind bleeds a little over the cut-out edges (keeps them from looking pasted on)

Usage: python sky_plate_compose.py            writes Saved/GlobalAbilityIcons/sky/out/*.png and the study sheets
"""
import importlib.util
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# the same module object the round-3a/3b composers import, so patching it reaches their icons too
import compose_ability_icons as cab  # noqa: E402

SKY = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/sky/"
PLATES = SKY + "plates/"
OUT = SKY + "out/"
CANVAS = cab.CANVAS

ORIGINAL_OVER = cab.over
ORIGINAL_BACKGROUND = cab.background
ORIGINAL_RELIGHT = (cab.RELIGHT_SUN_RGB.copy(), cab.RELIGHT_SKY_RGB.copy(), cab.RELIGHT_GROUND_RGB.copy())

FACTION_GRADE = {
    # shadow, mid, highlight the plate's luminance is mapped onto
    "ger": ((0.10, 0.15, 0.21), (0.38, 0.47, 0.53), (0.93, 0.85, 0.70)),
    "sov": ((0.18, 0.10, 0.09), (0.55, 0.34, 0.26), (0.98, 0.80, 0.56)),
}


def luminance(rgb):
    return rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)


def load_plate(name, crop=None, flip=False):
    """crop = (x, y, size) in the 1024 plate; None = the whole plate."""
    img = Image.open(PLATES + name + ".png").convert("RGB")
    if crop:
        x, y, s = crop
        img = img.crop((x, y, x + s, y + s))
    if flip:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    img = img.resize((CANVAS, CANVAS), Image.LANCZOS)
    return np.asarray(img).astype(np.float32) / 255


def treat(rgb, treatment, faction):
    """The ways the plate is turned into a card background."""
    if treatment == "raw":
        return rgb
    lum = luminance(rgb)[..., None]
    if treatment in ("grade", "grade_soft"):
        # keep the clouds' light and shape, take the colours from the faction
        lo, mid, hi = (np.array(c, np.float32) for c in FACTION_GRADE[faction])
        t = np.clip(lum, 0, 1)
        mapped = np.where(t < 0.5, lo + (mid - lo) * (t / 0.5), mid + (hi - mid) * ((t - 0.5) / 0.5))
        out = rgb * 0.35 + mapped * 0.65
        if treatment == "grade_soft":
            out = cab.blur_float(out, CANVAS * 0.006)
        return np.clip(out, 0, 1)
    if treatment == "soft":
        # a long lens: the sky slightly out of focus and a touch darker, so the aircraft carry the card
        out = cab.blur_float(rgb, CANVAS * 0.008)
        grey = luminance(out)[..., None]
        out = grey + (out - grey) * 0.8
        return np.clip(out * 0.9, 0, 1)
    if treatment == "deep":
        # darker top, the sky's own light kept low: the dusk look of the old cards but from the real sky
        v = (cab.ys / CANVAS)[..., None]
        out = rgb * (0.55 + 0.45 * cab.smooth(0.0, 0.9, v))
        grey = luminance(out)[..., None]
        out = grey + (out - grey) * 0.85
        return np.clip(out, 0, 1)
    if treatment == "mono":
        # monochrome photograph with a faint warm tone
        tone = np.array([1.04, 1.0, 0.92], np.float32)
        return np.clip(lum * tone * 1.02, 0, 1)
    raise ValueError(treatment)


def match_light_to_plate(rgb):
    """The relight's sun = the brightest part of the sky, its sky fill = the upper half, its bounce = a darker sun."""
    lum = luminance(rgb)
    hot = rgb[lum >= np.quantile(lum, 0.97)].mean(axis=0)
    upper = rgb[: CANVAS // 2].reshape(-1, 3).mean(axis=0)
    sun = hot / max(hot.max(), 1e-3)
    sky = upper / max(upper.max(), 1e-3) * 0.68
    # do not let a grey plate make the sun white or a green plate make it green: lean on the old warm values
    cab.RELIGHT_SUN_RGB = (sun * 0.6 + ORIGINAL_RELIGHT[0] * 0.4).astype(np.float32)
    cab.RELIGHT_SKY_RGB = (sky * 0.6 + ORIGINAL_RELIGHT[1] * 0.4).astype(np.float32)
    cab.RELIGHT_GROUND_RGB = (cab.RELIGHT_SUN_RGB * 0.5).astype(np.float32)
    cab.CUTOUT_CACHE.clear()
    haze = tuple(int(c) for c in (rgb[CANVAS // 4: CANVAS * 3 // 4].reshape(-1, 3).mean(axis=0) * 255))
    cab.SKY_GER = haze
    cab.SKY_SOV = haze


def light_wrap_over(strength):
    def over(base, layer):
        out = ORIGINAL_OVER(base, layer)
        a = layer[..., 3]
        if strength <= 0 or a.max() < 0.05:
            return out
        edge = np.clip(a - cab.blur_float(a, 5.0) * 1.0, 0, 1) + np.clip(a * (1 - cab.blur_float(a, 9.0)), 0, 1)
        edge = np.clip(edge, 0, 1)[..., None]
        glow = cab.blur_float(base[..., :3], 10.0)
        out[..., :3] = np.clip(out[..., :3] + (glow - out[..., :3]) * edge * strength, 0, 1)
        return out
    return over


def set_look(plate, crop=None, flip=False, treatment="raw", faction="ger", wrap=0.35, match_light=True, rim=0.0,
             grain=1.0):
    rgb = treat(load_plate(plate, crop, flip), treatment, faction)
    globals()["faction"] = faction

    def background(_faction, _seed, ground=True, horizon=cab.HORIZON):
        img = np.ones((CANVAS, CANVAS, 4), np.float32)
        img[..., :3] = rgb
        if not ground:
            return img
        # ground icons: the plate is the sky down to the horizon (crop it so its own horizon sits there); below it the
        # drawn ground, lit by the plate's horizon colour, so the map edge of the cinematic never shows
        drawn_bg = ORIGINAL_BACKGROUND(_faction, _seed, ground=True, horizon=horizon)
        v = (cab.ys / CANVAS)[..., None]
        band = rgb[int(CANVAS * (horizon - 0.04)):int(CANVAS * horizon)].reshape(-1, 3).mean(axis=0)
        # the drawn ground's light pattern (darker towards the viewer, field patches) in the sky's colours: the plate's
        # horizon colour mixed with a neutral earth, so a cold sky gets a cold field and a warm one a warm field
        drawn_lum = luminance(drawn_bg[..., :3])[..., None]
        far = drawn_lum[int(CANVAS * horizon) + 4:int(CANVAS * horizon) + 40].mean()
        earth = np.array([0.42, 0.38, 0.30], np.float32)
        tone = band / max(luminance(band), 1e-3) * 0.55 + earth / luminance(earth) * 0.45
        patches = (cab.noise(_seed + 7, CANVAS * 0.12, 4)[..., None] - 0.5) * 0.22
        ground_rgb = np.clip(tone * drawn_lum / max(far, 1e-3) * min(luminance(band), 0.6) * 0.62 * (1 + patches), 0, 1)
        is_ground = cab.smooth(horizon - 0.002, horizon + 0.004, v)
        img[..., :3] = rgb * (1 - is_ground) + ground_rgb * is_ground
        haze = np.exp(-((v - horizon) / 0.018) ** 2) * 0.5
        img[..., :3] = img[..., :3] + (band - img[..., :3]) * haze
        # keep the drawn tree line
        rng = np.random.default_rng(_seed + 2)
        heights = np.interp(np.arange(CANVAS), np.arange(0, CANVAS + 9, 9), rng.random(CANVAS // 9 + 2)) * 0.012 + 0.003
        trees = ((v[..., 0] > horizon - heights[None, :]) & (v[..., 0] < horizon + 0.004))[..., None]
        img[..., :3] = np.where(trees, img[..., :3] * 0.42 + band * 0.08, img[..., :3])
        return img

    cab.background = background
    if match_light:
        match_light_to_plate(rgb)
    else:
        cab.RELIGHT_SUN_RGB, cab.RELIGHT_SKY_RGB, cab.RELIGHT_GROUND_RGB = (c.copy() for c in ORIGINAL_RELIGHT)
        cab.CUTOUT_CACHE.clear()
        cab.SKY_GER, cab.SKY_SOV = (112, 134, 146), (150, 110, 96)
    cab.over = light_wrap_over(wrap)


ICONS = {
    "strafe": lambda: cab.ju87_strafe(3),
    "drop": lambda: cab.ju390_drop(("FlakLuchs", "FlakLuchs", "StugIII"), 89),
    "para": cab.ICONS["GerParatroopers"],
    "sovdrop": cab.ICONS["SovietT34Drop"],
}


def render(look, icon):
    set_look(**look)
    return cab.ICONS_OVERRIDE[icon]() if hasattr(cab, "ICONS_OVERRIDE") else ICONS[icon]()


def label_sheet(items, columns, cell=300, title=None):
    """items = [(PIL image, caption)]; each card is shown big and at the real 96 px button size."""
    rows = (len(items) + columns - 1) // columns
    top = 40 if title else 0
    sheet = Image.new("RGB", (columns * (cell + 12) + 12, top + rows * (cell + 130) + 12), (24, 24, 26))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 15)
        big = ImageFont.truetype("arialbd.ttf", 20)
    except OSError:
        font = big = ImageFont.load_default()
    if title:
        draw.text((14, 10), title, fill=(230, 230, 230), font=big)
    for i, (img, caption) in enumerate(items):
        x = 12 + (i % columns) * (cell + 12)
        y = top + 12 + (i // columns) * (cell + 130)
        card = img.convert("RGBA")
        sheet.paste(card.resize((cell, cell), Image.LANCZOS), (x, y), card.resize((cell, cell), Image.LANCZOS))
        small = card.resize((96, 96), Image.LANCZOS)
        sheet.paste(small, (x, y + cell + 6), small)
        small = card.resize((64, 64), Image.LANCZOS)
        sheet.paste(small, (x + 104, y + cell + 6), small)
        draw.text((x + 176, y + cell + 8), caption, fill=(220, 220, 220), font=font)
    return sheet


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    looks = json.load(open(SKY + os.environ.get("LOOKS", "looks.json"), encoding="utf-8"))
    which = sys.argv[1:] or list(looks)
    for name in which:
        look = looks[name]
        icon = look.pop("icon")
        img = render(look, icon)
        img.save(OUT + name + ".png")
        print("ICON", name)
