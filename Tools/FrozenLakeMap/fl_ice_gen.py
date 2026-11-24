"""Ice data for Ger_FrozenLake: one resolved ice-type grid -> lake mesh, lead water, state mask, nav boxes, checks.

    python Source/Tools/FrozenLakeMap/fl_ice_gen.py      (after fl_terrain_gen.py; reads the same sketch)

Everything comes from ONE grid of ice types, painted in the sketch's region order (a later region covers an earlier
one, so the heavy ice road stays thick where it crosses a black vein finger):
    heavy  (T = 1)    lake_ice, inlet_ice, forge_road_ice, heavy_road_ice, rig_pad: holds everything
    medium (T = 0.5)  thin_*: medium and light tanks and infantry (no heavy tanks)
    black  (T = 0)    vein_*: infantry only
    open              the steam leads and pump 1's outflow lead: holes in the ice mesh, nobody crosses

Writes Saved/FrozenLake/ice/map/
    FL_IceLake.obj        the walkable ice: the lake outline pushed TUCK under the shore, minus the leads
    FL_IceLeadWater.obj   water surface under the lead holes (no collision, no navigation)
    T_FL_IceState.png     state mask for M_FrozenLakeIce (FrozenLakeIce.md): R thickness, G warn (0), B open water,
                          A vein. u = world X (north, image right), v = world Y (east, image down). Linear, clamped.
    FL_IceNav.json        nav boxes per area (rectangles on the 1 m grid) + the state mask's LakeOrigin/LakeSize
    FL_IceCheck.txt       per unit class reach from the HQ and path lengths on the class's ice
    preview_ice.png       the type grid, the mesh outline and the nav boxes
"""
import heapq
import json
import math
import os
import sys

import numpy as np
from PIL import Image
import cv2
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt   # noqa: E402
import matplotlib.tri as mtri     # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fl_terrain_gen as tg       # noqa: E402

OUT = os.path.join(tg.ROOT, "Saved", "FrozenLake", "ice", "map")
TUCK = 150.0                  # ice reaches this far under the shore (matches fl_map_build.ICE_TUCK_CM)
MASK_PX = 1024
MASK_MARGIN = 1000.0          # cm round the lake in the state mask
INTERIOR_STEP = 800.0         # interior mesh vertex spacing (cm)
EDGE_STEP = 100.0             # outline vertex spacing (cm)
B_RIM = 300.0                 # open-water channel ramps 0 -> 1 over 2 x this across the lead edge (0.5 at the cut)
VEIN_SIGMA = 200.0            # cm blur of the vein channel
T_SIGMA = 80.0                # cm blur of the thickness channel (keeps the band borders on the grid borders)
GROW_INFANTRY = 100.0         # black ice blocks vehicles this far past its edge (light agent radius is 120 cm)
GROW_MEDIUM = 100.0           # medium ice blocks heavies this far past its edge
GROW_LEAD = 150.0
NAV_BLOCK = 2                 # nav boxes snap to 2 m blocks (spill at most 1 m past the area)             # leads block everyone this far past the hole

THICK_IDS = ("lake_ice", "inlet_ice", "forge_road_ice", "heavy_road_ice", "rig_pad")
LEAD_IDS = ("lead_finger", "lead_ne", "outflow_lead")

# nav areas (Blueprint meta areas made by fl_map_build.step_ice_nav): agent order Character, LightTank, MediumTank,
# HeavyTank (DefaultEngine.ini SupportedAgents)
NAV_AREAS = {
    "NoHeavy": {"z": (-50.0, 150.0), "agents_blocked": ["HeavyTank"]},
    "InfantryOnly": {"z": (-50.0, 150.0), "agents_blocked": ["LightTank", "MediumTank", "HeavyTank"]},
    "Lead": {"z": (-800.0, 300.0), "agents_blocked": ["Character", "LightTank", "MediumTank", "HeavyTank"]},
}


def type_value(reg_id):
    if reg_id in THICK_IDS:
        return 1.0
    if reg_id.startswith("thin_"):
        return 0.5
    if reg_id.startswith("vein_"):
        return 0.0
    return None


def blur_cm(a, sigma_cm):
    s = sigma_cm / tg.QUAD_CM
    return cv2.GaussianBlur(a.astype(np.float32), (0, 0), s, borderType=cv2.BORDER_REPLICATE)


def grow(mask, cm):
    k = int(math.ceil(cm / tg.QUAD_CM))
    if k <= 0:
        return mask.copy()
    ker = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * k + 1, 2 * k + 1))
    return cv2.dilate(mask.astype(np.uint8), ker).astype(bool)


def rects(mask):
    """Exact rectangle cover of a cell mask: row runs merged downwards while the next row has the same run.
    Returns (r0, r1, c0, c1) inclusive cell ranges."""
    out = []
    open_runs = {}
    rows = mask.shape[0]
    for r in range(rows + 1):
        runs = set()
        if r < rows:
            row = mask[r]
            if row.any():
                d = np.diff(np.concatenate(([0], row.astype(np.int8), [0])))
                starts, ends = np.nonzero(d == 1)[0], np.nonzero(d == -1)[0] - 1
                runs = set(zip(starts.tolist(), ends.tolist()))
        nxt = {}
        for run, r0 in open_runs.items():
            if run in runs:
                nxt[run] = r0
            else:
                out.append((r0, r - 1, run[0], run[1]))
        for run in runs:
            if run not in nxt:
                nxt[run] = r
        open_runs = nxt
    return out


def rects_loose(mask, waste=0.10, ok=None):
    """Rectangle cover with fewer boxes: a row run joins the open box above it when the union stays at least
    (1 - waste) mask cells, so a blob's slowly changing rows share one box. Over-covers by at most `waste` per box
    (blocking areas: a little too much is safe). Returns (r0, r1, c0, c1) inclusive cell ranges."""
    ok = mask if ok is None else (ok | mask)
    out = []
    open_boxes = []                     # [r0, c0, c1, area_cells_in_mask, last_row]
    rows = mask.shape[0]
    for r in range(rows + 1):
        runs = []
        if r < rows and mask[r].any():
            d = np.diff(np.concatenate(([0], mask[r].astype(np.int8), [0])))
            runs = list(zip(np.nonzero(d == 1)[0].tolist(), (np.nonzero(d == -1)[0] - 1).tolist()))
        taken = set()
        nxt = []
        for a, b in runs:
            best = None
            for k, box in enumerate(open_boxes):
                if k in taken or box[4] != r - 1 or b < box[1] or a > box[2]:
                    continue
                c0, c1 = min(a, box[1]), max(b, box[2])
                h = r - box[0] + 1
                filled = box[3] + (b - a + 1)
                if c0 != box[1] or c1 != box[2]:
                    # widening the box also widens every row above it: recount those cells exactly
                    filled = int(mask[box[0]:r + 1, c0:c1 + 1].sum())
                if filled >= (1.0 - waste) * h * (c1 - c0 + 1) and ok[box[0]:r + 1, c0:c1 + 1].all():
                    best = (k, c0, c1, filled)
                    break
            if best is None:
                nxt.append([r, a, b, b - a + 1, r])
            else:
                k, c0, c1, filled = best
                taken.add(k)
                box = open_boxes[k]
                box[1], box[2], box[3], box[4] = c0, c1, filled, r
        for k, box in enumerate(open_boxes):
            if k in taken:
                nxt.append(box)
            else:
                out.append((box[0], box[4], box[1], box[2]))
        open_boxes = nxt
    return out


def rects_on_blocks(mask, harmless, cell):
    """rects_loose on cell x cell blocks: a block is covered when any of its 1 m cells is in the mask (so the cover
    spills up to cell - 1 m past the mask), and boxes only merge across blocks that are wholly mask or harmless.
    Returns 1 m cell ranges (r0, r1, c0, c1) inclusive."""
    h, w = mask.shape
    hh, ww = (h + cell - 1) // cell, (w + cell - 1) // cell
    def blocks(a, fn):
        pad = np.zeros((hh * cell, ww * cell), dtype=bool)
        pad[:h, :w] = a
        return fn(pad.reshape(hh, cell, ww, cell), axis=(1, 3))
    coarse = blocks(mask, np.any)
    ok = blocks(mask if harmless is None else (mask | harmless), np.all)
    return [(r0 * cell, min((r1 + 1) * cell - 1, h - 1), c0 * cell, min((c1 + 1) * cell - 1, w - 1))
            for r0, r1, c0, c1 in rects_loose(coarse, ok=ok)]


def rects_coarse(mask, cell):
    """Rectangle cover on a coarser grid (cell x cell 1 m cells, any covered cell marks the coarse cell): fewer,
    slightly larger boxes. Returns boxes in 1 m cell ranges (r0, r1, c0, c1) inclusive."""
    h, w = mask.shape
    hh, ww = (h + cell - 1) // cell, (w + cell - 1) // cell
    pad = np.zeros((hh * cell, ww * cell), dtype=bool)
    pad[:h, :w] = mask
    coarse = pad.reshape(hh, cell, ww, cell).any(axis=(1, 3))
    return [(r0 * cell, min((r1 + 1) * cell - 1, h - 1), c0 * cell, min((c1 + 1) * cell - 1, w - 1))
            for r0, r1, c0, c1 in rects(coarse)]


class IceGen:
    def __init__(self):
        g = tg.Gen()
        g.build_heights()
        self.g = g
        self.spec = g.spec
        self.X, self.Y = g.X, g.Y
        self.report = []

    # ---- signed distances (cm, positive inside) ---------------------------------------------------------------
    def sd_region(self, reg):
        m = self.g.masks[reg.id]
        if "line" in reg.raw:
            d = self.g.line_dist(reg.raw["line"], reach=tg.SIZE) - reg.raw["width"] / 2.0
            return -d
        pts = [tuple(p) for p in reg.geom.pts] + [tuple(reg.geom.pts[0])]
        d = self.g.line_dist(pts, reach=tg.SIZE)
        return np.where(m, d, -d)

    def build(self):
        g, spec = self.g, self.spec
        regs = {r.id: r for r in spec.regions}
        lake = g.lake
        self.lake = lake
        # thickness in region order
        T = np.ones(tg.SHAPE, dtype=np.float32)
        for reg in spec.regions:
            v = type_value(reg.id)
            if v is not None:
                T[g.masks[reg.id] & lake] = v
        vein = np.zeros(tg.SHAPE, dtype=bool)
        for reg in spec.regions:
            if reg.id.startswith("vein_"):
                vein |= g.masks[reg.id]
        self.T, self.vein = T, vein
        # exact signed distances
        sd_lake = np.maximum(self.sd_region(regs["lake_ice"]), self.sd_region(regs["inlet_ice"]))
        sd_lead = np.max([self.sd_region(regs[i]) for i in LEAD_IDS], axis=0)
        self.sd_lead = sd_lead
        self.sd_ice = np.minimum(sd_lake + TUCK, -sd_lead)
        self.sd_water = sd_lead + 200.0
        lead = sd_lead > 0
        self.lead = lead
        # nav masks (cells), exclusive, leads first
        lead_nav = grow(lead, GROW_LEAD)
        ice = lake & ~lead
        inf = grow(ice & (T < 0.25), GROW_INFANTRY) & lake & ~lead_nav
        noheavy = grow(ice & (T < 0.75), GROW_MEDIUM) & lake & ~lead_nav & ~inf
        self.nav_masks = {"Lead": lead_nav, "InfantryOnly": inf, "NoHeavy": noheavy}

    # ---- mesh ---------------------------------------------------------------------------------------------------
    def sample(self, a, xs, ys):
        """Bilinear sample of a grid array at world x, y (cm)."""
        r = (tg.SIZE / 2.0 - np.asarray(xs)) / tg.QUAD_CM
        c = (np.asarray(ys) + tg.SIZE / 2.0) / tg.QUAD_CM
        r0 = np.clip(np.floor(r).astype(int), 0, tg.V - 2)
        c0 = np.clip(np.floor(c).astype(int), 0, tg.V - 2)
        fr, fc = np.clip(r - r0, 0, 1), np.clip(c - c0, 0, 1)
        return (a[r0, c0] * (1 - fr) * (1 - fc) + a[r0 + 1, c0] * fr * (1 - fc) + a[r0, c0 + 1] * (1 - fr) * fc
                + a[r0 + 1, c0 + 1] * fr * fc)

    def contours(self, sd):
        import contourpy
        # contourpy wants increasing axes: columns = Y (east, increasing), rows flipped so X increases
        gen = contourpy.contour_generator(self.Y[0, :], self.X[::-1, 0], sd[::-1, :].astype(np.float64))
        loops = []
        for seg in gen.lines(0.0):
            if len(seg) < 4:
                continue
            ys, xs = seg[:, 0], seg[:, 1]
            # resample at EDGE_STEP
            d = np.concatenate(([0.0], np.cumsum(np.hypot(np.diff(xs), np.diff(ys)))))
            n = max(int(d[-1] / EDGE_STEP), 4)
            t = np.linspace(0.0, d[-1], n, endpoint=False)
            loops.append(np.stack([np.interp(t, d, xs), np.interp(t, d, ys)], axis=1))

        return loops

    def mesh(self, sd, name):
        loops = self.contours(sd)
        edge = np.concatenate(loops, axis=0)
        inside = sd > 0
        xs = np.arange(self.X.min(), self.X.max(), INTERIOR_STEP)
        ys = np.arange(self.Y.min(), self.Y.max(), INTERIOR_STEP)
        gx, gy = np.meshgrid(xs, ys, indexing="ij")
        gx, gy = gx.ravel(), gy.ravel()
        keep = self.sample(sd, gx, gy) > INTERIOR_STEP * 0.35
        pts = np.concatenate([edge, np.stack([gx[keep], gy[keep]], axis=1)], axis=0)
        tri = mtri.Triangulation(pts[:, 0], pts[:, 1])
        tris = tri.triangles
        a, b, c = pts[tris[:, 0]], pts[tris[:, 1]], pts[tris[:, 2]]
        ok = self.sample(sd, (a[:, 0] + b[:, 0] + c[:, 0]) / 3.0, (a[:, 1] + b[:, 1] + c[:, 1]) / 3.0) > 0
        for p, q in ((a, b), (b, c), (c, a)):
            ok &= self.sample(sd, (p[:, 0] + q[:, 0]) / 2.0, (p[:, 1] + q[:, 1]) / 2.0) > -25.0
        tris = tris[ok]
        used = np.unique(tris)
        remap = -np.ones(len(pts), dtype=int)
        remap[used] = np.arange(len(used))
        pts, tris = pts[used], remap[tris]
        path = os.path.join(OUT, name + ".obj")
        with open(path, "w") as h:
            h.write("# Ger_FrozenLake %s (fl_ice_gen.py): UE cm, x north, y written as -y (the OBJ import mirrors y)\n"
                    % name)
            for x, y in pts:
                h.write("v %.1f %.1f 0.0\n" % (x, -y))
            for x, y in pts:
                h.write("vt %.5f %.5f\n" % (x / 1000.0, y / 1000.0))
            for t in tris:
                p0, p1, p2 = pts[t[0]], pts[t[1]], pts[t[2]]
                cross = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0])
                i0, i1, i2 = (t[0] + 1, t[1] + 1, t[2] + 1) if cross > 0 else (t[0] + 1, t[2] + 1, t[1] + 1)
                # positive (x, y) orientation is written a, c, b: the placeholder fan's convention, checked in UE
                h.write("f %d/%d %d/%d %d/%d\n" % (i0, i0, i2, i2, i1, i1))
        area = float(np.abs(np.cross(pts[tris[:, 1]] - pts[tris[:, 0]], pts[tris[:, 2]] - pts[tris[:, 0]])).sum() / 2e4)
        self.report.append("%s: %d verts, %d tris, %d loops, %.0f m2" % (name, len(pts), len(tris), len(loops), area))
        return path, loops, (float(pts[:, 0].min()), float(pts[:, 0].max()), float(pts[:, 1].min()),
                             float(pts[:, 1].max()))

    # ---- state mask ---------------------------------------------------------------------------------------------
    def state_mask(self):
        lake = self.lake
        rr, cc = np.nonzero(lake)
        x0, x1 = self.X[rr, cc].min() - MASK_MARGIN, self.X[rr, cc].max() + MASK_MARGIN
        y0, y1 = self.Y[rr, cc].min() - MASK_MARGIN, self.Y[rr, cc].max() + MASK_MARGIN
        size = float(math.ceil(max(x1 - x0, y1 - y0) / 100.0) * 100.0)
        ox, oy = float(x0), float(y0)
        # thickness: fill outside the lake from the nearest lake value so the shore edge keeps its type
        T = self.T.copy()
        known = lake.astype(np.float32)
        val = np.where(lake, T, 0.0).astype(np.float32)
        for _ in range(4):
            num, den = blur_cm(val * known, 300.0), blur_cm(known, 300.0)
            fill = np.where(den > 1e-4, num / np.maximum(den, 1e-4), 1.0)
            val = np.where(known > 0, val, fill)
            known = np.maximum(known, (den > 1e-3).astype(np.float32))
        R = blur_cm(val, T_SIGMA)
        B = np.clip(0.5 + self.sd_lead / (2.0 * B_RIM), 0.0, 1.0)
        A = np.clip(blur_cm(self.vein.astype(np.float32), VEIN_SIGMA) * 1.6, 0.0, 1.0)
        # image: column i = world X (north to the right), row j = world Y (east down)
        i = (np.arange(MASK_PX) + 0.5) / MASK_PX * size
        wx, wy = np.meshgrid(ox + i, oy + i, indexing="xy")      # wx varies along columns, wy along rows
        img = np.zeros((MASK_PX, MASK_PX, 4), dtype=np.uint8)
        for k, ch in enumerate((R, np.zeros_like(R), B, A)):
            img[..., k] = np.clip(np.round(self.sample(ch, wx, wy) * 255.0), 0, 255).astype(np.uint8)
        path = os.path.join(OUT, "T_FL_IceState.png")
        Image.fromarray(img, "RGBA").save(path)
        self.mask_info = {"lake_origin": [ox, oy], "lake_size": size, "px": MASK_PX, "file": path.replace("\\", "/")}
        self.report.append("state mask origin x %.0f y %.0f size %.0f cm (%.1f cm/px)" % (ox, oy, size, size / MASK_PX))
        return path

    # ---- nav ----------------------------------------------------------------------------------------------------
    def nav_boxes(self):
        boxes = {}
        nm = self.nav_masks
        # a box may only spill onto cells already blocked for the same units (never onto heavy ice or the shore)
        harmless = {"Lead": None, "InfantryOnly": nm["Lead"], "NoHeavy": nm["Lead"] | nm["InfantryOnly"]}
        for name, m in nm.items():
            out = []
            for r0, r1, c0, c1 in rects_on_blocks(m, harmless[name], NAV_BLOCK):
                xa, xb = tg.SIZE / 2.0 - (r1 + 0.5) * tg.QUAD_CM, tg.SIZE / 2.0 - (r0 - 0.5) * tg.QUAD_CM
                ya, yb = -tg.SIZE / 2.0 + (c0 - 0.5) * tg.QUAD_CM, -tg.SIZE / 2.0 + (c1 + 0.5) * tg.QUAD_CM
                out.append([round((xa + xb) / 2.0, 1), round((ya + yb) / 2.0, 1),
                            round((xb - xa) / 2.0, 1), round((yb - ya) / 2.0, 1)])
            boxes[name] = out
            self.report.append("nav %-13s %4d boxes, %7.0f m2 of cells" % (name, len(out), m.sum()))
        self.boxes = boxes
        return boxes

    # ---- checks -------------------------------------------------------------------------------------------------
    def check(self):
        g = self.g
        Zn = np.where(self.lake & ~self.lead, 0.0, g.Z)
        limit = math.tan(math.radians(tg.NAV_MAX_SLOPE_DEG))
        hq = g.feature("hq").points[0]
        seed = g.rc(hq)
        q = lambda m: m[:-1, :-1] | m[1:, 1:] | m[:-1, 1:] | m[1:, :-1]
        # the hole: the bed under a lead is NOT walkable (Null box from the bed to over the ice)
        # check what the BOXES block (they over-cover the masks a little), not the masks
        cover = {name: self.box_cover(name) for name in self.boxes}
        base_block = q(g.cuts) | q(cover["Lead"])
        classes = {
            "infantry": base_block,
            "light/medium": base_block | q(cover["InfantryOnly"]),
            "heavy": base_block | q(cover["InfantryOnly"] | cover["NoHeavy"]),
        }
        hr = q(self.g.masks["heavy_road_ice"] & self.lake)
        self.report.append("heavy road cells blocked for heavies by box over-cover: %d of %d"
                           % (int((hr & classes["heavy"]).sum()), int(hr.sum())))
        feats = ("rig", "shield_garrison", "frozen_tiger", "soviet_airfield", "pump_s", "pump_w", "pump_n", "pump_e",
                 "spawn_n", "spawn_e", "spawn_s", "radixite_upwelling", "metal_ice", "metal_bay", "wreck_bay",
                 "radixite_seep", "reveal")
        self.reach = {}
        for cname, blocked in classes.items():
            reach = tg.tri_reach(Zn, ~blocked, seed, limit)
            self.reach[cname] = reach
            miss = []
            for fid in feats:
                r, c = g.rc(g.feature(fid).points[0])
                if not reach[max(r - 2, 0):r + 3, max(c - 2, 0):c + 3].any():
                    miss.append(fid)
            self.report.append("reach %-12s %s" % (cname, "all features" if not miss else "NOT: " + ", ".join(miss)))
            dist = self.dijkstra(reach, seed)
            for fid in ("shield_garrison", "rig", "spawn_s", "frozen_tiger"):
                r, c = g.rc(g.feature(fid).points[0])
                win = dist[max(r - 3, 0):r + 4, max(c - 3, 0):c + 4]
                d = float(win.min()) if np.isfinite(win).any() else float("inf")
                self.report.append("   hq -> %-16s %s" % (fid, "%.0f m" % (d / 100.0) if math.isfinite(d) else "none"))
        # a lead hole and its bed must be unreachable for everyone
        for lid in LEAD_IDS:
            reg = next(r for r in self.spec.regions if r.id == lid)
            m = g.masks[lid] & self.lake
            hit = int((self.reach["infantry"] & q(m)).sum())
            self.report.append("lead %-13s reached cells %d (want 0)" % (lid, hit))

    def box_cover(self, name):
        m = np.zeros(tg.SHAPE, dtype=bool)
        for x, y, ex, ey in self.boxes[name]:
            r0, c0 = self.g.rc((x + ex - 50.0, y - ey + 50.0))
            r1, c1 = self.g.rc((x - ex + 50.0, y + ey - 50.0))
            m[r0:r1 + 1, c0:c1 + 1] = True
        return m

    def dijkstra(self, walk, seed):
        h, w = walk.shape
        dist = np.full((h, w), np.inf)
        r0, c0 = min(seed[0], h - 1), min(seed[1], w - 1)
        if not walk[r0, c0]:
            return dist
        dist[r0, c0] = 0.0
        heap = [(0.0, r0, c0)]
        steps = [(-1, 0, 100.0), (1, 0, 100.0), (0, -1, 100.0), (0, 1, 100.0),
                 (-1, -1, 141.4), (-1, 1, 141.4), (1, -1, 141.4), (1, 1, 141.4)]
        while heap:
            d, r, c = heapq.heappop(heap)
            if d > dist[r, c]:
                continue
            for dr, dc, cost in steps:
                rr, cc = r + dr, c + dc
                if 0 <= rr < h and 0 <= cc < w and walk[rr, cc]:
                    nd = d + cost
                    if nd < dist[rr, cc]:
                        dist[rr, cc] = nd
                        heapq.heappush(heap, (nd, rr, cc))
        return dist

    # ---- output -------------------------------------------------------------------------------------------------
    def preview(self, ice_loops):
        img = np.zeros(tg.SHAPE + (3,), dtype=np.uint8)
        img[:] = (120, 104, 84)
        T = self.T
        img[self.lake & (T >= 0.75)] = (226, 234, 242)
        img[self.lake & (T < 0.75) & (T >= 0.25)] = (150, 165, 178)
        img[self.lake & (T < 0.25)] = (40, 50, 62)
        img[self.lead] = (40, 90, 130)
        tint = {"Lead": (255, 0, 255), "InfantryOnly": (255, 140, 0), "NoHeavy": (0, 200, 255)}
        for name, bx in self.boxes.items():
            for x, y, ex, ey in bx:
                r0, c0 = self.g.rc((x + ex, y - ey))
                r1, c1 = self.g.rc((x - ex, y + ey))
                cv2.rectangle(img, (int(c0), int(r0)), (int(c1), int(r1)), tint[name], 1)
        for loop in ice_loops:
            pts = np.array([self.g.rc(p)[::-1] for p in loop], dtype=np.int32)
            cv2.polylines(img, [pts], True, (255, 40, 40), 1)
        Image.fromarray(img).save(os.path.join(OUT, "preview_ice.png"))

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        self.build()
        ice_path, ice_loops, ice_bounds = self.mesh(self.sd_ice, "FL_IceLake")
        water_path, _, water_bounds = self.mesh(self.sd_water, "FL_IceLeadWater")
        self.state_mask()
        self.nav_boxes()
        self.check()
        self.preview(ice_loops)
        info = {"ice_obj": ice_path.replace("\\", "/"), "ice_bounds": ice_bounds,
                "water_obj": water_path.replace("\\", "/"), "water_bounds": water_bounds,
                "state_mask": self.mask_info, "nav_areas": NAV_AREAS, "nav_boxes": self.boxes,
                "report": self.report}
        with open(os.path.join(OUT, "FL_IceNav.json"), "w") as h:
            json.dump(info, h, indent=1)
        with open(os.path.join(OUT, "FL_IceCheck.txt"), "w") as h:
            h.write("\n".join(self.report) + "\n")
        print("\n".join(self.report))
        print("wrote %s" % OUT)


if __name__ == "__main__":
    IceGen().write()
