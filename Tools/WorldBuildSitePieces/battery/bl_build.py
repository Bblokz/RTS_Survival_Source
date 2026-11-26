"""Blender (headless): builds the battery world map piece from the game's own meshes and writes one FBX.

Sources (exported from Unreal by dump_sources.py / dump_pit.py, sFH 18 from the commander portrait assets):
  - SM_GerPit_Howitzer (the German howitzer pit building), one of its LODs
  - SK_Ger_SFH18Anim (the sFH 18 team weapon), barrel and cradle raised about the trunnion, decimated
  - SM_GerPit_Shell_150 (150 mm shells), a few laid out behind the gun
Run: blender.exe -b --factory-startup -P bl_build.py -- [preview]
Out: Saved/WorldBuildSitePieces/battery/SM_BuildSitePiece_Battery.fbx (+ preview_*.png with "preview")
Units: the FBX is written so that one Blender unit = one Unreal unit; the Unreal import uses scale 1.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

SAVED = "D:/UE5Projects/RTS_Survival/Saved/WorldBuildSitePieces/battery/"
SRC = SAVED + "src/"
SFH_FBX = "D:/UE5Projects/RTS_Survival/CommanderPortraits/Assets/SFH18/SK_Ger_SFH18Anim.fbx"
OUT_FBX = SAVED + "SM_BuildSitePiece_Battery.fbx"
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

PIT_LOD = 3
GUN_DECIMATE = 0.40          # sFH 18 23.4k tris -> about 9.4k
PIT_CROP_RADIUS = 6.3        # metres from the pit centre: the outside of the sandbag ring
PIT_CROP_MIDDLE = 5.7        # metres: islands centred farther out are stray panel and post pieces
GUN_FORWARD = 0.6            # metres: the gun moves forward so its trail spades stay inside the ring
GUN_SCALE = 1.45             # gun a bit larger than real against the pit, so the barrel reads from far away
ELEVATION = 38.0             # barrel elevation in degrees
SFH_PIVOT = Vector((-0.339, 0.0, 1.473))   # trunnion (metres, from the commander portrait tool)
PIECE_DIAMETER = 39.0        # Unreal units; contract: inside a 40 unit circle
PIECE_YAW = 35.0             # degrees: barrel points diagonally across the map camera's view
BARREL_THICKEN = 2.0         # the raised parts get this much thicker across the barrel, else the barrel is ~1 px on the map
BARREL_AXIS_Z = 1.45         # metres: height of the barrel axis before it is raised
LIFT = 0.25                  # Unreal units: the pit floor stays off the hex plate's plane (no z-fighting)
# Source material -> slot name of the piece (the Unreal material instance MI_<slot>).
SLOTS = {"M_GerCamoConcretePalet_Atlas": "BuildSitePiece_Battery_Pit", "M_Ger_AmmoBox": "BuildSitePiece_Battery_Ammo",
         "M_GerSandBagScaffold_Atlas": "BuildSitePiece_Battery_Sandbags", "MI_SFH18_Gun": "BuildSitePiece_Battery_Gun",
         "MI_SFH18_Wheels": "BuildSitePiece_Battery_Wheels", "MI_SFH18_Trail": "BuildSitePiece_Battery_Trail"}
# Base colour textures, written small for the Unreal import (the piece is tens of pixels wide on the map).
ASSETS = "D:/UE5Projects/RTS_Survival/CommanderPortraits/Assets/"
TEXTURES = {"T_BuildSitePiece_Battery_Gun_D": (ASSETS + "SFH18/T_SFH18_Gun_BaseColor.png", 512),
            "T_BuildSitePiece_Battery_Wheels_D": (ASSETS + "SFH18/T_SFH18_Wheels_BaseColor.png", 256),
            "T_BuildSitePiece_Battery_Trail_D": (ASSETS + "SFH18/T_SFH18_Trail_BaseColor.png", 512),
            "T_BuildSitePiece_Battery_Sandbags_D": (ASSETS + "LaserPlatform/T_Sn0Sn1Sn2Scaffold_Atlas_Diffuse.png", 1024)}


def import_meshes(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False)
    new = set(bpy.data.objects) - before
    meshes = {}
    for obj in new:
        if obj.type == "MESH":
            data = obj.data.copy()
            data.transform(obj.matrix_world)
            meshes[obj.name] = data
    for obj in new:
        bpy.data.objects.remove(obj, do_unlink=True)
    return meshes


def link(name, data):
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def raise_barrel(data, elevation):
    """The islands above the axle and forward of the trail legs (barrel, cradle, recoil gear) turn about the trunnion."""
    matrix = (Matrix.Translation(SFH_PIVOT) @ Matrix.Rotation(-math.radians(elevation), 4, "Y")
              @ Matrix.Translation(-SFH_PIVOT))
    bm = bmesh.new()
    bm.from_mesh(data)
    seen = set()
    moved = 0
    for start in bm.verts:
        if start.index in seen:
            continue
        stack, island = [start], []
        seen.add(start.index)
        while stack:
            vert = stack.pop()
            island.append(vert)
            for edge in vert.link_edges:
                other = edge.other_vert(vert)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
        low = [min(v.co[i] for v in island) for i in range(3)]
        if low[2] > 1.05 and low[0] > -1.6:
            for vert in island:
                vert.co.y *= BARREL_THICKEN
                vert.co.z = BARREL_AXIS_Z + (vert.co.z - BARREL_AXIS_Z) * BARREL_THICKEN
                vert.co = matrix @ vert.co
            moved += len(island)
    bm.to_mesh(data)
    bm.free()
    print("BATTERY raised %d verts" % moved)


def islands(bm):
    seen = set()
    for start in bm.verts:
        if start.index in seen:
            continue
        stack, island = [start], []
        seen.add(start.index)
        while stack:
            vert = stack.pop()
            island.append(vert)
            for edge in vert.link_edges:
                other = edge.other_vert(vert)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
        yield island


def crop_pit(data):
    """Drops the islands outside the sandbag ring (ammo boxes and panels that stick out), so the ring fills the hex."""
    bm = bmesh.new()
    bm.from_mesh(data)
    # Centre of the sandbag ring: the box of the sandbag faces (the ammo boxes outside would pull a whole-mesh box).
    bag_slot = [m.name.split(".")[0] for m in data.materials].index("M_GerSandBagScaffold_Atlas")
    bag_verts = {v for face in bm.faces if face.material_index == bag_slot for v in face.verts}
    xs = [v.co.x for v in bag_verts]
    ys = [v.co.y for v in bag_verts]
    centre = Vector(((min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0))
    print("BATTERY ring centre (%.2f %.2f) half size (%.2f %.2f)" % (
        centre.x, centre.y, (max(xs) - min(xs)) / 2.0, (max(ys) - min(ys)) / 2.0))
    doomed = []
    kept = 0
    for island in islands(bm):
        middle = sum((v.co.xy for v in island), Vector((0.0, 0.0))) / len(island)
        reach = max((v.co.xy - centre).length for v in island)
        if reach > PIT_CROP_RADIUS or (middle - centre).length > PIT_CROP_MIDDLE:
            doomed.extend(island)
            print("BATTERY crop island verts=%d middle r=%.2f reach=%.2f" % (
                len(island), (middle - centre).length, reach))
        else:
            kept += 1
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(data)
    bm.free()
    print("BATTERY pit kept %d islands" % kept)


def decimate(obj, ratio):
    modifier = obj.modifiers.new("Decimate", "DECIMATE")
    modifier.ratio = ratio
    modifier.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    pit_meshes = import_meshes(SRC + "SM_GerPit_Howitzer_LODs.fbx")
    pit = link("Pit", pit_meshes["SM_GerPit_Howitzer_LOD%d" % PIT_LOD])
    crop_pit(pit.data)
    gun_data = list(import_meshes(SFH_FBX).values())[0]
    raise_barrel(gun_data, ELEVATION)
    gun = link("Gun", gun_data)
    decimate(gun, GUN_DECIMATE)
    # The pit Blueprint puts the gun at its origin, 4 cm up.
    gun.data.transform(Matrix.Translation((GUN_FORWARD, 0.0, 0.04)) @ Matrix.Scale(GUN_SCALE, 4))
    parts = [pit, gun]
    print("BATTERY tris pit=%d gun=%d" % (tris(pit), tris(gun)))

    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = pit
    bpy.ops.object.join()
    piece = bpy.context.view_layer.objects.active
    piece.name = "SM_BuildSitePiece_Battery"

    # Centre the footprint, base on z = 0, turn, and scale so the farthest vertex fits the circle.
    data = piece.data
    xs = [v.co.x for v in data.vertices]
    ys = [v.co.y for v in data.vertices]
    centre = Vector(((min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0, 0.0))
    data.transform(Matrix.Translation(-centre))
    radius = max(v.co.xy.length for v in data.vertices)
    scale = (PIECE_DIAMETER / 2.0) / radius
    data.transform(Matrix.Rotation(math.radians(PIECE_YAW), 4, "Z"))
    data.transform(Matrix.Scale(scale, 4))
    low_z = min(v.co.z for v in data.vertices)
    for vert in data.vertices:  # the low LOD dips a little under the ground plane; flatten it onto z = 0
        vert.co.z = max(vert.co.z, 0.0) + LIFT
    print("BATTERY footprint radius %.3f m -> scale %.4f; lowest z %.3f" % (radius, scale, low_z))
    high = [max(v.co[i] for v in data.vertices) for i in range(3)]
    low = [min(v.co[i] for v in data.vertices) for i in range(3)]
    print("BATTERY final min=%s max=%s tris=%d radius=%.2f" % (
        tuple(round(v, 2) for v in low), tuple(round(v, 2) for v in high), tris(piece),
        max(v.co.xy.length for v in data.vertices)))
    for index, material in enumerate(data.materials):
        material.name = SLOTS[material.name.split(".")[0]]
    print("BATTERY materials %s" % [m.name for m in data.materials])
    return piece


def write_textures():
    for name, (source, size) in TEXTURES.items():
        image = bpy.data.images.load(source)
        print("BATTERY texture %s source %dx%d" % (name, image.size[0], image.size[1]))
        if image.size[0] > size:
            image.scale(size, size)
        image.filepath_raw = SAVED + "tex/" + name + ".png"
        image.file_format = "PNG"
        os.makedirs(SAVED + "tex", exist_ok=True)
        image.save()


def export(piece):
    bpy.ops.object.select_all(action="DESELECT")
    piece.select_set(True)
    # 1 Blender unit = 1 Unreal unit: the scene is in metres, so write with a 0.01 global scale.
    bpy.ops.export_scene.fbx(filepath=OUT_FBX, use_selection=True, global_scale=0.01, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             axis_forward="-Z", axis_up="Y", bake_space_transform=False, add_leaf_bones=False,
                             path_mode="AUTO")
    print("BATTERY wrote %s (%d bytes)" % (OUT_FBX, os.path.getsize(OUT_FBX)))


def preview(piece):
    colours = {}
    old = {"M_GerCamoConcretePalet_Atlas": (0.45, 0.42, 0.36), "M_Ger_AmmoBox": (0.35, 0.3, 0.15),
               "M_GerSandBagScaffold_Atlas": (0.6, 0.52, 0.38), "MI_SFH18_Gun": (0.25, 0.28, 0.3),
               "MI_SFH18_Wheels": (0.1, 0.1, 0.1), "MI_SFH18_Trail": (0.3, 0.32, 0.3)}
    for material in piece.data.materials:
        key = [k for k, v in SLOTS.items() if v == material.name][0]
        material.diffuse_color = old.get(key, (1, 0, 1)) + (1.0,)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.show_shadows = True
    scene.render.resolution_x = scene.render.resolution_y = 900
    camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera.data.lens = 85
    for name, pitch in (("map", 70.0), ("low", 30.0)):
        distance = 150.0
        # Unreal map camera looks along +X; Unreal Y = -Blender Y after the FBX round trip.
        direction = Vector((math.cos(math.radians(pitch)), 0.0, -math.sin(math.radians(pitch))))
        camera.location = -direction * distance + Vector((0, 0, 5))
        camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = SAVED + "preview_%s.png" % name
        bpy.ops.render.render(write_still=True)


piece = build()
export(piece)
if "textures" in ARGS:
    write_textures()
if "preview" in ARGS:
    preview(piece)
