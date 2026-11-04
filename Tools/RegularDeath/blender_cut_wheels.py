"""Cuts one road wheel and the drive sprocket out of the exported Panzer IV and T-34 track meshes for the WheelThrow
regular death. Each wheel is a tyre ring plus two side discs (separate loose parts); they are joined, centred on the
axle and exported as static-mesh FBX next to this script (WheelExport/SM_WreckWheel_*.fbx). UVs stay in the tracks'
wheel atlas, so the wheel material only needs that atlas texture.
  Blender5.0.exe -b --factory-startup --python blender_cut_wheels.py
"""
import os
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
EXPORT_DIR = os.path.join(HERE, 'WheelExport')
# (source fbx, output name, ring centre x, ring centre z) in Blender metres; rings on the -Y side are used.
CUTS = [
    ('PanzerIV_Tracks.fbx', 'SM_WreckWheel_Ger_Road', 0.1, 0.3),
    ('PanzerIV_Tracks.fbx', 'SM_WreckWheel_Ger_Sprocket', -2.6, 0.7),
    ('T34_Tracks.fbx', 'SM_WreckWheel_Rus_Road', -1.0, 0.5),
    ('T34_Tracks.fbx', 'SM_WreckWheel_Rus_Sprocket', -2.7, 0.6),
]
# Parts whose centre is this close (m) in X and Z to the ring belong to the same wheel.
SAME_WHEEL_XZ = 0.06
# The two side discs sit within this distance (m) of the ring along the axle.
SAME_WHEEL_Y = 0.25
MATCH_RADIUS = 0.15


def bounds(obj):
    world = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector([min(p[i] for p in world) for i in range(3)])
    hi = Vector([max(p[i] for p in world) for i in range(3)])
    return lo, hi


def load_parts(fbx):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(EXPORT_DIR, fbx))
    for obj in [o for o in bpy.context.scene.objects if o.type != 'MESH']:
        bpy.data.objects.remove(obj)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for obj in meshes:
        # Bake the armature-parented world transform into the vertices; the parts are measured in world space.
        world = obj.matrix_world.copy()
        obj.modifiers.clear()
        obj.parent = None
        obj.data.transform(world)
        obj.matrix_world = Matrix.Identity(4)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.separate(type='LOOSE')
    bpy.ops.object.mode_set(mode='OBJECT')
    parts = []
    for obj in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
        lo, hi = bounds(obj)
        parts.append((obj, (lo + hi) / 2, hi - lo))
    return parts


def cut(parts, name, ring_x, ring_z):
    rings = [p for p in parts if p[2].y > 0.1 and p[1].y < 0
             and abs(p[1].x - ring_x) < MATCH_RADIUS and abs(p[1].z - ring_z) < MATCH_RADIUS]
    if len(rings) != 1:
        raise RuntimeError('RTS_WCUT %s: %d rings near x=%s z=%s' % (name, len(rings), ring_x, ring_z))
    ring_centre = rings[0][1]
    wheel = [p[0] for p in parts if abs(p[1].x - ring_centre.x) < SAME_WHEEL_XZ
             and abs(p[1].z - ring_centre.z) < SAME_WHEEL_XZ and abs(p[1].y - ring_centre.y) < SAME_WHEEL_Y]
    bpy.ops.object.select_all(action='DESELECT')
    for obj in wheel:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = wheel[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    lo, hi = bounds(joined)
    centre = (lo + hi) / 2
    joined.data.transform(Matrix.Translation(-centre))
    joined.location = (0, 0, 0)
    joined.name = name
    joined.data.name = name
    size = hi - lo
    print('RTS_WCUT %s parts=%d tris=%d size_m=(%.2f, %.2f, %.2f)' % (
        name, len(wheel), sum(len(p.vertices) - 2 for p in joined.data.polygons), size.x, size.y, size.z))
    bpy.ops.object.select_all(action='DESELECT')
    joined.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(EXPORT_DIR, name + '.fbx'), use_selection=True,
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE',
                             axis_forward='-Z', axis_up='Y', add_leaf_bones=False, bake_anim=False)


for fbx, name, ring_x, ring_z in CUTS:
    cut(load_parts(fbx), name, ring_x, ring_z)
