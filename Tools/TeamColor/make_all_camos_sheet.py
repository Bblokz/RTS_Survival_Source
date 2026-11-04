"""Builds the all-camos review sheets: one row per vehicle, Default/Red/Blue from the side and from the RTS camera.

Usage: python make_all_camos_sheet.py <capture folder> <output folder> [--cell 260] [--rows-per-sheet 8]
Reads the entries written by make_all_camos_list.py; writes all_camos_<n>.png, numbered rows to point at.
"""
import argparse
import os

from PIL import Image, ImageDraw

from make_all_camos_list import ALL_CAMOS, REVIEW_COLORS
from make_team_color_sheet import BACKGROUND, TEXT_COLOUR, get_font, load_cell

SHEET_VIEWS = ["FrontRight", "RtsCamera"]
LABEL_HEIGHT = 34
ROW_LABEL_WIDTH = 250
VIEW_GAP = 16


def get_label(index, unit):
    short = unit.replace("Ger_Vehicle_", "").replace("Ger_ArmoredCar_", "").replace("Ger_", "")
    return f"{index + 1}. {short}"


def build_sheet(capture_folder, first_index, units, cell_size):
    view_width = cell_size * len(REVIEW_COLORS)
    width = ROW_LABEL_WIDTH + view_width * len(SHEET_VIEWS) + VIEW_GAP * (len(SHEET_VIEWS) - 1)
    sheet = Image.new("RGB", (width, LABEL_HEIGHT + cell_size * len(units)), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    font = get_font(19)
    for view_index, view in enumerate(SHEET_VIEWS):
        view_left = ROW_LABEL_WIDTH + view_index * (view_width + VIEW_GAP)
        for column, colour in enumerate(REVIEW_COLORS):
            draw.text((view_left + column * cell_size + 10, 6), f"{colour} ({view})", fill=TEXT_COLOUR, font=font)
    for row, unit in enumerate(units):
        top = LABEL_HEIGHT + row * cell_size
        draw.text((10, top + cell_size // 2 - 12), get_label(first_index + row, unit), fill=TEXT_COLOUR, font=font)
        for view_index, view in enumerate(SHEET_VIEWS):
            view_left = ROW_LABEL_WIDTH + view_index * (view_width + VIEW_GAP)
            for column, colour in enumerate(REVIEW_COLORS):
                cell = load_cell(capture_folder, f"{unit}_{colour}", view, cell_size)
                if cell:
                    sheet.paste(cell, (view_left + column * cell_size, top))
                else:
                    draw.text((view_left + column * cell_size + 20, top + cell_size // 2), "not captured",
                              fill=(140, 140, 140), font=font)
    return sheet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_folder")
    parser.add_argument("output_folder")
    parser.add_argument("--cell", type=int, default=260)
    parser.add_argument("--rows-per-sheet", type=int, default=8)
    arguments = parser.parse_args()
    os.makedirs(arguments.output_folder, exist_ok=True)
    for start in range(0, len(ALL_CAMOS), arguments.rows_per_sheet):
        units = ALL_CAMOS[start:start + arguments.rows_per_sheet]
        path = os.path.join(arguments.output_folder, f"all_camos_{start // arguments.rows_per_sheet + 1}.png")
        build_sheet(arguments.capture_folder, start, units, arguments.cell).save(path)
        print(path)


if __name__ == "__main__":
    main()
