import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'D  Schienenportal: rail-mounted portal over stock row and lane'
SUBTITLE = sp.SUB + 'site 16 x 10 m between the rail ends, girder 5.8 m, flag 8 m'
NOTES = [
    'The lane runs INSIDE the portal, parallel to the rails: in from the left between the rail ends, out to the right.',
    'Parks at x 1.25, y -1.8, heading 0 deg (one stop). Rails on ballast beds at y -4.5 / +3.7, 0.8 m clear of the truck.',
    'Crane: portal gantry travels along x on the rails, trolley runs along y on the girder, hoist. No slew.',
    'Stocks in a straight row along the back rail (radixite, metal, parts at x -4.3 / 0 / 4.3) in low steel trays.',
    'Cycle: travel to a tray, trolley back, lift, trolley forward + travel to a bed gap, lower. Readable, mechanical motion.',
    'Loads drop between the bed bows: radixite skip front, metal bundle middle, parts pallet rear.',
    'Animated: flag rise; gantry travel + trolley + hoist; tray fill levels step down. Ballast beds take uneven ground.',
    'BP points: approach (-13, -1.8), park (1.25, -1.8) +x, exit (15, -1.8). Flag pole (-8.2, 4.4), 6.0 m travel.',
    'Stop error is free: gantry travel follows the real bed along x, the trolley takes the side error.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 0.4, 3.8)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

RAILS = (-4.5, 3.7)
X0, X1 = -7.2, 7.2
GZ = 5.8
GX = 0.0
TROLLEY_Y = -1.8


def build(k):
    sp.setup()
    sp.apron(k, [(-9.0, -3.1), (9.0, -3.1), (9.0, -0.5), (-9.0, -0.5)])
    sp.arrow(k, -7.8, -1.8, 0, 'Truck in')
    sp.arrow(k, 8.0, -1.8, 0, 'Truck out')
    sp.park(k, 1.25, -1.8, 0)
    sp.truck(k, 1.25, -1.8, 0, loads={'front': 'radixite'})

    for ry in RAILS:                               # ballast bed, sleepers, rail, buffer stops
        k.box('ballast', (X1 - X0 + 0.8, 1.3, 0.22), (0, ry, 0), color='concrete_dark', taper=0.15)
        n = 20
        for i in range(n):
            k.box('sleeper', (0.25, 1.1, 0.1), (X0 + (X1 - X0) * i / (n - 1), ry, 0.22), color='timber')
        k.box('rail', (X1 - X0 + 0.4, 0.14, 0.14), (0, ry, 0.32), color='steel')
        for x in (X0 - 0.2, X1 + 0.2):
            k.box('buffer', (0.5, 0.9, 0.7), (x, ry, 0.22), color='timber')

    for ry in RAILS:                               # portal legs on bogies
        k.box('bogie', (2.4, 0.7, 0.55), (GX, ry, 0.46), color='crane')
        for dx in (-1.1, 1.1):
            k.beam((GX + dx, ry, 1.0), (GX, ry, GZ), w=0.3, color='crane', name='leg')
        k.beam((GX - 0.6, ry, 3.2), (GX + 0.6, ry, 3.2), w=0.18, color='crane', name='tie')
    k.box('girder', (0.6, RAILS[1] - RAILS[0] + 1.0, 0.65), (GX, sum(RAILS) / 2, GZ), color='crane')
    k.box('cab', (1.1, 1.0, 1.0), (GX + 0.85, RAILS[1] - 0.6, GZ - 1.05), color='crane')
    k.box('trolley', (1.0, 1.0, 0.5), (GX, TROLLEY_Y, GZ + 0.65), color='steel')
    sp.hook(k, (GX, TROLLEY_Y, GZ), 'metal', sp.BED_Z + 0.35)
    sp.dashes(k, [(X0 + 0.6, 0.0), (X1 - 0.6, 0.0)])
    k.label('Portal: travel (x) + trolley (y) + hoist', 'crane', (GX + 0.2, RAILS[0], GZ + 1.2))

    for kind, x in (('radixite', -4.3), ('metal', 0.0), ('parts', 4.3)):
        sp.steel_bin(k, (x, 1.75, 0), 2.9, 2.3)
        sp.stock(k, kind, (x, 1.75, 0.2), 2.7, 2.1)

    sp.flag(k, -8.2, 4.4)
    sp.person(k, 4.8, -4.3)
    sp.progress_bar(k, 8.6, x=0, y=1.0)
