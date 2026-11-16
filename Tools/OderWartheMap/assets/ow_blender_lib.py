"""Blender helpers for the Oder-Warthe asset kit (Blender 5.2, background).

Geometry in centimetres (scene unit scale 0.01), +X forward, +Z up, pivot on the ground at the origin. Builder collects
faces (each face its own vertices, material index, UVs, smooth flag); to_object() welds coincident vertices and marks
sharp edges by angle so boxes stay crisp and lathed/organic parts shade smooth. UVs are planar per face from the
dominant normal axis (tile size in cm per material) unless given. UCX hulls are convex point sets. export_fbx follows the
Pripyat kit convention (FBX_SCALE_NONE, default axes), proven by the Pripyat Part B import.
Materials are named after their Unreal material instances (MI_OW_*), so the importer binds slots by name.
"""
import json
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

OW_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/'
OW_TEX = OW_SRC + 'Textures/'
OW_FBX = OW_SRC + 'FBX/'
OW_PREVIEW = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/previews/'
MATS_JSON = OW_SRC + 'ow_materials.json'

# name: texture, linear tint, roughness, metallic, UV tile in cm (planar projection)
MATS = {
    'MI_OW_Concrete': dict(tex='T_OW_Concrete_D', tint=(0.62, 0.62, 0.60), rough=0.9, metal=0.0, tile=300.0),
    'MI_OW_ConcreteDark': dict(tex='T_OW_Concrete_D', tint=(0.50, 0.50, 0.48), rough=0.92, metal=0.0, tile=300.0),
    'MI_OW_RustSteel': dict(tex='T_OW_RustSteel_D', tint=(0.85, 0.82, 0.80), rough=0.78, metal=0.25, tile=200.0),
    'MI_OW_CastSteel': dict(tex='T_OW_RustSteel_D', tint=(0.50, 0.47, 0.46), rough=0.72, metal=0.3, tile=150.0),
    'MI_OW_DarkSteel': dict(tex='T_OW_RustSteel_D', tint=(0.42, 0.40, 0.40), rough=0.7, metal=0.35, tile=200.0),
    'MI_OW_Wood': dict(tex='T_OW_Wood_D', tint=(0.85, 0.85, 0.85), rough=0.88, metal=0.0, tile=200.0),
    'MI_OW_PineBark': dict(tex='T_OW_PineBark_D', tint=(1.0, 1.0, 1.0), rough=0.9, metal=0.0, tile=None),
    'MI_OW_Granite': dict(tex='T_OW_Granite_D', tint=(0.74, 0.74, 0.72), rough=0.85, metal=0.0, tile=250.0),
    'MI_OW_Earth': dict(tex='T_OW_Earth_D', tint=(0.85, 0.85, 0.85), rough=0.95, metal=0.0, tile=400.0),
    'MI_OW_Void': dict(tex='T_OW_Concrete_D', tint=(0.0, 0.0, 0.0), rough=1.0, metal=0.0, tile=300.0),
}


def write_mats_json():
    os.makedirs(OW_SRC, exist_ok=True)
    with open(MATS_JSON, 'w') as f:
        json.dump(MATS, f, indent=1)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 0.01


def v3(p):
    return Vector((float(p[0]), float(p[1]), float(p[2])))


def newell(pts):
    n = Vector((0.0, 0.0, 0.0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    return n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))


class Builder:
    def __init__(self, name, materials):
        self.name = name
        self.materials = list(materials)
        self.verts = []
        self.faces = []          # (indices, mat index, uvs, smooth)

    def mi(self, mat):
        if isinstance(mat, int):
            return mat
        if mat not in self.materials:
            self.materials.append(mat)
        return self.materials.index(mat)

    def face(self, pts, mat, uvs=None, smooth=False, tile=None):
        pts = [v3(p) for p in pts]
        m = self.mi(mat)
        if uvs is None:
            name = self.materials[m]
            t = tile or (MATS.get(name, {}).get('tile') or 200.0)
            n = newell(pts)
            ax = max(range(3), key=lambda i: abs(n[i]))
            if ax == 0:
                uvs = [((p.y if n.x > 0 else -p.y) / t, p.z / t) for p in pts]
            elif ax == 1:
                uvs = [((-p.x if n.y > 0 else p.x) / t, p.z / t) for p in pts]
            else:
                uvs = [(p.x / t, p.y / t) for p in pts]
        base = len(self.verts)
        self.verts.extend(pts)
        self.faces.append((list(range(base, base + len(pts))), m, list(uvs), smooth))

    def box(self, lo, hi, mat, xform=None, smooth=False, skip=()):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        P = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1),
             (x0, y1, z1)]
        P = [v3(p) for p in P]
        if xform is not None:
            P = [xform @ p for p in P]
        quads = {'-y': (0, 1, 5, 4), '+x': (1, 2, 6, 5), '+y': (2, 3, 7, 6), '-x': (3, 0, 4, 7), '+z': (4, 5, 6, 7),
                 '-z': (3, 2, 1, 0)}
        for k, q in quads.items():
            if k in skip:
                continue
            self.face([P[i] for i in q], mat, smooth=smooth)
        return P

    def obox(self, center, size, yaw_deg, mat, pitch_deg=0.0, roll_deg=0.0, skip=()):
        """Box of size (sx, sy, sz) centred at center (z = centre), rotated yaw (Z), pitch (Y), roll (X)."""
        m = (Matrix.Translation(v3(center)) @ Matrix.Rotation(math.radians(yaw_deg), 4, 'Z') @
             Matrix.Rotation(math.radians(pitch_deg), 4, 'Y') @ Matrix.Rotation(math.radians(roll_deg), 4, 'X'))
        sx, sy, sz = size
        return self.box((-sx / 2, -sy / 2, -sz / 2), (sx / 2, sy / 2, sz / 2), mat, xform=m, skip=skip)

    def tube(self, points, radii, sides, mat, smooth=True, cap_start=True, cap_end=True, uv_mode='planar',
             v_of=None, u_repeat=1.0, jitter=None, phase=0.0):
        """Generalised tube through points with per-point radius. uv_mode 'planar' (tile) or 'strip': U around
        (u_repeat repeats), V from v_of(point) (bark strip)."""
        pts = [v3(p) for p in points]
        rings = []
        prev = None
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            if prev is None:
                ax = t.orthogonal().normalized()
            else:
                ax = (prev - t * prev.dot(t))
                ax = ax.normalized() if ax.length > 1e-6 else t.orthogonal().normalized()
            prev = ax
            bx = t.cross(ax).normalized()
            ring = []
            for s in range(sides):
                a = 2 * math.pi * s / sides + phase
                r = radii[i] * (jitter(i, s) if jitter else 1.0)
                ring.append(p + (ax * math.cos(a) + bx * math.sin(a)) * r)
            rings.append(ring)
        m = self.mi(mat)
        for i in range(len(pts) - 1):
            for s in range(sides):
                n = (s + 1) % sides
                quad = [rings[i][s], rings[i][n], rings[i + 1][n], rings[i + 1][s]]
                if uv_mode == 'strip':
                    ua, ub = u_repeat * s / sides, u_repeat * (s + 1) / sides
                    va, vb = v_of(pts[i]), v_of(pts[i + 1])
                    self.face(quad, m, [(ua, va), (ub, va), (ub, vb), (ua, vb)], smooth)
                else:
                    self.face(quad, m, None, smooth)
        if cap_start and radii[0] > 0.01:
            ring = list(reversed(rings[0]))
            self.face(ring, m, [((q - pts[0]).x / 100.0, (q - pts[0]).y / 100.0) for q in ring] if uv_mode == 'strip'
                      else None, False)
        if cap_end and radii[-1] > 0.01:
            ring = rings[-1]
            self.face(ring, m, [((q - pts[-1]).x / 100.0 + 0.3, (q - pts[-1]).y / 100.0) for q in ring]
                      if uv_mode == 'strip' else None, False)
        return rings

    def beam(self, a, b, r, mat, sides=4, smooth=False):
        return self.tube([a, b], [r, r], sides, mat, smooth=smooth, phase=math.pi / sides)

    def lathe(self, profile, segments, mat, smooth=True, face_fn=None, radius_fn=None, a0=0.0):
        """Revolve profile [(r, z)] (bottom to top) around Z. face_fn(i, j) -> material override or None;
        radius_fn(i, j, r) -> r. Returns ring point lists."""
        rings = []
        for i, (r, z) in enumerate(profile):
            ring = []
            for j in range(segments):
                a = a0 + 2 * math.pi * j / segments
                rr = radius_fn(i, j, r) if radius_fn else r
                ring.append(Vector((rr * math.cos(a), rr * math.sin(a), z)))
            rings.append(ring)
        for i in range(len(profile) - 1):
            for j in range(segments):
                n = (j + 1) % segments
                quad = [rings[i][j], rings[i][n], rings[i + 1][n], rings[i + 1][j]]
                if profile[i + 1][0] < 0.01:
                    quad = [rings[i][j], rings[i][n], rings[i + 1][j]]
                mat_i = (face_fn(i, j) if face_fn else None) or mat
                self.face(quad, mat_i, None, smooth)
        return rings

    def heightfield(self, xs, ys, hfun, mat, skip_fn=None, smooth=True):
        """Grid surface z = hfun(x, y); skip_fn(x0, y0, x1, y1) -> True drops a cell."""
        H = [[hfun(x, y) for x in xs] for y in ys]
        for j in range(len(ys) - 1):
            for i in range(len(xs) - 1):
                if skip_fn and skip_fn(xs[i], ys[j], xs[i + 1], ys[j + 1]):
                    continue
                q = [(xs[i], ys[j], H[j][i]), (xs[i + 1], ys[j], H[j][i + 1]), (xs[i + 1], ys[j + 1], H[j + 1][i + 1]),
                     (xs[i], ys[j + 1], H[j + 1][i])]
                self.face(q, mat, [(p[0] / MATS[mat]['tile'], p[1] / MATS[mat]['tile']) for p in q], smooth)
        return H

    def to_object(self, sharp_angle=35.0, weld=0.05):
        me = bpy.data.meshes.new(self.name)
        me.from_pydata([tuple(v) for v in self.verts], [], [f[0] for f in self.faces])
        uv = me.uv_layers.new(name='UVMap')
        for poly, (idx, mat, uvs, smooth) in zip(me.polygons, self.faces):
            poly.material_index = mat
            poly.use_smooth = smooth
            for k in range(len(idx)):
                uv.data[poly.loop_start + k].uv = uvs[k]
        for name in self.materials:
            me.materials.append(get_material(name))
        me.validate(clean_customdata=False)
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=weld)
        bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=0.01)
        bmesh.ops.triangulate(bm, faces=bm.faces, quad_method='BEAUTY', ngon_method='BEAUTY')
        bm.to_mesh(me)
        bm.free()
        # sharp edges: by angle, plus every edge between a flat-shaded and any face
        try:
            me.set_sharp_from_angle(angle=math.radians(sharp_angle))
        except Exception:  # noqa: BLE001
            pass
        # edges whose faces are all smooth-flagged (terrain, domes, trunks) stay soft whatever the angle
        attr = me.attributes.get('sharp_edge')
        if attr is not None:
            ek = {}
            for poly in me.polygons:
                for key in poly.edge_keys:
                    ek.setdefault(key, []).append(poly.use_smooth)
            for e in me.edges:
                flags = ek.get(e.key, [])
                if flags and all(flags):
                    attr.data[e.index].value = False
        uv = me.uv_layers['UVMap']
        lm = me.uv_layers.new(name='LightMapUV')
        uv = me.uv_layers['UVMap']
        for i, d in enumerate(uv.data):
            lm.data[i].uv = d.uv
        me.update()
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def hull_object(name, points):
    bm = bmesh.new()
    vs = [bm.verts.new(v3(p)) for p in points]
    bmesh.ops.convex_hull(bm, input=vs)
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context='VERTS')
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def box_pts(lo, hi, xform=None):
    pts = [Vector((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    return [xform @ p for p in pts] if xform is not None else pts


def obox_pts(center, size, yaw_deg, pitch_deg=0.0, roll_deg=0.0):
    m = (Matrix.Translation(v3(center)) @ Matrix.Rotation(math.radians(yaw_deg), 4, 'Z') @
         Matrix.Rotation(math.radians(pitch_deg), 4, 'Y') @ Matrix.Rotation(math.radians(roll_deg), 4, 'X'))
    sx, sy, sz = size
    return box_pts((-sx / 2, -sy / 2, -sz / 2), (sx / 2, sy / 2, sz / 2), m)


def tri_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def bounds(obj):
    xs = [v.co for v in obj.data.vertices]
    lo = [min(p[i] for p in xs) for i in range(3)]
    hi = [max(p[i] for p in xs) for i in range(3)]
    return [round(v, 1) for v in lo], [round(v, 1) for v in hi]


def export_fbx(obj, path, hulls=(), keep_hulls=False):
    """hulls: list of point lists, written as UCX_<name>_NN. Returns a small report."""
    hobjs = [hull_object('UCX_%s_%02d' % (obj.name, k), pts) for k, pts in enumerate(hulls)]
    objs = [obj] + hobjs
    for o in bpy.context.scene.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = obj
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             object_types={'MESH'}, use_mesh_modifiers=False, mesh_smooth_type='FACE',
                             bake_space_transform=False, add_leaf_bones=False, use_custom_props=False,
                             colors_type='NONE')
    if not keep_hulls:
        for o in hobjs:
            bpy.data.objects.remove(o)
    lo, hi = bounds(obj)
    return dict(fbx=path, min=lo, max=hi, tris=tri_count(obj), hulls=len(hulls),
                slots=[m.name for m in obj.data.materials])


def get_material(name):
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    mat = bpy.data.materials.new(name)
    cfg = MATS.get(name)
    if cfg is None:
        return mat
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = cfg['rough']
    bsdf.inputs['Metallic'].default_value = cfg['metal']
    png = OW_TEX + cfg['tex'] + '.png'
    if os.path.exists(png):
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(png, check_existing=True)
        if cfg['tex'] == 'T_OW_PineBark_D':
            tex.extension = 'EXTEND'
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.blend_type = 'MULTIPLY'
        mix.inputs['Factor'].default_value = 1.0
        t = cfg['tint']
        mix.inputs[7].default_value = (t[0], t[1], t[2], 1.0)
        nt.links.new(tex.outputs['Color'], mix.inputs[6])
        nt.links.new(mix.outputs[2], bsdf.inputs['Base Color'])
    else:
        t = cfg['tint']
        bsdf.inputs['Base Color'].default_value = (t[0] * 0.4, t[1] * 0.4, t[2] * 0.4, 1)
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    return mat


def flat_material(name, rgb, roughness=0.9):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


def backface_debug():
    """Preview-only: back faces render flat magenta (UE culls them, Cycles does not)."""
    for mat in bpy.data.materials:
        if not mat.use_nodes or mat.get('bf_dbg'):
            continue
        nt = mat.node_tree
        out = next((n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL'), None)
        if out is None or not out.inputs['Surface'].links:
            continue
        src = out.inputs['Surface'].links[0].from_socket
        geo = nt.nodes.new('ShaderNodeNewGeometry')
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Color'].default_value = (1, 0, 1, 1)
        em.inputs['Strength'].default_value = 2.0
        mix = nt.nodes.new('ShaderNodeMixShader')
        nt.links.new(geo.outputs['Backfacing'], mix.inputs[0])
        nt.links.new(src, mix.inputs[1])
        nt.links.new(em.outputs['Emission'], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs['Surface'])
        mat['bf_dbg'] = 1


def render_preview(path, target=(0, 0, 0), distance=3000.0, elevation_deg=52.0, azimuth_deg=-50.0, lens=50.0,
                   resolution=(1200, 800), ground_rgb=(0.20, 0.17, 0.12), samples=24, ortho_scale=None,
                   ground_z=-1.0):
    """Cycles CPU still from an RTS-like high angle (GPU is shared, keep it off)."""
    scene = bpy.context.scene
    if os.environ.get('OW_BACKFACE') == '1':
        backface_debug()
        path = path.replace('.png', '_backface.png')
    for o in [o for o in scene.objects if o.name.startswith(('PreviewCam', 'PreviewSun', 'PreviewGround'))]:
        bpy.data.objects.remove(o)
    target = v3(target)
    bpy.ops.mesh.primitive_plane_add(size=200000, location=(target.x, target.y, ground_z))
    plane = bpy.context.active_object
    plane.name = 'PreviewGround'
    plane.data.materials.append(flat_material('Preview_Ground_%d' % int(ground_z), ground_rgb, 1.0))
    el, az = math.radians(elevation_deg), math.radians(azimuth_deg)
    loc = target + Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))) * distance
    cam_data = bpy.data.cameras.new('PreviewCam')
    cam_data.lens = lens
    cam_data.clip_end = 1e6
    cam_data.clip_start = 10
    if ortho_scale:
        cam_data.type = 'ORTHO'
        cam_data.ortho_scale = ortho_scale
    cam = bpy.data.objects.new('PreviewCam', cam_data)
    scene.collection.objects.link(cam)
    cam.location = loc
    cam.rotation_euler = (target - loc).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    sun_data = bpy.data.lights.new('PreviewSun', 'SUN')
    sun_data.energy = 3.2
    sun_data.angle = math.radians(2.0)
    sun = bpy.data.objects.new('PreviewSun', sun_data)
    sun.rotation_euler = (math.radians(42), math.radians(8), math.radians(-35))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.get('PreviewWorld') or bpy.data.worlds.new('PreviewWorld')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.6, 0.66, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.7
    scene.world = world
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = samples
    try:
        scene.cycles.use_denoising = True
        scene.cycles.denoiser = 'OPENIMAGEDENOISE'
    except Exception:  # noqa: BLE001
        pass
    try:
        scene.view_settings.view_transform = 'AgX'
    except Exception:  # noqa: BLE001
        scene.view_settings.view_transform = 'Filmic'
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.resolution_percentage = 100
    scene.render.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    return path


def rng(seed):
    return random.Random(seed)
