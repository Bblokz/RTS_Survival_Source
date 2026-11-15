"""Soviet wall gates A (twin swing leaves) and C (lift portal): game meshes + baked rigid VAT (Blender 4.0, headless).

blender -b --factory-startup -P build_gates.py -- <out_dir> [--preview]

Writes per gate into <out_dir>:
  SM_RusWallGate<X>_Frame.fbx   static part (pillars / towers), slots 0 Concrete, 1 GateSteel
  SM_RusWallGate<X>_Leaf.fbx    moving part as one mesh; UV2 = (piece u, pivot.x), UV3 = (pivot.y, pivot.z), the
                                nomadic vat_bake.py layout moved down two sets; MF_WallGate_VAT plays it
  T_RusWallGate<X>_VAT_Offset.exr / _Rot.exr   width = pieces, height = FRAMES
  gate<X>.json                  pieces, pivots, frames, bounds (for the Unreal side)
--preview renders workbench stills at progress 1 / 0.66 / 0.33 / 0 instead (preview_<X>_<p>.png).

Conventions (same texture layout as the nomadic VAT, own material function MF_WallGate_VAT):
  progress = custom primitive data 3; 1.0 = CLOSED = rest pose (zero offset), 0.0 = fully OPEN.
  Unreal mesh space, cm: the wall runs along X, the gate's +X end is the origin (like SM_Wall_14, X -2260..0),
  wall centre plane Y = -10 (the slab is Y -20..0). Outside (stars, enemy side) = -Y, inside = +Y.
Geometry is authored here in Unreal cm and converted to Blender metres (Blender Y = -Unreal Y).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ARGS = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(ARGS[0])
PREVIEW = '--preview' in ARGS
FRAMES = 61
UV_CONCRETE_CM = 760.0  # SM_Wall_14: one UV unit is 7.6 m

# Atlas regions (must match make_gate_atlas.py), Blender UV space (v up).
REGIONS = {
    'plate': (0.0, 0.5, 0.5, 1.0),
    'roundel': (0.5, 0.75, 0.75, 1.0),
    'dark': (0.75, 0.75, 1.0, 1.0),
    'tube': (0.5, 0.5, 0.75, 0.75),
    'lamp': (0.75, 0.625, 0.875, 0.75),
    'rust': (0.875, 0.625, 1.0, 0.75),
}
MODES = ['concrete'] + list(REGIONS)
SLOT = {'concrete': 0}  # everything else is slot 1 (GateSteel)


def B(v):
    """Unreal cm -> Blender m."""
    return Vector((v[0] / 100.0, -v[1] / 100.0, v[2] / 100.0))


def U(v):
    """Blender m -> Unreal cm."""
    return Vector((v[0] * 100.0, -v[1] * 100.0, v[2] * 100.0))


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def quat_axis_angle(axis, angle):
    """(x, y, z, w) rotating numerically by angle about axis (right-hand rule on the raw numbers), as the
    VAT shader applies it: v' = v + 2 cross(q, cross(q, v) + w v)."""
    s = math.sin(angle / 2)
    a = Vector(axis).normalized()
    return (a.x * s, a.y * s, a.z * s, math.cos(angle / 2))


def rotate(q, v):
    qv = Vector(q[:3])
    v = Vector(v)
    return v + 2.0 * qv.cross(qv.cross(v) + q[3] * v)


class Builder:
    """Collects primitives (Unreal cm) into one bmesh with per-face mode / piece."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mode = self.bm.faces.layers.int.new('mode')
        self.piece = self.bm.faces.layers.int.new('piece')
        self.cur_piece = -1

    def _tag(self, verts, mode):
        faces = {f for v in verts for f in v.link_faces}
        for f in faces:
            f[self.mode] = MODES.index(mode)
            f[self.piece] = self.cur_piece
            f.material_index = SLOT.get(mode, 1)
        return faces

    def _fix_winding(self, verts):
        """B() mirrors Y (Unreal -> Blender), which turns the primitives' outward winding inward; recalc outward.
        recalc_face_normals reads the cached face normals, so refresh them after moving the verts first."""
        faces = list({f for v in verts for f in v.link_faces})
        for f in faces:
            f.normal_update()
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)

    def box(self, lo, hi, mode='concrete'):
        lo, hi = Vector(lo), Vector(hi)
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        c = (lo + hi) / 2
        s = hi - lo
        for v in res['verts']:
            v.co = B(Vector((c.x + v.co.x * s.x, c.y + v.co.y * s.y, c.z + v.co.z * s.z)))
        self._fix_winding(res['verts'])
        return self._tag(res['verts'], mode)

    def frustum(self, lo, hi, top_half, mode='concrete'):
        """Box lo..hi whose top face shrinks to +-top_half (x, y) around its centre (pyramid caps)."""
        lo, hi = Vector(lo), Vector(hi)
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        c = (lo + hi) / 2
        s = hi - lo
        for v in res['verts']:
            if v.co.z > 0:
                p = Vector((c.x + math.copysign(top_half[0], v.co.x), c.y + math.copysign(top_half[1], v.co.y), hi.z))
            else:
                p = Vector((c.x + v.co.x * s.x, c.y + v.co.y * s.y, lo.z))
            v.co = B(p)
        self._fix_winding(res['verts'])
        return self._tag(res['verts'], mode)

    def beam(self, a, b, w, h, mode='tube', up=(0, 1, 0)):
        """Square member from a to b (Unreal cm); w across 'up' x dir, h along up."""
        a, b = Vector(a), Vector(b)
        d = (b - a)
        x = d.normalized()
        upv = Vector(up)
        z = x.cross(upv).normalized()
        y = z.cross(x).normalized()
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        c = (a + b) / 2
        for v in res['verts']:
            p = c + x * (v.co.x * d.length) + y * (v.co.y * h) + z * (v.co.z * w)
            v.co = B(p)
        self._fix_winding(res['verts'])
        return self._tag(res['verts'], mode)

    def cyl(self, centre, axis, r, length, mode='dark', segs=16, cap_mode=None):
        """Cylinder whose base centre is 'centre', running 'length' along axis 'x'|'y'|'z' (Unreal)."""
        res = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r, radius2=r,
                                    depth=length)
        for v in res['verts']:
            p = Vector((v.co.x, v.co.y, v.co.z + length / 2))  # local: axis z, 0..length
            if axis == 'x':
                p = Vector((p.z, p.y, p.x))
            elif axis == 'y':
                p = Vector((p.x, p.z, p.y))
            v.co = B(Vector(centre) + p)
        self._fix_winding(res['verts'])
        faces = self._tag(res['verts'], mode)
        if cap_mode:
            for f in faces:
                if len(f.verts) > 4:
                    f[self.mode] = MODES.index(cap_mode)
                    f.material_index = SLOT.get(cap_mode, 1)
        return faces

    def wheel(self, centre, r, thick, spokes=4, mode='dark'):
        """Sheave in the Unreal YZ plane (axle along X): rim of segments, spokes, hub."""
        c = Vector(centre)
        n = 14
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            p0 = c + Vector((0, r * math.cos(a0), r * math.sin(a0)))
            p1 = c + Vector((0, r * math.cos(a1), r * math.sin(a1)))
            self.beam(p0, p1, thick, 7, mode, up=(0, math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2)))
        for i in range(spokes // 2):
            a = math.pi * i / (spokes // 2) + 0.3
            off = Vector((0, r * math.cos(a), r * math.sin(a)))
            self.beam(c - off, c + off, thick * 0.6, 5, mode, up=(1, 0, 0))
        self.cyl(c - Vector((thick, 0, 0)), 'x', 9, thick * 2, mode, segs=10)

    def to_object(self):
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        for slot in ('Concrete', 'GateSteel'):
            mat = bpy.data.materials.get(slot) or bpy.data.materials.new(slot)
            me.materials.append(mat)
        return obj


def uv_project(obj):
    """UV0: concrete faces box-projected at the wall's density; steel faces fitted into their atlas region."""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    mode_l = bm.faces.layers.int['mode']
    uv = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        pts = [U(v.co) for v in f.verts]
        if ax == 0:
            pl = [(p.y, p.z) for p in pts]
        elif ax == 1:
            pl = [(p.x, p.z) for p in pts]
        else:
            pl = [(p.x, p.y) for p in pts]
        mode = MODES[f[mode_l]]
        if mode == 'concrete':
            for loop, (a, b) in zip(f.loops, pl):
                loop[uv].uv = (a / UV_CONCRETE_CM, b / UV_CONCRETE_CM)
            continue
        u0, v0, u1, v1 = REGIONS[mode]
        ins = 0.012 * (u1 - u0)
        amin, amax = min(p[0] for p in pl), max(p[0] for p in pl)
        bmin, bmax = min(p[1] for p in pl), max(p[1] for p in pl)
        for loop, (a, b) in zip(f.loops, pl):
            ta = (a - amin) / max(amax - amin, 1e-6)
            tb = (b - bmin) / max(bmax - bmin, 1e-6)
            loop[uv].uv = (u0 + ins + ta * (u1 - u0 - 2 * ins), v0 + ins + tb * (v1 - v0 - 2 * ins))
    bm.to_mesh(me)
    bm.free()
    # UV1: lightmap channel (non-overlapping)
    lm = me.uv_layers.new(name='Lightmap')
    me.uv_layers.active = lm
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    me.uv_layers.active = me.uv_layers['UVMap']


def add_vat_uvs(obj, pivots):
    """UV2 = (piece u, pivot.x), UV3 = (pivot.y, pivot.z); V stored as 1 - value (FBX flips V).
    UV2/3, not 4/5: the wall masters are also used with skeletal meshes, whose vertex factory takes at most 4 UV
    sets, so a VAT read of TexCoord 4+ fails to compile there (MF_WallGate_VAT reads 2/3)."""
    me = obj.data
    if len(me.uv_layers) != 2:
        raise SystemExit('expected UV0 + lightmap UV1, got %d sets' % len(me.uv_layers))
    uv2 = me.uv_layers.new(name='VatPiece')
    uv3 = me.uv_layers.new(name='VatPivot')
    piece_attr = me.attributes['piece'].data
    width = len(pivots)
    for poly in me.polygons:
        pid = piece_attr[poly.index].value
        u = (pid + 0.5) / width
        px, py, pz = pivots[pid]
        for li in poly.loop_indices:
            uv2.data[li].uv = (u, 1.0 - px)
            uv3.data[li].uv = (py, 1.0 - pz)


def export(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', add_leaf_bones=False,
                             bake_anim=False, axis_forward='-Z', axis_up='Y', use_custom_props=False)
    print('wrote', path)


# ------------------------------------------------------------------------------------------------ gate A
A_HINGE_R, A_HINGE_L = -122.0, -798.0
A_LEAF_Z0, A_LEAF_Z1 = 15.0, 455.0


def gate_a():
    frame = Builder('SM_RusWallGateA_Frame')
    for xa, xb in ((-110.0, 0.0), (-920.0, -810.0)):
        cx = (xa + xb) / 2
        frame.box((cx - 65, -75, 0), (cx + 65, 55, 40))
        frame.frustum((cx - 65, -75, 40), (cx + 65, 55, 50), (58, 63))
        frame.box((cx - 55, -65, 48), (cx + 55, 45, 600))
        for y0, y1 in ((-68, -65), (45, 48)):  # raised panel frame on both faces
            for xs in ((cx - 45, cx - 37), (cx + 37, cx + 45)):
                frame.box((xs[0], y0, 90), (xs[1], y1, 540))
            for zs in ((90, 98), (532, 540)):
                frame.box((cx - 45, y0, zs[0]), (cx + 45, y1, zs[1]))
        frame.box((cx - 70, -80, 600), (cx + 70, 60, 622))
        frame.box((cx - 63, -73, 622), (cx + 63, 53, 636))
        frame.frustum((cx - 55, -65, 636), (cx + 55, 45, 682), (8, 8))
        # lantern on the cap
        frame.cyl((cx, -10, 680), 'z', 5, 25, 'dark', segs=10)
        frame.box((cx - 14, -24, 703), (cx + 14, 4, 708), 'dark')
        frame.box((cx - 12, -22, 708), (cx + 12, 2, 742), 'lamp')
        for dx in (-12, 12):
            for dy in (-22, 2):
                frame.box((cx + dx - 1.5, dy - 1.5, 708), (cx + dx + 1.5, dy + 1.5, 742), 'dark')
        frame.frustum((cx - 17, -27, 742), (cx + 17, 7, 758), (3, 3), 'dark')
    # hinge pintles on the inner pillar faces
    for face_x, axis_x in ((-110.0, A_HINGE_R), (-810.0, A_HINGE_L)):
        for z in (92.0, 410.0):
            x0, x1 = sorted((face_x, axis_x + math.copysign(4, axis_x - face_x)))
            frame.box((x0, -14, z), (x1, -6, z + 8), 'dark')
            frame.cyl((axis_x, -10, z + 8), 'z', 2.5, 12, 'dark', segs=8)

    leaf = Builder('SM_RusWallGateA_Leaf')
    pivots = []
    for pid, (hinge, near, far) in enumerate(((A_HINGE_R, -127.0, -461.0), (A_HINGE_L, -793.0, -459.0))):
        leaf.cur_piece = pid
        pivots.append((hinge, -10.0, 0.0))
        xl, xr = min(near, far), max(near, far)
        z0, z1 = A_LEAF_Z0, A_LEAF_Z1
        sgn = 1 if near > far else -1  # +1: hinge on the right (+X) side
        # frame tubes
        leaf.box((xr - 8, -14, z0), (xr, -6, z1), 'tube')
        leaf.box((xl, -14, z0), (xl + 8, -6, z1), 'tube')
        for za, zb in ((z0, z0 + 8), (128, 136), (z1 - 8, z1)):
            leaf.box((xl + 8, -14, za), (xr - 8, -6, zb), 'tube')
        # plate (both faces get the plate texture)
        leaf.box((xl + 8, -12, z0 + 8), (xr - 8, -8, z1 - 8), 'plate')
        # inside diagonal brace (+Y side), from low near corner to high far corner
        nx = near - sgn * 8
        fx = far + sgn * 8
        leaf.beam((nx, -5.5, 140), (fx, -5.5, z1 - 12), 7, 5, 'tube', up=(0, 0, 1))
        leaf.box((xl + 8, -8.5, 290), (xr - 8, -4, 297), 'tube')  # inside mid rail
        # roundel on the outside
        xc = (xl + xr) / 2
        leaf.cyl((xc, -11.5, 292), 'y', 100, -4.5, 'tube', segs=40, cap_mode='roundel')
        # hinge knuckles + straps (rotate with the leaf about the knuckle axis)
        for z in (101.0, 419.0):
            leaf.cyl((hinge, -10, z), 'z', 5, 28, 'dark', segs=10)
            hx0, hx1 = sorted((hinge - sgn * 4, near))
            leaf.box((hx0, -13.5, z + 4), (hx1, -6.5, z + 24), 'dark')
            sx0, sx1 = sorted((near, near - sgn * 60))
            leaf.box((sx0, -16.5, z + 6), (sx1, -13, z + 22), 'dark')
        if pid == 0:  # drop bolt + latch on the meeting stile
            leaf.cyl((far + 5, -17, z0 - 5), 'z', 2.5, 90, 'dark', segs=8)
            leaf.box((far + 2, -19, 60), (far + 9, -14, 66), 'dark')
            leaf.box((far + 2, -19, 230), (far + 26, -14, 246), 'dark')

    def anim(pid, p):
        # closing = progress 0 -> 1; the right leaf leads, the left one follows (and opens first)
        if pid == 0:
            s = smooth(p / 0.85)
            ang = -math.radians(92) * (1 - s)
        else:
            s = smooth((p - 0.15) / 0.85)
            ang = math.radians(92) * (1 - s)
        return Vector((0, 0, 0)), quat_axis_angle((0, 0, 1), ang)

    return frame, leaf, pivots, anim


# ------------------------------------------------------------------------------------------------ gate C
C_LIFT = 500.0
C_WHEEL_R = 45.0


def gate_c():
    frame = Builder('SM_RusWallGateC_Frame')
    towers = ((-160.0, 0.0), (-1070.0, -910.0))
    for xa, xb in towers:
        cx = (xa + xb) / 2
        frame.box((cx - 95, -120, 0), (cx + 95, 140, 60))
        frame.box((cx - 80, -100, 60), (cx + 80, 120, 1100))
        for xs in ((cx - 80, cx - 62), (cx + 62, cx + 80)):  # pilasters on the front
            frame.box((xs[0], -104, 60), (xs[1], -100, 960))
        frame.box((cx - 80, -106, 960), (cx + 80, -100, 985))
        frame.box((cx - 80, 120, 960), (cx + 80, 126, 985))
        frame.box((cx - 95, -115, 1100), (cx + 95, 135, 1125))
        frame.box((cx - 85, -105, 1125), (cx + 85, 125, 1140))
        # counterweight rails on the back face
        for dx in (-55, 45):
            frame.box((cx + dx, 120, 120), (cx + dx + 10, 132, 1090), 'dark')
    # lintel, its cornice and the star frame
    frame.box((-910, -90, 1000), (-160, 110, 1100))
    frame.box((-910, -115, 1100), (-160, 135, 1125))
    frame.box((-910, -98, 990), (-160, 118, 1004))
    for xs in ((-600, -592), (-478, -470)):
        frame.box((xs[0], -96, 1010), (xs[1], -89, 1090))
    for zs in ((1010, 1018), (1082, 1090)):
        frame.box((-600, -96, zs[0]), (-470, -89, zs[1]))
    frame.cyl((-535, -89, 1050), 'y', 32, -5.0, 'tube', segs=32, cap_mode='roundel')
    # guide channels (U section) on the inner tower faces
    for web_x0, web_x1, fl_x0, fl_x1 in ((-165, -160, -178, -165), (-910, -905, -905, -892)):
        frame.box((web_x0, -42, 0), (web_x1, 22, 1000), 'dark')
        for y0, y1 in ((-42, -34), (14, 22)):
            frame.box((fl_x0, y0, 0), (fl_x1, y1, 1000), 'dark')
    # sheave bearings on the tower tops
    for wx in (-100.0, -970.0):
        for dx in (-16, 10):
            frame.box((wx + dx, -8, 1140), (wx + dx + 6, 28, 1200), 'dark')
        frame.cyl((wx - 22, 10, 1185), 'x', 4, 44, 'dark', segs=8)

    leaf = Builder('SM_RusWallGateC_Leaf')
    pivots = []
    # 0: the slab
    leaf.cur_piece = 0
    pivots.append((-535.0, -10.0, 0.0))
    leaf.box((-895, -27, 0), (-175, 7, 470))
    leaf.box((-903, -29, 0), (-895, 9, 470), 'tube')
    leaf.box((-175, -29, 0), (-167, 9, 470), 'tube')
    leaf.box((-903, -32, 470), (-167, 12, 495), 'tube')
    for z in (150, 320):
        leaf.box((-895, -31, z), (-175, -26, z + 12), 'dark')
        leaf.box((-895, 6, z), (-175, 11, z + 12), 'dark')
    for lx in (-720, -350):
        leaf.box((lx - 8, -16, 495), (lx + 8, -4, 525), 'dark')
        leaf.cyl((lx - 12, -10, 515), 'x', 3, 24, 'dark', segs=8)
    # 1, 2: counterweights on the tower backs
    for pid, cx in ((1, -80.0), (2, -990.0)):
        leaf.cur_piece = pid
        pivots.append((cx, 172.0, 790.0))
        leaf.box((cx - 50, 132, 700), (cx + 50, 212, 880))
        for dx in (-58, 42):
            for z in (720, 820):
                leaf.box((cx + dx, 118, z), (cx + dx + 16, 140, z + 40), 'rust')
        leaf.box((cx - 8, 160, 880), (cx + 8, 184, 935), 'rust')
        leaf.cyl((cx - 10, 172, 920), 'x', 4, 20, 'dark', segs=8)
    # 3, 4: sheaves (rotate about X)
    for pid, wx in ((3, -100.0), (4, -970.0)):
        leaf.cur_piece = pid
        pivots.append((wx, 10.0, 1185.0))
        leaf.wheel((wx, 10, 1185), C_WHEEL_R, 6)

    def anim(pid, p):
        lift = C_LIFT * (1 - smooth(p))
        if pid == 0:
            return Vector((0, 0, lift)), (0, 0, 0, 1)
        if pid in (1, 2):
            return Vector((0, 0, -lift)), (0, 0, 0, 1)
        return Vector((0, 0, 0)), quat_axis_angle((1, 0, 0), -lift / C_WHEEL_R)

    return frame, leaf, pivots, anim


# ------------------------------------------------------------------------------------------------ bake
def bake(key, leaf_obj, pivots, anim):
    width = len(pivots)
    off_px = [0.0] * (width * FRAMES * 4)
    rot_px = [0.0] * (width * FRAMES * 4)
    prev = [None] * width
    for f in range(FRAMES):
        p = f / (FRAMES - 1)
        row = FRAMES - 1 - f
        for i in range(width):
            off, q = anim(i, p)
            q = list(q)
            if prev[i] is not None and sum(a * b for a, b in zip(prev[i], q)) < 0:
                q = [-c for c in q]
            prev[i] = q
            k = (row * width + i) * 4
            off_px[k:k + 4] = (off.x, off.y, off.z, 1.0)
            rot_px[k:k + 4] = q
    for name, px in (('VAT_Offset', off_px), ('VAT_Rot', rot_px)):
        img = bpy.data.images.new('T_RusWallGate%s_%s' % (key, name), width, FRAMES, alpha=True, float_buffer=True)
        img.colorspace_settings.name = 'Non-Color'
        img.pixels.foreach_set(px)
        path = os.path.join(OUT, 'T_RusWallGate%s_%s.exr' % (key, name))
        scene = bpy.context.scene
        scene.render.image_settings.file_format = 'OPEN_EXR'
        scene.render.image_settings.color_depth = '16'
        scene.render.image_settings.exr_codec = 'ZIP'
        img.save_render(path, scene=scene)
        print('wrote', path)
    # rest check: last frame (row 0) must be identity
    bad = [i for i in range(width) if any(abs(off_px[i * 4 + c]) > 0.01 for c in range(3))
           or abs(abs(rot_px[i * 4 + 3]) - 1) > 1e-4]
    print('pieces not at rest at progress 1:', bad)


def pose(leaf_obj, rest_cos, piece_of_vert, pivots, anim, p):
    """Move the leaf mesh vertices exactly as the shader will (Unreal space), for previews."""
    me = leaf_obj.data
    for v, rest in zip(me.vertices, rest_cos):
        pid = piece_of_vert[v.index]
        off, q = anim(pid, p)
        piv = Vector(pivots[pid])
        lp = U(rest)
        new = piv + off + rotate(q, lp - piv)
        v.co = B(new)
    me.update()


def preview(key, frame_obj, leaf_obj, pivots, anim):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_cavity = True
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = 1100, 800
    bpy.data.materials['Concrete'].diffuse_color = (0.56, 0.41, 0.37, 1)
    bpy.data.materials['GateSteel'].diffuse_color = (0.24, 0.31, 0.22, 1)
    ground = bpy.data.meshes.new('ground')
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=20)
    bm.to_mesh(ground)
    g = bpy.data.objects.new('ground', ground)
    scene.collection.objects.link(g)
    gm = bpy.data.materials.new('ground')
    gm.diffuse_color = (0.75, 0.68, 0.52, 1)
    ground.materials.append(gm)
    # wall stubs either side for context
    wall_me = bpy.data.meshes.new('walls')
    wb = Builder('walls')
    xmin = min(U(v.co).x for v in frame_obj.data.vertices)
    wb.box((xmin - 600, -20, 0), (xmin + 30, 0, 500))
    wb.box((-30, -20, 0), (600, 0, 500))
    wall = wb.to_object()
    cam_data = bpy.data.cameras.new('cam')
    cam_data.type = 'ORTHO'
    cam_data.ortho_scale = 22 if key == 'A' else 26
    cam = bpy.data.objects.new('cam', cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    centre = B(Vector((xmin / 2, 0, 350)))
    for view, yaw in (('front', -35.0), ('back', 145.0)):
        cam.rotation_euler = (math.radians(58), 0, math.radians(yaw))
        fwd = cam.rotation_euler.to_matrix() @ Vector((0, 0, -1))
        cam.location = centre - fwd * 60
        me = leaf_obj.data
        rest = [v.co.copy() for v in me.vertices]
        piece_of_vert = [0] * len(me.vertices)
        attr = me.attributes['piece'].data
        for poly in me.polygons:
            for vi in poly.vertices:
                piece_of_vert[vi] = attr[poly.index].value
        for p in (1.0, 0.66, 0.33, 0.0):
            pose(leaf_obj, rest, piece_of_vert, pivots, anim, p)
            scene.render.filepath = os.path.join(OUT, 'preview_%s_%s_%03d.png' % (key, view, int(p * 100)))
            bpy.ops.render.render(write_still=True)
        for v, r in zip(me.vertices, rest):
            v.co = r
        me.update()


def run(key, build):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    frame, leaf, pivots, anim = build()
    fo = frame.to_object()
    lo = leaf.to_object()
    if PREVIEW:
        preview(key, fo, lo, pivots, anim)
        return
    for o in (fo, lo):
        uv_project(o)
    add_vat_uvs(lo, pivots)
    bake(key, lo, pivots, anim)
    export(fo, os.path.join(OUT, fo.name + '.fbx'))
    export(lo, os.path.join(OUT, lo.name + '.fbx'))

    def bounds(o):
        pts = [U(v.co) for v in o.data.vertices]
        return [[round(min(p[i] for p in pts), 1) for i in range(3)], [round(max(p[i] for p in pts), 1) for i in range(3)]]

    info = dict(key=key, frames=FRAMES, pieces=len(pivots), pivots=pivots, frame_bounds=bounds(fo),
                leaf_bounds=bounds(lo), frame_tris=sum(len(p.vertices) - 2 for p in fo.data.polygons),
                leaf_tris=sum(len(p.vertices) - 2 for p in lo.data.polygons))
    json.dump(info, open(os.path.join(OUT, 'gate%s.json' % key), 'w'), indent=1)
    print('GATE_DONE', json.dumps(info))


os.makedirs(OUT, exist_ok=True)
run('A', gate_a)
run('C', gate_c)
