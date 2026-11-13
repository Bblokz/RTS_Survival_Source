import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gate_common import *  # noqa: F401,F403

TITLE = 'D  Checkpoint (KPP): boom, hedgehogs and a guard booth'
SUBTITLE = ('Gap 9.0 m between wall returns  |  open top  |  booth 3.2 m  |  '
            'returns = new short slabs in the wall material  |  Pz IV for scale')
NOTES = [
    'No heavy leaf: the wall turns inward on both sides (3.5 m returns) into a throat, closed by a striped boom and hedgehogs.',
    'Closed (shown): boom down, three hedgehogs dragged across the throat. Open: boom up, hedgehogs pushed to the sides.',
    'Holes: returns (new 3.5 m slab, not a scaled SM_Wall_14) and booth are flat concrete: wall hole material + debris. Boom/hedgehogs just break.',
    'Most lived-in: sentry booth with searchlight and sandbags in front (could garrison 1-2 riflemen later).',
    'Weakest physical barrier of the five; reads as an entrance rather than a fortress gate.',
]


def hedgehog(k, x, y, s=1.0):
    for rot in ((0, 45, 0), (45, 0, 90), (0, -45, 90)):
        k.box('hedgehog', (2.2 * s, 0.22, 0.22), (x, y, 0.85 * s), color='steel', rot=rot, origin=(0, 0, 0))


def build(k):
    half = 4.5
    for sx in (-1, 1):
        wall(k, sx * half, sx * (half + 10.0))
        wall_y(k, sx * (half - 0.1), 0.1, 3.6)
    # boom barrier across the front of the throat
    k.box('boom_post', (0.5, 0.5, 1.2), (-half - 0.6, -1.4, 0), color='rus_white')
    k.box('boom_counterweight', (0.9, 0.35, 0.5), (-half - 1.3, -1.4, 0.8), color='rus_concrete_dark')
    k.box('boom_rest', (0.25, 0.25, 1.0), (half + 0.4, -1.4, 0), color='rus_white')
    n = 14
    seg = (2 * half + 1.0) / n
    for i in range(n):
        x = -half - 0.6 + seg * (i + 0.5)
        k.box('boom', (seg, 0.16, 0.16), (x, -1.4, 1.05), color='rus_red' if i % 2 == 0 else 'rus_white')
    # hedgehogs closing the throat
    for x in (-3.0, 0.0, 3.0):
        hedgehog(k, x, 1.8)
    # guard booth, forward right
    bx, by = 7.2, -3.0
    k.box('booth', (2.4, 2.4, 2.9), (bx, by, 0), color='rus_concrete')
    k.box('booth_roof', (3.1, 3.1, 0.3), (bx, by, 2.9), color='rus_concrete_dark')
    k.box('booth_window', (1.8, 0.06, 0.45), (bx, by - 1.22, 1.6), color='slot')
    k.box('booth_window_side', (0.06, 1.6, 0.45), (bx - 1.22, by, 1.6), color='slot')
    k.cyl('searchlight', 0.32, 0.45, (bx + 0.6, by + 0.4, 3.2), color='steel', rot=(80, 0, 0), segments=16)
    k.box('sandbags', (3.6, 0.8, 1.0), (bx, by - 2.0, 0), color='sandbag', round_top=0.2, segments=2)
    k.box('sandbags_side', (0.8, 2.2, 1.0), (bx - 2.2, by - 1.2, 0), color='sandbag', round_top=0.2, segments=2)
    k.soldier((bx + 1.6, by - 1.6, 0))
    # lamp post at the throat
    k.cyl('lamp_post', 0.08, 6.0, (-half - 1.0, -0.6, 0), color='rus_green', segments=8)
    k.beam((-half - 1.0, -0.6, 6.0), (-half + 0.2, -0.6, 6.0), w=0.08, color='rus_green')
    k.cyl('lamp_head', 0.25, 0.25, (-half + 0.2, -0.6, 5.7), color='rus_white', segments=12)
    banner(k, 9.5, -0.1, 4.7)
    k.label('Wall returns 3.5 m', 'gate', (half - 0.1, 3.4, 5.0))
    k.label('Striped boom', 'gate', (-1.0, -1.4, 1.2))
    k.label('Hedgehogs close the throat', 'gate', (0, 1.8, 1.7))
    k.label('Guard booth + searchlight', 'gate', (bx, by, 3.4))
    scale_tank(k, (-1.5, -10.0))
