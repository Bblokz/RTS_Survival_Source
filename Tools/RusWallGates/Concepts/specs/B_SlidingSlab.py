import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gate_common import *  # noqa: F401,F403

TITLE = 'B  Sliding slab: a wall panel that rolls aside'
SUBTITLE = ('Gate module 9.1 m + 9 m parking run  |  clear opening 7.5 m, 5.2 m under the top guide  |  '
            'wall stubs = SM_Wall_14  |  Pz IV for scale')
NOTES = [
    'The leaf is a piece of the wall itself (same concrete, 0.3 m), hung from a steel top guide, running on a sill rail.',
    'Opening: it rolls left behind the wall (dashed run). Closed, the wall reads as nearly unbroken apart from the green frame.',
    'Holes: best fit of the five. The leaf is a flat concrete slab: same hole material and debris as the wall (own instance per mesh).',
    'The 5.2 m steel guide and its posts mark the gate from above; the guide clears an E-100 (3.9 m) with margin.',
    'Moving part = one straight slide along the wall, so collision/nav only change inside one box.',
]


def build(k):
    half = 3.75
    ly = 0.45  # leaf runs just inside the wall line
    for sx in (-1, 1):
        k.box('jamb', (0.8, 0.8, 5.4), (sx * (half + 0.4), 0, 0), color='rus_concrete_dark')
        wall(k, sx * (half + 0.8), sx * (half + 0.8 + 9.0))
    # sill rail (at grade) from the closing jamb to the end of the parking run
    k.box('sill', (17.2, 0.6, 0.12), (-4.4, ly, 0), color='rus_concrete_dark')
    k.box('rail', (17.2, 0.12, 0.08), (-4.4, ly, 0.12), color='steel')
    # top guide on posts (inside the wall)
    for x in (half + 0.4, -(half + 0.4), -8.6, -12.6):
        k.box('guide_post', (0.3, 0.3, 5.4), (x, ly + 0.55, 0), color='rus_green')
        k.beam((x, ly + 0.55, 5.25), (x, ly, 5.25), w=0.22, color='rus_green')
    k.box('top_guide', (17.6, 0.3, 0.35), (-4.4, ly, 5.2), color='rus_green')
    # the leaf: concrete slab in a steel frame on a wheeled bottom channel, shown half rolled back
    o = -2.4
    k.box('leaf', (7.9, 0.3, 4.55), (o, ly, 0.45), color='rus_concrete')
    k.box('leaf_channel', (7.9, 0.36, 0.25), (o, ly, 0.2), color='rus_green')
    for x in (-3.0, 0, 3.0):
        k.cyl('wheel', 0.22, 0.12, (o + x, ly - 0.2, 0.25), color='steel', rot=(90, 0, 0), segments=14)
    k.box('leaf_top_frame', (7.9, 0.36, 0.15), (o, ly, 4.95), color='rus_green')
    star(k, o, ly - 0.15, 2.9, 1.0, ring=(1.25, 'rus_white'))
    arrow(k, (1.6, -1.6), 180, 'Leaf rolls left behind the wall (shown part open)')
    k.box('catch', (0.25, 0.5, 1.0), (half + 0.05, ly, 2.0), color='steel')
    # travel: rolls behind the left wall stub
    dashed(k, (-half - 0.8, ly + 1.4), (-half - 8.6, ly + 1.4))
    banner(k, 9.2, -0.1, 4.7)
    k.label('Concrete leaf 7.9 x 4.55 m (wall material)', 'gate', (-1.0, ly - 0.15, 4.2))
    k.label('Steel top guide 5.2 m', 'gate', (-10.5, ly, 5.5))
    k.label('Sill rail, at grade', 'gate', (-6.5, ly, 0.15))
    scale_tank(k)
