# blender -b <work>/deploy.blend --factory-startup -P vat_bake.py -- <building work dir> [--preview]
# writes <work>/vat_offset.exr, vat_rot.exr, deploy_mesh.fbx (or vat_preview_*.png with --preview)
# PROTOTYPE 2: bakes the keyframed piece objects of the .blend into two vertex animation textures and writes the
# VAT mesh (source HQ mesh + piece index/pivot in UV4-5).
#   T_HQDeploy_VAT_Offset.exr  RGBA = pivot offset from rest (Unreal cm) + uniform scale     (width = pieces)
#   T_HQDeploy_VAT_Rot.exr     RGBA = rotation quaternion in Unreal space (x, y, z, w)        (height = frames)
# Frame f is written to image row (frames-1-f) so that in Unreal (V = 0 at the top) frame f sits at
# v = (f + 0.5) / frames. Mesh: UV4 = (piece u = (i + 0.5) / width, pivot.x), UV5 = (pivot.y, pivot.z);
# the static truck gets u = -1. V is written as 1 - value because Unreal's FBX import flips V.
# --preview renders workbench frames of the animation instead of baking.
import json
import os
import sys

import bpy
from mathutils import Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
out_dir = argv[0]
src_fbx, table = os.path.join(out_dir, 'building.fbx'), os.path.join(out_dir, 'pieces_vat.json')
scene = bpy.context.scene
f0, f1 = scene['deploy_frame_start'], scene['deploy_frame_end']
frames = f1 - f0 + 1
pieces_objs = sorted((o for o in scene.objects if 'piece_id' in o), key=lambda o: o['piece_id'])
width = len(pieces_objs)

if '--preview' in argv:
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'RANDOM'
    scene.render.resolution_x, scene.render.resolution_y = 900, 700
    cam_data = bpy.data.cameras.new('cam')
    cam = bpy.data.objects.new('cam', cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    c = common.from_ue(Vector((-148, -49, 250)))
    cam.location = c + Vector((16, -14, 15))
    cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
    for p in (0.0, 0.1, 0.25, 0.4, 0.55, 0.7, 0.82, 0.9, 1.0):
        scene.frame_set(int(round(f0 + (f1 - f0) * p)))
        scene.render.filepath = os.path.join(out_dir, 'vat_preview_%03d.png' % int(p * 100))
        bpy.ops.render.render(write_still=True)
    sys.exit(0)

offset_px = [0.0] * (width * frames * 4)
rot_px = [0.0] * (width * frames * 4)
max_offset = 0.0
prev = [None] * width
for f in range(frames):
    scene.frame_set(f0 + f)
    row = frames - 1 - f
    for i, ob in enumerate(pieces_objs):
        loc, rot, sca = ob.matrix_world.decompose()
        rest = common.from_ue(Vector(ob['rest_pivot_ue']))
        d = loc - rest
        off = Vector((d.x * common.TO_CM, -d.y * common.TO_CM, d.z * common.TO_CM))
        # mirror Blender's Y into Unreal: quaternion (w, x, y, z) -> (w, -x, y, -z)
        q = Quaternion((rot.w, -rot.x, rot.y, -rot.z))
        if prev[i] is not None and prev[i].dot(q) < 0:
            q = -q  # keep neighbouring frames in one hemisphere so bilinear filtering blends the short way
        prev[i] = q
        k = (row * width + i) * 4
        offset_px[k:k + 4] = (off.x, off.y, off.z, sca.x)
        rot_px[k:k + 4] = (q.x, q.y, q.z, q.w)
        max_offset = max(max_offset, off.length)
print('baked', width, 'pieces x', frames, 'frames; max offset cm', round(max_offset, 1))
# last frame must be the rest pose exactly
last = (0 * width) * 4
bad = [i for i in range(width) if abs(offset_px[i * 4]) + abs(offset_px[i * 4 + 1]) + abs(offset_px[i * 4 + 2]) > 0.05
       or abs(offset_px[i * 4 + 3] - 1.0) > 1e-3]
print('pieces not at rest on the last frame:', bad[:20])


def save(name, px):
    img = bpy.data.images.new(name, width, frames, alpha=True, float_buffer=True)
    img.colorspace_settings.name = 'Non-Color'
    img.pixels.foreach_set(px)
    img.filepath_raw = os.path.join(out_dir, name + '.exr')
    img.file_format = 'OPEN_EXR'
    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.image_settings.color_depth = '16'
    scene.render.image_settings.exr_codec = 'ZIP'
    img.save_render(img.filepath_raw, scene=scene)
    print('wrote', img.filepath_raw)


save('vat_offset', offset_px)
save('vat_rot', rot_px)

# --- mesh: the untouched source mesh + piece index and pivot -------------------------------------------------
data = json.load(open(table))
obj = common.load_fbx(src_fbx)  # resets the scene (factory settings)
me = obj.data
piece_of_part = data['piece_of_part']
vert_piece = [piece_of_part[p] for p in data['vert_part']]
# the shader reads TexCoord 4 / 5: pad meshes with fewer UV sets so the VAT layers land exactly there
CONFIG = json.load(open(os.path.join(out_dir, 'config.json')))
if len(me.uv_layers) > 4 and CONFIG.get('drop_extra_uvs'):
    # config "drop_extra_uvs": only when no material of the mesh reads TexCoord 4+ (check the masters first);
    # the building mesh itself keeps them, only the deploy mesh loses them
    while len(me.uv_layers) > 4:
        print('dropping UV set', me.uv_layers[-1].name)
        me.uv_layers.remove(me.uv_layers[-1])
if len(me.uv_layers) > 4:
    raise SystemExit('source mesh has %d UV sets; VAT needs them at 4 and 5 (see config drop_extra_uvs)'
                     % len(me.uv_layers))
while len(me.uv_layers) < 4:
    me.uv_layers.new(name='VatPad%d' % len(me.uv_layers))
uv4 = me.uv_layers.new(name='VatPiece')
uv5 = me.uv_layers.new(name='VatPivot')
for poly in me.polygons:
    r = data['pieces'][vert_piece[poly.vertices[0]]]
    u = -1.0 if r['cat'] == 'truck' else (r['id'] + 0.5) / width
    px, py, pz = r['vat_pivot']
    for li in poly.loop_indices:
        uv4.data[li].uv = (u, 1.0 - px)
        uv5.data[li].uv = (py, 1.0 - pz)
common.export_fbx(os.path.join(out_dir, 'deploy_mesh.fbx'))
print('wrote mesh', os.path.join(out_dir, 'deploy_mesh.fbx'), 'uv', [u.name for u in me.uv_layers])
