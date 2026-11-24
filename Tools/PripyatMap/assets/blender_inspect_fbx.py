"""Blender (background): import FBX files one by one and print objects, materials, size, triangles, UV layers.

blender --background --factory-startup --python blender_inspect_fbx.py -- out.json file1.fbx file2.fbx ...
"""
import json
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
out_path, files = args[0], args[1:]
report = {}
for path in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    entry = []
    for obj in bpy.context.scene.objects:
        item = {"name": obj.name, "type": obj.type, "loc": list(obj.location), "rot": list(obj.rotation_euler),
                "scale": list(obj.scale)}
        if obj.type == "MESH":
            mesh = obj.data
            mesh.calc_loop_triangles()
            xs = [(obj.matrix_world @ v.co) for v in mesh.vertices]
            if xs:
                item["min"] = [min(p[i] for p in xs) for i in range(3)]
                item["max"] = [max(p[i] for p in xs) for i in range(3)]
            item["tris"] = len(mesh.loop_triangles)
            item["materials"] = [m.name if m else None for m in mesh.materials]
            item["uvs"] = [uv.name for uv in mesh.uv_layers]
            item["vcols"] = [c.name for c in mesh.color_attributes]
            imgs = []
            for m in mesh.materials:
                if m and m.use_nodes:
                    for n in m.node_tree.nodes:
                        if n.type == "TEX_IMAGE" and n.image:
                            imgs.append(n.image.filepath)
            item["images"] = imgs
        entry.append(item)
    report[path] = entry
with open(out_path, "w") as handle:
    json.dump(report, handle, indent=1)
print("INSPECT_DONE", len(report))
