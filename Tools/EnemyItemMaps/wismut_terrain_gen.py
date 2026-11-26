"""Heights + Ore landscape layer weights for EnemyItem_WismutPlateau (enemy item map 1, Ore Mountains area).

Design source: MapDesigner/EnemyItemMaps/item_ore_01.layout.json (owner approved 2026-11-25, 510 x 510 m).
Writes Saved/EnemyItemMaps/WismutPlateau/terrain/{WP_height.raw, WP_layer_<name>.raw, WP_layout.json, preview*.png,
check.txt}. wismut_map_build.py (in a throwaway editor) builds the landscape from these files.

    python Source/Tools/EnemyItemMaps/wismut_terrain_gen.py
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
from mapdesigner.grid import Raster, Terrain  # noqa: E402
from mapdesigner.spec import Spec, dist_to_polyline  # noqa: E402

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "EnemyItemMaps", "item_ore_01.layout.json")
OUT = os.path.join(ROOT, "Saved", "EnemyItemMaps", "WismutPlateau", "terrain")
MAP_PATH = "/Game/RTS_Survival/Maps/Campaign/EnemyMapObjects/EnemyItem_WismutPlateau"

COMPONENTS = 11                         # 693 m: the 510 m play area plus 91 m of camera margin a side
QUADS = 63
QUAD_CM = 100.0
V = COMPONENTS * QUADS + 1              # 694 vertices a side
SIZE = COMPONENTS * QUADS * QUAD_CM     # 69 300 cm
SHAPE = (V, V)                          # work arrays: row = north..south, col = west..east

# the Ore Mountains material (MI_RTS_Landscape_DryCastle_OreMountains): same layer set as Ger_2ndOreMountains
LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop", "Asphalt", "DarkEarth",
          "IronOxide", "Ochre", "SlateScree"]
NAV_MAX_SLOPE_DEG = 44.0                # agent_max_slope of every RecastNavMesh agent in the template

PLATEAU_Z = 800.0
PIT_Z = -600.0
PLATEAU_FACE_RUN = 5.0                  # m: 8 m over 5 m = 58 deg face (cliff kit C goes on it)
PIT_WALL_RUN = 4.5                      # m: 6 m over 4.5 m = 53 deg wall (cliff kit E benches)
CREST_FACE_Z, CREST_FACE_RUN = 1500.0, 9.0
CREST_RISE_PER_M = 40.0                 # cm per m behind the crest face, up to CREST_MAX
CREST_MAX = 5200.0
MARGIN_RISE = 1400.0                    # the camera margin rolls up into hills past the play edge
RELIEF_CM = 55.0
GULLY_DEPTH = 130.0
BOG_DEPTH = 90.0


# ---- helpers (same as Source/Tools/OreMountainsMap/ore_terrain_gen.py) ---------------------------------------------
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


def dist_outside(mask):
    """Metres from each cell outside the mask to the mask (0 inside)."""
    return ndimage.distance_transform_edt(~mask) * QUAD_CM / 100.0


def dist_inside(mask):
    """Metres from each cell inside the mask to its edge (0 outside)."""
    return ndimage.distance_transform_edt(mask) * QUAD_CM / 100.0


class Gen:
    def __init__(self):
        self.spec = Spec(SPEC_FILE)
        if self.spec.errors():
            raise RuntimeError("spec errors: %s" % [str(i.msg) for i in self.spec.errors()])
        raster = Raster(self.spec, QUAD_CM)
        raster.xmax = SIZE / 2.0 + QUAD_CM / 2.0      # pixel centres on the landscape vertices
        raster.ymin = -SIZE / 2.0 - QUAD_CM / 2.0
        raster.rows, raster.cols = V, V
        self.raster = raster
        self.terrain = Terrain(self.spec, raster)
        self.masks = self.terrain.region_masks
        self.X = (SIZE / 2.0 - np.arange(V) * QUAD_CM)[:, None] * np.ones((1, V))
        self.Y = np.ones((V, 1)) * (-SIZE / 2.0 + np.arange(V) * QUAD_CM)[None, :]
        self.inside = (np.abs(self.X) <= self.spec.xmax) & (np.abs(self.Y) <= self.spec.ymax)
        self.E = self.spec.elevation.raster(raster) * 100.0     # cm, the drawing's own height field
        self.W = {}
        self.out = {}

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
        return next(f for f in self.spec.features if f.id == fid)

    def path(self, pid):
        return next(p for p in self.spec.paths if p.id == pid)

    def dist_line(self, pts):
        pts = np.asarray(pts, float)
        flat = np.c_[self.X.ravel(), self.Y.ravel()]
        lo = pts.min(axis=0) - 6000.0
        hi = pts.max(axis=0) + 6000.0
        near = (flat[:, 0] >= lo[0]) & (flat[:, 0] <= hi[0]) & (flat[:, 1] >= lo[1]) & (flat[:, 1] <= hi[1])
        d = np.full(flat.shape[0], 1e9)
        d[near] = dist_to_polyline(flat[near], pts)
        return d.reshape(SHAPE)

    def at(self, p):
        r, c = self.raster.to_rc(p)
        return self.Z[r, c]

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        ramp_ids = [f.id for f in self.spec.features if f.kind == "ramp"]
        ramps = self.openings(*ramp_ids)
        self.ramps = ramps
        ramp_keep = soft(ndimage.binary_dilation(ramps, iterations=3).astype(np.float64), 2)
        plateau = self.region("plateau")
        pit = self.region("pit")
        crest = self.region("crest") | (self.X < -27000.0)     # the spec polygon stops at 275 m; run it to the edge
        self.plateau, self.pit, self.crest = plateau, pit, crest

        # roads and tracks (flattening + paint)
        self.road_dist = self.dist_line(self.path("haul_road").pts)
        track = np.full(SHAPE, 1e9)
        for pid in ("east_track", "woods_track", "north_track"):
            track = np.minimum(track, self.dist_line(self.path(pid).pts))
        self.track_dist = track

        # 1. ground: the drawing's field (heaps + ramps), plus low relief on open ground
        flat_zone = soft(self.region("start_shelf") | plateau | pit | ramps | (self.road_dist < 700.0), 4)
        relief = fbm(11, 90) * RELIEF_CM + fbm(23, 30) * 18.0
        Z = self.E + relief * (1.0 - np.clip(flat_zone, 0.0, 1.0))
        # gullies and bogs cut in
        for rid, depth in (("gully_s", GULLY_DEPTH), ("gully_n", GULLY_DEPTH * 0.8)):
            reg = self.spec.by_id[rid]
            d = self.dist_line(reg.geom.line_pts if hasattr(reg.geom, "line_pts") else reg.raw["line"])
            half = reg.geom.width / 2.0 if reg.geom.width else 600.0
            Z -= depth * (1.0 - smoothstep(0.0, half, d)) * (0.75 + 0.25 * noise(31, 25))
        for rid in ("bog_n", "bog_s"):
            Z -= BOG_DEPTH * soft(self.masks[rid], 4)

        # 2. plateau: level top, sharp faces (the drawing's 10 m blend would be a walkable 39 deg slope)
        d_out = dist_outside(plateau)
        face = PLATEAU_Z * (1.0 - smoothstep(0.0, PLATEAU_FACE_RUN, d_out))
        # the drawing ramps 8 m -> 0 m over its 10 m foot band: cut that blend back to the ground relief there
        foot_band = 11.0
        ground = np.where(d_out <= foot_band, np.minimum(Z - np.maximum(self.E, 0.0) * (self.E > 0.0), 60.0), Z)
        Zp = np.where(plateau, PLATEAU_Z, np.maximum(face, ground))

        # 3. pit: level floor, sharp walls
        d_in = dist_inside(pit)
        wall = PIT_Z * smoothstep(0.0, PIT_WALL_RUN, d_in) + np.minimum(Z, 0.0) * (1.0 - smoothstep(0.0, PIT_WALL_RUN, d_in))
        floor_relief = fbm(41, 40) * 15.0
        Zp = np.where(pit, wall + floor_relief * smoothstep(PIT_WALL_RUN, PIT_WALL_RUN + 6.0, d_in), Zp)

        # 4. ramps follow the drawing exactly
        Zp = Zp * (1.0 - ramp_keep) + self.E * ramp_keep

        # 5. the Erzgebirge crest: a quarried face, then rising ground past the play edge
        dc = dist_inside(crest)
        crest_z = CREST_FACE_Z * np.power(np.clip(dc / CREST_FACE_RUN, 0.0, 1.0), 0.8) + \
            np.maximum(0.0, dc - CREST_FACE_RUN) * CREST_RISE_PER_M
        crest_z = np.minimum(crest_z, CREST_MAX) + fbm(51, 40) * 220.0 * smoothstep(2.0, 12.0, dc)
        Zp = np.where(crest, np.maximum(Zp, crest_z), Zp)

        # 6. camera margin: hills past the play edge (blocking volumes keep units in)
        dx = np.maximum(np.abs(self.X) - self.spec.xmax, 0.0)
        dy = np.maximum(np.abs(self.Y) - self.spec.ymax, 0.0)
        d_edge = np.hypot(dx, dy) / 100.0
        margin = MARGIN_RISE * smoothstep(6.0, 70.0, d_edge) * (0.7 + 0.3 * fbm(61, 60))
        Zp = Zp + margin * (~crest)
        self.Z = Zp
        self.slope = slope_deg(self.Z)

    # ---- paint --------------------------------------------------------------------------------------------------
    def build_layers(self):
        W = {name: np.zeros(SHAPE) for name in LAYERS}
        n1, n2, n3 = noise(71, 60), noise(72, 18), noise(73, 9)
        # base: khaki desert with pale grey-tan drifts (the Ore valley floor look)
        W["Desert"] += 1.0
        W["MountainTop"] += 0.55 * crisp(n1, 0.15, 0.25)
        W["GraySand"] += 0.25 * crisp(n2, 0.55, 0.12)
        # steep ground: charcoal rock
        rock = smoothstep(28.0, 40.0, self.slope)
        # plateau: levelled spoil, pale with slate scree; the long slot packed and greyer
        plat = soft(self.plateau, 2)
        W["MountainTop"] += 1.4 * plat
        W["SlateScree"] += 0.6 * plat * crisp(n2, 0.1, 0.2)
        strip = self.feature("strip")
        sd = self.dist_line(strip.points if len(strip.points) > 1 else strip.raw["ends"])
        W["Asphalt"] += 0.8 * (1.0 - smoothstep(1200.0, 2200.0, sd)) * plat
        # pit: ochre floor, sparse iron-oxide streaks, scree benches on the walls
        pit = soft(self.pit, 2)
        W["Ochre"] += 1.6 * pit
        W["IronOxide"] += 0.9 * pit * crisp(n3 + 0.4 * n2, 0.62, 0.05)
        W["SlateScree"] += 0.8 * soft(self.pit & (dist_inside(self.pit) < PIT_WALL_RUN + 2.0), 2)
        # spoil heaps: slate scree
        heaps = soft(self.region("heap_w", "heap_n"), 3)
        W["SlateScree"] += 1.5 * heaps
        # ore chute under the plateau: ochre + a little oxide around the metal
        for fid in ("metal_chute", "metal_creek", "metal_heap_w", "metal_heap_n"):
            for p in self.feature(fid).points:
                d = np.hypot(self.X - p[0], self.Y - p[1])
                W["Ochre"] += 0.9 * (1.0 - smoothstep(300.0, 1100.0, d)) * (0.7 + 0.3 * n2)
                W["IronOxide"] += 0.6 * (1.0 - smoothstep(150.0, 450.0, d)) * crisp(n3, 0.2, 0.2)
        # dead spruce woods + bogs: dark earth
        woods = soft(self.region("woods_w", "woods_nw", "woods_e", "woods_se"), 4)
        W["DarkEarth"] += 0.9 * woods * (0.6 + 0.4 * crisp(n2, -0.1, 0.3))
        bogs = soft(self.region("bog_n", "bog_s"), 3)
        W["DarkEarth"] += 2.2 * bogs
        # gullies: dark earth + scree in the bed
        gul = soft(self.region("gully_s", "gully_n"), 3)
        W["DarkEarth"] += 0.7 * gul
        W["SlateScree"] += 0.5 * gul * crisp(n3, 0.0, 0.3)
        # haul road: compacted pale gravel; tracks: dark wheel wear
        road = 1.0 - smoothstep(300.0, 650.0, self.road_dist)
        W["MountainTop"] += 1.2 * road
        W["Asphalt"] += 0.35 * road * crisp(n3, 0.0, 0.4)
        trk = 1.0 - smoothstep(150.0, 380.0, self.track_dist)
        W["DarkEarth"] += 0.8 * trk * (0.6 + 0.4 * crisp(n3, -0.2, 0.3))
        # start shelf: slightly paler, tidy
        W["MountainTop"] += 0.5 * soft(self.region("start_shelf"), 4)
        # crest + cliff faces
        crest = soft(self.crest, 3)
        W["RockDesert"] += 2.5 * rock + 1.2 * crest * (1.0 - rock)
        W["MountainTop"] += 0.6 * crest * crisp(n1, 0.0, 0.3)
        # normalise to 255
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
        base = self.feature("enemy_base").points[0]
        pit_c = [-1000, -4000]
        lines = []
        z = self.Z[self.inside]
        lines.append("heights inside: %.0f..%.0f cm; landscape %.0f..%.0f cm" % (z.min(), z.max(), self.Z.min(), self.Z.max()))
        for fid in ("hq", "enemy_base", "slot_L1", "slot_L3", "slot_G1"):
            lines.append("z %-12s %6.0f cm" % (fid, self.at(self.feature(fid).points[0])))
        lines.append("z pit centre   %6.0f cm" % self.at(pit_c))
        for f in self.spec.features:
            if f.kind == "ramp":
                m = self.terrain.opening_masks[f.id]
                lines.append("ramp %-13s slope deg p50 %.1f p95 %.1f max %.1f" % (
                    f.id, np.percentile(self.slope[m], 50), np.percentile(self.slope[m], 95), self.slope[m].max()))
        full = self.reach(hq, blocked)
        lines.append("reach from hq (slope <= %.0f): base %s, pit floor %s" % (NAV_MAX_SLOPE_DEG, full[self.raster.to_rc(base)],
                                                                           full[self.raster.to_rc(pit_c)]))
        plateau_ramps = ("ramp_gate", "ramp_incline", "ramp_forest")
        shut_all = blocked | ndimage.binary_dilation(self.openings(*plateau_ramps), iterations=4)
        lines.append("all plateau ramps shut: base reachable %s (must be False)" % self.reach(hq, shut_all)[self.raster.to_rc(base)])
        for name in plateau_ramps:
            others = self.openings(*[n for n in plateau_ramps if n != name])
            one = self.reach(hq, blocked | ndimage.binary_dilation(others, iterations=4))
            lines.append("only %-13s open: base reachable %s (must be True)" % (name, one[self.raster.to_rc(base)]))
        pit_ways = ("haul_sw", "haul_ne", "miners_stair")
        shut_pit = blocked | ndimage.binary_dilation(self.openings(*pit_ways), iterations=4)
        lines.append("pit ways shut: pit floor reachable %s (must be False)" % self.reach(hq, shut_pit)[self.raster.to_rc(pit_c)])
        for fid in ("metal_base", "metal_heap_w", "metal_creek", "metal_pit", "metal_chute", "metal_heap_n", "radix"):
            pts = self.feature(fid).points
            ok = [bool(full[self.raster.to_rc(p)]) for p in pts]
            lines.append("reach %-13s %s" % (fid, ok))
        lines.append("walkable share of play area: %.1f %%" % (100.0 * (full & self.inside).sum() / self.inside.sum()))
        self.reach_full = full
        text = "\n".join(lines)
        os.makedirs(OUT, exist_ok=True)
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
        files = {"height": os.path.join(OUT, "WP_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "WP_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        spec = self.spec
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
            "layers": LAYERS, "files": files, "regions": regions, "features": features, "paths": paths,
        }
        with open(os.path.join(OUT, "WP_layout.json"), "w") as handle:
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
        Image.fromarray(rgb.astype(np.uint8)).resize((V * 2, V * 2), Image.NEAREST).save(os.path.join(OUT, "preview.png"))
        img = np.zeros((V, V, 3), dtype=np.uint8)
        img[..., 1] = np.clip((self.Z + 600.0) / 3000.0 * 255, 0, 255)
        img[..., 2] = np.where(self.reach_full, 90, 0)
        img[..., 0] = np.clip(self.slope * 4, 0, 255)
        img[self.slope > NAV_MAX_SLOPE_DEG] = (255, 0, 255)
        Image.fromarray(img).resize((V * 2, V * 2), Image.NEAREST).save(os.path.join(OUT, "preview_slope.png"))


def main():
    gen = Gen()
    gen.build_heights()
    gen.build_layers()
    gen.check()
    gen.write()


if __name__ == "__main__":
    main()
