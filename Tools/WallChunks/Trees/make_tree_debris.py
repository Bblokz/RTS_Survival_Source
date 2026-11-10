"""Splintered wood debris pieces cut from the trunk of SM_RTS_DeadTree_1, for the tree hole damage (Niagara debris).

Run headless (Blender 4.0):
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P make_tree_debris.py -- \
      <tree export fbx> <diffuse png> <out folder> [seed=7] [preview=1]

How a piece is made:
  - A site on the main trunk (height, yaw) gives a frame: n = outward radial normal, g = the trunk axis (the grain),
    w = g x n.
  - A grid of rows (along g) x columns (along w) with a per-piece outline (pointed splinter tips, saw-tooth ends) is
    ray cast onto the real LOD0 trunk. The outer (bark) surface therefore is the tree's own surface, and its UVs are the
    tree's own UV0, interpolated at the hit point (barycentric). Sites are rejected when the patch straddles a UV seam
    (texel density off) or sits on too much painted moss.
  - The inner surface is the outer one pushed inward along the hit normals by a varying thickness (ridges along the
    grain, thin edges, a wedge profile for the wedge). Inner and rim faces are the broken wood: they are mapped by box
    projection into the palest wood strip of T_RTS_DeadTree_1_D (px 619..643 x 147..237, grain vertical), at the
    trunk's texel density (102 px/m) or smaller when a piece would not fit.
  - Output frame for UWallChunkDamage debris sizing: X = width (w), Y = thickness (-n), Z = length (g), pivot at the
    bounds centre. One material slot "MI_Env_DeadTree_1".

Output in <out folder>: SM_DeadTree_Debris_<n>.fbx, debris_report.json, and with preview=1 a Cycles render per piece
(preview_<n>.png) plus uv_footprint.png (the UVs drawn on the texture).
"""
import json
import math
import os
import random
import re
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

TEXTURE_SIZE = 1024.0
TRUNK_DENSITY_PX_PER_M = 102.0
# The palest wood-looking region of T_RTS_DeadTree_1_D (image pixels, y down): yellow-grey wood with vertical grain.
PALE_RECT = (619.0, 147.0, 643.0, 237.0)
PALE_MARGIN_PX = 1.5
MATERIAL_NAME = 'MI_Env_DeadTree_1'
MAX_SITE_TRIES = 400

# name, length (along grain), width, thickness, rows, columns, shape
PIECES = [
    ('long_splinter', 0.44, 0.085, 0.04, 8, 3, 'splinter'),
    ('bark_slab', 0.30, 0.21, 0.04, 6, 5, 'slab'),
    ('chunky_wedge', 0.24, 0.17, 0.09, 6, 4, 'wedge'),
    ('thin_shard', 0.17, 0.05, 0.022, 6, 3, 'splinter'),
    ('split_plank', 0.36, 0.11, 0.05, 7, 4, 'plank'),
    ('broken_chunk', 0.20, 0.15, 0.07, 5, 4, 'chunk'),
]


def parse_arguments():
    arguments = sys.argv[sys.argv.index('--') + 1:]
    options = {'seed': 7, 'preview': 1, 'global_scale': 1}
    for extra in arguments[3:]:
        key, value = extra.split('=')
        options[key] = int(value)
    return arguments[0], arguments[1], arguments[2], options


# ---------------------------------------------------------------------------------------------------------------- tree
def import_trunk(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    lod = re.compile(r'_LOD([1-9]\d*)$')
    lod0 = [o for o in bpy.context.scene.objects
            if o.type == 'MESH' and not o.name.startswith('UCX_') and not lod.search(o.name)][0]
    bpy.ops.object.select_all(action='DESELECT')
    lod0.select_set(True)
    bpy.context.view_layer.objects.active = lod0
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for other in list(bpy.context.scene.objects):
        if other != lod0:
            bpy.data.objects.remove(other, do_unlink=True)
    bm = bmesh.new()
    bm.from_mesh(lod0.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.faces.ensure_lookup_table()
    uv_layer = bm.loops.layers.uv[0]
    return lod0, bm, uv_layer


def trunk_axis(bm):
    """Centroid of the trunk vertices at two heights: the axis the grain follows."""
    def centre(z):
        points = [v.co for v in bm.verts if abs(v.co.z - z) < 0.2 and Vector((v.co.x, v.co.y)).length < 1.0]
        return sum(points, Vector()) / len(points)
    low, high = centre(1.0), centre(4.0)
    return low, (high - low).normalized()


def load_moss_mask(png_path):
    image = bpy.data.images.load(png_path)
    width, height = image.size
    pixels = list(image.pixels)
    # Blender image rows run bottom up, so row r holds v = r / height.
    moss = bytearray(width * height)
    for index in range(width * height):
        r, g, b = pixels[index * 4], pixels[index * 4 + 1], pixels[index * 4 + 2]
        moss[index] = 1 if (g >= r * 0.98 and g > b * 1.5) else 0
    return image, moss, width, height


def moss_share(uvs, moss, width, height):
    """Moss share over the UV triangles' bounding samples (a coarse raster of each triangle)."""
    hits = total = 0
    for a, b, c in uvs:
        for s in range(6):
            for t in range(6 - s):
                u = a.x + (b.x - a.x) * (s + 0.33) / 6 + (c.x - a.x) * (t + 0.33) / 6
                v = a.y + (b.y - a.y) * (s + 0.33) / 6 + (c.y - a.y) * (t + 0.33) / 6
                x = min(width - 1, max(0, int(u * width)))
                y = min(height - 1, max(0, int(v * height)))
                hits += moss[y * width + x]
                total += 1
    return hits / max(total, 1)


# ------------------------------------------------------------------------------------------------------------- outline
def outline(shape, rows, columns, generator):
    """Per-column start/end along the length (0..1) and per-row width profile (0..1): the piece silhouette."""
    starts, ends = [], []
    for column in range(columns):
        centre = 1.0 - abs((column / (columns - 1)) * 2.0 - 1.0)  # 1 in the middle, 0 at the sides
        if shape == 'splinter':
            # Columns end at different lengths: a long pointed fibre tip, ragged elsewhere.
            starts.append(generator.uniform(0.0, 0.08) + (1.0 - centre) * generator.uniform(0.06, 0.24))
            ends.append(1.0 - generator.uniform(0.0, 0.08) - (1.0 - centre) * generator.uniform(0.06, 0.28))
        elif shape == 'wedge':
            starts.append(generator.uniform(0.0, 0.10))
            ends.append(1.0 - generator.uniform(0.0, 0.18))
        else:  # slab, plank, chunk: saw-tooth broken ends
            starts.append(generator.uniform(0.0, 0.16))
            ends.append(1.0 - generator.uniform(0.0, 0.20))
    profile = []
    for row in range(rows):
        along = row / (rows - 1)
        if shape == 'splinter':
            # Nearly parallel torn sides; the tips come from the staggered column ends.
            width = 0.60 + 0.40 * min(1.0, min(along, 1.0 - along) * 4.0)
        elif shape == 'wedge':
            width = 0.70 + 0.30 * along
        else:
            width = 1.0
        low = 0.70 if shape == 'splinter' else 0.85
        profile.append((width * generator.uniform(low, 1.0), width * generator.uniform(low, 1.0)))
    return starts, ends, profile


def thickness(shape, row, column, rows, columns, base, generator):
    side = abs((column / (columns - 1)) * 2.0 - 1.0)  # 0 in the middle, 1 at the sides
    along = row / (rows - 1)
    end = min(along, 1.0 - along) * 2.0  # 0 at the ends, 1 in the middle
    if shape == 'splinter':
        value = base * (1.0 - 0.55 * side) * (0.45 + 0.55 * end ** 0.5)
    elif shape == 'wedge':
        value = base * (0.30 + 0.70 * along) * (1.0 - 0.25 * side)
    elif shape == 'chunk':
        value = base * (0.75 + 0.25 * end) * (1.0 - 0.30 * side)
    else:  # slab, plank
        value = base * (0.80 + 0.20 * end) * (1.0 - 0.35 * side)
    # Grain ridges: alternate columns stand proud, plus noise.
    ridge = 1.12 if column % 2 else 0.90
    return max(0.008, value * ridge * generator.uniform(0.82, 1.18))


# ---------------------------------------------------------------------------------------------------------------- site
def cast_patch(spec, site, tree, trunk_bm, uv_layer, axis_point, axis_dir, generator):
    """Ray casts the piece's grid onto the trunk. Returns the grid or None when the site is not usable."""
    _, length, width, base_thickness, rows, columns, shape = spec
    height, yaw = site
    centre_on_axis = axis_point + axis_dir * ((height - axis_point.z) / axis_dir.z)
    radial = Vector((math.cos(yaw), math.sin(yaw), 0.0))
    n = (radial - axis_dir * radial.dot(axis_dir)).normalized()
    g = axis_dir.copy()
    w = g.cross(n)
    starts, ends, profile = outline(shape, rows, columns, generator)
    grid = []
    for row in range(rows):
        line = []
        for column in range(columns):
            along = starts[column] + (ends[column] - starts[column]) * row / (rows - 1)
            left, right = profile[row]
            across = (column / (columns - 1)) * 2.0 - 1.0
            across *= left if across < 0 else right
            a = (along - 0.5) * length
            b = across * width * 0.5
            origin = centre_on_axis + g * a + w * b + n * 1.5
            location, normal, index, distance = tree.ray_cast(origin, -n, 3.0)
            if location is None:
                return None
            radius = (location - centre_on_axis - g * a).dot(n)
            if radius < 0.35 or radius > 1.0:
                return None
            face = trunk_bm.faces[index]
            corners = [loop.vert.co for loop in face.loops]
            uvs = [loop[uv_layer].uv.to_3d() for loop in face.loops]
            uv = barycentric_transform(location, corners[0], corners[1], corners[2], uvs[0], uvs[1], uvs[2]).to_2d()
            if normal.dot(n) < 0.35:
                return None
            line.append({'co': location, 'normal': normal, 'uv': uv, 'a': a, 'b': b})
        grid.append(line)
    return {'grid': grid, 'n': n, 'g': g, 'w': w, 'centre': centre_on_axis}


def patch_quality(patch, moss, width, height):
    """(ok, moss share, worst density ratio): rejects seams (texel density off) and steep folds."""
    grid = patch['grid']
    triangles = []
    worst = 1.0
    for row in range(len(grid) - 1):
        for column in range(len(grid[0]) - 1):
            quad = [grid[row][column], grid[row][column + 1], grid[row + 1][column + 1], grid[row + 1][column]]
            for tri in ((quad[0], quad[1], quad[2]), (quad[0], quad[2], quad[3])):
                world = (tri[1]['co'] - tri[0]['co']).cross(tri[2]['co'] - tri[0]['co']).length * 0.5
                uv_area = abs((tri[1]['uv'] - tri[0]['uv']).cross(tri[2]['uv'] - tri[0]['uv'])) * 0.5
                if world < 1e-6:
                    continue
                ratio = math.sqrt(uv_area / world) * TEXTURE_SIZE / TRUNK_DENSITY_PX_PER_M
                worst = max(worst, ratio, 1.0 / max(ratio, 1e-6))
                for p, q in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
                    edge_ratio = (p['uv'] - q['uv']).length * TEXTURE_SIZE / max((p['co'] - q['co']).length, 1e-6)
                    if edge_ratio > TRUNK_DENSITY_PX_PER_M * 2.2:
                        return False, 1.0, 99.0
                triangles.append((tri[0]['uv'], tri[1]['uv'], tri[2]['uv']))
    share = moss_share(triangles, moss, width, height)
    return worst < 1.7, share, worst


# --------------------------------------------------------------------------------------------------------------- build
def pale_uv(x_m, y_m, extent_x, extent_y, offset):
    """Metres in the broken-face projection -> UV in the pale strip (grain = image y)."""
    x0, y0, x1, y1 = PALE_RECT
    room_x = (x1 - x0) - 2 * PALE_MARGIN_PX
    room_y = (y1 - y0) - 2 * PALE_MARGIN_PX
    scale_x = min(TRUNK_DENSITY_PX_PER_M, room_x / max(extent_x, 1e-4))
    scale_y = min(TRUNK_DENSITY_PX_PER_M, room_y / max(extent_y, 1e-4))
    spare_x = room_x - extent_x * scale_x
    spare_y = room_y - extent_y * scale_y
    px = x0 + PALE_MARGIN_PX + spare_x * offset[0] + x_m * scale_x
    py = y0 + PALE_MARGIN_PX + spare_y * offset[1] + y_m * scale_y
    return Vector((px / TEXTURE_SIZE, 1.0 - py / TEXTURE_SIZE))


def build_piece(index, spec, patch, generator, material):
    name_hint, length, width, base_thickness, rows, columns, shape = spec
    grid = patch['grid']
    n, g, w = patch['n'], patch['g'], patch['w']
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new('UVMap')
    outer = [[bm.verts.new(cell['co']) for cell in line] for line in grid]
    inner = []
    depth = []
    for row, line in enumerate(grid):
        inner_line, depth_line = [], []
        for column, cell in enumerate(line):
            t = thickness(shape, row, column, rows, columns, base_thickness, generator)
            push = (cell['normal'] * 0.6 + n * 0.4).normalized()
            inner_line.append(bm.verts.new(cell['co'] - push * t))
            depth_line.append(t)
        inner.append(inner_line)
        depth.append(depth_line)
    # Projection frame for the broken faces: along grain (a), across (b), into the trunk (d).
    reference = patch['centre']

    def coords(vert):
        offset = vert.co - reference
        return offset.dot(g), offset.dot(w), -offset.dot(n)

    all_coords = [coords(v) for v in bm.verts]
    a_min = min(c[0] for c in all_coords)
    b_min = min(c[1] for c in all_coords)
    d_min = min(c[2] for c in all_coords)
    extent_a = max(c[0] for c in all_coords) - a_min
    extent_b = max(c[1] for c in all_coords) - b_min
    extent_d = max(c[2] for c in all_coords) - d_min
    faces_bark, faces_broken = [], []

    def add_face(verts, bark_uvs=None):
        face = bm.faces.new(verts)
        face.material_index = 0
        if bark_uvs is not None:
            for loop, value in zip(face.loops, bark_uvs):
                loop[uv].uv = value
            faces_bark.append(face)
        else:
            faces_broken.append(face)
        return face

    for row in range(rows - 1):
        for column in range(columns - 1):
            cells = [grid[row][column], grid[row][column + 1], grid[row + 1][column + 1], grid[row + 1][column]]
            verts = [outer[row][column], outer[row][column + 1], outer[row + 1][column + 1], outer[row + 1][column]]
            # Outward winding: g x w = -n would face inward, so order (row,col) -> (row+1,col) -> ... for +n.
            add_face([verts[0], verts[3], verts[2], verts[1]], [cells[0]['uv'], cells[3]['uv'], cells[2]['uv'], cells[1]['uv']])
            add_face([inner[row][column], inner[row][column + 1], inner[row + 1][column + 1], inner[row + 1][column]])
    # Rim: walk the grid boundary.
    boundary = ([(0, c) for c in range(columns)] + [(r, columns - 1) for r in range(1, rows)] +
                [(rows - 1, c) for c in range(columns - 2, -1, -1)] + [(r, 0) for r in range(rows - 2, 0, -1)])
    for k in range(len(boundary)):
        r0, c0 = boundary[k]
        r1, c1 = boundary[(k + 1) % len(boundary)]
        add_face([outer[r0][c0], outer[r1][c1], inner[r1][c1], inner[r0][c0]])
    bm.normal_update()
    # Winding: bark faces point out of the trunk (+n), inner faces into it (-n), rim faces away from the piece centre
    # measured across the slab (the slab is thin and curved, so a plain centroid test fails on the big faces).
    centre = sum((v.co for v in bm.verts), Vector()) / len(bm.verts)
    bad = [f for f in faces_bark if f.normal.dot(n) < 0.0]
    bad += [f for f in faces_broken[:(rows - 1) * (columns - 1)] if f.normal.dot(n) > 0.0]
    for face in faces_broken[(rows - 1) * (columns - 1):]:
        lateral = face.calc_center_median() - centre
        lateral -= n * lateral.dot(n)
        if face.normal.dot(lateral) < 0.0:
            bad.append(face)
    if bad:
        bmesh.ops.reverse_faces(bm, faces=bad)
    bm.normal_update()
    # Broken-face UVs: box projection in (a, b, d), grain always along image y where the face shows it.
    offset = (generator.random(), generator.random())
    for face in faces_broken:
        local = Vector((abs(face.normal.dot(g)), abs(face.normal.dot(w)), abs(face.normal.dot(n))))
        for loop in face.loops:
            a, b, d = coords(loop.vert)
            a -= a_min
            b -= b_min
            d -= d_min
            if local.x >= local.y and local.x >= local.z:
                # End grain (faces along the grain): across x, depth y.
                loop[uv].uv = pale_uv(b, d, extent_b, extent_d, offset)
            elif local.y >= local.z:
                # Side rims: depth x, grain y.
                loop[uv].uv = pale_uv(d, a, extent_d, extent_a, offset)
            else:
                # Inner split face: across x, grain y.
                loop[uv].uv = pale_uv(b, a, extent_b, extent_a, offset)
    # Output frame: X = w, Y = -n, Z = g, pivot at the bounds centre.
    rotation = Matrix((w, -n, g))  # rows: new axes in old coordinates
    for vert in bm.verts:
        vert.co = rotation @ (vert.co - reference)
    # The trunk facets tilt the patch against the axis frame: turn it (about Z, then X) so the thickness axis Y is
    # as thin as it gets, keeping X and Z the two large extents for the C++ area rule.
    best = None
    for yaw_step in range(-14, 15):
        for pitch_step in range(-10, 11):
            turn = Matrix.Rotation(math.radians(pitch_step * 2.5), 3, 'X') @ Matrix.Rotation(math.radians(yaw_step * 2.5), 3, 'Z')
            ys = [(turn @ v.co).y for v in bm.verts]
            extent = max(ys) - min(ys)
            if best is None or extent < best[0] - 1e-6:
                best = (extent, turn)
    for vert in bm.verts:
        vert.co = best[1] @ vert.co
    xs = [v.co.x for v in bm.verts]
    ys = [v.co.y for v in bm.verts]
    zs = [v.co.z for v in bm.verts]
    middle = Vector(((max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(zs) + min(zs)) / 2))
    for vert in bm.verts:
        vert.co -= middle
    name = f'SM_DeadTree_Debris_{index}'
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    triangles = sum(len(p.vertices) - 2 for p in mesh.polygons)
    dims = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    return obj, triangles, dims, len(faces_bark), len(faces_broken)


GLOBAL_SCALE = [1.0]


def export(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # Metre scene + apply_unit_scale + global_scale 1 imports at the right size with import_uniform_scale 1.0
    # (checked 2026-11-09: 8.18 x 4.86 x 38.75 cm for piece 0). Don't import while any PIE runs: an import during PIE
    # gave "Bad MeshDescription" (empty mesh) and a delete during PIE left unsaved zombie assets in memory.
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, global_scale=GLOBAL_SCALE[0],
                             apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', axis_forward='-Z',
                             axis_up='Y', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)


# ------------------------------------------------------------------------------------------------------------- preview
def preview(objects, image, out_folder):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.device = 'CPU'
    scene.render.resolution_x = 640
    scene.render.resolution_y = 480
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.45, 0.5, 0.55, 1.0)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6
    scene.world = world
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(35))
    scene.collection.objects.link(sun)
    camera = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    scene.collection.objects.link(camera)
    scene.camera = camera
    for obj in objects:
        obj.hide_render = True
    for index, obj in enumerate(objects):
        obj.hide_render = False
        size = max(obj.dimensions)
        for view in range(2):
            distance = size * 2.1
            # The bark faces -Y in the output frame: view 0 looks at the bark, view 1 at the broken inside.
            direction = Vector((0.45, -0.8 if view == 0 else 0.8, 0.4)).normalized()
            camera.location = obj.location + direction * distance
            sun.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
            camera.rotation_euler = (obj.location - camera.location).to_track_quat('-Z', 'Y').to_euler()
            scene.render.filepath = os.path.join(out_folder, f'preview_{index}_{"bark" if view == 0 else "inside"}.png')
            bpy.ops.render.render(write_still=True)
        obj.hide_render = True


def make_material(image):
    # The tree import brought a material of the same name; free the name so the FBX slot is exactly MATERIAL_NAME.
    existing = bpy.data.materials.get(MATERIAL_NAME)
    if existing is not None:
        existing.name = MATERIAL_NAME + '_TreeSource'
    material = bpy.data.materials.new(MATERIAL_NAME)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    texture = nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.interpolation = 'Linear'
    material.node_tree.links.new(texture.outputs['Color'], nodes['Principled BSDF'].inputs['Base Color'])
    nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.9
    return material


def main():
    fbx_path, png_path, out_folder, options = parse_arguments()
    os.makedirs(out_folder, exist_ok=True)
    generator = random.Random(options['seed'])
    GLOBAL_SCALE[0] = float(options['global_scale'])
    tree_obj, trunk_bm, uv_layer = import_trunk(fbx_path)
    tree = BVHTree.FromBMesh(trunk_bm)
    axis_point, axis_dir = trunk_axis(trunk_bm)
    image, moss, width, height = load_moss_mask(png_path)
    material = make_material(image)
    report = {'axis_point': list(axis_point), 'axis_dir': list(axis_dir), 'pale_rect_px': PALE_RECT, 'pieces': []}
    objects = []
    used_yaws = []
    for index, spec in enumerate(PIECES):
        best = None
        for attempt in range(MAX_SITE_TRIES):
            site = (generator.uniform(1.1, 3.6), generator.uniform(0.0, math.tau))
            if any(abs(math.remainder(site[1] - other, math.tau)) < 0.25 for other in used_yaws):
                continue
            state = generator.getstate()
            patch = cast_patch(spec, site, tree, trunk_bm, uv_layer, axis_point, axis_dir, generator)
            if patch is None:
                continue
            ok, share, worst = patch_quality(patch, moss, width, height)
            if not ok:
                continue
            score = share + 0.05 * (worst - 1.0)
            if best is None or score < best[0]:
                best = (score, site, patch, share, worst)
            if share < 0.08 and attempt > 40:
                break
        if best is None:
            raise RuntimeError(f'no usable trunk site for piece {index}')
        _, site, patch, share, worst = best
        used_yaws.append(site[1])
        obj, triangles, dims, bark_faces, broken_faces = build_piece(index, spec, patch, generator, material)
        obj.location = (index * 0.8, 0.0, 0.0)
        objects.append(obj)
        uvs_bark = [list(cell['uv']) for line in patch['grid'] for cell in line]
        report['pieces'].append({'index': index, 'kind': spec[0], 'site_height_m': round(site[0], 3),
                                 'site_yaw_deg': round(math.degrees(site[1]), 1), 'moss_share': round(share, 3),
                                 'density_worst_ratio': round(worst, 3), 'triangles': triangles,
                                 'dims_cm_xyz': [round(d * 100, 1) for d in dims], 'bark_faces': bark_faces,
                                 'broken_faces': broken_faces, 'bark_uvs': uvs_bark})
        print('TREE_DEBRIS piece', index, spec[0], 'tris', triangles, 'dims cm', [round(d * 100, 1) for d in dims],
              'moss', round(share, 3), 'density', round(worst, 3), 'site', round(site[0], 2), round(math.degrees(site[1])))
    for obj in objects:
        saved = obj.location.copy()
        obj.location = (0.0, 0.0, 0.0)
        export(obj, os.path.join(out_folder, obj.name + '.fbx'))
        obj.location = saved
    bpy.data.objects.remove(tree_obj, do_unlink=True)
    with open(os.path.join(out_folder, 'debris_report.json'), 'w', encoding='utf-8') as handle:
        json.dump(report, handle, indent=1)
    if options['preview']:
        preview(objects, image, out_folder)
    print('TREE_DEBRIS done')


main()
