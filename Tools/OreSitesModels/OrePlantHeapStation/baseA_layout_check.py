"""Base A layout for the 3B ore plant: writes baseA_layout.json and checks it offline (no Unreal).

  python -B baseA_layout_check.py

Footprints: the new pieces from their UCX hulls (Blender model json, footprints_cm), everything else from the oriented
bounds of its class (Saved/OreMountains/sites/ue_oreplant/verify.json, read in a commandlet). Checks the same keep-clear
corridors as place_soviet_sites.py (valley road 520 cm, ammo track 350 cm, counterattack spawn 600 cm), overlaps
with every yard prop (Ore_detail.json) and Base A actor, strongpoint posts inside footprints, and the loading lane.
Writes baseA_layout.json beside this file and Saved/OreMountains/sites/models/baseA_layout_plan.png.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Model_OrePlant_HeapStation/OrePlant_HeapStation_model.json'
VERIFY = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/verify.json'
DETAIL = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_detail.json'
PLAN_PNG = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/baseA_layout_plan.png'
S = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/BaseA/'
RB = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
TAG_A = 'ORE_BaseA_Target'

# ---------------------------------------------------------------------------------------------------- the layout
# The crusher and the silo group are coupled: the silo group's gallery ends at the crusher's south-face stub, so
# keep SILO - CRUSHER = (-1250, +1150) cm and yaw 0 for both (or move both by the same offset).
LAYOUT = [
    dict(label='ORE_Site_BaseA_Crusher', name='Crusher', asset=S + 'BP_OreBaseA_CrusherHouse', x=3200, y=25000, yaw=0,
         tags=[TAG_A], replaces='ORE_Site_BaseA_AmmoStore_1 (Rus_Building_AmmoStorage at 3250, 24700)', foot='crusher'),
    dict(label='ORE_Site_BaseA_Silos', name='Silos', asset=S + 'BP_OreBaseA_SiloGroup', x=1950, y=26150, yaw=0,
         tags=[TAG_A], replaces='ORE_Site_BaseA_Supplies (Rus_Building_Supplies at 2600, 25700)', foot='silos'),
    dict(label='ORE_Site_BaseA_AmmoStore_2', name='AmmoStore_2', asset=RB + 'Rus_Building_AmmoStorage', x=1150, y=23750,
         yaw=0, tags=[TAG_A], replaces='itself, moved from (1500, 24800): that spot is the loading lane', foot='ammo_store'),
    dict(label='ORE_Site_BaseA_FuelStore', name='FuelStore', asset=RB + 'Rus_Building_FuelStorage', x=-1400, y=24300,
         yaw=0, tags=[TAG_A], replaces='unchanged', foot='fuel_store'),
    dict(label='ORE_Site_BaseA_NissenShed_1', name='NissenShed_1', asset=S + 'BP_OreBaseA_NissenShed', x=3350, y=22350,
         yaw=90, tags=[], replaces='new dressing (no health, no owner)', foot='shed'),
]
UNCHANGED = [   # Base A actors of the sites plan that stay as they are
    dict(label='ORE_Site_BaseA_Tower_SW', x=-1200, y=20950, yaw=270, foot='tower_mg'),
    dict(label='ORE_Site_BaseA_Tower_NW', x=3000, y=20850, yaw=270, foot='tower_mg'),
    dict(label='ORE_Site_BaseA_MGBunker_E', x=-200, y=26700, yaw=180, foot='mg_bunker'),
]
POSTS = {'LightTank_1': (1200, 23300), 'LightTank_2': (2700, 23200), 'Squad_1': (600, 26000), 'Squad_2': (-1100, 23000),
         'Squad_3': (2000, 26900)}
POST_MOVES = {'Squad_3': (3050, 26950)}          # inside the silo group's bounds box otherwise (pss.check uses boxes)
DETAIL_MOVES = {'Yard_Stack_3': None}            # filled by the search below (barrel stack in the loading lane)
LANE = dict(x0=1100, x1=1660, y0=25350, y1=26900)  # lorries stand here under the three spouts (no actor)

VALLEY_ROAD = [(-1905, 28475), (-1807, 27638), (-1483, 26800), (-1039, 25962), (-602, 25125), (-309, 24288),
               (-163, 23450), (19, 22612), (202, 21775), (366, 20938), (500, 20100), (587, 19262), (67, 10050)]
CORRIDORS = [('valley_road', VALLEY_ROAD, 520.0),
             ('ammo_track', [(2010, 24120), (3128, 23861), (4246, 23603), (5360, 23316), (6495, 23019)], 350.0),
             ('counterattack_spawn', [(2010, 22500)], 600.0)]
DETAIL_FOOT = {'Hangar': 'Yard_Hangar', 'Chimney': 'Yard_Chimney', 'WaterTower': 'Yard_WaterTower', 'Fuel': 'Yard_Fuel',
               'Container': 'Yard_Container', 'Fence': 'Yard_Fence'}


# ---------------------------------------------------------------------------------------------------- geometry
def seg_dist(px, py, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0.0 if L == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def line_dist(px, py, line):
    if len(line) == 1:
        return math.hypot(px - line[0][0], py - line[0][1])
    return min(seg_dist(px, py, a, b) for a, b in zip(line[:-1], line[1:]))


def xform(poly, x, y, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(x + px * c - py * s, y + px * s + py * c) for px, py in poly]


def box_poly(c, e):
    return [(c[0] - e[0], c[1] - e[1]), (c[0] + e[0], c[1] - e[1]), (c[0] + e[0], c[1] + e[1]), (c[0] - e[0], c[1] + e[1])]


def sat_overlap(p, q, margin=0.0):
    for poly in (p, q):
        n = len(poly)
        for i in range(n):
            ax, ay = poly[i]
            bx, by = poly[(i + 1) % n]
            nx, ny = by - ay, ax - bx
            L = math.hypot(nx, ny) or 1.0
            nx, ny = nx / L, ny / L
            pa = [vx * nx + vy * ny for vx, vy in p]
            qa = [vx * nx + vy * ny for vx, vy in q]
            if max(pa) + margin <= min(qa) or max(qa) + margin <= min(pa):
                return False
    return True


def inside(pt, poly):
    sign = None
    n = len(poly)
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        c = (bx - ax) * (pt[1] - ay) - (by - ay) * (pt[0] - ax)
        if abs(c) < 1e-9:
            continue
        s = c > 0
        if sign is None:
            sign = s
        elif s != sign:
            return False
    return True


def sample(poly):
    pts = list(poly)
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        for k in range(1, 4):
            pts.append((a[0] + (b[0] - a[0]) * k / 4, a[1] + (b[1] - a[1]) * k / 4))
    return pts


def main():
    model = json.load(open(MODEL))
    ver = json.load(open(VERIFY))['foot']
    det = json.load(open(DETAIL))['props']
    polys_local = {
        'crusher': model['footprints_cm']['SM_OreBaseA_CrusherHouse'],
        'silos': model['footprints_cm']['SM_OreBaseA_SiloTable'],
        'shed': model['footprints_cm']['SM_OreBaseA_NissenShed'],
    }

    def foot(key, x, y, yaw):
        if key in polys_local:
            return [xform(p, x, y, yaw) for p in polys_local[key]]
        f = ver[key]['colliding']
        return [xform(box_poly(f['c'][:2], f['e'][:2]), x, y, yaw)]

    actors = []
    for it in LAYOUT:
        actors.append(dict(label=it['label'], polys=foot(it['foot'], it['x'], it['y'], it['yaw']), new=True))
    for it in UNCHANGED:
        actors.append(dict(label=it['label'], polys=foot(it['foot'], it['x'], it['y'], it['yaw']), new=False))
    props = []
    for e in det:
        _, path, label, x, y, yaw = e[:6]
        if not (-3000 < x < 5000 and 19000 < y < 29000):
            continue
        key = next((v for k, v in DETAIL_FOOT.items() if k in label), None)
        if 'Stack' in label:
            key = 'Yard_Barrels' if 'Barrel' in path else 'Yard_Stack'
        if key is None:
            continue
        props.append(dict(label=label, x=x, y=y, yaw=yaw, key=key, polys=foot(key, x, y, yaw)))
    problems, notes = [], []
    # corridors
    for a in actors:
        for poly in a['polys']:
            for name, line, half in CORRIDORS:
                d = min(line_dist(px, py, line) for px, py in sample(poly))
                if d < half:
                    problems.append('%s within %d cm of %s (keep %d)' % (a['label'], d, name, half))
    # min distance of the new pieces to the corridors (report)
    for a in actors[:2]:
        for name, line, half in CORRIDORS[:2]:
            d = min(line_dist(px, py, line) for poly in a['polys'] for px, py in sample(poly))
            notes.append('%s: %d cm from %s (keep %d)' % (a['label'], d, name, half))
    # overlaps between actors (the crusher / silo pair touches on purpose: the gallery docks on the crusher stub)
    for i, a in enumerate(actors):
        for b in actors[i + 1:]:
            if {a['label'], b['label']} == {'ORE_Site_BaseA_Crusher', 'ORE_Site_BaseA_Silos'}:
                continue
            if any(sat_overlap(p, q, 20.0) for p in a['polys'] for q in b['polys']):
                problems.append('overlap %s / %s' % (a['label'], b['label']))
        for pr in props:
            if pr['label'] in DETAIL_MOVES:
                continue
            if any(sat_overlap(p, q, 20.0) for p in a['polys'] for q in pr['polys']):
                (problems if a['new'] else notes).append('overlap %s / %s%s' % (
                    a['label'], pr['label'], '' if a['new'] else ' (pre-existing, not part of this layout)'))
    # crusher / silo contact is only the gallery on the stub
    cr = actors[0]['polys']
    si = actors[1]['polys']
    contact = sum(1 for p in cr for q in si if sat_overlap(p, q, 0.0))
    notes.append('crusher/silo hull contacts: %d (gallery + trestle at the stub)' % contact)
    # posts
    posts = dict(POSTS)
    posts.update(POST_MOVES)
    for name, (px, py) in posts.items():
        for a in actors:
            if any(inside((px, py), p) for p in a['polys']):
                problems.append('post %s (%d, %d) inside %s' % (name, px, py, a['label']))
        for cname, line, half in CORRIDORS:
            if line_dist(px, py, line) < half and cname != 'counterattack_spawn':
                notes.append('post %s within %s corridor' % (name, cname))
    for name, (px, py) in POSTS.items():
        if name in POST_MOVES and any(inside((px, py), p) for a in actors for p in a['polys']):
            notes.append('post %s at its current spot (%d, %d) is inside a new footprint: move to %s' % (name, px, py, POST_MOVES[name]))
    # the loading lane must be free of anything colliding
    lane = box_poly(((LANE['x0'] + LANE['x1']) / 2, (LANE['y0'] + LANE['y1']) / 2),
                    ((LANE['x1'] - LANE['x0']) / 2, (LANE['y1'] - LANE['y0']) / 2))
    for a in actors:
        if any(sat_overlap(lane, p) for p in a['polys']):
            problems.append('lane blocked by %s' % a['label'])
    lane_props = [pr for pr in props if any(sat_overlap(lane, p) for p in pr['polys'])]
    for pr in lane_props:
        notes.append('Ore_detail prop %s (%d, %d) stands in the loading lane' % (pr['label'], pr['x'], pr['y']))
    # find a free spot for the barrel stack(s) in the lane: clear of everything, corridors and the lane
    moves = {}
    for pr in lane_props:
        best = None
        for cx in range(-1500, 3700, 50):
            for cy in range(21000, 27000, 50):
                q = foot(pr['key'], cx, cy, pr['yaw'])
                if any(sat_overlap(lane, p, 50.0) for p in q):
                    continue
                if any(line_dist(px, py, line) < half + 50 for p in q for px, py in sample(p) for _, line, half in CORRIDORS):
                    continue
                if any(sat_overlap(p, r, 80.0) for p in q for a in actors for r in a['polys']):
                    continue
                if any(sat_overlap(p, r, 80.0) for p in q for o in props if o is not pr for r in o['polys']):
                    continue
                if any(inside(pp, p) for p in q for pp in posts.values()):
                    continue
                d = math.hypot(cx - pr['x'], cy - pr['y'])
                if best is None or d < best[0]:
                    best = (d, cx, cy)
        if best:
            moves[pr['label']] = dict(from_=[pr['x'], pr['y']], to=[best[1], best[2]], yaw=pr['yaw'])
    # a free spot for AmmoStore_2 near its old place (the old place is the loading lane)
    a2 = next(a for a in actors if a['label'] == 'ORE_Site_BaseA_AmmoStore_2')
    others = [a for a in actors if a is not a2]
    best = None
    for cx in range(-1500, 3700, 50):
        for cy in range(21000, 27000, 50):
            q = foot('ammo_store', cx, cy, 0)
            if any(sat_overlap(lane, p, 150.0) for p in q):
                continue
            if any(line_dist(px, py, line) < half + 100 for p in q for px, py in sample(p) for _, line, half in CORRIDORS):
                continue
            if any(sat_overlap(p, r, 150.0) for p in q for o in others for r in o['polys']):
                continue
            if any(sat_overlap(p, r, 150.0) for p in q for o in props if o['label'] not in moves for r in o['polys']):
                continue
            d = math.hypot(cx - 1500, cy - 24800)
            if best is None or d < best[0]:
                best = (d, cx, cy)
    notes.append('free AmmoStore_2 spot nearest (1500, 24800): %s' % (str(best[1:]) if best else 'none'))
    out = dict(
        about='Base A ore plant (3B) layout, Unreal cm (+X north, +Y east), pivots on the ground, yaw degrees. '
              'Seat like place_soviet_sites.py (lowest landscape point under the footprint minus 5 cm). The crusher '
              'and the silo group are coupled by a fixed offset (silo - crusher = (-1250, +1150) cm, both yaw 0): the '
              'silo group\'s gallery docks on the crusher\'s south-face stub. Generated by baseA_layout_check.py.',
        items=[{k: v for k, v in it.items() if k != 'foot'} for it in LAYOUT],
        remove_labels=['ORE_Site_BaseA_Supplies', 'ORE_Site_BaseA_AmmoStore_1'],
        replaced_placeholders_add=['ORE_PH_wismut_conveyor', 'ORE_Note_PH_wismut_conveyor', 'ORE_PH_wismut_sheds',
                                   'ORE_Note_PH_wismut_sheds'],
        check_expect='pss.check() compares bounds boxes: it will report "overlap ORE_Site_BaseA_Crusher / '
                     'ORE_Site_BaseA_Silos" (the gallery docks on the crusher stub, by design).',
        unchanged=[it['label'] for it in UNCHANGED],
        strongpoint_post_moves={k: list(v) for k, v in POST_MOVES.items()},
        ore_detail_moves=moves,
        loading_lane_cm=LANE,
        plan_lines=[
            'item("BaseA", "%s", "%s", %d, %d, %d, [%s]),' % (it['name'], it['asset'], it['x'], it['y'], it['yaw'],
                                                             'TAG_A' if it['tags'] else '')
            for it in LAYOUT],
        problems=problems, notes=notes)
    with open(os.path.join(HERE, 'baseA_layout.json'), 'w') as f:
        json.dump(out, f, indent=1)
    print(json.dumps(dict(problems=problems, notes=notes, moves=moves), indent=1))
    plan(actors, props, posts, lane, moves, foot)


def plan(actors, props, posts, lane, moves, foot):
    from PIL import Image, ImageDraw
    S_, X0, Y0, X1, Y1 = 0.12, -2600, 19800, 4400, 28200   # px per cm, window
    W, H = int((Y1 - Y0) * S_), int((X1 - X0) * S_)
    im = Image.new('RGB', (W, H), (205, 190, 165))
    dr = ImageDraw.Draw(im)

    def px(p):
        return ((p[1] - Y0) * S_, (X1 - p[0]) * S_)          # east right, north up
    for name, line, half in CORRIDORS:
        if len(line) > 1:
            dr.line([px(p) for p in line], fill=(150, 140, 125) if name == 'ammo_track' else (110, 105, 100),
                    width=int(2 * half * S_))
        else:
            r = half * S_
            c = px(line[0])
            dr.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], outline=(200, 80, 0), width=2)
    dr.polygon([px(p) for p in lane], outline=(30, 60, 180), fill=(170, 180, 205))
    for pr in props:
        for p in pr['polys']:
            dr.polygon([px(q) for q in p], outline=(70, 90, 120), fill=(160, 185, 215) if pr['label'] not in moves else (240, 120, 120))
    for lab, m in moves.items():
        pr = next(o for o in props if o['label'] == lab)
        for p in foot(pr['key'], m['to'][0], m['to'][1], m['yaw']):
            dr.polygon([px(q) for q in p], outline=(200, 0, 0), fill=(250, 200, 200))
    for a in actors:
        for p in a['polys']:
            dr.polygon([px(q) for q in p], outline=(60, 30, 10), fill=(185, 120, 70) if a['new'] else (150, 140, 120))
        c = a['polys'][0][0]
        dr.text(px(c), a['label'].replace('ORE_Site_BaseA_', ''), fill=(0, 0, 0))
    for name, p in posts.items():
        c = px(p)
        dr.ellipse([c[0] - 4, c[1] - 4, c[0] + 4, c[1] + 4], fill=(200, 0, 0))
        dr.text((c[0] + 5, c[1] - 5), name, fill=(120, 0, 0))
    dr.line([(20, H - 20), (20 + 1000 * S_, H - 20)], fill=(0, 0, 0), width=3)
    dr.text((20, H - 38), '10 m   N up, E right', fill=(0, 0, 0))
    os.makedirs(os.path.dirname(PLAN_PNG), exist_ok=True)
    im.save(PLAN_PNG)


main()
