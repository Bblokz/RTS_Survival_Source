"""Lists the loose parts of an exported track mesh with size, axis and material, to pick the road wheel.
Blender5.0.exe -b --python blender_list_wheel_parts.py -- <fbx>"""
import sys
from collections import Counter
import bpy

fbx = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for obj in meshes:
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE')
bpy.ops.object.mode_set(mode='OBJECT')
rows = []
for obj in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
    world = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = [min(p[i] for p in world) for i in range(3)]
    hi = [max(p[i] for p in world) for i in range(3)]
    size = tuple(round(hi[i] - lo[i], 1) for i in range(3))
    centre = tuple(round((hi[i] + lo[i]) / 2, 1) for i in range(3))
    mats = Counter(obj.material_slots[p.material_index].name if obj.material_slots else '-' for p in obj.data.polygons)
    rows.append((size, centre, len(obj.data.polygons), dict(mats), obj.name))
rows.sort(key=lambda r: (r[0][2], r[1][0]))
print('PARTS', len(rows), 'scale', [round(s, 3) for s in meshes[0].scale] if meshes else None)
for r in rows:
    print('PART size=%s centre=%s tris=%d mats=%s %s' % r)
