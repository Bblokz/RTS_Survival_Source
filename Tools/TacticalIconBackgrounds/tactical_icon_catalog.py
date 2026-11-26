"""Standalone placard badges + catalog data for the selected unit description (UW_SelectedUnitDescription), which shows
the same one or two tactical icons as the unit portrait, each with a one-line explanation.

Reads the placard plans (the source of truth for which icons are engraved on which portrait) and writes:
  <out>/T_TacPlacard_<name>.png   one badge per icon spec (stem or stem@colour), same look as on the portraits
  tactical_icon_catalog.json (next to this script)  {"icons": {spec: {texture, description}}, "portraits": {texture path: [left, right]}}
  <out>/tactical_icon_badges_sheet.png  review sheet (badge + text)
fill_tactical_icon_catalog.py (run in the editor) imports the badges and fills DA_TacticalIconCatalog from the json.

Usage: python tactical_icon_catalog.py [--out D:/UE_Assets/Icons/TrainingUI/Placard/Badges]
"""
import argparse
import json
import sys
import zlib
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import enamel_handmade as eh  # noqa: E402
import icon_backgrounds as ib  # noqa: E402
from placard_colours import PALETTE, recommended_colour  # noqa: E402
from placard_portraits import read_plan  # noqa: E402

PLANS = ("light_medium.txt", "heavy_armoredcar.txt", "infantry_guns_air.txt", "bxps.txt")
DEFAULT_OUT = Path("D:/UE_Assets/Icons/TrainingUI/Placard/Badges")
BADGE_PIXELS = 128
# Inside the project: the editor bridge only reads files there.
CATALOG_JSON = HERE / "tactical_icon_catalog.json"
# bxps.txt keys are texture names only; the textures live here.
BXP_TEXTURE_FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/BuildingExpansions/"

# One line per icon spec, shown next to its badge. Keep them short: the column is narrow.
DESCRIPTIONS = {
    # Vehicle class (left corner).
    "T_LightTankClass_Icon_V2": "Light Tank",
    "T_ArmoredCar_Icon": "Armored Car",
    "T_MediumTankClass_Icon_V2": "Medium Tank",
    "T_HeavyTankClass_Icon_V2": "Heavy Tank",
    "T_SuperHeavyTankClass_Icon": "Super Heavy Tank",
    "T_TankDestroyerClass_Icon": "Tank Destroyer: anti-armour specialist",
    "T_ArtilleryClass_Icon_V3": "Artillery: long-range indirect fire",
    # Vehicle / weapon role (right corner).
    "T_AutoCannon_Class_V2": "Fires multiple rounds before reloading",
    "T_MGClass_Icon_V2": "Machine guns: rapid fire vs infantry",
    "T_AntiAirClass_Icon_V3_NoStar": "Anti-Air specialist",
    "T_RocketWeaponClass_Icon_V2": "Fires a rocket salvo before reloading",
    "T_RailgunClass_Icon_V2": "Railgun: pierces heavy armour",
    "T_LaserClass_Icon": "Laser: burns through heavy armour",
    "T_FlameClass_Icon": "Flamethrowers: sets targets on fire",
    "T_FlameClass_Icon_TankGun": "Flame gun: sets targets on fire",
    "T_RadiationClass_Icon": "Radiation: damages over time",
    # Infantry role.
    "T_AssaultClass_Icon_V3": "Assault: close-range anti-infantry",
    "T_ATInfantryClass_Icon_V2": "Anti-tank infantry",
    "T_CommandoClass_Icon_V3": "Elite infantry",
    "T_SniperClass_Icon_V2": "Sniper: long-range precision",
    "T_EngineerClass_Icon_V2": "Engineers: build and repair",
    # Support.
    "T_RepairAuraClass_V2": "Repairs nearby vehicles",
    "T_RangeProviderClass_ICon_V2": "Spotter: extends vision and range",
    "T_ExtraIcon_V2": "Medic: heals nearby infantry",
    "T_CargoGarrisionClass_Icon": "Transport: carries a squad",
    "T_CargoGarrisionClass_Icon@teal": "Garrison: holds a squad",
    # Aircraft.
    "T_CASAircraftClass_Icon_V3_Clean": "Ground attack aircraft",
    "T_BombAircraftClass_Icon_V2": "Bomber",
    # Building expansions (left = option category, right = role).
    "T_HQ_Icon": "Defensive structure",
    "T_HQ_Icon@teal": "Cheaper field defences",
    "T_MetalStorageClass_Icon_V2": "Economy expansion",
    "T_ResearchBuilding": "Tech expansion",
    "T_ResearchBuilding@teal": "Unlocks new technology",
    "T_EnergyBuilidngClass_Icon_V2": "Supplies energy",
    "T_RadixiteStorageClass_Icon_V2": "Stores Radixite",
    "T_RocketAbiltyClass_ICon_V3@teal": "Shortens ability cooldowns",
    "T_Solar_Icon_V2@teal": "Solar power",
}


def texture_name(spec):
    stem, _, colour = spec.partition("@")
    name = "T_TacPlacard_" + (stem[2:] if stem.startswith("T_") else stem)
    return name + ("_" + colour.replace(" ", "") if colour else "")


def badge(spec):
    stem, _, colour_name = spec.partition("@")
    colour = PALETTE[colour_name] if colour_name else recommended_colour(stem)
    seed = zlib.crc32(spec.encode()) % 100000
    plate = eh.instrument_placard(ib.ICON_SET / f"{stem}.png", colour, seed)
    return plate.resize((BADGE_PIXELS, BADGE_PIXELS), Image.LANCZOS)


def portrait_path(texture_key):
    return texture_key if texture_key.startswith("/Game/") else BXP_TEXTURE_FOLDER + texture_key


def write_sheet(icons, path):
    row_h, width = BADGE_PIXELS + 8, 560
    sheet = Image.new("RGB", (width, len(icons) * row_h + 8), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    for index, (spec, picture) in enumerate(icons):
        y = 4 + index * row_h
        sheet.paste(picture, (4, y), picture)
        draw.text((BADGE_PIXELS + 14, y + 40), spec, fill=(150, 148, 140), font=ib.font(14))
        draw.text((BADGE_PIXELS + 14, y + 64), DESCRIPTIONS[spec], fill=(235, 232, 220), font=ib.font(18))
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    portraits, specs = {}, []
    for plan in PLANS:
        for texture, _portrait, left, right, _where in read_plan(HERE / "plans" / plan):
            pair = [side if side != "-" else "" for side in (left, right)]
            portraits[portrait_path(texture)] = pair
            specs.extend(spec for spec in pair if spec and spec not in specs)

    missing = [spec for spec in specs if spec not in DESCRIPTIONS]
    if missing:
        sys.exit("No description for: " + ", ".join(missing))

    icons, sheet_rows = {}, []
    for spec in specs:
        picture = badge(spec)
        name = texture_name(spec)
        picture.save(args.out / f"{name}.png")
        icons[spec] = {"texture": name, "description": DESCRIPTIONS[spec]}
        sheet_rows.append((spec, picture))
    catalog = {"icons": icons, "portraits": portraits}
    CATALOG_JSON.write_text(json.dumps(catalog, indent=1), encoding="utf-8")
    write_sheet(sheet_rows, args.out / "tactical_icon_badges_sheet.png")
    print(f"{len(icons)} badges, {len(portraits)} portraits -> {args.out}")


if __name__ == "__main__":
    main()
