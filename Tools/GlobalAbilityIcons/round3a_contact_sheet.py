"""Round 3a review sheet: every unit-drop icon next to six existing icons, at the ability panel's size.

The panel button is 96 slate units with the hotkey label over the bottom fifth; the sheet draws each icon at 96 px
with a dark label strip like the button's, plus 64 px, on the panel's dark backdrop.
Usage: python round3a_contact_sheet.py   -> Saved/AbilityIconsA/contact_sheet.png
"""
import os

from PIL import Image, ImageDraw

OUT = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out/"
SHEET = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsA/contact_sheet.png"
EXISTING = ["3xJu87Strafing", "GerFlakLuchsStugDrop", "GerMammutE100Drop", "SovietT34Drop", "GerSpiesInfiltration",
            "GerPzIIIFlak41Drop"]
MINE = ["GerE50Drop", "GerE75Drop", "GerMausDrop", "GerJagdTigerLaserDrop", "GerJagdPantherFlamerDrop",
        "GerSturmtigerDrop", "GerJagdTigerDrop", "GerKingTiger105Drop", "GerTigerRailDrop",
        "GerRadixitePzIIDrop", "GerRadixitePzIIIWespeDrop", "GerPzJagerLaserDrop", "GerPzIIIFlammDrop", "GerPumaDrop",
        "GerMarderScavengerDrop", "GerEliteScavengersPakDrop", "GerSturmPionierenPakPitDrop",
        "GerEliteScavengersParadrop", "GerRailTroopersParadrop", "GerLightBringersParadrop", "GerFeuerSturmParadrop",
        "GerParatroopers", "GerIronStormParadrop", "GerPzGrenIronStormParadrop",
        "GerRedSpineDrop", "GerRedSpinePairDrop", "GerRailTroopersDrop", "GerFelsKriegerDrop",
        "GerHarvesterDrop", "GerPzIHarvesterDrop", "GerPz38tRailgunDrop", "GerScavengersDrop", "GerSpotterPzIIIDrop"]
CELL_W, CELL_H, COLUMNS = 176, 150, 8
BACKDROP = (24, 26, 30, 255)


def button(icon, size):
    """The icon as the panel draws it: scaled, with the hotkey strip over the bottom fifth."""
    img = icon.resize((size, size), Image.LANCZOS)
    strip = Image.new("RGBA", (size, size // 5), (0, 0, 0, 120))
    img.alpha_composite(strip, (0, size - size // 5))
    ImageDraw.Draw(img).text((size // 2 - 3, size - size // 5 + 2), "Q", fill=(235, 235, 235, 255))
    return img


def section(title, names, draw, sheet, top):
    draw.text((10, top), title, fill=(220, 220, 220, 255))
    top += 18
    for i, name in enumerate(names):
        x, y = 10 + (i % COLUMNS) * CELL_W, top + (i // COLUMNS) * CELL_H
        path = OUT + "T_GA_%s.png" % name
        if not os.path.exists(path):
            draw.text((x, y + 40), "missing\n" + name, fill=(255, 120, 120, 255))
            continue
        icon = Image.open(path).convert("RGBA")
        sheet.alpha_composite(button(icon, 96), (x, y))
        sheet.alpha_composite(button(icon, 64), (x + 102, y + 32))
        draw.text((x, y + 100), name[:26], fill=(200, 200, 200, 255))
    return top + ((len(names) + COLUMNS - 1) // COLUMNS) * CELL_H + 6


def main():
    rows = (len(EXISTING) + COLUMNS - 1) // COLUMNS + (len(MINE) + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGBA", (COLUMNS * CELL_W + 20, rows * CELL_H + 70), BACKDROP)
    draw = ImageDraw.Draw(sheet)
    top = section("Existing icons (reference)", EXISTING, draw, sheet, 6)
    section("Round 3a unit-drop icons (Icons A)", MINE, draw, sheet, top)
    sheet.save(SHEET)
    print("SHEET", SHEET)


if __name__ == "__main__":
    main()
