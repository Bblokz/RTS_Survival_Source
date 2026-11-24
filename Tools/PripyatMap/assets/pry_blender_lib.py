"""Small Blender helpers for the Pripyat Part B / bog foliage models (run inside Blender 5, background).

Geometry is built in centimetres (scene unit 0.01), +X forward, +Z up, pivot at the origin on the ground.
A Builder collects quads/tris with a material index and UVs, then becomes one mesh object. UCX hulls are boxes or
point sets turned into convex hulls. export_fbx writes the Ore-kit FBX convention (FBX_SCALE_NONE).
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 0.01


class Builder:
    def __init__(self, name, materials):
        self.name = name
        self.materials = materials
        self.verts = []
        self.faces = []          # (indices, mat, uvs)
        self.normals = {}        # face index -> custom normal for all its loops

    def face(self, pts, mat, uvs, normal=None):
        base = len(self.verts)
        self.verts.extend([Vector(p) for p in pts])
        self.faces.append((list(range(base, base + len(pts))), mat, uvs))
        if normal is not None:
            self.normals[len(self.faces) - 1] = Vector(normal).normalized()

    def quad_uv(self, pts, mat, rect, normal=None):
        """pts in order bottom-left, bottom-right, top-right, top-left; rect = (u0, v0, u1, v1)."""
        u0, v0, u1, v1 = rect
        self.face(pts, mat, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], normal)

    def box(self, lo, hi, mat, uv_scale=200.0, uv_offset=(0.0, 0.0), xform=None, uv_rect=None):
        """Axis box (optionally transformed by a 4x4 Matrix). UVs: planar per face, uv_scale cm per UV unit, or the
        whole face squeezed into uv_rect."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        P = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        quads = [(0, 1, 5, 4, 'xz'), (1, 2, 6, 5, 'yz'), (2, 3, 7, 6, 'xz'), (3, 0, 4, 7, 'yz'), (4, 5, 6, 7, 'xy'),
                 (3, 2, 1, 0, 'xy')]
        for a, b, c, d, plane in quads:
            pts = [Vector(P[i]) for i in (a, b, c, d)]
            if xform is not None:
                pts = [xform @ p for p in pts]
            if uv_rect is not None:
                self.quad_uv(pts, mat, uv_rect)
                continue
            uvs = []
            for i in (a, b, c, d):
                p = P[i]
                s, t = {'xz': (p[0] + p[1] * 0.0, p[2]), 'yz': (p[1], p[2]), 'xy': (p[0], p[1])}[plane]
                uvs.append((s / uv_scale + uv_offset[0], t / uv_scale + uv_offset[1]))
            self.face(pts, mat, uvs)

    def prism(self, a, b, radius, sides, mat, rect, radius_b=None, cap=True):
        """Tapered n-gon prism from point a to b. UVs: u around (rect u0..u1), v along (rect v0..v1)."""
        a, b = Vector(a), Vector(b)
        axis = (b - a)
        length = axis.length
        axis.normalize()
        helper = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
        side1 = axis.cross(helper).normalized()
        side2 = axis.cross(side1).normalized()
        rb = radius if radius_b is None else radius_b
        u0, v0, u1, v1 = rect
        ring_a = [a + (side1 * math.cos(2 * math.pi * k / sides) + side2 * math.sin(2 * math.pi * k / sides)) * radius
                  for k in range(sides)]
        ring_b = [b + (side1 * math.cos(2 * math.pi * k / sides) + side2 * math.sin(2 * math.pi * k / sides)) * rb
                  for k in range(sides)]
        for k in range(sides):
            n = (k + 1) % sides
            ua = u0 + (u1 - u0) * k / sides
            ub = u0 + (u1 - u0) * (k + 1) / sides
            self.face([ring_a[k], ring_a[n], ring_b[n], ring_b[k]], mat, [(ua, v0), (ub, v0), (ub, v1), (ua, v1)])
        if cap:
            cu, cv = (u0 + u1) / 2, (v0 + v1) / 2
            self.face(list(reversed(ring_a)), mat, [(cu, cv)] * sides)
            self.face(ring_b, mat, [(cu, cv)] * sides)
        return length

    def card(self, base, yaw_deg, width, height, tilt_deg, mat, rect, up_normal=0.75, bend=0.0):
        """Vertical-ish card standing on base, facing yaw, tilted tilt_deg away from vertical (outward along its
        normal). Two quads (lower/upper) so it can bend. Custom normals lean to +Z so both sides light alike."""
        yaw = math.radians(yaw_deg)
        right = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        out = Vector((-math.sin(yaw), math.cos(yaw), 0.0))
        tilt = math.radians(tilt_deg)
        b = Vector(base)
        mid_dir = (Vector((0, 0, 1)) * math.cos(tilt * 0.6) + out * math.sin(tilt * 0.6)).normalized()
        top_dir = (Vector((0, 0, 1)) * math.cos(tilt) + out * math.sin(tilt)).normalized()
        m = b + mid_dir * height * 0.5
        t = m + top_dir * height * 0.5 + out * bend
        hw = width / 2
        u0, v0, u1, v1 = rect
        vm = (v0 + v1) / 2
        nrm = (Vector((0, 0, 1)) * up_normal + out * (1 - up_normal))
        self.quad_uv([b - right * hw, b + right * hw, m + right * hw, m - right * hw], mat, (u0, v0, u1, vm), nrm)
        self.quad_uv([m - right * hw, m + right * hw, t + right * hw * 0.9, t - right * hw * 0.9], mat, (u0, vm, u1, v1), nrm)

    def to_object(self, uv_lightmap=True):
        me = bpy.data.meshes.new(self.name)
        me.from_pydata([tuple(v) for v in self.verts], [], [f[0] for f in self.faces])
        uv = me.uv_layers.new(name='UVMap')
        loop = 0
        for poly, (idx, mat, uvs) in zip(me.polygons, self.faces):
            poly.material_index = mat
            for k in range(len(idx)):
                uv.data[poly.loop_start + k].uv = uvs[k]
        for name in self.materials:
            me.materials.append(bpy.data.materials.get(name) or bpy.data.materials.new(name))
        me.validate(clean_customdata=False)
        me.update()
        if self.normals:
            normals = []
            for fi, poly in enumerate(me.polygons):
                n = self.normals.get(fi)
                for _ in range(poly.loop_total):
                    normals.append(tuple(n) if n is not None else (0.0, 0.0, 0.0))
            me.normals_split_custom_set(normals)
        if uv_lightmap:
            lm = me.uv_layers.new(name='LightMapUV')
            for i, d in enumerate(uv.data):
                lm.data[i].uv = d.uv
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def hull_object(name, points):
    bm = bmesh.new()
    vs = [bm.verts.new(Vector(p)) for p in points]
    bmesh.ops.convex_hull(bm, input=vs)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def box_points(lo, hi, xform=None):
    pts = [Vector((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    return [xform @ p for p in pts] if xform is not None else pts


def export_fbx(obj, path, hulls=()):
    """hulls: list of point lists; written as UCX_<name>_NN."""
    objs = [obj] + [hull_object('UCX_%s_%02d' % (obj.name, k), pts) for k, pts in enumerate(hulls)]
    for o in bpy.context.scene.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = obj
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             object_types={'MESH'}, use_mesh_modifiers=False, mesh_smooth_type='FACE',
                             bake_space_transform=False, add_leaf_bones=False, use_custom_props=False)
    for o in objs[1:]:
        bpy.data.objects.remove(o)
    me = obj.data
    xs = [v.co for v in me.vertices]
    lo = [min(p[i] for p in xs) for i in range(3)]
    hi = [max(p[i] for p in xs) for i in range(3)]
    return dict(fbx=path, min=[round(v, 1) for v in lo], max=[round(v, 1) for v in hi],
                tris=sum(len(p.vertices) - 2 for p in me.polygons), hulls=len(hulls),
                slots=[m.name for m in me.materials])


def image_material(name, image_path, alpha=False, tint=None, roughness=0.85):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = roughness
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(image_path, check_existing=True)
    if tint is not None:
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.blend_type = 'MULTIPLY'
        mix.inputs['Factor'].default_value = 1.0
        mix.inputs[7].default_value = (tint[0], tint[1], tint[2], 1.0)
        nt.links.new(tex.outputs['Color'], mix.inputs[6])
        nt.links.new(mix.outputs[2], bsdf.inputs['Base Color'])
    else:
        nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    if alpha:
        nt.links.new(tex.outputs['Alpha'], bsdf.inputs['Alpha'])
        try:
            mat.surface_render_method = 'DITHERED'
        except Exception:  # noqa: BLE001
            pass
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    return mat


def flat_material(name, rgb, roughness=0.8, metallic=0.0):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    return mat


def render_preview(path, target=(0, 0, 0), distance=2500.0, elevation_deg=55.0, azimuth_deg=-60.0, lens=50.0,
                   ortho_scale=None, resolution=(1280, 800), ground=True, ground_rgb=(0.17, 0.16, 0.12), ground_z=-1.0):
    """EEVEE still of the scene from an RTS-like high angle. Scene objects are rendered as they are."""
    scene = bpy.context.scene
    target = Vector(target)
    if ground:
        bpy.ops.mesh.primitive_plane_add(size=200000, location=(target.x, target.y, ground_z))
        plane = bpy.context.active_object
        plane.data.materials.append(flat_material('Preview_Ground', ground_rgb, 1.0))
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
    sun_data.energy = 3.5
    sun = bpy.data.objects.new('PreviewSun', sun_data)
    sun.rotation_euler = (math.radians(40), math.radians(10), math.radians(-35))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('PreviewWorld')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.6, 0.66, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
    scene.world = world
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
    scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in engines else (
        'BLENDER_EEVEE' if 'BLENDER_EEVEE' in engines else 'CYCLES')
    if scene.render.engine == 'CYCLES':
        scene.cycles.samples = 32
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    return path
