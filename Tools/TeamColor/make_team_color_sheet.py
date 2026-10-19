"""Builds comparison sheets from run_team_color_capture.sh output: one row per shader, one column per team colour.

Usage: python make_team_color_sheet.py <capture folder> <output folder> [--cell 360] [--unit PzIIIM] [--variants A,B]
Writes sheet_<Unit>_<View>.png per camera view, each cell cropped on the logged vehicle box (crops.csv).
"""
import argparse
import csv
import os

from PIL import Image, ImageDraw, ImageFont

VARIANTS = ["CamoSwap", "PaintTint", "TopologyAccent"]
COLORS = ["Red", "Blue", "Green", "Yellow"]
VIEWS = ["RtsCamera", "FrontRight", "Right"]
LABEL_HEIGHT = 34
ROW_LABEL_WIDTH = 230
CROP_ZOOM = 1.7
BACKGROUND = (24, 24, 26)
TEXT_COLOUR = (235, 235, 235)


def load_crop_boxes(unit_folder):
    boxes = {}
    crop_file = os.path.join(unit_folder, "crops.csv")
    if not os.path.exists(crop_file):
        return boxes
    with open(crop_file, newline="") as handle:
        for row in csv.reader(handle):
            if len(row) == 5:
                boxes[row[0]] = tuple(float(value) for value in row[1:])
    return boxes


def load_cell(capture_folder, unit_name, view, cell_size):
    unit_folder = os.path.join(capture_folder, unit_name)
    file_name = f"{unit_name}_{view}_idle.png"
    path = os.path.join(unit_folder, file_name)
    if not os.path.exists(path):
        return None
    picture = Image.open(path).convert("RGB")
    box = load_crop_boxes(unit_folder).get(file_name)
    if box:
        centre_x, centre_y = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
        half = max(box[2] - box[0], box[3] - box[1]) / 2 / CROP_ZOOM
        picture = picture.crop((int(centre_x - half), int(centre_y - half), int(centre_x + half), int(centre_y + half)))
    return picture.resize((cell_size, cell_size), Image.LANCZOS)


def get_font(size):
    try:
        return ImageFont.truetype("arial.ttf", size)
    except OSError:
        return ImageFont.load_default()


def build_sheet(capture_folder, unit, variants, view, cell_size):
    rows = [("Original", [f"{unit}_Original"])] + [
        (variant, [f"{unit}_{variant}_{colour}" for colour in COLORS]) for variant in variants]
    width = ROW_LABEL_WIDTH + cell_size * len(COLORS)
    height = LABEL_HEIGHT + cell_size * len(rows)
    sheet = Image.new("RGB", (width, height), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    font = get_font(22)
    for column, colour in enumerate(COLORS):
        draw.text((ROW_LABEL_WIDTH + column * cell_size + 10, 6), colour, fill=TEXT_COLOUR, font=font)
    for row_index, (label, unit_names) in enumerate(rows):
        top = LABEL_HEIGHT + row_index * cell_size
        draw.text((10, top + cell_size // 2 - 12), label, fill=TEXT_COLOUR, font=font)
        for column, unit_name in enumerate(unit_names):
            cell = load_cell(capture_folder, unit_name, view, cell_size)
            if cell:
                sheet.paste(cell, (ROW_LABEL_WIDTH + column * cell_size, top))
    return sheet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_folder")
    parser.add_argument("output_folder")
    parser.add_argument("--cell", type=int, default=360)
    # Entry name prefix in the capture list (PzIIIM_..., KingTiger_...).
    parser.add_argument("--unit", default="PzIIIM")
    parser.add_argument("--variants", default=",".join(VARIANTS))
    arguments = parser.parse_args()
    os.makedirs(arguments.output_folder, exist_ok=True)
    for view in VIEWS:
        sheet_path = os.path.join(arguments.output_folder, f"sheet_{arguments.unit}_{view}.png")
        build_sheet(arguments.capture_folder, arguments.unit, arguments.variants.split(","), view, arguments.cell).save(sheet_path)
        print(sheet_path)


if __name__ == "__main__":
    main()
