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
# Ger_MarderStugMission is a 51 000 cm square. The owner asked for 20 % more area (55 800 cm a side), then 10 % more on top
# of that: 55 800 x sqrt(1.1) = 58 522 cm a side.
PLAYABLE_HALF = 29260.0
LANDSCAPE_HALF = COMPONENTS * QUADS * QUAD_CM / 2.0
LAYERS = ["Desert", "GrassCastle", "RockDesert", "GraySand", "RTSSand", "MountainTop"]
# Weight (of 255) of the invisible per-component layer traces in paint().
TRACE_WEIGHT = 2
# Outward bearing of the bluff faces that sit in the sun's shadow (the Marder sun points towards -Y).
SHADED_BEARING = 270.0
# Sign of the corner order of FIELDS once mapped to the world (the farmstead pad's side flips it).
FIELD_WINDING = 1

CORE_Z = 1120.0
# The plateau is not a disc: its rim radius per bearing comes from plateau_r(). These two are its typical size.
CORE_FLAT_R = 9600.0
BUILD_RADIUS = 7200.0
# (bearing, width in degrees, push out in cm): a long prow to the south-east, a deep bay to the south-west.
PLATEAU_LOBES = [(140.0, 28.0, 1900.0), (205.0, 20.0, -1500.0), (318.0, 22.0, 1500.0), (62.0, 18.0, -1000.0),
                 (250.0, 14.0, 900.0), (20.0, 12.0, 600.0)]
TERRACE_Z = 640.0
VALLEY_Z = 330.0
# (bearing, half width of the road at the rim, point of interest on its approach, side of the road it stands on)
GATES = [(22.0, 1150.0, "farmstead", 1.0), (101.0, 1450.0, "convoy", -1.0), (163.0, 1050.0, "crash", 1.0),
         (238.0, 1350.0, "trenches", -1.0), (304.0, 1200.0, "waterworks", 1.0)]
GATE_BEARINGS = [g[0] for g in GATES]
RAMP_LENGTH = 4300.0
# Knolls on the rim, higher than the plateau, flat on top for dug-in tanks: (bearing, top radius, height over the plateau).
# Each has one ramp down into the base; the other faces are too steep to climb.
KNOLLS = [(130.0, 2100.0, 400.0), (272.0, 1800.0, 430.0), (345.0, 2300.0, 330.0)]
KNOLL_RAMP_HALF_WIDTH = 650.0
KNOLL_RAMP_DEGREES = 11.0
# The knoll outline per bearing: top_radius * (1 + sum of amp * sin(k * a + phase)), so no knoll is a disc.
KNOLL_WOBBLE = {130.0: [(2, 0.13, 0.4), (3, 0.07, 2.1), (5, 0.04, 4.0)],
                272.0: [(2, 0.10, 2.6), (3, 0.09, 0.3), (4, 0.05, 1.7)],
                345.0: [(2, 0.15, 5.1), (3, 0.05, 1.2), (5, 0.05, 3.3)]}
# (scene, track name, control points in the pad frame, width): kessel_map_build lines the convoy up on the first one.
SCENE_TRACKS = [("convoy", "old_road", [(-7200.0, -3500.0), (-4600.0, -700.0), (-1500.0, 100.0), (1500.0, -100.0),
                                        (4600.0, -800.0), (7200.0, -3500.0)], 0.85),
                ("farmstead", "yard_a", [(-200.0, -3600.0), (300.0, -1600.0), (600.0, -400.0), (1300.0, 500.0)], 0.4),
                ("farmstead", "yard_b", [(600.0, -400.0), (-300.0, 900.0), (-400.0, 1600.0)], 0.35),
                ("waterworks", "service", [(-400.0, -3600.0), (-700.0, -1400.0), (300.0, -300.0), (1200.0, 900.0)], 0.5)]
# Ground scars in a pad's frame: (scene, u0, v0, u1, v1, half width at the start, at the end). The DC-3 ploughed in
# along its heading; its furrow ends under the fuselage.
SCARS = [("crash", -8000.0, -3500.0, 950.0, 1060.0, 350.0, 950.0)]
# Crop fields beside the farmstead, in its pad frame (u along the road, v away from it): corner lists. Kept off the pad.
FIELDS = [("BP_CropField_Wheat", [(-1500.0, 3900.0), (2600.0, 4300.0), (2900.0, 6900.0), (-1300.0, 6500.0)], 2),
          ("BP_CropField_WheatBurned", [(-5600.0, 3500.0), (-2200.0, 3800.0), (-2100.0, 6200.0), (-5900.0, 5800.0)], 1),
          ("BP_CropField_Pumpkin", [(-7700.0, 300.0), (-5500.0, 100.0), (-5300.0, 2600.0), (-7500.0, 2900.0)], 3)]
# Back from the crest: enemy infantry take cover on the far side of a wall, and that side must be on the navmesh.
SANDBAG_BACK = 300.0
# Narrow enough that the wall stays over the Character agent's 44 degree limit at the lowest bluff, or infantry climb it.
BLUFF_WIDTH = 340.0

# Flat shelves on the terrace, between the gates: expansion ground for buildings.
SHELVES = [(62.0, 15800.0, 3100.0), (200.0, 15200.0, 3300.0), (271.0, 16300.0, 2700.0), (358.0, 15900.0, 2600.0)]
POI_PAD = 3300.0
# Trench Blueprints are ground blocks with the trench cut into them: the terrain is dug out under the cut so it shows.
# (Blueprint, u along the road, v away from it, yaw relative to the road) on the trenches pad, like kessel_map_build.
# Unused since round 2 of the 2026-11-04 detail pass: the blocks' flat tops conform through the height RVT and showed as
# blurred slabs from the game camera. The trench line below (Red Line kit, earth berms on the ground) replaced them.
TRENCHES = []
TRENCH_PROFILES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "kessel_trench_profiles.json")
# The overrun Soviet line on the trenches pad: Red Line straights chained along their local Y (1 000 cm per piece, laid
# 940 apart so the joints overlap), a small turn at every joint, capped at one end and an MG nest at the other.
# Start and headings in the pad frame (heading 0 = +u along the road, 90 = +v away from it).
TRENCH_LINE = dict(start=(900.0, -3100.0), heading=84.0, turns=[0.0, 11.0, -7.0, -15.0, 9.0, 4.0], step=940.0)
# The communication trench runs back from the line to the dugout.
TRENCH_SEAM_BAGS = "DestructableEnvActor/NaturalCover/SandBagWalls/BP_Cover_RusSandbags0"
TRENCH_EXTRAS = [("BP_RL_CommTrench", 420.0, 230.0, 177.0), ("BP_RL_CommTrench", -520.0, 290.0, 173.0),
                 ("BP_RL_Dugout", -1560.0, 380.0, 173.0), ("BP_RL_GunBay", -600.0, 2900.0, 60.0)]
# (x, y, radius, height, flat top share)
HILLS = [(19800.0, 12500.0, 3900.0, 620.0, 0.34), (-20500.0, 12800.0, 3400.0, 520.0, 0.30),
         (-17500.0, -18800.0, 4200.0, 700.0, 0.36), (13000.0, -20000.0, 3300.0, 480.0, 0.30),
         (2500.0, 20000.0, 3000.0, 430.0, 0.30)]
# (x0, y0, x1, y1, half width, height, passes as t along the ridge)
# Kept 5 000 cm or more inside the play edge so the edge spawn zones stand on open ground.
RIDGES = [(-21800.0, -7500.0, -20800.0, 6500.0, 2300.0, 600.0, [0.31]),
          (21500.0, -8000.0, 19500.0, -17000.0, 2100.0, 540.0, [0.68]),
          (-8500.0, 22200.0, -18500.0, 21500.0, 2000.0, 500.0, [0.27]),
          (-4000.0, -22300.0, 6500.0, -22800.0, 1900.0, 450.0, [0.64])]


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


def erode(mask, cells):
    out = mask.copy()
    for _ in range(cells):
        grown = out.copy()
        grown[1:, :] &= out[:-1, :]
        grown[:-1, :] &= out[1:, :]
        grown[:, 1:] &= out[:, :-1]
        grown[:, :-1] &= out[:, 1:]
        out = grown
    return out


def dilate(mask, cells):
    return ~erode(~mask, cells)


def dilate_max(field, cells):
    out = field.copy()
    for _ in range(cells):
        grown = out.copy()
        grown[1:, :] = np.maximum(grown[1:, :], out[:-1, :])
        grown[:-1, :] = np.maximum(grown[:-1, :], out[1:, :])
        grown[:, 1:] = np.maximum(grown[:, 1:], out[:, :-1])
        grown[:, :-1] = np.maximum(grown[:, :-1], out[:, 1:])
        out = grown
    return out


def angle_delta(a, b):
    return np.abs((a - b + 180.0) % 360.0 - 180.0)


def plateau_r(bearing_deg):
    """Rim radius of the plateau at a bearing: overlapping waves plus prows and bays, so no two sides look alike."""
    a = np.radians(bearing_deg)
    r = (CORE_FLAT_R + 1150.0 * np.sin(a + 0.6) + 700.0 * np.sin(2.0 * a + 2.3) + 420.0 * np.sin(3.0 * a + 4.1)
         + 250.0 * np.sin(5.0 * a + 1.2))
    # Buttresses and gullies a few metres deep, so the lip is never one clean curve.
    bites = 300.0 * np.sin(11.0 * a + 0.9) + 200.0 * np.sin(19.0 * a + 2.7) + 90.0 * np.sin(29.0 * a + 4.4)
    for centre, width, push in PLATEAU_LOBES:
        r = r + push * np.exp(-(angle_delta(bearing_deg, centre) / width) ** 2)
    # Never bite into the build radius where the rim already comes close (the south-west bay).
    return r + bites * smooth(BUILD_RADIUS + 300.0, BUILD_RADIUS + 1300.0, r)


def knoll_centre(bearing, top_radius):
    r = float(plateau_r(bearing)) - 200.0
    return r * math.cos(math.radians(bearing)), r * math.sin(math.radians(bearing))


def knoll_edge(knoll_bearing, top_radius, direction_rad):
    """Radius of a knoll's flat top towards a direction (radians, from the knoll centre)."""
    wobble = 1.0
    for k, amp, phase in KNOLL_WOBBLE[knoll_bearing]:
        wobble = wobble + amp * np.sin(k * direction_rad + phase)
    # Small bites and spurs in the lip, like the bluff's.
    wobble = wobble + 0.035 * np.sin(9.0 * direction_rad + knoll_bearing) + 0.02 * np.sin(17.0 * direction_rad + 2.0 * knoll_bearing)
    return top_radius * wobble


def poi_anchor(bearing, side):
    """Beside the approach road, past the ramp foot, on the terrace."""
    a = math.radians(bearing)
    # Clear of the ramp: the pad's inner edge starts past the ramp foot, where the road is on the terrace.
    along = float(plateau_r(bearing)) + RAMP_LENGTH + POI_PAD + 700.0
    return (along * math.cos(a) - side * 3600.0 * math.sin(a), along * math.sin(a) + side * 3600.0 * math.cos(a))


def pad_point(q, u, v):
    """World point of (u, v) in a scene pad's frame: u along the road away from the base, v away from the road."""
    a = math.radians(q["yaw"])
    ux, uy = math.cos(a), math.sin(a)
    vx, vy = -uy * q["side"], ux * q["side"]
    return q["x"] + u * ux + v * vx, q["y"] + u * uy + v * vy


def catmull_rom(points, steps):
    """Smooth curve through the control points, steps samples per span (ends kept)."""
    padded = [points[0]] + list(points) + [points[-1]]
    out = []
    for k in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[k - 1], padded[k], padded[k + 1], padded[k + 2]
        for s in range(steps):
            t = s / float(steps)
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * (2.0 * p1[i] + (-p0[i] + p2[i]) * t + (2.0 * p0[i] - 5.0 * p1[i] + 4.0 * p2[i] - p3[i]) * t2
                                    + (-p0[i] + 3.0 * p1[i] - 3.0 * p2[i] + p3[i]) * t3) for i in (0, 1)))
    out.append(tuple(points[-1]))
    return out


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
        """Mask of the gate roads and, under it, the ramp profile of whichever gate is strongest there."""
        mask = np.zeros_like(self.R)
        ramp = np.zeros_like(self.R)
        for bearing, half_width, _, _ in GATES:
            rim = float(plateau_r(bearing))
            radial_x, radial_y = math.cos(math.radians(bearing)), math.sin(math.radians(bearing))
            along = self.X * radial_x + self.Y * radial_y
            # The cut walls wander in plan; the grade along the road is untouched.
            across = np.abs(-self.X * radial_y + self.Y * radial_x) + 260.0 * self.cut_noise
            wide = half_width * (1.0 + 0.9 * smooth(rim, rim + RAMP_LENGTH, along))
            one = (1.0 - smooth(wide * 0.62, wide * 1.55, across)) * (along > 0.0)
            stronger = one > mask
            ramp = np.where(stronger, smooth(rim - 300.0, rim + RAMP_LENGTH, along), ramp)
            mask = np.maximum(mask, one)
        return mask, ramp

    def knoll_field(self, base):
        """Raise the knolls over the plateau rim: flat tops, 60 degree faces, one ramp each towards the base."""
        z = base
        self.knolls = []
        steep = math.tan(math.radians(60.0))
        for bearing, top_radius, height in KNOLLS:
            cx, cy = knoll_centre(bearing, top_radius)
            top = CORE_Z + height
            dist = np.hypot(self.X - cx, self.Y - cy)
            direction = np.arctan2(self.Y - cy, self.X - cx)
            edge = knoll_edge(bearing, top_radius, direction)
            # Faces between 55 and 65 degrees round the knoll, never a lathe-turned cone.
            face = np.tan(np.radians(60.0 + 5.0 * np.sin(3.0 * direction + bearing)))
            body = top - np.maximum(dist - edge, 0.0) * face
            # The ramp runs from the knoll's inner edge towards the plateau centre.
            ux, uy = -cx / math.hypot(cx, cy), -cy / math.hypot(cx, cy)
            ramp_edge = float(knoll_edge(bearing, top_radius, math.atan2(uy, ux)))
            inward = (self.X - cx) * ux + (self.Y - cy) * uy
            lateral = np.abs(-(self.X - cx) * uy + (self.Y - cy) * ux)
            length = height / math.tan(math.radians(KNOLL_RAMP_DEGREES))
            ramp_z = top - np.clip(inward - ramp_edge + 200.0, 0.0, None) * (height / length)
            ramp_z = ramp_z - np.maximum(lateral - KNOLL_RAMP_HALF_WIDTH, 0.0) * steep
            ramp_z = np.where(inward > 0.0, ramp_z, -1e9)
            z = np.maximum(z, np.maximum(body, ramp_z))
            ramp_end = (cx + ux * (ramp_edge - 200.0 + length), cy + uy * (ramp_edge - 200.0 + length))
            self.knolls.append(dict(x=cx, y=cy, z=top, radius=top_radius, bearing=bearing, height=height,
                                    ramp_end=ramp_end, ramp_length=length, ramp_edge=ramp_edge,
                                    min_radius=float(knoll_edge(bearing, top_radius, np.linspace(0, 2 * math.pi, 720)).min())))
        return z

    def ground(self):
        rng = self.rng
        n = self.verts
        roll = fbm(rng, n, [34.0, 15.0, 6.0])
        warp = fbm(rng, n, [60.0, 25.0])
        detail = fbm(rng, n, [5.0, 2.2])
        self.grass_noise = fbm(rng, n, [20.0, 8.0, 3.5, 1.6])
        self.dirt_noise = fbm(rng, n, [13.0, 5.0, 2.2, 1.2])
        self.rough_noise = detail
        # Domain warp: pull the patch noise along a second noise field, so patches stretch and curl instead of sitting
        # as round blobs of one size.
        warp_u, warp_v = fbm(rng, n, [18.0, 7.0]), fbm(rng, n, [18.0, 7.0])
        rows, cols = np.indices((n, n))
        wi = np.clip(rows + np.round(warp_u * 28.0).astype(int), 0, n - 1)
        wj = np.clip(cols + np.round(warp_v * 28.0).astype(int), 0, n - 1)
        self.grass_noise = self.grass_noise[wi, wj]
        self.dirt_noise = self.dirt_noise[wi, wj]
        self.cut_noise = fbm(rng, n, [6.0, 2.5])
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

        for k, (x0, y0, x1, y1, half_w, height, passes) in enumerate(RIDGES):
            # The crest wanders sideways, rises and falls along its length and has no flat top; the pass is a saddle,
            # off centre, so the two halves differ in size.
            length = math.hypot(x1 - x0, y1 - y0)
            nx, ny = -(y1 - y0) / length, (x1 - x0) / length
            _, t0 = segment_distance(self.X, self.Y, x0, y0, x1, y1)
            sway = 900.0 * np.sin(t0 * math.pi * 2.2 + k) + 500.0 * np.sin(t0 * math.pi * 5.0 + 2.0 * k)
            dist, t = segment_distance(self.X - nx * sway, self.Y - ny * sway, x0, y0, x1, y1)
            # One flank falls away twice as steeply as the other.
            side = ((self.X - nx * sway - x0) * nx + (self.Y - ny * sway - y0) * ny) > 0.0
            dist = np.where(side, dist * 1.9, dist)
            width = half_w * (0.55 + 0.55 * np.abs(np.sin(t * math.pi * 2.0 + k)) + 0.15 * warp)
            bump = 1.0 - smooth(0.0, width, dist)
            bump = bump * bump * (3.0 - 2.0 * bump)
            # Two to four humps of different height along the crest, with real saddles between them.
            humps = 2 + k % 3
            along = (0.25 + 0.75 * np.abs(np.sin(t * math.pi * humps + 0.7 * k)) ** 0.8) * (0.7 + 0.3 * np.sin(t * 4.1 + k))
            along = along * smooth(0.0, 0.18, t) * smooth(0.0, 0.18, 1.0 - t)
            for centre in passes:
                along = along * (0.35 + 0.65 * smooth(0.03, 0.16, np.abs(t - centre)))
            terrace += height * bump * along * (1.0 + 0.16 * roll)
        self.hill_tops = []
        for k, (x, y, radius, height, top) in enumerate(HILLS):
            # No mesas: the crest sits off centre, the outline wanders round it and one side falls away steeper.
            phase = 1.7 * k + 0.4
            cx, cy = x + 0.22 * radius * math.cos(phase), y + 0.22 * radius * math.sin(phase)
            theta = np.arctan2(self.Y - cy, self.X - cx)
            outline = (1.0 + 0.2 * np.sin(2.0 * theta + phase) + 0.1 * np.sin(3.0 * theta + 2.0 * phase)
                       - 0.22 * np.cos(theta - phase * 1.3))
            dist = np.hypot(self.X - cx, self.Y - cy) * (1.0 + 0.16 * warp) / outline
            rise = 1.0 - smooth(radius * top * 0.35, radius, dist)
            terrace += height * rise * rise * (3.0 - 2.0 * rise) * (1.0 + 0.1 * roll)
            self.hill_tops.append((cx, cy, radius * top * 0.35))

        # Shelves are exactly level: flat ground for buildings on the middle tier. The point of interest pads too.
        self.shelf_mask = np.zeros_like(terrace)
        self.shelves = []
        self.pois = []
        # Scene pads first, shelves last: a scene pad's lobed outer blend must never tilt a shelf.
        pads = []
        for bearing, _, kind, side in GATES:
            ax, ay = poi_anchor(bearing, side)
            pads.append((math.degrees(math.atan2(ay, ax)) % 360.0, math.hypot(ax, ay), POI_PAD, (kind, bearing, side)))
        pads += [(b, r, d, None) for b, r, d in SHELVES]
        for bearing, radius_at, disc, poi in pads:
            x, y = radius_at * math.cos(math.radians(bearing)), radius_at * math.sin(math.radians(bearing))
            ix = int(round(x / QUAD_CM + (n - 1) / 2.0))
            iy = int(round(y / QUAD_CM + (n - 1) / 2.0))
            level = float(np.round(terrace[iy, ix] / 10.0) * 10.0) + 30.0
            # Scene pads are level all the way out to where their buildings stand; past that every edge wobbles, so the
            # pads never read as discs.
            true_dist = np.hypot(self.X - x, self.Y - y)
            wobble = 0.22 * warp + 0.12 * roll
            if poi:
                # The level ground pushes out in lobes past the scene, so its edge never shows as an arc.
                theta = np.arctan2(self.Y - y, self.X - x)
                lobes = np.clip(np.sin(3.0 * theta + 1.1 * len(self.pois)) + 0.6 * np.sin(5.0 * theta + 0.3), 0.0, None)
                true_dist = np.maximum(true_dist - 380.0 * lobes * smooth(disc, disc + 400.0, true_dist), np.minimum(true_dist, disc))
            dist = true_dist * (1.0 + (wobble * smooth(disc + 200.0, disc + 900.0, true_dist) if poi else 0.1 * warp + 0.1 * roll))
            blend = 1.0 - smooth(disc + (500.0 if poi else 0.0), disc + 2000.0 + 900.0 * roll, dist)
            terrace = terrace * (1.0 - blend) + level * blend
            self.shelf_mask = np.maximum(self.shelf_mask, 1.0 - smooth(disc - 600.0, disc + 400.0, dist * (1.0 + 0.25 * self.dirt_noise)))
            if poi is None:
                self.shelves.append(dict(x=x, y=y, z=level, radius=disc, bearing=bearing))
            else:
                self.pois.append(dict(kind=poi[0], gate=poi[1], side=poi[2], x=x, y=y, z=level, radius=disc,
                                      yaw=poi[1]))

        # Plateau: an irregular rim, bluffs between the gates, long ramps of different widths through them.
        gates, ramp = self.gate_mask()
        self.gates = gates
        self.rim = plateau_r(self.bearing)
        bluff_r = self.rim + 150.0 * (0.5 + 0.5 * roll)
        # The face is wider in some places and abrupt in others; never so wide that infantry could climb it.
        width = BLUFF_WIDTH * (1.0 + 0.22 * detail + 0.18 * roll)
        bluff = smooth(bluff_r, bluff_r + width, self.R)
        fall = bluff * (1.0 - gates) + ramp * gates
        # A low talus apron of fallen soil at the foot, in lobes, so the base of the wall wobbles.
        foot = self.R - (bluff_r + width)
        apron = np.clip(0.35 + 0.9 * self.cut_noise + 0.4 * roll, 0.0, 1.0) * (1.0 - smooth(-50.0, 400.0 + 500.0 * (roll + 1.0), foot))
        terrace = terrace + 110.0 * apron * (foot > -width) * (1.0 - gates)
        z = CORE_Z * (1.0 - fall) + terrace * fall
        z = np.where(self.R <= self.rim, CORE_Z, z)
        z = self.knoll_field(z)
        z = self.dig_trenches(z)
        z = self.plough_scars(z)

        # The rim outside the playable square: hills, then a band that hides the end of the landscape.
        out = np.maximum(self.edge - PLAYABLE_HALF - 400.0, 0.0)
        rim = smooth(0.0, 5200.0, out) * (760.0 + 620.0 * fbm(rng, n, [30.0, 12.0]))
        z += np.maximum(rim, 0.0)
        z += 60.0 * detail * smooth(0.0, 2000.0, out)
        self.Z = z
        self.slope_map()

    def plough_scars(self, z):
        """Shallow furrows with thrown-up lips where something ploughed in (SCARS); kept out of the pad level check."""
        self.scar_cells = np.zeros_like(z, dtype=bool)
        for kind, u0, v0, u1, v1, w0, w1 in SCARS:
            q = next(p for p in self.pois if p["kind"] == kind)
            (ax, ay), (bx, by) = pad_point(q, u0, v0), pad_point(q, u1, v1)
            d, t = segment_distance(self.X, self.Y, ax, ay, bx, by)
            half = w0 + (w1 - w0) * t
            inside = (t > 0.0) & (t < 1.0) & (d < half * 2.4)
            trough = np.exp(-(d / (half * 0.55)) ** 2)
            lips = np.exp(-((d - half) / (half * 0.3)) ** 2) * np.clip(0.5 + 0.7 * self.cut_noise, 0.0, 1.0)
            depth = 30.0 * np.sin(np.pi * np.clip(t, 0.0, 1.0)) ** 0.5
            z = np.where(inside, z - depth * trough + 0.9 * depth * lips, z)
            self.scar_cells |= inside & (d < half * 2.4)
        return z

    def dig_trenches(self, z):
        """Lower the ground under each trench cut to below its floor; the block's own top covers the rest."""
        with open(TRENCH_PROFILES) as handle:
            profiles = json.load(handle)
        self.trenches = []
        self.dug = np.zeros_like(z, dtype=bool)
        pad = next(q for q in self.pois if q["kind"] == "trenches")
        a = math.radians(pad["yaw"])
        ux, uy = math.cos(a), math.sin(a)
        vx, vy = -uy * pad["side"], ux * pad["side"]
        for name, u, v, yaw in TRENCHES:
            prof = profiles[name]
            grid = np.array([np.nan if h is None else h for h in prof["z"]]).reshape(prof["ny"], prof["nx"])
            defined = ~np.isnan(grid)
            # The cut: well inside the block, below its top. Grown a landscape quad so no triangle between a dug and an
            # undug vertex can lean into the trench. The block sits a little under the ground (kessel_map_build), so
            # outside the dug ring the landscape covers its flat top and only the trench and its lip show.
            cut = erode(defined, 2) & (np.nan_to_num(grid, nan=0.0) < -20.0)
            dig = dilate(cut, 2) & erode(defined, 1)
            floor = -dilate_max(np.where(cut, -grid, 0.0), 3)
            x = pad["x"] + u * ux + v * vx
            y = pad["y"] + u * uy + v * vy
            actor_yaw = pad["yaw"] + yaw
            c, s_ = math.cos(math.radians(actor_yaw)), math.sin(math.radians(actor_yaw))
            dx, dy = self.X - x, self.Y - y
            lx, ly = dx * c + dy * s_, -dx * s_ + dy * c
            i = np.round((lx - prof["x0"]) / prof["step"]).astype(int)
            j = np.round((ly - prof["y0"]) / prof["step"]).astype(int)
            inside = (i >= 0) & (i < prof["nx"]) & (j >= 0) & (j < prof["ny"])
            ii, jj = np.clip(i, 0, prof["nx"] - 1), np.clip(j, 0, prof["ny"] - 1)
            mask = inside & dig[jj, ii]
            z = np.where(mask, np.minimum(z, pad["z"] + floor[jj, ii] - 15.0), z)
            self.dug |= mask
            block = inside & defined[jj, ii]
            self.trenches.append(dict(name=name, x=x, y=y, z=pad["z"], yaw=actor_yaw, block=block))
        # Spoil thrown up in front of the trenches (away from the base): a low, lumpy parapet berm, never on a block.
        blocks = np.zeros_like(self.dug)
        for t in self.trenches:
            blocks |= t["block"]
        self.berm = np.zeros_like(self.dug)
        along_road = (self.X - pad["x"]) * ux + (self.Y - pad["y"]) * uy
        for t, (_, u, _, _) in zip(self.trenches, TRENCHES):
            self.berm |= dilate(t["block"], 6) & (along_road > u + 100.0)
        self.berm &= ~dilate(blocks, 2)
        lump = np.clip(0.55 + 0.8 * self.cut_noise, 0.0, 1.0)
        z = np.where(self.berm, z + gauss(self.berm.astype(float), 1.2) * 60.0 * lump, z)
        return z

    def slope_map(self):
        gy, gx = np.gradient(self.Z, QUAD_CM)
        self.slope = np.degrees(np.arctan(np.hypot(gx, gy)))

    def routes(self):
        """Tracks from the map edge up each gate, plus the terrace lane that links the shelves."""
        self.route_lines = []
        edge = PLAYABLE_HALF
        ends = {22.0: (edge, 9500.0), 101.0: (-4000.0, edge), 163.0: (-edge, 11000.0),
                238.0: (-15000.0, -edge), 304.0: (17000.0, -edge)}
        rng = np.random.default_rng(SEED + 21)
        # Where each lane ends inside the base: not all at the centre, they peter out round the HQ ground.
        hubs = {22.0: (1800.0, 900.0), 101.0: (300.0, 2100.0), 163.0: (-2300.0, 700.0), 238.0: (-1200.0, -1900.0),
                304.0: (1500.0, -1400.0)}
        for bearing, end in ends.items():
            a = math.radians(bearing)
            rim = float(plateau_r(bearing))
            top = rim - 900.0
            foot = rim + RAMP_LENGTH + 300.0
            # On the plateau the lane bends from its hub to the gate; on the ramp it runs straight with the cut.
            hub = hubs[bearing]
            inner = (top * 0.55 * math.cos(a) + hub[0] * 0.45 + 1700.0 * math.sin(a * 2.0 + 0.5),
                     top * 0.55 * math.sin(a) + hub[1] * 0.45 - 1700.0 * math.cos(a * 3.0))
            control = [hub, inner, (top * math.cos(a), top * math.sin(a)), (foot * math.cos(a), foot * math.sin(a))]
            # Off the ramp the track wanders: jittered control points, so no road is a ruled line.
            count = 4
            for k in range(1, count + 1):
                t = k / float(count)
                bx = foot * math.cos(a) * (1.0 - t) + end[0] * t
                by = foot * math.sin(a) * (1.0 - t) + end[1] * t
                side = rng.uniform(-1.0, 1.0) * 1500.0 * math.sin(math.pi * t)
                nx, ny = -(end[1] - foot * math.sin(a)), end[0] - foot * math.cos(a)
                nl = math.hypot(nx, ny)
                control.append((bx + side * nx / nl, by + side * ny / nl))
            self.route_lines.append(dict(name="gate_%d" % int(bearing), points=catmull_rom(control, 10)))
        # Inside the base the lanes meet on a worn, lopsided loop round the HQ ground instead of stopping in mid-air.
        hub_ring = []
        for k in range(13):
            b = math.radians(k * 30.0 + 10.0)
            r = 2400.0 + 700.0 * math.sin(2.0 * b + 0.8) + rng.uniform(-300.0, 300.0)
            hub_ring.append((r * math.cos(b), r * math.sin(b)))
        hub_ring[-1] = hub_ring[0]
        self.route_lines.append(dict(name="gate_hub", points=catmull_rom(hub_ring, 8), width=0.8))
        # The terrace lane is not a ring: three arcs that link neighbouring gate roads and a shelf, each its own wobble.
        for name, b0, b1 in (("lane_ne", 30.0, 92.0), ("lane_sw", 170.0, 231.0), ("lane_nw", 312.0, 372.0)):
            control = []
            for k in range(7):
                b = b0 + (b1 - b0) * k / 6.0
                r = float(plateau_r(b % 360.0)) + 3700.0 + 700.0 * math.sin(math.radians(b) * 3.0 + 1.0) + rng.uniform(-500.0, 500.0)
                control.append((r * math.cos(math.radians(b)), r * math.sin(math.radians(b))))
            self.route_lines.append(dict(name=name, points=catmull_rom(control, 8)))
        # Tracks inside the scenes, in each pad's frame (u along the road away from the base, v away from the road):
        # the old road the convoy died on, yard paths between the farm buildings, the waterworks' service track.
        for kind, name, control, width in SCENE_TRACKS:
            q = next(p for p in self.pois if p["kind"] == kind)
            self.route_lines.append(dict(name=name, points=catmull_rom([pad_point(q, u, v) for u, v in control], 8), width=width))
        # Short spurs from each road to its scene and to two shelves.
        for q in self.pois:
            gate_line = next(l for l in self.route_lines if l["name"] == "gate_%d" % int(q["gate"]))
            near = min(gate_line["points"], key=lambda p: math.hypot(p[0] - q["x"], p[1] - q["y"]))
            mid = ((near[0] + q["x"]) / 2.0 + rng.uniform(-500.0, 500.0), (near[1] + q["y"]) / 2.0 + rng.uniform(-500.0, 500.0))
            self.route_lines.append(dict(name="spur_%s" % q["kind"], points=catmull_rom([near, mid, (q["x"], q["y"])], 6),
                                         width=0.6))
        self.route_distance = np.full_like(self.R, 1e9)
        self.line_distance = {}
        for line in self.route_lines:
            dist = np.full_like(self.R, 1e9)
            pts = line["points"]
            for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
                d, _ = segment_distance(self.X, self.Y, ax, ay, bx, by)
                dist = np.minimum(dist, d)
            # A spur is a narrower track: its distance counts as farther from the middle.
            dist = dist / line.get("width", 1.0)
            self.line_distance[line["name"]] = dist
            self.route_distance = np.minimum(self.route_distance, dist)

    def paint(self):
        # What the layers look like (one wedge per layer, 2026-11-03): GrassCastle green, Desert cream, GraySand yellow sand,
        # RTSSand salmon, RockDesert grey, MountainTop cream.
        # Grass follows the water: hollows, the wash banks, the foot of every slope and the shaded north faces. Crests and
        # south faces dry out to soil. Noise alone gave even blobs that ignored the ground.
        hollow = np.clip((gauss(self.Z, 8.0) - self.Z) / 70.0, -1.0, 1.0)
        self.hollow = hollow
        gy, gx = np.gradient(gauss(self.Z, 3.0), QUAD_CM)
        north_face = np.clip(gx * -6.0, -1.0, 1.0)  # +X is north: ground falling towards +X faces north
        wash_bank = np.clip(gauss(self.wash, 6.0) * 1.6 - self.wash * 0.8, 0.0, 1.0)
        slope_foot = np.clip(gauss(smooth(12.0, 30.0, self.slope), 10.0) * 2.0, 0.0, 1.0) * (self.slope < 12.0)
        # Fine grain breaks every patch edge into tufts and bites; without it the big noise reads as soft blobs.
        # No octave under 3 cells: the breakup re-thresholds these weights, and anything finer turned into 1 m stair steps.
        fine = fbm(self.rng, self.verts, [6.0, 3.0])
        # Whole 50-100 m stretches have no soil splotches at all.
        self.splotch_area = smooth(-0.15, 0.35, fbm(self.rng, self.verts, [70.0, 35.0]))
        self.fine_noise = fine
        grass = (0.5 + 0.45 * self.grass_noise + 0.6 * hollow + 0.55 * wash_bank + 0.35 * north_face + 0.6 * slope_foot
                 + 0.45 * fine)
        desert = (0.14 + 0.2 * np.clip(self.dirt_noise, 0.0, 1.0) + 0.3 * np.clip(-self.grass_noise, 0.0, 1.0)
                  + 0.25 * np.clip(-north_face, 0.0, 1.0) + 0.3 * np.clip(-fine, 0.0, 1.0) * self.splotch_area)
        # Rain runs down off the hills in streaks: grass along the fall lines below every slope.
        gyl, gxl = np.gradient(gauss(self.Z, 6.0), QUAD_CM)
        fall_line = np.abs(np.sin((gxl * self.Y - gyl * self.X) / np.maximum(np.hypot(gxl, gyl), 1e-4) / 700.0 + 3.0 * self.dirt_noise))
        # Only where the ground really falls (over ~8 degrees), a few rills rather than a comb.
        slope_l = np.degrees(np.arctan(np.hypot(gxl, gyl)))
        rills = smooth(6.0, 11.0, slope_l) * smooth(0.82, 0.98, fall_line) * np.clip(0.2 + self.dirt_noise, 0.0, 1.0)
        # (Rills were removed in round 6 of the detail pass: below the ramps they read as combed stripes.)
        # Grass against soil: a 4-12 m octave and a contrast curve, so patch edges resolve over a few metres instead of a
        # 20 m airbrush, while nothing finer than 3 m reaches the weightmap.
        mid = fbm(self.rng, self.verts, [10.0, 5.0])
        cover = grass + desert
        share = smooth(0.42, 0.58, grass / np.maximum(cover, 1e-4) + 0.35 * mid)
        grass, desert = cover * share, cover * (1.0 - share)
        # Rock shows where the slope is steep, but not along a contour: the threshold wanders with the noise.
        rock_g = smooth(15.0 + 7.0 * self.rough_noise, 25.0 + 9.0 * self.rough_noise, self.slope)
        # The bluff and knoll faces are bare rock, not soil stretched down a wall.
        rock_g = np.maximum(rock_g, smooth(30.0, 40.0, self.slope))
        mountain = smooth(40.0, 52.0, self.slope) * 0.0
        sand = 0.3 * smooth(0.25, 0.75, self.wash) + 0.12 * np.clip(self.rough_noise - 0.35, 0.0, 1.0)
        gray = (0.1 + 0.42 * np.clip(self.rough_noise, 0.0, 1.0) + 0.8 * smooth(0.2, 0.7, self.wash)
                + 0.45 * np.clip(hollow, 0.0, 1.0) * np.clip(mid + 0.3, 0.0, 1.0) + 0.35 * slope_foot)
        # Scree: broken rock and grit spilled out below the bluffs and the knoll faces, in tongues, not a band.
        below = (self.R > self.rim + 200.0) & (self.gates < 0.05)
        scree = gauss((smooth(28.0, 45.0, self.slope)).astype(float), 5.0) * below
        scree = np.clip(scree * 2.2 * (0.35 + 0.9 * np.clip(self.rough_noise + 0.4 * self.dirt_noise, 0.0, 1.0)), 0.0, 1.0)
        # Each outcrop sheds a tongue of grit and stones out and downhill from it.
        self.outcrops = self.outcrop_centres()
        for ox, oy, size in self.outcrops:
            ang = math.atan2(oy, ox)
            du = (self.X - ox) * math.cos(ang) + (self.Y - oy) * math.sin(ang)
            dv = -(self.X - ox) * math.sin(ang) + (self.Y - oy) * math.cos(ang)
            tongue = np.exp(-(dv / (size * 1.3 * (1.0 + 0.4 * self.rough_noise))) ** 2) * np.exp(-np.clip(du, -size, None) ** 2 / (size * 3.5) ** 2)
            scree = np.maximum(scree, np.clip(tongue * 1.4 * (0.5 + 0.7 * np.clip(self.fine_noise + 0.5, 0.0, 1.0)), 0.0, 1.0))
        rock_g = np.maximum(rock_g, 0.55 * scree * (self.slope < 28.0))
        gray = gray + 0.9 * scree
        grass = grass * (1.0 - 0.7 * scree)
        # The tracks: worn to soil down the middle, ragged at the edge.
        track_edge = 300.0 + 170.0 * self.rough_noise
        track = 1.0 - smooth(track_edge * 0.55, track_edge * 1.35, self.route_distance)
        track = track * (self.edge < PLAYABLE_HALF + 300.0)
        desert = desert * (1.0 - track) + 1.25 * track
        grass = grass * (1.0 - 0.92 * track)
        gray = gray + 0.28 * track
        # The plateau is a trampled position: mostly packed soil with grass left in patches.
        core = 1.0 - smooth(self.rim - 300.0, self.rim + 200.0, self.R)
        for knoll in self.knolls:
            # The knoll tops are worn gun positions: packed soil like the base, worn most round the gun pits.
            kd = np.hypot(self.X - knoll["x"], self.Y - knoll["y"])
            kedge = knoll_edge(knoll["bearing"], knoll["radius"], np.arctan2(self.Y - knoll["y"], self.X - knoll["x"]))
            core = np.maximum(core, (1.0 - smooth(kedge - 500.0, kedge + 100.0, kd)) * (0.6 + 0.4 * np.clip(self.dirt_noise + 0.6, 0.0, 1.0)))
        # Trampled hardest in the middle and along the lanes from the gates; the rim keeps grass in tufts and fringes.
        trample = smooth(0.95, 0.35, self.R / np.maximum(self.rim, 1.0))
        lanes = np.zeros_like(self.R)
        for name, dist in self.line_distance.items():
            if name.startswith("gate_"):
                lanes = np.maximum(lanes, 1.0 - smooth(250.0, 900.0 + 300.0 * self.rough_noise, dist))
        lanes = lanes * core
        desert = desert * (1.0 - core) + core * (0.5 + 0.3 * trample + 0.35 * np.clip(self.dirt_noise + 0.3, 0.0, 1.0))
        # Inside the base grass survives only as a broken fringe under the rim and in small tufts, never in big patches.
        fringe = smooth(self.rim - 1500.0 - 500.0 * self.grass_noise, self.rim - 250.0, self.R)
        tufts = np.clip(fine - 0.3, 0.0, 1.0) * 1.6 * (1.0 - trample)
        grass = (grass * (1.0 - core) + core * (grass * 0.55 * fringe + 0.4 * tufts)) * (1.0 - 0.8 * lanes)
        gray = gray + 0.2 * core * np.clip(self.grass_noise + 0.4, 0.0, 1.0) + 0.35 * lanes * np.clip(0.6 + self.rough_noise, 0.0, 1.0)
        shelf = self.shelf_mask * np.clip(0.45 + 0.9 * self.dirt_noise, 0.0, 1.0)
        # Shelves get no paint of their own: a cap painted to their outline gave them away.
        # Worn ground where people worked: round the scene buildings and behind every sandbag wall.
        wear = np.zeros_like(self.R)
        for q in self.pois:
            d = np.hypot(self.X - q["x"], self.Y - q["y"]) * (1.0 + 0.3 * self.dirt_noise)
            if q["kind"] == "waterworks":
                # A gravelled yard round the tower and silos.
                yard = (1.0 - smooth(q["radius"] * 0.35, q["radius"] * 0.75, d)) * np.clip(0.6 + 0.6 * self.rough_noise, 0.0, 1.0)
                gray = gray + 1.1 * yard
                grass = grass * (1.0 - 0.8 * yard)
                continue
            # Worn ground stretches along the road and throws spurs out to the buildings: lobed, never a disc.
            a = math.radians(q["yaw"])
            du = (self.X - q["x"]) * math.cos(a) + (self.Y - q["y"]) * math.sin(a)
            dv = -(self.X - q["x"]) * math.sin(a) + (self.Y - q["y"]) * math.cos(a)
            theta = np.arctan2(dv, du)
            lobed = np.hypot(du / 1.45, dv) * (1.0 + 0.28 * np.sin(3.0 * theta + q["gate"]) + 0.18 * np.sin(5.0 * theta) + 0.35 * self.dirt_noise)
            wear = np.maximum(wear, 1.0 - smooth(q["radius"] * 0.3, q["radius"] * 0.95, lobed))
        # Spoil and trampled ground along the trench line.
        self.trench_layout = self.trench_pieces()
        self.spoil = np.zeros_like(self.R)
        for piece in self.trench_layout:
            d = np.hypot(self.X - piece["x"], self.Y - piece["y"]) * (1.0 + 0.3 * self.rough_noise)
            spoil = (1.0 - smooth(450.0, 1150.0, d)) * np.clip(0.5 + 0.8 * self.fine_noise + 0.3, 0.0, 1.0)
            # The kit's berms are reddish fresh earth: the spoil round them is the same RTSSand.
            desert = desert + 0.35 * spoil
            sand = sand + 0.9 * spoil
            grass = grass * (1.0 - 0.6 * spoil)
            self.spoil = np.maximum(self.spoil, spoil)
        for kind, u0, v0, u1, v1, w0, w1 in SCARS:
            q = next(p for p in self.pois if p["kind"] == kind)
            (ax, ay), (bx, by) = pad_point(q, u0, v0), pad_point(q, u1, v1)
            d, t = segment_distance(self.X, self.Y, ax, ay, bx, by)
            half = w0 + (w1 - w0) * t
            scar = (1.0 - smooth(half * 0.5, half * (1.2 + 0.4 * self.rough_noise), d)) * (t > 0.0) * (t < 1.0)
            # Fresh, turned earth, the reddish RTSSand, darkest in the trough.
            desert = desert + 0.4 * scar
            sand = sand + 1.4 * scar * np.clip(self.fine_noise + 0.6, 0.0, 1.0)
            grass = grass * (1.0 - 0.95 * scar)
            self.spoil = np.maximum(self.spoil, scar)
        for bag in self.sandbags():
            d = np.hypot(self.X - bag["x"], self.Y - bag["y"]) * (1.0 + 0.35 * self.rough_noise)
            wear = np.maximum(wear, 0.8 * (1.0 - smooth(250.0, 800.0, d)))
        wear = wear * np.clip(0.55 + 0.7 * self.dirt_noise, 0.0, 1.0)
        desert = desert + 0.6 * wear
        grass = grass * (1.0 - 0.6 * wear)
        # Packed, trampled soil goes on MountainTop, the one soft layer the dry grass tufts never grow on: the base, the
        # knoll tops, every track and worked yard, plus bare drifts out on the terrace so tufts gather in patches.
        packed = core * (0.55 + 0.45 * trample) * np.clip(0.75 + 0.5 * self.dirt_noise, 0.0, 1.0)
        packed = np.maximum(packed, 0.85 * track)
        packed = np.maximum(packed, lanes)
        packed = np.maximum(packed, 0.5 * wear * np.clip(0.4 + self.dirt_noise, 0.0, 1.0))
        bare = 0.7 * smooth(0.1, 0.5, self.dirt_noise) * (1.0 - core) * smooth(0.0, 0.4, desert - grass * 0.6)
        packed = np.maximum(packed, bare)
        packed = packed * (1.0 - self.spoil)
        desert = desert * (1.0 - 0.85 * packed)
        gray = gray * (1.0 - 0.6 * packed)
        # Packed soil is not one value: grit and reddish sand worked through it at mid scale, so the base reads in tones.
        tone = np.clip(mid * 1.4 + 0.15, 0.0, 1.0)
        gray = gray + 0.45 * packed * tone
        sand = sand + 0.35 * packed * np.clip(-mid * 1.4 + 0.1, 0.0, 1.0)
        rock_g = np.maximum(rock_g, 0.18 * packed * np.clip(self.fine_noise - 0.2, 0.0, 1.0))
        # Ash and scorched stubble spill 3-10 m past the burned field's fence, with a ragged edge.
        for field in self.fields():
            if "Burned" not in field["bp"]:
                continue
            corners = field["corners"]
            inside = np.ones_like(self.R, dtype=bool)
            edge_d = np.full_like(self.R, 1e9)
            for (ax, ay), (bx, by) in zip(corners, corners[1:] + corners[:1]):
                d, _ = segment_distance(self.X, self.Y, ax, ay, bx, by)
                edge_d = np.minimum(edge_d, d)
                inside &= ((bx - ax) * (self.Y - ay) - (by - ay) * (self.X - ax)) * (1 if FIELD_WINDING > 0 else -1) >= 0.0
            reach = 300.0 + 700.0 * np.clip(0.5 + self.cut_noise, 0.0, 1.0)
            ash = np.where(inside, 1.0, 1.0 - smooth(0.0, reach, edge_d))
            rock_g = np.maximum(rock_g, 0.45 * ash * np.clip(0.6 + 0.5 * self.fine_noise, 0.0, 1.0))
            gray = gray + 0.4 * ash
            grass = grass * (1.0 - 0.8 * ash)
            packed = packed * (1.0 - ash)
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
            "MountainTop": np.maximum(mountain, packed * 1.4) * rock_keep,
        }
        # Sharpen a little so the layers read as patches rather than a soup, then quantise to sum 255.
        stack = np.stack([raw[name] ** 1.7 for name in LAYERS], axis=-1)
        stack /= np.maximum(stack.sum(axis=-1, keepdims=True), 1e-5)
        quantised = np.floor(stack * 255.0).astype(np.int32)
        rest = 255 - quantised.sum(axis=-1)
        biggest = stack.argmax(axis=-1)
        rows, cols = np.indices(rest.shape)
        quantised[rows, cols, biggest] += rest
        # Every component gets a trace of every layer at one inner vertex. A component painted with only Desert, grass,
        # GraySand and MountainTop compiles to a layer permutation that renders washed out (pale rectangles on the
        # component grid, 2026-11-04); with all six allocated each component uses the permutation that looks right.
        for cy in range(COMPONENTS):
            for cx in range(COMPONENTS):
                iy, ix = cy * QUADS + QUADS // 2, cx * QUADS + QUADS // 2
                block = quantised[cy * QUADS:(cy + 1) * QUADS + 1, cx * QUADS:(cx + 1) * QUADS + 1]
                for k in range(len(LAYERS)):
                    if block[..., k].max() == 0:
                        donor = int(quantised[iy, ix].argmax())
                        quantised[iy, ix, donor] -= TRACE_WEIGHT
                        quantised[iy, ix, k] += TRACE_WEIGHT
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
        """Along the true bluff, past the gate shoulders, a walkable strip behind them, and on the knoll rims. Infantry defend
        from them and enemy infantry take cover on them, so both sides of a wall have to stay on the navmesh. Each gate gets
        its own arrangement, not a copy of the next one."""
        out = []
        # (gate bearing, walls left of the road, walls right of the road)
        layout = {22.0: (2, 0), 101.0: (2, 1), 163.0: (0, 3), 238.0: (1, 2), 304.0: (2, 1)}
        for bearing, half_width, _, _ in GATES:
            left, right = layout[bearing]
            rim = float(plateau_r(bearing))
            shoulder = math.degrees(math.atan2(half_width * 1.55 + 900.0, rim))
            for side, count in ((-1.0, left), (1.0, right)):
                b = bearing + side * shoulder
                for k in range(count):
                    radius = float(plateau_r(b)) - SANDBAG_BACK
                    a = math.radians(b)
                    jitter = math.sin(bearing * 1.7 + k * 2.3 + side)
                    out.append(dict(x=radius * math.cos(a), y=radius * math.sin(a), yaw=b + 7.0 * jitter, group="gate_%d" % bearing))
                    # Gaps of 0-3 m between walls, never the same twice.
                    b += side * math.degrees((980.0 + 150.0 + 150.0 * math.sin(bearing + 3.1 * k)) / radius)
        for knoll in self.knolls:
            # On the outer edge of the top, looking over the bluff.
            # Gun pits: a horseshoe of three walls round each gun spot, open towards the ramp and the base, set back
            # from the lip by the nearest it comes over the front wall's 9 m.
            out_bearing = knoll["bearing"]
            spots = (-22.0, 24.0) if knoll["radius"] > 2000.0 else (6.0,)
            for offset in spots:
                ob = out_bearing + offset + (knoll["bearing"] % 5.0)
                a = math.radians(ob)
                span = np.radians(np.linspace(ob - 18.0, ob + 18.0, 13))
                r = float(knoll_edge(knoll["bearing"], knoll["radius"], span).min()) - SANDBAG_BACK - 150.0
                fx, fy = knoll["x"] + r * math.cos(a), knoll["y"] + r * math.sin(a)
                tx, ty, nx, ny = -math.sin(a), math.cos(a), math.cos(a), math.sin(a)
                group = "knoll_%d_%d" % (knoll["bearing"], len(out))
                # An open V facing out over the lip, the two arms swung back by different angles, a 2 m gap at the point.
                arms = {130.0: ((-1.0, 12.0, 470.0), (1.0, 34.0, 620.0)), 272.0: ((-1.0, 40.0, 560.0), (1.0, 8.0, 470.0)),
                        345.0: ((-1.0, 22.0, 650.0), (1.0, 18.0, 470.0))}[knoll["bearing"]]
                if offset > 0.0 and knoll["radius"] > 2000.0:
                    arms = tuple((sd, sw + 9.0, rc) for sd, sw, rc in arms[::-1])
                for side, swing, reach in arms:
                    reach = 100.0 + reach
                    sw = math.radians(swing)
                    ax_, ay_ = tx * side * math.cos(sw) - nx * math.sin(sw), ty * side * math.cos(sw) - ny * math.sin(sw)
                    out.append(dict(x=fx + ax_ * reach, y=fy + ay_ * reach, yaw=ob + side * swing, group=group, z=knoll["z"]))
        # A gate's shoulder wall and a knoll's arc can land on the same stretch of rim: never stack two walls.
        kept = []
        for bag in out:
            if all(bag["group"] == k["group"] or math.hypot(bag["x"] - k["x"], bag["y"] - k["y"]) > 700.0 for k in kept):
                kept.append(bag)
        return kept

    def power_lines(self):
        """Two runs in from the map edge to the plateau's rear, poles every ~20 m; they keep off the tracks."""
        specs = [("north_east", [(33500.0, 26500.0), (24000.0, 24500.0), (12000.0, 16500.0), (6800.0, 7800.0), (4800.0, 5600.0)]),
                 ("south_west", [(-33500.0, -24000.0), (-24500.0, -18500.0), (-20500.0, -9500.0), (-11500.0, -5600.0), (-5600.0, -3600.0)])]
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
                # Never on a face or a ramp: slide along the line to flat ground.
                for shift in (0.0, 400.0, -400.0, 800.0, -800.0, 1200.0, -1200.0):
                    dx, dy = math.cos(math.radians(yaw)) * shift, math.sin(math.radians(yaw)) * shift
                    if self.slope_at(x + dx, y + dy) < 12.0 and self.gate_at(x + dx, y + dy) < 0.05:
                        x, y = x + dx, y + dy
                        break
                items.append(dict(x=x, y=y, z=self.z_at(x, y), yaw=yaw))
            lines.append(dict(name=name, poles=items))
        return lines

    def fields(self):
        """Crop fields of the farmstead in world corners, with the steepest ground under each (crops lie on the ground)."""
        pad = next(q for q in self.pois if q["kind"] == "farmstead")
        a = math.radians(pad["yaw"])
        ux, uy = math.cos(a), math.sin(a)
        vx, vy = -uy * pad["side"], ux * pad["side"]
        out = []
        for k, (bp, corners, seed) in enumerate(FIELDS):
            world = [(pad["x"] + u * ux + v * vx, pad["y"] + u * uy + v * vy) for u, v in corners]
            cx, cy = sum(p[0] for p in world) / 4.0, sum(p[1] for p in world) / 4.0
            steepest = max(self.slope_at(px * t + cx * (1 - t), py * t + cy * (1 - t)) for px, py in world for t in (0.0, 0.5, 1.0))
            out.append(dict(bp=bp, corners=world, x=cx, y=cy, seed=seed, yaw=pad["yaw"], steepest=round(steepest, 1),
                            radius=max(math.hypot(px - cx, py - cy) for px, py in world)))
        return out

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
            ("Radixite", 4, 350.0, 4700.0, 6900.0, 5.0, 300.0, 900.0),
            ("Metal", 4, 350.0, 4700.0, 6900.0, 5.0, 300.0, 900.0),
            ("Animals", 12, 1800.0, 11000.0, 30000.0, 420.0, 500.0, 300.0),
            ("Metal", 7, 1350.0, 10500.0, 19500.0, 340.0, 700.0, 200.0),
            ("Radixite", 7, 1350.0, 10500.0, 19500.0, 340.0, 700.0, 200.0),
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
                # Outer rings start at the bluff foot, wherever the irregular rim puts it.
                if ring0 > BUILD_RADIUS and r < float(plateau_r(math.degrees(a))) + BLUFF_WIDTH + radius + 400.0:
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

    def trench_pieces(self):
        """World placement of the Red Line pieces on the trenches pad: (Blueprint, x, y, actor yaw)."""
        pad = next(q for q in self.pois if q["kind"] == "trenches")
        a = math.radians(pad["yaw"])
        u_axis = (math.cos(a), math.sin(a))
        v_axis = (-u_axis[1] * pad["side"], u_axis[0] * pad["side"])

        def world_yaw(heading):
            # Pad-frame heading to the actor yaw that points the piece's local +Y along it.
            h = math.radians(heading)
            dx, dy = math.cos(h) * u_axis[0] + math.sin(h) * v_axis[0], math.cos(h) * u_axis[1] + math.sin(h) * v_axis[1]
            return math.degrees(math.atan2(dy, dx)) - 90.0

        def place(bp, u, v, heading):
            x, y = pad_point(pad, u, v)
            return dict(bp=bp, path=("RedLine/" + bp) if bp.startswith("BP_RL_") else bp, x=x, y=y, yaw=world_yaw(heading))

        line = TRENCH_LINE
        u, v = line["start"]
        heading = line["heading"]
        out = [place("BP_RL_TrenchEnd", u - 60.0 * math.cos(math.radians(heading)), v - 60.0 * math.sin(math.radians(heading)),
                     heading + 180.0)]
        for turn in line["turns"]:
            heading += turn
            du, dv = math.cos(math.radians(heading)), math.sin(math.radians(heading))
            out.append(place("BP_RL_Trench_Straight", u + du * 500.0, v + dv * 500.0, heading))
            u, v = u + du * line["step"], v + dv * line["step"]
            # Sandbags heaped over most joints hide the seam between two pieces; not every one, never the same angle.
            if (len(out) * 7) % 10 < 7:
                out.append(place(TRENCH_SEAM_BAGS, u - du * 30.0, v - dv * 30.0, heading + 90.0 + ((len(out) * 37) % 25 - 12)))
        out.append(place("BP_RL_MGNest", u + math.cos(math.radians(heading)) * 330.0, v + math.sin(math.radians(heading)) * 330.0,
                         heading))
        out += [place(bp, pu, pv, h) for bp, pu, pv, h in TRENCH_EXTRAS]
        # A camouflage net over the dugout mound.
        out.append(place("DestructableEnvActor/Military/BP_Military_Dest_Camonet", -1560.0, 380.0, 173.0))
        return out

    def outcrop_centres(self):
        """Where the bluff has shed rock: (x, y, size of the biggest block). One or two per stretch, never a ring."""
        rng = np.random.default_rng(SEED + 14)
        out = []
        for bearing, clusters in ((48.0, 2), (77.0, 1), (186.0, 2), (215.0, 1), (322.0, 2), (262.0, 1)):
            rim = float(plateau_r(bearing))
            for k in range(clusters):
                b = bearing + rng.uniform(-5.0, 5.0) + 7.0 * k
                r = rim + BLUFF_WIDTH + rng.uniform(500.0, 1100.0)
                x, y = r * math.cos(math.radians(b)), r * math.sin(math.radians(b))
                if self.gate_at(x, y) > 0.05 or self.slope_at(x, y) > 20.0:
                    continue
                out.append((x, y, float(rng.uniform(380.0, 620.0))))
        return out

    def rocks(self, taken):
        """Boulders that break the bluff line and crown the hills and ridges."""
        rng = np.random.default_rng(SEED + 13)
        out = []
        # Outcrops at the foot of the bluff: each one tight cluster, one big block and smaller ones falling off in size,
        # half buried, over the scree tongue the paint gives it.
        for cx, cy, size in self.outcrops:
            if not self.free(cx, cy, 300.0, taken, 50.0):
                continue
            taken.append((cx, cy, size * 0.6))
            count = int(rng.integers(4, 9))
            for k in range(count):
                rank = k / float(count)
                height = size * (1.0 - 0.75 * rank) * rng.uniform(0.8, 1.1)
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = 0.0 if k == 0 else size * rng.uniform(0.35, 0.9) * (0.6 + rank)
                x, y = cx + r * math.cos(a), cy + r * math.sin(a)
                if self.gate_at(x, y) > 0.05 or self.slope_at(x, y) > 25.0:
                    continue
                out.append(dict(x=x, y=y, height=float(height), kind=int(rng.integers(2, 7)), yaw=float(rng.uniform(0, 360)),
                                bury=float(rng.uniform(0.3, 0.5)), collision=height > 150.0))
        # Hills and ridges: rock breaks out of their crests in a few clusters, not strung along them.
        spots = []
        for x, y, radius, height, top in HILLS:
            for _ in range(int(rng.integers(1, 3))):
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = radius * rng.uniform(0.1, 0.6)
                spots.append((x + r * math.cos(a), y + r * math.sin(a)))
        for x0, y0, x1, y1, half_w, height, passes in RIDGES:
            for t in rng.uniform(0.12, 0.88, int(rng.integers(2, 4))):
                if min(abs(t - c) for c in passes) < 0.14:
                    continue
                spots.append((x0 + t * (x1 - x0) + rng.uniform(-400, 400), y0 + t * (y1 - y0) + rng.uniform(-400, 400)))
        for cx, cy in spots:
            if self.route_at(cx, cy) < 900.0 or not self.free(cx, cy, 320.0, taken, 150.0):
                continue
            taken.append((cx, cy, 700.0))
            size = float(rng.uniform(220.0, 420.0))
            count = int(rng.integers(3, 7))
            for k in range(count):
                rank = k / float(count)
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = 0.0 if k == 0 else size * rng.uniform(0.4, 1.0) * (0.7 + rank)
                x, y = cx + r * math.cos(a), cy + r * math.sin(a)
                if self.route_at(x, y) < 700.0:
                    continue
                height = size * (1.0 - 0.7 * rank) * rng.uniform(0.8, 1.1)
                out.append(dict(x=x, y=y, height=float(height), kind=int(rng.integers(2, 7)), yaw=float(rng.uniform(0, 360)),
                                bury=float(rng.uniform(0.3, 0.45)), collision=height > 150.0))
        # Rubble round every boulder: a few small stones that have broken off it, never collision.
        for rock in list(out):
            for _ in range(int(rng.integers(1, 4))):
                a = rng.uniform(0.0, 2.0 * math.pi)
                r = rock["height"] * rng.uniform(0.7, 1.5)
                x, y = rock["x"] + r * math.cos(a), rock["y"] + r * math.sin(a)
                if self.route_at(x, y) < 500.0 or self.gate_at(x, y) > 0.05 or self.slope_at(x, y) > 25.0:
                    continue
                out.append(dict(x=x, y=y, height=float(rock["height"] * rng.uniform(0.15, 0.32)), kind=int(rng.integers(2, 7)),
                                yaw=float(rng.uniform(0, 360)), bury=0.3, collision=False))
        # Rock breaking out of the bluff face itself, so the wall is not one smooth band. Visual only: a collider on the
        # face could give infantry a step up it.
        face_spots = []
        # Faces turned away from the sun (it shines towards -Y, so bearings 180-360) sit in shadow and show the stretched
        # face texture most: a dense run of clusters there.
        for _ in range(90):
            centre = (rng.normal(SHADED_BEARING, 45.0)) % 360.0
            face_spots += [centre + rng.normal(0.0, 0.7) for _ in range(int(rng.integers(3, 10)))]
        for _ in range(55):
            centre = rng.uniform(0.0, 360.0)
            face_spots += [centre + rng.normal(0.0, 0.8) for _ in range(int(rng.integers(1, 8)))]
        for bearing in face_spots:
            bearing = bearing % 360.0
            rim = float(plateau_r(bearing))
            r = rim + 150.0 + BLUFF_WIDTH * rng.uniform(0.35, 0.8)
            a = math.radians(bearing)
            x, y = r * math.cos(a), r * math.sin(a)
            if self.gate_at(x, y) > 0.02 or self.slope_at(x, y) < 30.0:
                continue
            if any(math.hypot(x - k["x"], y - k["y"]) < k["radius"] * 1.4 + 600.0 for k in self.knolls):
                continue
            if rng.uniform() > 0.45 + 0.55 * max(0.0, math.cos(a - math.radians(SHADED_BEARING))):
                continue
            # Long axis along the face, like strata breaking out of it; one big block per few, the rest smaller.
            out.append(dict(x=x, y=y, height=float(rng.choice([620.0, 460.0, 340.0, 340.0, 220.0]) * rng.uniform(0.85, 1.15)),
                            kind=int(rng.integers(2, 7)),
                            yaw=float(bearing + 90.0 + rng.uniform(-25.0, 25.0)), bury=0.45, collision=False, face=True))
        # A broken, rocky first metre under each knoll lip, everywhere but its ramp.
        for knoll in self.knolls:
            ramp_dir = math.atan2(-knoll["y"], -knoll["x"])
            lip_spots = []
            for _ in range(int(rng.integers(5, 9))):
                centre = rng.uniform(0.0, 2.0 * math.pi)
                lip_spots += [centre + rng.normal(0.0, 0.07) for _ in range(int(rng.integers(1, 7)))]
            for a in lip_spots:
                if abs((a - ramp_dir + math.pi) % (2.0 * math.pi) - math.pi) < math.radians(28.0):
                    continue
                r = float(knoll_edge(knoll["bearing"], knoll["radius"], a)) + rng.uniform(60.0, 260.0)
                x, y = knoll["x"] + r * math.cos(a), knoll["y"] + r * math.sin(a)
                if self.slope_at(x, y) < 30.0:
                    continue
                out.append(dict(x=x, y=y, height=float(rng.choice([420.0, 280.0, 180.0, 180.0]) * rng.uniform(0.85, 1.15)),
                                kind=int(rng.integers(2, 7)),
                                yaw=float(math.degrees(a) + 90.0 + rng.uniform(-30.0, 30.0)), bury=0.45, collision=False, face=True))
        return out

    def plant_patches(self, taken):
        """Foliage patches on the grass: (x, y, radius, density per m2), away from the plateau and the tracks."""
        rng = np.random.default_rng(SEED + 17)
        n = self.verts
        out = []
        tries = 0
        while len(out) < 150 and tries < 30000:
            tries += 1
            x, y = rng.uniform(-PLAYABLE_HALF + 800.0, PLAYABLE_HALF - 800.0, 2)
            i, j = int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)
            if self.R[i, j] < 11000.0 or self.weights["GrassCastle"][i, j] < 90 or self.slope_at(x, y) > 16.0:
                continue
            if self.route_at(x, y) < 900.0:
                continue
            # Plants crowd where water gathers: most patches go in hollows and on the wash banks.
            if rng.uniform() > 0.25 + 1.2 * max(0.0, float(self.hollow[i, j])) + float(self.wash[i, j]):
                continue
            out.append(dict(x=x, y=y, z=self.z_at(x, y), radius=float(rng.uniform(600.0, 1700.0)), density=float(rng.uniform(0.01, 0.03)),
                            kind="plants"))
        # Stones kicked up along the base's rim, where the soil is thinnest (foliage, no collision: never in the way).
        for _ in range(16):
            bearing = rng.uniform(0.0, 360.0)
            r = float(plateau_r(bearing)) - rng.uniform(400.0, 1600.0)
            x, y = r * math.cos(math.radians(bearing)), r * math.sin(math.radians(bearing))
            if self.gate_at(x, y) < 0.05:
                out.append(dict(x=x, y=y, z=self.z_at(x, y), radius=float(rng.uniform(400.0, 900.0)),
                                density=float(rng.uniform(0.01, 0.025)), kind="stones"))
        # Stones scattered where the ground is thin: round the outcrops and over the scree.
        for ox, oy, size in self.outcrops:
            for _ in range(2):
                a, r = rng.uniform(0.0, 2.0 * math.pi), size * rng.uniform(1.0, 3.0)
                x, y = ox + r * math.cos(a), oy + r * math.sin(a)
                if self.route_at(x, y) > 500.0 and self.gate_at(x, y) < 0.05:
                    out.append(dict(x=x, y=y, z=self.z_at(x, y), radius=float(rng.uniform(500.0, 1100.0)),
                                    density=float(rng.uniform(0.015, 0.04)), kind="stones"))
        return out

    def trees(self, taken):
        """Copses on the slopes and the rim forest; (kind, x, y) with kinds resolved by the editor script."""
        rng = np.random.default_rng(SEED + 9)
        n = self.verts
        wood = fbm(rng, n, [30.0, 10.0])
        out = []
        tree_taken = []

        field_spots = {(round(f["x"]), round(f["y"])) for f in self.fields()}
        beside_fields = [t for t in taken if (round(t[0]), round(t[1])) not in field_spots]

        def can_grow(x, y, gap, blockers=taken):
            if abs(x) > PLAYABLE_HALF - 300.0 or abs(y) > PLAYABLE_HALF - 300.0:
                return False
            i, j = int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)
            if self.R[i, j] < float(plateau_r(math.degrees(math.atan2(y, x)))) + 2000.0:
                return False
            if self.slope_at(x, y) > 22.0 or self.route_at(x, y) < 700.0 or self.gate_at(x, y) > 0.02:
                return False
            return self.free(x, y, 260.0, blockers, 100.0) and self.free(x, y, gap, tree_taken)

        def plant(x, y, kind, gap):
            tree_taken.append((x, y, gap))
            out.append(dict(x=x, y=y, z=self.z_at(x, y), kind=kind))

        # Copses: a few big trees in the middle, smaller trees round them and bushes on the edge, where the ground holds
        # water (hollows, the wash, the foot of the slopes) more often than on open ground.
        moist = np.clip(gauss(self.weights["GrassCastle"].astype(float) / 255.0, 8.0), 0.0, 1.0)
        copses = 0
        attempts = 0
        while copses < 32 and attempts < 9000:
            attempts += 1
            x, y = rng.uniform(-PLAYABLE_HALF + 1500.0, PLAYABLE_HALF - 1500.0, 2)
            i, j = int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)
            if wood[i, j] + 0.8 * moist[i, j] < 0.75 or not can_grow(x, y, 900.0):
                continue
            copses += 1
            heart = "leaf" if rng.uniform() < 0.55 else str(rng.choice(["spruce", "fir"]))
            count = int(rng.integers(5, 13))
            spread = rng.uniform(700.0, 1400.0)
            stretch, turn = rng.uniform(1.0, 2.2), rng.uniform(0.0, math.pi)
            grown = 0
            for _ in range(count * 3):
                if grown >= count:
                    break
                du, dv = rng.normal(0.0, spread * stretch * 0.5), rng.normal(0.0, spread * 0.5)
                tx = x + du * math.cos(turn) - dv * math.sin(turn)
                ty = y + du * math.sin(turn) + dv * math.cos(turn)
                ring = math.hypot(du / stretch, dv) / spread
                kind = heart if ring < 0.45 else str(rng.choice(["leaf_small", heart, "bush"] if ring < 0.9 else ["bush", "bush", "leaf_small"]))
                if can_grow(tx, ty, 330.0 if kind != "bush" else 200.0):
                    plant(tx, ty, kind, 330.0 if kind != "bush" else 200.0)
                    grown += 1
        # Hedgerows: a broken line of bushes and small trees along a field edge or a track, with gaps.
        for line in self.route_lines:
            if not (line["name"].startswith("gate_") or line["name"].startswith("lane_")) or rng.uniform() < 0.15:
                continue
            pts = line["points"]
            start = int(len(pts) * rng.uniform(0.35, 0.6))
            side = rng.choice([-1.0, 1.0])
            for k in range(start, min(len(pts) - 1, start + int(rng.integers(8, 16)))):
                (ax, ay), (bx, by) = pts[k], pts[k + 1]
                length = math.hypot(bx - ax, by - ay) or 1.0
                nx, ny = -(by - ay) / length, (bx - ax) / length
                off = side * rng.uniform(950.0, 1150.0)
                if rng.uniform() < 0.22:
                    continue
                tx, ty = ax + nx * off + rng.normal(0.0, 90.0), ay + ny * off + rng.normal(0.0, 90.0)
                kind = str(rng.choice(["bush", "bush", "leaf_small", "leaf"]))
                if can_grow(tx, ty, 260.0):
                    plant(tx, ty, kind, 260.0)
        # Lone trees out in the open: a few, mostly broad leaf trees.
        lone = 0
        attempts = 0
        # Field edges: the farm's fields are lined with bushes and the odd tree on two sides.
        for field in self.fields():
            corners = field["corners"]
            for side in rng.choice(4, size=2, replace=False):
                (ax, ay), (bx, by) = corners[side], corners[(side + 1) % 4]
                length = math.hypot(bx - ax, by - ay)
                for t in np.arange(0.05, 0.95, 650.0 / length):
                    if rng.uniform() < 0.25:
                        continue
                    ox, oy = (by - ay) / length * 450.0, -(bx - ax) / length * 450.0
                    tx, ty = ax + t * (bx - ax), ay + t * (by - ay)
                    # Outside the field: whichever normal points away from its centre.
                    if math.hypot(tx + ox - field["x"], ty + oy - field["y"]) < math.hypot(tx - ox - field["x"], ty - oy - field["y"]):
                        ox, oy = -ox, -oy
                    kind = str(rng.choice(["bush", "bush", "leaf_small"]))
                    if "Burned" in field["bp"]:
                        # The fire took the hedge with it.
                        kind = "dead"
                    if can_grow(tx + ox, ty + oy, 250.0, beside_fields):
                        plant(tx + ox, ty + oy, kind, 250.0)
        while lone < 30 and attempts < 8000:
            attempts += 1
            x, y = rng.uniform(-PLAYABLE_HALF, PLAYABLE_HALF, 2)
            if can_grow(x, y, 1200.0):
                plant(x, y, str(rng.choice(["leaf", "leaf", "leaf_small", "fir"])), 300.0)
                lone += 1
                # Every other one gets a few companions: smaller trees and a bush or two in its lee.
                if lone % 2 == 0:
                    for _ in range(int(rng.integers(2, 5))):
                        a, r = rng.uniform(0.0, 2.0 * math.pi), rng.uniform(350.0, 900.0)
                        kind = str(rng.choice(["bush", "leaf_small", "bush"]))
                        if can_grow(x + r * math.cos(a), y + r * math.sin(a), 230.0):
                            plant(x + r * math.cos(a), y + r * math.sin(a), kind, 230.0)
        inside = len(out)
        rim = 0
        attempts = 0
        while rim < 300 and attempts < 60000:
            attempts += 1
            x, y = rng.uniform(-LANDSCAPE_HALF + 300.0, LANDSCAPE_HALF - 300.0, 2)
            e = max(abs(x), abs(y))
            if e < PLAYABLE_HALF + 500.0 or self.slope_at(x, y) > 30.0:
                continue
            # Thickest just past the play edge, thinning out towards the end of the landscape.
            if rng.uniform() > (1.0 if e < PLAYABLE_HALF + 3500.0 else 0.35):
                continue
            # Clearings and denser stands, so the rim forest is not an even plantation.
            if wood[int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)] < -0.2:
                continue
            if not self.free(x, y, 300.0, tree_taken):
                continue
            tree_taken.append((x, y, 300.0))
            out.append(dict(x=x, y=y, z=self.z_at(x, y), kind=str(rng.choice(["fir", "spruce", "spruce_dark"]))))
            rim += 1
        # The forest's inner edge spills a ragged distance into play: 3-25 m deep, never over a spawn zone or a track.
        fringe = 0
        attempts = 0
        while fringe < 170 and attempts < 40000:
            attempts += 1
            x, y = rng.uniform(-PLAYABLE_HALF + 300.0, PLAYABLE_HALF - 300.0, 2)
            depth = PLAYABLE_HALF - max(abs(x), abs(y))
            i, j = int(y / QUAD_CM + (n - 1) / 2), int(x / QUAD_CM + (n - 1) / 2)
            if depth > 400.0 + 3200.0 * np.clip(wood[i, j] + 0.5, 0.0, 1.0):
                continue
            if can_grow(x, y, 300.0):
                plant(x, y, str(rng.choice(["fir", "spruce", "spruce_dark", "fir_small", "bush"])), 300.0)
                fringe += 1
        return out

    def report(self):
        out = {}
        knoll_area = np.zeros_like(self.R, dtype=bool)
        for knoll in self.knolls:
            knoll_area |= np.hypot(self.X - knoll["x"], self.Y - knoll["y"]) < knoll["radius"] + knoll["ramp_length"] + 900.0
        base = (self.R < self.rim - 10.0) & ~knoll_area
        out["core z range"] = float(self.Z[base].ptp())
        out["plateau rim min max"] = [round(float(self.rim.min())), round(float(self.rim.max()))]
        out["plateau flat area m2"] = round(float(base.sum()) * QUAD_CM * QUAD_CM / 1e4)
        out["knoll top z ranges"] = [round(float(self.Z[np.hypot(self.X - k["x"], self.Y - k["y"]) < k["min_radius"] - 150.0].ptp()), 1)
                                     for k in self.knolls]
        out["poi pad z ranges"] = [round(float(self.Z[(np.hypot(self.X - q["x"], self.Y - q["y"]) < q["radius"] - 300.0)
                                                      & ~self.dug & ~self.berm & ~self.scar_cells].ptp()), 1) for q in self.pois]
        # The ground round each trench's cut must be the pad level, or the block floats or sinks at its edges.
        out["trench block ground off pad (cm)"] = [round(float(np.abs(self.Z[t["block"] & ~self.dug] - t["z"]).max()), 1)
                                                   for t in self.trenches]
        out["trench cells dug"] = [int((t["block"] & self.dug).sum()) for t in self.trenches]
        out["height range"] = [float(self.Z.min()), float(self.Z.max())]
        for shelf in self.shelves:
            mask = np.hypot(self.X - shelf["x"], self.Y - shelf["y"]) < shelf["radius"] - 300.0
            shelf["z_range"] = float(self.Z[mask].ptp())
        out["shelf z ranges"] = [round(s["z_range"], 1) for s in self.shelves]
        gate_slopes = []
        for bearing in GATE_BEARINGS:
            a = math.radians(bearing)
            rim = float(plateau_r(bearing))
            rs = np.arange(rim - 200.0, rim + RAMP_LENGTH + 600.0, 200.0)
            zs = np.array([self.z_at(r * math.cos(a), r * math.sin(a)) for r in rs])
            gate_slopes.append(round(float(np.degrees(np.arctan(np.abs(np.diff(zs)) / 200.0)).max()), 1))
        out["gate centre max slope"] = gate_slopes
        # Every bearing of the true bluff must cross ground over 44 degrees, or the wall has a way up.
        steepest = []
        for bearing in np.arange(0.0, 360.0, 0.5):
            a = math.radians(float(bearing))
            rim = float(plateau_r(float(bearing)))
            rs = np.arange(rim, rim + 900.0, 50.0)
            points = [(r * math.cos(a), r * math.sin(a)) for r in rs]
            if max(self.gate_at(x, y) for x, y in points) > 0.02:
                continue
            # Where a knoll sits on the rim its own faces are the wall; they have their own check below.
            if any(math.hypot(x - k["x"], y - k["y"]) < k["radius"] + 700.0 for k in self.knolls for x, y in points):
                continue
            steepest.append(max(self.slope_at(x, y) for x, y in points))
        out["bluff weakest steepest slope"] = round(float(min(steepest)), 1)
        # Knoll faces: from the top edge outwards every way except down the ramp.
        weakest = []
        for knoll in self.knolls:
            ramp_bearing = math.degrees(math.atan2(-knoll["y"], -knoll["x"]))
            for b in np.arange(0.0, 360.0, 2.0):
                if abs((b - ramp_bearing + 180.0) % 360.0 - 180.0) < 25.0:
                    continue
                a = math.radians(float(b))
                edge = float(knoll_edge(knoll["bearing"], knoll["radius"], a))
                rs = np.arange(edge, edge + 400.0, 25.0)
                weakest.append(max(self.slope_at(knoll["x"] + r * math.cos(a), knoll["y"] + r * math.sin(a)) for r in rs))
        out["knoll faces weakest steepest slope"] = round(float(min(weakest)), 1)
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
        taken += [(q["x"], q["y"], q["radius"] + 600.0) for q in self.pois]
        fields = self.fields()
        taken += [(f["x"], f["y"], f["radius"] + 300.0) for f in fields]
        for knoll in self.knolls:
            # The knoll and its ramp stay free of generated content.
            taken.append((knoll["x"], knoll["y"], knoll["radius"] + 500.0))
            for t in (0.35, 0.7, 1.0):
                ex, ey = knoll["ramp_end"]
                taken.append((knoll["x"] + t * (ex - knoll["x"]), knoll["y"] + t * (ey - knoll["y"]), KNOLL_RAMP_HALF_WIDTH + 400.0))
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
            gates=GATE_BEARINGS, gate_widths=[g[1] for g in GATES], knolls=self.knolls, pois=self.pois,
            trenches=[{k: v for k, v in t.items() if k != "block"} for t in self.trenches], trench_line=self.trench_layout,
            scars=[dict(kind=kind, start=pad_point(next(q for q in self.pois if q["kind"] == kind), u0, v0),
                        end=pad_point(next(q for q in self.pois if q["kind"] == kind), u1, v1), w0=w0, w1=w1)
                   for kind, u0, v0, u1, v1, w0, w1 in SCARS],
            plateau_rim=[round(float(plateau_r(b))) for b in range(360)], shelves=self.shelves, hills=[list(h) for h in HILLS], edge_zones=zones, sandbags=bags,
            power_lines=lines, sites=sites, trees=trees, rocks=rocks, plants=plants, routes=self.route_lines, fields=fields,
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
        report["field steepest ground (deg)"] = [f["steepest"] for f in fields]
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
        for poi in layout["pois"]:
            c = px(poi["x"], poi["y"])
            r = poi["radius"] / QUAD_CM / stride
            draw.rectangle([c[0] - r, c[1] - r, c[0] + r, c[1] + r], outline=(255, 255, 0))
            draw.text((c[0] - r, c[1] - r - 12), poi["kind"], fill=(255, 255, 0))
        for knoll in layout["knolls"]:
            c = px(knoll["x"], knoll["y"])
            r = knoll["radius"] / QUAD_CM / stride
            draw.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], outline=(0, 255, 255))
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
