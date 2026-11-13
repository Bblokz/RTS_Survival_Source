import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gate_common import *  # noqa: F401,F403

TITLE = 'C  Lift portal: slab hoisted between two towers'
SUBTITLE = ('Gate module 10.9 m  |  clear opening 7.5 m, 5.0 m under the raised leaf  |  towers 11 m  |  '
            'wall stubs = SM_Wall_14  |  Pz IV for scale')
NOTES = [
    'Two tall concrete towers joined by a lintel; the concrete leaf lifts straight up in steel guide channels (sluice style).',
    'Counterweights hang on the back of the towers, sheaves on top. Open, the leaf parks under the lintel at 5.0-9.8 m.',
    'Holes: towers, lintel and leaf are flat concrete: same hole material and debris as the wall. The leaf moves: own component.',
    'A landmark: at 11 m it is the tallest piece of a Soviet wall line and reads as the base entrance from anywhere.',
    'Tall casts long shadows and blocks sight lines; a lower variant would be 8.5 m towers with a 4 m leaf.',
]


def build(k):
    half = 3.75
    tw = 1.6
    for sx in (-1, 1):
        x = sx * (half + tw / 2)
        k.box('tower', (tw, 2.2, 11.0), (x, 0.3, 0), color='rus_concrete_dark')
        k.box('tower_cap', (tw + 0.3, 2.5, 0.3), (x, 0.3, 11.0), color='rus_concrete')
        k.box('guide', (0.25, 0.5, 9.6), (sx * (half - 0.05), 0.0, 0), color='rus_green')
        # counterweight on the back
        k.box('counterweight', (1.0, 0.8, 1.8), (x, 1.85, 6.4), color='rus_concrete')
        k.beam((x, 1.85, 8.2), (x, 1.85, 11.4), w=0.06, color='steel')
        k.cyl('sheave', 0.5, 0.2, (x - 0.1, 0.6, 11.9), color='steel', rot=(0, 90, 0), segments=20)
        wall(k, sx * (half + tw), sx * (half + tw + 9.0))
    k.box('lintel', (2 * half + 2 * tw + 0.3, 2.0, 1.2), (0, 0.3, 9.8), color='rus_concrete')
    star(k, 0, -0.7, 10.3, 0.55, ring=(0.68, 'rus_gold'))
    # the leaf, closed
    k.box('leaf', (7.6, 0.35, 4.8), (0, 0.0, 0.0), color='rus_concrete')
    k.box('leaf_top_beam', (7.6, 0.45, 0.25), (0, 0.0, 4.6), color='rus_green')
    for x in (-2.6, 2.6):
        k.beam((x, 0, 4.85), (x, 0, 9.8), w=0.05, color='steel')
    banner(k, 0, -0.18, 4.4, w=3.0, h=2.2)
    k.label('Concrete tower 11 m', 'gate', (half + tw / 2, -0.8, 10.0))
    k.label('Counterweight (back of tower)', 'gate', (half + tw / 2 + 0.5, 2.3, 7.2))
    k.label('Leaf lifts to 5.0 m clear', 'gate', (-2.0, -0.2, 4.9))
    k.label('Lintel, sheaves', 'gate', (-2.5, -0.7, 11.0))
    scale_tank(k)
