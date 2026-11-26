"""Blender (headless, CPU): flat-colour preview renders of OBJ dumps written by the UE scripts (UE coordinates).

blender.exe -b --factory-startup -P blender_preview.py -- <out dir> <prefix> <ortho scale> <look z> <obj> [<obj> ...]
Each OBJ uses "usemtl m<id>" material groups. UE (X fwd, Y right, Z up, left-handed) -> Blender (x, -y, z).
Views: map (looking along UE +X, 70 degrees down), top, side (from UE -X/-Y, 25 degrees down).
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir, prefix, ortho, look_z = args[0], args[1], float(args[2]), float(args[3])
objs = args[4:]
UNIT = 0.01   # OBJ values are map units x 100
PALETTE = [(0.72, 0.55, 0.25), (0.85, 0.8, 0.62), (0.35, 0.33, 0.3), (0.95, 0.75, 0.05), (0.5, 0.6, 0.45),
           (0.15, 0.15, 0.16), (0.6, 0.3, 0.2), (0.3, 0.45, 0.6), (0.9, 0.9, 0.9), (0.4, 0.2, 0.5)]

bpy.ops.wm.read_factory_settings(use_empty=True)
materials = {}


def material(index):
    if index not in materials:
        mat = bpy.data.materials.new("m%d" % index)
        mat.use_nodes = True
        mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = PALETTE[index % len(PALETTE)] + (1,)
        materials[index] = mat
    return materials[index]


for path in objs:
    verts, faces, face_mat = [], [], []
    current = 0
    with open(path) as handle:
        for line in handle:
            if line.startswith("v "):
                _, x, y, z = line.split()
                verts.append((float(x) * UNIT, -float(y) * UNIT, float(z) * UNIT))
            elif line.startswith("usemtl"):
                current = int(line.split()[1][1:])
            elif line.startswith("f "):
                a, b, c = (int(t) - 1 for t in line.split()[1:4])
                faces.append((a, c, b))   # mirror flips winding
                face_mat.append(current)
    mesh = bpy.data.meshes.new(path.split("/")[-1].split("\\")[-1])
    mesh.from_pydata(verts, [], faces)
    used = sorted(set(face_mat))
    for index in used:
        mesh.materials.append(material(index))
    for poly, index in zip(mesh.polygons, face_mat):
        poly.material_index = used.index(index)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)

# Ground plane (hex plate stand-in), 57.6 units wide.
bpy.ops.mesh.primitive_circle_add(vertices=6, radius=28.8, fill_type="NGON", location=(0, 0, -0.01))
plate = bpy.context.active_object
plate_mat = bpy.data.materials.new("plate")
plate_mat.use_nodes = True
plate_mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.08, 0.2, 0.1, 1)
plate.data.materials.append(plate_mat)
bpy.ops.mesh.primitive_circle_add(vertices=64, radius=20.0, location=(0, 0, 0.005))

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 12
scene.render.resolution_x = 800
scene.render.resolution_y = 800
world = bpy.data.worlds.new("w")
world.color = (0.45, 0.45, 0.45)
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3.5
sun.rotation_euler = (math.radians(35), 0, math.radians(-40))
scene.collection.objects.link(sun)
camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
camera.data.type = "ORTHO"
camera.data.ortho_scale = ortho
camera.data.clip_end = 2000.0
scene.collection.objects.link(camera)
scene.camera = camera
look_at = Vector((0, 0, look_z))
# UE +X looking direction = Blender +X; camera sits at -X.
for name, direction in (("map", Vector((-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70))))),
                        ("top", Vector((-0.0001, 0.0, 1.0))),
                        ("side", Vector((-0.75, 0.75, 0.47)))):
    direction.normalize()
    camera.location = look_at + direction * 300.0
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = out_dir + "/" + prefix + "_" + name + ".png"
    bpy.ops.render.render(write_still=True)
