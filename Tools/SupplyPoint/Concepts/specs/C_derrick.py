import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'C  Mastenkran: stiff-leg derrick behind a side apron'
SUBTITLE = sp.SUB + 'site 16 x 13 m, mast 8.6 m, flag 8 m'
NOTES = [
    'Side apron along the front: the truck drives straight along +x (in left, out right), the depot stays behind it.',
    'Parks at x 1.25, y -3.6, heading 0 deg (one stop for all three loads).',
    'Crane: stiff-leg derrick, lattice mast at x 0, y 2.4, braced by two timber stiff legs to ballasted cribs at the back.',
    'Motion: slew about 230 deg (left bay -> bed -> right bay, never between the legs) + luff (radius 4.6-6.0 m) + hoist.',
    'Stocks in three sleeper bays round the front of the mast: radixite left, metal front-left, parts right.',
    'Loads drop front -> middle -> rear between the bed bows: radixite skip, metal bundle, parts pallet.',
    'Animated: flag rise; slew + luff out to a bay, lift, slew over the bed, lower; bay fill levels step down.',
    'Old-fashioned field depot look, wide but low mass; the long boom swing is the show piece.',
    'BP points: approach (-13, -3.6), park (1.25, -3.6) +x, exit (15, -3.6). Flag pole (6.2, 6.0), 6.0 m travel. Bar 10.5 m.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 0.8, 3.4)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

M = (0.0, 2.4)
MAST_TOP = 8.6
FOOT_Z = 1.2
BOOM = 7.4
LEG_FEET = ((-3.2, 6.6), (3.2, 6.6))


def build(k):
    sp.setup()
    sp.apron(k, [(-8.0, -5.0), (8.0, -5.0), (8.0, -2.2), (-8.0, -2.2)])
    sp.arrow(k, -6.4, -3.6, 0, 'Truck in')
    sp.arrow(k, 6.6, -3.6, 0, 'Truck out')
    sp.park(k, 1.25, -3.6, 0)
    slots = sp.truck(k, 1.25, -3.6, 0, loads={'front': 'radixite'})

    # stiff-leg derrick: pivot block, lattice mast, two stiff legs + sills to ballasted cribs
    sp.footing(k, (M[0], M[1], 0), (1.6, 1.6), 0.4)
    k.cyl('bull_wheel', 0.9, 0.25, (M[0], M[1], 0.4), color='crane', segments=20)
    sp.lattice(k, (M[0], M[1], 0.65), MAST_TOP - 0.65, 0.75, 0.6, panels=6, member=0.1)
    k.cyl('mast_cap', 0.35, 0.3, (M[0], M[1], MAST_TOP), color='steel', segments=12)
    for fx, fy in LEG_FEET:
        k.box('ballast_crib', (1.8, 1.6, 0.9), (fx, fy, 0), color='timber')
        k.box('ballast', (1.4, 1.2, 0.5), (fx, fy, 0.9), color='concrete_dark')
        k.beam((M[0], M[1], MAST_TOP), (fx, fy, 1.2), w=0.32, color='timber', name='stiff_leg')
        k.beam((M[0], M[1], 0.5), (fx, fy, 0.5), w=0.28, color='timber', name='sill')
    target = slots['mid']
    tip_r = math.hypot(target[0] - M[0], target[1] - M[1])
    elev = math.acos(tip_r / BOOM)
    tip = (target[0], target[1], FOOT_Z + BOOM * math.sin(elev))
    for dx in (-0.25, 0.25):
        k.beam((M[0] + dx * 2, M[1] - 0.3, FOOT_Z), (tip[0] + dx * 0.4, tip[1], tip[2]), w=0.2, color='crane',
               name='boom')
    k.beam((M[0], M[1], MAST_TOP - 0.2), tip, w=0.05, color='cable', name='topping_lift')
    k.box('winch', (1.4, 1.1, 1.0), (1.9, 4.2, 0), color='steel')
    sp.hook(k, tip, 'metal', sp.BED_Z + 0.35)
    sp.dashes(k, sp.arc(M, 4.8, 128, 360, 40))   # working sector: left bay -> bed -> right bay
    k.label('Stiff-leg derrick: slew + luff + hoist', 'crane', (tip[0] - 0.6, tip[1], tip[2] + 0.8))

    for kind, (x, y), yaw in (('radixite', (-4.7, 2.4), 90), ('metal', (-3.8, -0.2), 124), ('parts', (4.7, 2.4), -90)):
        sp.timber_bin(k, (x, y, 0), 2.8, 2.2, yaw=yaw, h=0.8)
        sp.stock(k, kind, (x, y, 0.12), 2.6, 2.0, yaw=yaw)

    sp.flag(k, 6.2, 6.0)
    sp.person(k, 4.6, -5.6)
    sp.progress_bar(k, 10.4, x=0, y=2.0)
