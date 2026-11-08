"""Review sheet for the placard colour coding: every unit icon as an instrument placard in every palette colour, the
recommended colour framed, and a strip with the whole recommended set (plus a Pz II F at game size).

    python placard_colour_matrix.py   -> Saved/TacticalIconBackgrounds/placard/PlacardColours_sheet.png
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import enamel_handmade as eh  # noqa: E402
import icon_backgrounds as ib  # noqa: E402
from placard_colours import GROUPS, PALETTE  # noqa: E402

OUT = ib.OUT / "placard"
CELL = 84
GAP = 10
LABEL_W = 250
CACHE = OUT / "cells"


def placard(stem, colour_name, seed):
    path = CACHE / f"{stem}__{colour_name.replace(' ', '_')}.png"
    if path.exists():
        return Image.open(path)
    badge = eh.instrument_placard(ib.ICON_SET / f"{stem}.png", PALETTE[colour_name], seed)
    badge = badge.resize((ib.BADGE_SIZE, ib.BADGE_SIZE), Image.LANCZOS)
    badge.save(path)
    return badge


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    colours = list(PALETTE)
    entries = [(group, e) for group, items in GROUPS for e in items]
    font, small, title = ib.font(14), ib.font(12), ib.font(22)

    strip_cell = 64
    per_row = 14
    strip_rows = (len(entries) + per_row - 1) // per_row
    strip_h = 60 + strip_rows * (strip_cell + 34)
    matrix_top = max(90 + strip_h + 40, 122 + 256 + 40)
    rows_h = sum(30 + len(items) * (CELL + GAP) for _, items in GROUPS)
    width = LABEL_W + len(colours) * (CELL + GAP) + GAP + 20
    height = matrix_top + 40 + rows_h + 20
    sheet = Image.new("RGB", (width, height), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    draw.text((GAP, 14), "Instrument placard - colour per unit icon", fill=(235, 232, 220), font=title)
    draw.text((GAP, 44), "Class icons (left corner) in camouflage colours: sand = light, olive = medium, Panzergrau = "
                         "heavy.", fill=(170, 168, 160), font=font)
    draw.text((GAP, 64), "Role icons (right corner) in signal colours: red = anti-armour, amber = rapid fire, blue = "
                         "anti-air, plum = explosive, yellow = hazard, teal = support, cream = elite.",
              fill=(170, 168, 160), font=font)

    # Recommended set + a Pz II F at game size.
    y = 96
    draw.text((GAP, y), "Recommended set", fill=(235, 232, 220), font=ib.font(16))
    y += 26
    seeds = {}
    for index, (_, (stem, colour_name, _)) in enumerate(entries):
        seeds[stem] = 5000 + index * 17
        x = GAP + (index % per_row) * (strip_cell + 14)
        row_y = y + (index // per_row) * (strip_cell + 34)
        sheet.paste(ib.on_dark(placard(stem, colour_name, seeds[stem]).resize((strip_cell, strip_cell),
                                                                             Image.LANCZOS)), (x, row_y))
        draw.text((x, row_y + strip_cell + 2), stem.replace("T_", "").split("Class")[0].split("_Icon")[0][:11],
                  fill=(160, 158, 150), font=ib.font(10))
    light = placard("T_LightTankClass_Icon_V2", "sand", seeds["T_LightTankClass_Icon_V2"])
    belt = placard("T_AutoCannon_Class_V2", "amber", seeds["T_AutoCannon_Class_V2"])
    picture, _ = eh.portrait_row("", light, belt)
    px = GAP + per_row * (strip_cell + 14) + 10
    if px + 256 > width:
        px = width - 256 - GAP - 75 - GAP
    sheet.paste(picture, (px, y))
    sheet.paste(picture.resize((75, 75), Image.LANCZOS), (px + 256 + GAP, y))
    draw.text((px, y + 258), "Pz II F: sand + amber (256 / 75 px)", fill=(160, 158, 150), font=small)

    # Matrix: icon x colour, recommended framed.
    y = matrix_top
    for index, colour_name in enumerate(colours):
        draw.text((LABEL_W + index * (CELL + GAP), y), colour_name, fill=(200, 198, 190), font=small)
    y += 26
    for group, items in GROUPS:
        draw.text((GAP, y + 6), group, fill=(235, 232, 220), font=ib.font(16))
        y += 30
        for stem, recommended, reason in items:
            draw.text((GAP, y + 24), stem.replace("T_", ""), fill=(225, 222, 212), font=small)
            draw.text((GAP, y + 42), f"pick: {recommended}", fill=(232, 196, 120), font=small)
            draw.text((GAP, y + 58), reason, fill=(150, 148, 140), font=ib.font(11))
            for index, colour_name in enumerate(colours):
                x = LABEL_W + index * (CELL + GAP)
                cell = placard(stem, colour_name, seeds[stem]).resize((CELL, CELL), Image.LANCZOS)
                sheet.paste(ib.on_dark(cell), (x, y))
                if colour_name == recommended:
                    draw.rectangle([x - 4, y - 4, x + CELL + 3, y + CELL + 3], outline=(232, 196, 120), width=3)
            y += CELL + GAP
        print(f"group {group}")
    sheet.save(OUT / "PlacardColours_sheet.png")
    print(f"sheet {OUT / 'PlacardColours_sheet.png'}")


if __name__ == "__main__":
    main()
