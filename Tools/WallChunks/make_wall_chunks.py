"""Cuts a straight concrete wall slab into Voronoi chunks for the wall chunk damage prototype (WallHitMasking.md, D).

Run headless:
  Blender5.0.exe --background --factory-startup --python make_wall_chunks.py -- <export folder> [<name>]

The slab matches SM_Wall_14 (UE mesh space, centimetres): X -2260..0, Y -20..0, Z 0..500, pivot at the origin.
Output (UE axes, imports 1:1 with the FBX settings below):
  <name>.fbx              every chunk as a closed prism, all in one mesh; slot 0 "Concrete" (the slab's outer faces),
                          slot 1 "ConcreteBroken" (the fracture faces between chunks). UV0 = the slab's own planar map,
                          UV1 = placeholder for the generated lightmap, UV2.x = chunk index (exact integer).
                          UCX_<name> is the slab's box collision.
  <name>_Debris_<n>.fbx   eight typical chunks on their own, pivot at the chunk centre, for the Niagara debris.
  <name>_layout.json      chunk centres (mesh space), areas, the debris piece each chunk flies off as, and its scale.

Every chunk keeps its own vertices, so a chunk index is per vertex and the material can clip whole chunks. The faces
between two chunks exist twice (once per chunk). While both stand they are buried inside the slab; when one goes the
neighbour's face shows the broken concrete.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy

WALL_MIN_X = -2260.0
WALL_MAX_X = 0.0
WALL_MIN_Y = -20.0
WALL_MAX_Y = 0.0
WALL_HEIGHT = 500.0
# 20 x 5 seeds = 100 chunks = 5 mask floats of 20 bits.
SEED_COLUMNS = 20
SEED_ROWS = 5
SEED_JITTER = 0.36
RANDOM_SEED = 9113
DEBRIS_PIECE_COUNT = 8

# SM_Wall_14's planar UV0 on its two big faces (read from the UE export): one UV unit is 7.6 m, U grows towards -X.
UV_SCALE_PER_CM = 0.0013155
UV_OFFSET_U = 4.699
UV_OFFSET_V = 0.334


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


def wall_outline():
    """Wall face outline in (x, z), counter-clockwise seen from -Y."""
    return [(WALL_MIN_X, 0.0), (WALL_MAX_X, 0.0), (WALL_MAX_X, WALL_HEIGHT), (WALL_MIN_X, WALL_HEIGHT)]


def make_seeds():
    generator = random.Random(RANDOM_SEED)
    cell_width = (WALL_MAX_X - WALL_MIN_X) / SEED_COLUMNS
    cell_height = WALL_HEIGHT / SEED_ROWS
    seeds = []
    for row in range(SEED_ROWS):
        for column in range(SEED_COLUMNS):
            x = WALL_MIN_X + (column + 0.5 + generator.uniform(-SEED_JITTER, SEED_JITTER)) * cell_width
            z = (row + 0.5 + generator.uniform(-SEED_JITTER, SEED_JITTER)) * cell_height
            seeds.append((x, z))
    return seeds


def voronoi_cells(seeds):
    cells = []
    for seed_index, seed in enumerate(seeds):
        cell = wall_outline()
        for other_index, other in enumerate(seeds):
            if other_index == seed_index:
                continue
            # Only near seeds can bound this cell; skipping far ones keeps the clip exact and fast.
            if abs(other[0] - seed[0]) > 400.0:
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
    # The FBX export maps Blender X/Z to UE X/Z and Blender -Y to UE Y.
    return (x, -y, z)


def is_on_wall_outline(a, b):
    """True for a cell edge on the slab's own outline (top, bottom or an end), not between two chunks."""
    epsilon = 0.01
    for axis, limits in ((0, (WALL_MIN_X, WALL_MAX_X)), (1, (0.0, WALL_HEIGHT))):
        for limit in limits:
            if abs(a[axis] - limit) < epsilon and abs(b[axis] - limit) < epsilon:
                return True
    return False


def planar_uv(ue_point, ue_normal):
    """The slab's own planar map, continued over every face at the same density."""
    x, y, z = ue_point
    nx, ny, nz = ue_normal
    if abs(ny) > 0.7:
        return (UV_OFFSET_U - x * UV_SCALE_PER_CM, UV_OFFSET_V + z * UV_SCALE_PER_CM)
    if abs(nz) > 0.7:
        return (UV_OFFSET_U - x * UV_SCALE_PER_CM, UV_OFFSET_V + y * UV_SCALE_PER_CM)
    # A vertical side face: run U along the face's horizontal direction.
    tangent_x, tangent_y = -ny, nx
    length = math.hypot(tangent_x, tangent_y) or 1.0
    along = (x * tangent_x + y * tangent_y) / length
    return (UV_OFFSET_U + along * UV_SCALE_PER_CM, UV_OFFSET_V + z * UV_SCALE_PER_CM)


def add_chunk(bm, uv_layers, polygon, chunk_index, offset):
    """Adds one chunk prism (UE space minus offset) to bm; side faces between chunks get material 1."""
    ox, oy, oz = offset
    front = [bm.verts.new(ue_to_blender(x - ox, WALL_MAX_Y - oy, z - oz)) for x, z in polygon]
    back = [bm.verts.new(ue_to_blender(x - ox, WALL_MIN_Y - oy, z - oz)) for x, z in polygon]
    faces = [(bm.faces.new(front), 0), (bm.faces.new(list(reversed(back))), 0)]
    count = len(polygon)
    for index in range(count):
        following = (index + 1) % count
        face = bm.faces.new([front[index], back[index], back[following], front[following]])
        faces.append((face, 0 if is_on_wall_outline(polygon[index], polygon[following]) else 1))
    uv_main, uv_lightmap, uv_chunk = uv_layers
    for face, material_index in faces:
        face.material_index = material_index
        face.smooth = False
    bmesh.ops.recalc_face_normals(bm, faces=[face for face, _ in faces])
    for face, _ in faces:
        ue_normal = (face.normal.x, -face.normal.y, face.normal.z)
        for loop in face.loops:
            co = loop.vert.co
            ue_point = (co.x + ox, -co.y + oy, co.z + oz)
            loop[uv_main].uv = planar_uv(ue_point, ue_normal)
            loop[uv_lightmap].uv = (0.0, 0.0)
            loop[uv_chunk].uv = (float(chunk_index), 0.0)


def new_bmesh_with_layers():
    bm = bmesh.new()
    layers = (bm.loops.layers.uv.new('UVMap'), bm.loops.layers.uv.new('LightMapUV'), bm.loops.layers.uv.new('ChunkId'))
    return bm, layers


def make_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for material_name in ('Concrete', 'ConcreteBroken'):
        material = bpy.data.materials.get(material_name) or bpy.data.materials.new(material_name)
        mesh.materials.append(material)
    return bpy.data.objects.new(name, mesh)


def make_collision(name):
    mesh = bpy.data.meshes.new('UCX_' + name)
    bm = bmesh.new()
    corners = [(x, y, z) for z in (0.0, WALL_HEIGHT) for y in (WALL_MIN_Y, WALL_MAX_Y) for x in (WALL_MIN_X, WALL_MAX_X)]
    verts = [bm.verts.new(ue_to_blender(*corner)) for corner in corners]
    bmesh.ops.convex_hull(bm, input=verts)
    bm.to_mesh(mesh)
    bm.free()
    return bpy.data.objects.new('UCX_' + name, mesh)


def export_objects(objects, path):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        bpy.context.scene.collection.objects.link(obj)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    # Centimetres in the scene, exported without unit conversion and scaled 0.01: imports 1:1 in Unreal.
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             global_scale=0.01, axis_forward='X', axis_up='Z', mesh_smooth_type='FACE',
                             use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
    for obj in objects:
        bpy.context.scene.collection.objects.unlink(obj)


def pick_debris_chunks(chunks):
    """Eight typical chunks: from the middle rows, spread along the wall, none on the slab's outline."""
    middle_x = [(WALL_MIN_X + (index + 0.5) * (WALL_MAX_X - WALL_MIN_X) / DEBRIS_PIECE_COUNT)
                for index in range(DEBRIS_PIECE_COUNT)]
    picks = []
    for target_x in middle_x:
        best = None
        for chunk in chunks:
            if chunk['touches_outline'] or chunk['index'] in picks:
                continue
            distance = abs(chunk['centre'][0] - target_x) + abs(chunk['centre'][2] - WALL_HEIGHT * 0.5) * 0.5
            if best is None or distance < best[0]:
                best = (distance, chunk['index'])
        picks.append(best[1])
    return picks


def main():
    arguments = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    folder = arguments[0] if arguments else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Exports')
    name = arguments[1] if len(arguments) > 1 else 'SM_Wall_14_Chunks'
    os.makedirs(folder, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 0.01

    cells = voronoi_cells(make_seeds())
    chunks = []
    wall_bm, wall_layers = new_bmesh_with_layers()
    for index, cell in enumerate(cells):
        centre, area = polygon_centroid(cell)
        touches = any(is_on_wall_outline(cell[i], cell[(i + 1) % len(cell)]) for i in range(len(cell)))
        add_chunk(wall_bm, wall_layers, cell, index, (0.0, 0.0, 0.0))
        chunks.append({'index': index, 'centre': [round(centre[0], 3), (WALL_MIN_Y + WALL_MAX_Y) * 0.5,
                                                  round(centre[1], 3)],
                       'area': round(area, 2), 'corners': len(cell), 'touches_outline': touches})
    wall = make_object(name, wall_bm)
    export_objects([wall, make_collision(name)], os.path.join(folder, name + '.fbx'))

    picks = pick_debris_chunks(chunks)
    debris = []
    for piece, chunk_index in enumerate(picks):
        chunk = chunks[chunk_index]
        piece_name = f'{name}_Debris_{piece}'
        bm, layers = new_bmesh_with_layers()
        offset = (chunk['centre'][0], chunk['centre'][1], chunk['centre'][2])
        add_chunk(bm, layers, cells[chunk_index], chunk_index, offset)
        export_objects([make_object(piece_name, bm)], os.path.join(folder, piece_name + '.fbx'))
        debris.append({'mesh': piece_name, 'chunk': chunk_index, 'area': chunk['area']})
    # Every chunk flies off as the debris piece nearest to it in size, scaled to its own area.
    for chunk in chunks:
        best = min(range(len(debris)), key=lambda piece: abs(math.log(debris[piece]['area'] / chunk['area'])))
        chunk['debris'] = best
        chunk['debris_scale'] = round(math.sqrt(chunk['area'] / debris[best]['area']), 4)

    layout = {'mesh': name, 'wall_min': [WALL_MIN_X, WALL_MIN_Y, 0.0], 'wall_max': [WALL_MAX_X, WALL_MAX_Y, WALL_HEIGHT],
              'chunks': chunks, 'debris': debris}
    with open(os.path.join(folder, name + '_layout.json'), 'w', encoding='utf-8') as handle:
        json.dump(layout, handle, indent=1)
    areas = [chunk['area'] for chunk in chunks]
    print('WALL_CHUNKS done', len(chunks), 'chunks, area min/max cm2', min(areas), max(areas), 'debris', picks)


main()
