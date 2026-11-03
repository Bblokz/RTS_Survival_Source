"""Offline terrain generator for survival maps (run with a local Python 3 + numpy, NOT inside the editor).

Writes the raw files that `RTSShortsLandscapeLibrary.create_painted_landscape` imports (see survival_map_build.py):
  <out>/<map>_height.raw          uint16 LE, 32768 + cm * 128 / 100 (landscape Z scale 100), row by row along X
  <out>/<map>_<Layer>.raw         uint8 per vertex, all layers sum to 255
  <out>/<map>_layout.json         start locations, defense points, edge zones, dressing spots (world cm, Z on ground)
  <out>/<map>_preview.png         shaded height + layer preview (needs Pillow)

Layout: N start areas on a regular polygon around the map centre. Every start area is its own landform, designed by
hand in survival_landforms_desert01.py (terraces, ridges, ramps; see survival_landforms.py for the operations), with
one flat build disc of the same size. Around them the open desert with low dunes, craters, a central mesa with one
ramp per base and small rock buttes between neighbouring bases. A band along the map edge stays free of obstacles so
waves can enter anywhere.

Usage: python survival_terrain_gen.py <out_dir> [config_name]
"""
import json
import math
import os
import sys
from collections import deque

import numpy as np

import survival_landforms as lf
import survival_landforms_desert01 as desert01

# ----------------------------------------------------------------------------------------------------------------------
# Config. Copy this dict (new name + seed + tweaks) to make the next survival map.
# ----------------------------------------------------------------------------------------------------------------------
CONFIGS = {
    "SM_Desert_01": dict(
        components=24, quads=63, quad_cm=100.0,  # 24 * 63 = 1512 quads -> 151 200 cm square, 1513^2 vertices
        base_z=600.0,                            # world Z of the flat build discs (landscape actor at Z = 0)
        seed=1729,
        start_count=5, start_radius=desert01.START_RADIUS, start_first_angle_deg=desert01.START_FIRST_ANGLE_DEG,
        build_radius=8000.0,                     # exactly flat out to survival_landforms.BUILD_FLAT
        resource_ring=(8000.0, 14600.0),
        wreck_ring=(13000.0, 17500.0),
        landforms=desert01.LANDFORMS,            # one designed landform per start, same order as the starts
        power_lines=desert01.POWER_LINES,
        world_features=[desert01.WORLD] + desert01.OUTER_REMNANTS,
        dunes=[(9000.0, 230.0), (2600.0, 55.0), (750.0, 10.0)],  # (wavelength cm, amplitude cm)
        edge_band=9000.0,                        # no buttes/craters closer than this to the landscape edge
        edge_dune_scale=0.55,
        craters_random=34, crater_radius=(500.0, 1400.0), crater_depth=(110.0, 260.0),
        wreck_craters=[(25.0, 24500.0, 700.0), (160.0, 25500.0, 900.0), (280.0, 24800.0, 600.0)],
        edge_zones=[
            # centre x, centre y, length along edge (cm); inward direction is derived from the nearest edge
            # Round 5 (islands slid in to R 40 000): 3200 inside survival_frontier.PLAY, at least 21 000 from any build disc
            # edge and about 23 500+ from any gate foot. The zone in front of the Red Mesa / Dry Wells outer gates (+X south,
            # 63000,-20000) stays in the map disabled (bEnabled false), so it is not listed here.
            (-53000.0, 68000.0, 14000.0), (53000.0, 68000.0, 14000.0),
            (63000.0, 44000.0, 14000.0),
            (55000.0, -61200.0, 14000.0), (0.0, -61200.0, 14000.0), (-31000.0, -61200.0, 14000.0),
            (-66200.0, -29000.0, 14000.0), (-66200.0, 34000.0, 14000.0),
        ],
        edge_zone_inset=2500.0,
        layers=["Sand", "DesertGravel", "DesertRockDark", "ScorchConcrete", "ScorchSandyStone", "Weedy"],
    ),
}


smoothstep = lf.smoothstep
value_noise = lf.value_noise


def box_blur(a, radius):
    """Fast square blur (summed area table), for masks."""
    h, w = a.shape
    sat = np.zeros((h + 1, w + 1))
    sat[1:, 1:] = a.cumsum(0).cumsum(1)
    y0 = np.clip(np.arange(h) - radius, 0, h)
    y1 = np.clip(np.arange(h) + radius + 1, 0, h)
    x0 = np.clip(np.arange(w) - radius, 0, w)
    x1 = np.clip(np.arange(w) + radius + 1, 0, w)
    area = ((y1 - y0)[:, None] * (x1 - x0)[None, :]).astype(float)
    return (sat[y1][:, x1] - sat[y0][:, x1] - sat[y1][:, x0] + sat[y0][:, x0]) / area


def capsule_distance(px, py, cx, cy, dir_x, dir_y, half_length):
    """Distance from points to a segment centred on (cx, cy) along unit (dir_x, dir_y)."""
    rx = px - cx
    ry = py - cy
    t = np.clip(rx * dir_x + ry * dir_y, -half_length, half_length)
    qx = rx - t * dir_x
    qy = ry - t * dir_y
    return np.sqrt(qx * qx + qy * qy)


# E sockets of the wired trellis Blueprints (BP_Electricity_SinglePole*), local x, y, z. The wires pair them by name.
POLE_SOCKETS = {
    "pole1": [(1.4, 230.4, 1848.6), (1.2, -229.7, 1848.4), (1.2, -237.8, 1566.9), (1.2, 241.6, 1567.5), (1.2, 236.8, 1256.4)],
    "pole2": [(-0.3, -321.4, 1665.5), (0.0, 348.4, 1415.7), (0.0, -348.7, 1131.5)],
    "pole3": [(0.0, 0.0, 1226.0), (0.0, 394.4, 1226.0), (0.0, -400.1, 1226.0)],
}


def terrace_list(spots):
    return spots


class Generator:
    def __init__(self, name):
        self.name = name
        self.c = CONFIGS[name]
        c = self.c
        self.verts = c["components"] * c["quads"] + 1
        self.half = (self.verts - 1) * c["quad_cm"] / 2.0
        coords = -self.half + np.arange(self.verts) * c["quad_cm"]
        self.X, self.Y = np.meshgrid(coords, coords)  # [row=y, col=x]
        self.rng = np.random.default_rng(c["seed"])
        self.starts = []
        for k in range(c["start_count"]):
            ang = math.radians(c["start_first_angle_deg"] + 360.0 * k / c["start_count"])
            sx, sy = c["start_radius"] * math.cos(ang), c["start_radius"] * math.sin(ang)
            # local +X points at the map centre
            ix, iy = -math.cos(ang), -math.sin(ang)
            self.starts.append(dict(index=k, x=sx, y=sy, angle_deg=math.degrees(ang), inward=(ix, iy)))
        self.features = dict(landforms=[], world=[], ramps=[], craters=[])

    # ---- helpers -----------------------------------------------------------------------------------------------------
    def local_to_world(self, start, lx, ly):
        ix, iy = start["inward"]
        # local X = inward, local Y = inward rotated +90 deg
        jx, jy = -iy, ix
        return start["x"] + lx * ix + ly * jx, start["y"] + lx * iy + ly * jy

    def dist_to_nearest_start(self, x, y):
        d = np.full(np.shape(x), 1e12)
        for s in self.starts:
            d = np.minimum(d, np.hypot(x - s["x"], y - s["y"]))
        return d

    def edge_distance(self, x, y):
        return np.minimum(self.half - np.abs(x), self.half - np.abs(y))

    # ---- shape primitives (return height above base, and a 'feature' mask) -----------------------------------------
    def plateau(self, cx, cy, dx, dy, half_length, radius, flank, height):
        d = capsule_distance(self.X, self.Y, cx, cy, dx, dy, half_length)
        return height * (1.0 - smoothstep(radius, radius + flank, d)), d

    def ramp(self, top_x, top_y, dir_x, dir_y, length, half_width, side_flank, height):
        """Ramp from (top_x, top_y) at `height` down to 0 at `length` along (dir_x, dir_y)."""
        rx = self.X - top_x
        ry = self.Y - top_y
        t = rx * dir_x + ry * dir_y
        w = np.abs(-rx * dir_y + ry * dir_x)
        along = np.clip(t / length, 0.0, 1.0)
        # smooth ends so the ramp meets the plateau and the plain without a kink
        profile = 1.0 - (along + (smoothstep(0.0, 1.0, along) - along) * 0.35)
        inside_len = (t >= -400.0) & (t <= length + 50.0)
        lateral = 1.0 - smoothstep(half_width, half_width + side_flank, w)
        h = np.where(inside_len, height * profile * lateral, 0.0)
        side = inside_len & (w >= half_width - 100.0) & (w <= half_width + side_flank + 200.0) & (h > 40.0)
        self.rock_mask = np.maximum(self.rock_mask, side.astype(float))
        return h, t, w

    # ---- build -----------------------------------------------------------------------------------------------------
    def build_height(self):
        c = self.c
        X, Y = self.X, self.Y
        salt = c["seed"]
        dunes = np.zeros_like(X)
        for i, (wl, amp) in enumerate(c["dunes"]):
            # anisotropic dunes: stretch along a prevailing wind direction (from the south-west)
            u = (X * 0.83 + Y * 0.55) / wl
            v = (-X * 0.55 + Y * 0.83) / (wl * 0.55)
            dunes += amp * value_noise(u, v, salt + 31 * i)
        edge = self.edge_distance(X, Y)
        dune_mask = c["edge_dune_scale"] + (1.0 - c["edge_dune_scale"]) * smoothstep(0.0, c["edge_band"], edge)
        relief = dunes * dune_mask

        raised = np.zeros_like(X)  # max-combined positive features (mesa, buttes, mounds)
        self.rock_mask = np.zeros_like(X)
        self.ramp_mask = np.zeros_like(X)   # drivable core of every ramp
        self.road_mask = np.zeros_like(X)   # ramp corridors including their blended sides
        self.road_paint = np.zeros_like(X)  # the same, fading out on the pad at the ramp's head
        self.plateau_mask = np.zeros_like(X)
        self.pad_mask = np.zeros_like(X)    # exactly flat terraces
        self.ridge_mask = np.zeros_like(X)  # ridge tops
        self.disc_mask = np.zeros_like(X)
        self.platform_mask = np.zeros_like(X)
        self.cut_mask = np.zeros_like(X)    # rock walls beside ramps
        self.paint_masks = {k: np.zeros_like(X) for k in ("salt", "wash", "dune", "damp")}
        # craters: identical small ones in every wreck ring + random ones in the open desert
        craters = []
        for s in self.starts:
            for (a, r, rad) in c["wreck_craters"]:
                ar = math.radians(a)
                cx, cy = self.local_to_world(s, r * math.cos(ar), r * math.sin(ar))
                craters.append((cx, cy, rad, min(140.0, 0.17 * rad)))
        tries = 0
        while len(craters) < len(self.starts) * len(c["wreck_craters"]) + c["craters_random"] and tries < 20000:
            tries += 1
            cx, cy = self.rng.uniform(-self.half, self.half, 2)
            rad = self.rng.uniform(*c["crater_radius"])
            if min(self.half - abs(cx), self.half - abs(cy)) < c["edge_band"] + rad:
                continue
            if float(self.dist_to_nearest_start(np.array(cx), np.array(cy))) < 25000.0 + rad:
                continue
            if math.hypot(cx, cy) < 14000 + rad or math.hypot(cx - desert01.OASIS_CENTRE[0], cy - desert01.OASIS_CENTRE[1]) < 11000 + rad:
                continue
            if any(math.hypot(cx - q[0], cy - q[1]) < q[2] + rad + 2500 for q in craters):
                continue
            craters.append((cx, cy, rad, min(self.rng.uniform(*c["crater_depth"]), 0.17 * rad)))
        crater_field = np.zeros_like(X)
        self.crater_mask = np.zeros_like(X)
        for (cx, cy, rad, depth) in craters:
            win = (np.abs(X - cx) < rad * 1.8) & (np.abs(Y - cy) < rad * 1.8)
            d = np.hypot(X[win] - cx, Y[win] - cy) / rad
            bowl = -depth * (1.0 - smoothstep(0.0, 1.0, d)) ** 1.0
            rim = depth * 0.38 * np.exp(-((d - 1.0) / 0.22) ** 2)
            crater_field[win] += bowl + rim
            self.crater_mask[win] = np.maximum(self.crater_mask[win], 1.0 - smoothstep(0.55, 1.05, d))
            self.features["craters"].append(dict(x=cx, y=cy, radius=rad, depth=depth))

        # features (mesa, buttes, mounds) sit on clean ground: fade the dunes out under them
        relief = relief * (1.0 - smoothstep(0.0, 150.0, raised))
        Z = c["base_z"] + relief + raised + crater_field
        # The land between the starts first, then the starts: scarps fall to whatever lies around them.
        salt_world = c["seed"] + 700
        for k, spec in enumerate(c["world_features"]):
            frame = dict(index=-1, x=spec["centre"][0], y=spec["centre"][1], inward=(1.0, 0.0))
            form = lf.Landform(self, frame, spec, salt_world + k * 13)
            Z[form.win] = form.run(Z[form.win])
            self.features["world"].append(form.record)
        for s, spec in zip(self.starts, c["landforms"]):
            form = lf.Landform(self, s, spec, c["seed"] + 101 * (s["index"] + 1))
            Z[form.win] = form.run(Z[form.win])
            self.features["landforms"].append(form.record)
        self.Z = Z
        self.raised = raised
        self.finish_relief()
        import survival_frontier
        before = self.Z.copy()
        for line in getattr(desert01, "GAP_CORRIDORS", {}).values():
            self.Z = survival_frontier.apply_corridors(self.Z, self.X, self.Y, [line], half_width=1500.0)
        # Ramps, roads and gun platforms keep their ground: the corridors fade out around them.
        keep = np.clip(box_blur(np.maximum(self.ramp_mask, self.platform_mask), 8) * 3.0, 0.0, 1.0)
        # ... and so does the oasis with its planted shore (frozen by the owner): untouched within 45 m, full corridor by 65 m.
        basin = [b for b in self.c["world_features"][0]["ops"] if b.get("name") == "oasis"][0]
        sd = lf.polygon_sdf(self.X, self.Y, np.array(basin["outline"], dtype=float))
        keep = np.maximum(keep, 1.0 - smoothstep(4500.0, 6500.0, sd))
        self.Z = self.Z * (1.0 - keep) + before * keep
        self.keep_oasis("height")
        self.edge_band()
        return self.Z

    OASIS_PATCH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SM_Desert_01_oasis_r4.npz")

    def keep_oasis(self, what):
        """Round 5: the owner wants the oasis exactly as it was. Heights ("height") or paint ("weights") of the round 4 map are
        pasted back over the basin, its shore and rim, feathered into the new ground over the patch's outer 20 m."""
        if not os.path.exists(self.OASIS_PATCH):
            return
        patch = np.load(self.OASIS_PATCH)
        q = self.c["quad_cm"]
        i0 = int(round((float(patch["x0"]) + self.half) / q))
        j0 = int(round((float(patch["y0"]) + self.half) / q))
        h, w = patch["height"].shape
        jj, ii = np.mgrid[0:h, 0:w]
        px = float(patch["x0"]) + ii * q
        py = float(patch["y0"]) + jj * q
        basin = [b for b in self.c["world_features"][0]["ops"] if b.get("name") == "oasis"][0]
        sd = lf.polygon_sdf(px, py, np.array(basin["outline"], dtype=float))  # < 0 inside the water outline
        # Full weight on the basin and 7 m of shore, fading out by 15 m: ramps nearby keep their own ground.
        t = np.clip((1500.0 - sd) / 800.0, 0.0, 1.0)
        blend = t * t * (3.0 - 2.0 * t)
        win = (slice(j0, j0 + h), slice(i0, i0 + w))
        if what == "height":
            old = (patch["height"].astype(float) - 32768.0) * 100.0 / 128.0
            self.Z[win] = self.Z[win] * (1.0 - blend) + old * blend
        else:
            for n in self.weights:
                if n in patch.files:
                    self.weights[n][win] = self.weights[n][win] * (1.0 - blend) + patch[n].astype(float) * blend

    def edge_band(self):
        """Outside the playable square a dune rim rises to a 3-7 m crest, then the landscape's outer 9 m fall to one height
        so the flat horizon skirt (survival_map_dress.step_horizon_skirt) meets it: no camera sees past the map."""
        import survival_frontier
        self.Z, _ = survival_frontier.frontier(self.Z, self.X, self.Y, half=self.half)

    def finish_relief(self):
        """Rock detail, strata and a little slumping on the scarps; flat tops and ramps stay untouched."""
        c = self.c
        X, Y = self.X, self.Y
        salt = c["seed"] + 500
        keep = np.maximum.reduce([self.pad_mask, self.disc_mask, np.clip(self.road_mask * 1.5, 0.0, 1.0)])
        for _ in range(3):
            # Slumping: anything steeper than the cliff limit relaxes a touch, so no single-quad walls remain.
            gy, gx = np.gradient(self.Z, c["quad_cm"])
            steep = smoothstep(2.1, 3.2, np.hypot(gx, gy)) * (1.0 - keep)
            self.Z = self.Z + (box_blur(self.Z, 1) - self.Z) * steep
        slope = self.slope_deg()
        rough = box_blur((slope > 24.0).astype(float), 3)
        rough = np.clip(rough * 1.4, 0.0, 1.0) * (1.0 - box_blur(keep, 2))
        strata = lf.fbm(X, Y, 3800.0, salt, 2)
        phase = (self.Z + 90.0 * strata) / 165.0
        saw = phase - np.floor(phase)
        terrace = (smoothstep(0.1, 0.9, saw) - saw) * 62.0
        chips = (lf.fbm(X, Y, 950.0, salt + 3, 3) * 42.0 + (lf.ridged(X, Y, 520.0, salt + 4, 2) - 0.6) * 34.0)
        self.Z = self.Z + rough * (terrace + chips)
        self.rough_mask = rough
        # Only the build disc and the gun platforms stay dead flat: the outer terraces get a whisper of relief (< 2 deg).
        still = np.maximum.reduce([box_blur(self.disc_mask, 12), box_blur(self.platform_mask, 4),
                                   np.clip(box_blur(self.road_mask, 3) * 1.5, 0.0, 1.0)])
        micro = 30.0 * lf.fbm(X, Y, 3000.0, salt + 7, 2) + 7.0 * lf.fbm(X, Y, 650.0, salt + 8, 2)
        self.Z = self.Z + micro * (1.0 - still) * (1.0 - self.disc_mask)

    def oasis_moisture(self):
        """Wet ground round the pool: wide and dark where the washes arrive, thin against the dry side."""
        if hasattr(self, "wet_field"):
            return self.wet_field
        damp = self.paint_masks["damp"]
        inflow = np.zeros_like(self.X)
        for x, y in desert01.OASIS_INFLOWS:
            inflow = np.maximum(inflow, np.exp(-(np.hypot(self.X - x, self.Y - y) / 3400.0) ** 2))
        patchy = 0.5 + 0.5 * lf.fbm(self.X, self.Y, 2400.0, self.c["seed"] + 91, 3)
        self.wet_field = np.clip(box_blur(damp, 5), 0.0, 1.0) ** 1.5 * (0.25 + 0.75 * inflow) * (0.55 + 0.45 * patchy)
        return self.wet_field

    def slope_deg(self):
        gy, gx = np.gradient(self.Z, self.c["quad_cm"])
        return np.degrees(np.arctan(np.hypot(gx, gy)))

    def build_weights(self):
        c = self.c
        X, Y = self.X, self.Y
        slope = self.slope_deg()
        salt = c["seed"] + 999
        n1 = 0.5 + 0.5 * (value_noise(X / 3500.0, Y / 3500.0, salt) + 0.5 * value_noise(X / 1200.0, Y / 1200.0,
                                                                                        salt + 1)) / 1.5
        n2 = 0.5 + 0.5 * (value_noise(X / 5200.0, Y / 5200.0, salt + 7) + 0.5 * value_noise(X / 1700.0, Y / 1700.0,
                                                                                            salt + 8)) / 1.5
        pads = box_blur(self.pad_mask, 6)
        ridge = box_blur(self.ridge_mask, 4)
        road = np.clip(self.road_paint, 0.0, 1.0)
        n3 = 0.5 + 0.5 * lf.fbm(X, Y, 11000.0, salt + 15, 2)
        steep = (slope > 40.0).astype(float)
        high = smoothstep(780.0, 1250.0, box_blur(self.Z, 18))  # 0 on the desert floor, 1 on the tops
        z_local = box_blur(self.Z, 14)
        below = smoothstep(4.0, 45.0, z_local - self.Z)  # the low side of a nearby cliff
        above = smoothstep(4.0, 45.0, self.Z - z_local)
        talus = np.clip(box_blur(steep, 5) * 7.0, 0.0, 1.0) * below
        sand_bank = np.clip(box_blur(steep, 18) * 6.0, 0.0, 1.0) * (1.0 - high * 0.4) * smoothstep(0.0, 25.0, z_local - self.Z)
        lip = np.clip(box_blur(steep, 7) * 6.0, 0.0, 1.0) * above * high
        rock = np.maximum(smoothstep(21.0, 34.0, slope) * (1.0 - 0.85 * smoothstep(0.2, 0.7, road)),
                          self.rock_mask * 0.3 * smoothstep(0.45, 0.8, n1) * (1.0 - pads))
        rock = np.clip(rock + 0.45 * ridge * smoothstep(0.35, 0.7, n2) + 0.62 * talus + 0.34 * lip, 0.0, 1.0)
        wash = self.paint_masks["wash"]
        oasis_wet = self.oasis_moisture()
        salt_paint = self.paint_masks["salt"]
        dune = self.paint_masks["dune"]
        # Tops: hard dark ground. Low ground: pale sand. Value survives any camera angle; patches stay broad and slow.
        gravel = (0.5 * road * (0.55 + 0.45 * n2) + 0.66 * wash + 0.5 * salt_paint + 0.42 * sand_bank * (1.0 - talus)
                  + 0.26 * high * smoothstep(0.45, 0.8, n2) + 0.22 * ridge)
        gravel = gravel * (1.0 - 0.8 * dune)
        weedy = 0.62 * high * (0.55 + 0.45 * n3) * (1.0 - road * 0.7) + 0.16 * (1.0 - high) * smoothstep(0.55, 0.85, n3) + 0.9 * oasis_wet
        weedy = weedy * (1.0 - 0.9 * dune) * (1.0 - sand_bank * 0.7)
        scorch = 0.85 * self.crater_mask
        # Round 7: a worn, compacted band 8-11 m wide under every route, paler and calmer than the sand round it.
        band = self.route_band()
        gravel = np.maximum(gravel, 0.62 * band * (0.85 + 0.15 * n2))
        weedy = weedy * (1.0 - 0.85 * band)
        # Round 7: outside the playable square the dune rim is plain sand (no pale pad paint along its crest).
        import survival_frontier
        x0, x1, y0, y1 = survival_frontier.PLAY
        dout = np.hypot(np.maximum(np.maximum(x0 - X, X - x1), 0.0), np.maximum(np.maximum(y0 - Y, Y - y1), 0.0))
        plain = 1.0 - smoothstep(0.0, 1500.0, dout)
        rock, gravel, weedy, scorch = rock * plain, gravel * plain, weedy * plain, scorch * plain
        concrete = np.zeros_like(X)  # ScorchConcrete tiles as a visible grid
        others = {"DesertRockDark": rock, "DesertGravel": gravel, "Weedy": weedy, "ScorchSandyStone": scorch,
                  "ScorchConcrete": concrete}
        # priority: rock > scorch > the rest; scale so the sum stays <= 1, sand takes the remainder
        remaining = np.ones_like(X)
        out = {}
        for name in ["DesertRockDark", "ScorchSandyStone", "ScorchConcrete", "DesertGravel", "Weedy"]:
            w = np.minimum(np.clip(others[name], 0.0, 1.0), remaining)
            out[name] = w
            remaining = remaining - w
        out["Sand"] = remaining
        # quantise to bytes summing to 255; rounding remainder goes to the strongest layer
        layers = c["layers"]
        q = np.stack([np.floor(out[n] * 255.0 + 0.5) for n in layers]).astype(np.int32)
        diff = 255 - q.sum(axis=0)
        strongest = np.argmax(q, axis=0)
        np.put_along_axis(q, strongest[None], np.take_along_axis(q, strongest[None], 0) + diff[None], 0)
        q = np.clip(q, 0, 255).astype(np.uint8)
        self.weights = {n: q[i] for i, n in enumerate(layers)}
        return self.weights

    def route_band(self):
        """0..1 mask of the worn band under the route lines (desert01.ROUTE_LINES), ragged edges fading over ~2.5 m."""
        band = np.zeros_like(self.X)
        lines = getattr(desert01, "ROUTE_LINES", {})
        if not lines:
            return band
        edge = 420.0 + 130.0 * lf.fbm(self.X, self.Y, 1400.0, self.c["seed"] + 61, 2)
        for pts in lines.values():
            P = np.array(pts, dtype=float)
            x0, x1 = P[:, 0].min() - 1200.0, P[:, 0].max() + 1200.0
            y0, y1 = P[:, 1].min() - 1200.0, P[:, 1].max() + 1200.0
            js = np.where((self.Y[:, 0] >= y0) & (self.Y[:, 0] <= y1))[0]
            is_ = np.where((self.X[0] >= x0) & (self.X[0] <= x1))[0]
            if len(js) == 0 or len(is_) == 0:
                continue
            win = (slice(js[0], js[-1] + 1), slice(is_[0], is_[-1] + 1))
            px, py = self.X[win], self.Y[win]
            d = np.full(px.shape, 1e18)
            for k in range(len(P) - 1):
                ex, ey = P[k + 1, 0] - P[k, 0], P[k + 1, 1] - P[k, 1]
                t = np.clip(((px - P[k, 0]) * ex + (py - P[k, 1]) * ey) / max(ex * ex + ey * ey, 1e-9), 0.0, 1.0)
                d = np.minimum(d, np.hypot(px - (P[k, 0] + t * ex), py - (P[k, 1] + t * ey)))
            band[win] = np.maximum(band[win], 1.0 - smoothstep(edge[win], edge[win] + 250.0, d))
        return band

    # ---- checks ----------------------------------------------------------------------------------------------------
    def ground_z(self, x, y):
        c = self.c
        fx = (x + self.half) / c["quad_cm"]
        fy = (y + self.half) / c["quad_cm"]
        i0 = int(np.clip(math.floor(fx), 0, self.verts - 2))
        j0 = int(np.clip(math.floor(fy), 0, self.verts - 2))
        tx, ty = fx - i0, fy - j0
        z = self.Z
        return float((z[j0, i0] * (1 - tx) + z[j0, i0 + 1] * tx) * (1 - ty) +
                     (z[j0 + 1, i0] * (1 - tx) + z[j0 + 1, i0 + 1] * tx) * ty)

    def to_index(self, x, y, stride=1):
        c = self.c
        i = int(round((x + self.half) / c["quad_cm"] / stride))
        j = int(round((y + self.half) / c["quad_cm"] / stride))
        return j, i

    def reachability(self, max_slope=25.0, stride=2):
        slope = self.slope_deg()[::stride, ::stride]
        walk = slope < max_slope
        seen = np.zeros_like(walk, dtype=bool)
        s0 = self.starts[0]
        start = self.to_index(s0["x"], s0["y"], stride)
        q = deque([start])
        seen[start] = True
        H, W = walk.shape
        while q:
            j, i = q.popleft()
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nj, ni = j + dj, i + di
                if 0 <= nj < H and 0 <= ni < W and walk[nj, ni] and not seen[nj, ni]:
                    seen[nj, ni] = True
                    q.append((nj, ni))
        self.reach = seen
        self.reach_stride = stride
        return seen

    def reachable(self, x, y):
        j, i = self.to_index(x, y, self.reach_stride)
        return bool(self.reach[j, i])

    def report(self):
        c = self.c
        slope = self.slope_deg()
        out = []
        for s, form in zip(self.starts, self.features["landforms"]):
            d = np.hypot(self.X - s["x"], self.Y - s["y"])
            disc = d <= c["build_radius"]
            near = (d > 21000.0) | (d < 8500.0)
            out.append("start %d %-15s z %.0f: disc z range %.3f..%.3f, max disc slope %.3f deg" % (
                s["index"], form["name"], form["z_main"], self.Z[disc].min(), self.Z[disc].max(), slope[disc].max()))
            for ramp in form["ramps"]:
                line = np.array(ramp["line"])
                mask = np.zeros_like(self.X, dtype=bool)
                for px, py in line:
                    j, i = self.to_index(px, py)
                    r = int(max(ramp["half_width"] - 150.0, 100.0) / c["quad_cm"])
                    mask[max(0, j - r):j + r + 1, max(0, i - r):i + r + 1] = True
                mask &= self.ramp_mask > 0.5
                out.append("    ramp %-18s %5.0f -> %5.0f, %5.0f long: slope p95 %4.1f max %4.1f" % (
                    ramp["name"], ramp["z_from"], ramp["z_to"], ramp["length"], np.percentile(slope[mask], 95),
                    slope[mask].max()))
        for ramp in self.features["world"][0]["ramps"]:
            out.append("    ramp %-18s %5.0f -> %5.0f, %5.0f long" % (ramp["name"], ramp["z_from"], ramp["z_to"], ramp["length"]))
        open_ground = (self.raised < 1.0) & (self.crater_mask < 0.05) & (self.rock_mask < 0.05)
        out.append("open desert: p99 slope %.1f deg, max %.1f" % (np.percentile(slope[open_ground], 99), slope[open_ground].max()))
        out.append("Z range %.0f .. %.0f" % (self.Z.min(), self.Z.max()))
        return out

    def line_of_sight(self, a, b, eye=260.0, target=120.0, edge=800.0):
        steps = 24
        # Guns stand at the edge of the platform that faces the target, not at its middle.
        d0 = math.hypot(b[0] - a[0], b[1] - a[1]) or 1.0
        a = (a[0] + (b[0] - a[0]) / d0 * min(edge, d0), a[1] + (b[1] - a[1]) / d0 * min(edge, d0))
        za = self.ground_z(*a) + eye
        zb = self.ground_z(*b) + target
        for k in range(1, steps):
            t = k / float(steps)
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            if self.ground_z(x, y) + 20.0 > za + (zb - za) * t:
                return False
        return True

    def coverage(self, cannon=3680.0):
        """For each gate: which platforms reach its climb with a medium cannon, with line of sight, and how much of it."""
        out = []
        for s, form in zip(self.starts, self.features["landforms"]):
            for ramp in form["ramps"]:
                if not ramp["gate"]:
                    continue
                line = np.array(ramp["line"])
                seg = np.hypot(*(line[1:] - line[:-1]).T)
                cum = np.concatenate([[0.0], np.cumsum(seg)])
                climb = min(cum[-1], max(2400.0, abs(ramp["z_to"] - ramp["z_from"]) * 5.2))
                d = np.arange(cum[-1] - climb, cum[-1], 300.0)
                points = np.stack([np.interp(d, cum, line[:, 0]), np.interp(d, cum, line[:, 1])], -1)
                upper = points[len(points) // 2:]
                best_all, best_up, best_name, best_near = 0.0, 0.0, "NONE", 1e9
                any_hit = np.zeros(len(points), dtype=bool)
                for p in form["platforms"]:
                    near = np.hypot(points[:, 0] - p["x"], points[:, 1] - p["y"])
                    hit = np.array([n <= cannon and self.line_of_sight((p["x"], p["y"]), q) for n, q in zip(near, points)])
                    any_hit |= hit
                    up = hit[len(points) // 2:].mean()
                    if up > best_up or (up == best_up and near.min() < best_near):
                        best_all, best_up, best_name, best_near = hit.mean(), up, p["name"], near.min()
                dist = math.hypot(s["x"] - line[-1][0], s["y"] - line[-1][1])
                out.append("start %d %-14s climb %5.0f: %3.0f%% of climb in cannon range with sight (any platform), upper half %3.0f%% by %-18s nearest %5.0f, head r %5.0f" % (
                    s["index"], ramp["name"], climb, any_hit.mean() * 100.0, best_up * 100.0, best_name, best_near, dist))
        return out

    def scarp_leaks(self):
        """Ramps closed, can anything still climb from the open desert onto a build disc over navmesh-walkable ground?"""
        slope = self.slope_deg()
        h, w = slope.shape
        h2, w2 = (h - 1) // 2, (w - 1) // 2
        block = np.maximum.reduce([slope[:h2 * 2:2, :w2 * 2:2], slope[1:h2 * 2:2, :w2 * 2:2], slope[:h2 * 2:2, 1:w2 * 2:2],
                                   slope[1:h2 * 2:2, 1:w2 * 2:2]])
        walk = (block < lf.NAV_MAX_SLOPE) & (self.road_mask[:h2 * 2:2, :w2 * 2:2] < 0.02)
        # Recast erodes walkable ground by the agent radius: a slit under a vehicle's width is no way up.
        closed = walk.copy()
        closed[1:, :] &= walk[:-1, :]
        closed[:-1, :] &= walk[1:, :]
        closed[:, 1:] &= walk[:, :-1]
        closed[:, :-1] &= walk[:, 1:]
        walk = closed
        seen = np.zeros_like(walk, dtype=bool)
        parent = np.full(walk.shape + (2,), -1, dtype=np.int32)
        queue = deque([(40, 40)])
        seen[40, 40] = True
        while queue:
            j, i = queue.popleft()
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nj, ni = j + dj, i + di
                if 0 <= nj < h2 and 0 <= ni < w2 and walk[nj, ni] and not seen[nj, ni]:
                    seen[nj, ni] = True
                    parent[nj, ni] = (j, i)
                    queue.append((nj, ni))
        self.leak_parent = parent
        self.leak_seen = seen
        out = []
        for s, form in zip(self.starts, self.features["landforms"]):
            j, i = self.to_index(s["x"], s["y"], 2)
            tiers = []
            for pad in form["pads"] + form["platforms"]:
                if "outline" in pad:
                    x, y = np.mean(np.array(pad["outline"]), axis=0)
                else:
                    x, y = pad["x"], pad["y"]
                jj, ii = self.to_index(x, y, 2)
                tiers.append("%s:%s" % (pad["name"], "OPEN" if seen[min(jj, h2 - 1), min(ii, w2 - 1)] else "closed"))
            out.append("start %d with ramps closed: disc %s; %s" % (s["index"], "OPEN" if seen[j, i] else "closed", " ".join(tiers)))
            if seen[j, i]:
                # The way in, from the desert: the first point where it stands on the start's own height.
                path = []
                cj, ci = j, i
                while cj >= 0:
                    path.append((cj, ci))
                    cj, ci = parent[cj, ci]
                path.reverse()
                z_main = self.Z[self.to_index(s["x"], s["y"])]
                for n, (pj, pi) in enumerate(path):
                    z = self.Z[pj * 2, pi * 2]
                    if z > z_main - 60.0:
                        for qj, qi in path[max(0, n - 30):n + 3:3]:
                            out.append("    leak step world (%.0f, %.0f) z %.0f slope %.1f" % (self.X[qj * 2, qi * 2], self.Y[qj * 2, qi * 2], self.Z[qj * 2, qi * 2], block[qj, qi]))
                        break
        return out

    def field_ground_ok(self, x, y, max_slope, ring_radius, slope):
        """Same test as FSurvivalStartContentPlanner::GetIsFieldGroundValid."""
        j, i = self.to_index(x, y)
        if slope[j, i] > max_slope:
            return False
        z0 = self.ground_z(x, y)
        limit = math.tan(math.radians(max_slope)) * ring_radius
        for k in range(8):
            px = x + ring_radius * math.cos(k * math.pi / 4.0)
            py = y + ring_radius * math.sin(k * math.pi / 4.0)
            j, i = self.to_index(px, py)
            if slope[j, i] > max_slope or abs(self.ground_z(px, py) - z0) > limit:
                return False
        return True

    def plan_fields_once(self, s, defense, count_each, rng, slope):
        """Python port of FSurvivalStartContentPlanner::PlanResourceFields for one start, seed and difficulty."""
        kinds = dict(radixite=(1800.0, 600.0, 1200.0, 30.0), metal=(2600.0, 600.0, 800.0, 25.0))
        circles = [(s["x"], s["y"], self.c["build_radius"])]
        circles += [(p["x"], p["y"], 900.0) for p in defense]
        order = []
        for _ in range(count_each):
            order += ["radixite", "metal"]
        slot = 2.0 * math.pi / len(order)
        first = rng.uniform(0.0, 2.0 * math.pi)
        step = min(slot * 0.25, math.radians(12.0))
        placed = []
        for n, kind in enumerate(order):
            radius, gap, extra, max_slope = kinds[kind]
            preferred = first + n * slot
            for attempt in range(48):
                steps = (attempt + 1) // 2
                angle = preferred + (steps if attempt % 2 == 1 else -steps) * step
                sweep = attempt // 16
                dist = self.c["build_radius"] + gap + radius + rng.uniform(0.0, extra) + sweep * radius * 0.5
                x = s["x"] + dist * math.cos(angle)
                y = s["y"] + dist * math.sin(angle)
                if any(math.hypot(x - cx, y - cy) < cr + radius + 400.0 for cx, cy, cr in circles):
                    continue
                if not self.field_ground_ok(x, y, max_slope, radius * 0.75, slope):
                    continue
                circles.append((x, y, radius))
                placed.append((kind, x, y, radius, self.ground_z(x, y)))
                break
        return placed

    def simulate_planner(self, seeds=120):
        """Fields per start and difficulty: placed, reachable from the base, and how they spread around it."""
        rng = np.random.default_rng(self.c["seed"] + 5)
        slope = self.slope_deg()
        out = []
        layout_points = {s["index"]: self.defense_candidates(s, form)[:6] for s, form in zip(self.starts, self.features["landforms"])}
        for s in self.starts:
            for label, count_each in (("Normal/Hard/Brutal/Ironman (2+2)", 2), ("NewToRTS (3+3)", 3)):
                placed_counts = []
                unreachable = 0
                on_level = 0
                total = 0
                gaps = []
                for _ in range(seeds):
                    fields = self.plan_fields_once(s, layout_points[s["index"]], count_each, rng, slope)
                    placed_counts.append(len(fields))
                    angles = sorted(math.atan2(f[2] - s["y"], f[1] - s["x"]) for f in fields)
                    if len(angles) > 1:
                        spread = [b - a for a, b in zip(angles, angles[1:])] + [angles[0] + 2 * math.pi - angles[-1]]
                        gaps.append(math.degrees(max(spread)))
                    for f in fields:
                        total += 1
                        if not self.reachable(f[1], f[2]):
                            unreachable += 1
                        if abs(f[4] - self.Z[self.to_index(s["x"], s["y"])]) < 900.0:
                            on_level += 1
                out.append("start %d %-14s %s: fields min %d avg %.1f of %d, unreachable %d of %d, within 9 m of base height %.0f%%, "
                           "widest empty sector avg %.0f max %.0f deg" % (
                               s["index"], self.features["landforms"][s["index"]]["name"], label, min(placed_counts),
                               sum(placed_counts) / float(seeds), count_each * 2, unreachable, total,
                               100.0 * on_level / max(total, 1), sum(gaps) / max(len(gaps), 1), max(gaps) if gaps else 0))
            d = np.hypot(self.X - s["x"], self.Y - s["y"])
            band = (d > 13000.0) & (d < 17500.0)
            ok = band & (slope <= 35.0)
            out.append("start %d wreck band r13000-17500: %.0f%% under 35 deg" % (s["index"], 100.0 * ok.sum() / band.sum()))
        return out

    # ---- layout ----------------------------------------------------------------------------------------------------
    def defense_candidates(self, s, form):
        """Spots for guns: the wide stretches of every ridge top, then the heads of the ramps that come up from the desert."""
        guards = [o["name"] for o in self.c["landforms"][s["index"]]["ops"] if o["op"] == "platform" and o.get("covers")]
        points = [dict(kind="ridge_top", name=p["name"], x=p["x"], y=p["y"], z=self.ground_z(p["x"], p["y"]), guard=p["name"] in guards)
                  for p in form["platforms"]]
        for ramp in form["ramps"]:
            # Ramp heads on the main terrace, or wherever a road comes up from the desert.
            if ramp["gate"] or abs(ramp["z_to"] - form["z_main"]) < 1.0:
                x, y = ramp["line"][-1]
                points.append(dict(kind="ramp_head", name=ramp["name"], x=x, y=y, z=self.ground_z(x, y)))
        for p in points:
            p["reachable"] = self.reachable(p["x"], p["y"])
            p["distance"] = math.hypot(p["x"] - s["x"], p["y"] - s["y"])
        # Soviet gun pits only use points within 1.6 * build radius: closest ridge tops first, ramp heads after them.
        points.sort(key=lambda p: (not p.get("guard", False), p["kind"] != "ridge_top", p["distance"]))
        return points

    def layout(self):
        c = self.c
        starts = []
        for s, form in zip(self.starts, self.features["landforms"]):
            starts.append(dict(index=s["index"], name=form["name"], x=s["x"], y=s["y"], z=form["z_main"],
                               yaw_to_map_centre_deg=math.degrees(math.atan2(s["inward"][1], s["inward"][0])),
                               build_radius=c["build_radius"], flat_radius=lf.BUILD_FLAT,
                               resource_ring=c["resource_ring"], wreck_ring=c["wreck_ring"],
                               defense_points=self.defense_candidates(s, form),
                               edge_clearance=float(min(self.half - abs(s["x"]), self.half - abs(s["y"]))),
                               reachable=self.reachable(s["x"], s["y"])))
        zones = []
        for (x, y, length) in c["edge_zones"]:
            dists = {"+X": self.half - x, "-X": self.half + x, "+Y": self.half - y, "-Y": self.half + y}
            edge = min(dists, key=dists.get)
            inward = {"+X": (-1.0, 0.0), "-X": (1.0, 0.0), "+Y": (0.0, -1.0), "-Y": (0.0, 1.0)}[edge]
            zones.append(dict(x=x, y=y, z=self.ground_z(x, y), edge=edge, length=length, inward=inward,
                              yaw_deg=math.degrees(math.atan2(inward[1], inward[0])),
                              reachable=self.reachable(x, y)))
        return dict(map=self.name, landscape=dict(
            corner=(-self.half, -self.half), size_cm=2 * self.half, vertices=self.verts,
            components=c["components"], quads_per_component=c["quads"], quad_cm=c["quad_cm"], base_z=c["base_z"]),
            starts=starts, edge_zones=zones, features=self.features, playable=self.playable())

    def playable(self):
        import survival_frontier
        x0, x1, y0, y1 = survival_frontier.PLAY
        return dict(x0=x0, x1=x1, y0=y0, y1=y1)

    # ---- dressing data ------------------------------------------------------------------------------------------------
    def sample_bins(self, mask, spacing, rng, keep=1.0):
        """One random cell of `mask` per bin of `spacing` cm: evenly spread but irregular spots."""
        j, i = np.nonzero(mask)
        if len(j) == 0:
            return []
        order = rng.permutation(len(j))
        bins = {}
        step = spacing / self.c["quad_cm"]
        for k in order:
            key = (int(j[k] / step), int(i[k] / step))
            if key not in bins:
                bins[key] = (int(j[k]), int(i[k]))
        return [v for v in bins.values() if rng.random() < keep]

    def local_drop(self, j, i, r=7):
        z = self.Z[max(j - r, 0):j + r + 1, max(i - r, 0):i + r + 1]
        return float(z.max() - z.min())

    def dress_data(self):
        """Spots for survival_map_dress.py, chosen here because only the generator knows slopes and masks."""
        c = self.c
        rng = np.random.default_rng(c["seed"] + 77)
        slope = self.slope_deg()
        gy, gx = np.gradient(box_blur(self.Z, 2), c["quad_cm"])
        road = box_blur((self.road_mask > 0.02).astype(float), 6) > 0.01
        disc = self.dist_to_nearest_start(self.X, self.Y)
        forms = self.rock_mask > 0.0
        massif = box_blur(np.maximum(self.rock_mask, self.pad_mask), 20) > 0.01

        def spot(j, i, **extra):
            d = dict(x=float(self.X[j, i]), y=float(self.Y[j, i]), z=float(self.Z[j, i]),
                     downhill=float(math.degrees(math.atan2(-gy[j, i], -gx[j, i]))), slope=float(slope[j, i]),
                     start=int(np.argmin([math.hypot(self.X[j, i] - s["x"], self.Y[j, i] - s["y"]) for s in self.starts])))
            d.update(extra)
            return d

        cliff = (slope > 40.0) & forms & (disc > 8600.0) & (self.platform_mask < 0.5) & ((self.ramp_mask < 0.5))
        faces = self.rock_chains(cliff, rng, gx, gy)
        talus = (slope > 7.0) & (slope < 27.0) & (box_blur((slope > 42.0).astype(float), 8) > 0.02) & ~road & (disc > 8800.0)
        near = box_blur((slope > 40.0).astype(float), 3)
        foot = [spot(j, i, near=float(near[j, i])) for j, i in self.sample_bins(talus, 900.0, rng, 0.7)]
        foot = [q for q in foot if q["near"] > 0.08]
        crest = (self.ridge_mask > 0.5) & (self.platform_mask < 0.5) & ~road & (box_blur(self.platform_mask, 9) < 0.01)
        tops = [spot(j, i) for j, i in self.sample_bins(crest, 1000.0, rng, 0.75)]
        terrace = (self.pad_mask > 0.5) & (slope < 6.0) & ~road & (disc > 9000.0) & (box_blur((slope > 20.0).astype(float), 5) < 0.01)
        shelf = [spot(j, i) for j, i in self.sample_bins(terrace, 2400.0, rng, 0.8)]
        edge = self.edge_distance(self.X, self.Y)
        open_ground = (~massif & ~road & (slope < 9.0) & (disc > 16000.0) & (edge > 9000.0) & (self.crater_mask < 0.02)
                       & (box_blur(self.paint_masks["salt"], 12) < 0.02))
        open_spots = [spot(j, i) for j, i in self.sample_bins(open_ground, 2200.0, rng, 1.0)]
        return dict(faces=faces, foot=foot, tops=tops, terrace=shelf, open=open_spots, oasis=self.oasis_spots(rng),
                    water_boxes=self.water_boxes())

    def water_boxes(self, strip=700.0, inset=60.0):
        """Null-area nav boxes over the pool: wading tanks would look wrong, so the water is off the navmesh."""
        basin = [b for f in self.features["world"] for b in f["basins"] if b.get("water_z")][0]
        poly = np.array(basin["outline"])
        boxes = []
        for y in np.arange(poly[:, 1].min() - 1500.0, poly[:, 1].max() + 1500.0, strip):
            j, i0 = self.to_index(poly[:, 0].min() - 2500.0, y)
            _, i1 = self.to_index(poly[:, 0].max() + 2500.0, y)
            row = self.Z[j, i0:i1] <= basin["water_z"] - 2.0
            if row.any():
                xs = self.X[j, i0:i1][row]
                boxes.append((float(xs.min() + inset), float(xs.max() - inset), float(y), float(y + strip)))
        return boxes

    def oasis_spots(self, rng):
        """Plants by moisture: reeds in the shallows, palm groves where water arrives, then bank plants, scrub and bare sand."""
        basin = [b for f in self.features["world"] for b in f["basins"] if b.get("water_z")][0]
        level = basin["water_z"]
        wet = self.oasis_moisture()
        slope = self.slope_deg()
        road = box_blur((self.road_mask > 0.02).astype(float), 5) > 0.01
        dry = self.Z > level + 8.0
        ground = dry & (slope < 14.0) & ~road
        out = dict(reeds=[], palms=[], bank=[], scrub=[])

        def cell(j, i):
            return float(self.X[j, i]), float(self.Y[j, i])

        shallow = (self.Z <= level - 3.0) & (self.Z > level - 42.0) & (box_blur(wet, 4) > 0.12)
        for j, i in self.sample_bins(shallow, 300.0, rng, 0.75):
            x, y = cell(j, i)
            out["reeds"].append(dict(x=x, y=y))
        # Groves: centres on the wettest ground, 5-9 trunks each, tight and uneven.
        groves = []
        candidates = self.sample_bins(ground & (wet > 0.3), 1500.0, rng, 1.0)
        rng.shuffle(candidates)
        for j, i in candidates[:8]:
            cx, cy = cell(j, i)
            groves.append((cx, cy))
            for k in range(int(rng.integers(5, 10))):
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = rng.uniform(0.0, 1.0) ** 0.7 * 620.0
                x, y = cx + r * math.cos(a), cy + r * math.sin(a)
                jj, ii = self.to_index(x, y)
                if ground[jj, ii]:
                    out["palms"].append(dict(x=x, y=y, scale=float(rng.uniform(0.65, 1.25)), lean=float(rng.uniform(-7.0, 7.0))))
        for j, i in self.sample_bins(ground & (wet > 0.1), 330.0, rng, 0.55):
            x, y = cell(j, i)
            out["bank"].append(dict(x=x, y=y, wet=float(wet[j, i])))
        for j, i in self.sample_bins(ground & (wet > 0.012) & (wet <= 0.1), 800.0, rng, 0.6):
            x, y = cell(j, i)
            out["scrub"].append(dict(x=x, y=y))
        return out

    def salt_decal(self, out_dir):
        """Mask + tone texture for the dry lake bed decal: R = opacity with a ragged edge, G = brightness (silt tone and wind
        streaks; no crack cells, the owner did not like the tiled polygon look), B unused."""
        from PIL import Image
        form = self.features["landforms"][1]
        pan = np.array([p for p in form["pads"] if p["name"] == "salt_pan"][0]["outline"])
        margin = 1200.0
        x0, x1 = pan[:, 0].min() - margin, pan[:, 0].max() + margin
        y0, y1 = pan[:, 1].min() - margin, pan[:, 1].max() + margin
        size = 2048
        side = max(x1 - x0, y1 - y0)
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        u = np.linspace(cx - side / 2.0, cx + side / 2.0, size)
        v = np.linspace(cy - side / 2.0, cy + side / 2.0, size)
        X, Y = np.meshgrid(u, v)
        # Bilinear from a slightly blurred mask: the 1 m grid would otherwise stair-step the edge.
        soft = box_blur(self.paint_masks["salt"], 2)
        fx = np.clip((X + self.half) / 100.0, 0.0, self.verts - 1.001)
        fy = np.clip((Y + self.half) / 100.0, 0.0, self.verts - 1.001)
        i, j = fx.astype(int), fy.astype(int)
        tx, ty = fx - i, fy - j
        mask = ((soft[j, i] * (1 - tx) + soft[j, i + 1] * tx) * (1 - ty) + (soft[j + 1, i] * (1 - tx) + soft[j + 1, i + 1] * tx) * ty)
        tone = 0.86 + 0.14 * (0.5 + 0.5 * lf.fbm(X, Y, 2600.0, 77, 3))
        edge = lf.fbm(X, Y, 1400.0, 78, 4)
        speck = lf.fbm(X, Y, 260.0, 79, 3)
        # A fringe 10-30 m wide: salt thins into patches over the sand, it does not stop at a line.
        thin = smoothstep(0.12, 0.92, mask + 0.26 * edge)
        alpha = np.clip(thin * 1.25 - 0.28 * (1.0 - thin) * (0.5 + 0.5 * speck) * 2.0, 0.0, 1.0) * smoothstep(-0.25, 0.25, thin - 0.35 + 0.5 * speck)
        streak = lf.fbm(X * 0.35 + Y * 0.12, Y * 0.9 - X * 0.2, 2000.0, 80, 3)
        dirt = 0.82 + 0.18 * smoothstep(-0.5, 0.2, streak)
        # Fine wind ripples across the silt, warped so they never read as a grid, plus darker damp hollows.
        warp = lf.fbm(X, Y, 1800.0, 81, 2) * 260.0
        ripple = 0.5 + 0.5 * np.sin((X * 0.94 + Y * 0.34 + warp) / 55.0)
        ripple_mask = 0.5 + 0.5 * lf.fbm(X, Y, 900.0, 82, 2)
        hollow = smoothstep(0.15, 0.6, lf.fbm(X, Y, 5200.0, 83, 3))
        tone = tone * (1.0 - 0.06 * ripple * ripple_mask) * (1.0 - 0.05 * hollow)
        # Round 6: crust at the half-metre scale and thin pressure ridges, so the flat reads as crust, not fog; the broad
        # smudges are halved.
        crust = 0.5 + 0.5 * lf.fbm(X, Y, 110.0, 84, 2)
        ridges = smoothstep(0.82, 0.97, lf.ridged(X, Y, 900.0, 85, 2))
        # Round 7 (coordinator): a plain, slightly paler sand crust that blends with the sand: no cloud, faint crust only.
        tone = 0.96 + 0.025 * crust - 0.02 * ridges + 0.0 * tone
        dirt = np.ones_like(dirt)
        img = np.stack([alpha, tone * dirt, np.zeros_like(alpha)], -1)
        Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save(os.path.join(out_dir, "T_SM_SaltPanDecal.png"))
        return dict(centre=(cx, cy), side=side)

    def rock_chains(self, cliff, rng, gx, gy):
        """Walks the cliff cells into contour chains and lays overlapping rock along stretches of them, with real gaps
        between the stretches. Every piece faces downhill; the landscape under it stays too steep to drive."""
        spots = self.sample_bins(cliff, 420.0, rng, 1.0)
        pts = np.array([(self.X[j, i], self.Y[j, i]) for j, i in spots])
        used = np.zeros(len(pts), dtype=bool)
        chains = []
        for seed in rng.permutation(len(pts)):
            if used[seed]:
                continue
            used[seed] = True
            chain = [seed]
            for direction in (0, 1):
                cur = seed
                while True:
                    d = np.hypot(pts[:, 0] - pts[cur, 0], pts[:, 1] - pts[cur, 1])
                    d[used] = 1e9
                    k = int(np.argmin(d))
                    if d[k] > 950.0:
                        break
                    used[k] = True
                    cur = k
                    if direction == 0:
                        chain.append(k)
                    else:
                        chain.insert(0, k)
            if len(chain) >= 3:
                chains.append(chain)
        n_band = lf.fbm(self.X, self.Y, 5200.0, self.c["seed"] + 41, 2)
        faces = []
        for chain in chains:
            q = pts[chain]
            seg = np.hypot(*(q[1:] - q[:-1]).T)
            cum = np.concatenate([[0.0], np.cumsum(seg)])
            along = float(rng.uniform(0.0, 500.0))
            while along < cum[-1]:
                x = float(np.interp(along, cum, q[:, 0]))
                y = float(np.interp(along, cum, q[:, 1]))
                j, i = self.to_index(x, y)
                dx = float(np.interp(along + 300.0, cum, q[:, 0])) - float(np.interp(along - 300.0, cum, q[:, 0]))
                dy = float(np.interp(along + 300.0, cum, q[:, 1])) - float(np.interp(along - 300.0, cum, q[:, 1]))
                drop = self.local_drop(j, i, 11)
                length = max(520.0, min(1900.0, 1.2 * drop))
                if n_band[j, i] > -0.62 and drop > 200.0:
                    downhill = math.degrees(math.atan2(-gy[j, i], -gx[j, i]))
                    tangent = math.degrees(math.atan2(dy, dx))
                    # the contour direction, turned so the mesh's front (-Y) faces downhill
                    yaw = tangent if math.cos(math.radians(tangent - 90.0 - downhill)) < 0.0 else tangent + 180.0
                    faces.append(dict(x=x, y=y, z=float(self.Z[j, i]), drop=drop, length=length, yaw=yaw, downhill=downhill,
                                      start=int(np.argmin([math.hypot(x - s["x"], y - s["y"]) for s in self.starts]))))
                along += length * 0.7
        return faces

    def pole_ground_ok(self, x, y, slope, road):
        j, i = self.to_index(x, y)
        if not (6 < j < self.verts - 7 and 6 < i < self.verts - 7):
            return False
        if road[j, i] or slope[j, i] > 11.0:
            return False
        d_start = float(self.dist_to_nearest_start(np.array(x), np.array(y)))
        if d_start < 9200.0 or self.crater_mask[j, i] > 0.02 or self.platform_mask[j, i] > 0.5:
            return False
        if d_start < 14800.0 and self.ridge_mask[j, i] < 0.5:
            return False  # the resource band stays free
        return self.paint_masks["damp"][j, i] < 0.6

    def snap_corner(self, x, y, slope, road, name):
        """Corner towers may shift a little onto sound ground; tangents then run between the snapped corners."""
        for radius in (0.0, 300.0, 600.0, 900.0, 1300.0, 1800.0, 2400.0):
            for k in range(1 if radius == 0.0 else 12):
                px, py = x + radius * math.cos(k * math.pi / 6.0), y + radius * math.sin(k * math.pi / 6.0)
                if self.pole_ground_ok(px, py, slope, road):
                    return px, py
        print("POWER: corner without ground", name, x, y)
        return x, y

    def plan_power_line(self, spec, rng):
        """Straight tangents between corner towers. Spans come from the ground: from each pole the next one is the best
        high spot in reach, so spans run long over dips and short on rises and before a corner."""
        # Footing slope at tower scale: dune ripples do not matter, a scarp does.
        gy, gx = np.gradient(box_blur(self.Z, 3), self.c["quad_cm"])
        slope = np.degrees(np.arctan(np.hypot(gx, gy)))
        road = box_blur((self.road_mask > 0.02).astype(float), 7) > 0.01
        corners = [self.snap_corner(x, y, slope, road, spec["name"]) for x, y in spec["corners"]]
        poles = []
        min_span, max_span = 4500.0, 7300.0
        for k in range(len(corners) - 1):
            ax, ay = corners[k]
            bx, by = corners[k + 1]
            total = math.hypot(bx - ax, by - ay)
            ux, uy = (bx - ax) / total, (by - ay) / total
            if k == 0:
                poles.append(dict(x=ax, y=ay, corner=True))
            along = 0.0
            while total - along > max_span:
                best = None
                reach = max_span - float(rng.uniform(0.0, 900.0))  # a varied cap, so the limit does not show as a rhythm
                top = min(along + reach, total - min_span * 0.75)
                for d in np.arange(along + min_span, top + 1.0, 100.0):
                    x, y = ax + ux * d, ay + uy * d
                    if not self.pole_ground_ok(x, y, slope, road):
                        continue
                    # A slight high point and reach both score; a dip ahead is simply stepped over.
                    score = self.ground_z(x, y) * 1.6 + d * 0.035
                    if best is None or score > best[0]:
                        best = (score, d)
                if best is None:
                    print("POWER: no pole ground on", spec["name"], "tangent", k, "after", round(along),
                          "near", round(ax + ux * (along + 6000)), round(ay + uy * (along + 6000)))
                    along += reach
                    continue
                along = best[1]
                poles.append(dict(x=ax + ux * along, y=ay + uy * along, corner=False))
            poles.append(dict(x=bx, y=by, corner=True))
        for n, pole in enumerate(poles):
            pole["z"] = self.ground_z(pole["x"], pole["y"])
            a = poles[max(n - 1, 0)]
            b = poles[min(n + 1, len(poles) - 1)]
            pole["yaw"] = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
            pole["kind"] = spec["corner_kind"] if pole["corner"] else spec["tangent_kind"]
            if not self.pole_ground_ok(pole["x"], pole["y"], slope, road):
                print("POWER: bad ground at", spec["name"], n, round(pole["x"]), round(pole["y"]))
        return dict(name=spec["name"], branch_of=spec.get("branch_of"), poles=poles)

    def wire_clearance(self, line, sockets):
        """Lowest wire above the ground for each span, with the deepest droop the pole Blueprints can roll."""
        report = []
        for a, b in zip(line["poles"][:-1], line["poles"][1:]):
            low = 1e9
            for sa, sb in zip(sockets[a["kind"]], sockets[b["kind"]]):
                ya, yb = math.radians(a["yaw"]), math.radians(b["yaw"])
                pa = np.array([a["x"] + sa[0] * math.cos(ya) - sa[1] * math.sin(ya),
                               a["y"] + sa[0] * math.sin(ya) + sa[1] * math.cos(ya), a["z"] + sa[2]])
                pb = np.array([b["x"] + sb[0] * math.cos(yb) - sb[1] * math.sin(yb),
                               b["y"] + sb[0] * math.sin(yb) + sb[1] * math.cos(yb), b["z"] + sb[2]])
                mid = (pa + pb) * 0.5 - np.array([0.0, 0.0, 800.0])
                for t in np.linspace(0.0, 1.0, 21):
                    q = (1 - t) ** 2 * pa + 2 * (1 - t) * t * mid + t * t * pb
                    low = min(low, q[2] - self.ground_z(q[0], q[1]))
            report.append(low)
        return report

    def local_offset(self, s, q):
        """Local (x, y, radius) to world-oriented offset from the deploy centre, for ReservedContentSpots."""
        x, y = self.local_to_world(s, q[0], q[1])
        return x - s["x"], y - s["y"], q[2]

    def write(self, out_dir, skip_dress=False):
        os.makedirs(out_dir, exist_ok=True)
        heights = np.clip(np.round(32768.0 + self.Z * 128.0 / 100.0), 0, 65535).astype("<u2")
        hp = os.path.join(out_dir, "%s_height.raw" % self.name)
        heights.tofile(hp)
        paths = []
        for n in self.c["layers"]:
            p = os.path.join(out_dir, "%s_%s.raw" % (self.name, n))
            self.weights[n].astype(np.uint8).tofile(p)
            paths.append(p)
        lay = self.layout()
        for form, spec in zip(self.features["landforms"], self.c["landforms"]):
            form["no_fields"] = [list(self.local_offset(self.starts[form["start"]], q)) for q in spec.get("no_fields", [])]
        lay = self.layout()
        lay["dress"] = {} if skip_dress else self.dress_data()
        lay["salt_decal"] = self.salt_decal(out_dir)
        rng = np.random.default_rng(self.c["seed"] + 33)
        lay["power_lines"] = [self.plan_power_line(spec, rng) for spec in self.c["power_lines"]]
        for line in lay["power_lines"]:
            spans = [math.hypot(a["x"] - b["x"], a["y"] - b["y"]) for a, b in zip(line["poles"][:-1], line["poles"][1:])]
            low = self.wire_clearance(line, POLE_SOCKETS)
            for k, v in enumerate(low):
                if v < 300.0:
                    a = line["poles"][k]
                    print("  wire low %.0f at span %d (%.0f, %.0f) length %.0f" % (v, k, a["x"], a["y"], spans[k]))
            ends = (line["poles"][0], line["poles"][-1])
            for end in ends:
                gap = min(max(abs(end["x"] - z["x"]) - (z["length"] / 2.0 if z["edge"] in ("+Y", "-Y") else 300.0),
                              abs(end["y"] - z["y"]) - (z["length"] / 2.0 if z["edge"] in ("+X", "-X") else 300.0))
                          for z in lay["edge_zones"])
                print("  end pole (%.0f, %.0f): %.0f cm from the nearest spawn zone" % (end["x"], end["y"], gap))
            print("power %-10s %3d poles, spans %.0f..%.0f, lowest wire %.0f cm above ground" % (
                line["name"], len(line["poles"]), min(spans), max(spans), min(low)))
        lay["files"] = dict(height=hp, layers=dict(zip(self.c["layers"], paths)))
        with open(os.path.join(out_dir, "%s_layout.json" % self.name), "w") as f:
            json.dump(lay, f, indent=1, default=float)
        try:
            self.preview(os.path.join(out_dir, "%s_preview.png" % self.name))
        except ImportError:
            pass
        return lay

    def preview(self, path, size=1024):
        from PIL import Image
        stride = max(1, self.verts // size)
        Z = self.Z[::stride, ::stride]
        gy, gx = np.gradient(Z, self.c["quad_cm"] * stride)
        shade = np.clip(0.6 + 0.4 * (-gx * 0.7 - gy * 0.7) / np.sqrt(1 + gx * gx + gy * gy) * 3, 0, 1)
        w = {n: self.weights[n][::stride, ::stride] / 255.0 for n in self.weights}
        cols = {"Sand": (0.86, 0.72, 0.52), "DesertGravel": (0.62, 0.58, 0.52), "DesertRockDark": (0.36, 0.30, 0.26),
                "ScorchConcrete": (0.5, 0.5, 0.5), "ScorchSandyStone": (0.55, 0.42, 0.30), "Weedy": (0.58, 0.50, 0.38)}
        rgb = np.zeros(Z.shape + (3,))
        for n, col in cols.items():
            if n in w:
                rgb += w[n][..., None] * np.array(col)
        rgb *= shade[..., None]
        reach = getattr(self, "reach", None)
        if reach is not None:
            k = self.reach_stride
            full = np.repeat(np.repeat(reach, k, 0), k, 1)[:self.verts, :self.verts]
            r = full[::stride, ::stride][:rgb.shape[0], :rgb.shape[1]]
            rgb[~r] = rgb[~r] * 0.5 + np.array([0.5, 0.0, 0.0]) * 0.5
        img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1])  # +Y up
        img.save(path)


def shade(Z, step, sun=(-0.55, -0.45, 0.70)):
    gy, gx = np.gradient(Z, step)
    n = np.sqrt(1.0 + gx * gx + gy * gy)
    return np.clip((-gx * sun[0] - gy * sun[1] + sun[2]) / n, 0.0, 1.0)


def preview_starts(g, path, half=30000.0):
    """Per start: shaded relief with contours (image up = +Y), steep ground in red, ramps blue, build disc outlined."""
    from PIL import Image
    tiles = []
    c = g.c
    for s in g.starts:
        i0, i1 = int((s["x"] - half + g.half) / 100.0), int((s["x"] + half + g.half) / 100.0)
        j0, j1 = int((s["y"] - half + g.half) / 100.0), int((s["y"] + half + g.half) / 100.0)
        win = (slice(max(j0, 0), j1), slice(max(i0, 0), i1))
        Z = g.Z[win]
        lum = shade(Z, 100.0)
        rgb = np.stack([0.96 * lum, 0.80 * lum, 0.62 * lum], -1)
        band = (np.floor(Z / 100.0) != np.floor(np.roll(Z, 1, 0) / 100.0)) | (np.floor(Z / 100.0) != np.floor(np.roll(Z, 1, 1) / 100.0))
        rgb[band] *= 0.75
        gy, gx = np.gradient(Z, 100.0)
        slope = np.degrees(np.arctan(np.hypot(gx, gy)))
        walk = (slope >= 25.0) & (slope < lf.NAV_MAX_SLOPE)
        rgb[walk] = rgb[walk] * 0.6 + np.array([0.9, 0.7, 0.0]) * 0.4
        rgb[slope >= lf.NAV_MAX_SLOPE] = rgb[slope >= lf.NAV_MAX_SLOPE] * 0.5 + np.array([0.9, 0.05, 0.05]) * 0.5
        r = g.ramp_mask[win] > 0.5
        rgb[r] = rgb[r] * 0.6 + np.array([0.1, 0.4, 1.0]) * 0.4
        d = np.hypot(g.X[win] - s["x"], g.Y[win] - s["y"])
        ring = (np.abs(d - 8000.0) < 40.0) | (np.abs(d - 12500.0) < 25.0)
        rgb[ring] = (0.2, 1.0, 0.3)
        tiles.append((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1])
    height = max(t.shape[0] for t in tiles)
    width = max(t.shape[1] for t in tiles)
    sheet = np.zeros((height * 2, width * 3, 3), dtype=np.uint8)
    for k, t in enumerate(tiles):
        r, col = k // 3, k % 3
        sheet[r * height:r * height + t.shape[0], col * width:col * width + t.shape[1]] = t
    Image.fromarray(sheet).resize((width * 3 * 2 // 3, height * 2 * 2 // 3)).save(path)


def oblique(g, s, path, yaw_deg=0.0, half=30000.0, width=1100, pitch_deg=38.0, exaggeration=1.6):
    """Cheap painter's-algorithm view from the map centre side, to judge silhouettes before going into the editor."""
    from PIL import Image
    yaw = math.radians(yaw_deg) + math.atan2(s["inward"][1], s["inward"][0])
    fx, fy = -math.cos(yaw), -math.sin(yaw)  # view forward (from the centre side, looking at the start)
    rx, ry = -fy, fx
    n = 900
    u = np.linspace(-half, half, width)
    v = np.linspace(half * 1.25, -half * 1.0, n)  # far to near
    pitch = math.radians(pitch_deg)
    height_px = 760
    scale = width / (2.0 * half)
    img = np.zeros((height_px, width, 3))
    img[:] = (0.8, 0.72, 0.6)
    gyy, gxx = np.gradient(g.Z, 100.0)
    lum_all = shade(g.Z, 100.0)
    for vv in v:
        wx = s["x"] + fx * vv + rx * u
        wy = s["y"] + fy * vv + ry * u
        i = np.clip(((wx + g.half) / 100.0).astype(int), 0, g.verts - 1)
        j = np.clip(((wy + g.half) / 100.0).astype(int), 0, g.verts - 1)
        z = (g.Z[j, i] - 600.0) * exaggeration
        lum = lum_all[j, i]
        yy = height_px * 0.62 - (vv * math.sin(pitch) + z * math.cos(pitch)) * scale
        y0 = np.clip(yy.astype(int), 0, height_px - 1)
        col = np.stack([lum * 0.95, lum * 0.72, lum * 0.52], -1)
        for x in range(width):
            img[y0[x]:min(y0[x] + 12, height_px), x] = col[x]
    Image.fromarray((img * 255).astype(np.uint8)).save(path)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    name = sys.argv[2] if len(sys.argv) > 2 else "SM_Desert_01"
    fast = "--fast" in sys.argv  # skips the planner audit: for iterating on shapes
    g = Generator(name)
    g.build_height()
    g.build_weights()
    g.keep_oasis("weights")
    g.reachability(max_slope=30.0)
    for line in g.report():
        print(line)
    if not fast:
        for line in g.simulate_planner():
            print(line)
    for line in g.scarp_leaks():
        print(line)
    for line in g.coverage():
        print(line)
    lay = g.write(out_dir, "--power-only" in sys.argv)
    preview_starts(g, os.path.join(out_dir, "%s_starts.png" % name))
    if "--oblique" in sys.argv:
        for s in g.starts:
            oblique(g, s, os.path.join(out_dir, "%s_oblique_%d.png" % (name, s["index"])))
    bad = [("start", s["index"]) for s in lay["starts"] if not s["reachable"]]
    bad += [("defense", s["index"], d["kind"], d["name"]) for s in lay["starts"] for d in s["defense_points"] if
            not d["reachable"]]
    bad += [("zone", z["x"], z["y"]) for z in lay["edge_zones"] if not z["reachable"]]
    print("unreachable (slope < 30 deg flood fill from start 0):", bad if bad else "none")
    for s in lay["starts"]:
        print(s["index"], s["name"], [("%s %s r%.0f z%.0f" % (d["kind"], d["name"], d["distance"], d["z"])) for d in s["defense_points"]])
    print("wrote", out_dir)


if __name__ == "__main__":
    main()
