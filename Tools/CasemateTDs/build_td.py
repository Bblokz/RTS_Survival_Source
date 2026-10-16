"""Split a KADO casemate tank destroyer into hull SM, collision SM, turret base SM and a rigged weapon SK.

Usage: blender -b --factory-startup --python build_td.py -- <config.json>

The weapon rig copies the SU-122 bone names (B_Turret yaw, B_Mantlet pitch, B_Gun recoil) so the
embedded turret anim BP setup can be reused. Every animation is exported as its own FBX.
"""
import bpy, bmesh, sys, json, math, os
from mathutils import Vector, Matrix

cfg = json.load(open(sys.argv[sys.argv.index('--') + 1]))
NAME = cfg['name']
OUT = cfg['out_dir']
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- load
src = cfg['source']
if src.endswith('.blend'):
    bpy.ops.wm.open_mainfile(filepath=src)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
for o in list(bpy.data.objects):
    if o.type != 'MESH':
        bpy.data.objects.remove(o)
bpy.context.view_layer.update()

# Parts are built in centimetres; the FBX export below scales them back to metres (FBX_SCALE_NONE then gives centimetres in Unreal).
U = 100.0
fit = Matrix.Diagonal(Vector((U, U, U, 1))) @ Matrix.Translation(Vector(cfg['fit_offset'])) @ Matrix.Diagonal(Vector(cfg['fit_scale']).to_4d())

# Bake world transforms plus the track fit into the mesh data so every part sits in track-mesh space.
meshes = {}
for o in list(bpy.data.objects):
    mw = o.matrix_world.copy()
    o.data = o.data.copy()
    o.data.transform(fit @ mw)
    if (fit @ mw).determinant() < 0:
        o.data.flip_normals()
    meshes[o.name] = o
for o in meshes.values():
    o.parent = None
    o.matrix_world = Matrix.Identity(4)
bpy.context.view_layer.update()

mat = bpy.data.materials.new(cfg['material_slot'])
for o in meshes.values():
    o.data.materials.clear()
    o.data.materials.append(mat)


def fitp(p):
    return (fit @ Vector(p).to_4d()).to_3d()


def split_islands(obj, box_raw, name):
    """Move the islands fully inside box_raw (source coordinates) of obj into a new object."""
    lo, hi = fitp(box_raw[0]), fitp(box_raw[1])
    lo, hi = Vector([min(lo[i], hi[i]) for i in range(3)]), Vector([max(lo[i], hi[i]) for i in range(3)])
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    seen = set()
    take = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        vs = [v.co for g in comp for v in g.verts]
        if all(min(v[i] for v in vs) >= lo[i] and max(v[i] for v in vs) <= hi[i] for i in range(3)):
            take.extend(comp)
    take_idx = {f.index for f in take}
    bm2 = bm.copy()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index in take_idx], context='FACES')
    bm.to_mesh(obj.data)
    bm2.faces.ensure_lookup_table()
    bmesh.ops.delete(bm2, geom=[f for f in bm2.faces if f.index not in take_idx], context='FACES')
    me = bpy.data.meshes.new(name)
    bm2.to_mesh(me)
    new = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(new)
    print('SPLIT', name, len(take_idx), 'faces')
    return new


def join(objs, name):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    objs[0].name = name
    objs[0].data.name = name
    return objs[0]


# ---------------------------------------------------------------- parts
hull = meshes[cfg['hull_object']]
base_parts = [meshes[n] for n in cfg.get('turret_base_objects', [])]
if 'turret_base_box' in cfg:
    base_parts.append(split_islands(hull, cfg['turret_base_box'], 'TurretBaseIslands'))
turret_base = join(base_parts, 'SM_%s_TurretBase' % NAME)
hull.name = hull.data.name = 'SM_%s_Hull' % NAME

mantlet = join([meshes[n] for n in cfg['mantlet_objects']], 'Mantlet')
barrel = join([meshes[n] for n in cfg['barrel_objects']], 'Barrel')


# ---------------------------------------------------------------- collision
def convex_part(objs, zmin, zmax, name):
    pts = []
    for o in objs:
        for v in o.data.vertices:
            if zmin <= v.co.z <= zmax:
                pts.append(v.co.copy())
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    res = bmesh.ops.convex_hull(bm, input=bm.verts)
    drop = {g for g in res['geom_unused'] + res['geom_interior'] if isinstance(g, bmesh.types.BMVert)}
    bmesh.ops.delete(bm, geom=list(drop), context='VERTS')
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(cfg.get('collision_dissolve_deg', 6)), verts=bm.verts, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    print('COLLISION', name, len(me.polygons), 'tris')
    return ob


deck = fitp((0, 0, cfg['deck_z_raw'])).z
coll_parts = [convex_part([hull], -1e6, deck + 2, 'CollLower'),
              convex_part([hull, turret_base], deck - 2, 1e6, 'CollUpper')]
collision = join(coll_parts, 'SM_%s_Collision' % NAME)
collision.data.materials.clear()
collision.data.materials.append(bpy.data.materials.new('M_Collision'))

# ---------------------------------------------------------------- weapon rig
pivot = fitp(cfg['pivot_raw'])
muzzle = fitp(cfg['muzzle_raw'])
print('PIVOT', list(pivot), 'MUZZLE', list(muzzle))
for o in (mantlet, barrel):
    o.data.transform(Matrix.Translation(-pivot))

arm_data = bpy.data.armatures.new('Armature')
arm = bpy.data.objects.new('Armature', arm_data)
bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
for o in bpy.context.selected_objects:
    o.select_set(False)
arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
eb = arm_data.edit_bones
b_turret = eb.new('B_Turret')
b_turret.head = (0, 0, 0)
b_turret.tail = (0, 0, 30)
b_mantlet = eb.new('B_Mantlet')
b_mantlet.head = (0, 0, 0)
b_mantlet.tail = (30, 0, 0)
b_mantlet.parent = b_turret
b_gun = eb.new('B_Gun')
b_gun.head = (0.1, 0, 0)
b_gun.tail = (30, 0, 0)
b_gun.parent = b_mantlet
bpy.ops.object.mode_set(mode='OBJECT')

for o, bone in ((mantlet, 'B_Mantlet'), (barrel, 'B_Gun')):
    vg = o.vertex_groups.new(name=bone)
    vg.add(list(range(len(o.data.vertices))), 1.0, 'REPLACE')
weapon = join([mantlet, barrel], 'SK_%s_WeaponSystem' % NAME)
for bone in ('B_Turret',):
    weapon.vertex_groups.new(name=bone)
weapon.parent = arm
mod = weapon.modifiers.new('Armature', 'ARMATURE')
mod.object = arm

# ---------------------------------------------------------------- export helpers
def select_only(objs):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def export_fbx(objs, path, **kw):
    select_only(objs)
    args = dict(filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_NONE', global_scale=0.01, axis_forward='-Z',
                axis_up='Y', mesh_smooth_type='FACE', add_leaf_bones=False, use_armature_deform_only=True,
                bake_anim=False, path_mode='STRIP', use_triangles=False)
    args.update(kw)
    bpy.ops.export_scene.fbx(**args)
    print('EXPORT', path)


export_fbx([hull], os.path.join(OUT, 'SM_%s_Hull.fbx' % NAME))
export_fbx([turret_base], os.path.join(OUT, 'SM_%s_TurretBase.fbx' % NAME))
export_fbx([collision], os.path.join(OUT, 'SM_%s_Collision.fbx' % NAME))
export_fbx([arm, weapon], os.path.join(OUT, 'SK_%s_WeaponSystem.fbx' % NAME), object_types={'ARMATURE', 'MESH'})

# ---------------------------------------------------------------- animations
fps = 30
bpy.context.scene.render.fps = fps
recoil = cfg['recoil_m'] * U


def bone_matrix(name, rot_world=None, offset_world=None):
    """Armature-space matrix for a bone: rest matrix rotated about its head, optionally translated."""
    rest = arm.data.bones[name].matrix_local.copy()
    m = rest
    if rot_world is not None:
        head = rest.to_translation()
        m = Matrix.Translation(head) @ rot_world @ Matrix.Translation(-head) @ rest
    if offset_world is not None:
        m = Matrix.Translation(offset_world) @ m
    return m


def make_action(anim_name, keys):
    """keys: list of (frame, {bone: armature-space matrix}). Exports the action as its own FBX."""
    arm.animation_data_clear()
    bpy.context.view_layer.objects.active = arm
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    for frame, poses in keys:
        bpy.context.scene.frame_set(frame)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        bpy.context.view_layer.update()
        # parents first so children resolve against the posed parent
        for bname in ('B_Turret', 'B_Mantlet', 'B_Gun'):
            if bname in poses:
                arm.pose.bones[bname].matrix = poses[bname]
                bpy.context.view_layer.update()
        for pb in arm.pose.bones:
            pb.keyframe_insert('location', frame=frame)
            pb.keyframe_insert('rotation_quaternion', frame=frame)
            pb.keyframe_insert('scale', frame=frame)
    act = arm.animation_data.action
    act.name = anim_name
    last = keys[-1][0]
    bpy.context.scene.frame_start = 0
    bpy.context.scene.frame_end = last
    export_fbx([arm], os.path.join(OUT, '%s.fbx' % anim_name), object_types={'ARMATURE'}, bake_anim=True,
               bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
               bake_anim_simplify_factor=0.0)


Z = Vector((0, 0, 1))
Y = Vector((0, 1, 0))
yaw = cfg['yaw_deg']
up, down = cfg['pitch_up_deg'], cfg['pitch_down_deg']


def yaw_pose(deg):
    r = Matrix.Rotation(math.radians(deg), 4, Z)
    return {'B_Turret': bone_matrix('B_Turret', r), 'B_Mantlet': bone_matrix('B_Mantlet', r), 'B_Gun': bone_matrix('B_Gun', r)}


def pitch_pose(deg_up):
    # Rotating +X towards +Z is a negative rotation about +Y.
    r = Matrix.Rotation(math.radians(-deg_up), 4, Y)
    return {'B_Mantlet': bone_matrix('B_Mantlet', r), 'B_Gun': bone_matrix('B_Gun', r)}


rest = {}
P = 'Anim_%s_' % NAME
make_action(P + 'TPose', [(0, rest), (1, rest)])
make_action(P + 'Base', [(0, rest), (1, rest)])
# UE yaw is clockwise seen from above while Blender's Z rotation is counter clockwise: Blender +deg is UE left.
make_action(P + 'Left%d' % yaw, [(0, yaw_pose(yaw)), (1, yaw_pose(yaw))])
make_action(P + 'Right%d' % yaw, [(0, yaw_pose(-yaw)), (1, yaw_pose(-yaw))])
make_action(P + 'Up%d' % up, [(0, pitch_pose(up)), (1, pitch_pose(up))])
make_action(P + 'Down%d' % down, [(0, pitch_pose(-down)), (1, pitch_pose(-down))])
back = {'B_Gun': bone_matrix('B_Gun', offset_world=Vector((-recoil, 0, 0)))}
make_action(P + 'Fire', [(0, rest), (2, back), (6, back), (20, rest)])
arm.animation_data_clear()
for pb in arm.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)

# ---------------------------------------------------------------- textures
for t in cfg.get('textures', []):
    img = bpy.data.images.load(t['src'])
    dst = os.path.join(OUT, t['dst'])
    if t['src'].lower().endswith('.psd'):
        _ = img.pixels[0]
        bpy.context.scene.render.image_settings.file_format = 'PNG'
        bpy.context.scene.render.image_settings.color_mode = 'RGB'
        img.save_render(dst)
    else:
        import shutil
        shutil.copyfile(t['src'], dst)
    print('TEXTURE', dst, tuple(img.size))

json.dump({'pivot_cm': [pivot.x, -pivot.y, pivot.z],
           'muzzle_rel_cm': [muzzle.x - pivot.x, -(muzzle.y - pivot.y), muzzle.z - pivot.z]},
          open(os.path.join(OUT, 'rig_info.json'), 'w'), indent=1)
bpy.ops.wm.save_as_mainfile(filepath=cfg['save_blend'])
print('DONE')
