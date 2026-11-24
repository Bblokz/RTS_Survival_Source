"""Counts the litter sprites in the pictures of ambient_cull_test.py: old system in the left half, new in the right.
A sprite is a small spot clearly brighter (in blue) than the sand around it: paper and pale ash. Also writes
wisp_old_left_new_right_<picture>.png: the two stretches cut out and put side by side.

    python ambient_cull_report.py [folder]      default Saved/Berlin/ambient/cull_test
"""
import glob
import os
import sys

import numpy as np
from PIL import Image, ImageFilter


def spots(mask):
    height, width = mask.shape
    seen = np.zeros_like(mask, bool)
    found = []
    for y, x in zip(*np.nonzero(mask)):
        if seen[y, x]:
            continue
        stack, count, sum_x = [(y, x)], 0, 0
        seen[y, x] = True
        while stack:
            cy, cx = stack.pop()
            count += 1
            sum_x += cx
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = cy + dy, cx + dx
                if 0 <= ny < height and 0 <= nx < width and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        found.append((count, sum_x / count))
    return found


def main(folder):
    for path in sorted(glob.glob(os.path.join(folder, "visit*.png")) + glob.glob(os.path.join(folder, "away.png"))):
        image = Image.open(path).convert("RGB")
        blue = np.asarray(image).astype(float)[..., 2]
        around = np.asarray(image.filter(ImageFilter.GaussianBlur(20))).astype(float)[..., 2]
        found = [s for s in spots((blue - around) > 25) if s[0] >= 2]
        half = blue.shape[1] / 2
        print("%-13s sprites  old (left) %3d   new (right) %3d" % (os.path.basename(path), sum(1 for _n, x in found if x < half),
                                                                   sum(1 for _n, x in found if x >= half)))
    for name in ("visit1_b", "visit2_a", "visit3_b"):
        path = os.path.join(folder, name + ".png")
        if not os.path.exists(path):
            continue
        image = Image.open(path).convert("RGB")
        sheet = Image.new("RGB", (1210, 900))
        sheet.paste(image.crop((150, 0, 750, 900)), (0, 0))
        sheet.paste(image.crop((950, 0, 1550, 900)), (610, 0))
        sheet.save(os.path.join(folder, "wisp_old_left_new_right_%s.png" % name))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/cull_test")
