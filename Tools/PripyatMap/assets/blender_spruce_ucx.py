"""Blender (background): add a UCX trunk box to each Polesie dead spruce LOD0 and re-export it for Unreal.

The Polesie kit ships without collision ("use simple trunk collision in Unreal"). AWpoTree crushing needs a hull on the
tree mesh, as the Ore Mountains kit has (one trunk box, r0 x 1.25, up to 3.2 m). Output: PolesieDeadSpruces/UE/*.FBX
(source FBX untouched), in the Ore kit's FBX convention: scene unit 0.01, vertices in cm, identity objects, FBX_SCALE_NONE.

blender --background --factory-startup --python blender_spruce_ucx.py
"""
import json
import os

import bmesh
import bpy
import numpy as np

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PolesieDeadSpruces/'
OUT = SRC + 'UE/'
NAMES = ['SM_DeadSpruce_01_TallSentinel', 'SM_DeadSpruce_02_BogLean', 'SM_DeadSpruce_03_SnappedCrown',
         'SM_DeadSpruce_04_WindFlag', 'SM_DeadSpruce_05_OldGiant', 'SM_DeadSpruce_06_DrownedYoung',
         'SM_DeadSpruce_07_ForkedSnag', 'SM_DeadSpruce_08_Uprooted']
HULL_TOP_CM = 320.0
report = {}


def world_cm(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    mw = np.array(obj.matrix_world)
    pts = (np.c_[co, np.ones(len(co))] @ mw.T)[:, :3] * 100.0
    return pts


def standing_hull(p):
    low = p[(p[:, 2] > 20) & (p[:, 2] < 120)]
    rad = np.hypot(low[:, 0], low[:, 1])
    core = low[rad < 80]
    cx, cy = np.median(core[:, 0]), np.median(core[:, 1])
    r = np.percentile(np.hypot(core[:, 0] - cx, core[:, 1] - cy), 90) * 1.25
    r = float(min(max(r, 18.0), 60.0))
    top = float(min(p[:, 2].max(), HULL_TOP_CM))
    return (cx - r, cy - r, 0.0), (cx + r, cy + r, top)


def lying_hull(p):
    length = p[:, 0].max()
    mid = p[(p[:, 0] > 0.25 * length) & (p[:, 0] < 0.6 * length)]
    y0, y1 = np.percentile(mid[:, 1], 15), np.percentile(mid[:, 1], 85)
    z1 = np.percentile(mid[:, 2], 85)
    return (0.0, float(y0), 0.0), (float(0.8 * length), float(y1), float(min(max(z1, 40.0), 120.0)))


def box_object(name, lo, hi):
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    bmesh.ops.convex_hull(bm, input=vs)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


os.makedirs(OUT, exist_ok=True)
for name in NAMES:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 0.01
    bpy.ops.import_scene.fbx(filepath=SRC + name + '.FBX')
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    src = meshes[0]
    # Bake the transform (normals included): vertices in cm, identity object.
    for o in bpy.context.scene.objects:
        o.select_set(o == src)
    bpy.context.view_layer.objects.active = src
    # With the scene unit at 0.01 before the import, the importer already gives cm vertices and scale 1.
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    src.name = name
    me = src.data
    pts = world_cm(src) / 100.0
    lo, hi = lying_hull(pts) if 'Uprooted' in name else standing_hull(pts)
    hull = box_object('UCX_%s_00' % name, lo, hi)
    for o in bpy.context.scene.objects:
        o.select_set(o in (src, hull))
    bpy.context.view_layer.objects.active = src
    bpy.ops.export_scene.fbx(filepath=OUT + name + '.FBX', use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             object_types={'MESH'}, use_mesh_modifiers=False, mesh_smooth_type='FACE',
                             bake_space_transform=False, add_leaf_bones=False, use_custom_props=False)
    report[name] = dict(bounds_min=[round(v, 1) for v in pts.min(0)], bounds_max=[round(v, 1) for v in pts.max(0)],
                        hull_min=[round(v, 1) for v in lo], hull_max=[round(v, 1) for v in hi],
                        slots=[m.name for m in me.materials], uvs=[u.name for u in me.uv_layers])
with open(OUT + 'ucx_report.json', 'w') as handle:
    json.dump(report, handle, indent=1)
print('SPRUCE_UCX_DONE', json.dumps(report))
