"""Generates the rubble decal placements for Ger_Berlin (offline; applied by rubble_place_apply.py in the editor).

Run:  python rubble_place_gen.py      -> Saved/Berlin/rubble_decals/rubble_placements.json + placements_preview.png

Inputs (all read-only):
  Saved/Berlin/rubble_decals/level_dump.json   live level actors (rubble_dump_level.py): ruins with mesh boxes, cover
                                               heaps, smoulder sites, existing decals, POIs, yard/power/wreck props
  Saved/Berlin/terrain/Ger_Berlin_masks.npz    terrain generator masks on the 1 m vertex grid ([iy, ix])
  Saved/Berlin/terrain/Ger_Berlin_layout.json  landscape corner, landmark positions

Decal convention (measured on RTS_ToCopySmall, 2026-11-17, rubble_orient_test.py): DecalActor rotation (0, -90, yaw)
maps atlas right -> world (cos yaw, sin yaw) and atlas up (the source edge of fan/corner cells) -> world
(sin yaw, -cos yaw). So a fan whose wall lies in world bearing B (degrees, from the decal towards the wall) gets
yaw = B + 90; a strip running along bearing R gets yaw = R. DecalColor B < 0.5 mirrors left-right.

Each placement: cell, x, y, yaw, s (scale: decal half size = 200 * s cm), mirror, g (brightness 0..1), a (opacity),
why (rule), and optional check = [bearing, max_cm]: the apply step traces horizontally from the centre along that
bearing and drops the decal unless a ruin wall is hit within max_cm (fans and corner piles must touch a wall).
"""
import json
import math
import os
import random

import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/'
OUT = ROOT + 'rubble_decals/'
SEED = 1945

C_FAN, C_HEAP, C_STRIP, C_CORNER, C_PLASTER, C_CONCRETE, C_TILES, C_SLATE, C_TIMBER, C_ASH, C_GLASS, C_SETTS, \
    C_PAVING, C_STEEL, C_SANDSTONE, C_GRIT = range(16)
NAMES = ["BrickSpillFan", "BrickHeapSpread", "MasonryStrip", "CornerPile", "PlasterDust", "ConcreteRebar", "RoofTiles",
         "SlateShards", "CharredTimber", "AshSoot", "GlassShards", "SettsTornUp", "PavingSlabs", "SteelScrap",
         "SandstoneBlocks", "GritSpill"]

FACADES = {  # piece -> (fan probability per 6 m of front, scatter cell family, back-side spill probability)
    'B1_Facade1St_A': (0.55, 'tiles', 0.25), 'B1_Facade1St_B': (0.55, 'tiles', 0.25),
    'B2_Facade2St_A': (0.65, 'tiles', 0.30), 'B2_Facade2St_B': (0.65, 'tiles', 0.30),
    'B3_Facade4St': (0.75, 'slate', 0.35), 'E1_CornerTower': (0.7, 'slate', 0.3),
    'E7_HalfHouse_A': (0.6, 'tiles', 0.3), 'E7_HalfHouse_B': (0.6, 'tiles', 0.3),
    'E5_Shell1St_A': (0.5, 'tiles', 0.4), 'E5_Shell1St_B': (0.5, 'tiles', 0.4),
    'E2_Courtyard': (0.5, 'slate', 0.4), 'E3_CoreRow': (0.45, 'tiles', 0.3),
    'B5_CornerL': (0.45, 'tiles', 0.0), 'B6_Stub_A': (0.35, None, 0.0), 'B6_Stub_B': (0.35, None, 0.0),
}


class Gen:
    def __init__(self):
        self.rng = random.Random(SEED)
        dump = json.load(open(OUT + 'level_dump.json'))
        self.actors = dump['actors']
        self.layout = json.load(open(ROOT + 'terrain/Ger_Berlin_layout.json'))
        m = np.load(ROOT + 'terrain/Ger_Berlin_masks.npz')
        self.masks = {k: m[k] for k in m.files}
        self.corner = self.layout['landscape']['corner']
        # ruins also stand in the outer ring beyond the play area (visible from the edge of the map): the landscape
        # limit, not the play area, bounds the decals
        lim = -self.corner[0] - 2500.0
        self.bounds = (-lim, lim, -lim, lim)
        self.out = []
        self.puddles, self.craters = [], []
        for a in self.actors:
            if a['class'] != 'DecalActor':
                continue
            r = a['size'][1]
            if 'Water' in (a['mat'] or ''):
                self.puddles.append((a['x'], a['y'], r))
            elif 'Crater' in (a['mat'] or ''):
                self.craters.append((a['x'], a['y'], r))
        self.puddles = np.array(self.puddles) if self.puddles else np.zeros((0, 3))
        self.craters = np.array(self.craters) if self.craters else np.zeros((0, 3))
        self.ruins = [a for a in self.actors if 'BerlinRuin' in a['class'] and 'box' in a]
        self.ruin_xy = np.array([[a['x'], a['y']] for a in self.ruins])
        self.stats = {}

    # ---------------------------------------------------------------- helpers
    def mask(self, name, x, y):
        ix = int(round((x - self.corner[0]) / 100.0))
        iy = int(round((y - self.corner[1]) / 100.0))
        g = self.masks[name]
        if 0 <= iy < g.shape[0] and 0 <= ix < g.shape[1]:
            return bool(g[iy, ix])
        return False

    def mask_any(self, name, x, y, r):
        for dx, dy in ((0, 0), (r, 0), (-r, 0), (0, r), (0, -r)):
            if self.mask(name, x + dx, y + dy):
                return True
        return False

    def keep_out(self, x, y, s):
        h = 200.0 * s
        if not (self.bounds[0] < x < self.bounds[1] and self.bounds[2] < y < self.bounds[3]):
            return 'bounds'
        for name in ('river', 'sinkhole', 'sink_gap', 'core_gap', 'bridge'):
            if self.mask_any(name, x, y, h * 0.6):
                return name
        if self.mask('crater', x, y):
            return 'blast'
        if len(self.puddles):
            d = np.hypot(self.puddles[:, 0] - x, self.puddles[:, 1] - y)
            if (d < self.puddles[:, 2] * 0.85 + h * 0.35).any():
                return 'puddle'
        if len(self.craters):
            d = np.hypot(self.craters[:, 0] - x, self.craters[:, 1] - y)
            if (d < self.craters[:, 2] * 0.45).any():
                return 'crater'
        return None

    def add(self, cell, x, y, yaw, s, why, check=None, mirror=None, overlap=0.5, landmark=False):
        reason = self.keep_out(x, y, s)
        if reason and landmark:
            self.stats['lm_drop_' + reason] = self.stats.get('lm_drop_' + reason, 0) + 1
        if reason and not (landmark and reason == 'landmark'):
            self.stats['drop_' + reason] = self.stats.get('drop_' + reason, 0) + 1
            return False
        if self.mask('landmark_pad', x, y) and not landmark:
            self.stats['drop_pad'] = self.stats.get('drop_pad', 0) + 1
            return False
        h = 200.0 * s
        for p in self.out:          # no heavy stacking of rubble decals
            if math.hypot(p['x'] - x, p['y'] - y) < overlap * (h + 200.0 * p['s']) and (p['cell'] == cell or overlap > 0.45):
                self.stats['drop_overlap'] = self.stats.get('drop_overlap', 0) + 1
                return False
        rng = self.rng
        self.out.append({'cell': cell, 'x': round(x, 1), 'y': round(y, 1), 'yaw': round(yaw % 360.0, 2),
                         's': round(s, 3), 'mirror': bool(rng.random() < 0.5) if mirror is None else mirror,
                         'g': round(rng.uniform(0.55, 1.0), 3), 'a': round(rng.uniform(0.85, 1.0), 3),
                         'why': why, 'check': check})
        self.stats[why] = self.stats.get(why, 0) + 1
        return True

    @staticmethod
    def frame(a):
        yaw = math.radians(a['yaw'] + a.get('mesh_rel', [0, 0, 0])[2])
        f = (math.cos(yaw), math.sin(yaw))
        r = (-math.sin(yaw), math.cos(yaw))
        rel = a.get('mesh_rel', [0, 0, 0])
        ox = a['x'] + rel[0] * math.cos(math.radians(a['yaw'])) - rel[1] * math.sin(math.radians(a['yaw']))
        oy = a['y'] + rel[0] * math.sin(math.radians(a['yaw'])) + rel[1] * math.cos(math.radians(a['yaw']))
        return (ox, oy), f, r, math.degrees(yaw)

    @staticmethod
    def at(o, f, r, lx, ly):
        return o[0] + f[0] * lx + r[0] * ly, o[1] + f[1] * lx + r[1] * ly

    def scatter_cell(self, family):
        rr = self.rng.random()
        if family == 'slate':
            return C_SLATE if rr < 0.5 else (C_GLASS if rr < 0.8 else C_TILES)
        return C_TILES if rr < 0.55 else (C_GLASS if rr < 0.85 else C_SLATE)

    # ---------------------------------------------------------------- rules
    def facades(self):
        rng = self.rng
        for a in self.ruins:
            piece = a['class'][len('BP_BerlinRuin_'):-2]
            if piece not in FACADES:
                continue
            p_fan, family, p_back = FACADES[piece]
            o, f, r, yaw = self.frame(a)
            sc = a.get('scale', 1.0)
            x0, y0, _, x1, y1, _ = [v * sc for v in a['box']]
            width = y1 - y0
            slots = max(1, int(round(width / 600.0)))
            for k in range(slots):
                if rng.random() > p_fan:
                    continue
                s = rng.uniform(0.8, 1.25)
                h = 200.0 * s
                ly = y0 + (k + 0.5) * width / slots + rng.uniform(-0.2, 0.2) * width / slots
                ly = max(y0 + h * 0.5, min(y1 - h * 0.5, ly))
                cell = C_FAN if rng.random() < 0.72 else (C_STRIP if rng.random() < 0.6 else C_PLASTER)
                if cell == C_STRIP:
                    x, y = self.at(o, f, r, x1 + h * 0.35, ly)
                    self.add(C_STRIP, x, y, yaw + 90.0, s, 'facade_strip', check=[yaw + 180.0, h * 0.35 + 220.0])
                elif cell == C_PLASTER:
                    x, y = self.at(o, f, r, x1 + h * 0.75, ly)
                    self.add(C_PLASTER, x, y, rng.uniform(0, 360), s, 'facade_plaster')
                else:
                    x, y = self.at(o, f, r, x1 + h - 45.0, ly)
                    self.add(C_FAN, x, y, yaw + 180.0 + 90.0, s, 'facade_fan', check=[yaw + 180.0, h + 160.0])
            # roof and window litter further out in front of the taller pieces
            if family and rng.random() < 0.45:
                s = rng.uniform(0.8, 1.2)
                ly = rng.uniform(y0, y1)
                x, y = self.at(o, f, r, x1 + rng.uniform(380.0, 700.0), ly)
                self.add(self.scatter_cell(family), x, y, rng.uniform(0, 360), s, 'facade_scatter', overlap=0.35)
            # the building's own collapse behind the front
            if rng.random() < p_back:
                s = rng.uniform(0.85, 1.3)
                h = 200.0 * s
                ly = rng.uniform(y0 + h * 0.5, max(y0 + h * 0.5, y1 - h * 0.5))
                if rng.random() < 0.6:
                    x, y = self.at(o, f, r, x0 - h + 45.0, ly)
                    self.add(C_FAN, x, y, yaw + 90.0, s, 'facade_back_fan', check=[yaw, h + 160.0])
                else:
                    x, y = self.at(o, f, r, x0 - h * 0.9, ly)
                    self.add(C_HEAP, x, y, rng.uniform(0, 360), s, 'facade_back_heap')

    def firewalls(self):
        rng = self.rng
        for a in self.ruins:
            piece = a['class'][len('BP_BerlinRuin_'):-2]
            if piece not in ('B4_Firewall', 'B7_WallCore'):
                continue
            o, f, r, yaw = self.frame(a)
            x0, y0, _, x1, y1, _ = a['box']
            for side, lx, bearing in ((1, x1, yaw + 180.0), (-1, x0, yaw)):
                if rng.random() > 0.6:
                    continue
                s = rng.uniform(0.85, 1.2)
                h = 200.0 * s
                ly = rng.uniform(y0 + h * 0.6, max(y0 + h * 0.6, y1 - h * 0.6))
                x, y = self.at(o, f, r, lx + side * h * 0.35, ly)
                self.add(C_STRIP, x, y, yaw + 90.0, s, 'wall_strip', check=[bearing, h * 0.35 + 220.0])

    def corners(self):
        """Corner piles inside the L of B5_CornerL and E2 courtyards: four candidates, the editor keeps those whose
        two edges both touch walls (two checks)."""
        rng = self.rng
        for a in self.ruins:
            piece = a['class'][len('BP_BerlinRuin_'):-2]
            if piece not in ('B5_CornerL', 'E2_Courtyard', 'E1_CornerTower'):
                continue
            if rng.random() > 0.8:
                continue
            o, f, r, yaw = self.frame(a)
            x0, y0, _, x1, y1, _ = a['box']
            s = rng.uniform(0.8, 1.1)
            h = 200.0 * s
            cands = []
            for cx, cy, sx, sy in ((x1, y1, -1, -1), (x1, y0, -1, 1), (x0, y1, 1, -1), (x0, y0, 1, 1)):
                # pile centre inside the box near this corner; its corner (atlas up + atlas left) points at the corner
                lx, ly = cx + sx * (h - 70.0), cy + sy * (h - 70.0)
                px, py = self.at(o, f, r, lx, ly)
                to_x = math.degrees(math.atan2(*reversed(self.vec(f, -sx))))   # bearing of the wall along local x
                to_y = math.degrees(math.atan2(*reversed(self.vec(r, -sy))))
                # unmirrored: atlas up = bearing yaw - 90, atlas left = yaw + 180.  up -> to_x wall, left -> to_y wall
                yaw_a = to_x + 90.0
                mirror = abs(((yaw_a + 180.0) - to_y + 180.0) % 360.0 - 180.0) > 1.0
                cands.append((px, py, yaw_a, mirror, to_x, to_y))
            rng.shuffle(cands)
            for px, py, yw, mirror, to_x, to_y in cands:
                if self.add(C_CORNER, px, py, yw, s, 'corner_pile', check=[[to_x, h + 120.0], [to_y, h + 120.0]],
                            mirror=mirror, overlap=0.3):
                    self.out[-1]['alts'] = True
                    break

    @staticmethod
    def vec(axis, sign):
        # direction (x, y) = -sign * axis: from the pile towards the wall at this corner
        return (-sign * axis[0] * -1, -sign * axis[1] * -1) if False else (-sign * axis[0], -sign * axis[1])

    def heaps(self):
        rng = self.rng
        for a in self.actors:
            c = a['class']
            if not (c.startswith('BP_BerlinCoverHeap') or 'B8_DebrisBed' in c or 'E6_HeapFin' in c or 'E4_StubRing' in c):
                continue
            if rng.random() > 0.6:
                continue
            box = a.get('box') or [-200, -200, 0, 200, 200, 100]
            rad = 0.5 * math.hypot(box[3] - box[0], box[4] - box[1]) * a.get('scale', 1.0)
            s = max(0.85, min(1.6, rad / 160.0))
            ang = rng.uniform(0, 2 * math.pi)
            off = rng.uniform(0.0, 0.35) * rad
            x, y = a['x'] + math.cos(ang) * off, a['y'] + math.sin(ang) * off
            if 'Concrete' in c or 'Block' in c:
                cell = C_CONCRETE if rng.random() < 0.55 else C_GRIT
            else:
                rr = rng.random()
                cell = C_HEAP if rr < 0.55 else (C_GRIT if rr < 0.8 else C_PLASTER)
            self.add(cell, x, y, rng.uniform(0, 360), s, 'heap_' + NAMES[cell])

    def smoulder(self):
        rng = self.rng
        for a in self.actors:
            if not a['label'].startswith(('ATM_Smoulder', 'ATM_Fire_')):
                continue
            for cell, p in ((C_TIMBER, 0.6), (C_ASH, 0.9)):
                if rng.random() > p:
                    continue
                ang = rng.uniform(0, 2 * math.pi)
                off = rng.uniform(100.0, 380.0)
                self.add(cell, a['x'] + math.cos(ang) * off, a['y'] + math.sin(ang) * off, rng.uniform(0, 360),
                         rng.uniform(0.9, 1.35), 'smoulder_' + NAMES[cell], overlap=0.3)

    def streets(self):
        """Street surface near ruins: setts torn up beside craters on the carriageway, paving and grit on the
        berms/pavements, a little glass under facades."""
        rng = self.rng
        tried = 0
        target = 230
        placed = 0
        xs = self.bounds
        while placed < target and tried < 60000:
            tried += 1
            x, y = rng.uniform(xs[0], xs[1]), rng.uniform(xs[2], xs[3])
            on_cw = self.mask('carriageway', x, y)
            on_berm = self.mask('street_berm', x, y) or self.mask('side_street', x, y)
            if not (on_cw or on_berm):
                continue
            d_ruin = np.hypot(self.ruin_xy[:, 0] - x, self.ruin_xy[:, 1] - y).min()
            if d_ruin > 1500.0:
                continue
            near_crater = len(self.craters) and (np.hypot(self.craters[:, 0] - x, self.craters[:, 1] - y)
                                                 < self.craters[:, 2] + 250.0).any()
            if on_cw:
                if near_crater:
                    cell = C_SETTS if rng.random() < 0.75 else C_GRIT
                else:
                    if rng.random() < 0.55:
                        continue        # most of the carriageway stays clear: it is the route
                    cell = C_GRIT if rng.random() < 0.5 else (C_SETTS if rng.random() < 0.5 else C_HEAP)
            else:
                rr = rng.random()
                cell = C_PAVING if rr < 0.4 else (C_GRIT if rr < 0.7 else (C_GLASS if rr < 0.88 else C_HEAP))
            if self.add(cell, x, y, rng.uniform(0, 360), rng.uniform(0.8, 1.2), 'street_' + NAMES[cell], overlap=0.6):
                placed += 1

    def landmarks(self):
        rng = self.rng
        sets = {'POI_ReichstagRuin': ([(C_SANDSTONE, 0.45), (C_CONCRETE, 0.2), (C_PLASTER, 0.15), (C_GRIT, 0.2)], 34),
                'POI_BrandenburgGateRuin': ([(C_SANDSTONE, 0.7), (C_GRIT, 0.3)], 16),
                'POI_FlakTower_Friedrichshain': ([(C_CONCRETE, 0.7), (C_GRIT, 0.3)], 18),
                'POI_FlakTower_Zoo': ([(C_CONCRETE, 0.7), (C_GRIT, 0.3)], 18),
                'POI_ZooRadioBunker': ([(C_CONCRETE, 1.0)], 4)}
        for a in self.actors:
            if a['label'] not in sets:
                continue
            if 'box' not in a and a['label'] == 'POI_ReichstagRuin':
                a = dict(a, box=[-5850.0, -4700.0, 0.0, 5850.0, 4700.0, 3000.0])     # BP_ReichstagRuin_B2: 117 x 94 m
            if 'box' not in a:
                continue
            mix, n = sets[a['label']]
            o, f, r, yaw = self.frame(a)
            sc = a.get('scale', 1.0)
            x0, y0, _, x1, y1, _ = [v * sc for v in a['box']]
            per = 2 * ((x1 - x0) + (y1 - y0))
            clusters = [(rng.uniform(0, per), rng.randint(1, 4)) for _ in range(max(2, n // 2))]
            tries = [(t + rng.uniform(-600.0, 600.0)) % per for t, k in clusters for _ in range(k * 2)]
            for t in tries:     # rubble lies in clumps where the facade came down, not as an even ring
                if sum(1 for p in self.out if p['why'] == 'landmark_' + a['label']) >= n:
                    break
                out_d = rng.uniform(100.0, 900.0)
                if t < x1 - x0:
                    lx, ly = x0 + t, y1 + out_d
                elif t < (x1 - x0) + (y1 - y0):
                    lx, ly = x1 + out_d, y0 + t - (x1 - x0)
                elif t < 2 * (x1 - x0) + (y1 - y0):
                    lx, ly = x1 - (t - (x1 - x0) - (y1 - y0)), y0 - out_d
                else:
                    lx, ly = x0 - out_d, y1 - (t - 2 * (x1 - x0) - (y1 - y0))
                px, py = self.at(o, f, r, lx, ly)
                rr, cell = rng.random(), mix[-1][0]
                for c, w in mix:
                    rr -= w
                    if rr <= 0:
                        cell = c
                        break
                self.add(cell, px, py, rng.uniform(0, 360), rng.uniform(0.9, 1.4), 'landmark_' + a['label'],
                         landmark=True, overlap=0.45)

    def industry(self):
        rng = self.rng
        for a in self.actors:
            lab = a['label']
            if not lab.startswith(('BER_Power', 'BER_Yard', 'BER_PileupWreck', 'BER_StreetWreck', 'BER_Wreck',
                                   'POI_RailYard', 'ScavObj')) and 'ScavObj' not in a['class']:
                continue
            p = 0.35 if lab.startswith(('BER_Power_Wall', 'BER_YardFence')) else 0.7
            if rng.random() > p:
                continue
            ang = rng.uniform(0, 2 * math.pi)
            off = rng.uniform(150.0, 450.0)
            cell = C_STEEL if rng.random() < 0.7 else C_CONCRETE
            self.add(cell, a['x'] + math.cos(ang) * off, a['y'] + math.sin(ang) * off, rng.uniform(0, 360),
                     rng.uniform(0.85, 1.25), 'industry_' + NAMES[cell], overlap=0.4)

    # ---------------------------------------------------------------- output
    def run(self):
        self.landmarks()
        self.corners()
        self.facades()
        self.firewalls()
        self.heaps()
        self.smoulder()
        self.industry()
        self.streets()
        per_cell = [sum(1 for p in self.out if p['cell'] == c) for c in range(16)]
        json.dump({'note': __doc__.split('\n')[0], 'names': NAMES, 'placements': self.out},
                  open(OUT + 'rubble_placements.json', 'w'), indent=0)
        print('placements', len(self.out))
        print('per cell', dict(zip(NAMES, per_cell)))
        print('stats', dict(sorted(self.stats.items())))
        self.preview()

    def preview(self):
        import cv2
        W = 2000
        sx = W / 100000.0
        H = int(52000 * sx)
        img = np.full((H, W, 3), 235, np.uint8)

        def px(x, y):
            return int((y + 50000) * sx), int((26000 - x) * sx)     # image right = +Y (east), up = +X (north)
        cw = self.masks['carriageway']
        ys, xs = np.nonzero(cw[::4, ::4])
        for iy, ix in zip(ys * 4, xs * 4):
            x, y = self.corner[0] + ix * 100, self.corner[1] + iy * 100
            u, v = px(x, y)
            if 0 <= u < W and 0 <= v < H:
                img[v, u] = (200, 200, 200)
        for a in self.ruins:
            u, v = px(a['x'], a['y'])
            cv2.circle(img, (u, v), 2, (90, 90, 90), -1)
        cols = [tuple(int(c) for c in np.array(cv2.cvtColor(np.uint8([[[int(i * 180 / 16), 220, 210]]]), cv2.COLOR_HSV2BGR)[0, 0])) for i in range(16)]
        for p in self.out:
            u, v = px(p['x'], p['y'])
            cv2.circle(img, (u, v), max(2, int(200 * p['s'] * sx)), cols[p['cell']], 1)
        for i, n in enumerate(NAMES):
            cv2.putText(img, '%02d %s' % (i, n), (10, 20 + i * 18), cv2.FONT_HERSHEY_SIMPLEX, 0.5, cols[i], 1, cv2.LINE_AA)
        cv2.imwrite(OUT + 'placements_preview.png', img)


if __name__ == '__main__':
    Gen().run()
