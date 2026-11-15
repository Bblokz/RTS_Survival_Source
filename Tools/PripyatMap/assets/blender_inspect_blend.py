"""Blender (background): list objects of the open .blend (name, type, dims, materials, images, collections, visibility).

blender --background <file.blend> --python blender_inspect_blend.py
"""
import bpy

for obj in bpy.data.objects:
    info = [obj.name, obj.type, 'hide=%s/%s' % (obj.hide_viewport, obj.hide_render),
            'dims=%s' % [round(v, 2) for v in obj.dimensions], 'loc=%s' % [round(v, 2) for v in obj.location],
            'coll=%s' % [c.name for c in obj.users_collection]]
    if obj.type == 'MESH':
        info.append('tris~%d' % sum(len(p.vertices) - 2 for p in obj.data.polygons))
        info.append('mats=%s' % [m.name if m else None for m in obj.data.materials])
        info.append('uvs=%s' % [u.name for u in obj.data.uv_layers])
        info.append('mods=%s' % [m.type for m in obj.modifiers])
    print('OBJ', ' | '.join(info))
for img in bpy.data.images:
    print('IMG', img.name, img.filepath, img.size[:], 'packed' if img.packed_file else '')
for mat in bpy.data.materials:
    print('MAT', mat.name, [n.type for n in mat.node_tree.nodes] if mat.use_nodes else '')
print('UNIT', bpy.context.scene.unit_settings.scale_length, bpy.context.scene.unit_settings.system)
