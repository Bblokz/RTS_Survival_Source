# blender -b --factory-startup -P analyze.py -- <in.fbx> <out_dir>
# Prints loose-part statistics of the HQ building mesh and renders top/oblique views coloured per part
# (or per material with --material) so the piece grouping can be planned. PROTOTYPE tool.
import colorsys
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
src, out_dir = argv[0], argv[1]
by_material = '--material' in argv
obj = common.load_fbx(src)
parts = common.Parts(obj)
me = obj.data
print('verts', len(me.vertices), 'polys', len(me.polygons), 'parts', parts.count)
sizes = sorted(((parts.hi[i] - parts.lo[i]).length for i in range(parts.count)), reverse=True)
print('largest part diagonals', [round(s) for s in sizes[:20]])
print('parts under 30 cm', sum(1 for s in sizes if s < 30), 'under 100 cm', sum(1 for s in sizes if s < 100))
for i in sorted(range(parts.count), key=lambda k: -(parts.hi[k] - parts.lo[k]).length)[:40]:
    lo, hi = parts.lo[i], parts.hi[i]
    print('part', i, 'faces', parts.faces[i], 'mat', obj.material_slots[parts.main_material(i)].material.name,
          'lo', tuple(round(c) for c in lo), 'hi', tuple(round(c) for c in hi))

random.seed(3)
colors = []
for i in range(max(parts.count, len(obj.material_slots))):
    r, g, b = colorsys.hsv_to_rgb(random.random(), 0.55 + 0.4 * random.random(), 0.6 + 0.4 * random.random())
    colors.append((r, g, b, 1.0))
attr = me.color_attributes.new('PartColor', 'FLOAT_COLOR', 'CORNER')
for p in me.polygons:
    c = colors[p.material_index] if by_material else colors[parts.poly_part[p.index]]
    for li in p.loop_indices:
        attr.data[li].color = c

scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'VERTEX'
scene.display.shading.light = 'STUDIO'
scene.render.resolution_x = 1400
scene.render.resolution_y = 1400
lo = Vector((min(v.co.x for v in me.vertices), min(v.co.y for v in me.vertices), min(v.co.z for v in me.vertices)))
hi = Vector((max(v.co.x for v in me.vertices), max(v.co.y for v in me.vertices), max(v.co.z for v in me.vertices)))
centre = (lo + hi) / 2
span = max(hi - lo)
cam_data = bpy.data.cameras.new('cam')
cam = bpy.data.objects.new('cam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
tag = 'mat' if by_material else 'parts'
views = {
    'top': (centre + Vector((0, 0, span * 2)), True),
    'oblique': (centre + Vector((span * 1.2, span * 1.0, span * 1.1)), False),
    'oblique2': (centre + Vector((-span * 1.2, -span * 1.0, span * 1.1)), False),
}
for name, (eye, ortho) in views.items():
    cam.location = eye
    d = centre - eye
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    cam_data.type = 'ORTHO' if ortho else 'PERSP'
    cam_data.ortho_scale = span * 1.1
    cam_data.lens = 35
    cam_data.clip_end = span * 20
    scene.render.filepath = os.path.join(out_dir, '%s_%s.png' % (tag, name))
    bpy.ops.render.render(write_still=True)
print('rendered to', out_dir)
