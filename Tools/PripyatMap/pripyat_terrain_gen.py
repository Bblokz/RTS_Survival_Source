"""Terrain generator for Ger_PripyatMe410: heights + paint layers + a resolved layout, from the MapDesigner sketch v10.

    python Source/Tools/PripyatMap/pripyat_terrain_gen.py

Reads  MapDesigner/Pripyat/pripyat_01.layout.json (design source, +X north, +Y east, cm).
Writes Saved/Pripyat/terrain/
    Pripyat_height.raw          uint16 LE, 32768 + cm * 128 / 100, [iy, ix]
    Pripyat_layer_<Layer>.raw   uint8 weights, same grid, the layers sum to 255 per vertex
    Pripyat_layout.json         landscape grid, resolved regions / features / paths, road profiles, airfield transform
    Pripyat_masks.npz           masks on the vertex grid ([iy, ix]) for the dressing steps
    preview.png, preview_slope.png, check.txt

Height frame: the bog water table is z = 0 cm. Bog ground sits just above it, open water is carved below it.
Everything a ground unit may drive on (causeways, rises, the hill) stands at least 2 m above the bog behind banks of
about 58 degrees: all four nav agents stop at 44 degrees, so the bog, the flank scarps and the island are cut off
without any nav volume. Roads are cut / filled into the base terrain with the same 58 degree bank rule.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "MapDesigner"))
from mapdesigner.grid import Raster, Terrain  # noqa: E402
from mapdesigner.spec import Spec  # noqa: E402

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "Pripyat", "pripyat_01.layout.json")
OUT = os.path.join(ROOT, "Saved", "Pripyat", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_PripyatMe410"

COMPONENTS = 14                         # 882 m: the 670 x 700 m play area plus about 100 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1
SIZE = COMPONENTS * QUADS * QUAD_CM
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

LAYERS = ["PripPeat", "PripMoss", "PripMud", "PripGrass", "PripSand", "PripGravel", "PripEarth", "PripAsh", "PripAlgae"]

NAV_MAX_SLOPE_DEG = 44.0
BANK = math.tan(math.radians(60.0))     # cm of height per cm of run on every cut / fill bank
DRY = 260.0                             # causeway crown above the water table
BENCH = 750.0                           # the 5 m bench with the ring road
TOP = 1240.0                            # the 10 m ridge top
START_H = 450.0
ISLAND_H = 480.0
R_TOP = 12700.0                         # ridge top radius (sketch 122 m + 5 m so the airfield bays fit)
R_BENCH_IN = 14000.0
R_BENCH_OUT = 18900.0
R_FOOT = 21100.0

# The airfield kit is 200 x 30 m with bays (sketch: 173 x 20 m). Heading 45 as drawn; moved 6 m to the south-east of the
# ridge centre so the six bays (local -Y, north-west) stay on the top.
AIRFIELD = {"centre": [-424.0, 424.0], "yaw": 45.0, "z": TOP}
KHUTOR = (-180.0, 25700.0, 2500.0)                     # x, y, radius: a dry sand rise on the east lane
AIRFIELD_PAD = (-11400.0, 11400.0, -8300.0, 7200.0)      # local x0, x1, y0, y1 kept dead flat

ROADS = {
    # id: (half width cm, kind, crown height or None = profile)
    "rail_north": (700.0, "rail"), "road_east": (1750.0, "sand"), "causeway_south": (700.0, "logs"),
    "road_west": (700.0, "gravel"), "fuel_siding": (500.0, "rail"), "dam_far_radixite": (500.0, "dam"),
    "ring_road": (700.0, "gravel"), "ramp_n": (700.0, "gravel"), "ramp_s": (700.0, "gravel"), "ramp_w": (700.0, "gravel"),
    "boardwalk_pod": (125.0, "boardwalk"),
}
ROAD_ORDER = ["rail_north", "road_east", "causeway_south", "road_west", "fuel_siding", "dam_far_radixite", "ring_road",
              "ramp_n", "ramp_s", "ramp_w", "boardwalk_pod"]


def box_blur(a, r):
    r = int(round(r))
    if r <= 0:
        return a.astype(np.float64)
    out = a.astype(np.float64)
    for axis in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[axis] = (r + 1, r)
        p = np.pad(out, pad, mode="edge")
        c = np.cumsum(p, axis=axis)
        n = out.shape[axis]
        hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
        lo = np.take(c, np.arange(0, n), axis=axis)
        out = (hi - lo) / (2 * r + 1)
    return out


def soft(mask, r):
    return box_blur(box_blur(np.asarray(mask, dtype=np.float64), r), r)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(seed, wavelength_cells, shape=SHAPE):
    rng = np.random.default_rng(seed)
    n = max(2, int(math.ceil(max(shape) / wavelength_cells)) + 3)
    small = rng.random((n, n)).astype(np.float32)
    img = Image.fromarray(small, mode="F").resize((int(n * wavelength_cells), int(n * wavelength_cells)), Image.BICUBIC)
    big = np.asarray(img, dtype=np.float64)[: shape[0], : shape[1]]
    big = (big - big.mean()) / (big.std() + 1e-9)
    return np.clip(big / 2.2, -1.0, 1.0)


def fbm(seed, wavelength_cells, octaves=3):
    total, amp, norm = 0.0, 1.0, 0.0
    for k in range(octaves):
        total = total + amp * noise(seed + 17 * k, max(2.0, wavelength_cells / (2 ** k)))
        norm += amp
        amp *= 0.5
    return total / norm


def slope_deg(Z):
    gx = np.gradient(Z, axis=1) / QUAD_CM
    gy = np.gradient(Z, axis=0) / QUAD_CM
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def warp(mask, seed, amp_cells, wavelength):
    """The mask sampled at noise-displaced positions: straight sketch outlines turn ragged."""
    dr = np.rint(amp_cells * noise(seed, wavelength)).astype(int)
    dc = np.rint(amp_cells * noise(seed + 1, wavelength)).astype(int)
    rr = np.clip(np.arange(mask.shape[0])[:, None] + dr, 0, mask.shape[0] - 1)
    cc = np.clip(np.arange(mask.shape[1])[None, :] + dc, 0, mask.shape[1] - 1)
    return soft(mask[rr, cc], 2) > 0.5


def dist_outside(mask, limit):
    """Distance in cells from every cell to the mask (0 inside), octagonal metric, capped at limit."""
    d = np.where(mask, 0.0, float(limit))
    cur = mask.copy()
    for n in range(1, int(limit)):
        p = np.pad(cur, 1, mode="edge")
        if n % 2:
            nxt = p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:] | cur
        else:
            nxt = (p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:] | p[:-2, :-2] | p[:-2, 2:] | p[2:, :-2] |
                   p[2:, 2:] | cur)
        d[nxt & ~cur] = n * (1.0 if n % 2 else 1.0)
        cur = nxt
    return d


def tri_reach(Z, allowed_q, seed_rc, limit):
    """Flood fill over the landscape triangles (two per quad) whose slope is under the limit; neighbours share an
    edge. Returns the quads with a reached triangle."""
    a, b, c, d = Z[:-1, :-1], Z[:-1, 1:], Z[1:, :-1], Z[1:, 1:]
    w1 = (np.hypot(b - a, c - a) / QUAD_CM < limit) & allowed_q
    w2 = (np.hypot(d - c, d - b) / QUAD_CM < limit) & allowed_q
    r1 = np.zeros(w1.shape, dtype=bool)
    r2 = r1.copy()
    i, j = min(seed_rc[0], w1.shape[0] - 1), min(seed_rc[1], w1.shape[1] - 1)
    r1[i, j] = r2[i, j] = True
    count = 0
    while True:
        for _ in range(48):
            n1 = r1 | r2
            n1[1:, :] |= r2[:-1, :]
            n1[:, 1:] |= r2[:, :-1]
            n2 = r2 | r1
            n2[:-1, :] |= r1[1:, :]
            n2[:, :-1] |= r1[:, 1:]
            r1, r2 = n1 & w1, n2 & w2
        n = int(r1.sum() + r2.sum())
        if n == count:
            return r1 | r2
        count = n


def fillet(pts, rounds=3, cut=0.25):
    """Chaikin corner cutting; the two ends stay where they are."""
    pts = np.asarray(pts, float)
    for _ in range(rounds):
        out = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            out.append(a + (b - a) * cut)
            out.append(a + (b - a) * (1.0 - cut))
        out.append(pts[-1])
        pts = np.array(out)
    return pts


# ground pads under the encounter sets and the compound street: (x, y, radius cm) painted as worn earth
PADS = []


class Road:
    def __init__(self, rid, pts, half, kind):
        self.id, self.half, self.kind = rid, half, kind
        pts = np.asarray(pts, float)
        # resample every metre
        seg = np.hypot(*np.diff(pts, axis=0).T)
        cum = np.r_[0.0, np.cumsum(seg)]
        s = np.arange(0.0, cum[-1], 100.0)
        self.s = s
        self.p = np.c_[np.interp(s, cum, pts[:, 0]), np.interp(s, cum, pts[:, 1])]
        self.h = np.full(len(s), DRY)
        self.w = np.full(len(s), half)
        self.length = cum[-1]

    def nearest(self, X, Y, reach):
        """(distance, sample index) per cell; cells further than reach get distance 1e9."""
        lo, hi = self.p.min(axis=0) - reach, self.p.max(axis=0) + reach
        sel = (X >= lo[0]) & (X <= hi[0]) & (Y >= lo[1]) & (Y <= hi[1])
        d = np.full(X.shape, 1e9)
        k = np.zeros(X.shape, dtype=np.int32)
        xs, ys = X[sel], Y[sel]
        best = np.full(xs.shape, 1e9)
        arg = np.zeros(xs.shape, dtype=np.int32)
        step = 1
        idx = np.arange(0, len(self.p), step)
        for start in range(0, len(idx), 64):
            part = idx[start:start + 64]
            dd = np.hypot(xs[:, None] - self.p[part, 0][None, :], ys[:, None] - self.p[part, 1][None, :])
            j = dd.argmin(axis=1)
            m = dd[np.arange(len(xs)), j]
            better = m < best
            best[better] = m[better]
            arg[better] = part[j[better]]
        d[sel] = best
        k[sel] = arg
        return d, k


class Gen:
    def __init__(self):
        self.spec = Spec(SPEC_FILE)
        if self.spec.errors():
            raise RuntimeError("spec errors: %s" % [str(i.msg) for i in self.spec.errors()])
        raster = Raster(self.spec, QUAD_CM)
        raster.xmax = SIZE / 2.0 + QUAD_CM / 2.0
        raster.ymin = -SIZE / 2.0 - QUAD_CM / 2.0
        raster.rows, raster.cols = V, V
        self.raster = raster
        self.terrain = Terrain(self.spec, raster)
        self.masks = self.terrain.region_masks
        self.X = (SIZE / 2.0 - np.arange(V) * QUAD_CM)[:, None] * np.ones((1, V))
        self.Y = np.ones((V, 1)) * (-SIZE / 2.0 + np.arange(V) * QUAD_CM)[None, :]
        self.inside = (np.abs(self.X) <= self.spec.xmax) & (np.abs(self.Y) <= self.spec.ymax)
        self.R = np.hypot(self.X, self.Y)
        self.TH = np.arctan2(self.Y, self.X)
        self.Z = None
        self.W = {}
        self.out_masks = {}
        self.roads = {}

    def rc(self, p):
        r = int(round((SIZE / 2.0 - p[0]) / QUAD_CM))
        c = int(round((p[1] + SIZE / 2.0) / QUAD_CM))
        return min(max(r, 0), V - 1), min(max(c, 0), V - 1)

    def region(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for rid in ids:
            m |= self.masks[rid]
        return m

    def path(self, pid):
        return next(p for p in self.spec.paths if p.id == pid)

    def feature(self, fid):
        return next(f for f in self.spec.features if f.id == fid)

    @staticmethod
    def extended(pts, a_len=0.0, b_len=0.0):
        pts = [np.asarray(p, float) for p in pts]
        out = list(pts)
        if a_len:
            out.insert(0, pts[0] + (pts[0] - pts[1]) / np.linalg.norm(pts[0] - pts[1]) * a_len)
        if b_len:
            out.append(pts[-1] + (pts[-1] - pts[-2]) / np.linalg.norm(pts[-1] - pts[-2]) * b_len)
        return np.array(out)

    def line_dist(self, pts, reach=6000.0):
        road = Road("tmp", pts, 0.0, "tmp")
        d, _ = road.nearest(self.X, self.Y, reach)
        return d

    def airfield_local(self):
        """Cell coordinates in the airfield frame (x along the runway, y to its right = south-east)."""
        yaw = math.radians(AIRFIELD["yaw"])
        dx, dy = self.X - AIRFIELD["centre"][0], self.Y - AIRFIELD["centre"][1]
        return dx * math.cos(yaw) + dy * math.sin(yaw), -dx * math.sin(yaw) + dy * math.cos(yaw)

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        X, Y, R = self.X, self.Y, self.R
        # 1. the bog: soggy ground a hand above the water table, tussock relief, many small pools
        bog = 20.0 + 14.0 * fbm(11, 46) + 7.0 * noise(12, 5) + 4.0 * noise(13, 2.2)
        pools = fbm(21, 30, 3) + 0.35 * noise(22, 9)
        pool_w = smoothstep(-0.34, -0.52, pools)
        bog = bog * (1.0 - pool_w) + (-48.0 - 25.0 * noise(23, 12)) * pool_w
        # wetter towards the map edges and in broad drowned swales
        swale = smoothstep(0.25, 0.7, fbm(31, 110, 2))
        bog -= 26.0 * swale
        wet = bog < 0.0
        core = wet.copy()
        for _ in range(2):                              # erode twice, dilate three times: pools under about 5 m across go
            q = np.pad(core, 1, mode="edge")
            core = q[:-2, 1:-1] & q[2:, 1:-1] & q[1:-1, :-2] & q[1:-1, 2:] & core
        for _ in range(3):
            q = np.pad(core, 1, mode="edge")
            core = q[:-2, 1:-1] | q[2:, 1:-1] | q[1:-1, :-2] | q[1:-1, 2:] | core
        bog = np.where(wet & ~core, 10.0 + 6.0 * noise(24, 4), bog)
        Z = bog
        self.pool_w = pool_w

        # 2. open water from the sketch: lakes, pools, oxbows, flooded strips, the two rivers
        water = np.zeros(SHAPE, dtype=bool)
        for reg in self.spec.regions:
            if reg.kind == "water":
                m = self.masks[reg.id]
                if reg.id.startswith("flood_"):
                    # the flooded strips beside the lane entries: ragged drowned ground, not basins
                    m = warp(m, 400 + len(reg.id), 9.0, 26) & (fbm(47, 22) > -0.25)
                else:
                    m = warp(m, 420 + len(reg.id), 6.0, 30)
                water |= m
        # the island lake drains into the north backwater through a reedy arm
        arm = self.line_dist(np.array([[24500.0, -17500.0], [26200.0, -15200.0], [27500.0, -13500.0]]), 3000.0)
        water |= arm + 250.0 * noise(48, 12) < 420.0
        river_d = np.full(SHAPE, 1e9)
        for pid in ("algae_river_n", "algae_river_s"):
            pts = self.extended(self.path(pid).pts, 0.0, 16000.0)
            river_d = np.minimum(river_d, self.line_dist(pts, 5000.0))
        self.river_d = river_d
        # the backwaters wander (up to 6 m either way), but hold their line at the three crossings
        calm = np.ones(SHAPE)
        for cid in ("bridge_n", "bridge_s", "ford_e"):
            c = np.asarray(self.spec.by_id[cid].points[0], float)
            calm = np.minimum(calm, smoothstep(2500.0, 7000.0, np.hypot(X - c[0], Y - c[1])))
        wobble = 160.0 * noise(41, 14) + calm * (1500.0 * noise(44, 85) + 420.0 * noise(45, 30))
        river = river_d + wobble < 800.0 + calm * (520.0 * noise(46, 40) - 150.0)
        # two cut-off oxbows beside the wandering reaches
        for (ox, oy, rad, seed) in ((23500.0, -9000.0, 2100.0, 71), (-24000.0, -11500.0, 1900.0, 72)):
            ring_d = np.abs(np.hypot(X - ox, Y - oy) - rad) + 200.0 * noise(seed, 10)
            water |= (ring_d < 380.0) & (np.arctan2(Y - oy, X - ox) > -1.9) & (np.arctan2(Y - oy, X - ox) < 1.6)
        water |= river
        # the ford: the gravel bar dips under the water, so deep water has to flank it on both sides
        ford = np.asarray(self.spec.by_id["ford_e"].points[0], float)
        water |= np.hypot(X - ford[0], Y - ford[1]) < 5200.0
        water = soft(water, 2) > 0.5
        wsoft = soft(water, 3)
        deep = -165.0 - 45.0 * smoothstep(0.6, 1.0, soft(water, 7)) + 14.0 * noise(42, 10)
        Z = Z * (1.0 - wsoft) + deep * wsoft
        reeds = self.region("reed_pond_n_reeds", "reed_pond_w_reeds", "reed_pond_s_reeds") & ~water
        Z = np.where(reeds, np.minimum(Z, -14.0 + 8.0 * noise(43, 4)), Z)
        self.water_sketch = water
        self.reeds = reeds

        # 3. smouldering peat: cracked, slumped, ash-grey humps
        burn = warp(self.region("burning_peat_se", "burning_peat_nw"), 500, 9.0, 18) & ~water
        bsoft = soft(burn, 3)
        core = smoothstep(0.55, 0.95, soft(burn, 6))
        pits = smoothstep(0.25, 0.6, noise(52, 7))
        Z = Z + bsoft * (30.0 + 22.0 * np.abs(noise(51, 5))) - core * (55.0 + 45.0 * pits)
        self.burn = bsoft
        self.burn_core = core

        # 4. the central hill: talus, lower scarp, bench, upper scarp, flat top. The scarp lines wander a little.
        wob = 130.0 * noise(61, 34) + 60.0 * noise(62, 11)
        r = R + wob
        # the bench edge swings 15 m in and out in a few broad lobes and bites back in three gullies; the foot swings more
        lobes = 1500.0 * noise(66, 150) + 500.0 * noise(67, 55)
        gully = np.zeros(SHAPE)
        for bearing, width in ((38.0, 9.0), (196.0, 8.0), (288.0, 10.0)):
            dth = np.abs((np.degrees(self.TH) - bearing + 180.0) % 360.0 - 180.0)
            gully = np.maximum(gully, smoothstep(width, 0.0, dth))
        edge = np.clip(lobes - 1700.0 * gully, -1700.0, 1700.0)            # shift of the bench's outer edge
        r_low = r - edge
        hill = np.full(SHAPE, -1e9)
        r_foot = r_low + 1.6 * wob + 900.0 * noise(64, 90) + 500.0 * noise(65, 40)
        talus_low = 30.0 + (330.0 - 30.0) * smoothstep(R_FOOT + 300.0, R_BENCH_OUT + 330.0, r_foot) ** 1.3
        scarp_low = 330.0 + (BENCH - 330.0) * np.clip((R_BENCH_OUT + 330.0 - r_low) / 300.0, 0.0, 1.0)
        low = np.where(r_low > R_BENCH_OUT + 330.0, talus_low, scarp_low)
        talus_up = BENCH + (880.0 - BENCH) * smoothstep(R_BENCH_IN, R_TOP + 260.0, r) ** 1.5
        scarp_up = 880.0 + (TOP - 880.0) * np.clip((R_TOP + 260.0 - r) / 260.0, 0.0, 1.0)
        up = np.where(r > R_TOP + 260.0, talus_up, scarp_up)
        hill = np.where(r > R_BENCH_IN, low, up)
        hill = np.where((r_low <= R_BENCH_OUT) & (r > R_BENCH_IN), BENCH + 9.0 * noise(63, 18), hill)
        self.r_low = r_low
        on_hill = np.minimum(r, r_foot) < R_FOOT + 300.0
        Z = np.where(on_hill, np.maximum(Z, hill), Z)
        self.hill_r = r

        # 5. dry rises: flat tops, steep peat-hag sides
        def rise(mask, height, dome=12.0, seed=70):
            """Flat top, 60 degree sides, and a moat so the foot is always 2.6 m under the top (a bank lower than
            2.4 m can be walked down obliquely on a 1 m grid)."""
            nonlocal Z
            # sub-cell distance past the rim from the blurred mask: the bank follows the blob, not the grid cells
            blur = soft(mask, 2)
            near_d = np.clip((0.5 - blur) / 0.2, 0.0, 2.5)
            far_d = dist_outside(mask, 14)
            d = np.where(far_d > 2.5, far_d, near_d) * QUAD_CM
            inside_m = blur >= 0.5
            top = height + dome * noise(seed, 16) * smoothstep(0.0, 1.0, soft(mask, 3) * 2.0 - 1.0)
            side = height - BANK * d
            Z = np.maximum(Z, np.where(inside_m, top, side))
            moat = ~inside_m & (soft(mask, 3) > 0.07) & (Z < height - 150.0)
            Z = np.where(moat, np.minimum(Z, np.maximum(side, height - 262.0)), Z)
            Z = np.where(~inside_m & (d <= 400.0) & (Z >= height - 150.0), np.maximum(Z, np.minimum(height, Z + 150.0 - d * 0.4)), Z)

        for k, rid in enumerate(("rise_aa_w", "rise_aa_n", "rise_aa_e", "rise_aa_s")):
            self.masks[rid] = warp(self.masks[rid], 600 + 7 * k, 4.0, 14)
            rise(self.masks[rid], BENCH, 6.0, 71)
        self.masks["island"] = warp(self.masks["island"], 640, 7.0, 20)
        for rid in ("rise_pilot_n", "rise_pilot_s", "rise_fuel", "rise_far_radixite", "pod_hummock", "hummock_bog_pilot_a", "hummock_bog_pilot_b"):
            self.masks[rid] = warp(self.masks[rid], 660 + len(rid), 3.0, 12)
        rise(self.masks["start_rise"], START_H, 14.0, 72)
        khutor = warp(np.hypot(X - KHUTOR[0], Y - KHUTOR[1]) < KHUTOR[2], 690, 4.0, 15)
        self.masks["rise_khutor"] = khutor
        rise(khutor, DRY + 35.0, 10.0, 81)
        rise(self.masks["rise_pilot_n"], DRY + 40.0, 10.0, 73)
        rise(self.masks["rise_pilot_s"], DRY + 40.0, 10.0, 74)
        rise(self.masks["rise_fuel"], DRY + 30.0, 8.0, 75)
        rise(self.masks["rise_far_radixite"], DRY + 40.0, 12.0, 76)
        rise(self.masks["pod_hummock"], 215.0, 10.0, 77)
        rise(self.masks["island"], ISLAND_H, 10.0, 78)
        rise(self.masks["hummock_bog_pilot_a"], 170.0, 14.0, 79)
        rise(self.masks["hummock_bog_pilot_b"], 170.0, 14.0, 80)
        self.rises = khutor | self.region("start_rise", "rise_pilot_n", "rise_pilot_s", "rise_fuel", "rise_far_radixite",
                                 "pod_hummock", "island", "hummock_bog_pilot_a", "hummock_bog_pilot_b")
        self.aa_rises = self.region("rise_aa_w", "rise_aa_n", "rise_aa_e", "rise_aa_s")

        # 6. ridge top: dead flat under the airfield, a breath of relief elsewhere
        lx, ly = self.airfield_local()
        pad = (lx > AIRFIELD_PAD[0]) & (lx < AIRFIELD_PAD[1]) & (ly > AIRFIELD_PAD[2]) & (ly < AIRFIELD_PAD[3])
        top = (r < R_TOP - 100.0)
        relief = 7.0 * noise(81, 22) * (1.0 - soft(pad, 5))
        Z = np.where(top, TOP + relief, Z)
        self.pad = pad
        self.top = top

        # 7. roads: every corridor is cut / filled into the terrain with 60 degree banks
        self.rise_join = dist_outside(self.rises, 4) <= 1.0          # the rises and one cell round them
        self.ditch_ok = (self.r_low > R_BENCH_OUT + 120.0) & ~self.rise_join
        self.road_d = np.full(SHAPE, 1e9)          # distance past the road edge (<= 0 on the road)
        self.road_kind = np.full(SHAPE, "", dtype=object)
        self.road_centre = np.full(SHAPE, 1e9)
        self.ditches = []
        for rid in ROAD_ORDER:
            Z = self.lay_road(rid, Z)
        # side ditches: off the hill and away from the rises nothing may meet a causeway at grade (the lower talus and
        # the bog would otherwise join the road where their heights cross). Never across another road.
        off_roads = self.road_d > 0.0
        for near, run, lo, surf in self.ditches:
            ditch = near & (run > 0.0) & (run < 330.0 + 300.0 * np.clip(noise(49, 13) + 0.4, 0.0, 1.0) + 180.0 * np.clip(noise(50, 5), 0.0, 1.0)) & self.ditch_ok & off_roads
            Z = np.where(ditch, np.minimum(Z, np.maximum(lo, surf - 262.0)), Z)

        # waterline: everything at or below +45 cm (bog, moat floors, pool rims) is smoothed among itself, so the hard
        # outer edge of a moat or ditch becomes a 3-4 m slope and the waterline follows that slope instead of a mask
        # edge. Cells above +45 take no part, so every blocking bank keeps its height and angle; the first 2.3 m of a
        # road ditch are left alone.
        low = (Z <= 45.0)
        num = soft(np.where(low, Z, 0.0), 2)
        den = soft(low, 2)
        smooth_low = np.where(den > 1e-3, num / np.maximum(den, 1e-3), Z)
        Z = np.where(low & (self.road_d > 230.0), smooth_low + 5.0 * noise(57, 6), Z)
        ford = (self.road_kind == "sand") & (Z < 12.0)
        shoal = smoothstep(-0.1, 0.45, noise(55, 7) + 0.5 * noise(56, 3))
        Z = np.where(ford, Z + 24.0 * shoal, Z)
        self.ford = ford
        # 8. past the play edge the bog simply carries on; keep the outermost ring of vertices low
        self.Z = Z
        self.slope = slope_deg(Z)

    def profile(self, road):
        """Crown height and half width along a corridor."""
        rid, p, s = road.id, road.p, road.s
        r = np.hypot(p[:, 0], p[:, 1])
        climb = smoothstep(R_FOOT + 500.0, R_BENCH_OUT - 250.0, r)
        if rid == "ring_road":
            road.h[:] = BENCH
        elif rid in ("ramp_n", "ramp_s", "ramp_w"):
            t = np.clip((s - 600.0) / max(road.length - 1500.0, 1.0), 0.0, 1.0)
            road.h[:] = BENCH + (TOP - BENCH) * t
        elif rid == "rail_north":
            road.h[:] = 300.0 + (BENCH - 300.0) * climb
        elif rid == "causeway_south":
            road.h[:] = 215.0 + (BENCH - 215.0) * climb
        elif rid == "road_east":
            road.h[:] = 285.0 + (BENCH - 285.0) * climb
        elif rid == "road_west":
            start = np.asarray(self.spec.by_id["start_rise"].geom.centre, float)
            ds = np.hypot(p[:, 0] - start[0], p[:, 1] - start[1])
            base = DRY + (START_H - DRY) * smoothstep(6500.0, 3300.0, ds)
            road.h[:] = base + (BENCH - base) * climb
            cp = np.asarray(self.spec.by_id["checkpoint"].points[0], float)
            k = int(np.hypot(p[:, 0] - cp[0], p[:, 1] - cp[1]).argmin())
            pad = smoothstep(1900.0, 1100.0, np.abs(s - s[k] - 600.0))
            road.w[:] = road.w + 600.0 * pad                         # a 26 m wide checkpoint pad
        elif rid == "fuel_siding":
            road.h[:] = 290.0
        elif rid == "dam_far_radixite":
            road.h[:] = 235.0
        elif rid == "boardwalk_pod":
            road.h[:] = 205.0
        # chokes and crossings from the sketch
        path = self.path(rid) if rid in self.spec.by_id else None
        raw = path.raw if path is not None else {}
        for ch in raw.get("chokes", []):
            at = np.asarray(ch["at"], float)
            k = int(np.hypot(p[:, 0] - at[0], p[:, 1] - at[1]).argmin())
            along = np.abs(s - s[k])
            inside = smoothstep(ch["length"] / 2.0 + 500.0, ch["length"] / 2.0 - 200.0, along)
            road.w[:] = road.w - (road.w - ch["width"] / 2.0) * inside
            road.h[:] = road.h - 55.0 * inside            # the slumped bank / the sunken logs sit lower
        for cr in raw.get("crossings", []):
            at = np.asarray(cr["at"], float)
            k = int(np.hypot(p[:, 0] - at[0], p[:, 1] - at[1]).argmin())
            along = np.abs(s - s[k])
            if cr["kind"] == "ford":
                inside = smoothstep(4200.0, 1000.0, along)
                road.w[:] = road.w - (road.w - cr["width"] / 2.0) * inside
                road.h[:] = road.h + (-13.0 - road.h) * inside       # a hand of water over a gravel bar
            else:
                inside = smoothstep(1500.0 + 500.0, 1500.0 - 200.0, along)
                road.w[:] = road.w - (road.w - cr["width"] / 2.0) * inside
        # smooth the profile so no step is left where two rules meet
        kernel = np.ones(9) / 9.0
        road.h[:] = np.convolve(np.pad(road.h, 4, mode="edge"), kernel, mode="valid")

    def lay_road(self, rid, Z):
        half, kind = ROADS[rid]
        pts = self.path(rid).pts
        if rid in ("rail_north", "road_east", "causeway_south"):
            pts = self.extended(pts, 9000.0, 0.0)        # the lanes run on to the landscape edge
        if rid == "road_west":
            # the forestry causeway comes in from the west edge: the player arrives from somewhere
            pts = np.vstack([[[-14500.0, -45000.0], [-13600.0, -38500.0], [-12600.0, -34300.0]], pts])
        if rid in ("road_west", "rail_north", "causeway_south", "fuel_siding"):
            pts = fillet(pts, 3, 0.22)
        road = Road(rid, pts, half, kind)
        self.profile(road)
        if rid in ("boardwalk_pod", "fuel_siding", "dam_far_radixite"):
            # a spur starts at the height of the lane it leaves
            z0 = Z[self.rc(road.p[0])]
            road.h[:] = road.h + (z0 - road.h) * smoothstep(3200.0, 400.0, road.s)
        self.roads[rid] = road
        d, k = road.nearest(self.X, self.Y, 2600.0)
        near = d < 1e8
        h = np.where(near, road.h[k], 0.0)
        w = np.where(near, road.w[k], 0.0)
        e = np.maximum(d - w, 0.0)                       # run past the road edge
        if kind == "sand":
            crown = -18.0 * (np.clip(d / np.maximum(w, 1.0), 0.0, 1.0) ** 2)      # a low sand ridge
        elif kind == "rail":
            crown = 0.0
        else:
            crown = -8.0 * (np.clip(d / np.maximum(w, 1.0), 0.0, 1.0) ** 2)
        surf = h + crown
        run = e                                          # no level verge: a verge lets units walk down a bank obliquely
        lo, hi = surf - BANK * run, surf + BANK * run
        Zn = np.where(near, np.clip(Z, lo, hi), Z)
        # where a corridor meets a rise the rise eases down to it (17 degrees) instead of standing as a step
        ease = near & self.rise_join
        Zn = np.where(ease, np.clip(Zn, surf - 0.3 * run, surf + 0.3 * run), Zn)
        self.ditches.append((near, run, lo, surf))
        on = near & (e <= 0.0)
        self.road_d = np.where(near, np.minimum(self.road_d, d - w), self.road_d)
        self.road_kind = np.where(on, kind, self.road_kind)
        self.road_centre = np.where(on, np.minimum(self.road_centre, d), self.road_centre)
        self.out_masks["road_" + rid] = on
        return Zn

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        Z, slope = self.Z, self.slope
        n1, n2, n3 = fbm(101, 28), noise(102, 7), fbm(103, 70, 2)
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        dry_ground = Z > 150.0
        wet = smoothstep(26.0, -6.0, Z)                        # at and under the water table
        # bog: peat with moss lawns, mud at the pools
        moss = smoothstep(-0.15, 0.35, n1 + 0.4 * n2) * smoothstep(8.0, 24.0, Z)
        W["PripPeat"] += 1.0
        W["PripMoss"] += 1.5 * moss
        W["PripMud"] += 3.0 * wet
        # purple algae: a stained band round the sketch's algae water and along the rivers, and on the river beds
        shore = soft(self.water_sketch | (Z < -30.0), 4)
        east_shore = np.clip(0.45 - 14.0 * np.gradient(shore, axis=1), 0.0, 1.0)      # shore falls towards the east = lee shore
        filaments = smoothstep(0.05, 0.5, noise(104, 5) + 0.6 * noise(105, 2.2) + 0.5 * n1)
        band = np.clip(1.0 - np.abs(shore - 0.45) / 0.33, 0.0, 1.0) * smoothstep(-80.0, -20.0, Z) * smoothstep(16.0, 4.0, Z)
        algae = np.clip(band * east_shore * filaments * 1.6, 0.0, 1.0)
        W["PripAlgae"] += 6.0 * algae
        # burning peat: ash and char
        # burning peat: black charred core with white-ash veins, a scorched earth rim, then the brown bog
        veins = smoothstep(0.3, 0.55, np.abs(noise(106, 4)) + 0.4 * np.abs(noise(107, 9)))
        W["PripMud"] += 9.0 * self.burn_core * (1.0 - veins)
        W["PripAsh"] += 10.0 * self.burn_core * veins + 2.5 * self.burn * (1.0 - self.burn_core) * np.clip(n2, 0.0, 1.0)
        rim = np.clip(self.burn * (1.0 - self.burn_core) * 2.0, 0.0, 1.0)
        W["PripEarth"] += 7.0 * rim * (0.6 + 0.4 * n1)
        # dry ground: pale dead grass with bare earth; scarps and banks are raw earth (sand on the east lane)
        grass = np.clip(0.75 + 0.5 * n1, 0.0, 1.0)
        bare = np.clip(0.35 - 0.6 * n1 + 0.3 * n2, 0.0, 1.0)
        dry_w = smoothstep(120.0, 190.0, Z)
        for name in ("PripPeat", "PripMoss", "PripMud", "PripAlgae"):
            W[name] *= (1.0 - dry_w)
        W["PripGrass"] += 4.0 * dry_w * grass
        W["PripEarth"] += 4.0 * dry_w * bare
        steep = smoothstep(18.0, 34.0, slope)
        east = smoothstep(14000.0, 21000.0, self.Y) * (np.abs(self.X) < 14000.0)
        patch = smoothstep(-0.35, 0.35, noise(108, 9) + 0.5 * n1)
        W["PripEarth"] += 14.0 * steep * (1.0 - 0.8 * east) * (0.35 + 0.65 * patch)
        W["PripSand"] += 14.0 * steep * (0.8 * east + 0.25 * (1.0 - patch))
        W["PripGrass"] += 5.0 * steep * (1.0 - patch) * (Z > 300.0)
        W["PripPeat"] += 6.0 * steep * (1.0 - patch) * (Z <= 300.0)
        # the bog foot of every bank: mud
        # roads
        d = self.road_d
        kind = self.road_kind
        on = d <= 0.0
        verge = smoothstep(260.0, 0.0, d) * (d > 0.0)
        W["PripEarth"] += 5.0 * verge
        centre = self.road_centre + 90.0 * noise(109, 3) + 60.0 * n2
        for k, layer, width, mix in (("gravel", "PripGravel", 520.0, "PripEarth"), ("rail", "PripGravel", 330.0, "PripEarth"),
                                     ("logs", "PripMud", 700.0, "PripEarth"), ("dam", "PripEarth", 500.0, "PripPeat"),
                                     ("boardwalk", "PripEarth", 225.0, "PripPeat"), ("sand", "PripSand", 1750.0, "PripSand")):
            m = on & (kind == k)
            core = m * smoothstep(width, width - 260.0, centre) * (0.55 + 0.45 * np.clip(0.5 + n2 + 0.5 * n1, 0.0, 1.0))
            W[layer] += 30.0 * core
            W[mix] += 12.0 * m * (1.0 - smoothstep(width, width - 180.0, centre))
        sand_road = on & (kind == "sand")
        W["PripGravel"] += 110.0 * sand_road * smoothstep(560.0, 380.0, centre) * (0.75 + 0.25 * n2)
        # sand ridge shoulders: the east lane's ridge is sand well past the road
        ridge_e = smoothstep(900.0 + 500.0 * n1, 0.0, d) * (soft(sand_road, 12) > 0.02) * dry_w
        W["PripSand"] += 10.0 * ridge_e
        W["PripGrass"] += 22.0 * sand_road * smoothstep(0.1, 0.6, noise(110, 8) + 0.5 * n1) * smoothstep(500.0, 800.0, centre)
        # ford: gravel bar under the water
        ford = self.ford
        W["PripGravel"] += 40.0 * ford * (Z < 0.0)
        W["PripSand"] += 60.0 * ford * (Z >= 0.0)
        # ridge top: worn earth around the compound, grass elsewhere; the airfield verges are earth and gravel
        lx, ly = self.airfield_local()
        compound = np.hypot(self.X + 1800.0, self.Y - 1800.0) < 6900.0
        W["PripEarth"] += 5.0 * self.top * soft(compound, 10) * (0.6 + 0.6 * n2)
        strip = (np.abs(lx) < 10300.0) & (ly > -7900.0) & (ly < 1900.0)
        W["PripGravel"] += 6.0 * self.top * soft(strip, 3) * (0.5 + 0.5 * n3)
        W["PripEarth"] += 5.0 * self.top * soft(strip, 6)
        # the ramp heads carry on as worn dirt tracks to the airfield apron
        for rid, (tx, ty) in (("ramp_w", (-6400.0, -4400.0)), ("ramp_n", (5600.0, 6300.0)), ("ramp_s", (-7600.0, -5000.0))):
            head = self.roads[rid].p[-1]
            dtrack = self.line_dist(np.array([head, [(head[0] + tx) / 2.0 + 300.0, (head[1] + ty) / 2.0 - 300.0], [tx, ty]]), 2500.0)
            track = smoothstep(330.0, 120.0, dtrack + 70.0 * n2)
            W["PripEarth"] += 14.0 * track
            W["PripGravel"] += 4.0 * track * np.clip(n2, 0.0, 1.0)
        # worn ground under the encounter sets and along the compound street
        for px, py, pr in PADS:
            dpad = np.hypot(self.X - px, self.Y - py) + 250.0 * n2 + 150.0 * n1
            W["PripEarth"] += 16.0 * smoothstep(pr, pr * 0.55, dpad) * (Z > 150.0)
        lx, ly = self.airfield_local()
        street = smoothstep(620.0, 380.0, np.abs(ly - 7750.0) + 80.0 * n2) * (lx > -9800.0) * (lx < 7600.0) * self.top
        W["PripEarth"] += 16.0 * street
        W["PripGravel"] += 5.0 * street * np.clip(n2, 0.0, 1.0)
        # island: a pale packed-sand strip, worn earth round it
        isl = self.feature("island_airfield")
        a, b = np.asarray(isl.raw["ends"][0], float), np.asarray(isl.raw["ends"][1], float)
        dstrip = self.line_dist(np.array([a, b]), 3000.0)
        W["PripSand"] += 40.0 * smoothstep(820.0, 560.0, dstrip + 110.0 * n2 + 60.0 * n1)
        W["PripEarth"] += 12.0 * smoothstep(1500.0, 700.0, dstrip) * self.masks["island"]
        self.island_strip = dstrip < 750.0

        total = sum(W.values())
        out = {}
        acc = np.zeros(SHAPE)
        for name in LAYERS:
            out[name] = np.round(W[name] / total * 255.0)
            acc += out[name]
        # fix rounding on the strongest layer
        stack = np.stack([out[n] for n in LAYERS])
        top = stack.argmax(axis=0)
        fix = 255.0 - acc
        for i, name in enumerate(LAYERS):
            out[name] = np.clip(out[name] + np.where(top == i, fix, 0.0), 0, 255).astype(np.uint8)
        self.W = out

        m = self.out_masks
        m["bog"] = (Z < 120.0)
        m["water"] = (Z < 0.0)
        m["open_water"] = self.water_sketch
        m["river"] = self.river_d < 800.0
        m["reeds"] = self.reeds
        m["burn"] = self.burn > 0.4
        m["dry"] = dry_ground
        m["road"] = on
        m["road_near"] = d < 250.0
        m["scarp"] = slope > 30.0
        m["top"] = self.top
        m["pad"] = self.pad
        m["bench"] = (self.r_low <= R_BENCH_OUT) & (self.hill_r > R_BENCH_IN) & ~on
        m["rises"] = self.rises
        m["khutor"] = self.masks["rise_khutor"]
        m["aa_rises"] = self.aa_rises
        m["island"] = self.masks["island"]
        m["island_strip"] = self.island_strip
        m["inside"] = self.inside
        m["algae"] = algae > 0.4
        m["moss"] = moss > 0.5
        m["bank_toe"] = (slope > 28.0) & (Z > -70.0) & (Z < 70.0)
        m["bank_face"] = (slope > 24.0) & (Z >= 70.0) & (Z < 520.0) & (self.r_low > R_BENCH_OUT + 500.0) & (d > 40.0)
        m["road_dist"] = np.clip(d, -2000.0, 6000.0).astype(np.float32)
        m["hill_r"] = self.hill_r.astype(np.float32)
        m["slope"] = slope.astype(np.float32)
        m["height"] = Z.astype(np.float32)

    # ---- checks -------------------------------------------------------------------------------------------------
    def check(self):
        lines = []
        Z, slope = self.Z, self.slope
        start = self.rc(self.spec.by_id["start_rise"].geom.centre)
        limit = math.tan(math.radians(NAV_MAX_SLOPE_DEG))
        inside_q = self.inside[:-1, :-1]
        loose = np.zeros(SHAPE, dtype=bool)            # at the nav limit: must NOT reach the bog
        loose[:-1, :-1] = tri_reach(Z, inside_q, start, limit)
        strict = np.zeros(SHAPE, dtype=bool)           # with a margin: must reach every dry place
        strict[:-1, :-1] = tri_reach(Z, inside_q, start, limit * 0.85)
        reach = loose
        self.reach_full = reach
        self.reach_strict = strict

        def ok(name, p, want=True):
            got = bool((strict if want else loose)[self.rc(p)])
            lines.append("%-5s reach %-22s %s (z %.0f)" % ("ok" if got == want else "FAIL", name, got, Z[self.rc(p)]))

        ok("ring N", (15408, 0))
        ok("ridge top / hq", (-1800, 1800))
        for rid in ("rise_aa_w", "rise_aa_n", "rise_aa_e", "rise_aa_s", "rise_pilot_n", "rise_pilot_s", "rise_fuel",
                    "rise_far_radixite", "pod_hummock"):
            ok(rid, self.spec.by_id[rid].geom.centre)
        ok("N entry", (33300, 7020))
        ok("E entry", (4050, 34700))
        ok("S entry", (-33300, 8010))
        ok("island", self.spec.by_id["island"].geom.centre, False)
        ok("bog pilot a", self.spec.by_id["hummock_bog_pilot_a"].geom.centre, False)
        ok("bog pilot b", self.spec.by_id["hummock_bog_pilot_b"].geom.centre, False)
        for name, p in (("bog NE", (20000, 20000)), ("bog SW", (-20000, -12000)), ("bog W", (5000, -24000)),
                        ("bog SE", (-22000, 22000)), ("lower talus N", (20300, -6000))):
            ok(name, p, False)
        leak = reach & (Z < 150.0) & ~(self.road_d <= 120.0)
        lines.append("%-5s reached bog cells off the roads: %d" % ("ok" if leak.sum() == 0 else "FAIL", leak.sum()))
        if leak.sum():
            rr, cc = np.nonzero(leak)
            for i in range(0, len(rr), max(1, len(rr) // 12)):
                lines.append("      leak at x %.0f y %.0f z %.0f" % (self.X[rr[i], cc[i]], self.Y[rr[i], cc[i]], Z[rr[i], cc[i]]))
        for rid, road in self.roads.items():
            g = np.abs(np.gradient(road.h, 100.0)) * 100.0
            lines.append("road %-18s len %4.0f m  h %4.0f..%4.0f  max grade %.1f %%" % (rid, road.length / 100.0, road.h.min(), road.h.max(), g.max()))
        ins = self.inside
        lines.append("share of play area: water %.1f %%, bog %.1f %%, reachable %.1f %%" % (
            100.0 * ((Z < 0) & ins).sum() / ins.sum(), 100.0 * ((Z < 120) & ins).sum() / ins.sum(), 100.0 * (reach & ins).sum() / ins.sum()))
        text = "\n".join(lines)
        with open(os.path.join(OUT, "check.txt"), "w") as handle:
            handle.write(text)
        print(text)

    # ---- output -------------------------------------------------------------------------------------------------
    @staticmethod
    def to_grid(a):
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        files = {"height": os.path.join(OUT, "Pripyat_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "Pripyat_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        masks_file = os.path.join(OUT, "Pripyat_masks.npz").replace("\\", "/")
        np.savez_compressed(masks_file, **{k: self.to_grid(np.asarray(v)) for k, v in self.out_masks.items()})
        files["masks"] = masks_file
        spec = self.spec
        regions = []
        for reg in spec.regions:
            regions.append({"id": reg.id, "kind": reg.kind, "label": reg.label, "shape": reg.geom.kind,
                            "width": reg.geom.width, "centre": [float(v) for v in reg.geom.centre],
                            "points": [[float(a), float(b)] for a, b in reg.geom.pts], "raw": reg.raw})
        features = []
        for f in spec.features:
            features.append({"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                             "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw})
        paths = [{"id": p.id, "kind": p.kind, "label": p.label, "width": p.width,
                  "points": [[float(a), float(b)] for a, b in p.pts], "raw": p.raw} for p in spec.paths]
        roads = {rid: {"kind": r.kind, "length": r.length,
                       "points": [[float(a), float(b), float(h), float(w)] for (a, b), h, w in
                                  zip(r.p[::3], r.h[::3], r.w[::3])]} for rid, r in self.roads.items()}
        layout = {
            "map": MAP_PATH, "spec": SPEC_FILE.replace("\\", "/"),
            "landscape": {"components_x": COMPONENTS, "components_y": COMPONENTS, "quads_per_component": QUADS,
                          "quad_cm": QUAD_CM, "verts_x": V, "verts_y": V, "size_x": SIZE, "size_y": SIZE,
                          "corner": [-SIZE / 2.0, -SIZE / 2.0]},
            "bounds": {"xmin": spec.xmin, "xmax": spec.xmax, "ymin": spec.ymin, "ymax": spec.ymax},
            "z_range": [float(self.Z.min()), float(self.Z.max())],
            "levels": {"water": 0.0, "dry": DRY, "bench": BENCH, "top": TOP, "start": START_H, "island": ISLAND_H},
            "airfield": AIRFIELD, "khutor": list(KHUTOR),
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths, "roads": roads,
        }
        with open(os.path.join(OUT, "Pripyat_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        self.preview()
        z = self.Z[self.inside]
        print("heights cm inside: min %.0f max %.0f" % (z.min(), z.max()))
        for name in LAYERS:
            print("layer %-12s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"PripPeat": (72, 60, 46), "PripMoss": (96, 110, 60), "PripMud": (44, 40, 38), "PripGrass": (170, 156, 104),
                   "PripSand": (206, 192, 150), "PripGravel": (150, 146, 138), "PripEarth": (120, 96, 72),
                   "PripAsh": (170, 170, 174), "PripAlgae": (120, 70, 150)}
        rgb = np.zeros((V, V, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        water = self.Z < 0.0
        rgb[water] = rgb[water] * 0.35 + np.array((70, 50, 110)) * 0.65
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.85 + 0.9 * (gx + gy) / 1.4, 0.3, 1.5)
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        r0, c0 = self.rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.rc([self.spec.xmin, self.spec.ymax])
        for r in (r0, r1):
            rgb[r, c0:c1] = (255, 220, 0)
        for c in (c0, c1):
            rgb[r0:r1, c] = (255, 220, 0)
        Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(OUT, "preview.png"))
        img = np.zeros((V, V, 3), dtype=np.uint8)
        img[..., 1] = np.clip((self.Z + 200.0) / 1500.0 * 255, 0, 255)
        img[..., 2] = np.where(self.reach_full, 160, 0)
        img[..., 0] = np.clip(self.slope * 4, 0, 255)
        img[self.slope > NAV_MAX_SLOPE_DEG] = (255, 0, 255)
        Image.fromarray(img).save(os.path.join(OUT, "preview_slope.png"))


def main():
    import math as _m
    for bearing, rr, pr in ((80.0, 17500.0, 1500.0), (112.0, 17500.0, 1200.0), (236.0, 17500.0, 1300.0), (47.0, 17650.0, 1700.0),
                            (200.0, 17650.0, 1500.0), (312.0, 17650.0, 1600.0)):
        PADS.append((rr * _m.cos(_m.radians(bearing)), rr * _m.sin(_m.radians(bearing)), pr))
    PADS.append((KHUTOR[0], KHUTOR[1], KHUTOR[2] * 0.8))
    gen = Gen()
    gen.build_heights()
    gen.build_layers()
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
