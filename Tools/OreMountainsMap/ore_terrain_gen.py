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
LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop", "Asphalt"]
LAYER_TRACE = 2

NAV_MAX_SLOPE_DEG = 44.0                # agent_max_slope of all four RecastNavMesh actors in the template
CLIFF_R = 8                             # cells: half width of the band a slope is sharpened over
CREST_EXTRA = 5200.0                    # the border crest keeps rising past the play edge (cm above its 35 m)
NORTH_RISE = 2200.0
GULLY_DEPTH = 130.0
BOG_DEPTH = 45.0
RELIEF_GAIN = 1.55                      # mid-map relief: ridges to about 5.5 m, hollows to about 3 m


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
        self.ammo_dist = self.dist_line(self.path("ammo_route").pts)
        self.depot_dist = self.dist_line(self.path("depot_track").pts)
        track = np.minimum(self.ammo_dist, self.depot_dist)
        for pid in ("flank_w", "flank_e"):
            track = np.minimum(track, self.dist_line(self.path(pid).pts) + 150.0)
        self.track_dist = track
        self.out_masks["road"] = self.road_dist < 1000.0
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
            return height * np.exp(-(u / (length / 2.0)) ** 2 - (v / (width / 2.0)) ** 2)

        # (x, y, length, width, height, yaw): low ridges and knobs; negative heights are hollows
        for form in ((-4500, -13000, 5000, 1500, 300, 85), (-5300, 9500, 4200, 1300, 260, 100), (3200, -9500, 4500, 1400, 290, 70),
                     (5200, 17500, 3800, 1200, 250, 95), (11000, 9000, 6000, 1900, 380, 60), (16000, -20000, 6000, 1900, 350, 120),
                     (8000, -16000, 4500, 1600, 300, 45), (20000, -3000, 5000, 1700, 330, 20), (13500, 22000, 4500, 1500, 280, 110),
                     (-5500, -2500, 2600, 2400, -190, 0), (2900, 3200, 2400, 2000, -160, 0), (7500, 2500, 3000, 2600, -200, 30),
                     (10500, -8500, 2800, 2400, -170, 0)):
            relief = relief + ridge(*form)
        # a dry wash between the road and the foothills: 1.4 m deep, 14 m wide, with a low bank on its north side
        wash_pts = np.array([[2600, -29500], [1900, -19000], [2700, -11000], [2000, -2500], [2900, 5000], [2300, 12500], [3000, 19500]], float)
        wash_d = self.dist_line(wash_pts) + 160.0 * noise(133, 14.0)
        wash = smoothstep(760.0, 180.0, wash_d)
        relief = relief - 140.0 * wash + 45.0 * np.exp(-((wash_d - 950.0) / 320.0) ** 2)
        relief = relief * RELIEF_GAIN
        Z = Z + relief * zone
        self.relief_zone = zone
        self.wash_w = wash * zone
        self.out_masks["wash"] = self.wash_w > 0.4

        # 3. spoil heaps: the pads give a 5 m level top; make them rounded heaps with a small crown
        heaps = self.regions_like("spoil_heap_")
        Z += 140.0 * smoothstep(0.55, 1.0, soft(heaps, 6)) * (1.0 + 0.3 * noise(31, 8.0))
        self.out_masks["spoil_heap"] = heaps

        # 4. erosion gullies: shallow V channels (walkable, about 30 % sides)
        gully = np.full(SHAPE, 1e9)
        for reg in spec.regions:
            if reg.id.startswith("gully_"):
                gully = np.minimum(gully, self.dist_line(reg.geom.pts))
        gully += 120.0 * noise(41, 6.0)
        cut = smoothstep(480.0, 60.0, gully) * (1.0 - np.clip(ramp_keep + smoothstep(900.0, 600.0, self.road_dist), 0, 1))
        Z -= GULLY_DEPTH * cut
        self.gully_w = cut
        self.out_masks["gully"] = cut > 0.35

        # 5. peat bogs: sunken, hummocky
        bog = soft(self.regions_like("peat_bog_"), 5)
        Z += bog * (-BOG_DEPTH + 22.0 * noise(51, 5.0)) * (1.0 - smoothstep(900.0, 600.0, self.road_dist))
        self.bog_w = bog
        self.out_masks["peat_bog"] = bog > 0.5

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

    def band_between(self, outer_id, inner_id):
        """Cells inside the outer contour and outside the inner one."""
        el = self.spec.elevation
        geoms = {c.id: c.geom for c in el.contours}
        outer = self.raster.mask_geom(geoms[outer_id])
        inner = self.raster.mask_geom(geoms[inner_id])
        return outer & ~inner

    # ---- paint layers -------------------------------------------------------------------------------------------
    def build_layers(self):
        n_big, n_mid, n_fine = fbm(81, 70.0, 3), fbm(82, 18.0, 2), noise(83, 5.0)
        slope = slope_deg(self.Z)
        self.slope = slope
        height = self.Z

        # How the DryCastle layers look on this map (checked in a top-down capture): Desert = khaki dry ground with
        # orange flecks, GraySand = saturated orange sand, RTSSand = dark red-brown earth, MountainTop = pale grey-tan,
        # RockDesert = grey-brown rock, Asphalt = near black, GrassCastle = green.
        bench = soft(self.region("ridge_bench", "border_crest"), 10)
        # cooler, paler ground on the ridge bench and the high ground
        pale = np.clip(0.8 * bench + 0.3 * smoothstep(500.0, 1500.0, height) + 0.3 * n_big, 0.0, 0.92)
        pale = np.maximum(pale, smoothstep(2900.0, 3700.0, height + 250.0 * n_mid))
        # worn dirt tracks read pale
        pale = np.maximum(pale, 0.65 * smoothstep(330.0, 200.0, self.track_dist) * np.clip(0.7 + 0.5 * n_fine, 0.0, 1.0))
        # orange dust drifts on the valley floor, thin
        dust = 0.4 * smoothstep(0.2, 0.7, n_big + 0.4 * n_mid) * (1.0 - bench) * smoothstep(700.0, 150.0, height)
        # dark earth: peat bogs, gully beds, the stain under the dense dead woods
        woods = soft(self.regions_like("fh_woods_") | self.regions_like("bench_woods_"), 8)
        thin = soft(self.regions_like("dead_thin_") | self.regions_like("dead_face_"), 6)
        dark = 0.9 * smoothstep(0.25, 0.7, self.bog_w + 0.25 * n_mid * (self.bog_w > 0.05))
        dark = np.maximum(dark, 0.7 * smoothstep(0.3, 0.8, self.gully_w))
        dark = np.maximum(dark, (0.4 * woods + 0.15 * thin) * smoothstep(-0.2, 0.5, n_mid + 0.4 * n_fine))
        # mid-map relief: wind-scoured pale crests, darker damp hollows, drifted sand in the dry wash, bare ribs where
        # a swell is steep
        curve = height - box_blur(height, 10)                      # cm above (+) or below (-) the surrounding ground
        zone = self.relief_zone
        pale = np.maximum(pale, 0.7 * zone * smoothstep(25.0, 110.0, curve + 20.0 * n_fine))
        dark = np.maximum(dark, 0.4 * zone * smoothstep(25.0, 110.0, -curve + 20.0 * n_fine) * (1.0 - smoothstep(0.2, 0.6, self.wash_w)))
        # From the RTS camera (pitch -52 to -70) a 4 m swell hardly shades at all, so the paint carries the relief:
        # slopes that face the map's sun (north-north-east, 46 degrees up) are scoured pale, the lee slopes hold darker
        # soil. sun = unit vector towards the sun in (x north, y east, z up).
        sun = (0.613, 0.326, 0.719)
        dzdx = -np.gradient(height, axis=0) / QUAD_CM          # rows run north to south
        dzdy = np.gradient(height, axis=1) / QUAD_CM
        lambert = (-dzdx * sun[0] - dzdy * sun[1] + sun[2]) / np.sqrt(dzdx ** 2 + dzdy ** 2 + 1.0)
        lit = box_blur(lambert - sun[2], 2)
        pale = np.maximum(pale, 0.6 * zone * smoothstep(0.025, 0.14, lit + 0.02 * n_fine))
        dark = np.maximum(dark, 0.5 * zone * smoothstep(0.025, 0.14, -lit + 0.02 * n_fine) * (1.0 - smoothstep(0.2, 0.6, self.wash_w)))
        self.out_masks["relief_crest"] = (zone > 0.5) & (curve > 55.0)
        self.out_masks["relief_hollow"] = (zone > 0.5) & (curve < -55.0) & (self.wash_w < 0.2)
        dust = np.maximum(dust, 0.75 * smoothstep(0.25, 0.8, self.wash_w) * np.clip(0.7 + 0.5 * n_mid, 0.0, 1.0))
        relief_scrub = 0.4 * zone * smoothstep(40.0, 120.0, -curve) * smoothstep(0.0, 0.5, n_mid) * (1.0 - smoothstep(0.2, 0.6, self.wash_w))
        # survivors: scrub pockets, a little dead grass at the bog rims
        scrub = soft(self.regions_like("survivors_"), 4) * np.clip(0.8 + 0.6 * n_mid, 0.0, 1.0)
        scrub = np.maximum(scrub, 0.35 * smoothstep(0.1, 0.4, self.bog_w) * (1.0 - smoothstep(0.5, 0.8, self.bog_w)))
        scrub = np.maximum(scrub, relief_scrub)
        self.out_masks["woods_dense"] = woods > 0.5
        self.out_masks["woods_thin"] = thin > 0.5
        self.out_masks["scrub"] = soft(self.regions_like("survivors_"), 4) > 0.5
        # rock: faces, anything too steep to walk, the spoil heaps and the viaduct rubble
        rock = np.maximum(smoothstep(30.0, 42.0, slope + 6.0 * n_fine), smoothstep(0.35, 0.8, self.cliff_w))
        rock = np.maximum(rock, 0.55 * smoothstep(0.3, 0.8, soft(self.out_masks["viaduct_rubble"], 2)))
        rock = np.maximum(rock, 0.45 * zone * smoothstep(11.0, 19.0, slope + 3.0 * n_fine))
        rock = np.maximum(rock, 0.6 * smoothstep(0.2, 0.7, soft(self.regions_like("spoil_heap_"), 4)) * np.clip(0.7 + 0.6 * n_mid, 0, 1))
        # asphalt: the old valley road (8 m), broken by drift; the checkpoint and the Wismut yard paved in patches
        patch = smoothstep(-0.7, -0.1, 0.7 * fbm(84, 5.0, 2) + 0.5 * n_mid)
        # the road itself is a spline mesh (BP_GroundPathSpline_Road): under it only a dark stain, so its dithered
        # shoulders fade into worn ground
        asphalt = 0.3 * smoothstep(420.0, 300.0, self.road_dist) * patch
        yard = smoothstep(0.35, 0.7, soft(self.region("base_a_yard", "checkpoint_yard"), 4) + 0.25 * n_mid)
        asphalt = np.maximum(asphalt, 0.6 * yard * smoothstep(-0.3, 0.3, n_mid + 0.5 * n_fine))

        w = {"Desert": np.ones(SHAPE)}
        for name, layer in (("MountainTop", pale), ("GraySand", dust), ("RTSSand", dark), ("GrassCastle", scrub),
                            ("RockDesert", rock), ("Asphalt", asphalt)):
            layer = np.clip(box_blur(layer, 1), 0.0, 1.0)
            for other in w:
                w[other] = w[other] * (1.0 - layer)
            w[name] = layer
        total = sum(w.values())
        budget = 255 - LAYER_TRACE * len(LAYERS)
        ints = {name: np.floor(w[name] / total * budget).astype(np.int32) + LAYER_TRACE for name in LAYERS}
        ints["Desert"] += 255 - sum(ints.values())
        self.W = {name: ints[name].astype(np.uint8) for name in LAYERS}
        assert int(sum(v.astype(np.int32) for v in self.W.values()).min()) == 255

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
                   "Asphalt": (70, 70, 74)}
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
    gen.build_layers()
    os.makedirs(OUT, exist_ok=True)
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
