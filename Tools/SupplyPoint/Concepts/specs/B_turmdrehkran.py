import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'B  Turmdrehkran: tower crane over a fan of three bays'
SUBTITLE = sp.SUB + 'site 15 x 14 m, tower head 8.4 m, flag 8 m'
NOTES = [
    'Shallow V pull-through in front: in from the back-left, straight past the crane, out to the back-right. No reversing.',
    'Truck parks at x 1.25, y -3.3, heading 0 deg (one stop for all three loads).',
    'Crane: hammerhead tower crane at x 0, y 2.2. Slew (360 deg) + trolley along the jib (2.5-6.6 m) + hoist.',
    'All three bed gaps are 5.5-5.6 m from the tower: slew picks the slot, the trolley stays nearly at one radius.',
    'Stocks in a fan of three timber bays opening toward the tower: radixite left, metal behind, parts right.',
    'Animated: flag rise; slew to a bay, trolley in, hook down, lift, slew to the bed, lower; bay fill levels step down.',
    'Most readable "crane" silhouette; it is also the tallest (8.4 m head) under the progress bar.',
    'BP points: approach (-11, 1.5), park (1.25, -3.3) +x, exit (13.5, 1.5). Flag pole (-5.6, 7.4), 6.0 m travel.',
    'Aims at the real bed: slew + trolley absorb a 2 m stop error if the jib grows to about 7.5 m. Bar offset 10.5 m.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 1.0, 3.4)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

T = (0.0, 2.2)      # tower axis
TOP = 6.6           # top of the tower lattice
JIB_Z = 6.9


def build(k):
    sp.setup()
    sp.apron(k, [(-4.2, -4.6), (4.2, -4.6), (8.4, -1.4), (8.4, 1.2), (4.2, -2.0), (-4.2, -2.0), (-8.4, 1.2),
                 (-8.4, -1.4)])
    sp.arrow(k, -6.6, -1.0, -35, 'Truck in')
    sp.arrow(k, 6.6, -1.0, 35, 'Truck out')
    sp.park(k, 1.25, -3.3, 0)
    sp.truck(k, 1.25, -3.3, 0, loads={'front': 'radixite'})

    # tower on a cross footing with ballast
    k.box('footing', (3.4, 3.4, 0.5), (T[0], T[1], 0), color='concrete', round_top=0.08, segments=2)
    for dx, dy in ((-1.1, -1.1), (1.1, -1.1), (-1.1, 1.1), (1.1, 1.1)):
        k.box('ballast', (0.9, 0.9, 0.5), (T[0] + dx, T[1] + dy, 0.5), color='concrete_dark')
    sp.lattice(k, (T[0], T[1], 0.5), TOP - 0.5, 1.5, 1.3, panels=5)
    k.cyl('slew_ring', 0.95, 0.3, (T[0], T[1], TOP), color='steel', segments=20)
    k.box('cab', (1.0, 1.1, 1.0), (T[0] + 1.0, T[1] - 0.2, TOP + 0.05), color='crane')
    # jib toward the bed (-y), counter-jib behind with counterweights, tower head with ties
    k.box('jib', (0.55, 7.4, 0.6), (T[0], T[1] - 3.3, JIB_Z), color='crane')
    k.box('counter_jib', (0.6, 3.2, 0.45), (T[0], T[1] + 1.9, JIB_Z), color='crane')
    k.box('counterweight', (1.1, 0.9, 1.1), (T[0], T[1] + 3.1, JIB_Z - 0.6), color='concrete_dark')
    k.beam((T[0], T[1], JIB_Z + 0.5), (T[0], T[1], 8.4), w=0.3, color='crane', name='tower_head')
    k.beam((T[0], T[1], 8.4), (T[0], T[1] - 6.9, JIB_Z + 0.6), w=0.05, color='cable', name='tie')
    k.beam((T[0], T[1], 8.4), (T[0], T[1] + 3.3, JIB_Z + 0.45), w=0.05, color='cable', name='tie')
    k.box('trolley', (0.8, 0.9, 0.4), (T[0], -3.3, JIB_Z - 0.35), color='steel')
    sp.hook(k, (T[0], -3.3, JIB_Z - 0.35), 'metal', sp.BED_Z + 0.35)
    sp.dashes(k, sp.arc(T, 5.5, -160, 200, 48))
    k.label('Tower crane: slew + trolley + hoist', 'crane', (T[0] - 0.5, T[1] - 5.0, JIB_Z + 1.2))

    # fan of three timber bays opening toward the tower
    for kind, ang in (('radixite', 160), ('metal', 90), ('parts', 20)):
        cx, cy = sp.rot(T, (4.5, 0), ang)
        sp.timber_bin(k, (cx, cy, 0), 2.8, 2.2, yaw=ang - 90, h=0.8)
        sp.stock(k, kind, (cx, cy, 0.12), 2.6, 2.0, yaw=ang - 90)

    sp.flag(k, -5.6, 7.4)   # outside the 7.0 m jib circle
    sp.crates(k, 5.6, 6.4, 20)
    sp.person(k, 5.6, -5.4)
    sp.progress_bar(k, 10.0, x=0, y=1.5)
