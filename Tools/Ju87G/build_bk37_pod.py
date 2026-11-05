"""Builds the Ju 87 G's BK 3,7 gun pod (one mesh, used mirrored-free on both wings) in headless Blender.

  blender.exe -b --factory-startup -P build_bk37_pod.py -- [--preview]

Writes Saved/Ju87GTest/blender/SM_Ju87G_BK37Pod.fbx (origin = pylon top on the wing underside, +X = barrel forward,
metres -> cm on import) and, with --preview, renders the pods on the exported SK_Stuka_VTO
(Saved/Ju87GTest/inspect/SK_Stuka_VTO.fbx) to Saved/Ju87GTest/blender/preview_*.png.
Placement (shared with author_ju87g_blueprint.py, in UE component space, cm): pod attach point at
x=POD_X, y=+-POD_Y, z=POD_Z. The muzzle socket is added in UE (socket 'Muzzle' at the flash hider tip).
"""
import math
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/Ju87GTest/blender/"
STUKA_FBX = "D:/UE5Projects/RTS_Survival/Saved/Ju87GTest/inspect/SK_Stuka_VTO.fbx"

# Placement on the Stuka (metres, UE component space; Blender's import keeps X and Z).
POD_X, POD_Y, POD_Z = 0.0, 2.45, 1.40

# Pod dimensions (metres), local frame: origin on the pylon top, +X forward, -Z down.
FAIRING_AFT, FAIRING_FWD = -0.80, 1.00
FAIRING_RADIUS = 0.165
FAIRING_CENTER_Z = -0.36
BARREL_END = 2.45
BARREL_RADIUS = 0.038
JACKET_END = 1.45
JACKET_RADIUS = 0.055
HIDER_START, HIDER_END = 2.42, 2.70
HIDER_RADIUS = 0.062
PYLON_AFT, PYLON_FWD = -0.35, 0.55
PYLON_HALF_WIDTH = 0.035
PYLON_TOP = 0.12
CLIP_AFT, CLIP_FWD = -0.10, 0.42
SEGMENTS = 14
MUZZLE_X = HIDER_END


def new_object(name, mesh_builder, material_index):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    mesh_builder(bm)
    for face in bm.faces:
        face.material_index = material_index
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def lathe(profile, center_z):
    """profile: list of (x, radius) from aft to fwd; closed with caps where radius > 0 at the ends."""
    def build(bm):
        rings = []
        for x, radius in profile:
            ring = []
            for i in range(SEGMENTS):
                angle = 2.0 * math.pi * i / SEGMENTS
                ring.append(bm.verts.new((x, radius * math.cos(angle), center_z + radius * math.sin(angle))))
            rings.append(ring)
        for a, b in zip(rings, rings[1:]):
            for i in range(SEGMENTS):
                j = (i + 1) % SEGMENTS
                bm.faces.new((a[i], a[j], b[j], b[i]))
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return build


def box(x0, x1, y0, y1, z0, z1):
    def build(bm):
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector((x0 if v.co.x < 0 else x1, y0 if v.co.y < 0 else y1, z0 if v.co.z < 0 else z1))
    return build


def build_pod():
    fairing = lathe([(FAIRING_AFT, 0.03), (FAIRING_AFT + 0.18, 0.11), (FAIRING_AFT + 0.45, FAIRING_RADIUS),
                     (FAIRING_FWD - 0.35, FAIRING_RADIUS), (FAIRING_FWD - 0.08, 0.12), (FAIRING_FWD, 0.07)],
                    FAIRING_CENTER_Z)
    barrel = lathe([(FAIRING_FWD - 0.02, JACKET_RADIUS), (JACKET_END, JACKET_RADIUS), (JACKET_END + 0.04, BARREL_RADIUS),
                    (BARREL_END, BARREL_RADIUS)], FAIRING_CENTER_Z)
    hider = lathe([(HIDER_START, BARREL_RADIUS + 0.008), (HIDER_START + 0.04, HIDER_RADIUS),
                   (HIDER_END - 0.03, HIDER_RADIUS), (HIDER_END, HIDER_RADIUS - 0.012)], FAIRING_CENTER_Z)
    pylon = box(PYLON_AFT, PYLON_FWD, -PYLON_HALF_WIDTH, PYLON_HALF_WIDTH, FAIRING_CENTER_Z + FAIRING_RADIUS - 0.03,
                PYLON_TOP)
    # The six-round clip sits on top of the breech, in front of the pylon's trailing edge.
    clip = box(CLIP_AFT, CLIP_FWD, -0.075, 0.075, FAIRING_CENTER_Z + FAIRING_RADIUS - 0.02,
               FAIRING_CENTER_Z + FAIRING_RADIUS + 0.09)
    parts = [new_object("Fairing", fairing, 0), new_object("Pylon", pylon, 0), new_object("Clip", clip, 0),
             new_object("Barrel", barrel, 1), new_object("Hider", hider, 1)]
    body = bpy.data.materials.new("MI_Ju87G_BK37Pod_Body")
    steel = bpy.data.materials.new("MI_Ju87G_BK37Pod_Barrel")
    body.diffuse_color = (0.09, 0.12, 0.08, 1.0)
    steel.diffuse_color = (0.05, 0.05, 0.05, 1.0)
    for part in parts:
        part.data.materials.append(body)
        part.data.materials.append(steel)
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    pod = bpy.context.view_layer.objects.active
    pod.name = "SM_Ju87G_BK37Pod"
    pod.data.name = "SM_Ju87G_BK37Pod"
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    for poly in pod.data.polygons:
        poly.use_smooth = False
    return pod


def export_pod(pod):
    bpy.ops.object.select_all(action="DESELECT")
    pod.select_set(True)
    bpy.context.view_layer.objects.active = pod
    bpy.ops.export_scene.fbx(filepath=OUT_DIR + "SM_Ju87G_BK37Pod.fbx", use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                             axis_forward="-Z", axis_up="Y", use_mesh_modifiers=True, add_leaf_bones=False)
    stats = (len(pod.data.vertices), len(pod.data.polygons))
    print("POD_EXPORTED verts=%d faces=%d" % stats)


def render_preview(pod):
    bpy.ops.import_scene.fbx(filepath=STUKA_FBX)
    grey = bpy.data.materials.new("StukaGrey")
    grey.diffuse_color = (0.55, 0.57, 0.6, 1.0)
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj != pod:
            obj.data.materials.clear()
            obj.data.materials.append(grey)
    left = pod.copy()
    left.data = pod.data
    bpy.context.scene.collection.objects.link(left)
    pod.location = (POD_X, POD_Y, POD_Z)
    left.location = (POD_X, -POD_Y, POD_Z)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_cavity = True
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 900
    cam_data = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    views = {
        "front_low": ((9.0, 6.5, -0.5), 9.0),
        "side": ((0.0, 14.0, 1.2), 9.0),
        "under": ((1.0, 4.0, -6.0), 9.0),
        "close": ((4.2, 4.8, 0.6), 4.5),
    }
    for name, (location, lens_dist) in views.items():
        cam.location = location
        target = Vector((0.6, 1.5, 1.2)) if name == "close" else Vector((-1.2, 0.0, 1.6))
        direction = target - Vector(location)
        cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
        cam_data.lens = 35
        scene.render.filepath = OUT_DIR + "preview_%s.png" % name
        bpy.ops.render.render(write_still=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    pod = build_pod()
    export_pod(pod)
    if "--preview" in sys.argv:
        render_preview(pod)


main()
