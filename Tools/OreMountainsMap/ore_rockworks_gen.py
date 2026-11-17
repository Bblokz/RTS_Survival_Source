"""Placement plan for the large rockworks (Environment/OreMountains/Rockworks) on Ger_2ndOreMountains.

    python Source/Tools/OreMountainsMap/ore_rockworks_gen.py      (after ore_crush_rocks_gen.py)

Writes Saved/OreMountains/terrain/Ore_rockworks.json:
    rocks  [name, x, y, yaw, sink_cm, zone]     name = R01_Outcrop ... R08_Spur (Blueprint BP_OreRockwork_<name>)
The rockworks cannot be crushed and block every unit (NavArea_Null), so they are few and stand alone as landmarks:
at least 6.5 m of free ground all round (a heavy tank is 4 m wide), 60 m between two of them, none on the player's
building ground. Scale 1 only: a scaled rock stretches its shell holes.

The keep-out mask is the crushable rocks' one: this script runs the first part of ore_crush_rocks_gen.py (up to its
zones) so both plans share one definition.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_source = open(os.path.join(HERE, "ore_crush_rocks_gen.py"), encoding="utf-8").read()
_head = _source.split("# ---- zones:")[0]
assert _head != _source, "zones marker missing in ore_crush_rocks_gen.py"
exec(compile(_head, "ore_crush_rocks_gen.py (keep-out part)", "exec"))      # layout, masks, H, SLOPE, keep, OBS, helpers

ROCKS = json.load(open("D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRockworks/out/rocks.json"))
NAMES = sorted(ROCKS)
HALF = {n: max(ROCKS[n]["dims"][0], ROCKS[n]["dims"][1]) * 50.0 for n in NAMES}        # cm
CLEAR = 650.0
APART = 6000.0
MAX_SLOPE = 7.0
rng = np.random.default_rng(1947)
crush = json.load(open(os.path.join(OUT, "Ore_crush_rocks.json")))["rocks"]
CR = np.array([[r[1], r[2]] for r in crush], float)

keep_big = keep.copy()
hq = next(f for f in layout["features"] if f["id"] == "hq")
stamp(keep_big, hq["points"][0][0], hq["points"][0][1], hq.get("radius") or 5700.0)
keep_big |= grow(masks["woods_dense"], 2) | grow(masks["wash"], 6) | grow(masks["crater_rim"] | masks["crater"], 4)
keep_far = grow(keep_big, 12)                 # centre this far from anything kept clear: half a rock plus the gap

cliff_any = masks["cliff"] | masks["escarpment"]
_top = np.where(cliff_any, H, -1e6).astype(np.float32)
_top = np.asarray(Image.fromarray(_top, mode="F").filter(ImageFilter.MaxFilter(61)))
ZONES = [
    ("cliff_foot", grow(cliff_any, 30) & (H < _top - 250.0), 6),
    ("crest", grow(masks["relief_crest"] | masks["pavement"], 3), 4),
    ("open", ~(masks["crest"] | grow(cliff_any, 30)), 5),
]
rocks, placed = [], []
order = list(rng.permutation(len(NAMES))) + list(rng.permutation(len(NAMES)))        # every rockwork once, then again


def fits(name, x, y):
    ix, iy = cell(x, y)
    if keep_far[iy, ix]:
        return False
    r = int(HALF[name] / Q) + 1
    if SLOPE[max(0, iy - r):iy + r + 1, max(0, ix - r):ix + r + 1].max() > MAX_SLOPE * 1.6 or SLOPE[iy, ix] > MAX_SLOPE:
        return False
    reach = HALF[name] + CLEAR
    if (np.hypot(OBS[:, 0] - x, OBS[:, 1] - y) - OBS[:, 2] < reach).any():
        return False
    if (np.hypot(CR[:, 0] - x, CR[:, 1] - y) < HALF[name] + 450.0).any():
        return False
    return all(math.hypot(px - x, py - y) > APART for px, py in placed)


for zone, mask, wanted in ZONES:
    iys, ixs = np.nonzero(mask & ~keep_far)
    done = tries = 0
    while done < wanted and tries < 4000 and len(iys):
        tries += 1
        k = int(rng.integers(0, len(iys)))
        x, y = world(ixs[k], iys[k])
        name = NAMES[order[len(rocks) % len(order)]]
        if not fits(name, x, y):
            continue
        ix, iy = cell(x, y)
        sink = min(35.0, math.tan(math.radians(SLOPE[iy, ix])) * HALF[name] * 0.8) + 5.0
        rocks.append([name, round(x), round(y), round(float(rng.uniform(0.0, 360.0)), 1), round(sink, 1), zone])
        placed.append((x, y))
        done += 1
    print("zone %-10s %d of %d (%d tries, %d free cells)" % (zone, done, wanted, tries, len(iys)))

by_name = {}
for r in rocks:
    by_name[r[0]] = by_name.get(r[0], 0) + 1
json.dump({"rocks": rocks, "by_name": by_name,
           "blueprint": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks/Blueprints/BP_OreRockwork_<name>"},
          open(os.path.join(OUT, "Ore_rockworks.json"), "w"), indent=1)
print("rockworks", len(rocks), by_name)
for r in rocks:
    print("  ", r)

img = np.zeros((V, V, 3), np.uint8)
img[...] = 60
img[keep_far] = (45, 45, 45)
img[keep_big] = (30, 30, 30)
img[cliff_any] = (110, 100, 80)
img[masks["road"]] = (90, 90, 140)
for name, x, y, *_ in rocks:
    ix, iy = cell(x, y)
    img[iy - 4:iy + 5, ix - 4:ix + 5] = (255, 120, 40)
Image.fromarray(img[::-1]).resize((V * 2, V * 2), Image.NEAREST).save(
    os.path.join(ROOT, "Saved", "OreMountains", "rockworks", "placement_plan.png"))
