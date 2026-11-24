"""Points of interest for the empty flanks of Ger_IntroMission (offline planner; poi_apply.py spawns the result).

Screen-left (-Y, "west") : POIW  abandoned drought farmstead ("Hof Brandt") on the flat between two track stripes,
                           centre (-20700, -18700), yard axis along the stripes (yaw 112).
Screen-right (+Y, "east"): POIE  dried-out quarry swimming lake ("Baggersee") in the 19 m deep pit at (-7900, 13200):
                           day-trippers' rim (hut, abandoned cars, tables, signs), a jetty stranded on the dry floor,
                           rowboats left on the exposed slope, dead willows on the old shoreline, dry reeds at the
                           shrunken pond.

    python poi_plan.py --round 1   -> Saved/IntroFoliage/poi/poi_r1.json (+ poi_r1.png) and prints every keep-out hit
Every item carries its footprint (half extents in its own frame); the checker tests the rotated footprint against the
same keep-outs as fol_plan (roads, mission routes, track stripes, water, slope, other actors) and refuses violations.
"""
import argparse
import json
import math

import cv2
import numpy as np

import fol_plan as fp

OUT = fp.ROOT + 'poi/'
ENV = '/Game/RTS_Survival/Blueprints/Environment/'
FARM = ENV + 'DestructableEnvActor/Farm/'
CROPS = ENV + 'CrushableInstancedActor/FarmCrops/'
SCRUB = ENV + 'CrushableInstancedActor/DryScrub/'
GF = ENV + 'GroundFoliage/'
DEADTREE = ENV + 'DestructableEnvActor/BP_Scorch_Dest_DeadTree'
WILLOW = ENV + 'DestructableEnvActor/BP_Scorch_Dest_DeadWillow'
OLDCAR = ENV + 'Props/OldCars/BP_OldCar'
CARS = ENV + 'Props/OldCars/Meshes/SM_OldCar_'

# keep-outs every POI item must respect (metres); 'pit' items may stand on slopes up to 'slope'
RULES = {'road': 6.0, 'route': 9.0, 'stripe': 5.0, 'water': 1.0, 'foot': 1.0, 'slope': 14.0, 'resource': 9.0}


def frame(cx, cy, yaw_deg):
    a = math.radians(yaw_deg)
    u = (math.cos(a), math.sin(a))
    v = (-math.sin(a), math.cos(a))

    def w(du, dv):
        return cx + du * u[0] + dv * v[0], cy + du * u[1] + dv * v[1]
    return w


def item(kind, path, x, y, yaw, half=(100, 100), label='', **extra):
    d = {'kind': kind, 'path': path, 'x': round(x), 'y': round(y), 'yaw': round(yaw % 360, 1), 'half': list(half),
         'label': label}
    d.update(extra)
    return d


def west_farm():
    C = (-19400.0, -18900.0)        # middle of the 45 m band between the two track stripes
    A = 109.0                       # along the stripes
    w = frame(C[0], C[1], A)
    it = []
    # buildings round a yard (open towards the field side)
    it.append(item('bp', FARM + 'BP_Farm_Dest_Barn', *w(-1050, 700), A, (900, 500), 'Barn'))
    it.append(item('bp', FARM + 'BP_Farm_Dest_FarmHouse', *w(950, 750), A + 90, (550, 400), 'FarmHouse'))
    it.append(item('bp', FARM + 'BP_Farm_Dest_HayShed', -18500, -20100, A, (500, 300), 'HayShed'))
    # yard clutter: hay, cart, abandoned car, buckets
    it.append(item('mesh', CROPS + 'Meshes/SM_HayBale_Round_Stack', -18850, -18800, A + 8, (150, 70), 'HayStack'))
    it.append(item('mesh', CROPS + 'Meshes/SM_HayBale_Round_A', *w(150, -800), A + 63, (75, 62), 'Bale1'))
    it.append(item('mesh', CROPS + 'Meshes/SM_HayBale_Round_B', -19150, -18700, A + 141, (75, 62), 'Bale2'))
    # review r2: was tucked under the barn eave
    it.append(item('mesh', CROPS + 'Meshes/SM_HayStack_A', -19790, -21560, 139, (175, 175), 'LooseStack'))
    it.append(item('mesh', '/Game/Greek_island/Assets/Cart/SM_Cart', -18950, -19350, 110, (230, 90), 'HayCart', scale=0.9))
    it.append(item('oldcar', OLDCAR, -19250, -18300, 20, (260, 110), 'FarmCar', mesh=CARS + 'PanelVan', wear=0.85))
    # failed harvest: a burnt wheat field (wildfire) and a parched one beside it, both crushable
    # burnt strip outside the yard fence (+v, away from the radixite crystals at the farm's -v/+u corner)
    # review r2: no crisp rectangle; ragged outline, plus a scorch fringe (burnt stubble + ash) bleeding past it
    rnd = np.random.default_rng(31)
    burnt = []
    for k in range(16):
        t = 2 * math.pi * k / 16
        du = 1150 * math.cos(t) * rnd.uniform(0.82, 1.08)
        dv = 2500 + 560 * math.sin(t) * rnd.uniform(0.8, 1.12)
        burnt.append(w(du - 200, dv))
    it.append(item('field', CROPS + 'BP_CropField_WheatBurned', *w(-200, 2500), A, (1100, 550), 'FieldBurnt',
                   outline=burnt, seed=3171))
    it.append(scorch_fringe(burnt, rnd))
    # review r3: the parched wheat read bright gold (user: yellow does not fit). Now a fallow field gone to dead
    # thistle (concept D), ragged edge
    rnd_f = np.random.default_rng(44)
    fallow = []
    for k in range(14):
        t = 2 * math.pi * k / 14
        fallow.append(w(-3300 + 1000 * math.cos(t) * rnd_f.uniform(0.85, 1.08),
                        950 * math.sin(t) * rnd_f.uniform(0.85, 1.08)))
    it.append(item('field', SCRUB + 'BP_CrushableScrub_Thistle', *w(-3300, 0), A, (1000, 950), 'FieldFallow',
                   outline=fallow, seed=4471, layout={'spacing': 165, 'smin': 1.3, 'smax': 1.8, 'jitter': 0.95,
                                                      'missing': 0.4}))   # review r4: read as an even weed bed
    # orchard east of the house (review r1: real fruit trees, not four scorched oaks); two dead ones among them
    orchard = ENV + 'OrchardTrees/BP_OrchardTree_'
    rows = (((1850, -500), 'Apple_A'), ((1850, 300), 'Pear_B'), ((1850, 1100), 'DEAD'), ((2350, -100), 'Cherry_A'),
            ((2350, 700), 'Apple_B'), ((2350, 1500), 'DEAD'), ((1850, 1900), 'Pear_A'))
    for k, ((du, dv), kind) in enumerate(rows):
        if kind == 'DEAD':      # review r2: full-size scorched oaks smothered the orchard; orchard-sized dead trees
            # review r3: Orchard2's crown overlapped the farmhouse by 2.1 m at 0.55
            # review r4: the BP picks one of three ~10 m dead-tree meshes per spawn (8-10 m crowns); 0.35-0.4 keeps
            # any of them inside the 8 m orchard spacing (Orchard5 at 0.55 overhung Orchard6)
            it.append(item('bp', DEADTREE, *w(du, dv), A + 37 * k, (220, 220), 'Orchard%d' % k,
                           scale=0.35 if k == 2 else 0.4))
        elif k == 3:            # review r3: crown 7.4 m from a radixite crystal; reviewer's spot
            it.append(item('bp', orchard + kind, -20200, -16800, A + 37 * k, (220, 220), 'Orchard%d' % k,
                           material=SCRUB + 'MI_DryScrub_OrchardDrought'))
        else:                   # review r2: lush green orchard; drought-yellowed canopy
            it.append(item('bp', orchard + kind, *w(du, dv), A + 37 * k, (220, 220), 'Orchard%d' % k,
                           material=SCRUB + 'MI_DryScrub_OrchardDrought'))
    # farm fence: broken run along the yard's road-facing side (gap = gate)
    fence = [w(-2000, 1500), w(-500, 1550), w(400, 1550)]
    fence2 = [w(1350, 1650), w(1450, 1000)]       # review r2: ran through the orchard crowns
    it.append(item('fence', CROPS + 'BP_DestSpline_FarmFence', *w(-800, 1530), A, (900, 60), 'FenceA', pts=fence))
    it.append(item('fence', CROPS + 'BP_DestSpline_FarmFence', *w(1400, 1320), A, (330, 60), 'FenceB', pts=fence2))
    # review r4: thistle (concept D) also at the fences, behind the barn and on the fallow edge, not only the field
    for k, (du, dv, ru, rv) in enumerate(((-1500, 1360, 300, 120), (480, 1660, 200, 150), (1600, 1300, 150, 280),
                                          (-1600, 40, 320, 130), (-2200, -700, 160, 260))):
        pts = [w(du + ru * math.cos(t) * (1.0 + 0.15 * math.sin(3 * t + k)), dv + rv * math.sin(t))
               for t in np.linspace(0, 2 * math.pi, 9)[:-1]]
        it.append(item('field', SCRUB + 'BP_CrushableScrub_Thistle', *w(du, dv), A, (ru, rv), 'ThistleAccent%d' % k,
                       outline=pts, seed=5100 + k, layout={'spacing': 110, 'smin': 1.15, 'smax': 1.55, 'inset': 10,
                                                           'missing': 0.15}))
    it.append(yard_litter(it, w))
    return it


def scorch_fringe(outline, rnd):
    """Flat burnt stubble and ash cards in a band up to 3.5 m outside the burnt field."""
    poly = np.array(outline)
    cx, cy = poly.mean(0)
    pts = []
    for k in range(len(poly)):
        a, b = poly[k], poly[(k + 1) % len(poly)]
        for t in np.linspace(0, 1, 9, endpoint=False):
            px, py = a + (b - a) * t
            dx, dy = px - cx, py - cy
            L = math.hypot(dx, dy)
            for _ in range(2):
                out = rnd.uniform(-60, 350) * (1.0 if rnd.random() < 0.8 else 0.4)
                pts.append((px + dx / L * out + rnd.normal(0, 60), py + dy / L * out + rnd.normal(0, 60)))
    cards = {CROPS + 'Meshes/SM_WheatBurnt_Crushed': [], CROPS + 'Meshes/SM_FarmFX_Ash': []}
    for k, (x, y) in enumerate(pts):
        m = CROPS + ('Meshes/SM_WheatBurnt_Crushed' if k % 3 else 'Meshes/SM_FarmFX_Ash')
        sc = float(rnd.uniform(0.9, 1.6) if 'Wheat' in m else rnd.uniform(3.0, 6.0))
        cards[m].append([round(float(x)), round(float(y)), float(rnd.uniform(0, 360)), round(sc, 2)])
    return item('cards', '', float(cx), float(cy), 0, (0, 0), 'BurntFringe', cards=cards,
                materials={CROPS + 'Meshes/SM_WheatBurnt_Crushed': CROPS + 'Materials/MI_FarmCrops_Burnt',
                           CROPS + 'Meshes/SM_FarmFX_Ash': CROPS + 'Materials/MI_FarmCrops_Burnt'})


def _inside_any(x, y, items, pad):
    for it in items:
        if it['kind'] in ('cards', 'fence', 'field') or not it['half'][0]:
            continue
        hx, hy = it['half']
        s = it.get('scale', 1.0)
        a = math.radians(it['yaw'])
        dx, dy = x - it['x'], y - it['y']
        u = dx * math.cos(a) + dy * math.sin(a)
        v = -dx * math.sin(a) + dy * math.cos(a)
        if abs(u) < hx * s + pad and abs(v) < hy * s + pad:
            return True
    return False


def yard_litter(items, w):
    """Review r2: the farmyard was bare sand. Rust-mat and ash-grass cards (the round-3 ground types) in 1-6 m rings
    round the barn, hay shed, house, stacks, bales and along the fences, and litter under the orchard trees; never
    inside a footprint."""
    rnd = np.random.default_rng(77)
    names = ('Barn', 'HayShed', 'HayStack', 'Bale1', 'Bale2', 'LooseStack', 'HayCart', 'FarmHouse')
    hubs = [dict(i) for i in items if i['label'] in names or i['label'].startswith('Orchard')]
    for f_ in (i for i in items if i['kind'] == 'fence'):
        for (ax, ay), (bx, by) in zip(f_['pts'][:-1], f_['pts'][1:]):
            for t in np.linspace(0, 1, 6):
                hubs.append({'x': ax + (bx - ax) * t, 'y': ay + (by - ay) * t, 'half': [60, 60], 'label': 'fencept'})
    keys = [GF + 'MeshesTop/SM_GFT_StrawLitter', GF + 'Meshes/SM_GF_DryLitter', GF + 'MeshesTop/SM_GFT_DryStar_B']
    cards = {k: [] for k in keys}
    for h in hubs:
        orch = h['label'].startswith('Orchard')
        n = 14 if h['label'] in ('Barn', 'HayShed', 'FarmHouse') else 6
        r0 = max(h['half']) * h.get('scale', 1.0)
        for _ in range(n):
            t = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(0, 250) if orch else r0 + rnd.uniform(60, 600)
            x, y = h['x'] + r * math.cos(t), h['y'] + r * math.sin(t)
            if not orch and _inside_any(x, y, items, 60):
                continue
            m = keys[rnd.choice(3, p=[0.45, 0.3, 0.25])]
            cards[m].append([round(float(x)), round(float(y)), float(rnd.uniform(0, 360)),
                             round(float(rnd.uniform(1.8, 3.2)), 2)])
    c = w(0, 0)
    return item('cards', '', c[0], c[1], 0, (0, 0), 'YardLitter', cards=cards,
                materials={keys[0]: SCRUB + 'MI_DryScrub_RustMatTop', keys[1]: SCRUB + 'MI_DryScrub_RustMat',
                           keys[2]: SCRUB + 'MI_DryScrub_AshGrassTop'})


def east_lake(f):
    it = []
    # rim: flat ground on the pit's east side; hut, cars nose-in to the drop, tables, a bench looking over it
    R = (-1700.0, 10700.0)          # flat north rim between the pit and the base ridge, clear of the track stripes
    A = 180.0                       # u points into the pit (-X)
    w = frame(R[0], R[1], A)
    it.append(item('mesh', '/Game/MillitaryBase/Meshes/SM_Shed', *w(-600, -900), A + 90, (250, 200), 'BathHut'))
    # review r1/r2: an identical row; parked as people left them
    for k, (du, dv, dyaw, mesh, wear) in enumerate(((-230, 330, -7, 'Cabriolet', 0.7), (-310, 720, 4, 'Sedan', 0.8),
                                                    (-200, 1130, 11, 'Compact', 0.75))):
        it.append(item('oldcar', OLDCAR, *w(du, dv), A + dyaw, (240, 100), 'Car%d' % k, mesh=CARS + mesh, wear=wear))
    it.append(item('mesh', '/Game/MilitaryBaseCamp/Meshes/SM_PicnicTable', *w(-50, -350), A + 12, (100, 85), 'Table1'))
    it.append(item('mesh', '/Game/MilitaryBaseCamp/Meshes/SM_PicnicTable', *w(-300, -1600), A - 25, (100, 85), 'Table2'))
    it.append(item('mesh', '/Game/NordicHarbour/Props/StreetFurniture/SM_Bench', *w(-150, -1150), A + 90, (105, 60), 'Bench'))
    it.append(item('sign', '/Game/NordicHarbour/Props/TrafficSigns/SM_TrafficSign_Forbidden', *w(300, 100), A + 180, (30, 30), 'SignBathing',
                   second='/Game/NordicHarbour/Props/TrafficSigns/SM_TrafficSign_WaterWarning'))
    it.append(item('mesh', '/Game/NordicHarbour/Props/StreetFurniture/SM_BoatRings', *w(-80, -700), A, (50, 50), 'LifeRing'))
    # pit floor: jetty from the dry floor towards the shrunken pond, ending well short of it
    it.extend(jetty(f))
    # rowboats left on the exposed slope, one keel-up; a paddle
    for k, (x, y, roll) in enumerate(((-6500, 15600, 0), (-9300, 15300, 0), (-6600, 11600, 180))):
        it.append(item('pitmesh', '/Game/Greek_island/Assets/Boat/SM_Boat_02', x, y, 30 + 50 * k, (230, 60), 'Rowboat%d' % k,
                       scale=0.48, roll=roll, lift=10.0 if roll == 180 else 0.0))
    it.append(item('pitmesh', '/Game/Greek_island/Assets/Boat/SM_Paddle', -6300, 15350, 75, (160, 10), 'Paddle'))
    # dead willows on the old shoreline, dry reeds round the pond
    # review r2: validated gentle spots (8.9 and 6.6 deg) for the two that stood on the bank face
    for k, (x, y) in enumerate(((-5200, 10400), (-9507, 11700), (-8007, 16300))):
        it.append(item('pitbp', WILLOW, x, y, 70 * k + 20, (200, 200), 'Willow%d' % k, fixed=True))
    return it


def jetty(f):
    """Plank walkway down the exposed north bank to the shrunken pond (bathers kept extending it as the water fell).
    Review r2: built from shared joints (deck-top z = highest ground across the deck + 20 cm) so the pieces meet with
    no gaps or steps; the reviewer validated these joints and the per-piece pitch / length scale / roll."""
    y = 12800.0
    joints = [(-3031, -609), (-3621, -881), (-4211, -1022), (-4801, -1276), (-5391, -1425)]
    rolls = (8.0, -5.5, -19.0, -20.5)
    items = []
    for k in range(4):
        (x0, z0), (x1, z1) = joints[k], joints[k + 1]
        L = math.hypot(x1 - x0, z1 - z0)
        pitch = -math.degrees(math.atan2(z0 - z1, abs(x1 - x0)))
        path = 'SM_PrypyatBoardwalk_Broken_6m' if k == 3 else 'SM_PrypyatBoardwalk_6m'
        items.append(item('jetty', ENV + 'Prypyat/Crossings/' + path, (x0 + x1) / 2, y, 180.0, (300, 125),
                          'Walkway%d' % k, scale3=(round(L / 607.0, 3), 0.55, 0.8), piece=k, deck_z=(z0 + z1) / 2,
                          pitch=pitch, roll=rolls[k]))
    return items


# ------------------------------------------------------------------------------------------------------------------
def footprint_cells(it):
    """World points sampling the rotated footprint (every 50 cm)."""
    hx, hy = it['half']
    s = it.get('scale', 1.0)
    hx, hy = hx * s, hy * s
    a = math.radians(it['yaw'])
    pts = []
    for du in np.arange(-hx, hx + 1, 50):
        for dv in np.arange(-hy, hy + 1, 50):
            pts.append((it['x'] + du * math.cos(a) - dv * math.sin(a), it['y'] + du * math.sin(a) + dv * math.cos(a)))
    return pts


def check(f, items):
    bad = []
    for it in items:
        if it['kind'] == 'cards':
            continue
        in_pit = it['kind'] in ('pitmesh', 'pitbp', 'jetty')
        worst = {}
        for x, y in footprint_cells(it):
            i, j = fp.to_ij(x, y)
            i, j = int(i), int(j)
            if not (0 <= i < fp.NI and 0 <= j < fp.NJ):
                worst['outside'] = 1
                continue
            stripe_lim = 2.0 if it['kind'] in ('field', 'fence', 'pitmesh', 'pitbp', 'jetty') else RULES['stripe']
            for key, arr, lim in (('road', f.road_edge, RULES['road']), ('route', f.route_d, RULES['route']),
                                  ('stripe', f.stripe_d, stripe_lim), ('water', f.water_d, RULES['water']),
                                  ('foot', f.foot_d, RULES['foot']), ('resource', f.resource_d, RULES['resource'])):
                if arr[j, i] < lim:
                    worst[key] = min(worst.get(key, 99), round(float(arr[j, i]), 1))
            if not in_pit and f.slope[j, i] > RULES['slope']:
                worst['slope'] = max(worst.get('slope', 0), round(float(f.slope[j, i]), 1))
        if worst:
            bad.append((it['label'], worst))
    return bad


def picture(f, items, name):
    for site, (cx, cy, half) in {'west': (-20700, -18700, 6500), 'east': (-7500, 14500, 7000)}.items():
        i0, j0 = fp.to_ij(cx - half, cy - half)
        i1, j1 = fp.to_ij(cx + half, cy + half)
        i0, j0, i1, j1 = int(i0), int(j0), int(i1), int(j1)
        sl = (slice(j0, j1), slice(i0, i1))
        img = np.zeros((j1 - j0, i1 - i0, 3), np.uint8) + 55
        img[f.slope[sl] > 20] = (0, 110, 200)
        img[f.stripe_d[sl] < 0.5] = (120, 170, 220)
        img[f.route_d[sl] < 1.5] = (255, 0, 255)
        img[f.road_edge[sl] < 0] = (90, 90, 90)
        img[f.foot_d[sl] < 0.5] = (0, 0, 255)
        img[f.wet[sl]] = (255, 120, 0)
        sc = 6
        out = cv2.resize(img.transpose(1, 0, 2)[::-1].copy(), None, fx=sc, fy=sc, interpolation=cv2.INTER_NEAREST)
        H = out.shape[0]

        def px(x, y):
            ii, jj = fp.to_ij(x, y)
            return int((jj - j0) * sc), int(H - (ii - i0) * sc)
        for it in items:
            pts = footprint_cells(dict(it, half=it['half']))
            corners = []
            hx, hy = it['half']
            s = it.get('scale', 1.0)
            a = math.radians(it['yaw'])
            for du, dv in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)):
                du, dv = du * s, dv * s
                corners.append(px(it['x'] + du * math.cos(a) - dv * math.sin(a), it['y'] + du * math.sin(a) + dv * math.cos(a)))
            cv2.polylines(out, [np.array(corners, np.int32)], True, (0, 255, 0), 2)
            cv2.putText(out, it['label'], px(it['x'], it['y']), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 255), 1)
            if it['kind'] == 'fence':
                cv2.polylines(out, [np.array([px(*p) for p in it['pts']], np.int32)], False, (0, 255, 255), 2)
        cv2.putText(out, '%s  grid 10 m  up=+X right=+Y' % site, (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        cv2.imwrite(OUT + '%s_%s.png' % (name, site), out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--round', type=int, default=1)
    args = ap.parse_args()
    f = fp.Field(skip_prefixes=('FOL_', 'POIW_', 'POIE_'))   # the POIs being re-planned never block themselves
    items = west_farm() + east_lake(f)
    for it in items:
        it['label'] = ('POIW_' if it['x'] < 0 and it['y'] < 0 else 'POIE_') + it['label']
    bad = check(f, items)
    for b in bad:
        print('KEEP-OUT', b)
    with open(OUT + 'poi_r%d.json' % args.round, 'w') as h:
        json.dump({'items': items, 'violations': bad, 'rules': RULES}, h, indent=1)
    picture(f, items, 'poi_r%d' % args.round)
    print('items', len(items), 'violations', len(bad))


if __name__ == '__main__':
    main()
