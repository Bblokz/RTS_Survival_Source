"""Contact sheet of the round 3b icons next to six existing ones, at the ability panel's button size (96 slate units)
and smaller, on a dark panel colour. Writes Saved/AbilityIconsB/contact_sheet_b.png.
Usage: python round3b_contact_sheet.py
"""
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from round3b_compose_icons import TEMPLATES  # noqa: E402

OUT = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out/"
SHEET = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsB/contact_sheet_b.png"
# Existing icons that no round-3 icon agent re-composes.
EXISTING = ["3xJu87Strafing", "Barrage", "Ju390Carpet250", "GerFlareCurtain", "GerAirMinefield", "GerReconFlight"]
BUTTON = 96
CELL_W, CELL_H = 150, 196
COLUMNS = 7


def cell(sheet, draw, index, name, icon_name, existing):
    x = 12 + (index % COLUMNS) * CELL_W
    y = 40 + (index // COLUMNS) * CELL_H
    icon = Image.open(OUT + "T_GA_%s.png" % icon_name).convert("RGBA")
    sheet.alpha_composite(icon.resize((BUTTON, BUTTON), Image.LANCZOS), (x, y))
    sheet.alpha_composite(icon.resize((64, 64), Image.LANCZOS), (x, y + BUTTON + 6))
    sheet.alpha_composite(icon.resize((40, 40), Image.LANCZOS), (x + 70, y + BUTTON + 6))
    colour = (150, 200, 255, 255) if existing else (235, 225, 205, 255)
    draw.text((x, y + BUTTON + 74), ("[existing] " if existing else "") + name[:22], fill=colour)


def main():
    entries = [(n, n, True) for n in EXISTING] + [(key.replace("GA_", ""), icon, False) for key, icon in TEMPLATES.items()]
    rows = (len(entries) + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGBA", (24 + COLUMNS * CELL_W, 50 + rows * CELL_H), (24, 26, 28, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 12), "Icons B (round 3b): 22 new icons after 6 existing ones (blue labels). Sizes 96 / 64 / 40 px.",
              fill=(220, 220, 220, 255))
    for i, (name, icon_name, existing) in enumerate(entries):
        cell(sheet, draw, i, name, icon_name, existing)
    sheet.save(SHEET)
    print("SHEET", SHEET)


if __name__ == "__main__":
    main()
