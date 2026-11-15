"""Intro cinematic set pieces - shared helpers (paths, noise, texture IO, mesh accumulator, FBX export).

Geometry is authored in METRES in the builders and written to Blender in CENTIMETRES
(1 Blender unit = 1 cm), exported with global_scale 0.01 exactly like the Ural Shield kit.
Texture arrays: row 0 = bottom of the image (V up), same as Blender's pixel order.
"""
import os
import sys
import math
import json
import random
import numpy as np
import bpy
import bmesh
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..', '..'))
OUT = os.path.join(PROJECT, 'Saved', 'IntroCinematic', 'import', 'Sets')
WORK = os.path.join(OUT, 'work')
CM = 100.0
F = np.float32


def tex_path(name):
    return os.path.join(OUT, name + '.png')


# ------------------------------------------------------------------ noise
def fft_noise(h, w, beta, seed, cut=None, aniso=(1.0, 1.0)):
    """Periodic fractal noise, zero mean / unit std."""
    r = np.random.default_rng(seed)
    f = np.fft.rfft2(r.standard_normal((h, w)).astype(F))
    ky = np.fft.fftfreq(h)[:, None] * aniso[0]
    kx = np.fft.rfftfreq(w)[None, :] * aniso[1]
    k = np.sqrt(ky * ky + kx * kx)
    k[0, 0] = 1.0
    amp = k ** (-beta)
    amp[0, 0] = 0.0
    if cut:
        amp = amp * np.exp(-(k / cut) ** 2)
    n = np.fft.irfft2(f * amp, s=(h, w))
    n -= n.mean()
    n /= (n.std() + 1e-9)
    return n.astype(F)


def sstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def sn(x, y, seed, freq, octaves=3, gain=0.5):
    """Smooth analytic noise (sum of random plane waves), ~unit std, any x/y (arrays or floats)."""
    r = np.random.default_rng(seed)
    out, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        for _ in range(5):
            a = r.uniform(0, 2 * np.pi)
            f = freq * (2 ** o) * r.uniform(0.7, 1.3)
            ph = r.uniform(0, 2 * np.pi)
            out = out + amp * np.sin(2 * np.pi * f * (x * np.cos(a) + y * np.sin(a)) + ph)
        tot += amp * amp * 2.5
        amp *= gain
    return out / np.sqrt(tot)


def ang_noise(th, seed, k0=2, k1=9):
    """Periodic noise of an angle, ~unit std."""
    r = np.random.default_rng(seed)
    out, tot = 0.0, 0.0
    for k in range(k0, k1 + 1):
        a = r.uniform(0.5, 1.0) / math.sqrt(k)
        out = out + a * np.sin(k * th + r.uniform(0, 2 * np.pi))
        tot += a * a * 0.5
    return out / math.sqrt(tot)


def normal_from_height(hm, ppm, wrap=True):
    """hm in metres -> tangent normal, +x right, +y up."""
    if wrap:
        gx = (np.roll(hm, -1, 1) - np.roll(hm, 1, 1)) * 0.5
        gy = (np.roll(hm, -1, 0) - np.roll(hm, 1, 0)) * 0.5
    else:
        gx = np.gradient(hm, axis=1)
        gy = np.gradient(hm, axis=0)
    n = np.stack([-gx * ppm, -gy * ppm, np.ones_like(hm)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n.astype(F)


def mixc(a, b, m):
    b = np.asarray(b, F)
    if b.ndim == 1:
        b = b[None, None, :]
    return a * (1 - m[..., None]) + b * m[..., None]


def rgb(r, g, b):
    return np.array([r, g, b], F) / 255.0


# ------------------------------------------------------------------ texture IO
def save_img(name, arr, cs):
    """arr: (h, w, 3) or (h, w) floats 0..1 in the FILE's encoding (sRGB values for colour)."""
    os.makedirs(OUT, exist_ok=True)
    if arr.ndim == 2:
        arr = np.repeat(arr[..., None], 3, -1)
    h, w = arr.shape[:2]
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = cs
    px = np.ones((h, w, 4), F)
    px[..., :3] = np.clip(arr, 0, 1)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = tex_path(name)
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)
    print('SAVED', tex_path(name))
    sys.stdout.flush()


def load_img(name):
    img = bpy.data.images.load(tex_path(name))
    w, h = img.size
    buf = np.empty(w * h * 4, F)
    img.pixels.foreach_get(buf)
    bpy.data.images.remove(img)
    return buf.reshape(h, w, 4)[..., :3]


def save_set(prefix, alb, height_m, ppm, ao, rough, metal=None, wrap=True, nrm=None):
    """Writes <prefix>_BaseColor / _Normal (DirectX, green down) / _ORM (R=AO G=rough B=metal)."""
    save_img(prefix + '_BaseColor', alb, 'sRGB')
    n = nrm if nrm is not None else normal_from_height(height_m, ppm, wrap)
    save_img(prefix + '_Normal', np.stack([n[..., 0] * 0.5 + 0.5, -n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5], -1),
             'Non-Color')
    if metal is None:
        metal = np.zeros_like(rough)
    save_img(prefix + '_ORM', np.stack([ao, rough, metal], -1), 'Non-Color')
    m = load_img(prefix + '_BaseColor').reshape(-1, 3).mean(0) * 255
    print('MEAN %s BaseColor sRGB = %.0f,%.0f,%.0f' % (prefix, m[0], m[1], m[2]))
    return m


# ------------------------------------------------------------------ scene / materials
def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1.0
    return sc


def make_material(name, prefix, emis_mask=None, emis_strength=0.0, emis_color=(1.0, 0.33, 0.04)):
    """Preview material: <prefix>_BaseColor/_Normal/_ORM. Normal is DirectX, green flipped for Blender."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    if not os.path.exists(tex_path(prefix + '_BaseColor')):
        bsdf.inputs['Base Color'].default_value = (0.4, 0.4, 0.4, 1)
        return mat

    def img(key, cs):
        n = nt.nodes.new('ShaderNodeTexImage')
        n.image = bpy.data.images.load(tex_path(key), check_existing=True)
        n.image.colorspace_settings.name = cs
        return n
    bc, nm, orm = img(prefix + '_BaseColor', 'sRGB'), img(prefix + '_Normal', 'Non-Color'), img(prefix + '_ORM', 'Non-Color')
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
    sn_ = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(nm.outputs['Color'], sn_.inputs['Color'])
    cn = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(sn_.outputs['Red'], cn.inputs['Red'])
    nt.links.new(sn_.outputs['Blue'], cn.inputs['Blue'])
    inv = nt.nodes.new('ShaderNodeMath')
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    nt.links.new(sn_.outputs['Green'], inv.inputs[1])
    nt.links.new(inv.outputs[0], cn.inputs['Green'])
    nmap = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(cn.outputs['Color'], nmap.inputs['Color'])
    nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    if emis_mask and os.path.exists(tex_path(emis_mask)):
        mk = img(emis_mask, 'Non-Color')
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Color'].default_value = (*emis_color, 1)
        mul = nt.nodes.new('ShaderNodeMath')
        mul.operation = 'MULTIPLY'
        mul.inputs[1].default_value = emis_strength
        nt.links.new(mk.outputs['Color'], mul.inputs[0])
        nt.links.new(mul.outputs[0], em.inputs['Strength'])
        add = nt.nodes.new('ShaderNodeAddShader')
        nt.links.new(bsdf.outputs['BSDF'], add.inputs[0])
        nt.links.new(em.outputs[0], add.inputs[1])
        nt.links.new(add.outputs[0], out.inputs['Surface'])
    return mat


# ------------------------------------------------------------------ mesh accumulator
class Acc:
    """Collects polygons (metres) with per-corner UVs and a material slot name per polygon."""

    def __init__(self, name, mats):
        self.name, self.mats = name, list(mats)
        self.v, self.f, self.uv, self.mi = [], [], [], []

    def poly(self, pts, uvs, mat):
        b = len(self.v)
        self.v.extend([tuple(p) for p in pts])
        self.f.append(tuple(range(b, b + len(pts))))
        self.uv.append(list(uvs))
        self.mi.append(self.mats.index(mat))

    def mesh(self, verts, faces, uvs, mat):
        """Indexed add: verts shared between faces; uvs = per face list of per-corner uv."""
        b = len(self.v)
        self.v.extend([tuple(p) for p in verts])
        m = self.mats.index(mat)
        for fc, u in zip(faces, uvs):
            self.f.append(tuple(b + i for i in fc))
            self.uv.append(list(u))
            self.mi.append(m)

    def build(self, origin=(0, 0, 0), smooth_angle=45.0, merge=0.01):
        o = np.array(origin, float)
        me = bpy.data.meshes.new(self.name)
        me.from_pydata([tuple((np.array(p) - o) * CM) for p in self.v], [], self.f)
        uvl = me.uv_layers.new(name='UVMap')
        flat = [c for u in self.uv for xy in u for c in xy]
        assert len(flat) == 2 * len(me.loops), '%s uv/loop mismatch' % self.name
        uvl.data.foreach_set('uv', flat)
        me.polygons.foreach_set('material_index', self.mi)
        me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
        me.update()
        bm = bmesh.new()
        bm.from_mesh(me)
        if merge:
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=merge)
        bmesh.ops.triangulate(bm, faces=bm.faces, quad_method='BEAUTY', ngon_method='EAR_CLIP')
        dead = [f for f in bm.faces if f.calc_area() < 1e-7]
        if dead:
            bmesh.ops.delete(bm, geom=dead, context='FACES')
        for f in bm.faces:
            f.normal_update()
        closed, flipped, opened = orient_closed_islands(bm)
        for f in bm.faces:
            f.normal_update()
        print('NORMALS %-28s tris %6d | closed islands %d (flipped %d) | open islands %d (kept as constructed)' % (
            self.name, len(bm.faces), closed, flipped, opened))
        self.island_stats = (closed, flipped, opened)
        bm.to_mesh(me)
        bm.free()
        me.use_auto_smooth = True
        me.auto_smooth_angle = math.radians(smooth_angle)
        for mname in self.mats:
            me.materials.append(bpy.data.materials.get(mname) or bpy.data.materials.new(mname))
        me.update()
        ob = bpy.data.objects.new(self.name, me)
        ob.location = tuple(o * CM)
        bpy.context.scene.collection.objects.link(ob)
        return ob


def orient_closed_islands(bm):
    """Signed-volume check per connected island: closed (2-manifold) islands with negative volume
    are flipped. Open islands keep the winding they were constructed with."""
    bm.faces.ensure_lookup_table()
    seen = set()
    closed = flipped = opened = 0
    for f0 in bm.faces:
        if f0.index in seen:
            continue
        stack, isl = [f0], []
        seen.add(f0.index)
        while stack:
            f = stack.pop()
            isl.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if all(len(e.link_faces) == 2 and e.is_contiguous for f in isl for e in f.edges):
            closed += 1
            vol = 0.0
            for f in isl:
                a, b, c = [v.co for v in f.verts]
                vol += a.dot(b.cross(c))
            if vol < 0:
                bmesh.ops.reverse_faces(bm, faces=isl)
                flipped += 1
        else:
            opened += 1
    return closed, flipped, opened


def tri_count(ob):
    ob.data.calc_loop_triangles()
    return len(ob.data.loop_triangles)


def export_fbx(ob, name=None):
    """Exports one object with its origin at the FBX origin (location zeroed for the export)."""
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, (name or ob.name) + '.fbx')
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    loc = ob.location.copy()
    ob.location = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             global_scale=0.01, axis_forward='-Z', axis_up='Y', mesh_smooth_type='FACE',
                             bake_anim=False, path_mode='STRIP', object_types={'MESH'},
                             use_custom_props=False)
    ob.location = loc
    bpy.context.view_layer.update()
    print('EXPORT', path)
    return path


def ob_stats(ob):
    co = np.array([v.co for v in ob.data.vertices])
    return {'tris': tri_count(ob), 'min_cm': [round(float(x), 1) for x in co.min(0)],
            'max_cm': [round(float(x), 1) for x in co.max(0)],
            'pivot_cm': [round(float(x), 1) for x in ob.location],
            'slots': [m.name for m in ob.data.materials], 'uv_layers': len(ob.data.uv_layers)}


def write_json(name, data):
    os.makedirs(WORK, exist_ok=True)
    with open(os.path.join(WORK, name), 'w') as fh:
        json.dump(data, fh, indent=1)


# ------------------------------------------------------------------ small geometry helpers (metres)
def vadd(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vmul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def vnorm(a):
    l = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def vcross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def frame(c, ax, ay, az):
    """Returns f(lx, ly, lz) -> world point for an orthonormal local frame at c."""
    def f(x, y, z):
        return (c[0] + ax[0] * x + ay[0] * y + az[0] * z,
                c[1] + ax[1] * x + ay[1] * y + az[1] * z,
                c[2] + ax[2] * x + ay[2] * y + az[2] * z)
    return f


def yaw_frame(c, yaw_deg, pitch_deg=0.0, roll_deg=0.0):
    """Local X = length axis (yawed from world +Y... no: from world X), Z up; pitch about local Y, roll about local X."""
    from mathutils import Euler
    m = Euler((math.radians(roll_deg), math.radians(pitch_deg), math.radians(yaw_deg)), 'XYZ').to_matrix()
    ax, ay, az = [tuple(m.col[i]) for i in range(3)]
    return frame(c, ax, ay, az)


_wood_rng = random.Random(7)


def wood_uv(lu, lv):
    """Wood atlas: 4 m x 4 m, grain along U, board rows 0.2 m tall. Returns 4 corner uvs (u0v0,u1v0,u1v1,u0v1)."""
    u0 = _wood_rng.random()
    if lv <= 0.2:
        vc = (_wood_rng.randrange(20) + 0.5) * 0.05
        v0 = vc - lv / 8.0
    else:
        v0 = _wood_rng.randrange(20) * 0.05
    return [(u0, v0), (u0 + lu / 4.0, v0), (u0 + lu / 4.0, v0 + lv / 4.0), (u0, v0 + lv / 4.0)]


def rect_uv_fn(u0, v0, u1, v1):
    """Fits any face rectangle into a fixed atlas rect."""
    def fn(lu, lv):
        return [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
    return fn


def obox(acc, fr, size, mat, uvfn=wood_uv, skip=''):
    """Box centred on the local frame fr with full size (sx, sy, sz). Long in-plane axis of each face -> U."""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    faces = {
        '+x': ([(hx, -hy, -hz), (hx, hy, -hz), (hx, hy, hz), (hx, -hy, hz)], size[1], size[2]),
        '-x': ([(-hx, hy, -hz), (-hx, -hy, -hz), (-hx, -hy, hz), (-hx, hy, hz)], size[1], size[2]),
        '+y': ([(hx, hy, -hz), (-hx, hy, -hz), (-hx, hy, hz), (hx, hy, hz)], size[0], size[2]),
        '-y': ([(-hx, -hy, -hz), (hx, -hy, -hz), (hx, -hy, hz), (-hx, -hy, hz)], size[0], size[2]),
        '+z': ([(-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)], size[0], size[1]),
        '-z': ([(-hx, hy, -hz), (hx, hy, -hz), (hx, -hy, -hz), (-hx, -hy, -hz)], size[0], size[1]),
    }
    for k, (pts, a, b) in faces.items():
        if k in skip:
            continue
        if a >= b:
            uv = uvfn(a, b)
        else:
            q = uvfn(b, a)
            uv = [q[1], q[2], q[3], q[0]]      # rotate so U follows the long (second) axis
        acc.poly([fr(*p) for p in pts], uv, mat)


def revolve(acc, fr, profile, segs, mat, uvrect, sy=1.0):
    """Surface of revolution about local Z. profile: list of (r, z); r == 0 makes a pole.
    uvrect = (u0, v0, u1, v1): U around, V along the profile. sy stretches local Y."""
    u0, v0, u1, v1 = uvrect
    ln = [0.0]
    for i in range(1, len(profile)):
        ln.append(ln[-1] + math.hypot(profile[i][0] - profile[i - 1][0], profile[i][1] - profile[i - 1][1]))
    tot = ln[-1] or 1.0
    verts, idx = [], []
    for (r, z) in profile:
        if r < 1e-6:
            idx.append([len(verts)] * segs)
            verts.append(fr(0, 0, z))
        else:
            row = []
            for k in range(segs):
                a = 2 * math.pi * k / segs
                row.append(len(verts))
                verts.append(fr(r * math.cos(a), r * math.sin(a) * sy, z))
            idx.append(row)
    faces, uvs = [], []
    for i in range(len(profile) - 1):
        va, vb = v0 + (v1 - v0) * ln[i] / tot, v0 + (v1 - v0) * ln[i + 1] / tot
        for k in range(segs):
            k2 = (k + 1) % segs
            ua, ub = u0 + (u1 - u0) * k / segs, u0 + (u1 - u0) * (k + 1) / segs
            a, b, c, d = idx[i][k], idx[i][k2], idx[i + 1][k2], idx[i + 1][k]
            f, u = [], []
            for vi, uvv in ((a, (ua, va)), (b, (ub, va)), (c, (ub, vb)), (d, (ua, vb))):
                if vi not in f:
                    f.append(vi)
                    u.append(uvv)
            if len(f) >= 3:
                faces.append(f)
                uvs.append(u)
    acc.mesh(verts, faces, uvs, mat)


def tube(acc, pts, radii, sides, mat, uscale=1.0, vspan=0.25, caps=True, u0=0.0, v0=0.0):
    """Tube along a 3D polyline (parallel-transport frames). radii: per point. U along length (m * uscale)."""
    P = [np.array(p, float) for p in pts]
    n = len(P)
    if n < 2:
        return
    T = []
    for i in range(n):
        d = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
        T.append(d / (np.linalg.norm(d) + 1e-12))
    up = np.array((0.0, 0.0, 1.0)) if abs(T[0][2]) < 0.9 else np.array((1.0, 0.0, 0.0))
    N = np.cross(T[0], up)
    N /= np.linalg.norm(N)
    verts, faces, uvs = [], [], []
    L = 0.0
    us = []
    for i in range(n):
        if i:
            L += float(np.linalg.norm(P[i] - P[i - 1]))
            N = N - T[i] * np.dot(N, T[i])
            N /= (np.linalg.norm(N) + 1e-12)
        B = np.cross(T[i], N)
        us.append(u0 + L * uscale)
        for k in range(sides):
            a = 2 * math.pi * k / sides
            verts.append(tuple(P[i] + radii[i] * (math.cos(a) * N + math.sin(a) * B)))
    for i in range(n - 1):
        for k in range(sides):
            k2 = (k + 1) % sides
            faces.append((i * sides + k, i * sides + k2, (i + 1) * sides + k2, (i + 1) * sides + k))
            va, vb = v0 + vspan * k / sides, v0 + vspan * (k + 1) / sides
            uvs.append([(us[i], va), (us[i], vb), (us[i + 1], vb), (us[i + 1], va)])
    if caps:
        for end, i in ((0, 0), (1, n - 1)):
            ci = len(verts)
            verts.append(tuple(P[i] + T[i] * radii[i] * (0.6 if end else -0.6)))
            for k in range(sides):
                k2 = (k + 1) % sides
                tri = (i * sides + k, i * sides + k2, ci) if end else (i * sides + k2, i * sides + k, ci)
                faces.append(tri)
                uvs.append([(us[i], v0), (us[i], v0 + 0.04), (us[i] + 0.01, v0 + 0.02)])
    acc.mesh(verts, faces, uvs, mat)


def smooth_path(pts, step, passes=3):
    """Resample a 3D polyline at ~step metres and relax it (rounds corners, keeps the ends)."""
    P = np.array(pts, float)
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    n = max(int(cum[-1] / step), 2)
    t = np.linspace(0, cum[-1], n + 1)
    Q = np.stack([np.interp(t, cum, P[:, i]) for i in range(3)], 1)
    for _ in range(passes):
        Q[1:-1] = 0.25 * Q[:-2] + 0.5 * Q[1:-1] + 0.25 * Q[2:]
    return Q
