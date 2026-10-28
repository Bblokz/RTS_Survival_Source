"""Preview renders of the ISU-130: the ISU-152 hull and gun mount with the S-26 weapon system at the gun pivot.

Usage: Blender5.0.exe -b --factory-startup --python render_isu130.py -- <out_dir> [pitch_up_deg]
"""
import bpy, math, sys, os
from mathutils import Vector, Matrix

OUT = sys.argv[sys.argv.index('--') + 1]
PITCH = float(sys.argv[sys.argv.index('--') + 2]) if len(sys.argv) > sys.argv.index('--') + 2 else 0.0
SPLIT = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_152/ISU_152_Split.blend'
GUN = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_130/SK_ISU130_WeaponSystem.blend'
TEX = 'D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/ISU152/T_ISU152_Camo_D.png'
NRM = 'D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/ISU152/T_ISU152_N.jpg'
# Unreal's pivot (293.58, 35.2, 192.1) in Blender axes (Y flipped).
PIVOT = Vector((293.58, -35.2, 192.1))
os.makedirs(OUT, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=SPLIT)
bpy.data.batch_remove([o for o in bpy.data.objects if o.name not in ('SM_ISU152_Hull', 'SM_ISU152_TurretBase')])
bpy.data.batch_remove([a for a in bpy.data.armatures])
with bpy.data.libraries.load(GUN) as (src, dst):
    dst.objects = [n for n in src.objects]
for o in dst.objects:
    bpy.context.scene.collection.objects.link(o)
arm = bpy.data.objects['Armature']
gun = bpy.data.objects['SK_ISU130_WeaponSystem']
arm.location = PIVOT
bpy.context.view_layer.update()
pb = arm.pose.bones['B_Mantlet']
pb.rotation_mode = 'XYZ'
# B_Mantlet points along +X with roll 0: its local frame is the armature frame turned so local Y = +X; pitching the
# bore up is a rotation about the bone's local Z (armature -Y axis) -> use matrix in armature space.
rest = arm.data.bones['B_Mantlet'].matrix_local
pb.matrix = Matrix.Rotation(math.radians(-PITCH), 4, 'Y') @ rest
bpy.context.view_layer.update()

s = bpy.context.scene
img = bpy.data.images.load(TEX)
nimg = bpy.data.images.load(NRM)
nimg.colorspace_settings.name = 'Non-Color'
for mat in {m for o in (gun, bpy.data.objects['SM_ISU152_Hull'], bpy.data.objects['SM_ISU152_TurretBase']) for m in o.data.materials}:
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    nt.links.new(bs.outputs[0], out.inputs[0])
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = img
    nt.links.new(t.outputs[0], bs.inputs['Base Color'])
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = nimg
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(tn.outputs[0], nm.inputs['Color']); nt.links.new(nm.outputs[0], bs.inputs['Normal'])
    bs.inputs['Roughness'].default_value = 0.6
    bs.inputs['Metallic'].default_value = 0.3
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
s.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in engines else 'BLENDER_EEVEE_NEXT'
s.render.resolution_x, s.render.resolution_y = 1600, 900
w = bpy.data.worlds.new('w'); s.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.42, 0.45, 0.5, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); s.collection.objects.link(sun)
sun.rotation_euler = (math.radians(45), 0, math.radians(-35)); sun.data.energy = 3.5
ground = bpy.data.meshes.new('g'); ground.from_pydata([(-3000, -3000, 0), (3000, -3000, 0), (3000, 3000, 0), (-3000, 3000, 0)], [], [(0, 1, 2, 3)])
g = bpy.data.objects.new('ground', ground); s.collection.objects.link(g)
gm = bpy.data.materials.new('gmat'); gm.use_nodes = True; gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.23, 0.2, 1)
ground.materials.append(gm)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
cam.data.clip_end = 100000


def shot(name, loc, tgt, lens=35, ortho=None):
    cam.location = loc
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam.data.type = 'ORTHO'; cam.data.ortho_scale = ortho
    else:
        cam.data.type = 'PERSP'; cam.data.lens = lens
    s.render.filepath = os.path.join(OUT, name + '.png')
    bpy.ops.render.render(write_still=True)


tag = '_up%d' % PITCH if PITCH else ''
shot('ISU130_oblique' + tag, (1250, -1150, 650), (250, 0, 150), 32)
shot('ISU130_side' + tag, (250, -3000, 180), (250, 0, 180), ortho=1400)
shot('ISU130_front3q' + tag, (-500, 900, 500), (300, 0, 150), 30)
shot('ISU130_brake_close' + tag, (980, -260, 260), (860, -35, 195), 50)
shot('ISU130_mount_close' + tag, (560, -330, 300), (360, -35, 192), 40)
print('DONE')
