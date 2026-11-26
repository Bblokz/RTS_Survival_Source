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


def add_decal(group, mat, x, y, half, rot=None, sort=3, color=None):
    P.append({"kind": "decal", "group": group, "mat": mat, "x": round(x, 1), "y": round(y, 1),
              "half": round(half, 1), "rot": round(RNG.uniform(0, 360) if rot is None else rot, 1), "sort": sort,
              **({"color": color} if color else {})})
    jj_, ii_ = rect_cells(x, y, 0, half * 0.8, half * 0.8)
    dec_cov[jj_, ii_] += 1


def add_bp(group, path, x, y, yaw, hx, hy, extra=None, pad=50, solid=True):
    if solid and not claim(x, y, yaw, hx, hy, pad):
        return False
    P.append({"kind": "bp", "group": group, "path": path, "x": round(x, 1), "y": round(y, 1), "yaw": round(yaw, 1),
              **(extra or {})})
    return True


def add_ism(mesh, x, y, yaw, scale=1.0, collide=False, sink=0.0, check=True, pad=20):
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
              "scale": round(scale, 3), "collide": collide, "sink": sink})
    return True


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
    # 1b sandstone, plaster and grit decals all round the ruin apron (2-14 m out)
    k = 0
    while k < 120:
        ang = RNG.uniform(0, 2 * math.pi)
        r = RNG.uniform(4500, 9000)
        x, y = RT_C[0] + r * math.cos(ang), RT_C[1] + r * math.sin(ang)
        i, j = ij(x, y)
        if not (1.5 < ruin_dist[j, i] < 14.0) or not free_decal[j, i] or dec_density[j, i] > 0.6:
            continue
        mat = RNG.choice(["14_SandstoneBlocks"] * 4 + ["04_PlasterDust", "15_GritSpill", "10_GlassShards",
                                                       "05_ConcreteRebar", "09_AshSoot"])
        add_decal("rt_apron", RUB[mat], x, y, RNG.uniform(180, 300))
        k += 1
    # 1c Soviet camp clutter round the base buildings (crate stacks, drums, barrels, ammo boxes, pallets)
    camps = [(7209.4, -18594.8), (9009.4, -18594.8), (7112.0, -13319.6), (9217.0, -13338.9), (1912.7, -18395.7),
             (1914.3, -13599.0), (3112.0, -17194.6), (5200, -21800), (11400, -21500), (5000, -10200), (11600, -10400)]
    bp_items = [("BP_Airport_Dest_CrateStack01", 130, 130), ("BP_Airport_Dest_CrateStack02", 130, 130),
                ("BP_Airport_Dest_CrateStack03", 130, 130), ("BP_Prypyat_FuelDrums", 90, 70),
                ("BP_Prypyat_ServiceCrates", 110, 90), ("BP_Airport_Dest_BarrelStack", 110, 110)]
    ism_items = ["SM_BarrelCollection1", "SM_CrateCollection1", "SM_Ger_Ammobox", "SM_Ger_Ammobox_Long", "SM_Pallet",
                 "SM_Wooden_Pallette", "Barrel1", "Barrel2", "Barrel4", "SM_Tire"]
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
    for (x, y, yaw, mesh) in [(4300, -22600, 0, "SM_Tente_1"), (12200, -22300, 180, "SM_Tente_1"),
                              (5600, -24200, 90, "SM_Tente_2"), (10800, -24300, 90, "SM_Tente_2"),
                              (12800, -9200, 0, "SM_Tente_2")]:
        near_free(x, y, 4000, lambda a, b: add_ism(mesh, a, b, yaw, 1.0, collide=True, sink=10.0, pad=150), 400)
    # 1e Soviet flags at the portico, factory and barracks
    for (x, y) in [(3500, -14300), (3500, -17700), (6300, -20200), (10000, -20200), (12900, -16100)]:
        near_free(x, y, 2500, lambda a, b: add_bp("rt_flag", BP["Dest_SovietFlag"], a, b, 90, 60, 60, pad=60))
    # 1f defence lines: hedgehog / wire runs on arcs 80-105 m out on the open approaches (gaps where blocked)
    for r0 in (8200, 9800):
        for ang in np.arange(0, 360, 2.6):
            x = RT_C[0] + r0 * math.cos(math.radians(ang)) + RNG.normal(0, 70)
            y = RT_C[1] + r0 * 1.15 * math.sin(math.radians(ang)) + RNG.normal(0, 70)
            i, j = ij(x, y)
            if not (0 <= i < N and 0 <= j < N) or M["street"][j, i] or M["side_street"][j, i] and RNG.random() < 0.5:
                continue
            tang = ang + 90
            if RNG.random() < 0.3:
                add_bp("rt_wire", BP["BP_Military_Dest_BarbedWireLong"], x, y, tang, 300, 60, pad=40)
            else:
                name = RNG.choice(["BP_Military_Dest_Hedgehog", "BP_Military_Dest_Hedgehog",
                                   "BP_Military_Dest_LargeHedgehog", "BP_Military_Dest_Hedgehog4"])
                add_bp("rt_hedgehog", BP[name], x, y, RNG.uniform(0, 360), 120, 120, pad=80)
    # 1g sandbag walls round the pits, the barracks and the portico
    for (x, y) in [(1912.7, -18395.7), (1914.3, -13599.0), (7112.0, -13319.6), (9217.0, -13338.9), (3100, -15900),
                   (7209.4, -18594.8), (9009.4, -18594.8), (3600, -20300), (3600, -11600)]:
        for _ in range(2):
            name = RNG.choice(["BP_Cover_RusSandbags_SingleStraight", "BP_Cover_RusSandbags_DoubleStraight",
                               "BP_Cover_RusSandbags_Cornered", "BP_Cover_RusSandbags_LongArc"])
            yaw = RNG.choice([0, 90, 180, 270]) + RNG.normal(0, 10)
            near_free(x + RNG.normal(0, 500), y + RNG.normal(0, 500), 2200,
                      lambda a, b: add_bp("rt_sandbags", BP[name], a, b, yaw, 280, 90, pad=50))
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
    for route in ("RT_PatrolRoute_Ring", "RT_PatrolRoute_Gate"):
        pts = ASSETS["routes"][route]
        pts = pts + ([pts[0]] if route.endswith("Ring") else [])
        for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
            yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
            n = int(math.hypot(x1 - x0, y1 - y0) / 700)
            for k in range(n):
                t = (k + 0.5) / n
                add_decal("rt_tracks", TRACKS % RNG.integers(1, 4), x0 + (x1 - x0) * t + RNG.normal(0, 80),
                          y0 + (y1 - y0) * t + RNG.normal(0, 80), 400, rot=yaw + 90 + RNG.normal(0, 4), sort=0)
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
                  extra={"burnt": True}, pad=120):
            k += 1
    print("reichstag", len(P) - n0)


# ------------------------------------------------------------------------------------------------ 2. decals
def map_decals(target=900):
    n0 = len(P)
    cand = np.argwhere(free_decal & (dec_density < 0.35))
    RNG.shuffle(cand)
    placed = 0
    taken = np.zeros((N, N), bool)
    for j, i in cand:
        if placed >= target:
            break
        if taken[j, i]:
            continue
        x, y = world(i, j)
        d = ruin_dist[j, i]
        if road[j, i]:
            mat = RNG.choice([RUB["11_SettsTornUp"], RUB["15_GritSpill"], CRATER % RNG.integers(1, 7),
                              TRACKS % RNG.integers(1, 4)])
            half = 400 if "Tracks" in mat else RNG.uniform(180, 300)
        elif M["street"][j, i] or M["side_street"][j, i] or M["street_berm"][j, i]:
            mat = RNG.choice([RUB["12_PavingSlabs"], RUB["15_GritSpill"], RUB["10_GlassShards"], RUB["11_SettsTornUp"],
                              CONC % "Debris_0%d" % RNG.integers(1, 6)])
            half = RNG.uniform(180, 300) if "Berlin" in mat else RNG.uniform(350, 600)
        elif d < 9:
            mat = RNG.choice([RUB["00_BrickSpillFan"], RUB["01_BrickHeapSpread"], RUB["04_PlasterDust"],
                              RUB["06_RoofTiles"], RUB["07_SlateShards"], RUB["08_CharredTimber"], RUB["15_GritSpill"]])
            half = RNG.uniform(180, 290)
        elif dom[j, i] == SCRUB:
            mat = RNG.choice([CRATER % RNG.integers(1, 7), (CRATER % RNG.integers(1, 7)) + "_Water", RUB["09_AshSoot"]])
            half = RNG.uniform(200, 520)
        else:
            mat = RNG.choice([RUB["15_GritSpill"], RUB["09_AshSoot"], RUB["05_ConcreteRebar"], RUB["13_SteelScrap"],
                              CONC % "Scatter_03", CRATER % RNG.integers(1, 7)])
            half = RNG.uniform(180, 300) if "Berlin" in mat else RNG.uniform(300, 650)
        add_decal("map", mat, x, y, half, sort=1 if "Crater" in mat or "Tracks" in mat else 3)
        r = int(half / 100 * 1.6) + 4
        taken[max(0, j - r):j + r, max(0, i - r):i + r] = True
        placed += 1
    print("map decals", len(P) - n0)


# ------------------------------------------------------------------------------------------------ 3. furniture
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
            key, info = pieces[RNG.integers(len(pieces))]
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
def clutter(target=1400):
    """small non-colliding instances: planks, loose sandbags, tyres, barrels, furniture debris near ruins/streets."""
    n0 = len(P)
    meshes = (["SM_BerlinFurn_Debris_0", "SM_BerlinFurn_Debris_3"] * 4 + ["Plank2", "Plank4"] * 3 +
              ["SM_Ger_LooseSandBag_%d" % k for k in range(1, 6)] + ["Barrel3"])
    cand = np.argwhere(inside & ~hard & ~occ & ~road & (ruin_dist < 12))
    RNG.shuffle(cand)
    placed = 0
    for j, i in cand:
        if placed >= target:
            break
        x, y = world(i, j)
        for c in range(RNG.integers(1, 4)):
            mesh = meshes[RNG.integers(len(meshes))]
            if add_ism(mesh, x + RNG.normal(0, 150), y + RNG.normal(0, 150), RNG.uniform(0, 360),
                       RNG.uniform(0.8, 1.2) if "Debris" in mesh else 1.0):
                placed += 1
    print("clutter", len(P) - n0)


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
                      extra={"burnt": True}, pad=300):
                k += 1
        elif poles < 45:
            name = RNG.choice(["BP_Civilian_Dest_CrushablePoleLight", "BP_Civilian_Dest_CrushablePole_Electric1"])
            if add_bp("pole", BP[name], x, y, RNG.uniform(0, 360), 40, 40, pad=900):
                poles += 1
        if k >= 40 and poles >= 45:
            break
    print("street props", len(P) - n0)


reichstag()
map_decals()
furniture()
clutter()
street_props()

json.dump({"tag": TAG, "placements": P}, open(OUT + "placements.json", "w"), indent=0)
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
