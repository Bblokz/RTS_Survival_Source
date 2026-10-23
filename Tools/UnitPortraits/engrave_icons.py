"""Engraves the tactical glyphs (class symbol bottom left, role glyph bottom right) on 256x256 unit portraits.

The glyphs were painted by hand on the existing training icons. This tool lifts them back out: where a portrait
exists with and without its glyphs (GerTanks/<class>/ and GerTanks/<class>/NoIcon/), the difference gives each glyph
pixel's colour and opacity; for a glyph that only exists on an iconed picture, the background under it is inpainted
first. Each glyph keeps the place it was painted at, so engraved icons match the hand-made ones.

Usage:
  python engrave_icons.py extract                      # writes glyphs/<name>.png (RGBA) from GLYPH_SOURCES
  python engrave_icons.py engrave <plan file> <out dir> [--sheet <sheet.png>]

Plan file lines: <output stem>=<portrait png>|<left glyph or ->|<right glyph or ->
"""
import argparse
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

ICONS = Path("D:/UE_Assets/Icons/TrainingUI/GerTanks")
GLYPH_DIR = Path(__file__).resolve().parent / "glyphs"
ICON_SIZE = 256
# Glyphs sit in the lower 100 px; the left half holds the class symbol, the right half the role glyph.
GLYPH_TOP = 150
HALF = ICON_SIZE // 2
# Summed RGB difference below which a pixel counts as untouched background (JPEG-free PNGs, so this is small).
BACKGROUND_TOLERANCE = 24
# Iconed-only sources: glyph pixels are cold blue or near white; scenery is warm or grey.
GLYPH_BLUE_MARGIN = 40
GLYPH_WHITE_LEVEL = 215
GLYPH_MASK_GROW = 5
INPAINT_RADIUS = 7

# name: (iconed picture, matching NoIcon picture or None, side)
GLYPH_SOURCES = {
    "class_light": ("ArmoredCars/T_Ger_ArmoredCar_Puma.png", "ArmoredCars/NoIcon/T_Ger_ArmoredCar_Puma.png", "left"),
    "class_medium": ("Medium/T_Ger_MediumTank_PZIV_G.png", "Medium/NoIcon/T_Ger_MediumTank_PZIV_G.png", "left"),
    "class_heavy": ("Heavy/T_Ger_HeavyTank_PantherD.png", "Heavy/NoIcon/T_Ger_HeavyTank_PantherD.png", "left"),
    "class_tank_hunter": ("Light/T_Ger_LightTank_JagdPz.png", "Light/NoIcon/T_Ger_LightTank_JagdPz.png", "left"),
    "class_artillery": ("Light/T_Ger_LightTank_Panzer1_150MM.png", "Light/NoIcon/T_Ger_LightTank_Panzer1_150MM.png",
                        "left"),
    "role_autocannon": ("Light/T_Ger_LightTank_PanzerII_F.png", "Light/NoIcon/T_Ger_LightTank_PanzerII_F.png", "right"),
    "role_anti_air": ("Medium/T_Ger_MediumTank_PZIIIAA.png", "Medium/NoIcon/T_Ger_MediumTank_PZIIIAA.png", "right"),
    "role_rockets": ("Heavy/T_Ger_HeavyTank_Jagdpanther.png", "Heavy/NoIcon/T_Ger_HeavyTank_Jagdpanther.png", "right"),
    "role_rail": ("Medium/T_Ger_MediumTank_PZIV_Rail.png", None, "right"),
}


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB").resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS),
                      dtype=np.float32)


def side_mask(side: str) -> np.ndarray:
    mask = np.zeros((ICON_SIZE, ICON_SIZE), dtype=bool)
    columns = slice(0, HALF) if side == "left" else slice(HALF, ICON_SIZE)
    mask[GLYPH_TOP:, columns] = True
    return mask


def inpaint_background(iconed: np.ndarray, region: np.ndarray) -> np.ndarray:
    red, green, blue = iconed[..., 0], iconed[..., 1], iconed[..., 2]
    is_cold = blue > np.maximum(red, green) + GLYPH_BLUE_MARGIN
    is_white = iconed.min(axis=2) > GLYPH_WHITE_LEVEL
    glyph = ((is_cold | is_white) & region).astype(np.uint8)
    kernel = np.ones((GLYPH_MASK_GROW * 2 + 1, GLYPH_MASK_GROW * 2 + 1), np.uint8)
    glyph = cv2.dilate(glyph, kernel)
    filled = cv2.inpaint(iconed.astype(np.uint8), glyph * 255, INPAINT_RADIUS, cv2.INPAINT_TELEA)
    return filled.astype(np.float32)


def matte(iconed: np.ndarray, background: np.ndarray, region: np.ndarray) -> np.ndarray:
    """Least-opaque glyph that explains the picture: the glyph colour is pushed along background->picture to the
    edge of the colour cube, so glow edges come out as a bright colour at low opacity."""
    delta = iconed - background
    with np.errstate(divide="ignore", invalid="ignore"):
        room = np.where(delta > 0, (255.0 - background) / delta, np.where(delta < 0, -background / delta, np.inf))
    is_glyph = region & (np.abs(delta).sum(axis=2) > BACKGROUND_TOLERANCE)
    # Untouched pixels have no direction to stretch along (inf times zero); they stay background at zero opacity.
    stretch = np.where(is_glyph, np.clip(room.min(axis=2), 1.0, None), 1.0)
    alpha = np.where(is_glyph, 1.0 / stretch, 0.0)
    colour = np.clip(background + delta * stretch[..., None], 0, 255)
    rgba = np.dstack([colour, alpha * 255.0])
    return np.clip(rgba, 0, 255).astype(np.uint8)


def extract_glyphs():
    GLYPH_DIR.mkdir(exist_ok=True)
    for name, (iconed_name, plain_name, side) in GLYPH_SOURCES.items():
        iconed = load_rgb(ICONS / iconed_name)
        region = side_mask(side)
        background = load_rgb(ICONS / plain_name) if plain_name else inpaint_background(iconed, region)
        Image.fromarray(matte(iconed, background, region), "RGBA").save(GLYPH_DIR / f"{name}.png")
        print(f"glyph {name} from {iconed_name}")


def engrave(portrait: Path, glyph_names) -> Image.Image:
    picture = Image.open(portrait).convert("RGBA").resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    for glyph_name in glyph_names:
        if glyph_name and glyph_name != "-":
            picture = Image.alpha_composite(picture, Image.open(GLYPH_DIR / f"{glyph_name}.png"))
    return picture.convert("RGB")


def read_plan(plan_file: Path):
    for line in plan_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        stem, spec = line.split("=", 1)
        portrait, left, right = spec.split("|")
        yield stem.strip(), Path(portrait.strip()), (left.strip(), right.strip())


def write_sheet(entries, sheet_path: Path):
    columns = 6
    rows = (len(entries) + columns - 1) // columns
    label_height = 16
    sheet = Image.new("RGB", (columns * ICON_SIZE, rows * (ICON_SIZE + label_height)), (24, 24, 24))
    draw = ImageDraw.Draw(sheet)
    for index, (stem, picture) in enumerate(entries):
        x = (index % columns) * ICON_SIZE
        y = (index // columns) * (ICON_SIZE + label_height)
        sheet.paste(picture, (x, y))
        draw.text((x + 4, y + ICON_SIZE + 2), stem.replace("T_Ger_", ""), fill=(235, 235, 235))
    sheet.save(sheet_path)


def engrave_plan(plan_file: Path, out_dir: Path, sheet_path):
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for stem, portrait, glyph_names in read_plan(plan_file):
        picture = engrave(portrait, glyph_names)
        picture.save(out_dir / f"{stem}.png")
        entries.append((stem, picture))
        print(f"icon {stem} <- {portrait.name} {glyph_names}")
    if sheet_path:
        write_sheet(entries, Path(sheet_path))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("extract")
    engrave_parser = sub.add_parser("engrave")
    engrave_parser.add_argument("plan", type=Path)
    engrave_parser.add_argument("out_dir", type=Path)
    engrave_parser.add_argument("--sheet")
    args = parser.parse_args()
    if args.command == "extract":
        extract_glyphs()
    else:
        engrave_plan(args.plan, args.out_dir, args.sheet)


if __name__ == "__main__":
    main()
