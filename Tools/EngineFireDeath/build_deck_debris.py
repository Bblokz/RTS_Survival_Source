"""Builds the engine deck debris thrown by the engine fire (fuel explosion) tank death.

Run headless:  blender.exe -b --factory-startup --python build_deck_debris.py -- <out_dir>
Writes one FBX per piece (SM_EngineDeck_*.fbx) at real size, origin at the bounds centre, plus a preview .blend.
Pieces are low poly (they only fly for a few seconds and then lie as decoration) and share one material slot
(M_EngineDeckDebris in Unreal, a procedural burnt-steel material); UVs are a smart projection in 0-1.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.dirname(__file__)
random.seed(7)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0


def new_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def add_box(bm, size, location=(0, 0, 0), rotation=None):
    matrix = Matrix.Translation(Vector(location))
    if rotation is not None:
        matrix = matrix @ rotation
    geom = bmesh.ops.create_cube(bm, size=1.0, matrix=matrix @ Matrix.Diagonal(Vector((*size, 1.0))))
    return geom['verts']


def add_cylinder(bm, radius, depth, segments, location=(0, 0, 0), rotation=None, cap=True):
    matrix = Matrix.Translation(Vector(location))
    if rotation is not None:
        matrix = matrix @ rotation
    geom = bmesh.ops.create_cone(bm, cap_ends=cap, segments=segments, radius1=radius, radius2=radius, depth=depth,
                                 matrix=matrix)
    return geom['verts']


def bend(bm, axis_index, pivot, angle_degrees, along_index):
    """Rotates every vertex past `pivot` on `along_index` around the axis `axis_index` (a crease)."""
    angle = math.radians(angle_degrees)
    axis = Vector((0, 0, 0))
    axis[axis_index] = 1.0
    rotation = Matrix.Rotation(angle, 4, axis)
    for vert in bm.verts:
        if vert.co[along_index] > pivot:
            offset = vert.co.copy()
            offset[along_index] -= pivot
            rotated = rotation @ offset
            rotated[along_index] += pivot
            vert.co = rotated


def jitter(bm, amount, axis_mask=(1, 1, 1)):
    for vert in bm.verts:
        vert.co += Vector([random.uniform(-amount, amount) * m for m in axis_mask])


def deck_plate(name, length, width, thickness, crease_at, crease_degrees, dent, cuts):
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=cuts, y_segments=max(2, cuts * 2 // 3), size=0.5)
    for vert in bm.verts:
        vert.co.x *= length
        vert.co.y *= width
    # Torn corner: drop the vertices nearest one corner.
    corner = Vector((length * 0.5, width * 0.5, 0))
    torn = [v for v in bm.verts if (v.co - corner).length < min(length, width) * 0.28]
    bmesh.ops.delete(bm, geom=torn, context='VERTS')
    jitter(bm, dent, (0.15, 0.15, 1.0))
    bend(bm, 1, crease_at * length, crease_degrees, 0)
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=thickness)
    # Raised stiffener strip along the long edge.
    add_box(bm, (length * 0.62, 0.06, 0.035), (-length * 0.12, -width * 0.36, thickness * 0.5 + 0.017))
    return new_object(name, bm)


def louvre_grille(name):
    bm = bmesh.new()
    frame_length, frame_width, frame_height = 0.9, 0.6, 0.05
    bar = 0.045
    add_box(bm, (frame_length, bar, frame_height), (0, frame_width * 0.5 - bar * 0.5, 0))
    add_box(bm, (frame_length, bar, frame_height), (0, -frame_width * 0.5 + bar * 0.5, 0))
    add_box(bm, (bar, frame_width, frame_height), (frame_length * 0.5 - bar * 0.5, 0, 0))
    add_box(bm, (bar, frame_width, frame_height), (-frame_length * 0.5 + bar * 0.5, 0, 0))
    slat_count = 7
    for index in range(slat_count):
        x = -frame_length * 0.5 + bar + (index + 0.5) * (frame_length - 2 * bar) / slat_count
        tilt = Matrix.Rotation(math.radians(38 + random.uniform(-6, 6)), 4, 'Y')
        add_box(bm, (0.09, frame_width - 2 * bar, 0.008), (x, 0, 0), tilt)
    # Blast-bent: the far half of the grille is folded up.
    bend(bm, 1, 0.12, -14, 0)
    return new_object(name, bm)


def hatch_lid(name):
    bm = bmesh.new()
    add_cylinder(bm, 0.31, 0.03, 20)
    add_cylinder(bm, 0.26, 0.02, 20, (0, 0, 0.025))
    # Hinge knuckle on one side and a grab handle across the top.
    add_cylinder(bm, 0.035, 0.22, 10, (-0.33, 0, 0), Matrix.Rotation(math.radians(90), 4, 'X'))
    add_box(bm, (0.08, 0.06, 0.03), (-0.29, 0, 0))
    add_box(bm, (0.03, 0.22, 0.03), (0.08, 0, 0.07))
    add_box(bm, (0.03, 0.03, 0.06), (0.08, 0.1, 0.045))
    add_box(bm, (0.03, 0.03, 0.06), (0.08, -0.1, 0.045))
    jitter(bm, 0.006)
    return new_object(name, bm)


def muffler(name):
    bm = bmesh.new()
    y_axis = Matrix.Rotation(math.radians(90), 4, 'Y')
    add_cylinder(bm, 0.11, 0.62, 14, (0, 0, 0), y_axis)
    add_cylinder(bm, 0.122, 0.04, 14, (0.29, 0, 0), y_axis)
    add_cylinder(bm, 0.122, 0.04, 14, (-0.29, 0, 0), y_axis)
    # Torn exhaust pipe stub and a mounting bracket.
    add_cylinder(bm, 0.04, 0.32, 10, (0.42, 0.04, 0.05), Matrix.Rotation(math.radians(80), 4, 'Y'))
    add_box(bm, (0.08, 0.26, 0.012), (0.05, 0, -0.12))
    jitter(bm, 0.005)
    return new_object(name, bm)


def cooling_fan(name):
    bm = bmesh.new()
    add_cylinder(bm, 0.07, 0.09, 12)
    add_cylinder(bm, 0.285, 0.035, 24, (0, 0, -0.01), cap=False)
    blade_count = 6
    for index in range(blade_count):
        angle = index * 2 * math.pi / blade_count
        # Out along the blade's own Y, pitched around that long axis like a real fan blade.
        rotation = Matrix.Rotation(angle, 4, 'Z') @ Matrix.Translation((0, 0.16, 0)) \
            @ Matrix.Rotation(math.radians(28), 4, 'Y')
        add_box(bm, (0.075, 0.2, 0.006), (0, 0, 0), rotation)
    # The shroud dented by the blast.
    jitter(bm, 0.008)
    return new_object(name, bm)


def finish(obj, material):
    bpy.context.view_layer.objects.active = obj
    for other in bpy.context.selected_objects:
        other.select_set(False)
    obj.select_set(True)
    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    # Origin at the bounds centre so the piece tumbles around its middle.
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
    obj.location = (0, 0, 0)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    if bpy.app.version >= (4, 1, 0):
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))
    else:
        bpy.ops.object.shade_smooth(use_auto_smooth=True, auto_smooth_angle=math.radians(35))
    mesh.materials.clear()
    mesh.materials.append(material)


def export(obj):
    for other in bpy.context.scene.objects:
        other.select_set(other == obj)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT_DIR, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS',
                             axis_forward='X', axis_up='Z', mesh_smooth_type='FACE', add_leaf_bones=False,
                             use_mesh_modifiers=True, bake_space_transform=False)
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    dims = obj.dimensions
    print(f'EXPORTED {obj.name} tris={tris} size_cm={dims.x * 100:.0f}x{dims.y * 100:.0f}x{dims.z * 100:.0f}')


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    reset_scene()
    material = bpy.data.materials.new('M_EngineDeckDebris')
    pieces = [
        deck_plate('SM_EngineDeck_Plate_A', 1.3, 0.9, 0.025, 0.15, -22, 0.02, 6),
        deck_plate('SM_EngineDeck_Plate_B', 0.8, 0.55, 0.02, -0.1, 30, 0.035, 5),
        louvre_grille('SM_EngineDeck_Grille'),
        hatch_lid('SM_EngineDeck_HatchLid'),
        muffler('SM_EngineDeck_Muffler'),
        cooling_fan('SM_EngineDeck_Fan'),
    ]
    for obj in pieces:
        finish(obj, material)
    for obj in pieces:
        export(obj)
    # Preview layout for render checks.
    for index, obj in enumerate(pieces):
        obj.location = ((index % 3) * 1.6 - 1.6, (index // 3) * 1.4 - 0.7, 0)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, 'deck_debris_preview.blend'))


main()
