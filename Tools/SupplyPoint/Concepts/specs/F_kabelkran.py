import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'F  Kabelkran: cable crane from a timber head tower across the lane'
SUBTITLE = sp.SUB + 'site 16.5 x 10 m with the lane, head tower 8.2 m, tail mast 5.6 m, flag 8 m'
NOTES = [
    'A track cable runs from a timber head tower (left) to a tail mast (right), across the screen, over the stocks and the lane.',
    'The lane runs front to back (along +y) under the cable. The truck INDEXES forward 1.0 m per load (one gap per stop).',
    'Middle-load stop: x 3.4, y 3.46, heading 90 deg (stops at y 2.46 / 3.46 / 4.47 put each bed gap under the cable).',
    'Crane: carriage runs along the track cable (pulled by the winch house at the tower foot) + hoist. Nothing slews.',
    'Stocks in three steel trays strung under the cable: radixite, metal, parts (nearest the lane).',
    'Animated: flag rise; carriage runs out, hook drops, lifts, runs over the lane, lowers; tray fill levels step down.',
    'Lightest silhouette (thin lines) and very distinctive; the cable sag and long travel make the cycle slow to read.',
    'BP points: approach (3.4, -10.5), park (3.4, 3.46) +y, exit (3.4, 17.5). Flag pole (-4.6, 3.6), 6.0 m travel.',
    'NEEDS the truck settled to +-0.3 m and 3 stops (as A); with the 1.5-2 m stop error it needs a scripted settle.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 1.0, 3.4)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

HT = (-6.2, -0.4)     # head tower
TT = (6.4, 3.0)       # tail mast
H_Z, T_Z = 8.2, 5.6
LANE_X = 3.4
SAG = 0.5


def on_line(s):
    L = math.hypot(TT[0] - HT[0], TT[1] - HT[1])
    d = ((TT[0] - HT[0]) / L, (TT[1] - HT[1]) / L)
    t = s / L
    z = H_Z + (T_Z - H_Z) * t - SAG * 4 * t * (1 - t)
    return (HT[0] + d[0] * s, HT[1] + d[1] * s, z), d, L


def build(k):
    sp.setup()
    sp.apron(k, [(LANE_X - 1.4, -8.0), (LANE_X + 1.4, -8.0), (LANE_X + 1.4, 8.6), (LANE_X - 1.4, 8.6)])
    sp.arrow(k, LANE_X, -6.4, 90, 'Truck in')
    sp.arrow(k, LANE_X, 7.4, 90, 'Truck out')
    _, d, L = on_line(0)
    s_c = (LANE_X - HT[0]) / d[0]
    c, _, _ = on_line(s_c)
    ty = c[1] - sp.SLOTS['mid']
    sp.park(k, LANE_X, ty, 90)
    sp.truck(k, LANE_X, ty, 90, loads={'front': 'radixite'})
    yaw = math.degrees(math.atan2(d[1], d[0]))
    perp = (-d[1], d[0])

    # head tower (timber lattice) with winch house and backstay; tail mast A-frame with backstay
    k.box('tower_pad', (2.6, 2.6, 0.3), (HT[0], HT[1], 0), color='concrete')
    sp.lattice(k, (HT[0], HT[1], 0.3), H_Z - 0.3, 2.0, 1.0, color='timber', panels=5, member=0.16)
    k.box('saddle', (1.2, 1.2, 0.35), (HT[0], HT[1], H_Z), color='steel')
    k.box('winch_house', (1.8, 2.0, 1.6), (HT[0] + 0.4, HT[1] + 2.4, 0), color='timber', round_top=0.1, segments=2)
    back_h = (HT[0] - d[0] * 1.9, HT[1] - d[1] * 1.9)
    k.beam((HT[0], HT[1], H_Z + 0.2), (back_h[0], back_h[1], 0.3), w=0.05, color='cable', name='backstay')
    k.box('anchor', (1.2, 1.2, 0.4), (back_h[0], back_h[1], 0), color='concrete')
    for s in (-1, 1):
        foot = (TT[0] + perp[0] * 1.1 * s, TT[1] + perp[1] * 1.1 * s)
        sp.footing(k, (foot[0], foot[1], 0), (0.8, 0.8), 0.3)
        k.beam((foot[0], foot[1], 0.3), (TT[0], TT[1], T_Z + 0.2), w=0.26, color='timber', name='tail_leg')
    back = (TT[0] + d[0] * 1.8, TT[1] + d[1] * 1.8)
    k.beam((TT[0], TT[1], T_Z + 0.2), (back[0], back[1], 0.3), w=0.05, color='cable', name='backstay')
    k.box('anchor', (1.0, 1.0, 0.4), (back[0], back[1], 0), color='concrete')
    # track cable with sag, carriage over the bed gap
    pts = [on_line(L * i / 16)[0] for i in range(17)]
    for a, b in zip(pts, pts[1:]):
        k.beam(a, b, w=0.07, color='cable', name='track_cable')
    k.box('carriage', (1.0, 0.5, 0.45), (c[0], c[1], c[2] - 0.45), yaw=yaw, color='crane')
    sp.hook(k, (c[0], c[1], c[2] - 0.45), 'metal', sp.BED_Z + 0.35, yaw=90)
    sp.dashes(k, [(HT[0] + d[0] * 1.6, HT[1] + d[1] * 1.6), (TT[0] - d[0] * 1.2, TT[1] - d[1] * 1.2)])
    k.label('Cable carriage + hoist', 'crane', (c[0] - 0.4, c[1], c[2] + 0.6))
    k.label('Timber head tower + winch', 'crane', (HT[0], HT[1], H_Z + 0.8))

    for kind, s in (('radixite', 2.2), ('metal', 4.4), ('parts', 6.6)):
        p, _, _ = on_line(s)
        sp.steel_bin(k, (p[0], p[1], 0), 2.1, 2.4, yaw=yaw)
        sp.stock(k, kind, (p[0], p[1], 0.2), 1.9, 2.2, yaw=yaw)

    sp.flag(k, -4.6, 3.6)
    sp.person(k, 5.2, -2.0)
    sp.progress_bar(k, 10.2, x=0, y=1.5)
