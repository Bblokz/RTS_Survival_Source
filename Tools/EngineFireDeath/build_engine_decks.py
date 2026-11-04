"""Splits the engine deck off the hero tank wreck hulls for the engine fire (fuel explosion) death.

Run headless:  blender.exe -b --factory-startup --python build_engine_decks.py -- <mode> <out_dir> [tank ...]
Modes:
  roundtrip  exports each source hull unchanged (proves the export/reimport keeps bounds, sockets and slots).
  build      cuts the rear deck out of the hull, lines the hole with a burnt engine bay and exports
             SM_Destroyed_<Tank>_Hull.fbx (reimported over the existing wreck hull) and
             SM_Destroyed_<Tank>_EngineDeck.fbx, plus <Tank>_EngineDeck.blend (render_engine_decks.py previews it).

The deck keeps the hull's pivot, so the wreck Blueprints attach the EngineDeck component under Hull with an
identity transform. Deck plates get a thin solidified underside and everything new (underside, bay walls, floor,
engine block) uses one extra material slot M_EngineBay (burnt steel in Unreal).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

SOURCE_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/'
BAY_MATERIAL = 'M_EngineBay'

# Deck box in hull space (cm, Unreal axes: +X forward, +Y right). Faces whose centre lies inside the box and above
# CutZ (and that lie entirely above CutZ) become the deck. BayDepth is how far the burnt engine bay drops below it.
TANKS = {
    'Tiger1': dict(source='Tiger1/SM_Destroyed_Tiger1_Hull.fbx', x=(-292.0, -85.0), half_y=150.0, cut_z=160.0,
                   bay_depth=60.0),
    'PantherD': dict(source='PantherD/SM_Destroyed_PantherD_Hull.fbx', x=(-412.0, -218.0), half_y=124.0,
                     cut_z=170.0, bay_depth=60.0),
    'Tiger2': dict(source='KT/SM_Destroyed_Tiger2_Hull.fbx', x=(-372.0, -190.0), half_y=138.0, cut_z=175.0,
                   bay_depth=60.0),
}

DECK_THICKNESS = 0.03
# A vertex this close (cm) to a deck box plane lies on the cut.
PLANE_TOLERANCE_CM = 0.05
CM = 0.01


def parse_args():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    mode = args[0] if args else 'build'
    out_dir = args[1] if len(args) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Export', 'Decks')
    tanks = args[2:] or list(TANKS)
    return mode, out_dir, tanks


def import_hull(source):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SOURCE_DIR + source)
    hull = next(obj for obj in bpy.context.scene.objects if obj.type == 'MESH')
    for obj in list(bpy.context.scene.objects):
        if obj is not hull:
            bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    hull.select_set(True)
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return hull


def to_blender(x_cm, y_cm, z_cm):
    # Unreal +Y is Blender -Y.
    return Vector((x_cm * CM, -y_cm * CM, z_cm * CM))


def get_bay_material_index(mesh):
    material = bpy.data.materials.get(BAY_MATERIAL) or bpy.data.materials.new(BAY_MATERIAL)
    material.diffuse_color = (0.08, 0.07, 0.06, 1.0)
    if material.name not in [slot.name for slot in mesh.materials if slot]:
        mesh.materials.append(material)
    return [slot.name if slot else '' for slot in mesh.materials].index(material.name)


def is_in_deck_band(face, cut_z):
    return min(vert.co.z for vert in face.verts) > cut_z


def cut_along_deck_box(bm, tank):
    cut_z = tank['cut_z'] * CM
    planes = [
        (to_blender(tank['x'][0], 0, 0), Vector((1, 0, 0))),
        (to_blender(tank['x'][1], 0, 0), Vector((1, 0, 0))),
        (to_blender(0, tank['half_y'], 0), Vector((0, 1, 0))),
        (to_blender(0, -tank['half_y'], 0), Vector((0, 1, 0))),
    ]
    for plane_co, plane_no in planes:
        band_faces = [face for face in bm.faces if is_in_deck_band(face, cut_z)]
        band_edges = {edge for face in band_faces for edge in face.edges}
        band_verts = {vert for face in band_faces for vert in face.verts}
        bmesh.ops.bisect_plane(bm, geom=list(band_verts) + list(band_edges) + band_faces, dist=1e-5,
                               plane_co=plane_co, plane_no=plane_no)


def is_deck_face(face, tank):
    centre = face.calc_center_median()
    x_cm, y_cm = centre.x / CM, -centre.y / CM
    return (tank['x'][0] < x_cm < tank['x'][1] and abs(y_cm) < tank['half_y']
            and is_in_deck_band(face, tank['cut_z'] * CM))


def add_box(bm, centre, size, material_index, inward=False):
    hx, hy, hz = size.x / 2, size.y / 2, size.z / 2
    c = centre
    corners = [Vector((c.x + sx * hx, c.y + sy * hy, c.z + sz * hz)) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    quads = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]
    verts = [bm.verts.new(corner) for corner in corners]
    faces = []
    for quad in quads:
        ordered = [verts[i] for i in (reversed(quad) if inward else quad)]
        face = bm.faces.new(ordered)
        face.material_index = material_index
        faces.append(face)
    return faces


def add_cylinder_x(bm, centre, radius, length, material_index, segments=10):
    rings = []
    for end in (-0.5, 0.5):
        ring = []
        for i in range(segments):
            angle = 2 * math.pi * i / segments
            ring.append(bm.verts.new(centre + Vector((end * length, math.cos(angle) * radius, math.sin(angle) * radius))))
        rings.append(ring)
    faces = []
    for i in range(segments):
        j = (i + 1) % segments
        faces.append(bm.faces.new((rings[0][i], rings[0][j], rings[1][j], rings[1][i])))
    faces.append(bm.faces.new(list(reversed(rings[0]))))
    faces.append(bm.faces.new(rings[1]))
    for face in faces:
        face.material_index = material_index
    return faces


def is_on_cut_plane(vert, tank):
    x_cm, y_cm = vert.co.x / CM, -vert.co.y / CM
    return (abs(x_cm - tank['x'][0]) < PLANE_TOLERANCE_CM or abs(x_cm - tank['x'][1]) < PLANE_TOLERANCE_CM
            or abs(abs(y_cm) - tank['half_y']) < PLANE_TOLERANCE_CM)


def get_cut_edges(deck_faces, tank):
    """Edges where the deck met the rest of the hull on the box planes; after the cut they are the rim of the hole."""
    deck_set = set(deck_faces)
    cut_edges = []
    for face in deck_faces:
        for edge in face.edges:
            touches_hull = any(linked not in deck_set for linked in edge.link_faces)
            if touches_hull and all(is_on_cut_plane(vert, tank) for vert in edge.verts):
                cut_edges.append(edge)
    return list(set(cut_edges))


def add_bay_walls(bm, cut_edge_coords, floor_z, centre, material_index):
    """Drops a wall from every rim edge to the bay floor so the hole has no slits however uneven the roof is."""
    faces = []
    for top_a, top_b in cut_edge_coords:
        corners = [top_a, top_b, Vector((top_b.x, top_b.y, floor_z)), Vector((top_a.x, top_a.y, floor_z))]
        if (top_a - top_b).length < 1e-4:
            continue
        verts = [bm.verts.new(corner) for corner in corners]
        face = bm.faces.new(verts)
        face.normal_update()
        to_centre = Vector((centre.x, centre.y, face.calc_center_median().z)) - face.calc_center_median()
        if face.normal.dot(to_centre) < 0:
            face.normal_flip()
        faces.append(face)
    return faces


def build_engine_bay(bm, tank, cut_edge_coords, material_index):
    """Burnt bay under the deck opening with a crude engine block; normals face into the bay."""
    x0, x1 = tank['x'][0] * CM, tank['x'][1] * CM
    half_y = tank['half_y'] * CM
    rim_bottom = min(min(top_a.z, top_b.z) for top_a, top_b in cut_edge_coords)
    floor_z = rim_bottom - tank['bay_depth'] * CM
    centre = Vector(((x0 + x1) / 2, 0, floor_z))
    new_faces = add_bay_walls(bm, cut_edge_coords, floor_z, centre, material_index)
    floor_verts = [bm.verts.new(Vector((x, y, floor_z))) for x, y in ((x0, -half_y), (x0, half_y), (x1, half_y),
                                                                      (x1, -half_y))]
    new_faces.append(bm.faces.new(floor_verts))

    length = (x1 - x0) * 0.62
    block_height = tank['bay_depth'] * CM * 0.55
    new_faces += add_box(bm, Vector((centre.x, 0, floor_z + block_height / 2)),
                         Vector((length, half_y * 0.75, block_height)), material_index)
    # Cylinder banks either side of the block.
    for side in (-1, 1):
        new_faces += add_cylinder_x(bm, Vector((centre.x, side * half_y * 0.48, floor_z + block_height * 0.9)),
                                    half_y * 0.13, length * 0.92, material_index)
    # Radiator boxes in the rear corners, where the grilles were.
    for side in (-1, 1):
        new_faces += add_box(bm, Vector((x0 + (x1 - x0) * 0.12, side * half_y * 0.7, floor_z + block_height * 0.6)),
                             Vector(((x1 - x0) * 0.18, half_y * 0.45, block_height * 1.2)), material_index)
    for face in new_faces:
        face.material_index = material_index
        face.smooth = False
    print('BAY rim edges=%d floor=%.1f cm' % (len(cut_edge_coords), floor_z / CM))
    return new_faces


def box_project_uvs(bm, faces, scale=1.0):
    for uv_layer in bm.loops.layers.uv.values():
        for face in faces:
            normal = face.normal
            axis = max(range(3), key=lambda i: abs(normal[i]))
            for loop in face.loops:
                co = loop.vert.co
                u, v = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][axis]
                loop[uv_layer].uv = (u * scale, v * scale)


def split_deck(hull, tank):
    mesh = hull.data
    bay_index = get_bay_material_index(mesh)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    cut_along_deck_box(bm, tank)
    bm.faces.ensure_lookup_table()
    deck_flags = [is_deck_face(face, tank) for face in bm.faces]
    deck_faces = [face for face, is_deck in zip(bm.faces, deck_flags) if is_deck]
    if not deck_faces:
        raise RuntimeError('no deck faces for %s' % hull.name)
    cut_edge_coords = [tuple(vert.co.copy() for vert in edge.verts) for edge in get_cut_edges(deck_faces, tank)]

    # Deck object: a copy of the cut hull without everything that is not deck.
    deck_bm = bm.copy()
    deck_bm.faces.ensure_lookup_table()
    bmesh.ops.delete(deck_bm, geom=[face for face, is_deck in zip(deck_bm.faces, deck_flags) if not is_deck],
                     context='FACES')
    bmesh.ops.delete(bm, geom=deck_faces, context='FACES')
    bay_faces = build_engine_bay(bm, tank, cut_edge_coords, bay_index)
    box_project_uvs(bm, bay_faces)
    bm.to_mesh(mesh)
    bm.free()

    deck_name = hull.name + '_EngineDeck'
    deck_mesh = mesh.copy()
    deck_mesh.name = deck_name
    deck_bm.to_mesh(deck_mesh)
    deck_bm.free()
    deck = bpy.data.objects.new(deck_name, deck_mesh)
    bpy.context.scene.collection.objects.link(deck)
    thicken_deck(deck, bay_index)
    print('DECK %s faces=%d' % (hull.name, len(deck_mesh.polygons)))
    return deck


def thicken_deck(deck, bay_index):
    """Solidify the plates so the tumbling deck is not see-through from below; the new faces get the bay material."""
    original_faces = len(deck.data.polygons)
    modifier = deck.modifiers.new('Thickness', 'SOLIDIFY')
    modifier.thickness = DECK_THICKNESS
    modifier.offset = -1.0
    modifier.use_rim = True
    bpy.ops.object.select_all(action='DESELECT')
    deck.select_set(True)
    bpy.context.view_layer.objects.active = deck
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bm = bmesh.new()
    bm.from_mesh(deck.data)
    bm.faces.ensure_lookup_table()
    new_faces = list(bm.faces)[original_faces:]
    for face in new_faces:
        face.material_index = bay_index
    bm.to_mesh(deck.data)
    bm.free()


def export_fbx(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', add_leaf_bones=False,
                             bake_anim=False, axis_forward='-Z', axis_up='Y', use_custom_props=False)
    print('EXPORT %s' % path)


def main():
    mode, out_dir, tanks = parse_args()
    os.makedirs(out_dir, exist_ok=True)
    for key in tanks:
        tank = TANKS[key]
        hull = import_hull(tank['source'])
        hull_name = os.path.splitext(os.path.basename(tank['source']))[0]
        if mode == 'roundtrip':
            export_fbx(hull, os.path.join(out_dir, hull_name + '_Roundtrip.fbx'))
            continue
        deck = split_deck(hull, tank)
        export_fbx(hull, os.path.join(out_dir, hull_name + '.fbx'))
        export_fbx(deck, os.path.join(out_dir, hull_name.replace('_Hull', '_EngineDeck') + '.fbx'))
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, key + '_EngineDeck.blend'))


if __name__ == '__main__':
    main()
