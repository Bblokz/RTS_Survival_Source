import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gate_common import *  # noqa: F401,F403

TITLE = 'A  Zvezda swing gate: twin green leaves with red stars'
SUBTITLE = ('Gate module 9.2 m  |  clear opening 7.0 m, open top  |  pillars 6.6 m  |  '
            'wall stubs = SM_Wall_14 (5 m tall, 0.2 m)  |  Pz IV for scale')
NOTES = [
    'The classic Soviet base gate (KPP): two square concrete pillars with caps, two steel leaves with a star roundel each.',
    'Leaves swing inward (dashed arcs). Each is about 3.45 x 4.4 m; nothing spans the top, so tall units always fit.',
    'Holes: pillars get the wall hole material + concrete debris (own instance). Leaves are steel plate: steel hole material + debris, one component per leaf.',
    'Reads as a gate at once from the RTS camera: pillars stand 1.6 m above the wall line, stars face the enemy.',
    'Cheapest to build: one pillar mesh + one leaf mesh, both mirrored. Leaf hinge = component pivot.',
]


def build(k):
    half = 3.5
    for sx in (-1, 1):
        x = sx * (half + 0.55)
        k.box('pillar', (1.1, 1.1, 6.0), (x, 0, 0), color='rus_concrete_dark')
        k.box('pillar_cap', (1.4, 1.4, 0.3), (x, 0, 6.0), color='rus_concrete')
        k.box('pillar_cap_top', (1.0, 1.0, 0.3), (x, 0, 6.3), color='rus_concrete', taper=0.3)
        k.cyl('lamp', 0.16, 0.35, (x, -0.7, 5.3), color='rus_white', segments=12)
        wall(k, sx * (half + 1.1), sx * (half + 1.1 + 9.0))
        # leaf: plate + frame, built in the hinge frame; the left leaf is shown ajar (40 deg inward)
        k.group((sx * half, 0, 0), yaw=40 if sx < 0 else 0)
        lx0, lx1 = 0.0, -sx * (half - 0.04)
        cx, w = (lx0 + lx1) / 2, abs(lx1 - lx0)
        k.box('leaf_plate', (w, 0.08, 4.1), (cx, 0, 0.3), color='rus_green')
        for z in (0.2, 2.35, 4.4):
            k.box('leaf_rail', (w, 0.16, 0.12), (cx, 0, z), color='steel')
        for xx in (lx0 - sx * 0.06, lx1 + sx * 0.06):
            k.box('leaf_stile', (0.12, 0.16, 4.32), (xx, 0, 0.2), color='steel')
        star(k, cx, -0.08, 2.75, 0.85, ring=(1.05, 'rus_white'))
        k.end_group()
        for z in (0.9, 3.8):
            k.cyl('hinge', 0.07, 0.35, (sx * (half - 0.02), -0.05, z), color='steel', segments=10)
        # swing travel: inward (+Y)
        a0, a1 = (0, 90) if sx < 0 else (90, 180)
        arc(k, sx * half, 0.0, w, a0, a1)
    banner(k, -9.0, -0.1, 4.7)
    k.label('Concrete pillar 6.6 m', 'gate', (-(half + 0.55), -0.6, 6.5))
    k.label('Steel leaf, star roundel', 'gate', (1.8, -0.2, 4.3))
    k.label('Leaves swing inward (left one shown ajar)', 'gate', (-1.0, 2.2, 4.4))
    k.label('Clear opening 7.0 m', 'gate', (0, -0.3, 0.2))
    scale_tank(k)
