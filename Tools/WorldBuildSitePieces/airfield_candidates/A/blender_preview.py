"""Blender (headless, CPU Cycles): preview pictures of an airfield candidate A FBX export, flat colour per material slot
or the vertex colours, seen from the map camera (looking along +X, 70 degrees down) and from the side.

blender.exe -b --factory-startup -P blender_preview.py -- <fbx> <out prefix> [vertexcolor]
"""
import math
import sys

import bpy
from mathutils import Vector

arguments = sys.argv[sys.argv.index("--") + 1:]
fbx, prefix = arguments[0], arguments[1]
use_vertex_color = len(arguments) > 2 and arguments[2] == "vertexcolor"
final_palette = len(arguments) > 2 and arguments[2] == "final"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
PALETTE = [(0.75, 0.72, 0.62), (0.55, 0.45, 0.25), (0.2, 0.35, 0.6), (1, 0, 1), (0.6, 0.2, 0.15), (0.85, 0.65, 0.2),
           (0.3, 0.6, 0.65), (0.75, 0.35, 0.55), (1, 0, 1), (0.25, 0.5, 0.2), (0.9, 0.9, 0.9), (0.95, 0.8, 0.1)]
if final_palette:
    # Body, Detail, Floor, Flaps, Stuka (texture stand-in), Accent: the linear colours of the UE instances.
    PALETTE = [(0.68, 0.46, 0.165), (0.36, 0.25, 0.10), (0.075, 0.075, 0.07), (0.87, 0.77, 0.55), (0.5, 0.42, 0.25),
               (1.0, 0.68, 0.02)]
for obj in meshes:
    print("PREVIEW object %s verts %d faces %d dims %s slots %s" % (
        obj.name, len(obj.data.vertices), len(obj.data.polygons), tuple(round(d, 3) for d in obj.dimensions),
        [s.material.name if s.material else None for s in obj.material_slots]))
    for index, slot in enumerate(obj.material_slots):
        material = bpy.data.materials.new("preview_%d" % index)
        material.use_nodes = True
        nodes = material.node_tree.nodes
        shader = nodes["Principled BSDF"]
        if use_vertex_color and obj.data.color_attributes:
            attribute = nodes.new("ShaderNodeVertexColor")
            attribute.layer_name = obj.data.color_attributes[0].name
            material.node_tree.links.new(attribute.outputs["Color"], shader.inputs["Base Color"])
        else:
            color = PALETTE[index % len(PALETTE)]
            shader.inputs["Base Color"].default_value = (color[0], color[1], color[2], 1)
        if final_palette and slot.material and slot.material.name.startswith("MI_AirfieldA_Stuka") and \
                len(arguments) >= 8:
            # Same object-space accent mask as M_AirfieldA_Paint (UE units / 100 = metres, UE Y = -Blender Y).
            tip_y, nose_x, band_x0, band_x1, band_half_y = [float(value) / 100.0 for value in arguments[3:8]]
            links = material.node_tree.links
            geometry = nodes.new("ShaderNodeNewGeometry")
            separate = nodes.new("ShaderNodeSeparateXYZ")
            links.new(geometry.outputs["Position"], separate.inputs[0])

            def math_node(operation, a, b):
                node = nodes.new("ShaderNodeMath")
                node.operation = operation
                for slot_index, value in enumerate((a, b)):
                    if isinstance(value, float):
                        node.inputs[slot_index].default_value = value
                    else:
                        links.new(value, node.inputs[slot_index])
                return node.outputs[0]
            absolute_y = math_node("ABSOLUTE", separate.outputs["Y"], 0.0)
            tip = math_node("GREATER_THAN", absolute_y, tip_y)
            nose = math_node("LESS_THAN", separate.outputs["X"], nose_x)
            band = math_node("MULTIPLY", math_node("GREATER_THAN", separate.outputs["X"], band_x0),
                             math_node("LESS_THAN", separate.outputs["X"], band_x1))
            band = math_node("MULTIPLY", band, math_node("LESS_THAN", absolute_y, band_half_y))
            mask = math_node("MINIMUM", math_node("ADD", math_node("ADD", tip, nose), band), 1.0)
            mix = nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            links.new(mask, mix.inputs["Factor"])
            mix.inputs[6].default_value = (color[0], color[1], color[2], 1)
            mix.inputs[7].default_value = (1.0, 0.68, 0.02, 1)
            links.new(mix.outputs[2], shader.inputs["Base Color"])
        shader.inputs["Roughness"].default_value = 0.8
        slot.material = material

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 24
scene.render.resolution_x = 900
scene.render.resolution_y = 900
world = bpy.data.worlds.new("w")
world.color = (0.12, 0.2, 0.14)
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3.0
sun.rotation_euler = (math.radians(40), 0, math.radians(-60))
scene.collection.objects.link(sun)
ground = bpy.data.meshes.new("ground")
ground.from_pydata([(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)], [], [(0, 1, 2, 3)])
ground_object = bpy.data.objects.new("ground", ground)
ground_object.scale = (0.4, 0.4, 1)
ground_object.location = (0, 0, -0.0005)
plate = bpy.data.materials.new("plate")
plate.use_nodes = True
plate.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.05, 0.12, 0.06, 1)
ground.materials.append(plate)
scene.collection.objects.link(ground_object)
# 40 unit footprint circle (0.2 m radius) as a thin ring.
bpy.ops.mesh.primitive_torus_add(major_radius=0.2, minor_radius=0.002, location=(0, 0, 0.001))
camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
camera.data.type = "ORTHO"
camera.data.ortho_scale = 0.5
scene.collection.objects.link(camera)
scene.camera = camera
look_at = Vector((0, 0, 0.06))
# FBX import: Unreal +X -> Blender +X? Unreal is left-handed; the importer maps Unreal Y to Blender -Y.
for name, direction in (("map", Vector((-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70))))),
                        ("side", Vector((-1.0, 0.7, 0.45))), ("top", Vector((0.0, 0.0001, 1.0)))):
    direction.normalize()
    camera.location = look_at + direction * 5.0
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = prefix + "_" + name + ".png"
    bpy.ops.render.render(write_still=True)
