"""Terrain generator for Ger_Berlin: heights + paint layers + a resolved layout, from the MapDesigner sketch.

    python Source/Tools/BerlinMap/berlin_terrain_gen.py

Reads  MapDesigner/Berlin/berlin_01.layout.json (the design source, +X north, +Y east, cm).
Writes Saved/Berlin/terrain/
    Ger_Berlin_height.raw        uint16 LE, 32768 + cm * 128 / 100, row by row along X (Z[iy, ix])
    Ger_Berlin_layer_<Layer>.raw uint8 weights, same grid, the six sum to 255 per vertex
    Ger_Berlin_layout.json       landscape grid, resolved regions / features / paths, gaps, wall-core runs, masks
    Ger_Berlin_masks.npz         boolean masks on the vertex grid ([iy, ix]) for the dressing step
    preview.png                  hill-shaded layer colours, north up, with the play bounds

The regions are rasterised with MapDesigner's own Raster/Terrain so blobs, wobble and openings match the sketch.
Heights only use box blurs of those masks and distances to polylines (no scipy on this machine).
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
from mapdesigner.spec import Spec, dist_to_polyline  # noqa: E402

SPEC_FILE = os.path.join(ROOT, "MapDesigner", "Berlin", "berlin_01.layout.json")
OUT = os.path.join(ROOT, "Saved", "Berlin", "terrain")

# The play area is 520 m (X, north-south) by 1000 m (Y, east-west). The landscape is the sketch's own
# (bounds.landscape): 19 x 19 components of 63 quads at 100 cm = 1197 m square, centred.
COMPONENTS_X = 19
COMPONENTS_Y = 19
QUADS = 63
QUAD_CM = 100.0
VX = COMPONENTS_X * QUADS + 1            # 1198 vertices along X
VY = COMPONENTS_Y * QUADS + 1            # 1198 vertices along Y
SIZE_X = COMPONENTS_X * QUADS * QUAD_CM  # 119 700
SIZE_Y = COMPONENTS_Y * QUADS * QUAD_CM  # 119 700
SHAPE = (VX, VY)                         # work arrays: row = north..south, col = west..east

LAYERS = ["BerlinRubble", "BerlinAsh", "BerlinCobble", "BerlinGlass", "BerlinScrub", "BerlinMud"]
LAYER_TRACE = 2  # of 255: a component missing a layer renders washed pale

# heights, cm
CRATER_DEPTH = 520.0
CRATER_RIM = 110.0
SINK_DEPTH = 800.0
RIVER_BED = -480.0
WATER_Z = -190.0
CORE_BERM = 260.0       # collapsed-wall mound under the impassable ruin cores
FIELD_BUMP = 70.0       # lumpy rubble in the dense ruin fields (still walkable)
RAIL_BANK = 60.0
EDGE_RISE = 420.0       # rubble hills past the play edge
RIVER_HALF = 1250.0     # water half width (sketch: 25 m river)
QUAY = 280.0            # horizontal run of the quay wall


def box_blur(a, r):
    """Box blur of radius r cells (edge-clamped), separable, via cumulative sums."""
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
    """Smooth 0..1 version of a mask: two box blurs, about a tent of half width 2r."""
    return box_blur(box_blur(mask.astype(np.float64), r), r)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(seed, wavelength_cells, shape=SHAPE):
    """Smooth value noise in -1..1 with the given feature size."""
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


# Implementation overrides on the sketch (the sketch file itself stays the design source):
# the landmark assets other sessions built are larger than the sketch's pads, so the ground reserved for them grows
# to their real footprints, and what they then cover moves aside.
#   Reichstag ruin (BP_ReichstagRuin_B2): 117 x 94 m plus the weapon-pit apron in front of the portico. It only fits
#     between the Gate and the Spree turned with the portico to the south and moved about 70 m west.
#   Brandenburg Gate ruin: 65 x 26 m, moved 14 m west so it clears the crater rim.
#   Flak towers: 40 x 34 m, 58 x 62 m with their rubble.
#   North lane: runs round the north side of the Reichstag, along the south bank.
REICHSTAG_AT = [8100.0, -15500.0]
GATE_AT = [0.0, -8300.0]
# The Zoo flak tower ruin covers 62 x 57 m with its rubble, so the radio bunker annex stands at its north-east foot,
# on the side the lanes arrive from (the sketch point lies inside the tower).
ZOO_BUNKER_AT = [-10500.0, -43000.0]
# Ruined electrical station (owner request 2026-11-12): between the central and north lanes, about 95 m from the start,
# with scavengeable tank wrecks and vehicle-parts piles on its south side. Half sizes: 21 m north-south, 37 m east-west.
POWER_STATION_AT = [13800.0, 35000.0]
POWER_STATION_HALF = [2700.0, 3800.0]   # walled yard 54 m north-south, 76 m east-west


def implementation_spec():
    with open(SPEC_FILE, encoding="utf-8") as handle:
        raw = json.load(handle)
    regions = {r["id"]: r for r in raw["regions"]}
    features = {f["id"]: f for f in raw["features"]}
    paths = {p["id"]: p for p in raw["paths"]}
    reich = regions["reichstag_ruins"]
    reich.pop("blob")
    reich["rect"] = {"at": REICHSTAG_AT, "size": [9800, 12000]}
    apron = {"id": "reichstag_apron", "kind": "pad", "show_label": False, "label": "Reichstag forecourt (pits)",
             "rect": {"at": [REICHSTAG_AT[0] - 5900.0, REICHSTAG_AT[1]], "size": [2600, 7600]}}
    raw["regions"].insert(raw["regions"].index(reich) + 1, apron)
    regions["gate_ruins"]["rect"] = {"at": GATE_AT, "size": [6800, 3000]}
    features["gate_passage"]["at"] = GATE_AT
    features["brandenburg_gate"]["at"] = [GATE_AT[0] + 1733.0, GATE_AT[1]]
    for rid in ("fhain_tower_block", "zoo_tower_block"):
        regions[rid]["rect"]["size"] = [6000, 6400]
        regions[rid]["rect"].pop("yaw", None)
    lane = paths["north_lane"]["points"]
    k = lane.index([10292, -12222])
    lane[k:k + 2] = [[13700, -9000], [13550, -22500], [9500, -28000]]
    flank = paths["flank_lane"]["points"]
    k = flank.index([13500, -16000])
    flank[k] = [13650, -16000]
    # the Spree runs on through the camera margin instead of ending at the play edge
    spree = paths["spree"]["points"]
    spree.insert(0, [59000, 23400])
    spree.append([13600, -60500])
    out = os.path.join(OUT, "berlin_01_impl.layout.json")
    os.makedirs(OUT, exist_ok=True)
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(raw, handle, indent=1)
    return out


class Gen:
    def __init__(self):
        self.spec = Spec(implementation_spec())
        if self.spec.errors():
            raise RuntimeError("spec errors: %s" % [str(i.msg) for i in self.spec.errors()])
        raster = Raster(self.spec, QUAD_CM)
        # Pixel centres on the landscape vertices: row r is x = SIZE/2 - r*100, col c is y = -SIZE/2 + c*100.
        raster.xmax = SIZE_X / 2.0 + QUAD_CM / 2.0
        raster.ymin = -SIZE_Y / 2.0 - QUAD_CM / 2.0
        raster.rows, raster.cols = VX, VY
        self.raster = raster
        self.terrain = Terrain(self.spec, raster)
        self.masks = self.terrain.region_masks
        self.X = (SIZE_X / 2.0 - np.arange(VX) * QUAD_CM)[:, None] * np.ones((1, VY))   # north-south per row
        self.Y = np.ones((VX, 1)) * (-SIZE_Y / 2.0 + np.arange(VY) * QUAD_CM)[None, :]  # west-east per col
        self.inside = self.terrain.inside
        self.Z = np.zeros(SHAPE)
        self.W = {}
        self.out_masks = {}
        # set by the dressing generator before its second paint pass: where ruins stand (0..1 stain, 0..1 nearness)
        self.ruin_stain = None
        self.ruin_near = None

    # ---- helpers on the raster (row = north..south, col = west..east) -------------------------------------------
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

    def path(self, pid):
        return next(p for p in self.spec.paths if p.id == pid)

    def feature(self, fid):
        return next(f for f in self.spec.features if f.id == fid)

    def dist_line(self, pts):
        pts = np.asarray(pts, float)
        flat = np.c_[self.X.ravel(), self.Y.ravel()]
        lo = pts.min(axis=0) - 6000.0
        hi = pts.max(axis=0) + 6000.0
        near = (flat[:, 0] >= lo[0]) & (flat[:, 0] <= hi[0]) & (flat[:, 1] >= lo[1]) & (flat[:, 1] <= hi[1])
        d = np.full(flat.shape[0], 1e9)
        d[near] = dist_to_polyline(flat[near], pts)
        return d.reshape(SHAPE)

    def dist_point(self, p):
        return np.hypot(self.X - p[0], self.Y - p[1])

    # ---- heights ------------------------------------------------------------------------------------------------
    def build_heights(self):
        spec = self.spec
        Z = 45.0 * fbm(11, 90.0, 3) + 10.0 * noise(12, 9.0)

        # Tiergarten: rolling dead parkland
        tier = soft(self.region("tiergarten"), 6)
        Z += tier * 70.0 * fbm(21, 45.0, 2)

        # dense ruin fields: lumpy rubble; their cores: a steep mound of collapsed walls, cut by the gaps
        field_ids = [r.id for r in spec.regions if r.id.startswith("field_")]
        core_ids = [r.id for r in spec.regions if r.id.startswith("core_")]
        gap_ids = [f.id for f in spec.features if f.is_opening and f.raw.get("region", "").startswith("core_")]
        fields = self.region(*field_ids)
        Z += soft(fields, 3) * (FIELD_BUMP * 0.6 + FIELD_BUMP * np.abs(fbm(31, 7.0, 2)))
        cores = self.region(*core_ids) & ~self.openings(*gap_ids)
        # Narrow the mound to the middle of the 15 m band so its top carries the wall line and the gap floors stay flat.
        # the outline is broken by noise (never a clean capsule from above); the gaps are cut again right below
        core_soft = smoothstep(0.42, 0.78, soft(cores, 2) + 0.2 * noise(33, 5.0) + 0.1 * noise(34, 14.0))
        Z = Z * (1.0 - 0.6 * core_soft) + core_soft * (CORE_BERM + 60.0 * noise(32, 6.0) + 45.0 * noise(35, 15.0))
        gaps = self.openings(*gap_ids)
        gap_soft = smoothstep(0.3, 0.8, soft(gaps, 1))
        Z = Z * (1.0 - gap_soft) + gap_soft * 8.0
        self.out_masks["ruin_field"] = fields
        self.out_masks["ruin_core"] = cores
        self.out_masks["core_gap"] = gaps

        # flat pads: start, towers, gate, Reichstag, rail yard, Hbf, Alexanderplatz, Potsdamer Platz
        pads = self.region("start_ruins", "fhain_tower_block", "zoo_tower_block", "gate_ruins", "reichstag_ruins",
                           "reichstag_apron", "rail_yard", "hbf_hall")
        for fid, rad in (("alexanderplatz", 2500.0), ("potsdamer_platz", 1800.0), ("forge_station", 2300.0),
                         ("demeter_pod", 1100.0)):
            pads |= self.dist_point(self.feature(fid).points[0]) < rad
        pads |= self.dist_point(ZOO_BUNKER_AT) < 1300.0
        pads |= (np.abs(self.X - POWER_STATION_AT[0]) < POWER_STATION_HALF[0] + 1200.0) &                 (self.Y > POWER_STATION_AT[1] - POWER_STATION_HALF[1] - 4200.0) &                 (self.Y < POWER_STATION_AT[1] + POWER_STATION_HALF[1] + 500.0)
        pad_soft = smoothstep(0.2, 0.9, soft(pads, 4))
        Z = Z * (1.0 - 0.9 * pad_soft)
        # the landmark assets stand on dead-flat ground
        flat = smoothstep(0.25, 0.75, soft(self.region("fhain_tower_block", "zoo_tower_block", "gate_ruins",
                                                         "reichstag_ruins", "reichstag_apron"), 3))
        Z = Z * (1.0 - flat)
        self.out_masks["landmark_pad"] = flat > 0.5
        self.out_masks["pads"] = pads

        # old bomb craters, away from pads, lanes and the special regions
        Z += self.bomb_craters(pads)

        # streets as channels: the carriageway of every lane and of the road lies 20 cm low behind a 1 m kerb ramp, and
        # the collapsed frontages form a low rubble berm 1-6 m outside the clear width. Nothing is raised near wall-core
        # gaps, openings, pads, the crater, sinkholes or the river, and the berm stays gentle (always walkable).
        lane = np.full(SHAPE, 1e9)
        for p in spec.paths:
            if p.kind == "lane":
                lane = np.minimum(lane, self.dist_line(p.pts))
            elif p.kind == "road":
                lane = np.minimum(lane, self.dist_line(p.pts) - 300.0)
        self.lane_dist = lane
        calm = soft(pads | self.region("crater", "sinkholes", "sinkhole_a", "sinkhole_b", "spree_banks", "tiergarten"), 6)
        calm = np.maximum(calm, soft(gaps, 8) * 3.0)
        calm = np.maximum(calm, smoothstep(0.02, 0.2, soft(cores, 3)))
        for f in spec.features:
            if f.is_opening and f.points:
                calm = np.maximum(calm, smoothstep(1500.0, 900.0, self.dist_point(f.points[0])))
        calm = np.clip(calm, 0.0, 1.0)
        carriage = smoothstep(720.0, 620.0, lane)
        Z = Z * (1.0 - carriage) + carriage * (Z * 0.35 - 20.0)
        # critic round 3: the berm is ragged. Its outer foot wanders 2-9 m from the clear width, it breaks off
        # completely every 30-60 m, its ends taper through noise, and it stops where the blast glass starts.
        width = 1000.0 + 700.0 * np.clip(0.5 + 0.9 * fbm(94, 20.0, 2), 0.0, 1.0)
        breaks = smoothstep(-0.28, 0.02, fbm(95, 38.0, 2) + 0.25 * noise(96, 9.0))
        blast_d = self.dist_point(spec.by_id["crater"].geom.centre) + 900.0 * noise(97, 12.0)
        no_glass = np.maximum(smoothstep(9500.0, 12500.0, blast_d), 1.0 - smoothstep(0.1, 0.5, soft(self.region("blast_zone"), 6)))
        ends = np.clip((1.0 - calm) * 1.6 - 0.6 * np.clip(0.5 + noise(98, 6.0), 0.0, 1.0), 0.0, 1.0)
        berm = smoothstep(780.0, 960.0, lane) * smoothstep(width, width - 380.0, lane) * ends * breaks * no_glass
        self.berm_w = berm
        berm_h = 55.0 + 45.0 * (0.5 + 0.5 * fbm(91, 14.0, 2)) + 25.0 * noise(92, 5.0)
        Z += berm * berm_h * np.clip(0.55 + 0.75 * fbm(93, 30.0, 2), 0.0, 1.2)
        self.out_masks["street_berm"] = berm > 0.4
        self.out_masks["carriageway"] = carriage > 0.5

        # rail embankments (passable low banks)
        rail = np.full(SHAPE, 1e9)
        for p in spec.paths:
            if p.kind == "rail":
                rail = np.minimum(rail, self.dist_line(p.pts))
        bank = smoothstep(700.0, 350.0, rail)
        Z = Z * (1.0 - bank) + bank * RAIL_BANK
        self.out_masks["rail"] = rail < 250.0
        self.rail_dist = rail

        # the nuclear crater: bowl with a raised glassy rim
        crater = self.region("crater")
        bowl = soft(crater, 11)
        Z -= CRATER_DEPTH * smoothstep(0.30, 0.98, bowl)
        Z += CRATER_RIM * np.exp(-((bowl - 0.36) / 0.13) ** 2)
        self.out_masks["crater"] = crater

        # U-Bahn sinkholes: steep-walled pits, causeways left at the openings
        sink_gap_ids = [f.id for f in spec.features if f.is_opening and f.raw.get("region") == "sinkholes"]
        sink = self.region("sinkholes", "sinkhole_a", "sinkhole_b") & ~self.openings(*sink_gap_ids)
        # the rim is ragged (no ruler-straight edge or square corner), but exact at the causeways
        near_gap = np.clip(soft(self.openings(*sink_gap_ids), 8) * 6.0, 0.0, 1.0)
        pit = smoothstep(0.45, 0.80, soft(sink, 2) + (0.26 * noise(42, 7.0) + 0.1 * noise(43, 3.0)) * (1.0 - near_gap)
                         * (soft(sink, 4) > 0.03))
        floor_noise = 60.0 * fbm(41, 8.0, 2)
        Z = Z * (1.0 - pit) + pit * (-SINK_DEPTH + floor_noise)
        self.out_masks["sinkhole"] = sink
        self.out_masks["sink_gap"] = self.openings(*sink_gap_ids)

        # the Spree: quay-walled trench; the half-collapsed Moltke bridge is a rubble causeway across it
        river = self.dist_line(self.path("spree").pts)
        bed = smoothstep(RIVER_HALF + QUAY, RIVER_HALF, river)
        bridge = self.terrain.opening_masks["moltke_bridge"]
        bridge_soft = smoothstep(0.35, 0.75, soft(bridge, 1))
        bed = bed * (1.0 - bridge_soft)
        Z = Z * (1.0 - bed) + bed * (RIVER_BED + 25.0 * noise(51, 10.0))
        # banks dip gently toward the quay edge
        Z -= 35.0 * smoothstep(2250.0, RIVER_HALF + QUAY, river) * (1.0 - bed) * (1.0 - bridge_soft)
        self.river_dist = river
        self.out_masks["river"] = (river < RIVER_HALF + QUAY) & ~bridge
        self.out_masks["bridge"] = bridge

        # past the play edge: rising rubble hills (the Spree and the rail lines run out through them)
        d_out = np.maximum.reduce([self.X - spec.xmax, spec.xmin - self.X, self.Y - spec.ymax, spec.ymin - self.Y])
        rise = smoothstep(300.0, 3200.0, d_out) * (1.0 - bed) * (1.0 - 0.8 * bank)
        Z += rise * (EDGE_RISE + 260.0 * fbm(61, 40.0, 3))
        self.edge_dist = d_out
        self.Z = Z

    def bomb_craters(self, pads):
        rng = np.random.default_rng(71)
        keep_out = pads | self.region("crater", "sinkholes", "sinkhole_a", "sinkhole_b", "spree_banks")
        keep_out |= self.region(*[r.id for r in self.spec.regions if r.id.startswith("core_")])
        lane = np.full(SHAPE, 1e9)
        for p in self.spec.paths:
            if p.kind in ("lane", "road", "rail"):
                lane = np.minimum(lane, self.dist_line(p.pts))
        keep_out |= lane < 900.0
        keep_out = soft(keep_out, 5) > 0.02
        out = np.zeros(SHAPE)
        placed = []
        tries = 0
        while len(placed) < 70 and tries < 6000:
            tries += 1
            x = rng.uniform(self.spec.xmin + 600, self.spec.xmax - 600)
            y = rng.uniform(self.spec.ymin + 600, self.spec.ymax - 600)
            r, c = self.raster.to_rc([x, y])
            if keep_out[r, c]:
                continue
            rad = rng.uniform(300.0, 650.0)
            if any(math.hypot(x - q[0], y - q[1]) < rad + q[2] + 400.0 for q in placed):
                continue
            depth = rad * rng.uniform(0.13, 0.2)
            d = self.dist_point((x, y)) / rad
            out += -depth * np.clip(1.0 - d * d, 0.0, 1.0) ** 1.5 + depth * 0.22 * np.exp(-((d - 1.1) / 0.22) ** 2)
            placed.append((x, y, rad, depth))
        self.craters = placed
        return out

    # ---- paint layers -------------------------------------------------------------------------------------------
    def build_layers(self):
        spec = self.spec
        n_big, n_mid, n_fine = fbm(81, 60.0, 3), fbm(82, 18.0, 2), noise(83, 5.0)

        ash = 0.30 + 0.45 * n_big + 0.25 * n_mid
        ash += 0.35 * soft(self.region("east_ruins", "start_ruins", "fhain_rubble"), 8)
        ash = np.clip(ash, 0.0, 1.0)

        # streets: the carriageways are cobble across their whole clear width, with rubble and ash patches breaking a
        # fifth to a third of it; side streets are thinner and patchier; squares are cobbled whole
        patch = smoothstep(-0.75, -0.15, 0.7 * fbm(84, 4.0, 2) + 0.5 * n_mid)
        street = smoothstep(760.0, 640.0, self.lane_dist) * patch
        for fid, rad in (("alexanderplatz", 2500.0), ("potsdamer_platz", 1800.0)):
            square = smoothstep(rad, rad - 700.0, self.dist_point(self.feature(fid).points[0]) + 350.0 * n_mid)
            street = np.maximum(street, square * np.clip(patch + 0.25, 0.0, 1.0))
        yard = smoothstep(0.3, 0.7, soft(self.region("rail_yard", "hbf_hall"), 6) + 0.3 * n_mid)     # feathered pad edge
        street = np.maximum(street, 0.85 * yard * patch)
        ang = math.radians(14.0)
        u = self.X * math.cos(ang) + self.Y * math.sin(ang)
        v = -self.X * math.sin(ang) + self.Y * math.cos(ang)
        grid = np.maximum(smoothstep(380.0, 240.0, np.abs((u + 2100.0) % 7800.0 - 3900.0)),
                          smoothstep(380.0, 240.0, np.abs((v + 900.0) % 9600.0 - 4800.0)))
        self.out_masks["side_street"] = grid > 0.5
        street = np.maximum(street, 0.6 * grid * smoothstep(-0.4, 0.2, n_mid + 0.5 * n_fine))
        park = smoothstep(0.35, 0.65, soft(self.region("tiergarten"), 10) + 0.3 * n_mid + 0.12 * n_fine)   # ragged park edge
        cobble = street * (1.0 - 0.75 * park)
        self.out_masks["street"] = street > 0.5

        # blast glass: solid near the crater, breaking up toward the edge of the blast zone
        centre = self.spec.by_id["crater"].geom.centre
        d_crater = self.dist_point(centre)
        blast = soft(self.region("blast_zone"), 6)
        glass = blast * np.clip(smoothstep(12500.0, 5200.0, d_crater) + 0.35 * n_mid + 0.2 * n_fine, 0.0, 1.0)
        glass = np.maximum(glass, smoothstep(0.25, 0.6, soft(self.region("crater"), 5)))
        ash = np.maximum(ash * (1.0 - 0.5 * blast), 0.8 * smoothstep(0.15, 0.6, blast) * (1.0 - glass))

        scrub = park * np.clip(0.8 + 0.5 * n_mid, 0.0, 1.0)
        scrub = np.maximum(scrub, 0.55 * soft(self.region("zoo_grounds"), 6) * np.clip(0.6 + n_mid, 0.0, 1.0))

        mud = smoothstep(2350.0, 1500.0, self.river_dist)
        mud = np.maximum(mud, smoothstep(0.1, 0.5, soft(self.out_masks["sinkhole"], 2)))
        for x, y, rad, depth in self.craters:
            mud = np.maximum(mud, 0.8 * smoothstep(0.75, 0.25, self.dist_point((x, y)) / rad))
        bridge = soft(self.out_masks["bridge"], 1)
        mud *= 1.0 - 0.9 * bridge

        # district ground, feathered: Mitte stays rubble, the north is ash with dead scrub in the lots, the south is wet
        north = smoothstep(13500.0, 16500.0, self.X + 1500.0 * n_big)
        south = smoothstep(-5500.0, -8500.0, self.X + 1500.0 * n_big)
        mitte = (1.0 - north) * (1.0 - south)
        ash = np.clip(ash * (1.0 - 0.55 * mitte) + 0.35 * north, 0.0, 1.0)
        # critic round 3: scrub and mud patches belong to ruins (courtyards, lots), never free-floating blotches
        near = self.ruin_near if self.ruin_near is not None else np.ones(SHAPE)
        scrub = np.maximum(scrub, 0.6 * north * smoothstep(0.15, 0.6, fbm(86, 16.0, 2)) * (1.0 - street) * near)
        mud = np.maximum(mud, 0.75 * south * smoothstep(0.1, 0.55, fbm(87, 20.0, 2)) * (1.0 - street) * (1.0 - park) * near)
        # every ruin and rubble mass sits in a stain: ash cleared to bare rubble, a little wet dirt in it
        if self.ruin_stain is not None:
            stain = self.ruin_stain * (1.0 - glass)
            ash = ash * (1.0 - 0.9 * stain)
            cobble = cobble * (1.0 - 0.7 * stain)
            mud = np.maximum(mud, 0.3 * stain * np.clip(0.6 + 0.8 * n_fine, 0.0, 1.0))
        # the street berms read as rubble heaps, darker than the carriageway: rubble with wet dirt and dead weeds
        bw = np.clip(self.berm_w, 0.0, 1.0)
        ash = ash * (1.0 - bw)
        cobble = cobble * (1.0 - bw)
        mud = np.maximum(mud, 0.4 * bw * np.clip(0.7 + 0.6 * n_fine, 0.0, 1.0))
        scrub = np.maximum(scrub * (1.0 - 0.6 * bw), 0.22 * bw * smoothstep(0.0, 0.5, n_mid))

        # priority stack: later entries cover earlier ones
        w = {"BerlinRubble": np.ones(SHAPE)}
        for name, layer in (("BerlinAsh", ash), ("BerlinScrub", scrub), ("BerlinCobble", cobble),
                            ("BerlinGlass", glass), ("BerlinMud", mud)):
            layer = np.clip(box_blur(layer, 1), 0.0, 1.0)
            for other in w:
                w[other] = w[other] * (1.0 - layer)
            w[name] = layer
        # the wall-core mounds and the edge hills are plain rubble
        hard = np.clip(smoothstep(0.3, 0.7, soft(self.out_masks["ruin_core"], 2)) +
                       smoothstep(1500.0, 3500.0, self.edge_dist) * 0.7, 0.0, 1.0)
        hard *= 1.0 - np.clip(w["BerlinMud"] * 2.0, 0.0, 1.0)
        for name in w:
            w[name] = w[name] * (1.0 - hard) + (hard if name == "BerlinRubble" else 0.0)
        total = sum(w.values())
        budget = 255 - LAYER_TRACE * len(LAYERS)
        ints = {name: np.floor(w[name] / total * budget).astype(np.int32) + LAYER_TRACE for name in LAYERS}
        short = 255 - sum(ints.values())
        ints["BerlinRubble"] += short   # rounding remainder goes to the base layer
        self.W = {name: ints[name].astype(np.uint8) for name in LAYERS}
        assert int(sum(v.astype(np.int32) for v in self.W.values()).min()) == 255

    # ---- output -------------------------------------------------------------------------------------------------
    @staticmethod
    def to_grid(a):
        """[row north..south, col west..east] -> [iy, ix] with ix along +X and iy along +Y."""
        return np.ascontiguousarray(a[::-1, :].T)

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        files = {"height": os.path.join(OUT, "Ger_Berlin_height.raw").replace("\\", "/"), "layers": {}}
        heights = np.clip(np.round(32768.0 + self.to_grid(self.Z) * 128.0 / 100.0), 0, 65535).astype("<u2")
        heights.tofile(files["height"])
        for name in LAYERS:
            path = os.path.join(OUT, "Ger_Berlin_layer_%s.raw" % name).replace("\\", "/")
            self.to_grid(self.W[name]).tofile(path)
            files["layers"][name] = path
        masks_file = os.path.join(OUT, "Ger_Berlin_masks.npz").replace("\\", "/")
        np.savez_compressed(masks_file, **{k: self.to_grid(v) for k, v in self.out_masks.items()})
        files["masks"] = masks_file

        spec = self.spec
        regions = []
        for reg in spec.regions:
            regions.append({"id": reg.id, "kind": reg.kind, "label": reg.label, "passable": reg.passable,
                            "shape": reg.geom.kind, "width": reg.geom.width,
                            "centre": [float(v) for v in reg.geom.centre],
                            "points": [[float(a), float(b)] for a, b in reg.geom.pts], "raw": reg.raw})
        features = []
        for f in spec.features:
            entry = {"id": f.id, "kind": f.kind, "label": f.label, "radius": f.radius, "width": f.width,
                     "points": [[float(a), float(b)] for a, b in f.points], "raw": f.raw,
                     "dir": [float(v) for v in f.dir] if f.dir is not None else None}
            if f.geom is not None:
                entry["shape"] = {"kind": f.geom.kind, "centre": [float(v) for v in f.geom.centre],
                                  "points": [[float(a), float(b)] for a, b in f.geom.pts]}
            if f.id in self.terrain.opening_cuts:
                entry["cut_ends"] = [[float(a), float(b)] for a, b in self.terrain.opening_cuts[f.id]["ends"]]
            features.append(entry)
        paths = [{"id": p.id, "kind": p.kind, "label": p.label, "width": p.width,
                  "points": [[float(a), float(b)] for a, b in p.pts]} for p in spec.paths]
        layout = {
            "map": "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin",
            "spec": SPEC_FILE.replace("\\", "/"),
            "landscape": {"components_x": COMPONENTS_X, "components_y": COMPONENTS_Y,
                          "quads_per_component": QUADS, "quad_cm": QUAD_CM, "verts_x": VX, "verts_y": VY,
                          "size_x": SIZE_X, "size_y": SIZE_Y, "corner": [-SIZE_X / 2.0, -SIZE_Y / 2.0]},
            "bounds": {"xmin": spec.xmin, "xmax": spec.xmax, "ymin": spec.ymin, "ymax": spec.ymax},
            "layers": LAYERS, "files": files, "reichstag_at": REICHSTAG_AT, "gate_at": GATE_AT, "zoo_bunker_at": ZOO_BUNKER_AT, "power_station_at": POWER_STATION_AT, "water_z": WATER_Z, "river_half": RIVER_HALF,
            "bomb_craters": [[float(v) for v in c] for c in self.craters],
            "regions": regions, "features": features, "paths": paths,
        }
        with open(os.path.join(OUT, "Ger_Berlin_layout.json"), "w") as handle:
            json.dump(layout, handle, indent=1)
        self.preview()
        z = self.Z[self.inside]
        print("heights cm inside: min %.0f max %.0f mean %.1f" % (z.min(), z.max(), z.mean()))
        for name in LAYERS:
            print("layer %-13s mean %.3f (inside)" % (name, self.W[name][self.inside].mean() / 255.0))
        print("bomb craters %d" % len(self.craters))
        print("wrote %s" % OUT)

    def preview(self):
        colours = {"BerlinRubble": (126, 104, 92), "BerlinAsh": (150, 148, 142), "BerlinCobble": (92, 92, 96),
                   "BerlinGlass": (34, 52, 46), "BerlinScrub": (112, 108, 62), "BerlinMud": (58, 48, 38)}
        rgb = np.zeros((VX, VY, 3))
        for name, col in colours.items():
            rgb += (self.W[name].astype(np.float64) / 255.0)[:, :, None] * np.array(col)[None, None, :]
        gx = np.gradient(self.Z, axis=1) / QUAD_CM
        gy = np.gradient(self.Z, axis=0) / QUAD_CM
        shade = np.clip(0.75 + 1.6 * (gx + gy) / 1.4, 0.25, 1.4)   # light from the north-west
        rgb = np.clip(rgb * shade[:, :, None], 0, 255)
        water = (self.Z < WATER_Z) & (self.river_dist < RIVER_HALF + QUAY)
        rgb[water] = rgb[water] * 0.4 + np.array([40, 70, 90]) * 0.6
        r0, c0 = self.raster.to_rc([self.spec.xmax, self.spec.ymin])
        r1, c1 = self.raster.to_rc([self.spec.xmin, self.spec.ymax])
        for r in (r0, r1):
            rgb[r, c0:c1] = (255, 220, 0)
        for c in (c0, c1):
            rgb[r0:r1, c] = (255, 220, 0)
        Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(OUT, "preview.png"))
        slope = np.degrees(np.arctan(np.hypot(gx, gy)))
        img = np.zeros((VX, VY, 3), dtype=np.uint8)
        img[..., 0] = np.clip(slope * 5, 0, 255)
        img[..., 1] = np.clip((self.Z + 900.0) / 1800.0 * 255, 0, 255)
        img[slope > 40.0] = (255, 0, 255)
        Image.fromarray(img).save(os.path.join(OUT, "preview_slope.png"))


def main():
    gen = Gen()
    gen.build_heights()
    gen.build_layers()
    gen.write()


if __name__ == "__main__":
    main()
