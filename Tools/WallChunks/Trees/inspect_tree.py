"""Inspect the UE export of a dead tree: objects, LOD0 stats and the UV0 triangles (dumped to JSON for plotting).
blender -b --factory-startup -P inspect_tree.py -- <in.fbx> <out.json>
"""
import json
import re
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=args[0])
for obj in bpy.context.scene.objects:
    print('OBJ', obj.name, obj.type, obj.parent.name if obj.parent else None, tuple(obj.scale), tuple(obj.dimensions),
          len(obj.data.polygons) if obj.type == 'MESH' else '')
lod = re.compile(r'_LOD([1-9]\d*)$')
lod0 = [o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.name.startswith('UCX_') and not lod.search(o.name)][0]
bpy.ops.object.select_all(action='DESELECT')
lod0.select_set(True)
bpy.context.view_layer.objects.active = lod0
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mesh = lod0.data
print('LOD0', lod0.name, 'dims', tuple(lod0.dimensions), 'uv', [l.name for l in mesh.uv_layers], 'mats',
      [m.name for m in mesh.materials if m])
uv = mesh.uv_layers[0].data
tris = []
mesh.calc_loop_triangles()
for tri in mesh.loop_triangles:
    corners = [tuple(round(c, 3) for c in mesh.vertices[v].co) for v in tri.vertices]
    uvs = [tuple(round(c, 5) for c in uv[l].uv) for l in tri.loops]
    tris.append({'p': corners, 'uv': uvs, 'n': tuple(round(c, 3) for c in tri.normal)})
with open(args[1], 'w') as handle:
    json.dump(tris, handle)
print('TRIS', len(tris))
