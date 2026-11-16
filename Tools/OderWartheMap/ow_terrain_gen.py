"""Terrain generator for Ger_OderWarthe: heights + paint layers + a resolved layout, from MapDesigner sketch C.

    python Source/Tools/OderWartheMap/ow_terrain_gen.py

Reads  MapDesigner/OderWarthe/df2_c.layout.json (design source, +X north, +Y east, cm).
Writes Saved/OderWarthe/terrain/
    OW_height.raw            uint16 LE, 32768 + cm * 128 / 100, [iy, ix]
    OW_layer_<Layer>.raw     uint8 weights, same grid, the layers sum to 255 per vertex
    OW_layout.json           landscape grid, resolved regions / features / paths, road profiles
    OW_masks.npz             masks on the vertex grid ([iy, ix]) for the dressing steps
    preview.png, preview_slope.png, check.txt

Height frame: the lake water table is z = 0 cm (one water sheet). Dry lowland sits about 2.2 m above it; every lake is
carved below it behind a bank of about 60 degrees, so the lakes block all four nav agents (they stop at 44 degrees)
without any nav volume. The base ridge in the south-west corner is a flat top 18 m above the lowland behind a 60 degree
crag band; only the three graded ramps (and the road from Berlin past the play edge) cut it.
The paint layers are the nine Pripyat layers (same lake/peat/dead-forest palette, layer infos already exist).
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

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "OderWarthe", "df2_c.layout.json")
OUT = os.path.join(ROOT, "Saved", "OderWarthe", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe"

COMPONENTS = 14                         # 882 m: the 700 x 620 m play area plus 91 / 131 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1
SIZE = COMPONENTS * QUADS * QUAD_CM
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

LAYERS = ["PripPeat", "PripMoss", "PripMud", "PripGrass", "PripSand", "PripGravel", "PripEarth", "PripAsh", "PripAlgae"]

NAV_MAX_SLOPE_DEG = 44.0
BANK = math.tan(math.radians(60.0))     # cm of height per cm of run on every blocking bank
LOW = 175.0                             # dry lowland (glacial sand, rolling a metre or so)
SHORE = 25.0                            # land at the waterline: shores run down gently to it
BOG = 80.0                              # peat bog ground (passable, slow by paint only; never open water)
BED = -330.0                            # lake bed
CUT_TOP = -60.0                         # the 60 degree blocking cut starts 60 cm under the waterline (critic R2-1)
SHORE_BAND = (-75.0, 75.0)              # every cell in this band is smoothed among itself: no stair-stepped shores
# critic R2-6: the tunnel portal is dug in: its floor sits 2.5 m under the apron, reached by a cut ramp (Werk frame)
PORTAL_DEPTH = 250.0
PORTAL_CUT = (-5000.0, -7400.0, 650.0)  # local x of the portal floor, local x where the ramp meets the apron, half width
CROWN = 265.0                           # dam / causeway crown
TOP = 1975.0                            # ridge top (18 m over the lowland)
CRAG_DROP = 720.0                       # height of the 60 degree anti-tank escarpment under the rim
BOARDWALK_CROWN = 205.0
UP_WATER = 170.0                        # the dammed (upstream) water behind the Packlitz and Lubow dams
# upstream pond: dam id -> (side of the dam, cut line cm, x range): the lake cells there stand at UP_WATER
DAMMED = {"dam_packlitz": ("y_lt", -29000.0, (-2500.0, 13000.0)), "dam_lubow": ("y_gt", 28300.0, (4500.0, 20000.0))}

# Werk template: one identical hill stamp per bunker group, placed by centre and yaw only (critic R0-1). The tunnel
# mouth sits 50 m behind the centre (local -X); the yaw is chosen so every tunnel mouth stays more than 100 m from
# every resource and direction-finder station (the sketch's tunnel points sat 13-41 m from resources).
WERK_HILL = 700.0                       # the hill top stands 7 m over the ground round it
WERK_TOP_R = 2000.0
WERK_SIDE_DEG = 16.0
WERK_TUNNEL = 5000.0
WERK_APRON_R = 1000.0
WERK_TUNNEL_BEARING = {"w1": 50.0, "w2": 210.0, "w3": 50.0, "w4": 240.0, "w5": 100.0}
# critic R1-4: one irregular drumlin (not a compass disc), defined in the Werk's own frame (+X = its front)
DRUMLIN_A, DRUMLIN_B = 3400.0, 2600.0   # semi-axes along local X / Y
DRUMLIN_TOP = 0.5                       # plateau out to this normalised radius
DRUMLIN_FOOT = 1.22                     # the stamp blends into the ground between 1.12 and this
# worn ground on every Werk, local frame: paths (polylines), craters (x, y, radius), a trench scar
WERK_PATHS = [[(-5000.0, 0.0), (-3200.0, 250.0), (-1200.0, 0.0)], [(-300.0, 0.0), (900.0, -900.0)],
              [(-300.0, 0.0), (1150.0, 650.0)], [(-300.0, 0.0), (250.0, 1250.0)], [(-1200.0, 0.0), (-1300.0, -1100.0)]]
WERK_CRATERS = [(2100.0, -1500.0, 260.0), (-2600.0, 1900.0, 320.0), (1500.0, 1900.0, 220.0), (-900.0, -2100.0, 280.0)]
WERK_BLOCK = (-300.0, 0.0)                                       # = ow_dress_gen WERK_TEMPLATE block / cupolas
WERK_CUPOLAS = [(900.0, -900.0), (1150.0, 650.0), (250.0, 1250.0)]
BREACHES = ((-21000.0, -2000.0), (-2960.0, 2790.0))                # Kalau / island causeway breach centres (= ow_dress_gen)
WERK_BLOCK_BOX = (-1372.0, 772.0, 1080.0)                         # buried block footprint: front x, back x, half width
WERK_MOUND = 560.0                                                 # earth over the block roof (5 m over the plateau)
WERK_MOUND_FALL = 1500.0                                           # its sides and back fall over 15 m (27 degrees)
WERK_BENCH = (380.0, 720.0)                                        # flat cupola bench radius, blend out to


def werk_local_height(lx, ly):
    """The drumlin's height over its base at a Werk-frame point (no mound): same formula as the stamp."""
    rho = math.hypot(lx / DRUMLIN_A, ly / DRUMLIN_B)
    prof = 1.0 - float(smoothstep(DRUMLIN_TOP, 1.12, np.array(rho)))
    relief = 35.0 * math.sin(lx / 610.0) * math.cos(ly / 470.0) + 25.0 * math.sin((lx + ly) / 390.0)
    return WERK_HILL * prof * (1.0 + 0.10 * max(-1.0, min(1.0, lx / DRUMLIN_A))) + relief * prof


def werk_mound_abs(lx, ly):
    """The mound surface over the Werk base: 60 cm under the plateau where it fades out, so the plateau relief wins there."""
    return WERK_HILL - 60.0 + werk_block_mound(lx, ly)


def werk_block_mound(lx, ly):
    """Earth heaped over the block: flat over the roof, a steep front at the concrete face, 25 degree sides and back."""
    fx, bx, hw = WERK_BLOCK_BOX
    side = np.maximum(np.abs(ly) - hw, 0.0)
    back = np.maximum(lx - bx, 0.0)
    outer = np.hypot(side, back)
    m = WERK_MOUND * (1.0 - smoothstep(0.0, WERK_MOUND_FALL, outer))
    front = np.clip((lx - (fx - 60.0)) / 300.0, 0.0, 1.0)            # 0 just before the face, full 2.4 m behind it
    return m * front


WERK_TRENCH = [(-1700.0, -1600.0), (-2000.0, -800.0), (-1750.0, 0.0), (-2050.0, 800.0), (-1800.0, 1600.0)]
# critic R1-10: the rail yard (rich metal) sits on one ballast bed that runs off the east edge of the map
RAIL_LINE = [(3500.0, 15300.0), (4800.0, 21000.0), (3800.0, 30000.0), (3500.0, 45000.0)]

# station hills: feature id -> (extra height cm, radius cm)
STATION_HILLS = {"s1": (60.0, 1800.0), "s2": (420.0, 3600.0), "s3": (140.0, 3200.0), "s4": (220.0, 2600.0), "s5": (520.0, 3800.0)}

# corridors: id -> (half width cm, kind). Profiles come from the ground under them (see profile()).
ROADS = {
    "dam_packlitz": (700.0, "dam"), "causeway_kalau": (600.0, "dam"), "causeway_island": (650.0, "dam"),
    "dam_lubow": (650.0, "dam"), "boardwalk_nischlitz": (125.0, "boardwalk"), "boardwalk_hochwald": (125.0, "boardwalk"),
    "ford_hochwald": (750.0, "ford"),
    "ramp_north": (750.0, "ramp"), "ramp_prow": (600.0, "ramp"), "ramp_east": (650.0, "ramp"), "road_berlin": (450.0, "ramp"),
}
ROAD_ORDER = ["dam_packlitz", "causeway_kalau", "causeway_island", "dam_lubow", "boardwalk_nischlitz", "boardwalk_hochwald",
              "ford_hochwald", "ramp_north", "ramp_prow", "ramp_east", "road_berlin"]
TRACKS = ["track_n", "track_ford", "track_ne", "track_lubow", "track_c", "track_se", "track_se2"]


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
    """The mask sampled at noise-displaced positions: straight sketch outlines turn ragged. amp_cells may be an array
    (the displacement then grows smoothly, with no seam)."""
    dr = np.rint(amp_cells * noise(seed, wavelength)).astype(int)
    dc = np.rint(amp_cells * noise(seed + 1, wavelength)).astype(int)
    rr = np.clip(np.arange(mask.shape[0])[:, None] + dr, 0, mask.shape[0] - 1)
    cc = np.clip(np.arange(mask.shape[1])[None, :] + dc, 0, mask.shape[1] - 1)
    return soft(mask[rr, cc], 3) > 0.5          # blur after the warp: integer shifts leave 1 m jags


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
        d[nxt & ~cur] = n
        cur = nxt
    return d


def sub_cell_dist(mask, limit):
    """Distance past the mask rim in cm, sub-cell near the rim (from the blurred mask) so banks follow the outline."""
    blur = soft(mask, 2)
    near_d = np.clip((0.5 - blur) / 0.2, 0.0, 2.5)
    far_d = dist_outside(mask, limit)
    return np.where(far_d > 2.5, far_d, near_d) * QUAD_CM


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


class Road:
    def __init__(self, rid, pts, half, kind):
        self.id, self.half, self.kind = rid, half, kind
        pts = np.asarray(pts, float)
        seg = np.hypot(*np.diff(pts, axis=0).T)
        cum = np.r_[0.0, np.cumsum(seg)]
        s = np.arange(0.0, cum[-1], 100.0)
        self.s = s
        self.p = np.c_[np.interp(s, cum, pts[:, 0]), np.interp(s, cum, pts[:, 1])]
        self.h = np.full(len(s), CROWN)
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
        idx = np.arange(len(self.p))
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
        self.masks = dict(self.terrain.region_masks)
        self.X = (SIZE / 2.0 - np.arange(V) * QUAD_CM)[:, None] * np.ones((1, V))
        self.Y = np.ones((V, 1)) * (-SIZE / 2.0 + np.arange(V) * QUAD_CM)[None, :]
        self.inside = (np.abs(self.X) <= self.spec.xmax) & (np.abs(self.Y) <= self.spec.ymax)
        # past the play edge every sketch shape simply carries on: sample masks at the clamped position
        rc = (np.clip((SIZE / 2.0 - np.clip(self.X, self.spec.xmin + 200.0, self.spec.xmax - 200.0)) / QUAD_CM, 0, V - 1).round().astype(int),
              np.clip((np.clip(self.Y, self.spec.ymin + 200.0, self.spec.ymax - 200.0) + SIZE / 2.0) / QUAD_CM, 0, V - 1).round().astype(int))
        self.clamp_rc = rc
        self.Z = None
        self.W = {}
        self.out_masks = {}
        self.roads = {}

    def extend(self, mask):
        return np.where(self.inside, mask, mask[self.clamp_rc])

    def rc(self, p):
        r = int(round((SIZE / 2.0 - p[0]) / QUAD_CM))
        c = int(round((p[1] + SIZE / 2.0) / QUAD_CM))
        return min(max(r, 0), V - 1), min(max(c, 0), V - 1)

    def bilinear(self, A, x, y):
        fr = (SIZE / 2.0 - x) / QUAD_CM
        fc = (y + SIZE / 2.0) / QUAD_CM
        r0, c0 = int(math.floor(fr)), int(math.floor(fc))
        tr, tc = fr - r0, fc - c0
        return float(A[r0, c0] * (1 - tr) * (1 - tc) + A[r0 + 1, c0] * tr * (1 - tc) + A[r0, c0 + 1] * (1 - tr) * tc +
                     A[r0 + 1, c0 + 1] * tr * tc)

    def region(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for rid in ids:
            m |= self.masks[rid]
        return m

    def regions_of(self, kind, prefix=""):
        return [r.id for r in self.spec.regions if r.kind == kind and r.id.startswith(prefix)]

    def path(self, pid):
        return next(p for p in self.spec.paths if p.id == pid)

    def feature(self, fid):
        return next(f for f in self.spec.features if f.id == fid)

    def line_dist(self, pts, reach=6000.0):
        road = Road("tmp", pts, 0.0, "tmp")
        d, _ = road.nearest(self.X, self.Y, reach)
        return d

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        X, Y = self.X, self.Y
        # 1. lowland: rolling glacial ground, a few metres of relief in broad swells and kettle hollows
        Z = LOW + 55.0 * fbm(11, 90) + 22.0 * fbm(12, 30) + 5.0 * noise(13, 6)

        # 2. bogs: peat sinks under the forest edge, passable but low and wet; one is smouldering
        bog_ids = self.regions_of("marsh")
        bog = np.zeros(SHAPE, dtype=bool)
        for k, rid in enumerate(bog_ids):
            self.masks[rid] = warp(self.masks[rid] & self.inside, 300 + 7 * k, 7.0, 22)
            bog |= self.masks[rid]
        bsoft = soft(bog, 6)
        bog_z = BOG + 18.0 * fbm(21, 12) + 8.0 * noise(22, 4)
        Z = Z * (1.0 - bsoft) + bog_z * bsoft
        burn = self.masks.get("bog_se", np.zeros(SHAPE, dtype=bool))
        burn_core = smoothstep(0.55, 0.95, soft(burn, 6))
        pits = smoothstep(0.25, 0.6, noise(52, 7))
        Z = Z + soft(burn, 3) * (25.0 + 18.0 * np.abs(noise(51, 5))) - burn_core * (40.0 + 30.0 * pits)
        self.bog = bog
        self.burn = soft(burn, 3)
        self.burn_core = burn_core

        # 3. werk hills: one identical drumlin stamp in the Werk frame (authoritative inside its footprint, so the five
        # are identical to the centimetre), worn paths, trench scar and old craters pressed into it, a flat tunnel apron
        self.werks = {}
        base_ref = soft(Z, 25)
        for k in range(1, 6):
            fid = "w%d" % k
            c = np.asarray(self.feature(fid).points[0], float)
            bearing = WERK_TUNNEL_BEARING[fid]
            yaw = math.radians((bearing + 180.0) % 360.0)
            base = float(base_ref[self.rc(c)])
            lx = (X - c[0]) * math.cos(yaw) + (Y - c[1]) * math.sin(yaw)
            ly = -(X - c[0]) * math.sin(yaw) + (Y - c[1]) * math.cos(yaw)
            rho = np.hypot(lx / DRUMLIN_A, ly / DRUMLIN_B)
            prof = 1.0 - smoothstep(DRUMLIN_TOP, 1.12, rho)
            # local, deterministic relief (identical in every Werk frame): a lumpy top and a stoss end
            local_relief = 35.0 * np.sin(lx / 610.0) * np.cos(ly / 470.0) + 25.0 * np.sin((lx + ly) / 390.0)
            stoss = 1.0 + 0.10 * np.clip(lx / DRUMLIN_A, -1.0, 1.0)
            hz = base + WERK_HILL * prof * stoss + local_relief * prof
            for cx_, cy_, cr in WERK_CRATERS:
                dc = np.hypot(lx - cx_, ly - cy_)
                hz = hz - 45.0 * smoothstep(cr, cr * 0.3, dc) + 18.0 * smoothstep(cr * 1.5, cr, dc) * smoothstep(cr * 0.8, cr, dc)
            # r8 (critic R7-5): the cupolas stand on level 7.6 m benches, so every skirt sits flat. (Burying the block
            # under a mound was tried and dropped: the block fills the plateau, so the mound's flank lands on the
            # drumlin's 33 degree flank and the sum walls off the hill top for every agent.)
            for cx_, cy_ in WERK_CUPOLAS:
                hc = werk_local_height(cx_, cy_)
                wb = smoothstep(WERK_BENCH[1], WERK_BENCH[0], np.hypot(lx - cx_, ly - cy_))
                hz = hz * (1.0 - wb) + (base + hc) * wb
            blend = 1.0 - smoothstep(1.12, DRUMLIN_FOOT, rho)
            Z = Z * (1.0 - blend) + hz * blend
            t_pt = c + WERK_TUNNEL * np.array([math.cos(math.radians(bearing)), math.sin(math.radians(bearing))])
            a_pt = c + (-PORTAL_CUT[1] + WERK_APRON_R) * np.array([math.cos(math.radians(bearing)), math.sin(math.radians(bearing))])
            ra = np.hypot(X - a_pt[0], Y - a_pt[1])
            apron = smoothstep(WERK_APRON_R + 800.0, WERK_APRON_R, ra)
            Z = Z * (1.0 - apron) + base * apron
            # the dug-in portal: a ramp from the apron down to the portal floor, 60 degree walls either side
            px0, px1, phw = PORTAL_CUT
            along = np.clip((lx - px1) / (px0 - px1), 0.0, 1.0)            # 0 at the apron end, 1 at the portal
            depth = min(PORTAL_DEPTH, base - 50.0)                   # the portal floor stays half a metre over the water table
            floor = base - depth * along ** 0.8
            inside_cut = (lx < px0 + 900.0) & (lx > px1) & (np.abs(ly) < phw)
            wall = floor + BANK * np.maximum(np.abs(ly) - phw, 0.0) + BANK * np.maximum(lx - px0 - 900.0, 0.0)
            pcut = np.where(inside_cut, floor, np.where((lx > px1 - 50.0) & (lx < px0 + 2500.0), wall, 1e9))
            # the berm: a mound over the portal's roof and flanks (critic R3-1); the cut ramp in front stays open
            bx = (lx - (px0 + 700.0)) / 1500.0
            by = ly / 1050.0                                             # only the portal's own flanks: lanes stay open
            berm = base + 400.0 * (1.0 - smoothstep(0.55, 1.0, np.hypot(bx, by)))
            front_open = (lx < px0 + 300.0) & (np.abs(ly) < phw + 100.0)
            hz = np.where(front_open, hz, np.maximum(hz, berm))
            bw = 1.0 - smoothstep(0.55, 1.05, np.hypot(bx, by))
            Z = np.where(front_open, Z, np.maximum(Z, berm * (bw > 0.0) + Z * (bw <= 0.0)))
            Z = np.minimum(Z, pcut)
            self.werks[fid] = {"centre": c, "base": base, "yaw": math.degrees(yaw) % 360.0, "tunnel": t_pt,
                               "apron_pt": a_pt, "portal_cut": pcut, "berm": np.where(front_open, -1e9, berm), "portal_floor": base - depth,
                               "mask": rho <= DRUMLIN_TOP, "hill": rho < DRUMLIN_FOOT + 0.05, "apron": ra < WERK_APRON_R,
                               "hill_z": hz, "blend": blend, "apron_w": apron, "lx": lx, "ly": ly, "rho": rho}

        # 4. station hills: broad domes under the masts (kept under 25 degrees)
        for fid, (extra, rad) in STATION_HILLS.items():
            p = self.feature(fid).points[0]
            r = np.hypot(X - p[0], Y - p[1]) + 300.0 * noise(360 + int(extra), 12)
            dome = LOW + extra * smoothstep(rad, rad * 0.25, r)
            Z = np.maximum(Z, dome)

        # 5. the sluice islet in the Mittelsee
        islet = warp(self.masks["sluice_islet"], 380, 2.0, 8)
        self.masks["sluice_islet"] = islet

        # 6. the base ridge: flat top, a 60 degree crag band under the rim, a talus apron down to the foot
        top = warp(self.extend(self.masks["ridge_top"]), 400, 4.0, 26)
        top = warp(top, 402, 11.0, 70)                                  # spurs and gullies, +-11 m
        beyond = np.maximum(np.maximum(np.abs(X) - self.spec.xmax, np.abs(Y) - self.spec.ymax), 0.0)
        ramp_amp = 26.0 * smoothstep(0.0, 6000.0, beyond)              # ragged past the play edge, no seam at it
        top = warp(top, 405, ramp_amp, 60)
        for rid in ("ramp_north", "ramp_prow", "ramp_east"):            # a flat 30 x 30 m pad at every ramp head
            head = np.asarray(self.feature(rid).centreline[0], float)
            top |= np.hypot(X - head[0], Y - head[1]) < 1500.0
        foot = self.extend(self.masks["ridge_face"])
        foot = warp(foot, 406, ramp_amp, 60)
        d_top = sub_cell_dist(top, 80)                                  # cm past the rim
        d_foot = sub_cell_dist(foot, 40)
        # talus: from the crag foot down to the lowland at the sketch's foot contour (concave, under 30 degrees)
        drop = CRAG_DROP + 220.0 * fbm(415, 40)                         # 5-9.4 m: the face varies in width (critic R2-3)
        crag_h = TOP - drop
        crag_run = drop / BANK
        talus_len = np.maximum(d_top - crag_run, 0.0)
        span = np.maximum(d_top + d_foot - crag_run, 600.0)             # crag foot to the sketch foot, per cell
        t = np.clip(talus_len / span, 0.0, 1.0)
        base = Z
        talus = crag_h + (base - crag_h) * (1.0 - (1.0 - t) ** 1.6)
        ridge = np.where(d_top <= 0.0, TOP + 9.0 * noise(410, 20) * smoothstep(0.0, 1.0, soft(top, 4) * 2.0 - 1.0),
                         np.where(d_top < crag_run, TOP - BANK * d_top, talus))
        on_ridge = (d_top <= 0.0) | (d_foot <= 0.0)
        Z = np.where(on_ridge, np.maximum(Z, ridge), Z)
        self.ridge_top = (d_top <= 0.0)
        self.ridge_any = on_ridge
        self.crag = (d_top > 0.0) & (d_top < crag_run + 80.0)

        # 7. lakes: every water region of the sketch (lakes, inlets, bays, ponds), carried past the play edge
        water = np.zeros(SHAPE, dtype=bool)
        for k, reg in enumerate(r for r in self.spec.regions if r.kind == "water"):
            m = self.extend(self.masks[reg.id])
            m = warp(m, 420 + k, 5.0 if reg.id.startswith(("lake_", "pond_")) else 3.0, 30)
            water |= m
        water &= ~islet
        # critic R1-1: meander the arms (+-18 m), vary their width (about -8..+12 m a side), and close every arm 40-80 m
        # past the play edge instead of running it to the landscape edge as a canal
        water = warp(water, 450, 16.0, 95)
        sdf = sub_cell_dist(water, 40) - sub_cell_dist(~water, 40)              # cm, negative inside the water
        beyond = np.maximum(np.maximum(np.abs(X) - self.spec.xmax, np.abs(Y) - self.spec.ymax), 0.0)
        close_len = 4000.0 + 4000.0 * (0.5 + 0.5 * noise(452, 40))
        width_mod = 1000.0 * fbm(451, 55) + 200.0
        water = sdf < width_mod - 1.6 * beyond - 3000.0 * smoothstep(close_len * 0.6, close_len, beyond)
        water &= ~islet
        # every site the mission arms (stations, garrisons, outposts, resources, tunnel mouths) stays on dry ground
        for f in self.spec.features:
            if f.kind in ("capture", "enemy_group", "enemy_spawn", "resource", "enemy_base") and f.points:
                p = f.points[0]
                water &= np.hypot(X - p[0], Y - p[1]) > 900.0
        for wk in self.werks.values():
            water &= wk["rho"] > DRUMLIN_FOOT + 0.12
            water &= np.hypot(X - wk["tunnel"][0], Y - wk["tunnel"][1]) > WERK_APRON_R + 900.0
            water &= np.hypot(X - wk["apron_pt"][0], Y - wk["apron_pt"][1]) > WERK_APRON_R + 900.0
        water = soft(water, 2) > 0.5
        # no water slivers under about 8 m: they cannot be deep enough to block (morphological opening, 4 cells)
        er = water.copy()
        for _ in range(4):
            q = np.pad(er, 1, mode="edge")
            er = q[:-2, 1:-1] & q[2:, 1:-1] & q[1:-1, :-2] & q[1:-1, 2:] & er
        for _ in range(4):
            q = np.pad(er, 1, mode="edge")
            er = q[:-2, 1:-1] | q[2:, 1:-1] | q[1:-1, :-2] | q[1:-1, 2:] | er
        water = soft(er & water, 1) > 0.5
        self.water_sketch = water
        d_in = sub_cell_dist(~water, 30)                                 # cm inside the water from its rim
        d_out = sub_cell_dist(water, 30)                                 # cm outside on land
        # the integer distance past 2.5 cells draws 1 m stairs into the underwater cut, visible through shallow water:
        # blur the distance field itself (critic R2-1)
        d_in = np.where(water, soft(d_in, 2), 0.0)
        # the dammed ponds: the lake cells on the upstream side of the two dams stand at UP_WATER
        level = np.zeros(SHAPE)
        self.ponds = []
        for did, (side, cut, (xa, xb)) in DAMMED.items():
            sel = water & (X > xa) & (X < xb) & ((Y < cut) if side == "y_lt" else (Y > cut))
            level = np.where(sel, UP_WATER, level)
            rr, cc = np.nonzero(sel)
            self.ponds.append({"dam": did, "xmin": float(X[rr, cc].min() - 300.0), "xmax": float(X[rr, cc].max() + 300.0),
                               "ymin": float(Y[rr, cc].min() - 300.0), "ymax": float(Y[rr, cc].max() + 300.0), "z": UP_WATER,
                               "mask": sel})
        # the land round a pond rises with it: a max filter (not an average), so no shore dips under the pond surface
        lvl_land = level.copy()
        for _ in range(22):
            q = np.pad(lvl_land, 1, mode="edge")
            lvl_land = np.maximum.reduce([lvl_land, q[:-2, 1:-1], q[2:, 1:-1], q[1:-1, :-2], q[1:-1, 2:]])
        lvl_land = soft(lvl_land, 6)
        deep = BED + 30.0 * noise(431, 14) - 40.0 * smoothstep(0.6, 1.0, soft(water, 9))
        # in a narrow channel the shelf shrinks (its half width is the largest rim distance nearby), so the cut still
        # reaches deep water: an 8-12 m inlet must block as well as a lake
        half_w = d_in.copy()
        for _ in range(16):
            q = np.pad(half_w, 1, mode="edge")
            half_w = np.maximum.reduce([half_w, q[:-2, 1:-1], q[2:, 1:-1], q[1:-1, :-2], q[1:-1, 2:]])
        shelf = np.where(half_w > 600.0, np.clip(0.3 * half_w, 90.0, 230.0), 40.0)   # wide water: never a cliff at the waterline
        # critic R2-1: the waterline is the zero contour of a SMOOTHED signed distance (not the 1 m mask edge, which
        # draws stairs): the shore rises at 8.5 degrees from it, the water shelves down to about -75 cm, then the cut
        sdf = soft(d_out - np.where(water, d_in, 0.0), 3)
        lake = np.maximum(level + 0.25 * np.maximum(sdf, -shelf) - 2.6 * BANK * np.maximum(-sdf - shelf, 0.0), level + deep)
        shore_h = lvl_land + SHORE + 20.0 * noise(433, 5)
        fade = smoothstep(0.0, 1800.0, d_out + 250.0 * noise(434, 9))
        land = shore_h + (np.maximum(Z, shore_h) - shore_h) * fade
        near = lvl_land + 0.15 * np.clip(sdf, 0.0, 600.0)               # the first 6 m of shore only (capped)
        w_near = smoothstep(150.0, 600.0, sdf)
        land = near * (1.0 - w_near) + land * w_near
        Z = np.where(sdf < 0.0, lake, land)
        water = (sdf < 0.0) & ((level > 0.0) | water | (soft(water, 2) > 0.2))
        self.water_sketch = water
        # the pond water is a set of 4 m tiles over the pond cells (one tile of margin); land under a tile stands above
        # the pond level, so the sheet shows only on the pond (no rectangle raised round it)
        TILE = 400.0
        self.pond_tiles = []
        for pond in self.ponds:
            mask = pond["mask"]
            rr, cc = np.nonzero(mask)
            tx = np.floor(X[rr, cc] / TILE).astype(int)
            ty = np.floor(Y[rr, cc] / TILE).astype(int)
            cover = set()
            for a_, b_ in set(zip(tx.tolist(), ty.tolist())):
                for da in (-1, 0, 1):
                    for db in (-1, 0, 1):
                        cover.add((a_ + da, b_ + db))
            # never over the lower lake beside the dam: drop tiles that hold lake cells of the low level
            low_water = water & ~mask
            lr, lc = np.nonzero(low_water)
            low_tiles = set(zip(np.floor(X[lr, lc] / TILE).astype(int).tolist(), np.floor(Y[lr, lc] / TILE).astype(int).tolist()))
            cover -= low_tiles
            tile_mask = np.zeros(SHAPE, dtype=bool)
            gx = np.floor(X / TILE).astype(int)
            gy = np.floor(Y / TILE).astype(int)
            for a_, b_ in cover:
                tile_mask |= (gx == a_) & (gy == b_)
            Z = np.where(tile_mask & ~mask, np.maximum(Z, UP_WATER + 35.0 + 15.0 * noise(437, 4)), Z)
            pond["tiles"] = [[(a_ + 0.5) * TILE, (b_ + 0.5) * TILE] for a_, b_ in sorted(cover)]
            pond["tile_cm"] = TILE
        self.level = level
        # critic R2-7: the Mittelsee is the design's algae lake: 4 m tiles of purple algae water over it (z +2 cm)
        mfield = soft(self.extend(self.masks["lake_mittel"]), 14) + 0.35 * noise(441, 6) + 0.2 * noise(442, 2.5)
        mittel = water & (mfield > 0.45) & (level == 0.0) & (sub_cell_dist(~water, 6) > 150.0)
        rr, cc = np.nonzero(mittel)
        tiles = set(zip(np.floor(X[rr, cc] / 400.0).astype(int).tolist(), np.floor(Y[rr, cc] / 400.0).astype(int).tolist()))
        self.algae_tiles = [[(a_ + 0.5) * 400.0, (b_ + 0.5) * 400.0] for a_, b_ in sorted(tiles)]
        self.mittel = mittel
        # the shores must not eat into a werk hill: stamp the hills and aprons again (identical in the werk frame)
        for wk in self.werks.values():
            Z = Z * (1.0 - wk["blend"]) + wk["hill_z"] * wk["blend"]
            Z = Z * (1.0 - wk["apron_w"]) + wk["base"] * wk["apron_w"]
        # the islet rises out of the Mittelsee
        d_isl = sub_cell_dist(islet, 10)
        Z = np.where(d_isl <= 0.0, CROWN + 20.0 + 6.0 * noise(381, 6), np.where(d_isl < 600.0, np.maximum(Z, CROWN + 20.0 - BANK * d_isl), Z))

        # 8. corridors: dams, causeways, boardwalks, the ford, the ramps
        self.road_d = np.full(SHAPE, 1e9)
        self.road_kind = np.full(SHAPE, "", dtype=object)
        self.road_centre = np.full(SHAPE, 1e9)
        self.road_end = np.full(SHAPE, 1e9)
        self.road_kind_near = np.full(SHAPE, "", dtype=object)
        for rid in ROAD_ORDER:
            Z = self.lay_road(rid, Z)

        # R7-3: the two breaches are notches: the crown drops 1.1 m over 7 m into a 10-11 m wide rubble gap
        for bx_, by_ in BREACHES:
            db_ = np.hypot(X - bx_, Y - by_)
            notch = smoothstep(1300.0, 600.0, db_) * (self.road_d < 250.0)
            Z = Z - 110.0 * notch
        toe = (self.road_d > 0.0) & (self.road_d < 900.0) & (Z < self.level) & np.isin(self.road_kind_near, ["dam"])
        Z = np.where(toe, np.minimum(Z, self.level - 170.0), Z)
        # R2-1: smooth every cell in the waterline band among itself (corridor decks and their banks excluded)
        lo_b, hi_b = SHORE_BAND
        band = (Z - self.level >= lo_b) & (Z - self.level <= hi_b) & (self.road_d > 400.0)
        for lvl in np.unique(self.level):                                # each water level on its own (pond vs lake)
            bl = band & (self.level == lvl)
            for _ in range(2):
                num = soft(np.where(bl, Z, 0.0), 2)
                den = soft(bl, 2)
                Z = np.where(bl & (den > 1e-3), num / np.maximum(den, 1e-3), Z)
        # the werk stamps are authoritative: stamp them again (identity) after every terrain operation
        for wk in self.werks.values():
            Z = Z * (1.0 - wk["blend"]) + wk["hill_z"] * wk["blend"]
            Z = Z * (1.0 - wk["apron_w"]) + wk["base"] * wk["apron_w"]
            Z = np.maximum(Z, np.where(np.hypot((wk["lx"] - (PORTAL_CUT[0] + 700.0)) / 1500.0, wk["ly"] / 1050.0) < 1.05,
                                       wk["berm"], -1e9))
            Z = np.minimum(Z, wk["portal_cut"])
        self.Z = Z
        self.slope = slope_deg(Z)

    def corridor_points(self, rid):
        if rid in ("ramp_north", "ramp_prow", "ramp_east"):
            return np.asarray(self.feature(rid).centreline, float)
        if rid == "ford_hochwald":
            f = self.feature(rid)
            c = np.asarray(f.points[0], float)
            return np.array([[c[0], c[1] - 4600.0], [c[0] + 150.0, c[1] - 900.0], [c[0] - 100.0, c[1] + 2400.0], [c[0], c[1] + 4400.0]])
        pts = np.asarray(self.path(rid).pts, float)
        if rid == "road_berlin":
            # the road from Berlin comes in from the south-west corner of the landscape and climbs onto the ridge
            a, b = pts[0], pts[1]
            u = (a - b) / np.linalg.norm(a - b)
            pts = np.vstack([a + u * 9000.0, pts])
        if rid.startswith(("dam_", "causeway_", "boardwalk_")):
            # each end runs a few metres on into the land so the crown meets the ground
            a, b = pts[0], pts[1]
            pts = np.vstack([a + (a - b) / np.linalg.norm(a - b) * 900.0, pts])
            a, b = pts[-1], pts[-2]
            pts = np.vstack([pts, a + (a - b) / np.linalg.norm(a - b) * 900.0])
        return pts

    def profile(self, road, Z):
        """Crown height and half width along a corridor: on land it follows the ground, over water it is the crown."""
        rid, p, s = road.id, road.p, road.s
        idx = np.array([self.rc(q) for q in p])
        ground = Z[idx[:, 0], idx[:, 1]]
        wet = self.water_sketch[idx[:, 0], idx[:, 1]]
        lvl = self.level[idx[:, 0], idx[:, 1]]
        if road.kind == "dam":
            h = np.where(wet, CROWN, np.maximum(ground, CROWN - 40.0))
        elif road.kind == "boardwalk":
            h = np.where(wet, BOARDWALK_CROWN + lvl, ground)
        elif road.kind == "ford":
            h = np.where(wet, 2.0, ground)                 # a wet gravel bar at the waterline: it reads as a ford
        else:                                           # ramps: top of the ridge to the ground at the foot
            f = self.feature(rid) if rid in ("ramp_north", "ramp_prow", "ramp_east") else None
            foot = ground[-1] if f is not None else ground[0]
            t = np.clip(s / max(road.length, 1.0), 0.0, 1.0)
            if f is not None:
                h = TOP + (foot - TOP) * t
            else:                                       # road_berlin runs from the far corner up to the start
                lead = 9000.0
                t = np.clip((s - lead * 0.3) / max(road.length - lead * 0.3 - 2500.0, 1.0), 0.0, 1.0)
                h = foot + (TOP - foot) * t
        # chokes from the sketch (sluice gates, breaches): narrower, the breach sits a little lower
        raw = self.path(rid).raw if rid in self.spec.by_id and rid in [q.id for q in self.spec.paths] else {}
        for ch in raw.get("chokes", []):
            at = np.asarray(ch["at"], float)
            k = int(np.hypot(p[:, 0] - at[0], p[:, 1] - at[1]).argmin())
            along = np.abs(s - s[k])
            inside = smoothstep(ch["length"] / 2.0 + 400.0, ch["length"] / 2.0 - 150.0, along)
            road.w[:] = road.w - (road.w - ch["width"] / 2.0) * inside
            if "breach" in ch["id"]:
                h = h - 70.0 * inside
        kernel = np.ones(15) / 15.0
        road.h[:] = np.convolve(np.pad(h, 7, mode="edge"), kernel, mode="valid")
        if road.kind == "ramp":
            road.h[:] = h                               # the ramp grade is already linear

    def lay_road(self, rid, Z):
        half, kind = ROADS[rid]
        pts = self.corridor_points(rid)
        road = Road(rid, pts, half, kind)
        if rid in ("ramp_north", "ramp_prow", "ramp_east"):
            f = self.feature(rid)
            road.w[:] = np.linspace(f.width, f.width_end, len(road.s)) / 2.0
        self.profile(road, Z)
        self.roads[rid] = road
        d, k = road.nearest(self.X, self.Y, 3200.0)
        near = d < 1e8
        h = np.where(near, road.h[k], 0.0)
        w = np.where(near, road.w[k], 0.0)
        e = np.maximum(d - w, 0.0)
        crown = -6.0 * (np.clip(d / np.maximum(w, 1.0), 0.0, 1.0) ** 2)
        surf = h + crown
        lo, hi = surf - BANK * e, surf + BANK * e
        if kind == "ford":
            # the ford's approaches are cut at 37 degrees above the water; under the water its sides stay a 60 degree cut
            gentle = surf - 0.75 * e
            lo = np.where(gentle > -25.0, gentle, np.minimum(-25.0, surf) - BANK * np.maximum(e - np.maximum(surf + 25.0, 0.0) / 0.75, 0.0))
            hi = surf + 0.75 * e
        Zn = np.where(near, np.clip(Z, lo, hi), Z)
        on = near & (e <= 0.0)
        end_s = np.minimum(road.s[k], road.length - road.s[k])
        self.road_end = np.where(near & (d - w < self.road_d), end_s, self.road_end)
        self.road_d = np.where(near, np.minimum(self.road_d, d - w), self.road_d)
        self.road_kind = np.where(on, kind, self.road_kind)
        self.road_kind_near = np.where(near & (d - w < 900.0), kind, self.road_kind_near)
        self.road_centre = np.where(on, np.minimum(self.road_centre, d), self.road_centre)
        self.out_masks["road_" + rid] = on
        return Zn

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        Z, slope = self.Z, self.slope
        X, Y = self.X, self.Y
        n1, n2, n3 = fbm(101, 28), noise(102, 7), fbm(103, 70, 2)
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        # lowland: pale dead grass on sandy glacial soil, bare earth and sand in the swells
        grass = np.clip(0.7 + 0.5 * n1, 0.0, 1.0)
        bare = np.clip(0.3 - 0.6 * n1 + 0.35 * n2, 0.0, 1.0)
        W["PripGrass"] += 4.0 * grass
        W["PripEarth"] += 3.0 * bare
        W["PripSand"] += 2.0 * np.clip(n3 + 0.2 * n2, 0.0, 1.0)
        # the Kalau heath (south-east basin): dead ochre heath on sand, a ragged edge (critic R1-6: not a white disc)
        heath = soft(warp(np.hypot(X + 21200.0, Y - 10300.0) < 7500.0, 460, 14.0, 40), 6)
        W["PripGrass"] += 3.0 * heath * (0.6 + 0.4 * n2)
        W["PripEarth"] += 2.5 * heath * np.clip(0.4 - n1, 0.0, 1.0)
        W["PripSand"] += 1.2 * heath * np.clip(n2 + 0.2, 0.0, 1.0)
        # dead pine forest floor: needle litter, char and moss
        forest = np.zeros(SHAPE, dtype=bool)
        for k, rid in enumerate(self.regions_of("forest")):
            m = warp(self.masks[rid] & self.inside, 500 + k, 6.0, 20)
            self.masks[rid] = m
            forest |= m
        # past the play edge: ragged dead woods from noise (critic R1-8: no extruded stripes in the margin)
        forest |= (~self.inside) & (fbm(520, 45) > 0.12) & (Z > 60.0)
        fsoft = soft(forest, 5)
        self.forest = forest
        for name in ("PripGrass", "PripSand"):
            W[name] *= (1.0 - 0.75 * fsoft)
        W["PripPeat"] += 5.0 * fsoft * (0.6 + 0.4 * n1)
        W["PripAsh"] += 2.5 * fsoft * np.clip(n2 + 0.3, 0.0, 1.0)
        W["PripMoss"] += 2.0 * fsoft * smoothstep(-0.1, 0.4, n1)
        # bogs: peat and mud, moss lawns; the smouldering one ash and char
        bsoft = soft(self.bog, 4)
        for name in ("PripGrass", "PripSand", "PripEarth"):
            W[name] *= (1.0 - 0.9 * bsoft)
        W["PripPeat"] += 7.0 * bsoft
        W["PripMud"] += 4.0 * bsoft * smoothstep(0.0, 0.5, noise(111, 6))
        W["PripMoss"] += 3.0 * bsoft * smoothstep(-0.1, 0.4, n1)
        veins = smoothstep(0.3, 0.55, np.abs(noise(106, 4)) + 0.4 * np.abs(noise(107, 9)))
        W["PripMud"] += 9.0 * self.burn_core * (1.0 - veins)
        W["PripAsh"] += 10.0 * self.burn_core * veins + 3.0 * self.burn * (1.0 - self.burn_core)
        # lake shores: mud at the waterline, a purple algae band on the lee (east) shores, under water silt
        wet = smoothstep(40.0, -10.0, Z)
        for name in LAYERS:
            W[name] *= (1.0 - 0.85 * wet)
        W["PripMud"] += 6.0 * wet
        shore = soft(self.water_sketch, 4)
        lee = np.clip(0.45 - 14.0 * np.gradient(shore, axis=1), 0.0, 1.0)
        filaments = smoothstep(0.05, 0.5, noise(104, 5) + 0.6 * noise(105, 2.2) + 0.5 * n1)
        band = np.clip(1.0 - np.abs(shore - 0.5) / 0.45, 0.0, 1.0) * smoothstep(60.0, 0.0, Z)
        algae = np.clip(band * (0.35 + lee) * filaments * 1.5, 0.0, 1.0)
        W["PripAlgae"] += 7.0 * algae
        # r6: the Mittelsee is the algae lake: its shallow shelf carries algae, seen through the murky water
        mit_w = soft(self.extend(self.masks["lake_mittel"]), 8) * smoothstep(-260.0, -60.0, Z) * (Z < 20.0)
        W["PripAlgae"] += 26.0 * mit_w * (0.5 + 0.5 * filaments)
        # r8 (R7-8): no algae speckle at the Hochwald ford
        fp = self.feature("ford_hochwald").points[0]
        W["PripAlgae"] *= smoothstep(3000.0, 5500.0, np.hypot(X - fp[0], Y - fp[1]))
        fd = self.line_dist(np.asarray(self.corridor_points("ford_hochwald"), float), 4000.0)    # R8-2: the whole bar + 3-7 m
        W["PripAlgae"] *= smoothstep(1050.0, 1450.0, fd)
        # a 2-3 m band of wet dark mud on land at every shoreline (critic R1-2)
        d_shore = sub_cell_dist(Z < 0.0, 6)
        band_w = 250.0 + 250.0 * fbm(119, 18)                          # 0-5 m, varying along the shore
        mud = smoothstep(np.maximum(band_w, 20.0), 0.0, d_shore + 40.0 * n2) * (Z >= -5.0)
        for name in ("PripGrass", "PripSand", "PripEarth"):
            W[name] *= (1.0 - 0.8 * mud)
        W["PripMud"] += 9.0 * mud
        # banks and the crag: raw sand and earth; the crag sandstone with gravel scree at its foot
        steep = smoothstep(20.0, 36.0, slope)
        patch = smoothstep(-0.35, 0.35, noise(108, 9) + 0.5 * n1)
        W["PripEarth"] += 10.0 * steep * (0.35 + 0.65 * patch)
        W["PripSand"] += 10.0 * steep * (1.0 - patch) * (Z > 400.0)
        # the escarpment reads as a cut face: bare sand by slope, gravel talus fringe at its foot (critic R1-7)
        crag = np.clip(soft(self.crag, 2) + smoothstep(28.0, 45.0, slope) * self.ridge_any, 0.0, 1.0)
        for name in LAYERS:
            W[name] *= (1.0 - 0.85 * crag)
        streak = smoothstep(0.2, 0.6, np.abs(noise(417, 3)) + 0.5 * noise(418, 11))     # rills down the face
        gully = smoothstep(0.35, 0.7, noise(419, 9)) * self.ridge_any * ~self.ridge_top
        fan = smoothstep(0.3, 0.75, noise(419, 9)) * soft(dist_outside(self.crag, 14) <= 12.0, 3) * ~self.ridge_top * ~self.crag
        W["PripSand"] += 9.0 * fan * (0.6 + 0.4 * n2)
        W["PripGravel"] += 5.0 * fan
        W["PripGrass"] *= (1.0 - 0.7 * np.clip(fan + gully, 0.0, 1.0))
        W["PripEarth"] += 18.0 * crag * (0.7 + 0.3 * patch)
        W["PripMud"] += 7.0 * crag * (1.0 - streak)
        W["PripSand"] += 7.0 * crag * streak
        fringe = soft(dist_outside(self.crag, 5) <= 4.0, 2) * (1.0 - soft(self.crag, 1)) * ~self.ridge_top
        W["PripGravel"] += 10.0 * fringe * np.clip(n2 + 0.4, 0.0, 1.0)
        scree = self.ridge_any & ~self.ridge_top & ~self.crag & (Z > LOW + 150.0)
        W["PripGravel"] += 4.0 * soft(scree, 3) * np.clip(n2 + 0.3, 0.0, 1.0)
        # ridge top: dry heath on sand, worn earth round the start
        topsoft = soft(self.ridge_top, 3)
        W["PripSand"] += 4.0 * topsoft * (0.5 + 0.5 * n3)
        start = self.feature("start").points[0]
        dstart = np.hypot(X - start[0], Y - start[1]) + 300.0 * n2
        W["PripSand"] += 5.0 * topsoft * smoothstep(6000.0, 2500.0, dstart)
        # werk hills: one paint set in the Werk frame (local patterns only, so the five are identical): an earth cap,
        # grass flanks, worn paths between entrance, cupolas, mast and tunnel, a trench scar, old craters, gravel apron
        def lseg_dist(lx, ly, pts):
            best = np.full(lx.shape, 1e9)
            for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
                vx, vy = bx - ax, by - ay
                tt = np.clip(((lx - ax) * vx + (ly - ay) * vy) / (vx * vx + vy * vy), 0.0, 1.0)
                best = np.minimum(best, np.hypot(lx - ax - tt * vx, ly - ay - tt * vy))
            return best

        for fid, wk in self.werks.items():
            lx, ly, rho = wk["lx"], wk["ly"], wk["rho"]
            near = rho < 1.6
            hill_w = (1.0 - smoothstep(1.15, 1.35, rho)) * near
            for name in LAYERS:
                W[name] = W[name] * (1.0 - hill_w)
            lpat = 0.5 + 0.5 * np.sin(lx / 330.0 + 1.3) * np.cos(ly / 270.0 - 0.4)
            W["PripGrass"] += 5.0 * hill_w * (0.6 + 0.4 * lpat)
            W["PripEarth"] += 7.0 * hill_w * (1.0 - smoothstep(0.35, 0.62, rho)) * (0.7 + 0.3 * lpat)
            W["PripAsh"] += 1.5 * hill_w * (1.0 - smoothstep(0.2, 0.5, rho)) * lpat
            for pts in WERK_PATHS:
                pd = lseg_dist(lx, ly, pts)
                W["PripEarth"] += 14.0 * hill_w * smoothstep(170.0, 70.0, pd)
            # r5: dug earth round the block and each cupola, so their earth skirts sit in worked ground, not on grass
            ragged = 120.0 * np.sin(lx / 140.0 + 0.7) * np.cos(ly / 115.0 + 1.9)
            for cx_, cy_ in WERK_CUPOLAS:
                dc = np.hypot(lx - cx_, ly - cy_) + ragged
                W["PripSand"] += 9.0 * hill_w * smoothstep(820.0, 520.0, dc)
                W["PripMud"] += 4.0 * hill_w * smoothstep(760.0, 450.0, dc) * (0.5 + 0.5 * lpat)
            fx_, bx_, hw_ = WERK_BLOCK_BOX
            fore = (lx < fx_ + 50.0) & (lx > fx_ - 900.0 - ragged) & (np.abs(ly) < hw_ + 200.0 + ragged)
            W["PripSand"] += 8.0 * hill_w * fore
            W["PripGravel"] += 4.0 * hill_w * fore * (0.5 + 0.5 * lpat)
            db = np.maximum(np.maximum(lx - bx_, fx_ - lx), np.abs(ly) - hw_) + ragged     # a sod band round the block
            W["PripGrass"] += 6.0 * hill_w * smoothstep(500.0, 0.0, db) * (lx > fx_ + 50.0)
            td = lseg_dist(lx, ly, WERK_TRENCH)
            W["PripMud"] += 10.0 * hill_w * smoothstep(110.0, 40.0, td)
            W["PripEarth"] += 6.0 * hill_w * smoothstep(240.0, 110.0, td)
            for cx_, cy_, cr in WERK_CRATERS:
                dc = np.hypot(lx - cx_, ly - cy_)
                W["PripAsh"] += 6.0 * hill_w * smoothstep(cr * 0.9, cr * 0.2, dc)
                W["PripEarth"] += 6.0 * hill_w * smoothstep(cr * 1.6, cr * 0.9, dc) * smoothstep(cr * 0.5, cr * 0.9, dc)
            tp = wk["apron_pt"]
            ra = np.hypot(X - tp[0], Y - tp[1])
            apron = smoothstep(WERK_APRON_R + 500.0, WERK_APRON_R - 200.0, ra)
            # the cut ramp down to the portal: worn gravel
            cut_w = (lx < PORTAL_CUT[0] + 900.0) & (lx > PORTAL_CUT[1]) & (np.abs(ly) < PORTAL_CUT[2] + 150.0)
            apron = np.maximum(apron, cut_w.astype(float))
            for name in LAYERS:
                W[name] = W[name] * (1.0 - apron)
            W["PripGravel"] += 6.0 * apron
            W["PripEarth"] += 4.0 * apron
        # station hills: worn tops
        for fid in STATION_HILLS:
            p = self.feature(fid).points[0]
            dd = np.hypot(X - p[0], Y - p[1]) + 200.0 * n2
            W["PripEarth"] += 8.0 * smoothstep(1500.0, 600.0, dd)
        # corridors: dams gravel on earth, boardwalk dykes earth, the ford gravel, ramps worn gravel
        d = self.road_d
        kind = self.road_kind
        on = d <= 0.0
        verge = smoothstep(260.0, 0.0, d) * (d > 0.0)
        W["PripEarth"] += 5.0 * verge
        centre = self.road_centre + 80.0 * noise(109, 3) + 50.0 * n2
        # corridor banks: earth and riprap gravel, not a grass halo (critic R1-6)
        bankz = (d > 0.0) & (d < 700.0) & (slope > 22.0)
        W["PripGravel"] += 8.0 * bankz * np.clip(n2 + 0.5, 0.0, 1.0)
        W["PripEarth"] += 6.0 * bankz
        W["PripGrass"] *= (1.0 - 0.8 * bankz)
        end_fade = smoothstep(300.0, 1500.0, self.road_end)            # no grey blobs where a corridor ends on land
        breach = np.zeros(SHAPE)
        for at_ in BREACHES:
            breach = np.maximum(breach, smoothstep(1100.0, 600.0, np.hypot(X - at_[0], Y - at_[1]) + 120.0 * n2))
        breach = breach * (d <= 0.0)
        end_fade = end_fade * (1.0 - breach)
        W["PripMud"] += 14.0 * breach * np.clip(0.4 + n2, 0.0, 1.0)
        W["PripAsh"] += 6.0 * breach * np.clip(0.6 - n2, 0.0, 1.0)
        W["PripGravel"] += 4.0 * breach * np.clip(noise(443, 2) + 0.3, 0.0, 1.0)
        for k, layer, width, mix in (("dam", "PripGravel", 500.0, "PripEarth"), ("boardwalk", "PripEarth", 125.0, "PripPeat"),
                                     ("ford", "PripGravel", 700.0, "PripSand"), ("ramp", "PripGravel", 520.0, "PripEarth")):
            m = on & (kind == k)
            core = m * smoothstep(width, width - 260.0, centre) * (0.55 + 0.45 * np.clip(0.5 + n2 + 0.5 * n1, 0.0, 1.0))
            core = core * end_fade
            W[layer] += 30.0 * core
            W[mix] += 12.0 * m * (1.0 - smoothstep(width, width - 180.0, centre))
        ramp_bank = (kind == "ramp") | ((d > 0.0) & (d < 900.0) & self.ridge_any)
        W["PripGrass"] *= (1.0 - 0.9 * ramp_bank)
        W["PripMoss"] *= (1.0 - 0.9 * ramp_bank)
        W["PripSand"] += 6.0 * ramp_bank           # r8 (R7-4): the earth layer renders olive; banks are sand and mud
        W["PripMud"] += 3.0 * ramp_bank
        # tracks: two-track dirt between the crossings and the bunker groups (paint only)
        track_d = np.full(SHAPE, 1e9)
        for tid in TRACKS:
            track_d = np.minimum(track_d, self.line_dist(np.asarray(self.path(tid).pts, float), 1500.0))
        start_pt = np.asarray(self.feature("start").points[0], float)
        for rid in ("ramp_north", "ramp_prow", "ramp_east"):
            head = self.roads[rid].p[0]
            mid = (head + start_pt) / 2.0 + np.array([350.0, -250.0])
            track_d = np.minimum(track_d, self.line_dist(np.array([head, mid, start_pt]), 1500.0))
        # two narrow ruts (a 4 m two-track, critic R1-6); they stop at the Werk hill feet
        off_hills = np.ones(SHAPE)
        for wk in self.werks.values():
            off_hills = off_hills * smoothstep(1.1, 1.4, wk["rho"])
        rut = smoothstep(80.0, 25.0, np.abs(track_d - 110.0) + 25.0 * n2) * (Z > 60.0) * off_hills
        W["PripEarth"] += 16.0 * rut
        W["PripMud"] += 3.0 * rut * np.clip(n2, 0.0, 1.0)
        self.track_d = track_d
        # the rail yard's ballast bed and its line off the east edge
        rail_d = self.line_dist(np.array(RAIL_LINE), 2500.0)
        yard = np.asarray(RAIL_LINE[0], float)
        yard_w = 400.0 + 650.0 * smoothstep(9000.0, 5000.0, np.hypot(X - yard[0], Y - yard[1]) + 0.0 * n2)
        bed = smoothstep(yard_w + 120.0, yard_w, rail_d + 30.0 * n2) * (Z > 60.0)
        for name in LAYERS:
            W[name] = W[name] * (1.0 - 0.8 * bed)
        W["PripGravel"] += 1.0 * bed                # r8 (R7-8): old cinder ballast, darker
        W["PripMud"] += 11.0 * bed * np.clip(n2 + 0.7, 0.3, 1.0)
        W["PripAsh"] += 2.0 * bed
        self.rail_d = rail_d

        total = sum(W.values()) + 1e-6
        out = {}
        acc = np.zeros(SHAPE)
        for name in LAYERS:
            out[name] = np.round(W[name] / total * 255.0)
            acc += out[name]
        stack = np.stack([out[n] for n in LAYERS])
        top = stack.argmax(axis=0)
        fix = 255.0 - acc
        for i, name in enumerate(LAYERS):
            out[name] = np.clip(out[name] + np.where(top == i, fix, 0.0), 0, 255).astype(np.uint8)
        self.W = out

        m = self.out_masks
        m["water"] = (Z < 0.0)
        m["open_water"] = self.water_sketch
        m["bog"] = self.bog
        m["burn"] = self.burn > 0.4
        m["forest"] = self.forest
        m["road"] = on
        m["road_near"] = d < 250.0
        m["track_near"] = track_d < 400.0
        m["rail_near"] = self.rail_d < 500.0
        m["scarp"] = slope > 30.0
        m["ridge_top"] = self.ridge_top
        m["ridge_any"] = self.ridge_any
        m["crag"] = self.crag
        m["inside"] = self.inside
        m["algae"] = algae > 0.4
        m["heath"] = heath > 0.5
        m["islet"] = self.masks["sluice_islet"]
        for fid, wk in self.werks.items():
            m["werk_" + fid] = wk["hill"]
        m["pond"] = self.level > 0.0
        m["bank_toe"] = (slope > 28.0) & (Z > -70.0) & (Z < 70.0)
        m["bank_face"] = (slope > 24.0) & (Z >= 70.0) & (Z < 500.0) & ~self.ridge_any & (d > 40.0)
        m["road_dist"] = np.clip(d, -2000.0, 6000.0).astype(np.float32)
        m["slope"] = slope.astype(np.float32)
        m["height"] = Z.astype(np.float32)

    # ---- checks -------------------------------------------------------------------------------------------------
    def check(self):
        lines = []
        Z = self.Z
        start = self.rc(self.feature("start").points[0])
        limit = math.tan(math.radians(NAV_MAX_SLOPE_DEG))
        inside_q = self.inside[:-1, :-1]
        loose = np.zeros(SHAPE, dtype=bool)            # at the nav limit: must NOT reach the lakes
        loose[:-1, :-1] = tri_reach(Z, inside_q, start, limit)
        strict = np.zeros(SHAPE, dtype=bool)           # with a margin: must reach every dry place
        strict[:-1, :-1] = tri_reach(Z, inside_q, start, limit * 0.85)
        self.reach_full = loose
        self.reach_strict = strict

        def ok(name, p, want=True):
            got = bool((strict if want else loose)[self.rc(p)])
            lines.append("%-5s reach %-26s %s (z %.0f)" % ("ok" if got == want else "FAIL", name, got, Z[self.rc(p)]))

        for f in self.spec.features:
            if f.kind in ("capture", "enemy_base", "enemy_spawn", "enemy_group", "resource") and f.points:
                ok(f.id, f.points[0])
        for rid in ("ramp_north", "ramp_prow", "ramp_east"):
            road = self.roads[rid]
            ok(rid + " foot", road.p[-1])
        others = [(f.id, p) for f in self.spec.features if f.kind == "resource" or f.raw.get("df") == "station" for p in f.points]
        for fid, wk in self.werks.items():
            ok(fid + " tunnel apron", wk["tunnel"])
            ok(fid + " hill top", wk["centre"])
            near = min((math.hypot(wk["tunnel"][0] - p[0], wk["tunnel"][1] - p[1]) / 100.0, n) for n, p in others)
            lines.append("%-5s %s tunnel mouth: nearest resource/station %s at %.0f m" % ("ok" if near[0] >= 90.0 else "FAIL", fid, near[1], near[0]))
        # identity (critic R0-1): heights relative to the werk base and the paint, sampled on a grid in the Werk frame
        # over the drumlin (normalised radius <= 1.1), must match W1
        ref_h, ref_w = None, None
        for fid, wk in self.werks.items():
            c, yaw = wk["centre"], math.radians(wk["yaw"])
            hs, ws = [], []
            for gx in range(-3600, 3601, 300):
                for gy in range(-2800, 2801, 300):
                    if math.hypot(gx / DRUMLIN_A, gy / DRUMLIN_B) > 1.1:
                        continue
                    x = c[0] + gx * math.cos(yaw) - gy * math.sin(yaw)
                    y = c[1] + gx * math.sin(yaw) + gy * math.cos(yaw)
                    hs.append(self.bilinear(Z, x, y) - wk["base"])          # bilinear: the 1 m grid is turned under
                    ws.append([self.bilinear(self.W[n].astype(np.float64), x, y) for n in LAYERS] if self.W else [0])
            hs, ws = np.array(hs), np.array(ws)
            if ref_h is None:
                ref_h, ref_w = hs, ws
            lines.append("werk %s identity: %d samples, max height difference to w1 %.0f cm (95th pct %.0f cm), "
                         "paint weight difference 95th pct %.0f / 255" % (
                             fid, len(hs), np.abs(hs - ref_h).max(), np.percentile(np.abs(hs - ref_h), 95),
                             np.percentile(np.abs(ws - ref_w), 95)))
        # basins (critic R1-8): with every crossing removed, the start must not reach any other basin's station
        crossing = np.zeros(SHAPE, dtype=bool)
        for rid in ("dam_packlitz", "causeway_kalau", "causeway_island", "dam_lubow", "boardwalk_nischlitz",
                    "boardwalk_hochwald", "ford_hochwald"):
            dd, _ = self.roads[rid].nearest(self.X, self.Y, 2500.0)
            crossing |= dd < self.roads[rid].w.max() + 900.0
        cut = np.zeros(SHAPE, dtype=bool)
        cut[:-1, :-1] = tri_reach(Z, (self.inside & ~crossing)[:-1, :-1], start, limit)
        for fid in ("s2", "s3", "s4", "s5", "w1", "w2", "w3", "w4", "w5"):
            got = bool(cut[self.rc(self.feature(fid).points[0])])
            lines.append("%-5s basins: %s reached from the start with every crossing removed: %s" % ("ok" if not got else "FAIL", fid, got))
        # crag: a point just under the rim, away from the ramps, must not be reachable from the lowland side
        ford_d, _ = self.roads["ford_hochwald"].nearest(self.X, self.Y, 3000.0)
        # wading the first metre of a shore is fine (the blocking cut starts just under the waterline); deeper is a leak
        # a reached quad counts as a leak only when all four corners are under the water by 60 cm or more (a reached
        # quad on a staircase shore has one dry triangle and one wet corner: that is the shore, not a leak)
        qmax = np.maximum(np.maximum(Z[:-1, :-1], Z[:-1, 1:]), np.maximum(Z[1:, :-1], Z[1:, 1:]))
        leak = np.zeros(SHAPE, dtype=bool)
        walk = np.zeros(SHAPE, dtype=bool)          # the boardwalk dykes are walkways over the water, not leaks
        for rid in ("boardwalk_nischlitz", "boardwalk_hochwald"):
            dd, _ = self.roads[rid].nearest(self.X, self.Y, 1500.0)
            walk |= dd < 450.0
        leak[:-1, :-1] = loose[:-1, :-1] & (qmax < self.level[:-1, :-1] - 100.0) & (ford_d[:-1, :-1] > 1300.0) & ~walk[:-1, :-1]
        lines.append("%-5s reached lake cells (off the ford): %d" % ("ok" if leak.sum() == 0 else "FAIL", leak.sum()))
        if leak.sum():
            rr, cc = np.nonzero(leak)
            for i in range(0, len(rr), max(1, len(rr) // 12)):
                lines.append("      leak at x %.0f y %.0f z %.0f" % (self.X[rr[i], cc[i]], self.Y[rr[i], cc[i]], Z[rr[i], cc[i]]))
        # the ridge top must be reachable only through the ramps: fill from the lowland with the ramp corridors cut out
        no_ramps = ~((self.road_kind == "ramp"))
        allowed = self.inside & no_ramps
        low_seed = self.rc(self.feature("s1").points[0])
        low = np.zeros(SHAPE, dtype=bool)
        low[:-1, :-1] = tri_reach(Z, allowed[:-1, :-1], low_seed, limit)
        climb = low & self.ridge_top
        lines.append("%-5s ridge top reached from the lowland without the ramps: %d cells" % ("ok" if climb.sum() == 0 else "FAIL", climb.sum()))
        if climb.sum():
            rr, cc = np.nonzero(climb)
            for i in range(0, len(rr), max(1, len(rr) // 8)):
                lines.append("      climb at x %.0f y %.0f z %.0f" % (self.X[rr[i], cc[i]], self.Y[rr[i], cc[i]], Z[rr[i], cc[i]]))
        for rid, road in self.roads.items():
            g = np.abs(np.gradient(road.h, 100.0)) * 100.0
            lines.append("road %-20s len %4.0f m  h %5.0f..%5.0f  max grade %.1f %%  min half %.0f" % (
                rid, road.length / 100.0, road.h.min(), road.h.max(), g.max(), road.w.min()))
        ins = self.inside
        lines.append("share of play area: water %.1f %%, ridge top %.1f %%, reachable %.1f %%" % (
            100.0 * ((Z < 0) & ins).sum() / ins.sum(), 100.0 * (self.ridge_top & ins).sum() / ins.sum(),
            100.0 * (loose & ins).sum() / ins.sum()))
        start_f = self.feature("start")
        sp = start_f.points[0]
        circle = np.hypot(self.X - sp[0], self.Y - sp[1]) < start_f.raw.get("build_radius", 8500)
        flat = circle & (self.slope < 6.0)
        lines.append("build circle flat share %.1f %%, on the ridge top %.1f %%" % (
            100.0 * flat.sum() / circle.sum(), 100.0 * (circle & self.ridge_top).sum() / circle.sum()))
        text = "\n".join(lines)
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, "check.txt"), "w") as handle:
            handle.write(text)
        print(text)
        return text

    # ---- output -------------------------------------------------------------------------------------------------
    @staticmethod
    def to_grid(a):
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        files = {"height": os.path.join(OUT, "OW_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "OW_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        masks_file = os.path.join(OUT, "OW_masks.npz").replace("\\", "/")
        np.savez_compressed(masks_file, **{k: self.to_grid(np.asarray(v)) for k, v in self.out_masks.items()})
        files["masks"] = masks_file
        spec = self.spec
        regions = [{"id": reg.id, "kind": reg.kind, "label": reg.label, "shape": reg.geom.kind,
                    "centre": [float(v) for v in reg.geom.centre],
                    "points": [[float(a), float(b)] for a, b in reg.geom.pts], "raw": reg.raw} for reg in spec.regions]
        features = []
        for f in spec.features:
            item = {"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                    "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw}
            if getattr(f, "graded", False):
                item["centreline"] = [[float(a), float(b)] for a, b in f.centreline]
            features.append(item)
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
            "levels": {"water": 0.0, "low": LOW, "bog": BOG, "crown": CROWN, "top": TOP, "werk_hill": WERK_HILL,
                       "up_water": UP_WATER},
            "werks": {fid: {"centre": [float(v) for v in wk["centre"]], "base": wk["base"], "yaw": float(wk["yaw"]),
                            "portal_floor": float(wk["portal_floor"]),
                            "tunnel": [float(v) for v in wk["tunnel"]], "top_r": WERK_TOP_R} for fid, wk in self.werks.items()},
            "ponds": [{k: v for k, v in pond.items() if k != "mask"} for pond in self.ponds],
            "rail_line": [list(p) for p in RAIL_LINE],
            "algae_tiles": {"tiles": [], "tile_cm": 400.0, "z": 2.0},     # off: 4 m tiles read as squares (r4)
            "drumlin": {"a": DRUMLIN_A, "b": DRUMLIN_B, "top": DRUMLIN_TOP, "foot": DRUMLIN_FOOT},
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths, "roads": roads,
        }
        with open(os.path.join(OUT, "OW_layout.json"), "w") as handle:
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
        img[..., 1] = np.clip((self.Z + 300.0) / 2400.0 * 255, 0, 255)
        img[..., 2] = np.where(self.reach_full, 160, 0)
        img[..., 0] = np.clip(self.slope * 4, 0, 255)
        img[self.slope > NAV_MAX_SLOPE_DEG] = (255, 0, 255)
        Image.fromarray(img).save(os.path.join(OUT, "preview_slope.png"))


def main():
    gen = Gen()
    gen.build_heights()
    gen.build_layers()
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
