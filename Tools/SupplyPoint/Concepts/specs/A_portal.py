import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'common'))
import sp_parts as sp  # noqa: E402

TITLE = 'A  Portalbruecke: fixed gantry over a drive-through lane'
SUBTITLE = sp.SUB + 'site 15 x 13 m, gantry girder 6.6 m, flag 8 m'
NOTES = [
    'Truck drives straight through along +x (in from the left, out to the right); never reverses, no turning inside.',
    'Parks at x 0.98, y 0, heading 0 deg for the middle load; it INDEXES forward ~1.0 m per load (stops x -0.02 / 0.98 / 1.99).',
    'Crane: fixed portal gantry across the lane at x -0.3. Only the trolley moves (along y over the girder) and the hoist.',
    'Loads land front -> middle -> rear of the bed, each between two canvas bows: radixite skip, metal bundle, parts pallet.',
    'Stocks under the girder line: radixite in front of the lane, metal and parts behind it, in low steel trays.',
    'Animated: flag rise; trolley out over a tray, hook down, lift, trolley back, lower into the bed; tray fill levels step down.',
    'Simplest crane (2 moving parts); the forward index stops are the price. Flag stands back-left, clear of the girder.',
    'BP points: approach (-13, 0), park (0.98, 0) +x, exit (15, 0). Flag pole (-5.6, 5.2), flag travels 6.0 m.',
    'NEEDS the truck settled to +-0.3 m and 3 stops; the contract stops it once within 1.5-2 m, so A needs a scripted settle.',
]
SCALE_REFS = False
VIEW_ZOOM = 1.7
CAMERA_TARGET = (0, 0.5, 3.4)
VIEW_OVERRIDES = {'top': dict(zoom=1.3)}

GX = -0.3          # gantry line
GZ = 6.0           # girder underside


def build(k):
    sp.setup()
    sp.apron(k, [(-7.5, -2.4), (7.5, -2.4), (7.5, 2.4), (-7.5, 2.4)])
    sp.apron(k, [(-2.0, -5.7), (1.4, -5.7), (1.4, -2.4), (-2.0, -2.4)])
    sp.apron(k, [(-2.0, 2.4), (1.4, 2.4), (1.4, 7.6), (-2.0, 7.6)])
    sp.arrow(k, -6.0, 0, 0, 'Truck in')
    sp.arrow(k, 6.6, 0, 0, 'Truck out')
    tx = GX - sp.SLOTS['mid']         # truck bottom centre with the middle bed gap under the girder
    sp.park(k, tx, 0, 0)
    sp.truck(k, tx, 0, 0, loads={'front': 'radixite'})

    # portal: two A-frame legs outside the lane, one box girder across it
    for ly in (-2.9, 2.9):
        for dx in (-1.4, 1.4):
            sp.footing(k, (GX + dx, ly, 0))
            k.beam((GX + dx, ly, 0.35), (GX, ly, GZ), w=0.3, color='crane', name='leg')
        k.beam((GX - 1.4, ly, 0.6), (GX + 1.4, ly, 0.6), w=0.22, color='crane', name='sill')
        k.beam((GX - 0.8, ly, 3.0), (GX + 0.8, ly, 3.0), w=0.18, color='crane', name='tie')
    k.box('girder', (0.55, 13.0, 0.65), (GX, 0.95, GZ), color='crane')
    k.box('walkway', (0.5, 13.0, 0.06), (GX + 0.55, 0.95, GZ + 0.55), color='steel')
    k.box('trolley', (1.0, 1.1, 0.55), (GX, 0, GZ + 0.65), color='crane')
    k.box('winch_house', (1.2, 1.4, 1.0), (GX, 6.7, GZ + 0.65), color='steel')
    sp.hook(k, (GX, 0, GZ), 'metal', sp.BED_Z + 0.35)
    sp.dashes(k, [(GX, -5.4), (GX, 7.2)])
    k.label('Fixed gantry: trolley + hoist only', 'crane', (GX - 0.4, -4.6, GZ + 1.0))

    # stocks in low steel trays under the girder line
    sp.steel_bin(k, (GX, -4.3, 0), 2.8, 2.2)
    sp.stock(k, 'radixite', (GX, -4.3, 0.2), 2.6, 2.0)
    sp.steel_bin(k, (GX, 4.1, 0), 2.8, 2.2)
    sp.stock(k, 'metal', (GX, 4.1, 0.2), 2.6, 2.0)
    sp.steel_bin(k, (GX, 6.6, 0), 2.8, 2.2)
    sp.stock(k, 'parts', (GX, 6.6, 0.2), 2.6, 2.0, fill=0.75)

    sp.flag(k, -5.6, 5.2)
    for x, y in ((4.6, 4.2), (5.2, 4.6), (4.9, 3.6)):
        sp.drum(k, x, y)
    sp.person(k, 3.0, -3.4)
    sp.progress_bar(k, 9.6, x=GX, y=1.0)
