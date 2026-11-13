"""First dressing pass for Ger_2ndOreMountains: resources, dead-tree stand-ins and placeholder blocks.

    python Source/Tools/OreMountainsMap/ore_dress_gen.py      (after ore_terrain_gen.py)

Writes Saved/OreMountains/terrain/Ore_dress.json. Heights are added in the editor (ore_map_build.ground_z).
Placeholders mark every landmark that has no model in the project yet: [id, label, x, y, size_x, size_y, size_z, yaw].
"""
import json
import math
import os

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "Saved", "OreMountains", "terrain")
layout = json.load(open(os.path.join(OUT, "Ore_layout.json")))
masks = np.load(os.path.join(OUT, "Ore_masks.npz"))
lay = layout["landscape"]
B = layout["bounds"]
rng = np.random.default_rng(2)
S = layout.get("scale", 1.0)      # the map is the sketch scaled about its centre
F = {f["id"]: f for f in layout["features"]}
R = {r["id"]: r for r in layout["regions"]}


def mask_at(name, x, y):
    ix = int(round((x - lay["corner"][0]) / lay["quad_cm"]))
    iy = int(round((y - lay["corner"][1]) / lay["quad_cm"]))
    if ix < 0 or iy < 0 or ix >= lay["verts_x"] or iy >= lay["verts_y"]:
        return False
    return bool(masks[name][iy, ix])


def blocked(x, y):
    return any(mask_at(n, x, y) for n in ("road", "track", "ramps", "cliff", "pads", "gully")) or \
        abs(x) > B["xmax"] - 300 or abs(y) > B["ymax"] - 300


def scatter(mask_name, spacing, keep):
    out = []
    n = int(lay["size_x"] / spacing)
    for i in range(n):
        for j in range(n):
            x = lay["corner"][0] + (i + rng.uniform(0.1, 0.9)) * spacing
            y = lay["corner"][1] + (j + rng.uniform(0.1, 0.9)) * spacing
            if not mask_at(mask_name, x, y) or blocked(x, y) or rng.random() > keep:
                continue
            out.append([round(x), round(y), round(float(rng.uniform(0, 360)), 1), round(float(rng.uniform(0.75, 1.25)), 2),
                        int(rng.integers(0, 6))])
    return out


# The dead forest is the OreDeadForest kit (option B "Gerippe", picked by the owner 2026-11-13): crushable
# BP_OreDeadSpruce_<Piece> actors for standing trees, instanced meshes for windthrown trees. Mixes follow the kit's
# OreDeadForest_pieces.json. Tree entries: [x, y, yaw, scale, kind, variant]; kind is a kit piece name or "willow"
# (BP_Scorch_Dest_DeadWillow: a few bare broadleaf trees among the lone trees of the valley).
KIT = json.load(open(os.path.join(ROOT, "Saved", "OreMountains", "foliage", "OreDeadForest_pieces.json")))
MIX = KIT["suggested_mixes"]


def pick(shares):
    names = list(shares)
    weights = np.array([shares[n] for n in names], float)
    return names[int(rng.choice(len(names), p=weights / weights.sum()))]


def as_trees(points, shares, willow_share=0.0):
    out = []
    for x, y, yaw, scale, _ in points:
        if rng.random() < willow_share:
            out.append([x, y, yaw, round(float(rng.uniform(0.8, 1.2)), 2), "willow", int(rng.integers(0, 5))])
        else:
            out.append([x, y, yaw, 1.0, pick(shares), 0])
    return out


def lone_ok(x, y):
    """Open ground away from woods, the bench, resources and sites: single dead trees dotted over the valley."""
    if blocked(x, y) or mask_at("woods_dense", x, y) or mask_at("woods_thin", x, y) or mask_at("crest", x, y):
        return False
    if x < -8500 * S:                        # the bench and the escarpment: kept clear for the base
        return False
    for fid in ("base_a", "artillery_battery", "checkpoint", "survey_post"):
        fx, fy = F[fid]["points"][0]
        if math.hypot(x - fx, y - fy) < 4500.0:
            return False
    cx, cy = R["radixite_ground"]["centre"]
    kx_, ky_ = R["base_b_knoll"]["centre"]
    return math.hypot(x - cx, y - cy) > 5500.0 and math.hypot(x - kx_, y - ky_) > 4200.0


def scatter_where(accept, spacing, keep):
    out = []
    n = int(lay["size_x"] / spacing)
    for i in range(n):
        for j in range(n):
            x = lay["corner"][0] + (i + rng.uniform(0.1, 0.9)) * spacing
            y = lay["corner"][1] + (j + rng.uniform(0.1, 0.9)) * spacing
            if rng.random() > keep or not accept(x, y):
                continue
            out.append([round(x), round(y), round(float(rng.uniform(0, 360)), 1), 1.0, 0])
    return out


dense_pts = scatter("woods_dense", 560.0, 0.75)
thin_pts = scatter("woods_thin", 900.0, 0.62)
lone_pts = scatter_where(lone_ok, 3600.0, 0.4)
trees = as_trees(dense_pts, MIX["dense_wood"]["shares"]) + as_trees(thin_pts, MIX["thin_wood"]["shares"]) \
    + as_trees(lone_pts, MIX["lone_trees"]["shares"], willow_share=0.3)
# the rare dying survivor: in the scrub pockets and one or two per wood
for x, y, yaw, _, _ in scatter_where(lambda x, y: mask_at("scrub", x, y) and not blocked(x, y), 800.0, 0.4) + dense_pts[::70] + thin_pts[::45]:
    trees.append([x, y, yaw, 1.0, "Survivor_9m", 0])

# resources: the sketch's points are the big nodes; the Radixite field is filled in round them
res = []
for x, y in F["radixite_field"]["points"]:
    res.append(["Radixite/Resource_Radixite_Crystal_Large", x, y, float(rng.uniform(0, 360))])
centre = R["radixite_ground"]["centre"]
poly = np.array(R["radixite_ground"]["points"])


def in_poly(x, y):
    inside = False
    for (x0, y0), (x1, y1) in zip(poly, np.roll(poly, -1, axis=0)):
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
            inside = not inside
    return inside


placed = [(r[1], r[2]) for r in res]
tries = 0
want = [("Radixite/Resource_Radixite_Crystal", 9), ("Radixite/Resource_Radixite_Crystal_SmallDecal", 13)]
for name, count in want:
    done = 0
    while done < count and tries < 20000:
        tries += 1
        x = centre[0] + rng.uniform(-6500, 6500) * S
        y = centre[1] + rng.uniform(-6500, 6500) * S
        if not in_poly(x, y) or blocked(x, y) or any(math.hypot(x - a, y - b) < 750 for a, b in placed):
            continue
        placed.append((x, y))
        res.append([name, round(x), round(y), float(rng.uniform(0, 360))])
        done += 1
metal = ["Metal/Resource_Metal_Barrels", "Metal/Resource_Metal_Rails", "Metal/Resource_Metal_BrokenWaterTank"]
k = 0
for fid in ("metal_bench", "metal_valley_w"):
    for x, y in F[fid]["points"]:
        res.append([metal[k % 3], x, y, float(rng.uniform(0, 360))])
        k += 1

P = []      # placeholders


def at(fid):
    return F[fid]["points"][0]


for fid, label in (("adit_1", "MINE ADIT"), ("adit_2", "MINE ADIT (MAIN)"), ("adit_3", "MINE ADIT")):
    x, y = at(fid)
    P.append([fid, label + " portal 12 x 8 m", x + 300, y, 1000, 1200, 800, 0])
kx, ky = R["base_b_knoll"]["centre"]
P.append(["wismut_headframe", "WISMUT SHAFT HEADFRAME 12 x 12 x 25 m", kx + 900, ky + 500, 1200, 1200, 2500, 20])
P.append(["wismut_hoist_house", "HOIST HOUSE 10 x 16 m", kx + 700, ky - 1400, 1000, 1600, 600, 20])
ax, ay = R["base_a_yard"]["centre"]
P.append(["wismut_ore_plant", "WISMUT ORE PLANT 15 x 26 x 14 m", ax + 1500, ay + 1300, 1500, 2600, 1400, 0])
P.append(["wismut_conveyor", "CONVEYOR + LOADING BUNKER 6 x 22 m", ax + 2450, ay - 1500, 600, 2200, 900, 0])
P.append(["wismut_sheds", "DEPOT SHEDS 10 x 20 m", ax - 1900, ay + 1700, 1000, 2000, 600, 0])
x, y = at("cp_weighbridge")
P.append(["cp_weighbridge", "WEIGHBRIDGE DECK 4 x 18 m", x, y, 400, 1800, 40, 0])
x, y = at("cp_booth")
P.append(["cp_booth", "GUARD BOOTH 4 x 5 m", x, y, 400, 500, 300, 0])
x, y = at("cp_gate")
P.append(["cp_barrier", "BOOM BARRIER 12 m", x, y, 1200, 40, 120, 0])
for fid in ("cp_tower_n", "cp_tower_s"):
    x, y = at(fid)
    P.append([fid, "WATCHTOWER (Rus guard tower exists)", x, y, 400, 400, 900, 0])
# The viaduct. The sketch's four deck regions shrank with the map scale (their road gap came out 6.9 m) and its road
# crosses the viaduct line 7 m south of that gap. The road is what matters: the 10 m gap is centred where the road
# really crosses the line, and the parts follow the concept brief (CustomMapItems/Environment/A4Viaduct), south to
# north: end span 22.5 | fallen span 17 | stub 22.5 | gap 10 | stub 24 | fallen span 17 | end span 19 (132 m).
va = np.array(R["viaduct_south"]["points"][0], float)
vb = np.array(R["viaduct_north"]["points"][-1], float)
vd = (vb - va) / np.linalg.norm(vb - va)
vn = np.array([-vd[1], vd[0]])
road = np.array(next(p for p in layout["paths"] if p["id"] == "valley_road")["points"], float)
cross = None
for q0, q1 in zip(road[:-1], road[1:]):
    s0, s1 = float(np.dot(q0 - va, vn)), float(np.dot(q1 - va, vn))
    if s0 * s1 <= 0 and s0 != s1:
        cross = q0 + (q1 - q0) * (s0 / (s0 - s1))
        break
gap_s = float(np.dot(cross - va, vd))
viaduct_yaw = math.degrees(math.atan2(vd[1], vd[0]))
parts = (("viaduct_south", "A4 VIADUCT: standing end span", -5.0 - 22.5 - 17.0 - 22.5, 22.5),
         ("viaduct_south_pier", "A4 VIADUCT: stub at the road gap", -5.0 - 22.5, 22.5),
         ("viaduct_north_pier", "A4 VIADUCT: stub at the road gap", 5.0, 24.0),
         ("viaduct_north", "A4 VIADUCT: standing end span", 5.0 + 24.0 + 17.0, 19.0))
for rid, label, start_m, length_m in parts:
    c = va + vd * (gap_s + (start_m + length_m / 2.0) * 100.0)
    P.append([rid, label, float(c[0]), float(c[1]), length_m * 100.0, 1200, 900, viaduct_yaw])
print("viaduct: road crosses at (%.0f, %.0f), %.1f m along the line; south end at (%.0f, %.0f), north end at (%.0f, %.0f)" % (
    cross[0], cross[1], gap_s / 100.0, *(va + vd * (gap_s - 6700.0)), *(va + vd * (gap_s + 6500.0))))
x, y = at("sp_tent")
P.append(["sp_tent", "FORGE SURVEY TENT 4 x 6 m", x, y, 400, 600, 280, 30])
x, y = at("sp_cryo")
P.append(["sp_cryo", "CRYO CANISTER RACK 1.5 x 3 m", x, y, 150, 300, 200, 30])
x, y = at("sp_generator")
P.append(["sp_generator", "GENERATOR", x, y, 150, 250, 150, 30])
for i, (x, y) in enumerate(F["sp_stakes"]["points"]):
    P.append(["sp_stake_%d" % i, "" if i else "SAMPLING STAKES + FLAGS", x, y, 30, 30, 220, 0])
for i in (1, 2):
    x, y = at("smoulder_%d" % i)
    P.append(["smoulder_%d" % i, "SMOULDERING PEAT (effect)", x, y, 200, 200, 30, 0])

# notes: things that exist in the project and are placed in the detail pass or by mission scripting
N = [["hq", "PLAYER HQ (expanded)"], ["b_gamma", "GAMMA FACILITY"], ["b_refinery", "REFINERY"], ["b_barracks", "BARRACKS"],
     ["base_a", "SOVIET BASE A"], ["base_b", "SOVIET BASE B"], ["artillery_battery", "ARTILLERY BATTERY (Rus pits)"],
     ["convoy_entry", "CONVOYS ENTER"], ["survey_post", "FORGE SURVEY POST"], ["checkpoint", "CHECKPOINT"],
     ["overlook", "PROW OVERLOOK"]]
notes = [[fid, label, at(fid)[0], at(fid)[1]] for fid, label in N]
for fid in ("pit_bs3_a", "pit_bs3_b", "pit_53k", "pit_mortar_a", "pit_mortar_b", "pit_dshk_a", "pit_dshk_b", "pit_ampulomet"):
    notes.append([fid, "PIT " + F[fid]["label"], at(fid)[0], at(fid)[1]])

# rock chunks on the faces. A first try covered the faces evenly and read as lumps stuck on a wall, so the chunks
# now sit along the lip and the foot of each face (where a real crag breaks up), with only a few on the face itself.
cliff = masks["cliff"].astype(np.float64)
pad = np.pad(cliff, 3, mode="edge")
share = np.zeros_like(cliff)
for di in range(7):
    for dj in range(7):
        share += pad[di:di + cliff.shape[0], dj:dj + cliff.shape[1]]
share /= 49.0
rocks = []
step = 360.0
n = int(lay["size_x"] / step)
for i in range(n):
    for j in range(n):
        x = lay["corner"][0] + (i + rng.uniform(0.15, 0.85)) * step
        y = lay["corner"][1] + (j + rng.uniform(0.15, 0.85)) * step
        if abs(x) > B["xmax"] + 1500 or abs(y) > B["ymax"] + 1500:
            continue
        if not mask_at("cliff", x, y) or mask_at("ramps", x, y):
            continue
        ix = int(round((x - lay["corner"][0]) / lay["quad_cm"]))
        iy = int(round((y - lay["corner"][1]) / lay["quad_cm"]))
        edge = share[iy, ix] < 0.78
        if not edge:
            continue
        rocks.append([round(x), round(y), round(float(rng.uniform(0, 360)), 1), round(float(rng.uniform(-10, 10)), 1),
                      round(float(rng.uniform(-10, 10)), 1), round(float(rng.uniform(1.3, 2.4)), 2), int(rng.integers(0, 6))])

json.dump({"trees": trees, "rocks": rocks, "resources": res, "placeholders": P, "notes": notes},
          open(os.path.join(OUT, "Ore_dress.json"), "w"))
print("rocks %d" % len(rocks))
kinds = {}
for t in trees:
    kinds[t[4]] = kinds.get(t[4], 0) + 1
print("tree kinds", kinds)
print("trees %d, resources %d, placeholders %d, notes %d" % (len(trees), len(res), len(P), len(notes)))
