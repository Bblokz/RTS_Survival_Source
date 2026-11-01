"""Hand-designed landforms for the start locations of a survival map (offline, numpy only).

survival_terrain_gen.py hands every start area a list of terrain operations, applied in order on top of the desert:

  pad      flat ground at one height with an irregular outline; drops (or rises) to whatever lies around it through a
           natural scarp: lip, cliff band, ledge, concave talus. Lower tiers first, then the higher ones.
  ridge    a crest line with a flat top wide enough for guns; steep outer side, shorter inner side.
  ramp     a corridor that overrides the ground with a smooth climb. It cuts through a rim and fills across low ground.
  platform a small flat gun position on a ridge top: flat enough for field defenses, and what the defense points use.
  wash     a shallow dry riverbed.
  dune     a wind-built sand ridge: long gentle windward side, shorter lee side.
  basin    a low hollow with a soft rim (oasis, pans).
  well     a small dry well pit.

All positions are in the start's local frame: (x, y) cm around the deploy centre, +x points at the map centre and +y is
to the left when looking at it. Every landform keeps the build disc (radius BUILD_FLAT) exactly flat, so a player base
or a Soviet base fits on every location. Terrain that must stay drivable is only ever made by pads and ramps; every
cliff band is steeper than the navmesh's AgentMaxSlope (44 deg), so vehicles use the ramps.
"""
import math

import numpy as np

BUILD_FLAT = 8400.0  # exactly flat radius around every deploy centre (build radius 8000 + margin)
NAV_MAX_SLOPE = 44.0


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


def fbm(x, y, wavelength, salt, octaves=3):
    """Fractal value noise, roughly in [-1, 1]."""
    total = np.zeros_like(x, dtype=float)
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(x / wavelength, y / wavelength, salt + 17 * octave)
        norm += amplitude
        amplitude *= 0.5
        wavelength *= 0.5
    return total / norm


def ridged(x, y, wavelength, salt, octaves=2):
    """Ridged noise in [0, 1]: sharp crests, wide valleys. Cuts gullies into a scarp."""
    return 1.0 - np.abs(fbm(x, y, wavelength, salt, octaves))


def catmull_rom(points, closed=False, samples=14):
    """Resamples control points into a smooth polyline."""
    pts = [np.array(p, dtype=float) for p in points]
    if closed:
        pts = [pts[-1]] + pts + [pts[0], pts[1]]
    else:
        pts = [2 * pts[0] - pts[1]] + pts + [2 * pts[-1] - pts[-2]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for k in range(samples):
            t = k / samples
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    if not closed:
        out.append(pts[-2])
    return np.array(out)


def resample_attribute(values, count):
    """Spreads per control point values over a resampled polyline of `count` points."""
    xs = np.linspace(0.0, len(values) - 1.0, count)
    return np.interp(xs, np.arange(len(values)), np.array(values, dtype=float))


def polygon_sdf(px, py, poly):
    """Signed distance to a closed polygon, negative inside."""
    ax, ay = poly[:, 0], poly[:, 1]
    bx, by = np.roll(ax, -1), np.roll(ay, -1)
    best = np.full(px.shape, 1e18)
    inside = np.zeros(px.shape, dtype=bool)
    for i in range(len(ax)):
        ex, ey = bx[i] - ax[i], by[i] - ay[i]
        wx, wy = px - ax[i], py - ay[i]
        t = np.clip((wx * ex + wy * ey) / max(ex * ex + ey * ey, 1e-9), 0.0, 1.0)
        qx, qy = wx - t * ex, wy - t * ey
        best = np.minimum(best, qx * qx + qy * qy)
        crossing = ((ay[i] <= py) != (by[i] <= py)) & (px < (bx[i] - ax[i]) * (py - ay[i]) /
                                                       (by[i] - ay[i] + 1e-9) + ax[i])
        inside ^= crossing
    dist = np.sqrt(best)
    return np.where(inside, -dist, dist)


def polyline_frame(px, py, line):
    """Per point: arc length of the closest spot, signed side distance (+ = left of travel), distance, segment index."""
    seg = line[1:] - line[:-1]
    lengths = np.hypot(seg[:, 0], seg[:, 1])
    cum = np.concatenate([[0.0], np.cumsum(lengths)])
    best = np.full(px.shape, 1e18)
    arc = np.zeros(px.shape)
    side = np.zeros(px.shape)
    for i in range(len(seg)):
        ex, ey = seg[i, 0], seg[i, 1]
        wx, wy = px - line[i, 0], py - line[i, 1]
        t = np.clip((wx * ex + wy * ey) / max(lengths[i] ** 2, 1e-9), 0.0, 1.0)
        qx, qy = wx - t * ex, wy - t * ey
        d2 = qx * qx + qy * qy
        closer = d2 < best
        best = np.where(closer, d2, best)
        arc = np.where(closer, cum[i] + t * lengths[i], arc)
        cross = (ex * wy - ey * wx) / max(lengths[i], 1e-9)
        side = np.where(closer, cross, side)
    return arc, side, np.sqrt(best), cum[-1]


# Scarp profiles: distance from the rim (0..1 of the scarp width) -> how far the ground has fallen (0..1). The cliff core
# falls 0.62 of the drop in 0.20 of the width. Widths are SCARP_FACTOR times the local drop, so the core is about 61 deg
# whatever the drop (at least ~54 deg with the width noise): steeper than the navmesh's 44 deg, so only ramps lead up.
SCARP_T = [0.0, 0.03, 0.06, 0.26, 0.31, 0.46, 0.70, 1.0]
SCARP_P = [0.0, 0.015, 0.06, 0.68, 0.72, 0.86, 0.955, 1.0]
BLUFF_T = [0.0, 0.05, 0.25, 0.42, 1.0]
BLUFF_P = [0.0, 0.05, 0.70, 0.86, 1.0]
SCARP_FACTOR = 1.5
MIN_DROP = 200.0  # very low steps keep a real width
SOFT_STEP = 340.0  # below this a rim is a soft grade, not a cliff
SOFT_WIDTH = 2200.0


def scarp_profile(t, kind="scarp"):
    t = np.clip(t, 0.0, 1.0)
    if kind == "scarp":
        return np.interp(t, SCARP_T, SCARP_P)
    if kind == "bluff":
        return np.interp(t, BLUFF_T, BLUFF_P)
    return t * t * (3.0 - 2.0 * t)  # "soft": a plain grade


def ramp_profile(t):
    """Climb with eased ends so the ramp meets plain and plateau without a kink."""
    return t + (smoothstep(0.0, 1.0, t) - t) * 0.55


class Landform:
    """Applies one start's operations on the height window of that start and records masks and dressing data."""

    def __init__(self, gen, start, spec, salt):
        self.gen = gen
        self.start = start
        self.spec = spec
        self.salt = salt
        half = spec.get("window", 33000.0)
        c = gen.c
        i0 = max(0, int((start["x"] - half + gen.half) / c["quad_cm"]))
        i1 = min(gen.verts, int((start["x"] + half + gen.half) / c["quad_cm"]))
        j0 = max(0, int((start["y"] - half + gen.half) / c["quad_cm"]))
        j1 = min(gen.verts, int((start["y"] + half + gen.half) / c["quad_cm"]))
        self.win = (slice(j0, j1), slice(i0, i1))
        self.px = gen.X[self.win]
        self.py = gen.Y[self.win]
        # Large warp bends the outlines, mid warp makes bays and headlands, small warp frays them. Flat tops are not
        # affected, only where their rim runs.
        s = salt
        px, py = self.px, self.py
        self.wx = px + 520.0 * fbm(px, py, 7000.0, s + 1) + 300.0 * fbm(px, py, 2600.0, s + 12, 2) + 110.0 * fbm(px, py, 1000.0, s + 2)
        self.wy = py + 520.0 * fbm(px, py, 7000.0, s + 3) + 300.0 * fbm(px, py, 2600.0, s + 13, 2) + 110.0 * fbm(px, py, 1000.0, s + 4)
        self.fray = 130.0 * fbm(px, py, 900.0, s + 5, 3) - 260.0 * ridged(px, py, 2100.0, s + 6) ** 4
        self.record = dict(start=start["index"], name=spec["name"], pads=[], ridges=[], ramps=[], washes=[], wells=[],
                           platforms=[], basins=[])

    def world(self, lx, ly):
        return self.gen.local_to_world(self.start, lx, ly)

    def line(self, pts, closed=False, samples=14):
        return catmull_rom([self.world(x, y) for x, y in pts], closed, samples)

    def rim_width(self, h, top, factor=1.0, spread=0.22):
        """Scarp width from the local drop, so every cliff core stays steeper than the navmesh limit.
        A slow noise makes the same scarp sheer on one side and broken on the other."""
        drop = np.maximum(np.abs(h - top), MIN_DROP)
        slow = 1.0 + 0.28 * fbm(self.px, self.py, 11000.0, self.salt + 8, 1)
        return SCARP_FACTOR * factor * drop * slow * (1.0 + spread * fbm(self.px, self.py, 3600.0, self.salt + 7, 2))

    def over(self, h, top, width, dist, kind="scarp"):
        """Composites `top` over the ground: inside (dist <= 0) it is top, beyond dist = width the ground is untouched.
        A step too low to carry real rock is no step at all: it becomes a soft grade."""
        if kind == "soft":
            fall = scarp_profile(np.maximum(dist, 0.0) / width, kind)
            return top + (h - top) * fall
        self.low_step = 1.0 - smoothstep(SOFT_STEP * 0.7, SOFT_STEP, np.abs(h - top))
        hard = scarp_profile(np.maximum(dist, 0.0) / width, kind)
        soft = scarp_profile(np.maximum(dist, 0.0) / SOFT_WIDTH, "soft")
        fall = hard + (soft - hard) * self.low_step
        return top + (h - top) * fall

    def pad_top(self, op):
        """Pad height; a shelf with `dip` tilts along an axis, so its far end sinks to the desert and the rim pinches out."""
        if "dip" not in op:
            return float(op["z"])
        (ax, ay), (bx, by), z_far = op["dip"]
        ax, ay = self.world(ax, ay)
        bx, by = self.world(bx, by)
        ex, ey = bx - ax, by - ay
        t = np.clip(((self.px - ax) * ex + (self.py - ay) * ey) / (ex * ex + ey * ey), 0.0, 1.0)
        return op["z"] + (z_far - op["z"]) * t

    def apply_pad(self, h, op):
        poly = self.line(op["outline"], closed=True)
        sdf = polygon_sdf(self.wx, self.wy, poly)
        d = sdf + op.get("fray", 1.0) * self.fray
        profile = op.get("profile", "scarp")
        self.low_step = np.zeros_like(h)
        top = self.pad_top(op)
        width = np.full(h.shape, op["scarp"]) if profile == "soft" else self.rim_width(h, top, op.get("steep", 1.0))
        out = self.over(h, top, width, d, profile)
        inside = d < 0.0
        if op.get("paint"):
            edge = op.get("paint_edge", 700.0)
            self.gen.paint_masks[op["paint"]][self.win] = np.maximum(self.gen.paint_masks[op["paint"]][self.win],
                                                                     1.0 - smoothstep(-edge, 200.0, d))
        self.gen.pad_mask[self.win] = np.maximum(self.gen.pad_mask[self.win], inside.astype(float))
        rock_band = (d > -200.0) & (d < width * 0.62) & (profile != "soft") & (self.low_step < 0.5)
        self.gen.rock_mask[self.win] = np.maximum(self.gen.rock_mask[self.win], rock_band.astype(float))
        self.record["pads"].append(dict(name=op["name"], z=op["z"], outline=poly[::3].tolist(), build=op.get("build", False)))
        return out, inside

    def apply_ridge(self, h, op):
        line = self.line(op["points"])
        arc, side, dist, length = polyline_frame(self.wx, self.wy, line)
        crest = np.interp(arc / max(length, 1.0), np.linspace(0, 1, len(op["crest"])), np.array(op["crest"], dtype=float))
        half_width = np.interp(arc / max(length, 1.0), np.linspace(0, 1, len(op["crest"])),
                               np.array(op.get("half_width", [750.0] * len(op["crest"])), dtype=float))
        # The crest wanders in width and height, so it reads as rock and not as a road; gun platforms stay on it.
        half_width = half_width * (1.0 + 0.3 * fbm(self.px, self.py, 2200.0, self.salt + 21, 2)) + 0.35 * self.fray
        crest = crest + 38.0 * fbm(self.px, self.py, 1900.0, self.salt + 22, 2)
        dd = dist - half_width
        # `out` is the side of the crest that faces away from the base: the steep one
        left_is_out = op.get("out_side", 1) > 0
        is_out = (side > 0) == left_is_out
        width = np.where(is_out, self.rim_width(h, crest, 1.0), self.rim_width(h, crest, 0.85))
        out = self.over(h, crest, width, dd, "scarp")
        # Ends: the crest sinks into the ground over the last stretch rather than ending in a wall.
        self.gen.rock_mask[self.win] = np.maximum(self.gen.rock_mask[self.win], ((dd > -150.0) & (dd < width * 0.62) & (self.low_step < 0.5)).astype(float))
        self.gen.ridge_mask[self.win] = np.maximum(self.gen.ridge_mask[self.win], (dd <= 0.0).astype(float))
        self.record["ridges"].append(dict(name=op["name"], line=line[::3].tolist(), crest=op["crest"]))
        return out

    def ground_at(self, h, x, y, radius=500.0):
        near = np.hypot(self.px - x, self.py - y) < radius
        return float(h[near].mean()) if near.any() else float(self.gen.c["base_z"])

    def apply_ramp(self, h, op):
        """A road cut into the land: cut side steep with a rock wall, fill side a soft embankment, foot fanned out."""
        line = self.line(op["points"])
        arc, side, dist, length = polyline_frame(self.px, self.py, line)
        z0 = op["z_from"] if not isinstance(op["z_from"], str) else self.ground_at(h, line[0, 0], line[0, 1])
        z1 = op["z_to"] if not isinstance(op["z_to"], str) else self.ground_at(h, line[-1, 0], line[-1, 1])
        from_ground = isinstance(op["z_from"], str) and not op.get("full", False)
        # From the desert the road follows the ground and only climbs over the last stretch, so it raises no tongue
        # of embankment across the plain. The climb starts where the rise really begins, at about 11 deg.
        climb = min(length, max(2400.0, abs(z1 - z0) * 5.2)) if from_ground else length
        start = length - climb
        t = np.clip((arc - start) / max(climb, 1.0), 0.0, 1.0)
        road = z0 + (z1 - z0) * ramp_profile(t)
        half_width = op.get("half_width", 700.0)
        if from_ground:
            half_width = half_width * (1.0 + op.get("fan", 0.85) * (1.0 - t) ** 2.0)  # the foot fans out into the plain
        fill = road - h
        base_side = op.get("side", 500.0)
        # Cut: the wall takes the difference in a short steep run. Fill: a long soft shoulder, so no thin dark sliver.
        side_width = np.where(fill > 0.0, np.clip(2.6 * fill, base_side, 1500.0), np.clip(0.75 * np.abs(fill), 320.0, base_side * 1.4))
        # A slow wobble breaks a straight cut into a few chutes instead of teeth.
        side_width = side_width * (1.0 + 0.45 * fbm(self.px, self.py, 1800.0, self.salt + 31, 2))
        lateral = dist + 110.0 * fbm(self.px, self.py, 1500.0, self.salt + 11, 2) * smoothstep(half_width * 0.7, half_width, dist)
        mask = 1.0 - smoothstep(half_width, half_width + side_width, lateral)
        # Fade in at the foot only: the head runs into a pad of the same height, so nothing can show there.
        foot = smoothstep(start - 300.0, start + 1100.0, arc) if from_ground else smoothstep(0.0, 250.0, arc)
        mask = mask * foot
        core = dist < half_width
        wall = (fill < -160.0) & (dist >= half_width - 40.0) & (dist < half_width + side_width + 80.0) & (foot > 0.3)
        self.gen.rock_mask[self.win] = np.maximum(self.gen.rock_mask[self.win], wall.astype(float))
        self.gen.cut_mask[self.win] = np.maximum(self.gen.cut_mask[self.win], wall.astype(float))
        self.gen.ramp_mask[self.win] = np.maximum(self.gen.ramp_mask[self.win], (core & (arc > 30.0) & (arc < length - 30.0)).astype(float))
        self.gen.road_mask[self.win] = np.maximum(self.gen.road_mask[self.win], mask * (~wall))
        # Round 6: the road paint dissolves over the last ~28 m on the pad with a ragged edge, so no square tab shows.
        wear = 1.0 - smoothstep(length - 2800.0, length - 200.0, arc + 700.0 * fbm(self.px, self.py, 1100.0, self.salt + 33, 2))
        self.gen.road_paint[self.win] = np.maximum(self.gen.road_paint[self.win], mask * (~wall) * wear)
        # A low berm along the fill edge, the spoil of the cut.
        berm = 26.0 * np.exp(-((dist - half_width - 120.0) / 170.0) ** 2) * (fill > 40.0) * foot
        heading = np.degrees(np.arctan2(line[-1, 1] - line[0, 1], line[-1, 0] - line[0, 0]))
        self.record["ramps"].append(dict(name=op["name"], line=line[::3].tolist(), z_from=z0, z_to=z1, length=length,
                                         half_width=op.get("half_width", 700.0), heading=float(heading),
                                         gate=op.get("gate", False)))
        return h + (road - h) * mask + berm

    def apply_platform(self, h, op):
        x, y = self.world(*op["at"])
        radius = op.get("radius", 1200.0)
        z = op.get("z", self.ground_at(h, x, y, 450.0))
        rng = np.random.default_rng(self.salt + int(abs(x) + abs(y)))
        spokes = [(x + radius * (0.8 + 0.4 * rng.random()) * math.cos(k * math.pi / 4.0),
                   y + radius * (0.8 + 0.4 * rng.random()) * math.sin(k * math.pi / 4.0)) for k in range(8)]
        poly = catmull_rom(spokes, True, 6)
        d = polygon_sdf(self.px, self.py, poly)
        out = self.over(h, z, op.get("blend", 520.0), d, "soft")
        inside = d < 0.0
        self.gen.platform_mask[self.win] = np.maximum(self.gen.platform_mask[self.win], inside.astype(float))
        self.gen.ridge_mask[self.win] = np.maximum(self.gen.ridge_mask[self.win], inside.astype(float))
        self.record["platforms"].append(dict(name=op["name"], x=x, y=y, z=z))
        return out

    def apply_wash(self, h, op):
        line = self.line(op["points"])
        arc, side, dist, length = polyline_frame(self.wx, self.wy, line)
        half_width = op.get("half_width", 500.0)
        depth = op.get("depth", 110.0)
        ends = smoothstep(0.0, 1500.0, arc) * smoothstep(0.0, 1500.0, length - arc)
        bed = 1.0 - smoothstep(half_width * 0.5, half_width * 1.8, dist)
        self.gen.paint_masks["wash"][self.win] = np.maximum(self.gen.paint_masks["wash"][self.win], bed * ends)
        self.record["washes"].append(dict(name=op["name"], line=line[::3].tolist()))
        return h - depth * bed * ends

    def apply_dune(self, h, op):
        line = self.line(op["points"])
        arc, side, dist, length = polyline_frame(self.wx, self.wy, line)
        crest = op.get("height", 240.0)
        along = np.sin(np.clip(arc / max(length, 1.0), 0.0, 1.0) * math.pi) ** 0.7
        lee = (side > 0.0) == (op.get("lee_side", 1) > 0)
        reach = np.where(lee, crest * 1.7, crest * 8.0)
        # Crescent lobes along the crest and a sharp slip face: dunes, not a smooth bank.
        lobes = 0.75 + 0.25 * fbm(self.px, self.py, 2600.0, self.salt + 41, 2)
        rise = np.clip(1.0 - dist / reach, 0.0, 1.0) ** np.where(lee, 1.0, 1.7) * crest * along * lobes
        self.gen.paint_masks["dune"][self.win] = np.maximum(self.gen.paint_masks["dune"][self.win], np.clip(rise / (0.35 * crest), 0.0, 1.0))
        return h + rise

    def apply_basin(self, h, op):
        poly = self.line(op["outline"], closed=True)
        sdf = polygon_sdf(self.wx, self.wy, poly)
        d = sdf + op.get("fray", 0.6) * self.fray
        fall = smoothstep(-op.get("slope", 1500.0), op.get("rim", 1800.0), d)
        out = op["z"] + (h - op["z"]) * fall
        self.gen.paint_masks["damp"][self.win] = np.maximum(self.gen.paint_masks["damp"][self.win], 1.0 - smoothstep(200.0, 3800.0, d))
        self.record["basins"].append(dict(name=op["name"], z=op["z"], outline=poly[::2].tolist(), water_z=op.get("water_z")))
        return out

    def apply_well(self, h, op):
        x, y = self.world(*op["at"])
        radius = op.get("radius", 380.0)
        d = np.hypot(self.px - x, self.py - y) / radius
        bowl = -op.get("depth", 150.0) * (1.0 - smoothstep(0.35, 1.0, d))
        rim = 22.0 * np.exp(-((d - 1.15) / 0.3) ** 2)
        self.record["wells"].append(dict(x=x, y=y, radius=radius))
        self.gen.paint_masks["wash"][self.win] = np.maximum(self.gen.paint_masks["wash"][self.win], 1.0 - smoothstep(0.8, 2.2, d))
        return h + bowl + rim

    def run(self, h):
        z_main = None
        for op in self.spec["ops"]:
            kind = op["op"]
            if kind == "pad":
                h, inside = self.apply_pad(h, op)
                if op.get("build"):
                    z_main = op["z"]
            elif kind == "ridge":
                h = self.apply_ridge(h, op)
            elif kind == "ramp":
                h = self.apply_ramp(h, op)
            elif kind == "platform":
                h = self.apply_platform(h, op)
            elif kind == "wash":
                h = self.apply_wash(h, op)
            elif kind == "dune":
                h = self.apply_dune(h, op)
            elif kind == "basin":
                h = self.apply_basin(h, op)
            elif kind == "well":
                h = self.apply_well(h, op)
            else:
                raise ValueError(kind)
        if z_main is not None:
            d = np.hypot(self.px - self.start["x"], self.py - self.start["y"])
            # The deploy centre and the whole build disc stay exactly one height, whatever the warps did near it.
            h = np.where(d <= BUILD_FLAT, z_main, h)
            self.gen.disc_mask[self.win] = np.maximum(self.gen.disc_mask[self.win], (d <= BUILD_FLAT).astype(float))
        self.record["z_main"] = z_main
        return h
