"""Heights, Berlin landscape layers and dressing for Field_BerlinKanal (convoy field map, Berlin, owner pick B "Kanalufer").

Design source: MapDesigner/ConvoyMaps/Berlin/convoy_berlin_B.layout.json (owner pick 2026-11-25, 680 x 680 m). Writes
Saved/FieldMaps/BerlinKanal/terrain/{BKU_height.raw, BKU_layer_<name>.raw, BKU_layout.json, BKU_dress.json,
preview*.png, check.txt}. bku_map_build.py (in a throwaway editor) builds the map from these files.

    python Source/Tools/FieldMaps/bku_terrain_gen.py

Terrain: flat ruined city on MI_RTS_Landscape_Berlin, a 24 m canal in a 4.2 m deep channel with quay walls past the
44 deg nav slope (nothing walks into it). The road bridge, the freight-line bridge and the lock footbridge are earth
dams flush with the banks (decks and parapets are meshes on top), so every crossing is plain landscape navmesh.
Dressing: the Ger_Berlin ruin kit (BP_BerlinRuin_E*/B5, one destructible actor per piece) on the drawing's blocks,
their footprint skirts, rubble heap cover, dead trees, cover pieces at the ambush spots, wrecks and crater decals.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "MapDesigner"))
from mapdesigner.grid import Raster, Terrain  # noqa: E402
from mapdesigner.spec import Spec, dist_to_polyline  # noqa: E402

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "ConvoyMaps", "Berlin", "convoy_berlin_B.layout.json")
OUT = os.path.join(ROOT, "Saved", "FieldMaps", "BerlinKanal", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/Field_BerlinKanal"
FOOT_TABLE = os.path.join(ROOT, "Source", "Tools", "BerlinMap", "ruinkit", "footprint_table.json")

COMPONENTS = 14                         # 882 m: the 680 m play area plus 101 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1              # 883 vertices a side
SIZE = COMPONENTS * QUADS * QUAD_CM     # 88 200 cm
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

LAYERS = ["BerlinRubble", "BerlinAsh", "BerlinCobble", "BerlinGlass", "BerlinScrub", "BerlinMud"]
NAV_MAX_SLOPE_DEG = 44.0

CANAL_HALF = 1200.0                     # water 24 m
QUAY_FOOT = 1060.0                      # the quay wall runs from 12 m to 10.6 m off the axis
CANAL_BED_Z = -420.0
WATER_Z = -150.0
# earth dams under the crossings: (feature id, half width cm). Decks/parapets are meshes on top.
DAMS = (("road_bridge", 1000.0), ("rail_bridge", 560.0), ("weir", 260.0))
RELIEF_CM = 22.0
MARGIN_RISE = 380.0

# Berlin ruin kit pieces: (half extent along local X = depth, along local Y = frontage, centre offset x, y), from
# berlin_dress_gen.Dress.KIT (Unreal bounds, rubble included). Local X faces the street.
KIT = {
    "E1_CornerTower": (928.0, 1078.0, 147.0, -147.0), "E2_Courtyard": (1322.0, 930.0, 91.0, 0.0),
    "E4_StubRing_A": (475.0, 528.0, 0.0, 0.0), "E4_StubRing_B": (475.0, 528.0, 0.0, 0.0),
    "E5_Shell1St_A": (528.0, 630.0, 0.0, 0.0), "E5_Shell1St_B": (529.0, 628.0, 0.0, 0.0),
    "E6_HeapFin_A": (476.0, 530.0, 0.0, 0.0), "E6_HeapFin_B": (477.0, 530.0, 0.0, 0.0),
    "E7_HalfHouse_A": (331.0, 425.0, 0.0, 0.0), "E7_HalfHouse_B": (324.0, 425.0, 0.0, 0.0),
    "B5_CornerL": (422.0, 431.0, 0.0, 0.0),
}
MAIN_ROAD_CLEAR = 1450.0                # footprint edge to main-road centre: 8 m road + flank escorts 9 m off it
BRANCH_CLEAR = 1000.0
STREET_CLEAR = 700.0
TRACK_CLEAR = 500.0
RAIL_CLEAR = 650.0
DEST = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/"
COVER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCover_"
HEAP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCoverHeap_"


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
    return box_blur(box_blur(mask.astype(np.float64), r), r)


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


def bearing_dir(b):
    return math.cos(math.radians(b)), math.sin(math.radians(b))


def extend(pts, reach=15000.0):
    pts = np.asarray(pts, float)
    a = pts[0] + (pts[0] - pts[1]) / np.linalg.norm(pts[0] - pts[1]) * reach
    b = pts[-1] + (pts[-1] - pts[-2]) / np.linalg.norm(pts[-1] - pts[-2]) * reach
    return np.vstack([a, pts, b])


class Gen:
    def __init__(self):
        os.makedirs(OUT, exist_ok=True)
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
        self.E = self.spec.elevation.raster(raster) * 100.0
        self.canal = extend(self.spec.by_id["canal"].geom.pts)
        self.rng = np.random.default_rng(1953)

    # ---- helpers ------------------------------------------------------------------------------------------------
    def region(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for rid in ids:
            m |= self.masks[rid]
        return m

    def feature(self, fid):
        return self.spec.by_id[fid]

    def pt(self, fid):
        return tuple(self.spec.by_id[fid].points[0])

    def path(self, pid):
        return self.spec.by_id[pid]

    def dist_line(self, pts, reach=9000.0):
        pts = np.asarray(pts, float)
        flat = np.c_[self.X.ravel(), self.Y.ravel()]
        lo = pts.min(axis=0) - reach
        hi = pts.max(axis=0) + reach
        near = (flat[:, 0] >= lo[0]) & (flat[:, 0] <= hi[0]) & (flat[:, 1] >= lo[1]) & (flat[:, 1] <= hi[1])
        d = np.full(flat.shape[0], 1e9)
        d[near] = dist_to_polyline(flat[near], pts)
        return d.reshape(SHAPE)

    def dist_point(self, p):
        return np.hypot(self.X - p[0], self.Y - p[1])

    def rc(self, x, y):
        return self.raster.to_rc((x, y))

    def z(self, x, y):
        r, c = self.rc(x, y)
        r = min(max(r, 0), V - 1)
        c = min(max(c, 0), V - 1)
        return float(self.Z[r, c])

    def dam_frame(self, fid):
        f = self.feature(fid)
        p = np.asarray(f.points[0], float)
        b = float(f.raw.get("bearing", 0.0))
        d = np.array(bearing_dir(b))
        return p, d

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        sp = self.spec
        self.canal_d = self.dist_line(self.canal, reach=CANAL_HALF + 6000.0)
        self.road_d = self.dist_line(self.path("main_road").pts)
        self.branch_d = self.dist_line(self.path("branch_road").pts)
        street = np.full(SHAPE, 1e9)
        track = np.full(SHAPE, 1e9)
        for p in sp.paths:
            if p.id in ("main_road", "branch_road"):
                continue
            if p.kind == "road":
                street = np.minimum(street, self.dist_line(p.pts))
            elif p.kind == "track":
                track = np.minimum(track, self.dist_line(p.pts))
        self.street_d, self.track_d = street, track
        self.rail_d = self.dist_line(self.path("freight_line").pts)

        flat_zone = soft(self.region("start_yard", "freight_yard", "quay_s", "warehouse") | (self.road_d < 1200.0) |
                         (self.branch_d < 800.0) | (street < 700.0) | (self.rail_d < 600.0), 4)
        relief = fbm(11, 70) * RELIEF_CM + fbm(23, 22) * 8.0
        Z = self.E + relief * (1.0 - np.clip(flat_zone, 0.0, 1.0))
        # collapsed blocks: low rubble mounds; craters: dish + rim
        rough_blocks = np.zeros(SHAPE, dtype=bool)
        for r in sp.regions:
            if r.id.startswith("blk_") and r.kind == "rough":
                rough_blocks |= self.masks[r.id]
        Z += 45.0 * soft(rough_blocks, 3) * (0.6 + 0.4 * fbm(31, 8))
        self.craters = []
        for r in sp.regions:
            if r.raw.get("look") == "crater":
                c = r.geom.centre
                rad = math.sqrt(abs(r.geom.area_cm2()) / math.pi)
                dd = self.dist_point(c) / rad
                Z += -90.0 * (1.0 - smoothstep(0.0, 0.9, dd)) + 28.0 * np.exp(-((dd - 1.05) / 0.22) ** 2)
                self.craters.append((float(c[0]), float(c[1]), rad))
        # canal: quay walls past the nav slope, flat bed under the water
        d = self.canal_d
        wall_t = np.clip((CANAL_HALF - d) / (CANAL_HALF - QUAY_FOOT), 0.0, 1.0)
        canal_z = CANAL_BED_Z * wall_t
        canal_z = np.where(d < QUAY_FOOT, CANAL_BED_Z + fbm(41, 12) * 10.0, canal_z)
        in_canal = d < CANAL_HALF
        # earth dams: the crossings are landscape flush with the banks
        dam = np.zeros(SHAPE, dtype=bool)
        self.dams = {}
        for fid, half in DAMS:
            p, dv = self.dam_frame(fid)
            along = (self.X - p[0]) * dv[0] + (self.Y - p[1]) * dv[1]
            across = -(self.X - p[0]) * dv[1] + (self.Y - p[1]) * dv[0]
            m = (np.abs(across) <= half) & (np.abs(along) <= CANAL_HALF + 2500.0) & (d < CANAL_HALF + 50.0)
            dam |= m
            self.dams[fid] = {"centre": [float(p[0]), float(p[1])], "dir": [float(dv[0]), float(dv[1])],
                              "half_width_cm": half, "bearing": float(self.feature(fid).raw.get("bearing", 0.0))}
        self.dam = dam
        bank_level = np.where(np.abs(Z) < 200.0, 0.0, Z)
        Z = np.where(in_canal & ~dam, canal_z, Z)
        Z = np.where(dam, bank_level * 0.0, Z)
        self.in_canal = in_canal & ~dam
        # camera margin: a low rubble rise past the play edge, the canal and the road ends run on out
        dx = np.maximum(np.abs(self.X) - sp.xmax, 0.0)
        dy = np.maximum(np.abs(self.Y) - sp.ymax, 0.0)
        d_edge = np.hypot(dx, dy) / 100.0
        keep_open = np.maximum(1.0 - smoothstep(CANAL_HALF + 400.0, CANAL_HALF + 3000.0, d),
                               1.0 - smoothstep(1500.0, 4000.0, self.road_d))
        margin = MARGIN_RISE * smoothstep(8.0, 70.0, d_edge) * (0.6 + 0.4 * fbm(61, 50)) * (1.0 - keep_open)
        self.Z = Z + margin * (~in_canal | dam)
        self.slope = slope_deg(self.Z)

    # ---- dressing -----------------------------------------------------------------------------------------------
    def build_dress(self):
        sp = self.spec
        self.foot_table = json.load(open(FOOT_TABLE))["pieces"]
        self.ruins, self.feet, self.actors, self.decals, self.rects = [], [], [], [], []
        self.ruin_mask = np.zeros(SHAPE, dtype=bool)
        lines = [(np.asarray(self.path("main_road").pts, float), MAIN_ROAD_CLEAR),
                 (np.asarray(self.path("branch_road").pts, float), BRANCH_CLEAR),
                 (np.asarray(self.path("freight_line").pts, float), RAIL_CLEAR)]
        for p in sp.paths:
            if p.id in ("main_road", "branch_road", "freight_line"):
                continue
            lines.append((np.asarray(p.pts, float), STREET_CLEAR if p.kind == "road" else TRACK_CLEAR))
        self.clear_lines = lines
        keep = [(self.pt("hq"), 6800.0), (self.pt("director"), 4800.0), (self.pt("battery_dest"), 1800.0),
                (self.pt("radio_post"), 1200.0), (self.pt("fork"), 1400.0)]
        for f in sp.features:
            if f.raw.get("strength_slot"):
                keep.append((tuple(f.points[0]), 1300.0))
            if f.raw.get("convoy") == "ambush":
                keep.append((tuple(f.points[0]), 700.0))
        for fid, _ in DAMS:
            keep.append((self.pt(fid), 2600.0))
        self.keep = keep
        bx = sp.xmax
        for r in sp.regions:
            if not r.id.startswith("blk_"):
                continue
            self.fill_block(r.id, r.geom.pts, r.kind)
        self.fill_special()
        self.backdrop()
        self.trees()
        self.cover_at_ambush()
        self.wrecks()
        self.crater_decals()
        self.rails()
        print("dress: ruins %d, feet %d, actors %d, decals %d" % (len(self.ruins), len(self.feet), len(self.actors),
                                                                   len(self.decals)))

    def rect_points(self, cx, cy, hx, hy, yaw):
        ca, sa = math.cos(yaw), math.sin(yaw)
        out = []
        for u, v in ((0, 0), (1, 1), (1, -1), (-1, 1), (-1, -1), (1, 0), (-1, 0), (0, 1), (0, -1)):
            lx, ly = u * hx, v * hy
            out.append((cx + lx * ca - ly * sa, cy + lx * sa + ly * ca))
        return out

    def rect_ok(self, cx, cy, hx, hy, yaw, backdrop=False):
        pts = self.rect_points(cx, cy, hx, hy, yaw)
        arr = np.asarray(pts)
        if not backdrop:
            if np.any(np.abs(arr[:, 0]) > self.spec.xmax - 200.0) or np.any(np.abs(arr[:, 1]) > self.spec.ymax - 200.0):
                return False
        else:
            lim = SIZE / 2.0 - 800.0
            if np.any(np.abs(arr[:, 0]) > lim) or np.any(np.abs(arr[:, 1]) > lim):
                return False
        if dist_to_polyline(arr, self.canal).min() < CANAL_HALF + 450.0:
            return False
        for line, clear in self.clear_lines:
            if dist_to_polyline(arr, line).min() < clear:
                return False
        for (q, rad) in self.keep:
            if np.min(np.hypot(arr[:, 0] - q[0], arr[:, 1] - q[1])) < rad:
                return False
        r_me = math.hypot(hx, hy)
        for (ox, oy, orad, ohx, ohy, oyaw) in self.rects:
            if math.hypot(cx - ox, cy - oy) > r_me + orad + 50.0:
                continue
            if self.rects_overlap((cx, cy, hx + 60.0, hy + 60.0, yaw), (ox, oy, ohx, ohy, oyaw)):
                return False
        if self.berg_top(cx, cy):
            return False
        return True

    @staticmethod
    def rects_overlap(a, b):
        """Separating-axis test of two oriented rectangles (cx, cy, hx, hy, yaw)."""
        def axes(r):
            return [(math.cos(r[4]), math.sin(r[4])), (-math.sin(r[4]), math.cos(r[4]))]

        def corners(r):
            ca, sa = math.cos(r[4]), math.sin(r[4])
            return [(r[0] + u * r[2] * ca - v * r[3] * sa, r[1] + u * r[2] * sa + v * r[3] * ca)
                    for u, v in ((1, 1), (1, -1), (-1, -1), (-1, 1))]

        ca_, cb_ = corners(a), corners(b)
        for ax in axes(a) + axes(b):
            pa = [p[0] * ax[0] + p[1] * ax[1] for p in ca_]
            pb = [p[0] * ax[0] + p[1] * ax[1] for p in cb_]
            if max(pa) < min(pb) or max(pb) < min(pa):
                return False
        return True

    def berg_top(self, x, y):
        r, c = self.rc(x, y)
        if not (0 <= r < V and 0 <= c < V):
            return False
        return self.masks["berg"][r, c]

    def ruin(self, piece, fx, fy, yaw, kind):
        """Footprint centre (fx, fy), yaw in radians (local X = street front). Returns True when placed."""
        hx, hy, ox, oy = KIT[piece]
        backdrop = kind == "backdrop"
        if not self.rect_ok(fx, fy, hx, hy, yaw, backdrop):
            return False
        ca, sa = math.cos(yaw), math.sin(yaw)
        px, py = fx - (ox * ca - oy * sa), fy - (ox * sa + oy * ca)
        z = self.z(px, py)
        shade = float(self.rng.uniform(0.82, 1.08))
        warm = float(self.rng.uniform(-0.035, 0.035))
        yaw_deg = math.degrees(yaw) % 360.0
        self.ruins.append({"piece": piece, "x": round(px, 1), "y": round(py, 1), "z": round(z, 1),
                           "yaw": round(yaw_deg, 2), "kind": kind,
                           "tint": [round(shade + warm, 3), round(shade, 3), round(shade - warm, 3)],
                           "soot": round(float(self.rng.uniform(0.55, 1.35)), 3)})
        self.rects.append((fx, fy, math.hypot(hx, hy), hx, hy, yaw))
        if piece in self.foot_table:
            bucket = int(self.rng.integers(0, 4))
            for it in self.foot_table[piece]["footprints"]:
                self.feet.append([it["mesh"], round(px + it["x"] * ca - it["y"] * sa, 1),
                                  round(py + it["x"] * sa + it["y"] * ca, 1), round(z, 1),
                                  round((yaw_deg + it["yaw"]) % 360.0, 2), 1.0, bucket])
        r, c = self.rc(fx, fy)
        rad = int(max(hx, hy) / QUAD_CM)
        if 0 <= r < V and 0 <= c < V:
            self.ruin_mask[max(r - rad, 0):r + rad + 1, max(c - rad, 0):c + rad + 1] = True
        return True

    def pick(self, kind):
        ab = "A" if self.rng.random() < 0.5 else "B"
        r = self.rng.random()
        if kind == "rough":
            return ("E6_HeapFin_" if r < 0.55 else "E4_StubRing_") + ab
        if kind == "rock":
            return "E2_Courtyard" if r < 0.5 else "E1_CornerTower"
        if kind == "fields":
            return "E7_HalfHouse_" + ab
        if r < 0.08:
            return "E2_Courtyard"
        if r < 0.38:
            return "E5_Shell1St_" + ab
        if r < 0.70:
            return "E7_HalfHouse_" + ab
        if r < 0.86:
            return "E6_HeapFin_" + ab
        return "E4_StubRing_" + ab

    def fill_block(self, rid, poly, kind, backdrop=False):
        poly = np.asarray(poly, float)
        cen = poly.mean(axis=0)
        n = len(poly)
        placed = 0
        tag = "backdrop" if backdrop else kind
        # corners first: a corner tower or an L on about half the town corners
        if kind in ("town", "rock"):
            for i in range(n):
                if self.rng.random() > (0.5 if kind == "town" else 0.8):
                    continue
                p = poly[i]
                inward = cen - p
                inward /= np.linalg.norm(inward)
                piece = "E1_CornerTower" if (kind == "rock" or self.rng.random() < 0.5) else "B5_CornerL"
                hx, hy, _, _ = KIT[piece]
                f = p + inward * (math.hypot(hx, hy) * 0.95)
                # the tower's street front faces out along the corner's outward diagonal
                yaw = math.atan2(-inward[1], -inward[0])
                if self.ruin(piece, f[0], f[1], yaw, tag):
                    placed += 1
        for i in range(n):
            a, b = poly[i], poly[(i + 1) % n]
            t = b - a
            L = float(np.linalg.norm(t))
            t /= L
            nrm = np.array([t[1], -t[0]])
            if np.dot(nrm, (a + b) / 2.0 - cen) < 0:
                nrm = -nrm
            yaw = math.atan2(nrm[1], nrm[0])
            s = float(self.rng.uniform(100.0, 600.0))
            fails = 0
            while s < L - 300.0 and fails < 6:
                piece = self.pick(kind)
                hx, hy, _, _ = KIT[piece]
                if s + 2 * hy > L - 100.0:
                    piece = "E7_HalfHouse_" + ("A" if self.rng.random() < 0.5 else "B")
                    hx, hy, _, _ = KIT[piece]
                    if s + 2 * hy > L - 100.0:
                        break
                f = a + t * (s + hy) - nrm * (hx + 60.0)
                if self.ruin(piece, f[0], f[1], yaw, tag):
                    placed += 1
                    s += 2 * hy + float(self.rng.uniform(80.0, 450.0))
                else:
                    fails += 1
                    s += 350.0
        # courtyard / lot: rubble heap cover, a cover piece now and then
        if backdrop:
            return placed
        heaps = {"town": 2, "rough": 5, "rock": 1, "fields": 1}[kind]
        for _ in range(heaps):
            q = cen + self.rng.uniform(-0.3, 0.3, 2) * (poly.max(axis=0) - poly.min(axis=0))
            self.heap(q[0], q[1], large=kind == "rough")
        if kind == "fields":
            for _ in range(4):
                q = cen + self.rng.uniform(-0.4, 0.4, 2) * (poly.max(axis=0) - poly.min(axis=0))
                self.tree(q[0], q[1])
        return placed

    def heap(self, x, y, large=False):
        key = self.rng.choice(["Collapse_L", "Collapse_S"] if large else ["Brick", "Block", "Concrete_A", "Concrete_B",
                                                                           "Concrete_C", "Collapse_S"])
        if not self.rect_ok(x, y, 450.0, 450.0, 0.0):
            return False
        s = float(self.rng.uniform(0.85, 1.15))
        self.actor("Heap_%s_%03d" % (key, len(self.actors)), "Cover/Heaps", x, y, self.z(x, y) - 5.0,
                   float(self.rng.uniform(0, 360)), (s, s, s), HEAP + key, tint=True)
        self.rects.append((x, y, 450.0, 320.0, 320.0, 0.0))
        return True

    def tree(self, x, y, willow=None, check=True):
        if check and not self.rect_ok(x, y, 120.0, 120.0, 0.0):
            return False
        willow = (self.rng.random() < 0.25) if willow is None else willow
        kind = "BP_Scorch_Dest_DeadWillow" if willow else "BP_Scorch_Dest_DeadTree"
        s = float(self.rng.uniform(0.85, 1.25))
        self.actor("Tree_%04d" % len(self.actors), "Trees", x, y, self.z(x, y) - 10.0, float(self.rng.uniform(0, 360)),
                   (s, s, s), DEST + kind)
        self.rects.append((x, y, 170.0, 120.0, 120.0, 0.0))
        return True

    def actor(self, label, folder, x, y, z, yaw, scale, bp, tint=False, owner=None):
        entry = {"label": label, "folder": folder, "x": round(float(x), 1), "y": round(float(y), 1),
                 "z": round(float(z), 1), "yaw": round(float(yaw), 2), "scale": [round(float(v), 3) for v in scale],
                 "bp": bp}
        if tint:
            entry["tint"] = int(self.rng.integers(0, 4))
        if owner is not None:
            entry["owner"] = owner
        self.actors.append(entry)
        return entry

    def fill_special(self):
        """Warehouse block (big courtyard ruins), south quay (crates, crane), freight yard edge, park trees."""
        sp = self.spec
        w = sp.by_id["warehouse"].geom
        self.fill_block("warehouse", w.pts, "rock")
        q = sp.by_id["quay_s"].geom
        cen = np.asarray(q.pts).mean(axis=0)
        for k, (dx, dy) in enumerate(((-3000.0, 500.0), (1500.0, 300.0), (5000.0, -200.0))):
            x, y = cen[0] + dx, cen[1] + dy
            if self.rect_ok(x, y, 400.0, 300.0, 0.0):
                self.actor("Quay_Crates_%d" % k, "Quay", x, y, self.z(x, y), float(self.rng.uniform(0, 360)), (1, 1, 1),
                           DEST + "Airport/BP_Airport_Dest_CrateStack01")
                self.rects.append((x, y, 500.0, 400.0, 300.0, 0.0))
        x, y = cen[0] - 6500.0, cen[1] + 1200.0
        self.actor("Quay_Crane", "Quay", x, y, self.z(x, y), 95.0, (1, 1, 1), DEST + "Soviet/BP_Dest_SovietCrane_1")
        park = sp.by_id["park"].geom
        pts = np.asarray(park.pts)
        lo, hi = pts.min(axis=0), pts.max(axis=0)
        from mapdesigner.spec import Geom  # noqa: F401
        count = 0
        for _ in range(900):
            x, y = self.rng.uniform(lo[0], hi[0]), self.rng.uniform(lo[1], hi[1])
            if not park.contains((x, y)):
                continue
            if self.tree(x, y):
                count += 1
            if count >= 70:
                break
        allot = sp.by_id["allot_n"].geom
        pts = np.asarray(allot.pts)
        lo, hi = pts.min(axis=0), pts.max(axis=0)
        for _ in range(500):
            x, y = self.rng.uniform(lo[0], hi[0]), self.rng.uniform(lo[1], hi[1])
            if allot.contains((x, y)) and self.rng.random() < 0.35:
                self.tree(x, y)
        # the flak hill: a few dead trees on its flanks, rubble heaps round the foot
        bc = sp.by_id["berg"].geom.centre
        for k in range(10):
            a = 2 * math.pi * k / 10 + self.rng.uniform(-0.2, 0.2)
            rr = self.rng.uniform(4300.0, 5600.0)
            self.heap(bc[0] + math.cos(a) * rr, bc[1] + math.sin(a) * rr, large=self.rng.random() < 0.4)

    def backdrop(self):
        """Ruined blocks in the camera margin (past the boundary volumes): the city carries on to the horizon."""
        sp = self.spec
        step_x, step_y = 4600.0 + 1300.0, 3600.0 + 1300.0
        lim = SIZE / 2.0 - 900.0
        count = 0
        x = -lim
        while x <= lim:
            y = -lim
            while y <= lim:
                cx, cy = x + self.rng.uniform(-300, 300), y + self.rng.uniform(-300, 300)
                y += step_y
                if max(abs(cx) - sp.xmax, abs(cy) - sp.ymax) < 2800.0:
                    continue
                poly = [[cx + 2300, cy + 1800], [cx + 2300, cy - 1800], [cx - 2300, cy - 1800], [cx - 2300, cy + 1800]]
                count += self.fill_block("bd", poly, "town" if self.rng.random() < 0.7 else "rough", backdrop=True)
            x += step_x
        print("backdrop ruins %d" % count)

    def trees(self):
        """Dead willows along both canal banks, dead street trees along the side streets."""
        canal = self.canal
        s_tot = float(np.sum(np.hypot(*np.diff(canal, axis=0).T)))
        s = 0.0
        cum = np.concatenate([[0.0], np.cumsum(np.hypot(*np.diff(canal, axis=0).T))])
        while s < s_tot:
            i = min(int(np.searchsorted(cum, s, side="right")) - 1, len(canal) - 2)
            t = (s - cum[i]) / max(cum[i + 1] - cum[i], 1e-6)
            p = canal[i] + t * (canal[i + 1] - canal[i])
            tv = canal[i + 1] - canal[i]
            tv /= np.linalg.norm(tv)
            nv = np.array([tv[1], -tv[0]])
            for side in (-1, 1):
                if self.rng.random() < 0.55:
                    q = p + nv * side * self.rng.uniform(CANAL_HALF + 500.0, CANAL_HALF + 1300.0)
                    if abs(q[0]) < self.spec.xmax - 300 and abs(q[1]) < self.spec.ymax - 300:
                        self.tree(q[0], q[1], willow=self.rng.random() < 0.6)
            s += float(self.rng.uniform(1400.0, 2600.0))
        for p in self.spec.paths:
            if p.kind != "road" or p.id in ("main_road", "branch_road"):
                continue
            pts = np.asarray(p.pts, float)
            for a, b in zip(pts[:-1], pts[1:]):
                L = float(np.linalg.norm(b - a))
                tv = (b - a) / L
                nv = np.array([tv[1], -tv[0]])
                s = 600.0
                while s < L:
                    if self.rng.random() < 0.45:
                        q = a + tv * s + nv * self.rng.choice([-1, 1]) * (STREET_CLEAR + 200.0)
                        self.tree(q[0], q[1], willow=False)
                    s += float(self.rng.uniform(1600.0, 2600.0))

    def cover_at_ambush(self):
        """Two or three Berlin cover pieces (wall runs, breastworks, slabs) at every ambush spot, facing the road."""
        road = np.asarray(self.path("main_road").pts, float)
        kinds = ["01_WallRun", "02_CornerL", "04_ToppledSlab", "06_Breastwork", "08_FacadePanels", "10_MasonryBlock"]
        for f in self.spec.features:
            if f.raw.get("convoy") != "ambush":
                continue
            p = np.asarray(f.points[0], float)
            # direction to the nearest road point
            best, q = 1e18, p
            for a, b in zip(road[:-1], road[1:]):
                e = b - a
                tt = max(0.0, min(1.0, float(np.dot(p - a, e) / np.dot(e, e))))
                c = a + tt * e
                if np.linalg.norm(c - p) < best:
                    best, q = float(np.linalg.norm(c - p)), c
            fwd = (q - p) / max(best, 1.0)
            side = np.array([-fwd[1], fwd[0]])
            yaw = math.degrees(math.atan2(fwd[1], fwd[0]))
            for k, off in enumerate((-650.0, 0.0, 650.0)):
                x, y = p + side * off + fwd * 250.0
                kind = kinds[int(self.rng.integers(0, len(kinds)))]
                self.actor("Cover_%s_%s" % (f.id, k), "Cover/Ambush", x, y, self.z(x, y), yaw + 90.0, (1, 1, 1),
                           COVER + kind, tint=True)

    def wrecks(self):
        bps = ["Military/DestTanks/BP_Env_DestroyedPzIII", "Military/DestTanks/BP_Env_DestroyedT34",
               "Military/DestTanks/BP_Env_DestroyedPzII", "Military/DestTanks/BP_Env_Destroyed_KV1E",
               "Military/DestTanks/BP_Env_DestroyedPz38T"]
        spots = [(-9500.0, -14500.0), (17000.0, -9500.0), (-20500.0, 10500.0), (-3000.0, 21000.0), (25000.0, 9000.0),
                 (12500.0, 2500.0)]
        k = 0
        for x, y in spots:
            if self.rect_ok(x, y, 450.0, 250.0, 0.0):
                self.actor("Wreck_%d" % k, "Wrecks", x, y, self.z(x, y), float(self.rng.uniform(0, 360)), (1, 1, 1),
                           DEST + bps[k % len(bps)])
                self.rects.append((x, y, 520.0, 450.0, 250.0, 0.0))
                k += 1
        print("wrecks %d" % k)

    def crater_decals(self):
        for x, y, rad in self.craters:
            self.decals.append({"mat": "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_%02d" %
                                int(self.rng.integers(1, 7)), "kind": "crater", "x": round(x, 1), "y": round(y, 1),
                                "z": round(self.z(x, y), 1), "size": round(rad * 2.4, 1),
                                "yaw": round(float(self.rng.uniform(0, 360)), 1)})
        # debris along the main road verges
        road = np.asarray(self.path("main_road").pts, float)
        for a, b in zip(road[:-1], road[1:]):
            L = float(np.linalg.norm(b - a))
            tv = (b - a) / L
            nv = np.array([tv[1], -tv[0]])
            s = 500.0
            while s < L:
                q = a + tv * s + nv * self.rng.choice([-1, 1]) * self.rng.uniform(500.0, 1100.0)
                self.decals.append({"mat": "/Game/RTS_Survival/Materials/Decals/Concrete/MI_RTS_Decal_Concrete_Debris_0%d"
                                    % int(self.rng.choice([1, 2, 3, 5])), "kind": "debris", "x": round(float(q[0]), 1),
                                    "y": round(float(q[1]), 1), "z": round(self.z(q[0], q[1]), 1),
                                    "size": round(float(self.rng.uniform(600.0, 1100.0)), 1),
                                    "yaw": round(float(self.rng.uniform(0, 360)), 1)})
                s += float(self.rng.uniform(1500.0, 3000.0))

    def rails(self):
        self.rail_line = [list(map(float, p)) for p in extend(self.path("freight_line").pts, 6000.0)]

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        sp = self.spec
        n_big, n_mid, n_fine = fbm(81, 60.0, 3), fbm(82, 18.0, 2), noise(83, 5.0)
        stain = soft(ndimage.binary_dilation(self.ruin_mask, iterations=4), 4)
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        W["BerlinRubble"] += 1.0
        ash = np.clip(0.25 + 0.4 * n_big + 0.2 * n_mid, 0.0, 1.0)
        blocks_rough = np.zeros(SHAPE, dtype=bool)
        blocks_town = np.zeros(SHAPE, dtype=bool)
        lots = np.zeros(SHAPE, dtype=bool)
        for r in sp.regions:
            if not r.id.startswith("blk_"):
                continue
            if r.kind == "rough":
                blocks_rough |= self.masks[r.id]
            elif r.kind in ("town", "rock"):
                blocks_town |= self.masks[r.id]
            elif r.kind == "fields":
                lots |= self.masks[r.id]
        ash = np.maximum(ash, 0.75 * soft(blocks_rough | self.region("blast", "berg"), 6))
        ash = np.clip(ash - 0.5 * stain, 0.0, 1.0)
        W["BerlinAsh"] += 1.4 * ash
        patch = smoothstep(-0.75, -0.15, 0.7 * fbm(84, 4.0, 2) + 0.5 * n_mid)
        road = smoothstep(1050.0, 760.0, self.road_d)
        side = smoothstep(560.0, 400.0, self.street_d) * patch
        branch = smoothstep(620.0, 450.0, self.branch_d)
        yards = smoothstep(0.3, 0.7, soft(self.region("freight_yard", "quay_s", "warehouse"), 5) + 0.3 * n_mid)
        cobble = np.maximum.reduce([road * np.clip(patch + 0.35, 0.0, 1.0), side, branch * patch, 0.85 * yards * patch])
        W["BerlinCobble"] += 3.0 * cobble
        start = soft(self.region("start_yard"), 5)
        W["BerlinCobble"] += 1.2 * start * patch
        glass = soft(self.region("blast"), 6) * np.clip(0.4 + 0.5 * n_mid + 0.3 * n_fine, 0.0, 1.0)
        W["BerlinGlass"] += 1.6 * glass
        scrub = np.maximum.reduce([soft(self.region("park", "allot_n"), 8) * np.clip(0.8 + 0.5 * n_mid, 0.0, 1.0),
                                   0.8 * soft(lots, 4),
                                   0.45 * smoothstep(0.2, 0.6, fbm(86, 16.0, 2)) * (1.0 - cobble) * stain])
        W["BerlinScrub"] += 1.6 * scrub
        mud = smoothstep(CANAL_HALF + 900.0, CANAL_HALF + 150.0, self.canal_d) * (1.0 - cobble)
        mud = np.maximum(mud, smoothstep(350.0, 150.0, self.track_d))
        for x, y, rad in self.craters:
            mud = np.maximum(mud, 0.9 * smoothstep(0.8, 0.25, self.dist_point((x, y)) / rad))
        mud = np.maximum(mud, 0.35 * stain * np.clip(0.6 + 0.8 * n_fine, 0.0, 1.0))
        mud = np.maximum(mud, 1.0 * self.in_canal)
        W["BerlinMud"] += 1.6 * mud
        total = sum(W.values()) + 1e-9
        out, acc = {}, np.zeros(SHAPE)
        for name in LAYERS:
            out[name] = np.floor(W[name] / total * 255.0)
            acc += out[name]
        out["BerlinRubble"] += 255.0 - acc
        self.W = {name: np.clip(out[name], 0, 255).astype(np.uint8) for name in LAYERS}

    # ---- checks -------------------------------------------------------------------------------------------------
    def reach(self, start, blocked):
        lab, _ = ndimage.label(~blocked)
        r, c = self.rc(*start)
        return lab == lab[r, c]

    def check(self):
        steep = self.slope > NAV_MAX_SLOPE_DEG
        blocked = steep | ~self.inside
        lines = []
        z = self.Z[self.inside]
        lines.append("heights inside: %.0f..%.0f cm; landscape %.0f..%.0f cm" % (z.min(), z.max(), self.Z.min(),
                                                                                 self.Z.max()))
        full = self.reach(self.pt("hq"), blocked)
        for f in self.spec.features:
            if not f.points or f.kind in ("bridge",):
                continue
            p = f.points[0]
            r, c = self.rc(*p)
            lines.append("reach from hq %-15s %s  z %5.0f" % (f.id, bool(full[r, c]), self.Z[r, c]))
        for fid, half in DAMS:
            p = self.dams[fid]["centre"]
            r, c = self.rc(*p)
            lines.append("dam %-12s on reach %s, z %.0f, slope at centre %.1f" % (fid, bool(full[r, c]), self.Z[r, c],
                                                                                  self.slope[r, c]))
        quay = (self.canal_d > QUAY_FOOT + 30.0) & (self.canal_d < CANAL_HALF - 30.0) & self.inside & ~self.dam
        lines.append("quay wall slope deg p5 %.1f p50 %.1f (must stay > %.0f)" % (np.percentile(self.slope[quay], 5),
                                                                                 np.percentile(self.slope[quay], 50),
                                                                                 NAV_MAX_SLOPE_DEG))
        bed = (self.canal_d < QUAY_FOOT - 100.0) & self.inside & ~ndimage.binary_dilation(self.dam, iterations=3)
        lines.append("canal bed cells reachable from hq: %d (must be 0)" % int((full & bed).sum()))
        # with the road and rail dams shut, the north bank must only be reachable over the lock footbridge (infantry)
        shut = blocked.copy()
        for fid in ("road_bridge", "rail_bridge", "weir"):
            p = self.dams[fid]["centre"]
            rr, cc = self.rc(*p)
            shut[max(rr - 14, 0):rr + 15, max(cc - 14, 0):cc + 15] = True
        north = self.reach(self.pt("hq"), shut)[self.rc(*self.pt("director"))]
        lines.append("all three crossings shut: director reachable from hq %s (must be False)" % north)
        road = np.asarray(self.path("main_road").pts, float)
        bad = 0
        for a, b in zip(road[:-1], road[1:]):
            L = float(np.linalg.norm(b - a))
            for s in np.arange(0.0, L, 150.0):
                q = a + (b - a) * s / L
                r, c = self.rc(*q)
                if 0 <= r < V and 0 <= c < V and (self.slope[r, c] > 20.0 or not full[r, c]):
                    bad += 1
        lines.append("main road samples steeper than 20 deg or off the reach area: %d (must be 0)" % bad)
        lines.append("walkable share of play area: %.1f %%" % (100.0 * (full & self.inside).sum() / self.inside.sum()))
        self.reach_full = full
        text = "\n".join(lines)
        with open(os.path.join(OUT, "check.txt"), "w") as handle:
            handle.write(text)
        print(text)

    # ---- output -------------------------------------------------------------------------------------------------
    @staticmethod
    def to_grid(a):
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        files = {"height": os.path.join(OUT, "BKU_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "BKU_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        spec = self.spec
        regions = [{"id": r.id, "kind": r.kind, "label": r.label, "centre": [float(v) for v in r.geom.centre],
                    "points": [[float(a), float(b)] for a, b in r.geom.pts], "raw": r.raw} for r in spec.regions]
        features = [{"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                     "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw} for f in spec.features]
        paths = [{"id": p.id, "kind": p.kind, "label": p.label, "width": p.width,
                  "points": [[float(a), float(b)] for a, b in p.pts]} for p in spec.paths]
        layout = {
            "map": MAP_PATH, "spec": SPEC_FILE.replace("\\", "/"),
            "landscape": {"components_x": COMPONENTS, "components_y": COMPONENTS, "quads_per_component": QUADS,
                          "quad_cm": QUAD_CM, "verts_x": V, "verts_y": V, "size_x": SIZE, "size_y": SIZE,
                          "corner": [-SIZE / 2.0, -SIZE / 2.0]},
            "bounds": {"xmin": spec.xmin, "xmax": spec.xmax, "ymin": spec.ymin, "ymax": spec.ymax},
            "z_range": [float(self.Z.min()), float(self.Z.max())],
            "canal": {"axis": [[float(a), float(b)] for a, b in self.canal], "half_cm": CANAL_HALF,
                      "quay_foot_cm": QUAY_FOOT, "bed_z": CANAL_BED_Z, "water_z": WATER_Z},
            "dams": self.dams, "rail_line": self.rail_line,
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths,
        }
        with open(os.path.join(OUT, "BKU_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        with open(os.path.join(OUT, "BKU_dress.json"), "w") as handle:
            json.dump({"ruins": self.ruins, "feet": self.feet, "actors": self.actors, "decals": self.decals}, handle)
        self.preview()
        for name in LAYERS:
            print("layer %-13s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"BerlinRubble": (178, 172, 163), "BerlinAsh": (104, 101, 98), "BerlinCobble": (146, 144, 141),
                   "BerlinGlass": (112, 120, 128), "BerlinScrub": (134, 124, 92), "BerlinMud": (96, 86, 74)}
        rgb = np.zeros((V, V, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.9 + 1.2 * (gy - gx), 0.45, 1.3)
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        rgb[self.in_canal] = (60, 72, 76)
        img = Image.fromarray(rgb.astype(np.uint8))
        d = ImageDraw.Draw(img)
        for e in self.ruins:
            hx, hy, ox, oy = KIT[e["piece"]]
            yaw = math.radians(e["yaw"])
            ca, sa = math.cos(yaw), math.sin(yaw)
            cx, cy = e["x"] + ox * ca - oy * sa, e["y"] + ox * sa + oy * ca
            poly = []
            for u, v in ((1, 1), (1, -1), (-1, -1), (-1, 1)):
                x, y = cx + u * hx * ca - v * hy * sa, cy + u * hx * sa + v * hy * ca
                r, c = self.raster.to_rc((x, y))
                poly.append((c, r))
            d.polygon(poly, outline=(70, 62, 58), fill=(120, 112, 106) if e["kind"] != "backdrop" else (100, 96, 92))
        for a in self.actors:
            r, c = self.raster.to_rc((a["x"], a["y"]))
            col = (40, 60, 30) if "Tree" in a["label"] else (90, 70, 60)
            d.ellipse([c - 2, r - 2, c + 2, r + 2], fill=col)
        r0, c0 = self.raster.to_rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.raster.to_rc([self.spec.xmin, self.spec.ymax])
        d.rectangle([c0, r0, c1, r1], outline=(255, 220, 0))
        img.save(os.path.join(OUT, "preview.png"))
        sl = np.zeros((V, V, 3), dtype=np.uint8)
        sl[..., 1] = np.clip((self.Z + 450.0) / 1600.0 * 255, 0, 255)
        sl[..., 2] = np.where(self.reach_full, 90, 0)
        sl[..., 0] = np.clip(self.slope * 4, 0, 255)
        sl[self.slope > NAV_MAX_SLOPE_DEG] = (255, 0, 255)
        Image.fromarray(sl).save(os.path.join(OUT, "preview_slope.png"))


def main():
    gen = Gen()
    gen.build_heights()
    gen.build_dress()
    gen.build_layers()
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
