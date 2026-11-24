"""Placement plan for the crushable rocks (Environment/OreMountains/CrushableRocks) on Ger_2ndOreMountains.

    python Source/Tools/OreMountainsMap/ore_crush_rocks_gen.py      (after ore_terrain_gen.py and ore_detail_gen.py)

Writes Saved/OreMountains/terrain/Ore_crush_rocks.json:
    rocks  [name, x, y, yaw, scale, sink_cm, zone]     name = S01_Block ... M08_Long (Blueprint BP_OreRock_<name>)
Own seed and own file: the committed scatter in Ore_detail.json is not touched. Heights are added in the editor.

Rocks lie where something put them: rockfall at the cliff feet, on the ramp fans, in the dry wash, on the scoured
crests, on crater rims, a few kicked to the road shoulders, a few strays on the foothills. Every rock is a Blueprint
actor with a nav area (infantry path round the small ones, infantry and light vehicles round the medium ones), so
the plan keeps gaps: 3 m clear round a medium rock, 1 m round a small one, nothing on roads, tracks, ramps, pads,
gullies, bogs, the spur, the viaduct, bases, resources, adits or the player's start.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "Saved", "OreMountains", "terrain")
layout = json.load(open(os.path.join(OUT, "Ore_layout.json")))
dress = json.load(open(os.path.join(OUT, "Ore_dress.json")))
detail = json.load(open(os.path.join(OUT, "Ore_detail.json")))
cliffs = json.load(open(os.path.join(OUT, "Ore_cliffs.json")))
spur = json.load(open(os.path.join(ROOT, "Saved", "OreMountains", "viaduct", "A4ViaductSpur_pieces.json")))
with np.load(os.path.join(OUT, "Ore_masks.npz")) as archive:
    masks = {name: archive[name] for name in archive.files}
lay = layout["landscape"]
B = layout["bounds"]
V = lay["verts_x"]
Q = lay["quad_cm"]
H = (np.fromfile(layout["files"]["height"], dtype="<u2").reshape(V, V).astype(np.float64) - 32768.0) * 100.0 / 128.0   # [iy, ix]
GY, GX = np.gradient(H, Q)
SLOPE = np.degrees(np.arctan(np.hypot(GX, GY)))
rng = np.random.default_rng(1632)

SMALL = ("S01_Block", "S02_Wedge", "S03_Slab", "S04_Chunk", "S05_Split", "S06_Shard", "S07_Low", "S08_Pair")
MEDIUM = ("M01_Block", "M02_Wedge", "M03_Slab", "M04_Split", "M05_Stack", "M06_Chunk", "M07_Tilted", "M08_Long")
RADIUS = {"S": 50.0, "M": 125.0}            # half the longest side, cm
CLEAR = {"S": 100.0, "M": 300.0}            # free ground kept round a rock
MAX_SLOPE = {"S": 16.0, "M": 11.0}


def grow(mask, metres):
    """Boolean mask grown by a number of metres (1 cell = 1 m)."""
    size = int(metres) * 2 + 1
    img = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(size))
    return np.asarray(img) > 127


def cell(x, y):
    ix = int(round((x - lay["corner"][0]) / Q))
    iy = int(round((y - lay["corner"][1]) / Q))
    return min(max(ix, 0), V - 1), min(max(iy, 0), V - 1)


def world(ix, iy):
    return lay["corner"][0] + ix * Q, lay["corner"][1] + iy * Q


def stamp(mask, x, y, radius):
    ix, iy = cell(x, y)
    r = int(math.ceil(radius / Q))
    y0, y1, x0, x1 = max(0, iy - r), min(V, iy + r + 1), max(0, ix - r), min(V, ix + r + 1)
    yy, xx = np.ogrid[y0:y1, x0:x1]
    mask[y0:y1, x0:x1] |= (xx - ix) ** 2 + (yy - iy) ** 2 <= r * r


def stamp_line(mask, points, radius):
    for (x0, y0), (x1, y1) in zip(points[:-1], points[1:]):
        steps = max(1, int(math.hypot(x1 - x0, y1 - y0) / (radius * 0.5)))
        for k in range(steps + 1):
            stamp(mask, x0 + (x1 - x0) * k / steps, y0 + (y1 - y0) * k / steps, radius)


def stamp_poly(mask, pts, margin):
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    for x in np.arange(min(xs), max(xs) + 1.0, Q):
        for y in np.arange(min(ys), max(ys) + 1.0, Q):
            inside, j = False, len(pts) - 1
            for i in range(len(pts)):
                if (pts[i][1] > y) != (pts[j][1] > y) and x < (pts[j][0] - pts[i][0]) * (y - pts[i][1]) / (pts[j][1] - pts[i][1]) + pts[i][0]:
                    inside = not inside
                j = i
            if inside:
                stamp(mask, x, y, margin)


# ---- keep-out: where no rock may lie ------------------------------------------------------------------------------------
keep = np.zeros((V, V), bool)
keep |= grow(masks["road"], 5) | grow(masks["track"], 4) | grow(masks["wear"], 3) | grow(masks["ramps"], 7)
keep |= grow(masks["pads"], 4) | grow(masks["cliff"], 2) | grow(masks["escarpment"], 2)
keep |= grow(masks["viaduct_deck"], 8) | grow(masks["viaduct_rubble"], 3) | grow(masks["gully"], 4)
keep |= grow(masks["peat_bog"], 2) | masks["bog_belt"] | grow(masks["spoil_heap"], 1) | masks["crater"] | grow(masks["ditch_lip"], 3)
stamp_line(keep, layout["base_b_road"], 900.0)
for track in layout["wear_tracks"]:
    stamp_line(keep, track, 450.0)
for p in layout["paths"]:
    stamp_line(keep, p["points"], 700.0 if p["id"] != "cp_fence" else 400.0)
for rid in ("base_a_yard", "battery_pad", "checkpoint_yard", "radixite_ground", "viaduct_south_pier", "viaduct_north_pier"):
    region = next(r for r in layout["regions"] if r["id"] == rid)
    if region.get("points"):
        stamp_poly(keep, region["points"], 500.0)
FEATURE_RADIUS = {"player_start": 3200.0, "resource": 2200.0, "enemy_group": 900.0, "enemy_base": 1500.0, "enemy_spawn": 1500.0,
                  "capture": 1000.0, "gate": 1500.0, "wreck_block": 1500.0, "ramp": 1200.0, "poi": 1200.0, "defense_point": 800.0}
for f in layout["features"]:
    radius = FEATURE_RADIUS.get(f["kind"])
    if f["id"].startswith(("adit_", "fossil_roots", "sp_", "cp_", "smoulder", "b_")):
        radius = max(radius or 0.0, 1400.0)
    if radius:
        for x, y in f["points"]:
            stamp(keep, x, y, radius)
for item in dress["resources"]:
    stamp(keep, item[1], item[2], 900.0)
for item in dress["placeholders"]:
    stamp(keep, item[2], item[3], max(item[4], item[5]) * 0.5 + 500.0)
stamp_poly(keep, spur["footprint_polygon_world_cm"], 800.0)
stamp_poly(keep, spur["approach_slab_polygon_world_cm"], 1500.0)
for adit in cliffs["adit_openings"]:
    stamp(keep, adit["x"], adit["y"], 1800.0)
edge = np.ones((V, V), bool)
x0, y0 = cell(B["xmin"] + 400.0, B["ymin"] + 400.0)
x1, y1 = cell(B["xmax"] - 400.0, B["ymax"] - 400.0)
edge[y0:y1, x0:x1] = False
keep |= edge

# things a rock must stay clear of by its own margin (trees, props, kit pieces): tested per rock
obstacles = [(t[0], t[1], 60.0) for t in dress["trees"]]
obstacles += [(p[3], p[4], 250.0 if p[0] == "bp" else 120.0) for p in detail["props"]]
OBS = np.array(obstacles, float)

# ---- zones: (name, candidate mask, clumps wanted, chance of a medium rock in the clump, small rocks per clump) -----------
cliff_any = masks["cliff"] | masks["escarpment"]
# rockfall lies at the foot, not on the lip: the ground must be well below the top of the face beside it
_top = np.where(cliff_any, H, -1e6).astype(np.float32)
_top = np.asarray(Image.fromarray(_top, mode="F").filter(ImageFilter.MaxFilter(23)))
talus = grow(cliff_any, 11) & ~grow(cliff_any, 4) & (H < _top - 250.0)
shoulder = grow(masks["road"], 13) & ~grow(masks["road"], 6)
open_ground = ~(masks["woods_dense"] | masks["crest"] | grow(cliff_any, 14) | grow(masks["road"], 14))
ZONES = [
    ("talus", talus, 26, 0.75, (1, 4)),
    ("ramp_fan", masks["ramp_fan"] | masks["fan"], 9, 0.35, (2, 4)),
    ("wash", grow(masks["wash"] | masks["wash_bar"] | masks["wash_foot"], 2), 9, 0.35, (1, 3)),
    ("crest", masks["relief_crest"] | masks["pavement"], 14, 0.4, (1, 3)),
    ("crater_rim", grow(masks["crater_rim"], 1), 8, 0.0, (1, 2)),
    ("shoulder", shoulder, 16, 0.15, (1, 2)),
    ("stray", open_ground, 14, 0.3, (0, 2)),
]

placed = []          # (x, y, class)


def fits(x, y, cls):
    if not (B["xmin"] < x < B["xmax"] and B["ymin"] < y < B["ymax"]):
        return False
    ix, iy = cell(x, y)
    if keep[iy, ix] or SLOPE[iy, ix] > MAX_SLOPE[cls]:
        return False
    reach = RADIUS[cls] + CLEAR[cls]
    d = np.hypot(OBS[:, 0] - x, OBS[:, 1] - y) - OBS[:, 2]
    if (d < reach).any():
        return False
    for px, py, pc in placed:
        gap = math.hypot(px - x, py - y) - RADIUS[cls] - RADIUS[pc]
        if gap < (CLEAR["M"] if "M" in (cls, pc) and cls == pc else CLEAR["S"]):
            return False
    return True


def add(name, x, y, zone):
    cls = name[0]
    ix, iy = cell(x, y)
    scale = float(rng.uniform(0.85, 1.2))
    # flat base on a slope: sink by the drop across half the rock so the downhill edge does not hover (capped)
    sink = min(18.0 if cls == "M" else 9.0, math.tan(math.radians(SLOPE[iy, ix])) * RADIUS[cls] * scale * 0.8) + 2.0
    placed.append((x, y, cls))
    rocks.append([name, round(x), round(y), round(float(rng.uniform(0.0, 360.0)), 1), round(scale, 2), round(sink, 1), zone])


rocks = []
centres = []
used = {"S": 0, "M": 0}


def pick(cls):
    names = SMALL if cls == "S" else MEDIUM
    used[cls] += 1
    return names[(used[cls] * 5 + int(rng.integers(0, 3))) % 8]      # walks the set so all sixteen appear


for zone, mask, clumps, medium_chance, small_range in ZONES:
    iys, ixs = np.nonzero(mask & ~keep)
    if len(iys) == 0:
        print("zone", zone, "has no free cells")
        continue
    done = tries = 0
    while done < clumps and tries < clumps * 60:
        tries += 1
        k = int(rng.integers(0, len(iys)))
        x, y = world(ixs[k], iys[k])
        x, y = x + float(rng.uniform(-40, 40)), y + float(rng.uniform(-40, 40))
        if any(math.hypot(px - x, py - y) < 1400.0 for px, py in centres):
            continue
        lead = "M" if rng.random() < medium_chance else "S"
        if not fits(x, y, lead):
            continue
        add(pick(lead), x, y, zone)
        centres.append((x, y))                # keeps clumps apart
        for _ in range(int(rng.integers(small_range[0], small_range[1] + 1))):
            for _try in range(12):
                ang, dist = float(rng.uniform(0, 2 * math.pi)), float(rng.uniform(170.0, 420.0)) + (RADIUS[lead] - 50.0)
                sx, sy = x + math.cos(ang) * dist, y + math.sin(ang) * dist
                if fits(sx, sy, "S"):
                    add(pick("S"), sx, sy, zone)
                    break
        done += 1
    print("zone %-10s clumps %2d of %2d (%d tries)" % (zone, done, clumps, tries))

count = {"S": sum(1 for r in rocks if r[0][0] == "S"), "M": sum(1 for r in rocks if r[0][0] == "M")}
by_name = {}
for r in rocks:
    by_name[r[0]] = by_name.get(r[0], 0) + 1
json.dump({"rocks": rocks, "count": count, "by_name": by_name,
           "blueprint": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreRock_<name>"},
          open(os.path.join(OUT, "Ore_crush_rocks.json"), "w"), indent=1)
print("rocks", len(rocks), count, "variants used", len(by_name))

# preview: rocks over the keep-out mask
img = np.zeros((V, V, 3), np.uint8)
img[...] = 60
img[keep] = (30, 30, 30)
img[cliff_any] = (110, 100, 80)
img[masks["road"]] = (90, 90, 140)
big = Image.fromarray(img[::-1]).resize((V * 2, V * 2), Image.NEAREST)
px = big.load()
for name, x, y, *_ in rocks:
    ix, iy = cell(x, y)
    r = 3 if name[0] == "M" else 1
    for dx in range(-r, r + 1):
        for dy in range(-r, r + 1):
            cx, cy = ix * 2 + dx, (V - 1 - iy) * 2 + dy
            if 0 <= cx < V * 2 and 0 <= cy < V * 2:
                px[cx, cy] = (255, 170, 60) if name[0] == "M" else (255, 240, 120)
big.save(os.path.join(ROOT, "Saved", "OreMountains", "rocks", "placement_plan.png"))
