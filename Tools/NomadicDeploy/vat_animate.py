# blender -b --factory-startup -P vat_animate.py -- <building work dir>   (reads building.fbx, pieces.json, config.json;
#   writes deploy.blend + pieces_vat.json). Edit the choreography per building below or in config.json.
# PROTOTYPE 2 (rigid-piece vertex animation texture): builds one keyframed Blender object per deploy piece and
# saves the .blend. This file is the hand-editable source: tweak keys in Blender, then rerun vat_bake.py.
# Frame FRAME_START = progress 0 (everything packed), FRAME_END = progress 1 (HQ built). Objects sit at their
# rest pose on the last frame. Object origin = the piece's rotation pivot (hinge edge for planks/mast).
# Blender space (metres, Blender Y = -Unreal Y); timing comes from pieces.json like prototype 1.
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
work = argv[0]
src, table, dst = os.path.join(work, 'building.fbx'), os.path.join(work, 'pieces.json'), os.path.join(work, 'deploy.blend')
CONFIG = json.load(open(os.path.join(work, 'config.json')))
FRAME_START, FRAME_END = 1, 181  # 180 steps; the bake samples every frame
src_obj = common.load_fbx(src)
data = json.load(open(table))
pieces = data['pieces']
piece_of_part = data['piece_of_part']
vert_piece = [piece_of_part[p] for p in data['vert_part']]
TRUCK_BED = common.from_ue(Vector(CONFIG['truck_bed']))
random.seed(7)


def frame_at(progress):
    return FRAME_START + (FRAME_END - FRAME_START) * min(max(progress, 0.0), 1.0)


def bl(v):
    return common.from_ue(Vector(v))


# --- proxy objects: one mesh per piece, origin at its pivot ---------------------------------------------------
me = src_obj.data
faces_of_piece = [[] for _ in pieces]
for p in me.polygons:
    faces_of_piece[vert_piece[p.vertices[0]]].append(p.index)


def hinge_pivot(r):
    """Planks hinge on their long edge nearest the truck; everything else on its bottom centre."""
    lo, hi = Vector(r['lo']), Vector(r['hi'])
    if r['cat'] != 'deck':
        return Vector(r['pivot']), None
    ext = hi - lo
    centre = (lo + hi) / 2
    truck = Vector(CONFIG['truck_centre'])
    if ext.x >= ext.y:  # long along X: hinge axis X, edge on the Y side nearer the truck
        y = lo.y if abs(lo.y - truck.y) < abs(hi.y - truck.y) else hi.y
        return Vector((centre.x, y, lo.z)), Vector((1, 0, 0))
    x = lo.x if abs(lo.x - truck.x) < abs(hi.x - truck.x) else hi.x
    return Vector((x, centre.y, lo.z)), Vector((0, 1, 0))


coll = bpy.data.collections.new('DeployPieces')
bpy.context.scene.collection.children.link(coll)
objects = []
for r in pieces:
    pivot_ue, axis_ue = hinge_pivot(r)
    r['vat_pivot'] = list(pivot_ue)
    pivot = bl(pivot_ue)
    bm = bmesh.new()
    vmap = {}
    for fi in faces_of_piece[r['id']]:
        poly = me.polygons[fi]
        vs = []
        for vi in poly.vertices:
            if vi not in vmap:
                vmap[vi] = bm.verts.new(me.vertices[vi].co - pivot)
            vs.append(vmap[vi])
        try:
            bm.faces.new(vs)
        except ValueError:
            pass
    mesh = bpy.data.meshes.new('P%03d_%s' % (r['id'], r['cat']))
    bm.to_mesh(mesh)
    bm.free()
    ob = bpy.data.objects.new(mesh.name, mesh)
    coll.objects.link(ob)
    ob.location = pivot
    ob.rotation_mode = 'XYZ'  # Euler keys so tumbles can spin more than half a turn between keys
    ob['piece_id'] = r['id']
    ob['category'] = r['cat']
    ob['rest_pivot_ue'] = list(pivot_ue)
    if axis_ue is not None:
        ob['hinge_axis_ue'] = list(axis_ue)
    objects.append(ob)
bpy.data.objects.remove(src_obj)


# --- keyframing helpers --------------------------------------------------------------------------------------
def key(ob, progress, loc=None, rot=None, scale=None, interp='BEZIER'):
    f = frame_at(progress)
    if loc is not None:
        ob.location = loc
        ob.keyframe_insert('location', frame=f)
    if rot is not None:
        ob.rotation_euler = rot if isinstance(rot, Euler) else rot.to_euler('XYZ', ob.rotation_euler)
        ob.keyframe_insert('rotation_euler', frame=f)
    if scale is not None:
        ob.scale = (scale, scale, scale)
        ob.keyframe_insert('scale', frame=f)


def rest_key(ob, progress):
    key(ob, progress, loc=ob['rest'], rot=Quaternion(), scale=1.0)


def hidden_until(ob, progress, loc, rot):
    """Zero scale (collapsed) from the start until the piece's own start time."""
    key(ob, 0.0, loc=loc, rot=rot, scale=0.0)
    key(ob, max(progress - 0.001, 0.0), loc=loc, rot=rot, scale=0.0)


def rest_of(ob):
    """The piece's rest location as a Vector (ob['rest'] is stored as an ID property array)."""
    return Vector(ob['rest'])


def repivot(ob, r, p_ue):
    """Move a piece's rotation pivot to p_ue (Unreal cm, mesh space) without moving its geometry."""
    p_ue = Vector(p_ue)
    new = bl(p_ue)
    delta = ob.location - new
    for v in ob.data.vertices:
        v.co += delta
    ob.location = new
    ob['rest'] = new.copy()
    ob['rest_pivot_ue'] = list(p_ue)
    r['vat_pivot'] = list(p_ue)


def random_axis():
    a = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1)))
    return a.normalized() if a.length > 1e-3 else Vector((0, 0, 1))


def axis_bl(axis_ue):
    return Vector((axis_ue[0], -axis_ue[1], axis_ue[2]))


# --- optional per-building choreography: <work>/choreo.py ------------------------------------------------------
# It is exec'd with this script's globals (objects, pieces, key, rest_key, hidden_until, TRUCK_BED, CONFIG, bl, ...)
# and may define:
#   animate(ob, r) -> True when it keyed the piece itself (the default choreography below is then skipped for it;
#                     every piece is still held at rest at progress 1.0)
#   post()         -> runs after all pieces are keyed (e.g. re-time or add secondary motion)
CHOREO = {}
if os.path.exists(os.path.join(work, 'choreo.py')):
    CHOREO = dict(globals())
    exec(open(os.path.join(work, 'choreo.py'), encoding='utf-8').read(), CHOREO)
    print('choreo.py loaded:', [k for k in ('animate', 'post') if k in CHOREO])


# --- choreography per category ---------------------------------------------------------------------------------
for ob, r in zip(objects, pieces):
    ob['rest'] = ob.location.copy()
    if 'animate' in CHOREO and CHOREO['animate'](ob, r):
        rest_key(ob, 1.0)
        continue
    rest = ob.location.copy()
    t0, dur = r['t0'], r['dur']
    t1 = t0 + dur
    cat = r['cat']
    if cat == 'truck':
        rest_key(ob, 0.0)
        continue
    if cat == 'deck':
        # plank stands up on its truck-side edge, appears, then swings down flat with a small bounce
        axis = axis_bl(ob['hinge_axis_ue'])
        centre = (Vector(r['lo']) + Vector(r['hi'])) / 2
        side = 1.0 if axis.cross(bl(centre) - rest).z >= 0 else -1.0  # body swings up, not into the ground
        up = Quaternion(axis, side * math.radians(95))
        hidden_until(ob, t0, rest, up)
        key(ob, t0 + 0.12 * dur, loc=rest, rot=up, scale=1.0)
        key(ob, t0 + 0.80 * dur, loc=rest, rot=Quaternion(axis, -side * math.radians(4)), scale=1.0)
        key(ob, t0 + 0.92 * dur, loc=rest, rot=Quaternion(axis, side * math.radians(2)), scale=1.0)
        rest_key(ob, t1)
    elif cat in ('sandbag', 'prop'):
        # thrown off the truck bed: ballistic arc with a tumble, lands tilted and settles
        spins = (1.0 + random.random()) if cat == 'sandbag' else (0.5 + random.random() * 0.7)
        tumble = random_axis() * (math.tau * spins)
        start = TRUCK_BED + Vector((0, 0, random.uniform(0.0, 0.4)))
        arc = 2.2 + 2.0 * random.random() if cat == 'sandbag' else 1.5 + 1.5 * random.random()
        e0 = Euler(tumble)
        hidden_until(ob, t0, start, e0)
        key(ob, t0 + 0.05 * dur, loc=start, rot=e0, scale=0.7)
        mid = (start + rest) / 2 + Vector((0, 0, arc))
        key(ob, t0 + 0.45 * dur, loc=mid, rot=Euler(tumble * 0.45), scale=1.0)
        tilt = Quaternion(random_axis().cross(Vector((0, 0, 1))).normalized(), math.radians(random.uniform(8, 16)))
        key(ob, t0 + 0.82 * dur, loc=rest + Vector((0, 0, 0.05)), rot=tilt, scale=1.0)
        key(ob, t0 + 0.91 * dur, loc=rest + Vector((0, 0, 0.12)), rot=tilt.slerp(Quaternion(), 0.6), scale=1.0)
        rest_key(ob, t1)
    elif cat == 'structure':
        # craned off the truck: lifted, swung over with a yaw, lowered with a pendulum sway
        lift = TRUCK_BED + Vector((0, 0, 3.0))
        yaw = Quaternion((0, 0, 1), random.choice((-1, 1)) * math.radians(random.uniform(60, 110)))
        hidden_until(ob, t0, TRUCK_BED, yaw)
        key(ob, t0 + 0.08 * dur, loc=TRUCK_BED, rot=yaw, scale=0.35)
        key(ob, t0 + 0.30 * dur, loc=lift, rot=yaw, scale=1.0)
        sway_axis = (rest - TRUCK_BED).cross(Vector((0, 0, 1)))
        sway_axis = sway_axis.normalized() if sway_axis.length > 1e-3 else Vector((1, 0, 0))
        key(ob, t0 + 0.60 * dur, loc=rest + Vector((0, 0, 4.0)), rot=Quaternion(sway_axis, math.radians(7)) @ yaw.slerp(Quaternion(), 0.8), scale=1.0)
        key(ob, t0 + 0.80 * dur, loc=rest + Vector((0, 0, 0.6)), rot=Quaternion(sway_axis, math.radians(-4)), scale=1.0)
        key(ob, t0 + 0.92 * dur, loc=rest + Vector((0, 0, 0.05)), rot=Quaternion(sway_axis, math.radians(1.5)), scale=1.0)
        rest_key(ob, t1)
    elif cat in ('mast', 'antenna'):
        # lies flat on the ground (away from the truck), then hinges up on its base with an overshoot
        away = (rest - TRUCK_BED)
        away.z = 0
        away = away.normalized() if away.length > 1e-3 else Vector((-1, 0, 0))
        axis = Vector((0, 0, 1)).cross(away).normalized()  # rotating +90 about this tips +Z towards `away`
        flat = Quaternion(axis, math.radians(90))
        start = t0 if cat == 'mast' else t0 - 0.04
        hidden_until(ob, start, rest, flat)
        key(ob, start + 0.10 * dur, loc=rest, rot=flat, scale=1.0)
        key(ob, start + 0.25 * dur, loc=rest, rot=flat, scale=1.0)
        key(ob, start + 0.78 * dur, loc=rest, rot=Quaternion(axis, math.radians(-5)), scale=1.0)
        key(ob, start + 0.90 * dur, loc=rest, rot=Quaternion(axis, math.radians(2)), scale=1.0)
        rest_key(ob, min(start + dur, 1.0))
    # hold the rest pose to the end so nothing drifts after its landing
    rest_key(ob, 1.0)

if 'post' in CHOREO:
    CHOREO['post']()

scene = bpy.context.scene
scene.frame_start, scene.frame_end = FRAME_START, FRAME_END
scene.render.fps = 15
scene['deploy_frame_start'], scene['deploy_frame_end'] = FRAME_START, FRAME_END
json.dump(data, open(table.replace('.json', '_vat.json'), 'w'))
bpy.ops.wm.save_as_mainfile(filepath=dst)
print('saved', dst, 'objects', len(objects))
