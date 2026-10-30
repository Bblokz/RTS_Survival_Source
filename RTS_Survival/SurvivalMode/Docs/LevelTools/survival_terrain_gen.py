"""Offline terrain generator for survival maps (run with a local Python 3 + numpy, NOT inside the editor).

Writes the raw files that `RTSShortsLandscapeLibrary.create_painted_landscape` imports (see survival_map_build.py):
  <out>/<map>_height.raw          uint16 LE, 32768 + cm * 128 / 100 (landscape Z scale 100), row by row along X
  <out>/<map>_<Layer>.raw         uint8 per vertex, all layers sum to 255
  <out>/<map>_layout.json         start locations, defense points, edge zones, dressing spots (world cm, Z on ground)
  <out>/<map>_preview.png         shaded height + layer preview (needs Pillow)

Layout: N start areas on a regular polygon around the map centre, each an identical stamp (rotated so its local
+X points at the map centre) -> every start area is equally good. Per start area:
  flat build disc (exactly one height) -> resource ring (near-flat) with 4 rocky hills, each with one ramp down
  towards the base -> wreck ring (gentle) -> open desert with low dunes, craters, a central mesa with one ramp per
  base and small rock buttes between neighbouring bases. A band along the map edge stays free of obstacles so waves
  can enter anywhere.

Usage: python survival_terrain_gen.py <out_dir> [config_name]
"""
import json
import math
import os
import sys
from collections import deque

import numpy as np

# ----------------------------------------------------------------------------------------------------------------------
# Config. Copy this dict (new name + seed + tweaks) to make the next survival map.
# ----------------------------------------------------------------------------------------------------------------------
CONFIGS = {
    "SM_Desert_01": dict(
        components=24, quads=63, quad_cm=100.0,  # 24 * 63 = 1512 quads -> 151 200 cm square, 1513^2 vertices
        base_z=600.0,                            # world Z of the flat build discs (landscape actor at Z = 0)
        seed=1729,
        start_count=5, start_radius=42000.0, start_first_angle_deg=90.0,
        build_radius=8000.0,                     # exactly flat
        flat_until=8600.0,                       # still exactly flat (margin for the ramp feet)
        resource_ring=(8000.0, 14000.0),
        wreck_ring=(15000.0, 21000.0),
        dunes_full_at=25000.0,                   # distance from a start centre where dunes reach full amplitude
        # Hills of each start area, in the start's local frame (angle 0 = towards the map centre).
        hills=dict(angles_deg=[45.0, 135.0, 225.0, 315.0], radius=13000.0, height=750.0,
                   plateau_radius=1300.0, plateau_half_length=1700.0, flank=520.0,
                   ramp_length=3000.0, ramp_half_width=650.0, ramp_side_flank=420.0),
        mesa=dict(height=900.0, plateau_radius=3400.0, flank=650.0, ramp_length=3800.0, ramp_half_width=700.0,
                  ramp_side_flank=450.0),
        buttes=dict(radius_from_centre=31000.0, height=650.0, plateau_radius=1500.0, plateau_half_length=900.0,
                    flank=550.0),
        outer_mounds=dict(radius_from_centre=53000.0, angle_offsets_deg=[-9.0, 9.0], height=420.0,
                          plateau_radius=700.0, plateau_half_length=500.0, flank=700.0),
        dunes=[(9000.0, 230.0), (2600.0, 55.0), (750.0, 10.0)],  # (wavelength cm, amplitude cm)
        edge_band=9000.0,                        # no buttes/craters closer than this to the landscape edge
        edge_dune_scale=0.55,
        craters_random=34, crater_radius=(500.0, 1400.0), crater_depth=(110.0, 260.0),
        wreck_craters=[(25.0, 17500.0, 700.0), (160.0, 18500.0, 900.0), (280.0, 16800.0, 600.0)],
        edge_zones=[
            # centre x, centre y, length along edge (cm); inward direction is derived from the nearest edge
            (-42000.0, 72000.0, 16000.0), (42000.0, 72000.0, 16000.0),
            (72000.0, 48000.0, 16000.0), (72000.0, -22000.0, 16000.0),
            (52000.0, -72000.0, 16000.0), (0.0, -72000.0, 16000.0), (-52000.0, -72000.0, 16000.0),
            (-72000.0, -22000.0, 16000.0), (-72000.0, 48000.0, 16000.0),
        ],
        edge_zone_inset=2500.0,
        layers=["Sand", "DesertGravel", "DesertRockDark", "ScorchConcrete", "ScorchSandyStone", "Weedy"],
    ),
}


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, salt):
    """Smooth value noise in [-1, 1] on an integer lattice (vectorised)."""
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    fx = x - x0
    fy = y - y0
    sx = fx * fx * (3 - 2 * fx)
    sy = fy * fy * (3 - 2 * fy)

    def corner(cx, cy):
        h = (cx * 73856093) ^ (cy * 19349663) ^ (salt * 83492791)
        h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
        return (h & 0xFFFF) / 32767.5 - 1.0

    c00 = corner(x0, y0)
    c10 = corner(x0 + 1, y0)
    c01 = corner(x0, y0 + 1)
    c11 = corner(x0 + 1, y0 + 1)
    top = c00 + (c10 - c00) * sx
    bottom = c01 + (c11 - c01) * sx
    return top + (bottom - top) * sy


def capsule_distance(px, py, cx, cy, dir_x, dir_y, half_length):
    """Distance from points to a segment centred on (cx, cy) along unit (dir_x, dir_y)."""
    rx = px - cx
    ry = py - cy
    t = np.clip(rx * dir_x + ry * dir_y, -half_length, half_length)
    qx = rx - t * dir_x
    qy = ry - t * dir_y
    return np.sqrt(qx * qx + qy * qy)


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
        self.features = dict(hills=[], ramps=[], craters=[], mesa=None, buttes=[], mounds=[])

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
        d_start = self.dist_to_nearest_start(X, Y)
        dune_mask = smoothstep(c["wreck_ring"][0] - 1000.0, c["dunes_full_at"], d_start)
        edge = self.edge_distance(X, Y)
        dune_mask *= c["edge_dune_scale"] + (1.0 - c["edge_dune_scale"]) * smoothstep(0.0, c["edge_band"], edge)
        # resource ring gets only a whisper of relief (<= ~25 cm) so resource fields sit flat
        ring_mask = smoothstep(c["flat_until"], c["resource_ring"][1] + 500.0, d_start) * 0.12
        relief = dunes * np.maximum(dune_mask, ring_mask * (d_start > c["flat_until"]))

        raised = np.zeros_like(X)  # max-combined positive features (hills, ramps, mesa, buttes)
        self.rock_mask = np.zeros_like(X)
        self.ramp_mask = np.zeros_like(X)
        self.plateau_mask = np.zeros_like(X)
        h = c["hills"]
        for s in self.starts:
            for a in h["angles_deg"]:
                ar = math.radians(a)
                lx, ly = h["radius"] * math.cos(ar), h["radius"] * math.sin(ar)
                cx, cy = self.local_to_world(s, lx, ly)
                # tangential direction of the capsule
                rdx, rdy = (cx - s["x"]) / h["radius"], (cy - s["y"]) / h["radius"]
                tdx, tdy = -rdy, rdx
                hill, dist = self.plateau(cx, cy, tdx, tdy, h["plateau_half_length"], h["plateau_radius"], h["flank"],
                                          h["height"])
                raised = np.maximum(raised, hill)
                self.plateau_mask = np.maximum(self.plateau_mask, (dist < h["plateau_radius"] - 150).astype(float))
                self.rock_mask = np.maximum(self.rock_mask, 1.0 - smoothstep(h["plateau_radius"] - 300,
                                                                             h["plateau_radius"] + h["flank"] + 250, dist))
                # ramp: starts at the plateau edge facing the start centre, runs straight at the start centre
                top_x = cx - rdx * h["plateau_radius"]
                top_y = cy - rdy * h["plateau_radius"]
                rmp, t, w = self.ramp(top_x, top_y, -rdx, -rdy, h["ramp_length"], h["ramp_half_width"],
                                      h["ramp_side_flank"], h["height"])
                raised = np.maximum(raised, rmp)
                self.ramp_mask = np.maximum(self.ramp_mask, ((t > -100) & (t < h["ramp_length"]) &
                                                             (w < h["ramp_half_width"] - 200.0)).astype(float))
                foot = (top_x - rdx * h["ramp_length"], top_y - rdy * h["ramp_length"])
                self.features["hills"].append(dict(start=s["index"], local_angle_deg=a, x=cx, y=cy,
                                                   ramp_head=(top_x, top_y), ramp_foot=foot))
        # central mesa with one ramp per start, pointing at it
        m = c["mesa"]
        mesa, dist = self.plateau(0.0, 0.0, 1.0, 0.0, 0.0, m["plateau_radius"], m["flank"], m["height"])
        raised = np.maximum(raised, mesa)
        self.plateau_mask = np.maximum(self.plateau_mask, (dist < m["plateau_radius"] - 200).astype(float))
        self.rock_mask = np.maximum(self.rock_mask, 1.0 - smoothstep(m["plateau_radius"] - 400,
                                                                     m["plateau_radius"] + m["flank"] + 300, dist))
        mesa_ramps = []
        for s in self.starts:
            dx, dy = -s["inward"][0], -s["inward"][1]  # from centre towards the start
            top_x, top_y = dx * m["plateau_radius"], dy * m["plateau_radius"]
            rmp, t, w = self.ramp(top_x, top_y, dx, dy, m["ramp_length"], m["ramp_half_width"], m["ramp_side_flank"],
                                  m["height"])
            raised = np.maximum(raised, rmp)
            self.ramp_mask = np.maximum(self.ramp_mask, ((t > -100) & (t < m["ramp_length"]) &
                                                         (w < m["ramp_half_width"] - 200.0)).astype(float))
            mesa_ramps.append(dict(head=(top_x, top_y), foot=(top_x + dx * m["ramp_length"],
                                                             top_y + dy * m["ramp_length"])))
        self.features["mesa"] = dict(x=0.0, y=0.0, ramps=mesa_ramps)
        # buttes between neighbouring starts (pure cover/obstacles, no ramp) and small outer mounds
        b = c["buttes"]
        o = c["outer_mounds"]
        for k, s in enumerate(self.starts):
            mid = math.radians(s["angle_deg"] + 180.0 / c["start_count"])
            bx, by = b["radius_from_centre"] * math.cos(mid), b["radius_from_centre"] * math.sin(mid)
            butte, dist = self.plateau(bx, by, -math.sin(mid), math.cos(mid), b["plateau_half_length"],
                                       b["plateau_radius"], b["flank"], b["height"])
            raised = np.maximum(raised, butte)
            self.rock_mask = np.maximum(self.rock_mask, 1.0 - smoothstep(b["plateau_radius"] - 500,
                                                                         b["plateau_radius"] + b["flank"] + 300, dist))
            self.features["buttes"].append(dict(x=bx, y=by))
            for off in o["angle_offsets_deg"]:
                ang = mid + math.radians(off)
                mx, my = o["radius_from_centre"] * math.cos(ang), o["radius_from_centre"] * math.sin(ang)
                if min(self.half - abs(mx), self.half - abs(my)) < c["edge_band"] + 1500:
                    continue
                mound, dist = self.plateau(mx, my, math.cos(ang), math.sin(ang), o["plateau_half_length"],
                                           o["plateau_radius"], o["flank"], o["height"])
                raised = np.maximum(raised, mound)
                self.rock_mask = np.maximum(self.rock_mask, 0.7 * (1.0 - smoothstep(o["plateau_radius"] - 300,
                                                                                    o["plateau_radius"] + o["flank"],
                                                                                    dist)))
                self.features["mounds"].append(dict(x=mx, y=my))

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
            if float(self.dist_to_nearest_start(np.array(cx), np.array(cy))) < c["wreck_ring"][1] + 2000 + rad:
                continue
            if math.hypot(cx, cy) < m["plateau_radius"] + m["ramp_length"] + 2000 + rad:
                continue
            if any(math.hypot(cx - f["x"], cy - f["y"]) < 4500 + rad for f in self.features["buttes"] +
                   self.features["mounds"]):
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

        # features (hills, ramps, mesa, buttes) sit on clean ground: fade the dunes out under them
        relief = relief * (1.0 - smoothstep(0.0, 150.0, raised))
        Z = c["base_z"] + relief + raised + crater_field
        # build discs: exactly flat (zero gradient), soft blend outside flat_until
        for s in self.starts:
            d = np.hypot(X - s["x"], Y - s["y"])
            blend = smoothstep(c["flat_until"], c["flat_until"] + 1800.0, d)
            # raised features never reach inside flat_until (ramp feet stop at ~8.9 km), keep them intact
            Z = np.where(d < c["flat_until"], c["base_z"], c["base_z"] + (Z - c["base_z"]) *
                         np.maximum(blend, (raised > 1.0).astype(float)))
        self.Z = Z
        self.raised = raised
        return Z

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
        d_start = self.dist_to_nearest_start(X, Y)
        build = 1.0 - smoothstep(c["build_radius"] - 600.0, c["build_radius"] + 1200.0, d_start)
        rock = np.maximum(smoothstep(17.0, 30.0, slope), self.rock_mask * 0.55 * smoothstep(0.35, 0.75, n1))
        rock = np.clip(rock, 0.0, 1.0)
        gravel = (0.45 * self.ramp_mask + 0.55 * self.plateau_mask * (0.4 + 0.6 * n2)
                  + 0.30 * build * smoothstep(0.45, 0.8, n2)
                  + 0.42 * (1.0 - build) * smoothstep(0.62, 0.85, n1))
        weedy = 0.62 * build * (0.55 + 0.45 * n1) + 0.30 * (1.0 - build) * smoothstep(0.66, 0.9, n2)
        scorch = 0.85 * self.crater_mask
        concrete = np.zeros_like(X)
        others = {"DesertRockDark": rock, "DesertGravel": gravel, "Weedy": weedy, "ScorchSandyStone": scorch,
                  "ScorchConcrete": concrete}
        # priority: rock > scorch > the rest; scale so the sum stays <= 1, sand takes the remainder
        remaining = np.ones_like(X)
        out = {}
        for name in ["DesertRockDark", "ScorchSandyStone", "DesertGravel", "Weedy", "ScorchConcrete"]:
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
        for s in self.starts:
            d = np.hypot(self.X - s["x"], self.Y - s["y"])
            disc = d <= c["build_radius"]
            out.append("start %d (%.0f, %.0f): disc z range %.3f..%.3f, max slope in disc %.3f deg, "
                       "max slope in resource ring (excl. hills) %.1f deg" % (
                           s["index"], s["x"], s["y"], self.Z[disc].min(), self.Z[disc].max(), slope[disc].max(),
                           slope[(d > c["build_radius"]) & (d < c["resource_ring"][1]) & (self.rock_mask < 0.05)
                                 & (self.raised < 1.0)].max()))
        rm = self.ramp_mask > 0.5
        core = rm & (self.plateau_mask < 0.5)
        out.append("ramp cells: max slope %.1f deg, p99 %.1f deg" % (slope[core].max(), np.percentile(slope[core], 99)))
        out.append("open desert (no feature): p99 slope %.1f deg, max %.1f" % (
            np.percentile(slope[(self.raised < 1.0) & (self.crater_mask < 0.05)], 99),
            slope[(self.raised < 1.0) & (self.crater_mask < 0.05)].max()))
        out.append("Z range %.0f .. %.0f" % (self.Z.min(), self.Z.max()))
        return out

    # ---- layout ----------------------------------------------------------------------------------------------------
    def layout(self):
        c = self.c
        starts = []
        for s in self.starts:
            hills = [h for h in self.features["hills"] if h["start"] == s["index"]]
            defense = []
            for h in hills:
                defense.append(dict(kind="hilltop", x=h["x"], y=h["y"], z=self.ground_z(h["x"], h["y"]),
                                    local_angle_deg=h["local_angle_deg"], reachable=self.reachable(h["x"], h["y"])))
            for h in hills:
                if h["local_angle_deg"] in (45.0, 315.0):  # the two hills facing the map centre
                    hx, hy = h["ramp_head"]
                    defense.append(dict(kind="ramp_head", x=hx, y=hy, z=self.ground_z(hx, hy),
                                        local_angle_deg=h["local_angle_deg"], reachable=self.reachable(hx, hy)))
            resource = []
            for a in (0.0, 90.0, 180.0, 270.0):
                ar = math.radians(a)
                rx, ry = self.local_to_world(s, 11000.0 * math.cos(ar), 11000.0 * math.sin(ar))
                resource.append(dict(x=rx, y=ry, z=self.ground_z(rx, ry)))
            starts.append(dict(index=s["index"], x=s["x"], y=s["y"], z=self.ground_z(s["x"], s["y"]),
                               yaw_to_map_centre_deg=math.degrees(math.atan2(s["inward"][1], s["inward"][0])),
                               build_radius=c["build_radius"], resource_ring=c["resource_ring"],
                               wreck_ring=c["wreck_ring"], defense_points=defense, resource_gap_centres=resource,
                               edge_clearance=float(min(self.half - abs(s["x"]), self.half - abs(s["y"]))),
                               reachable=self.reachable(s["x"], s["y"]),
                               ramp_feet=[h["ramp_foot"] for h in hills]))
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
            starts=starts, edge_zones=zones, features=self.features)

    def write(self, out_dir):
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


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    name = sys.argv[2] if len(sys.argv) > 2 else "SM_Desert_01"
    g = Generator(name)
    g.build_height()
    g.build_weights()
    g.reachability()
    for line in g.report():
        print(line)
    lay = g.write(out_dir)
    bad = [("start", s["index"]) for s in lay["starts"] if not s["reachable"]]
    bad += [("defense", s["index"], d["kind"], d["local_angle_deg"]) for s in lay["starts"] for d in
            s["defense_points"] if not d["reachable"]]
    bad += [("zone", z["x"], z["y"]) for z in lay["edge_zones"] if not z["reachable"]]
    print("unreachable (slope < 25 deg flood fill from start 0):", bad if bad else "none")
    print("wrote", out_dir)


if __name__ == "__main__":
    main()
