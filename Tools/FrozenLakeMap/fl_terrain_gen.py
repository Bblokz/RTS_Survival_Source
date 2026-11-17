"""Terrain generator for Ger_FrozenLake: heights + paint layers + a resolved layout, from MapDesigner sketch A.

    python Source/Tools/FrozenLakeMap/fl_terrain_gen.py

Reads  MapDesigner/FrozenLake/frozen_A.layout.json (design source, +X north, +Y east, cm). Change the layout in
       MapDesigner/FrozenLake/generators/gen_frozen_A.py, never in the JSON.
Writes Saved/FrozenLake/terrain/
    FL_height.raw            uint16 LE, 32768 + cm * 128 / 100, [iy, ix]
    FL_layer_<Layer>.raw     uint8 weights, same grid, the layers sum to 255 per vertex
    FL_layout.json           landscape grid, resolved regions / features / paths, levels
    FL_masks.npz             masks on the vertex grid ([iy, ix]) for the dressing steps
    preview.png, preview_slope.png, check.txt

Height frame: the ice surface (the waterline) is z = 0. The ice is NOT landscape: it is a separate plane over the lake
(owner's decision, so vehicles can break through and sink), so the landscape under the lake is a lake bed 0.6-4.5 m
below the ice. The shore runs up gently from the waterline, so vehicles drive straight from land onto the ice plane.
Blocking ground (the Forge dam, the pump 1 outflow gorge, the spillway canal) uses 60 degree banks of at least 2.4 m,
which the 44 degree nav agents cannot climb (Pripyat / Oder-Warthe rule).
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

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "FrozenLake", "frozen_A.layout.json")
OUT = os.path.join(ROOT, "Saved", "FrozenLake", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake"

COMPONENTS = 12                         # 756 m: the 586 m play area plus 85 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1
SIZE = COMPONENTS * QUADS * QUAD_CM
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

LAYERS = ["FLSnow", "FLFrost", "FLScorched", "FLDryGrass", "FLGravel", "FLMud", "FLAsh"]

NAV_MAX_SLOPE_DEG = 44.0
BANK = math.tan(math.radians(60.0))     # cm of height per cm of run on every blocking bank
LAND = 180.0                            # dry ground round the lake
LAND_ROLL = 70.0                        # +- roll of the open ground (fbm), flattened on pads and roads
SHORE_RUN = 1100.0                      # the shore climbs from the waterline to LAND over this run (about 9 degrees)
BED_EDGE = -60.0                        # lake bed right at the waterline edge
BED_DEEP = -450.0                       # lake bed far from the shore
BED_RUN = 2500.0                        # from BED_EDGE to BED_DEEP over this run
DAM_CROWN = 560.0                       # Forge dam crest (5.6 m over the ice, 3.8 m over the land behind)
DAM_TOP_HALF = 300.0                    # half the crest width (the sketch's 6 m wall)
CUT_FLOOR = -380.0                      # gorge and canal water floor
FROST_EDGE_BAND = 9                     # cells of frost crust between snow and scorched ground


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
    """The mask sampled at noise-displaced positions: sketch outlines turn ragged (blurred after: no 1 m jags)."""
    dr = np.rint(amp_cells * noise(seed, wavelength)).astype(int)
    dc = np.rint(amp_cells * noise(seed + 1, wavelength)).astype(int)
    rr = np.clip(np.arange(mask.shape[0])[:, None] + dr, 0, mask.shape[0] - 1)
    cc = np.clip(np.arange(mask.shape[1])[None, :] + dc, 0, mask.shape[1] - 1)
    return soft(mask[rr, cc], 3) > 0.5


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
    """Flood fill over the landscape triangles (two per quad) whose slope is under the limit (Pripyat rule: vertex
    slopes give wrong answers both ways). Returns the quads with a reached triangle."""
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
        self.Z = None
        self.W = {}
        self.out_masks = {}
        self.report = []

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

    def line_dist(self, pts, reach=8000.0):
        """Distance in cm from every cell to a polyline (cells further than reach get 1e9)."""
        pts = np.asarray(pts, float)
        lo, hi = pts.min(axis=0) - reach, pts.max(axis=0) + reach
        sel = (self.X >= lo[0]) & (self.X <= hi[0]) & (self.Y >= lo[1]) & (self.Y <= hi[1])
        xs, ys = self.X[sel], self.Y[sel]
        best = np.full(xs.shape, 1e9)
        for a, b in zip(pts[:-1], pts[1:]):
            ab = b - a
            t = np.clip(((xs - a[0]) * ab[0] + (ys - a[1]) * ab[1]) / max(ab @ ab, 1e-9), 0.0, 1.0)
            best = np.minimum(best, np.hypot(xs - (a[0] + t * ab[0]), ys - (a[1] + t * ab[1])))
        d = np.full(SHAPE, 1e9)
        d[sel] = best
        return d

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        lake = self.region("lake_ice", "inlet_ice")
        self.lake = lake
        # exact euclidean distance to the shoreline (the blob outline and the inlet strip): the cell-metric distance
        # left radial facets on the shore
        reg = next(r for r in self.spec.regions if r.id == "lake_ice")
        outline = [tuple(p) for p in reg.geom.pts] + [tuple(reg.geom.pts[0])]
        d_blob = self.line_dist(outline, reach=SIZE)
        inlet = next(r for r in self.spec.regions if r.id == "inlet_ice")
        d_inlet = np.abs(self.line_dist(inlet.raw["line"], reach=SIZE) - inlet.raw["width"] / 2.0)
        in_blob = self.masks["lake_ice"]
        in_inlet = self.masks["inlet_ice"]
        d_out = np.where(lake, 0.0, np.minimum(d_blob, d_inlet))
        d_in = np.where(lake, np.where(in_blob & in_inlet, np.maximum(d_blob, d_inlet),
                                       np.where(in_blob, d_blob, d_inlet)), 0.0)
        roll = fbm(5, 90) * LAND_ROLL + fbm(9, 28) * 18.0
        flat = self.region("player_shelf", "shield_pad", "landing_field", "pump_s_pad", "pump_w_pad", "pump_n_pad",
                           "pump_e_pad")
        flat_soft = soft(flat, 6)
        land = LAND + roll * (1.0 - np.clip(flat_soft * 1.4, 0.0, 1.0))
        # shore: the waterline edge sits at 0 and climbs to the land over SHORE_RUN (pads keep their flat top)
        shore_t = smoothstep(0.0, SHORE_RUN, d_out)
        Z = np.where(lake, 0.0, land * shore_t)
        # lake bed under the ice plane
        bed = BED_EDGE + (BED_DEEP - BED_EDGE) * smoothstep(0.0, BED_RUN, d_in) + fbm(21, 40) * 40.0 * smoothstep(0.0, BED_RUN, d_in)
        Z = np.where(lake, bed, Z)

        # roads: blend toward the 7 m-blurred ground so they read as graded, never cutting a bank
        road_mask = np.zeros(SHAPE, dtype=bool)
        for pid in ("shore_road_w", "shore_road_s", "shore_road_se", "east_track"):
            p = self.path(pid)
            half = (p.width or 900.0) / 2.0
            d = self.line_dist(p.pts)
            road_mask |= (d < half) & ~lake
        smooth = box_blur(Z, 7)
        rw = soft(road_mask, 3)
        Z = np.where(lake, Z, Z * (1 - rw) + np.maximum(smooth, 30.0) * rw)
        self.road_mask = road_mask

        # gorge (pump 1 outflow) and spillway canal: water cut with 60 degree banks
        cuts = np.zeros(SHAPE, dtype=bool)
        for pid in ("outflow_gorge", "canal"):
            p = self.path(pid)
            half = p.width / 2.0
            d = self.line_dist(p.pts)
            floor = d <= half
            cuts |= floor
            bank = CUT_FLOOR + (d - half) * BANK
            Z = np.where(d <= half, CUT_FLOOR, np.minimum(Z, np.where(d < half + 2000.0, bank, 1e9)))
        self.cuts = cuts
        # placeholder fill under the canal bridge (the bridge mesh comes from the ForgeDam kit): a 10 m causeway
        bridge = self.feature("canal_bridge").points[0]
        canal = self.path("canal")
        a, b = np.asarray(canal.pts[1], float), np.asarray(canal.pts[2], float)
        u = (b - a) / np.linalg.norm(b - a)
        along = (self.X - bridge[0]) * u[0] + (self.Y - bridge[1]) * u[1]
        across = np.abs((self.X - bridge[0]) * -u[1] + (self.Y - bridge[1]) * u[0])
        fill = (np.abs(along) < 500.0) & (across < 1200.0)
        Z = np.where(fill, np.maximum(Z, LAND - 20.0), Z)
        self.bridge_fill = fill

        # Forge dam: 6 m crest, 60 degree banks both sides (blocks every agent)
        dam = np.zeros(SHAPE, dtype=bool)
        for pid in ("dam_w", "dam_e"):
            d = self.line_dist(self.path(pid).pts)
            crest = (d <= DAM_TOP_HALF) & ~lake          # the dam stops at the ice: the spillway gap stays clear
            dam |= crest
            raised = np.maximum(Z, np.where(d < DAM_TOP_HALF + 1200.0, DAM_CROWN - np.maximum(d - DAM_TOP_HALF, 0.0) * BANK, -1e9))
            Z = np.where(lake, Z, raised)
        self.dam = dam

        # scorched craters (south-east): shallow dents
        craters = self.masks["scorch_se"]
        rng = np.random.default_rng(3)
        rr, cc = np.nonzero(craters)
        crater_z = np.zeros(SHAPE)
        for k in rng.choice(len(rr), size=min(9, len(rr)), replace=False):
            x0, y0 = self.X[rr[k], cc[k]], self.Y[rr[k], cc[k]]
            r0 = rng.uniform(250.0, 550.0)
            dd = np.hypot(self.X - x0, self.Y - y0)
            crater_z -= np.where(dd < r0, 90.0 * (1 - (dd / r0) ** 2), 0.0)
        Z = Z + crater_z
        self.Z = Z
        self.slope = slope_deg(Z)

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        frost = warp(self.masks["frost"], 77, 9.0, 60)
        d_frost_out = dist_outside(frost, FROST_EDGE_BAND + 2)
        d_frost_in = dist_outside(~frost, FROST_EDGE_BAND + 2)
        # snow inside the pump range, a band of frost crust at its rim, scorched dry ground outside
        crust = (d_frost_in > 0) & (d_frost_in <= FROST_EDGE_BAND // 2) | (d_frost_out > 0) & (d_frost_out <= FROST_EDGE_BAND // 2)
        snow = frost & ~crust
        speckle = noise(41, 9) * 0.5 + 0.5
        W["FLSnow"] += np.where(snow, 1.0, 0.0)
        W["FLFrost"] += np.where(crust, 1.0, 0.0)
        outside = ~frost & ~crust
        W["FLScorched"] += np.where(outside, 0.75 + 0.25 * speckle, 0.0)
        W["FLDryGrass"] += np.where(outside, 0.25 * (1 - speckle), 0.0)
        # bare earth shows through thin snow on slopes over 14 degrees and on the dam banks
        bare = smoothstep(14.0, 30.0, self.slope) * (snow | crust)
        W["FLSnow"] *= (1 - bare)
        W["FLFrost"] += bare
        # dead forest floors: dry grass and frost-bitten earth
        forest = self.region("dead_forest_nw", "dead_forest_se", "dead_forest_w", "dead_forest_ne")
        forest_s = soft(forest, 3)
        for name in LAYERS:
            W[name] *= (1 - 0.6 * forest_s)
        W["FLDryGrass"] += 0.6 * forest_s * np.where(frost, 0.3, 1.0)
        W["FLFrost"] += 0.6 * forest_s * np.where(frost, 0.7, 0.0)
        # roads and pads: gravel (under snow inside the frost: half and half)
        pads = self.region("player_shelf", "shield_pad", "landing_field", "pump_s_pad", "pump_w_pad", "pump_n_pad",
                           "pump_e_pad")
        hard = soft(self.road_mask, 2) * 0.9 + soft(pads & ~self.lake, 3) * 0.5
        hard = np.clip(hard, 0.0, 0.95) * np.where(frost, 0.55, 1.0)
        for name in LAYERS:
            W[name] *= (1 - hard)
        W["FLGravel"] += hard
        # scorched craters and the burnt convoy: ash
        burnt = soft(self.masks["scorch_se"], 4)
        conv = self.feature("convoy_wreck_se").points[0]
        burnt = np.maximum(burnt, smoothstep(2600.0, 1200.0, np.hypot(self.X - conv[0], self.Y - conv[1])) * 0.8)
        for name in LAYERS:
            W[name] *= (1 - burnt)
        W["FLAsh"] += burnt
        # lake bed, gorge and canal floors under ice / water: mud
        wet = self.lake | self.cuts | (self.Z < 0.0)
        for name in LAYERS:
            W[name] = np.where(wet, 0.0, W[name])
        W["FLMud"] = np.where(wet, 1.0, W["FLMud"])
        # normalise to 255 per vertex
        total = sum(W.values())
        total = np.where(total <= 1e-6, 1.0, total)
        for name in LAYERS:
            W[name] = W[name] / total
        out = {name: np.floor(W[name] * 255.0) for name in LAYERS}
        rest = 255 - sum(out.values())
        biggest = np.argmax(np.stack([W[n] for n in LAYERS]), axis=0)
        for k, name in enumerate(LAYERS):
            out[name] = out[name] + np.where(biggest == k, rest, 0)
        self.W = {name: out[name].astype(np.uint8) for name in LAYERS}
        self.frost = frost
        self.out_masks.update({"lake": self.lake, "frost": frost, "dam": self.dam, "cuts": self.cuts,
                               "roads": self.road_mask})

    # ---- checks -------------------------------------------------------------------------------------------------
    def check(self):
        """Nav reach over landscape triangles from the HQ for the 44 degree agents, with the lake counted as
        walkable (the ice plane sits on it). The dam crest and the cut floors must stay unreached."""
        Z = self.Z
        limit = math.tan(math.radians(NAV_MAX_SLOPE_DEG))
        # the ice plane: treat the lake as flat at z = 0 for the reach (vehicles drive on the plane, not the bed)
        Zn = np.where(self.lake, 0.0, Z)
        cut_q = self.cuts[:-1, :-1] | self.cuts[1:, 1:]
        allowed = ~cut_q
        hq = self.feature("hq").points[0]
        reach = tri_reach(Zn, allowed, self.rc(hq), limit)
        self.reach_full = np.zeros(SHAPE, dtype=bool)
        self.reach_full[:-1, :-1] = reach
        lines = []
        for fid in ("rig", "shield_garrison", "spawn_n", "spawn_e", "spawn_s", "pump_s", "pump_w", "pump_n", "pump_e",
                    "frozen_tiger", "metal_yard", "radixite_seep", "convoy_wreck_se", "soviet_airfield"):
            f = self.feature(fid)
            r, c = self.rc(f.points[0])
            ok = bool(self.reach_full[max(r - 1, 0):r + 2, max(c - 1, 0):c + 2].any())
            lines.append("reach %-18s %s" % (fid, "ok" if ok else "NOT REACHED"))
        dam_reached = int((self.reach_full & self.dam).sum())
        lines.append("dam crest cells reached: %d (want 0)" % dam_reached)
        for r, c in list(zip(*np.nonzero(self.reach_full & self.dam)))[:5]:
            lines.append("  reached crest cell x %.0f y %.0f z %.0f in lake %s" % (self.X[r, c], self.Y[r, c], Z[r, c],
                                                                                   bool(self.lake[r, c])))
        cut_reached = int((self.reach_full & self.cuts).sum())
        lines.append("gorge/canal floor cells reached: %d (want 0)" % cut_reached)
        inside = self.inside
        lines.append("heights inside: min %.0f max %.0f cm" % (Z[inside].min(), Z[inside].max()))
        near_block = dist_outside(self.dam | self.cuts | self.bridge_fill, 26) < 25
        shore = (~self.lake) & (dist_outside(self.lake, 12) <= 11) & (dist_outside(self.lake, 12) > 0) & ~near_block
        lines.append("shore band max slope (ice plane as ground) %.1f deg (want under 20)"
                     % float(slope_deg(Zn)[shore & inside].max()))
        self.report = lines
        print("\n".join(lines))

    @staticmethod
    def to_grid(a):
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        files = {"height": os.path.join(OUT, "FL_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "FL_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        masks_file = os.path.join(OUT, "FL_masks.npz").replace("\\", "/")
        np.savez_compressed(masks_file, **{k: self.to_grid(np.asarray(v)) for k, v in self.out_masks.items()})
        files["masks"] = masks_file
        spec = self.spec
        regions = [{"id": reg.id, "kind": reg.kind, "label": reg.label, "shape": reg.geom.kind,
                    "centre": [float(v) for v in reg.geom.centre],
                    "points": [[float(a), float(b)] for a, b in reg.geom.pts], "raw": reg.raw} for reg in spec.regions]
        features = [{"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                     "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw} for f in spec.features]
        paths = [{"id": p.id, "kind": p.kind, "label": p.label, "width": p.width,
                  "points": [[float(a), float(b)] for a, b in p.pts], "raw": p.raw} for p in spec.paths]
        layout = {
            "map": MAP_PATH, "spec": SPEC_FILE.replace("\\", "/"),
            "landscape": {"components_x": COMPONENTS, "components_y": COMPONENTS, "quads_per_component": QUADS,
                          "quad_cm": QUAD_CM, "verts_x": V, "verts_y": V, "size_x": SIZE, "size_y": SIZE,
                          "corner": [-SIZE / 2.0, -SIZE / 2.0]},
            "bounds": {"xmin": spec.xmin, "xmax": spec.xmax, "ymin": spec.ymin, "ymax": spec.ymax},
            "z_range": [float(self.Z.min()), float(self.Z.max())],
            "levels": {"ice": 0.0, "land": LAND, "dam_crown": DAM_CROWN, "cut_floor": CUT_FLOOR, "bed_deep": BED_DEEP},
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths,
            "check": self.report,
        }
        with open(os.path.join(OUT, "FL_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        with open(os.path.join(OUT, "check.txt"), "w") as handle:
            handle.write("\n".join(self.report) + "\n")
        self.preview()
        for name in LAYERS:
            print("layer %-12s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"FLSnow": (236, 238, 242), "FLFrost": (196, 196, 192), "FLScorched": (166, 128, 88),
                   "FLDryGrass": (176, 156, 104), "FLGravel": (140, 136, 128), "FLMud": (60, 52, 46),
                   "FLAsh": (90, 88, 86)}
        rgb = np.zeros((V, V, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        ice = self.lake
        rgb[ice] = rgb[ice] * 0.25 + np.array((200, 218, 232)) * 0.75
        water = self.cuts
        rgb[water] = np.array((70, 110, 140))
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.9 + 0.9 * (gx + gy) / 1.4, 0.3, 1.5)
        shade = np.where(ice, 1.0, shade)
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        r0, c0 = self.rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.rc([self.spec.xmin, self.spec.ymax])
        for r in (r0, r1):
            rgb[r, c0:c1] = (255, 220, 0)
        for c in (c0, c1):
            rgb[r0:r1, c] = (255, 220, 0)
        Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(OUT, "preview.png"))
        img = np.zeros((V, V, 3), dtype=np.uint8)
        img[..., 1] = np.clip((self.Z + 500.0) / 1200.0 * 255, 0, 255)
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
