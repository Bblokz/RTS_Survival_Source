"""Contact sheets for a corpse pose bake (run_pose_bake.sh) and a check that each baked mesh matches its ragdoll.

Usage: python pose_contact_sheets.py <pictures dir> <log file>
Writes sheet_<side>_top.png and sheet_<side>_side.png into the pictures dir and prints the ragdoll/baked difference.
"""
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

COLUMNS = 4
# Centre crop of the 1280x720 shots: the body sits in the middle.
CROP = (240, 60, 1040, 660)
TILE = (400, 300)
LABEL_FONT = "C:/Windows/Fonts/arialbd.ttf"
SIDES = ("Scav", "Mosin")
# The speckled floor shimmers between frames, so the mean difference is noise. A wrong pose moves a whole limb:
# thousands of pixels change by more than BIG_CHANGE. Matching shots stay near 0.01 %.
BIG_CHANGE = 60
MATCH_LIMIT_PERCENT = 0.05


def read_log(log_path):
    info = {}
    pattern = re.compile(r"pose=(\w+) asleep=(\d) saved=(\d) tris=(\d+) lowest_z=([-\d.]+)")
    for line in Path(log_path).read_text(encoding="utf-8", errors="ignore").splitlines():
        match = pattern.search(line)
        if match and "RTS_RGP" in line:
            info[match.group(1)] = {"asleep": match.group(2) == "1", "lowest_z": float(match.group(5))}
    return info


def make_sheet(directory, names, shot, info, out_path):
    font = ImageFont.truetype(LABEL_FONT, 22)
    small = ImageFont.truetype(LABEL_FONT, 16)
    rows = (len(names) + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGB", (COLUMNS * TILE[0], rows * TILE[1]), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    for index, name in enumerate(names):
        picture = Image.open(directory / f"pose_{name}_{shot}.png").convert("RGB").crop(CROP).resize(TILE)
        x, y = (index % COLUMNS) * TILE[0], (index // COLUMNS) * TILE[1]
        sheet.paste(picture, (x, y))
        draw.rectangle([x, y, x + TILE[0] - 1, y + TILE[1] - 1], outline=(0, 0, 0), width=2)
        label = name.replace("_", " ")
        draw.rectangle([x + 6, y + 6, x + 118, y + 36], fill=(0, 0, 0))
        draw.text((x + 12, y + 9), label, font=font, fill=(255, 255, 255))
        sink = info.get(name, {}).get("lowest_z", 0.0)
        if sink < -5.0:
            note = f"sinks {-sink:.0f} cm"
            draw.rectangle([x + 6, y + TILE[1] - 32, x + 140, y + TILE[1] - 8], fill=(150, 20, 20))
            draw.text((x + 12, y + TILE[1] - 30), note, font=small, fill=(255, 255, 255))
    sheet.save(out_path)


def main():
    directory = Path(sys.argv[1])
    info = read_log(sys.argv[2])
    for side in SIDES:
        names = sorted(name for name in info if name.startswith(side + "_"))
        for shot, suffix in (("baked", "top"), ("side", "side")):
            make_sheet(directory, names, shot, info, directory / f"sheet_{side}_{suffix}.png")
        for name in names:
            ragdoll = np.asarray(Image.open(directory / f"pose_{name}_ragdoll.png").convert("RGB"), dtype=np.float32)
            baked = np.asarray(Image.open(directory / f"pose_{name}_baked.png").convert("RGB"), dtype=np.float32)
            changed = 100.0 * float((np.abs(ragdoll - baked).max(axis=2) > BIG_CHANGE).mean())
            verdict = "match" if changed <= MATCH_LIMIT_PERCENT else "DIFFERS"
            print(f"{name} changed={changed:.4f}% {verdict} lowest_z={info[name]['lowest_z']:.1f}")


if __name__ == "__main__":
    main()
