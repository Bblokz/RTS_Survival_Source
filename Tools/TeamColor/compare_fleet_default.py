"""Checks that the team colour rollout left the default look alone: compares each vehicle's Default picture after the
rollout with the baseline taken before it.

Usage: python compare_fleet_default.py <baseline capture folder> <after capture folder>
Prints the mean and 99th percentile absolute difference (0-255) inside the vehicle crop per view, next to the noise
floor: the same unit's Warmup_ picture against its Default picture in the baseline run (same material, other moment;
the map's light drifts). Before/after at that floor means the rollout changed nothing visible.
"""
import os
import sys

import numpy as np
from PIL import Image

from make_team_color_sheet import VIEWS, load_crop_boxes


def load_picture(folder, entry, view):
    path = os.path.join(folder, entry, f"{entry}_{view}_idle.png")
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) if os.path.exists(path) else None


def get_crop_box(folder, entry, view):
    box = load_crop_boxes(os.path.join(folder, entry)).get(f"{entry}_{view}_idle.png")
    return tuple(int(max(value, 0)) for value in box) if box else None


def diff_in_box(first, second, box):
    """@return (mean, p99) absolute difference inside the box, or None when a picture is missing."""
    if first is None or second is None or first.shape != second.shape:
        return None
    if box:
        first = first[box[1]:box[3], box[0]:box[2]]
        second = second[box[1]:box[3], box[0]:box[2]]
    difference = np.abs(first - second)
    return float(difference.mean()), float(np.percentile(difference, 99))


def format_cell(view, result):
    return f"{view}: missing" if result is None else f"{view}: mean {result[0]:.2f} p99 {result[1]:.0f}"


def main():
    baseline, after = sys.argv[1], sys.argv[2]
    worst_change, worst_noise = 0.0, 0.0
    for entry in sorted(os.listdir(baseline)):
        if not entry.endswith("_Default"):
            continue
        unit = entry[:-len("_Default")]
        change_cells, noise_cells = [], []
        for view in VIEWS:
            # The baseline's crop for both runs: settled bounds differ by a few pixels between runs.
            box = get_crop_box(baseline, entry, view)
            before_picture = load_picture(baseline, entry, view)
            change = diff_in_box(before_picture, load_picture(after, entry, view), box)
            noise = diff_in_box(before_picture, load_picture(baseline, f"Warmup_{unit}", view), box)
            change_cells.append(format_cell(view, change))
            noise_cells.append(format_cell(view, noise))
            worst_change = max(worst_change, change[1] if change else 0.0)
            worst_noise = max(worst_noise, noise[1] if noise else 0.0)
        print(f"{unit:<34} after   " + " | ".join(change_cells))
        print(f"{'':<34} noise   " + " | ".join(noise_cells))
    print(f"worst p99: before/after {worst_change:.0f}, same-run noise {worst_noise:.0f}")


if __name__ == "__main__":
    main()
