"""Squares raw unit portrait pictures on the vehicle and scales them to the 256x256 portrait size.

Reads the crops.csv the capture run writes next to each unit's pictures (the vehicle's projected bounds per picture).

Usage:
  python crop_portraits.py <capture folder> <output folder> [--size 256] [--zoom 1.0] [--sheet]

<capture folder> holds one sub folder per unit (as written by run_capture.sh). Every picture is cropped into
<output folder>/<unit>/; --sheet also writes <output folder>/<unit>_sheet.jpg with all crops for picking.
--zoom > 1 crops tighter than the vehicle bounds (they include the barrel and some margin).
"""
import argparse
import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw

SHEET_TILE = 256
SHEET_COLUMNS = 6


def read_crop_boxes(unit_folder: Path) -> dict:
    boxes = {}
    crops_file = unit_folder / "crops.csv"
    if not crops_file.exists():
        return boxes
    with crops_file.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.reader(handle):
            if len(row) == 5:
                boxes[row[0]] = tuple(float(value) for value in row[1:])
    return boxes


def square_crop_box(box, image_size, zoom):
    """Square around the box centre, as large as the box's longer side, moved (not shrunk) to stay in the image."""
    min_x, min_y, max_x, max_y = box
    width, height = image_size
    side = min(max(max_x - min_x, max_y - min_y) / zoom, width, height)
    centre_x = (min_x + max_x) * 0.5
    centre_y = (min_y + max_y) * 0.5
    left = min(max(centre_x - side * 0.5, 0), width - side)
    top = min(max(centre_y - side * 0.5, 0), height - side)
    return int(round(left)), int(round(top)), int(round(left + side)), int(round(top + side))


def crop_unit(unit_folder: Path, output_folder: Path, size: int, zoom: float) -> list:
    boxes = read_crop_boxes(unit_folder)
    output_folder.mkdir(parents=True, exist_ok=True)
    written = []
    for picture in sorted(unit_folder.glob("*.png")):
        image = Image.open(picture).convert("RGB")
        box = boxes.get(picture.name, (0, 0, image.width, image.height))
        cropped = image.crop(square_crop_box(box, image.size, zoom))
        cropped = cropped.resize((size, size), Image.LANCZOS)
        target = output_folder / picture.name
        cropped.save(target)
        written.append(target)
    return written


def write_sheet(pictures: list, sheet_path: Path) -> None:
    if not pictures:
        return
    rows = math.ceil(len(pictures) / SHEET_COLUMNS)
    sheet = Image.new("RGB", (SHEET_COLUMNS * SHEET_TILE, rows * SHEET_TILE), (30, 30, 30))
    draw = ImageDraw.Draw(sheet)
    for index, picture in enumerate(pictures):
        x = (index % SHEET_COLUMNS) * SHEET_TILE
        y = (index // SHEET_COLUMNS) * SHEET_TILE
        sheet.paste(Image.open(picture).resize((SHEET_TILE, SHEET_TILE)), (x, y))
        draw.text((x + 4, y + 4), picture.stem.split("_", 1)[-1], fill=(255, 255, 0))
    sheet.save(sheet_path, quality=85)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture_folder", type=Path)
    parser.add_argument("output_folder", type=Path)
    parser.add_argument("--size", type=int, default=256)
    parser.add_argument("--zoom", type=float, default=1.0)
    parser.add_argument("--sheet", action="store_true")
    arguments = parser.parse_args()
    for unit_folder in sorted(path for path in arguments.capture_folder.iterdir() if path.is_dir()):
        written = crop_unit(unit_folder, arguments.output_folder / unit_folder.name, arguments.size, arguments.zoom)
        if arguments.sheet:
            write_sheet(written, arguments.output_folder / f"{unit_folder.name}_sheet.jpg")
        print(f"{unit_folder.name}: {len(written)} pictures")


if __name__ == "__main__":
    main()
