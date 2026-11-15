"""Blender (background): UCX collision for the Polesie village + signs, and an FBX of the partisan memorial.

Writes <kit>/UE/<name>.FBX (source FBX untouched) and Saved/Pripyat/assets/partA_ucx_report.json with the mesh size
(cm), slots and hull boxes. FBX convention as the Ore kit: scene unit 0.01, vertices in cm, identity objects.

blender --background --factory-startup --python blender_partA_ucx.py
"""
import json
import math
import os

import bmesh
import bpy
import numpy as np

CMI = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/partA_ucx_report.json'
report = {}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 0.01


def bake(obj):
    for o in bpy.context.scene.objects:
        o.select_set(o == obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def verts(obj, slot=None):
    me = obj.data
    if slot is None:
        co = np.array([v.co[:] for v in me.vertices])
    else:
        idx = {i for p in me.polygons if p.material_index == slot for i in p.vertices}
        co = np.array([me.vertices[i].co[:] for i in sorted(idx)])
    return co


def pbox(co, zlo, zhi, plo=2, phi=98, z0=0.0, z1=None):
    sel = co[(co[:, 2] >= zlo) & (co[:, 2] <= zhi)]
    lo = [float(np.percentile(sel[:, i], plo)) for i in range(2)]
    hi = [float(np.percentile(sel[:, i], phi)) for i in range(2)]
    return [lo[0], lo[1], z0], [hi[0], hi[1], z1 if z1 is not None else zhi]


def box_object(name, lo, hi):
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    bmesh.ops.convex_hull(bm, input=vs)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export(main, name, boxes, out_dir):
    hulls = [box_object('UCX_%s_%02d' % (name, k), lo, hi) for k, (lo, hi) in enumerate(boxes)]
    main.name = name
    for o in bpy.context.scene.objects:
        o.select_set(o == main or o in hulls)
    bpy.context.view_layer.objects.active = main
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=out_dir + name + '.FBX', use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             object_types={'MESH'}, use_mesh_modifiers=False, mesh_smooth_type='FACE',
                             bake_space_transform=False, add_leaf_bones=False, use_custom_props=False)
    co = verts(main)
    report[name] = dict(fbx=out_dir + name + '.FBX', size_cm=[round(float(v), 2) for v in co.max(0) - co.min(0)],
                        min_cm=[round(float(v), 1) for v in co.min(0)], max_cm=[round(float(v), 1) for v in co.max(0)],
                        slots=[m.name if m else None for m in main.data.materials],
                        used_slots=sorted({p.material_index for p in main.data.polygons}),
                        hulls=[[[round(v, 1) for v in lo], [round(v, 1) for v in hi]] for lo, hi in boxes],
                        uvs=[u.name for u in main.data.uv_layers],
                        tris=sum(len(p.vertices) - 2 for p in main.data.polygons))


def import_main(path):
    reset()
    bpy.ops.import_scene.fbx(filepath=path)
    main = [o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.name.startswith('UCX_')][0]
    bake(main)
    return main


# ------------------------------------------------------------------ village
VIL = CMI + 'PolesieVillage/'
for name in ('SM_LogHouse_ReedThatch_A', 'SM_LogHouse_ReedThatch_B'):
    main = import_main(VIL + name + '.FBX')
    co = verts(main, 0)
    top = float(verts(main)[:, 2].max())
    walls = pbox(co, 40, 160, 3, 97, 0.0, 300.0)
    cx, cy = (walls[0][0] + walls[1][0]) / 2, (walls[0][1] + walls[1][1]) / 2
    hx, hy = (walls[1][0] - walls[0][0]) * 0.36, (walls[1][1] - walls[0][1]) * 0.36
    roof = ([cx - hx, cy - hy, 300.0], [cx + hx, cy + hy, top * 0.9])
    export(main, name, [walls, roof], VIL + 'UE/')
for name in ('SM_StorkNestPole_A', 'SM_StorkNestPole_B'):
    main = import_main(VIL + name + '.FBX')
    co = verts(main, 0)
    export(main, name, [pbox(co, 80, 260, 5, 95, 0.0, 300.0)], VIL + 'UE/')
main = import_main(VIL + 'SM_WoodenSweepWell.FBX')
co = verts(main, 0)
shaft = pbox(co, 5, 90, 3, 97, 0.0, 110.0)
post = pbox(co, 150, 230, 10, 90, 0.0, 260.0)
export(main, 'SM_WoodenSweepWell', [shaft, post], VIL + 'UE/')

# ------------------------------------------------------------------ signs
SIG = CMI + 'PolesieSigns/'
for name in ('SM_RoadSign_BlankRoad', 'SM_RoadSign_Mozyr', 'SM_RoadSign_Pinsk', 'SM_RoadSign_Stolin',
             'SM_RoadSign_Turov', 'SM_RailSign_BlankRail', 'SM_RailSign_Kalinkavichy', 'SM_RailSign_Luninets'):
    main = import_main(SIG + name + '.FBX')
    co = verts(main)
    export(main, name, [(list(co.min(0)), list(co.max(0)))], SIG + 'UE/')

# ------------------------------------------------------------------ partisan memorial
MEM = CMI + 'PartisanMemorial/'
bpy.ops.wm.open_mainfile(filepath=MEM + 'PartisanMemorial.blend')
coll = bpy.data.collections['PartisanMemorial_Asset']
objs = [o for o in coll.all_objects if o.type in ('MESH', 'FONT')]
bpy.ops.object.select_all(action='DESELECT')
for o in objs:
    o.hide_set(False)
    o.select_set(True)
bpy.context.view_layer.objects.active = [o for o in objs if o.type == 'MESH'][0]
fonts = [o for o in objs if o.type == 'FONT']
if fonts:
    bpy.ops.object.select_all(action='DESELECT')
    for o in fonts:
        o.select_set(True)
    bpy.context.view_layer.objects.active = fonts[0]
    bpy.ops.object.convert(target='MESH')
    objs = [o for o in coll.all_objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for o in objs:
    o.select_set(True)
bpy.context.view_layer.objects.active = objs[0]
bpy.ops.object.convert(target='MESH')          # apply bevel etc. modifiers
bpy.ops.object.join()
mem = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# Three slots: concrete (aggregate, cracks, recesses, lettering), paint (faded red star), iron (audio log box).
slot_of = {}
for i, m in enumerate(mem.data.materials):
    n = m.name.lower() if m else ''
    slot_of[i] = 1 if 'red' in n else (2 if 'iron' in n else 0)
new_index = [slot_of.get(p.material_index, 0) for p in mem.data.polygons]
mem.data.materials.clear()
for n in ('M_Polesie_MemorialConcrete', 'M_Polesie_MemorialRed', 'M_Polesie_MemorialIron'):
    mem.data.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
mem.data.polygons.foreach_set('material_index', new_index)
mem.data.update()
# metres -> cm, pivot on the ground at the foundation centre
co = np.array([v.co[:] for v in mem.data.vertices])
mem.data.transform(__import__('mathutils').Matrix.Diagonal((100.0, 100.0, 100.0, 1.0)))
mem.data.update()
# Box UVs for the tiling concrete texture: 1 UV = 2 m; a copy as lightmap UV.
bm = bmesh.new()
bm.from_mesh(mem.data)
uv = bm.loops.layers.uv.verify()
for f in bm.faces:
    n = f.normal
    ax = max(range(3), key=lambda i: abs(n[i]))
    for loop in f.loops:
        p = loop.vert.co
        a, b = [(p.y, p.z), (p.x, p.z), (p.x, p.y)][ax]
        loop[uv].uv = (a / 200.0, b / 200.0)
bm.to_mesh(mem.data)
bm.free()
while len(mem.data.uv_layers) > 1:
    mem.data.uv_layers.remove(mem.data.uv_layers[-1])
mem.data.uv_layers[0].name = 'UVMap'
lm = mem.data.uv_layers.new(name='LightMapUV')
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
mem.data.uv_layers.active = lm
bpy.ops.uv.smart_project(island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')
mem.data.uv_layers.active = mem.data.uv_layers[0]
for o in list(bpy.context.scene.objects):
    if o != mem:
        bpy.data.objects.remove(o)
bpy.context.scene.unit_settings.scale_length = 0.01
co = verts(mem)
found = (co[:, 2] < 40)
boxes = [pbox(co, 0, 30, 0, 100, 0.0, 32.0),
         pbox(co, 50, 125, 1, 99, 30.0, 140.0),
         pbox(co, 145, 520, 0, 100, 140.0, float(co[:, 2].max()))]
export(mem, 'SM_PartisanMemorial', boxes, MEM + 'UE/')

os.makedirs(os.path.dirname(REPORT), exist_ok=True)
with open(REPORT, 'w') as handle:
    json.dump(report, handle, indent=1)
print('PARTA_UCX_DONE', len(report))
