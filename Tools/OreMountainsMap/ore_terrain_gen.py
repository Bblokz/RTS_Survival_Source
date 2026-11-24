"""Terrain generator for Ger_2ndOreMountains: heights + paint layers + a resolved layout, from the MapDesigner sketch.

    python Source/Tools/OreMountainsMap/ore_terrain_gen.py

Reads  MapDesigner/OreMountains/ore_01.layout.json (design source, +X north, +Y east, cm).
Writes Saved/OreMountains/terrain/
    Ore_height.raw          uint16 LE, 32768 + cm * 128 / 100, row by row along X (Z[iy, ix])
    Ore_layer_<Layer>.raw   uint8 weights, same grid, the layers sum to 255 per vertex
    Ore_layout.json         landscape grid, resolved regions / features / paths
    Ore_masks.npz           masks on the vertex grid ([iy, ix]) for the dressing steps
    preview.png, preview_slope.png, check.txt

Heights come from the sketch's elevation block (contours, pads, graded ramps). On top of that:
  - the escarpment, the knoll sides and the border crest are sharpened into rock faces steeper than the nav limit
    (all four nav agents: 44 degrees), except on the graded ramps;
  - gullies, peat bogs, spoil heaps, road beds and gentle rolling noise;
  - the ~110 m camera margin past the play edge continues the valley east and west and rises north and south.
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
from mapdesigner.spec import Spec, dist_to_polyline  # noqa: E402

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "OreMountains", "ore_01.layout.json")
OUT = os.path.join(ROOT, "Saved", "OreMountains", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains"

SCALE = 0.67                            # owner 2026-11-13: "the map feels too large, reduce it by 33 %" (850 -> 570 m)
COMPONENTS = 12                         # 756 m: the 570 m play area plus 93 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1              # 1072 vertices a side
SIZE = COMPONENTS * QUADS * QUAD_CM     # 107 100 cm
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

# DryCastle master layers (the Ger_MarderStugMission set)
LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop", "Asphalt", "DarkEarth",
          "IronOxide", "Ochre", "SlateScree"]
LAYER_TRACE = 2

NAV_MAX_SLOPE_DEG = 44.0                # agent_max_slope of all four RecastNavMesh actors in the template
CLIFF_R = 8                             # cells: half width of the band a slope is sharpened over
CREST_EXTRA = 5200.0                    # the border crest keeps rising past the play edge (cm above its 35 m)
NORTH_RISE = 2200.0
GULLY_DEPTH = 150.0
BOG_DEPTH = 140.0
RELIEF_GAIN = 1.55                      # mid-map relief: ridges to about 5.5 m, hollows to about 3 m


# the fossil roots sit on the gully cut at these spots (OreDeadForest_pieces.json): the cut must not move there
FOSSIL_ROOT_SPOTS = ((-7103, 3953), (-7827, -6667), (5513, 10519), (5794, -9916), (-7035, -24790))


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


def streak_noise(seed, along_cells, across_cells, bearing_deg, shape=SHAPE):
    """Value noise in -1..1 stretched along a compass bearing (wind streaks): long along it, short across."""
    rng = np.random.default_rng(seed)
    size = int(max(shape) * 1.5)
    small = rng.random((max(2, int(size / across_cells) + 2), max(2, int(size / along_cells) + 2))).astype(np.float32)
    img = Image.fromarray(small, mode="F").resize((size, size), Image.BICUBIC)       # streaks run west-east
    img = img.rotate(90.0 - bearing_deg, resample=Image.BICUBIC)                    # then turn them to the bearing
    off = (size - shape[0]) // 2
    big = np.asarray(img, dtype=np.float64)[off:off + shape[0], off:off + shape[1]]
    big = (big - big.mean()) / (big.std() + 1e-9)
    return np.clip(big / 2.2, -1.0, 1.0)


def crisp(v, t, w=0.06):
    """A sharp threshold: 0 below t - w, 1 above t + w."""
    return smoothstep(t - w, t + w, v)


def band(v, lo, hi, w=0.06):
    return crisp(v, lo, w) * (1.0 - crisp(v, hi, w))


def grey(a, r, fn):
    """Grey dilation (fn = np.maximum) or erosion (np.minimum) with a disc of radius r cells."""
    pad = np.pad(a, r, mode="edge")
    out = a.copy()
    n0, n1 = a.shape
    for dx in range(-r, r + 1):
        for dy in range(-r, r + 1):
            if dx * dx + dy * dy > r * r or (dx == 0 and dy == 0):
                continue
            fn(out, pad[r + dx:r + dx + n0, r + dy:r + dy + n1], out=out)
    return out


def slope_deg(Z):
    gx = np.gradient(Z, axis=1) / QUAD_CM
    gy = np.gradient(Z, axis=0) / QUAD_CM
    return np.degrees(np.arctan(np.hypot(gx, gy)))


COORD_KEYS = ("at", "polygon", "line", "points", "path", "ends", "offset")
LENGTH_KEYS = ("r", "dist", "radius", "build_radius", "half_size", "length")
KEEP_WIDTH = ("viaduct", "gully", "escarpment")     # real-world widths that do not shrink with the map


def _scale_coords(value, k):
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return value * k
    if isinstance(value, list):
        return [_scale_coords(v, k) for v in value]
    if isinstance(value, dict):
        return _scale_node(value, k, "point")
    return value


def _scale_node(node, k, context):
    out = {}
    for key, value in node.items():
        if key in COORD_KEYS:
            out[key] = _scale_coords(value, k)
        elif key in LENGTH_KEYS and isinstance(value, (int, float)) and context != "choke":
            out[key] = value * k
        elif key == "size" and isinstance(value, list):
            out[key] = [v * k for v in value]
        elif key == "lobes":
            out[key] = [[b, w, push * k] for b, w, push in value]
        elif key in ("width", "width_end") and isinstance(value, (int, float)):
            if context == "region" and not str(node.get("id", "")).startswith(KEEP_WIDTH):
                out[key] = value * k
            elif context == "feature" and node.get("kind") == "ramp":
                out[key] = max(1200.0, value * k)          # ramps stay wide enough for a heavy tank
            else:
                out[key] = value                            # roads, tracks, gates, chokes: real widths
        elif key == "blend_m" and context == "pad":
            out[key] = value * k
        elif key == "chokes":
            out[key] = [_scale_node(c, k, "choke") | {"at": _scale_coords(c["at"], k)} for c in value]
        elif isinstance(value, dict):
            out[key] = _scale_node(value, k, context)
        else:
            out[key] = value
    return out


def implementation_spec():
    """The sketch scaled to SCALE about the map centre. Heights, road and gate widths stay as drawn, so every ramp is
    1 / SCALE steeper than in the sketch. The sketch file itself is not changed."""
    with open(SPEC_FILE, encoding="utf-8") as handle:
        raw = json.load(handle)
    raw["bounds"]["half_size"] = raw["bounds"]["half_size"] * SCALE
    raw["bounds"]["landscape"]["components"] = COMPONENTS
    raw["settings"]["ramp_min_turn_radius_cm"] = raw["settings"]["ramp_min_turn_radius_cm"] * SCALE
    raw["regions"] = [_scale_node(r, SCALE, "region") for r in raw["regions"]]
    raw["features"] = [_scale_node(f, SCALE, "feature") for f in raw["features"]]
    raw["paths"] = [_scale_node(p, SCALE, "path") for p in raw["paths"]]
    el = raw["elevation"]
    el["contours"] = [_scale_node(c, SCALE, "contour") for c in el["contours"]]
    el["pads"] = [_scale_node(c, SCALE, "pad") for c in el["pads"]]
    raw["title"] = raw.get("title", "") + " x%.2f" % SCALE
    out = os.path.join(OUT, "ore_01_impl.layout.json")
    os.makedirs(OUT, exist_ok=True)
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(raw, handle, indent=1)
    return out


class Gen:
    def __init__(self):
        self.spec = Spec(implementation_spec())
        if self.spec.errors():
            raise RuntimeError("spec errors: %s" % [str(i.msg) for i in self.spec.errors()])
        raster = Raster(self.spec, QUAD_CM)
        # pixel centres on the landscape vertices: row r is x = SIZE/2 - r*100, col c is y = -SIZE/2 + c*100
        raster.xmax = SIZE / 2.0 + QUAD_CM / 2.0
        raster.ymin = -SIZE / 2.0 - QUAD_CM / 2.0
        raster.rows, raster.cols = V, V
        self.raster = raster
        self.terrain = Terrain(self.spec, raster)
        self.masks = self.terrain.region_masks
        self.X = (SIZE / 2.0 - np.arange(V) * QUAD_CM)[:, None] * np.ones((1, V))
        self.Y = np.ones((V, 1)) * (-SIZE / 2.0 + np.arange(V) * QUAD_CM)[None, :]
        self.inside = (np.abs(self.X) <= self.spec.xmax) & (np.abs(self.Y) <= self.spec.ymax)
        self.E = self.spec.elevation.raster(raster) * 100.0     # cm; clamps past the play edge
        self.Z = None
        self.W = {}
        self.out_masks = {}

    def region(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for rid in ids:
            m |= self.masks[rid]
        return m

    def regions_like(self, prefix):
        return self.region(*[r.id for r in self.spec.regions if r.id.startswith(prefix)])

    def openings(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for fid in ids:
            m |= self.terrain.opening_masks[fid]
        return m

    def path(self, pid):
        return next(p for p in self.spec.paths if p.id == pid)

    def feature(self, fid):
        return next(f for f in self.spec.features if f.id == fid)

    def dist_line(self, pts):
        pts = np.asarray(pts, float)
        flat = np.c_[self.X.ravel(), self.Y.ravel()]
        lo = pts.min(axis=0) - 8000.0
        hi = pts.max(axis=0) + 8000.0
        near = (flat[:, 0] >= lo[0]) & (flat[:, 0] <= hi[0]) & (flat[:, 1] >= lo[1]) & (flat[:, 1] <= hi[1])
        d = np.full(flat.shape[0], 1e9)
        d[near] = dist_to_polyline(flat[near], pts)
        return d.reshape(SHAPE)

    def dist_point(self, p):
        return np.hypot(self.X - p[0], self.Y - p[1])

    @staticmethod
    def extended(pts, length=14000.0):
        """A polyline run on past both ends (roads leave through the camera margin)."""
        pts = [np.asarray(p, float) for p in pts]
        a = pts[0] + (pts[0] - pts[1]) / np.linalg.norm(pts[0] - pts[1]) * length
        b = pts[-1] + (pts[-1] - pts[-2]) / np.linalg.norm(pts[-1] - pts[-2]) * length
        return np.array([a] + pts + [b])

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        spec = self.spec
        E = self.E
        ramp_ids = [f.id for f in spec.features if f.kind == "ramp"]
        ramps = self.openings(*ramp_ids)
        ramp_keep = soft(grey(ramps.astype(np.float64), 5, np.maximum), 2)       # ramps plus 5 m, feathered
        self.out_masks["ramps"] = ramps

        # roads: the valley road runs out both ends, the ammo track and depot track stay inside
        road_pts = self.extended(self.path("valley_road").pts)
        self.road_dist = self.dist_line(road_pts)
        # the Base B road (2026-11-14, user request): south-west corner -> across the valley road -> knoll west ramp.
        # Routed by ore_road_route.py; runs out through the west margin at its start.
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "ore_base_b_road.json"), "r") as handle:
            self.road2_pts = np.asarray(json.load(handle)["points"], float)
        lead = self.road2_pts[0] + (self.road2_pts[0] - self.road2_pts[1]) / np.linalg.norm(self.road2_pts[0] - self.road2_pts[1]) * 4500.0
        self.road2_line = np.vstack([lead[None, :], self.road2_pts])
        self.road2_dist = self.dist_line(self.road2_line)
        self.ammo_dist = self.dist_line(self.path("ammo_route").pts)
        self.depot_dist = self.dist_line(self.path("depot_track").pts)
        track = np.minimum(self.ammo_dist, self.depot_dist)
        for pid in ("flank_w", "flank_e"):
            track = np.minimum(track, self.dist_line(self.path(pid).pts) + 150.0)
        self.track_dist = track
        self.out_masks["road"] = (self.road_dist < 1000.0) | (self.road2_dist < 1000.0)
        self.out_masks["track"] = track < 650.0

        # 1. rock faces. A slope is sharpened by taking the dilated height above the local mean and the eroded height
        # below it: a face of about (slope x 2 x CLIFF_R) with talus under it.
        escarp = self.region("escarpment")
        knoll_band = self.band_between("c_knoll_foot", "c_knoll_top")
        crest_band = self.band_between("c_bench_back", "c_crest")
        self.out_masks["escarpment"] = escarp
        # the sketch's contour polygons leave straight creases in the foothills: round them off everywhere except on
        # the faces, the ramps and the level tops
        keep_sharp = np.clip(soft(grey((self.region("escarpment") | knoll_band | crest_band | ramps).astype(np.float64), 6, np.maximum), 3) * 2.0, 0.0, 1.0)
        E = E * keep_sharp + soft(E, 6) * (1.0 - keep_sharp)
        self.E = E
        hi = grey(E, CLIFF_R, np.maximum)
        lo = grey(E, CLIFF_R, np.minimum)
        # ragged crag line: the split height moves with noise so the face is not a ruler line
        jag = 170.0 * noise(101, 9.0) + 80.0 * noise(102, 4.0)
        # each face splits at a fixed contour height, so it is continuous along its whole length
        along = self.dist_line(self.spec.by_id["escarpment"].geom.pts) < 3200.0 * SCALE
        esc_band = along & (E > 500.0) & (E < 1790.0) & (self.Y > -30500.0 * SCALE)
        w_cliff = np.zeros(SHAPE)
        sharp = E.copy()
        for band, split in ((esc_band, 1150.0), (knoll_band, 1300.0), (crest_band, 2600.0)):
            w = smoothstep(0.3, 0.7, soft(band, 2))
            face = np.where(E + jag >= split, hi, lo)
            sharp = np.where(w > w_cliff, face, sharp)
            w_cliff = np.maximum(w_cliff, w)
        w_cliff = w_cliff * (1.0 - smoothstep(0.05, 0.5, ramp_keep))
        # the open west shoulder stays a plain slope (the sketch leaves it passable)
        Z = E * (1.0 - w_cliff) + sharp * w_cliff
        # a landscape cannot hold a vertical wall (the texture smears): lay the faces back to about 60 degrees
        lay_back = smoothstep(0.1, 0.6, soft(w_cliff > 0.3, 3))
        Z = Z * (1.0 - lay_back) + soft(Z, 1) * lay_back
        self.cliff_w = w_cliff
        self.out_masks["cliff"] = w_cliff > 0.5

        # 2. rolling ground: none on the bench build area, pads, ramps and roads
        flat_ids = ["battery_pad", "checkpoint_yard", "base_a_yard"]
        pads = self.region(*flat_ids) | (self.dist_point(self.feature("hq").points[0]) < 9000.0 * SCALE)
        knoll_top = self.spec.by_id["base_b_knoll"]
        pads |= self.masks["base_b_knoll"] & (E > 1550.0)
        for fid in ("b_gamma", "b_refinery", "b_barracks"):
            pads |= self.dist_point(self.feature(fid).points[0]) < 2600.0
        for fid in ("survey_post", "checkpoint", "viaduct"):
            pads |= self.dist_point(self.feature(fid).points[0]) < 1400.0
        self.out_masks["pads"] = pads
        calm = np.clip(soft(pads, 5) * 1.6 + ramp_keep + smoothstep(1100.0, 600.0, self.road_dist)
                       + smoothstep(900.0, 500.0, track) + w_cliff, 0.0, 1.0)
        bench = soft(self.region("ridge_bench"), 6)
        foot = soft(self.region("foothills", "west_shoulder"), 8)
        roll = 22.0 * fbm(11, 80.0, 3) + 6.0 * noise(12, 10.0)
        roll += foot * (95.0 * fbm(21, 70.0, 3) + 30.0 * fbm(22, 22.0, 2))
        roll *= 1.0 - 0.75 * bench
        Z = Z + roll * (1.0 - calm)

        # 2b. mid-map relief (owner 2026-11-13: "interesting height changes in the middle of the map; still traversible
        # with vehicles"). Swells, low ridges, hollows and a dry wash on the valley floor and the foothills. Nothing
        # changes within 20 m of a rock face or 25 m of the viaduct line (the cliff kit and the viaduct are placed at
        # absolute heights), near the road, on ramps or on the level pads, and the bench stays flat for building.
        via_a = np.array(self.spec.by_id["viaduct_south"].geom.pts[0], float)
        via_b = np.array(self.spec.by_id["viaduct_north"].geom.pts[-1], float)
        via_d = self.dist_line(np.array([via_a - (via_b - via_a) * 0.08, via_b + (via_b - via_a) * 0.08]))
        near_face = soft(grey((w_cliff > 0.1).astype(np.float64), 20, np.maximum), 3)
        quiet = np.clip(soft(pads, 5) * 1.6 + soft(grey(ramps.astype(np.float64), 8, np.maximum), 3) + near_face
                        + smoothstep(1900.0, 1000.0, self.road_dist) + smoothstep(1000.0, 500.0, track)
                        + smoothstep(3300.0, 2500.0, via_d) + soft(self.region("ridge_bench", "border_crest", "base_b_knoll"), 8) * 1.5
                        + soft(self.regions_like("spoil_heap_"), 6), 0.0, 1.0)
        zone = soft(self.region("valley_floor", "foothills", "west_shoulder", "side_valley"), 6) * (1.0 - quiet) \
            * smoothstep(0.0, 1500.0, np.minimum(self.spec.xmax - np.abs(self.X), self.spec.ymax - np.abs(self.Y)) + 1500.0)
        relief = 170.0 * fbm(131, 75.0, 3) + 80.0 * fbm(132, 28.0, 2)

        def ridge(x, y, length, width, height, yaw_deg):
            a = math.radians(yaw_deg)
            u = (self.X - x) * math.cos(a) + (self.Y - y) * math.sin(a)
            v = -(self.X - x) * math.sin(a) + (self.Y - y) * math.cos(a)
            half = width / 2.0
            if height > 0.0:
                # critic round 4: a cuesta, not a symmetric swell. The face turned away from the sun is as steep as a
                # vehicle takes (about 27 degrees), so it stands in shadow; the sunny side runs out twice as far.
                sun_side = 1.0 if (-math.sin(a) * 0.613 + math.cos(a) * 0.326) >= 0.0 else -1.0
                half = np.where(v * sun_side > 0.0, width, max(0.36 * width, 2.6 * height))
            return height * np.exp(-(u / (length / 2.0)) ** 2 - (v / half) ** 2)

        # (x, y, length, width, height, yaw): low ridges and knobs; negative heights are hollows
        for form in ((-4500, -13000, 5000, 1500, 420, 85), (-5300, 9500, 4200, 1300, 260, 100), (3200, -9500, 4500, 1400, 400, 70),
                     (5200, 17500, 3800, 1200, 250, 95), (11000, 9000, 6000, 1900, 460, 60), (16000, -20000, 6000, 1900, 350, 120),
                     (8000, -16000, 4500, 1600, 300, 45), (20000, -3000, 5000, 1700, 330, 20), (13500, 22000, 4500, 1500, 280, 110),
                     (-5500, -2500, 2600, 2400, -190, 0), (2900, 3200, 2400, 2000, -160, 0), (7500, 2500, 3000, 2600, -200, 30),
                     (10500, -8500, 2800, 2400, -170, 0),
                     # critic round 1: the ribs were all one size; short sharp ones (15-30 m) and a long low one (80 m)
                     (-5600, -19500, 1900, 800, 170, 60), (-4200, 5200, 1600, 750, 160, 110), (4300, -15500, 2300, 850, 180, 95),
                     (6800, 14500, 1700, 800, 165, 40), (14500, 1500, 2600, 900, 185, 75), (18500, -13500, 2000, 850, 170, 130),
                     (9500, 20500, 8000, 1500, 210, 85), (-3600, -24000, 2200, 850, 170, 100)):
            relief = relief + ridge(*form)
        # a dry wash between the road and the foothills: 1.4 m deep, 14 m wide, with a low bank on its north side
        wash_pts = np.array([[2600, -29500], [1900, -19000], [2700, -11000], [2000, -2500], [2900, 5000], [2300, 12500], [3000, 19500], [2500, 24500], [3100, 30500]], float)
        wash_d = self.dist_line(wash_pts) + 160.0 * noise(133, 14.0)
        # critic round 4: the bank is cut (28 degrees) in 8-15 m scallops and slumped to about 10 degrees between them;
        # low gravel bars lie in the bed
        gap = smoothstep(0.1, 0.5, noise(134, 16.0))
        wash = smoothstep(680.0 + 520.0 * gap, 280.0, wash_d)
        self.wash_d = wash_d
        self.wash_gap = gap
        self.wash_bars = None
        bars = smoothstep(330.0, 200.0, wash_d) * crisp(noise(135, 7.0), 0.45, 0.2)
        self.wash_bars = bars
        relief = relief - 140.0 * wash + 45.0 * np.exp(-((wash_d - 950.0) / 320.0) ** 2) + 20.0 * bars
        # shallow deflation pans on the open floor: 8-15 m, 0.45 m deep
        pan = crisp(fbm(161, 38.0, 3) + 0.3 * noise(163, 8.0), 0.4, 0.06) * smoothstep(900.0, 1500.0, wash_d)
        self.pan_w = pan
        relief = relief - 30.0 * pan
        relief = relief * RELIEF_GAIN
        Z = Z + relief * zone
        self.relief_zone = zone
        self.wash_w = wash * zone
        self.pan_w = self.pan_w * zone
        self.out_masks["wash"] = self.wash_w > 0.4

        # 3. spoil heaps: the pads give a 5 m level top; make them rounded heaps with a small crown
        heaps = self.regions_like("spoil_heap_")
        Z += 140.0 * smoothstep(0.55, 1.0, soft(heaps, 6)) * (1.0 + 0.3 * noise(31, 8.0))
        self.out_masks["spoil_heap"] = heaps

        # 4. erosion gullies: shallow V channels (walkable, about 30 % sides)
        gully = np.full(SHAPE, 1e9)
        gully_m = np.full(SHAPE, 1e9)
        keep_face = np.clip(near_face * 1.5, 0.0, 1.0)
        for k, reg in enumerate(r for r in spec.regions if r.id.startswith("gully_")):
            gully = np.minimum(gully, self.dist_line(reg.geom.pts))
            gully_m = np.minimum(gully_m, self.dist_line(self.meander(reg.geom.pts, keep_face, 300 + k)))
        # critic round 1: a gully is a cut with banks, not a V smear: flat bed about 3 m wide, banks of about 30 degrees,
        # and the line meanders. Within 20 m of a rock face the old shallow V stays exactly as it was: the cliff kit's
        # scree is placed there at absolute heights.
        keep_old = np.clip(near_face * 1.5, 0.0, 1.0)
        open_ground = 1.0 - np.clip(ramp_keep + smoothstep(900.0, 600.0, self.road_dist), 0, 1)
        old_cut = smoothstep(480.0, 60.0, gully + 120.0 * noise(41, 6.0)) * open_ground
        # critic round 2: the line itself meanders (5 m swings every 25-40 m), see meander()
        gully = gully_m + 230.0 * noise(41, 9.0) + 60.0 * noise(141, 3.0)
        new_cut = smoothstep(430.0, 170.0, gully) * open_ground
        Z -= 130.0 * old_cut * keep_old + GULLY_DEPTH * new_cut * (1.0 - keep_old)
        cut = old_cut * keep_old + new_cut * (1.0 - keep_old)
        self.gully_d = gully + 1e6 * (keep_old > 0.5)          # no bed/bank paint on the kept stretches
        self.gully_w = cut
        self.out_masks["gully"] = cut > 0.35

        # 5. peat bogs: sunken, hummocky
        # critic round 1: the bogs were clean ovals; the outline is now warped into lobes at 8-15 m and the bog sinks
        # critic round 4: a 1 m dish over 20 m is invisible. Now a flat pan 1.4 m down behind a cut bank about 4 m wide,
        # with peat hags (3-6 m, half a metre high) left standing in it.
        bog = soft(smoothstep(0.44, 0.56, soft(self.regions_like("peat_bog_"), 5) + 0.34 * fbm(151, 12.0, 2)), 1)
        hag = crisp(noise(152, 6.0) + 0.3 * noise(153, 3.0), 0.5, 0.12) * smoothstep(0.9, 1.0, soft(bog, 2))
        Z += bog * (-BOG_DEPTH + 10.0 * noise(51, 5.0) + 50.0 * hag) * (1.0 - smoothstep(900.0, 600.0, self.road_dist))
        self.bog_w = bog
        self.bog_hag = hag * bog
        self.out_masks["peat_bog"] = bog > 0.5

        # 5b. realism round: every gully and bog drains to the wash by a rill (0.35 m deep, 2 m wide, free ground only);
        # shell craters sunk 0.55-0.85 m with a 0.2 m rim on free valley ground
        self.wash_pts = wash_pts
        self.tribs = self.tributary_lines(Z, wash_pts)
        trib_d = np.full(SHAPE, 1e9)
        for line in self.tribs:
            trib_d = np.minimum(trib_d, self.dist_line(line))
        trib_d = trib_d + 90.0 * noise(181, 7.0)
        root_free = np.ones(SHAPE)
        for spot in FOSSIL_ROOT_SPOTS:
            root_free = root_free * smoothstep(1000.0, 2200.0, self.dist_point(spot))
        free_ground = zone * root_free * (1.0 - np.clip(self.bog_w * 2.0, 0.0, 1.0))
        Z -= 35.0 * smoothstep(230.0, 60.0, trib_d) * free_ground
        self.trib_d = trib_d
        self.craters = self.crater_sites(zone, near_face, via_d, pads)
        self.crater_r = np.full(SHAPE, 9.0)                    # distance to the nearest crater in radii
        for x, y, radius, sunk in self.craters:
            rho = self.dist_point((x, y)) / radius
            self.crater_r = np.minimum(self.crater_r, rho)
            if sunk:
                depth = 55.0 + 30.0 * (radius - 240.0) / 280.0
                Z += (-depth * smoothstep(1.0, 0.3, rho) + 20.0 * np.exp(-((rho - 1.05) / 0.22) ** 2)) * free_ground
        self.near_face = near_face

        # 6. viaduct: a low rubble swell under the collapsed spans, the 10 m road gap left clear
        rubble = soft(self.region("viaduct_rubble"), 3) * (1.0 - smoothstep(1600.0, 1050.0, self.road_dist))
        Z += rubble * (45.0 + 40.0 * np.abs(noise(61, 5.0)))
        self.out_masks["viaduct_rubble"] = rubble > 0.4
        self.out_masks["viaduct_deck"] = self.regions_like("viaduct_south") | self.regions_like("viaduct_north")

        # 7. road beds: cross-fall removed, the carriageway a touch below the verge
        level = box_blur(Z, 5)
        bed = smoothstep(750.0, 450.0, self.road_dist)
        Z = Z * (1.0 - bed) + (level - 12.0) * bed
        tbed = smoothstep(520.0, 300.0, track) * (1.0 - bed)
        Z = Z * (1.0 - tbed) + (level - 8.0) * tbed

        # 7b. critic round 4: a real ditch on the north side of the road, 1.5 m wide and 0.4 m deep, in 10-25 m runs
        north_side = (np.gradient(self.road_dist, axis=0) < 0.0).astype(np.float64)
        calm_d = np.clip(soft(pads, 5) * 1.6 + ramp_keep + near_face + smoothstep(3300.0, 2500.0, via_d)
                         + smoothstep(900.0, 500.0, track) + self.bog_w, 0.0, 1.0)
        ditch = crisp(self.road_dist, 800.0, 70.0) * (1.0 - crisp(self.road_dist, 1010.0, 70.0)) * north_side * (1.0 - calm_d)
        ditch = ditch * (np.abs(self.Y) < spec.ymax + 2000.0) * smoothstep(1100.0, 1700.0, self.road2_dist)
        Z -= 80.0 * ditch
        self.ditch_run = north_side * (1.0 - calm_d)
        self.ditch_w = ditch

        # 7c. the Base B road: a graded bed. The long profile is the ground along the line, smoothed over about 60 m
        # (cut and fill), pinned where the ground must stay (ramps, pads, rock faces); side slopes blend out over 10 m.
        keep2 = np.clip(ramp_keep + near_face + soft(pads, 5) * 1.6, 0.0, 1.0)
        seg = np.hypot(*np.diff(self.road2_line, axis=0).T)
        cum = np.concatenate([[0.0], np.cumsum(seg)])
        s = np.arange(0.0, cum[-1], QUAD_CM)
        lx, ly = np.interp(s, cum, self.road2_line[:, 0]), np.interp(s, cum, self.road2_line[:, 1])
        lr = np.clip(np.round((SIZE / 2.0 - lx) / QUAD_CM).astype(int), 0, V - 1)
        lc = np.clip(np.round((ly + SIZE / 2.0) / QUAD_CM).astype(int), 0, V - 1)
        z_line = Z[lr, lc].copy()
        pin = keep2[lr, lc] > 0.3
        pin[0] = pin[-1] = True
        prof = z_line.copy()
        kernel = np.ones(21) / 21.0
        for _ in range(12):
            prof = np.convolve(np.pad(prof, 10, mode="edge"), kernel, mode="valid")
            prof[pin] = z_line[pin]
        near2 = np.nonzero(self.road2_dist.ravel() < 1700.0)[0]
        cx, cy = self.X.ravel()[near2], self.Y.ravel()[near2]
        target = np.zeros(near2.size)
        for k in range(0, near2.size, 4000):
            d2 = (cx[k:k + 4000, None] - lx[None, :]) ** 2 + (cy[k:k + 4000, None] - ly[None, :]) ** 2
            target[k:k + 4000] = prof[np.argmin(d2, axis=1)]
        level2 = Z.copy()
        level2.ravel()[near2] = target
        bed2 = smoothstep(1600.0, 560.0, self.road2_dist) * (1.0 - keep2)
        Z = Z * (1.0 - bed2) + (level2 - 12.0 * smoothstep(750.0, 450.0, self.road2_dist)) * bed2
        grade2 = np.abs(np.diff(prof)) / QUAD_CM * 100.0
        print("base B road: %.0f m, profile moved up to %.0f cm (mean %.0f cm), max grade %.1f %% off the ramp" % (
            cum[-1] / 100.0, np.abs(prof - z_line).max(), np.abs(prof - z_line).mean(), grade2[~pin[1:]].max()))

        # 8. past the play edge: the crest keeps climbing south, the foothills rise north; east and west the valley
        # runs on (the contour field already repeats its edge profile there)
        d_s = np.clip(-spec.xmax - self.X, 0.0, None)          # cm past the south edge
        d_n = np.clip(self.X - spec.xmax, 0.0, None)
        d_ew = np.clip(np.abs(self.Y) - spec.ymax, 0.0, None)
        Z += smoothstep(0.0, 9000.0, d_s) * (CREST_EXTRA + 900.0 * fbm(71, 50.0, 3))
        Z += smoothstep(500.0, 10000.0, d_n) * (NORTH_RISE + 700.0 * fbm(72, 60.0, 3)) * (1.0 - smoothstep(700.0, 400.0, self.ammo_dist))
        valley = np.exp(-(self.road_dist / 6500.0) ** 2)
        Z += smoothstep(1500.0, 10500.0, d_ew) * (300.0 + 350.0 * fbm(73, 55.0, 3)) * (1.0 - valley)
        Z += smoothstep(300.0, 6000.0, d_ew) * (120.0 * fbm(75, 30.0, 3) + 60.0 * noise(76, 12.0))
        # the crest itself inside the play area: broken skyline
        crest = soft(self.region("border_crest"), 5) * smoothstep(3300.0, 3500.0, E)
        Z += crest * (180.0 * fbm(74, 25.0, 3) + 150.0)
        self.out_masks["crest"] = self.region("border_crest")
        self.Z = Z
        self.edge_dist = np.maximum.reduce([d_s, d_n, d_ew])

    def meander(self, pts, keep, seed):
        """A gully line with sideways swings of about 5 m every 25-40 m. No swing at its ends, where keep (a 0..1 grid:
        the stretches by a rock face) is set, or within 12 m of a fossil-root spot (those were measured on the cut)."""
        pts = np.asarray(pts, float)
        seg = np.hypot(*np.diff(pts, axis=0).T)
        s_knots = np.r_[0.0, np.cumsum(seg)]
        s = np.arange(0.0, s_knots[-1] + 1.0, 300.0)
        line = np.c_[np.interp(s, s_knots, pts[:, 0]), np.interp(s, s_knots, pts[:, 1])]
        tangent = np.gradient(line, axis=0)
        tangent /= np.hypot(tangent[:, 0], tangent[:, 1])[:, None] + 1e-9
        normal = np.c_[-tangent[:, 1], tangent[:, 0]]
        rng = np.random.default_rng(seed)
        ph = rng.random(2) * 6.283
        swing = 380.0 * np.sin(6.283 * s / 3300.0 + ph[0]) + 220.0 * np.sin(6.283 * s / 2100.0 + ph[1])
        taper = smoothstep(0.0, 700.0, s) * smoothstep(0.0, 700.0, s_knots[-1] - s)
        rows = np.clip(np.round((SIZE / 2.0 - line[:, 0]) / QUAD_CM).astype(int), 0, V - 1)
        cols = np.clip(np.round((line[:, 1] + SIZE / 2.0) / QUAD_CM).astype(int), 0, V - 1)
        taper = taper * (1.0 - box_blur(keep, 6)[rows, cols])
        for spot in FOSSIL_ROOT_SPOTS:
            taper = taper * smoothstep(900.0, 2200.0, np.hypot(line[:, 0] - spot[0], line[:, 1] - spot[1]))
        return line + normal * (swing * taper)[:, None]

    # ---- realism round: drainage, craters, wear --------------------------------------------------------------------
    def low_end(self, pts, Z):
        """The lower end of a line, and the point just before it."""
        pts = np.asarray(pts, float)

        def h(p):
            r = int(np.clip(round((SIZE / 2.0 - p[0]) / QUAD_CM), 0, V - 1))
            c = int(np.clip(round((p[1] + SIZE / 2.0) / QUAD_CM), 0, V - 1))
            return Z[r, c]
        return (pts[-1], pts[-2]) if h(pts[-1]) < h(pts[0]) else (pts[0], pts[1])

    def tributary_lines(self, Z, wash_pts):
        """Every gully and every bog drains to the wash: a line from the gully's low end (or the bog) to the wash,
        meeting it at an acute angle pointing downstream (west)."""
        lines = []
        starts = [self.low_end(r.geom.pts, Z)[0] for r in self.spec.regions if r.id.startswith("gully_")]
        starts += [np.asarray(r.geom.pts, float).mean(axis=0) for r in self.spec.regions if r.id.startswith("peat_bog_")]
        for k, start in enumerate(starts):
            start = np.asarray(start, float)[:2]
            y_join = start[1] - 700.0 - 0.15 * abs(start[0] - 2500.0)
            x_join = float(np.interp(y_join, wash_pts[:, 1], wash_pts[:, 0]))
            end = np.array([x_join, y_join])
            if np.hypot(*(end - start)) < 900.0:
                continue
            # a wandering line, not a ruler stroke: sideways swings of 2-3 m every 15-25 m, none at the two ends
            t = np.linspace(0.0, 1.0, max(4, int(np.hypot(*(end - start)) / 400.0)))
            line = start[None, :] + (end - start)[None, :] * t[:, None]
            swing = 260.0 * np.sin(t * np.hypot(*(end - start)) / 330.0 + k) * np.sin(3.1416 * t)
            line[:, 1] += swing
            lines.append(line)
        return lines

    def crater_sites(self, zone, near_face, via_d, pads):
        """Shell and bomb craters: misses beside the viaduct, round the battery and the checkpoint, along the bench
        lip. [x, y, radius, sunk]: only craters on free valley ground are sunk, the rest are decal and paint."""
        rng = np.random.default_rng(606)
        sites = []

        def cell(x, y):
            return (int(np.clip(round((SIZE / 2.0 - x) / QUAD_CM), 0, V - 1)), int(np.clip(round((y + SIZE / 2.0) / QUAD_CM), 0, V - 1)))

        def free(x, y, gap):
            r, c = cell(x, y)
            return (self.inside[r, c] and self.road_dist[r, c] > 1300.0 and self.road2_dist[r, c] > 1300.0 and self.track_dist[r, c] > 700.0 and not pads[r, c]
                    and near_face[r, c] < 0.05 and self.bog_w[r, c] < 0.05
                    and all(math.hypot(x - s[0], y - s[1]) > gap for s in sites)
                    and all(math.hypot(x - s[0], y - s[1]) > 2200.0 for s in FOSSIL_ROOT_SPOTS))

        def ring(centre, lo, hi, count, r_lo, r_hi, need=None):
            tries = 0
            placed = 0
            while placed < count and tries < 4000:
                tries += 1
                a, d = rng.uniform(0.0, 6.283), rng.uniform(lo, hi)
                x, y = centre[0] + d * math.cos(a), centre[1] + d * math.sin(a)
                if not free(x, y, 1100.0) or (need is not None and not need(x, y)):
                    continue
                r, c = cell(x, y)
                sites.append([float(x), float(y), float(rng.uniform(r_lo, r_hi)), bool(zone[r, c] > 0.85)])
                placed += 1

        via_a = np.array(self.spec.by_id["viaduct_south"].geom.pts[0], float)
        via_b = np.array(self.spec.by_id["viaduct_north"].geom.pts[-1], float)

        def via(x, y):
            r, c = cell(x, y)
            return via_d[r, c]
        for t in np.linspace(0.08, 0.92, 9):                       # the stick along the bridge: 6-20 m off its axis
            p = via_a + (via_b - via_a) * t
            ring(p, 500.0, 1900.0, 1, 280.0, 420.0, lambda x, y: 600.0 < via(x, y) < 2100.0)
        ring(0.5 * (via_a + via_b), 3600.0, 6000.0, 4, 380.0, 520.0, lambda x, y: via(x, y) > 3500.0)     # the misses
        ring(self.feature("artillery_battery").points[0], 2200.0, 6000.0, 7, 260.0, 440.0)
        ring(self.feature("checkpoint").points[0], 2400.0, 5200.0, 4, 240.0, 380.0)
        bench = self.region("ridge_bench")
        hq = self.feature("hq").points[0]
        lip = self.feature("overlook").points[0]

        def on_lip(x, y):
            r, c = cell(x, y)
            return bench[r, c] and math.hypot(x - hq[0], y - hq[1]) > 6200.0
        for dy in (-9000.0, -4500.0, 0.0, 5000.0, 9500.0):
            ring((lip[0] - 1500.0, lip[1] + dy), 300.0, 1800.0, 1, 240.0, 360.0, on_lip)
        return sites

    def build_wear(self):
        """Vehicle wear as a track network: least-cost routes over the built ground (slope, woods, bogs, cliffs) from
        the start to every ramp and on to the road, to the adits and the overlook, and from the road to each spoil
        heap and knoll ramp. Later routes prefer ground an earlier one already used, so tracks merge into trunks."""
        import heapq
        step = 3
        slope = slope_deg(self.Z)[::step, ::step]
        m = self.out_masks
        sub = lambda a: np.asarray(a)[::step, ::step]
        blocked = (slope > 33.0) | sub(grey(m["cliff"].astype(np.float64), 2, np.maximum) > 0) | sub(self.bog_w > 0.3) | ~sub(self.inside)
        cost = 1.0 + (slope / 9.0) ** 2
        cost = cost * np.where(sub(self.regions_like("fh_woods_") | self.regions_like("bench_woods_")), 3.0, 1.0)
        cost = cost * np.where(sub(self.regions_like("dead_thin_") | self.regions_like("dead_face_")), 1.6, 1.0)
        cost = cost * np.where(sub(self.region("radixite_ground")), 6.0, 1.0) * np.where(sub(m["gully"]), 3.0, 1.0)
        cost = cost * np.where(sub(self.region("base_a_yard", "battery_pad", "checkpoint_yard")), 4.0, 1.0)
        cost = cost * np.where(sub(self.road_dist) < 450.0, 0.3, 1.0) * np.where(sub(self.track_dist) < 300.0, 0.4, 1.0)
        cost = cost * np.where(sub(self.road2_dist) < 450.0, 0.3, 1.0)
        n = cost.shape[0]

        def node(p):
            r = int(np.clip(round((SIZE / 2.0 - p[0]) / QUAD_CM / step), 0, n - 1))
            c = int(np.clip(round((p[1] + SIZE / 2.0) / QUAD_CM / step), 0, n - 1))
            if blocked[r, c]:
                rr, cc = np.nonzero(~blocked)
                k = int(np.argmin((rr - r) ** 2 + (cc - c) ** 2))
                r, c = int(rr[k]), int(cc[k])
            return r, c

        def world(r, c):
            return [SIZE / 2.0 - r * step * QUAD_CM, c * step * QUAD_CM - SIZE / 2.0]

        def route(a, b):
            a, b = node(a), node(b)
            dist = np.full(cost.shape, np.inf)
            prev = {}
            dist[a] = 0.0
            heap = [(0.0, a)]
            while heap:
                d, cur = heapq.heappop(heap)
                if cur == b:
                    break
                if d > dist[cur]:
                    continue
                r, c = cur
                for dr, dc, w in ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)):
                    rr, cc = r + dr, c + dc
                    if rr < 0 or cc < 0 or rr >= n or cc >= n or blocked[rr, cc]:
                        continue
                    nd = d + w * 0.5 * (cost[r, c] + cost[rr, cc])
                    if nd < dist[rr, cc]:
                        dist[rr, cc] = nd
                        prev[(rr, cc)] = cur
                        heapq.heappush(heap, (nd, (rr, cc)))
            if b not in prev:
                return None
            nodes = [b]
            while nodes[-1] != a:
                nodes.append(prev[nodes[-1]])
            for r, c in nodes:
                cost[max(0, r - 1):r + 2, max(0, c - 1):c + 2] *= 0.6
                cost[r, c] *= 0.5
            nodes = nodes[::-1]
            pts = np.array([world(r, c) for r, c in nodes[::4] + [nodes[-1]]], float)
            for _ in range(2):                                   # Chaikin corner cutting
                q = 0.75 * pts[:-1] + 0.25 * pts[1:]
                s = 0.25 * pts[:-1] + 0.75 * pts[1:]
                pts = np.vstack([pts[:1], np.column_stack([q, s]).reshape(-1, 2), pts[-1:]])
            return pts

        road = np.asarray(self.path("valley_road").pts, float)

        def on_road(p):
            return road[int(np.argmin(np.hypot(road[:, 0] - p[0], road[:, 1] - p[1])))]

        f = lambda fid: np.asarray(self.feature(fid).points[0], float)
        hq = f("hq") + np.array([900.0, 0.0])
        legs = []
        for ramp in ("ramp_main", "ramp_sally", "ramp_west", "ramp_east"):
            legs += [(hq, f(ramp)), (f(ramp), on_road(f(ramp)))]
        legs += [(hq, f("adit_1")), (hq, f("adit_2")), (hq, f("adit_3")), (hq, f("overlook"))]
        for heap_id in ("spoil_heap_1", "spoil_heap_2", "spoil_heap_3"):
            centre = np.asarray(self.spec.by_id[heap_id].geom.pts, float).mean(axis=0)
            legs.append((on_road(centre), centre))
        for lane in ("flank_w", "flank_e"):
            pts = np.asarray(self.path(lane).pts, float)
            legs.append((pts[0], pts[-1]))
        self.wear_paths = []
        wear_d = np.full(SHAPE, 1e9)
        for a, b in legs:
            pts = route(a, b)
            if pts is None or len(pts) < 4:
                continue
            self.wear_paths.append(pts)
            wear_d = np.minimum(wear_d, self.dist_line(pts))
        self.wear_d = wear_d
        self.out_masks["wear"] = wear_d < 350.0
        print("wear tracks: %d of %d legs, %.0f m" % (len(self.wear_paths), len(legs),
                                                    sum(np.hypot(*np.diff(p, axis=0).T).sum() for p in self.wear_paths) / 100.0))

    def wear_runs(self):
        """The wear tracks for the map, cut where they run along the Base B road (no two-track drawn on the macadam)."""
        runs = []
        for pts in self.wear_paths:
            off = dist_to_polyline(np.asarray(pts, float), self.road2_line) > 620.0
            start = None
            for k, flag in enumerate(list(off) + [False]):
                if flag and start is None:
                    start = k
                elif not flag and start is not None:
                    if k - start >= 4:
                        runs.append(pts[start:k])
                    start = None
        return runs

    def band_between(self, outer_id, inner_id):
        """Cells inside the outer contour and outside the inner one."""
        el = self.spec.elevation
        geoms = {c.id: c.geom for c in el.contours}
        outer = self.raster.mask_geom(geoms[outer_id])
        inner = self.raster.mask_geom(geoms[inner_id])
        return outer & ~inner

    # ---- paint layers -------------------------------------------------------------------------------------------
    def build_layers(self):
        """Paint weights. Critic round 1 (2026-11-14): transitions thresholded and broken by noise instead of feathered,
        a base mix per zone, feature paint follows the shape that causes it (bank, bed, rim, apron, shoulder).
        Critic round 2: RTSSand reads salmon red in sunlight, so every "dark" is now soot = a partial Asphalt weight;
        edges wander 2-3 m (5 m noise in the edge term, distances perturbed in cm); the valley Desert is diluted with
        MountainTop (its orange blotches come from the texture, no material scalar removes them)."""
        n_big, n_mid, n_fine = fbm(81, 70.0, 3), fbm(82, 18.0, 2), noise(83, 5.0)
        n_micro, n_tiny = noise(85, 2.2), noise(86, 1.2)
        n_patch = fbm(87, 30.0, 2)                    # 20-40 m ground patches
        n_gravel = fbm(88, 8.0, 2)                    # 3-8 m stony patches
        n_piece = noise(90, 4.5)                      # 3-6 m pieces (rims, outcrop lines)
        streak = streak_noise(89, 26.0, 5.0, 35.0)    # wind streaks along the project wind (bearing 35 degrees)
        grit = 0.45 * n_fine + 0.35 * n_micro + 0.2 * n_tiny
        wander = 170.0 * n_fine + 70.0 * n_micro      # cm: added to a distance so its contour wanders 2-3 m

        def broken(v, t, amp=0.34, w=0.05):
            """Sharp threshold of a 0..1 field with a ragged edge."""
            return crisp(v + amp * grit, t, w)

        slope = slope_deg(self.Z)
        self.slope = slope
        height = self.Z
        zone = self.relief_zone
        bench = soft(self.region("ridge_bench", "border_crest"), 6)
        foot = soft(self.region("foothills", "west_shoulder", "side_valley"), 8) * (1.0 - bench)
        valley = np.clip(1.0 - bench - foot, 0.0, 1.0)
        woods = soft(self.regions_like("fh_woods_") | self.regions_like("bench_woods_"), 8)
        thin = soft(self.regions_like("dead_thin_") | self.regions_like("dead_face_"), 6)
        heap = self.regions_like("spoil_heap_")
        radix = soft(self.region("radixite_ground"), 10)
        cliff_core = smoothstep(0.35, 0.8, self.cliff_w)
        in_zone = (zone > 0.3).astype(np.float64)
        away_wash = crisp(self.wash_d, 850.0, 60.0) * in_zone + (1.0 - in_zone)

        # -- base mix per zone: valley warm (Desert thinned with pale), foothills greyer and stonier, bench pale patches --
        patches = broken(0.5 + 0.5 * n_patch + 0.15 * n_mid, 0.5, 0.25)
        patch_hi = broken(0.5 + 0.5 * n_patch, 0.68, 0.25)
        pale = bench * (0.66 + 0.3 * patches) + foot * (0.14 + 0.6 * broken(0.5 + 0.5 * n_patch, 0.58, 0.25)) \
            + valley * (0.36 + 0.45 * patch_hi)
        pale = np.maximum(pale, crisp(height + 250.0 * n_mid, 3300.0, 400.0))
        gravel = broken(0.5 + 0.5 * n_gravel, 0.5, 0.3)
        rock = 0.7 * gravel * foot * crisp(0.5 + 0.5 * n_big + 0.3 * n_mid, 0.62, 0.1)
        rock = np.maximum(rock, 0.32 * foot)          # critic round 4: the foothills carry a stony base tone
        rock = np.maximum(rock, 0.32 * bench * crisp(0.5 + 0.5 * n_gravel + 0.25 * n_patch + 0.1 * n_micro, 0.6, 0.04))
        dark = np.zeros(SHAPE)                        # RTSSand: salmon red, traces only
        soot = np.zeros(SHAPE)                        # partial Asphalt: oil and tar only since DarkEarth exists
        earth = np.zeros(SHAPE)                       # DarkEarth: the real dark (damp soil, peat, litter, scorch)
        dust = np.zeros(SHAPE)
        scrub = np.zeros(SHAPE)

        # -- mid-map relief: lit slopes scoured pale, lee slopes sooty, a broken outcrop line along the sharp crests ------
        curve = height - box_blur(height, 10)
        curve3 = height - box_blur(height, 3)
        sun = (0.613, 0.326, 0.719)
        dzdx = -np.gradient(height, axis=0) / QUAD_CM
        dzdy = np.gradient(height, axis=1) / QUAD_CM
        lambert = (-dzdx * sun[0] - dzdy * sun[1] + sun[2]) / np.sqrt(dzdx ** 2 + dzdy ** 2 + 1.0)
        lit = box_blur(lambert - sun[2], 1)
        pale = np.maximum(pale, 0.9 * zone * broken(smoothstep(0.02, 0.12, lit), 0.5, 0.3))
        pale = np.maximum(pale, 0.9 * zone * broken(smoothstep(25.0, 110.0, curve), 0.55, 0.3))
        lee = broken(smoothstep(0.02, 0.12, -lit), 0.55, 0.3) * zone * away_wash
        earth = np.maximum(earth, 0.12 * lee)
        dark = np.maximum(dark, 0.06 * lee)
        ridge_cells = (zone > 0.5) & (curve > 45.0) & (self.wash_d > 1100.0)
        if ridge_cells.any():
            t_lo, t_hi = np.percentile(curve3[ridge_cells], 40.0), np.percentile(curve3[ridge_cells], 85.0)
            line = smoothstep(t_lo, t_hi, curve3) * ridge_cells
            outcrop = crisp(line + 0.4 * n_piece + 0.15 * n_micro, 0.52, 0.07)
            rock = np.maximum(rock, zone * outcrop * (0.5 + 0.3 * (0.5 + 0.5 * n_micro)))
            earth = earth * (1.0 - soft(outcrop, 2))
        rock = np.maximum(rock, 0.25 * zone * broken(smoothstep(12.0, 22.0, slope), 0.55, 0.3))
        hollow = smoothstep(30.0, 120.0, -curve) * zone * away_wash
        hol = broken(hollow * (0.6 + 0.4 * (0.5 + 0.5 * n_fine)), 0.5, 0.35)
        earth = np.maximum(earth, 0.2 * hol)
        dark = np.maximum(dark, 0.07 * hol)
        scrub = np.maximum(scrub, 0.55 * broken(hollow * (0.5 + 0.5 * n_mid), 0.5, 0.3))
        self.out_masks["relief_crest"] = (zone > 0.5) & (curve > 55.0)
        self.out_masks["relief_hollow"] = (zone > 0.5) & (curve < -55.0) & (self.wash_w < 0.2)

        # -- orange drift sand: only thin wind streaks in the lee of the ribs ---------------------------------------------
        lee_streak = crisp(0.5 + 0.5 * streak + 0.5 * smoothstep(0.0, 0.08, -lit), 1.0, 0.07)
        dust = np.maximum(dust, 0.3 * zone * valley * lee_streak * away_wash)
        # critic round 4: zone identity. Long warm sand drifts (40-80 m, along the wind) on the valley floor only.
        drift_big = crisp(0.5 + 0.5 * streak_noise(91, 70.0, 28.0, 35.0) + 0.1 * n_fine, 0.63, 0.05)
        dust = np.maximum(dust, 0.4 * valley * drift_big * away_wash * (1.0 - soft(heap, 6)))
        # deflation pans: a pale crust on the floor; stone pavements beside them
        pan = crisp(self.pan_w, 0.55, 0.2)
        pale = np.maximum(pale, 0.92 * pan)
        pavement = crisp(fbm(162, 20.0, 2), 0.45, 0.05) * zone * (1.0 - pan) * away_wash
        rock = np.maximum(rock, 0.3 * pavement)
        self.out_masks["pavement"] = pavement > 0.5
        dust = dust * (1.0 - crisp(radix, 0.15, 0.1))                  # none round the Radixite field

        # -- the dry wash: a pale bed 5-6 m wide with a clear edge, rock only on the steep bank faces ----------------------
        wd = self.wash_d + 0.6 * wander
        bed = crisp(-wd, -300.0, 25.0) * in_zone
        pale = np.maximum(pale * (1.0 - 0.6 * band(wd, 300.0, 700.0, 30.0) * in_zone), 0.93 * bed)
        wash_bank = band(wd, 300.0, 690.0, 30.0) * in_zone
        rock = rock * (1.0 - wash_bank)
        rock = np.maximum(rock, (0.1 + 0.12 * (lit > 0.0)) * wash_bank * crisp(slope + 3.0 * n_micro, 14.0, 3.0)
                          * crisp(0.5 + 0.5 * n_gravel + 0.3 * n_mid, 0.5, 0.06) * (1.0 - self.wash_gap))
        earth = np.maximum(earth, 0.3 * smoothstep(560.0, 320.0, wd) * wash_bank * (1.0 - self.wash_gap)
                           * crisp(0.5 + 0.5 * n_fine + 0.2 * n_micro, 0.38, 0.1))
        self.out_masks["wash_bar"] = (self.wash_bars * in_zone) > 0.5
        self.out_masks["ditch_lip"] = (band(self.road_dist, 1030.0, 1130.0, 20.0) * self.ditch_run) > 0.5
        self.out_masks["wash_foot"] = (band(self.wash_d, 270.0, 400.0, 20.0) * in_zone) > 0.5

        # -- gullies: pale bed, rock on both bank faces, a sooty line at the bank foot ------------------------------------
        in_gully = self.gully_w > 0.02
        gd = self.gully_d + 0.6 * wander
        gully_bed = crisp(-gd, -165.0, 20.0) * in_gully
        pale = np.maximum(pale, 0.85 * gully_bed)
        rock = np.maximum(rock, 0.93 * band(gd, 170.0, 460.0, 25.0) * in_gully)
        earth = np.maximum(earth, 0.6 * band(gd, 140.0, 250.0, 20.0) * in_gully)

        # -- peat bogs: soot body, tar core, a rock lip, a cracked pale rim in 3-6 m pieces round a third of the edge ------
        bw = self.bog_w
        body = broken(bw, 0.45, 0.15)
        lip = body * (1.0 - np.roll(body, 2, axis=0)) * crisp(0.5 + 0.5 * n_piece, 0.3, 0.08)      # its north edge
        hag = crisp(self.bog_hag, 0.5, 0.2)
        earth = np.maximum(earth, 0.95 * body * (1.0 - lip) * (1.0 - hag))  # peat
        dark = np.maximum(dark * (1.0 - body), 0.1 * body)
        earth = np.maximum(earth, 0.8 * hag)
        self.out_masks["bog_hag"] = hag > 0.5
        belt = band(soft(body, 3), 0.04, 0.5, 0.03) * (1.0 - body) * crisp(0.5 + 0.5 * n_piece + 0.2 * n_mid, 0.42, 0.05)
        self.out_masks["bog_belt"] = belt > 0.5
        self.out_masks["bog_near"] = soft(body, 4) > 0.02
        self.out_masks["bog_lip"] = (band(soft(body, 1), 0.05, 0.6, 0.03) * (1.0 - body)) > 0.3
        tar = np.zeros(SHAPE)
        rock = np.maximum(rock * (1.0 - body), 0.85 * lip)
        rim = band(soft(body, 3), 0.04, 0.5, 0.03) * (1.0 - body)                                   # about 3 m outside
        pale = np.maximum(pale * (1.0 - body), 0.92 * rim * crisp(0.5 + 0.5 * n_piece + 0.15 * n_micro, 0.7, 0.05))
        scrub = np.maximum(scrub * (1.0 - body), 0.15 * hag)
        dust = dust * (1.0 - body)
        scrub = np.maximum(scrub, 0.12 * rim * crisp(0.5 + 0.5 * n_fine, 0.7, 0.08))

        # -- dead woods floor: sooty needle litter broken at 1-3 m with stones showing, densest under the dense woods ------
        litter = np.clip(0.75 * woods + 0.35 * thin, 0.0, 1.0)
        pale = pale * (1.0 - 0.85 * crisp(litter, 0.3, 0.2))
        lit_b = broken(litter * (0.55 + 0.45 * (0.5 + 0.5 * n_fine)), 0.42, 0.35)
        lit_p = lit_b * crisp(0.5 + 0.5 * n_micro + 0.2 * n_tiny, 0.48, 0.06)
        dark = np.maximum(dark, 0.08 * lit_b)
        rock = np.maximum(rock, 0.28 * lit_p)
        # critic round 4: 0.15 is invisible; near-black litter in 2-3 m patches under the densest clumps only
        scrub = np.maximum(scrub, soft(self.regions_like("survivors_"), 3) * broken(0.5 + 0.5 * n_mid, 0.4, 0.3))
        self.out_masks["woods_dense"] = woods > 0.5
        self.out_masks["woods_thin"] = thin > 0.5
        self.out_masks["scrub"] = soft(self.regions_like("survivors_"), 4) > 0.5

        # -- spoil heaps: bare broken rock with a pale fines skirt streaked downwind ---------------------------------------
        heap_s = soft(heap, 4)
        rock = np.maximum(rock, 0.9 * broken(heap_s, 0.45, 0.3))
        pale = np.maximum(pale, 0.85 * band(soft(heap, 7), 0.06, 0.5, 0.05) * crisp(0.5 + 0.5 * streak + 0.3 * grit, 0.45, 0.1))

        # -- rock faces and their aprons: a broken gravel band along every lip and foot -----------------------------------
        # full rock only on and beside the faces; a steep bank elsewhere (ramp sides) gets 0.35 and scree scatter instead
        by_face = np.clip(soft(grey((self.cliff_w > 0.1).astype(np.float64), 8, np.maximum), 2) + cliff_core, 0.0, 1.0)
        steep = smoothstep(30.0, 42.0, slope + 6.0 * n_fine)
        rock = rock * (1.0 - steep * (1.0 - by_face))
        rock = np.maximum(rock, np.maximum(steep * (0.35 + 0.65 * by_face), cliff_core))
        self.out_masks["steep_bank"] = (slope > 24.0) & (by_face < 0.1) & self.inside
        apron = soft(grey((self.cliff_w > 0.3).astype(np.float64), 5, np.maximum), 2) * (1.0 - cliff_core)
        rock = np.maximum(rock, 0.8 * broken(apron * (0.6 + 0.4 * (0.5 + 0.5 * n_fine)), 0.42, 0.35))
        rock = np.maximum(rock, 0.55 * crisp(soft(self.out_masks["viaduct_rubble"], 2), 0.5, 0.15))

        # -- road: a compacted pale shoulder 3-4 m wide with a ragged outer edge, a sooty ditch in 5-15 m dashes -----------
        rd_any = np.minimum(self.road_dist, self.road2_dist)
        shoulder = crisp(rd_any, 395.0, 20.0) * (1.0 - crisp(rd_any + wander, 790.0, 35.0))
        pale = np.maximum(pale, 0.92 * shoulder)
        rock = rock * (1.0 - 0.8 * shoulder)
        ditch = crisp(self.ditch_w, 0.5, 0.25)
        earth = np.maximum(earth, 0.5 * ditch)
        pale = pale * (1.0 - 0.9 * ditch)
        pale = pale * (1.0 - 0.55 * band(rd_any + wander, 800.0, 1500.0, 80.0))
        pale = np.maximum(pale, 0.7 * broken(smoothstep(330.0, 200.0, self.track_dist), 0.5, 0.35))
        paving = 0.3 * crisp(-rd_any, -380.0, 40.0)

        # -- yards: concrete slabs with chipped edges, broken-out holes showing rubble, sand drifted over the windward side
        yard = soft(self.region("base_a_yard", "checkpoint_yard"), 3)
        paved = soft(self.region("base_a_yard"), 3)
        slab_cells = crisp(0.5 + 0.5 * fbm(84, 9.0, 2), 0.42, 0.05)
        slabs = crisp(paved * (0.5 + 0.5 * slab_cells) + 0.2 * n_micro + 0.12 * n_tiny, 0.62, 0.03)
        holes = crisp(0.5 + 0.5 * n_gravel + 0.25 * n_micro, 0.72, 0.04)
        drift = crisp(0.5 + 0.5 * streak + 0.25 * grit, 0.68, 0.06)
        rock = np.maximum(rock, 0.75 * slabs * holes)
        slabs = slabs * (1.0 - holes)
        pale = np.maximum(pale, 0.85 * yard * drift)
        pale = np.maximum(pale, 0.6 * yard * (1.0 - slabs) * crisp(0.5 + 0.5 * n_fine, 0.4, 0.1))
        paving = np.maximum(paving, 0.92 * slabs * (1.0 - 0.9 * drift))
        # the worn ground round the player's start
        hq = self.feature("hq").points[0]
        wear = smoothstep(3200.0, 900.0, self.dist_point(hq) + 500.0 * n_mid)

        soot = soot * (1.0 - bed) * (1.0 - gully_bed)
        earth = earth * (1.0 - bed) * (1.0 - gully_bed)

        # ==== realism round ============================================================================================
        pads_m = soft(self.out_masks["pads"], 2)
        # the wash carries on as a pale cobbled bed where the ground could not be cut (viaduct, road ends)
        thru = crisp(-wd, -280.0, 25.0) * (1.0 - in_zone) * (1.0 - pads_m) * crisp(self.road_dist, 700.0, 60.0) * (1.0 - cliff_core)
        pale = np.maximum(pale, 0.9 * thru)
        self.out_masks["wash_bar"] = self.out_masks["wash_bar"] | ((thru > 0.5) & (n_fine > 0.0))
        # tributary rills: pale bed, a fan of fines where each reaches the wash and where each gully leaves the slope
        td = self.trib_d + 0.4 * wander
        rill = crisp(-td, -130.0, 25.0) * (1.0 - pads_m) * crisp(np.minimum(self.road_dist, self.road2_dist), 480.0, 40.0) * (1.0 - cliff_core)
        rill = rill * (1.0 - crisp(soft(body, 2), 0.05, 0.04))        # a bog drains from its rim, not through its middle
        pale = np.maximum(pale, 0.85 * rill)
        soot = soot * (1.0 - rill)
        earth = earth * (1.0 - rill)
        fan = np.zeros(SHAPE)
        for line in self.tribs:
            fan = np.maximum(fan, smoothstep(650.0, 250.0, self.dist_point(line[0]) + 200.0 * n_fine))
            fan = np.maximum(fan, smoothstep(520.0, 200.0, self.dist_point(line[-1]) + 200.0 * n_fine))
        fan = fan * (1.0 - pads_m) * crisp(np.minimum(self.road_dist, self.road2_dist), 480.0, 40.0) * (1.0 - cliff_core) * (1.0 - crisp(self.bog_w, 0.3, 0.1))
        pale = np.maximum(pale, 0.75 * crisp(fan + 0.25 * n_micro, 0.5, 0.08))
        self.out_masks["fan"] = fan > 0.5
        self.out_masks["headwall"] = (self.trib_d < 260.0) & (self.road_dist > 430.0) & (self.road_dist < 640.0)
        # ramp feet: a wash fan of fines and small stones below each ramp
        ramps_m = self.out_masks["ramps"]
        foot_fan = np.zeros(SHAPE, dtype=bool)
        rr, cc = np.nonzero(ramps_m)
        if rr.size:
            from collections import deque
            seen = np.zeros(SHAPE, dtype=bool)
            for r0, c0 in zip(rr[::40], cc[::40]):
                if seen[r0, c0]:
                    continue
                queue, cells = deque([(r0, c0)]), []
                seen[r0, c0] = True
                while queue:
                    r, c = queue.popleft()
                    cells.append((r, c))
                    for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        r2, c2 = r + dr, c + dc
                        if 0 <= r2 < V and 0 <= c2 < V and ramps_m[r2, c2] and not seen[r2, c2]:
                            seen[r2, c2] = True
                            queue.append((r2, c2))
                cells = np.array(cells)
                hs = height[cells[:, 0], cells[:, 1]]
                low_cells = cells[hs < hs.min() + 120.0]
                foot_fan[low_cells[:, 0], low_cells[:, 1]] = True
        foot_fan = soft(grey(foot_fan.astype(np.float64), 9, np.maximum), 3) * (1.0 - cliff_core) * crisp(self.road_dist, 480.0, 40.0)
        foot_fan = foot_fan * (height < box_blur(height, 12) + 60.0)
        pale = np.maximum(pale, 0.8 * crisp(foot_fan + 0.3 * n_fine + 0.15 * n_micro, 0.55, 0.08))
        self.out_masks["ramp_fan"] = foot_fan > 0.5
        # dead woods: the whole footprint stony and dark (three rounds of faint litter never showed)
        wood_f = crisp(soft(self.regions_like("fh_woods_") | self.regions_like("bench_woods_"), 5) + 0.25 * n_fine, 0.4, 0.08)
        thin_f = crisp(thin + 0.25 * n_fine, 0.55, 0.08)
        floor = np.maximum(wood_f, 0.6 * thin_f) * (1.0 - cliff_core)
        pale = pale * (1.0 - 0.9 * floor)
        rock = np.maximum(rock, 0.4 * wood_f * (1.0 - cliff_core))
        rock = np.maximum(rock, 0.3 * thin_f * (1.0 - cliff_core))
        # the floor itself: dark earth and litter over most of the dense woods, stones showing through the gaps;
        # thinner and patchier under the thin woods, fading out over a few metres past the last trunks
        floor_gap = crisp(0.5 + 0.5 * n_gravel + 0.2 * n_micro, 0.24, 0.07)
        earth = np.maximum(earth, (0.78 + 0.18 * crisp(0.5 + 0.5 * n_piece, 0.5, 0.15)) * wood_f * floor_gap * (1.0 - cliff_core))
        earth = np.maximum(earth, 0.5 * thin_f * crisp(0.5 + 0.5 * n_piece + 0.2 * n_micro, 0.5, 0.08) * (1.0 - cliff_core))
        earth = np.maximum(earth, 0.3 * crisp(soft(wood_f, 4), 0.25, 0.2) * (1.0 - wood_f) * (1.0 - cliff_core)
                           * crisp(0.5 + 0.5 * n_fine, 0.45, 0.1))
        # dark patches under the dense woods only: on the thin woods they read as smoke shadows on open ground
        ring = grey((woods > 0.5).astype(np.float64), 12, np.maximum) > 0
        self.out_masks["woods_ring"] = ring & ~(woods > 0.35) & self.inside
        # craters: scorched bowl, pale ejecta thrown out in rays
        cr = self.crater_r + 0.12 * n_micro
        scorch = crisp(-cr, -0.62, 0.08)
        ejecta = band(cr, 0.95, 2.1, 0.08) * crisp(0.5 + 0.5 * n_micro + 0.25 * n_tiny, 0.45, 0.06)
        pale = np.maximum(pale * (1.0 - scorch), 0.85 * ejecta)
        rock = np.maximum(rock * (1.0 - scorch), 0.5 * band(cr, 0.6, 1.0, 0.06))
        earth = np.maximum(earth * (1.0 - band(cr, 0.95, 2.1, 0.08)), 0.92 * scorch)
        self.out_masks["crater_rim"] = (cr > 0.8) & (cr < 1.45)
        self.out_masks["crater"] = self.crater_r < 1.5
        # wear: compacted ground under every track, khaki on the pale bench and pale on the valley floor; churned
        # ground where the tracks leave the road and at ramp feet
        wd_t = self.wear_d + 0.5 * wander
        worn = crisp(-wd_t, -170.0, 30.0) * (1.0 - cliff_core)
        wide = crisp(-wd_t, -420.0, 60.0) * crisp(-self.road_dist, -1500.0, 200.0) * (1.0 - cliff_core)
        worn = np.maximum(worn, wide * crisp(0.5 + 0.5 * n_fine, 0.4, 0.1))
        pale = np.maximum(pale * (1.0 - worn * bench), 0.7 * worn * (1.0 - bench))
        rock = np.maximum(rock * (1.0 - worn), 0.25 * worn * bench)
        scrub = scrub * (1.0 - worn)
        soot = soot * (1.0 - worn)
        earth = earth * (1.0 - 0.85 * worn)
        dust = dust * (1.0 - worn)
        # the start: a worn apron round the buildings instead of untouched ground
        apron_hq = crisp(smoothstep(2600.0, 900.0, self.dist_point(hq) + 500.0 * n_mid) + 0.3 * n_fine, 0.5, 0.08)
        pale = pale * (1.0 - 0.6 * apron_hq)
        rock = np.maximum(rock, 0.22 * apron_hq)
        # mine yard and checkpoint: oil patches at the parking spots, pale fines spilled between the sheds
        work = soft(self.region("base_a_yard", "checkpoint_yard"), 3)
        oil = crisp(noise(191, 3.0) + 0.3 * n_tiny, 0.78, 0.04) * work
        soot = np.maximum(soot, 0.88 * oil)
        pale = np.maximum(pale, 0.85 * crisp(noise(192, 7.0), 0.55, 0.06) * work * (1.0 - oil) * (1.0 - paving))
        # ===============================================================================================================
        # the Base B road and its shoulder stay clear of woods floor, scorch, scrub and dust paint
        clear2 = crisp(self.road2_dist, 760.0, 60.0)
        earth, soot, scrub, dust = earth * clear2, soot * clear2, scrub * clear2, dust * clear2
        rock = rock * (0.25 + 0.75 * clear2)
        self.out_masks["wear"] = self.out_masks["wear"] | (self.road2_dist < 650.0)
        asphalt = np.maximum(np.maximum(paving, tar), soot)
        iron, ochre, slate = self.mineral_layers(locals())
        w = {"Desert": np.ones(SHAPE)}
        for name, layer in (("MountainTop", pale), ("GraySand", dust), ("RTSSand", dark), ("GrassCastle", scrub),
                            ("SlateScree", slate), ("Ochre", ochre), ("RockDesert", rock), ("IronOxide", iron),
                            ("DarkEarth", earth), ("Asphalt", asphalt)):
            layer = np.clip(0.5 * layer + 0.5 * box_blur(layer, 1), 0.0, 1.0)
            for other in w:
                w[other] = w[other] * (1.0 - layer)
            w[name] = layer
        total = sum(w.values())
        budget = 255 - LAYER_TRACE * len(LAYERS)
        ints = {name: np.floor(w[name] / total * budget).astype(np.int32) + LAYER_TRACE for name in LAYERS}
        ints["Desert"] += 255 - sum(ints.values())
        self.W = {name: ints[name].astype(np.uint8) for name in LAYERS}
        assert int(sum(v.astype(np.int32) for v in self.W.values()).min()) == 255

    def mineral_layers(self, f):
        """Mineral round (2026-11-17): three layers that break the large single-layer patches (pale MountainTop on the
        bench and foothills, warm Desert on the valley floor). SlateScree: cool grey gravel sheets 30-60 m across on
        the pale ground and as scree tongues below the faces. Ochre: yellow-brown limonite stains on the valley floor
        and the foothills, the halo of every iron showing, the fans of the rills. IronOxide: vivid red, an accent in
        named places only: tailings tongues at the three adits, seams on the spoil heaps, a few thin vein outcrops
        across the foothills, rust seeps at cliff feet, the gully banks. None of them on roads, shoulders, tracks,
        yards, beds, the start apron or the woods floor. Returns (iron, ochre, slate) weights."""
        broken, grit, wander = f["broken"], f["grit"], f["wander"]
        n_mid, n_fine, n_micro, n_piece = f["n_mid"], f["n_fine"], f["n_micro"], f["n_piece"]
        bench, foot, valley, cliff_core = f["bench"], f["foot"], f["valley"], f["cliff_core"]
        open_ = crisp(f["rd_any"], 950.0, 80.0) * (1.0 - f["worn"]) * (1.0 - crisp(f["work"], 0.2, 0.15))             * (1.0 - f["bed"]) * (1.0 - f["gully_bed"]) * (1.0 - f["thru"]) * (1.0 - f["rill"]) * (1.0 - f["apron_hq"])             * (1.0 - f["pads_m"]) * (1.0 - crisp(f["floor"], 0.3, 0.2)) * (1.0 - crisp(f["body"], 0.3, 0.2))             * (1.0 - f["scorch"]) * crisp(self.track_dist, 420.0, 60.0) * (1.0 - crisp(f["radix"], 0.2, 0.15))
        ground = open_ * (1.0 - cliff_core)

        # -- slate: sheets on the pale ground, scree tongues on the aprons -------------------------------------------
        sheet = 0.5 + 0.5 * fbm(201, 48.0, 3) + 0.1 * n_mid
        slate = 0.88 * broken(sheet, 0.63, 0.3) * np.clip(bench + 0.9 * foot, 0.0, 1.0)
        slate = np.maximum(slate, 0.8 * broken(sheet, 0.74, 0.3) * valley)
        slate = np.maximum(slate, 0.7 * broken(f["apron"] * (0.5 + 0.5 * fbm(205, 22.0, 2)), 0.42, 0.3))
        slate = slate * ground

        # -- iron: named places only -----------------------------------------------------------------------------------
        iron = np.zeros(SHAPE)
        for fid in ("adit_1", "adit_2", "adit_3"):
            d = self.dist_point(self.feature(fid).points[0]) + 2.2 * wander
            iron = np.maximum(iron, 0.95 * crisp(-d, -1500.0, 110.0) * crisp(0.5 + 0.5 * n_piece + 0.2 * n_micro, 0.42, 0.06))
        heap_s = f["heap_s"]
        seam = crisp(0.5 + 0.5 * streak_noise(203, 34.0, 3.2, 112.0) + 0.08 * n_micro, 0.7, 0.04)
        iron = np.maximum(iron, 0.95 * crisp(heap_s, 0.5, 0.2) * seam)
        vein = crisp(0.5 + 0.5 * streak_noise(204, 110.0, 3.6, 118.0) + 0.05 * n_fine, 0.9, 0.015)
        vein = vein * crisp(0.5 + 0.5 * fbm(206, 90.0, 2), 0.6, 0.05) * np.clip(foot + 0.6 * valley + 0.5 * bench, 0.0, 1.0) * self.inside
        iron = np.maximum(iron, 0.92 * vein)
        seep = crisp(f["apron"], 0.4, 0.12) * crisp(0.5 + 0.5 * fbm(207, 18.0, 2) + 0.15 * n_micro, 0.82, 0.03)
        iron = np.maximum(iron, 0.85 * seep)
        iron = np.maximum(iron, 0.8 * band(f["gd"], 150.0, 330.0, 25.0) * f["in_gully"] * crisp(0.5 + 0.5 * n_piece, 0.55, 0.08))
        iron_open = open_ * (1.0 - 0.6 * cliff_core)
        iron = iron * np.maximum(iron_open, crisp(heap_s, 0.5, 0.2) * crisp(f["rd_any"], 950.0, 80.0))

        # -- ochre: stains on the warm ground, the halo of the iron, the rill fans -------------------------------------
        stain = 0.5 + 0.5 * fbm(202, 36.0, 3) + 0.1 * n_mid
        ochre = 0.82 * broken(stain, 0.66, 0.3) * np.clip(valley + 0.55 * foot, 0.0, 1.0)
        ochre = np.maximum(ochre, 0.6 * broken(stain, 0.78, 0.3) * bench)
        halo = soft(grey((iron > 0.4).astype(np.float64), 4, np.maximum), 3)
        ochre = np.maximum(ochre, 0.85 * crisp(halo + 0.3 * grit, 0.4, 0.1))
        ochre = np.maximum(ochre, 0.7 * crisp(f["fan"] + 0.25 * n_micro, 0.55, 0.08))
        ochre = np.maximum(ochre, 0.55 * f["wash_bank"] * crisp(0.5 + 0.5 * n_piece + 0.2 * n_micro, 0.6, 0.06))
        ochre = ochre * ground * (1.0 - 0.9 * crisp(slate, 0.4, 0.2))
        self.out_masks["iron"] = iron > 0.5
        return iron, ochre, slate

    # ---- checks -------------------------------------------------------------------------------------------------
    def reach(self, start, blocked):
        """Flood fill over cells the nav could walk (slope under the limit, with a 2-degree safety margin)."""
        r0, c0 = self.raster.to_rc(start)
        seen = np.zeros(SHAPE, dtype=bool)
        seen[r0, c0] = True
        open_ = ~blocked
        while True:
            grow = seen.copy()
            grow[1:, :] |= seen[:-1, :]
            grow[:-1, :] |= seen[1:, :]
            grow[:, 1:] |= seen[:, :-1]
            grow[:, :-1] |= seen[:, 1:]
            grow &= open_
            if grow.sum() == seen.sum():
                return seen
            seen = grow

    def check(self):
        lines = []
        slope = self.slope
        steep = slope > NAV_MAX_SLOPE_DEG - 2.0
        ramps = self.out_masks["ramps"]
        hq = self.feature("hq").points[0]
        road = [0.0, 0.0]
        road = [float(v) for v in self.path("valley_road").pts[len(self.path("valley_road").pts) // 2]]

        def at(p):
            r, c = self.raster.to_rc(p)
            return self.Z[r, c]

        for fid in ("hq", "b_gamma", "base_a", "artillery_battery", "checkpoint", "survey_post", "adit_2"):
            p = self.feature(fid).points[0]
            lines.append("z %-18s %7.0f cm (sketch %5.1f m)" % (fid, at(p), self.E[self.raster.to_rc(p)] / 100.0))
        kc = self.spec.by_id["base_b_knoll"].geom.centre
        lines.append("z %-18s %7.0f cm" % ("base_b knoll", at(kc)))
        lines.append("z %-18s %7.0f cm" % ("road mid", at(road)))
        for f in self.spec.features:
            if f.kind == "ramp":
                m = self.terrain.opening_masks[f.id]
                lines.append("ramp %-14s slope deg: p50 %.1f p95 %.1f max %.1f" % (
                    f.id, np.percentile(slope[m], 50), np.percentile(slope[m], 95), slope[m].max()))
        rz = (self.relief_zone > 0.5) & self.inside
        lines.append("mid relief: height range %.0f..%.0f cm, slope deg p50 %.1f p99 %.1f max %.1f  (nav limit %.0f)" % (
            self.Z[rz].min(), self.Z[rz].max(), np.percentile(slope[rz], 50), np.percentile(slope[rz], 99), slope[rz].max(), NAV_MAX_SLOPE_DEG))
        rd = self.road_dist < 400.0
        lines.append("valley road slope deg: p95 %.1f max %.1f" % (np.percentile(slope[rd & self.inside], 95), slope[rd & self.inside].max()))
        blocked = steep | ~self.inside
        full = self.reach(hq, blocked)
        lines.append("reach from hq, everything open: road %s, base_a %s, knoll top %s, battery %s" % (
            full[self.raster.to_rc(road)], full[self.raster.to_rc(self.feature("base_a").points[0])],
            full[self.raster.to_rc(kc)], full[self.raster.to_rc(self.feature("artillery_battery").points[0])]))
        ridge_ramps = self.openings("ramp_east", "ramp_main", "ramp_sally", "ramp_west")
        shut = self.reach(hq, blocked | grey(ridge_ramps.astype(np.float64), 12, np.maximum).astype(bool) | (self.Y < -29000.0 * SCALE))
        lines.append("reach from hq, ridge ramps + west shoulder shut: road %s  (must be False: the escarpment holds)" %
                     shut[self.raster.to_rc(road)])
        knoll_ramps = self.openings("knoll_ramp_w", "knoll_ramp_ne")
        shut2 = self.reach(road, blocked | grey(knoll_ramps.astype(np.float64), 12, np.maximum).astype(bool))
        lines.append("reach from road, knoll ramps shut: knoll top %s  (must be False)" % shut2[self.raster.to_rc(kc)])
        for name in ("ramp_east", "ramp_main", "ramp_sally", "ramp_west"):
            others = self.openings(*[n for n in ("ramp_east", "ramp_main", "ramp_sally", "ramp_west") if n != name])
            one = self.reach(hq, blocked | grey(others.astype(np.float64), 12, np.maximum).astype(bool) | (self.Y < -29000.0 * SCALE))
            lines.append("only %-10s open: road %s  (must be True)" % (name, one[self.raster.to_rc(road)]))
        for name in ("knoll_ramp_w", "knoll_ramp_ne"):
            other = self.openings("knoll_ramp_ne" if name == "knoll_ramp_w" else "knoll_ramp_w")
            one = self.reach(road, blocked | grey(other.astype(np.float64), 12, np.maximum).astype(bool))
            lines.append("only %-12s open: knoll top %s  (must be True)" % (name, one[self.raster.to_rc(kc)]))
        lines.append("walkable share of play area: %.1f %%" % (100.0 * (full & self.inside).sum() / self.inside.sum()))
        self.reach_full = full
        text = "\n".join(lines)
        with open(os.path.join(OUT, "check.txt"), "w") as handle:
            handle.write(text)
        print(text)

    # ---- output -------------------------------------------------------------------------------------------------
    @staticmethod
    def to_grid(a):
        """[row north..south, col west..east] -> [iy, ix] with ix along +X and iy along +Y."""
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        files = {"height": os.path.join(OUT, "Ore_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "Ore_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        masks_file = os.path.join(OUT, "Ore_masks.npz").replace("\\", "/")
        np.savez_compressed(masks_file, **{k: self.to_grid(np.asarray(v)) for k, v in self.out_masks.items()})
        files["masks"] = masks_file

        spec = self.spec
        regions = []
        for reg in spec.regions:
            regions.append({"id": reg.id, "kind": reg.kind, "label": reg.label, "passable": reg.passable,
                            "shape": reg.geom.kind, "width": reg.geom.width,
                            "centre": [float(v) for v in reg.geom.centre],
                            "points": [[float(a), float(b)] for a, b in reg.geom.pts], "raw": reg.raw})
        features = []
        for f in spec.features:
            entry = {"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                     "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw,
                     "dir": [float(v) for v in f.dir] if f.dir is not None else None}
            if f.geom is not None:
                entry["shape"] = {"kind": f.geom.kind, "centre": [float(v) for v in f.geom.centre],
                                  "points": [[float(a), float(b)] for a, b in f.geom.pts]}
            if f.kind == "ramp" and getattr(f, "centreline", None) is not None:
                entry["centreline"] = [[float(a), float(b)] for a, b in f.centreline]
            features.append(entry)
        paths = [{"id": p.id, "kind": p.kind, "label": p.label, "width": p.width,
                  "points": [[float(a), float(b)] for a, b in p.pts]} for p in spec.paths]
        layout = {
            "map": MAP_PATH, "spec": SPEC_FILE.replace("\\", "/"),
            "landscape": {"components_x": COMPONENTS, "components_y": COMPONENTS, "quads_per_component": QUADS,
                          "quad_cm": QUAD_CM, "verts_x": V, "verts_y": V, "size_x": SIZE, "size_y": SIZE,
                          "corner": [-SIZE / 2.0, -SIZE / 2.0]},
            "bounds": {"xmin": spec.xmin, "xmax": spec.xmax, "ymin": spec.ymin, "ymax": spec.ymax},
            "z_range": [float(self.Z.min()), float(self.Z.max())], "scale": SCALE,
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths,
            "wear_tracks": [[[round(float(x)), round(float(y))] for x, y in p] for p in self.wear_runs()],
            "base_b_road": [[round(float(x)), round(float(y))] for x, y in self.road2_line],
            "craters": [[round(c[0]), round(c[1]), round(c[2]), bool(c[3])] for c in self.craters],
            "tributaries": [[[round(float(x)), round(float(y))] for x, y in t] for t in self.tribs],
        }
        with open(os.path.join(OUT, "Ore_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        self.preview()
        z = self.Z[self.inside]
        print("heights cm inside: min %.0f max %.0f; whole landscape: min %.0f max %.0f" % (z.min(), z.max(), self.Z.min(), self.Z.max()))
        for name in LAYERS:
            print("layer %-12s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"Desert": (176, 150, 110), "GrassCastle": (120, 120, 70), "RockDesert": (104, 92, 80),
                   "GraySand": (150, 146, 134), "RTSSand": (206, 186, 146), "MountainTop": (128, 122, 116),
                   "Asphalt": (70, 70, 74), "DarkEarth": (58, 50, 44)}
        rgb = np.zeros((V, V, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.8 + 1.1 * (gx + gy) / 1.4, 0.3, 1.4)
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        r0, c0 = self.raster.to_rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.raster.to_rc([self.spec.xmin, self.spec.ymax])
        for r in (r0, r1):
            rgb[r, c0:c1] = (255, 220, 0)
        for c in (c0, c1):
            rgb[r0:r1, c] = (255, 220, 0)
        Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(OUT, "preview.png"))
        img = np.zeros((V, V, 3), dtype=np.uint8)
        img[..., 1] = np.clip(self.Z / 4000.0 * 255, 0, 255)
        img[..., 2] = np.where(self.reach_full, 90, 0)
        img[..., 0] = np.clip(self.slope * 4, 0, 255)
        img[self.slope > NAV_MAX_SLOPE_DEG] = (255, 0, 255)
        Image.fromarray(img).save(os.path.join(OUT, "preview_slope.png"))


def main():
    gen = Gen()
    gen.build_heights()
    gen.build_wear()
    gen.build_layers()
    os.makedirs(OUT, exist_ok=True)
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
