"""Placement generator for the Ger_Berlin atmosphere per district (plain python with numpy and Pillow, outside Unreal).

    python Source/Tools/BerlinMap/atmosphere/atmosphere_gen.py

Reads   Saved/Berlin/terrain/Ger_Berlin_layout.json    regions, features, lanes and roads, bounds, the height file
        Saved/Berlin/terrain/Ger_Berlin_dress.json     ruins, wrecks, cover, furniture
        Saved/Berlin/terrain/Ger_Berlin_masks.npz      carriageway and crater masks on the landscape vertex grid
        Source/Tools/BerlinMap/atmosphere/fire_host_table.json   where a fire stands inside a hero ruin (atmosphere_host_probe.py)
Writes  Saved/Berlin/terrain/Ger_Berlin_atmosphere.json          everything atmosphere_place.py puts on the map
        Saved/Berlin/atmosphere/textures/T_BerlinAtmo_Districts.png   the district mask the sheet material reads:
                R low smoke and dust of Mitte, G the crater's green mist, B the smoke of the east (downwind of the fires)
        Saved/Berlin/atmosphere/atmosphere_preview.png           the mask over the lanes, fires and sprite systems

Order: this file, then atmosphere_build.py (imports the mask), then berlin_map_build.step_atmosphere().
Deterministic: no random numbers except a fixed-seed jitter.
"""
import array
import json
import math
import os
import random

import numpy as np
from PIL import Image

ROOT = "D:/UE5Projects/RTS_Survival/"
LAYOUT_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_layout.json"
DRESS_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_dress.json"
MASKS_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_masks.npz"
HOST_TABLE = ROOT + "Source/Tools/BerlinMap/atmosphere/fire_host_table.json"
OUT_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_atmosphere.json"
TEXTURE_DIR = ROOT + "Saved/Berlin/atmosphere/textures/"
MASK_NAME = "T_BerlinAtmo_Districts"
PREVIEW = ROOT + "Saved/Berlin/atmosphere/atmosphere_preview.png"
SEED = 19450430

# ---- the wind: the sun's light function drifts its cloud shadows this way (Docs/Lighting/BerlinLighting.md) ------------
WIND = (0.22, 1.0)                      # world XY, from the west and a little north; normalised below
WIND_SPEED = 140.0                      # cm/s, MPC_BerlinSmokePall RayDetailDrift x CloudDriftScale at the time of writing

# ---- the district mask ---------------------------------------------------------------------------------------------
MASK_RECT = (-30000.0, 30000.0, -54000.0, 54000.0)      # world xmin, xmax, ymin, ymax covered by the texture
MASK_SIZE = (256, 512)                                  # texels along X (image width), along Y (image height)
MITTE_X = (-10000.0, -5500.0, 13500.0, 18000.0)         # fade in from, to; fade out from, to (cm, north-south)
MITTE_Y = (1500.0, 6500.0, 19000.0, 25000.0)            # east of the crater up to the start of the east
MITTE_BASE = 0.34                                       # haze on open ground inside Mitte; ruin blocks go up to 1
LANE_CUT = 0.72                                         # how much of the haze a carriageway loses
CRATER_HALO = (4300.0, 11500.0, 0.42)                   # the glow holds to this radius, is gone at this one, halo strength
EAST_BASE = (19000.0, 27000.0, 0.16)                    # standing smoke of the east: fade in over this Y range, strength
START_KEEP = (5500.0, 10000.0)                          # no smoke inside the first radius of the player start, full outside the second
PLUME = (1200.0, 0.2, 15000.0)                          # half width at the fire, widening per cm downwind, length

# ---- sheets: ground-parallel translucent cards carrying the mask ---------------------------------------------------------
TILE = 8000.0                           # cm; one card. Cards outside the view are frustum-culled one by one
TILE_MIN = 0.05                         # a tile is placed when the mask reaches this anywhere on it
SHEET_HEIGHTS = (300.0, 650.0)          # cm above the median ground: two layers give the haze its depth (parallax)

# ---- ground smoke sprites of Mitte ------------------------------------------------------------------------------------
SMOKE_SPACING = 5200.0                  # cm grid; one sprite system covers a 50 m box and drifts on downwind
SMOKE_MIN_DENSITY = 0.62                # only in ruin blocks, not over lanes
SMOKE_MAX = 34

# ---- fires of the east -----------------------------------------------------------------------------------------------
FIRE_COUNT = 5
FIRE_MIN_Y = 20000.0
FIRE_START_CLEAR = 9000.0               # cm from the player start: the first minute stays readable
FIRE_SPACING = 5000.0
FIRE_LANE_CLEAR = 350.0                 # cm beyond the lane's half width
FIRE_OPENING_CLEAR = 2000.0             # from passes, gates, bridges
FIRE_COVER_CLEAR = 600.0                # from cover pieces and rubble heaps that carry prone sockets
FIRE_FURNITURE_CLEAR = 300.0
FIRE_EDGE_CLEAR = 2500.0                # from the play area edge
HERO_PIECES = ("E1_CornerTower", "E2_Courtyard", "E3_CoreRow")
WRECK_BPS = ("BP_Env_DestroyedT34", "BP_Env_DestroyedPzIII", "BP_Env_DestroyedPzII", "BP_Env_DestroyedPz38T",
             "BP_Env_Destroyed_KV1E", "BP_Civilian_Dest_Bus")
WRECK_FIRE_HEIGHT = {"BP_Civilian_Dest_Bus": 350.0}     # cm above the wreck's pivot: the roof of the bus ...
WRECK_FIRE_HEIGHT_TANK = 150.0          # ... the engine deck of a tank
HEAP_COVER = ("DBK_Brick_Rubble_A", "DBK_Cinderblock_Rubble_A", "DBK_Concrete_Rubble_Mid_A", "DBK_Concrete_Rubble_Mid_B",
              "DBK_Concrete_Rubble_Mid_C")
LIGHT_HEIGHT = 260.0                    # cm above the fire
SHIMMER_CARD = 2600.0           # cm, one card; its shimmer fades to nothing at the edge
SHIMMER_RING = 2500.0           # cm from the crater centre to the ring of cards
SHIMMER_COUNT = 6
SHIMMER_HEIGHT = 140.0          # cm above the rim: the cards never cut the ground
LIGHT_PHASES = 6                        # saved light-function instances MI_LF_BerlinAtmo_Fire_0..5


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def box_blur(image, radius_x, radius_y, passes=3):
    """Three box blurs are close to a Gaussian; cumulative sums keep it fast without scipy."""
    out = image.astype(np.float64)
    for _ in range(passes):
        for axis, radius in ((1, int(round(radius_x))), (0, int(round(radius_y)))):
            if radius < 1:
                continue
            padded = np.concatenate([np.repeat(out.take([0], axis=axis), radius + 1, axis=axis), out,
                                     np.repeat(out.take([-1], axis=axis), radius, axis=axis)], axis=axis)
            summed = np.cumsum(padded, axis=axis)
            n = out.shape[axis]
            hi = summed.take(range(2 * radius + 1, 2 * radius + 1 + n), axis=axis)
            lo = summed.take(range(0, n), axis=axis)
            out = (hi - lo) / (2 * radius + 1)
    return out


class Ground:
    """Landscape height from the generator's height file, as berlin_map_build.ground_z reads it."""

    def __init__(self, layout):
        self.lay = layout["landscape"]
        with open(layout["files"]["height"], "rb") as handle:
            self.heights = array.array("H", handle.read())

    def z(self, x, y):
        lay = self.lay
        vx, vy = lay["verts_x"], lay["verts_y"]
        fx = min(max((x - lay["corner"][0]) / lay["quad_cm"], 0.0), vx - 1.001)
        fy = min(max((y - lay["corner"][1]) / lay["quad_cm"], 0.0), vy - 1.001)
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy

        def h(i, j):
            return (self.heights[j * vx + i] - 32768.0) * 100.0 / 128.0

        return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) + h(ix, iy + 1) * (1 - tx) * ty + h(ix + 1, iy + 1) * tx * ty)


def segment_distance(px, py, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length2 = dx * dx + dy * dy
    t = 0.0 if length2 == 0.0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length2))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def lane_clearance(layout, x, y):
    """Distance to the nearest lane or road edge (negative: on the carriageway)."""
    best = 1e9
    for path in layout["paths"]:
        if path["kind"] not in ("lane", "road"):
            continue
        points = path["points"]
        for a, b in zip(points[:-1], points[1:]):
            best = min(best, segment_distance(x, y, a, b) - path["width"] / 2.0)
    return best


def grid_views(layout):
    """Texel centre coordinates of the mask (world cm) and the crop of the landscape vertex masks that covers it."""
    x0, x1, y0, y1 = MASK_RECT
    width, height = MASK_SIZE
    xs = x0 + (np.arange(width) + 0.5) * (x1 - x0) / width
    ys = y0 + (np.arange(height) + 0.5) * (y1 - y0) / height
    lay = layout["landscape"]
    ix = np.clip(np.round((xs - lay["corner"][0]) / lay["quad_cm"]).astype(int), 0, lay["verts_x"] - 1)
    iy = np.clip(np.round((ys - lay["corner"][1]) / lay["quad_cm"]).astype(int), 0, lay["verts_y"] - 1)
    return xs, ys, ix, iy


def district_mask(layout, dress, fires):
    x0, x1, y0, y1 = MASK_RECT
    width, height = MASK_SIZE
    xs, ys, ix, iy = grid_views(layout)
    X, Y = np.meshgrid(xs, ys)                      # arrays [row = Y, column = X], like the vertex masks
    texel_x, texel_y = (x1 - x0) / width, (y1 - y0) / height
    masks = np.load(MASKS_FILE)
    lane = box_blur(masks["carriageway"][np.ix_(iy, ix)].astype(float), 350.0 / texel_x, 350.0 / texel_y)
    lane = np.clip(lane * 1.5, 0.0, 1.0)
    crater_shape = box_blur(masks["crater"][np.ix_(iy, ix)].astype(float), 900.0 / texel_x, 900.0 / texel_y)

    # ruin density: every ruin splats its footprint, blurred to the size of a block
    splat = np.zeros((height, width))
    for ruin in dress["ruins"]:
        c = int((ruin["x"] - x0) / texel_x)
        r = int((ruin["y"] - y0) / texel_y)
        if 0 <= c < width and 0 <= r < height:
            splat[r, c] += 2.0 if ruin["piece"][:2] in ("E1", "E2", "E3", "B3") else 1.0
    density = box_blur(splat, 1500.0 / texel_x, 1500.0 / texel_y)
    density = np.clip(density / max(np.percentile(density[density > 0], 93), 1e-6), 0.0, 1.0)

    mitte_env = smoothstep(MITTE_X[0], MITTE_X[1], X) * (1.0 - smoothstep(MITTE_X[2], MITTE_X[3], X)) * \
        smoothstep(MITTE_Y[0], MITTE_Y[1], Y) * (1.0 - smoothstep(MITTE_Y[2], MITTE_Y[3], Y))
    mitte = mitte_env * (MITTE_BASE + (1.0 - MITTE_BASE) * density) * (1.0 - LANE_CUT * lane)

    cx, cy = next(r for r in layout["regions"] if r["id"] == "crater")["centre"]
    crater_dist = np.hypot(X - cx, Y - cy)
    crater = np.maximum(np.clip(crater_shape * 1.15, 0.0, 1.0), CRATER_HALO[2] * (1.0 - smoothstep(CRATER_HALO[0], CRATER_HALO[1], crater_dist)))

    wind = np.array(WIND) / math.hypot(*WIND)
    east = EAST_BASE[2] * smoothstep(EAST_BASE[0], EAST_BASE[1], Y)
    for fire in fires:
        along = (X - fire["x"]) * wind[0] + (Y - fire["y"]) * wind[1]
        across = -(X - fire["x"]) * wind[1] + (Y - fire["y"]) * wind[0]
        half = PLUME[0] + PLUME[1] * np.clip(along, 0.0, None)
        plume = np.exp(-(across / half) ** 2) * smoothstep(-900.0, 1200.0, along) * (1.0 - smoothstep(PLUME[2] * 0.55, PLUME[2], along))
        east = np.maximum(east, plume * (0.55 + 0.45 * (PLUME[0] / half)))
    sx, sy = next(f for f in layout["features"] if f["id"] == "start")["points"][0]
    east *= smoothstep(START_KEEP[0], START_KEEP[1], np.hypot(X - sx, Y - sy)) * (1.0 - 0.45 * lane)
    # the play area edge: nothing hangs over the map border
    b = layout["bounds"]
    edge = smoothstep(b["xmin"] - 2500.0, b["xmin"] + 1500.0, X) * (1.0 - smoothstep(b["xmax"] - 1500.0, b["xmax"] + 2500.0, X)) * \
        smoothstep(b["ymin"] - 2500.0, b["ymin"] + 1500.0, Y) * (1.0 - smoothstep(b["ymax"] - 1500.0, b["ymax"] + 2500.0, Y))
    rgb = np.stack([mitte * edge, crater * edge, east * edge], axis=-1)
    return np.clip(rgb, 0.0, 1.0), density, lane, (X, Y)


def sample(field, x, y):
    x0, x1, y0, y1 = MASK_RECT
    c = int((x - x0) / (x1 - x0) * MASK_SIZE[0])
    r = int((y - y0) / (y1 - y0) * MASK_SIZE[1])
    if 0 <= c < MASK_SIZE[0] and 0 <= r < MASK_SIZE[1]:
        return float(field[r, c])
    return 0.0


# ---- fires ---------------------------------------------------------------------------------------------------------
def rotate(lx, ly, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return lx * c - ly * s, lx * s + ly * c


def fire_candidates(layout, dress):
    table = {}
    if os.path.exists(HOST_TABLE):
        with open(HOST_TABLE, "r") as handle:
            table = json.load(handle)
    out = []
    for index, ruin in enumerate(dress["ruins"]):
        if ruin["piece"] not in HERO_PIECES:
            continue
        lx, ly, lz = table.get(ruin["piece"], [0.0, 0.0, 60.0])[:3]
        dx, dy = rotate(lx, ly, ruin["yaw"])
        out.append(dict(kind="ruin", host="RUIN_%s_%04d" % (ruin["piece"], index), host_x=ruin["x"], host_y=ruin["y"],
                        x=ruin["x"] + dx, y=ruin["y"] + dy, z=ruin["z"] + lz, system="Ruin", piece=ruin["piece"]))
    for actor in dress["actors"]:
        name = actor["bp"].rsplit("/", 1)[1]
        if name in WRECK_BPS:
            out.append(dict(kind="wreck", host=actor["label"], host_x=actor["x"], host_y=actor["y"], x=actor["x"], y=actor["y"],
                            z=actor["z"] + WRECK_FIRE_HEIGHT.get(name, WRECK_FIRE_HEIGHT_TANK), system="Wreck", piece=name))
    return out


def fire_checks(layout, dress, candidate):
    """Every rule a fire must pass; returns (ok, measured distances)."""
    x, y = candidate["x"], candidate["y"]
    b = layout["bounds"]
    sx, sy = next(f for f in layout["features"] if f["id"] == "start")["points"][0]
    openings = [f["points"][0] for f in layout["features"] if f["kind"] in ("pass", "gate", "bridge")]
    cover = [(a["x"], a["y"]) for a in dress["actors"] if "Cover" in a["bp"]]
    for key, items in dress["ism"].items():
        if key.rsplit("/", 1)[1] in HEAP_COVER:
            cover += [(i[0], i[1]) for i in items if i[6] >= 0.8]
    cover += [(f[1], f[2]) for f in dress["feet"] if "CollapseHeap" in f[0]]
    furniture = [(f[1], f[2]) for f in dress["furniture"]]
    measured = dict(
        lane=round(lane_clearance(layout, x, y)),
        start=round(math.hypot(x - sx, y - sy)),
        opening=round(min(math.hypot(x - ox, y - oy) for ox, oy in openings)),
        cover=round(min(math.hypot(x - cx, y - cy) for cx, cy in cover)),
        furniture=round(min(math.hypot(x - fx, y - fy) for fx, fy in furniture)),
        edge=round(min(x - b["xmin"], b["xmax"] - x, y - b["ymin"], b["ymax"] - y)))
    ok = (y >= FIRE_MIN_Y and measured["lane"] >= FIRE_LANE_CLEAR and measured["start"] >= FIRE_START_CLEAR and
          measured["opening"] >= FIRE_OPENING_CLEAR and measured["cover"] >= FIRE_COVER_CLEAR and
          measured["furniture"] >= FIRE_FURNITURE_CLEAR and measured["edge"] >= FIRE_EDGE_CLEAR)
    return ok, measured


def pick_fires(layout, dress):
    """Greedy: ruins and wrecks alternate where possible, each new fire as far from the ones already picked as it can be."""
    passed, rejected = [], []
    for candidate in fire_candidates(layout, dress):
        ok, measured = fire_checks(layout, dress, candidate)
        candidate["checks"] = measured
        (passed if ok else rejected).append(candidate)
    alex = next(f for f in layout["features"] if f["id"] == "alexanderplatz")["points"][0]
    picked = []
    while len(picked) < FIRE_COUNT and passed:
        def score(c):
            spacing = min([math.hypot(c["x"] - p["x"], c["y"] - p["y"]) for p in picked] or [30000.0])
            if spacing < FIRE_SPACING:
                return -1.0
            want = "wreck" if sum(1 for p in picked if p["kind"] == "ruin") > sum(1 for p in picked if p["kind"] == "wreck") * 2 else "ruin"
            near_alex = 1.0 - min(math.hypot(c["x"] - alex[0], c["y"] - alex[1]) / 30000.0, 1.0)
            return min(spacing, 20000.0) / 20000.0 + (0.5 if c["kind"] == want else 0.0) + 0.6 * near_alex
        best = max(passed, key=score)
        if score(best) < 0.0:
            break
        picked.append(best)
        passed.remove(best)
    picked.sort(key=lambda c: (c["y"], c["x"]))
    for index, fire in enumerate(picked):
        fire["phase"] = index % LIGHT_PHASES
        fire["label"] = "ATM_Fire_%d" % index
    return picked, rejected


# ---- sheets and sprite systems -----------------------------------------------------------------------------------------
# ---- smouldering ruins: the core of the smoke layer, map-wide ------------------------------------------------------------
SMOULDER_CLASS = {"E1": "Hero", "E2": "Hero", "E3": "Hero", "B3": "Shell", "B4": "Shell", "B5": "Shell", "E5": "Shell", "E7": "Shell",
                  "B7": "Shell", "E4": "Wisp", "E6": "Wisp", "B8": "Wisp", "B6": "Wisp"}     # B1/B2 lone facades do not smoulder
SMOULDER_BASE = 0.17            # share of the ruins that smoulder anywhere ...
SMOULDER_EAST = 0.30            # ... plus this where the east mask is full, plus this round the crater
SMOULDER_CRATER = 0.30
SMOULDER_MITTE = 0.08
SMOULDER_SPARSE = 0.35          # factor in the north (x above 14000) and at the park edge (y below -9000)
SMOULDER_HEAP_FACTOR = 0.6      # collapse heaps of the skirts smoulder a little less often than ruins
SMOULDER_SPACING = 1400.0
SMOULDER_MAX = 120
SMOULDER_LANE_CLEAR = 250.0
SMOULDER_START_CLEAR = 7000.0
SMOULDER_FIRE_CLEAR = 1800.0
SMOULDER_HEIGHT = {"Hero": 120.0, "Shell": 90.0, "Wisp": 60.0}
AWAKE_RADIUS = 15000.0          # the effect type pauses a system further than this from the camera


def smoulder_spots(layout, dress, rgb, fires, ground):
    rng = random.Random(SEED + 7)
    table = json.load(open(HOST_TABLE)) if os.path.exists(HOST_TABLE) else {}
    b = layout["bounds"]
    sx, sy = next(f for f in layout["features"] if f["id"] == "start")["points"][0]
    openings = [f["points"][0] for f in layout["features"] if f["kind"] in ("pass", "gate", "bridge")]
    cover = [(a["x"], a["y"]) for a in dress["actors"] if "Cover" in a["bp"]]
    candidates = []
    for index, ruin in enumerate(dress["ruins"]):
        kind = SMOULDER_CLASS.get(ruin["piece"][:2])
        if kind is None:
            continue
        lx, ly = (table.get(ruin["piece"], [0.0, 0.0])[:2]) if kind == "Hero" else (0.0, 0.0)
        dx, dy = rotate(lx, ly, ruin["yaw"])
        candidates.append((ruin["x"] + dx, ruin["y"] + dy, kind, "RUIN_%s_%04d" % (ruin["piece"], index), 3.0 if kind == "Hero" else 1.0))
    for index, foot in enumerate(dress["feet"]):
        if "CollapseHeap" in foot[0]:
            candidates.append((foot[1], foot[2], "Wisp" if foot[0].endswith("_S") else "Shell", "FOOT_heap_%04d" % index, SMOULDER_HEAP_FACTOR))
    rng.shuffle(candidates)
    picked = []
    for x, y, kind, host, factor in candidates:
        if not (b["xmin"] + 1500 < x < b["xmax"] - 1500 and b["ymin"] + 1500 < y < b["ymax"] - 1500):
            continue
        chance = SMOULDER_BASE + SMOULDER_EAST * min(sample(rgb[..., 2], x, y) * 2.5, 1.0) + \
            SMOULDER_CRATER * min(sample(rgb[..., 1], x, y) * 2.0, 1.0) + SMOULDER_MITTE * sample(rgb[..., 0], x, y)
        if x > 14000.0 or y < -9000.0:
            chance *= SMOULDER_SPARSE
        if rng.random() > chance * factor:
            continue
        if lane_clearance(layout, x, y) < SMOULDER_LANE_CLEAR or math.hypot(x - sx, y - sy) < SMOULDER_START_CLEAR:
            continue
        if min(math.hypot(x - ox, y - oy) for ox, oy in openings) < 1500.0:
            continue
        if not host.startswith("FOOT_") and min(math.hypot(x - cx, y - cy) for cx, cy in cover) < 400.0:
            continue
        if any(math.hypot(x - f["x"], y - f["y"]) < SMOULDER_FIRE_CLEAR for f in fires):
            continue
        if any(math.hypot(x - p[0], y - p[1]) < SMOULDER_SPACING for p in picked):
            continue
        picked.append([round(x, 1), round(y, 1), round(ground.z(x, y) + SMOULDER_HEIGHT[kind], 1), kind, host])
        if len(picked) >= SMOULDER_MAX:
            break
    picked.sort(key=lambda p: (p[1], p[0]))
    return picked


def worst_counts(spots, half_x=1800.0, half_y=3250.0):
    """Most smoulder systems in one game view (about 36 x 65 m of ground) and within the awake radius of one point."""
    view = awake = 0
    view_at = None
    for cx, cy, *_ in spots:
        n = sum(1 for p in spots if abs(p[0] - cx) <= half_x and abs(p[1] - cy) <= half_y)
        if n > view:
            view, view_at = n, [round(cx), round(cy)]
        awake = max(awake, sum(1 for p in spots if math.hypot(p[0] - cx, p[1] - cy) <= AWAKE_RADIUS))
    return view, view_at, awake


def sheet_tiles(rgb, ground):
    x0, x1, y0, y1 = MASK_RECT
    best = rgb.max(axis=-1)
    tiles = []
    heights = []
    nx, ny = int((x1 - x0) / TILE), int((y1 - y0) / TILE)
    for j in range(ny):
        for i in range(nx):
            cx, cy = x0 + (i + 0.5) * TILE, y0 + (j + 0.5) * TILE
            c0, c1 = int(i * TILE / (x1 - x0) * MASK_SIZE[0]), int((i + 1) * TILE / (x1 - x0) * MASK_SIZE[0])
            r0, r1 = int(j * TILE / (y1 - y0) * MASK_SIZE[1]), int((j + 1) * TILE / (y1 - y0) * MASK_SIZE[1])
            block = rgb[r0:r1, c0:c1]
            if best[r0:r1, c0:c1].max() < TILE_MIN:
                continue
            channel_max = [float(block[..., k].max()) for k in range(3)]
            # the second, higher layer only where the haze is thick enough to be worth a second translucent pass
            layers = 2 if max(channel_max[0], channel_max[2]) > 0.3 else 1
            tiles.append([cx, cy, layers])
            heights.append(ground.z(cx, cy))
    base_z = float(np.median(heights)) if heights else 0.0
    return tiles, base_z


def smoke_systems(rgb, density, lane, ground):
    """One sprite system per ruin block of Mitte, on a grid, kept where the mask is dense and off the lanes."""
    rng = random.Random(SEED)
    x0, x1, y0, y1 = MASK_RECT
    out = []
    y = MITTE_Y[0]
    while y < MITTE_Y[3]:
        x = MITTE_X[0]
        while x < MITTE_X[3]:
            px, py = x + rng.uniform(-900.0, 900.0), y + rng.uniform(-900.0, 900.0)
            value = sample(rgb[..., 0], px, py)
            if value >= SMOKE_MIN_DENSITY * (MITTE_BASE + (1.0 - MITTE_BASE) * 0.6) and sample(lane, px, py) < 0.35:
                out.append([round(px, 1), round(py, 1), round(ground.z(px, py), 1), round(value, 3)])
            x += SMOKE_SPACING
        y += SMOKE_SPACING
    out.sort(key=lambda item: -item[3])
    return out[:SMOKE_MAX]


def best_view(field, half_x=2200.0, half_y=3400.0):
    """Centre of the game-camera view (about 44 x 68 m of ground here) that holds the most of one mask channel."""
    x0, x1, y0, y1 = MASK_RECT
    rx = max(1, int(half_x / (x1 - x0) * MASK_SIZE[0]))
    ry = max(1, int(half_y / (y1 - y0) * MASK_SIZE[1]))
    blurred = box_blur(field, rx, ry, passes=1)
    r, c = np.unravel_index(np.argmax(blurred), blurred.shape)
    return [round(x0 + (c + 0.5) * (x1 - x0) / MASK_SIZE[0]), round(y0 + (r + 0.5) * (y1 - y0) / MASK_SIZE[1])]


def preview(rgb, layout, fires, smoke, tiles):
    scale = 2
    image = Image.fromarray((np.clip(rgb * 1.6, 0, 1) * 255).astype(np.uint8), "RGB").resize((MASK_SIZE[0] * scale, MASK_SIZE[1] * scale))
    from PIL import ImageDraw
    draw = ImageDraw.Draw(image)
    x0, x1, y0, y1 = MASK_RECT

    def px(x, y):
        return ((x - x0) / (x1 - x0) * MASK_SIZE[0] * scale, (y - y0) / (y1 - y0) * MASK_SIZE[1] * scale)

    for path in layout["paths"]:
        if path["kind"] in ("lane", "road"):
            draw.line([px(*p) for p in path["points"]], fill=(110, 110, 110), width=1)
    for cx, cy, layers in tiles:
        a, b = px(cx - TILE / 2, cy - TILE / 2), px(cx + TILE / 2, cy + TILE / 2)
        draw.rectangle([a, b], outline=(70, 70, 90) if layers == 1 else (120, 120, 150))
    for x, y, _z, _v in smoke:
        p = px(x, y)
        draw.ellipse([p[0] - 3, p[1] - 3, p[0] + 3, p[1] + 3], outline=(255, 255, 255))
    for fire in fires:
        p = px(fire["x"], fire["y"])
        draw.ellipse([p[0] - 5, p[1] - 5, p[0] + 5, p[1] + 5], fill=(255, 150, 0))
    sx, sy = next(f for f in layout["features"] if f["id"] == "start")["points"][0]
    p = px(sx, sy)
    draw.rectangle([p[0] - 4, p[1] - 4, p[0] + 4, p[1] + 4], outline=(0, 255, 255))
    # image rows are world Y (east down), columns world X (north right): turn it so north is up and east is right
    image.transpose(Image.Transpose.ROTATE_90).save(PREVIEW)


def main():
    with open(LAYOUT_FILE, "r") as handle:
        layout = json.load(handle)
    with open(DRESS_FILE, "r") as handle:
        dress = json.load(handle)
    ground = Ground(layout)
    fires, rejected = pick_fires(layout, dress)
    rgb, density, lane, _grid = district_mask(layout, dress, fires)
    os.makedirs(TEXTURE_DIR, exist_ok=True)
    rgba = np.concatenate([rgb, np.ones(rgb.shape[:2] + (1,))], axis=-1)
    Image.fromarray((rgba * 255.0 + 0.5).astype(np.uint8), "RGBA").save(TEXTURE_DIR + MASK_NAME + ".png")
    tiles, base_z = sheet_tiles(rgb, ground)
    smoke = smoke_systems(rgb, density, lane, ground)
    spots = smoulder_spots(layout, dress, rgb, fires, ground)
    in_view, view_at, awake = worst_counts(spots)
    classes = {k: sum(1 for p in spots if p[3] == k) for k in ("Wisp", "Shell", "Hero")}
    wind = [WIND[0] / math.hypot(*WIND), WIND[1] / math.hypot(*WIND)]
    crater = next(r for r in layout["regions"] if r["id"] == "crater")
    cx, cy = crater["centre"]
    crater_radius = max(math.hypot(p[0] - cx, p[1] - cy) for p in crater["points"])
    crater_floor = min(ground.z(cx + dx, cy + dy) for dx in (-1500, 0, 1500) for dy in (-1500, 0, 1500))
    crater_rim = float(np.median([ground.z(cx + crater_radius * math.cos(a), cy + crater_radius * math.sin(a))
                                  for a in np.linspace(0, 2 * math.pi, 24, endpoint=False)]))
    lights = [dict(label="ATM_FireLight_%d" % i, fire=f["label"], x=round(f["x"], 1), y=round(f["y"], 1),
                   z=round(f["z"] + LIGHT_HEIGHT, 1), phase=f["phase"]) for i, f in enumerate(fires)]
    out = dict(
        wind=dict(direction=wind, speed=WIND_SPEED, yaw=round(math.degrees(math.atan2(wind[1], wind[0])), 2)),
        mask=dict(texture=MASK_NAME, rect=list(MASK_RECT), size=list(MASK_SIZE), channels="R Mitte smoke, G crater mist, B east smoke"),
        sheets=dict(tile=TILE, base_z=round(base_z, 1), heights=list(SHEET_HEIGHTS), tiles=[[round(t[0]), round(t[1]), t[2]] for t in tiles]),
        mitte_smoke=smoke,
        smoulder=spots,
        smoulder_stats=dict(classes=classes, worst_in_game_view=in_view, worst_view_at=view_at, worst_awake=awake,
                            districts=dict(east=sum(1 for p in spots if p[1] > 19000), mitte=sum(1 for p in spots if 3000 < p[1] <= 19000),
                                           crater=sum(1 for p in spots if -14000 < p[1] <= 3000), west=sum(1 for p in spots if p[1] <= -14000))),
        crater=dict(x=cx, y=cy, radius=round(crater_radius), floor_z=round(crater_floor, 1), rim_z=round(crater_rim, 1),
                    # heat shimmer: ground-parallel cards over the bowl, one in the middle and a ring, a little above the rim
                    shimmer_cards=[[round(cx), round(cy), round(crater_rim + SHIMMER_HEIGHT), SHIMMER_CARD]] +
                    [[round(cx + SHIMMER_RING * math.cos(a)), round(cy + SHIMMER_RING * math.sin(a)), round(crater_rim + SHIMMER_HEIGHT + 40 * (k % 2)),
                      SHIMMER_CARD] for k, a in enumerate(np.linspace(0.4, 0.4 + 2 * math.pi, SHIMMER_COUNT, endpoint=False))]),
        fires=[dict(label=f["label"], kind=f["kind"], host=f["host"], host_x=f["host_x"], host_y=f["host_y"], piece=f["piece"],
                    system=f["system"], x=round(f["x"], 1), y=round(f["y"], 1), z=round(f["z"], 1), phase=f["phase"],
                    checks=f["checks"]) for f in fires],
        fire_lights=lights,
        views=dict(smoulder=view_at, mitte=best_view(rgb[..., 0]), crater=[round(cx), round(cy)],
                   east=[round(fires[len(fires) // 2]["x"]), round(fires[len(fires) // 2]["y"])] if fires else [11000, 26500]),
        rejected_fire_hosts=[dict(host=r["host"], checks=r["checks"]) for r in rejected if r["y"] >= FIRE_MIN_Y],
    )
    with open(OUT_FILE, "w") as handle:
        json.dump(out, handle, indent=1)
    preview(rgb, layout, fires, smoke, tiles)
    sheets = sum(t[2] for t in tiles)
    print("tiles %d (sheet cards %d), base z %.0f; mitte smoke systems %d; fires %d" % (len(tiles), sheets, base_z, len(smoke), len(fires)))
    for fire in fires:
        print("  %s %-5s on %-28s at %7.0f %7.0f %5.0f  %s" % (fire["label"], fire["kind"], fire["host"], fire["x"], fire["y"], fire["z"], fire["checks"]))
    for r in out["rejected_fire_hosts"]:
        print("  rejected %-28s %s" % (r["host"], r["checks"]))
    print("smoulder %d %s" % (len(spots), out["smoulder_stats"]))
    print("crater %s; views %s" % (out["crater"], out["views"]))
    print("mask coverage: mitte %.3f crater %.3f east %.3f (mean over the texture)" % tuple(rgb.reshape(-1, 3).mean(axis=0)))


if __name__ == "__main__":
    main()
