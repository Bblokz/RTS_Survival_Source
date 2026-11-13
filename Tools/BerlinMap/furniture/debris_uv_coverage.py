"""Headless Blender: rasterise UV0 of the 16 furniture FBX files into a 2048 coverage mask (which atlas texels are used).

    Blender5.0.exe -b --factory-startup --python debris_uv_coverage.py -- <out.npy>

Used by debris_mask_gen.py: debris shards may only show atlas texels that belong to a furniture surface.
"""
import glob
import sys

import bpy
import numpy as np

FBX = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFurniture/FBX/"
SIZE = 2048
out = sys.argv[sys.argv.index("--") + 1]
cover = np.zeros((SIZE, SIZE), dtype=np.uint8)
for path in sorted(glob.glob(FBX + "SM_BerlinFurn_*.FBX")):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    for obj in bpy.data.objects:
        if obj.type != "MESH" or obj.name.startswith("UCX"):
            continue
        mesh = obj.data
        mesh.calc_loop_triangles()
        uv = mesh.uv_layers[0].data
        for tri in mesh.loop_triangles:
            pts = np.array([[uv[i].uv[0] * SIZE, (1.0 - uv[i].uv[1]) * SIZE] for i in tri.loops])
            x0, y0 = np.floor(pts.min(axis=0)).astype(int)
            x1, y1 = np.ceil(pts.max(axis=0)).astype(int)
            x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, SIZE - 1), min(y1, SIZE - 1)
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            (ax, ay), (bx, by), (cx, cy) = pts
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9:
                continue
            w0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / den
            w1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / den
            inside = (w0 >= -0.01) & (w1 >= -0.01) & (w0 + w1 <= 1.01)
            cover[y0:y1 + 1, x0:x1 + 1][inside] = 255
np.save(out, cover)
print("COVERAGE", out, float((cover > 0).mean()))
