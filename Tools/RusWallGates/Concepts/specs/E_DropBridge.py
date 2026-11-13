import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gate_common import *  # noqa: F401,F403

TITLE = 'E  Drop-leaf: a slab that falls outward and becomes a ramp'
SUBTITLE = ('Gate module 10.3 m  |  clear opening 7.1 m under a 6.4 m winch beam  |  battered pylons 7 m  |  '
            'wall stubs = SM_Wall_14  |  Pz IV for scale')
NOTES = [
    'Two battered concrete pylons carry a steel winch beam; the concrete leaf is hinged at its foot on the enemy side.',
    'Opening: the winches pay out the chains and the leaf lowers outward until it lies flat (dashed outline) as a 4.8 m ramp.',
    'Holes: pylons and leaf are flat concrete: same hole material and debris as the wall. The leaf rotates: own component.',
    'Distinct silhouette: sloped buttresses plus a beam with two drums; open, the ramp shows clearly on the ground.',
    'The lowered leaf lies on the ground (no pit): one rotation about the hinge; units drive over a 0.35 m slab.',
]


def build(k):
    half = 3.55
    for sx in (-1, 1):
        x = sx * (half + 0.7)
        # battered buttress: side profile (local X = world Y with yaw 90), 1.4 m wide
        k.prism('pylon', [(-1.6, 0), (1.6, 0), (0.55, 7.0), (-0.75, 7.0)], 1.4, (x, 0, 0), yaw=90,
                color='rus_concrete_dark')
        k.cyl('drum', 0.42, 1.0, (sx * 3.0 - 0.5, -0.45, 6.95), color='steel', rot=(0, 90, 0), segments=18)
        wall(k, sx * (half + 1.4), sx * (half + 1.4 + 9.0))
    k.box('winch_beam', (2 * half + 2.6, 0.55, 0.55), (0, -0.45, 6.4), color='rus_green')
    # the leaf, closed upright; hinge on the front foot
    k.box('leaf', (7.0, 0.35, 4.8), (0, -0.45, 0), color='rus_concrete')
    k.box('leaf_edge', (7.0, 0.45, 0.2), (0, -0.45, 4.7), color='rus_green')
    k.cyl('hinge', 0.18, 7.2, (-3.6, -0.7, 0.2), color='steel', rot=(0, 90, 0), segments=14)
    for x in (-3.0, 3.0):
        k.beam((x, -0.62, 4.85), (x, -0.62, 6.5), w=0.07, color='steel')
    star(k, 0, -0.63, 2.6, 0.95, ring=(1.2, 'rus_white'))
    # open position ghosted on the ground (leaf lies outward)
    dashed(k, (-3.5, -0.8), (-3.5, -5.6))
    dashed(k, (3.5, -0.8), (3.5, -5.6))
    dashed(k, (-3.5, -5.6), (3.5, -5.6))
    k.label('Leaf lowers outward to a ramp', 'gate', (2.2, -5.6, 0.2))
    k.label('Winch beam 6.4 m, two drums', 'gate', (-2.0, -0.45, 7.2))
    k.label('Battered pylon 7 m', 'gate', (half + 0.7, -1.4, 4.0))
    k.label('Foot hinge', 'gate', (-2.5, -0.7, 0.3))
    banner(k, -9.2, -0.1, 4.7)
    scale_tank(k, (-1.5, -11.0))
