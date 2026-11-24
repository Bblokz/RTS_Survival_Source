"""Dead thistle clumps (grass concept D, "Distelhorst") for the Ger_IntroMission foliage pass. Runs in Blender 5:

Blender5.0.exe -b --factory-startup --python thistle_blender.py -- <out_dir>

Writes <out_dir>/SM_DeadThistle_A.fbx, _B, _C (intact clumps, 0.6-1.0 m) and SM_DeadThistle_Crushed_A..C (the same clump
laid flat by a track), plus preview_top.png / preview_side.png. Opaque low-poly geometry with vertex colours (the UE
material is VertexColor x Tint), pivot on the ground, +Z up, metres (FBX export scales to cm).

A clump is 2-5 plants: a dark basal rosette of dead lobed spiky leaves (the top-down mass the RTS camera actually sees),
1-3 grey-brown stems per plant branching near the top, each branch ending in a dead head: a brown spiky involucre ball,
most of them burst into a pale pappus tuft (the pale dots that read from above).
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.dirname(os.path.abspath(__file__))
os.makedirs(OUT, exist_ok=True)

LEAF = (128, 110, 92)
LEAF_DARK = (104, 88, 74)
STEM = (140, 122, 104)
HEAD = (118, 94, 70)
PAPPUS = (196, 186, 168)
PAPPUS_DIM = (178, 164, 140)


def lin(c, jit=0.0, rnd=None):
    """sRGB 0-255 -> linear 0-1 (+- jit in sRGB units)."""
    out = []
    for v in c:
        if rnd is not None and jit:
            v = v + rnd.uniform(-jit, jit)
        v = max(0.0, min(255.0, v)) / 255.0
        out.append(v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4)
    return out


class Builder:
    def __init__(self):
        self.bm = bmesh.new()
        self.col = self.bm.loops.layers.color.new('Col')

    def face(self, verts, colour):
        vs = [self.bm.verts.new(v) for v in verts]
        f = self.bm.faces.new(vs)
        for loop in f.loops:
            loop[self.col] = (*colour, 1.0)
        return f

    def tube(self, pts, radii, colour, sides=5):
        """Tapered tube along pts (list of Vector), closed at the tip."""
        rings = []
        for k, p in enumerate(pts):
            d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
            a = d.orthogonal().normalized()
            b = d.cross(a).normalized()
            ring = []
            for s in range(sides):
                t = 2 * math.pi * s / sides
                ring.append(self.bm.verts.new(p + (a * math.cos(t) + b * math.sin(t)) * radii[k]))
            rings.append(ring)
        for r0, r1 in zip(rings[:-1], rings[1:]):
            for s in range(sides):
                f = self.bm.faces.new((r0[s], r0[(s + 1) % sides], r1[(s + 1) % sides], r1[s]))
                for loop in f.loops:
                    loop[self.col] = (*colour, 1.0)
        tip = self.bm.verts.new(pts[-1] + (pts[-1] - pts[-2]).normalized() * radii[-1])
        for s in range(sides):
            f = self.bm.faces.new((rings[-1][s], rings[-1][(s + 1) % sides], tip))
            for loop in f.loops:
                loop[self.col] = (*colour, 1.0)

    def blob(self, c, r, colour, spikes=0, spike_len=0.0, rnd=None, flat=1.0, segs=6, rings=4):
        """Low-poly ellipsoid (z scaled by flat) with optional spiky bract cones."""
        vs = []
        top = self.bm.verts.new(c + Vector((0, 0, r * flat)))
        bot = self.bm.verts.new(c - Vector((0, 0, r * flat)))
        for i in range(1, rings):
            phi = math.pi * i / rings
            ring = []
            for s in range(segs):
                t = 2 * math.pi * (s + 0.5 * (i % 2)) / segs
                ring.append(self.bm.verts.new(c + Vector((r * math.sin(phi) * math.cos(t), r * math.sin(phi) * math.sin(t),
                                                          r * flat * math.cos(phi)))))
            vs.append(ring)
        faces = []
        for s in range(segs):
            faces.append((top, vs[0][s], vs[0][(s + 1) % segs]))
            faces.append((bot, vs[-1][(s + 1) % segs], vs[-1][s]))
        for r0, r1 in zip(vs[:-1], vs[1:]):
            for s in range(segs):
                faces.append((r0[s], r1[s], r1[(s + 1) % segs], r0[(s + 1) % segs]))
        for fv in faces:
            f = self.bm.faces.new(fv)
            for loop in f.loops:
                loop[self.col] = (*colour, 1.0)
        for _ in range(spikes):
            d = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.6, 0.8))).normalized()
            base = c + Vector((d.x * r, d.y * r, d.z * r * flat))
            a = d.orthogonal().normalized() * r * 0.25
            b = d.cross(a).normalized() * r * 0.25
            tipv = base + d * spike_len
            q = [self.bm.verts.new(base + a), self.bm.verts.new(base + b), self.bm.verts.new(base - a),
                 self.bm.verts.new(tipv)]
            for fv in ((q[0], q[1], q[3]), (q[1], q[2], q[3]), (q[2], q[0], q[3])):
                f = self.bm.faces.new(fv)
                for loop in f.loops:
                    loop[self.col] = (*colour, 1.0)

    def leaf(self, base, yaw, length, width, droop, curl, colour, rnd, lobes=5):
        """Dead pinnatifid thistle leaf: a centre rib strip with spiky lobes either side, arching up then down."""
        dirv = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        side = Vector((-math.sin(yaw), math.cos(yaw), 0.0))
        n = lobes * 2 + 1

        def spine(t):
            z = math.sin(t * math.pi) * droop * (1.0 - 0.6 * t) + 0.01
            return base + dirv * (t * length) + Vector((0, 0, z))

        prev_l = prev_r = None
        prev_c = spine(0.0)
        for k in range(1, n + 1):
            t = k / n
            c = spine(t)
            w = width * math.sin(t * math.pi) ** 0.7 * (1.15 if k % 2 else 0.45)    # lobes alternate wide / notch
            cz = Vector((0, 0, curl * w))          # edges curl up (dry leaf)
            l, r = c + side * w + cz, c - side * w + cz
            if prev_l is not None:
                cl = (colour[0] * rnd.uniform(0.85, 1.1), colour[1] * rnd.uniform(0.85, 1.1), colour[2] * rnd.uniform(0.85, 1.1))
                self.face((prev_c, c, l, prev_l), cl)
                self.face((prev_c, prev_r, r, c), cl)
                if k % 2:                           # spike at each lobe tip
                    sp = side * (w + length * 0.06)
                    self.face((l, l + sp * 0.3 + dirv * 0.012, l + sp * 0.25 - dirv * 0.012), cl)
                    self.face((r, r - sp * 0.25 - dirv * 0.012, r - sp * 0.3 + dirv * 0.012), cl)
            else:
                self.face((prev_c, c, l), colour)
                self.face((prev_c, r, c), colour)
            prev_l, prev_r, prev_c = l, r, c

    def done(self, name):
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def plant(b, root, rnd, height):
    # basal rosette
    nl = rnd.randint(6, 9)
    y0 = rnd.uniform(0, 2 * math.pi)
    for k in range(nl):
        yaw = y0 + 2 * math.pi * k / nl + rnd.uniform(-0.25, 0.25)
        b.leaf(root + Vector((0, 0, 0.0)), yaw, rnd.uniform(0.22, 0.36), rnd.uniform(0.045, 0.07), rnd.uniform(0.03, 0.09),
               rnd.uniform(0.2, 0.6), lin(LEAF if k % 3 else LEAF_DARK, 10, rnd), rnd)
    # stems
    for s in range(rnd.randint(1, 3)):
        h = height * rnd.uniform(0.75, 1.0)
        lean = Vector((rnd.uniform(-0.12, 0.12), rnd.uniform(-0.12, 0.12), 0.0))
        bend = Vector((rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06), 0.0))
        pts = []
        for k in range(6):
            t = k / 5
            pts.append(root + Vector((rnd.uniform(-0.02, 0.02), rnd.uniform(-0.02, 0.02), 0)) * (k == 0) + lean * t * h
                       + bend * (t * t) * h + Vector((0, 0, t * h)))
        r0 = rnd.uniform(0.008, 0.012)
        b.tube(pts, [r0 * (1.0 - 0.55 * k / 5) for k in range(6)], lin(STEM, 12, rnd))
        # a few withered stem leaves hanging down
        for k in range(rnd.randint(1, 3)):
            p = pts[rnd.randint(1, 3)]
            b.leaf(p, rnd.uniform(0, 2 * math.pi), rnd.uniform(0.08, 0.14), 0.025, -0.06, 0.4, lin(LEAF_DARK, 10, rnd), rnd,
                   lobes=3)
        # branches + heads
        top = pts[-1]
        stem_dir = (pts[-1] - pts[-2]).normalized()
        for k in range(rnd.randint(2, 5)):
            yaw = rnd.uniform(0, 2 * math.pi)
            out = Vector((math.cos(yaw), math.sin(yaw), 0)) * rnd.uniform(0.06, 0.16)
            start = pts[-2] + (top - pts[-2]) * rnd.uniform(0.2, 0.9)
            end = start + out + stem_dir * rnd.uniform(0.05, 0.16)
            mid = (start + end) * 0.5 + Vector((0, 0, 0.02))
            b.tube([start, mid, end], [0.006, 0.005, 0.004], lin(STEM, 12, rnd), sides=4)
            head(b, end, rnd)
        head(b, top + stem_dir * 0.02, rnd)


def head(b, c, rnd):
    r = rnd.uniform(0.018, 0.028)
    b.blob(c, r, lin(HEAD, 14, rnd), spikes=rnd.randint(6, 10), spike_len=r * 0.9, rnd=rnd, flat=1.1)
    if rnd.random() < 0.75:                         # burst head: pale pappus tuft on top
        pr = rnd.uniform(0.03, 0.045)
        b.blob(c + Vector((0, 0, r * 0.9 + pr * 0.35)), pr, lin(PAPPUS if rnd.random() < 0.6 else PAPPUS_DIM, 8, rnd),
               flat=0.55, segs=7, rings=3)


def clump(name, seed, n_plants, spread, height):
    rnd = random.Random(seed)
    b = Builder()
    roots = []
    for k in range(n_plants):
        for _ in range(30):
            a, d = rnd.uniform(0, 2 * math.pi), spread * math.sqrt(rnd.random())
            p = Vector((math.cos(a) * d, math.sin(a) * d, 0))
            if all((p - q).length > 0.22 for q in roots):
                break
        roots.append(p)
        plant(b, p, rnd, height * rnd.uniform(0.8, 1.05))
    return b.done(name)


def crush(src, name, seed):
    """Lay the clump flat: everything bends over in one track direction, heads end up on the ground."""
    rnd = random.Random(seed)
    obj = src.copy()
    obj.data = src.data.copy()
    obj.name = name
    obj.data.name = name
    bpy.context.scene.collection.objects.link(obj)
    th = rnd.uniform(0, 2 * math.pi)
    d = Vector((math.cos(th), math.sin(th), 0))
    for v in obj.data.vertices:
        z = v.co.z
        wob = math.sin(v.co.x * 23.0) * 0.02 + math.cos(v.co.y * 19.0) * 0.02
        v.co = Vector((v.co.x + d.x * z * 0.95 + wob, v.co.y + d.y * z * 0.95 - wob, 0.012 + z * 0.05))
    # flattened plants are darker (bruised, dusted)
    col = obj.data.color_attributes['Col']
    for c in col.data:
        r, g, bb, a = c.color
        c.color = (r * 0.8, g * 0.78, bb * 0.76, a)
    return obj


def export(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', colors_type='LINEAR',
                             add_leaf_bones=False, bake_space_transform=True)


def preview(objs):
    scn = bpy.context.scene
    scn.render.engine = 'BLENDER_EEVEE'
    scn.render.resolution_x, scn.render.resolution_y = 1500, 500
    mat = bpy.data.materials.new('VC')
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes['Principled BSDF']
    attr = nt.nodes.new('ShaderNodeVertexColor')
    attr.layer_name = 'Col'
    nt.links.new(attr.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = 0.9
    ground = bpy.data.materials.new('Sand')
    ground.diffuse_color = (0.30, 0.13, 0.06, 1)
    ground.use_nodes = True
    ground.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.30, 0.13, 0.06, 1)
    bpy.ops.mesh.primitive_plane_add(size=40)
    bpy.context.object.data.materials.append(ground)
    for k, o in enumerate(objs):
        o.data.materials.append(mat)
        o.location = Vector(((k - (len(objs) - 1) / 2) * 2.2, 0, 0))
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(40), 0, math.radians(30))
    scn.collection.objects.link(sun)
    scn.world = bpy.data.worlds.new('W')
    scn.world.color = (0.4, 0.4, 0.45)
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 2.2 * len(objs)
    scn.collection.objects.link(cam)
    scn.camera = cam
    for view, loc, rot in (('top', (0, 0, 20), (0, 0, 0)), ('side', (0, -20, 0.5), (math.radians(90), 0, 0))):
        cam.location = loc
        cam.rotation_euler = rot
        scn.render.filepath = os.path.join(OUT, 'preview_%s.png' % view)
        bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
specs = [('A', 11, 3, 0.30, 0.95), ('B', 23, 2, 0.22, 0.75), ('C', 37, 5, 0.42, 0.85)]
intact, crushed = [], []
for key, seed, n, spread, h in specs:
    o = clump('SM_DeadThistle_' + key, seed, n, spread, h)
    intact.append(o)
    crushed.append(crush(o, 'SM_DeadThistle_Crushed_' + key, seed + 1))
for o in intact + crushed:
    export(o, os.path.join(OUT, o.name + '.fbx'))
    print('THISTLE', o.name, 'tris', sum(len(p.vertices) - 2 for p in o.data.polygons),
          'dims', [round(v, 2) for v in o.dimensions])
preview(intact + crushed)
print('THISTLE_DONE', OUT)
