"""Fit check: the built targets placed on the real Werk meshes (Blender 5.x, background, Cycles CPU).

    Blender5.0.exe -b --factory-startup -P ow_targets_fit.py -- [7c 8b 10] [--destroyed]
Imports SM_OW_Werk_<Name>.FBX, its landscape stamp (heightfield) and the kit placements (mast, tunnel portal), then the
target FBXs at their spots: the porch on the Entrance socket (yaw = the Werkeingang's), the jammer on the mast, the rail
gate on the portal. Renders one close view per target into Targets/Renders/Fit_<key>_<target>.png.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Euler, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ow_targets_parts as P  # noqa: E402
from ow_targets_parts import L  # noqa: E402

WERKE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Werke/Model/'
ROOT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/'
NAMES = {'7c': 'Kasemattennase', '8b': 'Flankierend', '10': 'Peilturm'}
ENTRANCE_YAW = {'7c': 0.0, '8b': 180.0, '10': 180.0}      # Unreal yaw of the Werkeingang (werk_parts eingang module)


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in [o for o in new if o.name.startswith('UCX_')]:
        bpy.data.objects.remove(o)
    return [o for o in bpy.data.objects if o not in before]


def place(objs, ue_xyz, ue_yaw):
    """Unreal (x, y, z cm, yaw) -> Blender (x, -y, z, -yaw)."""
    for o in objs:
        if o.parent is None:
            o.location = (ue_xyz[0], -ue_xyz[1], ue_xyz[2])
            o.rotation_euler = Euler((0, 0, math.radians(-ue_yaw)))


def werk(key):
    name = 'SM_OW_Werk_' + NAMES[key]
    objs = import_fbx(WERKE + name + '/' + name + '.FBX')
    mesh = [o for o in objs if o.type == 'MESH'][0]
    bpy.context.view_layer.update()
    span = max(mesh.dimensions)
    if span < 1000:          # imported in metres: bring it to the scene's centimetres
        for o in objs:
            if o.parent is None:
                o.scale = (100, 100, 100)
    bpy.context.view_layer.update()
    for o in objs:
        if o.type == 'MESH':
            for i, m in enumerate(o.data.materials):
                o.data.materials[i] = L.get_material('MI_BF_SootedFortress_UVTile')
    socks = {o.name.replace('SOCKET_', '').split('.')[0]: o.matrix_world.translation.copy() for o in objs
             if o.name.startswith('SOCKET_')}
    return name, socks


def stamp(name):
    H = np.load(WERKE + name + '/' + name + '_Stamp.npz')['H'] * 100.0
    n = H.shape[0]
    xs = np.linspace(-9000, 9000, n)
    step = 2
    idx = range(0, n, step)
    verts, faces = [], []
    cols = len(list(idx))
    for j in idx:
        for i in idx:
            verts.append((xs[i], -xs[j], H[j, i] - 8))   # H[j, i]: i along Werk X, j along Werk (Unreal) Y
    for r in range(cols - 1):
        for c in range(cols - 1):
            a = r * cols + c
            faces.append((a, a + 1, a + cols + 1, a + cols))
    me = bpy.data.meshes.new('stamp')
    me.from_pydata(verts, [], faces)
    me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
    me.materials.append(L.flat_material('Fit_Grass', (0.20, 0.24, 0.12), 1.0))
    o = bpy.data.objects.new('stamp', me)
    bpy.context.scene.collection.objects.link(o)


def kit(name):
    pl = json.load(open(WERKE + name + '/' + name + '_Placements.json'))
    spots = {}
    for k in pl['kit']:
        if k['kind'] in ('mast', 'portal'):
            objs = import_fbx(ROOT + '../FBX/' + k['mesh'] + '.fbx')
            place(objs, (k['x'], k['y'], k['z']), k['yaw'])
            spots[k['kind']] = (k['x'], k['y'], k['z'], k['yaw'])
    return spots


def target(fbx, ue_xyz, ue_yaw):
    objs = import_fbx(ROOT + 'FBX/' + fbx + '.fbx')
    place(objs, ue_xyz, ue_yaw)


def shot(key, tag, ue_target, distance, az, el=48.0):
    import ow_targets_build as B
    B.preview_materials()
    for o in bpy.context.scene.objects:
        if o.type == 'MESH':
            P.bind_kit_materials(o)
    t = (ue_target[0], -ue_target[1], ue_target[2])
    L.render_preview(ROOT + 'Renders/Fit_%s_%s.png' % (key, tag), target=t, distance=distance, azimuth_deg=az,
                     elevation_deg=el, resolution=(1000, 700), samples=16, ground_z=-500.0)


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    keys = [a for a in args if a in NAMES] or list(NAMES)
    sfx = '_Destroyed' if '--destroyed' in args else ''
    for key in keys:
        L.reset_scene()
        name, socks = werk(key)
        stamp(name)
        spots = kit(name)
        e = socks['Entrance']
        ent = (e.x, -e.y, e.z)
        target('SM_OW_Target_CommandPorch' + sfx, ent, ENTRANCE_YAW[key])
        m = spots['mast']
        target('SM_OW_Target_Jammer' + sfx, m[:3], m[3])
        p = spots['portal']
        target('SM_OW_Target_RailGate' + sfx, p[:3], p[3])
        yaw = ENTRANCE_YAW[key]
        shot(key, 'Porch' + sfx, (ent[0] + 300 * math.cos(math.radians(yaw)), ent[1], ent[2] + 100), 2600,
             -yaw + 40, el=50)
        shot(key, 'Jammer' + sfx, (m[0], m[1], m[2] + 400), 3600, -60, el=45)
        shot(key, 'RailGate' + sfx, (p[0] + 300, p[1], p[2] + 200), 3000, 25, el=35)
    print('OW_TARGETS_FIT_DONE')


if __name__ == '__main__':
    main()
