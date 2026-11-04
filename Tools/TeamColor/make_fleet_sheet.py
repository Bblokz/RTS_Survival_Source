"""Builds the fleet team colour sheets: one row per vehicle, one column per colour (Default first).

Usage: python make_fleet_sheet.py <capture folder> <output folder> [--cell 300] [--rows-per-sheet 10]
Reads the entries written by make_fleet_capture_list.py (<Unit>_<Color>); writes fleet_<View>_<n>.png.
"""
import argparse
import os

from PIL import Image, ImageDraw

from make_team_color_sheet import BACKGROUND, TEXT_COLOUR, VIEWS, get_font, load_cell
from make_fleet_capture_list import COLORS, FLEET

LABEL_HEIGHT = 34
ROW_LABEL_WIDTH = 250


def build_sheet(capture_folder, units, view, cell_size):
    colours = list(COLORS)
    sheet = Image.new("RGB", (ROW_LABEL_WIDTH + cell_size * len(colours), LABEL_HEIGHT + cell_size * len(units)),
                      BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    font = get_font(20)
    for column, colour in enumerate(colours):
        draw.text((ROW_LABEL_WIDTH + column * cell_size + 10, 6), colour, fill=TEXT_COLOUR, font=font)
    for row, unit in enumerate(units):
        top = LABEL_HEIGHT + row * cell_size
        label = unit.replace("Ger_Vehicle_", "").replace("Ger_ArmoredCar_", "").replace("Ger_", "")
        draw.text((10, top + cell_size // 2 - 12), label, fill=TEXT_COLOUR, font=font)
        for column, colour in enumerate(colours):
            cell = load_cell(capture_folder, f"{unit}_{colour}", view, cell_size)
            if cell:
                sheet.paste(cell, (ROW_LABEL_WIDTH + column * cell_size, top))
    return sheet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_folder")
    parser.add_argument("output_folder")
    parser.add_argument("--cell", type=int, default=300)
    parser.add_argument("--rows-per-sheet", type=int, default=10)
    arguments = parser.parse_args()
    os.makedirs(arguments.output_folder, exist_ok=True)
    for view in VIEWS:
        for start in range(0, len(FLEET), arguments.rows_per_sheet):
            units = FLEET[start:start + arguments.rows_per_sheet]
            path = os.path.join(arguments.output_folder, f"fleet_{view}_{start // arguments.rows_per_sheet}.png")
            build_sheet(arguments.capture_folder, units, view, arguments.cell).save(path)
            print(path)


if __name__ == "__main__":
    main()
