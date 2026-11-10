"""Cuts the concrete of an existing building mesh into Voronoi chunks for UWallChunkDamageComponent (option D).

Run headless (Blender 4.0, the version the UE FBX round trip was proven with, see the mesh-description-rebuild-crash
memory):
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P fracture_concrete_chunks.py -- \
      <in.fbx> <out folder> <name> <concrete material names, comma separated> [chunks=80] [seed=1] [cap=1]
      [solidify_cm=18] [debris=8]

Input: the building mesh exported from Unreal (AssetExportTask + StaticMeshExporterFBX). FBX material names are the
UE material asset names; pass the concrete slot materials, e.g. "MI_Rus_BunkerConcrete_05".

Only faces with a concrete material are cut. Every other slot (sandbags, camo nets, metal, guns) is copied
untouched. Seeds are spread evenly over the concrete surface (farthest-point sampling); each chunk is the concrete
inside one Voronoi cell, cut along the cell planes. With cap=1, closed cut loops are filled with faces on a new last
slot "ConcreteBroken", so a missing chunk shows broken concrete instead of the inside of a shell. Open cuts (thin
shells) stay open; give their masked material Two Sided in Unreal.

Output in <out folder>:
  <name>.fbx              the whole mesh, chunked concrete + untouched rest; a new UV layer "ChunkId" holds the chunk
                          index in U (exact integer) for concrete and cap faces, 127 (outside the mask) elsewhere.
  <name>_Debris_<n>.fbx   n typical chunks, solidified to solidify_cm, pivot at their centre: the pieces Niagara throws.
  <name>_report.json      chunk count, the UV channel index of ChunkId, the slot list, triangle counts.

Reimport <name>.fbx in place into a DUPLICATE of the original asset (keeps sockets and collision), then build the
layout in Unreal with RTS.WallChunks.BuildLayoutFromMesh (it reads the chunk ids from the mesh, so no coordinate
conversion is needed here). Check that the asset's generated lightmap UVs don't write into the ChunkId channel.
"""
import json
import math
import os
import random
import re
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

OUTSIDE_CHUNK = 127.0
NEIGHBOURS = 20
CANDIDATES_PER_CHUNK = 30


def parse_arguments():
    arguments = sys.argv[sys.argv.index('--') + 1:]
    options = {'chunks': 80, 'seed': 1, 'cap': 1, 'solidify_cm': 18.0, 'debris': 8}
    for extra in arguments[4:]:
        key, value = extra.split('=')
        options[key] = float(value) if key == 'solidify_cm' else int(value)
    return arguments[0], arguments[1], arguments[2], [n.strip() for n in arguments[3].split(',')], options


def import_joined(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    # UE exports every LOD (<name>_LOD1..n) and the collision (UCX_); only LOD0 is cut and reimported.
    lod = re.compile(r'_LOD([1-9]\d*)$')
    meshes = [obj for obj in bpy.context.scene.objects
              if obj.type == 'MESH' and not obj.name.startswith('UCX_') and not lod.search(obj.name)]
    # Slot order of the UE asset: materials in order of first use over all LODs (LOD1+ may use extra slots).
    lod_objects = sorted([obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and not obj.name.startswith('UCX_')],
                         key=lambda obj: int(lod.search(obj.name).group(1)) if lod.search(obj.name) else 0)
    slot_order = []
    for obj in lod_objects:
        for material in obj.data.materials:
            if material is not None and material not in slot_order:
                slot_order.append(material)
    # The source's simple collision (UCX_ of LOD0) goes through unchanged, so the new asset collides like the old.
    lod_in_name = re.compile(r'_LOD[1-9]')
    collision = [obj for obj in bpy.context.scene.objects
                 if obj.type == 'MESH' and obj.name.startswith('UCX_') and not lod_in_name.search(obj.name)]
    for obj in bpy.context.scene.objects:
        obj.select_set(obj in meshes or obj in collision)
    bpy.context.view_layer.objects.active = meshes[0]
    # Parents (the FBX root) carry scale; removing them later must not change the world-space geometry.
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    for obj in bpy.context.scene.objects:
        obj.select_set(obj in collision)
    if collision:
        bpy.context.view_layer.objects.active = collision[0]
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for obj in bpy.context.scene.objects:
        obj.select_set(obj in meshes)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for other in list(bpy.context.scene.objects):
        if other != obj and other not in collision:
            bpy.data.objects.remove(other, do_unlink=True)
    reorder_slots(obj, slot_order)
    return obj, collision


def reorder_slots(obj, slot_order):
    """Gives the mesh exactly the asset's slot list, in its order, remapping the faces."""
    mesh = obj.data
    old = list(mesh.materials)
    remap = [slot_order.index(material) if material in slot_order else 0 for material in old]
    indices = [polygon.material_index for polygon in mesh.polygons]
    # Clearing the slot list clamps every face's index, so rebuild the list first and remap after.
    mesh.materials.clear()
    for material in slot_order:
        mesh.materials.append(material)
    for polygon, index in zip(mesh.polygons, indices):
        polygon.material_index = remap[index] if index < len(remap) else 0


def concrete_slots(mesh, names):
    lowered = [name.lower() for name in names]
    return {index for index, material in enumerate(mesh.materials)
            if material is not None and any(name in material.name.lower() for name in lowered)}


def sample_surface(faces, count, generator):
    """Area-weighted random points on the faces (fan triangles)."""
    triangles = []
    for face in faces:
        corners = [vert.co.copy() for vert in face.verts]
        for index in range(1, len(corners) - 1):
            area = (corners[index] - corners[0]).cross(corners[index + 1] - corners[0]).length * 0.5
            if area > 1e-9:
                triangles.append((area, corners[0], corners[index], corners[index + 1]))
    total = sum(triangle[0] for triangle in triangles)
    points = []
    for _ in range(count):
        pick = generator.random() * total
        for area, a, b, c in triangles:
            pick -= area
            if pick <= 0.0:
                break
        u, v = generator.random(), generator.random()
        if u + v > 1.0:
            u, v = 1.0 - u, 1.0 - v
        points.append(a + (b - a) * u + (c - a) * v)
    return points, total


def farthest_point_seeds(candidates, count, generator):
    seeds = [candidates[generator.randrange(len(candidates))]]
    distances = [(point - seeds[0]).length for point in candidates]
    while len(seeds) < count:
        best = max(range(len(candidates)), key=lambda index: distances[index])
        seeds.append(candidates[best])
        for index, point in enumerate(candidates):
            distances[index] = min(distances[index], (point - candidates[best]).length)
    return seeds


def box_distance(face, point):
    xs = [vert.co.x for vert in face.verts]
    ys = [vert.co.y for vert in face.verts]
    zs = [vert.co.z for vert in face.verts]
    dx = max(min(xs) - point.x, 0.0, point.x - max(xs))
    dy = max(min(ys) - point.y, 0.0, point.y - max(ys))
    dz = max(min(zs) - point.z, 0.0, point.z - max(zs))
    return math.sqrt(dx * dx + dy * dy + dz * dz)


def cut_cell(concrete_bm, seed, neighbours, reach, broken_slot, cap):
    """The concrete inside the seed's Voronoi cell, as its own bmesh."""
    bm = concrete_bm.copy()
    far = [face for face in bm.faces if box_distance(face, seed) > reach]
    bmesh.ops.delete(bm, geom=far, context='FACES')
    cut_edges = set()
    for other in neighbours:
        if not bm.faces:
            break
        normal = (other - seed).normalized()
        result = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5,
                                        plane_co=(seed + other) * 0.5, plane_no=normal, clear_outer=True)
        cut_edges.update(element for element in result['geom_cut'] if isinstance(element, bmesh.types.BMEdge))
    loose = [vert for vert in bm.verts if not vert.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    if cap and bm.faces:
        boundary = [edge for edge in cut_edges if edge.is_valid and edge.is_boundary]
        if boundary:
            filled = bmesh.ops.holes_fill(bm, edges=boundary, sides=0)
            for face in filled['faces']:
                face.material_index = broken_slot
    return bm


def write_object(name, bm, template_mesh, chunk_value):
    mesh = template_mesh.copy()
    mesh.name = name
    bm.to_mesh(mesh)
    layer = mesh.uv_layers.get('ChunkId') or mesh.uv_layers.new(name='ChunkId')
    for loop_uv in layer.data:
        loop_uv.uv = (chunk_value, 0.0)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


EXPORT_SCALE = [1.0]


def export(objects, path):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    # A centimetre scene would export as metres (100x too big in Unreal); EXPORT_SCALE brings it back.
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, global_scale=EXPORT_SCALE[0],
                             apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', axis_forward='-Z',
                             axis_up='Y', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)


def main():
    in_path, out_folder, name, concrete_names, options = parse_arguments()
    os.makedirs(out_folder, exist_ok=True)
    generator = random.Random(options['seed'])
    source, collision = import_joined(in_path)
    mesh = source.data
    slots = concrete_slots(mesh, concrete_names)
    if not slots:
        raise RuntimeError(f'no material matches {concrete_names}: {[m.name for m in mesh.materials if m]}')
    broken = bpy.data.materials.get('ConcreteBroken') or bpy.data.materials.new('ConcreteBroken')
    mesh.materials.append(broken)
    broken_slot = len(mesh.materials) - 1
    triangles_before = sum(len(polygon.vertices) - 2 for polygon in mesh.polygons)
    # UE exports come in either in centimetres or in metres depending on the exporter's scale; buildings are metres
    # across, so a size over 50 units means the scene is in centimetres.
    centimetre = 1.0 if max(source.dimensions) > 50.0 else 0.01
    EXPORT_SCALE[0] = 0.01 if centimetre == 1.0 else 1.0

    full = bmesh.new()
    full.from_mesh(mesh)
    concrete = full.copy()
    bmesh.ops.delete(concrete, geom=[f for f in concrete.faces if f.material_index not in slots], context='FACES')
    bmesh.ops.delete(full, geom=[f for f in full.faces if f.material_index in slots], context='FACES')
    rest = write_object(name + '_Rest', full, mesh, OUTSIDE_CHUNK)

    count = min(options['chunks'], 100)
    candidates, concrete_area = sample_surface(concrete.faces, count * CANDIDATES_PER_CHUNK, generator)
    seeds = farthest_point_seeds(candidates, count, generator)
    tree = KDTree(len(seeds))
    for index, seed in enumerate(seeds):
        tree.insert(seed, index)
    tree.balance()
    chunks = []
    for index, seed in enumerate(seeds):
        found = tree.find_n(seed, NEIGHBOURS + 1)[1:]
        neighbours = [seeds[found_index] for _, found_index, _ in found]
        reach = 1.6 * found[min(7, len(found) - 1)][2]
        cell = cut_cell(concrete, seed, neighbours, reach, broken_slot, options['cap'])
        if cell.faces:
            area = sum(face.calc_area() for face in cell.faces if face.material_index != broken_slot)
            chunks.append((len(chunks), write_object(f'{name}_Chunk_{len(chunks)}', cell, mesh, float(len(chunks))), area))
        cell.free()
    # Chunk ids are written per object above; renumber densely in case cells came out empty.
    debris_count = min(options['debris'], len(chunks))
    by_area = sorted(chunks, key=lambda chunk: chunk[2])
    middle = by_area[len(by_area) // 4: len(by_area) * 3 // 4] or by_area
    step = max(1, len(middle) // max(debris_count, 1))
    picks = middle[::step][:debris_count]
    for piece_index, (_, chunk_obj, _) in enumerate(picks):
        piece = chunk_obj.copy()
        piece.data = chunk_obj.data.copy()
        bpy.context.scene.collection.objects.link(piece)
        modifier = piece.modifiers.new('Solidify', 'SOLIDIFY')
        modifier.thickness = options['solidify_cm'] * centimetre
        modifier.offset = -1.0
        bpy.ops.object.select_all(action='DESELECT')
        piece.select_set(True)
        bpy.context.view_layer.objects.active = piece
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
        piece.location = (0.0, 0.0, 0.0)
        export([piece], os.path.join(out_folder, f'{name}_Debris_{piece_index}.fbx'))
        bpy.data.objects.remove(piece, do_unlink=True)

    all_objects = [rest] + [chunk_obj for _, chunk_obj, _ in chunks]
    bpy.ops.object.select_all(action='DESELECT')
    for obj in all_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = rest
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    joined.data.name = name
    uv_index = [layer.name for layer in joined.data.uv_layers].index('ChunkId')
    triangles_after = sum(len(polygon.vertices) - 2 for polygon in joined.data.polygons)
    for index, hull in enumerate(collision):
        hull.name = f'UCX_{name}_{index:02d}'
    export([joined] + collision, os.path.join(out_folder, name + '.fbx'))
    report = {'name': name, 'chunks': len(chunks), 'chunk_uv_channel': uv_index,
              'uv_layers': [layer.name for layer in joined.data.uv_layers],
              'slots': [m.name if m else None for m in joined.data.materials],
              'concrete_slots': sorted(slots), 'broken_slot': broken_slot, 'collision_hulls': len(collision),
              'triangles_before': triangles_before, 'triangles_after': triangles_after,
              'concrete_area_m2': round(concrete_area * (0.01 if centimetre == 1.0 else 1.0) ** 2, 2),
              'debris_from_chunks': [chunk[0] for chunk in picks]}
    with open(os.path.join(out_folder, name + '_report.json'), 'w', encoding='utf-8') as handle:
        json.dump(report, handle, indent=1)
    print('FRACTURE_CONCRETE done', json.dumps(report))


main()
