"""Clearance of the targets against the Werk meshes (Blender 5.x, background).

    Blender5.0.exe -b --factory-startup -P ow_targets_clearance.py
For every design, probe columns under each target's footprint points (Unreal frame, cm): reports Werk concrete above
the target's base (+30 cm) and the stamp ground against the target's base. Results: Targets/clearance.json.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ow_targets_fit as F  # noqa: E402
from ow_targets_parts import L  # noqa: E402

OUT = F.ROOT + 'clearance.json'


def footprint(name, step=40.0):
    """Probe points of a target FBX in its own frame (Blender): vertices of its UCX hulls on a grid + post spots."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=F.ROOT + 'FBX/' + name + '.fbx')
    new = [o for o in bpy.data.objects if o not in before]
    pts = []
    for o in new:
        if not o.name.startswith('UCX_'):
            continue
        vs = [o.matrix_world @ v.co for v in o.data.vertices]
        lo = Vector([min(v[a] for v in vs) for a in range(3)])
        hi = Vector([max(v[a] for v in vs) for a in range(3)])
        nx, ny = max(1, int((hi.x - lo.x) / step)), max(1, int((hi.y - lo.y) / step))
        for i in range(nx + 1):
            for j in range(ny + 1):
                pts.append((lo.x + (hi.x - lo.x) * i / nx, lo.y + (hi.y - lo.y) * j / ny, lo.z, hi.z, o.name))
    for o in new:
        bpy.data.objects.remove(o)
    return pts


def main():
    report = {}
    for key, nm in F.NAMES.items():
        L.reset_scene()
        name, socks = F.werk(key)
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        dg = bpy.context.evaluated_depsgraph_get()
        tree = BVHTree.FromObject(meshes[0], dg)
        H = np.load(F.WERKE + name + '/' + name + '_Stamp.npz')['H'] * 100.0

        def ground(x, y):
            i = int(round((x / 100 + 90) / 0.5))
            j = int(round((y / 100 + 90) / 0.5))
            return float(H[j, i])
        pl = json.load(open(F.WERKE + name + '/' + name + '_Placements.json'))
        spots = {k['kind']: (k['x'], k['y'], k['z'], k['yaw']) for k in pl['kit'] if k['kind'] in ('mast', 'portal')}
        e = socks['Entrance']
        sites = {'SM_OW_Target_CommandPorch': ((e.x, -e.y, e.z), F.ENTRANCE_YAW[key]),
                 'SM_OW_Target_Jammer': (spots['mast'][:3], spots['mast'][3]),
                 'SM_OW_Target_RailGate': (spots['portal'][:3], spots['portal'][3])}
        rows = {}
        for tname, (o, yaw) in sites.items():
            hits, gaps = [], []
            for (bx, by, z0, z1, hull) in footprint(tname):
                # Blender local -> Unreal local (y flip) -> rotate by the Unreal yaw -> Unreal world
                lx, ly = bx, -by
                c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                ux, uy = o[0] + lx * c - ly * s, o[1] + lx * s + ly * c
                base = o[2]
                # Blender world for the BVH (Werk FBX in Blender frame): (ux, -uy)
                loc, nrm, idx, dist = tree.ray_cast(Vector((ux, -uy, base + z1 + 2000)), Vector((0, 0, -1)), 5000)
                if loc is not None and loc.z > base + 30 and loc.z > base + z0 + 30:
                    hits.append((round(ux), round(uy), round(loc.z - base), hull))
                g = ground(ux, uy) - base
                if abs(g) > 40:
                    gaps.append((round(ux), round(uy), round(g)))
            rows[tname] = dict(origin=[round(v) for v in o], yaw=yaw, werk_hits=hits[:40], n_hits=len(hits),
                               ground_off=gaps[:40], n_ground_off=len(gaps))
            print('CLEAR', key, tname, 'werk hits', len(hits), 'ground off >40cm', len(gaps))
        report[key] = rows
    with open(OUT, 'w') as f:
        json.dump(report, f, indent=1)
    print('OW_TARGETS_CLEARANCE_DONE')


main()
