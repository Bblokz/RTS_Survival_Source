import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'H  Kippbock: tilting A-frame in a round sandbag yard'
SUBTITLE = sp.SUB + 'ring 15.4 m across, A-frame 7 m legs, flag 8 m on the berm'
NOTES = [
    'A round sandbag berm (0.9 m) with two gates; the lane runs straight through it along +x (in left, out right).',
    'Parks at x 1.25, y -1.4, heading 0 deg (one stop) with the bed under the frame\'s forward tilt.',
    'Crane: portal A-frame (7 m legs + cross beam) hinged at y 0.7, held by back winch ropes and front stays.',
    'Motion: tilt (luff) forward over the bed / back over the stocks + trolley across the cross beam (x) + hoist. No slew.',
    'Stocks side by side behind the hinge line in steel trays: radixite, metal, parts at x -2.5 / 0 / 2.5.',
    'Loads drop front -> middle -> rear between the bed bows (the trolley picks the gap).',
    'Animated: flag rise on the berm; frame tilts back, trolley to a tray, lift, tilts forward, lower; tray levels step down.',
    'Big rocking motion reads from far away; the ring makes the point a clear "place" on the map.',
    'BP points: approach (-13, -1.4), park (1.25, -1.4) +x, exit (15, -1.4). Flag on the berm (-2.6, 6.75), 5.25 m travel.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 0.6, 3.8)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

R = 7.2
LANE_Y = -1.4
HINGE_Y = 0.7
LEG = 7.0
HALF = 3.4


def build(k):
    sp.setup()
    sp.apron(k, [(-9.0, LANE_Y - 1.4), (9.0, LANE_Y - 1.4), (9.0, LANE_Y + 1.4), (-9.0, LANE_Y + 1.4)])
    k.cyl('yard', R - 0.5, 0.015, (0, 0, 0), color='apron', segments=48, cat='mark')
    sp.arrow(k, -8.4, LANE_Y, 0, 'Truck in')
    sp.arrow(k, 8.6, LANE_Y, 0, 'Truck out')
    sp.park(k, 1.25, LANE_Y, 0)
    sp.truck(k, 1.25, LANE_Y, 0, loads={'front': 'radixite'})

    # sandbag berm with two gates on the lane
    n = 64
    for i in range(n):
        a = 2 * math.pi * (i + 0.5) / n
        x, y = R * math.cos(a), R * math.sin(a)
        if abs(y - LANE_Y) < 2.1:
            continue
        k.box('berm', (2 * math.pi * R / n * 1.08, 1.0, 0.9), (x, y, 0), yaw=math.degrees(a) + 90, color='sandbag',
              round_top=0.25, segments=2)
    for sx in (-1, 1):
        for gy in (LANE_Y - 2.0, LANE_Y + 2.0):
            gx = sx * math.sqrt(R * R - gy * gy)
            k.box('gate_post', (0.3, 0.3, 1.8), (gx, gy, 0), color='timber')

    # tilting portal frame, shown tilted forward over the bed
    theta = math.asin((HINGE_Y - LANE_Y) / LEG)
    top_y, top_z = HINGE_Y - LEG * math.sin(theta), 0.45 + LEG * math.cos(theta)
    for sx in (-1, 1):
        sp.footing(k, (sx * HALF, HINGE_Y, 0), (1.0, 1.0), 0.45)
        k.beam((sx * HALF, HINGE_Y, 0.45), (sx * HALF * 0.82, top_y, top_z), w=0.32, color='crane', name='leg')
        k.box('winch', (1.1, 1.0, 0.9), (sx * HALF, 5.4, 0), color='steel')
        k.beam((sx * HALF * 0.82, top_y, top_z), (sx * HALF, 5.4, 0.9), w=0.05, color='cable', name='tilt_rope')
        k.box('stay_anchor', (0.8, 0.8, 0.4), (sx * HALF, -5.6, 0), color='concrete')
        k.beam((sx * HALF * 0.82, top_y, top_z), (sx * HALF, -5.6, 0.4), w=0.05, color='cable', name='front_stay')
    k.beam((-HALF * 0.82 - 0.3, top_y, top_z + 0.2), (HALF * 0.82 + 0.3, top_y, top_z + 0.2), w=0.5, color='crane',
           name='cross_beam')
    k.box('trolley', (0.8, 0.7, 0.4), (0, top_y, top_z - 0.45), color='steel')
    sp.hook(k, (0, top_y, top_z - 0.45), 'metal', sp.BED_Z + 0.35)
    sp.dashes(k, [(0, LANE_Y), (0, 3.6)])
    sp.dashes(k, [(-2.7, LANE_Y - 0.1), (2.7, LANE_Y - 0.1)], w=0.08)
    k.label('Tilting A-frame: tilt + trolley + hoist', 'crane', (HALF * 0.82, top_y, top_z + 0.8))

    for kind, x in (('radixite', -2.5), ('metal', 0.0), ('parts', 2.5)):
        sp.steel_bin(k, (x, 3.6, 0), 2.3, 2.0)
        sp.stock(k, kind, (x, 3.6, 0.2), 2.1, 1.8)

    sp.flag(k, -2.6, 6.75, h=7.2, z=0.9)
    sp.person(k, 4.4, -3.6)
    sp.progress_bar(k, 9.6, x=0, y=1.0)
