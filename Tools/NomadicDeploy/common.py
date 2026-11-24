# Shared Blender helpers for the deploy-animation prototypes (PROTOTYPE, safe to delete with the whole folder).
# Loads the exported SD HQ building mesh, finds its loose parts and groups them into deploy pieces.
# Coordinates: everything here is in Unreal cm with Unreal axes (X forward, Y right, Z up); Blender Y = -Unreal Y.
import math
import random

import bmesh
import bpy
from mathutils import Vector

TO_CM = 100.0


def load_fbx(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def to_ue(co):
    return Vector((co.x * TO_CM, -co.y * TO_CM, co.z * TO_CM))


def from_ue(v):
    return Vector((v.x / TO_CM, -v.y / TO_CM, v.z / TO_CM))


def export_fbx(path):
    # export settings of the proven hull round trip (Saved/StrikeDivision_BuildingLook/sd_blender.py)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, object_types={'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', add_leaf_bones=False,
                             bake_anim=False, axis_forward='-Z', axis_up='Y', use_custom_props=False)


class Parts:
    """Loose parts of the mesh: faces connected through shared vertex positions (FBX splits verts on seams)."""

    def __init__(self, obj):
        me = obj.data
        self.obj = obj
        n = len(me.vertices)
        parent = list(range(n))

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a

        def union(a, b):
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[ra] = rb

        key_to_vert = {}
        for v in me.vertices:
            k = (round(v.co.x, 4), round(v.co.y, 4), round(v.co.z, 4))
            if k in key_to_vert:
                union(v.index, key_to_vert[k])
            else:
                key_to_vert[k] = v.index
        for p in me.polygons:
            vs = p.vertices
            for i in range(1, len(vs)):
                union(vs[0], vs[i])
        roots = {}
        self.vert_part = [0] * n
        for v in range(n):
            r = find(v)
            if r not in roots:
                roots[r] = len(roots)
            self.vert_part[v] = roots[r]
        self.count = len(roots)
        self.poly_part = [self.vert_part[p.vertices[0]] for p in me.polygons]
        lo = [Vector((1e9, 1e9, 1e9)) for _ in range(self.count)]
        hi = [Vector((-1e9, -1e9, -1e9)) for _ in range(self.count)]
        self.faces = [0] * self.count
        self.material = [{} for _ in range(self.count)]
        for v in me.vertices:
            u = to_ue(v.co)
            i = self.vert_part[v.index]
            for a in range(3):
                lo[i][a] = min(lo[i][a], u[a])
                hi[i][a] = max(hi[i][a], u[a])
        for p in me.polygons:
            i = self.poly_part[p.index]
            self.faces[i] += 1
            self.material[i][p.material_index] = self.material[i].get(p.material_index, 0) + 1
        self.lo, self.hi = lo, hi

    def main_material(self, i):
        return max(self.material[i], key=self.material[i].get)
