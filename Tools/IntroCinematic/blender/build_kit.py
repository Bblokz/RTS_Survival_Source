"""Ural Shield wall kit - geometry + FBX export.

Run:  blender.exe --factory-startup -b -P build_kit.py
Writes SM_UralShield_*.fbx to Saved/IntroCinematic/import/UralShield and a work .blend
(used by render_previews.py) plus kit_stats.json (used by verify_and_readme.py).
"""
import os
import sys
import json
import math
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import us_common as C
from us_common import MB, R_HOOD, R_LEAF, R_TRIM, R_DARK, R_LENS, R_PARA, R_IRON, R_GEN, R_TALL, ZUP

BUDGET = {'SM_UralShield_Segment': 18000, 'SM_UralShield_GateFrame': 25000,
          'SM_UralShield_GateLeaf_L': 4000, 'SM_UralShield_GateLeaf_R': 4000, 'SM_UralShield_Far': 300}

A0 = ('abs', 0.0)


# ------------------------------------------------------------------ oriented helpers
def frame(f):
    f = Vector(f).normalized()
    r = ZUP.cross(f).normalized()
    return f, r


def P(o, f, r, a, b, z):
    return Vector(o) + f * a + r * b + Vector((0, 0, z))


def obox(mb, o, fdir, a0, a1, b0, b1, z0, z1, region, skip='', v=None, regions=None, top_rise=0.0):
    """Box in a local frame: a = forward (fdir), b = sideways, z up. Faces: f,k(back),l,r,u,d.
    top_rise lifts the back edge of the top face (sloped hood)."""
    f, r = frame(fdir)
    regions = regions or {}
    zt_f, zt_b = z1, z1 + top_rise

    def q(key, pts, hint):
        if key in skip:
            return
        mb.face(pts, regions.get(key, region), hint, v)
    q('f', [P(o, f, r, a1, b0, z0), P(o, f, r, a1, b1, z0), P(o, f, r, a1, b1, zt_f), P(o, f, r, a1, b0, zt_f)], f)
    q('k', [P(o, f, r, a0, b0, z0), P(o, f, r, a0, b1, z0), P(o, f, r, a0, b1, zt_b), P(o, f, r, a0, b0, zt_b)], -f)
    q('l', [P(o, f, r, a0, b0, z0), P(o, f, r, a1, b0, z0), P(o, f, r, a1, b0, zt_f), P(o, f, r, a0, b0, zt_b)], -r)
    q('r', [P(o, f, r, a0, b1, z0), P(o, f, r, a1, b1, z0), P(o, f, r, a1, b1, zt_f), P(o, f, r, a0, b1, zt_b)], r)
    q('u', [P(o, f, r, a0, b0, zt_b), P(o, f, r, a1, b0, zt_f), P(o, f, r, a1, b1, zt_f), P(o, f, r, a0, b1, zt_b)], ZUP)
    q('d', [P(o, f, r, a0, b0, z0), P(o, f, r, a1, b0, z0), P(o, f, r, a1, b1, z0), P(o, f, r, a0, b1, z0)], -ZUP)


def inset_front(mb, o, fdir, a, b0, b1, z0, z1, mb_, mz, depth, region, back_region, v=None):
    """Front face at local a with a recessed opening (frame + reveal + back)."""
    f, r = frame(fdir)
    hb0, hb1, hz0, hz1 = b0 + mb_, b1 - mb_, z0 + mz, z1 - mz
    for pts in ([(b0, z0), (b1, z0), (b1, hz0), (b0, hz0)], [(b0, hz1), (b1, hz1), (b1, z1), (b0, z1)],
                [(b0, hz0), (hb0, hz0), (hb0, hz1), (b0, hz1)], [(hb1, hz0), (b1, hz0), (b1, hz1), (hb1, hz1)]):
        mb.face([P(o, f, r, a, b, z) for b, z in pts], region, f, v)
    ab = a - depth
    mb.face([P(o, f, r, a, hb0, hz0), P(o, f, r, a, hb1, hz0), P(o, f, r, ab, hb1, hz0), P(o, f, r, ab, hb0, hz0)], region, ZUP, v)
    mb.face([P(o, f, r, a, hb0, hz1), P(o, f, r, a, hb1, hz1), P(o, f, r, ab, hb1, hz1), P(o, f, r, ab, hb0, hz1)], region, -ZUP, v)
    mb.face([P(o, f, r, a, hb0, hz0), P(o, f, r, a, hb0, hz1), P(o, f, r, ab, hb0, hz1), P(o, f, r, ab, hb0, hz0)], region, r, v)
    mb.face([P(o, f, r, a, hb1, hz0), P(o, f, r, a, hb1, hz1), P(o, f, r, ab, hb1, hz1), P(o, f, r, ab, hb1, hz0)], region, -r, v)
    mb.face([P(o, f, r, ab, hb0, hz0), P(o, f, r, ab, hb1, hz0), P(o, f, r, ab, hb1, hz1), P(o, f, r, ab, hb0, hz1)], back_region, f)


def hood(mb, o, fdir, width, height, protrude, embed, region=R_TALL, v=None, margin=0.45, mz=0.55, depth=0.8):
    """Concrete casemate hood with a dark recessed embrasure. o = bottom centre on the wall face."""
    w2 = width / 2
    obox(mb, o, fdir, -embed, protrude, -w2, w2, 0.0, height, region, skip='fk', v=v, top_rise=0.5,
         regions={'u': R_GEN, 'd': R_GEN})
    inset_front(mb, o, fdir, protrude, -w2, w2, 0.0, height, margin, mz, depth, region, R_DARK, v)


def floodlight(mb, o, fdir, s=1.0):
    """Bracket arm + hooded lamp housing. o = attachment point on the wall."""
    obox(mb, o, fdir, -0.4, 1.3 * s, -0.16, 0.16, -0.16, 0.16, R_TRIM, skip='kf')
    obox(mb, o, fdir, -0.4, 1.0 * s, -0.12, 0.12, -1.3 * s, -0.16, R_TRIM, skip='ku')      # strut plate
    obox(mb, o, fdir, 1.0 * s, 2.2 * s, -0.95 * s, 0.95 * s, -1.15 * s, 0.55 * s, R_TRIM, skip='f')
    inset_front(mb, o, fdir, 2.2 * s, -0.95 * s, 0.95 * s, -1.15 * s, 0.55 * s, 0.16 * s, 0.16 * s, 0.22 * s, R_TRIM, R_LENS)
    obox(mb, o, fdir, 0.9 * s, 2.8 * s, -1.05 * s, 1.05 * s, 0.55 * s, 0.68 * s, R_TRIM)       # visor


def slit(mb, o, fdir, width, height, embed=0.05):
    """Dark vision slit, a thin iron-framed box."""
    obox(mb, o, fdir, -embed, 0.10, -width / 2 - 0.15, width / 2 + 0.15, -0.15, height + 0.15, R_TRIM, skip='k')
    obox(mb, o, fdir, 0.0, 0.13, -width / 2, width / 2, 0.0, height, R_TRIM, skip='k', regions={'f': R_DARK})


# ------------------------------------------------------------------ wall cross-section
def wall_profile(x_shift_w=0.0):
    """Hero cross-section. Returns (points, specs). West side first, over the top, down the east."""
    w = x_shift_w
    pts = [(-4.6, 0.0), (-4.6, 2.5), (-4.1, 3.0), (-3.79, 15.0), (-4.25, 15.0), (-4.25, 15.5), (-3.76, 16.1),
           (-3.4, 30.0),
           (-4.2, 30.0), (-4.2, 30.4), (-4.0, 30.4), (-4.0, 35.6), (-4.2, 35.6), (-4.2, 36.0), (-3.4, 36.0),
           (-3.4, 37.4), (-2.2, 37.4), (-2.2, 38.6), (-3.4, 38.6), (-3.4, 39.8), (-5.0, 41.4), (-5.0, 45.0),
           (-4.0, 45.0), (-4.0, 43.6)]
    specs = [(R_TALL, A0), (R_TALL, None), (R_TALL, A0), (R_TALL, None), (R_TALL, A0), (R_TALL, None), (R_TALL, A0),
             (R_IRON, None), (R_IRON, ('abs', 30.0)), (R_IRON, None), (R_IRON, ('abs', 30.0)), (R_IRON, None),
             (R_IRON, ('abs', 30.0)), (R_IRON, None),
             (R_GEN, ('abs', 36.0)), (R_GEN, None), (R_DARK, None), (R_GEN, None), (R_GEN, ('abs', 36.0)),
             (R_GEN, ('off', 3.7)), (R_PARA, ('abs', 41.4)), (R_PARA, None), (R_PARA, ('abs', 41.4))]
    pts = [(x + w, z) for x, z in pts]
    return pts, specs


def inner_profile():
    """East side from the walkway edge down to the ground."""
    pts = [(1.6, 43.6), (1.6, 44.8), (2.4, 44.8), (2.4, 41.6), (3.0, 41.0), (3.2, 36.0), (3.8, 35.4),
           (4.0, 30.6), (4.6, 30.0), (7.0, 0.0)]
    specs = [(R_GEN, ('abs', 40.0)), (R_GEN, None), (R_GEN, ('abs', 39.0)), (R_GEN, ('off', 1.0)),
             (R_GEN, ('abs', 35.5)), (R_GEN, ('off', 1.0)), (R_GEN, ('abs', 30.0)), (R_GEN, None), (R_TALL, A0)]
    return pts, specs


def wall_run(mb, y0, y1):
    wp, ws = wall_profile()
    ip, isp = inner_profile()
    pts = wp + ip
    specs = ws + [(R_GEN, ('off', 0.2))] + isp      # walkway joins the two halves
    mb.profile(pts, specs, y0, y1)


def x_wall_west(z):
    """X of the west batter between the plinth and the iron band."""
    return -4.6 + 1.2 * (z - 3.1) / 26.9


def buttress(mb, yc, uoff):
    xb = -2.6
    st = [(0.0, -7.0, 2.6, 0.55), (2.5, -7.0, 2.6, 0.55), (3.2, -6.4, 2.5, 0.5), (15.0, -5.6, 2.3, 0.45),
          (15.6, -5.1, 2.25, 0.45), (28.0, -4.6, 2.0, 0.4), (29.75, -3.2, 2.0, 0.0)]
    rings = []
    for z, xf, hw, c in st:
        rings.append([(xb, yc - hw, z), (xf + c, yc - hw, z), (xf, yc - hw + c, z), (xf, yc + hw - c, z),
                      (xf + c, yc + hw, z), (xb, yc + hw, z)])
    specs = [(R_TALL, A0), (R_TALL, None), (R_TALL, A0), (R_TALL, None), (R_TALL, A0), (R_GEN, None)]
    mb.loft(rings, specs, closed=False, uoff=uoff)


def pilaster(mb, yc, uoff):
    """East-side counterfort: vertical fin against the battered inner face."""
    xb, hw = 3.9, 1.6
    st = [(0.0, 6.9), (26.5, 6.9), (30.2, 4.2)]
    rings = [[(xb, yc + hw, z), (xf, yc + hw, z), (xf, yc - hw, z), (xb, yc - hw, z)] for z, xf in st]
    mb.loft(rings, [(R_TALL, A0), (R_GEN, None)], closed=False, uoff=uoff)


def wall_details(mb, ys_butt, ys_pier, ys_merlon, ys_cupola, hero=True):
    # The concrete below the iron band (z < 30 m) is blind: no casemate hoods or gun ports (owner, 2026-11-21:
    # rows of dark embrasures read as windows).
    for i, y in enumerate(ys_butt):
        buttress(mb, y, 0.137 * (i + 1))
        pilaster(mb, y, 0.211 * (i + 1))
        if hero:
            floodlight(mb, (-3.5, y, 38.7), (-1, 0, 0))
    for y in ys_pier:
        mb.box((-3.55, y - 0.6, 37.3), (-2.0, y + 0.6, 38.7), R_GEN, skip='+x', v=('abs', 36.0))   # gallery pier
        mb.box((-4.38, y - 0.25, 30.1), (-3.9, y + 0.25, 35.9), R_IRON, skip='+x', v=('abs', 30.0))  # band rib
    for y in ys_merlon:
        mb.box((-5.12, y - 3.0, 44.9), (-3.88, y + 3.0, 46.2), R_PARA, v=('abs', 41.6))
    for y in ys_cupola:
        # armoured observation pulpit bolted over the parapet, domed cap, bracket underneath
        mb.box((-5.75, y - 1.6, 42.4), (-3.3, y + 1.6, 45.5), R_PARA, v=('abs', 41.4))
        mb.face([(-5.0, y - 1.6, 41.4), (-5.0, y + 1.6, 41.4), (-5.75, y + 1.6, 42.4), (-5.75, y - 1.6, 42.4)],
                R_PARA, (-1, 0, -1))
        for s in (-1, 1):
            mb.face([(-5.0, y + s * 1.6, 41.4), (-5.75, y + s * 1.6, 42.4), (-5.0, y + s * 1.6, 42.4)], R_PARA, (0, s, 0))
        mb.cylinder(-4.5, y, 45.5, 46.2, 1.15, 14, R_TRIM, dome=0.9, rings=3)
        slit(mb, (-5.75, y, 44.2), (-1, 0, 0), 2.0, 0.35)


# ------------------------------------------------------------------ pieces
def build_segment():
    mb = MB('SM_UralShield_Segment')
    wall_run(mb, -C.HALF, C.HALF)
    wall_details(mb, [-22.5, -7.5, 7.5, 22.5],
                 [-27.5 + 5 * k for k in range(12)], [-25, -5, 5, 25], [-15, 15])
    return mb.build()


def build_far():
    mb = MB('SM_UralShield_Far')
    pts = [(-4.6, 0.0), (-3.4, 30.0), (-4.2, 30.0), (-4.2, 36.0), (-3.4, 36.0), (-3.4, 37.4), (-2.2, 37.4),
           (-2.2, 38.6), (-3.4, 38.6), (-3.4, 39.8), (-5.0, 41.4), (-5.0, 45.0), (2.4, 45.0),
           (3.0, 40.0), (3.8, 35.0), (4.6, 30.0), (7.0, 0.0)]
    specs = [(R_TALL, A0), (R_IRON, None), (R_IRON, ('abs', 30.0)), (R_IRON, None), (R_GEN, ('abs', 36.0)),
             (R_GEN, None), (R_DARK, None), (R_GEN, None), (R_GEN, ('abs', 36.0)), (R_GEN, ('off', 3.7)),
             (R_PARA, ('abs', 41.4)), (R_TALL, ('off', 10.0)),
             (R_GEN, ('abs', 39.5)), (R_GEN, ('abs', 34.5)), (R_GEN, ('abs', 29.5)), (R_TALL, A0)]
    mb.profile(pts, specs, -C.HALF, C.HALF)
    for i, yc in enumerate([-22.5, -7.5, 7.5, 22.5]):
        st = [(0.0, -7.0, 2.6), (2.5, -7.0, 2.6), (28.0, -4.6, 2.0), (29.75, -3.2, 2.0)]
        rings = [[(-2.6, yc - hw, z), (xf, yc - hw, z), (xf, yc + hw, z), (-2.6, yc + hw, z)] for z, xf, hw in st]
        mb.loft(rings, [(R_TALL, A0), (R_TALL, A0), (R_GEN, None)], closed=False, uoff=0.137 * (i + 1))
        rings = [[(3.9, yc + 1.6, z), (xf, yc + 1.6, z), (xf, yc - 1.6, z), (3.9, yc - 1.6, z)]
                 for z, xf in [(0.0, 6.9), (26.5, 6.9), (30.2, 4.2)]]
        mb.loft(rings, [(R_TALL, A0), (R_GEN, None)], closed=False, uoff=0.211 * (i + 1))
    for y in [-25, -5, 5, 25]:
        mb.box((-5.0, y - 3.0, 45.0), (-4.0, y + 3.0, 46.2), R_PARA, skip='-z', v=('abs', 41.6))   # flush: underside never visible
    for y in [-15, 15]:
        mb.box((-5.75, y - 1.6, 42.4), (-3.3, y + 1.6, 46.4), R_PARA, v=('abs', 41.8))
    return mb.build()


def tower(mb, sy):
    def ring(z, xw, xe, yo, yi, c):
        return [(xw, sy * yi, z), (xw, sy * (yo - c), z), (xw + c, sy * yo, z), (xe - c, sy * yo, z),
                (xe, sy * (yo - c), z), (xe, sy * yi, z)]
    st = [
        ((0.0, -12.0, 8.5, 19.6, 8.0, 1.5), (R_TALL, A0)),
        ((3.0, -12.0, 8.5, 19.6, 8.0, 1.5), (R_TALL, None)),
        ((3.6, -11.4, 8.1, 19.2, 8.0, 1.5), (R_TALL, A0)),
        # tower band lips sit 5 cm below the wall's band lips (z 30 / 36) so the two never share a plane
        ((29.95, -8.0, 6.2, 18.4, 8.0, 1.2), (R_IRON, None)),
        ((29.95, -8.4, 6.6, 18.8, 8.0, 1.2), (R_IRON, ('abs', 29.95))),
        ((35.95, -8.4, 6.6, 18.8, 8.0, 1.2), (R_IRON, None)),
        ((35.95, -7.8, 6.0, 18.2, 8.0, 1.2), (R_GEN, ('abs', 35.95))),
        ((41.4, -7.8, 6.0, 18.2, 8.0, 1.2), (R_GEN, None)),
        ((41.4, -8.15, 6.35, 18.55, 8.0, 1.2), (R_GEN, ('abs', 41.4))),
        ((42.0, -8.15, 6.35, 18.55, 8.0, 1.2), (R_GEN, None)),
        ((42.0, -7.8, 6.0, 18.2, 8.0, 1.2), (R_GEN, ('abs', 42.0))),
        ((48.0, -7.8, 6.0, 18.2, 8.0, 1.2), (R_GEN, ('off', 4.2))),
        ((49.2, -8.8, 7.0, 19.2, 7.0, 1.2), (R_PARA, ('abs', 49.2))),
        ((53.8, -8.8, 7.0, 19.2, 7.0, 1.2), (R_PARA, None)),
    ]
    rings = [ring(*a) for a, _ in st]
    specs = [s for _, s in st][:-1]
    mb.loft(rings, specs, closed=True, uoff=0.31 if sy > 0 else 0.63)

    cy = 13.5
    # roof: parapet top, inner wall, deck
    top_o = ring(53.8, -8.8, 7.0, 19.2, 7.0, 1.2)
    top_i = ring(53.8, -8.0, 6.2, 18.4, 7.8, 0.8)
    deck = ring(52.8, -8.0, 6.2, 18.4, 7.8, 0.8)
    for j in range(6):
        k = (j + 1) % 6
        mb.face([top_o[j], top_o[k], top_i[k], top_i[j]], R_PARA, ZUP)
    cen = Vector((-0.9, sy * 13.1, 53.3))
    for j in range(6):
        k = (j + 1) % 6
        q = [Vector(top_i[j]), Vector(top_i[k]), Vector(deck[k]), Vector(deck[j])]
        fc = (q[0] + q[1] + q[2] + q[3]) / 4
        mb.face(q, R_PARA, cen - fc, ('abs', 50.5))
    mb.face(deck, R_TALL, ZUP, ('off', 5.0))
    # merlons on the west / east parapets
    for xa, xb_ in ((-8.9, -7.9), (6.1, 7.1)):
        for yc in (9.9, 16.3):
            ya, yb = sorted((sy * (yc - 1.6), sy * (yc + 1.6)))
            mb.box((xa, ya, 53.7), (xb_, yb, 55.0), R_PARA, v=('abs', 50.5))
    # armoured cupola on the roof with a searchlight on its crown
    mb.cylinder(-0.9, sy * 13.1, 52.8, 55.6, 3.0, 16, R_TRIM, dome=1.6, rings=3)
    slit(mb, (-3.9, sy * 13.1, 54.5), (-1, 0, 0), 2.4, 0.35, embed=0.6)
    mb.box((-1.5, sy * 13.1 - 0.5, 56.9), (0.35, sy * 13.1 + 0.5, 58.9), R_TRIM, skip='-z')
    floodlight(mb, (-0.2, sy * 13.1, 58.6), (-1, 0, 0), 0.9)
    # casemates only on the iron-era top storey: the concrete below the band stays blind
    hood(mb, (-7.8, sy * cy, 43.8), (-1, 0, 0), 6.0, 2.4, 0.8, 0.5, region=R_GEN, v=('abs', 42.0), mz=0.5, depth=0.6)
    hood(mb, (6.0, sy * cy, 43.8), (1, 0, 0), 6.0, 2.4, 0.8, 0.5, region=R_GEN, v=('abs', 42.0), mz=0.5, depth=0.6)
    # iron storey: one long vision slit per face
    slit(mb, (-8.8, sy * 13.1, 51.2), (-1, 0, 0), 7.5, 0.7)
    slit(mb, (7.0, sy * 13.1, 51.2), (1, 0, 0), 7.5, 0.7)
    slit(mb, (-0.9, sy * 19.2, 51.2), (0, sy, 0), 9.0, 0.7)
    slit(mb, (-0.9, sy * 7.0, 51.2), (0, -sy, 0), 9.0, 0.7)
    # band ribs on the west face
    for yy in (9.5, 12.0, 14.5, 17.0):
        ya, yb = sorted((sy * (yy - 0.25), sy * (yy + 0.25)))
        mb.box((-8.78, ya, 30.1), (-8.3, yb, 35.9), R_IRON, skip='+x', v=('abs', 30.0))


def build_gateframe():
    mb = MB('SM_UralShield_GateFrame')
    for sy in (1, -1):
        ya, yb = sorted((sy * 17.5, sy * C.HALF))
        wall_run(mb, ya, yb)
        wall_details(mb, [sy * 22.5], [sy * 22.5, sy * 27.5], [sy * 25], [])
        tower(mb, sy)
        # door stop frame behind the leaf
        ya, yb = sorted((sy * 7.6, sy * 8.3))
        mb.box((-3.74, ya, 0.0), (-3.3, yb, 12.6), R_TRIM, skip='-z+z')
    mb.box((-3.74, -8.3, 12.6), (-3.3, 8.3, 13.3), R_TRIM, skip='+z')
    # gatehouse block over the portal
    wp, ws = wall_profile(-2.0)
    i30 = [i for i, p in enumerate(wp) if abs(p[1] - 30.0) < 1e-6][0]     # top of the battered face
    xi13 = 4.6 + 2.4 * (17.0 / 30.0)
    head = [(xi13, 13.0), (-6.6, 13.0), (-6.6, 16.5), (-5.4, 17.3)]
    hspec = [(R_TALL, ('off', 8.0)), (R_GEN, ('abs', 13.0)), (R_GEN, ('off', 4.0)), (R_TALL, A0)]
    ip, isp = inner_profile()
    ip = ip[:-1] + [(xi13, 13.0)]
    pts = head + wp[i30:] + ip
    specs = hspec + ws[i30:] + [(R_TALL, ('off', 12.0))] + isp
    mb.profile(pts, specs, -8.5, 8.5, uoff=0.45)
    for y in (-4.9, 0.0, 4.9):
        mb.box((-5.55, y - 0.6, 37.3), (-4.0, y + 0.6, 38.7), R_GEN, skip='+x', v=('abs', 36.0))
        mb.box((-6.38, y - 0.25, 30.1), (-5.9, y + 0.25, 35.9), R_IRON, skip='+x', v=('abs', 30.0))
    for y in (-4.4, 4.4):
        mb.box((-7.12, y - 3.0, 44.9), (-5.88, y + 3.0, 46.2), R_PARA, v=('abs', 41.6))
    for y in (-6.0, -2.0, 2.0, 6.0):
        floodlight(mb, (-5.4, y, 27.0), (-1, 0, 0), 1.1)
    ob = mb.build()
    for nm, sy in (('SOCKET_Hinge_L', 1), ('SOCKET_Hinge_R', -1)):
        e = bpy.data.objects.new(nm, None)
        e.empty_display_size = 100
        e.location = (C.HINGE_X * C.CM, sy * C.HINGE_Y * C.CM, 0)
        bpy.context.scene.collection.objects.link(e)
        e.parent = ob
    return ob


def build_leaf(sy, name):
    """sy=+1: leaf L (hinge at +Y, on the left seen from outside). Local w = distance from the hinge."""
    mb = MB(name)
    X0 = C.HINGE_X
    HY = C.HINGE_Y
    W = C.LEAF_W
    Z0, Z1 = C.LEAF_Z0, C.LEAF_Z1
    pad = (8.0 - W) / 2

    def yw(w):
        return sy * (HY - w)
    # LEAF patch references so that frame/rail faces show the plate texture at their true position
    ys = sorted((yw(0.0), yw(W)))
    ref_front = (-ys[1] - pad, 0.0)     # n = -X : s = -y
    ref_back = (ys[0] - pad, 0.0)       # n = +X : s = +y

    def lbox(w0, w1, d0, d1, z0, z1, skip='', leaf_front=True, leaf_back=False):
        ya, yb = sorted((yw(w0), yw(w1)))
        lo, hi = (X0 + d0, ya, z0), (X0 + d1, yb, z1)
        mb.box(lo, hi, R_TRIM, skip=skip + '-x+x')
        if '-x' not in skip:
            if leaf_front:
                mb.face([(lo[0], ya, z0), (lo[0], yb, z0), (lo[0], yb, z1), (lo[0], ya, z1)], R_LEAF, (-1, 0, 0), ref=ref_front)
            else:
                mb.face([(lo[0], ya, z0), (lo[0], yb, z0), (lo[0], yb, z1), (lo[0], ya, z1)], R_TRIM, (-1, 0, 0))
        if '+x' not in skip:
            if leaf_back:
                mb.face([(hi[0], ya, z0), (hi[0], yb, z0), (hi[0], yb, z1), (hi[0], ya, z1)], R_LEAF, (1, 0, 0), ref=ref_back)
            else:
                mb.face([(hi[0], ya, z0), (hi[0], yb, z0), (hi[0], yb, z1), (hi[0], ya, z1)], R_TRIM, (1, 0, 0))

    lbox(0.0, W, 0.14, 1.06, Z0, Z1, leaf_back=True)                       # core slab
    rails = [13.0 / 3, 26.0 / 3]
    for d0, d1, skip, front in ((0.0, 0.14, '+x', True), (1.06, 1.2, '-x', False)):
        kw = dict(skip=skip, leaf_front=front, leaf_back=not front)
        lbox(0.0, 0.5, d0, d1, Z0, Z1, **kw)                               # hinge stile
        lbox(W - 0.5, W, d0, d1, Z0, Z1, **kw)                             # meeting stile
        lbox(0.5, W - 0.5, d0, d1, Z0, 0.6, **kw)                          # bottom rail
        lbox(0.5, W - 0.5, d0, d1, 12.4, Z1, **kw)                         # top rail
        e = 0.03 if front else -0.03
        for zc in rails:
            lbox(0.5, W - 0.5, d0 + max(e, 0), d1 + min(e, 0), zc - 0.24, zc + 0.24, **kw)
    # cross bracing in the top and bottom panels (outer face)
    def brace(wa, za, wb, zb, d_front, width=0.38):
        a, b = Vector((wa, za)), Vector((wb, zb))
        t = (b - a).normalized()
        nrm = Vector((-t.y, t.x)) * (width / 2)
        cs = [a + nrm, b + nrm, b - nrm, a - nrm]
        fr = [(X0 + d_front, yw(c.x), c.y) for c in cs]
        bk = [(X0 + 0.14, yw(c.x), c.y) for c in cs]
        mb.face(fr, R_TRIM, (-1, 0, 0))
        cen = Vector((X0 + 0.1, yw((wa + wb) / 2), (za + zb) / 2))
        for i in (0, 2):
            j = (i + 1) % 4
            q = [fr[i], fr[j], bk[j], bk[i]]
            fc = sum((Vector(p) for p in q), Vector()) / 4
            mb.face(q, R_TRIM, fc - cen)
    for zlo, zhi in ((0.6, rails[0] - 0.24), (rails[1] + 0.24, 12.4)):
        brace(0.5, zlo, W - 0.5, zhi, 0.05)
        brace(0.5, zhi, W - 0.5, zlo, 0.095)
    # plain five-pointed star relief, faceted, in the middle panel
    cw, cz, R = W / 2, 6.5, 1.85
    xs = X0 + 0.14
    tip = (xs - 0.34, yw(cw), cz)
    star = []
    for i in range(10):
        ang = math.pi / 2 + i * math.pi / 5
        rr = R if i % 2 == 0 else R * 0.382
        star.append((xs, yw(cw + rr * math.cos(ang)), cz + rr * math.sin(ang)))
    for i in range(10):
        mb.face([star[i], star[(i + 1) % 10], tip], R_TRIM, (-1, 0, 0))
    # hinge knuckles + straps
    for zc in (1.9, 6.5, 11.1):
        mb.cylinder(X0, sy * HY, zc - 0.75, zc + 0.75, 0.4, 10, R_TRIM, bottom=True)
        lbox(0.0, 1.9 if abs(zc - 6.5) < 1 else 3.2, -0.07, 0.14, zc - 0.32, zc + 0.32, skip='+x', leaf_front=False)
    if sy > 0:   # meeting-edge cover strip on leaf L, overlaps leaf R
        lbox(W - 0.2, W + 0.2, -0.1, 0.0, Z0, Z1, leaf_front=False)
    return mb.build(origin=(X0, sy * HY, 0.0))


def export(ob, path):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    for ch in ob.children:
        ch.select_set(True)
    bpy.context.view_layer.objects.active = ob
    loc = ob.location.copy()
    ob.location = (0, 0, 0)          # leaves: pivot (hinge) goes to the FBX origin
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             global_scale=0.01, axis_forward='-Z', axis_up='Y', mesh_smooth_type='FACE',
                             bake_anim=False, path_mode='STRIP', object_types={'MESH', 'EMPTY'},
                             use_custom_props=False)
    ob.location = loc
    print('EXPORT', path)


def main():
    C.setup_scene()
    C.make_material()
    obs = [build_segment(), build_gateframe(), build_leaf(1, 'SM_UralShield_GateLeaf_L'),
           build_leaf(-1, 'SM_UralShield_GateLeaf_R'), build_far()]
    stats = {}
    for ob in obs:
        tris = C.tri_count(ob)
        assert tris <= BUDGET[ob.name], '%s over budget: %d' % (ob.name, tris)
        co = [v.co for v in ob.data.vertices]
        lo = [min(c[i] for c in co) for i in range(3)]
        hi = [max(c[i] for c in co) for i in range(3)]
        uv = ob.data.uv_layers[0].data
        vs = [l.uv[1] for l in uv]
        assert len(uv) == len(ob.data.loops) and min(vs) >= -1e-4 and max(vs) <= 1.0001
        stats[ob.name] = dict(tris=tris, verts=len(co), bounds_min_cm=[round(x, 1) for x in lo],
                              bounds_max_cm=[round(x, 1) for x in hi],
                              scene_location_cm=[round(x, 1) for x in ob.location],
                              materials=[m.name for m in ob.data.materials])
        print('STATS', ob.name, stats[ob.name])
        export(ob, os.path.join(C.OUT, ob.name + '.fbx'))
    os.makedirs(C.WORK, exist_ok=True)
    json.dump(stats, open(C.STATS, 'w'), indent=1)
    bpy.ops.wm.save_as_mainfile(filepath=C.BLEND)
    print('BUILD OK')


main()
