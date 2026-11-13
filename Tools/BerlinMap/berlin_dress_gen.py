"""Dressing generator for Ger_Berlin: ruins from the Destroyed Building Kit (DBK), trees, street wrecks, the rail yard.

    python Source/Tools/BerlinMap/berlin_dress_gen.py

Runs the terrain generator in memory (same masks and heights), then writes Saved/Berlin/terrain/Ger_Berlin_dress.json:
    ism      {static mesh path: [[x, y, z, yaw, pitch, roll, scale], ...]}   instanced by berlin_map_build.step_dress
    actors   [{bp | mesh, label, folder, x, y, z, yaw, scale:[x, y, z], ...}]
    tank_blockers [{x, y, yaw, length, width, label}]   infantry-only gaps (BP_NavBlocker_VolumeBlockTanks)
and dress_preview.png (footprints over the terrain preview).

Nothing here is mission content: no units, triggers or objectives.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw

import berlin_terrain_gen as tg

KIT_PROBE = os.path.join(tg.ROOT, "Saved", "Berlin", "logs", "probe_kit.json")
OUT_FILE = os.path.join(tg.OUT, "Ger_Berlin_dress.json")
GRID_ANGLE = math.radians(14.0)     # the side-street grid of the terrain generator
MODULE = 300.0                      # DBK wall module: 300 long, 20 thick, 300 high
DEST = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/"
FOOT_TABLE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ruinkit", "footprint_table.json")
# the dressing as it stood after critic round 2: street events and hero ruins the critic asked to keep are carried over
CARRY_FILE = os.path.join(tg.ROOT, "Saved", "Berlin", "Ger_Berlin_dress_r2.json")


class Kit:
    """DBK piece tables per wall family, resolved against the probed asset list."""

    def __init__(self):
        with open(KIT_PROBE) as handle:
            self.meshes = json.load(handle)["meshes"]
        self.by_name = {path.rsplit("/", 1)[1]: path for path in self.meshes}
        self.families = {}
        for fam, wall in (("Brick", "Brick_Wall_Full"), ("Cinderblock", "Cinderblock_Full"), ("Concrete", "Concete_Wall_Full")):
            pre = "DBK_%s_" % fam
            wall_pre = "DBK_Concrete_Wall_" if fam == "Concrete" else pre
            self.families[fam] = {
                "solid": self.pick(["DBK_" + wall]),
                "window": self.pick([pre + "Win%s_Full" % w for w in ("Small", "Big", "Wide", "Double")]),
                "door": self.pick(["DBK_Concete_Wall_Door" if fam == "Concrete" else pre + ("Wall_Door" if fam == "Brick" else "Door")]),
                "top": self.pick([wall_pre + "Tile_%s" % c for c in "ABCD"] + [wall_pre + "90_A", wall_pre + "90_B"] +
                                 [pre + "Win%s_Tile" % w for w in ("Small", "Big", "Wide", "Double")]),
                "stub": self.pick(["DBK_Concrete_Wall_Top" if fam == "Concrete" else pre + ("Top_Full" if fam == "Brick" else "Top")]),
                "pillar": self.pick([pre + "Pillar_%s" % c for c in ("A", "B", "C", "D", "End_A", "End_B")]),
            }
        self.rubble_big = self.pick(["DBK_Concrete_Rubble_Big", "DBK_Concrete_Rubble_Mid_A"])
        self.rubble_mid = self.pick(["DBK_Concrete_Rubble_Mid_B", "DBK_Concrete_Rubble_Mid_C", "DBK_Brick_Rubble_A",
                                     "DBK_Cinderblock_Rubble_A", "DBK_Concrete_Rubble_Mid_A"])
        self.rubble_flat = self.pick(["DBK_Concrete_Rubble_Small_A", "DBK_Concrete_Rubble_Small_B", "DBK_Concrete_Rubble_Small_C",
                                      "DBK_Concrete_Rubble_Small_D", "DBK_Brick_Rubble_C", "DBK_Brick_Rubble_D",
                                      "DBK_Brick_Rubble_B", "DBK_Cinderblock_Rubble_C", "DBK_Cinderblock_Rubble_D",
                                      "DBK_Concrete_Rubble_Mid_E", "DBK_Concrete_Rubble_Mid_D"])
        self.slabs = self.pick(["DBK_Concrete_Floor_Tile_A", "DBK_Concrete_Floor_Tile_B", "DBK_Concrete_Floor_Tile_C",
                                "DBK_Concrete_Floor_Tile_D", "DBK_Concrete_Floor_90_A", "DBK_Concrete_Floor_90_B",
                                "DBK_Concete_Floor_Full"])
        self.chunks = self.pick(["DBK_Concrete_Prop_Big_A", "DBK_Concrete_Prop_Big_B", "DBK_Concrete_Prop_Big_C",
                                 "DBK_Concrete_Prop_Big_D", "DBK_Concrete_Prop_Mid_A", "DBK_Concrete_Prop_Mid_B",
                                 "DBK_Concrete_Prop_Mid_C"])
        self.cars = [p for p in self.meshes if "/PostApoTown/Cars/" in p]

    def pick(self, names):
        found = [self.by_name[n] for n in names if n in self.by_name]
        if not found:
            raise RuntimeError("no kit piece among %s" % names)
        return found


class Dress:
    def __init__(self):
        self.gen = tg.Gen()
        self.gen.build_heights()
        self.gen.build_layers()
        self.spec = self.gen.spec
        self.kit = Kit()
        self.rng = np.random.default_rng(2026)
        self.ism = {}
        self.actors = []
        self.tank_blockers = []
        self.footprints = []
        self.ruins = []
        self.decals = []
        self.feet = []                       # modelled footprint skirts: [mesh, x, y, z, yaw, scale, tint bucket]
        with open(FOOT_TABLE) as handle:
            self.foot_table = json.load(handle)["pieces"]
        self.carried = False
        self.fallen_skips = {}
        self.build_keepout()
        self.build_street_index()
        self.body_mask = np.zeros(tg.SHAPE, dtype=bool)     # footprints of whole buildings and lots (not single walls)
        self.ruin_mask = np.zeros(tg.SHAPE, dtype=bool)     # where ruins and rubble masses stand: drives the ground stain
        self.openings_at = [f.points[0] for f in self.spec.features if f.is_opening and f.points]

    # ---- grid helpers -------------------------------------------------------------------------------------------
    def rc(self, x, y):
        r = int(round((tg.SIZE_X / 2.0 - x) / tg.QUAD_CM))
        c = int(round((y + tg.SIZE_Y / 2.0) / tg.QUAD_CM))
        return min(max(r, 0), tg.VX - 1), min(max(c, 0), tg.VY - 1)

    def z(self, x, y):
        r, c = self.rc(x, y)
        return float(self.gen.Z[r, c])

    def at(self, mask, x, y):
        r, c = self.rc(x, y)
        return bool(mask[r, c])

    def in_region(self, rid, x, y):
        return self.at(self.gen.masks[rid], x, y)

    def add(self, mesh, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0):
        self.ism.setdefault(mesh, []).append([round(x, 1), round(y, 1), round(z, 1), round(yaw, 2), round(pitch, 2),
                                              round(roll, 2), round(scale, 3)])

    def actor(self, label, folder, x, y, z, yaw=0.0, scale=(1.0, 1.0, 1.0), **kw):
        entry = {"label": label, "folder": folder, "x": round(x, 1), "y": round(y, 1), "z": round(z, 1),
                 "yaw": round(yaw, 2), "scale": list(scale)}
        entry.update(kw)
        self.actors.append(entry)

    def choice(self, items):
        return items[int(self.rng.integers(len(items)))]

    # ---- where nothing may be built -----------------------------------------------------------------------------
    def build_keepout(self):
        g = self.gen
        m = g.out_masks
        lane = np.full(tg.SHAPE, 1e9)
        road = np.full(tg.SHAPE, 1e9)
        for p in self.spec.paths:
            if p.kind == "lane":
                lane = np.minimum(lane, g.dist_line(p.pts))
            elif p.kind == "road":
                road = np.minimum(road, g.dist_line(p.pts))
        self.lane_dist, self.road_dist = lane, road
        keep = g.rail_dist < 700.0
        keep |= tg.soft(m["pads"], 3) > 0.05
        keep |= tg.soft(m["landmark_pad"], 4) > 0.05
        keep |= tg.soft(m["crater"], 5) > 0.03
        keep |= tg.soft(m["sinkhole"] | m["sink_gap"], 4) > 0.03
        keep |= g.river_dist < tg.RIVER_HALF + tg.QUAY + 700.0
        keep |= tg.soft(m["bridge"], 6) > 0.02
        keep |= tg.soft(m["ruin_core"], 3) > 0.03
        keep |= tg.soft(m["core_gap"], 6) > 0.01
        keep |= g.region("tiergarten", "zoo_debris")
        for x, y, rad, depth in g.craters:
            keep |= g.dist_point((x, y)) < rad * 1.25
        for f in self.spec.features:
            if f.kind in ("poi", "capture", "enemy_base", "enemy_spawn") and f.points:
                keep |= g.dist_point(f.points[0]) < max(900.0, min(f.radius, 2000.0))
            if f.is_opening and f.points:
                keep |= g.dist_point(f.points[0]) < 1200.0
        for f in self.spec.features:
            if f.id.startswith("exit_") and f.geom is not None:
                keep |= tg.soft(g.raster.mask_geom(f.geom), 3) > 0.02     # the escape exits stay open ground
        keep |= g.dist_point(tg.ZOO_BUNKER_AT) < 1800.0
        keep |= (np.abs(g.X - tg.POWER_STATION_AT[0]) < tg.POWER_STATION_HALF[0] + 1500.0) &                 (g.Y > tg.POWER_STATION_AT[1] - tg.POWER_STATION_HALF[1] - 4300.0) &                 (g.Y < tg.POWER_STATION_AT[1] + tg.POWER_STATION_HALF[1] + 700.0)
        # hard: ground nothing may stand on whatever the rule; keep adds the street clearances on top of it
        self.hard = keep.copy()
        keep = keep | (lane < 720.0) | (road < 1050.0) | (tg.soft(m["side_street"], 1) > 0.3)
        self.keep = keep
        self.occupied = keep.copy()
        self.occupied_objects = np.zeros(tg.SHAPE, dtype=bool)     # only what the generator itself has placed

    def rect_free(self, cx, cy, hu, hv, ang, margin=110.0):
        """True when the oriented rectangle (plus margin) touches nothing occupied; also returns its cells."""
        cu, su = math.cos(ang), math.sin(ang)
        us = np.arange(-hu - margin, hu + margin + 1.0, 90.0)
        vs = np.arange(-hv - margin, hv + margin + 1.0, 90.0)
        uu, vv = np.meshgrid(us, vs)
        xs = cx + uu * cu - vv * su
        ys = cy + uu * su + vv * cu
        rows = np.clip(np.round((tg.SIZE_X / 2.0 - xs) / tg.QUAD_CM).astype(int), 0, tg.VX - 1)
        cols = np.clip(np.round((ys + tg.SIZE_Y / 2.0) / tg.QUAD_CM).astype(int), 0, tg.VY - 1)
        if (np.abs(xs) > tg.SIZE_X / 2.0 - 400.0).any() or (np.abs(ys) > tg.SIZE_Y / 2.0 - 400.0).any():
            return False, None
        return (not self.occupied[rows, cols].any()), (rows, cols)

    # ---- one wall column ----------------------------------------------------------------------------------------
    def wall_column(self, fam, x, y, yaw, storeys, base_z, ground_floor_door=False):
        f = self.kit.families[fam]
        if storeys <= 0:
            if self.rng.random() < 0.55:
                self.add(self.choice(f["stub"]), x, y, base_z, yaw)
            return
        for k in range(storeys):
            if k == storeys - 1 and self.rng.random() < 0.8:
                mesh = self.choice(f["top"])
            elif k == 0 and ground_floor_door:
                mesh = self.choice(f["door"])
            else:
                mesh = self.choice(f["window"]) if self.rng.random() < 0.6 else self.choice(f["solid"])
            self.add(mesh, x, y, base_z + k * MODULE, yaw)

    def wall_run(self, fam, x0, y0, dx, dy, count, heights, sink=25.0):
        """Modules from (x0, y0) along the unit direction (dx, dy); heights[i] storeys per module."""
        yaw = math.degrees(math.atan2(dy, dx))
        for i in range(count):
            x = x0 + dx * MODULE * i
            y = y0 + dy * MODULE * i
            mid_z = self.z(x + dx * MODULE / 2.0, y + dy * MODULE / 2.0)
            self.wall_column(fam, x, y, yaw, int(heights[i]), mid_z - sink,
                             ground_floor_door=self.rng.random() < 0.08)

    def damage_profile(self, count, top, collapse):
        """Storeys per module round a perimeter: smooth noise, with collapsed stretches."""
        phase = self.rng.uniform(0.0, 6.28, 3)
        t = np.arange(count) / max(count, 1) * 2.0 * math.pi
        wave = 0.5 + 0.28 * np.sin(t + phase[0]) + 0.2 * np.sin(2.0 * t + phase[1]) + 0.14 * np.sin(5.0 * t + phase[2])
        wave += self.rng.normal(0.0, 0.12, count)
        h = np.clip(np.round(wave * (top + 0.6)), 0, top).astype(int)
        h[wave < collapse] = 0
        return h

    # ---- a ruined building shell --------------------------------------------------------------------------------
    def building(self, cx, cy, nu, nv, ang, top, collapse, fam, interior=True):
        """nu x nv modules, centred on (cx, cy), long sides along the grid direction `ang`."""
        cu, su = math.cos(ang), math.sin(ang)
        hu, hv = nu * MODULE / 2.0, nv * MODULE / 2.0

        def world(u, v):
            return cx + u * cu - v * su, cy + u * su + v * cu

        profile = self.damage_profile(2 * (nu + nv), top, collapse)
        k = 0
        sides = [((-hu, -hv), (1.0, 0.0), nu), ((hu, -hv), (0.0, 1.0), nv), ((hu, hv), (-1.0, 0.0), nu), ((-hu, hv), (0.0, -1.0), nv)]
        for (u0, v0), (du, dv), count in sides:
            x0, y0 = world(u0, v0)
            dx, dy = du * cu - dv * su, du * su + dv * cu
            self.wall_run(fam, x0, y0, dx, dy, count, profile[k:k + count])
            k += count
        if interior:
            area = nu * nv
            piles = 1 + (area > 16) + (area > 30)
            for _ in range(piles):
                u = self.rng.uniform(-hu * 0.5, hu * 0.5)
                v = self.rng.uniform(-hv * 0.5, hv * 0.5)
                x, y = world(u, v)
                mesh = self.choice(self.kit.rubble_big if (min(nu, nv) >= 5 and self.rng.random() < 0.4) else self.kit.rubble_mid)
                self.add(mesh, x, y, self.z(x, y) - 12.0, self.rng.uniform(0.0, 360.0), scale=self.rng.uniform(0.75, 1.05))
            if top >= 2 and self.rng.random() < 0.5:
                # an inner cross wall
                u = round(self.rng.uniform(-hu * 0.4, hu * 0.4) / MODULE) * MODULE
                x0, y0 = world(u, -hv)
                self.wall_run(fam, x0, y0, -su, cu, nv, self.damage_profile(nv, max(1, top - 1), collapse + 0.1))
        self.footprints.append((cx, cy, hu, hv, ang, top))

    # ---- city fabric --------------------------------------------------------------------------------------------
    def zone(self, x, y):
        """(density, max storeys, collapse threshold, families) for a building site."""
        g = self.gen
        inside = (g.spec.xmin < x < g.spec.xmax) and (g.spec.ymin < y < g.spec.ymax)
        if not inside:
            return 0.22, 4, 0.22, ("Brick", "Concrete", "Cinderblock")
        d_crater = math.hypot(x - g.spec.by_id["crater"].geom.centre[0], y - g.spec.by_id["crater"].geom.centre[1])
        if d_crater < 7500.0:
            return 0.0, 0, 1.0, ("Brick",)
        if self.in_region("blast_zone", x, y):
            return 0.75, 1, 0.48, ("Brick", "Concrete")       # flattened: stubs and waist-high walls
        if any(self.in_region(r.id, x, y) for r in g.spec.regions if r.id.startswith("field_")):
            return 1.0, 3, 0.3, ("Brick", "Brick", "Concrete")
        if self.in_region("north_ruin_belt", x, y) or self.in_region("moabit_ruins", x, y):
            return 0.9, 3, 0.34, ("Brick", "Cinderblock", "Concrete")
        if self.in_region("zoo_grounds", x, y):
            return 0.4, 1, 0.45, ("Brick",)
        if self.in_region("east_ruins", x, y):
            return 0.55, 2, 0.3, ("Brick", "Brick", "Cinderblock")   # light ruins: fewer shells, more standing wall
        return 0.85, 3, 0.36, ("Brick", "Brick", "Concrete", "Cinderblock")

    def city(self):
        cu, su = math.cos(GRID_ANGLE), math.sin(GRID_ANGLE)
        reach_u = tg.SIZE_X / 2.0 + tg.SIZE_Y / 2.0
        pitch_u, pitch_v = 1250.0, 1150.0
        count = 0
        for iu in range(int(-reach_u / pitch_u), int(reach_u / pitch_u) + 1):
            for iv in range(int(-reach_u / pitch_v), int(reach_u / pitch_v) + 1):
                u = iu * pitch_u + self.rng.uniform(-150.0, 150.0)
                v = iv * pitch_v + self.rng.uniform(-130.0, 130.0)
                x = u * cu - v * su
                y = u * su + v * cu
                # backdrop ruins reach 70 m past the play edge, not further
                if abs(x) > self.spec.xmax + 5500.0 or abs(y) > self.spec.ymax + 5500.0:
                    continue
                density, top, collapse, fams = self.zone(x, y)
                if self.rng.random() > density:
                    continue
                nu = int(self.rng.choice([3, 4, 5, 5, 6]))
                nv = int(self.rng.choice([3, 3, 4, 5]))
                ang = GRID_ANGLE + (math.pi / 2.0 if self.rng.random() < 0.3 else 0.0)
                free, cells = self.rect_free(x, y, nu * MODULE / 2.0, nv * MODULE / 2.0, ang)
                if not free:
                    nu, nv = 3, 3
                    free, cells = self.rect_free(x, y, nu * MODULE / 2.0, nv * MODULE / 2.0, ang, margin=80.0)
                    if not free:
                        continue
                storeys = max(1, int(self.rng.integers(max(1, top - 1), top + 1))) if top > 0 else 0
                self.building(x, y, nu, nv, ang, storeys, collapse, self.choice(list(fams)))
                self.occupied[cells] = True
                count += 1
        return count

    # ---- impassable wall cores and the station shell ------------------------------------------------------------
    @staticmethod
    def resample(points, step):
        pts = np.asarray(points, float)
        out = []
        for a, b in zip(pts[:-1], pts[1:]):
            length = float(np.hypot(*(b - a)))
            n = max(1, int(round(length / step)))
            t = (b - a) / length
            for i in range(n):
                out.append((a + (b - a) * (i / n), t))
        return out

    def cores(self):
        g = self.gen
        gap = tg.soft(g.out_masks["core_gap"], 1) > 0.08
        total = 0
        for reg in self.spec.regions:
            if not reg.id.startswith("core_"):
                continue
            fam = self.choice(["Brick", "Concrete"])
            samples = self.resample(reg.geom.pts, MODULE)
            n = len(samples)
            for side in (-1.0, 1.0):
                heights = np.maximum(self.damage_profile(n, 4, -1.0), 2)
                for i, (p, t) in enumerate(samples):
                    normal = np.array([-t[1], t[0]])
                    q = p + normal * side * 330.0
                    mid = q + t * MODULE / 2.0
                    if self.at(gap, mid[0], mid[1]) or not self.at(g.out_masks["ruin_core"], mid[0], mid[1]):
                        continue
                    yaw = math.degrees(math.atan2(t[1], t[0]))
                    self.wall_column(fam, q[0], q[1], yaw, int(heights[i]), self.z(mid[0], mid[1]) - 60.0)
                    total += 1
            for i, (p, t) in enumerate(samples):
                if i % 4:
                    continue
                if self.at(gap, p[0], p[1]) or not self.at(g.out_masks["ruin_core"], p[0], p[1]):
                    continue
                normal = np.array([-t[1], t[0]])
                q = p - normal * 300.0
                self.wall_run(fam, q[0], q[1], normal[0], normal[1], 2, [2 + int(self.rng.integers(0, 2))] * 2, sink=60.0)
                x, y = p + t * 600.0
                if self.at(g.out_masks["ruin_core"], x, y) and not self.at(gap, x, y):
                    self.add(self.choice(self.kit.rubble_mid), x, y, self.z(x, y) - 20.0, self.rng.uniform(0, 360))
            self.occupied |= tg.soft(g.masks[reg.id], 2) > 0.05
        # Hauptbahnhof shell: two long standing walls, the south one broken by the 6 m gate
        gate = self.gen.feature("hbf_gap_south").points[0]
        for pid in ("hbf_wall_n", "hbf_wall_s"):
            samples = self.resample(self.gen.path(pid).pts, MODULE)
            heights = np.maximum(self.damage_profile(len(samples), 4, -1.0), 2)
            for i, (p, t) in enumerate(samples):
                mid = p + t * MODULE / 2.0
                if pid == "hbf_wall_s" and math.hypot(mid[0] - gate[0], mid[1] - gate[1]) < 420.0:
                    continue
                self.wall_column("Concrete", p[0], p[1], math.degrees(math.atan2(t[1], t[0])), int(heights[i]),
                                 self.z(mid[0], mid[1]) - 25.0)
                if i % 3 == 0:
                    self.add(self.choice(self.kit.families["Concrete"]["pillar"]), p[0], p[1], self.z(p[0], p[1]) - 20.0)
                total += 1
        return total

    def gaps(self):
        """Infantry-only gaps get a tank blocker; the cut ends come from the sketch's opening."""
        for f in self.spec.features:
            if not f.is_opening or not str(f.raw.get("vehicle_class", "")).startswith("infantry"):
                continue
            p = f.points[0]
            d = f.dir if f.dir is not None else np.array([1.0, 0.0])
            self.tank_blockers.append({"label": "BER_TankBlock_" + f.id, "x": float(p[0]), "y": float(p[1]),
                                       "yaw": math.degrees(math.atan2(d[1], d[0])), "length": 2200.0,
                                       "width": float(f.width) + 500.0})

    def bollards(self):
        """Small-vehicle gaps: concrete posts leave one 3.45 m lane. The light-vehicle nav agent (radius 120) fits,
        the medium (160) and heavy (200) ones do not; there is no nav area that blocks only those two."""
        clear_half = 172.0
        posts = self.kit.families["Concrete"]["pillar"][:4]
        count = 0
        for f in self.spec.features:
            if not f.is_opening or not str(f.raw.get("vehicle_class", "")).startswith("small") or f.dir is None:
                continue
            p, d = np.asarray(f.points[0], float), np.asarray(f.dir, float)
            normal = np.array([-d[1], d[0]])
            # On a causeway (sinkhole gap, bridge) the lane has to sit on the middle of the ground that is really there.
            solid = [t for t in np.arange(-700.0, 701.0, 20.0) if self.z(*(p + normal * t)) > -110.0]
            if solid and (max(solid) - min(solid)) < 1150.0:
                p = p + normal * (max(solid) + min(solid)) / 2.0
            reach = float(f.width) / 2.0 + (1300.0 if f.id.startswith("hbf") else 260.0)
            for side in (-1.0, 1.0):
                off = clear_half + 25.0
                while off <= reach:
                    q = p + normal * side * off
                    self.add(self.choice(posts), q[0], q[1], self.z(q[0], q[1]) - 140.0, self.rng.uniform(0.0, 360.0),
                             pitch=self.rng.uniform(-4.0, 4.0))
                    off += 85.0
                    count += 1
        return count

    # ---- rubble fields, street litter ---------------------------------------------------------------------------
    def rubble(self):
        g = self.gen
        fields = g.out_masks["ruin_field"]
        b = self.spec
        n_field = n_flat = 0
        for _ in range(9000):
            x = self.rng.uniform(b.xmin + 300, b.xmax - 300)
            y = self.rng.uniform(b.ymin + 300, b.ymax - 300)
            r, c = self.rc(x, y)
            if self.occupied[r, c]:
                continue
            if fields[r, c] and self.rng.random() < 0.55:
                # collision rubble between the shells: infantry weave through, vehicles do not
                self.add(self.choice(self.kit.rubble_mid), x, y, self.z(x, y) - 15.0, self.rng.uniform(0, 360),
                         scale=self.rng.uniform(0.6, 0.95))
                self.occupied[max(r - 3, 0):r + 4, max(c - 3, 0):c + 4] = True
                n_field += 1
        return n_field, n_flat

    # ---- Tiergarten ---------------------------------------------------------------------------------------------
    def tiergarten(self):
        g = self.gen
        park = g.region("tiergarten") & ~(tg.soft(g.out_masks["landmark_pad"], 5) > 0.02)
        park &= ~(self.lane_dist < 900.0) & ~(tg.soft(g.out_masks["crater"], 6) > 0.02)
        clump = tg.fbm(301, 22.0, 2)
        b = self.spec
        trees = logs = 0
        placed = []
        tries = 0
        while trees < 170 and tries < 20000:
            tries += 1
            x = self.rng.uniform(b.xmin, b.xmax)
            y = self.rng.uniform(b.ymin, b.ymax)
            r, c = self.rc(x, y)
            if not park[r, c] or clump[r, c] < self.rng.uniform(-0.5, 0.6):
                continue
            if any(math.hypot(x - px, y - py) < 450.0 for px, py in placed):
                continue
            placed.append((x, y))
            kind = "BP_Scorch_Dest_DeadWillow" if self.rng.random() < 0.25 else "BP_Scorch_Dest_DeadTree"
            s = self.rng.uniform(0.85, 1.25)
            self.actor("BER_Tree_%03d" % trees, "Berlin/Tiergarten", x, y, self.z(x, y) - 10.0, self.rng.uniform(0, 360),
                       (s, s, s), bp=DEST + kind)
            trees += 1
        tries = 0
        while logs < 36 and tries < 5000:
            tries += 1
            x = self.rng.uniform(b.xmin, b.xmax)
            y = self.rng.uniform(b.ymin, b.ymax)
            r, c = self.rc(x, y)
            if not park[r, c] or any(math.hypot(x - px, y - py) < 500.0 for px, py in placed):
                continue
            placed.append((x, y))
            self.actor("BER_Log_%02d" % logs, "Berlin/Tiergarten", x, y, self.z(x, y), self.rng.uniform(0, 360),
                       bp=DEST + "NaturalCover/Logs/BP_Cover_DeadTreeLog_%02d" % int(self.rng.integers(1, 11)))
            logs += 1
        return trees, logs

    # ---- streets: wrecks and poles ------------------------------------------------------------------------------
    # ---- critic round 1: streets with edges, ruins that face them, rubble that belongs to something -------------------
    COVER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCover_"
    COVER_KERB = ["01_WallRun", "07_Doorway", "08_FacadePanels", "04_ToppledSlab", "10_MasonryBlock", "06_Breastwork"]
    COVER_YARD = ["02_CornerL", "09_CellarLip", "06_Breastwork", "03_HeapChunk", "05_FallenChimney", "10_MasonryBlock"]
    COVER_BLAST = ["03_HeapChunk", "05_FallenChimney", "09_CellarLip", "10_MasonryBlock"]

    def build_street_index(self):
        """Samples of every lane and the road (point, tangent, half width, path index) for nearest-street lookups."""
        pts, tans, halves, owner = [], [], [], []
        self.street_paths = [p for p in self.spec.paths if p.kind in ("lane", "road")]
        for k, p in enumerate(self.street_paths):
            for pt, t in self.resample(p.pts, 200.0):
                pts.append(pt)
                tans.append(t)
                halves.append(850.0 if p.kind == "lane" else 1150.0)
                owner.append(k)
        self.st_pts, self.st_tan = np.asarray(pts), np.asarray(tans)
        self.st_half, self.st_owner = np.asarray(halves), np.asarray(owner)

    def nearest_street(self, x, y, skip_owner=-1):
        d = np.hypot(self.st_pts[:, 0] - x, self.st_pts[:, 1] - y)
        if skip_owner >= 0:
            d = np.where(self.st_owner == skip_owner, 1e9, d)
        i = int(np.argmin(d))
        return float(d[i]), self.st_pts[i], self.st_tan[i]

    def site_angle(self, x, y):
        """Ruins within 30 m of a lane face it (their street front, local +X, toward the lane); deeper in the block they
        follow the side-street grid with a little jitter. One in ten stands a few degrees off, as shifted by the blast."""
        dist, pt, tan = self.nearest_street(x, y)
        if dist < 3000.0:
            ang = math.atan2(pt[1] - y, pt[0] - x) if dist > 300.0 else math.atan2(tan[1], tan[0]) + math.pi / 2.0
            # square it to the street: the direction to the nearest sample is already close to the street normal
            normal = math.atan2(tan[1], tan[0]) + math.pi / 2.0
            diff = (ang - normal + math.pi) % (2.0 * math.pi) - math.pi
            ang = normal if abs(diff) < math.pi / 2.0 else normal + math.pi
            ang += math.radians(self.rng.uniform(-3.0, 3.0))
        else:
            ang = GRID_ANGLE + math.pi / 2.0 * int(self.rng.integers(0, 4)) + math.radians(self.rng.uniform(-4.0, 4.0))
        if self.rng.random() < 0.1:
            ang += math.radians(self.rng.uniform(-14.0, 14.0))
        return ang

    def frontage(self):
        """Both sides of every lane get an edge: runs of facade slabs square to the street, and between the runs either
        a collapsed stretch (a berm of rubble heaps along the kerb line) or a short open gap, sometimes with a piece of
        rubble cover standing in it. Nothing is placed where another lane, a wall-core gap, a pad or a POI is near."""
        g = self.gen
        slabs = {"tall": [("B3_Facade4St", 1200.0), ("B4_Firewall", 1200.0), ("B2_Facade2St_A", 1200.0), ("B2_Facade2St_B", 1200.0)],
                 "mid": [("B2_Facade2St_A", 1200.0), ("B2_Facade2St_B", 1200.0), ("B1_Facade1St_A", 600.0), ("B1_Facade1St_B", 600.0)],
                 "low": [("B6_Stub_A", 900.0), ("B6_Stub_B", 900.0), ("B1_Facade1St_A", 600.0)]}
        counts = {"slab": 0, "berm": 0, "cover": 0}
        for owner, p in enumerate(self.street_paths):
            samples = self.resample(p.pts, 100.0)
            half = 850.0 if p.kind == "lane" else 1150.0
            for side in (-1.0, 1.0):
                i = int(self.rng.integers(5, 30))
                while i < len(samples) - 13:
                    pt, t = samples[i]
                    name = self.district(pt[0], pt[1])
                    if name in ("park", "rail") or not (abs(pt[0]) < self.spec.xmax - 500.0 and abs(pt[1]) < self.spec.ymax - 500.0):
                        i += 12
                        continue
                    klass = "low" if name == "blast" else ("mid" if name in ("east", "south", "north") else "tall")
                    roll = self.rng.random()
                    if roll < 0.52:                                   # a run of 2-5 facades
                        run_slabs = []
                        for _ in range(int(self.rng.integers(2, 6))):
                            piece, plen = self.choice(slabs[klass])
                            steps = int(plen / 100.0)
                            if i + steps >= len(samples):
                                break
                            mid_pt, mid_t = samples[i + steps // 2]
                            normal = np.array([-mid_t[1], mid_t[0]])
                            q = mid_pt + normal * side * (half + self.rng.uniform(0.0, 60.0))
                            if not self.frontage_ok(q, mid_t, plen, owner):
                                i += steps
                                break
                            # slab runs along its local Y; its street face (+X) looks at the lane
                            yaw = math.degrees(math.atan2(-normal[1] * side, -normal[0] * side))
                            self.ruin(piece, q[0], q[1], yaw + self.rng.uniform(-2.0, 2.0), "B")
                            self.mark(q, mid_t, plen / 2.0 - 40.0, 320.0)
                            run_slabs.append((q, mid_t, normal * side, plen))
                            counts["slab"] += 1
                            i += steps
                        self.run_depth(run_slabs, side, klass)
                        i += int(self.rng.integers(5, 14))            # gap before the next thing
                    elif roll < 0.82:                                 # a collapsed stretch: rubble berm along the kerb
                        length = int(self.rng.integers(10, 24))
                        j = i
                        while j < min(i + length, len(samples) - 1):
                            bp, bt = samples[j]
                            normal = np.array([-bt[1], bt[0]])
                            q = bp + normal * side * (half + self.rng.uniform(60.0, 260.0))
                            if self.frontage_ok(q, bt, 300.0, owner):
                                # heaps overlap along the berm crest: big and small, never one size in a row
                                self.add(self.choice(self.kit.rubble_mid), q[0], q[1], self.z(q[0], q[1]) - 28.0,
                                         math.degrees(math.atan2(bt[1], bt[0])) + self.rng.uniform(-25.0, 25.0),
                                         scale=float(self.rng.choice([0.55, 0.7, 0.9, 1.3, 1.7])))
                                self.mark(q, bt, 260.0, 260.0)
                                counts["berm"] += 1
                            j += int(self.rng.integers(2, 4))
                        i = j + int(self.rng.integers(3, 9))
                    else:                                             # open gap, half the time with rubble cover in it
                        if self.rng.random() < 0.55:
                            normal = np.array([-t[1], t[0]])
                            q = pt + normal * side * (half + 180.0)
                            if self.frontage_ok(q, t, 700.0, owner) and name != "blast":
                                # the piece's threat side (+X) looks west along the lane: the way the raid advances
                                forward = t if t[1] < 0 else -t
                                self.actor("BER_Cover_%03d" % (counts["cover"] + 500), "Berlin/Cover", q[0], q[1], self.z(q[0], q[1]),
                                           math.degrees(math.atan2(forward[1], forward[0])) + self.rng.uniform(-20.0, 20.0),
                                           bp=self.COVER + self.choice(self.COVER_KERB))
                                self.mark(q, t, 400.0, 400.0)
                                counts["cover"] += 1
                        i += int(self.rng.integers(8, 18))
        return counts

    RETURNS = [("B1_Facade1St_A", 600.0), ("B1_Facade1St_B", 600.0), ("B6_Stub_A", 900.0), ("B6_Stub_B", 900.0),
               ("B2_Facade2St_A", 1200.0), ("B2_Facade2St_B", 1200.0)]

    def run_depth(self, run_slabs, side, klass):
        """A facade run is the front of a building row, not a fence: return walls go back from its ends, and the 8-16 m
        behind it holds a filler ruin or a collapse. Round 3: the returns differ (6, 9 or 12 m, one in three runs has
        only one, one in five is a broken-off stub with its heap, none stands exactly square), so runs stop reading as
        staples of one size."""
        if len(run_slabs) < 2:
            return
        ends = [(-1.0, run_slabs[0]), (1.0, run_slabs[-1])]
        if self.rng.random() < 0.3:
            ends = [ends[int(self.rng.integers(2))]]
        for end, (q, t, out, plen) in ends:
            if self.rng.random() < 0.15:
                continue
            broken = klass == "low" or self.rng.random() < 0.2
            piece, rlen = self.choice(self.RETURNS[2:4] if broken else self.RETURNS)
            lean = math.radians(self.rng.uniform(-10.0, 10.0))
            d = np.array([out[0] * math.cos(lean) - out[1] * math.sin(lean), out[0] * math.sin(lean) + out[1] * math.cos(lean)])
            corner = q + t * end * (plen / 2.0 - 30.0)
            c = corner + d * (rlen / 2.0 + 40.0)
            far = corner + d * (rlen + 60.0)
            r0, c0 = self.rc(c[0], c[1])
            r1, c1 = self.rc(far[0], far[1])
            if self.hard[r0, c0] or self.hard[r1, c1] or self.occupied_objects[r1, c1] or self.lane_dist[r1, c1] < 780.0:
                continue
            # the return runs along the outward normal; its face (+X) looks away from the row, along the street
            yaw = math.degrees(math.atan2(t[1] * end, t[0] * end)) + math.degrees(lean)
            self.ruin(piece, c[0], c[1], yaw + self.rng.uniform(-3.0, 3.0), "B")
            self.mark(c, d, rlen / 2.0, 150.0)
            if broken:
                h = far + d * 120.0
                self.add(self.choice(self.kit.rubble_mid), h[0], h[1], self.z(h[0], h[1]) - 30.0, self.rng.uniform(0, 360),
                         scale=self.rng.uniform(0.55, 0.85))
                self.add(self.choice(self.kit.chunks), h[0] + self.rng.uniform(-150, 150), h[1] + self.rng.uniform(-150, 150),
                         self.z(h[0], h[1]) + 5.0, self.rng.uniform(0, 360), pitch=self.rng.uniform(-12, 12),
                         scale=self.rng.uniform(0.6, 1.0))
        mid_q, mid_t, out, _ = run_slabs[len(run_slabs) // 2]
        back = mid_q + out * self.rng.uniform(900.0, 1300.0)
        r, c = self.rc(back[0], back[1])
        if self.hard[r, c] or self.lane_dist[r, c] < 1500.0:
            return
        if self.rng.random() < 0.6:
            self.place_e(back[0], back[1], 2 if klass != "tall" else 3, 0.0)
        elif not self.occupied_objects[r, c]:
            self.collapsed_lot(back[0], back[1], math.degrees(math.atan2(-out[1], -out[0])))
            self.mark(back, mid_t, 450.0, 450.0)

    def frontage_ok(self, q, t, length, owner):
        r, c = self.rc(q[0], q[1])
        if self.hard[r, c] or self.occupied_objects[r, c]:
            return False
        for s in (-length / 2.0, length / 2.0):
            rr, cc = self.rc(q[0] + t[0] * s, q[1] + t[1] * s)
            if self.hard[rr, cc] or self.occupied_objects[rr, cc]:
                return False
        other, _, _ = self.nearest_street(q[0], q[1], skip_owner=owner)
        return other > 1500.0 and self.gen.out_masks["side_street"][r, c] == 0

    def mark(self, q, t, half_len, half_depth):
        """Block a strip (along t) for later placements."""
        for s in np.arange(-half_len, half_len + 1.0, 90.0):
            for d in np.arange(-half_depth, half_depth + 1.0, 90.0):
                r, c = self.rc(q[0] + t[0] * s - t[1] * d, q[1] + t[1] * s + t[0] * d)
                self.occupied[r, c] = True
                self.occupied_objects[r, c] = True

    def heap_mass(self, x, y, big=False):
        """A mass of loose rubble against a wall: one heap, a few concrete chunks and tilted slabs, one or two small
        heaps. Round 3: the brick heap mesh is at most a third of a mass and stays under scale 1.1 (several big heaps
        of one mesh read as pink cauliflower); only a hero's main heap is larger."""
        base = self.rng.uniform(1.15, 1.5) if big else self.rng.uniform(0.6, 1.1)
        parts = [("heap", 0.0, base)]
        for _ in range(int(self.rng.integers(2, 5)) if big else int(self.rng.integers(1, 4))):
            parts.append(("chunk", self.rng.uniform(120.0, 300.0) * max(base, 0.8), self.rng.uniform(0.6, 1.15)))
        for _ in range(int(self.rng.integers(1, 3)) if big else int(self.rng.integers(0, 2))):
            parts.append(("heap", self.rng.uniform(160.0, 330.0) * max(base, 0.8), self.rng.uniform(0.5, 0.8)))
        if self.rng.random() < 0.5:
            parts.append(("slab", self.rng.uniform(100.0, 260.0), self.rng.uniform(0.6, 0.9)))
        placed = 0
        for kind, rad, scale in parts:
            ang = self.rng.uniform(0.0, 2.0 * math.pi)
            hx, hy = x + math.cos(ang) * rad, y + math.sin(ang) * rad
            r, c = self.rc(hx, hy)
            if self.lane_dist[r, c] < 780.0 or self.road_dist[r, c] < 1100.0 or self.hard[r, c]:
                continue
            if kind == "heap":
                mesh = self.choice(self.kit.rubble_big if (big and rad == 0.0) else self.kit.rubble_mid)
                self.add(mesh, hx, hy, self.z(hx, hy) - self.rng.uniform(20.0, 40.0), self.rng.uniform(0, 360), scale=scale)
            elif kind == "chunk":
                self.add(self.choice(self.kit.chunks), hx, hy, self.z(hx, hy) - 5.0, self.rng.uniform(0, 360),
                         pitch=self.rng.uniform(-14, 14), scale=scale)
            else:
                self.add(self.choice(self.kit.slabs), hx, hy, self.z(hx, hy) + 25.0, self.rng.uniform(0, 360),
                         pitch=self.rng.uniform(8, 22), roll=self.rng.uniform(-8, 8), scale=scale)
            self.ruin_mask[r, c] = True
            placed += 1
        return placed

    # ---- modelled footprint skirts (owner's pick A "Schuttschuerze"; ruinkit/footprint_table.json) -------------------
    def foot(self, key, x, y, z, yaw, dead=False, alternate=False):
        """The skirt meshes of one kit piece or plot at its pivot. No collision, no nav effect: they only seat the ruin
        in its own rubble. The 'dead' entries are the collapse heaps (used here for lots that are already down)."""
        entry = self.foot_table[key]
        items = entry["dead"] if dead else (entry["alternates"] if alternate and "alternates" in entry else entry["footprints"])
        ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        bucket = int(self.rng.integers(0, 4))
        for it in items:
            self.feet.append([it["mesh"], round(x + it["x"] * ca - it["y"] * sa, 1), round(y + it["x"] * sa + it["y"] * ca, 1),
                              round(z, 1), round((yaw + it["yaw"]) % 360.0, 2), it.get("scale", 1.0) if dead else 1.0, bucket])

    def fallen_building(self, entry, back, gap_near):
        """A lone standing wall is what is left of a building: the rest of it lies behind the wall, where the house
        stood. The plot's rubble ring goes there (the wall is its front), on about half of them with the collapse heap,
        plus chunks and litter. Skipped where another ruin, a lane or a gap already has that ground."""
        long_wall = entry["piece"][:2] in ("B2", "B3", "B4")
        depth = 450.0 if long_wall else 400.0
        bx, by = math.cos(back), math.sin(back)
        cx, cy = entry["x"] + bx * depth, entry["y"] + by * depth
        half_w = 500.0 if long_wall else 330.0
        for along, deep in ((0.0, 0.0), (-half_w, 0.0), (half_w, 0.0), (0.0, 350.0), (-half_w, 350.0), (half_w, 350.0)):
            px, py = cx - by * along + bx * deep, cy + bx * along + by * deep
            r, c = self.rc(px, py)
            why = "ruin" if self.body_mask[r, c] else "hard" if self.hard[r, c] else "gap" if gap_near[r, c] else                 "lane" if self.lane_dist[r, c] < 800.0 else "road" if self.road_dist[r, c] < 1100.0 else None
            if why:
                self.fallen_skips[why + (" at the wall" if deep == 0.0 and along == 0.0 else "")] =                     self.fallen_skips.get(why + (" at the wall" if deep == 0.0 and along == 0.0 else ""), 0) + 1
                return 0
        z, yaw = entry["z"], entry["yaw"]
        key = "B_PLOT" if long_wall else "B_PLOT_STUB"
        self.foot(key, cx, cy, z, yaw, alternate=self.rng.random() < 0.5)
        heap = 0
        if self.rng.random() < 0.5:
            # the heap lies off-centre and turned, so two walls in a row do not get the same pile
            hx, hy = cx - by * self.rng.uniform(-150.0, 150.0) + bx * 80.0, cy + bx * self.rng.uniform(-150.0, 150.0) + by * 80.0
            mesh = "SM_BerlinFoot_CollapseHeap_L" if long_wall and self.rng.random() < 0.5 else "SM_BerlinFoot_CollapseHeap_S"
            scale = self.rng.uniform(0.7, 0.9) if mesh.endswith("_L") else self.rng.uniform(0.8, 1.0)
            self.feet.append([mesh, round(hx, 1), round(hy, 1), round(z, 1),
                              round((yaw + 180.0 + self.rng.uniform(-35.0, 35.0)) % 360.0, 2), round(scale, 2),
                              int(self.rng.integers(0, 4))])
            heap = 1
        for _ in range(int(self.rng.integers(2, 6))):
            px = cx - by * self.rng.uniform(-half_w, half_w) + bx * self.rng.uniform(-300.0, 380.0)
            py = cy + bx * self.rng.uniform(-half_w, half_w) + by * self.rng.uniform(-300.0, 380.0)
            self.add(self.choice(self.kit.chunks + self.kit.slabs[:4]), px, py, self.z(px, py) + 15.0, self.rng.uniform(0, 360),
                     pitch=self.rng.uniform(-14, 14), roll=self.rng.uniform(-8, 8), scale=self.rng.uniform(0.55, 1.05))
        for along in np.arange(-half_w, half_w + 1.0, 90.0):
            for deep in np.arange(-380.0, 400.0, 90.0):
                r, c = self.rc(cx - by * along + bx * deep, cy + bx * along + by * deep)
                self.ruin_mask[r, c] = True
                self.body_mask[r, c] = True
        return heap

    def collapsed_lot(self, x, y, yaw):
        """A building that is fully down: the rubble ring of its plot with its collapse heap, and a few chunks."""
        z = self.z(x, y)
        if self.rng.random() < 0.5:
            self.foot("B_PLOT", x, y, z, yaw, alternate=self.rng.random() < 0.5)
            self.foot("B_PLOT", x, y, z, yaw, dead=True)
        else:
            self.foot("B_PLOT_STUB", x, y, z, yaw)
            self.foot("B_PLOT_STUB", x, y, z, yaw, dead=True)
        for _ in range(int(self.rng.integers(1, 4))):
            cx2, cy2 = x + self.rng.uniform(-420, 420), y + self.rng.uniform(-420, 420)
            self.add(self.choice(self.kit.chunks), cx2, cy2, self.z(cx2, cy2) + 20.0, self.rng.uniform(0, 360),
                     pitch=self.rng.uniform(-12, 12), scale=self.rng.uniform(0.6, 1.1))
        r, c = self.rc(x, y)
        self.ruin_mask[max(r - 6, 0):r + 7, max(c - 6, 0):c + 7] = True
        self.body_mask[max(r - 6, 0):r + 7, max(c - 6, 0):c + 7] = True

    def carry_over(self):
        """Round 3 keeps what the critic singled out as good: the hero ruins where they stood, the street events (car
        pile-ups with their wrecks, pole runs, hedgehogs), the knocked-out tanks off the lanes and the ground marks
        round those. They are read back from the round-2 dressing instead of being rolled again."""
        if not os.path.exists(CARRY_FILE):
            return
        with open(CARRY_FILE) as handle:
            old = json.load(handle)
        keep_pts = []
        for entry in old["ruins"]:
            if not entry["piece"].startswith(("E1", "E2")):
                continue
            hx, hy, ox, oy = self.KIT[entry["piece"]]
            ang = math.radians(entry["yaw"])
            cx = entry["x"] + ox * math.cos(ang) - oy * math.sin(ang)
            cy = entry["y"] + ox * math.sin(ang) + oy * math.cos(ang)
            free, cells = self.rect_free(cx, cy, hx, hy, ang, margin=90.0)
            if cells is None:
                continue
            self.ruins.append(entry)
            self.foot(entry["piece"], entry["x"], entry["y"], entry["z"], entry["yaw"])
            self.occupied[cells] = True
            self.occupied_objects[cells] = True
            self.ruin_mask[cells] = True
            self.body_mask[cells] = True
            self.footprints.append((cx, cy, hx, hy, ang, 3))
            if abs(cx) < self.spec.xmax and abs(cy) < self.spec.ymax:
                keep_pts.append((cx, cy, 2600.0))
        self.carried_heroes = len(self.ruins)
        for mesh, items in old["ism"].items():
            if "/PostApoTown/Cars/" in mesh:
                self.ism.setdefault(mesh, []).extend(items)
        for a in old["actors"]:
            if a["label"].startswith(("BER_PileupWreck", "BER_Pole_", "BER_Hedgehog", "BER_StreetWreck")):
                self.actors.append(a)
                r, c = self.rc(a["x"], a["y"])
                if "Wreck" in a["label"]:
                    self.occupied[max(r - 4, 0):r + 5, max(c - 4, 0):c + 5] = True
                    self.occupied_objects[max(r - 3, 0):r + 4, max(c - 3, 0):c + 4] = True
                    keep_pts.append((a["x"], a["y"], 150.0))
        for d in old["decals"]:
            if any(math.hypot(d["x"] - px, d["y"] - py) < rad for px, py, rad in keep_pts):
                self.decals.append(d)
        self.carried = True

    def junction_heroes(self):
        """Where a side street meets a lane a designer puts the landmark: half of those corners get a corner tenement
        (E1), turned to the lane. Placed before the frontages, so the facade runs start and end against them."""
        side = self.gen.out_masks["side_street"]
        placed, last = 0, None
        for owner, p in enumerate(self.street_paths):
            samples = self.resample(p.pts, 100.0)
            half = 850.0 if p.kind == "lane" else 1150.0
            was = True
            for pt, t in samples:
                now = self.at(side, pt[0], pt[1])
                rising = now and not was
                was = now
                if not rising or self.rng.random() > 0.5:
                    continue
                if last is not None and math.hypot(pt[0] - last[0], pt[1] - last[1]) < 6000.0:
                    continue
                if self.district(pt[0], pt[1]) in ("park", "rail", "blast"):
                    continue
                n = np.array([-t[1], t[0]])
                for s_side, s_along in ((1.0, -1.0), (-1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)):
                    q = pt + t * (380.0 + s_along * 1500.0) + n * s_side * (half + 1100.0)
                    if abs(q[0]) > self.spec.xmax - 1200.0 or abs(q[1]) > self.spec.ymax - 1200.0:
                        continue
                    if self.place_e(q[0], q[1], 3, 0.0, force="E1_CornerTower"):
                        placed += 1
                        last = pt
                        break
        return placed

    def skirts(self):
        """What lies round each ruin. Pieces with a modelled footprint skirt already stand in their own rubble, so they
        get only flat litter (heroes: a spill of chunks too); a lone facade slab gets a small rubble mass against its
        back. Round 3: no rubble mass lies away from a wall any more."""
        g = self.gen
        gap_near = tg.soft(g.out_masks["core_gap"] | g.out_masks["sink_gap"], 6) > 0.01
        foot = {"blast": "crack", "mitte": "debris", "north": "debris", "east": "dirt", "south": "puddle", "rail": "dirt",
                "park": "dirt"}
        heaps = litter = 0
        for entry in list(self.ruins):
            piece = entry["piece"]
            if piece.startswith(("B8", "E3", "B7")):
                continue
            x, y = entry["x"], entry["y"]
            if not (abs(x) < self.spec.xmax - 300.0 and abs(y) < self.spec.ymax - 300.0):
                continue
            skirted = piece in self.foot_table or entry.get("plot")
            hero = piece.startswith(("E1", "E2"))
            reach = 700.0 if hero else (350.0 if piece.startswith("B") else 450.0)
            back = math.radians(entry["yaw"]) + math.pi          # a slab's street face is its +X: rubble lies behind it
            fall = back + self.rng.uniform(-0.6, 0.6) if not skirted else self.rng.uniform(0.0, 2.0 * math.pi)
            if not skirted and abs(entry["z"] - self.z(x, y)) < 5.0:
                heaps += self.fallen_building(entry, back, gap_near)
            if hero:
                for _ in range(int(self.rng.integers(4, 8))):
                    ang, dist = fall + self.rng.normal(0.0, 0.45), reach + self.rng.uniform(350.0, 900.0)
                    cx2, cy2 = x + math.cos(ang) * dist, y + math.sin(ang) * dist
                    r, c = self.rc(cx2, cy2)
                    if self.lane_dist[r, c] < 700.0 or self.hard[r, c] or gap_near[r, c]:
                        continue
                    self.add(self.choice(self.kit.chunks), cx2, cy2, self.z(cx2, cy2) - 8.0, self.rng.uniform(0, 360),
                             pitch=self.rng.uniform(-10, 10), scale=self.rng.uniform(0.5, 1.0))
            for _ in range(int(self.rng.integers(2, 5))):
                ang = self.rng.uniform(0.0, 2.0 * math.pi)
                dist = reach + self.rng.uniform(0.0, 550.0)
                lx, ly = x + math.cos(ang) * dist, y + math.sin(ang) * dist
                r, c = self.rc(lx, ly)
                if self.lane_dist[r, c] < 450.0 or self.hard[r, c]:
                    continue
                self.add(self.choice(self.kit.rubble_flat[:6]), lx, ly, self.z(lx, ly) - 3.0, self.rng.uniform(0, 360),
                         scale=self.rng.uniform(0.8, 1.5))
                litter += 1
            if self.rng.random() < (0.45 if skirted else 0.7) and not (hero and self.carried):
                self.decal(foot[self.district(x, y)], x + math.cos(fall) * reach * 0.6, y + math.sin(fall) * reach * 0.6,
                           self.rng.uniform(1300.0, 2200.0))
        return heaps, litter

    def yard_cover(self):
        """Rubble cover away from the kerbs: in the open ground between ruins, blast-zone pieces round the crater."""
        b = self.spec
        placed, spots, tries = 0, [], 0
        while placed < 70 and tries < 9000:
            tries += 1
            x = self.rng.uniform(b.xmin + 900.0, b.xmax - 900.0)
            y = self.rng.uniform(b.ymin + 900.0, b.ymax - 900.0)
            r, c = self.rc(x, y)
            if self.occupied[r, c] or self.hard[r, c] or self.lane_dist[r, c] < 1300.0:
                continue
            name = self.district(x, y)
            if name in ("park", "rail") or tg.soft(self.occupied_objects, 4)[r, c] < 0.02:
                continue                                    # only next to something: cover belongs to a ruin
            if any(math.hypot(x - px, y - py) < 2200.0 for px, py in spots):
                continue
            if any(math.hypot(x - o[0], y - o[1]) < 2600.0 for o in self.openings_at):
                continue                                    # the approach to a gap stays clear (nav probe axis too)
            spots.append((x, y))
            self.actor("BER_Cover_%03d" % placed, "Berlin/Cover", x, y, self.z(x, y), self.rng.uniform(0, 360),
                       bp=self.COVER + self.choice(self.COVER_BLAST if name == "blast" else self.COVER_YARD))
            self.occupied[max(r - 4, 0):r + 5, max(c - 4, 0):c + 5] = True
            placed += 1
        return placed

    def streets(self):
        """Street props come as events, not as evenly spaced tokens: a pile-up of cars (sometimes round a burnt-out
        tank), then a long empty stretch; poles on one side per stretch, some fallen; pushed-aside steel hedgehogs."""
        cars = poles = hogs = 0
        near_objects = tg.soft(self.occupied_objects, 3) > 0.03
        wreck_bps = ["Military/DestTanks/BP_Env_DestroyedPzIII", "Military/DestTanks/BP_Env_DestroyedT34",
                     "Military/DestTanks/BP_Env_DestroyedPzII"]
        for p in self.spec.paths:
            if p.kind not in ("lane", "road"):
                continue
            samples = self.resample(p.pts, 100.0)
            half = 560.0 if p.kind == "lane" else 800.0
            i = int(self.rng.integers(40, 120))
            pole_side = 1.0
            while i < len(samples) - 5:
                pt, t = samples[i]
                normal = np.array([-t[1], t[0]])
                kind = self.rng.random()
                if not self.clear_spot(pt[0], pt[1]) or self.district(pt[0], pt[1]) == "park":
                    i += 40
                    continue
                if kind < 0.45:                                   # pile-up: 2-4 cars nose to tail on one verge
                    side = 1.0 if self.rng.random() < 0.5 else -1.0
                    n = int(self.rng.integers(2, 5))
                    for k in range(n):
                        q = pt + t * (k * 520.0 + self.rng.uniform(-60.0, 60.0)) + normal * side * (half + self.rng.uniform(-30.0, 90.0))
                        if self.clear_spot(q[0], q[1]):
                            self.add(self.choice(self.kit.cars), q[0], q[1], self.z(q[0], q[1]),
                                     math.degrees(math.atan2(t[1], t[0])) + 90.0 + self.rng.uniform(-28.0, 28.0),
                                     roll=self.rng.uniform(-5.0, 5.0))
                            cars += 1
                    if self.rng.random() < 0.35:
                        q = pt + t * (n * 520.0 + 300.0) + normal * side * (half + 120.0)
                        if self.clear_spot(q[0], q[1]):
                            self.actor("BER_PileupWreck_%02d" % cars, "Berlin/Streets", q[0], q[1], self.z(q[0], q[1]),
                                       math.degrees(math.atan2(t[1], t[0])) + self.rng.uniform(-40.0, 40.0),
                                       bp=DEST + self.choice(wreck_bps))
                            self.decal("crater", q[0], q[1], self.rng.uniform(800.0, 1100.0))
                elif kind < 0.8:                                  # a run of poles on one side, some down
                    pole_side = -pole_side
                    for k in range(int(self.rng.integers(3, 6))):
                        j = i + k * int(self.rng.integers(25, 35))
                        if j >= len(samples):
                            break
                        pp, ptan = samples[j]
                        pn = np.array([-ptan[1], ptan[0]])
                        q = pp + pn * pole_side * (half + 190.0)
                        if not self.clear_spot(q[0], q[1]):
                            continue
                        rr, cc = self.rc(q[0] + pn[0] * pole_side * 250.0, q[1] + pn[1] * pole_side * 250.0)
                        if not near_objects[rr, cc]:
                            continue          # a lamp alone in open ground reads as a token: only beside a facade or berm
                        name = self.choice(["Civilian/Poles/BP_Civilian_Dest_CrushablePoleLight",
                                            "Civilian/Poles/BP_Civilian_Dest_CrushablePole_2"])
                        self.actor("BER_Pole_%03d" % poles, "Berlin/Streets", q[0], q[1], self.z(q[0], q[1]),
                                   math.degrees(math.atan2(pn[1] * pole_side, pn[0] * pole_side)), bp=DEST + name)
                        poles += 1
                else:                                             # hedgehogs shoved to both verges
                    for side in (-1.0, 1.0):
                        for k in range(int(self.rng.integers(1, 4))):
                            q = pt + t * (k * 260.0) + normal * side * (half + 110.0 + self.rng.uniform(0.0, 120.0))
                            if self.clear_spot(q[0], q[1]):
                                self.actor("BER_Hedgehog_%03d" % hogs, "Berlin/Streets", q[0], q[1], self.z(q[0], q[1]),
                                           self.rng.uniform(0, 360), bp=DEST + "Military/Barricades/BP_Military_Dest_Hedgehog")
                                hogs += 1
                i += int(self.rng.integers(90, 210))
        return cars, poles

    def clear_spot(self, x, y):
        g = self.gen
        if not (self.spec.xmin + 300 < x < self.spec.xmax - 300 and self.spec.ymin + 300 < y < self.spec.ymax - 300):
            return False
        r, c = self.rc(x, y)
        m = g.out_masks
        if m["pads"][r, c] or m["landmark_pad"][r, c] or m["crater"][r, c] or m["sinkhole"][r, c] or m["sink_gap"][r, c]:
            return False
        if g.river_dist[r, c] < 2400.0 or tg.soft(m["core_gap"], 5)[r, c] > 0.01 or m["ruin_core"][r, c]:
            return False
        if g.rail_dist[r, c] < 500.0:
            return False
        return True

    # ---- set pieces (generic dressing only; the custom POI objects are placed by berlin_map_build) --------------
    def set_pieces(self):
        g = self.gen
        yard = self.spec.by_id["rail_yard"].geom
        cx, cy = yard.centre
        hx, hy = (yard.pts[:, 0].max() - yard.pts[:, 0].min()) / 2.0, (yard.pts[:, 1].max() - yard.pts[:, 1].min()) / 2.0
        folder = "Berlin/RailYard"
        # sidings: wagons are not in the project, so containers and the crane carry the staging look
        n = 0
        for row, offset in enumerate((-1500.0, -600.0, 900.0)):
            for k in range(4):
                y = cy - hy + 1500.0 + k * 2300.0 + self.rng.uniform(-300.0, 300.0)
                x = cx + offset + self.rng.uniform(-80.0, 80.0)
                if self.rng.random() < 0.3:
                    continue
                self.actor("BER_YardContainer_%02d" % n, folder, x, y, self.z(x, y), self.rng.uniform(-6.0, 6.0),
                           bp=DEST + "Industrial/BP_Industrial_Dest_Container")
                n += 1
        self.actor("BER_YardCrane", folder, cx + 1900.0, cy - 2500.0, self.z(cx + 1900.0, cy - 2500.0), 90.0,
                   bp=DEST + "Soviet/BP_Dest_SovietCrane_1")
        self.actor("BER_YardShed", folder, cx - 1700.0, cy + 3600.0, self.z(cx - 1700.0, cy + 3600.0), 0.0,
                   bp=DEST + "Industrial/BP_Industrial_Dest_Hangar")
        for k in range(3):
            x, y = cx + 2100.0, cy + 1500.0 + k * 700.0
            self.actor("BER_YardBarrels_%d" % k, folder, x, y, self.z(x, y), self.rng.uniform(0, 360),
                       bp=DEST + "Airport/BP_Airport_Dest_BarrelStack%s" % ("", "2", "3")[k])
        # perimeter fence with openings where the lane and the patrol loop cross
        fence = DEST + "Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceLong"   # about 10 m long along its Y
        k = 0
        for side in (-1.0, 1.0):
            for i in range(int(2 * hy / 1020.0)):
                y = cy - hy + 510.0 + i * 1020.0
                x = cx + side * (hx + 150.0)
                if self.lane_dist[self.rc(x, y)] < 1400.0 or i in (3, 7) or g.rail_dist[self.rc(x, y)] < 900.0:
                    continue
                self.actor("BER_YardFence_%02d" % k, folder, x, y, self.z(x, y), 0.0, bp=fence)
                k += 1
        self.power_station()
        # Alexanderplatz: a wrecked bus and tank wrecks from the last street fighting of 1945
        alex = g.feature("alexanderplatz").points[0]
        self.actor("BER_AlexBus", "Berlin/Streets", alex[0] + 900.0, alex[1] - 700.0, self.z(alex[0] + 900.0, alex[1] - 700.0),
                   35.0, bp=DEST + "Civilian/BP_Civilian_Dest_Bus")
        # the bus ran into a collapse and a car into the bus: a group, not a lone token on the square
        self.heap_mass(alex[0] + 1500.0, alex[1] - 250.0, big=True)
        self.add(self.choice(self.kit.cars), alex[0] + 330.0, alex[1] - 1150.0, self.z(alex[0] + 330.0, alex[1] - 1150.0), 118.0, roll=4.0)
        self.add(self.choice(self.kit.cars), alex[0] + 1250.0, alex[1] - 1300.0, self.z(alex[0] + 1250.0, alex[1] - 1300.0), 160.0)
        wrecks = [("Military/DestTanks/BP_Env_DestroyedPzIII", alex[0] - 1100.0, alex[1] + 1300.0, 120.0),
                  ("Military/DestTanks/BP_Env_DestroyedPzII", 9600.0, 39200.0, 200.0),
                  ("Military/DestTanks/BP_Env_DestroyedT34", 2800.0, 9500.0, 60.0),
                  ("Military/DestTanks/BP_Env_DestroyedPz38T", -11000.0, -12500.0, 310.0),
                  ("Military/DestTanks/BP_Env_Destroyed_KV1E", 14600.0, 4200.0, 15.0)]
        for i, (bp, x, y, yaw) in enumerate(wrecks):
            self.actor("BER_Wreck_%d" % i, "Berlin/Streets", x, y, self.z(x, y), yaw, bp=DEST + bp)
        # Moltke bridge: broken deck slabs on the rubble causeway, pier stumps in the water beside it
        bridge = g.feature("moltke_bridge")
        p, d = np.asarray(bridge.points[0]), np.asarray(bridge.dir)
        normal = np.array([-d[1], d[0]])
        yaw = math.degrees(math.atan2(d[1], d[0]))
        for i in range(-7, 8):
            for lane_off in (-150.0, 150.0):
                if self.rng.random() < 0.2:
                    continue
                q = p + d * (i * 300.0) + normal * lane_off
                self.add(self.choice(self.kit.slabs), q[0] - 150.0 * d[0], q[1] - 150.0 * d[1], self.z(q[0], q[1]) - 22.0,
                         yaw + self.rng.uniform(-4.0, 4.0), pitch=self.rng.uniform(-2.5, 2.5), roll=self.rng.uniform(-2.5, 2.5))
        for i in (-5, -2, 2, 5):
            for side in (-1.0, 1.0):
                q = p + d * (i * 300.0) + normal * side * 520.0
                self.add(self.choice(self.kit.families["Concrete"]["pillar"]), q[0], q[1], tg.RIVER_BED - 30.0,
                         self.rng.uniform(0, 360), pitch=self.rng.uniform(-8.0, 8.0), scale=2.2)
        # sinkholes: collapsed street slabs hanging into the pits, big debris on the floors
        sink = g.out_masks["sinkhole"]
        rim = sink & ~(tg.soft(sink, 2) > 0.8) & ~(tg.soft(g.out_masks["sink_gap"], 5) > 0.01)   # crossings stay clear
        rows, cols = np.nonzero(rim)
        pick = self.rng.choice(len(rows), size=min(70, len(rows)), replace=False)
        for idx in pick:
            x = tg.SIZE_X / 2.0 - rows[idx] * tg.QUAD_CM
            y = -tg.SIZE_Y / 2.0 + cols[idx] * tg.QUAD_CM
            self.add(self.choice(self.kit.slabs), x, y, self.z(x, y) + 60.0, self.rng.uniform(0, 360),
                     pitch=self.rng.uniform(-35.0, 35.0), roll=self.rng.uniform(-35.0, 35.0), scale=1.4)
        floor = tg.soft(sink, 3) > 0.97
        rows, cols = np.nonzero(floor)
        if len(rows):
            pick = self.rng.choice(len(rows), size=min(45, len(rows)), replace=False)
            for idx in pick:
                x = tg.SIZE_X / 2.0 - rows[idx] * tg.QUAD_CM
                y = -tg.SIZE_Y / 2.0 + cols[idx] * tg.QUAD_CM
                self.add(self.choice(self.kit.rubble_mid + self.kit.chunks), x, y, self.z(x, y) - 20.0,
                         self.rng.uniform(0, 360), scale=self.rng.uniform(0.8, 1.3))
        # Zoo tower debris field: thrown concrete round the (other session's) tower
        zoo = self.spec.by_id["zoo_debris"].geom
        pad = tg.soft(g.out_masks["landmark_pad"], 2) > 0.3
        n = 0
        for _ in range(400):
            ang = self.rng.uniform(0, 2 * math.pi)
            rad = self.rng.uniform(3300.0, 6200.0)
            x, y = zoo.centre[0] + rad * math.cos(ang), zoo.centre[1] + rad * math.sin(ang)
            if not self.clear_spot(x, y) or self.at(pad, x, y) or self.lane_dist[self.rc(x, y)] < 700.0 or y < -46300.0:
                continue
            if not (self.spec.xmin + 400 < x < self.spec.xmax - 400 and self.spec.ymin + 400 < y < self.spec.ymax - 400):
                continue
            self.add(self.choice(self.kit.chunks + self.kit.rubble_mid), x, y, self.z(x, y) - 12.0,
                     self.rng.uniform(0, 360), pitch=self.rng.uniform(-10, 10), scale=self.rng.uniform(0.7, 1.3))
            n += 1
            if n >= 55:
                break

    def power_station(self):
        """Ruined electrical substation near the start, laid out as a real compound (owner feedback 2026-11-13: the first
        version read as pieces thrown together).

        Local frame: u = east (+Y), v = north (+X). A walled yard 76 x 54 m on concrete slabs. North row: the switch hall in
        the middle with a tall gantry unit at each end. South row: four transformer bays at even spacing. An 8 m service
        road runs between the rows from the gate in the west wall. The high-voltage line comes in over the north wall on
        two big pylons; low-voltage poles leave along the south wall. Outside the gate, on the approach from
        Alexanderplatz, a knocked-out armour column: the scavenge wrecks, each with a parts pile beside it."""
        cx, cy = tg.POWER_STATION_AT
        hu, hv = tg.POWER_STATION_HALF[1], tg.POWER_STATION_HALF[0]
        folder = "Berlin/PowerStation"
        post = DEST + "PostApo/BP_Dest_PostApo_"
        scav = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/"
        ground = "/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/PostApoTown/BP_PostApo_BuildingPlateauRect"

        def put(label, bp, u, v, yaw=0.0, dz=0.0):
            x, y = cx + v, cy + u
            self.actor("BER_Power_" + label, folder, x, y, self.z(x, y) + dz, yaw, bp=bp)

        # yard slabs: 42 x 12.7 m plates, long side east-west, two columns by four rows
        for col, u in enumerate((-2105.0, 2105.0)):
            for row in range(4):
                put("Slab_%d%d" % (col, row), ground, u, -1908.0 + row * 1272.0, 90.0, dz=-6.0)
        # north row: switch hall flanked by the two gantry units
        put("SwitchHall", post + "MasterPwrStation1", 0.0, 1300.0, 90.0)
        put("Gantry_West", post + "PwrStation3", -2750.0, 1250.0, 0.0)
        put("Gantry_East", post + "PwrStation10", 2500.0, 900.0, 180.0)
        # south row: four transformer bays, evenly spaced, all square to the yard
        for k, unit in enumerate(("PwrStation5", "PwrStation9", "PwrStation7", "PwrStation2")):
            put("Bay_%d" % k, post + unit, -2550.0 + k * 1700.0, -1300.0, 0.0)
        # perimeter wall: 10.2 m panels; the gate is the middle of the west wall, two breaches elsewhere
        fence = DEST + "Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceLong"     # runs along its local Y
        n = 0
        for side, v in (("N", hv), ("S", -hv)):
            for i in range(7):
                if (side == "N" and i == 5) or (side == "S" and i == 1):
                    u = -hu + 510.0 + i * 1086.0
                    self.add(self.choice(self.kit.rubble_mid), cx + v, cy + u, self.z(cx + v, cy + u) - 12.0,
                             self.rng.uniform(0, 360))            # a breach: the panel is down, its rubble lies there
                    continue
                put("Wall_%02d" % n, fence, -hu + 510.0 + i * 1086.0, v, 0.0)
                n += 1
        for side, u in (("W", -hu), ("E", hu)):
            for i in range(5):
                if side == "W" and i == 2:
                    continue                                       # the gate
                put("Wall_%02d" % n, fence, u, -hv + 540.0 + i * 1080.0, 90.0)
                n += 1
        gate = DEST + "Military/Barricades/BP_Military_Dest_ConcreteBarrier"
        for k, v in enumerate((-620.0, 620.0)):
            put("GatePost_%d" % k, gate, -hu, v, 0.0)
        # yard furniture along the service road
        put("Barrels_0", DEST + "Airport/BP_Airport_Dest_BarrelStack2", -3100.0, -250.0, 15.0)
        put("Barrels_1", DEST + "Airport/BP_Airport_Dest_BarrelStack", 3200.0, 150.0, 200.0)
        put("Crates", DEST + "Airport/BP_Airport_Dest_CrateStack02", 3250.0, -700.0, 90.0)
        # the high-voltage line: two 40 m pylons outside the north wall, on the line of the gantries; one mast down inside
        put("Pylon_NW", DEST + "Industrial/Electricity/BP_Electricity_TransmissionLIne", -2750.0, hv + 800.0, 90.0)
        put("Pylon_NE", DEST + "Industrial/Electricity/BP_Electricity_TransmissionLIne", 2500.0, hv + 800.0, 90.0)
        put("Mast_Fallen", DEST + "Industrial/SteelTowers/BP_Dest_SteelTower_009", hu + 900.0, 1500.0, 80.0)
        # low-voltage wooden poles leaving along the south wall (the lattice "single poles" are 33 m towers: too much)
        for k in range(5):
            put("Pole_%d" % k, DEST + "Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric%d" % (k % 2 + 1),
                -3400.0 + k * 1700.0, -hv - 300.0, 0.0)
        # the knocked-out column on the approach road west of the gate: nose to tail, slewed off the road
        tanks = "/Game/RTS_Survival/Blueprints/Environment/Berlin/PowerStation/ScavObj_Berlin_"
        column = [("Tiger", "Tiger1_Parts", -4700.0, 150.0, 80.0), ("PzIV", "PZIV_Parts", -5600.0, -260.0, 110.0),
                  ("Stug", "Stug_Parts", -6500.0, 230.0, 62.0), ("Pz38t", "Pz38T_Parts", -7300.0, -180.0, 135.0)]
        for k, (label, bp, u, v, yaw) in enumerate(column):
            put("Wreck_" + label, tanks + bp, u, v, yaw)
            side = -1.0 if k % 2 else 1.0
            put("Parts_%d" % k, scav + "VehicleParts/ScavObj_VehicleParts%d" % (k + 1), u + 250.0, v + side * 520.0,
                self.rng.uniform(0, 360))
            self.decal("crater", cx + v, cy + u, self.rng.uniform(700.0, 900.0))
        put("Car_0", scav + "VehicleParts/Cars/ScavObj_PostApo_Car_2", -4150.0, -700.0, 20.0)
        put("Car_1", scav + "VehicleParts/Cars/ScavObj_PostApo_Car_5_Tracks", -6050.0, 650.0, 300.0)
        for k in range(5):
            self.decal("tracks", cx + self.rng.uniform(-150.0, 150.0), cy - 4300.0 - k * 800.0, 800.0, yaw=90.0)

    # ---- decals and small details: each district gets its own mix ----------------------------------------------------
    DECALS = {
        "debris": ["Concrete/MI_RTS_Decal_Concrete_Debris_01", "Concrete/MI_RTS_Decal_Concrete_Debris_02",
                   "Concrete/MI_RTS_Decal_Concrete_Debris_03", "Concrete/MI_RTS_Decal_Concrete_Debris_05",
                   "Concrete/MI_RTS_Decal_Concrete_Scatter_03"],
        # Crack2 is left out: it is a square slab outline, which the owner called very ugly (2026-11-13)
        "crack": ["Concrete/MI_RTS_Decal_Crack%d" % i for i in (1, 3, 4, 5, 6)],
        "crater": ["CraterDirt/MI_Decal_Crater_Dirt_%02d" % i for i in range(1, 7)],
        "puddle": ["CraterDirt/MI_Decal_Crater_Dirt_%02d_Water" % i for i in range(1, 7)],
        "tracks": ["DirtTracks/MI_Decal_Dirt_Tracks_01", "DirtTracks/MI_Decal_Dirt_Tracks_02",
                   "DirtTracks/MI_Decal_Dirt_Tracks_03"],
        "dirt": ["DirtTracks/MI_Decal_Dirt_01", "DirtTracks/MI_Decal_Dirt_02", "DirtTracks/MI_Decal_Dirt_03",
                 "DirtTracks/MI_Decal_Dirt_04", "DirtTracks/MI_Decal_Bank_01", "DirtTracks/MI_Decal_Bank_02"],
        "prints": ["DirtTracks/MI_Decal_Dirt_Prints_01", "DirtTracks/MI_Decal_Dirt_Prints_02",
                   "DirtTracks/MI_Decal_Dirt_Prints_03"],
        "manhole": ["Covers/ManholeCover/MI_RTS_Decal_Round_Manhole_Cover_ui5idbzcw_1K",
                    "Covers/ManholeCover/MI_RTS_Decal_Round_Manhole_Cover_ujbmedgn_1K",
                    "Covers/ManholeCover/MI_RTS_Decal_Round_Manhole_Cover_ukfoedrcw_1K"],
        "utility": ["Covers/RectUtility/MI_RTS_Decal_Metal_Utility_Cover_uiukdigg_1K",
                    "Covers/RectUtility/MI_RTS_Decal_Rusty_Metal_Utility_Cover_uhbnbh1n_1K",
                    "Covers/RectUtility/MI_RTS_Decal_Rusty_Metal_Utility_Cover_uirlehpn_1K"],
    }

    def decal(self, kind, x, y, size, yaw=None):
        self.decals.append({"mat": "/Game/RTS_Survival/Materials/Decals/" + self.choice(self.DECALS[kind]),
                            "kind": kind, "x": round(x, 1), "y": round(y, 1), "z": round(self.z(x, y), 1),
                            "size": round(size, 1), "yaw": round(self.rng.uniform(0.0, 360.0) if yaw is None else yaw, 1)})

    def district(self, x, y):
        """Which part of the city a point is in: each gets a different mix of ground marks."""
        g = self.gen
        if self.in_region("blast_zone", x, y):
            return "blast"
        if self.in_region("tiergarten", x, y) or self.in_region("zoo_grounds", x, y):
            return "park"
        if self.in_region("rail_yard", x, y) or g.rail_dist[self.rc(x, y)] < 1500.0:
            return "rail"
        if self.in_region("east_ruins", x, y):
            return "east"
        if self.in_region("moabit_ruins", x, y) or self.in_region("north_ruin_belt", x, y) or x > 14500.0:
            return "north"
        if x < -6500.0:
            return "south"          # Potsdamer Platz, the sinkholes, Anhalter Bf: wet, collapsed ground
        return "mitte"

    # per district: decal kind -> relative weight, and how many marks per hectare
    DISTRICT_MIX = {
        "blast": ({"crack": 6, "crater": 3, "debris": 1}, 34.0, (800.0, 1700.0)),
        "mitte": ({"debris": 6, "crack": 2, "crater": 2, "dirt": 1}, 30.0, (600.0, 1300.0)),
        "east": ({"dirt": 3, "tracks": 3, "prints": 2, "debris": 2}, 16.0, (500.0, 1000.0)),
        "north": ({"debris": 3, "dirt": 3, "crater": 3, "prints": 1}, 22.0, (600.0, 1200.0)),
        "south": ({"puddle": 6, "crack": 3, "debris": 2, "dirt": 1}, 30.0, (700.0, 1500.0)),
        "rail": ({"tracks": 4, "dirt": 3, "prints": 2, "puddle": 2}, 26.0, (600.0, 1100.0)),
        "park": ({"puddle": 4, "dirt": 4, "prints": 1}, 14.0, (700.0, 1400.0)),
    }

    def details(self):
        g = self.gen
        b = self.spec
        m = g.out_masks
        area_ha = (b.xmax - b.xmin) * (b.ymax - b.ymin) / 1e8
        blocked = m["sinkhole"] | m["crater"] | (g.river_dist < 2000.0) | m["landmark_pad"]
        counts = {}
        near_ruin = tg.soft(self.occupied_objects, 5) > 0.03
        # marks come in clusters with empty ground between them: one big mark and a few small ones round it
        centres, tries = [], 0
        while len(centres) < int(area_ha * 9.0) and tries < 20000:
            tries += 1
            x = self.rng.uniform(b.xmin + 400.0, b.xmax - 400.0)
            y = self.rng.uniform(b.ymin + 400.0, b.ymax - 400.0)
            r, c = self.rc(x, y)
            if blocked[r, c] or any(math.hypot(x - px, y - py) < 2600.0 for px, py in centres[-60:]):
                continue
            name = self.district(x, y)
            mix, per_ha, (lo, hi) = self.DISTRICT_MIX[name]
            if self.rng.random() > per_ha / 34.0:
                continue
            kinds = list(mix)
            weights = np.array([mix[k] for k in kinds], float)
            main = kinds[int(self.rng.choice(len(kinds), p=weights / weights.sum()))]
            # round 3: dirt, debris, puddle and crack marks only beside a ruin; free-floating they read as stamped blotches
            if main not in ("tracks", "prints", "crater") and name not in ("blast", "park", "rail") and not near_ruin[r, c]:
                continue
            centres.append((x, y))
            radius = self.rng.uniform(400.0, 1000.0)
            for k in range(int(self.rng.integers(3, 10))):
                kind = main if self.rng.random() < 0.7 else kinds[int(self.rng.choice(len(kinds), p=weights / weights.sum()))]
                size = hi * 1.3 if k == 0 else lo * self.rng.uniform(0.5, 1.2)
                ang, rad = self.rng.uniform(0, 2 * math.pi), (0.0 if k == 0 else self.rng.uniform(0.3, 1.0) * radius)
                dx, dy = x + math.cos(ang) * rad, y + math.sin(ang) * rad
                rr, cc = self.rc(dx, dy)
                if not blocked[rr, cc]:
                    self.decal(kind, dx, dy, size)
            counts[name] = counts.get(name, 0) + 1
        # shell craters where the shells fell: on the lanes, two or three overlapping
        for p in self.spec.paths:
            if p.kind != "lane":
                continue
            samples = self.resample(p.pts, 100.0)
            i = int(self.rng.integers(60, 200))
            while i < len(samples):
                pt, t = samples[i]
                r, c = self.rc(pt[0], pt[1])
                if g.inside[r, c] and not blocked[r, c] and self.district(pt[0], pt[1]) != "park":
                    # a stick of shells: centres along a line that crosses the lane at an angle, sizes 2-14 m, overlapping
                    line = self.rng.uniform(0.0, math.pi)
                    span = self.rng.uniform(2000.0, 4000.0)
                    # round 3: no rows of equal small blots. 3-5 craters of 4 m and more, at most one smaller, and the
                    # largest at least 2.5 times the smallest
                    n = int(self.rng.integers(3, 6))
                    sizes = [float(self.rng.choice([420.0, 550.0, 750.0, 1000.0, 1400.0], p=[0.3, 0.25, 0.2, 0.15, 0.1]))
                             for _ in range(n)]
                    if self.rng.random() < 0.5:
                        sizes[int(self.rng.integers(n))] = 300.0
                    big = int(self.rng.integers(n))
                    sizes[big] = max(sizes[big], 2.6 * min(sizes))
                    for k in range(n):
                        along = (k / (n - 1.0) - 0.5) * span + self.rng.uniform(-250.0, 250.0)
                        size = sizes[k]
                        cx2, cy2 = pt[0] + math.cos(line) * along, pt[1] + math.sin(line) * along
                        rr, cc = self.rc(cx2, cy2)
                        if not blocked[rr, cc]:
                            self.decal("crater", cx2, cy2, size)
                i += int(self.rng.integers(260, 520))
        # wheel and track ruts along the carriageways: stretches of 20-45 m, then a gap, so the street has a direction
        ruts = 0
        for p in self.spec.paths:
            if p.kind not in ("lane", "road"):
                continue
            samples = self.resample(p.pts, 100.0)
            i = int(self.rng.integers(5, 25))
            while i < len(samples):
                stretch = int(self.rng.integers(20, 46))
                offset = self.rng.uniform(-170.0, 170.0)
                for j in range(i, min(i + stretch, len(samples)), 4):
                    pt, t = samples[j]
                    r, c = self.rc(pt[0], pt[1])
                    if blocked[r, c] or not g.inside[r, c] or self.district(pt[0], pt[1]) == "park":
                        continue
                    q = pt + np.array([-t[1], t[0]]) * (offset + self.rng.uniform(-40.0, 40.0))
                    self.decals.append({"mat": "/Game/RTS_Survival/Materials/Decals/" + self.choice(self.DECALS["tracks"]),
                                        "kind": "rut", "x": round(q[0], 1), "y": round(q[1], 1), "z": round(self.z(q[0], q[1]), 1),
                                        "size": round(self.rng.uniform(620.0, 745.0), 1),      # 4 m apart: they overlap into one track
                                        # the tracks texture runs across the decal's yaw axis: +90 lays it along the lane
                                        "yaw": round(math.degrees(math.atan2(t[1], t[0])) + 90.0 + self.rng.uniform(-6.0, 6.0), 1)})
                    ruts += 1
                i += stretch + int(self.rng.integers(8, 22))
        counts["ruts"] = ruts
        # street furniture marks: manholes on the lane centre lines, utility covers at their edges
        covers = 0
        for p in self.spec.paths:
            if p.kind not in ("lane", "road"):
                continue
            dist, next_cover = 0.0, self.rng.uniform(800.0, 2500.0)
            for pt, t in self.resample(p.pts, 100.0):
                dist += 100.0
                if dist < next_cover:
                    continue
                next_cover = dist + self.rng.uniform(6000.0, 9000.0)
                r, c = self.rc(pt[0], pt[1])
                if blocked[r, c] or not g.inside[r, c] or self.district(pt[0], pt[1]) in ("park", "blast"):
                    continue
                normal = np.array([-t[1], t[0]])
                if self.rng.random() < 0.6:
                    q = pt + normal * self.rng.uniform(-120.0, 120.0)
                    self.decal("manhole", q[0], q[1], 95.0)
                else:
                    q = pt + normal * (350.0 if self.rng.random() < 0.5 else -350.0)
                    self.decal("utility", q[0], q[1], 110.0, yaw=math.degrees(math.atan2(t[1], t[0])))
                covers += 1
        # knocked-out armour off the lanes: the last street fighting of 1945, each on its own scorch mark
        wreck_bps = ["Military/DestTanks/BP_Env_DestroyedPzIII", "Military/DestTanks/BP_Env_DestroyedPzII",
                     "Military/DestTanks/BP_Env_DestroyedT34", "Military/DestTanks/BP_Env_DestroyedPz38T",
                     "Military/DestTanks/BP_Env_Destroyed_KV1E", "Military/DestTanks/BP_Env_Destroyed_IS_3"]
        wrecks, tries, spots = 0, 0, []
        while wrecks < 12 and tries < 4000 and not self.carried:
            tries += 1
            x = self.rng.uniform(b.xmin + 1500.0, b.xmax - 1500.0)
            y = self.rng.uniform(b.ymin + 1500.0, b.ymax - 1500.0)
            r, c = self.rc(x, y)
            lane = self.lane_dist[r, c]
            if not (750.0 < lane < 1500.0) or self.occupied[r, c] or blocked[r, c] or m["pads"][r, c]:
                continue
            if self.district(x, y) in ("park", "blast") or not self.clear_spot(x, y):
                continue
            if any(math.hypot(x - px, y - py) < 9000.0 for px, py in spots):
                continue
            if tg.soft(self.occupied, 3)[r, c] > 0.08:
                continue
            spots.append((x, y))
            self.actor("BER_StreetWreck_%02d" % wrecks, "Berlin/Streets", x, y, self.z(x, y), self.rng.uniform(0, 360),
                       bp=DEST + wreck_bps[wrecks % len(wreck_bps)])
            self.decal("crater", x, y, self.rng.uniform(750.0, 1000.0))
            self.occupied[max(r - 4, 0):r + 5, max(c - 4, 0):c + 5] = True
            wrecks += 1
        return counts, covers, wrecks

    # ---- ruin kit (owner's pick: E about 67 % of the ruins, B the rest; every piece is one destructible actor) ----------
    # piece: (half extent along local X, along local Y, centre offset x, y), rubble included, from the Unreal bounds
    KIT = {
        "E1_CornerTower": (928.0, 1078.0, 147.0, -147.0), "E2_Courtyard": (1322.0, 930.0, 91.0, 0.0),
        "E4_StubRing_A": (475.0, 528.0, 0.0, 0.0), "E4_StubRing_B": (475.0, 528.0, 0.0, 0.0),
        "E5_Shell1St_A": (528.0, 630.0, 0.0, 0.0), "E5_Shell1St_B": (529.0, 628.0, 0.0, 0.0),
        "E6_HeapFin_A": (476.0, 530.0, 0.0, 0.0), "E6_HeapFin_B": (477.0, 530.0, 0.0, 0.0),
        "E7_HalfHouse_A": (331.0, 425.0, 0.0, 0.0), "E7_HalfHouse_B": (324.0, 425.0, 0.0, 0.0),
        "B5_CornerL": (422.0, 431.0, 0.0, 0.0),
    }
    E_SHARE = 0.67

    def ruin(self, piece, x, y, yaw, kind, z=None):
        """One kit actor. Tint and soot vary per actor through custom primitive data 0-3 (see BerlinRuinKitMaterial.md)."""
        shade = float(self.rng.uniform(0.82, 1.08))
        warm = float(self.rng.uniform(-0.035, 0.035))
        z = self.z(x, y) if z is None else z
        self.ruins.append({"piece": piece, "x": round(x, 1), "y": round(y, 1),
                           "z": round(z, 1), "yaw": round(yaw % 360.0, 2), "kind": kind,
                           "tint": [round(shade + warm, 3), round(shade, 3), round(shade - warm, 3)],
                           "soot": round(float(self.rng.uniform(0.55, 1.35)), 3)})
        if piece in self.foot_table:
            self.foot(piece, x, y, z, yaw)
        r, c = self.rc(x, y)
        self.ruin_mask[max(r - 2, 0):r + 3, max(c - 2, 0):c + 3] = True

    def pick_e(self, top, hero_chance):
        ab = "A" if self.rng.random() < 0.5 else "B"
        r = self.rng.random()
        if top <= 1:
            return ("E4_StubRing_" if r < 0.7 else "E6_HeapFin_") + ab
        if top >= 3 and r < hero_chance:
            return "E1_CornerTower" if self.rng.random() < 0.5 else "E2_Courtyard"
        r = self.rng.random()
        if top == 2:
            return ("E5_Shell1St_" if r < 0.5 else "E7_HalfHouse_" if r < 0.8 else "E6_HeapFin_") + ab
        return ("E7_HalfHouse_" if r < 0.45 else "E5_Shell1St_" if r < 0.8 else "E6_HeapFin_") + ab

    def place_e(self, x, y, top, hero_chance, force=None):
        piece = force or self.pick_e(top, hero_chance)
        small = "E7_HalfHouse_" + ("A" if self.rng.random() < 0.5 else "B")
        for attempt in ((piece,) if force else (piece, small) if self.rng.random() < 0.5 else (piece,)):
            hx, hy, ox, oy = self.KIT[attempt]
            ang = self.site_angle(x, y)
            cu, su = math.cos(ang), math.sin(ang)
            # the footprint centre goes on the site; the pivot is offset from it
            px, py = x - (ox * cu - oy * su), y - (ox * su + oy * cu)
            free, cells = self.rect_free(x, y, hx, hy, ang, margin=90.0)
            if free:
                self.ruin(attempt, px, py, math.degrees(ang), "E")
                self.occupied[cells] = True
                self.occupied_objects[cells] = True
                self.ruin_mask[cells] = True
                self.body_mask[cells] = True
                self.footprints.append((x, y, hx, hy, ang, 3 if attempt.startswith(("E1", "E2")) else 1))
                return True
        return False

    def place_b(self, x, y, top):
        """A plot of wall slabs round a debris bed: front and back slab, sometimes a side slab. One ruin, 3-5 actors."""
        ang = self.site_angle(x, y)
        cu, su = math.cos(ang), math.sin(ang)
        if self.rng.random() < 0.12 and top >= 2:
            hx, hy, _, _ = self.KIT["B5_CornerL"]
            free, cells = self.rect_free(x, y, hx, hy, ang, margin=90.0)
            if not free:
                return False
            self.ruin("B5_CornerL", x, y, math.degrees(ang), "B")
            self.occupied[cells] = True
            self.ruin_mask[cells] = True
            self.body_mask[cells] = True
            self.footprints.append((x, y, hx, hy, ang, 2))
            return True
        stub = top <= 1
        frontage = 900.0 if stub else 1200.0
        depth = 800.0 if stub else 900.0
        hu, hv = depth / 2.0 + 280.0, frontage / 2.0 + 120.0
        free, cells = self.rect_free(x, y, hu, hv, ang, margin=90.0)
        if not free:
            return False

        def world(u, v):
            return x + u * cu - v * su, y + u * su + v * cu

        ab = lambda: "A" if self.rng.random() < 0.5 else "B"
        yaw = math.degrees(ang)
        if stub:
            front, back = "B6_Stub_" + ab(), "B6_Stub_" + ab()
        elif top == 2:
            front, back = "B2_Facade2St_" + ab(), ("B2_Facade2St_" + ab() if self.rng.random() < 0.5 else "B6_Stub_" + ab())
        else:
            front = "B3_Facade4St" if self.rng.random() < 0.45 else "B2_Facade2St_" + ab()
            back = "B4_Firewall" if self.rng.random() < 0.55 else "B2_Facade2St_" + ab()
        first = len(self.ruins)
        fx, fy = world(depth / 2.0, 0.0)
        self.ruin(front, fx, fy, yaw, "B")
        if self.rng.random() < 0.85:
            bx, by = world(-depth / 2.0, 0.0)
            self.ruin(back, bx, by, yaw + 180.0, "B")
        if not stub and self.rng.random() < 0.55:
            side = 1.0 if self.rng.random() < 0.5 else -1.0
            sx, sy = world(0.0, side * (frontage / 2.0 - 30.0))
            self.ruin("B1_Facade1St_" + ab(), sx, sy, yaw + 90.0 * side, "B")
        self.ruin("B8_DebrisBed", x, y, yaw + (90.0 if frontage < 1000.0 else 0.0), "B")
        for entry in self.ruins[first:]:
            entry["plot"] = True
        self.foot("B_PLOT_STUB" if stub else "B_PLOT", x, y, self.z(x, y), yaw, alternate=self.rng.random() < 0.5)
        self.occupied[cells] = True
        self.occupied_objects[cells] = True
        self.ruin_mask[cells] = True
        self.body_mask[cells] = True
        self.footprints.append((x, y, hu, hv, ang, 2))
        return True

    def kit_city(self):
        cu, su = math.cos(GRID_ANGLE), math.sin(GRID_ANGLE)
        reach = tg.SIZE_X / 2.0 + tg.SIZE_Y / 2.0
        pitch_u, pitch_v = 1650.0, 1500.0
        placed = {"E": 0, "B": 0}
        # heroes first, while there is room: the landmarks of the skyline (about 36 in play, 24 as backdrop)
        heroes = {True: 0, False: 0}
        for _ in range(0 if self.carried else 6000):
            x = self.rng.uniform(-self.spec.xmax - 5000.0, self.spec.xmax + 5000.0)
            y = self.rng.uniform(-self.spec.ymax - 5000.0, self.spec.ymax + 5000.0)
            inside = abs(x) < self.spec.xmax - 600.0 and abs(y) < self.spec.ymax - 600.0
            if not inside and abs(x) < self.spec.xmax + 900.0 and abs(y) < self.spec.ymax + 900.0:
                continue
            if heroes[inside] >= (36 if inside else 24):
                continue
            density, top, collapse, fams = self.zone(x, y)
            if top < 3:
                continue
            if self.place_e(x, y, top, 1.01):
                heroes[inside] += 1
                placed["E"] += 1
        for iu in range(int(-reach / pitch_u), int(reach / pitch_u) + 1):
            for iv in range(int(-reach / pitch_v), int(reach / pitch_v) + 1):
                u = iu * pitch_u + self.rng.uniform(-170.0, 170.0)
                v = iv * pitch_v + self.rng.uniform(-150.0, 150.0)
                x, y = u * cu - v * su, u * su + v * cu
                if abs(x) > self.spec.xmax + 5500.0 or abs(y) > self.spec.ymax + 5500.0:
                    continue
                density, top, collapse, fams = self.zone(x, y)
                if top <= 0 or self.rng.random() > density:
                    continue
                inside = abs(x) < self.spec.xmax and abs(y) < self.spec.ymax
                # steer toward the owner's share: E while it is under two thirds of the ruins placed so far
                total = placed["E"] + placed["B"]
                want_e = total == 0 or placed["E"] / total < self.E_SHARE
                if self.rng.random() < 0.2:
                    want_e = not want_e          # keep the mix irregular
                hero_chance = 0.0
                lot = self.rng.random()
                if inside and lot < 0.3:
                    r, c = self.rc(x, y)
                    if not self.occupied[r, c]:
                        if lot < 0.18:
                            # a fully collapsed building: one heap mass on the lot, no walls left standing
                            self.collapsed_lot(x, y, math.degrees(self.site_angle(x, y)))
                            if self.rng.random() < 0.5:
                                self.decal("debris", x, y, self.rng.uniform(1600.0, 2400.0))
                        else:
                            # a cleared lot: nothing but tracks and prints (the landscape paints these mud or ash)
                            for _ in range(int(self.rng.integers(2, 5))):
                                self.decal(self.choice(["tracks", "prints", "dirt"]), x + self.rng.uniform(-500, 500),
                                           y + self.rng.uniform(-500, 500), self.rng.uniform(500.0, 1000.0))
                        self.occupied[max(r - 7, 0):r + 8, max(c - 7, 0):c + 8] = True
                        self.occupied_objects[max(r - 5, 0):r + 6, max(c - 5, 0):c + 6] = True
                    continue
                ok = self.place_e(x, y, top, hero_chance) if want_e else self.place_b(x, y, top)
                if not ok:
                    ok = self.place_b(x, y, top) if want_e else self.place_e(x, y, top, hero_chance)
                    want_e = not want_e
                if ok:
                    placed["E" if want_e else "B"] += 1
        return placed

    def core_runs(self, pts):
        """Stretches of a wall-core centre line that are not cut by a gap: lists of (point, tangent, arc length)."""
        g = self.gen
        gap = tg.soft(g.out_masks["core_gap"], 1) > 0.08
        runs, run, s = [], [], 0.0
        for p, t in self.resample(pts, 100.0):
            s += 100.0
            ok = self.at(g.out_masks["ruin_core"], p[0], p[1]) and not self.at(gap, p[0], p[1])
            if ok:
                run.append((p, t, s))
            elif run:
                runs.append(run)
                run = []
        if run:
            runs.append(run)
        # keep the pieces 1.5 m back from each end: their rubble hulls must not reach into a 4 m infantry gap
        return [r[1:-1] if len(r) > 10 else r for r in runs if len(r) >= 6]

    def kit_cores(self):
        """Wall cores: 30 m hero rows (E3) and 15 m core slabs (B7), end to end along each run, never into a gap.
        The terrain mound under them stays; the pieces bring their own rubble mound, so they stand at ground level."""
        counts = {"E": 0, "B": 0}
        for reg in self.spec.regions:
            if not reg.id.startswith("core_"):
                continue
            for run in self.core_runs(reg.geom.pts):
                length = run[-1][2] - run[0][2]
                if length < 1500.0:
                    # shorter than the smallest core piece: close it with one or two 6 m facade slabs on the mound
                    # 9 m wall stubs, overlapping, cover the whole run; a 6 m slab where even one stub is too long
                    plen, piece = (900.0, "B6_Stub_") if length >= 900.0 else (600.0, "B1_Facade1St_")
                    n = int(math.ceil(length / plen)) if length >= plen else 0
                    print("core run %s: %.0f m -> %d x %s" % (reg.id, length / 100.0, n, piece))
                    for i in range(n):
                        lo, hi = run[0][2] + plen / 2.0, run[-1][2] - plen / 2.0
                        centre_s = lo + (hi - lo) * (0.5 if n == 1 else i / (n - 1.0))
                        p, t, _ = min(run, key=lambda item: abs(item[2] - centre_s))
                        yaw = math.degrees(math.atan2(t[1], t[0])) - 90.0
                        self.ruin(piece + ("A" if i % 2 else "B"), p[0], p[1], yaw, "B", z=self.z(p[0], p[1]) - 40.0)
                        counts["B"] += 1
                    continue
                # Pieces never reach past the ends of their run (a gap starts there): choose pieces whose total length
                # is at least the run, then spread their centres between the two end positions so they overlap inside.
                units = int(math.ceil(length / 1500.0))
                pieces = []
                k = 0
                while k < units:
                    if units - k >= 2 and length >= 3000.0 and self.rng.random() < 0.67:
                        pieces.append(("E3_CoreRow", 3000.0))
                        k += 2
                    else:
                        pieces.append(("B7_WallCore", 1500.0))
                        k += 1
                start, end = run[0][2], run[-1][2]
                centres = []
                for i, (piece, plen) in enumerate(pieces):
                    lo, hi = start + plen / 2.0, end - plen / 2.0
                    t = 0.5 if len(pieces) == 1 else i / (len(pieces) - 1.0)
                    centres.append(lo + (hi - lo) * t)
                print("core run %s: %.0f m -> %s" % (reg.id, length / 100.0, [p[0][:2] for p in pieces]))
                for (piece, plen), centre_s in zip(pieces, centres):
                    p, t, _ = min(run, key=lambda item: abs(item[2] - centre_s))
                    yaw = math.degrees(math.atan2(t[1], t[0])) - 90.0     # the piece's row axis is its local Y
                    if self.rng.random() < 0.5:
                        yaw += 180.0
                    normal = (-t[1], t[0])
                    base = min(self.z(p[0] + normal[0] * 950.0, p[1] + normal[1] * 950.0),
                               self.z(p[0] - normal[0] * 950.0, p[1] - normal[1] * 950.0))
                    self.ruin(piece, p[0], p[1], yaw, "E" if piece.startswith("E") else "B", z=base)
                    counts["E" if piece.startswith("E") else "B"] += 1
            # rubble along both flanks of the core, so the mound never reads as a clean capsule
            gap = tg.soft(self.gen.out_masks["core_gap"], 3) > 0.02
            for run in self.core_runs(reg.geom.pts):
                for k, (p, t, sdist) in enumerate(run):
                    if k % int(self.rng.integers(3, 6)):
                        continue
                    for side in (-1.0, 1.0):
                        q = np.asarray(p) + np.array([-t[1], t[0]]) * side * self.rng.uniform(780.0, 1150.0)
                        r, c = self.rc(q[0], q[1])
                        if gap[r, c] or self.lane_dist[r, c] < 760.0:
                            continue
                        self.add(self.choice(self.kit.rubble_mid + self.kit.chunks), q[0], q[1], self.z(q[0], q[1]) - 18.0,
                                 self.rng.uniform(0, 360), scale=self.rng.uniform(0.6, 1.3))
            self.occupied |= tg.soft(self.gen.masks[reg.id], 2) > 0.05
        # Hauptbahnhof shell: tall facade and firewall slabs along the two station walls, the 6 m gate left open
        gate = self.gen.feature("hbf_gap_south").points[0]
        for pid in ("hbf_wall_n", "hbf_wall_s"):
            pts = self.gen.path(pid).pts
            samples = self.resample(pts, 100.0)
            total = len(samples) * 100.0
            n = int(round(total / 1200.0))
            for i in range(n):
                p, t = samples[min(len(samples) - 1, int((i + 0.5) * total / n / 100.0))]
                if pid == "hbf_wall_s" and math.hypot(p[0] - gate[0], p[1] - gate[1]) < 1000.0:
                    continue
                piece = "B4_Firewall" if self.rng.random() < 0.4 else "B3_Facade4St"
                yaw = math.degrees(math.atan2(t[1], t[0])) - 90.0 + (180.0 if pid == "hbf_wall_s" else 0.0)
                self.ruin(piece, p[0], p[1], yaw, "B")
                counts["B"] += 1
        return counts

    # ---- furniture: small things any tank crushes (Source/Docs/Environment/BerlinFurniture.md) ----------------------
    FURN_TABLE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "furniture", "furniture_table.json")
    FURN_DEBRIS = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Materials/MI_BerlinFurnitureDebris_Decal"
    FURN_TALL = ("Wardrobe", "TiledStove", "KitchenDresser", "Piano", "Washstand")     # read as "still standing indoors"

    def furniture(self):
        """Household wreckage where the houses were and where it was thrown out: one to three pieces on every other
        collapsed plot, and groups of two to four along the kerbs of the lanes, right where vehicles drive. It has its
        own random stream, so adding or removing furniture never moves anything else."""
        self.furn = []
        if not os.path.exists(self.FURN_TABLE):
            return 0, 0
        with open(self.FURN_TABLE) as handle:
            table = json.load(handle)
        pieces = table["pieces"] if isinstance(table.get("pieces"), list) else list(table["pieces"].values())
        names = [e["name"].replace("SM_BerlinFurn_", "") for e in pieces]
        low = [n for n in names if not n.endswith(self.FURN_TALL)]
        rng = np.random.default_rng(4711)
        heaps = [(f[1], f[2], 520.0 if f[0].endswith("_L") else 380.0) for f in self.feet if "CollapseHeap" in f[0]]
        spots = []

        def put(name, x, y, yaw):
            if not (abs(x) < self.spec.xmax - 200.0 and abs(y) < self.spec.ymax - 200.0):
                return False
            r, c = self.rc(x, y)
            if self.hard[r, c] or any(math.hypot(x - hx, y - hy) < rad for hx, hy, rad in heaps):
                return False
            if any(math.hypot(x - px, y - py) < 230.0 for px, py in spots):
                return False
            if any(math.hypot(x - o[0], y - o[1]) < 1500.0 for o in self.openings_at):
                return False
            spots.append((x, y))
            self.furn.append([name, round(x, 1), round(y, 1), round(self.z(x, y), 1), round(yaw % 360.0, 1)])
            # its spill: shards of the same piece on the ground beside it (decal variant = piece number - 1)
            if rng.random() < 0.7:
                ang, dist = rng.uniform(0.0, 2.0 * math.pi), rng.uniform(40.0, 150.0)
                dx, dy = x + math.cos(ang) * dist, y + math.sin(ang) * dist
                self.decals.append({"mat": self.FURN_DEBRIS, "kind": "furndebris", "cell": int(name[:2]) - 1,
                                    "x": round(dx, 1), "y": round(dy, 1), "z": round(self.z(dx, dy), 1),
                                    "size": round(float(rng.uniform(280.0, 440.0)), 1),
                                    "yaw": round(float(rng.uniform(0.0, 360.0)), 1)})
            return True

        in_plots = 0
        for mesh, x, y, z, yaw, scale, bucket in list(self.feet):
            if not ("SlabPlot" in mesh or "StubHeap" in mesh) or rng.random() > 0.55:
                continue
            ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            for _ in range(int(rng.integers(1, 4))):
                # inside the plot, clear of where its front and back walls stand (local x = +-450)
                u, v = rng.uniform(-300.0, 300.0), rng.uniform(-480.0, 480.0)
                name = names[int(rng.integers(len(names)))]
                # tall pieces keep their back to a wall line; the rest lie anyhow
                turn = yaw + (0.0 if u < 0 else 180.0) + rng.uniform(-12.0, 12.0) if name.endswith(self.FURN_TALL)                     else rng.uniform(0.0, 360.0)
                in_plots += put(name, x + u * ca - v * sa, y + u * sa + v * ca, turn)
        loose = 0
        for mesh, x, y, z, yaw, scale, bucket in list(self.feet):
            if "RowStrip" in mesh or "CollapseHeap" in mesh or rng.random() > 0.45:
                continue
            for _ in range(int(rng.integers(1, 3))):
                dx, dy = x + rng.uniform(-520.0, 520.0), y + rng.uniform(-520.0, 520.0)
                if not (abs(dx) < self.spec.xmax - 200.0 and abs(dy) < self.spec.ymax - 200.0):
                    continue
                self.decals.append({"mat": self.FURN_DEBRIS, "kind": "furndebris", "cell": int(rng.integers(16)),
                                    "x": round(dx, 1), "y": round(dy, 1), "z": round(self.z(dx, dy), 1),
                                    "size": round(float(rng.uniform(300.0, 460.0)), 1),
                                    "yaw": round(float(rng.uniform(0.0, 360.0)), 1)})
                loose += 1
        self.furn_debris_loose = loose
        on_kerbs = 0
        for p in self.street_paths:
            samples = self.resample(p.pts, 100.0)
            base = 0.0 if p.kind == "lane" else 300.0
            i = int(rng.integers(30, 90))
            while i < len(samples) - 8:
                pt, t = samples[i]
                i += int(rng.integers(55, 110))
                if self.district(pt[0], pt[1]) in ("park", "blast", "rail") or not self.clear_spot(pt[0], pt[1]):
                    continue
                n = np.array([-t[1], t[0]]) * (1.0 if rng.random() < 0.5 else -1.0)
                for k in range(int(rng.integers(2, 5))):
                    q = pt + t * (k * rng.uniform(200.0, 330.0)) + n * (base + rng.uniform(380.0, 700.0))
                    on_kerbs += put(low[int(rng.integers(len(low)))], q[0], q[1], rng.uniform(0.0, 360.0))
        return in_plots, on_kerbs

    # ---- seating: the ground is sculpted flat under every ruin, skirt and heap -------------------------------------
    # half extents (local X, local Y) of the skirt meshes, from the footprint report
    FOOT_HALF = {"HalfHouse_A": (530, 620), "HalfHouse_B": (545, 660), "Shell_A": (765, 970), "Shell_B": (765, 930),
                 "StubHeap": (755, 885), "SlabPlot_A": (775, 945), "SlabPlot_B": (790, 900), "CornerL": (525, 445),
                 "CornerTenement": (1170, 1420), "Courtyard": (1515, 1225), "CollapseHeap_S": (440, 475),
                 "CollapseHeap_L": (640, 715)}

    def seat(self):
        """A piece stands at the ground height of its pivot, so on a slope or a berm one edge of its base floated and
        the other was buried. Here the landscape is levelled to the piece's height under its whole footprint and
        blended back over 3.5 m; where footprints overlap the heights are averaged. Everything already placed (pieces,
        skirts, rubble, props, decals) then moves by the change of the ground under it. Carriageways, wall cores and
        the hard keep-outs are left alone."""
        g = self.gen
        old = g.Z.copy()
        num, den, wmax = np.zeros(tg.SHAPE), np.zeros(tg.SHAPE), np.zeros(tg.SHAPE)
        rects = []
        for mesh, x, y, z, yaw, scale, bucket in self.feet:
            half = self.FOOT_HALF.get(mesh.replace("SM_BerlinFoot_", ""))
            if half:
                rects.append((x, y, half[0] * scale, half[1] * scale, yaw, z))
        slab_len = {"B1": 600.0, "B2": 1200.0, "B3": 1200.0, "B4": 1200.0, "B6": 900.0}
        for e in self.ruins:
            length = slab_len.get(e["piece"][:2])
            if length and abs(e["z"] - self.z(e["x"], e["y"])) < 5.0:      # not the stubs sunk into a core mound
                rects.append((e["x"], e["y"], 170.0, length / 2.0, e["yaw"], e["z"]))
        for x, y, hu, hv, yaw, target in rects:
            reach = math.hypot(hu, hv) + 500.0
            r0, c0 = self.rc(x + reach, y - reach)
            r1, c1 = self.rc(x - reach, y + reach)
            xs, ys = g.X[r0:r1 + 1, c0:c1 + 1] - x, g.Y[r0:r1 + 1, c0:c1 + 1] - y
            ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            u, v = xs * ca + ys * sa, -xs * sa + ys * ca
            d = np.hypot(np.maximum(np.abs(u) - hu, 0.0), np.maximum(np.abs(v) - hv, 0.0))
            w = tg.smoothstep(350.0, 0.0, d)
            num[r0:r1 + 1, c0:c1 + 1] += w * target
            den[r0:r1 + 1, c0:c1 + 1] += w
            wmax[r0:r1 + 1, c0:c1 + 1] = np.maximum(wmax[r0:r1 + 1, c0:c1 + 1], w)
        m = g.out_masks
        keep = tg.soft(m["ruin_core"] | m["core_gap"] | m["sinkhole"] | m["sink_gap"] | m["crater"] | m["landmark_pad"], 2) > 0.02
        keep |= g.river_dist < tg.RIVER_HALF + tg.QUAY + 300.0
        weight = wmax * tg.smoothstep(620.0, 780.0, g.lane_dist) * (~keep)
        target = np.where(den > 1e-6, num / np.maximum(den, 1e-6), old)
        g.Z = old * (1.0 - weight) + target * weight
        delta = g.Z - old

        def dz(x, y):
            r, c = self.rc(x, y)
            return float(delta[r, c])

        for items in self.ism.values():
            for it in items:
                it[2] = round(it[2] + dz(it[0], it[1]), 1)
        for group in (self.actors, self.ruins, self.decals):
            for e in group:
                e["z"] = round(e["z"] + dz(e["x"], e["y"]), 1)
        for f in self.feet:
            f[3] = round(f[3] + dz(f[1], f[2]), 1)
        for f in self.furn:
            f[3] = round(f[3] + dz(f[1], f[2]), 1)
        changed = np.abs(delta) > 3.0
        return len(rects), float(changed[g.inside].mean()), float(np.abs(delta).max())

    # ---- output -------------------------------------------------------------------------------------------------
    def run(self):
        core_counts = self.kit_cores()
        self.gaps()
        self.bollards()
        self.set_pieces()
        self.carry_over()
        junctions = self.junction_heroes()
        print("carried heroes %d, junction corner tenements %d" % (getattr(self, "carried_heroes", 0), junctions))
        front = self.frontage()
        city_counts = self.kit_city()
        skirt_heaps, skirt_litter = self.skirts()
        yard_cover = self.yard_cover()
        print("fallen buildings skipped: %s" % self.fallen_skips)
        print("frontage %s, skirts: %d heaps, %d litter, yard cover %d" % (front, skirt_heaps, skirt_litter, yard_cover))
        n_field, n_flat = self.rubble()
        trees, logs = self.tiergarten()
        decal_counts, covers, street_wrecks = self.details()
        print("decals %d by district %s, street covers %d, street wrecks %d" % (len(self.decals), decal_counts, covers,
                                                                              street_wrecks))
        cars, poles = (0, 0) if self.carried else self.streets()
        target = self.gen.feature("yard_target").points[0]    # the custom ramp and switch tower stand here
        self.actors = [a for a in self.actors if not (a["label"].startswith("BER_Yard") and
                                                      math.hypot(a["x"] - target[0], a["y"] - target[1]) < 1900.0)]
        print("furniture: %d on plots, %d along kerbs" % self.furniture())
        seated, share, biggest = self.seat()
        print("seated %d footprints: ground changed on %.1f %% of the play area, largest change %.0f cm" % (
            seated, 100.0 * share, biggest))
        total = sum(len(v) for v in self.ism.values())
        with open(OUT_FILE, "w") as handle:
            json.dump({"ism": self.ism, "actors": self.actors, "tank_blockers": self.tank_blockers,
                       "ruins": self.ruins, "decals": self.decals, "feet": self.feet,
                       "furniture": self.furn}, handle)
        # second paint pass (paint only, heights untouched): the ground knows where the ruins ended up
        g = self.gen
        g.ruin_stain = np.clip(tg.smoothstep(0.04, 0.5, tg.soft(self.ruin_mask, 3) + 0.22 * tg.noise(201, 5.0) *
                                             (tg.soft(self.ruin_mask, 5) > 0.01)), 0.0, 1.0)
        g.ruin_near = tg.smoothstep(0.02, 0.2, tg.soft(self.ruin_mask, 8) + 0.05 * tg.noise(202, 9.0))
        g.build_layers()
        g.write()
        print("feet %d; stain share inside %.3f" % (len(self.feet), float(g.ruin_stain[g.inside].mean())))
        e_sites = city_counts["E"] + core_counts["E"]
        b_sites = city_counts["B"] + core_counts["B"]
        print("ruins: E %d, B %d (E share %.0f %%); city %s, cores %s; kit actors %d; field rubble %d, litter %d" % (
            e_sites, b_sites, 100.0 * e_sites / max(1, e_sites + b_sites), city_counts, core_counts, len(self.ruins),
            n_field, n_flat))
        import collections
        print("pieces", dict(collections.Counter(r["piece"] for r in self.ruins).most_common()))
        print("trees %d, logs %d, cars %d, poles %d" % (trees, logs, cars, poles))
        print("instances %d in %d meshes, actors %d, tank blockers %d" % (total, len(self.ism), len(self.actors),
                                                                           len(self.tank_blockers)))
        self.preview()

    def preview(self):
        base = Image.open(os.path.join(tg.OUT, "preview.png")).convert("RGB")
        scale = 2
        base = base.resize((base.width * scale, base.height * scale), Image.BILINEAR)
        draw = ImageDraw.Draw(base)

        def px(x, y):
            return ((y + tg.SIZE_Y / 2.0) / tg.QUAD_CM * scale, (tg.SIZE_X / 2.0 - x) / tg.QUAD_CM * scale)

        for cx, cy, hu, hv, ang, top in self.footprints:
            cu, su = math.cos(ang), math.sin(ang)
            corners = [(cx + u * cu - v * su, cy + u * su + v * cu) for u, v in ((-hu, -hv), (hu, -hv), (hu, hv), (-hu, hv))]
            shade = 120 + 40 * top
            draw.polygon([px(*c) for c in corners], outline=(shade, shade // 3, 20))
        for a in self.actors:
            x, y = px(a["x"], a["y"])
            col = (40, 200, 60) if "Tree" in a["label"] or "Log" in a["label"] else (240, 240, 80)
            draw.ellipse([x - 2, y - 2, x + 2, y + 2], fill=col)
        for mesh, items in self.ism.items():
            if "Rubble" in mesh or "Prop" in mesh or "Car" in mesh:
                col = (200, 60, 200) if "Car" in mesh else (30, 30, 30)
                for it in items:
                    x, y = px(it[0], it[1])
                    draw.point((x, y), fill=col)
        base.save(os.path.join(tg.OUT, "dress_preview.png"))


if __name__ == "__main__":
    Dress().run()
