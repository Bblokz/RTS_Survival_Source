"""Shared bits for the Soviet flat-wall gate concepts (runs inside Blender via _ConceptTools/render_concepts.py).

The wall is SM_Wall_14 (Rus_Wall_Concrete_Long): a flat slab 22.6 m long, 5 m tall, 0.2 m thick.
Here it runs along X at y = 0; the enemy/front side is -Y (camera side). Gates sit centred on x = 0.
"""
import math

import concept_kit

concept_kit.PAL.update({
    'rus_concrete': (0.56, 0.41, 0.37, 1),   # weathered red-grey wall concrete
    'rus_concrete_dark': (0.42, 0.30, 0.27, 1),
    'rus_red': (0.72, 0.07, 0.05, 1),        # flag / star red
    'rus_green': (0.24, 0.31, 0.22, 1),      # Soviet green steel paint
    'rus_white': (0.90, 0.89, 0.85, 1),
    'rus_gold': (0.78, 0.60, 0.22, 1),
    'travel': (0.97, 0.97, 0.97, 1),         # ground marks for open/close travel
})

WALL_H = 5.0
WALL_T = 0.2
VIEW_ZOOM = 1.3
# The small view becomes a straight front elevation (the wall is 0.2 m thick, so the plan view reads poorly).
VIEW_OVERRIDES = {'rear': dict(rot=(90, 0, 0), target=(0, 0, 4.6), zoom=1.6, caption='Front elevation (enemy side)',
                               labels=False)}
CAMERA_TARGET = (0, 0, 2.5)
SCALE_REFS = False


def wall(k, x0, x1, y=0.0, name='wall'):
    """A run of the flat Soviet wall from x0 to x1."""
    lo, hi = min(x0, x1), max(x0, x1)
    return k.box(name, (hi - lo, WALL_T, WALL_H), ((lo + hi) / 2, y, 0), color='rus_concrete')


def wall_y(k, x, y0, y1, name='wall_return'):
    """A run of the wall along Y (returns)."""
    lo, hi = min(y0, y1), max(y0, y1)
    return k.box(name, (WALL_T, hi - lo, WALL_H), (x, (lo + hi) / 2, 0), color='rus_concrete')


def star_pts(r, r_in=None, rot=90):
    r_in = r_in or r * 0.42
    pts = []
    for i in range(10):
        a = math.radians(rot + i * 36)
        rr = r if i % 2 == 0 else r_in
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    return pts


def star(k, cx, face_y, cz, r, color='rus_red', ring=None, depth=0.05):
    """Red star standing on a vertical face that looks toward -Y (face_y = that face's y).
    ring = (radius, colour) adds a disc behind it."""
    if ring:
        k.cyl('star_ring', ring[0], depth, (cx, face_y, cz), color=ring[1], rot=(90, 0, 0), segments=40)
        face_y -= depth
    s = k.poly('star', star_pts(r), depth, color=color)
    s.location = (cx, face_y, cz)
    s.rotation_euler = (math.radians(90), 0, 0)
    return s


def banner(k, cx, face_y, z_rod, w=3.4, h=2.6):
    """The wall's flag: steel rod with end knobs, red cloth with a star roundel (as on Rus_Wall_*)."""
    k.cyl('banner_rod', 0.06, w + 0.8, (cx - (w + 0.8) / 2, face_y - 0.12, z_rod), color='steel', rot=(0, 90, 0),
          segments=10)
    for sx in (-1, 1):
        k.cyl('banner_knob', 0.12, 0.2, (cx + sx * (w / 2 + 0.4), face_y - 0.12, z_rod - 0.1), color='steel', segments=10)
        k.box('banner_bracket', (0.08, 0.2, 0.3), (cx + sx * (w / 2 + 0.15), face_y - 0.06, z_rod - 0.15), color='steel')
    k.box('banner_cloth', (w, 0.04, h), (cx, face_y - 0.16, z_rod - 0.05), color='rus_red', origin=(0, 0, 0.5))
    star(k, cx, face_y - 0.18, z_rod - 0.05 - h * 0.5, h * 0.26, color='rus_red', ring=(h * 0.33, 'rus_gold'))


def arc(k, cx, cy, r, a0, a1, step=8, w=0.12):
    """Dashed swing arc on the ground."""
    a = a0
    while a < a1 - 1e-3:
        b = min(a + step * 0.6, a1)
        m = math.radians((a + b) / 2)
        length = r * math.radians(b - a)
        k.box('arc', (length, w, 0.04), (cx + r * math.cos(m), cy + r * math.sin(m), 0), color='travel',
              yaw=math.degrees(m) + 90)
        a += step


def dashed(k, p0, p1, dash=0.8, gap=0.5, w=0.12):
    """Dashed straight line on the ground."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    length = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    t = 0.0
    while t < length:
        seg = min(dash, length - t)
        c = t + seg / 2
        k.box('dash', (seg, w, 0.04), (p0[0] + dx * c / length, p0[1] + dy * c / length, 0), color='travel', yaw=yaw)
        t += dash + gap


def arrow(k, loc, yaw, label):
    """Travel arrow on the ground (white)."""
    k.group((loc[0], loc[1], 0), yaw)
    k.box('arrow_shaft', (2.0, 0.5, 0.05), (-0.5, 0, 0), color='travel')
    k.poly('arrow_head', [(0.5, -0.7), (1.8, 0), (0.5, 0.7)], 0.05, color='travel')
    k.end_group()
    k.label(label, 'gate', (loc[0], loc[1], 0.4))


def scale_tank(k, loc=(-1.5, -9.0)):
    """Panzer IV (5.9 x 2.9 m) heading for the gate, and a rifleman."""
    k.tank((loc[0], loc[1], 0), yaw=90)
    k.soldier((loc[0] + 3.4, loc[1] - 1.0, 0))
