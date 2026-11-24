# blender -b --factory-startup -P pieces.py -- <building work dir>
# Reads <dir>/config.json and <dir>/building.fbx; builds the piece table of one nomadic building:
#   - loose parts -> pieces (tiny bolts/rails merge into the closest larger part, the radio mast is one piece)
#   - truck parts are found by material AND position and never move
#   - each piece gets a category, a pivot and a normalised start time/duration in the deploy
# Writes <out_dir>/pieces.json and a colour check render <out_dir>/pieces_check_*.png.
import json
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
out_dir = argv[0]
src = os.path.join(out_dir, 'building.fbx')
CONFIG = json.load(open(os.path.join(out_dir, 'config.json')))
obj = common.load_fbx(src)
parts = common.Parts(obj)
mat_names = [s.material.name if s.material else '' for s in obj.material_slots]

# per building (config.json): truck = pieces whose faces are >= 70 % truck materials AND that lie in the truck box
TRUCK_MATERIALS = tuple(CONFIG['truck_materials'])
TRUCK_LO, TRUCK_HI = Vector(CONFIG['truck_box_lo']), Vector(CONFIG['truck_box_hi'])
TRUCK_CENTRE = Vector(CONFIG['truck_centre'])
# optional tall lattice (radio mast, crane): its loose parts become one piece that hinges up
MAST_XY = Vector(CONFIG['mast_xy'] + [0]) if CONFIG.get('mast_xy') else None
MAST_RADIUS = CONFIG.get('mast_radius', 70.0)
TINY_DIAGONAL = 25.0


def inside(lo, hi, box_lo, box_hi):
    return all(lo[a] >= box_lo[a] and hi[a] <= box_hi[a] for a in range(3))


def is_truck(i):
    # the hull island also carries a few faces of building atlases, so a 70 % majority of truck faces decides
    truck_faces = sum(v for m, v in parts.material[i].items() if any(t in mat_names[m] for t in TRUCK_MATERIALS))
    return truck_faces >= 0.7 * parts.faces[i] and inside(parts.lo[i], parts.hi[i], TRUCK_LO, TRUCK_HI)


def centre(i):
    return (parts.lo[i] + parts.hi[i]) * 0.5


def size(i):
    return parts.hi[i] - parts.lo[i]


def box_distance(i, j):
    d = 0.0
    for a in range(3):
        gap = max(parts.lo[j][a] - parts.hi[i][a], parts.lo[i][a] - parts.hi[j][a], 0.0)
        d += gap * gap
    return math.sqrt(d)


# 1) part -> piece
piece_of = [-1] * parts.count
pieces = []  # list of lists of part ids


def new_piece(members):
    pieces.append(members)
    for m in members:
        piece_of[m] = len(pieces) - 1


truck_parts = [i for i in range(parts.count) if is_truck(i)]
new_piece(truck_parts)  # piece 0 = the whole truck, static
mast = [] if MAST_XY is None else [i for i in range(parts.count) if piece_of[i] < 0
        and (Vector((centre(i).x, centre(i).y, 0)) - MAST_XY).length < MAST_RADIUS and size(i).length > 30]
mast_lo_z = min(parts.lo[i].z for i in mast) if mast else 0
new_piece(mast)
big = [i for i in range(parts.count) if piece_of[i] < 0 and size(i).length >= TINY_DIAGONAL]
for i in big:
    new_piece([i])
for i in range(parts.count):
    if piece_of[i] >= 0:
        continue
    # tiny part: ride along with the closest non-tiny part if it touches one, else its own piece
    best, best_d = None, 40.0
    for j in range(parts.count):
        if j == i or piece_of[j] < 0 or size(j).length < TINY_DIAGONAL:
            continue
        d = box_distance(i, j)
        if d < best_d:
            best, best_d = j, d
    if best is None:
        new_piece([i])
    else:
        pieces[piece_of[best]].append(i)
        piece_of[i] = piece_of[best]

# 2) per-piece bounds and category
records = []
for pid, members in enumerate(pieces):
    if not members:  # no truck / no mast in this mesh: an empty static placeholder keeps piece ids 0 and 1 fixed
        c = list(TRUCK_CENTRE)
        records.append({'id': pid, 'cat': 'truck', 'parts': [], 'lo': c, 'hi': c, 'material': '', 'pivot': c})
        continue
    lo = Vector((min(parts.lo[m].x for m in members), min(parts.lo[m].y for m in members), min(parts.lo[m].z for m in members)))
    hi = Vector((max(parts.hi[m].x for m in members), max(parts.hi[m].y for m in members), max(parts.hi[m].z for m in members)))
    ext = hi - lo
    mats = {}
    for m in members:
        for k, v in parts.material[m].items():
            mats[k] = mats.get(k, 0) + v
    main = mat_names[max(mats, key=mats.get)]
    horizontal = max(ext.x, ext.y)
    if pid == 0:
        cat = 'truck'
    elif pid == 1:
        cat = 'mast'
    elif 'SandBagScaffold' in main and ext.z < 22 and horizontal > 120:
        cat = 'deck'
    elif 'SandBagScaffold' in main:
        cat = 'sandbag'
    elif ext.z > 180 and horizontal < 45:
        cat = 'antenna'
    elif horizontal > 220 or ext.z > 220:
        cat = 'structure'
    else:
        cat = 'prop'
    records.append({'id': pid, 'cat': cat, 'parts': members, 'lo': list(lo), 'hi': list(hi), 'material': main,
                    'pivot': [(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z]})

# optional per-building hook: <work>/pieces_hook.py is exec'd with this script's globals and may define
#   relabel(records) -> change r['cat'] (or merge / re-pivot) before timing
#   post_bands(BANDS) -> return replacement category timing bands
HOOK = {}
if os.path.exists(os.path.join(out_dir, 'pieces_hook.py')):
    HOOK = dict(globals())
    exec(open(os.path.join(out_dir, 'pieces_hook.py'), encoding='utf-8').read(), HOOK)
    if 'relabel' in HOOK:
        HOOK['relabel'](records)
    print('pieces_hook.py loaded:', [k for k in ('relabel', 'post_bands') if k in HOOK])

# 3) timing (normalised 0..1 over the whole deploy); outward from the truck, category bands overlap
random.seed(11)


def flat_distance(r):
    return (Vector((r['pivot'][0], r['pivot'][1], 0)) - TRUCK_CENTRE).length


dmax = max(flat_distance(r) for r in records)
BANDS = {  # start window and duration per category
    'deck': (0.02, 0.30, 0.10),
    'structure': (0.22, 0.48, 0.16),
    'prop': (0.38, 0.72, 0.11),
    'sandbag': (0.45, 0.82, 0.09),
    'mast': (0.74, 0.74, 0.22),
    'antenna': (0.70, 0.86, 0.12),
}
if 'post_bands' in HOOK:
    BANDS = HOOK['post_bands'](BANDS)
structures = sorted((r for r in records if r['cat'] == 'structure'), key=flat_distance)
for r in records:
    r['seed'] = random.random()
    if r['cat'] == 'truck':
        r['t0'], r['dur'] = 0.0, 0.0
        continue
    a, b, dur = BANDS[r['cat']]
    if r['cat'] == 'structure':
        k = structures.index(r) / max(len(structures) - 1, 1)
    else:
        k = flat_distance(r) / dmax
        k = min(max(k + (r['seed'] - 0.5) * 0.12, 0.0), 1.0)
    r['t0'] = a + (b - a) * k
    r['dur'] = dur

counts = {}
for r in records:
    counts[r['cat']] = counts.get(r['cat'], 0) + 1
print('pieces', len(records), counts, 'truck parts', len(truck_parts), 'mast parts', len(mast))
with open(os.path.join(out_dir, 'pieces.json'), 'w') as f:
    json.dump({'source': src, 'part_count': parts.count, 'vert_part': parts.vert_part, 'piece_of_part': piece_of,
               'pieces': records}, f)

# 4) colour check render: truck red, deck tan, structure blue, prop green, sandbag yellow, mast magenta
CAT_COLOR = {'truck': (0.9, 0.05, 0.05, 1), 'deck': (0.55, 0.42, 0.28, 1), 'structure': (0.15, 0.35, 0.9, 1),
             'prop': (0.1, 0.75, 0.2, 1), 'sandbag': (0.95, 0.85, 0.2, 1), 'mast': (0.9, 0.1, 0.9, 1), 'antenna': (0.1, 0.9, 0.9, 1)}
me = obj.data
attr = me.color_attributes.new('Cat', 'FLOAT_COLOR', 'CORNER')
for p in me.polygons:
    c = CAT_COLOR[records[piece_of[parts.poly_part[p.index]]]['cat']]
    for li in p.loop_indices:
        attr.data[li].color = c
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'VERTEX'
scene.render.resolution_x = scene.render.resolution_y = 1200
cam_data = bpy.data.cameras.new('cam')
cam = bpy.data.objects.new('cam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
c = common.from_ue(Vector((-148, -49, 300)))
for name, eye in (('oblique', c + Vector((14, -12, 13))), ('top', c + Vector((0, 0, 30)))):
    cam.location = eye
    cam.rotation_euler = (c - eye).to_track_quat('-Z', 'Y').to_euler()
    cam_data.lens = 35
    scene.render.filepath = os.path.join(out_dir, 'pieces_check_%s.png' % name)
    bpy.ops.render.render(write_still=True)
