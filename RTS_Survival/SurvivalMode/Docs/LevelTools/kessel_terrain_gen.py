"""Terrain and layout generator for SM_Kessel_01 (plain python + numpy, no editor).

    python kessel_terrain_gen.py <out_dir>

Writes the raw files RTSShortsLandscapeLibrary.create_painted_landscape imports, a layout json the editor scripts read
(kessel_map_build.py) and a preview. Design: 11_LevelDesign_SM_Kessel_01.md.
    <out>/SM_Kessel_01_height.raw      uint16 LE, 32768 + cm * 128 / 100, row by row along X (Z[iy, ix])
    <out>/SM_Kessel_01_<Layer>.raw     uint8 per vertex, all layers sum to 255
"""
import json
import math
import os
import sys

import numpy as np

SEED = 71
COMPONENTS = 12
QUADS = 63
QUAD_CM = 100.0
# Ger_MarderStugMission is a 51 000 cm square; 20 % more area is a side of 51 000 x sqrt(1.2) = 55 868, rounded to 55 800.
PLAYABLE_HALF = 27900.0
LANDSCAPE_HALF = COMPONENTS * QUADS * QUAD_CM / 2.0
LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop"]

CORE_Z = 1120.0
CORE_FLAT_R = 8500.0
BUILD_RADIUS = 8000.0
TERRACE_Z = 640.0
VALLEY_Z = 330.0
GATE_BEARINGS = [15.0, 75.0, 135.0, 195.0, 255.0, 315.0]
GATE_HALF_DEG = 8.5
RAMP_END_R = 12800.0
# Back from the crest: enemy infantry take cover on the far side of a wall, and that side must be on the navmesh.
SANDBAG_RADIUS = 8200.0
# Narrow enough that the wall stays over the Character agent's 44 degree limit at the lowest bluff, or infantry climb it.
BLUFF_WIDTH = 300.0

# Flat shelves on the terrace: expansion ground for buildings, a field beside it.
SHELVES = [(41.0, 15200.0, 3300.0), (105.0, 14800.0, 2900.0), (165.0, 15400.0, 3200.0),
           (225.0, 14900.0, 3000.0), (285.0, 15300.0, 3200.0), (345.0, 14700.0, 2800.0)]
# (x, y, radius, height, flat top share)
HILLS = [(19800.0, 12500.0, 3900.0, 620.0, 0.34), (-20500.0, 12800.0, 3400.0, 520.0, 0.30),
         (-17500.0, -18800.0, 4200.0, 700.0, 0.36), (13000.0, -20000.0, 3300.0, 480.0, 0.30),
         (2500.0, 20000.0, 3000.0, 430.0, 0.30)]
# (x0, y0, x1, y1, half width, height, passes as t along the ridge)
# Kept 5 000 cm or more inside the play edge so the edge spawn zones stand on open ground.
RIDGES = [(-21800.0, -7500.0, -20800.0, 6500.0, 2300.0, 560.0, [0.5]),
          (21500.0, -8000.0, 19500.0, -17000.0, 2100.0, 520.0, [0.45]),
          (-8500.0, 22200.0, -18500.0, 21500.0, 2000.0, 470.0, [0.55]),
          (-4000.0, -22300.0, 6500.0, -22800.0, 1900.0, 420.0, [0.5])]


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def gauss(field, sigma_cells):
    """Gaussian blur through the FFT (periodic; the landscape edge is hidden past the rim)."""
    n = field.shape[0]
    freq = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(freq, freq)
    kernel = np.exp(-2.0 * (math.pi * sigma_cells) ** 2 * (fx * fx + fy * fy))
    return np.real(np.fft.ifft2(np.fft.fft2(field) * kernel))


def noise(rng, n, sigma_cells):
    field = gauss(rng.standard_normal((n, n)), sigma_cells)
    return field / max(1e-6, np.abs(field).max())


def fbm(rng, n, sigmas):
    out = np.zeros((n, n))
    amp = 1.0
    for sigma in sigmas:
        out += amp * noise(rng, n, sigma)
        amp *= 0.5
    return out / np.abs(out).max()


def angle_delta(a, b):
    return np.abs((a - b + 180.0) % 360.0 - 180.0)


def segment_distance(px, py, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    t = np.clip(((px - x0) * dx + (py - y0) * dy) / (dx * dx + dy * dy), 0.0, 1.0)
    return np.hypot(px - (x0 + t * dx), py - (y0 + t * dy)), t


class Kessel:
    def __init__(self):
        self.rng = np.random.default_rng(SEED)
        self.verts = COMPONENTS * QUADS + 1
        axis = (np.arange(self.verts) - (self.verts - 1) / 2.0) * QUAD_CM
        self.X, self.Y = np.meshgrid(axis, axis)  # Z[iy, ix]
        self.R = np.hypot(self.X, self.Y)
        self.bearing = np.degrees(np.arctan2(self.Y, self.X)) % 360.0
        self.edge = np.maximum(np.abs(self.X), np.abs(self.Y))  # square distance from the centre

    def gate_mask(self):
        mask = np.zeros_like(self.R)
        for bearing in GATE_BEARINGS:
            # A constant width road, not a wedge: the half angle is what it is at the plateau edge.
            half_width = math.radians(GATE_HALF_DEG) * CORE_FLAT_R
            radial_x, radial_y = math.cos(math.radians(bearing)), math.sin(math.radians(bearing))
            along = self.X * radial_x + self.Y * radial_y
            across = np.abs(-self.X * radial_y + self.Y * radial_x)
            wide = half_width * (1.0 + 0.9 * smooth(CORE_FLAT_R, RAMP_END_R, along))
            one = (1.0 - smooth(wide * 0.62, wide * 1.55, across)) * (along > 0.0)
            mask = np.maximum(mask, one)
        return mask

    def ground(self):
        rng = self.rng
        n = self.verts
        roll = fbm(rng, n, [34.0, 15.0, 6.0])
        warp = fbm(rng, n, [60.0, 25.0])
        detail = fbm(rng, n, [5.0, 2.2])
        self.grass_noise = fbm(rng, n, [20.0, 8.0, 3.5, 1.6])
        self.dirt_noise = fbm(rng, n, [13.0, 5.0, 2.2, 1.2])
        self.rough_noise = detail
        r = self.R * (1.0 + 0.07 * warp)
        # Valley floor, rising to the terrace round the plateau.
        terrace = VALLEY_Z + (TERRACE_Z - VALLEY_Z) * (1.0 - smooth(15500.0, 24500.0, r))
        terrace += 95.0 * roll * smooth(9000.0, 13000.0, self.R)
        terrace += 10.0 * detail
        # The dry wash winds round the terrace foot.
        bearing_rad = np.radians(self.bearing)
        wash_r = (24300.0 + 2300.0 * np.sin(3.0 * bearing_rad + 1.0) + 1300.0 * np.sin(5.0 * bearing_rad + 2.3)
                  + 2000.0 * warp + 1100.0 * fbm(rng, n, [14.0, 6.0]))
        # Two broken arms, not a ring round the map.
        wash_arms = smooth(-0.35, 0.05, np.sin(2.0 * bearing_rad + 0.7) + 0.35 * warp)
        self.wash = np.exp(-((self.R - wash_r) / 900.0) ** 2) * wash_arms * (self.edge < PLAYABLE_HALF - 900.0)
        terrace -= 120.0 * self.wash

        for x0, y0, x1, y1, half_w, height, passes in RIDGES:
            dist, t = segment_distance(self.X, self.Y, x0, y0, x1, y1)
            bump = (1.0 - smooth(half_w * 0.2, half_w, dist * (1.0 + 0.18 * warp)))
            taper = smooth(0.0, 0.2, t) * smooth(0.0, 0.2, 1.0 - t)
            for centre in passes:
                taper = taper * smooth(0.05, 0.13, np.abs(t - centre))
            terrace += height * bump * (0.55 + 0.45 * taper) * (0.25 + 0.75 * taper) * (1.0 + 0.16 * roll)
        self.hill_tops = []
        for x, y, radius, height, top in HILLS:
            dist = np.hypot(self.X - x, self.Y - y) * (1.0 + 0.16 * warp)
            terrace += height * (1.0 - smooth(radius * top, radius, dist))
            self.hill_tops.append((x, y, radius * top))

        # Shelves are exactly level: flat ground for buildings on the middle tier.
        self.shelf_mask = np.zeros_like(terrace)
        self.shelves = []
        for bearing, radius_at, disc in SHELVES:
            x, y = radius_at * math.cos(math.radians(bearing)), radius_at * math.sin(math.radians(bearing))
            ix = int(round(x / QUAD_CM + (n - 1) / 2.0))
            iy = int(round(y / QUAD_CM + (n - 1) / 2.0))
            level = float(np.round(terrace[iy, ix] / 10.0) * 10.0) + 30.0
            dist = np.hypot(self.X - x, self.Y - y) * (1.0 + 0.1 * warp)
            blend = 1.0 - smooth(disc, disc + 1500.0, dist)
            terrace = terrace * (1.0 - blend) + level * blend
            self.shelf_mask = np.maximum(self.shelf_mask, 1.0 - smooth(disc - 100.0, disc + 60.0, dist))
            self.shelves.append(dict(x=x, y=y, z=level, radius=disc, bearing=bearing))

        # Plateau: bluffs between the gates, long ramps through them.
        gates = self.gate_mask()
        self.gates = gates
        bluff_r = CORE_FLAT_R + 260.0 * (0.5 + 0.5 * roll)
        bluff = smooth(bluff_r, bluff_r + BLUFF_WIDTH, self.R)
        ramp = smooth(CORE_FLAT_R, RAMP_END_R, self.R)
        fall = bluff * (1.0 - gates) + ramp * gates
        z = CORE_Z * (1.0 - fall) + terrace * fall
        z = np.where(self.R <= CORE_FLAT_R, CORE_Z, z)

        # The rim outside the playable square: hills, then a band that hides the end of the landscape.
        out = np.maximum(self.edge - PLAYABLE_HALF - 400.0, 0.0)
        rim = smooth(0.0, 5200.0, out) * (760.0 + 620.0 * fbm(rng, n, [30.0, 12.0]))
        z += np.maximum(rim, 0.0)
        z += 60.0 * detail * smooth(0.0, 2000.0, out)
        self.Z = z
        self.slope_map()

    def slope_map(self):
        gy, gx = np.gradient(self.Z, QUAD_CM)
        self.slope = np.degrees(np.arctan(np.hypot(gx, gy)))

    def routes(self):
        """Tracks from the map edge up each gate, plus the terrace lane that links the shelves."""
        self.route_lines = []
        edge = PLAYABLE_HALF
        ends = {15.0: (edge, 7000.0), 75.0: (9500.0, edge), 135.0: (-18000.0, edge),
                195.0: (-edge, -9000.0), 255.0: (-8000.0, -edge), 315.0: (19000.0, -edge)}
        for bearing, end in ends.items():
            a = math.radians(bearing)
            top = (CORE_FLAT_R - 900.0, a)
            foot = RAMP_END_R + 300.0
            points = [(top[0] * math.cos(a), top[0] * math.sin(a)), (foot * math.cos(a), foot * math.sin(a))]
            mid = ((foot * math.cos(a) * 0.45 + end[0] * 0.55) + 900.0 * math.sin(a * 3.0),
                   (foot * math.sin(a) * 0.45 + end[1] * 0.55) - 900.0 * math.cos(a * 2.0))
            points += [mid, end]
            self.route_lines.append(dict(name="gate_%d" % int(bearing), points=points))
        lane = []
        for k in range(49):
            a = math.radians(k * 7.5 + 3.0)
            r = 13900.0 + 700.0 * math.sin(a * 3.0 + 1.0)
            lane.append((r * math.cos(a), r * math.sin(a)))
        self.route_lines.append(dict(name="terrace_lane", points=lane))
        self.route_distance = np.full_like(self.R, 1e9)
        self.line_distance = {}
        for line in self.route_lines:
            dist = np.full_like(self.R, 1e9)
            pts = line["points"]
            for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
                d, _ = segment_distance(self.X, self.Y, ax, ay, bx, by)
                dist = np.minimum(dist, d)
            self.line_distance[line["name"]] = dist
            self.route_distance = np.minimum(self.route_distance, dist)

    def paint(self):
        # What the layers look like (one wedge per layer, 2026-11-03): GrassCastle green, Desert cream, GraySand yellow sand,
        # RTSSand salmon, RockDesert grey, MountainTop cream.
        grass = 0.72 + 0.6 * self.grass_noise
        desert = 0.14 + 0.62 * np.clip(self.dirt_noise, 0.0, 1.0) + 0.4 * np.clip(-self.grass_noise, 0.0, 1.0)
        rock_g = smooth(15.0, 25.0, self.slope)
        mountain = smooth(40.0, 52.0, self.slope) * 0.0
        sand = 0.3 * smooth(0.25, 0.75, self.wash) + 0.12 * np.clip(self.rough_noise - 0.35, 0.0, 1.0)
        gray = 0.1 + 0.42 * np.clip(self.rough_noise, 0.0, 1.0) + 0.8 * smooth(0.2, 0.7, self.wash)
        # The tracks: worn to soil down the middle, ragged at the edge.
        track_edge = 300.0 + 170.0 * self.rough_noise
        track = 1.0 - smooth(track_edge * 0.55, track_edge * 1.35, self.route_distance)
        track = track * (self.edge < PLAYABLE_HALF + 300.0)
        desert = desert * (1.0 - track) + 1.25 * track
        grass = grass * (1.0 - 0.92 * track)
        gray = gray + 0.28 * track
        # The plateau is a trampled position: mostly packed soil with grass left in patches.
        core = 1.0 - smooth(CORE_FLAT_R - 300.0, CORE_FLAT_R + 200.0, self.R)
        desert = desert * (1.0 - core) + core * (0.62 + 0.35 * np.clip(self.dirt_noise + 0.3, 0.0, 1.0))
        grass = grass * (1.0 - 0.55 * core)
        gray = gray + 0.2 * core * np.clip(self.grass_noise + 0.4, 0.0, 1.0)
        shelf = self.shelf_mask
        grass = grass * (1.0 - 0.35 * shelf)
        gray = gray + 0.18 * shelf
        # The rim stays greener and rocky, past play.
        out = smooth(PLAYABLE_HALF, PLAYABLE_HALF + 2500.0, self.edge)
        grass = grass + 0.45 * out * (0.5 + 0.5 * self.grass_noise)

        rock_keep = 1.0 - np.clip(rock_g + mountain, 0.0, 1.0)
        raw = {
            "GrassCastle": np.maximum(grass, 0.0) * rock_keep,
            "Desert": np.maximum(desert, 0.0) * rock_keep,
            "GraySand": np.maximum(gray, 0.0) * rock_keep,
            "RTSSand": np.maximum(sand, 0.0) * rock_keep,
            "RockDesert": rock_g * (1.0 - 0.6 * mountain),
            "MountainTop": mountain,
        }
        # Sharpen a little so the layers read as patches rather than a soup, then quantise to sum 255.
        stack = np.stack([raw[name] ** 1.25 for name in LAYERS], axis=-1)
        stack /= np.maximum(stack.sum(axis=-1, keepdims=True), 1e-5)
        quantised = np.floor(stack * 255.0).astype(np.int32)
        rest = 255 - quantised.sum(axis=-1)
        biggest = stack.argmax(axis=-1)
        rows, cols = np.indices(rest.shape)
        quantised[rows, cols, biggest] += rest
        self.weights = {name: quantised[..., k].astype(np.uint8) for k, name in enumerate(LAYERS)}

    # ------------------------------------------------------------------------------------------------- flatness
    def relief_maps(self):
        """Height range inside growing discs (2 m grid), the proof a footprint sits on level enough ground."""
        step = 2
        small = self.Z[::step, ::step]
        hi, lo = small.copy(), small.copy()
        self.relief = {}
        radius = 0
        def grow(a, op):
            b = a.copy()
            for shift_axis in (0, 1):
                for sign in (-1, 1):
                    b = op(b, np.roll(a, sign, axis=shift_axis))
            return op(b, op(np.roll(np.roll(a, 1, 0), 1, 1), np.roll(np.roll(a, -1, 0), -1, 1)))
        for wanted in (300, 500, 700, 900, 1100, 1300, 1500, 1800, 2200, 2500):
            while radius * step * QUAD_CM < wanted:
                hi = grow(hi, np.maximum)
                lo = grow(lo, np.minimum)
                radius += 1
            self.relief[wanted] = hi - lo

    def relief_at(self, x, y, radius):
        key = min(k for k in self.relief if k >= radius)
        i = int(round(y / QUAD_CM + (self.verts - 1) / 2.0)) // 2
        j = int(round(x / QUAD_CM + (self.verts - 1) / 2.0)) // 2
        return float(self.relief[key][i, j])

    def z_at(self, x, y):
        fx = x / QUAD_CM + (self.verts - 1) / 2.0
        fy = y / QUAD_CM + (self.verts - 1) / 2.0
        i0, j0 = int(math.floor(fy)), int(math.floor(fx))
        ty, tx = fy - i0, fx - j0
        z = self.Z
        return float((z[i0, j0] * (1 - tx) + z[i0, j0 + 1] * tx) * (1 - ty) + (z[i0 + 1, j0] * (1 - tx) + z[i0 + 1, j0 + 1] * tx) * ty)

    def slope_at(self, x, y):
        i = int(round(y / QUAD_CM + (self.verts - 1) / 2.0))
        j = int(round(x / QUAD_CM + (self.verts - 1) / 2.0))
        return float(self.slope[i, j])

    def route_at(self, x, y):
        i = int(round(y / QUAD_CM + (self.verts - 1) / 2.0))
        j = int(round(x / QUAD_CM + (self.verts - 1) / 2.0))
        return float(self.route_distance[i, j])

    def gate_at(self, x, y):
        i = int(round(y / QUAD_CM + (self.verts - 1) / 2.0))
        j = int(round(x / QUAD_CM + (self.verts - 1) / 2.0))
        return float(self.gates[i, j])

    # ---------------------------------------------------------------------------------------------- placement
    def edge_zones(self):
        zones = []
        inset = PLAYABLE_HALF - 2600.0
        for name, nx, ny in (("North", 1, 0), ("South", -1, 0), ("East", 0, 1), ("West", 0, -1)):
            for t in (-0.62, 0.0, 0.62):
                # Along the edge: t; inward: back to the centre.
                x = nx * inset + (t * inset if nx == 0 else 0.0)
                y = ny * inset + (t * inset if ny == 0 else 0.0)
                zones.append(dict(x=x, y=y, z=self.z_at(x, y), yaw=math.degrees(math.atan2(-ny, -nx)), side=name))
        return zones

    def sandbags(self):
        """Along the true bluff, past the gate shoulders, a walkable strip behind them. Infantry defend from them and enemy
        infantry take cover on them, so both sides of a wall have to stay on the navmesh."""
        out = []
        def arc(centre_bearing, half_deg, radius, kind):
            count = max(1, int(round(math.radians(half_deg * 2.0) * radius / 900.0)))
            for k in range(count):
                b = centre_bearing - half_deg + (k + 0.5) * 2.0 * half_deg / count
                a = math.radians(b)
                out.append(dict(x=radius * math.cos(a), y=radius * math.sin(a), yaw=b, group=kind))
        for bearing in GATE_BEARINGS:
            arc(bearing - 20.0, 6.0, SANDBAG_RADIUS, "gate_%d_left" % bearing)
            arc(bearing + 20.0, 6.0, SANDBAG_RADIUS, "gate_%d_right" % bearing)
            arc(bearing + 30.0, 3.2, SANDBAG_RADIUS, "bluff_%d" % bearing)
        return out

    def power_lines(self):
        """Two runs in from the map edge to the plateau's rear, poles every ~20 m; they keep off the tracks."""
        specs = [("north_east", [(33500.0, 22500.0), (22500.0, 21000.0), (16500.0, 7000.0), (9000.0, 3200.0), (6300.0, 2200.0)]),
                 ("south_west", [(-33500.0, -24000.0), (-23000.0, -13500.0), (-14500.0, -3000.0), (-9300.0, -4600.0), (-6200.0, -3200.0)])]
        lines = []
        for name, way in specs:
            poles = []
            for (ax, ay), (bx, by) in zip(way[:-1], way[1:]):
                length = math.hypot(bx - ax, by - ay)
                count = max(1, int(round(length / 3400.0)))
                for k in range(count):
                    t = k / count
                    poles.append((ax + t * (bx - ax), ay + t * (by - ay)))
            poles.append(way[-1])
            items = []
            for k, (x, y) in enumerate(poles):
                ahead = poles[min(k + 1, len(poles) - 1)]
                behind = poles[max(k - 1, 0)]
                yaw = math.degrees(math.atan2(ahead[1] - behind[1], ahead[0] - behind[0]))
                items.append(dict(x=x, y=y, z=self.z_at(x, y), yaw=yaw))
            lines.append(dict(name=name, poles=items))
        return lines

    def occupied_lines(self, lines):
        self.pole_points = [(p["x"], p["y"]) for line in lines for p in line["poles"]]

    def free(self, x, y, radius, taken, extra=0.0):
        for ox, oy, other in taken:
            if math.hypot(x - ox, y - oy) < radius + other + extra:
                return False
        return True

    def sites(self, taken):
        rng = np.random.default_rng(SEED + 5)
        rules = [
            # category, count, radius, min ring, max ring, max relief in the footprint, min route distance, spacing
            ("Radixite", 4, 350.0, 4700.0, 7000.0, 5.0, 300.0, 900.0),
            ("Metal", 4, 350.0, 4700.0, 7000.0, 5.0, 300.0, 900.0),
            ("Animals", 12, 1800.0, 11000.0, 30000.0, 420.0, 500.0, 300.0),
            ("Metal", 9, 2000.0, 10500.0, 18500.0, 300.0, 800.0, 200.0),
            ("Radixite", 9, 1800.0, 10500.0, 18500.0, 320.0, 800.0, 200.0),
            ("Radixite", 9, 1700.0, 18500.0, 33000.0, 320.0, 700.0, 200.0),
            ("Metal", 9, 1900.0, 18500.0, 33000.0, 320.0, 700.0, 200.0),
            ("Capture", 28, 900.0, 10500.0, 30000.0, 70.0, 700.0, 500.0),
            ("VehicleParts", 18, 500.0, 10500.0, 31000.0, 80.0, 500.0, 300.0),
            ("Wreck", 34, 650.0, 10000.0, 31000.0, 80.0, 500.0, 300.0),
        ]
        out = []
        report = {}
        for category, count, radius, ring0, ring1, relief, route_gap, spacing in rules:
            placed = 0
            attempts = 0
            while placed < count and attempts < 60000:
                attempts += 1
                r = rng.uniform(ring0, ring1)
                # Stratified: every pick starts in the next wedge, so the sides fill evenly.
                spread = min(3.2, 0.5 + attempts / 2000.0)
                a = (placed * 2.399963 + rng.uniform(-spread, spread)) % (2.0 * math.pi)
                x, y = r * math.cos(a), r * math.sin(a)
                if abs(x) > PLAYABLE_HALF - 1900.0 - radius or abs(y) > PLAYABLE_HALF - 1900.0 - radius:
                    continue
                if self.relief_at(x, y, radius) > relief or self.route_at(x, y) < radius + route_gap:
                    continue
                if self.gate_at(x, y) > 0.05 or self.slope_at(x, y) > 9.0:
                    continue
                if not self.free(x, y, radius, taken, spacing):
                    continue
                taken.append((x, y, radius))
                out.append(dict(category=category, x=x, y=y, z=self.z_at(x, y), radius=radius, yaw=math.degrees(a)))
                placed += 1
            report["%s %d-%d" % (category, ring0, ring1)] = "%d/%d in %d tries" % (placed, count, attempts)
        return out, report

    def rocks(self, taken):
        """Boulders that break the bluff line and crown the hills and ridges."""
        rng = np.random.default_rng(SEED + 13)
        out = []
        bearing = 0.0
        while bearing < 360.0:
            bearing += rng.uniform(2.4, 4.6)
            # At the foot of the wall, leaning on it: the footprint stands on the terrace and never hangs over the drop.
            r = CORE_FLAT_R + rng.uniform(980.0, 1240.0)
            a = math.radians(bearing)
            x, y = r * math.cos(a), r * math.sin(a)
            if self.gate_at(x, y) > 0.05:
                continue
            out.append(dict(x=x, y=y, height=float(rng.uniform(230.0, 430.0)), kind=int(rng.integers(2, 7)), yaw=float(rng.uniform(0, 360)),
                            bury=0.2, collision=True))
        spots = []
        for x, y, radius, height, top in HILLS:
            for _ in range(14):
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = radius * rng.uniform(0.25, 0.95)
                spots.append((x + r * math.cos(a), y + r * math.sin(a)))
        for x0, y0, x1, y1, half_w, height, passes in RIDGES:
            for t in np.linspace(0.08, 0.92, 12):
                if min(abs(t - c) for c in passes) < 0.14:
                    continue
                spots.append((x0 + t * (x1 - x0) + rng.uniform(-500, 500), y0 + t * (y1 - y0) + rng.uniform(-500, 500)))
        for x, y in spots:
            if self.route_at(x, y) < 900.0 or not self.free(x, y, 320.0, taken, 150.0):
                continue
            taken.append((x, y, 320.0))
            out.append(dict(x=x, y=y, height=float(rng.uniform(150.0, 360.0)), kind=int(rng.integers(2, 7)), yaw=float(rng.uniform(0, 360)),
                            bury=0.22, collision=True))
        return out

    def plant_patches(self, taken):
        """Foliage patches on the grass: (x, y, radius, density per m2), away from the plateau and the tracks."""
        rng = np.random.default_rng(SEED + 17)
        n = self.verts
        out = []
        tries = 0
        while len(out) < 70 and tries < 20000:
            tries += 1
            x, y = rng.uniform(-PLAYABLE_HALF + 800.0, PLAYABLE_HALF - 800.0, 2)
            i, j = int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)
            if self.R[i, j] < 11000.0 or self.weights["GrassCastle"][i, j] < 90 or self.slope_at(x, y) > 16.0:
                continue
            if self.route_at(x, y) < 900.0:
                continue
            out.append(dict(x=x, y=y, z=self.z_at(x, y), radius=float(rng.uniform(1100.0, 2600.0)), density=float(rng.uniform(0.006, 0.02))))
        return out

    def trees(self, taken):
        """Copses on the slopes and the rim forest; (kind, x, y) with kinds resolved by the editor script."""
        rng = np.random.default_rng(SEED + 9)
        n = self.verts
        wood = fbm(rng, n, [30.0, 10.0])
        out = []
        tree_taken = []
        attempts = 0
        inside = 0
        while inside < 150 and attempts < 40000:
            attempts += 1
            x, y = rng.uniform(-PLAYABLE_HALF, PLAYABLE_HALF, 2)
            if self.R[int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)] < 10500.0:
                continue
            if wood[int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)] < 0.18:
                continue
            if self.slope_at(x, y) > 22.0 or self.route_at(x, y) < 700.0 or self.gate_at(x, y) > 0.02:
                continue
            if not self.free(x, y, 260.0, taken, 100.0) or not self.free(x, y, 200.0, tree_taken):
                continue
            tree_taken.append((x, y, 240.0))
            out.append(dict(x=x, y=y, z=self.z_at(x, y), kind=str(rng.choice(["fir", "fir", "spruce", "leaf", "leaf_small", "bush"]))))
            inside += 1
        rim = 0
        attempts = 0
        while rim < 240 and attempts < 40000:
            attempts += 1
            x, y = rng.uniform(-LANDSCAPE_HALF + 300.0, LANDSCAPE_HALF - 300.0, 2)
            e = max(abs(x), abs(y))
            if e < PLAYABLE_HALF + 500.0 or self.slope_at(x, y) > 30.0:
                continue
            if not self.free(x, y, 300.0, tree_taken):
                continue
            tree_taken.append((x, y, 300.0))
            out.append(dict(x=x, y=y, z=self.z_at(x, y), kind=str(rng.choice(["fir", "spruce", "spruce_dark"]))))
            rim += 1
        return out

    def report(self):
        out = {}
        out["core z range"] = float(self.Z[self.R < CORE_FLAT_R - 10.0].ptp())
        out["height range"] = [float(self.Z.min()), float(self.Z.max())]
        for shelf in self.shelves:
            mask = np.hypot(self.X - shelf["x"], self.Y - shelf["y"]) < shelf["radius"] - 300.0
            shelf["z_range"] = float(self.Z[mask].ptp())
        out["shelf z ranges"] = [round(s["z_range"], 1) for s in self.shelves]
        gate_slopes = []
        for bearing in GATE_BEARINGS:
            a = math.radians(bearing)
            rs = np.arange(CORE_FLAT_R - 200.0, RAMP_END_R + 600.0, 200.0)
            zs = np.array([self.z_at(r * math.cos(a), r * math.sin(a)) for r in rs])
            gate_slopes.append(round(float(np.degrees(np.arctan(np.abs(np.diff(zs)) / 200.0)).max()), 1))
        out["gate centre max slope"] = gate_slopes
        # Every bearing of the true bluff must cross ground over 44 degrees, or the wall has a way up.
        steepest = []
        for bearing in np.arange(0.0, 360.0, 0.5):
            a = math.radians(float(bearing))
            rs = np.arange(CORE_FLAT_R, CORE_FLAT_R + 900.0, 50.0)
            points = [(r * math.cos(a), r * math.sin(a)) for r in rs]
            if max(self.gate_at(x, y) for x, y in points) > 0.02:
                continue
            steepest.append(max(self.slope_at(x, y) for x, y in points))
        out["bluff weakest steepest slope"] = round(float(min(steepest)), 1)
        play = self.edge < PLAYABLE_HALF
        flat = (self.slope < 6.0) & play
        out["playable share under 6 deg"] = round(float(flat.sum() / play.sum()), 3)
        out["playable share over 30 deg"] = round(float(((self.slope > 30.0) & play).sum() / play.sum()), 3)
        return out

    def write(self, out_dir):
        os.makedirs(out_dir, exist_ok=True)
        heights = np.clip(np.round(32768.0 + self.Z * 128.0 / 100.0), 0, 65535).astype("<u2")
        height_path = os.path.join(out_dir, "SM_Kessel_01_height.raw").replace("\\", "/")
        heights.tofile(height_path)
        layer_paths = {}
        for name in LAYERS:
            path = os.path.join(out_dir, "SM_Kessel_01_%s.raw" % name).replace("\\", "/")
            self.weights[name].tofile(path)
            layer_paths[name] = path

        zones = self.edge_zones()
        bags = self.sandbags()
        lines = self.power_lines()
        self.occupied_lines(lines)
        taken = [(p[0], p[1], 500.0) for p in self.pole_points]
        taken += [(s["x"], s["y"], 300.0) for s in bags]
        for hill in self.hill_tops:
            pass
        for zone in zones:
            taken.append((zone["x"], zone["y"], 2600.0))
        sites, site_report = self.sites(taken)
        rocks = self.rocks(taken)
        trees = self.trees(taken)
        plants = self.plant_patches(taken)
        layout = dict(
            map="SM_Kessel_01",
            landscape=dict(corner=[-LANDSCAPE_HALF, -LANDSCAPE_HALF], quad_cm=QUAD_CM, components=COMPONENTS,
                           quads_per_component=QUADS, size_cm=2.0 * LANDSCAPE_HALF, base_z=CORE_Z),
            playable_half=PLAYABLE_HALF, core=dict(z=CORE_Z, build_radius=BUILD_RADIUS, flat_radius=CORE_FLAT_R),
            gates=GATE_BEARINGS, shelves=self.shelves, hills=[list(h) for h in HILLS], edge_zones=zones, sandbags=bags,
            power_lines=lines, sites=sites, trees=trees, rocks=rocks, plants=plants, routes=self.route_lines,
            files=dict(height=height_path, layers=layer_paths))
        with open(os.path.join(out_dir, "SM_Kessel_01_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=0, default=float)
        report = self.report()
        report["sites"] = site_report
        report["sandbag pieces"] = len(bags)
        report["poles"] = [len(line["poles"]) for line in lines]
        report["trees"] = len(trees)
        report["rocks"] = len(rocks)
        report["plant patches"] = len(plants)
        print(json.dumps(report, indent=1))
        self.preview(os.path.join(out_dir, "SM_Kessel_01_preview.png"), layout)
        return layout

    def preview(self, path, layout):
        from PIL import Image, ImageDraw
        size = 1400
        stride = max(1, self.verts // size)
        z = self.Z[::stride, ::stride]
        gy, gx = np.gradient(z, QUAD_CM * stride)
        shade = np.clip(0.72 + 0.9 * (-gx * 0.7 - gy * 0.7), 0.25, 1.25)
        colours = dict(Desert=(0.46, 0.36, 0.25), GrassCastle=(0.36, 0.42, 0.2), RockDesert=(0.42, 0.37, 0.32),
                       GraySand=(0.55, 0.53, 0.49), RTSSand=(0.72, 0.62, 0.45), MountainTop=(0.32, 0.3, 0.29))
        rgb = np.zeros(z.shape + (3,))
        for name, colour in colours.items():
            weight = self.weights[name][::stride, ::stride] / 255.0
            for k in range(3):
                rgb[..., k] += weight * colour[k]
        rgb = np.clip(rgb * shade[..., None] * 255.0, 0, 255).astype(np.uint8)
        image = Image.fromarray(rgb[::-1])  # +Y up in the picture
        draw = ImageDraw.Draw(image)
        h = image.size[1]
        def px(x, y):
            return ((x + LANDSCAPE_HALF) / QUAD_CM / stride, h - (y + LANDSCAPE_HALF) / QUAD_CM / stride)
        a, b = px(-PLAYABLE_HALF, -PLAYABLE_HALF), px(PLAYABLE_HALF, PLAYABLE_HALF)
        draw.rectangle([a[0], b[1], b[0], a[1]], outline=(255, 255, 255))
        colours = dict(Radixite=(70, 255, 90), Metal=(200, 200, 220), Capture=(255, 60, 60), Wreck=(255, 150, 40),
                       VehicleParts=(255, 220, 60), Animals=(120, 200, 255))
        for site in layout["sites"]:
            c = px(site["x"], site["y"])
            r = site["radius"] / QUAD_CM / stride
            draw.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], outline=colours[site["category"]])
        for bag in layout["sandbags"]:
            c = px(bag["x"], bag["y"])
            draw.ellipse([c[0] - 2, c[1] - 2, c[0] + 2, c[1] + 2], fill=(240, 220, 150))
        for line in layout["power_lines"]:
            pts = [px(p["x"], p["y"]) for p in line["poles"]]
            draw.line(pts, fill=(30, 30, 30))
            for c in pts:
                draw.rectangle([c[0] - 2, c[1] - 2, c[0] + 2, c[1] + 2], fill=(0, 0, 0))
        for tree in layout["trees"]:
            c = px(tree["x"], tree["y"])
            draw.point(c, fill=(10, 60, 10))
        for zone in layout["edge_zones"]:
            c = px(zone["x"], zone["y"])
            draw.rectangle([c[0] - 8, c[1] - 8, c[0] + 8, c[1] + 8], outline=(255, 0, 255))
        image.save(path)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    map_gen = Kessel()
    map_gen.ground()
    map_gen.routes()
    map_gen.paint()
    map_gen.relief_maps()
    map_gen.write(out_dir)


if __name__ == "__main__":
    main()
