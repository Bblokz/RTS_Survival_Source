"""Pripyat bog foliage kit (Blender 5 background): reeds, cattail, sedge tussocks, purple algae mats, dead birch snags.

Out: CustomMapItems/PripyatBogFoliage/FBX/*.FBX, PripyatBogFoliage.blend, Preview/BogFoliage_Preview.png and
Saved/Pripyat/assets/bog_foliage_report.json. Cards use T_Prypyat_BogFoliage.png (bog_atlas.py); algae mats reuse the
Phytophage Mat modules (their UVs, Phytophage material); snags use the dead-tree bark atlas region.
No collision on anything (HISM scatter).

blender --background --factory-startup --python blender_bog_foliage.py
"""
import json
import math
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets')
import pry_blender_lib as L  # noqa: E402

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatBogFoliage/'
ATLAS = OUT + 'T_Prypyat_BogFoliage.png'
PHYTO = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/'
BARK_TEX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PolesieDeadSpruces/Textures/T_RTS_DeadTree_1_D.png'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/bog_foliage_report.json'
REED = (0.0, 0.5, 0.5, 1.0)
CATTAIL = (0.5, 0.5, 1.0, 1.0)
SEDGE = (0.0, 0.0, 0.5, 0.5)
THATCH = (0.52, 0.27, 0.98, 0.48)
BROWN = (0.52, 0.02, 0.98, 0.23)
BARK = (0.205, 0.585, 0.385, 0.88)
CARD_MAT = 'M_Prypyat_BogFoliage'
report = {}


def reed(name, seed, height, cards, lying):
    rng = random.Random(seed)
    b = L.Builder(name, [CARD_MAT])
    for k in range(cards):
        yaw = k * 360.0 / cards + rng.uniform(-12, 12)
        off = Vector((rng.uniform(-25, 25), rng.uniform(-25, 25), -5))
        b.card(off, yaw, rng.uniform(80, 115), height * rng.uniform(0.85, 1.0), rng.uniform(4, 16), 0, REED,
               bend=rng.uniform(-10, 25))
    for k in range(lying):
        yaw = rng.uniform(0, 360)
        b.card(Vector((rng.uniform(-15, 15), rng.uniform(-15, 15), 0)), yaw, rng.uniform(70, 95),
               height * rng.uniform(0.55, 0.7), rng.uniform(62, 75), 0, REED, up_normal=0.9)
    return b


def cattail(name, seed):
    rng = random.Random(seed)
    b = L.Builder(name, [CARD_MAT])
    for k in range(6):
        b.card(Vector((rng.uniform(-20, 20), rng.uniform(-20, 20), -5)), k * 60 + rng.uniform(-10, 10),
               rng.uniform(75, 95), rng.uniform(130, 150), rng.uniform(5, 20), 0, CATTAIL)
    for k in range(2):
        b.card(Vector((0, 0, 0)), rng.uniform(0, 360), 80, 100, rng.uniform(60, 72), 0, CATTAIL, up_normal=0.9)
    for k in range(6):                      # chunky brown heads, readable from above
        x, y = rng.uniform(-35, 35), rng.uniform(-35, 35)
        z = rng.uniform(115, 150)
        lean = Vector((rng.uniform(-6, 6), rng.uniform(-6, 6), 0))
        b.prism((x, y, z), Vector((x, y, z + 30)) + lean, 6.0, 6, 0, BROWN)
        b.prism((x, y, z - 60), (x, y, z), 1.6, 4, 0, THATCH, cap=False)
    return b


def sedge(name, seed, radius, cards):
    rng = random.Random(seed)
    b = L.Builder(name, [CARD_MAT])
    rings, segs, top = 3, 9, radius * 0.6
    pts = {}
    for r in range(rings + 1):
        phi = (math.pi / 2) * r / rings
        for s in range(segs):
            th = 2 * math.pi * s / segs
            rr = radius * math.cos(phi) * rng.uniform(0.9, 1.08)
            pts[(r, s)] = Vector((rr * math.cos(th), rr * math.sin(th), -12 + (top + 12) * math.sin(phi)))
    u0, v0, u1, v1 = THATCH
    for r in range(rings):
        for s in range(segs):
            n = (s + 1) % segs
            quad = [pts[(r, s)], pts[(r, n)], pts[(r + 1, n)], pts[(r + 1, s)]]
            uvs = [(u0 + (u1 - u0) * (s + i % 2) / segs, v0 + (v1 - v0) * (r + (1 if i >= 2 else 0)) / rings)
                   for i in (0, 1, 3, 2)]
            uvs = [uvs[0], uvs[1], uvs[3], uvs[2]]
            b.face(quad, 0, uvs, normal=(0, 0, 1))
    for k in range(cards):
        yaw = k * 360.0 / cards + rng.uniform(-15, 15)
        out = Vector((-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0))
        b.card(out * radius * 0.3 + Vector((0, 0, top * 0.5)), yaw, radius * rng.uniform(1.3, 1.7),
               radius * rng.uniform(1.0, 1.3), rng.uniform(40, 62), 0, SEDGE, up_normal=0.85)
    return b


def algae(name, src, seed, size):
    rng = random.Random(seed)
    L.reset_scene()
    bpy.ops.import_scene.fbx(filepath=PHYTO + src)
    obj = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    for o in bpy.context.scene.objects:
        o.select_set(o == obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = obj.data
    xs = [v.co.copy() for v in me.vertices]
    half = max(max(abs(p.x) for p in xs), max(abs(p.y) for p in xs))
    phase = [rng.uniform(0, 6.28) for _ in range(3)]
    sx, sy = size / (2 * half), size * rng.uniform(0.6, 0.85) / (2 * half)
    for v in me.vertices:
        th = math.atan2(v.co.y, v.co.x)
        r = 1.0 + 0.18 * math.sin(3 * th + phase[0]) + 0.1 * math.sin(5 * th + phase[1]) + 0.06 * math.sin(7 * th + phase[2])
        v.co.x *= sx * r
        v.co.y *= sy * r
        v.co.z = 2.0 + 3.0 * (0.5 + 0.5 * math.sin(v.co.x * 0.02 + phase[1]) * math.cos(v.co.y * 0.025))
    obj.name = name
    me.materials.clear()
    me.materials.append(bpy.data.materials.new('M_Prypyat_BogAlgae'))
    return obj


def snag(name, seed, height, radius, branches, lean):
    rng = random.Random(seed)
    b = L.Builder(name, ['M_Prypyat_BogSnag'])
    tilt = Vector((math.sin(math.radians(lean)), 0, math.cos(math.radians(lean))))
    segs = 4
    prev, prev_r = Vector((0, 0, -15)), radius
    for k in range(segs):
        t = (k + 1) / segs
        nxt = tilt * height * t + Vector((rng.uniform(-6, 6), rng.uniform(-6, 6), 0))
        nr = radius * (1 - 0.55 * t)
        v0 = BARK[1] + (BARK[3] - BARK[1]) * k / segs
        v1 = BARK[1] + (BARK[3] - BARK[1]) * (k + 1) / segs
        b.prism(prev, nxt, prev_r, 7, 0, (BARK[0], v0, BARK[2], v1), radius_b=nr, cap=(k == segs - 1))
        prev, prev_r = nxt, nr
    # broken jagged top: a short leaning splinter
    b.prism(prev, prev + Vector((rng.uniform(-15, 15), rng.uniform(-15, 15), radius * 3)), prev_r * 0.8, 4, 0,
            BARK, radius_b=1.0)
    for k in range(branches):
        z = height * rng.uniform(0.35, 0.8)
        a = tilt * z
        d = Vector((math.cos(k * 2.4 + seed), math.sin(k * 2.4 + seed), rng.uniform(0.3, 0.8))).normalized()
        b.prism(a, a + d * rng.uniform(50, 110), radius * 0.28, 5, 0, BARK, radius_b=1.5)
    return b


def main():
    L.reset_scene()
    built = []
    specs = [
        (reed('SM_PrypyatBog_Reed_A', 1, 245, 9, 3), (0, 0)),
        (reed('SM_PrypyatBog_Reed_B', 2, 195, 7, 2), (300, 0)),
        (cattail('SM_PrypyatBog_Cattail_A', 3), (600, 0)),
        (sedge('SM_PrypyatBog_Sedge_A', 4, 42, 8), (0, 300)),
        (sedge('SM_PrypyatBog_Sedge_B', 5, 58, 10), (300, 300)),
        (snag('SM_PrypyatBog_Snag_A', 6, 260, 11, 1, 4), (600, 300)),
        (snag('SM_PrypyatBog_Snag_B', 7, 380, 13, 2, 7), (900, 300)),
    ]
    for builder, offset in specs:
        obj = builder.to_object()
        report[obj.name] = L.export_fbx(obj, OUT + 'FBX/' + obj.name + '.FBX')
        built.append((obj.name, offset))
    keep = {}
    for name, src, seed, size in (('SM_PrypyatBog_AlgaeMat_A', 'SM_Phytophage_Mat_01_LOD0.FBX', 8, 260),
                                  ('SM_PrypyatBog_AlgaeMat_B', 'SM_Phytophage_Mat_02_LOD0.FBX', 9, 390)):
        obj = algae(name, src, seed, size)
        report[name] = L.export_fbx(obj, OUT + 'FBX/' + name + '.FBX')
    # Preview scene: rebuild everything side by side with preview materials.
    L.reset_scene()
    card = L.image_material(CARD_MAT, ATLAS, alpha=True)
    bark = L.image_material('M_Prypyat_BogSnag', BARK_TEX)
    algae_mat = L.flat_material('M_Prypyat_BogAlgae', (0.28, 0.08, 0.36), 0.4)
    for path, (name, off) in zip([report[n]['fbx'] for n, _ in built], built):
        bpy.ops.import_scene.fbx(filepath=path)
        o = bpy.context.selected_objects[0]
        o.location = (off[0], off[1], 0)
        for i, m in enumerate(o.data.materials):
            o.data.materials[i] = bark if 'Snag' in (m.name if m else '') else card
    for name, off in (('SM_PrypyatBog_AlgaeMat_A', (0, 700)), ('SM_PrypyatBog_AlgaeMat_B', (500, 700))):
        bpy.ops.import_scene.fbx(filepath=report[name]['fbx'])
        o = bpy.context.selected_objects[0]
        o.location = (off[0], off[1], 0)
        o.data.materials[0] = algae_mat
    bpy.ops.wm.save_as_mainfile(filepath=OUT + 'PripyatBogFoliage.blend')
    L.render_preview(OUT + 'Preview/BogFoliage_Preview.png', target=(450, 350, 80), distance=1900,
                     elevation_deg=55, azimuth_deg=-70, ground_rgb=(0.12, 0.13, 0.09))
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    print('BOG_DONE', json.dumps({k: (v['min'], v['max'], v['tris']) for k, v in report.items()}))


main()
