"""Ural Shield wall kit - shared constants, atlas layout and mesh builder.

All modelling is done in METRES in these helpers; meshes are written to Blender in
CENTIMETRES (1 Blender unit = 1 cm) so the FBX arrives in Unreal 1:1.

Axes: wall runs along Y, outer (western) face toward -X, Z up, pivot on the ground.
"""
import os
import math
import random
import bpy
import bmesh
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
OUT = os.path.join(PROJECT, 'Saved', 'IntroCinematic', 'import', 'UralShield')
WORK = os.path.join(OUT, 'work')
BLEND = os.path.join(WORK, 'UralShield_kit.blend')
STATS = os.path.join(WORK, 'kit_stats.json')
MAT_NAME = 'M_UralShield'
TEX = {k: os.path.join(OUT, 'T_UralShield_%s.png' % k) for k in ('BaseColor', 'Normal', 'ORM')}

CM = 100.0
MODULE = 60.0          # tiling length along Y (m)
HALF = MODULE / 2.0
S = 4096               # atlas size (px)
PPM = S / MODULE       # px per metre of the full-width tiling bands (68.27)

# gate constants (metres, GateFrame space)
PORTAL_HALF = 8.0
PORTAL_H = 13.0
HINGE_X = -5.0         # closed outer face of the leaves / hinge line X
HINGE_Y = 7.95         # +Y for leaf L, -Y for leaf R
LEAF_W = 7.93
LEAF_T = 1.2
LEAF_Z0, LEAF_Z1 = 0.05, 12.95


class Region:
    """A rectangle of the atlas. tile=True regions span the full width and repeat in U."""

    def __init__(self, name, x0, y0, w, h, ppm, tile=False, rot=False, fit=False):
        self.name, self.x0, self.y0, self.w, self.h = name, x0, y0, w, h
        self.ppm, self.tile, self.rot, self.fit = ppm, tile, rot, fit

    @property
    def size_m(self):
        """(s extent, t extent) in metres of the upright patch."""
        if self.rot:
            return self.h / self.ppm, self.w / self.ppm
        return self.w / self.ppm, self.h / self.ppm


R_LEAF = Region('LEAF', 0, 0, 1300, 800, 100.0, rot=True)       # 8 m x 13 m leaf plate, laid sideways
R_TRIM = Region('TRIM', 1310, 0, 1536, 832, 64.0)               # plain iron, 24 m x 13 m
R_DARK = Region('DARK', 2856, 0, 128, 128, 64.0, fit=True)      # embrasure interior
R_LENS = Region('LENS', 2994, 0, 128, 128, 64.0, fit=True)      # floodlight glass
R_HOOD = Region('HOOD', 3132, 0, 256, 200, 64.0, fit=True)      # far-LOD casemate front (frame + dark slot)
R_PARA = Region('PARA', 0, 848, S, 320, PPM, tile=True)         # iron parapet cladding, 4.69 m
R_IRON = Region('IRON', 0, 1176, S, 410, PPM, tile=True)        # riveted iron band, 6.0 m
R_GEN = Region('GEN', 0, 1594, S, 410, PPM, tile=True)          # generic concrete lift, 6.0 m
R_TALL = Region('TALL', 0, 2012, S, 2048, PPM, tile=True)       # main concrete face, 30 m
MISC_ROW_H = 848
REGIONS = [R_LEAF, R_TRIM, R_DARK, R_LENS, R_HOOD, R_PARA, R_IRON, R_GEN, R_TALL]

ZUP = Vector((0, 0, 1))
_rng = random.Random(1947)


def newell(pts):
    n = Vector((0, 0, 0))
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        n.x += (p.y - q.y) * (p.z + q.z)
        n.y += (p.z - q.z) * (p.x + q.x)
        n.z += (p.x - q.x) * (p.y + q.y)
    return n


class MB:
    """Mesh builder: every face carries its own planar, true-scale UV projection."""

    def __init__(self, name):
        self.name = name
        self.verts, self.faces, self.uvs, self.smooth = [], [], [], []
        self.uv_errors = []

    # ---- UV projection -------------------------------------------------
    def _project(self, pts, n, region, v=None, uoff=0.0, ref=None):
        if abs(n.z) > 0.999:
            # horizontal face: U runs along its longest edge (so long strips tile in U)
            best, ua = -1.0, Vector((0, 1, 0))
            for i, p in enumerate(pts):
                e = pts[(i + 1) % len(pts)] - p
                if e.length > best * 1.02:
                    best, ua = e.length, e.normalized()
            if ua.y < -1e-6 or (abs(ua.y) <= 1e-6 and ua.x < 0):
                ua = -ua
        else:
            ua = ZUP.cross(n).normalized()
        va = n.cross(ua).normalized()
        steep = abs(n.z) < 0.2
        ss = [p.dot(ua) for p in pts]
        ts = [p.z for p in pts] if steep else [p.dot(va) for p in pts]
        if region.fit:
            s0, t0 = min(ss), min(ts)
            ext = max(max(ss) - s0, max(ts) - t0, 1e-6)
            k = (region.w - 16) / ext
            cx = region.x0 + region.w / 2 - (max(ss) - s0) * k / 2
            cy = region.y0 + region.h / 2 - (max(ts) - t0) * k / 2
            return [((cx + (s - s0) * k) / S, (cy + (t - t0) * k) / S) for s, t in zip(ss, ts)]
        if region.tile:
            tmin, tmax = min(ts), max(ts)
            if v is None:
                if region is R_TALL:
                    v0 = 0.0 if steep else tmin - min(p.z for p in pts)
                else:
                    v0 = tmin
            elif v[0] == 'abs':
                v0 = v[1] if steep else tmin - (min(p.z for p in pts) - v[1])
            else:  # ('off', metres above band bottom)
                v0 = tmin - v[1]
            hm = region.h / region.ppm
            if not steep and (tmin - v0 < -0.02 or tmax - v0 > hm + 0.02) and tmax - tmin <= hm:
                v0 = tmin       # flat face that does not fit at the requested offset: start at band bottom
            if tmin - v0 < -0.02 or tmax - v0 > hm + 0.02:
                self.uv_errors.append('%s: face t %.2f..%.2f outside %s (%.2f m)' % (
                    self.name, tmin - v0, tmax - v0, region.name, hm))
            return [(s * region.ppm / S + uoff,
                     (region.y0 + min(max(t - v0, 0.0), hm) * region.ppm) / S) for s, t in zip(ss, ts)]
        # non tiling patch
        sm, tm = region.size_m
        if ref is None:
            s0, t0 = min(ss), min(ts)
            es, et = max(ss) - s0, max(ts) - t0
            s0 -= _rng.random() * max(sm - es, 0.0)
            t0 -= _rng.random() * max(tm - et, 0.0)
        else:
            s0, t0 = ref
        out = []
        for s, t in zip(ss, ts):
            a, b = s - s0, t - t0
            if a < -0.02 or b < -0.02 or a > sm + 0.02 or b > tm + 0.02:
                self.uv_errors.append('%s: face outside patch %s (%.2f,%.2f)' % (self.name, region.name, a, b))
            a = min(max(a, 0.0), sm)
            b = min(max(b, 0.0), tm)
            if region.rot:
                px, py = region.x0 + b * region.ppm, region.y0 + (sm - a) * region.ppm
            else:
                px, py = region.x0 + a * region.ppm, region.y0 + b * region.ppm
            out.append((px / S, py / S))
        return out

    # ---- primitives ----------------------------------------------------
    def face(self, pts, region, hint=None, v=None, uoff=0.0, ref=None, smooth=False):
        pts = [Vector(p) for p in pts]
        clean = []
        for p in pts:
            if not clean or (p - clean[-1]).length > 1e-5:
                clean.append(p)
        if len(clean) > 1 and (clean[0] - clean[-1]).length <= 1e-5:
            clean.pop()
        if len(clean) < 3:
            return
        n = newell(clean)
        if n.length < 1e-4:      # zero-area (collinear) face
            return
        n.normalize()
        if hint is not None and n.dot(Vector(hint)) < 0:
            clean.reverse()
            n = -n
        uv = self._project(clean, n, region, v, uoff, ref)
        base = len(self.verts)
        self.verts.extend(clean)
        self.faces.append(list(range(base, base + len(clean))))
        self.uvs.append(uv)
        self.smooth.append(smooth)

    def profile(self, pts, specs, y0, y1, uoff=0.0):
        """Extrude an XZ polyline along Y. Path runs up the west side, over the top, down the east.
        specs[i] = (region, v) for the segment pts[i] -> pts[i+1]; region None skips it."""
        for i in range(len(pts) - 1):
            reg, v = specs[i]
            if reg is None:
                continue
            (xa, za), (xb, zb) = pts[i], pts[i + 1]
            dx, dz = xb - xa, zb - za
            hint = (-dz, 0, dx)
            self.face([(xa, y0, za), (xa, y1, za), (xb, y1, zb), (xb, y0, zb)], reg, hint, v, uoff)

    def box(self, lo, hi, region, skip='', v=None, uoff=0.0, regions=None, ref=None):
        """Axis aligned box. skip: string containing face keys like '-x+z'. regions: per-face override."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        fs = {
            '-x': ([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0)),
            '+x': ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
            '-y': ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
            '+y': ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)),
            '-z': ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1)),
            '+z': ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        }
        for k, (pts, n) in fs.items():
            if k in skip:
                continue
            reg = (regions or {}).get(k, region)
            self.face(pts, reg, n, v, uoff, ref)

    def loft(self, rings, specs, closed=True, uoff=0.0, flip=False, smooth=False, centers=None):
        """Quads between consecutive rings (lists of 3D points, equal length).
        specs[i] = (region, v) for the interval rings[i] -> rings[i+1]."""
        for i in range(len(rings) - 1):
            reg, v = specs[i]
            if reg is None:
                continue
            a, b = rings[i], rings[i + 1]
            n = len(a)
            if centers:
                c = Vector(centers[i])
            else:
                c = Vector((0, 0, 0))
                for p in a + b:
                    c += Vector(p)
                c /= (2 * n)
            rng = range(n) if closed else range(n - 1)
            for j in rng:
                k = (j + 1) % n
                quad = [Vector(a[j]), Vector(a[k]), Vector(b[k]), Vector(b[j])]
                fc = (quad[0] + quad[1] + quad[2] + quad[3]) / 4
                nn = newell(quad)
                # outward = away from the loft axis, judged in the plane of the face normal
                hint = fc - c
                if nn.length > 1e-9:
                    nn.normalize()
                    if abs(nn.z) > 0.9:
                        # horizontal step between two rings: a widening step faces down, a narrowing one up
                        ma, mb_ = (quad[0] + quad[1]) / 2 - c, (quad[3] + quad[2]) / 2 - c
                        grow = Vector((mb_.x, mb_.y, 0)).length - Vector((ma.x, ma.y, 0)).length
                        hint = Vector((0, 0, -1.0 if grow > 0 else 1.0))
                    else:
                        hint = nn if nn.dot(hint) >= 0 else -nn
                if flip:
                    hint = -hint
                self.face(quad, reg, hint, v, uoff, smooth=smooth)

    def cylinder(self, cx, cy, z0, z1, r, segs, region, dome=0.0, rings=3, smooth=True, cap=True, r1=None, bottom=False):
        r1 = r if r1 is None else r1

        def ring(z, rad):
            return [(cx + rad * math.cos(2 * math.pi * i / segs), cy + rad * math.sin(2 * math.pi * i / segs), z)
                    for i in range(segs)]
        rs = [ring(z0, r), ring(z1, r1)]
        if dome > 0:
            for i in range(1, rings):
                a = (math.pi / 2) * i / rings
                rs.append(ring(z1 + dome * math.sin(a), r1 * math.cos(a)))
        if bottom:
            self.face(rs[0], region, (0, 0, -1))
        self.loft(rs, [(region, None)] * (len(rs) - 1), smooth=smooth,
                  centers=[(cx, cy, (rs[i][0][2] + rs[i + 1][0][2]) / 2 - (0.6 * r if i > 0 else 0)) for i in range(len(rs) - 1)])
        last = rs[-1]
        top = (cx, cy, z1 + dome) if dome > 0 else None
        if top:
            for i in range(segs):
                self.face([last[i], last[(i + 1) % segs], top], region, (0, 0, 1), smooth=smooth)
        elif cap:
            self.face(last, region, (0, 0, 1))

    # ---- output --------------------------------------------------------
    def build(self, origin=(0, 0, 0)):
        if self.uv_errors:
            raise RuntimeError('UV errors:\n' + '\n'.join(self.uv_errors[:20]))
        o = Vector(origin)
        me = bpy.data.meshes.new(self.name)
        me.from_pydata([tuple((p - o) * CM) for p in self.verts], [], self.faces)
        uvl = me.uv_layers.new(name='UVMap')
        i = 0
        for f, uv in zip(self.faces, self.uvs):
            for c in uv:
                uvl.data[i].uv = c
                i += 1
        for poly, sm in zip(me.polygons, self.smooth):
            poly.use_smooth = sm
        me.update()
        constructed = [tuple(p.normal) for p in me.polygons]
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.05)
        bmesh.ops.triangulate(bm, faces=bm.faces, quad_method='FIXED', ngon_method='EAR_CLIP')
        # Winding is outward by construction (every face is oriented against an explicit
        # outward hint). Cross-check with Blender's own recalc: normal_update() first (stale
        # normals after edits were a past bug), then compare. The kit has open tiling ends and
        # interpenetrating shells, where recalc is only a heuristic, so disagreements are
        # logged and the constructed winding is kept; the backface debug render is the judge.
        for f in bm.faces:
            f.normal_update()
        before = [f.normal.copy() for f in bm.faces]
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        for f in bm.faces:
            f.normal_update()
        flipped = [f for f, n0 in zip(bm.faces, before) if f.normal.dot(n0) < 0]
        self.recalc_disagree = len(flipped)
        if flipped:
            bmesh.ops.reverse_faces(bm, faces=flipped)
            for f in bm.faces:
                f.normal_update()
        print('NORMALS %s: recalc disagrees on %d of %d tris' % (self.name, len(flipped), len(bm.faces)))
        bm.to_mesh(me)
        bm.free()
        me.use_auto_smooth = True
        me.auto_smooth_angle = math.radians(50)
        me.update()
        mat = bpy.data.materials.get(MAT_NAME)
        if mat:
            me.materials.append(mat)
        ob = bpy.data.objects.new(self.name, me)
        ob.location = o * CM
        bpy.context.scene.collection.objects.link(ob)
        return ob


def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1.0     # data is authored in cm, exported with global_scale 0.01
    return sc


def make_material(flip_green=True):
    """Preview material using the baked atlas. Normal map is DirectX, so green is flipped for Blender."""
    mat = bpy.data.materials.get(MAT_NAME) or bpy.data.materials.new(MAT_NAME)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    if not all(os.path.exists(p) for p in TEX.values()):
        bsdf.inputs['Base Color'].default_value = (0.4, 0.4, 0.4, 1)
        return mat

    def img(key, cs):
        n = nt.nodes.new('ShaderNodeTexImage')
        n.image = bpy.data.images.load(TEX[key], check_existing=True)
        n.image.colorspace_settings.name = cs
        return n
    bc, nm, orm = img('BaseColor', 'sRGB'), img('Normal', 'Non-Color'), img('ORM', 'Non-Color')
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
    mix = nt.nodes.new('ShaderNodeMix')
    mix.data_type = 'RGBA'
    mix.blend_type = 'MULTIPLY'
    mix.inputs[0].default_value = 1.0
    nt.links.new(bc.outputs['Color'], mix.inputs[6])
    nt.links.new(sep.outputs['Red'], mix.inputs[7])
    nt.links.new(mix.outputs[2], bsdf.inputs['Base Color'])
    nt.links.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    nt.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    sn = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(nm.outputs['Color'], sn.inputs['Color'])
    cn = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(sn.outputs['Red'], cn.inputs['Red'])
    nt.links.new(sn.outputs['Blue'], cn.inputs['Blue'])
    if flip_green:
        inv = nt.nodes.new('ShaderNodeMath')
        inv.operation = 'SUBTRACT'
        inv.inputs[0].default_value = 1.0
        nt.links.new(sn.outputs['Green'], inv.inputs[1])
        nt.links.new(inv.outputs[0], cn.inputs['Green'])
    else:
        nt.links.new(sn.outputs['Green'], cn.inputs['Green'])
    nmap = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(cn.outputs['Color'], nmap.inputs['Color'])
    nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    return mat


def tri_count(ob):
    ob.data.calc_loop_triangles()
    return len(ob.data.loop_triangles)
