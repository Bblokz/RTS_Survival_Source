"""Ger_Berlin detail pass, offline planner (2026-11-26, owner goal: "more detailed without much performance cost:
blend the landscape more, add decals, more furniture, the Reichstag area needs way more details").

Input: the live dump (detail_dump.py -> Saved/Berlin/detail/dump), the terrain masks of the generator
(Saved/Berlin/terrain/Ger_Berlin_masks.npz, same [Y][X] layout as the landscape raw) and assets.json (paths, mesh
boxes, patrol routes). Output: Saved/Berlin/detail/plan/placements.json + preview.png, applied by detail_apply.py.

Placement kinds:
  decal  DecalActor (material, half size, yaw)                     label DET_D_<group>_<n>
  bp     Blueprint actor (furniture, hedgehogs, sandbags, crates)  label DET_B_<group>_<n>
  ism    static-mesh instances, one HISM actor per mesh+collision  label DET_I_<mesh>
Everything is additive and carries the DET_ prefix; detail_apply.clear() removes only that prefix.

    python detail_gen.py [round_tag]
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

ROOT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/"
DUMP = ROOT + "detail/dump/"
OUT = ROOT + "detail/plan/"
os.makedirs(OUT, exist_ok=True)
TAG = sys.argv[1] if len(sys.argv) > 1 else "r0"
RNG = np.random.default_rng(20261126)

CORNER, Q, N = -59850.0, 100.0, 1198
PLAY_X, PLAY_Y = (-26000.0, 26000.0), (-50000.0, 50000.0)
EDGE_MARGIN = 1800.0                     # keep 18 m inside the play area (skirt, boundary blockers)

RT_C, RT_YAW = (8112.0, -15994.6), 90.0  # Reichstag ruin, portico faces -X (south)
GATE_C = (72.6, -8262.6)

DEC = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/MI_BerlinRubbleDecal_"
RUB = {k: DEC + k for k in ("00_BrickSpillFan", "01_BrickHeapSpread", "02_MasonryStrip", "03_CornerPile",
                            "04_PlasterDust", "05_ConcreteRebar", "06_RoofTiles", "07_SlateShards",
                            "08_CharredTimber", "09_AshSoot", "10_GlassShards", "11_SettsTornUp",
                            "12_PavingSlabs", "13_SteelScrap", "14_SandstoneBlocks", "15_GritSpill")}
CRATER = "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d"
TRACKS = "/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_Tracks_0%d"
CONC = "/Game/RTS_Survival/Materials/Decals/Concrete/MI_RTS_Decal_Concrete_%s"
FURN_DEBRIS = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Materials/MI_BerlinFurnitureDebris_Decal"
FURN_BP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Blueprints/BP_BerlinFurn_%s"
OLDCAR = "/Game/RTS_Survival/Blueprints/Environment/Props/OldCars/Cover/BP_Cover_OldCar_%s"

# body X x Y and top (cm) of the 32 crushable pieces (Docs/Environment/BerlinFurniture.md); top >= 150 = tall
FURN_BODY = {"01_Wardrobe": (61, 130, 188), "02_IronBed": (205, 100, 112), "03_KitchenTable": (70, 110, 71),
             "04_ChairFallen": (80, 102, 42), "05_Armchair": (93, 86, 92), "06_Sofa": (92, 190, 93),
             "07_Bathtub": (84, 179, 80), "08_TiledStove": (62, 92, 188), "09_Piano": (59, 150, 127),
             "10_ChestOfDrawers": (53, 102, 84), "11_BookshelfFallen": (180, 90, 30), "12_KitchenDresser": (57, 116, 191),
             "13_Desk": (72, 142, 76), "14_Washstand": (51, 84, 121), "15_ClockFallen": (222, 96, 35),
             "16_Radiator": (77, 99, 41), "17_Toilet": (57, 40, 86), "18_KitchenRange": (65, 94, 82),
             "19_Sideboard": (53, 182, 91), "20_RoundTableTipped": (115, 103, 102), "21_Pram": (100, 56, 94),
             "22_SewingTable": (44, 100, 96), "23_Gramophone": (54, 61, 164), "24_TrunkAndSuitcases": (70, 107, 59),
             "25_CoatStand": (36, 88, 208), "26_CellarShelf": (42, 105, 178), "27_WashTubMangle": (52, 137, 109),
             "28_Vitrine": (48, 98, 198), "29_LampAndRug": (159, 98, 58), "30_BarrelAndCrates": (51, 108, 76),
             "31_DoorLeafFallen": (203, 90, 24), "32_HandCart": (108, 71, 67)}

# ------------------------------------------------------------------------------------------------ inputs
A = json.load(open(DUMP + "actors.json"))
ACT, MBOX = A["actors"], A["mesh_box"]
BOX_BY_NAME = {k.split(".")[-1]: v for k, v in MBOX.items()}
ASSETS = json.load(open(ROOT + "detail/assets.json"))
SM = {k: v["path"] for k, v in ASSETS["sm"].items()}
SMBOX = {k: v["box"] for k, v in ASSETS["sm"].items()}
BP = ASSETS["bp"]
M = np.load(ROOT + "terrain/Ger_Berlin_masks.npz")
FT = json.load(open("D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/furniture_table.json"))
names = open(DUMP + "land_grid.txt").read().splitlines()[3].split()
W = {n: np.fromfile(DUMP + "land_%s.r8" % n, np.uint8).reshape(N, N) for n in names}


def ij(x, y):
    return int(round((x - CORNER) / Q)), int(round((y - CORNER) / Q))


def world(i, j):
    return CORNER + i * Q, CORNER + j * Q


def rect_cells(x, y, yaw, half_x, half_y, pad=0.0):
    """grid cells (j, i) covered by a yawed rectangle (cm)."""
    hx, hy = half_x + pad, half_y + pad
    r = math.hypot(hx, hy)
    i0, j0 = ij(x - r, y - r)
    i1, j1 = ij(x + r, y + r)
    i0, j0, i1, j1 = max(i0, 0), max(j0, 0), min(i1, N - 1), min(j1, N - 1)
    if i1 < i0 or j1 < j0:
        return np.zeros((0,), int), np.zeros((0,), int)
    jj, ii = np.mgrid[j0:j1 + 1, i0:i1 + 1]
    wx, wy = CORNER + ii * Q - x, CORNER + jj * Q - y
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    lx, ly = wx * c + wy * s, -wx * s + wy * c
    m = (np.abs(lx) <= hx) & (np.abs(ly) <= hy)
    return jj[m], ii[m]


def box_footprint(box, scale, x, y, yaw, pad=0.0):
    """mesh local box -> centre offset + half sizes, yawed into the grid."""
    cx, cy = (box[0] + box[3]) * 0.5 * scale, (box[1] + box[4]) * 0.5 * scale
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    wx, wy = x + cx * c - cy * s, y + cx * s + cy * c
    return rect_cells(wx, wy, yaw, (box[3] - box[0]) * 0.5 * scale, (box[4] - box[1]) * 0.5 * scale, pad)


# ------------------------------------------------------------------------------------------------ occupancy
occ = np.zeros((N, N), bool)          # anything solid standing there
dec_cov = np.zeros((N, N), np.float32)  # existing decal coverage
furn_pts, ruin_occ = [], np.zeros((N, N), bool)
SKIP_CLS = ("Landscape", "RecastNavMesh", "WorldSettings", "PostProcess", "SkyLight", "DirectionalLight",
            "NavMeshBounds", "Note", "TextRender", "BerlinDetectionZone", "EnemyWaveSpawner", "BerlinReichstagPatrol",
            "EnemyStrongpoint", "NiagaraActor", "BlockingVolume", "AnimatedTextPool", "FogOfWar")
for a in ACT:
    cls, lab = a["class"], a["label"]
    if any(s in cls for s in SKIP_CLS) or lab.startswith(("SKIRT", "DZ_", "RVT", "Boundary", "ATM_", "AMB_", "DET_")):
        continue
    if cls == "DecalActor":
        r = max(a["size"][1], a["size"][2]) * a.get("scale", 1.0)
        jj, ii = rect_cells(a["x"], a["y"], 0, r * 0.8, r * 0.8)
        dec_cov[jj, ii] += 1
        continue
    if lab.startswith("FURN"):
        furn_pts.append((a["x"], a["y"]))
    if a.get("box"):
        b = a["box"]
        if (b[3] - b[0]) < 30000 and (b[4] - b[1]) < 30000:
            jj, ii = box_footprint(b, a.get("scale", 1.0), a["x"], a["y"], a["yaw"], 60)
            occ[jj, ii] = True
            if lab.startswith(("RUIN", "POI_", "FOOT")):
                ruin_occ[jj, ii] = True
    for row in a.get("instances", []):
        b = BOX_BY_NAME.get(row[0])
        if not b or (b[3] - b[0]) > 30000:
            continue
        jj, ii = box_footprint(b, row[5], row[1], row[2], row[4], 40)
        occ[jj, ii] = True
        if lab.startswith(("RUIN", "POI_", "FOOT", "BER_Heap", "BER_Wall")):
            ruin_occ[jj, ii] = True
    if not a.get("box") and not a.get("instances"):
        jj, ii = rect_cells(a["x"], a["y"], 0, 150, 150)   # BP without a first mesh: assume a 3 m piece
        occ[jj, ii] = True

hard = (M["river"] | M["bridge"] | M["sinkhole"] | M["sink_gap"] | M["core_gap"] | M["rail"] | M["crater"])
hard = ndimage.binary_dilation(hard, iterations=3)
road = M["carriageway"]
jj, ii = np.mgrid[0:N, 0:N]
wxg, wyg = CORNER + ii * Q, CORNER + jj * Q
inside = ((wxg > PLAY_X[0] + EDGE_MARGIN) & (wxg < PLAY_X[1] - EDGE_MARGIN) &
          (wyg > PLAY_Y[0] + EDGE_MARGIN) & (wyg < PLAY_Y[1] - EDGE_MARGIN))
ruin_dist = ndimage.distance_transform_edt(~ruin_occ)          # metres to the nearest ruin / footprint
dec_density = ndimage.uniform_filter(dec_cov, 15)
dom = np.argmax(np.stack([W[n] for n in names]), 0)
SCRUB = names.index("BerlinScrub")

# routes: lanes (generator paths) and the Reichstag patrol rings keep a clear band for anything solid
L = json.load(open(ROOT + "terrain/Ger_Berlin_layout.json"))
route_mask = np.zeros((N, N), bool)


def stamp_polyline(pts, half_m, mask):
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        n = max(2, int(math.hypot(x1 - x0, y1 - y0) / 200))
        for k in range(n + 1):
            x, y = x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n
            jj_, ii_ = rect_cells(x, y, 0, half_m * 100, half_m * 100)
            mask[jj_, ii_] = True


for p in L["paths"]:
    if p["id"] in ("north_lane", "central_lane", "south_lane", "flank_lane", "road_east"):
        pts = [(q[0], q[1]) for q in p["points"]] if isinstance(p["points"][0], list) else []
        if pts:
            stamp_polyline(pts, 5.0, route_mask)
for k in ("RT_PatrolRoute_Ring", "RT_PatrolRoute_Gate"):
    pts = ASSETS["routes"][k]
    stamp_polyline(pts + ([pts[0]] if k.endswith("Ring") else []), 4.5, route_mask)

free_solid = inside & ~occ & ~hard & ~road & ~route_mask
free_decal = inside & ~hard & ~M["landmark_pad"]


def ground_ok(x, y, mask):
    i, j = ij(x, y)
    return 0 <= i < N and 0 <= j < N and bool(mask[j, i])


def claim(x, y, yaw, hx, hy, pad=50):
    jj_, ii_ = rect_cells(x, y, yaw, hx, hy, pad)
    if len(jj_) == 0 or not free_solid[jj_, ii_].all():
        return False
    free_solid[jj_, ii_] = False
    occ[jj_, ii_] = True
    return True


P = []          # placements


def add_decal(group, mat, x, y, half, rot=None, sort=3, color=None, half2=None, depth=160):
    P.append({"kind": "decal", "group": group, "mat": mat, "x": round(x, 1), "y": round(y, 1),
              "half": round(half, 1), "rot": round(RNG.uniform(0, 360) if rot is None else rot, 1), "sort": sort,
              **({"color": color} if color else {}), **({"half2": round(half2, 1)} if half2 else {}),
              **({"depth": depth} if depth != 160 else {})})
    jj_, ii_ = rect_cells(x, y, 0, half * 0.8, half * 0.8)
    dec_cov[jj_, ii_] += 1


def add_bp(group, path, x, y, yaw, hx, hy, extra=None, pad=50, solid=True):
    if solid and not claim(x, y, yaw, hx, hy, pad):
        return False
    P.append({"kind": "bp", "group": group, "path": path, "x": round(x, 1), "y": round(y, 1), "yaw": round(yaw, 1),
              **(extra or {})})
    return True


def add_ism(mesh, x, y, yaw, scale=1.0, collide=False, sink=0.0, check=True, pad=20, pitch=0.0, roll=0.0, cpd=None):
    b = SMBOX[mesh]
    if check:
        hx, hy = (b[3] - b[0]) * 0.5 * scale, (b[4] - b[1]) * 0.5 * scale
        if collide and not claim(x, y, yaw, hx, hy, pad):
            return False
        if not collide:
            jj_, ii_ = rect_cells(x, y, yaw, hx, hy)
            if len(jj_) == 0 or occ[jj_, ii_].any() or not inside[jj_, ii_].all() or hard[jj_, ii_].any():
                return False
    P.append({"kind": "ism", "mesh": mesh, "path": SM[mesh], "x": round(x, 1), "y": round(y, 1), "yaw": round(yaw, 1),
              "scale": round(scale, 3), "collide": collide, "sink": sink,
              **({"pitch": pitch} if pitch else {}), **({"roll": roll} if roll else {}), **({"cpd": cpd} if cpd else {})})
    return True


gy_r, gx_r = np.gradient(ndimage.gaussian_filter(ruin_dist, 1.5))


def wall_tangent(x, y):
    """angle (deg, world, from +X) along the nearest ruin wall."""
    i, j = ij(x, y)
    nx_, ny_ = gx_r[j, i], gy_r[j, i]
    return math.degrees(math.atan2(ny_, nx_)) + 90.0


def decal_roll(angle_deg):
    """roll that lays a decal's long (Y) axis along a world direction (pitch -90 decals: Y axis at 90 + roll;
    round 2 critic: 90 - a was right only on the axes and perpendicular at 45 degrees)."""
    return angle_deg - 90.0


def soot(group, x, y, half):
    add_decal(group, RUB["09_AshSoot"], x, y, half, rot=RNG.uniform(0, 360), half2=half * RNG.uniform(0.5, 0.65), depth=60)


OLDCAR_WRECK = "SM_OldCar_%s_Wreck"
BURNT_CPD = [8, 0.08, 9, 0.065, 10, 0.055, 11, 1.0, 12, 1.0]


def CAR_WRECK():
    """thirty winters: rust brown, never glossy black (round 3), fully worn."""
    base = [(0.16, 0.08, 0.04), (0.13, 0.09, 0.06), (0.18, 0.11, 0.06), (0.12, 0.1, 0.08)][RNG.integers(4)]
    return {"paint": [round(c * RNG.uniform(0.85, 1.15), 3) for c in base], "wear": 1.0}


def near_free(x, y, rmax, fn, step=250):
    """try fn(x', y') on a spiral round (x, y) until it returns True."""
    if fn(x, y):
        return True
    r = step
    while r <= rmax:
        n = max(6, int(2 * math.pi * r / step))
        off = RNG.uniform(0, 2 * math.pi)
        for k in range(n):
            a = off + 2 * math.pi * k / n
            if fn(x + r * math.cos(a), y + r * math.sin(a)):
                return True
        r += step
    return False


def rt_local(lx, ly):
    """Reichstag frame: +lx = north (away from the portico), +ly = east... rotated by the ruin yaw (90)."""
    c, s = math.cos(math.radians(RT_YAW - 90)), math.sin(math.radians(RT_YAW - 90))
    return RT_C[0] + lx * c - ly * s, RT_C[1] + lx * s + ly * c


FURN_COUNT = {}


def pick_furniture(pool=None):
    """least-used of a random sample, so no piece repeats much (round 2: 10 pianos, 9 toilets)."""
    pool = pool or list(FURN_BODY.keys())
    sample = [pool[RNG.integers(len(pool))] for _ in range(5)]
    key = min(sample, key=lambda k: FURN_COUNT.get(k, 0))
    FURN_COUNT[key] = FURN_COUNT.get(key, 0) + 1
    return key


# ------------------------------------------------------------------------------------------------ 1. Reichstag
def reichstag():
    n0 = len(P)
    # 1a sandstone rubble heaps hugging the outer walls (Reichstag kit's own heaps), outside the ruin
    ring = []
    for ang in np.linspace(0, 360, 144, endpoint=False):
        for r in np.arange(4600, 9800, 200):
            x, y = RT_C[0] + r * math.cos(math.radians(ang)), RT_C[1] + r * math.sin(math.radians(ang))
            i, j = ij(x, y)
            if ruin_dist[j, i] < 2.0:
                continue
            if 1.0 <= ruin_dist[j, i] <= 11.0 and free_solid[j, i]:
                ring.append((x, y, ang))
                break
    RNG.shuffle(ring)
    heaps = 0
    for x, y, ang in ring:
        if heaps >= 16:
            break
        mesh = "SM_RT_Rubble_S" if heaps % 4 else "SM_RT_Rubble_M"
        sc = RNG.uniform(0.5, 0.8) if mesh.endswith("S") else RNG.uniform(0.35, 0.5)
        if add_ism(mesh, x, y, RNG.uniform(0, 360), sc, collide=True, sink=25.0, pad=30):
            heaps += 1
            for _ in range(2):
                add_decal("rt_sandstone", RUB["14_SandstoneBlocks"], x + RNG.normal(0, 350), y + RNG.normal(0, 350),
                          RNG.uniform(190, 280))
    # 1b sandstone / plaster / grit spills hugging the wall bases, stretched 2:1 along the wall, overlapping
    k = 0
    for t in range(6000):
        if k >= 40:
            break
        ang = RNG.uniform(0, 2 * math.pi)
        r = RNG.uniform(4500, 8500)
        x, y = RT_C[0] + r * math.cos(ang), RT_C[1] + r * math.sin(ang)
        i, j = ij(x, y)
        if not (0.6 < ruin_dist[j, i] < 3.0) or not free_decal[j, i]:
            continue
        mat = RNG.choice(["02_MasonryStrip", "15_GritSpill", "08_CharredTimber", "14_SandstoneBlocks"])
        h = RNG.uniform(200, 280)
        add_decal("rt_apron", RUB[mat], x, y, h, rot=decal_roll(wall_tangent(x, y)) + RNG.normal(0, 6), half2=h * 0.5)
        k += 1
    # 1b2 inner courtyards: free regions inside the ruin rectangle -> 2-3 big spill decals each, heaps in corners
    i0, j0 = ij(RT_C[0] - 4700, RT_C[1] - 5900)
    i1, j1 = ij(RT_C[0] + 4700, RT_C[1] + 5900)
    sub = ~occ[j0:j1, i0:i1] & ~ruin_occ[j0:j1, i0:i1]
    lab, nreg = ndimage.label(ndimage.binary_opening(sub, iterations=2))
    courts = []
    for k in range(1, nreg + 1):
        jj_, ii_ = np.nonzero(lab == k)
        if len(jj_) < 150:
            continue
        courts.append((jj_ + j0, ii_ + i0))
        for c in range(3 if len(jj_) > 450 else 2):
            pick = RNG.integers(len(jj_))
            x, y = world(ii_[pick] + i0, jj_[pick] + j0)
            add_decal("rt_court", RUB[RNG.choice(["08_CharredTimber", "06_RoofTiles", "01_BrickHeapSpread", "08_CharredTimber"])],
                      x, y, RNG.uniform(600, 800), half2=RNG.uniform(450, 650), sort=2)
    k = 0
    for t in range(3000):
        if k >= 7:
            break
        x, y = RT_C[0] + RNG.uniform(-4200, 4200), RT_C[1] + RNG.uniform(-5400, 5400)
        i, j = ij(x, y)
        if 0.8 < ruin_dist[j, i] < 2.0 and add_ism(RNG.choice(["SM_RT_Rubble_S", "SM_RT_Rubble_M"]), x, y, RNG.uniform(0, 360),
                                                    RNG.uniform(0.25, 0.5), collide=True, sink=20.0, pad=10):
            k += 1
    # one pallet depot in the biggest courtyard: two rows of crate stacks on pallets
    if courts:
        jj_, ii_ = max(courts, key=lambda c: len(c[0]))
        cx_, cy_ = world(int(ii_.mean()), int(jj_.mean()))

        def depot(dx_, dy_):
            if not claim(dx_, dy_, 0, 650, 420, 30):
                return False
            for row in (-200, 200):
                for col in range(4):
                    x, y = dx_ - 450 + col * 300, dy_ + row
                    P.append({"kind": "ism", "mesh": "SM_Pallet", "path": SM["SM_Pallet"], "x": x, "y": y, "yaw": 0,
                              "scale": 1.5, "collide": False, "sink": 0.0})
                    P.append({"kind": "ism", "mesh": "SM_CrateCollection1", "path": SM["SM_CrateCollection1"], "x": x, "y": y,
                              "yaw": float(RNG.choice([0, 90, 180])), "scale": 0.9, "collide": True, "sink": -14.0})
            return True
        near_free(cx_, cy_, 2500, depot, 200)
    # scree at the foot of the big north-east sandstone heap (it had no blend at its foot)
    for (hx_, hy_) in ((11612, -13195), (10312, -13995), (10312, -12795), (10962, -15195)):
        for c in range(3):
            a = RNG.uniform(0, 2 * math.pi)
            x, y = hx_ + 750 * math.cos(a), hy_ + 750 * math.sin(a)
            add_decal("rt_heapfoot", RUB[RNG.choice(["15_GritSpill", "01_BrickHeapSpread", "08_CharredTimber"])], x, y,
                      RNG.uniform(320, 420), rot=decal_roll(math.degrees(a) + 90), half2=220)
            add_ism("SM_RT_Rubble_S", x + RNG.normal(0, 150), y + RNG.normal(0, 150), RNG.uniform(0, 360),
                    RNG.uniform(0.22, 0.35), collide=False, check=False, sink=12.0)
    # 1c Soviet camp clutter round the base buildings (crate stacks, drums, barrels, ammo boxes, pallets)
    camps = [(7209.4, -18594.8), (9009.4, -18594.8), (7112.0, -13319.6), (9217.0, -13338.9), (1912.7, -18395.7),
             (1914.3, -13599.0), (3112.0, -17194.6), (5200, -21800), (11400, -21500), (5000, -10200), (11600, -10400)]
    bp_items = [("BP_Airport_Dest_CrateStack01", 130, 130), ("BP_Airport_Dest_CrateStack02", 130, 130),
                ("BP_Prypyat_FuelDrums", 90, 70), ("BP_Prypyat_ServiceCrates", 110, 90)]
    ism_items = ["SM_BarrelCollection1", "SM_CrateCollection1", "SM_Ger_Ammobox", "SM_Ger_Ammobox_Long", "SM_Pallet",
                 "SM_Wooden_Pallette", "SM_Tire"]
    for cx, cy in camps:
        placed = 0
        for t in range(60):
            if placed >= 6:
                break
            ang, r = RNG.uniform(0, 2 * math.pi), RNG.uniform(900, 1900)
            x, y = cx + r * math.cos(ang), cy + r * math.sin(ang)
            yaw = RNG.choice([0, 90, 180, 270]) + RNG.normal(0, 8)
            if placed < 2:
                name, hx, hy = bp_items[RNG.integers(len(bp_items))]
                if name in BP and add_bp("rt_camp", BP[name], x, y, yaw, hx, hy, pad=80):
                    placed += 1
            else:
                mesh = ism_items[RNG.integers(len(ism_items))]
                if add_ism(mesh, x, y, yaw, 1.0, collide=mesh in ("SM_BarrelCollection1", "SM_CrateCollection1",
                                                                     "SM_Ger_Ammobox_Long"), pad=60):
                    placed += 1
    # 1d tents of the Shield camp: two big + two small, east and west of the barracks
    def tent_row(x0, y0):
        if not claim(x0, y0, 0, 900, 2700, 60):
            return False
        for k in range(3):
            P.append({"kind": "ism", "mesh": "SM_Tente_1", "path": SM["SM_Tente_1"], "x": x0, "y": y0 - 1800 + k * 1800,
                      "yaw": 0.0, "scale": 1.0, "collide": True, "sink": 10.0})
        for (dy_, item) in ((-2700, "SM_RusTentPodest_Rack"), (2700, "SM_RusTentPodest_Table"), (2500, "SM_RusTentPodest_Board")):
            P.append({"kind": "ism", "mesh": item, "path": SM[item], "x": x0 - 650, "y": y0 + dy_, "yaw": 0.0, "scale": 1.0,
                      "collide": True, "sink": 0.0})
        return True
    if not near_free(12600, -24500, 9000, tent_row, 400):
        for (tx_, ty_) in ((4300, -22600), (12200, -22300)):
            near_free(tx_, ty_, 5000, lambda a, b: add_ism("SM_Tente_1", a, b, 0.0, 1.0, collide=True, sink=10.0, pad=120), 400)
    for (x, y, yaw, mesh) in []:
        got = []
        near_free(x, y, 4000, lambda a, b: add_ism(mesh, a, b, yaw, 1.0, collide=True, sink=10.0, pad=150) and got.append((a, b)) is None, 400)
        if got:
            tx, ty = got[0]
            c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            for (dx, item) in ((900, "SM_RusTentPodest_Rack"), (-900, "SM_RusTentPodest_Table"), (1150, "SM_RusTentPodest_Board")):
                near_free(tx + dx * c, ty + dx * sn, 900, lambda a, b: add_ism(item, a, b, yaw, 1.0, collide=True, pad=40), 150)
    # 1e Soviet flags at the portico, factory and barracks
    for (x, y) in [(3500, -14300), (3500, -17700), (6300, -20200), (10000, -20200), (12900, -16100)]:
        near_free(x, y, 2500, lambda a, b: add_bp("rt_flag", BP["Dest_SovietFlag"], a, b, 90, 60, 60, pad=60))
    # 1f hedgehog lines: parallel to each ring leg, 16 m outside it, flanking the guard post gap
    ring_ = ASSETS["routes"]["RT_PatrolRoute_Ring"]
    for (x0, y0), (x1, y1) in zip(ring_, ring_[1:] + ring_[:1]):
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        ox, oy = mx - RT_C[0], my - RT_C[1]
        d = math.hypot(ox, oy)
        ux, uy = ox / d, oy / d
        lx_, ly_ = (x1 - x0), (y1 - y0)
        L_ = math.hypot(lx_, ly_)
        tx, ty = lx_ / L_, ly_ / L_
        for side in (1, -1):
            for k in range(6):
                t = side * (520 + k * 360)
                name = "BP_Military_Dest_Hedgehog" if k % 3 else "BP_Military_Dest_LargeHedgehog"
                for off in (1700, 1450, 2000, 2300):
                    x, y = mx + ux * off + tx * t + RNG.normal(0, 30), my + uy * off + ty * t + RNG.normal(0, 30)
                    if add_bp("rt_hedgehog", BP[name], x, y, RNG.uniform(0, 360), 110, 110, pad=20):
                        break
    # 1g guard posts: horseshoe sandbag pit facing out + warning signs + lamp pole + wire chicane, outside the ring
    ring = ASSETS["routes"]["RT_PatrolRoute_Ring"]
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        ox, oy = mx - RT_C[0], my - RT_C[1]
        d = math.hypot(ox, oy)
        ux, uy = ox / d, oy / d
        out_yaw = math.degrees(math.atan2(uy, ux))
        px_, py_ = mx + ux * 1100, my + uy * 1100
        if not near_free(px_, py_, 1500, lambda a, b: add_bp("rt_post", BP["BP_Cover_RusSandbags_Cornered"], a, b, out_yaw + 45, 200, 200, pad=40)):
            continue
        tx, ty = -uy, ux
        add_ism("SM_WarningSignA", px_ + tx * 650 + ux * 200, py_ + ty * 650 + uy * 200, out_yaw + 90, 1.3, check=False)
        add_ism("SM_Quarantine_Sign", px_ - tx * 650 + ux * 200, py_ - ty * 650 + uy * 200, out_yaw + 90, 1.3, check=False)
        near_free(px_ + tx * 450 - ux * 300, py_ + ty * 450 - uy * 300, 700,
                  lambda a, b: add_bp("rt_post", BP["BP_Civilian_Dest_CrushablePoleLight"], a, b, 0, 40, 40, pad=40))
        for side in (1, -1):
            near_free(px_ + ux * 600 + tx * side * 650, py_ + uy * 600 + ty * side * 650, 700,
                      lambda a, b: add_bp("rt_wire", BP["BP_Military_Dest_BarbedWire"], a, b, out_yaw + 90 + side * 20, 200, 60, pad=30))

    def fuel_dump(cx, cy):
        if not claim(cx, cy, 0, 650, 650, 40):
            return False
        for (dx, dy) in ((-250, -250), (250, -250), (-250, 250), (250, 250), (0, 0)):
            P.append({"kind": "ism", "mesh": "SM_Pallet", "path": SM["SM_Pallet"], "x": cx + dx, "y": cy + dy, "yaw": 0,
                      "scale": 1.6, "collide": False, "sink": 0.0})
            P.append({"kind": "ism", "mesh": "SM_BarrelCollection1", "path": SM["SM_BarrelCollection1"], "x": cx + dx,
                      "y": cy + dy, "yaw": float(RNG.uniform(0, 360)), "scale": 1.0, "collide": True, "sink": -14.0})
            add_decal("rt_stain", "/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_04", cx + dx, cy + dy, 200, sort=2)
        for (dx, dy, yw) in ((0, -650, 0), (0, 650, 0), (-650, 0, 90), (650, 0, 90)):
            P.append({"kind": "bp", "group": "rt_fuel_wire", "path": BP["BP_Military_Dest_BarbedWireLong"], "x": cx + dx,
                      "y": cy + dy, "yaw": yw})
        return True
    near_free(12600, -19000, 9000, fuel_dump, 350)

    def park(cx, cy):
        if not claim(cx, cy, 0, 700, 1000, 40):
            return False
        for k, mesh in enumerate(("SM_Hull_Zil", "SM_OldTruck_1", "SM_Hull_Zil")):
            yaw = 90 + (35 if k % 2 else -35)                        # herringbone
            P.append({"kind": "ism", "mesh": mesh, "path": SM[mesh], "x": cx - 600 + k * 600, "y": cy + (k % 2) * 250,
                      "yaw": yaw + float(RNG.normal(0, 4)), "scale": 1.0, "collide": True, "sink": 0.0})
            add_decal("rt_mud", "/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_04", cx - 600 + k * 600,
                      cy + 700, RNG.uniform(380, 480), sort=0)
        for k in range(4):
            add_decal("rt_mud", "/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_04", cx + RNG.normal(0, 200),
                      cy + 1300 + k * 700, RNG.uniform(380, 480), sort=0)
        return True
    near_free(9000, -8600, 9000, park, 350)
    # fallen portico: fans of small sandstone heaps against the portico front
    for fan in range(4):
        fx_, fy_ = RT_C[0] - 4900, RT_C[1] + RNG.uniform(-2600, 2600)
        for k in range(RNG.integers(2, 4)):
            add_ism(RNG.choice(["SM_RT_Rubble_S", "SM_RT_Rubble_M"]), fx_ - k * RNG.uniform(200, 380) + RNG.normal(0, 120),
                    fy_ + RNG.normal(0, 320), RNG.uniform(0, 360), RNG.uniform(0.22, 0.45), collide=True, sink=20.0, pad=10)
    # 1h shell craters and tracks on the open ground round the ruin (decals), puddles in some
    k = 0
    while k < 34:
        x, y = RT_C[0] + RNG.uniform(-10500, 8500), RT_C[1] + RNG.uniform(-11500, 11500)
        i, j = ij(x, y)
        if not free_decal[j, i] or ruin_dist[j, i] < 4 or occ[j, i] or dec_density[j, i] > 0.5:
            continue
        water = RNG.random() < 0.3
        add_decal("rt_crater", (CRATER % RNG.integers(1, 7)) + ("_Water" if water else ""), x, y,
                  RNG.uniform(220, 600), sort=1)
        k += 1
    pits = [(1912.7, -18395.7), (1914.3, -13599.0), (7112.0, -13319.6), (9217.0, -13338.9)]
    for route in ("RT_PatrolRoute_Ring", "RT_PatrolRoute_Gate"):
        pts = ASSETS["routes"][route]
        pts = pts + ([pts[0], pts[1]] if route.endswith("Ring") else [])
        tex = TRACKS % RNG.integers(1, 4)
        legs = list(zip(pts[:-1], pts[1:]))
        for li, ((x0, y0), (x1, y1)) in enumerate(legs):
            if route.endswith("Ring") and li == len(legs) - 1:
                break
            heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
            L_ = math.hypot(x1 - x0, y1 - y0)
            n = int(L_ / 350)
            for k in range(1, n):
                t = k / n
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                i, j = ij(x, y)
                if W["BerlinGlass"][j, i] > 64 or min(math.hypot(x - px_, y - py_) for px_, py_ in pits) < 900:
                    continue
                add_decal("rt_tracks", tex, x, y, 400, rot=decal_roll(heading), sort=0, half2=300)
            if li + 1 < len(legs):                                   # fillet into the next leg
                (nx0, ny0), (nx1, ny1) = legs[li + 1]
                h2 = math.degrees(math.atan2(ny1 - ny0, nx1 - nx0))
                dh = (h2 - heading + 180) % 360 - 180
                for f in (0.25, 0.5, 0.75):
                    add_decal("rt_tracks", tex, x1 + RNG.normal(0, 40), y1 + RNG.normal(0, 40), 400,
                              rot=decal_roll(heading + dh * f), sort=0, half2=300)
    # 1i dead trees and burnt cars on the Koenigsplatz / Tiergarten edge west and south of the ruin
    k = 0
    for t in range(400):
        if k >= 12:
            break
        x, y = RNG.uniform(-3000, 3500), RNG.uniform(-25000, -18500)
        if ground_ok(x, y, free_solid) and add_bp("rt_tree", BP["BP_Scorch_Dest_DeadTree"], x, y, RNG.uniform(0, 360),
                                                  150, 150, pad=200):
            k += 1
    cars = ["Sedan", "Limousine", "Coupe", "Compact", "PanelVan", "Pickup", "Fastback", "Cabriolet"]
    k = 0
    for t in range(600):
        if k >= 6:
            break
        x, y = RT_C[0] + RNG.uniform(-9000, 9000), RT_C[1] + RNG.uniform(-12000, 12000)
        i, j = ij(x, y)
        if not (M["street_berm"][j, i] or M["side_street"][j, i]) or ruin_dist[j, i] < 3:
            continue
        if add_bp("rt_car", OLDCAR % cars[k % len(cars)], x, y, RNG.uniform(0, 360), 240, 110,
                  extra=CAR_WRECK(), pad=120):
            soot("car_soot", x, y, RNG.uniform(300, 380))
            k += 1
    print("reichstag", len(P) - n0)


# ------------------------------------------------------------------------------------------------ 2. decals
def map_decals(clusters=90):
    """clusters of 3-6 overlapping decals along wall bases and street edges; voids in between (round 2: no sprinkle)."""
    n0 = len(P)
    cand = np.argwhere(free_decal & (dec_density < 0.3) & (ruin_dist > 0.5) & (ruin_dist < 5) |
                       free_decal & (dec_density < 0.3) & M["street_berm"])
    RNG.shuffle(cand)
    taken = np.zeros((N, N), bool)
    made = 0
    for j, i in cand:
        if made >= clusters:
            break
        if taken[j, i]:
            continue
        x, y = world(i, j)
        on_street = bool(M["street_berm"][j, i] or road[j, i])
        ang = wall_tangent(x, y) if not on_street else RNG.uniform(0, 180)
        mats = ([RUB["11_SettsTornUp"], RUB["13_SteelScrap"], CONC % "Debris_02", RUB["10_GlassShards"]] if on_street else
                [RUB["01_BrickHeapSpread"], RUB["02_MasonryStrip"], RUB["06_RoofTiles"], RUB["07_SlateShards"],
                 RUB["08_CharredTimber"], RUB["08_CharredTimber"], RUB["13_SteelScrap"]])
        for c in range(RNG.integers(2, 4)):
            d = RNG.uniform(0, 700)
            t = RNG.uniform(-1, 1)
            cx_ = x + d * t * math.cos(math.radians(ang)) + RNG.normal(0, 90)
            cy_ = y + d * t * math.sin(math.radians(ang)) + RNG.normal(0, 90)
            h = RNG.uniform(190, 290)
            add_decal("map", mats[RNG.integers(len(mats))], cx_, cy_, h, rot=decal_roll(ang) + RNG.normal(0, 15),
                      half2=h * RNG.uniform(0.55, 0.9))
        taken[max(0, j - 28):j + 28, max(0, i - 28):i + 28] = True
        made += 1
    # a few lone craters in the open (dry mostly)
    cand = np.argwhere(free_decal & (dec_density < 0.2) & ~road & (ruin_dist > 10))
    RNG.shuffle(cand)
    for j, i in cand[:60]:
        x, y = world(i, j)
        water = RNG.random() < 0.12
        add_decal("map_crater", (CRATER % RNG.integers(1, 7)) + ("_Water" if water else ""), x, y, RNG.uniform(220, 480), sort=1)
    print("map decals", len(P) - n0)


def pad_edges(limit=150, piece_limit=520):
    """dissolve the pad outlines: big dark decals half on / half off each footprint edge, rubble pieces along the rim."""
    n0 = len(P)
    pads = [a for a in ACT if a["label"].startswith("FOOT_") and a.get("box")]
    RNG.shuffle(pads)
    chunks = ["DBK_Concrete_Rubble_Small_A", "DBK_Concrete_Rubble_Small_B", "DBK_Concrete_Rubble_Small_C",
              "DBK_Brick_Rubble_C", "DBK_Brick_Rubble_D"]
    nd = npc = 0
    for a in pads:
        b, sc = a["box"], a.get("scale", 1.0)
        yaw = math.radians(a["yaw"])
        edges = (((b[3]) * sc, (b[1] + b[4]) * 0.5 * sc, 0, (b[4] - b[1]) * sc), ((b[0]) * sc, (b[1] + b[4]) * 0.5 * sc, 180, (b[4] - b[1]) * sc),
                 ((b[0] + b[3]) * 0.5 * sc, b[4] * sc, 90, (b[3] - b[0]) * sc), ((b[0] + b[3]) * 0.5 * sc, b[1] * sc, -90, (b[3] - b[0]) * sc))
        for (lx, ly, out, span) in edges:
            if RNG.random() < 0.45:
                continue
            x = a["x"] + lx * math.cos(yaw) - ly * math.sin(yaw)
            y = a["y"] + lx * math.sin(yaw) + ly * math.cos(yaw)
            if not ground_ok(x, y, free_decal):
                continue
            along = math.degrees(yaw) + out + 90
            if nd < limit:
                h = RNG.uniform(500, 650)
                add_decal("pad_edge", RNG.choice([RUB["08_CharredTimber"], RUB["01_BrickHeapSpread"], CONC % "Scatter_03"]),
                          x, y, h, rot=decal_roll(along), half2=h * 0.45)
                nd += 1
            for c in range(RNG.integers(2, 5)):
                if npc >= piece_limit:
                    break
                t = RNG.uniform(-0.45, 0.45) * span
                px_ = x + t * math.cos(math.radians(along)) + RNG.normal(0, 60)
                py_ = y + t * math.sin(math.radians(along)) + RNG.normal(0, 60)
                if add_ism(chunks[RNG.integers(len(chunks))], px_, py_, RNG.uniform(0, 360), RNG.uniform(0.45, 0.75),
                           collide=False, check=False, sink=float(RNG.uniform(3, 9))):
                    npc += 1
    print("pad edges", nd, "decals", npc, "pieces", len(P) - n0)


def ghost_streets(fields=16):
    """big empty plains (not the Tiergarten): ghost foundations as low rubble lines in house-sized rectangles."""
    n0 = len(P)
    empty = free_decal & (dec_density < 0.15) & (ruin_dist > 14) & ~road & (dom != SCRUB) & free_solid
    cand = np.argwhere(empty)
    RNG.shuffle(cand)
    taken = np.zeros((N, N), bool)
    pieces = ["DBK_Brick_Rubble_C", "DBK_Brick_Rubble_D", "DBK_Concrete_Rubble_Small_C", "DBK_Concrete_Rubble_Small_A"]
    made = 0
    for j, i in cand:
        if made >= fields:
            break
        if taken[j, i]:
            continue
        x, y = world(i, j)
        yaw = RNG.choice([0, 90]) + RNG.normal(0, 4) + 17
        L1, L2 = RNG.uniform(900, 1400), RNG.uniform(700, 1100)
        c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        for (ox, oy, ax_along) in ((L1 / 2, 0, 1), (-L1 / 2, 0, 1), (0, L2 / 2, 0), (0, -L2 / 2, 0)):
            span = L2 if ax_along else L1
            for k in range(int(span / 230) + 1):
                if RNG.random() < 0.2:
                    continue
                t = (k / max(1, int(span / 230)) - 0.5) * span
                lx_, ly_ = (ox, oy + t) if ax_along else (ox + t, oy)
                px_, py_ = x + lx_ * c - ly_ * sn, y + lx_ * sn + ly_ * c
                if ground_ok(px_, py_, free_decal):
                    add_ism(pieces[RNG.integers(len(pieces))], px_, py_, yaw + RNG.choice([0, 90]) + RNG.normal(0, 10),
                            RNG.uniform(0.5, 0.75), collide=False, check=False, sink=float(RNG.uniform(2, 6)))
        add_decal("ghost", RUB["08_CharredTimber"], x, y, RNG.uniform(380, 480), half2=300, sort=2)
        taken[max(0, j - 30):j + 30, max(0, i - 30):i + 30] = True
        made += 1
    print("ghost foundations", made, len(P) - n0)


# ------------------------------------------------------------------------------------------------ 3. furniture# ------------------------------------------------------------------------------------------------ 3. furniture
def furniture(target=160):
    n0 = len(P)
    pieces = [(k, {"body_size_cm": [v[0], v[1]], "tall": v[2] >= 150}) for k, v in FURN_BODY.items()]
    fp = np.array(furn_pts) if furn_pts else np.zeros((0, 2))
    cand = np.argwhere(free_solid & (ruin_dist > 1.5) & (ruin_dist < 10) & (M["ruin_field"] | M["pads"] | M["side_street"]))
    RNG.shuffle(cand)
    placed = 0
    for j, i in cand:
        if placed >= target:
            break
        x, y = world(i, j)
        if len(fp) and np.min(np.hypot(fp[:, 0] - x, fp[:, 1] - y)) < 1400:
            continue
        group = []
        for g in range(RNG.integers(1, 4)):
            key = pick_furniture()
            info = dict(pieces)[key]
            if info.get("tall") and not M["ruin_field"][j, i]:
                continue
            body = info.get("body_size_cm") or info.get("size_cm") or [100, 100, 100]
            gx, gy = x + RNG.normal(0, 220), y + RNG.normal(0, 220)
            yaw = RNG.uniform(0, 360)
            if add_bp("furn", FURN_BP % key, gx, gy, yaw, body[0] * 0.5, body[1] * 0.5, pad=60):
                group.append((gx, gy, key))
        for gx, gy, key in group:
            num = int(key.split("_")[0])
            if RNG.random() < 0.7:
                var = (num - 1) % 16
                add_decal("furn_debris", FURN_DEBRIS, gx + RNG.normal(0, 140), gy + RNG.normal(0, 140),
                          RNG.uniform(140, 200), color=[(var + 0.5) / 16.0, 1.0, 1.0, 1.0])
        if group:
            fp = np.vstack([fp, [[x, y]]])
            placed += len(group)
    print("furniture", len(P) - n0)


# ------------------------------------------------------------------------------------------------ 4. clutter
def clutter():
    """small non-colliding instances: charred plank fans at ruin wall gaps, collapsed 1945 sandbag rings, debris."""
    n0 = len(P)
    planks = ["DBK_WoodFrame_Plank_%s" % c for c in "ABCDEFG"]
    cand = np.argwhere(inside & ~hard & ~occ & ~road & (ruin_dist > 0.4) & (ruin_dist < 3.5))
    RNG.shuffle(cand)
    fans = 0
    for j, i in cand:
        if fans >= 55:
            break
        x, y = world(i, j)
        out = wall_tangent(x, y) - 90.0
        ok = 0
        for c in range(RNG.integers(3, 6)):
            d = RNG.uniform(50, 260)
            a = math.radians(out + RNG.normal(0, 35))
            if add_ism(planks[RNG.integers(len(planks))], x + d * math.cos(a), y + d * math.sin(a), RNG.uniform(0, 360),
                       RNG.uniform(0.6, 0.85), pitch=90.0, roll=float(RNG.normal(0, 8))):
                ok += 1
        if ok:
            fans += 1
    craters = [a for a in ACT if a["class"] == "DecalActor" and a.get("mat") and "Crater" in a["mat"]
               and 200 < a["size"][1] < 600]
    RNG.shuffle(craters)
    rings = 0
    for a in craters:
        if rings >= 16:
            break
        x, y, r = a["x"], a["y"], a["size"][1] * 0.95
        i, j = ij(x, y)
        if not (0 <= i < N and 0 <= j < N) or road[j, i] or occ[j, i] or not inside[j, i]:
            continue
        a0 = RNG.uniform(0, 360)
        n = 0
        for da in np.arange(0, RNG.uniform(90, 150), 18):
            ang = math.radians(a0 + da)
            bx, by = x + (r + RNG.normal(0, 15)) * math.cos(ang), y + (r + RNG.normal(0, 15)) * math.sin(ang)
            if add_ism("SM_Ger_LooseSandBag_%d" % RNG.integers(1, 6), bx, by, a0 + da + 90 + float(RNG.normal(0, 10)), 1.0):
                n += 1
        if n >= 3:
            rings += 1
    cand = np.argwhere(inside & ~hard & ~occ & ~road & (ruin_dist < 6))
    RNG.shuffle(cand)
    for j, i in cand[:250]:
        x, y = world(i, j)
        add_ism(RNG.choice(["SM_BerlinFurn_Debris_0", "SM_BerlinFurn_Debris_3"]), x, y, RNG.uniform(0, 360), RNG.uniform(0.8, 1.2))
    print("clutter", len(P) - n0, "fans", fans, "rings", rings)


# ------------------------------------------------------------------------------------------------ 5. street props
def street_props():
    n0 = len(P)
    cars = ["Sedan", "Limousine", "Coupe", "Compact", "PanelVan", "Pickup", "Fastback", "Cabriolet"]
    berm = np.argwhere(inside & (M["street_berm"] | (M["street"] & ~road)) & ~hard & ~route_mask)
    RNG.shuffle(berm)
    k = poles = 0
    for j, i in berm:
        x, y = world(i, j)
        if k < 40 and RNG.random() < 0.5:
            if add_bp("car", OLDCAR % cars[k % len(cars)], x, y, RNG.uniform(0, 360), 240, 110,
                      extra=CAR_WRECK(), pad=300):
                soot("car_soot", x, y, RNG.uniform(300, 380))
                k += 1
        elif poles < 45:
            name = RNG.choice(["BP_Civilian_Dest_CrushablePoleLight", "BP_Civilian_Dest_CrushablePole_Electric1"])
            if add_bp("pole", BP[name], x, y, RNG.uniform(0, 360), 40, 40, pad=900):
                poles += 1
        if k >= 40 and poles >= 45:
            break
    print("street props", len(P) - n0)


def facade_scenes(target=32):
    """house-spill scenes in front of broken fronts: one big furniture-debris decal, 3-5 pieces, a plank fan."""
    n0 = len(P)
    fronts = [a for a in ACT if a["label"].startswith("RUIN_") and any(k in a["class"] for k in
              ("B1_Facade", "B2_Facade", "B3_Facade", "E7_HalfHouse", "E1_", "E2_", "E3_"))]
    RNG.shuffle(fronts)
    keys = list(FURN_BODY.keys())
    planks = ["DBK_WoodFrame_Plank_%s" % c for c in "ABCDEFG"]
    made = 0
    for a in fronts:
        if made >= target:
            break
        yaw = math.radians(a["yaw"])
        fx, fy = math.cos(yaw), math.sin(yaw)
        side = 1 if RNG.random() < 0.5 else -1
        b = a.get("box") or [-500, -800, 0, 500, 800, 0]
        half_front = (b[3] - b[0]) * 0.5 * a.get("scale", 1.0)
        cx = a["x"] + fx * (half_front + RNG.uniform(250, 450)) - fy * side * RNG.uniform(0, 400)
        cy = a["y"] + fy * (half_front + RNG.uniform(250, 450)) + fx * side * RNG.uniform(0, 400)
        i, j = ij(cx, cy)
        if not (0 <= i < N and 0 <= j < N) or road[j, i] or not free_solid[j, i]:
            continue
        pieces = 0
        big = ["02_IronBed", "07_Bathtub", "01_Wardrobe", "06_Sofa", "12_KitchenDresser", "19_Sideboard", "28_Vitrine"]
        hero = made < 12
        for g in range(RNG.integers(3, 6)):
            key = pick_furniture(big if (hero and g == 0) else None)
            body = FURN_BODY[key]
            gx, gy = cx + RNG.normal(0, 160), cy + RNG.normal(0, 160)
            if add_bp("furn_scene", FURN_BP % key, gx, gy, RNG.uniform(0, 360), body[0] * 0.5, body[1] * 0.5, pad=30):
                pieces += 1
        if pieces < 2:
            continue
        var = int(RNG.integers(16))
        if hero:
            add_decal("furn_scene", RUB["08_CharredTimber"], cx, cy, RNG.uniform(320, 400), half2=RNG.uniform(250, 320), sort=2)
        add_decal("furn_scene", FURN_DEBRIS, cx, cy, RNG.uniform(420, 500) if hero else RNG.uniform(320, 420),
                  color=[(var + 0.5) / 16.0, 1.0, 1.0, 1.0])
        add_decal("furn_scene", RUB["00_BrickSpillFan"], cx - fx * 200, cy - fy * 200, 260,
                  rot=decal_roll(math.degrees(yaw) + 90))
        for c in range(3):
            add_ism(planks[RNG.integers(len(planks))], cx + RNG.normal(0, 200), cy + RNG.normal(0, 200), RNG.uniform(0, 360),
                    0.7, pitch=90.0)
        made += 1
    print("facade scenes", made, len(P) - n0)


def gate_rim():
    """blast-crater rim and the Gate: soot ring, outward glass shards, half-sunk concrete chunks, burnt hulks, drums."""
    n0 = len(P)
    cx, cy, R = -2167.0, -3000.0, 3850.0
    for k in range(26):
        a = RNG.uniform(0, 2 * math.pi)
        r = R + RNG.uniform(-200, 1100)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if ground_ok(x, y, free_decal):
            soot("rim_soot", x, y, RNG.uniform(340, 480))
            soot("rim_soot", x + RNG.normal(0, 250), y + RNG.normal(0, 250), RNG.uniform(260, 380))
    for k in range(45):
        a = RNG.uniform(0, 2 * math.pi)
        r = R + RNG.uniform(300, 1800)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if ground_ok(x, y, free_decal):
            add_decal("rim_glass", RUB["10_GlassShards"], x, y, RNG.uniform(240, 300), rot=decal_roll(math.degrees(a)),
                      half2=130)
    for k in range(26):                                   # long soot streaks pointing away from ground zero
        a = 2 * math.pi * k / 26 + RNG.normal(0, 0.08)
        r = RNG.uniform(2600, 6800)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if ground_ok(x, y, free_decal):
            add_decal("rim_streak", RUB["09_AshSoot"], x, y, RNG.uniform(650, 800), rot=decal_roll(math.degrees(a)),
                      half2=RNG.uniform(160, 210), depth=60)
    cars_ = ["Sedan", "Limousine", "Coupe", "Compact", "PanelVan", "Pickup", "Fastback", "Cabriolet"]
    k = 0
    for t in range(500):                                  # burnt wrecks blown outward, nose away from the centre
        if k >= 12:
            break
        a = RNG.uniform(0, 2 * math.pi)
        r = R + RNG.uniform(1200, 4200)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if add_ism(OLDCAR_WRECK % cars_[k % 8], x, y, math.degrees(a) + RNG.normal(0, 30), 1.0, collide=True, sink=6.0,
                   pad=100, cpd=BURNT_CPD):
            soot("rim_soot", x - 250 * math.cos(a), y - 250 * math.sin(a), 360)
            k += 1
    chunks = ["DBK_Concrete_Rubble_Small_%s" % c for c in "ABCD"] + ["DBK_Concrete_Rubble_Mid_B", "DBK_Concrete_Rubble_Mid_C"]
    k = 0
    for t in range(500):
        if k >= 28:
            break
        a = RNG.uniform(0, 2 * math.pi)
        r = R + RNG.uniform(400, 1600)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if add_ism(chunks[RNG.integers(len(chunks))], x, y, RNG.uniform(0, 360), RNG.uniform(0.7, 1.0), collide=True,
                   sink=float(RNG.uniform(6, 30)), pad=60):
            k += 1
    k = 0
    for t in range(400):
        if k >= 5:
            break
        a = RNG.uniform(0, 2 * math.pi)
        r = R + RNG.uniform(900, 2400)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        mesh = RNG.choice(["SM_Destroyed_Zil", "SM_Destroyed_GAZMM72K"])
        if add_ism(mesh, x, y, math.degrees(a) + 90 + RNG.normal(0, 25), 1.0, collide=True, sink=12.0, pad=120):
            soot("rim_soot", x, y, 420)
            k += 1
    k = 0
    for t in range(600):
        if k >= 8:
            break
        x, y = GATE_C[0] + RNG.uniform(-2400, 2400), GATE_C[1] + RNG.uniform(-2400, 2400)
        if add_ism("SM_RT_Rubble_S", x, y, RNG.uniform(0, 360), RNG.uniform(0.3, 0.45), collide=True, sink=20.0, pad=20):
            k += 1
    print("gate rim", len(P) - n0)


reichstag()
gate_rim()
facade_scenes()
map_decals()
pad_edges()
ghost_streets()
clutter()
street_props()

json.dump({"tag": TAG, "placements": P}, open(OUT + "placements.json", "w"), indent=0, default=lambda o: o.item())
counts = {}
for p in P:
    key = p["kind"] + ":" + p.get("group", p.get("mesh", ""))
    counts[key] = counts.get(key, 0) + 1
print(json.dumps(counts, indent=0))

# preview on the play area (north up)
img = Image.open(ROOT + "detail/analysis/layers.png").convert("RGB")
dr = ImageDraw.Draw(img)
j_lo, i_hi = (PLAY_Y[0] - CORNER) / Q, (PLAY_X[1] - CORNER) / Q
col = {"decal": (60, 120, 255), "bp": (255, 60, 60), "ism": (255, 220, 0)}
for p in P:
    c, r = (p["y"] - CORNER) / Q - j_lo, i_hi - (p["x"] - CORNER) / Q
    dr.point((c, r), fill=col[p["kind"]])
img.save(OUT + "preview.png")

# debug: masks round the Reichstag (red = occupied, blue = route band, dark = hard/road, green = free)
if os.environ.get("DET_DEBUG"):
    i0, j0 = ij(RT_C[0] - 14000, RT_C[1] - 14000)
    i1, j1 = ij(RT_C[0] + 14000, RT_C[1] + 14000)
    sub = lambda m: m[j0:j1, i0:i1].T[::-1, :]
    rgb = np.zeros(sub(occ).shape + (3,), np.uint8)
    rgb[sub(free_solid)] = (40, 160, 60)
    rgb[sub(occ)] = (200, 40, 40)
    rgb[sub(route_mask)] = (40, 60, 220)
    rgb[sub(hard | road)] = (30, 30, 30)
    rgb[sub(~inside)] = (90, 0, 90)
    Image.fromarray(rgb).resize((rgb.shape[1] * 3, rgb.shape[0] * 3), Image.NEAREST).save(OUT + "debug_reichstag.png")
