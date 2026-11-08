"""Turns the full-scene renders of render_training_icons.py into 256x256 squad training icons in the live style
(T_Ger_Portrait_*: dark warm dusk portrait, blue role glyph bottom left) and writes a contact sheet.

    python compose_training_icons.py            # all ICONS
    python compose_training_icons.py --sheet-only

Grading: each channel's histogram is matched to the pooled histogram of the live NoIcon portraits (their warm, dark dusk
look), blended with the render by GRADE_STRENGTH so the unit keeps its own colours. Glyphs come from
extract_squad_glyphs.py (pixel copies of the live glyphs) or UnitPortraits/glyphs (role_rail, moved to the left).
"""
import argparse
import os
from pathlib import Path

os.environ.setdefault("OPENCV_IO_ENABLE_OPENEXR", "1")
import cv2  # noqa: E402

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SAVED = Path("D:/UE5Projects/RTS_Survival/Saved/TrainingIcons")
RENDERS = SAVED / "renders"
GLYPHS = SAVED / "glyphs"
TANK_GLYPHS = Path("D:/UE5Projects/RTS_Survival/Source/Tools/UnitPortraits/glyphs")
OUT = SAVED / "icons"
REFERENCE_DIR = Path("D:/UE_Assets/Icons/TrainingUI/GerSquads/NoIcon")
LIVE_ICONED = Path("D:/UE_Assets/Icons/TrainingUI/GerSquads")
SIZE = 256
UI_SIZE = 75  # BP_W_TrainingItem M_TrainingItemSizeBox at 1080p
GRADE_STRENGTH = 0.85
SKY_DARK = 0.25
SKY_FADE_END = 0.6
VIGNETTE = 0.45
BALANCE_STRENGTH = 0.7
BG_DIM = 0.5
BG_DESATURATE = 0.15
SUBJECT_GAIN = 1.05
SKY_WARM = 0.7
WARM_TINT = (1.35, 0.85, 0.5)

# Scene captures here ignore the exposure bias, so the renders are linear HDR (EXR) exposed and tonemapped here.
EXPOSURE = 4.0
# icon stem: (render file stem, crop box in the 1024 render as fractions (l, t, r, b) or None = whole, glyph, glyph shift)
ICONS = {
    "T_Ger_Squad_EliteScavengers": ("EliteScavengers_finalhdr_lumen", None, "squad_rocket", (0, 0)),
    "T_Ger_Squad_RedSpine": ("RedSpine_finalhdr_lumen", None, "squad_bullets", (0, 0)),
    "T_Ger_Squad_FelsKrieger": ("FelsKrieger_finalhdr_lumen", None, "squad_skull", (0, 0)),
    "T_Ger_Squad_RailTroopers": ("RailTroopers_finalhdr_lumen", None, "role_rail", (-150, 0)),
}
# Live icons shown next to the new ones on the contact sheet.
LIVE = ["T_Ger_Portrait_PanzerGrenadier.png", "T_Ger_Portrait_Scavengers.PNG", "T_Ger_Portrait_Vulture.PNG",
        "T_Ger_Portrait_SturmPioneer.PNG", "T_Ger_Portrait_SturmKommando.PNG", "T_Ger_Portrait_LightBringer.PNG"]


def reference_cdfs():
    pooled = [np.zeros(256) for _ in range(3)]
    for path in REFERENCE_DIR.glob("*"):
        pixels = np.asarray(Image.open(path).convert("RGB").resize((SIZE, SIZE)), dtype=np.uint8)
        for c in range(3):
            pooled[c] += np.bincount(pixels[..., c].ravel(), minlength=256)
    return [np.cumsum(h) / h.sum() for h in pooled]


def match_histograms(image, cdfs):
    out = np.empty_like(image, dtype=np.float32)
    for c in range(3):
        channel = image[..., c]
        hist = np.bincount(channel.ravel(), minlength=256)
        src_cdf = np.cumsum(hist) / hist.sum()
        lut = np.interp(src_cdf, cdfs[c], np.arange(256))
        out[..., c] = lut[channel]
    return out


def load_glyph(name, shift):
    path = GLYPHS / f"{name}.png"
    if not path.exists():
        path = TANK_GLYPHS / f"{name}.png"
    glyph = Image.open(path).convert("RGBA").resize((SIZE, SIZE), Image.LANCZOS)
    if shift != (0, 0):
        moved = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        moved.paste(glyph, shift, glyph)
        glyph = moved
    return glyph


def load_render(render_stem):
    """EXR: exposure + ACES fit + sRGB; PNG: as is."""
    exr = RENDERS / f"{render_stem}.exr"
    if not exr.exists():
        return Image.open(RENDERS / f"{render_stem}.png").convert("RGB")
    linear = cv2.imread(str(exr), cv2.IMREAD_UNCHANGED)[..., :3][..., ::-1].astype(np.float32) * EXPOSURE
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    mapped = np.clip((linear * (a * linear + b)) / (linear * (c * linear + d) + e), 0.0, 1.0)
    srgb = np.where(mapped <= 0.0031308, 12.92 * mapped, 1.055 * np.power(mapped, 1 / 2.4) - 0.055)
    return Image.fromarray((np.clip(srgb, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")


def reference_lab_stats():
    """Mean and spread per Lab channel of the live NoIcon portraits (their dark warm dusk look)."""
    pixels = []
    for path in REFERENCE_DIR.glob("*"):
        rgb = np.asarray(Image.open(path).convert("RGB").resize((SIZE, SIZE)), dtype=np.uint8)
        pixels.append(cv2.cvtColor(rgb, cv2.COLOR_RGB2LAB).reshape(-1, 3).astype(np.float32))
    pooled = np.concatenate(pixels)
    rgb_mean = np.mean([np.asarray(Image.open(p).convert("RGB").resize((SIZE, SIZE)), dtype=np.float32)
                        .reshape(-1, 3).mean(0) for p in REFERENCE_DIR.glob("*")], axis=0)
    return pooled.mean(0), pooled.std(0), rgb_mean


def dusk(rgb):
    """The live portraits are night-dark at the top (sky) and lit around the soldiers: darken the upper part and
    the corners before the colour transfer."""
    h, w = rgb.shape[:2]
    rows = np.linspace(0.0, 1.0, h)[:, None]
    sky = np.clip(SKY_DARK + (1.0 - SKY_DARK) * (rows / SKY_FADE_END), SKY_DARK, 1.0)
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.sqrt(((xx - w * 0.55) / (w * 0.75)) ** 2 + ((yy - h * 0.55) / (h * 0.75)) ** 2)
    vignette = np.clip(1.0 - VIGNETTE * r ** 2, 0.0, 1.0)
    return rgb * (sky * vignette)[..., None]


def color_transfer(rgb_u8, stats):
    mean_ref, std_ref = stats[0], stats[1]
    lab = cv2.cvtColor(rgb_u8, cv2.COLOR_RGB2LAB).astype(np.float32)
    flat = lab.reshape(-1, 3)
    mean, std = flat.mean(0), flat.std(0) + 1e-3
    out = (lab - mean) / std * std_ref + mean_ref
    return cv2.cvtColor(np.clip(out, 0, 255).astype(np.uint8), cv2.COLOR_LAB2RGB)


def subject_mask(render_stem, size):
    """1 on the soldiers, 0 elsewhere, from <job>_mask.png (alpha = 1 - coverage of the soldiers-only HDR pass)."""
    job = render_stem.split("_")[0]
    path = RENDERS / f"{job}_mask.png"
    if not path.exists():
        return None
    alpha = np.asarray(Image.open(path).convert("RGBA"), dtype=np.float32)[..., 3] / 255.0
    mask = cv2.resize(1.0 - alpha, (size, size), interpolation=cv2.INTER_AREA)
    # pull the edge in by a pixel so no lit background seam (a fake rim light) survives around the soldiers
    mask = cv2.erode(mask, np.ones((3, 3), np.uint8))
    return cv2.GaussianBlur(mask, (0, 0), 0.7)


def warm_sky(rgb):
    """The colour transfer can turn the dark sky navy; the live skies are near-black brown."""
    h = rgb.shape[0]
    weight = np.clip(1.0 - np.linspace(0.0, 1.0, h) / SKY_FADE_END, 0.0, 1.0)[:, None, None] * SKY_WARM
    lum = rgb.mean(axis=2, keepdims=True)
    warm = lum * np.array(WARM_TINT, dtype=np.float32)
    return rgb * (1 - weight) + warm * weight


def compose(stem, spec, stats):
    render_stem, crop, glyph_name, shift = spec
    render = load_render(render_stem)
    if crop:
        w, h = render.size
        render = render.crop((int(crop[0] * w), int(crop[1] * h), int(crop[2] * w), int(crop[3] * h)))
    small = np.asarray(render.resize((SIZE, SIZE), Image.LANCZOS), dtype=np.float32)
    mask = subject_mask(render_stem, SIZE)
    if mask is not None:
        # the live portraits light the soldier and let the scenery fall off into the dark
        grey = small.mean(axis=2, keepdims=True)
        background = (small * (1 - BG_DESATURATE) + grey * BG_DESATURATE) * BG_DIM
        small = small * SUBJECT_GAIN * mask[..., None] + background * (1 - mask[..., None])
    darkened = np.clip(dusk(small), 0, 255).astype(np.uint8)
    graded = color_transfer(darkened, stats).astype(np.float32)
    mixed = warm_sky(darkened * (1 - GRADE_STRENGTH) + graded * GRADE_STRENGTH)
    # final white balance: the live portraits' channel means (warm, little blue)
    gains = (np.array(stats[2]) / np.maximum(mixed.reshape(-1, 3).mean(0), 1.0)) ** BALANCE_STRENGTH
    mixed = np.clip(mixed * gains, 0, 255).astype(np.uint8)
    picture = Image.fromarray(mixed, "RGB").filter(ImageFilter.UnsharpMask(radius=1.2, percent=40, threshold=2))
    plain = picture.copy()
    if glyph_name:
        picture = Image.alpha_composite(picture.convert("RGBA"), load_glyph(glyph_name, shift)).convert("RGB")
    return plain, picture


def write_sheet(new_icons):
    live = [(name.split(".")[0].replace("T_Ger_Portrait_", "live "), Image.open(LIVE_ICONED / name).convert("RGB")
             .resize((SIZE, SIZE), Image.LANCZOS)) for name in LIVE]
    new = [(stem.replace("T_Ger_Squad_", "NEW "), picture) for stem, picture in new_icons]
    entries = new + live
    label = 18
    columns = 5
    rows = (len(entries) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * (SIZE + 8), rows * (SIZE + label + 8) + UI_SIZE * 2 + 40), (22, 22, 22))
    draw = ImageDraw.Draw(sheet)
    for index, (name, picture) in enumerate(entries):
        x = (index % columns) * (SIZE + 8)
        y = (index // columns) * (SIZE + label + 8)
        sheet.paste(picture, (x, y))
        draw.text((x + 4, y + SIZE + 3), name, fill=(235, 235, 120) if name.startswith("NEW") else (220, 220, 220))
    # UI size row: every icon at the 75 px training button size, new ones first
    y = rows * (SIZE + label + 8) + 10
    draw.text((4, y), "at training-button size (75 px, 1080p):", fill=(220, 220, 220))
    for index, (name, picture) in enumerate(entries):
        sheet.paste(picture.resize((UI_SIZE, UI_SIZE), Image.LANCZOS), (4 + index * (UI_SIZE + 6), y + 18))
    path = SAVED / "contact_sheet.png"
    sheet.save(path)
    print("SHEET", path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sheet-only", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stats = reference_lab_stats()
    new_icons = []
    for stem, spec in ICONS.items():
        if args.sheet_only:
            path = OUT / f"{stem}.png"
            if path.exists():
                new_icons.append((stem, Image.open(path).convert("RGB")))
            continue
        if not any((RENDERS / f"{spec[0]}{ext}").exists() for ext in (".exr", ".png")):
            print("MISSING render", spec[0])
            continue
        plain, picture = compose(stem, spec, stats)
        plain.save(OUT / f"{stem}_NoIcon.png")
        picture.save(OUT / f"{stem}.png")
        new_icons.append((stem, picture))
        print("ICON", stem)
    write_sheet(new_icons)


if __name__ == "__main__":
    main()
