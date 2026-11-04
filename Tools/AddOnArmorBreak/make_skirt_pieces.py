"""Builds the test add-on armour (Schuerzen side skirt) pieces for the add-on armour break comparison.

Run headless with the user's Blender 5.2:
  Blender5.0.exe --background --factory-startup --python make_skirt_pieces.py -- <export folder>

Output (centimetres, UE axes: X along the tank, Y outward, Z up):
  SM_TestSkirtShard_<n>.fbx  one shard of a skirt panel; pivot at the shard's centroid
  SM_TestSkirtPanel.fbx      the whole panel (reference only); pivot at the panel's top centre
  SM_TestSkirtRail.fbx       the rail the panels hang from, with brackets towards the hull; pivot at the rail's centre
  skirt_layout.json          shard centroids in panel space (pivot = top centre of the panel), panel size

The panel is a 90 x 90 cm plate, 0.8 cm thick, cut into Voronoi cells from a jittered seed grid (fixed seed) so both
techniques under test (geometry collection and Niagara debris) break the same pieces.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy

PANEL_WIDTH = 90.0
PANEL_HEIGHT = 90.0
PANEL_THICKNESS = 0.8
# Seeds per row and column of the jittered grid; one corner cell is dropped for less regular pieces.
SEED_COLUMNS = 3
SEED_ROWS = 3
SEED_JITTER = 0.32
RANDOM_SEED = 4217
# Corner bevel on the plate outline (cm), so the panel is not a perfect rectangle.
CORNER_CUT = 4.0
RAIL_LENGTH = 5 * 92.0 + 6.0
RAIL_SIZE = 4.0
RAIL_THICKNESS = 0.6
BRACKET_COUNT = 4
BRACKET_LENGTH = 14.0


def clip_polygon(polygon, point_on_line, normal):
    """Keeps the part of a convex 2D polygon where dot(p - point, normal) <= 0 (Sutherland-Hodgman)."""
    result = []
    count = len(polygon)
    for index in range(count):
        current = polygon[index]
        following = polygon[(index + 1) % count]
        current_side = (current[0] - point_on_line[0]) * normal[0] + (current[1] - point_on_line[1]) * normal[1]
        following_side = (following[0] - point_on_line[0]) * normal[0] + (following[1] - point_on_line[1]) * normal[1]
        if current_side <= 0.0:
            result.append(current)
        if (current_side <= 0.0) != (following_side <= 0.0):
            fraction = current_side / (current_side - following_side)
            result.append((current[0] + (following[0] - current[0]) * fraction,
                           current[1] + (following[1] - current[1]) * fraction))
    return result


def panel_outline():
    """Panel outline in (x, z) with the top edge at z = 0; the lower corners are cut."""
    half = PANEL_WIDTH * 0.5
    bottom = -PANEL_HEIGHT
    return [(-half, 0.0), (half, 0.0), (half, bottom + CORNER_CUT), (half - CORNER_CUT, bottom),
            (-half + CORNER_CUT, bottom), (-half, bottom + CORNER_CUT)]


def make_seeds():
    generator = random.Random(RANDOM_SEED)
    cell_width = PANEL_WIDTH / SEED_COLUMNS
    cell_height = PANEL_HEIGHT / SEED_ROWS
    seeds = []
    for row in range(SEED_ROWS):
        for column in range(SEED_COLUMNS):
            if row == SEED_ROWS - 1 and column == SEED_COLUMNS - 1:
                continue
            x = -PANEL_WIDTH * 0.5 + (column + 0.5 + generator.uniform(-SEED_JITTER, SEED_JITTER)) * cell_width
            z = -(row + 0.5 + generator.uniform(-SEED_JITTER, SEED_JITTER)) * cell_height
            seeds.append((x, z))
    return seeds


def voronoi_cells(seeds):
    cells = []
    for seed_index, seed in enumerate(seeds):
        cell = panel_outline()
        for other_index, other in enumerate(seeds):
            if other_index == seed_index:
                continue
            midpoint = ((seed[0] + other[0]) * 0.5, (seed[1] + other[1]) * 0.5)
            normal = (other[0] - seed[0], other[1] - seed[1])
            cell = clip_polygon(cell, midpoint, normal)
        cells.append(cell)
    return cells


def polygon_centroid(polygon):
    area = 0.0
    centre_x = 0.0
    centre_z = 0.0
    count = len(polygon)
    for index in range(count):
        x0, z0 = polygon[index]
        x1, z1 = polygon[(index + 1) % count]
        cross = x0 * z1 - x1 * z0
        area += cross
        centre_x += (x0 + x1) * cross
        centre_z += (z0 + z1) * cross
    area *= 0.5
    return (centre_x / (6.0 * area), centre_z / (6.0 * area)), abs(area)


def ue_to_blender(x, y, z):
    # UE is left handed (Y right); Blender's Y points the other way. The FBX export maps Blender X/Z to UE X/Z
    # and Blender -Y to UE Y, so a UE point (x, y, z) is created at Blender (x, -y, z).
    return (x, -y, z)


def build_prism(name, polygon, centre, thickness):
    """Extrudes a convex (x, z) polygon through the plate thickness around y = 0, relative to centre."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    half = thickness * 0.5
    outer = [bm.verts.new(ue_to_blender(x - centre[0], half, z - centre[1])) for x, z in polygon]
    inner = [bm.verts.new(ue_to_blender(x - centre[0], -half, z - centre[1])) for x, z in polygon]
    bm.faces.new(outer)
    bm.faces.new(list(reversed(inner)))
    count = len(polygon)
    for index in range(count):
        following = (index + 1) % count
        bm.faces.new([outer[index], inner[index], inner[following], outer[following]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv_layer = bm.loops.layers.uv.new('UVMap')
    for face in bm.faces:
        for loop in face.loops:
            point = loop.vert.co
            # Planar map over the panel face so neighbouring shards continue the same texture.
            loop[uv_layer].uv = ((point.x + centre[0]) / PANEL_WIDTH + 0.5, -(point.z + centre[1]) / PANEL_HEIGHT)
    bm.to_mesh(mesh)
    bm.free()
    for polygon_face in mesh.polygons:
        polygon_face.use_smooth = False
    return bpy.data.objects.new(name, mesh)


def add_box(bm, minimum, maximum):
    x0, y0, z0 = minimum
    x1, y1, z1 = maximum
    corners = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
               (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    verts = [bm.verts.new(ue_to_blender(*corner)) for corner in corners]
    for face in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        bm.faces.new([verts[index] for index in face])


def build_rail():
    """An L-angle along the panels' top edge plus brackets reaching inward (negative Y) to the hull."""
    mesh = bpy.data.meshes.new('SM_TestSkirtRail')
    bm = bmesh.new()
    half_length = RAIL_LENGTH * 0.5
    # Horizontal flange, reaching inward over the plates' top edge, and the vertical web the plates hook onto.
    add_box(bm, (-half_length, -RAIL_SIZE, 0.0), (half_length, 0.0, RAIL_THICKNESS))
    add_box(bm, (-half_length, -RAIL_THICKNESS, -RAIL_SIZE), (half_length, 0.0, 0.0))
    for index in range(BRACKET_COUNT):
        x = -half_length + 20.0 + index * (RAIL_LENGTH - 40.0) / (BRACKET_COUNT - 1)
        add_box(bm, (x - 1.5, -BRACKET_LENGTH, 0.0), (x + 1.5, -RAIL_SIZE, RAIL_SIZE * 0.6))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv_layer = bm.loops.layers.uv.new('UVMap')
    for face in bm.faces:
        for loop in face.loops:
            loop[uv_layer].uv = (loop.vert.co.x / 100.0, loop.vert.co.z / 100.0 + loop.vert.co.y / 100.0)
    bm.to_mesh(mesh)
    bm.free()
    return bpy.data.objects.new('SM_TestSkirtRail', mesh)


def assign_material(obj, material_name):
    material = bpy.data.materials.get(material_name) or bpy.data.materials.new(material_name)
    obj.data.materials.clear()
    obj.data.materials.append(material)


def export_object(obj, folder):
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.scene.collection.objects.link(obj)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # Centimetres in the scene, exported without unit conversion and scaled 0.01: imports 1:1 in Unreal.
    bpy.ops.export_scene.fbx(filepath=os.path.join(folder, obj.name + '.fbx'), use_selection=True,
                             apply_scale_options='FBX_SCALE_NONE', global_scale=0.01,
                             axis_forward='X', axis_up='Z', mesh_smooth_type='FACE',
                             use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
    bpy.context.scene.collection.objects.unlink(obj)


def main():
    arguments = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    folder = arguments[0] if arguments else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Exports')
    os.makedirs(folder, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 0.01

    shards = []
    for index, cell in enumerate(voronoi_cells(make_seeds())):
        centre, area = polygon_centroid(cell)
        name = f'SM_TestSkirtShard_{index}'
        obj = build_prism(name, cell, centre, PANEL_THICKNESS)
        assign_material(obj, 'TestSkirtSteel')
        export_object(obj, folder)
        shards.append({'mesh': name, 'centroid': [round(centre[0], 3), 0.0, round(centre[1], 3)],
                       'area': round(area, 2), 'outline': [[round(x, 3), round(z, 3)] for x, z in cell]})
        print('SKIRT_PIECES shard', name, 'centroid', centre, 'area', round(area, 1), 'corners', len(cell))

    panel = build_prism('SM_TestSkirtPanel', panel_outline(), (0.0, 0.0), PANEL_THICKNESS)
    assign_material(panel, 'TestSkirtSteel')
    export_object(panel, folder)
    rail = build_rail()
    assign_material(rail, 'TestSkirtSteel')
    export_object(rail, folder)

    layout = {'panel_width': PANEL_WIDTH, 'panel_height': PANEL_HEIGHT, 'panel_thickness': PANEL_THICKNESS,
              'rail_length': RAIL_LENGTH, 'shards': shards}
    with open(os.path.join(folder, 'skirt_layout.json'), 'w', encoding='utf-8') as handle:
        json.dump(layout, handle, indent=1)
    print('SKIRT_PIECES done', len(shards), 'shards in', folder)


main()
