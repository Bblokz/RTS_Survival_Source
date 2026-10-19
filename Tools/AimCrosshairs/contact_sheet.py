"""Lays out the in-game reticle captures of run_capture.sh for review.

Per case and spot one row: the Close, Game and Far views, each cropped around the reticle at its real pixel size
(no rescaling, so edge quality can be judged), plus a 1:1 detail crop of the Game view's centre.
Usage: python contact_sheet.py [capture folder] [output folder]
"""

import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

CAPTURE_DIRECTORY = Path(sys.argv[1] if len(sys.argv) > 1 else "D:/UE5Projects/RTS_Survival/Saved/AimCrosshairs")
OUTPUT_DIRECTORY = Path(sys.argv[2] if len(sys.argv) > 2 else CAPTURE_DIRECTORY / "Sheets")
VIEWS = ("Close", "Game", "Far")
# Ground pixels per cm at the picture centre for each view (1920 px over 2 * distance * tan(45 deg)).
PIXELS_PER_CM = {"Close": 1920 / 3200, "Game": 1920 / 6000, "Far": 1920 / 11000}
RADIUS_PER_CASE = {}
LABEL_HEIGHT = 24
MAX_CROP = 1080
DETAIL_CROP = 360


def read_radii(log_path: Path) -> None:
    if not log_path.exists():
        return
    pattern = re.compile(r"RTS_AIM_CROSSHAIR showing (\w+) \([^,]+, radius (\d+)\)")
    for line in log_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = pattern.search(line)
        if match:
            RADIUS_PER_CASE[match.group(1)] = float(match.group(2))


def crop_around_centre(image: Image.Image, half_size: int) -> Image.Image:
    centre_x, centre_y = image.width // 2, image.height // 2
    half_size = min(half_size, MAX_CROP // 2)
    return image.crop((centre_x - half_size, centre_y - half_size, centre_x + half_size, centre_y + half_size))


def label(image: Image.Image, text: str) -> Image.Image:
    labelled = Image.new("RGB", (image.width, image.height + LABEL_HEIGHT), (24, 24, 24))
    labelled.paste(image, (0, LABEL_HEIGHT))
    ImageDraw.Draw(labelled).text((6, 6), text, fill=(235, 235, 235))
    return labelled


def build_row(case: str, spot: str) -> Image.Image:
    radius = RADIUS_PER_CASE.get(case, 800.0)
    cells = []
    for view in VIEWS:
        path = CAPTURE_DIRECTORY / f"{case}_{spot}_{view}.png"
        if not path.exists():
            continue
        image = Image.open(path).convert("RGB")
        half_size = int(radius * PIXELS_PER_CM[view] * 1.2) + 20
        cells.append(label(crop_around_centre(image, half_size), f"{case} {spot} {view} (radius {radius:.0f})"))
        if view == "Game":
            cells.append(label(crop_around_centre(image, DETAIL_CROP // 2), f"{case} Game 1:1 centre"))
    width = sum(cell.width for cell in cells)
    height = max(cell.height for cell in cells)
    row = Image.new("RGB", (width, height), (16, 16, 16))
    x = 0
    for cell in cells:
        row.paste(cell, (x, 0))
        x += cell.width
    return row


def main() -> int:
    read_radii(Path("D:/UE5Projects/RTS_Survival/Saved/Logs/AimCrosshairs.log"))
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    keys = sorted({tuple(path.stem.split("_")[:2]) for path in CAPTURE_DIRECTORY.glob("*_spot*_Game.png")})
    for case, spot in keys:
        build_row(case, spot).save(OUTPUT_DIRECTORY / f"{case}_{spot}.png")
        print(f"sheet {case}_{spot}.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
