"""Shared helpers for the intro-cinematic props (Blender 4.0, headless).

Geometry builder with per-part normal verification, UV + Cycles bake to BaseColor / Normal (DirectX) / ORM PNGs,
FBX export (metres in Blender -> centimetres in Unreal, same recipe as RusWallGates/build_gates.py) and preview renders.
"""
import bpy, bmesh, math, os, random, struct, zlib
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', '..', 'Saved', 'IntroCinematic', 'import', 'Props'))


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1.0
    return sc


def V3(*a):
    return Vector(a[0]) if len(a) == 1 else Vector(a)


def TRS(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    from mathutils import Euler
    m = Matrix.Translation(Vector(loc)) @ Euler(rot, 'XYZ').to_matrix().to_4x4()
    s = Matrix.Identity(4)
    s[0][0], s[1][1], s[2][2] = scale
    return m @ s


def smooth_path(pts, n, loop=False):
    """Catmull-Rom resample of a polyline to n points (n segments when loop)."""
    pts = [Vector(p) for p in pts]
    k = len(pts)
    out = []
    segs = k if loop else k - 1
    count = n if loop else n - 1
    for i in range(n):
        t = i / count * segs
        s = min(int(t), segs - 1)
        u = t - s

        def P(j):
            return pts[j % k] if loop else pts[max(0, min(k - 1, j))]
        p0, p1, p2, p3 = P(s - 1), P(s), P(s + 1), P(s + 2)
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u
                          + (-p0 + 3 * p1 - 3 * p2 + p3) * u * u * u))
    return out


def _components(faces):
    fs = set(faces)
    seen = set()
    comps = []
    for f in faces:
        if f in seen:
            continue
        comp = []
        stack = [f]
        seen.add(f)
        while stack:
            c = stack.pop()
            comp.append(c)
            for e in c.edges:
                for o in e.link_faces:
                    if o in fs and o not in seen:
                        seen.add(o)
                        stack.append(o)
        comps.append(comp)
    return comps


def _signed_volume(faces):
    vol = 0.0
    for f in faces:
        vs = [v.co for v in f.verts]
        for i in range(1, len(vs) - 1):
            vol += vs[0].dot(vs[i].cross(vs[i + 1])) / 6.0
    return vol


class Builder:
    """Accumulates parts into one bmesh. Every add() refreshes the cached face normals, recalculates them outward and
    then verifies each connected shell (closed: signed volume > 0; open: against a reference point or direction)."""

    def __init__(self, seed=1):
        self.bm = bmesh.new()
        self.mats = []
        self.uv = self.bm.loops.layers.uv.new('UVMap')
        self.pid = self.bm.loops.layers.float_color.new('pid')
        self.rng = random.Random(seed)
        self.checks = []

    # ------------------------------------------------------------------ bookkeeping
    def _mi(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def add(self, faces, mat, smooth=40.0, ref=None, flip=False, explicit=None):
        faces = [f for f in faces if f.is_valid]
        mi = self._mi(mat)
        for f in faces:
            f.normal_update()          # recalc reads cached normals: refresh first (mirrored / moved verts)
            f.material_index = mi
            f.smooth = True
        if explicit is None:
            bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        for comp in _components(faces):
            fs = set(comp)
            closed = all(len(e.link_faces) == 2 and e.link_faces[0] in fs and e.link_faces[1] in fs
                         for f in comp for e in f.edges)
            rec = dict(faces=comp, closed=closed, flip=flip, explicit=Vector(explicit) if explicit else None,
                       ref=Vector(ref) if ref is not None else None)
            if not self._ok(rec):
                for f in comp:
                    f.normal_flip()
            self.checks.append(rec)
            p = self.rng.random()
            for f in comp:
                for l in f.loops:
                    l[self.pid] = (p, p, p, 1.0)
        ang = math.radians(smooth)
        for e in set(e for f in faces for e in f.edges):
            if len(e.link_faces) == 2:
                e.smooth = e.calc_face_angle(0.0) <= ang
        return faces

    def _ok(self, rec):
        comp = rec['faces']
        if rec['explicit'] is not None:
            s = sum(f.normal.dot(rec['explicit']) * f.calc_area() for f in comp)
        elif rec['closed']:
            s = _signed_volume(comp)
        else:
            c = rec['ref']
            if c is None:
                c = Vector((0, 0, 0))
                for f in comp:
                    c += f.calc_center_median()
                c /= len(comp)
            s = sum(f.normal.dot(f.calc_center_median() - c) * f.calc_area() for f in comp)
        return (s > 0) != rec['flip']

    def verify(self):
        """Re-check every shell after all edits; returns (closed shells, open shells), raises on a flipped shell."""
        self.bm.normal_update()
        nc = no = 0
        for rec in self.checks:
            rec['faces'] = [f for f in rec['faces'] if f.is_valid]
            if not rec['faces']:
                continue
            if not self._ok(rec):
                raise RuntimeError('flipped shell detected')
            if rec['closed']:
                nc += 1
            else:
                no += 1
        return nc, no

    def transform(self, m):
        bmesh.ops.transform(self.bm, matrix=m, verts=self.bm.verts)
        if m.determinant() < 0:
            bmesh.ops.reverse_faces(self.bm, faces=self.bm.faces)
        self.bm.normal_update()

    def tris(self):
        return sum(len(f.verts) - 2 for f in self.bm.faces)

    def to_object(self, name, sc, get_mat):
        self.bm.normal_update()
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        me.use_auto_smooth = True
        me.auto_smooth_angle = math.pi
        for mn in self.mats:
            me.materials.append(get_mat(mn))
        ob = bpy.data.objects.new(name, me)
        sc.collection.objects.link(ob)
        return ob

    # ------------------------------------------------------------------ primitives (return new faces)
    def _v(self, co, m):
        co = Vector(co)
        return self.bm.verts.new(m @ co if m is not None else co)

    def lathe(self, prof, seg=24, m=None, loop=False, sy=1.0):
        """Revolve (r, z) profile about Z. r == 0 gives a pole. loop joins the last ring to the first."""
        rings = []
        for r, z in prof:
            if r < 1e-7:
                rings.append([self._v((0, 0, z), m)])
            else:
                rings.append([self._v((r * math.cos(2 * math.pi * i / seg), sy * r * math.sin(2 * math.pi * i / seg), z), m)
                              for i in range(seg)])
        faces = []
        pairs = list(zip(rings, rings[1:]))
        if loop:
            pairs.append((rings[-1], rings[0]))
        for a, b in pairs:
            if len(a) == 1 and len(b) == 1:
                continue
            for i in range(seg):
                j = (i + 1) % seg
                if len(a) == 1:
                    vs = (a[0], b[j], b[i])
                elif len(b) == 1:
                    vs = (a[i], a[j], b[0])
                else:
                    vs = (a[i], a[j], b[j], b[i])
                faces.append(self.bm.faces.new(vs))
        return faces

    def tube(self, pts, r, sides=6, caps=True, loop=False, m=None, radii=None):
        pts = [Vector(p) for p in pts]
        n = len(pts)
        tang = []
        for i in range(n):
            a = pts[(i - 1) % n] if (loop or i > 0) else pts[i]
            b = pts[(i + 1) % n] if (loop or i < n - 1) else pts[i]
            tang.append((b - a).normalized())
        t0 = tang[0]
        up = Vector((0, 0, 1)) if abs(t0.z) < 0.9 else Vector((1, 0, 0))
        nrm = (up - t0 * up.dot(t0)).normalized()
        rings = []
        for i, t in enumerate(tang):
            if i > 0:
                ax = tang[i - 1].cross(t)
                if ax.length > 1e-9:
                    nrm = Matrix.Rotation(tang[i - 1].angle(t), 3, ax.normalized()) @ nrm
                nrm = (nrm - t * nrm.dot(t)).normalized()
            bi = t.cross(nrm)
            rr = radii[i] if radii else r
            rings.append([self._v(pts[i] + (nrm * math.cos(2 * math.pi * k / sides) + bi * math.sin(2 * math.pi * k / sides)) * rr, m)
                          for k in range(sides)])
        faces = []
        pairs = list(zip(rings, rings[1:]))
        if loop:
            pairs.append((rings[-1], rings[0]))
        for a, b in pairs:
            for k in range(sides):
                j = (k + 1) % sides
                faces.append(self.bm.faces.new((a[k], a[j], b[j], b[k])))
        if caps and not loop:
            faces.append(self.bm.faces.new(rings[0]))
            faces.append(self.bm.faces.new(rings[-1]))
        return faces

    def cyl(self, p0, p1, r, sides=12, r1=None, m=None, caps=True):
        return self.tube([p0, p1], r, sides, caps=caps, m=m, radii=[r, r if r1 is None else r1])

    def box(self, c, s, m=None):
        c = Vector(c)
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        v = [self._v(c + Vector((x, y, z)), m) for x in (-hx, hx) for y in (-hy, hy) for z in (-hz, hz)]
        idx = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        return [self.bm.faces.new([v[i] for i in q]) for q in idx]

    def prism(self, poly, z0, z1, m=None, caps=True):
        """Extrude a 2D polygon (local XY) from z0 to z1 along local Z."""
        a = [self._v((x, y, z0), m) for x, y in poly]
        b = [self._v((x, y, z1), m) for x, y in poly]
        n = len(poly)
        faces = [self.bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i])) for i in range(n)]
        if caps:
            faces.append(self.bm.faces.new(a))
            faces.append(self.bm.faces.new(b))
        return faces

    def frame_sweep(self, prof, w, h, m=None):
        """Mitred rectangular frame in the local XZ plane (outer w x h, centred). prof = closed polygon of
        (inset from the outer edge, depth along local Y)."""
        rings = []
        for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            rings.append([self._v((sx * (w / 2 - d), y, sz * (h / 2 - d)), m) for d, y in prof])
        n = len(prof)
        faces = []
        for c in range(4):
            a, b = rings[c], rings[(c + 1) % 4]
            for i in range(n):
                j = (i + 1) % n
                faces.append(self.bm.faces.new((a[i], a[j], b[j], b[i])))
        return faces


# ====================================================================================== images
def write_png(path, a):
    """a: H x W x C uint8, first row = top of the image."""
    h, w, c = a.shape
    ct = {1: 0, 3: 2, 4: 6}[c]
    raw = np.hstack([np.zeros((h, 1), np.uint8), a.reshape(h, w * c)]).tobytes()

    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, ct, 0, 0, 0))
                + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


def to8(a):
    return (np.clip(a, 0, 1) * 255.0 + 0.5).astype(np.uint8)


def srgb(a):
    a = np.clip(a, 0, 1)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055)


def save_set(tex, bc, nrm, orm, out=None):
    """bc: linear RGB(A) float, nrm/orm float 0-1; arrays are bottom-up (Blender / UV orientation)."""
    out = out or OUT
    b = bc.copy()
    b[..., :3] = srgb(b[..., :3])
    write_png(os.path.join(out, 'T_Intro_%s_BC.png' % tex), to8(b[::-1]))
    write_png(os.path.join(out, 'T_Intro_%s_N.png' % tex), to8(nrm[::-1]))
    write_png(os.path.join(out, 'T_Intro_%s_ORM.png' % tex), to8(orm[::-1]))


def height_to_normal_dx(h, strength):
    """h bottom-up (row 0 = V 0), tileable. Returns DirectX normal map (green = -V)."""
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * strength
    dv = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * strength
    n = np.stack([-dx, dv, np.ones_like(h)], -1)       # OpenGL would be -dv; DirectX flips green
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def value_noise(size, cells, rng, octaves=4, persist=0.5, tile=True):
    out = np.zeros((size, size), np.float32)
    amp, tot = 1.0, 0.0
    ys = np.arange(size, dtype=np.float32)
    for o in range(octaves):
        c = cells * (2 ** o)
        g = rng.random((c, c)).astype(np.float32)
        t = ys / size * c
        i0 = np.floor(t).astype(int) % c
        i1 = (i0 + 1) % c
        f = t - np.floor(t)
        f = f * f * (3 - 2 * f)
        rows = g[i0][:, i0] * (1 - f)[None, :] + g[i0][:, i1] * f[None, :]
        rows1 = g[i1][:, i0] * (1 - f)[None, :] + g[i1][:, i1] * f[None, :]
        out += (rows * (1 - f)[:, None] + rows1 * f[:, None]) * amp
        tot += amp
        amp *= persist
    return out / tot


# ====================================================================================== UV + bake
def select_only(ob):
    for o in bpy.context.scene.objects:
        o.select_set(o == ob)
    bpy.context.view_layer.objects.active = ob


def smart_uv(ob, angle=62.0, margin=0.012):
    select_only(ob)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin)
    bpy.ops.uv.pack_islands(margin=margin, rotate=True)
    bpy.ops.object.mode_set(mode='OBJECT')


def _dup_without(ob, names):
    d = ob.copy()
    d.data = ob.data.copy()
    bpy.context.scene.collection.objects.link(d)
    idx = {i for i, m in enumerate(d.data.materials) if m.name in names}
    bm = bmesh.new()
    bm.from_mesh(d.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index in idx], context='FACES')
    bm.to_mesh(d.data)
    bm.free()
    return d


def _pixels(img):
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)


def bake_set(sc, ob, tex, size, matinfo, exclude=(), ao_exclude=(), alpha=False):
    """Bakes the procedural materials on ob into T_Intro_<tex>_BC / _N / _ORM. Faces whose bake material is in
    exclude are left out of every pass (overlapping UVs); ao_exclude only out of the AO pass (glass)."""
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.use_denoising = False
    sc.render.bake.margin = max(4, size // 256)
    sc.render.bake.margin_type = 'EXTEND'
    sc.render.bake.use_selected_to_active = False
    work = _dup_without(ob, set(exclude)) if exclude else ob
    aow = _dup_without(ob, set(exclude) | set(ao_exclude)) if (exclude or ao_exclude) else ob
    vis = {o: o.hide_render for o in sc.objects}

    def run(obj, kind, passname, samples, fill, clear=True, **kw):
        for o in sc.objects:
            o.hide_render = vis.get(o, False)
        for o in (ob, work, aow):
            o.hide_render = o != obj
        img = bpy.data.images.new('bake_' + passname, size, size, alpha=False, float_buffer=True)
        img.colorspace_settings.name = 'Non-Color'
        img.generated_color = fill
        px = np.tile(np.array(fill, np.float32), size * size)
        img.pixels.foreach_set(px)
        for m in obj.data.materials:
            matinfo[m.name].target(img, passname)
        sc.cycles.samples = samples
        select_only(obj)
        bpy.ops.object.bake(type=kind, use_clear=clear, **kw)
        a = _pixels(img).copy()
        bpy.data.images.remove(img)
        return a

    mask = run(work, 'EMIT', 'mask', 1, (0, 0, 0, 1))[..., 0] > 0.5
    col = run(work, 'EMIT', 'color', 4, (0, 0, 0, 1))[..., :3]
    rough = run(work, 'EMIT', 'rough', 4, (0, 0, 0, 1))[..., 0]
    metal = run(work, 'EMIT', 'metal', 4, (0, 0, 0, 1))[..., 0]
    al = run(work, 'EMIT', 'alpha', 4, (0, 0, 0, 1))[..., 0] if alpha else None
    nrm = run(work, 'NORMAL', 'normal', 8, (0.5, 0.5, 1, 1), normal_space='TANGENT',
              normal_r='POS_X', normal_g='NEG_Y', normal_b='POS_Z')[..., :3]
    ao = run(aow, 'AO', 'normal', 96, (1, 1, 1, 1), clear=False)[..., 0]
    for o in sc.objects:
        o.hide_render = vis.get(o, False)
    for d in {work, aow} - {ob}:
        bpy.data.objects.remove(d)
    # soften AO a little (3x3 blur + lift) and fill the unbaked area with neutral values
    ao = sum(np.roll(np.roll(ao, i, 0), j, 1) for i in (-1, 0, 1) for j in (-1, 0, 1)) / 9.0
    ao = 0.25 + 0.75 * np.clip(ao, 0, 1)
    if mask.any():
        col[~mask] = col[mask].mean(0)
        rough[~mask] = rough[mask].mean()
    metal[~mask] = 0
    nrm[~mask] = (0.5, 0.5, 1.0)
    ao[~mask] = 1.0
    bc = col if al is None else np.dstack([col, np.where(mask, al, 1.0)])
    save_set(tex, bc, nrm, np.dstack([ao, rough, metal]))
    return float(mask.mean())


# ====================================================================================== preview materials
def pv_material(name, tex, kind='opaque', out=None):
    """Principled material reading the delivered PNGs (DirectX normal flipped back for Blender)."""
    out = out or OUT
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes['Principled BSDF']

    def img(suffix, data):
        n = nt.nodes.new('ShaderNodeTexImage')
        n.image = bpy.data.images.load(os.path.join(out, 'T_Intro_%s_%s.png' % (tex, suffix)), check_existing=True)
        if data:
            n.image.colorspace_settings.name = 'Non-Color'
        return n
    bc, nr, orm = img('BC', False), img('N', True), img('ORM', True)
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(orm.outputs[0], sep.inputs[0])
    mul = nt.nodes.new('ShaderNodeMix')
    mul.data_type = 'RGBA'
    mul.blend_type = 'MULTIPLY'
    mul.inputs[0].default_value = 0.8
    nt.links.new(bc.outputs[0], mul.inputs[6])
    nt.links.new(sep.outputs[0], mul.inputs[7])
    nt.links.new(mul.outputs[2], b.inputs['Base Color'])
    nt.links.new(sep.outputs[1], b.inputs['Roughness'])
    nt.links.new(sep.outputs[2], b.inputs['Metallic'])
    s2 = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(nr.outputs[0], s2.inputs[0])
    inv = nt.nodes.new('ShaderNodeMath')
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    nt.links.new(s2.outputs[1], inv.inputs[1])
    cmb = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(s2.outputs[0], cmb.inputs[0])
    nt.links.new(inv.outputs[0], cmb.inputs[1])
    nt.links.new(s2.outputs[2], cmb.inputs[2])
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(cmb.outputs[0], nm.inputs['Color'])
    nt.links.new(nm.outputs[0], b.inputs['Normal'])
    if kind == 'glass':
        b.inputs['Transmission Weight'].default_value = 1.0
        b.inputs['IOR'].default_value = 1.45
        nt.links.new(bc.outputs['Alpha'], b.inputs['Alpha'])
        mx = nt.nodes.new('ShaderNodeMapRange')            # preview only: keep thin clear glass readable
        mx.inputs['To Min'].default_value = 0.25
        nt.links.new(bc.outputs['Alpha'], mx.inputs[0])
        nt.links.new(mx.outputs[0], b.inputs['Alpha'])
        b.inputs['Transmission Weight'].default_value = 0.0
    elif kind == 'emit':
        nt.links.new(bc.outputs[0], b.inputs['Emission Color'])
        b.inputs['Emission Strength'].default_value = 12.0
    elif kind == 'alpha':
        nt.links.new(bc.outputs['Alpha'], b.inputs['Alpha'])
    return m


# ====================================================================================== export / render
def export_fbx(ob, path):
    """Writes centimetre vertex data with node scale 1 and UnitScaleFactor 1 (scene unit set to 1 cm for the export),
    so Unreal needs no unit conversion: 1 uu = 1 cm, scale applied. Axes as in RusWallGates/build_gates.py."""
    sc = bpy.context.scene
    select_only(ob)
    ob.data.transform(Matrix.Scale(100.0, 4))
    sc.unit_settings.scale_length = 0.01
    try:
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                                 apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', add_leaf_bones=False,
                                 bake_anim=False, axis_forward='-Z', axis_up='Y', use_custom_props=False,
                                 path_mode='STRIP', embed_textures=False)
    finally:
        ob.data.transform(Matrix.Scale(0.01, 4))
        sc.unit_settings.scale_length = 1.0
        ob.data.update()


def bbox(objs):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            for i in range(3):
                lo[i] = min(lo[i], p[i])
                hi[i] = max(hi[i], p[i])
    return lo, hi


def setup_cycles(sc, res=(1200, 900), samples=48):
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGB'
    sc.view_settings.view_transform = 'Standard'
    sc.render.film_transparent = False


def add_stage(sc, lo, hi, view, ground_z=None, world=0.4):
    """Grey ground, two suns and a camera framing the box lo..hi from direction view. Returns the temp objects."""
    tmp = []
    c = (lo + hi) / 2
    rad = (hi - lo).length / 2
    if sc.world is None:
        sc.world = bpy.data.worlds.new('W')
    sc.world.use_nodes = True
    bg = sc.world.node_tree.nodes['Background']
    bg.inputs[0].default_value = (0.75, 0.78, 0.82, 1)
    bg.inputs[1].default_value = world
    gm = bpy.data.materials.new('PV_Ground')
    gm.use_nodes = True
    gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.22, 0.22, 0.22, 1)
    gm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.9
    me = bpy.data.meshes.new('pv_ground')
    s = max(rad * 60, 1.0)
    z = lo.z - rad * 0.004 if ground_z is None else ground_z      # just below the prop: no z-fight with flat meshes
    me.from_pydata([(-s, -s, z), (s, -s, z), (s, s, z), (-s, s, z)], [], [(0, 1, 2, 3)])
    g = bpy.data.objects.new('pv_ground', me)
    me.materials.append(gm)
    sc.collection.objects.link(g)
    tmp.append(g)
    d = Vector(view).normalized()
    for nm, en, dirv in (('key', 2.3, Vector((-0.5, -0.6, 0.9))), ('fill', 0.6, Vector((0.9, -0.2, 0.5))),
                         ('rim', 0.8, d * 0.7 + Vector((0, 0, 0.6)))):
        ld = bpy.data.lights.new('pv_' + nm, 'SUN')
        ld.energy = en
        ld.angle = math.radians(12)
        lo_ = bpy.data.objects.new('pv_' + nm, ld)
        lo_.rotation_euler = dirv.normalized().to_track_quat('Z', 'Y').to_euler()
        sc.collection.objects.link(lo_)
        tmp.append(lo_)
    cd = bpy.data.cameras.new('pv_cam')
    cd.lens = 60
    asp = sc.render.resolution_y / sc.render.resolution_x
    fov_v = 2 * math.atan(18 * asp / cd.lens)
    fov_h = 2 * math.atan(18 / cd.lens)
    dist = rad / math.sin(min(fov_v, fov_h) / 2) * 1.02
    cam = bpy.data.objects.new('pv_cam', cd)
    cam.location = c + d * dist
    cam.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
    cd.clip_start = dist * 0.02
    cd.clip_end = dist * 50 + s * 3
    sc.collection.objects.link(cam)
    sc.camera = cam
    tmp.append(cam)
    return tmp


def render_preview(sc, objs, path, view=(1.0, -1.3, 0.75), samples=48):
    setup_cycles(sc, samples=samples)
    lo, hi = bbox(objs)
    tmp = add_stage(sc, lo, hi, view)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    for o in tmp:
        bpy.data.objects.remove(o)
