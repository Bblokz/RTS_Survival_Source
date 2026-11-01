"""Round 8 (owner: "shrink EVERYTHING to one third of the area"): scales a finished SM_Desert_01 generator output down.

Run offline with Python 3 + numpy:  python survival_scale.py <source gen dir> <out dir> [report.txt]

What it does, in order (all lengths in cm, world XY scaled about the map origin, which is the centre of the start ring):
  1. Removes the oasis from the source heights and paint (harmonic fill), so no shrunk copy of it is left behind.
  2. Resamples heights and paint onto a 14 x 63 quad landscape (88 200 cm) at p = q / XY_SCALE.
     Heights: Z' = BASE_Z + (Z - BASE_Z) * Z_SCALE (owner: "lower them partly"), so slopes come out ~1.3x steeper.
  3. Re-stamps every ramp at the new scale: half width never below MIN_RAMP_HALF_WIDTH (heavy tanks), gates regraded so
     the climb is at least DESIGN_RUN x the height (the foot is extended into the desert where the shrunk line is too short).
  4. Pastes the oasis back UNSCALED at its own centre from SM_Desert_01_oasis_r4.npz (basin, shore and the bank where the
     palms stand, full to OASIS_FULL_SD beyond the water line, faded out by OASIS_FADE_SD): the shrunk landforms give way.
  5. Forces every build disc exactly flat to FLAT_RADIUS at the scaled deploy Z.
  6. Holds the landscape's outer band at EDGE_Z so the horizon skirt still meets it.
  7. Writes raws, a scaled layout json (dress lists thinned, z from the new ground), nav points for the -game checks and
     a report: gate slope audit, oasis clearances, disc flatness.
"""
import json
import math
import os
import sys

import numpy as np

import survival_landforms as lf
import survival_landforms_desert01 as desert01

XY_SCALE = math.sqrt(1.0 / 3.0)   # 0.577: one third of the area
Z_SCALE = 0.75
BASE_Z = 600.0
COMPONENTS = 14                    # 14 * 63 = 882 quads -> 88 200 cm, 883^2 vertices
QUADS = 63
QUAD_CM = 100.0
FLAT_RADIUS = 5700.0               # owner: bases need build radius 5 300, flat to 5 700 (was 8 400 at full size)
SRC_FLAT = 8400.0
# Islands grow only by what the bigger disc needs: inside the disc the source is scaled by FLAT_RADIUS / SRC_FLAT, outside it
# by XY_SCALE again (the landform keeps its look, offset outwards by GROWTH). From ISLAND_FULL to ISLAND_FADE the island
# mapping fades back into the plain map scale.
GROWTH = FLAT_RADIUS - SRC_FLAT * math.sqrt(1.0 / 3.0)          # 850 cm
RING_PUSH = GROWTH / math.sin(math.radians(36.0))               # keeps every gap between neighbouring islands as wide
ISLAND_FULL = 11000.0
ISLAND_FADE = 14000.0
MIN_RAMP_HALF_WIDTH = 620.0        # the narrowest ramp before the scale-down; heavy tanks are 400 cm wide
MIN_PLATFORM_RADIUS = 800.0
DESIGN_RUN = 5.6                   # gate climb >= 5.6 x height: mean ~10 deg, steepest part (smoothstep x1.5) ~15 deg
EDGE_Z = 470.0
# Round 9 (owner: "shrink the oasis a bit"): the oasis at OASIS_SCALE about its own centre, heights about the water level
# scaled by Z_SCALE like the rest of the map; palms and plants move in with it (editor script).
OASIS_SCALE = 0.75
OASIS_CENTRE = (15500.0, -3600.0)
WATER_Z = 372.0
OASIS_FULL_SD = 2700.0 * OASIS_SCALE   # palms stood up to 2 465 beyond the water outline, now 1 850
OASIS_FADE_SD = 2600.0
# Drivable banks: outside the oasis, ground that is not a base pad (below BANK_MAX_Z) is graded down to the oasis edge at
# BANK_SLOPE_DEG at most, out to BANK_REACH; higher pads keep their cliffs (no back door into a base).
BANK_SLOPE_DEG = 16.0
BANK_REACH = 4500.0
BANK_MAX_Z = 1300.0
SRC_HALF = 75600.0
SRC_RADIUS = 40000.0
# (start, name, world heading of the road from the pad outwards, deg)
# Round 9: the smaller oasis gives Red Mesa its bench_road back, so north_road is gone; Dry Wells' wash_ford head still lies
# inside the oasis, so east_road stays in its place.
NEW_GATES = [(4, "east_road", 0.0)]
if os.environ.get("SCALE_NEW_GATES") == "0":
    NEW_GATES = []


def scale_z(z):
    return BASE_Z + (z - BASE_Z) * Z_SCALE


def bilinear(grid, half, x, y):
    """Samples a [row=y, col=x] grid with corner -half and QUAD_CM spacing at world (x, y) arrays (clamped)."""
    n = grid.shape[0]
    fx = np.clip((x + half) / QUAD_CM, 0.0, n - 1.000001)
    fy = np.clip((y + half) / QUAD_CM, 0.0, n - 1.000001)
    i0 = np.floor(fx).astype(int)
    j0 = np.floor(fy).astype(int)
    tx, ty = fx - i0, fy - j0
    a = grid[j0, i0] * (1 - tx) + grid[j0, i0 + 1] * tx
    b = grid[j0 + 1, i0] * (1 - tx) + grid[j0 + 1, i0 + 1] * tx
    return a * (1 - ty) + b * ty


def box_blur(a, radius):
    """Square mean filter (summed-area table)."""
    h, w = a.shape
    sat = np.zeros((h + 1, w + 1))
    sat[1:, 1:] = a.cumsum(0).cumsum(1)
    y0 = np.clip(np.arange(h) - radius, 0, h)
    y1 = np.clip(np.arange(h) + radius + 1, 0, h)
    x0 = np.clip(np.arange(w) - radius, 0, w)
    x1 = np.clip(np.arange(w) + radius + 1, 0, w)
    area = ((y1 - y0)[:, None] * (x1 - x0)[None, :]).astype(float)
    return (sat[y1][:, x1] - sat[y0][:, x1] - sat[y1][:, x0] + sat[y0][:, x0]) / area


def harmonic_fill(a, hole, iterations=3000):
    """Fills a[hole] smoothly from its border (Jacobi on the bounding window)."""
    js, is_ = np.where(hole)
    j0, j1, i0, i1 = max(js.min() - 2, 0), js.max() + 3, max(is_.min() - 2, 0), is_.max() + 3
    w = a[j0:j1, i0:i1].copy()
    m = hole[j0:j1, i0:i1]
    w[m] = w[~m].mean()
    for _ in range(iterations):
        avg = 0.25 * (np.roll(w, 1, 0) + np.roll(w, -1, 0) + np.roll(w, 1, 1) + np.roll(w, -1, 1))
        w[m] = avg[m]
    out = a.copy()
    out[j0:j1, i0:i1] = w
    return out


class Scaler:
    def __init__(self, src, out):
        self.src, self.out = src, out
        self.L0 = json.load(open(os.path.join(src, "SM_Desert_01_layout.json")))
        n0 = self.L0["landscape"]["vertices"]
        self.H0 = (np.fromfile(os.path.join(src, "SM_Desert_01_height.raw"), dtype="<u2").reshape(n0, n0).astype(float)
                   - 32768.0) * 100.0 / 128.0
        self.layers = list(self.L0["files"]["layers"].keys())
        self.W0 = {n: np.fromfile(os.path.join(src, "SM_Desert_01_%s.raw" % n), dtype=np.uint8).reshape(n0, n0).astype(float)
                   for n in self.layers}
        c0 = -SRC_HALF + np.arange(n0) * QUAD_CM
        self.X0, self.Y0 = np.meshgrid(c0, c0)
        self.n = COMPONENTS * QUADS + 1
        self.half = (self.n - 1) * QUAD_CM / 2.0
        c = -self.half + np.arange(self.n) * QUAD_CM
        self.X, self.Y = np.meshgrid(c, c)
        basin = [o for o in desert01.WORLD["ops"] if o.get("name") == "oasis"][0]
        self.basin_poly_src = np.array(basin["outline"], dtype=float)
        c = np.array(OASIS_CENTRE)
        self.basin_poly = c + (self.basin_poly_src - c) * OASIS_SCALE
        self.report = []
        self.ramp_mask = np.zeros_like(self.X)

    def log(self, text):
        print(text)
        self.report.append(text)

    def setup_mapping(self):
        self.src_starts = [(s["x"], s["y"]) for s in self.L0["starts"]]
        self.new_centres = []
        for x, y in self.src_starts:
            r = math.hypot(x, y)
            k = (r * XY_SCALE + RING_PUSH) / r
            self.new_centres.append((x * k, y * k))
        self.ring_radius = SRC_RADIUS * XY_SCALE + RING_PUSH
        self.log("start ring r %.0f (plain scale %.0f, pushed %.0f so the islands, %.0f cm larger in radius, keep their gaps)" % (
            self.ring_radius, SRC_RADIUS * XY_SCALE, RING_PUSH, GROWTH))

    def back(self, qx, qy):
        """New world (qx, qy) -> source world (px, py): plain 1/XY_SCALE between islands, island-aware around each start."""
        qx = np.asarray(qx, dtype=float)
        qy = np.asarray(qy, dtype=float)
        px, py = qx / XY_SCALE, qy / XY_SCALE
        best_w = np.zeros_like(qx)
        bx, by = px.copy(), py.copy()
        for (sx, sy), (cx, cy) in zip(self.src_starts, self.new_centres):
            dx, dy = qx - cx, qy - cy
            rho = np.hypot(dx, dy)
            g = np.where(rho <= FLAT_RADIUS, rho * SRC_FLAT / FLAT_RADIUS, SRC_FLAT + (rho - FLAT_RADIUS) / XY_SCALE)
            safe = np.maximum(rho, 1e-6)
            ix, iy = sx + g * dx / safe, sy + g * dy / safe
            w = 1.0 - lf.smoothstep(ISLAND_FULL, ISLAND_FADE, rho)
            take = w > best_w
            bx = np.where(take, ix, bx)
            by = np.where(take, iy, by)
            best_w = np.where(take, w, best_w)
        return px * (1.0 - best_w) + bx * best_w, py * (1.0 - best_w) + by * best_w

    def fwd(self, x, y):
        """Source world -> new world (fixed-point inverse of back())."""
        x = np.asarray(x, dtype=float)
        y = np.asarray(y, dtype=float)
        qx, qy = x * XY_SCALE, y * XY_SCALE
        # Damped: in the islands' fade band back() stretches up to ~3.7/cm, an undamped step would oscillate.
        for _ in range(160):
            bx, by = self.back(qx, qy)
            qx, qy = qx + (x - bx) * XY_SCALE * 0.35, qy + (y - by) * XY_SCALE * 0.35
        return qx, qy

    def f1(self, x, y):
        a, b = self.fwd(np.array([x]), np.array([y]))
        return float(a[0]), float(b[0])

    # 1 ------------------------------------------------------------------------------------------------------------------
    def remove_source_oasis(self):
        """Round 9: only the source basin and shore are filled (the benches, knolls and fords round it stay in the source so
        Red Mesa and Dry Wells get them back); the fill gets the desert's dune relief, and the old oasis's wet paint round it
        is replaced by the open desert's mix, so no smooth or green ghost of it is left."""
        sd = lf.polygon_sdf(self.X0, self.Y0, self.basin_poly_src)
        hole = sd < 3000.0
        self.H0 = harmonic_fill(self.H0, hole)
        relief = 95.0 * lf.fbm(self.X0, self.Y0, 5200.0, 401, 3) + 30.0 * lf.fbm(self.X0, self.Y0, 1700.0, 402, 2)
        feather = 1.0 - lf.smoothstep(2200.0, 3000.0, sd)
        self.H0 = self.H0 + relief * feather
        gy, gx = np.gradient(self.H0, QUAD_CM)
        flat = (np.degrees(np.arctan(np.hypot(gx, gy))) < 9.0) & (self.H0 < 820.0)
        paint_zone = (sd < 5800.0) & flat
        ring = (sd > 6000.0) & (sd < 9000.0) & flat
        n = 0.5 + 0.5 * lf.fbm(self.X0, self.Y0, 3200.0, 403, 3)
        n2 = 0.5 + 0.5 * lf.fbm(self.X0, self.Y0, 1300.0, 404, 2)
        mix = {k: float(self.W0[k][ring].mean()) for k in self.layers}
        target = {k: np.full_like(self.H0, mix[k]) for k in self.layers}
        target["Weedy"] = mix["Weedy"] * 0.4 * (0.6 + 0.8 * lf.smoothstep(0.55, 0.85, n2))
        target["DesertGravel"] = mix["DesertGravel"] * (0.6 + 0.8 * n)
        total = sum(target.values())
        w = (1.0 - lf.smoothstep(4800.0, 5800.0, sd)) * paint_zone
        for k in self.layers:
            t = target[k] / np.maximum(total, 1e-6) * 255.0
            self.W0[k] = self.W0[k] * (1.0 - w) + t * w
        self.log("source oasis removed: %d cells filled with dune relief, %d cells repainted as open desert" % (
            int(hole.sum()), int((w > 0.01).sum())))

    # 2 ------------------------------------------------------------------------------------------------------------------
    def resample(self):
        px, py = self.back(self.X, self.Y)
        self.Z = scale_z(bilinear(self.H0, SRC_HALF, px, py))
        self.W = {n: bilinear(self.W0[n], SRC_HALF, px, py) for n in self.layers}

    # 3 ------------------------------------------------------------------------------------------------------------------
    def ramps(self):
        self.ramp_rows = []
        self.dropped = []
        forms = self.L0["features"]["landforms"] + self.L0["features"]["world"]
        for form in forms:
            for r in form["ramps"]:
                self.stamp_ramp(form, r)
        # Replacement gates for the ones the oasis took (S3 bench_road; S4 wash_ford and long_road): straight roads out of
        # the main pad, pointing away from the oasis and from the remaining gates.
        for start, name, angle in NEW_GATES:
            st = self.L0["starts"][start]
            (sx, sy), sz = self.new_centres[start], scale_z(st["z"])
            d = np.array([math.cos(math.radians(angle)), math.sin(math.radians(angle))])
            head = np.array([sx, sy]) + d * (FLAT_RADIUS - 200.0)
            run = (sz - 560.0) * DESIGN_RUN + 2500.0
            pts = [head + d * run * (1.0 - k / 8.0) for k in range(9)]
            form = dict(start=start)
            r = dict(name=name, line=[list(p) for p in pts], half_width=MIN_RAMP_HALF_WIDTH / XY_SCALE,
                     z_from=600.0, z_to=st["z"], gate=True, mapped=True)
            self.stamp_ramp(form, r)
            if self.ramp_rows and self.ramp_rows[-1]["name"] == name:
                self.ramp_rows[-1]["added"] = True
        # Second pass: gates are stamped again last, so no later road, knoll or shelf side slope steepens them.
        rows, self.ramp_rows = self.ramp_rows, []
        for row in rows:
            if not row["gate"]:
                self.ramp_rows.append(row)
                continue
            r = dict(name=row["name"], line=[list(q) for q in row["line"]], half_width=row["hw"] / XY_SCALE, gate=True,
                     z_from=600.0, z_to=(row["z_to"] - BASE_Z) / Z_SCALE + BASE_Z, mapped=True)
            self.stamp_ramp(dict(start=row["start"]), r)
            self.ramp_rows[-1]["added"] = row.get("added", False)
            self.ramp_rows[-1]["note"] = row.get("note", "")
            self.ramp_rows[-1]["extended"] = row["extended"]

    def stamp_ramp(self, form, r):
        src = np.array(r["line"], dtype=float)[:, :2]
        pts = src if r.get("mapped") else np.stack(self.fwd(src[:, 0], src[:, 1]), axis=1)
        # densify the recorded (every 3rd) points
        dense = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            k = max(int(np.hypot(*(b - a)) / 150.0), 1)
            for t in np.linspace(0, 1, k + 1)[1:]:
                dense.append(a + (b - a) * t)
        line = np.array(dense)
        hw = max(r["half_width"] * XY_SCALE, MIN_RAMP_HALF_WIDTH)
        z_to = scale_z(r["z_to"])
        gate = bool(r.get("gate"))
        note = ""
        # The oasis keeps its size: a ramp whose head now lies in the oasis zone is dropped; one that only passes through
        # it is swung about its head, by the smallest angle that keeps the whole road clear of the zone.
        clear = OASIS_FADE_SD + hw
        sd_line = lf.polygon_sdf(line[:, 0], line[:, 1], self.basin_poly)
        if sd_line[-1] < OASIS_FADE_SD:
            self.dropped.append((form.get("start"), r["name"], float(sd_line[-1])))
            return
        if sd_line.min() < clear:
            head = line[-1]
            best = None
            for deg in sorted(set(list(range(-120, 121, 5))), key=abs):
                c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
                rel = line - head
                rot = head + np.stack([rel[:, 0] * c - rel[:, 1] * s_, rel[:, 0] * s_ + rel[:, 1] * c], axis=1)
                if lf.polygon_sdf(rot[:, 0], rot[:, 1], self.basin_poly).min() >= clear:
                    best = (deg, rot)
                    break
            if best is None and not gate:
                self.dropped.append((form.get("start"), r["name"], float(sd_line.min())))
                return
            if best is None:
                note = "foot runs to the oasis edge (kept clear by the oasis mask)"
            else:
                line = best[1]
                note = "swung %+d deg clear of the oasis" % best[0]
        length = float(np.sum(np.hypot(*np.diff(line, axis=0).T)))
        extended = 0.0
        if gate:
            # The climb starts at the lowest ground on the way in (or earlier when that leaves too short a climb), so the
            # road rises steadily to the pad and cuts through anything higher on the way.
            arcs = np.arange(0.0, length * 0.85, 100.0)
            grounds = np.array([self.ground_on_line(line, a_) for a_ in arcs])
            arc_low = float(arcs[int(np.argmin(grounds))])
            need = max((z_to - float(grounds.min())) * DESIGN_RUN, 1400.0)
            start = min(arc_low, length - need)
            if start < 0.0:
                extended = -start + 300.0
                d = line[0] - line[1]
                d = d / np.hypot(*d)
                line = np.vstack([line[0] + d * extended, line])
                length += extended
                start = 300.0
            z_from = self.ground_on_line(line, start)
            climb = length - start
        else:
            z_from = scale_z(r["z_from"])
            start, climb = 0.0, length
        x0, x1 = line[:, 0].min() - hw - 2500, line[:, 0].max() + hw + 2500
        y0, y1 = line[:, 1].min() - hw - 2500, line[:, 1].max() + hw + 2500
        i0, i1 = int((x0 + self.half) / QUAD_CM), int((x1 + self.half) / QUAD_CM) + 2
        j0, j1 = int((y0 + self.half) / QUAD_CM), int((y1 + self.half) / QUAD_CM) + 2
        i0, j0 = max(i0, 0), max(j0, 0)
        win = (slice(j0, min(j1, self.n)), slice(i0, min(i1, self.n)))
        px, py = self.X[win], self.Y[win]
        arc, side, dist, _ = lf.polyline_frame(px, py, line)
        t = np.clip((arc - start) / max(climb, 1.0), 0.0, 1.0)
        h = self.Z[win]
        if gate:
            ground = h
            road = np.where(arc < start, ground, z_from + (z_to - z_from) * lf.ramp_profile(t))
            foot = lf.smoothstep(start - 300.0, start + 900.0, arc)
        else:
            road = z_from + (z_to - z_from) * lf.ramp_profile(t)
            foot = lf.smoothstep(0.0, 200.0, arc)
        fill = road - h
        side_w = np.where(fill > 0.0, np.clip(2.2 * fill, 350.0, 1300.0), np.clip(0.8 * np.abs(fill), 300.0, 700.0))
        # The oasis (basin, shore and the palm bank) stays exactly as it is: ramps stop at its edge.
        keep = lf.smoothstep(OASIS_FULL_SD - 200.0, OASIS_FULL_SD + 200.0, lf.polygon_sdf(px, py, self.basin_poly))
        mask = (1.0 - lf.smoothstep(hw, hw + side_w, dist)) * foot * keep
        self.Z[win] = h + (road - h) * mask
        core = (dist < hw).astype(float) * foot
        self.ramp_mask[win] = np.maximum(self.ramp_mask[win], core)
        paint = mask * (1.0 - lf.smoothstep(length - 1500.0, length, arc))
        self.W["DesertRockDark"][win] *= (1.0 - 0.9 * paint)
        self.W["DesertGravel"][win] = np.maximum(self.W["DesertGravel"][win], 255.0 * 0.45 * paint)
        self.ramp_rows.append(dict(start=form.get("start"), name=r["name"], gate=gate, line=line, hw=hw, z_from=z_from,
                                   z_to=z_to, length=length, extended=extended, half_width_src=r["half_width"], note=note))

    def ground_on_line(self, line, arc_at):
        seg = np.hypot(*np.diff(line, axis=0).T)
        cum = np.concatenate([[0.0], np.cumsum(seg)])
        k = int(np.clip(np.searchsorted(cum, arc_at) - 1, 0, len(seg) - 1))
        t = (arc_at - cum[k]) / max(seg[k], 1e-6)
        p = line[k] + (line[k + 1] - line[k]) * np.clip(t, 0.0, 1.0)
        return self.ground(p[0], p[1])

    # 4 ------------------------------------------------------------------------------------------------------------------
    def oasis(self):
        patch = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), "SM_Desert_01_oasis_r4.npz"))
        px0, py0 = float(patch["x0"]), float(patch["y0"])
        h, w = patch["height"].shape
        cx, cy = OASIS_CENTRE
        # the patch's footprint after the scale-down about the oasis centre
        x0, x1 = cx + (px0 - cx) * OASIS_SCALE, cx + (px0 + (w - 1) * QUAD_CM - cx) * OASIS_SCALE
        y0, y1 = cy + (py0 - cy) * OASIS_SCALE, cy + (py0 + (h - 1) * QUAD_CM - cy) * OASIS_SCALE
        i0, i1 = int(np.ceil((x0 + self.half) / QUAD_CM)), int(np.floor((x1 + self.half) / QUAD_CM))
        j0, j1 = int(np.ceil((y0 + self.half) / QUAD_CM)), int(np.floor((y1 + self.half) / QUAD_CM))
        win = (slice(j0, j1 + 1), slice(i0, i1 + 1))
        qx, qy = self.X[win], self.Y[win]
        sx, sy = cx + (qx - cx) / OASIS_SCALE, cy + (qy - cy) / OASIS_SCALE

        def sample(grid):
            fx = np.clip((sx - px0) / QUAD_CM, 0, w - 1.000001)
            fy = np.clip((sy - py0) / QUAD_CM, 0, h - 1.000001)
            ii, jj = np.floor(fx).astype(int), np.floor(fy).astype(int)
            tx, ty = fx - ii, fy - jj
            a = grid[jj, ii] * (1 - tx) + grid[jj, ii + 1] * tx
            b = grid[jj + 1, ii] * (1 - tx) + grid[jj + 1, ii + 1] * tx
            return a * (1 - ty) + b * ty

        sd = lf.polygon_sdf(qx, qy, self.basin_poly)
        t = np.clip((OASIS_FADE_SD - sd) / (OASIS_FADE_SD - OASIS_FULL_SD), 0.0, 1.0)
        blend = t * t * (3.0 - 2.0 * t)
        old = (patch["height"].astype(float) - 32768.0) * 100.0 / 128.0
        old = WATER_Z + (sample(old) - WATER_Z) * Z_SCALE
        self.land_before_oasis = self.Z.copy()
        self.Z[win] = self.Z[win] * (1.0 - blend) + old * blend
        for n in self.layers:
            self.W[n][win] = self.W[n][win] * (1.0 - blend) + sample(patch[n].astype(float)) * blend
        self.oasis_blend = np.zeros_like(self.X)
        self.oasis_blend[win] = blend
        self.log("oasis pasted at %.2f of its size about (%.0f, %.0f), heights x%.2f about the water (Z %.0f): full to %.0f, "
                 "faded by %.0f beyond the water line" % (OASIS_SCALE, OASIS_CENTRE[0], OASIS_CENTRE[1], Z_SCALE, WATER_Z,
                                                         OASIS_FULL_SD, OASIS_FADE_SD))

    def banks(self):
        """Grades the land round the oasis into drivable banks (non-pad ground only)."""
        cx, cy = OASIS_CENTRE
        r = 15000.0
        win = (slice(int((cy - r + self.half) / QUAD_CM), int((cy + r + self.half) / QUAD_CM)),
               slice(int((cx - r + self.half) / QUAD_CM), int((cx + r + self.half) / QUAD_CM)))
        sd = lf.polygon_sdf(self.X[win], self.Y[win], self.basin_poly)
        Z = self.Z[win]
        # edge height of the oasis, carried outwards (each step takes the mean of the known neighbours)
        edge = np.where((sd > OASIS_FULL_SD - 250.0) & (sd <= OASIS_FULL_SD), Z, np.nan)
        known = ~np.isnan(edge)
        e = np.where(known, edge, 0.0)
        for _ in range(int(BANK_REACH / QUAD_CM) + 10):
            num = np.zeros_like(e)
            cnt = np.zeros_like(e)
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                num += np.roll(np.roll(e * known, dj, 0), di, 1)
                cnt += np.roll(np.roll(known.astype(float), dj, 0), di, 1)
            grow = (~known) & (cnt > 0) & (sd > OASIS_FULL_SD)
            e = np.where(grow, num / np.maximum(cnt, 1), e)
            known = known | grow
        e = np.where(known, box_blur(np.where(known, e, 0.0), 6) / np.maximum(box_blur(known.astype(float), 6), 1e-3), e)
        cap = e + (sd - OASIS_FULL_SD) * math.tan(math.radians(BANK_SLOPE_DEG))
        land = self.land_before_oasis[win]
        weight = (sd > OASIS_FULL_SD) & known & (land <= BANK_MAX_Z)
        fade = 1.0 - lf.smoothstep(BANK_REACH - 1500.0, BANK_REACH, sd - OASIS_FULL_SD)
        cut = box_blur(np.maximum(Z - cap, 0.0) * weight * fade, 4)
        cut = cut * (sd > OASIS_FULL_SD)
        self.Z[win] = Z - cut
        carved = np.clip(cut / 150.0, 0.0, 1.0)
        for n in self.layers:
            target = {"Sand": 0.72, "DesertGravel": 0.22, "Weedy": 0.06}.get(n, 0.0) * 255.0
            self.W[n][win] = self.W[n][win] * (1.0 - carved) + target * carved
        self.log("banks: %d cells graded down to <= %.0f deg (max cut %.0f cm)" % (int((cut > 20).sum()), BANK_SLOPE_DEG, cut.max()))

    # 5 ------------------------------------------------------------------------------------------------------------------
    def discs(self):
        self.new_starts = []
        for s in self.L0["starts"]:
            (x, y), z = self.new_centres[s["index"]], scale_z(s["z"])
            r = np.hypot(self.X - x, self.Y - y)
            blend = 1.0 - lf.smoothstep(FLAT_RADIUS + 50.0, FLAT_RADIUS + 400.0, r)
            self.Z = self.Z * (1.0 - blend) + z * blend
            self.Z[r <= FLAT_RADIUS + 50.0] = z
            inside = self.Z[r <= FLAT_RADIUS]
            self.new_starts.append(dict(index=s["index"], name=s["name"], x=x, y=y, z=z))
            self.log("start %d %-15s (%.0f, %.0f) z %.1f (was %.1f): disc z range %.3f to r %.0f, oasis blend on disc %.2f" % (
                s["index"], s["name"], x, y, z, s["z"], inside.max() - inside.min(), FLAT_RADIUS,
                self.oasis_blend[r <= FLAT_RADIUS].max()))

    # 6 ------------------------------------------------------------------------------------------------------------------
    def edge(self):
        d = np.minimum(np.minimum(self.X + self.half, self.half - self.X), np.minimum(self.Y + self.half, self.half - self.Y))
        t = 1.0 - lf.smoothstep(600.0, 1200.0, d)
        self.Z = self.Z * (1.0 - t) + EDGE_Z * t

    # 7 ------------------------------------------------------------------------------------------------------------------
    def ground(self, x, y):
        return float(bilinear(self.Z, self.half, np.array([x]), np.array([y]))[0])

    def slope_deg(self):
        gy, gx = np.gradient(self.Z, QUAD_CM)
        return np.degrees(np.arctan(np.hypot(gx, gy)))

    def gate_audit(self):
        slope = self.slope_deg()
        self.log("ramp audit (scaled): name, gate, half width src -> new, length, extension, p95 / max slope on the core")
        rows = []
        for r in self.ramp_rows:
            line = r["line"]
            x0, x1 = line[:, 0].min() - r["hw"], line[:, 0].max() + r["hw"]
            y0, y1 = line[:, 1].min() - r["hw"], line[:, 1].max() + r["hw"]
            win = (slice(int((y0 + self.half) / QUAD_CM), int((y1 + self.half) / QUAD_CM) + 2),
                   slice(int((x0 + self.half) / QUAD_CM), int((x1 + self.half) / QUAD_CM) + 2))
            arc, side, dist, length = lf.polyline_frame(self.X[win], self.Y[win], line)
            core = (dist < max(r["hw"] - 150.0, 100.0)) & (arc > 100.0) & (arc < length - 100.0)
            s = slope[win][core]
            p95, mx = (float(np.percentile(s, 95)), float(s.max())) if s.size else (0.0, 0.0)
            rows.append(dict(start=r["start"], name=r["name"], gate=r["gate"], hw=r["hw"], length=r["length"],
                             extended=r["extended"], p95=p95, max=mx, z_from=r["z_from"], z_to=r["z_to"],
                             foot=[float(line[0, 0]), float(line[0, 1])], head=[float(line[-1, 0]), float(line[-1, 1])],
                             added=bool(r.get("added"))))
            self.log("  S%s %-18s gate=%d hw %4.0f -> %4.0f  len %5.0f  ext %5.0f  slope p95 %4.1f max %4.1f  z %5.0f -> %5.0f %s" % (
                r["start"], r["name"], r["gate"], r["half_width_src"], r["hw"], r["length"], r["extended"], p95, mx,
                r["z_from"], r["z_to"], r["note"]))
        for d_ in self.dropped:
            self.log("  S%s %-18s DROPPED: its head is %.0f from the oasis water line (inside the kept oasis)" % d_)
        gates = [r for r in rows if r["gate"]]
        self.log("gates: p95 %.1f..%.1f deg (round 6 band 3.9..16.3)" % (min(r["p95"] for r in gates), max(r["p95"] for r in gates)))
        self.ramp_audit = rows
        return rows

    def oasis_clearance(self, palms):
        self.log("oasis clearances (cm; water outline / nearest palm):")
        P = np.array(palms, dtype=float)
        for s in self.new_starts:
            sdw = float(lf.polygon_sdf(np.array([s["x"]]), np.array([s["y"]]), self.basin_poly)[0]) - FLAT_RADIUS
            dp = float(np.hypot(P[:, 0] - s["x"], P[:, 1] - s["y"]).min()) - FLAT_RADIUS
            self.log("  S%d disc edge: water %6.0f, palm %6.0f%s" % (s["index"], sdw, dp, "   CROWDED" if dp < 3000 else ""))
        for r in self.ramp_audit:
            line = np.array([r["foot"], r["head"]])
            sdw = float(lf.polygon_sdf(line[:, 0], line[:, 1], self.basin_poly).min())
            if sdw < 6000:
                self.log("  ramp S%s %s end within %.0f of the water" % (r["start"], r["name"], sdw))

    def write(self, palms):
        os.makedirs(self.out, exist_ok=True)
        hraw = np.clip(np.round(32768.0 + self.Z * 128.0 / 100.0), 0, 65535).astype("<u2")
        hraw.tofile(os.path.join(self.out, "SM_Desert_01_height.raw"))
        stack = np.stack([np.clip(self.W[n], 0, None) for n in self.layers])
        total = stack.sum(0)
        total[total <= 0] = 1.0
        q = np.floor(stack / total * 255.0 + 0.5).astype(np.int32)
        diff = 255 - q.sum(0)
        strongest = np.argmax(q, axis=0)
        np.put_along_axis(q, strongest[None], np.take_along_axis(q, strongest[None], 0) + diff[None], 0)
        q = np.clip(q, 0, 255).astype(np.uint8)
        files = {}
        for k, n in enumerate(self.layers):
            path = os.path.join(self.out, "SM_Desert_01_%s.raw" % n).replace("\\", "/")
            q[k].tofile(path)
            files[n] = path
        self.write_layout(files, palms)
        open(os.path.join(self.out, "scale_report.txt"), "w").write("\n".join(self.report) + "\n")

    # layout -------------------------------------------------------------------------------------------------------------
    def xy(self, p):
        x, y = self.f1(p[0], p[1])
        return [x, y] + list(p[2:])

    def scale_feature(self, f, keep=False):
        if keep:
            return f
        out = {}
        for k, v in f.items():
            if k == "x" and "y" in f:
                out["x"], out["y"] = self.f1(f["x"], f["y"])
            elif k == "y" and "x" in f:
                pass
            elif k in ("z", "z_from", "z_to", "z_main", "crest") and isinstance(v, (int, float)):
                out[k] = scale_z(v)
            elif k in ("points", "line", "outline") and isinstance(v, list):
                out[k] = [self.xy(p) for p in v]
            elif k == "at" and isinstance(v, (list, tuple)):
                out[k] = self.xy(v)
            elif k in ("radius",) and isinstance(v, (int, float)):
                out[k] = max(v * XY_SCALE, MIN_PLATFORM_RADIUS)
            elif k in ("length", "side") and isinstance(v, (int, float)):
                out[k] = v * XY_SCALE
            elif isinstance(v, list) and v and isinstance(v[0], dict):
                out[k] = [self.scale_feature(e) for e in v]
            else:
                out[k] = v
        return out

    def thin(self, items, spacing):
        kept = []
        cell = {}
        for it in items:
            gx, gy = int(it["x"] // spacing), int(it["y"] // spacing)
            near = [cell.get((gx + a, gy + b)) for a in (-1, 0, 1) for b in (-1, 0, 1)]
            if any(o is not None and math.hypot(o["x"] - it["x"], o["y"] - it["y"]) < spacing for o in near):
                continue
            cell[(gx, gy)] = it
            kept.append(it)
        return kept

    def write_layout(self, files, palms):
        L0 = self.L0
        L = dict(L0)
        L["landscape"] = dict(L0["landscape"], corner=[-self.half, -self.half], size_cm=2 * self.half, vertices=self.n,
                              components=COMPONENTS, quads_per_component=QUADS)
        starts = []
        for s, ns in zip(L0["starts"], self.new_starts):
            e = dict(s)
            e.update(x=ns["x"], y=ns["y"], z=ns["z"], flat_radius=FLAT_RADIUS,
                     resource_ring=[v * XY_SCALE for v in s["resource_ring"]], wreck_ring=[v * XY_SCALE for v in s["wreck_ring"]],
                     edge_clearance=s["edge_clearance"] * XY_SCALE)
            dps = []
            for d in s["defense_points"]:
                x, y = self.f1(d["x"], d["y"])
                if lf.polygon_sdf(np.array([x]), np.array([y]), self.basin_poly)[0] < OASIS_FADE_SD + 200.0:
                    self.log("  S%d defense point %s dropped: inside the kept oasis" % (s["index"], d["name"]))
                    continue
                dps.append(dict(d, x=x, y=y, z=self.ground(x, y), distance=d["distance"] * XY_SCALE))
            e["defense_points"] = dps
            starts.append(e)
        L["starts"] = starts
        L["edge_zones"] = [dict(z, x=z["x"] * XY_SCALE, y=z["y"] * XY_SCALE, z=self.ground(z["x"] * XY_SCALE, z["y"] * XY_SCALE),
                                length=z["length"] * XY_SCALE) for z in L0["edge_zones"]]
        feats = dict(L0["features"])
        ramp_by_name = {(r["start"], r["name"]): r for r in self.ramp_audit}
        forms = []
        for form in L0["features"]["landforms"]:
            f = self.scale_feature(form)
            kept = []
            for rr in form["ramps"]:
                a = ramp_by_name.get((form["start"], rr["name"]))
                if a is not None:
                    kept.append(dict(rr, line=[list(p) for p in a["foot_line"]], half_width=a["hw"], z_from=a["z_from"],
                                     z_to=a["z_to"], length=a["length"]))
            for a in self.ramp_audit:
                if a.get("added") and a["start"] == form["start"]:
                    kept.append(dict(name=a["name"], line=[list(p) for p in a["foot_line"]], half_width=a["hw"], z_from=a["z_from"],
                                     z_to=a["z_to"], length=a["length"], heading=0.0, gate=True))
            f["ramps"] = kept
            if "no_fields" in form:
                # offsets from the deploy centre (world axes), not world points
                st0 = self.L0["starts"][form["start"]]
                c1 = self.new_centres[form["start"]]
                f["no_fields"] = []
                for x, y, r in form["no_fields"]:
                    wx, wy = self.f1(st0["x"] + x, st0["y"] + y)
                    f["no_fields"].append([wx - c1[0], wy - c1[1], r * XY_SCALE])
            forms.append(f)
        feats["landforms"] = forms
        world = []
        for form in L0["features"]["world"]:
            f = self.scale_feature(form)
            f["basins"] = form["basins"]  # the oasis keeps its place (round 9: at OASIS_SCALE, see L["oasis"])
            world.append(f)
        feats["world"] = world
        craters = []
        for c in L0["features"]["craters"]:
            x, y = self.f1(c["x"], c["y"])
            if lf.polygon_sdf(np.array([x]), np.array([y]), self.basin_poly)[0] < OASIS_FADE_SD + 1500:
                continue
            craters.append(dict(c, x=x, y=y, radius=max(c["radius"] * XY_SCALE, 450.0), depth=c["depth"] * Z_SCALE))
        feats["craters"] = self.thin(craters, 2500.0)
        L["features"] = feats
        p = L0["playable"]
        L["playable"] = dict(x0=p["x0"] * XY_SCALE, x1=p["x1"] * XY_SCALE, y0=p["y0"] * XY_SCALE, y1=p["y1"] * XY_SCALE)
        dress = dict(L0["dress"])

        def moved(items, extra=None):
            out = []
            for it in items:
                x, y = self.f1(it["x"], it["y"])
                if lf.polygon_sdf(np.array([x]), np.array([y]), self.basin_poly)[0] < OASIS_FADE_SD + 500:
                    continue
                e = dict(it, x=x, y=y, z=self.ground(x, y))
                if extra:
                    extra(e, it)
                out.append(e)
            return out

        def face(e, it):
            e["drop"] = it["drop"] * Z_SCALE
            e["length"] = it["length"] * XY_SCALE
        dress["faces"] = self.thin(moved(L0["dress"]["faces"], face), 600.0)
        dress["foot"] = self.thin(moved(L0["dress"]["foot"]), 700.0)
        dress["tops"] = self.thin(moved(L0["dress"]["tops"]), 700.0)
        dress["terrace"] = self.thin(moved(L0["dress"]["terrace"]), 800.0)
        dress["open"] = self.thin(moved(L0["dress"]["open"]), 1100.0)
        L["dress"] = dress
        self.log("dress after thinning: faces %d (was %d), foot %d (%d), tops %d (%d), terrace %d (%d), open %d (%d), craters %d (%d)" % (
            len(dress["faces"]), len(L0["dress"]["faces"]), len(dress["foot"]), len(L0["dress"]["foot"]), len(dress["tops"]),
            len(L0["dress"]["tops"]), len(dress["terrace"]), len(L0["dress"]["terrace"]), len(dress["open"]), len(L0["dress"]["open"]),
            len(feats["craters"]), len(L0["features"]["craters"])))
        sd = L0["salt_decal"]
        L["salt_decal"] = dict(centre=list(self.f1(sd["centre"][0], sd["centre"][1])), side=sd["side"] * XY_SCALE)
        lines = []
        for ln in L0["power_lines"]:
            poles, last = [], None
            for k, pole in enumerate(ln["poles"]):
                x, y = self.f1(pole["x"], pole["y"])
                end = k == 0 or k == len(ln["poles"]) - 1
                if not (pole.get("corner") or end) and last is not None and math.hypot(x - last[0], y - last[1]) < 2600.0:
                    continue
                poles.append(dict(pole, x=x, y=y, z=self.ground(x, y)))
                last = (x, y)
            lines.append(dict(ln, poles=poles))
        L["power_lines"] = lines
        L["files"] = dict(height=os.path.join(self.out, "SM_Desert_01_height.raw").replace("\\", "/"), layers=files)
        L["oasis"] = dict(centre=list(OASIS_CENTRE), scale=OASIS_SCALE, water_z=WATER_Z, outline=self.basin_poly.tolist())
        L["scale"] = dict(xy=XY_SCALE, z=Z_SCALE, base_z=BASE_Z, flat_radius=FLAT_RADIUS, min_ramp_half_width=MIN_RAMP_HALF_WIDTH,
                          ring_radius=self.ring_radius, ring_push=RING_PUSH, island_growth=GROWTH)
        # Route centre lines (desert01.ROUTE_LINES) through the same mapping, for the editor's route splines and dust.
        L["routes"] = {name: [list(self.f1(x, y)) for x, y in pts] for name, pts in desert01.ROUTE_LINES.items()}
        json.dump(L, open(os.path.join(self.out, "SM_Desert_01_layout.json"), "w"), indent=1, default=float)


def main():
    src, out = sys.argv[1], sys.argv[2]
    palms_file = sys.argv[3] if len(sys.argv) > 3 else None
    palms = json.load(open(palms_file)) if palms_file else [[15500.0, -3600.0]]
    palms = [[OASIS_CENTRE[0] + (x - OASIS_CENTRE[0]) * OASIS_SCALE, OASIS_CENTRE[1] + (y - OASIS_CENTRE[1]) * OASIS_SCALE] for x, y in palms]
    s = Scaler(src, out)
    s.setup_mapping()
    s.remove_source_oasis()
    s.resample()
    s.oasis()
    s.banks()
    s.ramps()
    s.discs()
    s.edge()
    rows = s.gate_audit()
    for r, rr in zip(rows, s.ramp_rows):
        r["foot_line"] = rr["line"][::3].tolist() + [rr["line"][-1].tolist()]
    s.oasis_clearance(palms)
    s.write(palms)
    json.dump(rows, open(os.path.join(out, "ramp_audit.json"), "w"), indent=1, default=lambda o: o.tolist() if hasattr(o, "tolist") else float(o))
    print("wrote", out)


if __name__ == "__main__":
    main()
