"""Build the destroyed (wreck) meshes from a <Tank>_Split.blend made by build_td.py.

Usage: blender -b --factory-startup --python build_wreck.py -- <config.json>

The wreck replaces the whole vehicle, so the hull wreck merges the hull, the turret base and the donor tank's
running gear (Unreal's FBX export of the track skeletal mesh, frozen in its rest pose). The gun is a separate mesh
with its origin on the gun pivot, posed drooped and turned a little.
Material slots: 'Destroyed' (the tank's burnt material) and 'RunningGear' (the generic destroyed material).
"""
import bpy, sys, json, math, os
from mathutils import Matrix, Vector

cfg = json.load(open(sys.argv[sys.argv.index('--') + 1]))
NAME = cfg['name']
OUT = cfg['out_dir']
bpy.ops.wm.open_mainfile(filepath=cfg['split_blend'])

hull = bpy.data.objects['SM_%s_Hull' % NAME]
base = bpy.data.objects['SM_%s_TurretBase' % NAME]
weapon = bpy.data.objects['SK_%s_WeaponSystem' % NAME]
arm = bpy.data.objects['Armature']

destroyed = bpy.data.materials.new('Destroyed')
running_gear = bpy.data.materials.new('RunningGear')


def set_single_material(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    for poly in obj.data.polygons:
        poly.material_index = 0


# ---------------------------------------------------------------- running gear
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=cfg['tracks_fbx'])
imported = [o for o in bpy.data.objects if o not in before]
bpy.context.view_layer.update()
gear_parts = []
for o in imported:
    if o.type != 'MESH':
        continue
    # Freeze the rest pose and bring Unreal's centimetre export into this file's centimetre space.
    mw = o.matrix_world.copy()
    o.modifiers.clear()
    o.data = o.data.copy()
    o.data.transform(Matrix.Diagonal(Vector((100, 100, 100, 1))) @ mw)
    o.parent = None
    o.matrix_world = Matrix.Identity(4)
    o.vertex_groups.clear()
    set_single_material(o, running_gear)
    gear_parts.append(o)
for o in imported:
    if o not in gear_parts:
        bpy.data.objects.remove(o)
bpy.context.view_layer.update()

# ---------------------------------------------------------------- hull wreck
hull_wreck = hull.copy()
hull_wreck.data = hull.data.copy()
base_wreck = base.copy()
base_wreck.data = base.data.copy()
for o in (hull_wreck, base_wreck):
    bpy.context.scene.collection.objects.link(o)
    set_single_material(o, destroyed)
# A burnt-out wreck settles a little on its suspension.
sink = Matrix.Translation(Vector((0, 0, -cfg.get('sink_cm', 4))))
for o in (hull_wreck, base_wreck):
    o.data.transform(sink)

for o in bpy.context.selected_objects:
    o.select_set(False)
parts = [hull_wreck, base_wreck] + gear_parts
for o in parts:
    o.select_set(True)
bpy.context.view_layer.objects.active = hull_wreck
bpy.ops.object.join()
hull_wreck.name = hull_wreck.data.name = 'SM_%s_Hull_Destroyed' % NAME
# join() merges the material lists (Destroyed first, from the active hull) and remaps the face indices.
print('HULL WRECK', len(hull_wreck.data.polygons), [m.name for m in hull_wreck.data.materials])

# ---------------------------------------------------------------- gun wreck
gun = weapon.copy()
gun.data = weapon.data.copy()
bpy.context.scene.collection.objects.link(gun)
gun.modifiers.clear()
gun.parent = None
gun.matrix_world = Matrix.Identity(4)
gun.vertex_groups.clear()
set_single_material(gun, destroyed)
# Positive rotation about +Y takes +X downwards (barrel droops); positive Z rotation turns it to Blender +Y.
droop = Matrix.Rotation(math.radians(cfg.get('gun_droop_deg', 7)), 4, 'Y')
turn = Matrix.Rotation(math.radians(cfg.get('gun_turn_deg', 4)), 4, 'Z')
gun.data.transform(turn @ droop @ Matrix.Translation(Vector((0, 0, -cfg.get('sink_cm', 4)))))
gun.name = gun.data.name = 'SM_%s_Gun_Destroyed' % NAME


def export(obj, path):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE',
                             global_scale=0.01, axis_forward='-Z', axis_up='Y', mesh_smooth_type='FACE',
                             bake_anim=False, path_mode='STRIP', object_types={'MESH'})
    print('EXPORT', path)


export(hull_wreck, os.path.join(OUT, 'SM_%s_Hull_Destroyed.fbx' % NAME))
export(gun, os.path.join(OUT, 'SM_%s_Gun_Destroyed.fbx' % NAME))
print('DONE')
