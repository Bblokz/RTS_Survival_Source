"""Engraves instrument-placard badges (enamel_handmade.instrument_placard, colours from placard_colours) onto clean
256x256 unit portraits: class icon bottom left, role icon bottom right. Replaces the old blue neon glyphs of
UnitPortraits/engrave_icons.py; the original portraits and icons are only read.

Plan file, one line per in-game texture:
  <UE texture path>=<clean portrait png>|<left icon stem or ->|<right icon stem or ->
  /Game/RTS_Survival/Textures/GameUI/UnitIcons/LightTanks/T_Ger_LightTank_PanzerII_F=D:/UE_Assets/Icons/TrainingUI/GerTanks/Light/NoIcon/T_Ger_LightTank_PanzerII_F.png|T_LightTankClass_Icon_V2|T_AutoCannon_Class_V2
The portrait may be written inpaint:<iconed png> when no clean version exists; the neon glyphs in the bottom band are
then painted out first. Icon stems are Set08_FlatStencil file names (without .png) listed in placard_colours.GROUPS.

Usage:
  python placard_portraits.py <plan file> [<plan file> ...] [--out D:/UE_Assets/Icons/TrainingUI/Placard] [--sheet x.png]
Writes <out>/<path below /UnitIcons/>.png and a contact sheet.
"""
import argparse
import sys
import zlib
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "UnitPortraits"))
import enamel_handmade as eh  # noqa: E402
import icon_backgrounds as ib  # noqa: E402
from placard_colours import recommended_colour  # noqa: E402

DEFAULT_OUT = Path("D:/UE_Assets/Icons/TrainingUI/Placard")
UNIT_ICONS_ROOT = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/"
SIZE = ib.PORTRAIT_SIZE
# Neon glyphs sit in the lower band (UnitPortraits/engrave_icons.py GLYPH_TOP); painted out for inpaint: sources.
GLYPH_TOP = 150
GLYPH_BLUE_MARGIN = 40
GLYPH_WHITE_LEVEL = 215
GLYPH_GROW = 5
INPAINT_RADIUS = 7


def read_plan(path):
    # utf-8-sig: PowerShell writes plan files with a BOM.
    for number, line in enumerate(Path(path).read_text(encoding="utf-8-sig").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        texture, spec = (part.strip() for part in line.split("=", 1))
        portrait, left, right = (part.strip() for part in spec.split("|"))
        yield texture, portrait, left, right, f"{Path(path).name}:{number}"


def fit_width(image):
    """256 wide, source aspect kept: a few live icons are not square (e.g. 256x181) and must not be stretched."""
    height = max(1, round(image.height * SIZE / image.width))
    return image.resize((SIZE, height), Image.LANCZOS)


def load_portrait(spec):
    if not spec.startswith("inpaint:"):
        return fit_width(Image.open(spec).convert("RGBA"))
    iconed = np.asarray(fit_width(Image.open(spec[len("inpaint:"):]).convert("RGB")))
    red, green, blue = (iconed[..., c].astype(np.int32) for c in range(3))
    glyph = (blue > np.maximum(red, green) + GLYPH_BLUE_MARGIN) | (iconed.min(axis=2) > GLYPH_WHITE_LEVEL)
    glyph[:max(0, iconed.shape[0] - (SIZE - GLYPH_TOP))] = False
    kernel = np.ones((GLYPH_GROW * 2 + 1, GLYPH_GROW * 2 + 1), np.uint8)
    mask = cv2.dilate(glyph.astype(np.uint8), kernel) * 255
    return Image.fromarray(cv2.inpaint(iconed, mask, INPAINT_RADIUS, cv2.INPAINT_TELEA)).convert("RGBA")


def badge(icon_stem, texture, side):
    seed = zlib.crc32(f"{texture}|{side}".encode()) % 100000
    plate = eh.instrument_placard(ib.ICON_SET / f"{icon_stem}.png", recommended_colour(icon_stem), seed)
    return plate.resize((ib.BADGE_SIZE, ib.BADGE_SIZE), Image.LANCZOS)


def engrave(texture, portrait_spec, left, right):
    picture = load_portrait(portrait_spec)
    for icon_stem, side in ((left, "left"), (right, "right")):
        if icon_stem and icon_stem != "-":
            picture = eh.place_seated(picture, badge(icon_stem, texture, side), side)
    return picture.convert("RGB")


def output_path(out_root, texture):
    relative = texture[len(UNIT_ICONS_ROOT):] if texture.startswith(UNIT_ICONS_ROOT) else texture.rsplit("/", 1)[-1]
    return out_root / f"{relative}.png"


def write_sheet(entries, path, columns=6):
    cell, label_h = 256, 34
    rows = (len(entries) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * (cell + 8) + 8, rows * (cell + label_h) + 8), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    for index, (texture, picture, left, right) in enumerate(entries):
        x = 8 + (index % columns) * (cell + 8)
        y = 8 + (index // columns) * (cell + label_h)
        sheet.paste(picture, (x, y + cell - picture.height))
        small = picture.resize((75, max(1, round(picture.height * 75 / picture.width))), Image.LANCZOS)
        sheet.paste(small, (x + cell - 75, y))
        name = texture.rsplit("/", 1)[-1].replace("T_Ger_", "")
        draw.text((x + 2, y + cell + 2), name, fill=(235, 232, 220), font=ib.font(12))
        icons = f"{left.replace('T_', '').split('Class')[0]} | {right.replace('T_', '').split('Class')[0]}"
        draw.text((x + 2, y + cell + 17), icons, fill=(150, 148, 140), font=ib.font(11))
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("plans", nargs="+")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--sheet", type=Path)
    args = parser.parse_args()
    entries, failures = [], []
    for plan in args.plans:
        for texture, portrait, left, right, where in read_plan(plan):
            try:
                picture = engrave(texture, portrait, left, right)
            except (OSError, KeyError, ValueError) as error:
                failures.append(f"{where} {texture}: {error}")
                continue
            target = output_path(args.out, texture)
            target.parent.mkdir(parents=True, exist_ok=True)
            picture.save(target)
            entries.append((texture, picture, left, right))
            print(f"icon {target}")
    if args.sheet and entries:
        write_sheet(entries, args.sheet)
        print(f"sheet {args.sheet}")
    for failure in failures:
        print(f"FAIL {failure}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
