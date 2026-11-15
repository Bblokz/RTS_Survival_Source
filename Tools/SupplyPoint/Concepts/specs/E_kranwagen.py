import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'E  Kranwagen: parked crane lorry beside a front-to-back lane'
SUBTITLE = sp.SUB + 'site 12 x 15 m, boom tip 6.1 m, flag 8 m'
NOTES = [
    'Lane runs front to back (along +y): the truck comes up from the camera side and leaves at the back. No reversing.',
    'Parks at x 1.8, y 0.85, heading 90 deg (one stop). Lane apron x 0.4-3.2, clear of the outrigger pads.',
    'Crane: an immobile crane lorry jacked on four outriggers, slewing crane on its rear deck (pivot x -2.6, y -1.6).',
    'Motion: slew + luff (boom 5.5 m) + hoist. The lorry itself never moves: an explained, neutral piece of map dressing.',
    'Stocks in a horseshoe of low sleeper cribs round the lorry tail: radixite left, metal front-left, parts front.',
    'Loads drop front -> middle -> rear between the bed bows (bed slots run along y here).',
    'Animated: flag rise; slew + luff to a crib, lift, slew over the bed, lower; crib fill levels step down; exhaust puff.',
    'Asymmetric and characterful; the stocks sit on the camera side, so they read best of all eight.',
    'BP points: approach (1.8, -13), park (1.8, 0.85) +y, exit (1.8, 15). Flag pole (-5.9, 2.9), 6.0 m travel.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (-1.0, 0.0, 2.6)
VIEW_OVERRIDES = {'top': dict(zoom=1.3, target=(-1.0, 0.0, 0))}

LX = 1.8            # lane centre x
CX = -2.6           # crane lorry centre line
P = (CX, -1.6)      # slewing pivot
DECK = 1.45
BOOM = 5.5


def build(k):
    sp.setup()
    sp.apron(k, [(LX - 1.4, -8.0), (LX + 1.4, -8.0), (LX + 1.4, 8.0), (LX - 1.4, 8.0)])
    sp.arrow(k, LX, -6.6, 90, 'Truck in')
    sp.arrow(k, LX, 6.8, 90, 'Truck out')
    sp.park(k, LX, 0.85, 90)
    slots = sp.truck(k, LX, 0.85, 90, loads={'front': 'radixite'})

    # crane lorry, heading +y, on outriggers
    k.box('chassis', (2.3, 7.0, 0.9), (CX, 0.0, 0.55), color='lorry')
    for wy in (-2.6, -1.3, 2.4):
        for sx in (-1, 1):
            k.cyl('wheel', 0.5, 0.35, (CX + sx * 1.15, wy, 0.5), color='cable', rot=(0, 90 * sx, 0), segments=14)
    k.box('cab', (2.2, 1.7, 1.6), (CX, 2.3, 1.0), color='lorry', round_top=0.2, segments=2)
    k.box('bonnet', (1.6, 1.1, 1.0), (CX, 3.6, 0.9), color='lorry', round_top=0.25, segments=2)
    for oy in (-2.9, 1.0):
        for sx in (-1, 1):
            k.beam((CX, oy, 0.8), (CX + sx * 2.4, oy, 0.5), w=0.22, color='crane', name='outrigger')
            k.box('pad', (0.8, 0.8, 0.12), (CX + sx * 2.4, oy, 0), color='timber')
    target = slots['mid']
    dx, dy = target[0] - P[0], target[1] - P[1]
    yaw = math.degrees(math.atan2(dy, dx))
    k.cyl('turntable', 1.0, 0.3, (P[0], P[1], DECK), color='crane', segments=20)
    k.box('house', (2.0, 1.8, 1.3), (P[0] - 0.2 * dx / math.hypot(dx, dy), P[1] - 0.2 * dy / math.hypot(dx, dy),
                                      DECK + 0.3), yaw=yaw, color='crane', round_top=0.15, segments=2)
    foot = sp.rot(P, (0.6, 0), yaw)
    reach = math.hypot(dx, dy) - 0.6
    elev = math.acos(reach / BOOM)
    tip = (target[0], target[1], DECK + 0.9 + BOOM * math.sin(elev))
    for side in (-0.3, 0.3):
        a = sp.rot(foot, (0, side), yaw)
        b = sp.rot((tip[0], tip[1]), (0, side * 0.3), yaw)
        k.beam((a[0], a[1], DECK + 0.9), (b[0], b[1], tip[2]), w=0.22, color='crane', name='boom')
    k.beam((P[0], P[1], DECK + 1.6), tip, w=0.05, color='cable', name='luff_rope')
    sp.hook(k, tip, 'metal', sp.BED_Z + 0.35, yaw=90)
    sp.dashes(k, sp.arc(P, 4.6, 190, 375, 40))
    k.label('Crane lorry: slew + luff + hoist', 'crane', (P[0] - 0.4, P[1] + 0.6, tip[2] + 0.8))

    for kind, (x, y), yy in (('radixite', (-6.4, -2.5), 90), ('metal', (-5.0, -5.8), 30), ('parts', (-1.6, -6.1), 0)):
        sp.timber_bin(k, (x, y, 0), 2.6, 2.2, yaw=yy, h=0.55, open_side='none')
        sp.stock(k, kind, (x, y, 0.12), 2.4, 2.0, yaw=yy)

    sp.flag(k, -5.9, 2.9)
    sp.person(k, 4.2, -3.0)
    sp.progress_bar(k, 9.2, x=-1.0, y=0.0)
