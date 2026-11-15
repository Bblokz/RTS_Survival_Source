"""Ural Shield - preview renders (Cycles, sun + sky) from the work .blend written by build_kit.py.

Run:  blender.exe --factory-startup -b -P render_previews.py
      blender.exe --factory-startup -b -P render_previews.py -- backface      (flipped-face check: red = back face)
      blender.exe --factory-startup -b -P render_previews.py -- only=preview_segment
"""
import os
import sys
import math
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import us_common as C

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
BACKFACE = 'backface' in ARGS
ONLY = [a.split('=', 1)[1] for a in ARGS if a.startswith('only=')]
M = C.CM


def mat_flat(name, rgb, rough=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Roughness'].default_value = rough
    return m


def mat_backface():
    m = bpy.data.materials.new('M_BackfaceDebug')
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    geo = nt.nodes.new('ShaderNodeNewGeometry')
    mix = nt.nodes.new('ShaderNodeMixShader')
    d = nt.nodes.new('ShaderNodeBsdfDiffuse')
    d.inputs['Color'].default_value = (0.6, 0.6, 0.6, 1)
    e = nt.nodes.new('ShaderNodeEmission')
    e.inputs['Color'].default_value = (1, 0, 0, 1)
    e.inputs['Strength'].default_value = 3
    nt.links.new(geo.outputs['Backfacing'], mix.inputs[0])
    nt.links.new(d.outputs[0], mix.inputs[1])
    nt.links.new(e.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    return m


def look_at(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def main():
    bpy.ops.wm.open_mainfile(filepath=C.BLEND)
    sc = bpy.context.scene
    src = {o.name: o for o in sc.objects if o.type == 'MESH'}
    for o in list(sc.objects):
        o.hide_render = True
    if BACKFACE:
        dbg = mat_backface()
        for o in src.values():
            o.data.materials.clear()
            o.data.materials.append(dbg)

    sc.render.engine = 'CYCLES'
    sc.cycles.samples = 16 if BACKFACE else 96
    sc.cycles.use_denoising = not BACKFACE
    if BACKFACE:
        sc.cycles.max_bounces = 0        # no indirect light: red can only come from a back face itself
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for t in ('OPTIX', 'CUDA'):
            try:
                prefs.compute_device_type = t
                prefs.get_devices()
                if any(d.type == t for d in prefs.devices):
                    for d in prefs.devices:
                        d.use = d.type == t
                    sc.cycles.device = 'GPU'
                    break
            except Exception:
                pass
    except Exception as ex:
        print('GPU setup skipped', ex)
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.view_settings.view_transform = 'Filmic'
    sc.view_settings.look = 'Medium High Contrast'
    sc.view_settings.exposure = -0.3

    sun_dir = Vector((0.70, 0.52, -0.50)).normalized()       # direction the light travels
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    nt = world.node_tree
    sky = nt.nodes.new('ShaderNodeTexSky')
    sky.sky_type = 'HOSEK_WILKIE'
    sky.turbidity = 3.0
    sky.sun_direction = -sun_dir
    bg = nt.nodes['Background']
    bg.inputs['Strength'].default_value = 0.9
    nt.links.new(sky.outputs[0], bg.inputs['Color'])
    sc.world = world
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 4.0
    sun.data.angle = math.radians(1.0)
    sun.rotation_euler = sun_dir.to_track_quat('-Z', 'Y').to_euler()
    sc.collection.objects.link(sun)

    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.active_object
    ground.scale = (4.0e6, 4.0e6, 1)
    ground.location = (0, 0, 0.5)      # 5 mm above the open undersides so no ray slips beneath a piece
    ground.data.materials.append(mat_flat('M_Ground', (0.23, 0.21, 0.18)))

    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    cam.data.clip_start = 50
    cam.data.clip_end = 3.0e6
    sc.collection.objects.link(cam)
    sc.camera = cam
    tank_mat = mat_flat('M_Tank', (0.35, 0.36, 0.35), 0.7)

    def inst(name, loc=(0, 0, 0), rotz=0.0, at_own=False):
        o = bpy.data.objects.new('i_' + name, src[name].data)
        base = Vector(src[name].location) if at_own else Vector((0, 0, 0))
        o.location = base + Vector(loc) * M
        o.rotation_euler = (0, 0, math.radians(rotz))
        sc.collection.objects.link(o)
        return o

    def tank(x, y, rotz=0.0):
        bpy.ops.mesh.primitive_cube_add(size=1.0)
        t = bpy.context.active_object
        t.scale = (6.5 * M, 3.0 * M, 2.5 * M)
        t.location = (x * M, y * M, 1.25 * M)
        t.rotation_euler = (0, 0, math.radians(rotz))
        t.data.materials.append(tank_mat)
        return t

    def shot(name, builder, cam_pos, target, lens):
        if ONLY and name not in ONLY:
            return
        objs = builder()
        cam.location = Vector(cam_pos) * M
        cam.data.lens = lens
        look_at(cam, Vector(target) * M)
        sc.render.filepath = os.path.join(C.WORK if BACKFACE else C.OUT, name + ('_backface' if BACKFACE else '') + '.png')
        bpy.ops.render.render(write_still=True)
        print('RENDERED', sc.render.filepath)
        if BACKFACE:
            import numpy as np
            im = bpy.data.images.load(sc.render.filepath)
            px = np.empty(im.size[0] * im.size[1] * 4, np.float32)
            im.pixels.foreach_get(px)
            px = px.reshape(-1, 4)
            red = int(((px[:, 0] > 0.6) & (px[:, 1] < 0.25) & (px[:, 2] < 0.25)).sum())
            print('BACKFACE_PIXELS %-28s %d' % (name, red))
            bpy.data.images.remove(im)
        for o in objs:
            bpy.data.objects.remove(o, do_unlink=True)

    def seg_scene():
        return [inst('SM_UralShield_Segment', (0, y, 0)) for y in (-120, -60, 0, 60, 120)] + [tank(-13, -6, 90)]

    def gate_scene(angle):
        def b():
            o = [inst('SM_UralShield_GateFrame')]
            o += [inst('SM_UralShield_Segment', (0, y, 0)) for y in (-120, -60, 60, 120)]
            o.append(inst('SM_UralShield_GateLeaf_L', rotz=-angle, at_own=True))
            o.append(inst('SM_UralShield_GateLeaf_R', rotz=angle, at_own=True))
            o.append(tank(-34, 0, 0))
            return o
        return b

    def far_scene():
        return [inst('SM_UralShield_Far', (0, 60 * i, 0)) for i in range(40)]

    shot('preview_segment', seg_scene, (-78, -62, 16), (-2, 4, 23), 30)
    shot('preview_gate_closed', gate_scene(0), (-112, -52, 9), (-2, 0, 27), 32)
    shot('preview_gate_open', gate_scene(85), (-112, -52, 9), (-2, 0, 27), 32)
    shot('preview_far_line', far_scene, (-55, -40, 5), (0, 700, 40), 28)
    # extra self-check views (written to work/, not deliverables)
    if not ONLY or any(n.startswith('check') for n in ONLY):
        out_bak = C.OUT
        C.OUT = C.WORK
        shot('check_east', gate_scene(0), (120, 70, 30), (0, 0, 26), 30)
        shot('check_top', gate_scene(40), (-70, -40, 95), (0, 0, 30), 30)
        shot('check_gate_close', gate_scene(25), (-45, -14, 5), (-5, 2, 9), 35)
        shot('check_seg_close', seg_scene, (-40, 3, 30), (-4, 24, 33), 35)
        shot('check_gatehouse', gate_scene(60), (-38, -6, 16), (-5, 3, 24), 30)
        shot('check_towertop', gate_scene(0), (-45, -30, 70), (-2, 6, 50), 40)
        shot('check_junction', gate_scene(0), (-70, -45, 24), (-5, 19, 31), 130)
        shot('check_junction_low', gate_scene(0), (-112, -52, 9), (-5, -18.6, 30), 400)
        shot('check_seam', seg_scene, (-45, 30, 20), (-4, 30, 26), 24)
        shot('check_approach', gate_scene(85), (-80, 1.5, 3), (0, 0, 8), 45)       # shot (a) axis, gates open
        shot('check_inside', gate_scene(85), (30, -3, 4), (-5, 0, 7), 28)          # from inside, leaf backs
        C.OUT = out_bak


main()
