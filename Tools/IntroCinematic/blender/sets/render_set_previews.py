"""Intro cinematic sets - preview renders (Cycles) from the work .blend files.

Run:  blender.exe --factory-startup -b -P render_set_previews.py                  (all previews)
      blender.exe --factory-startup -b -P render_set_previews.py -- only=preview_trench_inside
      blender.exe --factory-startup -b -P render_set_previews.py -- backface      (red = back face, written to work/)
      blender.exe --factory-startup -b -P render_set_previews.py -- checks        (extra views into work/)
"""
import os
import sys
import math
import numpy as np
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sets_common as C

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
BACKFACE = 'backface' in ARGS
CHECKS = 'checks' in ARGS
ONLY = [a.split('=', 1)[1] for a in ARGS if a.startswith('only=')]
M = C.CM


def mat_backface():
    m = bpy.data.materials.new('M_BackfaceDebug')
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    geo = nt.nodes.new('ShaderNodeNewGeometry')
    mix = nt.nodes.new('ShaderNodeMixShader')
    # both sides are pure emission, so nothing receives light: a red pixel is a back face seen by the
    # camera itself (a diffuse front would also pick up red light from the open undersides of the slats)
    d = nt.nodes.new('ShaderNodeEmission')
    d.inputs['Color'].default_value = (0.35, 0.4, 0.45, 1)
    lw = nt.nodes.new('ShaderNodeLayerWeight')
    mulc = nt.nodes.new('ShaderNodeMath')
    mulc.operation = 'MULTIPLY_ADD'
    mulc.inputs[1].default_value = -0.8
    mulc.inputs[2].default_value = 1.0
    nt.links.new(lw.outputs['Facing'], mulc.inputs[0])
    nt.links.new(mulc.outputs[0], d.inputs['Strength'])
    e = nt.nodes.new('ShaderNodeEmission')
    e.inputs['Color'].default_value = (1, 0, 0, 1)
    e.inputs['Strength'].default_value = 1
    nt.links.new(geo.outputs['Backfacing'], mix.inputs[0])
    nt.links.new(d.outputs[0], mix.inputs[1])
    nt.links.new(e.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    return m


def setup(blend, sun_dir, sun_energy, sun_color, sky_strength, exposure, night=False):
    bpy.ops.wm.open_mainfile(filepath=os.path.join(C.WORK, blend))
    sc = bpy.context.scene
    if BACKFACE:
        dbg = mat_backface()
        for o in sc.objects:
            if o.type == 'MESH':
                for i in range(len(o.data.materials)):
                    o.data.materials[i] = dbg
    sc.render.engine = 'CYCLES'
    sc.cycles.samples = 16 if BACKFACE else 128
    sc.cycles.use_denoising = not BACKFACE
    if BACKFACE:
        sc.cycles.max_bounces = 0
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
    sc.view_settings.exposure = exposure
    if BACKFACE:                       # no tone mapping: the red counter needs the raw emission colour
        sc.view_settings.view_transform = 'Standard'
        sc.view_settings.look = 'None'
        sc.view_settings.exposure = 0.0
        if 'control' in ARGS:          # positive control: one deliberately flipped mesh must light up red
            bpy.data.objects['SM_Intro_Mould_Mat_B' if 'Trench' in blend else 'SM_Intro_CraterTerrain'].data.flip_normals()
    sd = Vector(sun_dir).normalized()
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes['Background']
    if night:
        bg.inputs['Color'].default_value = (0.012, 0.016, 0.03, 1)
        bg.inputs['Strength'].default_value = sky_strength
    else:
        sky = nt.nodes.new('ShaderNodeTexSky')
        sky.sky_type = 'HOSEK_WILKIE'
        sky.turbidity = 4.0
        sky.sun_direction = -sd
        bg.inputs['Strength'].default_value = sky_strength
        nt.links.new(sky.outputs[0], bg.inputs['Color'])
    sc.world = world
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = sun_energy
    sun.data.color = sun_color
    sun.data.angle = math.radians(1.5)
    sun.rotation_euler = sd.to_track_quat('-Z', 'Y').to_euler()
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    cam.data.clip_start = 3
    cam.data.clip_end = 1.0e6
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def shot(sc, cam, name, pos, target, lens, hide=(), deliver=True):
    if ONLY and name not in ONLY:
        return
    if not deliver and not (CHECKS or ONLY):
        return
    for o in sc.objects:
        if o.type == 'MESH':
            o.hide_render = any(o.name.startswith(h) for h in hide)
    cam.location = Vector(pos) * M
    cam.data.lens = lens
    cam.rotation_euler = (Vector(target) * M - cam.location).to_track_quat('-Z', 'Y').to_euler()
    folder = C.OUT if (deliver and not BACKFACE) else C.WORK
    sc.render.filepath = os.path.join(folder, name + ('_backface' if BACKFACE else '') + '.png')
    bpy.ops.render.render(write_still=True)
    print('RENDERED', sc.render.filepath)
    if BACKFACE:
        im = bpy.data.images.load(sc.render.filepath)
        px = np.empty(im.size[0] * im.size[1] * 4, np.float32)
        im.pixels.foreach_get(px)
        px = px.reshape(-1, 4)
        red = int(((px[:, 0] > 0.6) & (px[:, 1] < 0.25) & (px[:, 2] < 0.25)).sum())
        print('BACKFACE_PIXELS %-28s %d' % (name, red))


def want(prefix):
    return not ONLY or any(o.startswith(prefix) or o.startswith('check_' + prefix.split('_')[1]) for o in ONLY)


if want('preview_trench') and os.path.exists(os.path.join(C.WORK, 'IntroTrench.blend')):
    # low warm dawn sun travelling up the trench (+Y) and slightly east, 17 degrees above the horizon
    sc, cam = setup('IntroTrench.blend', (0.26, 0.90, -0.30), 5.0, (1.0, 0.74, 0.5), 0.55, 0.9)
    MOULD = ('SM_Intro_Mould',)
    shot(sc, cam, 'preview_trench_inside', (-0.05, -8.6, 1.5), (0.1, 4.0, 1.05), 20, hide=MOULD)
    shot(sc, cam, 'preview_trench_above', (-15.0, -21.0, 15.0), (0.0, 0.5, 0.5), 30, hide=MOULD)
    shot(sc, cam, 'preview_trench_mould', (0.05, -2.3, 1.5), (0.05, 5.5, 0.55), 19)
    shot(sc, cam, 'check_trench_back', (0.0, 8.8, 1.5), (0.0, -4.0, 1.0), 20, deliver=False)
    shot(sc, cam, 'check_trench_props', (0.25, 0.3, 1.45), (-0.3, 2.8, 0.3), 28, hide=MOULD, deliver=False)
    shot(sc, cam, 'check_trench_mould_far', (0.2, 10.6, 1.5), (0.2, 4.0, 0.4), 22, deliver=False)
    shot(sc, cam, 'check_trench_top', (0.0, 0.0, 34.0), (0.0, 0.5, 0.0), 30, deliver=False)
    shot(sc, cam, 'check_trench_wall', (-0.3, 3.2, 1.3), (1.6, 4.2, 1.0), 24, deliver=False)
    shot(sc, cam, 'check_trench_helmets', (0.0, 0.2, 1.3), (1.1, 1.2, 0.5), 30, hide=MOULD, deliver=False)

if want('preview_crater') and os.path.exists(os.path.join(C.WORK, 'IntroCrater.blend')):
    # night: faint blue moon from the south-west, cracks emissive through the mask
    sc, cam = setup('IntroCrater.blend', (0.45, 0.5, -0.5), 0.3, (0.6, 0.72, 1.0), 0.6, 2.2, night=True)
    shot(sc, cam, 'preview_crater_low', (20.0, 0.0, 1.6), (-28.0, 0.0, 7.0), 20)
    shot(sc, cam, 'preview_crater_above', (70.0, -85.0, 80.0), (0.0, 0.0, 2.0), 32)
    sc.view_settings.exposure = 0.0 if BACKFACE else 5.0
    shot(sc, cam, 'check_crater_day', (62.0, -70.0, 38.0), (0.0, 0.0, 4.0), 30, deliver=False)
    shot(sc, cam, 'check_crater_floor', (9.0, -6.0, 6.0), (0.0, 0.0, 0.0), 24, deliver=False)
    shot(sc, cam, 'check_crater_outside', (78.0, 0.0, 12.0), (0.0, 0.0, 9.0), 24, deliver=False)
print('RENDER DONE')
