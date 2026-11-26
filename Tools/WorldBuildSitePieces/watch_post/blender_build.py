"""Blender (headless): turns the exported Soviet guard tower into the watch post build site piece.

blender.exe -b --factory-startup -P blender_build.py -- <source fbx> <out fbx> <out preview dir>

- Scale so the piece is about 51 units tall and fits a circle of 40 units around the pivot.
- Pivot at the centre of the four legs, base at Z = 0.
- Turned so the ladder and the cabin door face Unreal -X (the world map camera looks along +X).
- Material slots: WP_Body (legs, cabin walls), WP_Roof (cabin roof), WP_Lattice (bracing, deck, rails, ladder),
  WP_Net (rail infill). The paint is in Unreal; these are names only.
"""
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

source_fbx, out_fbx, preview_dir = sys.argv[sys.argv.index("--") + 1:][:3]
TARGET_HEIGHT_M = 0.51          # Unreal units are centimetres: 51 units.
MAX_RADIUS_M = 0.192            # Footprint circle 40 units: radius 20, minus a little margin.
PIECE_NAME = "SM_BuildSitePiece_WatchPost"
# Source slot name -> piece slot name. The roof is cut out of the container slot by height and facing.
SLOT_NAMES = {"MI_metal": "WP_Lattice", "M_Russian_CamoPattern_Tent": "WP_Net", "M_container_blue": "WP_Body"}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=source_fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
assert len(meshes) == 1, "expected one mesh, got %d" % len(meshes)
obj = meshes[0]
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

mesh = obj.data
# Ladder (source -Y in Blender) to -X: turn -90 degrees about Z.
mesh.transform(Matrix.Rotation(math.radians(-90.0), 4, "Z"))

points = [v.co.copy() for v in mesh.vertices]
bottom = min(p.z for p in points)
base = [p for p in points if p.z < bottom + 0.05]
# Leg centre from the four legs only: the ladder foot is on -X, so use the +X/-X extremes of the legs at the base,
# which are symmetric in Y; X centre from the leg columns (vertices of the body slot at the base).
body_index = [i for i, m in enumerate(mesh.materials) if m and m.name.startswith("M_container")][0]
leg_points = [mesh.vertices[v].co for p in mesh.polygons if p.material_index == body_index for v in p.vertices
              if mesh.vertices[v].co.z < bottom + 0.05]
centre = Vector(((min(p.x for p in leg_points) + max(p.x for p in leg_points)) * 0.5,
                 (min(p.y for p in leg_points) + max(p.y for p in leg_points)) * 0.5, bottom))
print("BUILD legs x %.3f..%.3f y %.3f..%.3f bottom %.3f" % (
    min(p.x for p in leg_points), max(p.x for p in leg_points), min(p.y for p in leg_points),
    max(p.y for p in leg_points), bottom))
mesh.transform(Matrix.Translation(-centre))

points = [v.co for v in mesh.vertices]
height = max(p.z for p in points)
radius = max(math.hypot(p.x, p.y) for p in points)
scale = min(TARGET_HEIGHT_M / height, MAX_RADIUS_M / radius)
mesh.transform(Matrix.Scale(scale, 4))
print("BUILD source height %.3f m radius %.3f m -> scale %.5f" % (height, radius, scale))

# Rename the slots and cut the roof out of the body slot.
for material in mesh.materials:
    if material is None:
        continue
    for prefix, new_name in SLOT_NAMES.items():
        if material.name.startswith(prefix):
            material.name = new_name
roof = bpy.data.materials.new("WP_Roof")
mesh.materials.append(roof)
roof_index = len(mesh.materials) - 1
top = max(v.co.z for v in mesh.vertices)
bm = bmesh.new()
bm.from_mesh(mesh)
# The roof pyramid starts above the cabin walls: about 89% of the height in the source (9.5 of 10.7 m).
roof_floor = top * 0.86
cut = 0
for face in bm.faces:
    if face.material_index != body_index:
        continue
    centroid = face.calc_center_median()
    if centroid.z > roof_floor and face.normal.z > 0.25:
        face.material_index = roof_index
        cut += 1
bm.to_mesh(mesh)
bm.free()
print("BUILD roof faces %d above z %.4f" % (cut, roof_floor))

obj.name = PIECE_NAME
mesh.name = PIECE_NAME

points = [v.co for v in mesh.vertices]
tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
print("BUILD final z %.4f..%.4f x %.4f..%.4f y %.4f..%.4f max radius %.4f tris %d slots %s" % (
    min(p.z for p in points), max(p.z for p in points), min(p.x for p in points), max(p.x for p in points),
    min(p.y for p in points), max(p.y for p in points), max(math.hypot(p.x, p.y) for p in points), tris,
    [m.name for m in mesh.materials]))
for index, material in enumerate(mesh.materials):
    faces = [p for p in mesh.polygons if p.material_index == index]
    zs = [mesh.vertices[v].co.z for p in faces for v in p.vertices]
    print("BUILD slot %d %s faces %d z %.4f..%.4f" % (index, material.name, len(faces), min(zs) if zs else -1,
                                                       max(zs) if zs else -1))

bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=True, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", axis_forward="-Z",
                         axis_up="Y", use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
print("BUILD wrote " + out_fbx)

# Preview with flat slot colours (CPU), seen from the map camera side: looking along +X, 70 degrees down.
colors = {"WP_Body": (0.55, 0.42, 0.2, 1), "WP_Roof": (0.85, 0.8, 0.6, 1), "WP_Lattice": (0.45, 0.36, 0.2, 1),
          "WP_Net": (0.15, 0.2, 0.08, 1)}
for slot in obj.material_slots:
    preview = bpy.data.materials.new("preview_" + slot.material.name)
    preview.use_nodes = True
    preview.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = colors.get(slot.material.name,
                                                                                              (1, 0, 1, 1))
    slot.material = preview
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 16
scene.render.resolution_x = 600
scene.render.resolution_y = 600
world = bpy.data.worlds.new("w")
world.color = (0.5, 0.5, 0.5)
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3.0
sun.rotation_euler = (math.radians(40), 0, math.radians(-60))
scene.collection.objects.link(sun)
camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
camera.data.type = "ORTHO"
camera.data.ortho_scale = 0.6
scene.collection.objects.link(camera)
scene.camera = camera
look_at = Vector((0, 0, 0.25))
for name, direction in (("map", Vector((-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70))))),
                        ("side", Vector((-1.0, -0.6, 0.35)))):
    direction.normalize()
    camera.location = look_at + direction * 5.0
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = preview_dir + "/blender_" + name + ".png"
    bpy.ops.render.render(write_still=True)
