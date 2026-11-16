"""Oder-Warthe kit generator (Blender 5.2, background). One function per asset; each builds its mesh (+ LODs), its UCX
hulls, exports FBX to CustomMapItems/OderWarthe/FBX, saves a .blend, merges a row into ow_blender_report.json and
optionally renders a preview into Saved/OderWarthe/assets/previews.

    Blender5.0.exe -b --factory-startup -P ow_build.py -- <id> [<id> ...] [--preview]
ids: tooth, cupola, werk, portal, df_adcock, df_loop, mast, sluice, pines, erratics (or 'all').
Conventions: centimetres, +X forward, +Z up, pivot on the ground at the origin (sluice: pivot = deck level, water at -180).
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ow_blender_lib as L  # noqa: E402
from ow_blender_lib import Builder, box_pts, obox_pts, rng, v3  # noqa: E402

REPORT = L.OW_SRC + 'ow_blender_report.json'
BLEND_DIR = L.OW_SRC + 'Blend/'
PREVIEW = '--preview' in sys.argv
RESULTS = {}


def finish(name, builder_or_obj, hulls, lods=(), sharp=35.0, notes='', pivot='ground level, footprint centre',
           facing='+X front'):
    obj = builder_or_obj.to_object(sharp) if isinstance(builder_or_obj, Builder) else builder_or_obj
    info = L.export_fbx(obj, L.OW_FBX + name + '.fbx', hulls)
    info['lods'] = []
    lod_objs = []
    for k, lb in enumerate(lods, start=1):
        lo = lb.to_object(sharp)
        lo.name = name + '_LOD%d' % k
        li = L.export_fbx(lo, L.OW_FBX + name + '_LOD%d.fbx' % k, ())
        info['lods'].append(dict(fbx=li['fbx'], tris=li['tris']))
        lod_objs.append(lo)
    info['notes'] = notes
    info['pivot'] = pivot
    info['facing'] = facing
    info['previews'] = []
    RESULTS[name] = info
    print('BUILT', name, json.dumps(info))
    for lo in lod_objs:
        bpy.data.objects.remove(lo)
    return obj


def save_blend(tag):
    os.makedirs(BLEND_DIR, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_DIR + 'OW_%s.blend' % tag, compress=True)


def preview(tag, target, distance, azimuths=(-50,), elevation=52.0, lens=50.0, **kw):
    if not PREVIEW:
        return
    for az in azimuths:
        png = L.render_preview(L.OW_PREVIEW + 'OW_%s_az%d.png' % (tag, az), target=target, distance=distance,
                               azimuth_deg=az, elevation_deg=elevation, lens=lens, **kw)
        for row in RESULTS.values():
            if row.get('tag') == tag:
                row['previews'].append(png)


def tagged(tag, names):
    for n in names:
        RESULTS[n]['tag'] = tag


def clear_scene():
    L.reset_scene()


# --------------------------------------------------------------------------------------------------- dragon tooth
def tooth(variant):
    """Hoecker: truncated pyramid ~120 base, ~110 high, chamfered, chipped. Sunk 15 cm."""
    r = rng(11 if variant == 'A' else 23)
    name = 'SM_OW_DragonTooth_' + variant
    b = Builder(name, ['MI_OW_Concrete'])
    if variant == 'A':
        levels = [(-15, 62), (0, 60), (35, 50), (75, 38), (104, 28), (110, 22)]
    else:
        levels = [(-15, 58), (0, 56), (40, 46), (80, 33), (112, 24), (118, 17)]
    cham = 0.22  # chamfer fraction of the half-size at the corners
    rings = []
    for li, (z, h) in enumerate(levels):
        ring = []
        for c in range(4):
            a = math.radians(45 + 90 * c)
            cx, cy = math.copysign(1, math.cos(a)), math.copysign(1, math.sin(a))
            # two points per corner (the chamfer)
            p1 = Vector((cx * h, cy * h * (1 - cham), z))
            p2 = Vector((cx * h * (1 - cham), cy * h, z))
            if cx * cy < 0:
                p1, p2 = p2, p1
            ring += [p1, p2]
        # order counter-clockwise by angle
        ring.sort(key=lambda p: math.atan2(p.y, p.x))
        for p in ring:
            if 0 < li < len(levels) - 1:
                p.x += r.uniform(-2.5, 2.5)
                p.y += r.uniform(-2.5, 2.5)
                p.z += r.uniform(-2, 2)
        rings.append(ring)
    # chips: pull a few corner points inward
    chips = [(3, 1, 0.75), (4, 5, 0.7), (2, 6, 0.85)] if variant == 'A' else [(4, 2, 0.6), (5, 2, 0.6), (5, 3, 0.65), (2, 7, 0.82)]
    for li, k, f in chips:
        p = rings[li][k]
        p.x *= f
        p.y *= f
        p.z -= 6 if li == len(levels) - 1 else 0
    n = len(rings[0])
    for i in range(len(rings) - 1):
        for j in range(n):
            k = (j + 1) % n
            b.face([rings[i][j], rings[i][k], rings[i + 1][k], rings[i + 1][j]], 0)
    b.face(list(rings[-1]), 0)
    b.face(list(reversed(rings[0])), 0)
    if variant == 'B':  # slight settle tilt
        m = Matrix.Rotation(math.radians(3.0), 4, 'X') @ Matrix.Rotation(math.radians(-2.0), 4, 'Y')
        b.verts = [m @ v for v in b.verts]
    hull = [v.copy() for v in b.verts]
    obj = finish(name, b, [hull], sharp=30.0, facing='symmetric, any yaw',
                 pivot='ground centre; base sunk 15 cm below the pivot',
                 notes='Concrete Hoecker. Instance in staggered rows (~2.2 m pitch); random yaw is fine.')
    return obj


def build_tooth():
    clear_scene()
    a = tooth('A')
    bb = tooth('B')
    bb.location.x = 220
    tagged('DragonTooth', ['SM_OW_DragonTooth_A', 'SM_OW_DragonTooth_B'])
    save_blend('DragonTooth')
    preview('DragonTooth', (110, 0, 50), 900)


# ------------------------------------------------------------------------------------------------------ shared bits
def hnoise(seed):
    """Deterministic smooth-ish 2D noise: sum of a few random sines."""
    r = rng(seed)
    waves = [(r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(0, 6.3), r.uniform(0.004, 0.02)) for _ in range(6)]

    def f(x, y):
        s = 0.0
        for ax, ay, ph, fr in waves:
            s += math.sin((ax * x + ay * y) * fr + ph)
        return s / 6.0
    return f


def ss(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def polar_mound(b, rings, segs, mat, seed=5, amp=8.0, cx=0.0, cy=0.0):
    """Earth mound from rings [(r, h)] (inside out). Returns ring points."""
    nz = hnoise(seed)
    pts = []
    for i, (r, h) in enumerate(rings):
        ring = []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            x, y = cx + r * math.cos(a), cy + r * math.sin(a)
            k = 0.0 if i == 0 else 1.0
            rr = r * (1 + 0.05 * k * nz(x * 3.1, y * 2.7))
            x, y = cx + rr * math.cos(a), cy + rr * math.sin(a)
            ring.append(Vector((x, y, h + k * amp * nz(x, y))))
        pts.append(ring)
    t = L.MATS[mat]['tile']
    for i in range(len(rings) - 1):
        for j in range(segs):
            n = (j + 1) % segs
            q = [pts[i + 1][j], pts[i + 1][n], pts[i][n], pts[i][j]]
            b.face(q, mat, [(p.x / t, p.y / t) for p in q], True)
    return pts


def warped_grid(b, P, ni, nj, mat, smooth=True):
    """Quads over P(i, j) -> Vector for i < ni, j < nj (both inclusive end)."""
    t = L.MATS[mat]['tile']
    G = [[P(i, j) for j in range(nj + 1)] for i in range(ni + 1)]
    for i in range(ni):
        for j in range(nj):
            q = [G[i][j], G[i + 1][j], G[i + 1][j + 1], G[i][j + 1]]
            if newell_z(q) < 0:
                q = list(reversed(q))
            b.face(q, mat, [(p.x / t, p.y / t) for p in q], smooth)
    return G


def newell_z(q):
    return L.newell([v3(p) for p in q]).z


def cyl(b, c, r, z0, z1, mat, sides=12, smooth=True, cap=True):
    return b.tube([(c[0], c[1], z0), (c[0], c[1], z1)], [r, r], sides, mat, smooth=smooth, cap_start=cap, cap_end=cap)


def torus(b, center, R, r, mat, yaw=0.0, pitch=90.0, segs=20, sides=6):
    """Ring of radius R (tube r). Default: vertical ring (axis horizontal), yawed."""
    m = (Matrix.Translation(v3(center)) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @
         Matrix.Rotation(math.radians(pitch), 4, 'X'))
    pts = [m @ Vector((R * math.cos(2 * math.pi * k / segs), R * math.sin(2 * math.pi * k / segs), 0))
           for k in range(segs + 1)]
    b.tube(pts, [r] * len(pts), sides, mat, smooth=True, cap_start=False, cap_end=False)


def guy(b, top, anchor, mat='MI_OW_DarkSteel', r=1.6):
    b.tube([top, anchor], [r, r], 3, mat, smooth=False, cap_start=False, cap_end=False)


def anchor_block(b, p, mat='MI_OW_ConcreteDark', s=45):
    b.box((p[0] - s / 2, p[1] - s / 2, -15), (p[0] + s / 2, p[1] + s / 2, 12), mat)


# --------------------------------------------------------------------------------------------------- steel cupola
def build_cupola():
    clear_scene()
    name = 'SM_OW_SteelCupola'
    b = Builder(name, ['MI_OW_CastSteel', 'MI_OW_Concrete', 'MI_OW_Earth', 'MI_OW_Void', 'MI_OW_RustSteel'])
    # earth mound (inner ring hidden inside the collar)
    mound = polar_mound(b, [(185, 92), (240, 86), (320, 66), (420, 34), (520, -22)], 28, 'MI_OW_Earth', seed=7)
    # concrete collar
    b.lathe([(205, 50), (206, 112), (198, 124), (150, 128)], 36, 'MI_OW_Concrete', smooth=False)
    # cast steel dome with six recessed loopholes in the band between rows 2 and 3
    prof = [(r_, z_ + 20) for r_, z_ in [(150, 108), (149.5, 130), (148.5, 150), (147, 172), (143.5, 190), (136, 208),
                                         (123, 227), (103, 244), (77, 256), (45, 263), (0, 266)]]
    prof[3] = (146.5, 178 + 20)
    segs = 48
    slit_segs = set()
    for s in range(6):
        c = s * 8
        slit_segs.update({(c - 1) % segs, c, (c + 1) % segs})
    b.lathe(prof, segs, 'MI_OW_CastSteel', smooth=True,
            face_fn=lambda i, j: 'SKIP' if (i == 2 and j in slit_segs) else None)
    # remove the SKIP faces and build the recesses
    b.faces = [f for f in b.faces if b.materials[f[1]] != 'SKIP']
    assert b.materials[-1] == 'SKIP'
    b.materials.pop()
    depth = 16.0
    for j in sorted(slit_segs):
        a0, a1 = 2 * math.pi * j / segs, 2 * math.pi * (j + 1) / segs
        (r0, z0), (r1, z1) = prof[2], prof[3]
        o = [Vector((r0 * math.cos(a0), r0 * math.sin(a0), z0)), Vector((r0 * math.cos(a1), r0 * math.sin(a1), z0)),
             Vector((r1 * math.cos(a1), r1 * math.sin(a1), z1)), Vector((r1 * math.cos(a0), r1 * math.sin(a0), z1))]
        ri0, ri1 = r0 - depth, r1 - depth
        inn = [Vector((ri0 * math.cos(a0), ri0 * math.sin(a0), z0 + 4)), Vector((ri0 * math.cos(a1), ri0 * math.sin(a1), z0 + 4)),
               Vector((ri1 * math.cos(a1), ri1 * math.sin(a1), z1 - 4)), Vector((ri1 * math.cos(a0), ri1 * math.sin(a0), z1 - 4))]
        b.face(inn, 'MI_OW_Void')
        b.face([o[0], o[1], inn[1], inn[0]], 'MI_OW_RustSteel')     # sill
        b.face([inn[3], inn[2], o[2], o[3]], 'MI_OW_RustSteel')     # head
        if (j - 1) % segs not in slit_segs:
            b.face([o[0], inn[0], inn[3], o[3]], 'MI_OW_RustSteel')
        if (j + 1) % segs not in slit_segs:
            b.face([inn[1], o[1], o[2], inn[2]], 'MI_OW_RustSteel')
    # rust run-off below each slit (thin plates on the dome skin)
    for s in range(6):
        a = 2 * math.pi * (s * 8 + 0.5) / segs
        b.obox((149.3 * math.cos(a), 149.3 * math.sin(a), 150), (1.5, 40, 36), math.degrees(a), 'MI_OW_RustSteel')
    # roof: periscope / vent cap, lifting eyes
    cyl(b, (0, 0), 17, 282, 302, 'MI_OW_CastSteel', 12)
    cyl(b, (0, 0), 23, 302, 308, 'MI_OW_RustSteel', 12)
    for a in (40, 220):
        ar = math.radians(a)
        b.obox((96 * math.cos(ar), 96 * math.sin(ar), 266), (5, 14, 9), a + 90, 'MI_OW_RustSteel', pitch_deg=-38)
    hulls = []
    mpts = [p for ring in mound[1:] for p in ring[::2]] + [Vector((p.x, p.y, -25)) for p in mound[-1][::2]]
    hulls.append(mpts)
    dome = []
    for r, z in [(206, 50), (206, 126), (150, 128), (147, 192), (123, 247), (77, 276), (20, 290)]:
        for k in range(12):
            a = 2 * math.pi * k / 12
            dome.append((r * math.cos(a), r * math.sin(a), z))
    hulls.append(dome)
    finish(name, b, hulls, sharp=40.0, facing='rotationally symmetric (six loopholes at 60 deg); any yaw',
           pivot='ground level, centre; mound skirt dips to -22 cm, collar top +1.28 m, dome top +2.86 m, cap +3.08 m',
           notes='Panzerwerk six-loophole cupola: dark rusted cast-steel dome (3 m) on a round concrete collar in an '
                 'earth mound (r 5.2 m). No barrel. Can sit on SM_OW_WerkBlock roof (roof earth top ~+4.0 m).')
    tagged('SteelCupola', [name])
    save_blend('SteelCupola')
    preview('SteelCupola', (0, 0, 100), 2600)


# -------------------------------------------------------------------------------------------------------- werk block
def werk_h(x, y, nz):
    """Earth cover height over the Panzerwerk (rear face at x = -700)."""
    top = 395 + 14 * max(0.0, 1 - (x / 700) ** 2) * max(0.0, 1 - (y / 500) ** 2)
    dx = max(0.0, x - 700)
    dy = max(0.0, abs(y) - 500)
    d = math.sqrt((dx / 650) ** 2 + (dy / 520) ** 2) * (1 + 0.22 * nz(y * 0.37 + 900, x * 0.41))
    s = ss(0.0, 1.0, d) * 0.6 + min(1.0, d) * 0.4
    h = (top + 35) * (1 - s) - 35
    h += 9 * nz(x, y) * (0.4 + s)
    for cx, cy, cr, cd in WERK_CRATERS:
        q = math.hypot(x - cx, y - cy) / cr
        if q < 1.35:
            h += -cd * max(0.0, 1 - q * q) + cd * 0.35 * math.exp(-((q - 1.05) / 0.18) ** 2)
    return h


WERK_CRATERS = [(250, -180, 190, 45), (-320, 260, 140, 32), (980, 420, 220, 40), (420, -820, 160, 28)]


def build_werk():
    clear_scene()
    name = 'SM_OW_WerkBlock'
    b = Builder(name, ['MI_OW_Concrete', 'MI_OW_Earth', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Void',
                       'MI_OW_ConcreteDark'])
    nz = hnoise(31)
    # earth cover heightfield (x from just behind the rear wall)
    xs = [-660 + 70 * i for i in range(20)] + [700] + [700 + 75 * i for i in range(1, 10)]
    xs = sorted(set(xs))
    ys = [-1080 + 72 * i for i in range(31)]
    b.heightfield(xs, ys, lambda x, y: werk_h(x, y, nz), 'MI_OW_Earth')
    # rear facade (x = -700 .. -650): top follows the earth + 12, minus spall notches
    spalls = [(-380, 34, 30), (610, 46, 38), (170, 22, 18), (-820, 30, 22)]
    r = rng(5)

    def top(y):
        t = werk_h(-660, y, nz) + 12
        for c, w, d in spalls:
            if abs(y - c) < w:
                t -= d * (1 - abs(y - c) / w) ** 0.6 + r.uniform(0, 5)
        return max(t, -10)
    yfs = sorted(set([-1040 + 20 * i for i in range(105)] + [-110, 110]))
    tops = [top(y) for y in yfs]
    X0, X1, ZB = -700.0, -650.0, -35.0
    NICHE_Z = 250.0
    for k in range(len(yfs) - 1):
        ya, yb, ta, tb = yfs[k], yfs[k + 1], tops[k], tops[k + 1]
        in_niche = ya >= -110 and yb <= 110
        z0 = NICHE_Z if in_niche else ZB
        b.face([(X0, yb, z0), (X0, ya, z0), (X0, ya, ta), (X0, yb, tb)], 'MI_OW_Concrete')   # facade (normal -X)
        b.face([(X0, ya, ta), (X1, ya, ta), (X1, yb, tb), (X0, yb, tb)], 'MI_OW_Concrete')   # top
    # facade ends
    b.face([(X0, yfs[0], ZB), (X1, yfs[0], ZB), (X1, yfs[0], tops[0]), (X0, yfs[0], tops[0])], 'MI_OW_Concrete')
    b.face([(X1, yfs[-1], ZB), (X0, yfs[-1], ZB), (X0, yfs[-1], tops[-1]), (X1, yfs[-1], tops[-1])], 'MI_OW_Concrete')
    # door niche: side walls, ceiling, back wall, armoured door
    ND = -630.0
    b.face([(X0, -110, ZB), (ND, -110, ZB), (ND, -110, NICHE_Z), (X0, -110, NICHE_Z)][::-1], 'MI_OW_Concrete')
    b.face([(ND, 110, ZB), (X0, 110, ZB), (X0, 110, NICHE_Z), (ND, 110, NICHE_Z)][::-1], 'MI_OW_Concrete')
    b.face([(X0, -110, NICHE_Z), (ND, -110, NICHE_Z), (ND, 110, NICHE_Z), (X0, 110, NICHE_Z)][::-1], 'MI_OW_ConcreteDark')
    b.face([(ND, 110, ZB), (ND, -110, ZB), (ND, -110, NICHE_Z), (ND, 110, NICHE_Z)], 'MI_OW_ConcreteDark')
    b.box((ND - 8, -82, 0), (ND, 82, 218), 'MI_OW_RustSteel')                 # door leaf
    for z in (40, 110, 180):
        b.box((ND - 11, -80, z - 5), (ND - 8, 80, z + 5), 'MI_OW_DarkSteel')  # bands
    b.box((ND - 16, 50, 98), (ND - 8, 58, 130), 'MI_OW_DarkSteel')            # handle
    for z in (30, 185):
        b.box((ND - 12, -92, z), (ND - 2, -78, z + 22), 'MI_OW_DarkSteel')    # hinges
    # threshold step and lintel canopy
    b.box((-770, -135, -35), (-700, 135, 8), 'MI_OW_ConcreteDark')
    b.box((-765, -195, 252), (-700, 195, 292), 'MI_OW_Concrete')
    # close-defence loopholes with armour plates, vent grille
    for y in (-275, 275):
        b.box((-704, y - 30, 108), (-700, y + 30, 160), 'MI_OW_DarkSteel')
        b.box((-707, y - 17, 126), (-704, y + 17, 141), 'MI_OW_Void')
    b.box((-704, 400, 262), (-700, 452, 300), 'MI_OW_DarkSteel')
    for z in (268, 278, 288):
        b.box((-708, 402, z), (-704, 450, z + 4), 'MI_OW_RustSteel')
    # rebar in the spalls
    for c, w, d in spalls[:3]:
        for k in range(4):
            y = c - w * 0.6 + k * w * 0.4
            t = top(y)
            p0 = Vector((-675, y, t - 4))
            p1 = p0 + Vector((r.uniform(-25, -8), r.uniform(-6, 6), r.uniform(18, 36)))
            p2 = p1 + Vector((r.uniform(-20, 5), r.uniform(-10, 10), r.uniform(-4, 10)))
            b.tube([p0, p1, p2], [1.6, 1.6, 1.6], 4, 'MI_OW_RustSteel', smooth=False)
    # rust streaks under the loophole plates (thin dark plates flush with the facade)
    for y in (-275, 275):
        b.box((-701, y - 10, 30), (-700.2, y + 4, 108), 'MI_OW_ConcreteDark')
    hulls = []
    hulls.append(box_pts((-700, -500, -35), (700, 500, 405)))
    grid = [(x, y, werk_h(x, y, nz)) for x in xs for y in ys]

    def region(fx, fy):
        pts = [p for p in grid if fx(p[0]) and fy(p[1])]
        return pts + [(p[0], p[1], -35) for p in pts]
    hulls.append(region(lambda x: x >= 700, lambda y: abs(y) <= 500))
    hulls.append(region(lambda x: x <= 700, lambda y: y >= 500))
    hulls.append(region(lambda x: x <= 700, lambda y: y <= -500))
    hulls.append(region(lambda x: x >= 700, lambda y: y >= 500))
    hulls.append(region(lambda x: x >= 700, lambda y: y <= -500))
    for sgn in (1, -1):
        wy = [y for y in yfs if sgn * y >= 500]
        pts = [(x, y, top(y)) for y in wy for x in (X0, X1)] + [(x, y, ZB) for y in wy for x in (X0, X1)]
        hulls.append(pts)
    finish(name, b, hulls, sharp=35.0, facing='+X = front (east, enemy side, earth glacis); -X = rear entrance face (west)',
           pivot='ground level, centre of the 14 x 10 m concrete block (x -700..700, y -500..500)',
           notes='Buried Panzerwerk body: concrete block 14 m (X) x 10 m (Y) under an earth cover (roof earth top '
                 '~+3.95..4.1 m, glacis to +13.5 m in X, flanks to +/-10.8 m in Y, skirts dip to -35 cm). Exposed '
                 'concrete rear face at x = -7 m spans y +/-10.4 m with an armoured door niche at y 0, loophole plates '
                 'at y +/-2.75 m, spalls with rebar. Place SM_OW_SteelCupola on the roof (z ~ +3.9 m).')
    tagged('WerkBlock', [name])
    save_blend('WerkBlock')
    preview('WerkBlock', (150, 0, 150), 6200, azimuths=(150, -40))


# ------------------------------------------------------------------------------------------------------ tunnel portal
OPEN_W, OPEN_H, CHAM = 475.0, 520.0, 60.0     # half width, height, top chamfer
HW_X0, HW_X1 = 240.0, 300.0                    # headwall back / front face
WALL_TOP = 720.0
BACK_X = -350.0


def wing_y(x):
    return 750.0 + 70.0 * (x - 300.0) / 650.0


def wing_top(x):
    return 740.0 + (90.0 - 740.0) * (x - 300.0) / 650.0


def portal_berm_h(x, y, nz):
    top = 772.0
    dxb = max(0.0, -450.0 - x)
    dy = max(0.0, abs(y) - 760.0)
    d = math.sqrt((dxb / 650.0) ** 2 + (dy / 340.0) ** 2)
    s = ss(0.0, 1.0, d)
    return (top + 35) * (1 - s) - 35 + 10 * nz(x, y) * (0.3 + s)


def build_portal():
    clear_scene()
    name = 'SM_OW_TunnelPortal'
    b = Builder(name, ['MI_OW_Concrete', 'MI_OW_Earth', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Void',
                       'MI_OW_ConcreteDark', 'MI_OW_Wood'])
    nz = hnoise(77)
    # --- berm behind the headwall (regular grid, x <= 240)
    xs = sorted(set([-1100 + 75 * i for i in range(18)] + [HW_X0]))
    xs = [x for x in xs if x <= HW_X0]
    ys = [-1100 + 73.33 * i for i in range(31)]
    b.heightfield(xs, ys, lambda x, y: portal_berm_h(x, y, nz), 'MI_OW_Earth')
    # --- earth outside the wing walls (warped grid per side)
    NI, NJ = 13, 6
    for sgn in (1, -1):
        def P(i, j, sgn=sgn):
            x = HW_X0 + (1100.0 - HW_X0) * i / NI
            wy = wing_y(min(max(x, 300.0), 950.0)) + 25.0
            t = j / NJ
            y = wy + (1100.0 - wy) * t
            wt = wing_top(min(max(x, 300.0), 950.0)) - 18.0
            if x > 950.0:
                k = (x - 950.0) / 150.0
                wt = wt * (1 - k) - 35.0 * k
                y = (wy - 25.0 - 60.0 * k) + (1100.0 - (wy - 25.0 - 60.0 * k)) * t
            if x <= 300.0:
                wt = max(wt, portal_berm_h(HW_X0, sgn * y, nz))
            h = (wt + 35) * (1 - ss(0.0, 1.0, t)) - 35 + 8 * nz(x, y) * t
            return Vector((x, sgn * y, h))
        warped_grid(b, P, NI, NJ, 'MI_OW_Earth')
    # --- headwall (front face x = 300 with the opening), back face hidden in earth, coping
    def hw_quad(y0, y1, z0, z1):
        b.face([(HW_X1, y0, z0), (HW_X1, y1, z0), (HW_X1, y1, z1), (HW_X1, y0, z1)], 'MI_OW_Concrete')
    hw_quad(-750, -OPEN_W, -35, WALL_TOP)
    hw_quad(OPEN_W, 750, -35, WALL_TOP)
    hw_quad(-OPEN_W, OPEN_W, OPEN_H, WALL_TOP)
    b.face([(HW_X1, -OPEN_W, OPEN_H - CHAM), (HW_X1, -OPEN_W + CHAM, OPEN_H), (HW_X1, -OPEN_W, OPEN_H)], 'MI_OW_Concrete')
    b.face([(HW_X1, OPEN_W - CHAM, OPEN_H), (HW_X1, OPEN_W, OPEN_H - CHAM), (HW_X1, OPEN_W, OPEN_H)], 'MI_OW_Concrete')
    b.box((HW_X0 - 10, -770, WALL_TOP), (HW_X1 + 22, 770, WALL_TOP + 40), 'MI_OW_Concrete', skip=('-z',))
    b.box((HW_X0, -750, WALL_TOP - 2), (HW_X1, 750, WALL_TOP), 'MI_OW_Concrete', skip=('-z', '+z', '+x'))
    # --- tunnel lining: outline extruded from the front face back to BACK_X, fading to the void
    outline = [(-OPEN_W, 0), (-OPEN_W, OPEN_H - CHAM), (-OPEN_W + CHAM, OPEN_H), (OPEN_W - CHAM, OPEN_H),
               (OPEN_W, OPEN_H - CHAM), (OPEN_W, 0)]
    stations = [(HW_X1, 1.0, 'MI_OW_Concrete'), (60.0, 0.97, 'MI_OW_ConcreteDark'), (-120.0, 0.93, 'MI_OW_Void'),
                (BACK_X, 0.88, None)]
    for k in range(len(stations) - 1):
        xa, sa, mat = stations[k]
        xb, sb, _ = stations[k + 1]
        for m in range(len(outline) - 1):
            (ya0, za0), (ya1, za1) = outline[m], outline[m + 1]
            p = [(xa, ya0, za0 * sa), (xa, ya1, za1 * sa), (xb, ya1, za1 * sb), (xb, ya0, za0 * sb)]
            # inward-facing (normals toward the tunnel axis): wind so the normal points into the opening
            b.face(p[::-1], mat)
    # floor: concrete to x = 60, darker to -120, void beyond; back wall void
    b.face([(HW_X1, -OPEN_W, 2), (HW_X1, OPEN_W, 2), (60, OPEN_W, 2), (60, -OPEN_W, 2)], 'MI_OW_ConcreteDark')
    b.face([(60, -OPEN_W, 2), (60, OPEN_W, 2), (-120, OPEN_W, 2), (-120, -OPEN_W, 2)], 'MI_OW_Void')
    b.face([(-120, -OPEN_W, 2), (-120, OPEN_W, 2), (BACK_X, OPEN_W, 2), (BACK_X, -OPEN_W, 2)], 'MI_OW_Void')
    b.face([(BACK_X, -OPEN_W, 0), (BACK_X, OPEN_W, 0), (BACK_X, OPEN_W, OPEN_H), (BACK_X, -OPEN_W, OPEN_H)], 'MI_OW_Void')
    # --- wing walls (inner face toward the apron, outer face partly in the earth, sloped top, end cap)
    T = 50.0
    for sgn in (1, -1):
        xs_w = [300.0 + 650.0 * i / 6 for i in range(7)]
        for k in range(6):
            xa, xb = xs_w[k], xs_w[k + 1]
            ya_i, yb_i = sgn * wing_y(xa), sgn * wing_y(xb)
            ya_o, yb_o = sgn * (wing_y(xa) + T), sgn * (wing_y(xb) + T)
            ta, tb = wing_top(xa), wing_top(xb)
            inner = [(xa, ya_i, -35), (xb, yb_i, -35), (xb, yb_i, tb), (xa, ya_i, ta)]
            outer = [(xb, yb_o, -35), (xa, ya_o, -35), (xa, ya_o, ta), (xb, yb_o, tb)]
            topq = [(xa, ya_i, ta), (xb, yb_i, tb), (xb, yb_o, tb), (xa, ya_o, ta)]
            if sgn < 0:
                inner, outer, topq = inner[::-1], outer[::-1], topq[::-1]
            b.face(inner, 'MI_OW_Concrete')
            b.face(outer, 'MI_OW_Concrete')
            b.face(topq, 'MI_OW_Concrete')
        xe = 950.0
        cap = [(xe, sgn * wing_y(xe), -35), (xe, sgn * (wing_y(xe) + T), -35), (xe, sgn * (wing_y(xe) + T), wing_top(xe)),
               (xe, sgn * wing_y(xe), wing_top(xe))]
        b.face(cap if sgn > 0 else cap[::-1], 'MI_OW_Concrete')
    # --- apron slab (flush, cracked into four slabs), rails + sleepers
    for (x0, x1) in ((HW_X1, 520), (524, 740), (744, 950)):
        y0, y1 = wing_y(x0), wing_y(x1)
        b.face([(x0, -y0, 3), (x1, -y1, 3), (x1, y1, 3), (x0, y0, 3)], 'MI_OW_ConcreteDark')
    for x in [-100 + 72 * i for i in range(16)]:
        b.box((x - 13, -135, 3), (x + 13, 135, 13), 'MI_OW_Wood')
    for y in (-72, 72):
        b.box((BACK_X + 2, y - 4, 3), (1080, y + 4, 19), 'MI_OW_RustSteel')
    # --- blast door leaves: left open flat-ish (100 deg), right ajar (70 deg); hinged just outside the front face
    LW, LH, LT = 485.0, 520.0, 22.0
    for sgn, ang in ((-1, 100.0), (1, 70.0)):
        hinge = Vector((HW_X1 + 14, sgn * (OPEN_W + 15), 0))
        a = math.radians(ang)
        d = Vector((math.sin(a), -sgn * math.cos(a), 0))
        c = hinge + d * (LW / 2) + Vector((0, 0, LH / 2 + 4))
        yaw = math.degrees(math.atan2(d.y, d.x))
        b.obox(c, (LW, LT, LH), yaw, 'MI_OW_RustSteel', roll_deg=1.2 * sgn)
        nrm = Vector((-d.y, d.x, 0)) * (LT / 2 + 3)
        for z in (90, 260, 430):
            for side in (1, -1):
                b.obox(c + nrm * side + Vector((0, 0, z - LH / 2)), (LW - 30, 6, 16), yaw, 'MI_OW_DarkSteel')
        for f in (0.25, 0.75):
            b.obox(c + d * (LW * (f - 0.5)) + nrm, (14, 6, LH - 30), yaw, 'MI_OW_DarkSteel')
        for z in (60, LH - 60):
            b.obox(hinge + Vector((0, 0, z)), (34, 34, 46), 0, 'MI_OW_DarkSteel')
    # --- collision
    hulls = []
    grid = [(x, y, portal_berm_h(x, y, nz)) for x in xs for y in ys]

    def region(fx, fy):
        pts = [p for p in grid if fx(p[0]) and fy(p[1])]
        return pts + [(p[0], p[1], -35) for p in pts]
    hulls.append(region(lambda x: x <= BACK_X, lambda y: True))
    hulls.append(region(lambda x: x >= BACK_X, lambda y: y <= -OPEN_W))
    hulls.append(region(lambda x: x >= BACK_X, lambda y: y >= OPEN_W))
    hulls.append(box_pts((BACK_X, -OPEN_W, OPEN_H), (HW_X1, OPEN_W, 772)))
    hulls.append(box_pts((BACK_X - 30, -OPEN_W, -35), (BACK_X, OPEN_W, OPEN_H)))
    hulls.append(box_pts((HW_X0, -750, -35), (HW_X1, -OPEN_W, WALL_TOP + 40)))
    hulls.append(box_pts((HW_X0, OPEN_W, -35), (HW_X1, 750, WALL_TOP + 40)))
    for sgn in (1, -1):
        pts = []
        for x in (300.0, 950.0):
            for off in (0.0, T):
                y = sgn * (wing_y(x) + off)
                pts += [(x, y, -35), (x, y, wing_top(x))]
        hulls.append(pts)
        pts = []
        for x in (HW_X0, 600.0, 960.0):
            wy = wing_y(max(x, 300.0)) + T
            for yy in (wy, 1100.0):
                pts.append((x, sgn * yy, -35))
            pts.append((x, sgn * wy, max(wing_top(min(max(x, 300.0), 950.0)) - 18, -35)))
        hulls.append(pts)
    for sgn, ang in ((-1, 100.0), (1, 70.0)):
        hinge = Vector((HW_X1 + 14, sgn * (OPEN_W + 15), 0))
        a = math.radians(ang)
        d = Vector((math.sin(a), -sgn * math.cos(a), 0))
        c = hinge + d * (LW / 2) + Vector((0, 0, LH / 2 + 4))
        hulls.append(obox_pts(c, (LW, LT + 12, LH), math.degrees(math.atan2(d.y, d.x))))
    finish(name, b, hulls, sharp=35.0,
           facing='+X = tunnel mouth / apron side; the railway runs along X (rails at y +/-72 cm, x -3.5..+10.8 m)',
           pivot='ground level at the centre of the 22 x 22 m footprint; the mouth floor is at z 0',
           notes='Cut-and-cover railway tunnel mouth: concrete headwall (front face x = +3 m, coping +7.6 m) with a '
                 '9.5 m wide x 5.2 m high opening (chamfered top corners), concrete wing walls splaying to x = +9.5 m, '
                 'earth berm (+7.7 m) behind and beside. Two steel blast-door leaves hinged outside the mouth: the -Y '
                 'leaf swung fully open, the +Y leaf ajar at 70 deg (its free end at about x +7.7 m, leaving ~8 m clear '
                 'between the leaves). Interior: floor stays at z 0 (flat ground, landscape-safe); the lining fades '
                 'to an unlit-black void within 4 m and the ceiling dips 12% to read as descending. Apron slab '
                 'flush (3 cm) with NO collision; collision = berm, headwall piers + lintel, wing walls, door leaves '
                 'and a closed back wall 6.5 m inside the mouth. NOTE: Blender +Y = UE -Y, so in UE the open leaf is '
                 'on +Y and the ajar leaf on -Y.')
    tagged('TunnelPortal', [name])
    save_blend('TunnelPortal')
    preview('TunnelPortal', (100, 0, 200), 4600, azimuths=(-30, 30))


# ------------------------------------------------------------------------------------------------------ DF stations
def wood_pole(b, base, height, r0, r1, sides=8, mat='MI_OW_Wood'):
    b.tube([(base[0], base[1], -25), (base[0], base[1], height)], [r0, r1], sides, mat, smooth=True)


def hut_wood(b, cx, cy, w=300.0, d=250.0, h=245.0, ridge=85.0):
    b.box((cx - w / 2 - 12, cy - d / 2 - 12, -20), (cx + w / 2 + 12, cy + d / 2 + 12, 30), 'MI_OW_ConcreteDark')
    b.box((cx - w / 2, cy - d / 2, 30), (cx + w / 2, cy + d / 2, h), 'MI_OW_Wood', skip=('+z',))
    ov = 22.0
    x0, x1 = cx - w / 2 - ov, cx + w / 2 + ov
    y0, y1 = cy - d / 2 - ov, cy + d / 2 + ov
    zt = h + ridge
    eave = h - 12
    b.face([(x0, y0, eave), (x1, y0, eave), (x1, cy, zt), (x0, cy, zt)], 'MI_OW_RustSteel')
    b.face([(x1, y1, eave), (x0, y1, eave), (x0, cy, zt), (x1, cy, zt)], 'MI_OW_RustSteel')
    # gable walls up to the roof
    b.face([(cx + w / 2, cy - d / 2, h), (cx + w / 2, cy + d / 2, h), (cx + w / 2, cy, zt - 6)], 'MI_OW_Wood')
    b.face([(cx - w / 2, cy + d / 2, h), (cx - w / 2, cy - d / 2, h), (cx - w / 2, cy, zt - 6)], 'MI_OW_Wood')
    # door (+X), window (-Y)
    b.box((cx + w / 2, cy - 45, 32), (cx + w / 2 + 4, cy + 45, 222), 'MI_OW_DarkSteel')
    b.box((cx - 40, cy - d / 2 - 4, 120), (cx + 40, cy - d / 2, 190), 'MI_OW_Wood')
    b.box((cx - 32, cy - d / 2 - 6, 128), (cx + 32, cy - d / 2 - 4, 182), 'MI_OW_Void')
    return zt


def stovepipe(b, x, y, z0, z1):
    cyl(b, (x, y), 8, z0, z1, 'MI_OW_DarkSteel', 8)
    b.box((x - 15, y - 15, z1 + 6), (x + 15, y + 15, z1 + 10), 'MI_OW_DarkSteel')
    for dx, dy in ((-11, -11), (11, -11), (11, 11), (-11, 11)):
        b.box((x + dx - 1.5, y + dy - 1.5, z1), (x + dx + 1.5, y + dy + 1.5, z1 + 6), 'MI_OW_DarkSteel')


def generator(b, x, y, yaw=0.0):
    m = Matrix.Translation(Vector((x, y, 0))) @ Matrix.Rotation(math.radians(yaw), 4, 'Z')

    def bx(lo, hi, mat):
        b.box(lo, hi, mat, xform=m)
    bx((-80, -42, 0), (80, 42, 10), 'MI_OW_DarkSteel')          # skid
    bx((-70, -35, 10), (25, 35, 85), 'MI_OW_RustSteel')          # engine
    bx((25, -38, 10), (75, 38, 75), 'MI_OW_DarkSteel')           # dynamo housing
    bx((-78, -30, 15), (-70, 30, 80), 'MI_OW_DarkSteel')         # radiator
    p = m @ Vector((0, 20, 85))
    b.tube([p, p + Vector((0, 0, 55)), p + Vector((12, 0, 68))], [4, 4, 4], 6, 'MI_OW_DarkSteel', smooth=False)


def drum(b, x, y, tilt=0.0):
    cyl(b, (x, y), 29, 0, 88, 'MI_OW_RustSteel', 12)


def build_df_adcock():
    clear_scene()
    name = 'SM_OW_DFStation_Adcock'
    b = Builder(name, ['MI_OW_Wood', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_ConcreteDark', 'MI_OW_Void',
                       'MI_OW_Concrete'])
    H = 960.0
    S = 600.0
    zt = hut_wood(b, 0, 0)
    stovepipe(b, -80, 55, 270, 420)
    generator(b, 40, -235, 0)
    drum(b, -110, -215)
    drum(b, -110, -150)
    hulls = [box_pts((-170, -150, -20), (170, 150, zt)), box_pts((-40, -280, 0), (120, -190, 95))]
    for sx in (1, -1):
        for sy in (1, -1):
            px, py = sx * S, sy * S
            wood_pole(b, (px, py), H, 15, 10)
            b.box((px - 35, py - 35, -15), (px + 35, py + 35, 22), 'MI_OW_Concrete')
            # crossarm + insulators + whip
            ang = math.degrees(math.atan2(py, px)) + 90
            b.obox((px, py, H - 40), (14, 150, 14), ang, 'MI_OW_Wood')
            a = math.radians(ang)
            for off in (-62, 62):
                cyl(b, (px + off * math.cos(a), py + off * math.sin(a)), 5, H - 33, H - 12, 'MI_OW_ConcreteDark', 6)
            cyl(b, (px, py), 4, H, H + 120, 'MI_OW_DarkSteel', 6)
            # guys: outward diagonal and two sides
            out = math.atan2(py, px)
            for da in (0, 100, -100):
                a = out + math.radians(da)
                anc = (px + 430 * math.cos(a), py + 430 * math.sin(a), 8)
                guy(b, (px, py, H * 0.78), anc)
                anchor_block(b, anc)
            # cable trough from pole to hut, plus a raised feeder on two small posts
            d = Vector((-px, -py, 0)).normalized()
            start = Vector((px, py, 0)) + d * 60
            end = Vector((sx * 170, sy * 150, 0))
            L2 = (end - start).length
            mid = (start + end) / 2
            b.obox((mid.x, mid.y, 7), (L2, 34, 18), math.degrees(math.atan2(d.y, d.x)), 'MI_OW_ConcreteDark')
            fa = Vector((px, py, 260)) + d * 16
            fb = Vector((sx * 150, sy * 125, 215))
            b.tube([fa, fb], [2.4, 2.4], 4, 'MI_OW_DarkSteel', smooth=False)
            for f in (0.35, 0.68):
                p = fa.lerp(fb, f)
                b.tube([(p.x, p.y, -10), (p.x, p.y, p.z)], [6, 5], 6, 'MI_OW_Wood')
            hulls.append(box_pts((px - 40, py - 40, -15), (px + 40, py + 40, 260)))
    finish(name, b, hulls, sharp=40.0,
           facing='square layout; hut door faces +X',
           pivot='ground level at the centre of the 12 m mast square (hut centre)',
           notes='Adcock direction-finder: four 9.6 m wooden mast poles (whips to 10.8 m) on concrete pads at the '
                 'corners of a 12 m square, each with a crossarm and three guys to concrete anchor blocks (4.3 m out), '
                 'concrete cable troughs (X pattern from above) and a raised feeder to a 3 x 2.5 m plank hut with a '
                 'rusted iron gable roof, stovepipe, generator on a skid and fuel drums. Collision: hut, generator and '
                 'the four mast bases only.')
    tagged('DFStation_Adcock', [name])
    save_blend('DFStation_Adcock')
    preview('DFStation_Adcock', (0, 0, 250), 3600)


def build_df_loop():
    clear_scene()
    name = 'SM_OW_DFStation_Loop'
    b = Builder(name, ['MI_OW_Concrete', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Wood', 'MI_OW_Void',
                       'MI_OW_ConcreteDark'])
    W, D, H = 350.0, 270.0, 255.0
    b.box((-W / 2, -D / 2, -20), (W / 2, D / 2, H), 'MI_OW_Concrete')
    b.box((-W / 2 - 18, -D / 2 - 18, H), (W / 2 + 18, D / 2 + 18, H + 24), 'MI_OW_Concrete')
    b.box((W / 2, -45, 0), (W / 2 + 5, 45, 205), 'MI_OW_RustSteel')
    b.box((W / 2 + 5, -40, 95), (W / 2 + 9, 40, 103), 'MI_OW_DarkSteel')
    b.box((-60, -D / 2 - 4, 150), (60, -D / 2, 178), 'MI_OW_DarkSteel')
    b.box((-50, -D / 2 - 6, 156), (50, -D / 2 - 4, 172), 'MI_OW_Void')
    # rooftop rotator: housing, handwheel, short mast, big vertical loop
    RZ = H + 24
    b.box((-45, -45, RZ), (45, 45, RZ + 55), 'MI_OW_DarkSteel')
    torus(b, (0, -52, RZ + 30), 22, 2.5, 'MI_OW_RustSteel', yaw=0, pitch=90, segs=12, sides=4)
    cyl(b, (0, 0), 30, RZ + 55, RZ + 70, 'MI_OW_RustSteel', 12)
    cyl(b, (0, 0), 9, RZ + 70, RZ + 230, 'MI_OW_DarkSteel', 8)
    LOOP_R = 140.0
    LYAW = 35.0
    lc = (0, 0, RZ + 230 + LOOP_R)
    torus(b, lc, LOOP_R, 8, 'MI_OW_DarkSteel', yaw=LYAW, pitch=90, segs=24, sides=6)
    a = math.radians(LYAW)
    dvec = Vector((math.cos(a), math.sin(a), 0))
    b.tube([Vector(lc) - dvec * LOOP_R, Vector(lc) + dvec * LOOP_R], [3.5, 3.5], 4, 'MI_OW_DarkSteel', smooth=False)
    b.obox((0, 0, RZ + 238), (40, 40, 16), LYAW, 'MI_OW_RustSteel')
    # stovepipe on the hut, drums
    stovepipe(b, -120, 80, RZ, RZ + 110)
    drum(b, 120, 180)
    drum(b, 185, 165)
    # the tall single sense mast: thick wooden pole, top crossarm, three guys, wire down to the hut roof
    PX, PY, PH = -760.0, 120.0, 1150.0
    wood_pole(b, (PX, PY), PH, 19, 12, sides=8)
    b.box((PX - 40, PY - 40, -15), (PX + 40, PY + 40, 25), 'MI_OW_Concrete')
    b.obox((PX, PY, PH - 30), (16, 110, 16), 0, 'MI_OW_Wood')
    for da in (0, 120, 240):
        ang = math.radians(da + 180)
        anc = (PX + 480 * math.cos(ang), PY + 480 * math.sin(ang), 8)
        guy(b, (PX, PY, PH * 0.75), anc)
        anchor_block(b, anc)
    b.tube([(PX, PY - 50, PH - 30), (-W / 2 - 10, 60, RZ + 10)], [2.2, 2.2], 4, 'MI_OW_DarkSteel', smooth=False)
    hulls = [box_pts((-W / 2 - 18, -D / 2 - 18, -20), (W / 2 + 18, D / 2 + 18, RZ + 70)),
             box_pts((PX - 45, PY - 45, -15), (PX + 45, PY + 45, 300))]
    finish(name, b, hulls, sharp=40.0,
           facing='hut door faces +X; the tall mast stands 7.6 m to -X of the hut',
           pivot='ground level at the hut centre',
           notes='Loop direction-finder: 3.5 x 2.7 m concrete blockhouse with a roof rotator carrying a 2.8 m vertical '
                 'loop antenna (top +7.9 m), plus one tall 11.5 m wooden mast (3 guys) with a wire to the hut roof. '
                 'Silhouette: hut + ring, single pole; no lattice (SM_OW_WerkMast is the lattice mast). Collision: hut '
                 '(incl. rotator base) and the mast base only.')
    tagged('DFStation_Loop', [name])
    save_blend('DFStation_Loop')
    preview('DFStation_Loop', (-300, 0, 300), 3800)


# --------------------------------------------------------------------------------------------------------- werk mast
def build_mast():
    clear_scene()
    name = 'SM_OW_WerkMast'
    b = Builder(name, ['MI_OW_DarkSteel', 'MI_OW_RustSteel', 'MI_OW_ConcreteDark', 'MI_OW_Concrete'])
    b.box((-55, -55, -20), (55, 55, 28), 'MI_OW_Concrete')
    R = 28.0
    Z0, Z1 = 28.0, 1185.0
    legs = [(R * math.cos(math.radians(90 + 120 * k)), R * math.sin(math.radians(90 + 120 * k))) for k in range(3)]
    for lx, ly in legs:
        b.tube([(lx, ly, Z0), (lx, ly, Z1)], [4.0, 3.2], 6, 'MI_OW_DarkSteel', smooth=True)
    bay = 52.0
    nb = int((Z1 - Z0) / bay)
    for k in range(nb):
        za, zb = Z0 + k * bay, Z0 + (k + 1) * bay
        for f in range(3):
            a, c = legs[f], legs[(f + 1) % 3]
            if k % 2 == 0:
                b.beam((a[0], a[1], za), (c[0], c[1], zb), 1.5, 'MI_OW_RustSteel' if (k + f) % 5 == 0 else 'MI_OW_DarkSteel')
            else:
                b.beam((c[0], c[1], za), (a[0], a[1], zb), 1.5, 'MI_OW_DarkSteel')
            if k % 4 == 0:
                b.beam((a[0], a[1], za), (c[0], c[1], za), 1.5, 'MI_OW_DarkSteel')
    # top: T-bar dipole with insulators, lightning rod
    b.box((-36, -36, Z1), (36, 36, Z1 + 8), 'MI_OW_DarkSteel')
    b.tube([(0, -170, Z1 + 30), (0, 170, Z1 + 30)], [3.0, 3.0], 6, 'MI_OW_RustSteel', smooth=True)
    b.tube([(0, 0, Z1 + 8), (0, 0, Z1 + 30)], [5.0, 5.0], 6, 'MI_OW_DarkSteel')
    for y in (-150, 150):
        b.tube([(0, y, Z1 + 30), (0, y, Z1 + 12)], [4.5, 4.5], 6, 'MI_OW_Concrete')
    b.tube([(0, 0, Z1 + 30), (0, 0, Z1 + 110)], [1.6, 1.0], 4, 'MI_OW_DarkSteel')
    # three guy levels to three anchors
    GR = 640.0
    for k in range(3):
        a = math.radians(90 + 120 * k)
        anc = (GR * math.cos(a), GR * math.sin(a), 15)
        b.box((anc[0] - 34, anc[1] - 34, -18), (anc[0] + 34, anc[1] + 34, 22), 'MI_OW_ConcreteDark')
        for z in (420.0, 810.0, 1160.0):
            guy(b, (legs[k][0], legs[k][1], z), anc)
    hulls = [box_pts((-55, -55, -20), (55, 55, 28)), box_pts((-34, -34, 28), (34, 34, 320))]
    finish(name, b, hulls, sharp=40.0, facing='three-fold symmetric (guys at 90/210/330 deg); any yaw',
           pivot='ground level at the mast foot',
           notes='Plain 12 m triangular steel lattice aerial mast (0.5 m faces) on a concrete footing, three guy '
                 'levels to three concrete anchors 6.4 m out, T-bar dipole + lightning rod on top. Lattice '
                 'construction and the triangle of anchors set it apart from the wooden-pole DF stations. Collision: '
                 'footing + lower 3 m of the mast only.')
    tagged('WerkMast', [name])
    save_blend('WerkMast')
    preview('WerkMast', (0, 0, 500), 3200)


# --------------------------------------------------------------------------------------------------------- sluice gate
def build_sluice():
    clear_scene()
    name = 'SM_OW_SluiceGate'
    b = Builder(name, ['MI_OW_Concrete', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_ConcreteDark'])
    WATER = -180.0
    ZB = -460.0
    PT = 20.0                       # pier top
    piers = [-560.0, 0.0, 560.0]
    PW = 100.0
    for py in piers:
        y0, y1 = py - PW / 2, py + PW / 2
        b.box((-200, y0, ZB), (170, y1, PT), 'MI_OW_Concrete')
        # cutwater nose on the water side (+X)
        n0 = [(170, y0, ZB), (170, y1, ZB), (232, py, ZB)]
        n1 = [(170, y0, PT), (170, y1, PT), (232, py, PT)]
        b.face([n0[0], n0[2], n1[2], n1[0]], 'MI_OW_Concrete')
        b.face([n0[2], n0[1], n1[1], n1[2]], 'MI_OW_Concrete')
        b.face([n1[0], n1[2], n1[1]], 'MI_OW_Concrete')
        # wet / algae band at the waterline
        for (yy, sgn) in ((y0, -1), (y1, 1)):
            b.box((-200, yy + sgn * 0.0 - (1.2 if sgn < 0 else 0), WATER - 50),
                  (170, yy + (1.2 if sgn > 0 else 0), WATER + 45), 'MI_OW_ConcreteDark')
        # gate guides (channels) on the pier faces
        for yy, sgn in ((y0, -1), (y1, 1)):
            if (py == piers[0] and sgn < 0) or (py == piers[-1] and sgn > 0):
                continue
            b.box((24, min(yy, yy + sgn * 14), ZB + 40), (56, max(yy, yy + sgn * 14), 270), 'MI_OW_DarkSteel')
    # sill slab under water between the piers
    b.box((-200, -610, ZB - 10), (170, 610, ZB + 30), 'MI_OW_ConcreteDark')
    # gate leaves: bay A closed, bay B half raised
    bays = [(-510.0 + 4, -50.0 - 4), (50.0 + 4, 510.0 - 4)]
    leaf_z = [(-420.0, 30.0), (-220.0, 200.0)]
    for (ya, yb), (z0, z1) in zip(bays, leaf_z):
        b.box((30, ya + 6, z0), (50, yb - 6, z1), 'MI_OW_RustSteel')
        for z in range(int(z0) + 40, int(z1), 70):
            b.box((14, ya + 10, z), (30, yb - 10, z + 12), 'MI_OW_DarkSteel')
        # hoist portal: two columns on the piers, beam, gearbox, handwheel, spindle
        for yc in (ya - 26, yb + 26):
            b.box((30, yc - 12, PT), (54, yc + 12, 285), 'MI_OW_DarkSteel')
        b.box((26, ya - 40, 262), (58, yb + 40, 292), 'MI_OW_RustSteel')
        ym = (ya + yb) / 2
        b.box((14, ym - 34, 292), (70, ym + 34, 336), 'MI_OW_DarkSteel')
        torus(b, (-2, ym, 318), 34, 3.0, 'MI_OW_RustSteel', yaw=90, pitch=90, segs=14, sides=4)
        b.tube([(-2, ym, 318), (14, ym, 318)], [3, 3], 4, 'MI_OW_DarkSteel', smooth=False)
        for dy in (-60, 60):
            b.tube([(40, ym + dy, z1), (40, ym + dy, 262)], [4.0, 4.0], 6, 'MI_OW_DarkSteel', smooth=False)
    # walkway deck along the road side, handrails
    b.box((-200, -625, PT), (-75, 625, PT + 9), 'MI_OW_DarkSteel')
    for x in (-196, -79):
        for k in range(10):
            y = -620 + k * 137.8
            b.tube([(x, y, PT + 9), (x, y, PT + 105)], [2.6, 2.6], 4, 'MI_OW_RustSteel', smooth=False)
        b.tube([(x, -620, PT + 105), (x, 620, PT + 105)], [2.4, 2.4], 4, 'MI_OW_RustSteel', smooth=False)
        b.tube([(x, -620, PT + 58), (x, 620, PT + 58)], [1.8, 1.8], 4, 'MI_OW_RustSteel', smooth=False)
    hulls = []
    for py in piers:
        hulls.append(box_pts((-200, py - PW / 2, ZB), (232, py + PW / 2, PT)))
    hulls.append(box_pts((-200, -625, PT), (-75, 625, PT + 9)))
    for (ya, yb), (z0, z1) in zip(bays, leaf_z):
        hulls.append(box_pts((14, ya, z0), (50, yb, z1)))
        hulls.append(box_pts((14, ya - 40, 262), (70, yb + 40, 336)))
    finish(name, b, hulls, sharp=40.0,
           facing='long axis = Y (12.5 m, along the road/dam); +X = water side (cutwater noses, gate leaves); '
                  '-X = road side (steel walkway with handrails). Place the -X face against the road deck edge.',
           pivot='DECK LEVEL (z 0 = road deck / dam crest height), centre; waterline at z = -180 cm; piers run down '
                 'to -4.6 m so no gap shows above the water',
           notes='1940s dam outlet sluice: three concrete piers with cutwater noses, two 4.4 m bays with steel gate '
                 'leaves in guides (one closed, one half raised), steel hoist portals with gearboxes, handwheels and '
                 'spindles (top +3.36 m = 5.2 m above the water), wet algae band at the waterline.')
    tagged('SluiceGate', [name])
    save_blend('SluiceGate')
    preview('SluiceGate', (0, 0, -100), 2800, azimuths=(-40, 140), ground_z=-180.0, ground_rgb=(0.05, 0.07, 0.06))


# ---------------------------------------------------------------------------------------------------------- dead pines
PINES = [
    dict(key='01', H=1900.0, r0=29.0, crown=0.74, n=18, spread=1.0, lean=0.0, kind='tall', seed=101, voff=0.0),
    dict(key='02', H=1550.0, r0=25.0, crown=0.70, n=15, spread=0.85, lean=0.0, kind='medium', seed=202, voff=40.0),
    dict(key='03', H=1750.0, r0=27.0, crown=0.80, n=10, spread=0.95, lean=0.0, kind='sparse', seed=303, voff=-50.0),
    dict(key='04', H=880.0, r0=30.0, crown=1.0, n=0, spread=0.0, lean=0.0, kind='snapped', seed=404, voff=60.0),
    dict(key='05', H=1650.0, r0=26.0, crown=0.73, n=15, spread=0.9, lean=13.0, kind='leaning', seed=505, voff=-20.0),
]
PINE_V = 2100.0
GOLD = 2.39996


def trunk_axis(cfg, f):
    """Point on the trunk centre line at fraction f (before the lean rotation)."""
    H = cfg['H']
    r = rng(cfg['seed'] + 7)
    a1, a2 = r.uniform(0, 6.3), r.uniform(0, 6.3)
    return Vector((14 * math.sin(f * 4.1 + a1) * f, 11 * math.sin(f * 3.3 + a2) * f, H * f))


def trunk_radius(cfg, f):
    r0 = cfg['r0']
    z = cfg['H'] * f
    if z < 60:
        return r0 * (1.35 - 0.25 * z / 60)
    top = 6.0 if cfg['kind'] != 'snapped' else r0 * 0.62
    return r0 * 1.1 + (top - r0 * 1.1) * ((z - 60) / (cfg['H'] - 60)) ** 0.9


def pine_builder(cfg, lod):
    name = 'SM_OW_DeadPine_' + cfg['key'] + ('' if lod == 0 else '_LOD%d' % lod)
    b = Builder(name, ['MI_OW_PineBark'])
    r = rng(cfg['seed'])
    H = cfg['H']
    sides = (9, 6, 4)[lod]
    nseg = (14, 9, 5)[lod]
    fr = [0.0, 0.01] + [0.02 + 0.98 * (k / nseg) for k in range(1, nseg + 1)]
    pts = [trunk_axis(cfg, f) + Vector((0, 0, -25 if f == 0 else 0)) for f in fr]
    rad = [trunk_radius(cfg, f) * (1.1 if f == 0 else 1.0) for f in fr]
    voff = cfg['voff']

    def v_trunk(p):
        return min(0.92, max(0.002, (p.z + voff) / PINE_V))
    snapped = cfg['kind'] == 'snapped'
    jit = None
    if snapped:
        def jit(i, s):
            return 1.0
    b.tube(pts, rad, sides, 0, smooth=True, uv_mode='strip', v_of=v_trunk, u_repeat=2.0, cap_end=not snapped,
           cap_start=False)
    if snapped:
        # splintered top: a ring of spikes
        top = pts[-1]
        rr = rad[-1]
        ring = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            ring.append(top + Vector((math.cos(a) * rr, math.sin(a) * rr, 0)))
        tips = []
        for s in range(sides):
            a = 2 * math.pi * (s + 0.5) / sides
            h = r.uniform(8, 75) if lod < 2 else 30
            tips.append(top + Vector((math.cos(a) * rr * 0.55, math.sin(a) * rr * 0.55, h)))
        for s in range(sides):
            n = (s + 1) % sides
            vb = v_trunk(top)
            b.face([ring[s], ring[n], tips[s]], 0, [(s / sides * 2, vb), ((s + 1) / sides * 2, vb), ((s + .5) / sides * 2, 0.95)], True)
            b.face([ring[n], tips[n], tips[s]], 0, [((s + 1) / sides * 2, vb), ((s + 1.5) / sides * 2, 0.96), ((s + .5) / sides * 2, 0.95)], True)
        b.face(list(tips), 0, [(0.5 + 0.3 * math.cos(2 * math.pi * k / sides), 0.96) for k in range(sides)], False)
    # stubs on the mid trunk
    nst = (26, 12, 0)[lod]
    for k in range(nst):
        f = r.uniform(0.18, (cfg['crown'] if not snapped else 0.92) - 0.02)
        az = k * GOLD + r.uniform(-0.3, 0.3)
        base = trunk_axis(cfg, f)
        rb = trunk_radius(cfg, f)
        d = Vector((math.cos(az), math.sin(az), r.uniform(-0.35, 0.05))).normalized()
        L = r.uniform(14, 42)
        p0 = base + d * rb * 0.6
        p1 = base + d * (rb + L)
        rs = r.uniform(2.5, 4.5)
        b.tube([p0, p1], [rs, rs * 0.6], 4 if lod == 0 else 3, 0, smooth=False, uv_mode='strip',
               v_of=lambda p, p0=p0, L=L: 0.94 + 0.05 * min(1.0, (p - p0).length / (L + 1)), u_repeat=1.0,
               cap_start=False)
    # crown: main branches spread from the top part, nearly level near the top (flat broken crown)
    for k in range(cfg['n']):
        t = k / max(1, cfg['n'] - 1)
        f = cfg['crown'] + (0.985 - cfg['crown']) * t
        az = k * GOLD + r.uniform(-0.4, 0.4)
        base = trunk_axis(cfg, f)
        rb = trunk_radius(cfg, f)
        broken = r.random() < 0.28
        L = cfg['spread'] * (150 + 200 * min(1.0, 0.45 + 0.75 * t) * (1.0 - 0.5 * ss(0.85, 1.0, t))) * r.uniform(0.75, 1.15)
        if broken:
            L *= r.uniform(0.3, 0.55)
        elev = math.radians(r.uniform(4, 20))
        dirv = Vector((math.cos(az) * math.cos(elev), math.sin(az) * math.cos(elev), math.sin(elev)))
        side = Vector((-math.sin(az), math.cos(az), 0))
        bend = r.uniform(-0.15, 0.15)
        p0 = base
        p1 = base + dirv * L * 0.4 + side * L * bend * 0.3
        p2 = base + dirv * L * 0.75 + side * L * bend + Vector((0, 0, L * 0.06))
        p3 = base + Vector((dirv.x, dirv.y, 0)) * L + side * L * bend * 1.2 + Vector((0, 0, dirv.z * L * 0.75 + L * 0.16))
        bpts = [p0, p1, p2, p3] if lod < 2 else [p0, p2, p3]
        rr0 = max(6.5, min(11.0, rb * 0.7))
        brad = [rr0, rr0 * 0.72, rr0 * 0.45, 1.6 if lod == 0 else 2.4] if lod < 2 else [rr0, rr0 * 0.45, 2.6]
        if broken:
            brad[-1] = brad[-2] * 0.8
        b.tube(bpts, brad, (5, 4, 3)[lod], 0, smooth=True, uv_mode='strip',
               v_of=lambda p, p0=p0, L=L: 0.94 + 0.05 * min(1.0, (p - p0).length / L), u_repeat=1.0, cap_start=False)
        if lod == 2 or broken:
            continue
        # sub-branches and twigs
        for m in range(r.randint(3, 4)):
            g = r.uniform(0.35, 0.8)
            q0 = p1.lerp(p2, g) if g < 0.75 else p2
            sa = az + r.choice((-1, 1)) * r.uniform(0.5, 1.1)
            sl = L * r.uniform(0.3, 0.5)
            sd = Vector((math.cos(sa), math.sin(sa), r.uniform(0.05, 0.35))).normalized()
            q1 = q0 + sd * sl * 0.6
            q2 = q0 + sd * sl + Vector((0, 0, sl * 0.15))
            b.tube([q0, q1, q2], [3.8, 2.6, 1.3 if lod == 0 else 1.9], 4 if lod == 0 else 3, 0, smooth=True,
                   uv_mode='strip', v_of=lambda p, q0=q0, sl=sl: 0.94 + 0.05 * min(1.0, (p - q0).length / sl),
                   u_repeat=1.0, cap_start=False)
            if lod == 0:
                for n in range(2):
                    ta = sa + r.uniform(-1.2, 1.2)
                    tl = r.uniform(25, 55)
                    t0 = q1.lerp(q2, r.uniform(0.2, 0.9))
                    t1 = t0 + Vector((math.cos(ta), math.sin(ta), r.uniform(0.2, 0.8))).normalized() * tl
                    b.tube([t0, t1], [1.3, 0.6], 3, 0, smooth=False, uv_mode='strip',
                           v_of=lambda p: 0.97, u_repeat=1.0, cap_start=False, cap_end=False)
    # root flare: a few short roots at the base
    nr = (5, 4, 0)[lod]
    for k in range(nr):
        az = k * 2 * math.pi / nr + r.uniform(-0.3, 0.3)
        d = Vector((math.cos(az), math.sin(az), 0))
        p0 = Vector((0, 0, 45)) + d * cfg['r0'] * 0.55
        p1 = d * (cfg['r0'] * 1.2 + 22) + Vector((0, 0, 6))
        p2 = d * (cfg['r0'] * 1.2 + 55) + Vector((0, 0, -22))
        b.tube([p0, p1, p2], [cfg['r0'] * 0.55, cfg['r0'] * 0.38, cfg['r0'] * 0.22], 5 if lod == 0 else 4, 0, smooth=True,
               uv_mode='strip', v_of=lambda p: 0.02 + max(0.0, p.z) / PINE_V, u_repeat=1.0, cap_start=False)
    if cfg['lean']:
        m = Matrix.Rotation(math.radians(cfg['lean']), 4, 'Y')
        # keep the root plate on the ground: rotate about the base, then lift the low side a little
        b.verts = [m @ v for v in b.verts]
    return b


def build_pines():
    names = []
    for cfg in PINES:
        clear_scene()
        name = 'SM_OW_DeadPine_' + cfg['key']
        b0 = pine_builder(cfg, 0)
        lods = [pine_builder(cfg, 1), pine_builder(cfg, 2)]
        hz = 520.0 if cfg['kind'] != 'snapped' else min(600.0, cfg['H'])
        rr = cfg['r0'] * 1.15
        if cfg['lean']:
            hull = obox_pts((math.sin(math.radians(cfg['lean'])) * hz / 2, 0, hz / 2), (rr * 2, rr * 2, hz), 0,
                            pitch_deg=cfg['lean'])
        else:
            hull = box_pts((-rr, -rr, 0), (rr, rr, hz))
        finish(name, b0, [hull], lods=lods, sharp=70.0, facing='any yaw' + (
            '; leans 13 deg toward +X' if cfg['lean'] else ''),
            pivot='trunk base at ground level (roots dip to -25 cm)',
            notes='Dead Scots pine (%s, %.1f m): bare trunk, charred black to ~2.3 m, grey-brown lower bark, '
                  'reddish-grey fox bark above, flat broken crown of dead branches. 3 LODs; one trunk UCX box '
                  '(crown has no collision).' % (cfg['kind'], (cfg['H'] if cfg['kind'] != 'snapped' else cfg['H'] + 40) / 100.0))
        tagged('DeadPines', [name])
        names.append(name)
        save_blend('DeadPine_' + cfg['key'])
    if PREVIEW:
        clear_scene()
        for k, cfg in enumerate(PINES):
            o = pine_builder(cfg, 0).to_object(70.0)
            o.location.y = (k - 2) * 900
        preview('DeadPines', (0, 0, 800), 5200, azimuths=(-15,), elevation=42.0)


# ----------------------------------------------------------------------------------------------------------- erratics
ERRATICS = [dict(key='01', size=(160, 135, 135), seed=61), dict(key='02', size=(255, 210, 185), seed=62),
            dict(key='03', size=(350, 285, 245), seed=63)]


def noise3(seed):
    r = rng(seed)
    w = [(Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-1, 1))).normalized(), r.uniform(0, 6.3), r.uniform(1.2, 3.2))
         for _ in range(9)]

    def f(n):
        return sum(math.sin(d.dot(n) * fr * 1.7 + ph) / fr for d, ph, fr in w) / 3.0
    return f


def erratic_builder(cfg, subdiv, name):
    import bmesh as _bm
    bm = _bm.new()
    _bm.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    nz = noise3(cfg['seed'])
    sx, sy, sz = cfg['size']
    b = Builder(name, ['MI_OW_Granite'])
    pos = {}
    for v in bm.verts:
        n = v.co.normalized()
        k = 1.0 + 0.16 * nz(n) + 0.05 * nz(n * 2.7)
        p = Vector((n.x * sx / 2 * k, n.y * sy / 2 * k, n.z * sz / 2 * k))
        if p.z < 0:
            p.z *= 0.55          # flatter underside
        p.z += sz * 0.36         # ~22 % of the height below ground
        p.z -= sz * 0.5 * 0.0
        pos[v.index] = p
    # a few flattened facets (cleavage planes), then sink
    r = rng(cfg['seed'] + 1)
    planes = []
    for _ in range(4):
        n = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-0.2, 0.9))).normalized()
        ext = max(abs(n.x) * sx, abs(n.y) * sy, abs(n.z) * sz) / 2
        planes.append((n, ext * r.uniform(0.72, 0.85)))
    for p in pos.values():
        for n, d in planes:
            e = p.dot(n) - d - sz * 0.36 * n.z
            if e > 0:
                p -= n * e * 0.85
    zmin = min(p.z for p in pos.values())
    for p in pos.values():
        p.z -= (zmin + sz * 0.18)
    t = L.MATS['MI_OW_Granite']['tile']
    for f in bm.faces:
        q = [pos[v.index] for v in f.verts]
        b.face(q, 0, [(p.x / t, (p.y * 0.8 + p.z * 0.6) / t) for p in q], True)
    bm.free()
    return b


def build_erratics():
    names = []
    for cfg in ERRATICS:
        clear_scene()
        name = 'SM_OW_Erratic_' + cfg['key']
        b0 = erratic_builder(cfg, 4, name)
        b1 = erratic_builder(cfg, 3, name + '_LOD1')
        hb = erratic_builder(cfg, 2, 'hull')
        hull = [v.copy() for v in hb.verts]
        finish(name, b0, [hull], lods=[b1], sharp=80.0, facing='any yaw',
               pivot='ground level, centre; ~18 cm of the stone is below the pivot',
               notes='Glacial erratic granite boulder %.1f x %.1f m, %.1f m tall above ground; rounded, lichen grey. '
                     '2 LODs, one convex UCX.' % (cfg['size'][0] / 100, cfg['size'][1] / 100, cfg['size'][2] * 0.6 / 100))
        tagged('Erratics', [name])
        names.append(name)
        save_blend('Erratic_' + cfg['key'])
    if PREVIEW:
        clear_scene()
        x = -420
        for cfg in ERRATICS:
            o = erratic_builder(cfg, 4, 'p' + cfg['key']).to_object(80.0)
            o.location.x = x
            x += cfg['size'][0] + 140
        preview('Erratics', (0, 0, 80), 1500)


# ------------------------------------------------------------------------------------------------ mast comparison
def build_compare():
    """Side-by-side render of the three mast assets from their FBX (no export)."""
    clear_scene()
    xs = {'SM_OW_DFStation_Adcock': -2300, 'SM_OW_DFStation_Loop': 650, 'SM_OW_WerkMast': 2300}
    for name in L.MATS:
        L.get_material(name)
    for n, x in xs.items():
        bpy.ops.import_scene.fbx(filepath=L.OW_FBX + n + '.fbx')
        for o in list(bpy.context.selected_objects):
            if o.name.startswith('UCX_'):
                bpy.data.objects.remove(o)
                continue
            o.location.x = x
            for slot in o.material_slots:
                if slot.material:
                    slot.material = L.get_material(slot.material.name.split('.')[0])
    if PREVIEW:
        L.render_preview(L.OW_PREVIEW + 'OW_MastComparison.png', target=(0, 0, 300), distance=10500,
                         azimuth_deg=-90, elevation_deg=52, resolution=(1600, 900))


BUILDERS = {'tooth': build_tooth, 'cupola': build_cupola, 'werk': build_werk, 'portal': build_portal,
            'df_adcock': build_df_adcock, 'df_loop': build_df_loop, 'mast': build_mast, 'sluice': build_sluice,
            'pines': build_pines, 'erratics': build_erratics, 'compare': build_compare}


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ids = [a for a in args if not a.startswith('--')]
    if 'all' in ids:
        ids = list(BUILDERS)
    for i in ids:
        BUILDERS[i]()
    old = json.load(open(REPORT)) if os.path.exists(REPORT) else {}
    old.update(RESULTS)
    with open(REPORT, 'w') as f:
        json.dump(old, f, indent=1)
    print('OW_BUILD_DONE', list(RESULTS))


main()
