"""Render the concept options of the Oder-Warthe Werk targets (Blender 5.x, background, Cycles CPU).

    Blender5.0.exe -b --factory-startup -P ow_targets_concepts.py -- [group ...] [--only KEY ...]
groups: tunnel command jammer rail (default all). Writes <OUT>/renders/<KEY>_<intact|destroyed>.png; the sheets are
composed by ow_targets_sheets.py (system Python + PIL).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ow_targets_parts as P  # noqa: E402
import ow_targets_pieces as T  # noqa: E402
from ow_targets_parts import L  # noqa: E402

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/Concepts/renders/'
RES = (620, 420)
SAMPLES = 14

CAM = {
    'tunnel': dict(target=(250, 0, 200), distance=3900, elevation_deg=46, azimuth_deg=28),
    'command': dict(target=(420, 0, 80), distance=2500, elevation_deg=46, azimuth_deg=32),
    'jammer': dict(target=(0, -380, 420), distance=3300, elevation_deg=40, azimuth_deg=-62),
    'rail': dict(target=(520, 420, 200), distance=4200, elevation_deg=46, azimuth_deg=28),
}


def render(key, state, group):
    L.render_preview(OUT + '%s_%s.png' % (key, state), resolution=RES, samples=SAMPLES, ground_z=-2.0,
                     ground_rgb=(0.17, 0.17, 0.10), **CAM[group])


def portal(deform=None):
    objs = P.import_kit('SM_OW_TunnelPortal')
    for o in objs:
        P.bind_kit_materials(o)
        if deform is None:
            continue
        P.densify(o, 90.0)
        for v in o.data.vertices:
            v.co = v.co + deform(v.co)
        o.data.update()
    return objs


def mast():
    for o in P.import_kit('SM_OW_WerkMast'):
        P.bind_kit_materials(o)


def do_tunnel(keys):
    for key in keys:
        L.reset_scene()
        portal()
        render(key, 'intact', 'tunnel')
        L.reset_scene()
        portal(T.tm_deform(key))
        T.tm_rubble(key).to_object()
        render(key, 'destroyed', 'tunnel')
        if key == 'TM_G':
            L.render_preview(OUT + 'TM_G_destroyed_back.png', resolution=RES, samples=SAMPLES, ground_z=-2.0,
                             ground_rgb=(0.17, 0.17, 0.10), target=(-200, 0, 300), distance=4300,
                             elevation_deg=52, azimuth_deg=150)


def do_simple(group, table, keys, context):
    for key in keys:
        for d in (False, True):
            L.reset_scene()
            context()
            table[key](d).to_object()
            render(key, 'destroyed' if d else 'intact', group)


def wall():
    b = P.Builder('Wall', T.MATS)
    P.werkeingang_wall(b)
    b.to_object()


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    only = args[args.index('--only') + 1:] if '--only' in args else []
    groups = [a for a in args if not a.startswith('--') and a not in only] or ['tunnel', 'command', 'jammer', 'rail']
    os.makedirs(OUT, exist_ok=True)

    def pick(keys):
        return [k for k in keys if not only or k in only]
    if 'tunnel' in groups:
        do_tunnel(pick(T.TM_INFO))
    if 'command' in groups:
        do_simple('command', T.CE, pick(T.CE), wall)
    if 'jammer' in groups:
        do_simple('jammer', T.JT, pick(T.JT), mast)
    if 'rail' in groups:
        do_simple('rail', T.RE, pick(T.RE), portal)
    print('OW_TARGET_CONCEPTS_DONE')


main()
