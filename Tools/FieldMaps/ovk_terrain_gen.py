"""Heights + Ore landscape layer weights for Field_OreViadukt (convoy field map, Ore Mountains, owner pick B).

Design source: MapDesigner/ConvoyMaps/Ore/convoy_ore_B.layout.json (owner pick 2026-11-25, 640 x 640 m).
Writes Saved/FieldMaps/OreViadukt/terrain/{OVK_height.raw, OVK_layer_<name>.raw, OVK_layout.json, preview*.png,
check.txt}. ovk_map_build.py (in a throwaway editor) builds the map from these files.

    python Source/Tools/FieldMaps/ovk_terrain_gen.py

The viaduct and the trestle are meshes (Blender, GorgeViaduct): the drawing's flat "ramps" for them are left out of
the height field, so the gorge stays open under them. The gorge walls are sharpened past the 44 deg nav slope, the
river runs in a 3 m channel that only the ford crosses.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "MapDesigner"))
sys.path.insert(0, os.path.join(ROOT, "MapDesigner", "ConvoyMaps", "Ore"))
from mapdesigner.grid import Raster, Terrain  # noqa: E402
from mapdesigner.spec import Spec, dist_to_polyline  # noqa: E402
import gen_convoy_ore as G  # noqa: E402  (gorge axis + catmull, the spec's own generator)

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "ConvoyMaps", "Ore", "convoy_ore_B.layout.json")
OUT = os.path.join(ROOT, "Saved", "FieldMaps", "OreViadukt", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/Field_OreViadukt"
DECK_IDS = ("viaduct", "trestle")       # meshes, not earth

COMPONENTS = 13                         # 819 m: the 640 m play area plus 89 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1              # 820 vertices a side
SIZE = COMPONENTS * QUADS * QUAD_CM     # 81 900 cm
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop", "Asphalt", "DarkEarth",
          "IronOxide", "Ochre", "SlateScree"]
NAV_MAX_SLOPE_DEG = 44.0

RIM, FLOOR_EDGE = 5000.0, 4100.0        # cm from the gorge axis (crossings.json)
FLOOR_Z = -1400.0
RIVER_HALF, BANK = 400.0, 120.0         # channel half width, bank run (3 m over 1.2 m)
RIVER_BED_Z, FORD_BED_Z = -1700.0, -1440.0
FORD_HALF_ALONG = 1000.0
RELIEF_CM = 55.0
BOG_DEPTH = 90.0
MARGIN_RISE = 1500.0


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


def crisp(v, t, w=0.06):
    return smoothstep(t - w, t + w, v)


def slope_deg(Z):
    gx = np.gradient(Z, axis=1) / QUAD_CM
    gy = np.gradient(Z, axis=0) / QUAD_CM
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def extended_axis():
    """The spec's gorge axis (Catmull-Rom), run straight on past both ends to the landscape edge."""
    line = np.asarray(G.catmull(G.G_AXIS, 200.0), float)
    a0 = line[0] + (line[0] - line[3]) / np.linalg.norm(line[0] - line[3]) * 12000.0
    a1 = line[-1] + (line[-1] - line[-4]) / np.linalg.norm(line[-1] - line[-4]) * 12000.0
    return np.vstack([a0, line, a1])


class Gen:
    def __init__(self):
        raw = json.load(open(SPEC_FILE, encoding="utf-8"))
        raw["features"] = [f for f in raw["features"] if f["id"] not in DECK_IDS]
        os.makedirs(OUT, exist_ok=True)
        self.nodeck_file = os.path.join(OUT, "_spec_nodecks.layout.json")
        with open(self.nodeck_file, "w", encoding="utf-8") as handle:
            json.dump(raw, handle)
        self.full = Spec(SPEC_FILE)
        self.spec = Spec(self.nodeck_file)
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
        self.axis = extended_axis()

    def region(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for rid in ids:
            m |= self.masks[rid]
        return m

    def openings(self, *ids):
        m = np.zeros(SHAPE, dtype=bool)
        for fid in ids:
            m |= self.terrain.opening_masks[fid]
        return m

    def feature(self, fid):
        return next(f for f in self.full.features if f.id == fid)

    def path(self, pid):
        return next(p for p in self.full.paths if p.id == pid)

    def dist_line(self, pts, reach=8000.0):
        pts = np.asarray(pts, float)
        flat = np.c_[self.X.ravel(), self.Y.ravel()]
        lo = pts.min(axis=0) - reach
        hi = pts.max(axis=0) + reach
        near = (flat[:, 0] >= lo[0]) & (flat[:, 0] <= hi[0]) & (flat[:, 1] >= lo[1]) & (flat[:, 1] <= hi[1])
        d = np.full(flat.shape[0], 1e9)
        d[near] = dist_to_polyline(flat[near], pts)
        return d.reshape(SHAPE)

    def at(self, p):
        r, c = self.raster.to_rc(p)
        return self.Z[r, c]

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        # distance to the gorge axis: exact near the gorge (cells pre-selected by a coarse EDT)
        axis_mask = np.zeros(SHAPE, dtype=bool)
        for x, y in self.axis:
            r, c = self.raster.to_rc((x, y))
            axis_mask[r, c] = True
        coarse = ndimage.distance_transform_edt(~axis_mask) * QUAD_CM
        near = coarse < RIM + 3000.0
        flat = np.c_[self.X[near], self.Y[near]]
        d = np.full(SHAPE, 1e9)
        d[near] = dist_to_polyline(flat, self.axis)
        self.d_axis = d
        # along-axis coordinate around the ford (for the shallow crossing)
        fp, fd, _ = G.axis_frame((-21000, 17700))
        self.ford_c, self.ford_dir = np.asarray(fp), np.asarray(fd)
        along = (self.X - fp[0]) * fd[0] + (self.Y - fp[1]) * fd[1]

        ramp_ids = [f.id for f in self.spec.features if f.kind == "ramp"]
        ramps = self.openings(*ramp_ids)
        self.ramps = ramps
        ramp_keep = soft(ndimage.binary_dilation(ramps, iterations=2).astype(np.float64), 2)

        self.road_dist = self.dist_line(self.path("main_road").pts)
        track = np.full(SHAPE, 1e9)
        for pid in ("ne_track", "sw_track", "trestle_track", "branch_track"):
            track = np.minimum(track, self.dist_line(self.path(pid).pts))
        self.track_dist = track

        # 1. upland: the drawing's field (heaps, knoll) plus low relief away from roads and pads
        flat_zone = soft(self.region("shelf_start", "depot_yard", "shaft_yard") | ramps | (self.road_dist < 1100.0) |
                         (self.track_dist < 400.0), 4)
        relief = fbm(11, 90) * RELIEF_CM + fbm(23, 30) * 18.0
        up = np.where(d > RIM + 500.0, self.E, np.maximum(self.E, 0.0) * (d > RIM))   # heaps only, no contour blend
        Z = up + relief * (1.0 - np.clip(flat_zone, 0.0, 1.0)) * smoothstep(RIM, RIM + 1500.0, d)
        Z -= BOG_DEPTH * soft(self.masks["bog_ne"], 4)

        # deck-end pads: the upland under both ends of each crossing (wing walls included) sits 5 cm under the deck
        crossings = json.load(open(os.path.join(ROOT, "Saved", "FieldMaps", "OreViadukt", "crossings.json")))
        pads = np.zeros(SHAPE, dtype=bool)
        for key, half_w in (("viaduct", 1700.0), ("trestle", 600.0)):
            c = crossings[key]
            yaw_c = math.radians(c["yaw_deg"])
            cdx, cdy = self.X - c["centre_cm"][0], self.Y - c["centre_cm"][1]
            c_along = cdx * math.cos(yaw_c) + cdy * math.sin(yaw_c)
            c_across = -cdx * math.sin(yaw_c) + cdy * math.cos(yaw_c)
            pads |= (np.abs(c_along) >= RIM - 300.0) & (np.abs(c_along) <= c["length_cm"] / 2.0 + 400.0) &                 (np.abs(c_across) <= half_w)
        self.deck_pads = pads
        pad_w = np.clip(soft(ndimage.binary_dilation(pads, iterations=3), 2), 0.0, 1.0)
        Z = Z * (1.0 - pad_w) + (-5.0) * pad_w

        # 2. gorge: sharp rock walls, level floor with talus, a 3 m river channel crossed only at the ford
        wall_t = np.clip((RIM - d) / (RIM - FLOOR_EDGE), 0.0, 1.0)
        wall = FLOOR_Z * (0.12 * wall_t + 0.88 * smoothstep(0.0, 1.0, wall_t))
        wall += noise(31, 6) * 45.0 * (wall_t > 0.05) * (wall_t < 0.95)
        talus = 70.0 * (1.0 - smoothstep(FLOOR_EDGE - 500.0, FLOOR_EDGE, d)) * smoothstep(FLOOR_EDGE - 900.0, FLOOR_EDGE - 400.0, d)
        floor = FLOOR_Z + fbm(41, 25) * 22.0 + talus * 0 + 60.0 * smoothstep(FLOOR_EDGE - 400.0, FLOOR_EDGE, d)
        bed = np.where(np.abs(along) < FORD_HALF_ALONG, FORD_BED_Z, RIVER_BED_Z)
        chan_t = smoothstep(RIVER_HALF, RIVER_HALF + BANK, d)
        floor = np.where(d < RIVER_HALF + BANK, bed * (1.0 - chan_t) + floor * chan_t, floor)
        gz = np.where(d >= RIM, Z, np.where(d >= FLOOR_EDGE, wall, floor))
        Zg = gz

        # 3. ford ramps follow the drawing (their own linear grade, 0 -> -14 m)
        Zg = Zg * (1.0 - ramp_keep) + self.E * ramp_keep

        # 4. camera margin: hills past the play edge, but the gorge and the two road ends run on out
        dx = np.maximum(np.abs(self.X) - self.spec.xmax, 0.0)
        dy = np.maximum(np.abs(self.Y) - self.spec.ymax, 0.0)
        d_edge = np.hypot(dx, dy) / 100.0
        keep_open = np.maximum(1.0 - smoothstep(RIM + 1000.0, RIM + 6000.0, d),
                               1.0 - smoothstep(1500.0, 5000.0, self.road_dist))
        margin = MARGIN_RISE * smoothstep(6.0, 70.0, d_edge) * (0.7 + 0.3 * fbm(61, 60)) * (1.0 - keep_open)
        self.Z = Zg + margin
        self.slope = slope_deg(self.Z)
        self.gorge = d < RIM

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        n1, n2, n3 = noise(71, 60), noise(72, 18), noise(73, 9)
        d = self.d_axis
        W["Desert"] += 1.0
        W["MountainTop"] += 0.55 * crisp(n1, 0.15, 0.25)
        W["GraySand"] += 0.25 * crisp(n2, 0.55, 0.12)
        rock = smoothstep(28.0, 40.0, self.slope)
        # gorge floor: dark damp earth, scree, a little green by the river, oxide streaks
        floor = soft(d < FLOOR_EDGE, 2)
        W["DarkEarth"] += 0.9 * floor * (0.6 + 0.4 * crisp(n2, -0.1, 0.3))
        W["SlateScree"] += 0.7 * floor * crisp(n3, 0.15, 0.2)
        W["GrassCastle"] += 0.7 * soft(d < 1100.0, 3) * crisp(n2, -0.2, 0.3)
        W["IronOxide"] += 0.45 * floor * crisp(n3 + 0.5 * n2, 0.6, 0.05)
        W["SlateScree"] += 1.6 * soft(d < RIVER_HALF + BANK, 1)
        # rim lip: scree where the upland breaks over
        W["SlateScree"] += 0.6 * soft((d > RIM - 200.0) & (d < RIM + 400.0), 2)
        # roads, tracks, yards
        road = 1.0 - smoothstep(500.0, 950.0, self.road_dist)
        W["MountainTop"] += 1.2 * road
        W["Asphalt"] += 0.35 * road * crisp(n3, 0.0, 0.4)
        trk = 1.0 - smoothstep(150.0, 380.0, self.track_dist)
        W["DarkEarth"] += 0.8 * trk * (0.6 + 0.4 * crisp(n3, -0.2, 0.3))
        yards = soft(self.region("depot_yard", "shaft_yard"), 3)
        W["Asphalt"] += 0.8 * yards
        W["MountainTop"] += 0.6 * yards
        W["Ochre"] += 0.4 * yards * crisp(n3, 0.3, 0.2)
        W["MountainTop"] += 0.5 * soft(self.region("shelf_start"), 4)
        # heaps, knoll, crag
        W["SlateScree"] += 1.5 * soft(self.region("heap_n", "knoll_radio"), 3)
        W["RockDesert"] += 2.0 * soft(self.region("crag_exit"), 2)
        # woods + bog: dark earth
        woods = soft(self.region("woods_rim", "woods_n", "woods_sw", "woods_nw", "woods_e"), 4)
        W["DarkEarth"] += 0.9 * woods * (0.6 + 0.4 * crisp(n2, -0.1, 0.3))
        W["DarkEarth"] += 2.2 * soft(self.masks["bog_ne"], 3)
        # metal: ochre + oxide stains around the nodes
        for fid in ("metal_ne", "metal_gorge"):
            for p in self.feature(fid).points:
                dd = np.hypot(self.X - p[0], self.Y - p[1])
                W["Ochre"] += 0.9 * (1.0 - smoothstep(300.0, 1100.0, dd)) * (0.7 + 0.3 * n2)
                W["IronOxide"] += 0.6 * (1.0 - smoothstep(150.0, 450.0, dd)) * crisp(n3, 0.2, 0.2)
        # steep: charcoal rock
        W["RockDesert"] += 2.5 * rock
        total = sum(W.values()) + 1e-9
        out = {}
        acc = np.zeros(SHAPE)
        for name in LAYERS:
            out[name] = np.floor(W[name] / total * 255.0)
            acc += out[name]
        out["Desert"] += 255.0 - acc
        self.W = {name: np.clip(out[name], 0, 255).astype(np.uint8) for name in LAYERS}

    # ---- checks -------------------------------------------------------------------------------------------------
    def reach(self, start, blocked):
        lab, _ = ndimage.label(~blocked)
        r, c = self.raster.to_rc(start)
        return lab == lab[r, c]

    def check(self):
        steep = self.slope > NAV_MAX_SLOPE_DEG
        blocked = steep | ~self.inside
        hq = self.feature("hq").points[0]
        lines = []
        z = self.Z[self.inside]
        lines.append("heights inside: %.0f..%.0f cm; landscape %.0f..%.0f cm" % (z.min(), z.max(), self.Z.min(), self.Z.max()))
        for fid in ("hq", "director", "convoy_entry", "convoy_exit", "convoy_stop", "branch_dest", "radio_post"):
            lines.append("z %-13s %6.0f cm" % (fid, self.at(self.feature(fid).points[0])))
        for f in self.spec.features:
            if f.kind == "ramp":
                m = self.terrain.opening_masks[f.id]
                lines.append("ramp %-13s slope deg p50 %.1f p95 %.1f max %.1f" % (
                    f.id, np.percentile(self.slope[m], 50), np.percentile(self.slope[m], 95), self.slope[m].max()))
        pad_z = self.Z[self.deck_pads & (self.d_axis > RIM + 100.0)]
        lines.append("deck-end pads: landscape %.0f..%.0f cm under the deck ends (deck top +3 cm, must stay below)" % (
            pad_z.min(), pad_z.max()))
        wall = (self.d_axis > FLOOR_EDGE + 150.0) & (self.d_axis < RIM - 150.0) & self.inside & ~ndimage.binary_dilation(self.ramps, iterations=3)
        lines.append("gorge wall slope deg p5 %.1f p50 %.1f (must stay > %.0f)" % (
            np.percentile(self.slope[wall], 5), np.percentile(self.slope[wall], 50), NAV_MAX_SLOPE_DEG))
        full = self.reach(hq, blocked)
        ne_side = ["hq", "b1_abutment", "b2_rimwoods", "b3_heap", "b4_nroad", "b5_crag", "b7_shaft", "convoy_exit",
                   "branch_dest"]
        sw_side = ["director", "convoy_entry", "convoy_stop", "b6_depot", "radio_post"]
        for fid in ne_side + sw_side:
            lines.append("reach from hq %-13s %s" % (fid, bool(full[self.raster.to_rc(self.feature(fid).points[0])])))
        ford_ways = ("ramp_ford_ne", "ramp_ford_sw")
        shut = blocked | ndimage.binary_dilation(self.openings(*ford_ways), iterations=4)
        lines.append("ford tracks shut: director reachable from hq %s (must be False: the decks are meshes)" %
                     self.reach(hq, shut)[self.raster.to_rc(self.feature("director").points[0])])
        fr = self.raster.to_rc(self.ford_c)
        lines.append("gorge floor at the ford reachable from hq %s" % bool(full[fr]))
        river_deep = (self.d_axis < RIVER_HALF) & (np.abs((self.X - self.ford_c[0]) * self.ford_dir[0] +
                                                         (self.Y - self.ford_c[1]) * self.ford_dir[1]) > FORD_HALF_ALONG + 300)
        lines.append("deep river cells reachable from hq: %d (must be 0)" % int((full & river_deep).sum()))
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
        files = {"height": os.path.join(OUT, "OVK_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "OVK_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        spec = self.full
        regions = [{"id": r.id, "kind": r.kind, "label": r.label, "centre": [float(v) for v in r.geom.centre],
                    "points": [[float(a), float(b)] for a, b in r.geom.pts], "raw": r.raw} for r in spec.regions]
        features = []
        for f in spec.features:
            entry = {"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                     "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw}
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
            "z_range": [float(self.Z.min()), float(self.Z.max())],
            "gorge": {"axis": [[float(a), float(b)] for a, b in self.axis], "rim_cm": RIM, "floor_edge_cm": FLOOR_EDGE,
                      "floor_z": FLOOR_Z, "river_half_cm": RIVER_HALF, "river_bed_z": RIVER_BED_Z,
                      "ford_centre": [float(v) for v in self.ford_c], "ford_dir": [float(v) for v in self.ford_dir],
                      "ford_half_along_cm": FORD_HALF_ALONG, "water_z": -1430.0},
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths,
        }
        with open(os.path.join(OUT, "OVK_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        self.preview()
        for name in LAYERS:
            print("layer %-12s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"Desert": (186, 160, 118), "GrassCastle": (120, 120, 70), "RockDesert": (84, 76, 70),
                   "GraySand": (200, 130, 70), "RTSSand": (150, 96, 80), "MountainTop": (190, 180, 160),
                   "Asphalt": (110, 110, 112), "DarkEarth": (70, 58, 48), "IronOxide": (190, 50, 25),
                   "Ochre": (196, 140, 60), "SlateScree": (120, 118, 116)}
        rgb = np.zeros((V, V, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.85 + 1.0 * (gy - gx) / 1.2, 0.35, 1.4)
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        r0, c0 = self.raster.to_rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.raster.to_rc([self.spec.xmin, self.spec.ymax])
        for r in (r0, r1):
            rgb[r, c0:c1] = (255, 220, 0)
        for c in (c0, c1):
            rgb[r0:r1, c] = (255, 220, 0)
        Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(OUT, "preview.png"))
        img = np.zeros((V, V, 3), dtype=np.uint8)
        img[..., 1] = np.clip((self.Z + 1700.0) / 3500.0 * 255, 0, 255)
        img[..., 2] = np.where(self.reach_full, 90, 0)
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
