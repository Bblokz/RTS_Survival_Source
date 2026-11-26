"""Blender (headless): look at the exported sources (base + open roof, closed roof, Ju 87) with flat slot colours.

blender.exe -b --factory-startup -P blender_inspect.py -- <airfield_B dir>
Out: <dir>/inspect_*.png, numbers on stdout (INSPECT lines).
"""
import math
import sys

import bpy
from mathutils import Vector

folder = sys.argv[sys.argv.index("--") + 1]
PALETTE = [(0.8, 0.6, 0.3, 1), (0.3, 0.5, 0.8, 1), (0.8, 0.3, 0.3, 1), (0.3, 0.8, 0.3, 1), (0.7, 0.7, 0.7, 1),
           (0.6, 0.3, 0.7, 1)]


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=folder + "/" + name + ".obj", forward_axis="Y", up_axis="Z")
    objects = [o for o in bpy.data.objects if o not in before]
    for obj in objects:
        for index, slot in enumerate(obj.material_slots):
            material = slot.material
            if material is None:
                continue
            material.use_nodes = True
            colour = PALETTE[int(material.name.split("_")[0]) % len(PALETTE)] if material.name[0].isdigit() else (1, 0, 1, 1)
            material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = colour
        print("INSPECT %s verts %d slots %s" % (obj.name, len(obj.data.vertices), [s.material.name for s in obj.material_slots]))
    return objects


def render(name, look_at, direction, ortho):
    scene = bpy.context.scene
    camera = scene.camera
    camera.data.ortho_scale = ortho
    direction = Vector(direction).normalized()
    camera.location = Vector(look_at) + direction * 100.0
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = folder + "/inspect_" + name + ".png"
    bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 8
scene.render.resolution_x = 700
scene.render.resolution_y = 700
world = bpy.data.worlds.new("w")
world.color = (0.6, 0.6, 0.6)
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3.0
sun.rotation_euler = (math.radians(40), 0, math.radians(-60))
scene.collection.objects.link(sun)
camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
camera.data.type = "ORTHO"
camera.data.clip_end = 1000
scene.collection.objects.link(camera)
scene.camera = camera

building = load("base") + load("roof_open")
map_dir = (-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70)))
render("open_map", (-3, 0, 3), map_dir, 28)
render("open_side", (-3, 0, 3), (-1.0, -0.8, 0.5), 30)
render("open_front", (-3, 0, 4), (-1.0, 0.0, 0.05), 28)
render("open_top", (-3, 0, 3), (0, 0, 1), 28)
for obj in building:
    obj.hide_render = True
closed = load("base") + load("roof_ref")
render("closed_side", (-3, 0, 3), (-1.0, -0.8, 0.5), 30)
for obj in closed:
    obj.hide_render = True
load("stuka")
render("stuka_top", (-1.5, 0, 2), (0, 0, 1), 14)
