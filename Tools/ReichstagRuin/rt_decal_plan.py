"""Places the Reichstag ruin wall decals on the kit's faces (system python). Reads RT_Layout.json, writes RT_Decals.json
next to it; ue_reichstag_decals.py turns that into DecalComponents on BP_ReichstagRuin_B2.

    python rt_decal_plan.py [seed]

Atlas cells (Blender: BerlinReichstagBase/Decals/gen_decals.py): 0/1 soot plumes, 2 blast scorch, 3 burnt patch,
4 shell impact, 5 gouge, 6 shrapnel, 7 bullet line, 8 grime streaks, 9 rust runs, 10 brick core, 11-13 Soviet chalk /
charcoal writing, 14 red star SHTAB, 15 German LSR arrow; plus the DEM DEUTSCHEN VOLKE inscription on the portico frieze.

Frames (Unreal cm, actor local): a wall bay runs local +X 0..400, 140 thick, facade side local +Y (decoration out to
+95..+130); towers are 1640 square with outer faces at +/-820 and windows at -420/0/420 along each face; window rows
z 360-720 and 960-1340. Each entry: cell (or 'inscription'), material ('relief' | 'paint' | 'inscription'), location,
outward normal (the decal's X axis), width/height (cm, full), depth (cm, half), var (DecalColor.G), opacity (A), sort.
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WIN_ROWS = ((360, 720), (960, 1340))
WIN_X, WIN_HALF = 200, 90
ROUGH = {16: 320, 12: 280, 8: 220, 4: 120}
RELIEF = {4, 5, 6, 7, 10}
INNER = ('W_in', 'E_in', 'N_in', 'S_in', 'H_N', 'H_S')
OUTER = ('W_out', 'E_out', 'N_out', 'S_out')
TOWER_H = {'SM_RT_Tower_Tall': 3000, 'SM_RT_Tower_Mid': 2200, 'SM_RT_Tower_Low': 1800}
TOWER_HALF = 820


def rot(yaw, x, y):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return x * c - y * s, x * s + y * c


class Plan:
    def __init__(self, seed):
        self.rng = random.Random(seed)
        self.out = []

    def add(self, cell, base, yaw, lx, ly, z, side, w, h, depth=50, opacity=1.0, material=None, sort=None):
        dx, dy = rot(yaw, lx, ly)
        nx, ny = rot(yaw, 0, side)
        if material is None:        # relief is baked into the colour; the normal decal material is not used
            material = 'paint'
        if sort is None:
            sort = 0 if cell in (0, 1, 2, 3, 8, 9) else 1 if cell in RELIEF else 2
        self.out.append(dict(cell=cell, material=material, location_cm=[round(base[0] + dx, 1), round(base[1] + dy, 1), round(z, 1)],
                             normal=[round(nx, 4), round(ny, 4)], width_cm=round(w, 1), height_cm=round(h, 1),
                             depth_cm=depth, var=round(self.rng.random(), 3), opacity=round(opacity, 3), sort=sort))

    def in_window(self, lx, z, rows, centres):
        return any(abs(lx - c) < WIN_HALF + 40 and z0 - 40 < z < z1 + 40 for c in centres for z0, z1 in rows)

    def face(self, base, yaw, side, length, top, rows, centres, plane, p):
        """Random decals for one face: soot over a window, an impact, streaks from the top, rarer brick and burns."""
        r = self.rng
        ly = side * plane
        if rows and r.random() < p['soot']:
            z0, z1 = r.choice(rows)
            c = r.choice(centres)
            size = r.uniform(520, 640)
            self.add(0 if r.random() < 0.75 else 1, base, yaw, c, ly, z1 + size * 0.43, side, size, size, opacity=r.uniform(0.8, 1.0))
        if r.random() < p['impact']:
            for _ in range(8):
                lx, z = r.uniform(40, length - 40), r.uniform(160, max(170, top - 160))
                if not self.in_window(lx, z, rows, centres):
                    break
            k = r.random()
            cell, size = (4, r.uniform(200, 320)) if k < 0.4 else (5, r.uniform(220, 320)) if k < 0.6 else \
                (6, r.uniform(280, 380)) if k < 0.85 else (7, r.uniform(300, 380))
            self.add(cell, base, yaw, lx, ly, z, side, size, size)
        if top > 500 and r.random() < p['streak']:
            hgt = r.uniform(380, 600)
            self.add(8 if r.random() < 0.75 else 9, base, yaw, r.uniform(80, length - 80), ly, top - 60 - hgt * 0.42, side,
                     r.uniform(240, 340), hgt, opacity=r.uniform(0.7, 1.0))
        if top > 500 and r.random() < p['brick']:
            for _ in range(8):
                lx, z = r.uniform(80, length - 80), r.uniform(220, top - 220)
                if not self.in_window(lx, z, rows, centres):
                    break
            self.add(10, base, yaw, lx, ly, z, side, 260, 260)
        if r.random() < p['burn']:
            size = r.uniform(380, 560)
            self.add(3 if r.random() < 0.5 else 2, base, yaw, r.uniform(60, length - 60), ly, r.uniform(250, max(260, top - size / 2)), side, size, size,
                     opacity=r.uniform(0.6, 0.9))


def main(seed=1945):
    layout = json.load(open(os.path.join(HERE, 'RT_Layout.json'), encoding='utf-8'))
    plan = Plan(seed)
    r = plan.rng
    walls = [row for row in layout['pieces'] if row['piece'].startswith('SM_RT_Wall_H')]
    p_wall = dict(soot=0.42, impact=0.2, streak=0.2, brick=0.03, burn=0.07)
    faces = []
    for row in walls:
        h = int(row['piece'][12:14]) * 100
        bite = row['piece'].endswith('_B')
        top = h - ROUGH[h // 100] * (1.4 if bite else 0.75)
        rows = [wr for wr in WIN_ROWS if h - ROUGH[h // 100] * 0.6 > wr[1] + 50]
        for side, plane in ((1, 85), (-1, 75)):
            plan.face(row['location_cm'], row['yaw'], side, 400, top, rows, [WIN_X], plane, p_wall)
            faces.append((row, side, plane, top))
    # towers: four outer faces, three window columns, two rows
    p_tower = dict(soot=0.95, impact=0.9, streak=0.8, brick=0.25, burn=0.2)
    for row in layout['pieces']:
        if row['piece'] in TOWER_H:
            h = TOWER_H[row['piece']]
            for yaw in (0, 90, 180, 270):     # face normal local +Y after the yaw, at y = +820 (+15 out)
                base = [row['location_cm'][0], row['location_cm'][1]]
                for _ in range(1):
                    # the face frame: local x along the face from -820..820, shifted so lx 0..1640
                    bx, by = rot(yaw, -TOWER_HALF, 0)
                    plan.face([base[0] + bx, base[1] + by], yaw, 1, 2 * TOWER_HALF, h - 300, list(WIN_ROWS),
                              [TOWER_HALF - 420, TOWER_HALF, TOWER_HALF + 420], TOWER_HALF + 15, p_tower)
    # Soviet writing at standing height on the courtyard and hall walls
    inner = [f for f in faces if f[0]['group'] in INNER and f[3] > 400]
    r.shuffle(inner)
    for cell, count in ((11, 3), (12, 2), (13, 2)):
        for _ in range(count):
            row, side, plane, top = inner.pop()
            plan.add(cell, row['location_cm'], row['yaw'], r.uniform(130, 270), side * plane, r.uniform(170, 210), side,
                     260, 260, sort=2)
    outer = [f for f in faces if f[0]['group'] in OUTER and f[3] > 400]
    r.shuffle(outer)
    for _ in range(2):
        row, side, plane, top = outer.pop()
        plan.add(15, row['location_cm'], row['yaw'], r.uniform(120, 280), side * plane, 150, side, 220, 220, sort=2)
    # red star SHTAB on the Director's tower, facing into the base (+X south and +Y west)
    tall = next(p for p in layout['pieces'] if p['piece'] == 'SM_RT_Tower_Tall')['location_cm']
    for yaw in (-90, 0):     # face normals +X and +Y
        bx, by = rot(yaw, -TOWER_HALF, 0)
        plan.add(14, [tall[0] + bx, tall[1] + by], yaw, TOWER_HALF + 210, TOWER_HALF + 15, 220, 1, 240, 240, sort=2)
    # DEM DEUTSCHEN VOLKE on the portico's inscription panel (Blender y -7.05 -> Unreal +705, z 16.3-17.6 m)
    por = next(p for p in layout['pieces'] if p['piece'] == 'SM_RT_Portico')['location_cm']
    plan.out.append(dict(cell='inscription', material='inscription', location_cm=[por[0], por[1] + 705, 1695], normal=[0, 1],
                         width_cm=1800, height_cm=225, depth_cm=40, var=0.5, opacity=1.0, sort=2))
    path = os.path.join(HERE, 'RT_Decals.json')
    json.dump(dict(seed=seed, note=__doc__.split('\n')[0], decals=plan.out), open(path, 'w', encoding='utf-8'), indent=0)
    from collections import Counter
    print(len(plan.out), 'decals', sorted(Counter(str(d['cell']) for d in plan.out).items()))


main(int(sys.argv[1]) if len(sys.argv) > 1 else 1945)
