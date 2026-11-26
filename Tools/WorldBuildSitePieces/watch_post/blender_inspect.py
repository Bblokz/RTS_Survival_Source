"""Blender (headless): prints the parts of the exported guard tower and renders two CPU preview pictures.

blender.exe -b --factory-startup -P blender_inspect.py -- <fbx> <out_dir>
"""
import math
import sys

import bpy
from mathutils import Vector

fbx_path, out_dir = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path)
objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for obj in objects:
    print("OBJ", obj.name, "loc", tuple(obj.location), "rot", tuple(obj.rotation_euler), "scale", tuple(obj.scale))
    mesh = obj.data
    world = obj.matrix_world
    per_material = {}
    for polygon in mesh.polygons:
        per_material.setdefault(polygon.material_index, []).append(polygon)
    for index, polygons in per_material.items():
        points = [world @ mesh.vertices[v].co for p in polygons for v in p.vertices]
        low = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
        high = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
        tris = sum(len(p.vertices) - 2 for p in polygons)
        name = mesh.materials[index].name if index < len(mesh.materials) and mesh.materials[index] else None
        print("MAT %d %s tris=%d min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f)" % (
            index, name, tris, low.x, low.y, low.z, high.x, high.y, high.z))
    # Base ring: vertices within 5 cm of the lowest point.
    points = [world @ v.co for v in mesh.vertices]
    bottom = min(p.z for p in points)
    base = [p for p in points if p.z < bottom + 0.05]
    print("BASE z=%.2f n=%d x %.1f..%.1f y %.1f..%.1f" % (bottom, len(base), min(p.x for p in base),
                                                          max(p.x for p in base), min(p.y for p in base),
                                                          max(p.y for p in base)))
    for level in range(0, 12):
        z0 = bottom + level * 1.0
        layer = [p for p in points if z0 <= p.z < z0 + 1.0]
        if layer:
            print("LAYER z %.0f..%.0f n=%d x %.1f..%.1f y %.1f..%.1f" % (
                z0, z0 + 1, len(layer), min(p.x for p in layer), max(p.x for p in layer),
                min(p.y for p in layer), max(p.y for p in layer)))

colors = [(0.8, 0.6, 0.3, 1), (0.2, 0.6, 0.2, 1), (0.3, 0.3, 0.9, 1), (0.9, 0.2, 0.2, 1)]
for obj in objects:
    for index, slot in enumerate(obj.material_slots):
        material = bpy.data.materials.new("preview_%d" % index)
        material.use_nodes = True
        material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = colors[index % 4]
        slot.material = material

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 16
scene.render.resolution_x = 700
scene.render.resolution_y = 700
world = bpy.data.worlds.new("w")
world.color = (0.6, 0.6, 0.6)
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3.0
sun.rotation_euler = (math.radians(40), 0, math.radians(30))
scene.collection.objects.link(sun)
camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
camera.data.type = "ORTHO"
camera.data.ortho_scale = 13.0
scene.collection.objects.link(camera)
scene.camera = camera
centre = Vector((0, 0, 5.3))
for name, direction in (("side", Vector((1, -1, 0.35))), ("top", Vector((0.0001, -0.36, 1.0)))):
    direction.normalize()
    camera.location = centre + direction * 30.0
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = out_dir + "/source_" + name + ".png"
    bpy.ops.render.render(write_still=True)
