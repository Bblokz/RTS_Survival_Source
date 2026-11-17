"""Winter larch placement for Ger_FrozenLake: dense stands in the sketch's dead forests, loose clumps on the tundra,
a few in the snow ring; never on roads, pads, bases, spawns, the airfield, banks or the lake.

    python Source/Tools/FrozenLakeMap/fl_tree_gen.py      (after fl_terrain_gen.py)

Writes Saved/FrozenLake/trees/FL_Trees.json ([variant, x, y, yaw, scale] per tree) and preview_trees.png.
fl_map_build.step_trees() places them (BP_FrozenLake_WinterLarch_<variant>, destructible, tanks crush them).
Trees A-E block infantry and light vehicles in the navmesh (RTS_NavArea_BlockCharLight), F blocks infantry only,
so routes stay clear by keeping trees off every road, track and lane with a margin.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fl_terrain_gen as tg   # noqa: E402

OUT = os.path.join(tg.ROOT, "Saved", "FrozenLake", "trees")
SEED = 23
FOREST_IDS = ("dead_forest_nw", "dead_forest_se", "dead_forest_w", "dead_forest_ne")
PAD_IDS = ("player_shelf", "shield_pad", "landing_field", "pump_s_pad", "pump_w_pad", "pump_n_pad", "pump_e_pad")
PATH_KINDS = ("road", "track", "lane")
KEEP_CLEAR_CM = {"road": 500.0, "pad": 800.0, "lake": 600.0, "cut": 900.0, "dam": 900.0}
BASE_CLEAR = {"hq": 7500.0, "shield_garrison": 6500.0, "spawn_n": 2500.0, "spawn_e": 2500.0, "spawn_s": 2500.0,
              "convoy_wreck_se": 1800.0, "blown_bridge": 1500.0, "canal_bridge": 1500.0, "airfield_control": 1200.0}
MAX_SLOPE_DEG = 24.0
SPACING_CM = {"forest": 650.0, "clump": 700.0, "ring": 1400.0}
OUTSIDE_PLAY_KEEP = 0.35          # share of tundra clumps kept beyond the play edge (backdrop only)
VARIANTS_FOREST = ["A", "B", "C", "D", "E", "E", "F"]
VARIANTS_TUNDRA = ["A", "B", "C", "D", "E", "F", "F"]


def main():
    g = tg.Gen()
    g.build_heights()
    g.build_layers()
    rng = np.random.default_rng(SEED)
    cell = tg.QUAD_CM

    def clear_of(mask, cm):
        return tg.dist_outside(mask, int(cm / cell) + 2) * cell > cm

    roads = np.zeros(tg.SHAPE, dtype=bool)
    for p in g.spec.paths:
        if p.kind in PATH_KINDS:
            half = (p.width or 900.0) / 2.0
            roads |= g.line_dist(p.pts) < half
    pads = g.region(*PAD_IDS)
    ok = clear_of(roads, KEEP_CLEAR_CM["road"]) & clear_of(pads, KEEP_CLEAR_CM["pad"])
    ok &= clear_of(g.lake, KEEP_CLEAR_CM["lake"]) & clear_of(g.cuts, KEEP_CLEAR_CM["cut"])
    ok &= clear_of(g.dam, KEEP_CLEAR_CM["dam"]) & (g.slope < MAX_SLOPE_DEG) & ~g.bridge_fill
    for fid, r in BASE_CLEAR.items():
        f = g.feature(fid)
        for px, py in f.points:
            ok &= np.hypot(g.X - px, g.Y - py) > r
    air = next(f for f in g.spec.features if f.id == "soviet_airfield")
    (ax, ay), (bx, by) = air.raw["ends"]
    half = air.raw["width"] / 2.0 + air.raw.get("apron", 600) + 600.0
    ok &= np.where(g.line_dist([[ax, ay], [bx, by]]) < half, False, True)

    forest = g.region(*FOREST_IDS)
    frost = g.frost
    clump_field = tg.noise(91, 18) * 0.5 + 0.5
    clumps = (clump_field > 0.76) & ~forest & ~frost
    ring = frost & ~forest & ~g.lake & (tg.noise(93, 30) * 0.5 + 0.5 > 0.62)
    inside = g.inside

    trees = []
    occupied = []

    def far_enough(x, y, spacing):
        for ox, oy in occupied[-400:]:
            if (x - ox) ** 2 + (y - oy) ** 2 < spacing * spacing:
                return False
        return True

    def scatter(mask, spacing, keep, variants, label):
        rr, cc = np.nonzero(mask & ok)
        order = rng.permutation(len(rr))
        count = 0
        for k in order:
            if rng.random() > keep:
                continue
            x = g.X[rr[k], cc[k]] + rng.uniform(-50.0, 50.0)
            y = g.Y[rr[k], cc[k]] + rng.uniform(-50.0, 50.0)
            if not inside[rr[k], cc[k]] and label != "forest" and rng.random() > OUTSIDE_PLAY_KEEP:
                continue
            if not far_enough(x, y, spacing * rng.uniform(0.85, 1.25)):
                continue
            occupied.append((x, y))
            trees.append([str(rng.choice(variants)), round(float(x), 1), round(float(y), 1),
                          round(float(rng.uniform(0.0, 360.0)), 1), round(float(rng.uniform(0.85, 1.15)), 3)])
            count += 1
        return count

    counts = {
        "forest": scatter(forest, SPACING_CM["forest"], 0.08, VARIANTS_FOREST, "forest"),
        "clumps": scatter(clumps, SPACING_CM["clump"], 0.06, VARIANTS_TUNDRA, "clump"),
        "ring": scatter(ring, SPACING_CM["ring"], 0.01, ["E", "F", "F"], "ring"),
    }
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "FL_Trees.json"), "w") as h:
        json.dump({"trees": trees, "counts": counts}, h)
    img = np.zeros(tg.SHAPE + (3,), dtype=np.uint8)
    img[:] = (90, 100, 70)
    img[frost] = (200, 205, 210)
    img[g.lake] = (150, 170, 190)
    img[roads] = (130, 125, 115)
    img[~ok & ~g.lake & ~roads] = (img[~ok & ~g.lake & ~roads] * 0.6).astype(np.uint8)
    img[forest & ok] = (60, 70, 45)
    for v, x, y, *_ in trees:
        r, c = g.rc((x, y))
        img[max(r - 2, 0):r + 3, max(c - 2, 0):c + 3] = (20, 60, 20) if v != "F" else (80, 140, 60)
    Image.fromarray(img).save(os.path.join(OUT, "preview_trees.png"))
    print("trees", len(trees), counts)


if __name__ == "__main__":
    main()
