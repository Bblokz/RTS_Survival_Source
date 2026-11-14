"""Second dressing pass for Ger_2ndOreMountains, from content that already exists in the project.

    python Source/Tools/OreMountainsMap/ore_detail_gen.py      (after ore_terrain_gen.py)

Writes Saved/OreMountains/terrain/Ore_detail.json:
    props    [kind, asset, label, x, y, yaw, scale, dz]   kind = bp (Blueprint actor), mesh (static mesh, no collision),
                                                          niagara (effect)
    scatter  {name: {"mesh": path, "items": [[x, y, yaw, scale], ...]}}   instanced, no collision, no nav effect
Heights are added in the editor (ore_map_build.ground_z).
"""
import json
import math
import os

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "Saved", "OreMountains", "terrain")
layout = json.load(open(os.path.join(OUT, "Ore_layout.json")))
with np.load(os.path.join(OUT, "Ore_masks.npz")) as archive:
    masks = {name: archive[name] for name in archive.files}     # an NpzFile unpacks the array again on every access
lay = layout["landscape"]
B = layout["bounds"]
V = lay["verts_x"]
H = (np.fromfile(layout["files"]["height"], dtype="<u2").reshape(V, V).astype(np.float64) - 32768.0) * 100.0 / 128.0   # [iy, ix]
rng = np.random.default_rng(7)
F = {f["id"]: f for f in layout["features"]}
R = {r["id"]: r for r in layout["regions"]}
PATHS = {p["id"]: np.array(p["points"], float) for p in layout["paths"]}

ENV = "/Game/RTS_Survival/Blueprints/Environment/"
DEST = ENV + "DestructableEnvActor/"
RUS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/"
FOLIAGE = "/Game/RTS_Survival/Meshes/Nature/Foliage/"

props = []
scatter = {}
SANDBAG_FACE = -80.0   # the LongArc bulges about 80 degrees clockwise of its yaw (read off a top-down close-up)


def cell(x, y):
    ix = int(round((x - lay["corner"][0]) / lay["quad_cm"]))
    iy = int(round((y - lay["corner"][1]) / lay["quad_cm"]))
    return min(max(ix, 0), V - 1), min(max(iy, 0), V - 1)


def mask_at(name, x, y):
    ix, iy = cell(x, y)
    return bool(masks[name][iy, ix])


def dist_poly(pts, x, y):
    best = 1e9
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        dx, dy = x1 - x0, y1 - y0
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy + 1e-9)))
        best = min(best, math.hypot(x - x0 - t * dx, y - y0 - t * dy))
    return best


def road_dist(x, y):
    return dist_poly(PATHS["valley_road"], x, y)


def track_dist(x, y):
    return min(dist_poly(PATHS["ammo_route"], x, y), dist_poly(PATHS["depot_track"], x, y))


def inside_play(x, y, margin=300.0):
    return abs(x) < B["xmax"] - margin and abs(y) < B["ymax"] - margin


def free(x, y, road=900.0):
    """Open ground: not on a road, track, ramp, face or flat building pad."""
    return inside_play(x, y) and road_dist(x, y) > road and track_dist(x, y) > 500.0 and \
        not any(mask_at(n, x, y) for n in ("ramps", "cliff", "pads", "wear"))


def bp(asset, label, x, y, yaw=0.0, scale=1.0, dz=0.0):
    props.append(["bp", asset, label, float(x), float(y), float(yaw), float(scale), float(dz)])


def mesh(asset, label, x, y, yaw=0.0, scale=1.0, dz=0.0):
    props.append(["mesh", asset, label, float(x), float(y), float(yaw), float(scale), float(dz)])


def grid_scatter(name, asset, spacing, keep, accept, lo=0.8, hi=1.3):
    items = scatter.setdefault(name, {"mesh": asset, "items": []})["items"]
    n = int(lay["size_x"] / spacing)
    for i in range(n):
        for j in range(n):
            x = lay["corner"][0] + (i + rng.uniform(0.1, 0.9)) * spacing
            y = lay["corner"][1] + (j + rng.uniform(0.1, 0.9)) * spacing
            if rng.random() > keep or not accept(x, y):
                continue
            items.append([round(x), round(y), round(float(rng.uniform(0, 360)), 1), round(float(rng.uniform(lo, hi)), 2)])


def _smooth_field(seed, cells):
    from PIL import Image
    r = np.random.default_rng(seed)
    n = int(V / cells) + 3
    img = Image.fromarray(r.random((n, n)).astype(np.float32), mode="F").resize((int(n * cells), int(n * cells)), Image.BICUBIC)
    return np.asarray(img)[:V, :V]


CLUMP_FIELD = _smooth_field(901, 3.0)          # 2-4 m clumps
PATCH_FIELD = _smooth_field(902, 14.0)         # which patches exist at all


def stone_scatter(name, asset, spacing, accept, lo, hi, clump_at=0.56, patch_at=0.0, straggle=0.06, sink=0.3, grade=None):
    """Critic round 5: stones in clumps of touching pieces (2-4 m noise), a few stragglers, a size mix of 70 % small,
    25 % medium, 5 % large, each sunk about a third into the ground. grade(x, y) -> 0..1 shifts the mix to large."""
    group = scatter.setdefault(name, {"mesh": asset, "items": [], "sink": sink})
    n = int(lay["size_x"] / spacing)
    for i in range(n):
        for j in range(n):
            x = lay["corner"][0] + (i + rng.uniform(0.0, 1.0)) * spacing
            y = lay["corner"][1] + (j + rng.uniform(0.0, 1.0)) * spacing
            if not accept(x, y):
                continue
            ix, iy = cell(x, y)
            if PATCH_FIELD[iy, ix] < patch_at:
                continue
            if CLUMP_FIELD[iy, ix] < clump_at and rng.random() > straggle:
                continue
            roll = rng.random()
            big = grade(x, y) if grade else 0.0
            if roll < 0.70 - 0.5 * big:
                s = rng.uniform(lo, lo + 0.25 * (hi - lo))
            elif roll < 0.95 - 0.25 * big:
                s = rng.uniform(lo + 0.25 * (hi - lo), lo + 0.6 * (hi - lo))
            else:
                s = rng.uniform(lo + 0.6 * (hi - lo), hi)
            group["items"].append([round(x), round(y), round(float(rng.uniform(0, 360)), 1), round(float(s), 2)])


def at(fid):
    return F[fid]["points"][0]


def along(bearing_deg):
    """Unit vector (x north, y east) for a compass bearing."""
    b = math.radians(bearing_deg)
    return np.array([math.cos(b), math.sin(b)])


# ---- Base A: the Wismut yard --------------------------------------------------------------------------------------
yard = np.array(R["base_a_yard"]["points"], float)
x0, x1 = yard[:, 0].min(), yard[:, 0].max()
y0, y1 = yard[:, 1].min(), yard[:, 1].max()
ax, ay = (x0 + x1) / 2.0, (y0 + y1) / 2.0
# buildings stand north of the road (the road runs through the south part of the yard)
bp(DEST + "Industrial/BP_Industrial_Dest_Hangar", "Yard_Hangar", ax + 750.0, y0 + 1050.0, 0.0)     # 6 m clear of the road
bp(DEST + "PostApo/BP_Dest_PostApo_Chimney", "Yard_Chimney", x1 - 450.0, y1 - 450.0, 0.0)
bp(DEST + "PostApo/BP_Dest_PostApo_WaterTower", "Yard_WaterTower", ax - 350.0, y1 - 500.0, 0.0)
bp(RUS + "FuelStorage/BP_Dest_RusFuel1", "Yard_Fuel", ax - 250.0, ay - 1100.0, 90.0)
for k, (dx, dy, name) in enumerate(((-350.0, 500.0, "Airport/BP_Airport_Dest_CrateStack01"), (-300.0, 1500.0, "Airport/BP_Airport_Dest_BarrelStack3"),
                                    (-380.0, -2300.0, "Airport/BP_Airport_Dest_CrateStack01"), (350.0, 2600.0, "Airport/BP_Airport_Dest_BarrelStack3"))):
    if road_dist(ax + dx, ay + dy) > 800.0:
        bp(DEST + name, "Yard_Stack_%d" % k, ax + dx, ay + dy, float(rng.uniform(0, 360)))
for k, (x, y, yaw) in enumerate(((x0 + 450.0, y0 + 1400.0, 0.0), (x0 + 450.0, y1 - 1500.0, 8.0))):
    if road_dist(x, y) > 800.0:
        bp(DEST + "Industrial/BP_Industrial_Dest_Container", "Yard_Container_%d" % k, x, y, yaw)
# perimeter: concrete fence panels (3.4 m, long axis Y), broken in places, open where the road and the ammo track pass
fence_count = 0
run = True
for edge in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
    (ex0, ey0), (ex1, ey1) = edge
    length = math.hypot(ex1 - ex0, ey1 - ey0)
    yaw = math.degrees(math.atan2(ey1 - ey0, ex1 - ex0)) + 90.0
    for k in range(int(length // 340.0)):
        t = (k + 0.5) * 340.0 / length
        x, y = ex0 + (ex1 - ex0) * t, ey0 + (ey1 - ey0) * t
        if rng.random() < 0.12:
            run = not run                                  # stretches of the old fence are gone
        if road_dist(x, y) < 950.0 or dist_poly(PATHS["ammo_route"], x, y) < 750.0 or not run and rng.random() < 0.8:
            continue
        bp(DEST + "Military/ConcreteWall/BP_Dest_PostApo_ConcreteFence", "Yard_Fence_%02d" % fence_count, x, y, yaw)
        fence_count += 1

# ---- artillery battery: supplies at the rear, sandbags on the rim that faces the valley -----------------------------
bx, by = at("artillery_battery")
fire = along(200.0)
side = np.array([-fire[1], fire[0]])
rear = np.array([bx, by]) - fire * 2000.0
bp(RUS + "AmmoStorageMeshes/BP_Dest_RusSupplyStorage", "Battery_Supply", rear[0], rear[1], 20.0)
for k, off in enumerate((-1400.0, 1500.0)):
    p = rear + side * off + fire * 300.0
    bp(DEST + "Airport/BP_Airport_Dest_CrateStack01", "Battery_Crates_%d" % k, p[0], p[1], 200.0 + 15.0 * k)
for k, off in enumerate((-1500.0, 0.0, 1500.0)):
    p = np.array([bx, by]) + fire * 2500.0 + side * off
    bp(DEST + "NaturalCover/SandBagWalls/BP_Cover_RusSandbags_LongArc", "Battery_Sandbags_%d" % k, p[0], p[1], 200.0 + SANDBAG_FACE)
p = np.array([bx, by]) - fire * 300.0 + side * 2600.0
bp(RUS + "CamoNets/BP_Dest_RusCamoNetRegular", "Battery_CamoNet", p[0], p[1], 20.0)

# ---- Base B on the knoll: sandbag arcs at the three gun positions on the rim, stores by the shaft ---------------------
kx, ky = R["base_b_knoll"]["centre"]
for k, (gx, gy) in enumerate(F["base_b_at"]["points"]):
    out = math.degrees(math.atan2(gy - ky, gx - kx))             # yaw of the direction from the knoll centre to the gun
    d = np.array([gx - kx, gy - ky]) / math.hypot(gx - kx, gy - ky)
    bp(DEST + "NaturalCover/SandBagWalls/BP_Cover_RusSandbags_LongArc", "BaseB_Sandbags_%d" % k, gx + d[0] * 420.0, gy + d[1] * 420.0, out + SANDBAG_FACE)
bp(DEST + "Airport/BP_Airport_Dest_CrateStack01", "BaseB_Crates", kx - 900.0, ky + 1500.0, 40.0)
bp(DEST + "Airport/BP_Airport_Dest_BarrelStack3", "BaseB_Barrels", kx - 300.0, ky + 2200.0, 110.0)
bp(RUS + "CamoNets/BP_Dest_RusCamoNetRegular", "BaseB_CamoNet", kx - 1500.0, ky - 300.0, 200.0)

# ---- the bench: stores left at the adit mouths ------------------------------------------------------------------------
for k, (fid, items) in enumerate((("adit_2", ((1300.0, -900.0, "Airport/BP_Airport_Dest_CrateStack01"), (1500.0, 800.0, "Airport/BP_Airport_Dest_BarrelStack3"))),
                                  ("adit_3", ((1300.0, 700.0, "Airport/BP_Airport_Dest_CrateStack01"),)))):
    ox, oy = at(fid)
    for j, (dx, dy, name) in enumerate(items):
        bp(DEST + name, "Adit_Stores_%d_%d" % (k, j), ox + dx, oy + dy, float(rng.uniform(0, 360)))

# ---- Forge survey post ------------------------------------------------------------------------------------------
tx, ty = at("sp_tent")
bp(RUS + "Tent/BP_Dest_RusSmallTent1", "Survey_Tent", tx - 300.0, ty + 500.0, 30.0)
bp(DEST + "Airport/BP_Airport_Dest_CrateStack01", "Survey_Crates", tx + 900.0, ty - 300.0, 60.0)

# ---- smouldering peat ---------------------------------------------------------------------------------------------
wisp = ENV + "Berlin/Atmosphere/NS_BerlinAtmo_Smoulder_Wisp"
for k, fid in enumerate(("smoulder_1", "smoulder_2")):
    sx, sy = at(fid)
    props.append(["niagara", wisp, "Smoulder_%d" % k, sx, sy, 0.0, 1.0, 20.0])
    for j in range(2):
        props.append(["niagara", wisp, "Smoulder_%d_%d" % (k, j), sx + float(rng.uniform(-900, 900)), sy + float(rng.uniform(-900, 900)), 0.0, 1.0, 20.0])

# ---- valley road: old power line on the north verge, 1945 wrecks and cars -------------------------------------------
road = PATHS["valley_road"]
seg = np.hypot(*(road[1:] - road[:-1]).T)
cum = np.concatenate([[0.0], np.cumsum(seg)])
gate = at("cp_gate")
via = np.array([-470.0, -4303.0])                 # where the road passes under the viaduct


def road_point(s):
    k = int(np.searchsorted(cum, s) - 1)
    k = min(max(k, 0), len(seg) - 1)
    t = (s - cum[k]) / seg[k]
    p = road[k] + (road[k + 1] - road[k]) * t
    d = (road[k + 1] - road[k]) / seg[k]
    return p, d


pole = DEST + "Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric2"
s, k = 1500.0, 0
while s < cum[-1] - 1000.0:
    p, d = road_point(s)
    n = np.array([-d[1], d[0]])
    if n[0] < 0:
        n = -n                                    # the north side of the road
    q = p + n * 850.0
    near_site = math.hypot(q[0] - gate[0], q[1] - gate[1]) < 1500.0 or math.hypot(q[0] - via[0], q[1] - via[1]) < 2800.0 or \
        (x0 - 300.0 < q[0] < x1 + 300.0 and y0 - 300.0 < q[1] < y1 + 300.0)
    if not near_site and rng.random() > 0.2 and inside_play(q[0], q[1]):
        bp(pole, "Road_Pole_%02d" % k, q[0], q[1], math.degrees(math.atan2(d[1], d[0])) + float(rng.uniform(-6, 6)))
        k += 1
    s += 4200.0

wrecks = DEST + "Military/DestTanks/"
cars = ENV + "Props/OldCars/Cover/BP_Cover_OldCar_"
s_via = float(cum[np.argmin(np.hypot(*(road - via).T))])
for label, asset, ds, lateral, yaw in (("Wreck_PzIII", wrecks + "BP_Env_DestroyedPzIII", 2600.0, -1100.0, 35.0),
                                       ("Wreck_Pz38T", wrecks + "BP_Env_DestroyedPz38T", -3400.0, 1250.0, 140.0),
                                       ("Wreck_T34", wrecks + "BP_Env_DestroyedT34", 9500.0, 1150.0, 250.0),
                                       ("Wreck_PzII", wrecks + "BP_Env_DestroyedPzII", -15500.0, -1300.0, 80.0),
                                       ("Car_Sedan", cars + "Sedan", 1900.0, 780.0, 12.0), ("Car_Pickup", cars + "Pickup", -1500.0, -800.0, 170.0),
                                       ("Car_Limousine", cars + "Limousine", -6200.0, 760.0, 185.0), ("Car_PanelVan", cars + "PanelVan", 6800.0, -790.0, 20.0)):
    p, d = road_point(min(max(s_via + ds, 300.0), cum[-1] - 300.0))
    n = np.array([-d[1], d[0]])
    q = p + n * lateral
    if inside_play(q[0], q[1]) and not mask_at("ramps", q[0], q[1]):
        bp(asset, label, q[0], q[1], math.degrees(math.atan2(d[1], d[0])) + yaw)

# ---- the bench: old mine railway from the west adit to the tip by the bench metal ------------------------------------
a1 = np.array(at("adit_1"), float)
tip = np.array(F["metal_bench"]["points"][0], float)
start = a1 + np.array([900.0, 0.0])
d = (tip - start) / np.linalg.norm(tip - start)
track_yaw = math.degrees(math.atan2(d[1], d[0]))
PANEL = 550.0                                    # the 10 m track panel at 0.55 scale (narrow gauge)
panels = int((np.linalg.norm(tip - start) - 600.0) // PANEL)
for k in range(max(panels, 0)):
    p = start + d * (k * PANEL)
    mesh("/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01", "Mine_Track_%d" % k, p[0], p[1], track_yaw, 0.55, 55.0)
cart = "/Game/Megascans/3D_Assets/Mine_Cart_ufmodhpfa/S_Mine_Cart_ufmodhpfa_lod5_Var1"
for k, frac in enumerate((0.35, 0.42, 0.9)):
    p = start + d * (panels * PANEL * frac)
    mesh(cart, "Mine_Cart_%d" % k, p[0], p[1], track_yaw + 90.0 + (25.0 if k == 2 else 0.0), 1.6, 12.0)

# ---- fallen timber (natural cover) in and round the dead woods ---------------------------------------------------------
logs = [DEST + "NaturalCover/Logs/BP_Cover_DeadTreeLog_%02d" % i for i in range(1, 11)]
placed = []
tries = 0
while len(placed) < 38 and tries < 12000:
    tries += 1
    x = float(rng.uniform(-B["xmax"], B["xmax"]))
    y = float(rng.uniform(-B["ymax"], B["ymax"]))
    if not (mask_at("woods_thin", x, y) or mask_at("woods_dense", x, y)) or not free(x, y, 1400.0):
        continue
    if any(math.hypot(x - px, y - py) < 2200.0 for px, py in placed):
        continue
    placed.append((x, y))
    bp(logs[len(placed) % 10], "Log_%02d" % len(placed), x, y, float(rng.uniform(0, 360)))

# ---- instanced ground detail --------------------------------------------------------------------------------------
cliff = masks["cliff"].astype(np.float64)


def box_mean(a, r):
    pad = np.pad(a, r, mode="edge")
    c = np.cumsum(np.cumsum(pad, axis=0), axis=1)
    c = np.pad(c, ((1, 0), (1, 0)))
    k = 2 * r + 1
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


near_cliff = box_mean(cliff, 5)
low = H < box_mean(H, 5) - 60.0                    # lower than its surroundings: the foot of a face, a gully bed


def scree(x, y):
    ix, iy = cell(x, y)
    return 0.03 < near_cliff[iy, ix] < 0.5 and low[iy, ix] and not masks["cliff"][iy, ix] and not masks["ramps"][iy, ix] \
        and road_dist(x, y) > 700.0 and abs(x) < B["xmax"] + 1200 and abs(y) < B["ymax"] + 1200


def gully_bed(x, y):
    return mask_at("gully", x, y) and free(x, y, 700.0)


def heap(x, y):
    return mask_at("spoil_heap", x, y) and free(x, y, 700.0)


def toe_any(x, y):
    ix, iy = cell(x, y)
    return 1.0 if H[iy, ix] < box_H9[iy, ix] - 40.0 else 0.0


box_H9 = box_mean(H, 9)
STONES = ENV + "GroundStones/Meshes/SM_GS_"
for name, piece, spacing, keep, lo, hi in (("Scree_Chunk_A", "Chunk_A", 300.0, 0.5, 1.2, 2.8), ("Scree_Chunk_C", "Chunk_C", 340.0, 0.45, 1.2, 3.0),
                                         ("Scree_Field_B", "Field_B", 380.0, 0.5, 1.4, 2.6), ("Scree_Slab_B", "Slab_B", 420.0, 0.45, 1.5, 3.2),
                                         ("Scree_Cluster_A", "Cluster_A", 360.0, 0.5, 1.5, 2.6)):
    stone_scatter(name, STONES + piece, spacing * 0.6, scree, lo * 0.45, hi, clump_at=0.45, straggle=0.2, sink=0.25, grade=toe_any)
grid_scatter("Gully_Long_A", STONES + "Long_A", 330.0, 0.5, gully_bed, 1.0, 2.2)

# mid-map relief: stones on the wind-scoured crests and in the dry wash, dead scrub and tufts in the hollows
def crest(x, y):
    return mask_at("relief_crest", x, y) and free(x, y, 800.0)


def hollow(x, y):
    return mask_at("relief_hollow", x, y) and free(x, y, 800.0)


def wash_bed(x, y):
    return mask_at("wash", x, y) and free(x, y, 800.0)


# critic round 4: crest stones in clumps of 3-5 with gaps, not an even row; three times the cobbles in the wash bed;
# stones at the bank foot, on stone pavements and as scree on steep banks carry the edges paint cannot (1 m weights)
def clump(x, y, size=900.0, share=45):
    return ((int(math.floor(x / size)) * 73856093) ^ (int(math.floor(y / size)) * 19349663)) % 100 < share


def crest_clump(x, y):
    return crest(x, y) and clump(x, y)


def wash_foot(x, y):
    return mask_at("wash_foot", x, y) and free(x, y, 800.0)


def pavement(x, y):
    return mask_at("pavement", x, y) and free(x, y, 800.0)


def steep_bank(x, y):
    return mask_at("steep_bank", x, y) and road_dist(x, y) > 600.0


def bog_lip(x, y):
    return mask_at("bog_lip", x, y) and free(x, y, 800.0)


def bog_hag(x, y):
    return mask_at("bog_hag", x, y) and free(x, y, 800.0)


def wash_bar(x, y):
    return mask_at("wash_bar", x, y) and free(x, y, 800.0)


def bank_dry(x, y):
    return steep_bank(x, y) and not mask_at("bog_near", x, y)


def toe(x, y):
    ix, iy = cell(x, y)
    return 1.0 if low[iy, ix] else 0.0


def ditch_lip(x, y):
    return mask_at("ditch_lip", x, y)


def bog_belt(x, y):
    return mask_at("bog_belt", x, y) and free(x, y, 800.0)


# ledge stones along the crests in clumps; cobbles gathered on the gravel bars of the wash with bare sand between;
# pavements in half the patches; graded scree on the steep banks (blocks at the toe, small stuff at the top)
stone_scatter("Crest_Slab_A", STONES + "Slab_A", 190.0, crest, 0.8, 3.4, clump_at=0.6, straggle=0.03)
stone_scatter("Crest_Chunk_D", STONES + "Chunk_D", 170.0, crest, 0.5, 2.6, clump_at=0.6, straggle=0.04)
stone_scatter("Wash_Cluster_C", STONES + "Cluster_C", 150.0, wash_bar, 0.6, 2.0, clump_at=0.45, straggle=0.1)
stone_scatter("Wash_Long_B", STONES + "Long_B", 190.0, wash_bar, 0.6, 1.8, clump_at=0.45, straggle=0.1)
stone_scatter("Wash_Stray_C", STONES + "Chunk_C", 260.0, wash_bed, 0.4, 1.2, clump_at=0.72, straggle=0.03)
# realism round: stones only where something put them (faces, beds, crests, heaps, crater rims, fans, headwalls)
def m_(name):
    return lambda x, y: mask_at(name, x, y)


stone_scatter("Fan_Field_B", STONES + "Field_B", 140.0, m_("fan"), 0.3, 1.0, clump_at=0.5, straggle=0.1)
stone_scatter("RampFan_Field_B", STONES + "Field_B", 150.0, m_("ramp_fan"), 0.3, 1.1, clump_at=0.52, straggle=0.1)
stone_scatter("RampFan_Chunk_C", STONES + "Chunk_C", 190.0, m_("ramp_fan"), 0.3, 1.0, clump_at=0.55, straggle=0.08)
stone_scatter("Headwall_Chunk_A", STONES + "Chunk_A", 90.0, m_("headwall"), 0.6, 1.6, clump_at=0.3, straggle=0.5)
stone_scatter("CraterRim_Chunk_C", STONES + "Chunk_C", 110.0, m_("crater_rim"), 0.25, 1.1, clump_at=0.5, straggle=0.15)
stone_scatter("CraterRim_Chunk_A", STONES + "Chunk_A", 150.0, m_("crater_rim"), 0.25, 0.9, clump_at=0.55, straggle=0.1)
# salvage-felling belt round every wood: stumps and brash
grid_scatter("Ring_Stump", "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Meshes/SM_OreDead_Stump_Shattered", 330.0, 0.5,
             lambda x, y: mask_at("woods_ring", x, y) and free(x, y, 900.0), 0.8, 1.2)
grid_scatter("Ring_Brash", "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Meshes/SM_OreDead_BrashPile", 700.0, 0.5,
             lambda x, y: mask_at("woods_ring", x, y) and free(x, y, 900.0), 0.8, 1.3)
# Shell craters are heights + paint + rim stones only: the project's dirt crater decals (BP_RTS_RandomDirtCraterDecal_*)
# render brick red on this ground and were taken out again after the first in-game look.
stone_scatter("Bank_Chunk_A", STONES + "Chunk_A", 110.0, bank_dry, 0.4, 2.2, clump_at=0.42, straggle=0.25, grade=toe)
stone_scatter("Bank_Field_B", STONES + "Field_B", 140.0, bank_dry, 0.5, 1.8, clump_at=0.42, straggle=0.25, grade=toe)
stone_scatter("Bank_Chunk_C", STONES + "Chunk_C", 150.0, bank_dry, 0.4, 2.0, clump_at=0.42, straggle=0.25, grade=toe)
grid_scatter("DitchLip_Tuft_A", ENV + "GroundFoliage/Meshes/SM_GF_DryTuft_A", 110.0, 0.5, ditch_lip, 0.9, 1.7)
stone_scatter("DitchLip_Chunk_C", STONES + "Chunk_C", 150.0, ditch_lip, 0.3, 1.0, clump_at=0.5, straggle=0.1)
grid_scatter("Gully_Cluster_B", STONES + "Cluster_B", 380.0, 0.5, gully_bed, 1.2, 2.2)
stone_scatter("Heap_Chunk_A", STONES + "Chunk_A", 120.0, heap, 0.3, 1.2, clump_at=0.45, straggle=0.2)
stone_scatter("Heap_Chunk_C", STONES + "Chunk_C", 150.0, heap, 0.3, 1.1, clump_at=0.45, straggle=0.2)


def bog(x, y):
    return mask_at("peat_bog", x, y) and free(x, y, 800.0)


def scrub(x, y):
    return mask_at("scrub", x, y) and free(x, y, 800.0)


def wood_floor(x, y):
    return (mask_at("woods_thin", x, y) or mask_at("woods_dense", x, y)) and free(x, y, 900.0)


SHRUBS = [3, 5, 8, 10, 12]
for k, var in enumerate(SHRUBS):
    asset = FOLIAGE + "S_Dead_Shrubs_vdknafgha_Var%d_lod3" % var
    grid_scatter("Shrub_Bog_%d" % var, asset, 900.0, 0.5, bog, 0.9, 1.6)
    grid_scatter("Shrub_Scrub_%d" % var, asset, 700.0, 0.55, scrub, 0.9, 1.5)
    grid_scatter("Shrub_Woods_%d" % var, asset, 1900.0, 0.5, wood_floor, 0.9, 1.6)
GF = ENV + "GroundFoliage/Meshes/SM_GF_"
for var in (3, 8, 12):
    grid_scatter("Shrub_Hollow_%d" % var, FOLIAGE + "S_Dead_Shrubs_vdknafgha_Var%d_lod3" % var, 800.0, 0.5, hollow, 0.9, 1.6)
grid_scatter("Tuft_Hollow_A", GF + "DryTuft_A", 260.0, 0.4, hollow, 1.0, 1.8)
grid_scatter("Tuft_Hollow_B", GF + "DryTuft_B", 300.0, 0.4, hollow, 1.0, 1.8)
grid_scatter("Tuft_A", GF + "DryTuft_A", 190.0, 0.45, scrub, 1.0, 1.8)
grid_scatter("Tuft_B", GF + "DryTuft_B", 210.0, 0.45, scrub, 1.0, 1.8)
grid_scatter("Tuft_Bog_A", GF + "DryTuft_A", 330.0, 0.4, bog, 1.0, 1.8)
# a tussock belt half a metre to 3 m wide with gaps (not a one-plant necklace), and tussocks on every hag
grid_scatter("Tuft_BogLip_A", GF + "DryTuft_A", 90.0, 0.5, bog_belt, 1.0, 2.0)
grid_scatter("Tuft_BogLip_B", GF + "DryTuft_B", 100.0, 0.5, bog_belt, 1.0, 2.0)
grid_scatter("Tuft_BogHag_A", GF + "DryTuft_A", 70.0, 0.55, bog_hag, 1.0, 1.9)
grid_scatter("Tuft_BogHag_B", GF + "DryTuft_B", 90.0, 0.5, bog_hag, 1.0, 1.9)
grid_scatter("Litter_Woods", GF + "DryLitter", 240.0, 0.45, wood_floor, 1.0, 2.2)
# the dead-spruce kit's own clutter on the woods floor (OreDeadForest, instanced, no collision)
KITM = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Meshes/SM_OreDead_"
grid_scatter("Kit_Stump", KITM + "Stump_Shattered", 1500.0, 0.5, wood_floor, 0.9, 1.2)
grid_scatter("Kit_FallenTop", KITM + "FallenTop_5m", 1200.0, 0.5, wood_floor, 0.9, 1.15)
grid_scatter("Kit_Brash", KITM + "BrashPile", 650.0, 0.5, wood_floor, 0.8, 1.4)
grid_scatter("Kit_Bilberry", KITM + "BilberryMat", 760.0, 0.5, wood_floor, 0.9, 1.4)
grid_scatter("Kit_Fern", KITM + "DeadFern", 1000.0, 0.5, wood_floor, 0.9, 1.3)
grid_scatter("Kit_Tussock", KITM + "Tussock", 700.0, 0.5, wood_floor, 0.9, 1.4)
grid_scatter("Kit_Tussock_Scrub", KITM + "Tussock", 420.0, 0.5, scrub, 0.9, 1.5)
grid_scatter("Kit_Tussock_Hollow", KITM + "Tussock", 520.0, 0.45, hollow, 0.9, 1.5)

# ---- camera margin: the ground past the play edge is seen from the map edges, so the dead forest carries on there ----
def margin(x, y):
    out = max(abs(x) - B["xmax"], abs(y) - B["ymax"])
    ix, iy = cell(x, y)
    return 200.0 < out < 8500.0 and x > -B["xmax"] + 9000.0 and H[iy, ix] < 2600.0 and not masks["cliff"][iy, ix] \
        and road_dist(x, y) > 1100.0 and dist_poly(PATHS["ammo_route"], x, y) > 600.0 and not masks["road"][iy, ix]


for name, spacing in (("Skeleton_13m", 2300.0), ("Skeleton_11m", 2300.0), ("Skeleton_11m_B", 2600.0), ("Skeleton_9m", 2600.0),
                      ("Snapped_7m", 3200.0), ("Silver_10m", 3200.0), ("Windthrow_12m", 5200.0)):
    grid_scatter("Margin_Tree_" + name, KITM + name, spacing, 0.5, margin, 0.9, 1.15)
for var in SHRUBS[:3]:
    grid_scatter("Margin_Shrub_%d" % var, FOLIAGE + "S_Dead_Shrubs_vdknafgha_Var%d_lod3" % var, 1700.0, 0.5, margin, 0.9, 1.6)

json.dump({"props": props, "scatter": scatter}, open(os.path.join(OUT, "Ore_detail.json"), "w"))
print("props %d (fence panels %d)" % (len(props), fence_count))
for name, group in sorted(scatter.items()):
    print("  scatter %-18s %5d" % (name, len(group["items"])))
print("scatter total %d" % sum(len(g["items"]) for g in scatter.values()))
