import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'G  Laderampe: loading dock at bed height with a pillar jib crane'
SUBTITLE = sp.SUB + 'dock 12.4 x 4.2 m at 1.2 m, site 17 x 7.5 m with the lane, jib 6 m, flag 8 m'
NOTES = [
    'A sleeper-crib loading dock at bed height (1.2 m) along the back; stocks stand ON the dock in three bays.',
    'Truck runs along the dock face (+x, in left, out right), parks at x 1.25, y -1.1, heading 0 deg (one stop, 0.45 m clear).',
    'Crane: pillar jib crane on the dock edge at x 0, y 1.3. Slew (about 270 deg) + trolley along the 5.2 m jib + hoist.',
    'Short, flat lifts: bays and bed are at the same height, so the hook barely drops; reach 2.5-4.7 m.',
    'Shown: first lift of a cycle, radixite skip coming out of its bay; the bed is still empty.',
    'Animated: flag rise on the dock corner; jib slew + trolley + hoist; bay fill levels step down.',
    'Most compact (lowest crane, 7 m), and the dock shows "loading point" at a glance; dock faces need flat-ish ground.',
    'BP points: approach (-13, -1.1), park (1.25, -1.1) +x, exit (15, -1.1). Flag on the dock (-5.75, 1.35), 4.85 m travel.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 1.0, 2.6)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

DOCK = (-6.2, 6.2, 0.9, 5.1)   # x0, x1, y0, y1
DZ = 1.2
PIL = (0.0, 1.35)
JIB_Z = 6.2


def build(k):
    sp.setup()
    x0, x1, y0, y1 = DOCK
    sp.apron(k, [(-8.6, -2.7), (8.6, -2.7), (8.6, y0), (-8.6, y0)])
    sp.arrow(k, -7.0, -1.1, 0, 'Truck in')
    sp.arrow(k, 7.2, -1.1, 0, 'Truck out')
    sp.park(k, 1.25, -1.1, 0)
    sp.truck(k, 1.25, -1.1, 0)

    # dock: sleeper crib faces, plank deck, rubbing beam, steps at the right end
    k.box('dock', (x1 - x0, y1 - y0, DZ - 0.1), ((x0 + x1) / 2, (y0 + y1) / 2, 0), color='timber')
    k.box('deck', (x1 - x0 + 0.2, y1 - y0 + 0.2, 0.1), ((x0 + x1) / 2, (y0 + y1) / 2, DZ - 0.1), color='wood')
    for i in range(4):
        k.box('crib_line', (x1 - x0 + 0.05, 0.04, 0.03), ((x0 + x1) / 2, y0 - 0.02, 0.22 + i * 0.25), color='cable')
    k.box('rub_beam', (x1 - x0, 0.25, 0.25), ((x0 + x1) / 2, y0 - 0.12, DZ - 0.45), color='timber')
    for i in range(4):
        k.box('step', (0.4, 1.4, DZ - i * 0.3), (x1 + 0.2 + i * 0.4, y1 - 1.0, 0), color='wood')

    # pillar jib crane, slewed over the radixite bay
    k.box('pillar_base', (1.2, 1.2, 0.3), (PIL[0], PIL[1], DZ), color='concrete')
    k.cyl('pillar', 0.32, JIB_Z + 0.9 - DZ, (PIL[0], PIL[1], DZ + 0.3), color='crane', segments=16)
    target = (-4.1, 3.5)
    yaw = math.degrees(math.atan2(target[1] - PIL[1], target[0] - PIL[0]))
    r = math.hypot(target[0] - PIL[0], target[1] - PIL[1])
    end = sp.rot(PIL, (5.2, 0), yaw)
    k.beam((PIL[0], PIL[1], JIB_Z), (end[0], end[1], JIB_Z), w=0.45, color='crane', name='jib')
    k.beam((PIL[0], PIL[1], JIB_Z + 1.2), (end[0], end[1], JIB_Z + 0.2), w=0.08, color='crane', name='tie_rod')
    k.box('motor', (0.7, 0.7, 0.6), (sp.rot(PIL, (-0.7, 0), yaw) + (JIB_Z - 0.3,)), yaw=yaw, color='steel')
    tr = sp.rot(PIL, (r, 0), yaw)
    k.box('trolley', (0.7, 0.6, 0.35), (tr[0], tr[1], JIB_Z - 0.55), yaw=yaw, color='steel')
    sp.hook(k, (tr[0], tr[1], JIB_Z - 0.55), 'radixite', DZ + 1.55)
    sp.dashes(k, sp.arc(PIL, 2.1, -150, 120, 36), z=DZ + 0.12)
    k.label('Pillar jib: slew + trolley + hoist', 'crane', (end[0], end[1], JIB_Z + 0.8))

    for kind, x, fill in (('radixite', -4.1, 0.9), ('metal', 0.0, 1.0), ('parts', 4.1, 1.0)):
        sp.timber_bin(k, (x, 3.5, DZ), 3.2, 2.6, h=0.7, floor=False)
        sp.stock(k, kind, (x, 3.45, DZ), 3.0, 2.4, fill=fill)

    sp.flag(k, x0 + 0.45, y0 + 0.45, h=6.8, z=DZ)
    sp.person(k, 4.4, -2.9)
    sp.progress_bar(k, 9.2, x=0, y=2.0)
