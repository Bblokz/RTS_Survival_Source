"""Plots the aircraft MG strafe QA CSVs (Saved/AircraftStrafeQA/*.csv) written by -AircraftStrafeValidate.

Usage: python plot_strafe_qa.py <out.png> <label=csv> [<label=csv> ...]
One column per CSV: top-down impacts (ground hits green, rounds that hit nothing red) with aircraft gun positions,
and a side view (height over the ground along the flight) that shows rounds ending in the air.
"""
import csv
import math
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_rows(path):
    shots, impacts = [], []
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            (shots if row["kind"] == "shot" else impacts).append(row)
    return shots, impacts


def as_vec(row, prefix):
    return tuple(float(row[f"{prefix}_{axis}"]) for axis in "xyz")


def main():
    out_path = sys.argv[1]
    runs = [arg.split("=", 1) for arg in sys.argv[2:]]
    figure, axes = plt.subplots(2, len(runs), figsize=(7 * len(runs), 11), squeeze=False)
    for column, (label, path) in enumerate(runs):
        shots, impacts = read_rows(path)
        ground_z = min((as_vec(r, "end")[2] for r in impacts if r["hit"] == "1"), default=0.0)
        hits = [as_vec(r, "end") for r in impacts if r["hit"] == "1"]
        misses = [as_vec(r, "end") for r in impacts if r["hit"] != "1"]
        launches = [as_vec(r, "launch") for r in shots]
        aims = [as_vec(r, "aim") for r in shots]

        top = axes[0][column]
        top.scatter([p[0] / 100 for p in launches], [p[1] / 100 for p in launches], s=2, c="0.6", label="guns (aircraft)")
        top.scatter([p[0] / 100 for p in aims], [p[1] / 100 for p in aims], s=2, c="tab:blue", label="aim point")
        top.scatter([p[0] / 100 for p in hits], [p[1] / 100 for p in hits], s=4, c="tab:green", label="impact on ground/unit")
        top.scatter([p[0] / 100 for p in misses], [p[1] / 100 for p in misses], s=8, c="tab:red", marker="x",
                    label="round hit nothing (air puff)")
        top.set_aspect("equal", adjustable="datalim")
        top.set_title(f"{label}: top-down (m)")
        top.legend(loc="best", fontsize=8)

        side = axes[1][column]
        # Height above the lowest ground hit against the round's horizontal distance from its gun.
        for row in impacts:
            launch, end = as_vec(row, "launch"), as_vec(row, "end")
            distance = math.dist(launch[:2], end[:2]) / 100
            colour = "tab:green" if row["hit"] == "1" else "tab:red"
            side.plot([0, distance], [(launch[2] - ground_z) / 100, (end[2] - ground_z) / 100], c=colour, lw=0.2,
                      alpha=0.4)
        side.axhline(0, c="k", lw=0.8)
        side.set_xlabel("horizontal distance from gun (m)")
        side.set_ylabel("height above ground (m)")
        side.set_title(f"{label}: every round, side view")
    figure.tight_layout()
    figure.savefig(out_path, dpi=110)
    print(out_path)


if __name__ == "__main__":
    main()
