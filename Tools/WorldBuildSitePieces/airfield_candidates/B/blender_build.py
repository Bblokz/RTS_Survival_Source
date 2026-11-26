"""Blender (headless): builds airfield candidate B, "hangar and apron", from the exported game models.

blender.exe -b --factory-startup -P blender_build.py -- <airfield_B dir>
In : base.obj, roof_open.obj, stuka.obj (export_posed.py; metres, Blender axes)
Out: SM_BuildSitePiece_Airfield_B.fbx, paint_params.json (plane markings for the Unreal material), preview_*.png

Layout in Unreal units (the map camera looks along +X, 70 degrees down; -X is the side nearest the camera):
- Back (+X): the German airbase building (SM_Ger_AircraftBuilding_BaseMesh + SK_Ger_AircraftBuilding_Roof posed
  open), turned 180 degrees so its ground-level mouth faces the apron and the control tower stands at the back.
  Z is exaggerated a little so the walls and open roof flaps read from above.
- Front (-X): a light concrete apron with a white centre line, white edge lines and yellow threshold bars.
- On the apron, rolled out of the mouth: the game's Ju 87 (SK_Stuka_VTO) at about 2x the building's scale, nose to
  the camera, yawed 12 degrees. Markings (yellow wing tips and fuselage band, Balkenkreuz) are painted in the Unreal
  material from paint_params.json, so the plane needs no extra geometry.
Material slots: HB_Walls, HB_Roof, HB_Floor, HB_Pad, PL_Body, AP_Concrete, AP_Mark, AP_Accent.
"""
import json
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

folder = sys.argv[sys.argv.index("--") + 1]
PIECE_NAME = "SM_BuildSitePiece_Airfield_B"
UNIT = 0.01                      # one Unreal unit in Blender metres (the FBX export keeps 1 m = 100 units).
MAX_RADIUS = 19.5                # footprint circle is 40 units: radius 20 minus a margin.

# Building (units).
BUILDING_FRONT_X = 1.8           # the mouth starts here, the tower end is further +X
BUILDING_DEPTH = 16.0
BUILDING_Z_BOOST = 1.35
BUILDING_TRIS = 6800
# Plane.
PLANE_SPAN = 18.0
PLANE_YAW = 12.0                 # Unreal yaw degrees, applied after the 180 degree turn (nose to -X)
PLANE_AT = (-8.6, 0.0)           # Unreal x, y of the source origin (wing root)
PLANE_TRIS = 5200
SOURCE_SPAN_CM = 1243.4
# Apron (units).
APRON_X = (-16.8, 2.2)
APRON_HALF_WIDTH = 9.4
APRON_TOP = 0.6
MARK_HEIGHT = 0.06

COLOURS = {  # preview only (linear), close to the Unreal instances
    "HB_Walls": (0.62, 0.43, 0.16), "HB_Roof": (0.20, 0.23, 0.08), "HB_Floor": (0.50, 0.50, 0.47),
    "HB_Pad": (0.80, 0.80, 0.75), "PL_Body": (0.10, 0.15, 0.07), "AP_Concrete": (0.62, 0.60, 0.54),
    "AP_Mark": (0.90, 0.90, 0.86), "AP_Accent": (0.95, 0.62, 0.02),
}
BASE_SLOTS = {"0": "HB_Floor", "1": "HB_Walls", "2": "HB_Walls", "3": "HB_Pad", "4": "HB_Walls"}


def material(name):
    found = bpy.data.materials.get(name)
    if found is None:
        found = bpy.data.materials.new(name)
    return found


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=folder + "/" + name + ".obj", forward_axis="Y", up_axis="Z")
    objects = [o for o in bpy.data.objects if o not in before]
    assert len(objects) == 1, name
    obj = objects[0]
    # export_posed.py winds for the mirrored frame the wrong way round: flip to outward normals (positive volume).
    if signed_volume(obj) < 0.0:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        bm.to_mesh(obj.data)
        bm.free()
        print("BUILD %s faces flipped, volume now %.4f" % (name, signed_volume(obj)))
    return obj


def signed_volume(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    volume = bm.calc_volume(signed=True)
    bm.free()
    return volume


def remap_slots(obj, mapping):
    """Replace the object's slots by the piece slots; mapping: source slot name -> piece slot name."""
    mesh = obj.data
    old = [m.name if m else "" for m in mesh.materials]
    targets = []
    for name in old:
        target = mapping(name)
        if target not in targets:
            targets.append(target)
    indices = [targets.index(mapping(name)) for name in old]
    wanted = [indices[polygon.material_index] for polygon in mesh.polygons]
    # materials.clear() resets the face indices, so assign them after the new slots exist.
    mesh.materials.clear()
    for target in targets:
        mesh.materials.append(material(target))
    for polygon, index in zip(mesh.polygons, wanted):
        polygon.material_index = index


def decimate(obj, target):
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris <= target:
        return tris
    bpy.context.view_layer.objects.active = obj
    modifier = obj.modifiers.new("decimate", "DECIMATE")
    modifier.ratio = target / tris
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def points(obj):
    return [obj.matrix_world @ v.co for v in obj.data.vertices]


def to_blender(x, y, z=0.0):
    """Unreal units -> Blender metres (Unreal y is mirrored)."""
    return Vector((x * UNIT, -y * UNIT, z * UNIT))


def build_building():
    base = load("base")
    roof = load("roof_open")
    remap_slots(base, lambda name: BASE_SLOTS.get(name.split("_")[0], "HB_Walls"))
    remap_slots(roof, lambda name: "HB_Roof")
    bpy.ops.object.select_all(action="DESELECT")
    base.select_set(True)
    roof.select_set(True)
    bpy.context.view_layer.objects.active = base
    bpy.ops.object.join()
    building = base
    print("BUILD building source tris %d volume %.2f" % (sum(len(p.vertices) - 2 for p in building.data.polygons),
                                                         signed_volume(building)))
    print("BUILD building decimated to %d tris" % decimate(building, BUILDING_TRIS))
    mesh = building.data
    # Mouth (source +X) to the apron (-X): turn 180 degrees about Z.
    mesh.transform(Matrix.Rotation(math.pi, 4, "Z"))
    cloud = [v.co for v in mesh.vertices]
    min_x, max_x = min(p.x for p in cloud), max(p.x for p in cloud)
    mid_y = (min(p.y for p in cloud) + max(p.y for p in cloud)) * 0.5
    min_z = min(p.z for p in cloud)
    scale = BUILDING_DEPTH * UNIT / (max_x - min_x)
    for _attempt in range(40):
        transform = (Matrix.Translation(Vector((BUILDING_FRONT_X * UNIT, 0.0, 0.0)))
                     @ Matrix.Diagonal(Vector((scale, scale, scale * BUILDING_Z_BOOST, 1.0)))
                     @ Matrix.Translation(Vector((-min_x, -mid_y, -min_z))))
        radius = max(math.hypot(*(transform @ p).xy) for p in cloud) / UNIT
        if radius <= MAX_RADIUS:
            break
        scale *= 0.98
    mesh.transform(transform)
    print("BUILD building scale %.6f (%.4f units per cm) radius %.2f" % (scale, scale / UNIT / 100.0, radius))
    return building, scale


def wing_chord_centre(cloud, span_min, span_max):
    xs = [p.x for p in cloud if span_min <= abs(p.y) <= span_max and p.z > 1.0]
    return (min(xs) + max(xs)) * 0.5, min(xs), max(xs)


def build_plane():
    plane = load("stuka")
    remap_slots(plane, lambda name: "PL_Body")
    volume = signed_volume(plane)
    print("BUILD stuka volume %.3f (positive = outward winding)" % volume)
    cloud_cm = [Vector((v.co.x * 100.0, -v.co.y * 100.0, v.co.z * 100.0)) for v in plane.data.vertices]  # Unreal cm
    print("BUILD stuka decimated to %d tris" % decimate(plane, PLANE_TRIS))
    scale_units = PLANE_SPAN / SOURCE_SPAN_CM           # units per source cm
    yaw_blender = math.radians(-PLANE_YAW)
    transform = (Matrix.Translation(to_blender(PLANE_AT[0], PLANE_AT[1], APRON_TOP))
                 @ Matrix.Rotation(math.pi + yaw_blender, 4, "Z")
                 @ Matrix.Scale(scale_units * 100.0 * UNIT, 4))
    plane.data.transform(transform)
    nose = min(points(plane), key=lambda p: p.x)
    print("BUILD plane nose (Unreal) x %.2f y %.2f (yaw %+.0f: expect y %s 0)" % (
        nose.x / UNIT, -nose.y / UNIT, PLANE_YAW, "<" if PLANE_YAW > 0 else ">"))
    # Marking frame for the Unreal material: plane-local l = R(-yaw) (p.xy - centre); lx = -x_src * s, ly = -y_src * s.
    chord_mid, chord_min, chord_max = wing_chord_centre(cloud_cm, 400.0, 470.0)
    params = {
        "PlaneX": PLANE_AT[0], "PlaneY": PLANE_AT[1], "PlaneYaw": PLANE_YAW,
        "TipY": 505.0 * scale_units,
        "BandX0": 360.0 * scale_units, "BandX1": 445.0 * scale_units,
        "CrossX": -chord_mid * scale_units, "CrossY": 425.0 * scale_units,
        "CrossL": 50.0 * scale_units, "CrossW": 13.0 * scale_units, "CrossB": 9.0 * scale_units,
    }
    print("BUILD wing chord at 4.0-4.7 m span: x %.0f..%.0f cm; params %s" % (chord_min, chord_max, params))
    return plane, params, scale_units


def box(bm, x0, x1, y0, y1, z0, z1, slot):
    created = bmesh.ops.create_cube(bm, size=1.0)
    verts = created["verts"]
    for vert in verts:
        vert.co = Vector(((x0 if vert.co.x < 0 else x1) * UNIT, -(y0 if vert.co.y < 0 else y1) * UNIT,
                          (z0 if vert.co.z < 0 else z1) * UNIT))
    faces = {f for v in verts for f in v.link_faces}
    for face in faces:
        face.material_index = slot
        if face.normal.z < -0.5 and z0 <= 0.0:
            face.tag = True   # bottom on the plate: never seen
    # Mirrored y flipped the cube's winding.
    bmesh.ops.reverse_faces(bm, faces=list(faces))


def build_apron():
    mesh = bpy.data.meshes.new("apron")
    obj = bpy.data.objects.new("apron", mesh)
    bpy.context.scene.collection.objects.link(obj)
    for name in ("AP_Concrete", "AP_Mark", "AP_Accent"):
        mesh.materials.append(material(name))
    bm = bmesh.new()
    box(bm, APRON_X[0], APRON_X[1], -APRON_HALF_WIDTH, APRON_HALF_WIDTH, 0.0, APRON_TOP, 0)
    top, mark_top = APRON_TOP, APRON_TOP + MARK_HEIGHT
    # Edge lines.
    for side in (-1.0, 1.0):
        y = side * (APRON_HALF_WIDTH - 0.75)
        box(bm, APRON_X[0] + 0.6, APRON_X[1] - 0.3, y - 0.3, y + 0.3, top, mark_top, 1)
    # Yellow threshold bars ("piano keys") across the near end.
    for y in (-6.3, -3.9, -1.5, 1.5, 3.9, 6.3):
        box(bm, -16.2, -14.0, y - 0.75, y + 0.75, top, mark_top, 2)
    # White centre line dashes from the threshold to the hangar mouth.
    x = -12.8
    while x + 1.8 < APRON_X[1] - 0.4:
        box(bm, x, x + 1.8, -0.35, 0.35, top, mark_top, 1)
        x += 3.2
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.tag], context="FACES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    return obj


def preview(piece):
    for slot in piece.material_slots:
        node_tree = slot.material
        node_tree.use_nodes = True
        bsdf = node_tree.node_tree.nodes["Principled BSDF"]
        colour = COLOURS.get(slot.material.name, (1, 0, 1)) + (1.0,)
        bsdf.inputs["Base Color"].default_value = colour
        bsdf.inputs["Emission Color"].default_value = colour
        bsdf.inputs["Emission Strength"].default_value = 0.6
    # Context: dark green plate with a light rim, ground behind (not exported).
    scene = bpy.context.scene
    plate_material = bpy.data.materials.new("plate")
    plate_material.use_nodes = True
    plate_material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.03, 0.08, 0.03, 1)
    rim_material = bpy.data.materials.new("rim")
    rim_material.use_nodes = True
    rim_material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.6, 0.7, 0.55, 1)
    ground_material = bpy.data.materials.new("ground")
    ground_material.use_nodes = True
    ground_material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.05, 0.07, 0.05, 1)
    bpy.ops.mesh.primitive_circle_add(vertices=6, radius=28.8 * UNIT / math.cos(math.radians(30)), fill_type="NGON",
                                      location=(0, 0, -0.0002))
    bpy.context.object.data.materials.append(rim_material)
    bpy.ops.mesh.primitive_circle_add(vertices=6, radius=23.0 * UNIT, fill_type="NGON", location=(0, 0, -0.0001))
    bpy.context.object.data.materials.append(plate_material)
    bpy.ops.mesh.primitive_plane_add(size=3.0, location=(0, 0, -0.0004))
    bpy.context.object.data.materials.append(ground_material)

    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    world = bpy.data.worlds.new("w")
    world.color = (0.35, 0.33, 0.3)
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 2.5
    sun.rotation_euler = (math.radians(40), 0, math.radians(-60))
    scene.collection.objects.link(sun)
    camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    camera.data.type = "ORTHO"
    scene.collection.objects.link(camera)
    scene.camera = camera
    look_at = Vector((0, 0, 0.03))
    shots = (("map", Vector((-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70)))), 0.66, 700),
             ("map_small", Vector((-math.cos(math.radians(70)), 0.0, math.sin(math.radians(70)))), 0.66, 140),
             ("side", Vector((-1.0, -0.7, 0.45)), 0.5, 700))
    for name, direction, ortho, size in shots:
        direction.normalize()
        camera.data.ortho_scale = ortho
        camera.location = look_at + direction * 5.0
        camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        scene.render.resolution_x = size
        scene.render.resolution_y = size
        scene.render.filepath = folder + "/preview_" + name + ".png"
        bpy.ops.render.render(write_still=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    building, building_scale = build_building()
    plane, params, plane_scale = build_plane()
    apron = build_apron()
    bpy.ops.object.select_all(action="DESELECT")
    for obj in (building, plane, apron):
        obj.select_set(True)
    bpy.context.view_layer.objects.active = building
    bpy.ops.object.join()
    piece = building
    piece.name = PIECE_NAME
    piece.data.name = PIECE_NAME
    cloud = points(piece)
    tris = sum(len(p.vertices) - 2 for p in piece.data.polygons)
    top = max(cloud, key=lambda p: p.z)
    nose = min(cloud, key=lambda p: p.x)
    print("BUILD final x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f units, max radius %.2f, tris %d, volume %.4f" % (
        min(p.x for p in cloud) / UNIT, max(p.x for p in cloud) / UNIT, -max(p.y for p in cloud) / UNIT,
        -min(p.y for p in cloud) / UNIT, min(p.z for p in cloud) / UNIT, max(p.z for p in cloud) / UNIT,
        max(math.hypot(p.x, p.y) for p in cloud) / UNIT, tris, signed_volume(piece)))
    print("BUILD highest point (Unreal) x %.2f y %.2f z %.2f; most -X point x %.2f y %.2f" % (
        top.x / UNIT, -top.y / UNIT, top.z / UNIT, nose.x / UNIT, -nose.y / UNIT))
    print("BUILD building %.4f units/cm, plane %.4f units/cm (plane/building %.2f)" % (
        building_scale / UNIT / 100.0, plane_scale, plane_scale / (building_scale / UNIT / 100.0)))
    for index, slot in enumerate(piece.material_slots):
        faces = sum(1 for p in piece.data.polygons if p.material_index == index)
        print("BUILD slot %d %s faces %d" % (index, slot.material.name, faces))
    params["Tris"] = tris
    with open(folder + "/paint_params.json", "w", encoding="utf-8") as handle:
        json.dump(params, handle, indent=1)
    bpy.ops.object.select_all(action="DESELECT")
    piece.select_set(True)
    bpy.ops.export_scene.fbx(filepath=folder + "/" + PIECE_NAME + ".fbx", use_selection=True, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", axis_forward="-Z",
                             axis_up="Y", use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
    print("BUILD wrote FBX")
    preview(piece)


main()
