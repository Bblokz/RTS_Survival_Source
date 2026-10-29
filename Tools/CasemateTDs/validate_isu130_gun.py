"""Validate the ISU-130 S-26 weapon system (build_isu130_gun.py output).

Usage: Blender5.0.exe -b --factory-startup --python validate_isu130_gun.py

Native: the saved .blend against ISU_152_Split.blend (identical rig, preserved mantlet sleeve, rigid weights,
recoil and depression clearance). Clean FBX: SK_ISU130_WeaponSystem.fbx and the shipped SK_ISU152_WeaponSystem.fbx are
imported into fresh scenes and their skeletons compared, because the new mesh shares SK_ISU152_WeaponSystem_Skeleton.
Writes ISU130_validation.txt next to the exports and ends with OVERALL PASS / FAIL.
"""
import bpy, bmesh, math, os, json
from mathutils import Vector, Matrix

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_152/ISU_152_Split.blend'
BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_130/SK_ISU130_WeaponSystem.blend'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/ISU130'
FBX_NEW = OUT + '/SK_ISU130_WeaponSystem.fbx'
FBX_OLD = 'D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/ISU152/SK_ISU152_WeaponSystem.fbx'
FBX_WRECK = OUT + '/SM_ISU130_Gun_Destroyed.fbx'
PIVOT = Vector((293.58, -35.2, 192.1))   # Blender axes
RECOIL = 45.0                            # Anim_ISU152_Fire (isu152.json recoil_m 0.45)
YAW, UP, DOWN = 8.0, 20.0, 3.0           # AO_ISU152 range, the widest the shared anims can show

results = []


def check(name, ok, detail=''):
    results.append((bool(ok), name, detail))
    print(('PASS ' if ok else 'FAIL ') + name + ('  | ' + str(detail) if detail else ''))


def bone_table(arm):
    return {b.name: (b.parent.name if b.parent else None, b.head_local.copy(), b.tail_local.copy(), b.matrix_local.copy())
            for b in arm.data.bones}


def same_bones(a, b, tol):
    if set(a) != set(b):
        return False, 'names %s vs %s' % (sorted(a), sorted(b))
    worst = 0.0
    for n in a:
        if a[n][0] != b[n][0]:
            return False, 'parent of %s' % n
        for i in (1, 2):
            worst = max(worst, (a[n][i] - b[n][i]).length)
        worst = max(worst, max(abs(a[n][3][r][c] - b[n][3][r][c]) for r in range(4) for c in range(4)))
    return worst < tol, 'max deviation %.2e' % worst


# ---------------------------------------------------------------- source reference
bpy.ops.wm.open_mainfile(filepath=SRC)
src_bones = bone_table(bpy.data.objects['Armature'])
s_obj = bpy.data.objects['SK_ISU152_WeaponSystem']
s_gi = {g.index: g.name for g in s_obj.vertex_groups}
src_sleeve = [(v.co.copy(), [s_gi[g.group] for g in v.groups]) for v in s_obj.data.vertices
              if [s_gi[g.group] for g in v.groups] == ['B_Mantlet']]
s_uv = s_obj.data.uv_layers[0].data
src_sleeve_uv = sorted(set((round(s_uv[l.index].uv.x, 6), round(s_uv[l.index].uv.y, 6))
                       for p in s_obj.data.polygons for l in (s_obj.data.loops[i] for i in p.loop_indices)
                       if all([s_gi[g.group] for g in s_obj.data.vertices[s_obj.data.loops[j].vertex_index].groups] == ['B_Mantlet']
                              for j in p.loop_indices)))
hull = bpy.data.objects['SM_ISU152_Hull']
hull_pts = [hull.matrix_world @ v.co for v in hull.data.vertices]
base = bpy.data.objects['SM_ISU152_TurretBase']
base_front_x = max((base.matrix_world @ v.co).x for v in base.data.vertices)

# ---------------------------------------------------------------- native
bpy.ops.wm.open_mainfile(filepath=BLEND)
s = bpy.context.scene
objs = {o.name: o for o in bpy.data.objects}
check('native: objects are exactly Armature + SK_ISU130_WeaponSystem', set(objs) == {'Armature', 'SK_ISU130_WeaponSystem'}, sorted(objs))
arm, me_o = objs['Armature'], objs['SK_ISU130_WeaponSystem']
check('native: no actions in the file', len(bpy.data.actions) == 0, [a.name for a in bpy.data.actions])
for o in (arm, me_o):
    ident = o.matrix_world == Matrix.Identity(4) and tuple(o.scale) == (1, 1, 1)
    check('native: %s identity transform' % o.name, ident, (tuple(o.location), tuple(o.rotation_euler), tuple(o.scale)))
check('native: unit setup matches the ISU-152 split scene (scale 1.0, cm data)', abs(s.unit_settings.scale_length - 1.0) < 1e-9,
      s.unit_settings.scale_length)
ok, d = same_bones(bone_table(arm), src_bones, 1e-5)
check('native: bones identical to the ISU-152 rig (B_Turret > B_Mantlet > B_Gun)', ok, d)
check('native: mesh parented to the armature with one Armature modifier',
      me_o.parent == arm and [(m.type, m.object) for m in me_o.modifiers] == [('ARMATURE', arm)],
      [(m.type, m.object.name if m.object else None) for m in me_o.modifiers])
me = me_o.data
me.calc_loop_triangles()
check('native: single material slot M_ISU152', [m.name for m in me.materials] == ['M_ISU152'], [m.name for m in me.materials])
check('native: UV layers [map1] like SK_ISU152_WeaponSystem', [u.name for u in me.uv_layers] == ['map1'], [u.name for u in me.uv_layers])
check('native: every face is a triangle', all(len(p.vertices) == 3 for p in me.polygons), len(me.polygons))
check('native: triangle count (loop_triangles) 1188', len(me.loop_triangles) == len(me.polygons) == 1188, len(me.loop_triangles))
gi = {g.index: g.name for g in me_o.vertex_groups}
check('native: vertex groups B_Mantlet / B_Gun / B_Turret present', set(gi.values()) == {'B_Mantlet', 'B_Gun', 'B_Turret'}, sorted(gi.values()))
rigid = all(len(v.groups) == 1 and abs(v.groups[0].weight - 1.0) < 1e-6 for v in me.vertices)
check('native: every vertex in exactly one group at weight 1.0', rigid)
grp = [gi[v.groups[0].group] for v in me.vertices]
n_sl = len(src_sleeve)
prefix_ok = all(grp[i] == 'B_Mantlet' and (me.vertices[i].co - src_sleeve[i][0]).length < 1e-5 for i in range(n_sl))
check('native: mantlet sleeve is the untouched vertex prefix (%d verts, B_Mantlet)' % n_sl, prefix_ok and grp.count('B_Mantlet') == n_sl)
uv = me.uv_layers[0].data
new_sleeve_uv = sorted(set((round(uv[li].uv.x, 6), round(uv[li].uv.y, 6)) for p in me.polygons for li in p.loop_indices
                       if all(grp[me.loops[j].vertex_index] == 'B_Mantlet' for j in p.loop_indices)))
check('native: mantlet sleeve UVs preserved', new_sleeve_uv == src_sleeve_uv, '%d vs %d distinct UVs' % (len(new_sleeve_uv), len(src_sleeve_uv)))
uvs = [tuple(d.uv) for d in uv]
check('native: UVs inside the atlas (0..1)', all(0.0 <= a <= 1.0 and 0.0 <= b <= 1.0 for a, b in uvs))
gun = [v.co for v, g in zip(me.vertices, grp) if g == 'B_Gun']
gx = [c.x for c in gun]
radius = lambda c: math.hypot(c.y, c.z)
check('native: muzzle at x 600 cm from the pivot (S-26 ~4.8 m past the nose x1.1)', abs(max(gx) - 600.0) < 0.01, max(gx))
check('native: bore on the B_Mantlet axis (y, z centred)', all(abs(sum(c.y for c in gun) / len(gun)) < 0.05 for _ in [0]) and
      abs(sum(c.z for c in gun) / len(gun)) < 0.05, (sum(c.y for c in gun) / len(gun), sum(c.z for c in gun) / len(gun)))
check('native: max radius <= 16.4 cm (brake straps)', max(radius(c) for c in gun) <= 16.4001, max(radius(c) for c in gun))
sleeve = [v.co for v, g in zip(me.vertices, grp) if g == 'B_Mantlet']
sleeve_exit_r = min(radius(c) for c in sleeve if c.x > 90)
rear = [c for c in gun if c.x < 100]
check('native: barrel inside the sleeve is thinner than the sleeve exit (r %.1f < %.1f)' % (max(radius(c) for c in rear), sleeve_exit_r),
      max(radius(c) for c in rear) < sleeve_exit_r)
check('native: recoil stroke keeps the barrel end inside the sleeve (%.1f > %.1f)' % (min(gx) - RECOIL, min(c.x for c in sleeve)),
      min(gx) - RECOIL > min(c.x for c in sleeve))
bm = bmesh.new(); bm.from_mesh(me)
check('native: no zero-area triangles', min(f.calc_area() for f in bm.faces) > 1e-4, min(f.calc_area() for f in bm.faces))
# Outward normals: barrel side faces must point away from the bore axis.
bad = 0
side = 0
for f in bm.faces:
    c = f.calc_center_median()
    radial = Vector((0, c.y, c.z))
    if c.x > 100 and radial.length > 9.0 and abs(f.normal.x) < 0.3:
        side += 1
        if f.normal.dot(radial.normalized()) < -0.2 and radial.length > 13.5 and not (552 < c.x < 600 and radial.length < 13.2):
            bad += 1
# Inner strap and baffle faces point inwards by design (r 13 / r 7): only faces outside r 13.5 are tested; strap end
# caps face along the circumference (dot ~0) and are not counted.
check('native: outer side faces point outward', bad == 0, '%d bad of %d side faces' % (bad, side))
bm.free()

# Clearance against the hull at the extreme aim poses (yaw +-8, pitch -3) the anims can show. The hull ahead of the
# gun mount is low and sparse, so the bound is conservative: the highest hull vertex ahead of the mount within 40 cm
# of the bore line (in y) against the lowest gun vertex ahead of the mount.
worst = 1e9
for yaw in (-YAW, 0.0, YAW):
    for pitch in (-DOWN, 0.0):
        m = Matrix.Translation(PIVOT) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(math.radians(-pitch), 4, 'Y')
        pts = [m @ c for c in gun]
        pts = [p for p in pts if p.x > base_front_x]
        ys = [p.y for p in pts]
        band = [h.z for h in hull_pts if h.x > base_front_x - 10 and min(ys) - 40 < h.y < max(ys) + 40]
        if pts and band:
            worst = min(worst, min(p.z for p in pts) - max(band))
check('native: gun clears the hull nose at yaw +-8 / pitch -3 (min gap %.1f cm)' % worst, 1.0 < worst < 1e8)

# ---------------------------------------------------------------- clean FBX
def clean_import(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    bpy.context.view_layer.update()
    return {o.name: o for o in bpy.data.objects}


old = clean_import(FBX_OLD)
old_arm = [o for o in old.values() if o.type == 'ARMATURE'][0]
old_bones = bone_table(old_arm)
old_mesh = [o for o in old.values() if o.type == 'MESH'][0]
old_len = old_mesh.dimensions.x
old_arm_m = old_arm.matrix_world.copy()
old_mesh_m = old_mesh.matrix_world.copy()

new = clean_import(FBX_NEW)
types = sorted(o.type for o in new.values())
check('fbx: one armature + one mesh', types == ['ARMATURE', 'MESH'], types)
new_arm = [o for o in new.values() if o.type == 'ARMATURE'][0]
new_mesh = [o for o in new.values() if o.type == 'MESH'][0]
ok, d = same_bones(bone_table(new_arm), old_bones, 1e-4)
check('fbx: skeleton identical to the shipped SK_ISU152_WeaponSystem.fbx (shared UE skeleton)', ok, d)
check('fbx: armature / mesh object transforms identical to the ISU-152 import',
      all(abs(new_arm.matrix_world[r][c] - old_arm_m[r][c]) < 1e-6 and abs(new_mesh.matrix_world[r][c] - old_mesh_m[r][c]) < 1e-6
          for r in range(4) for c in range(4)))
nm = new_mesh.data
nm.calc_loop_triangles()
check('fbx: triangle count 1188 retained', len(nm.loop_triangles) == 1188, len(nm.loop_triangles))
check('fbx: material slot M_ISU152', [m.name for m in nm.materials] == ['M_ISU152'], [m.name for m in nm.materials])
check('fbx: UV channel map1', [u.name for u in nm.uv_layers] == ['map1'], [u.name for u in nm.uv_layers])
ngi = {g.index: g.name for g in new_mesh.vertex_groups}
w_ok = all(len(v.groups) == 1 and abs(v.groups[0].weight - 1) < 1e-4 for v in nm.vertices)
cnt = {n: sum(1 for v in nm.vertices if v.groups and ngi[v.groups[0].group] == n) for n in ('B_Mantlet', 'B_Gun')}
check('fbx: rigid weights retained (B_Mantlet %d, B_Gun %d)' % (cnt['B_Mantlet'], cnt['B_Gun']), w_ok and cnt['B_Mantlet'] == n_sl)
ratio = new_mesh.dimensions.x / old_len
check('fbx: length scales like the ISU-152 export (%.4f vs %.4f expected)' % (ratio, 603.8 / 321.2), abs(ratio - 603.8 / 321.2) < 0.01,
      'old %.4f new %.4f (Blender import units)' % (old_len, new_mesh.dimensions.x))
check('fbx: no animation stacks in the mesh FBX (anims are the shared ISU-152 takes)', len(bpy.data.actions) == 0, [a.name for a in bpy.data.actions])

wr = clean_import(FBX_WRECK)
check('fbx: wreck gun is one static mesh, 1188 tris',
      [o.type for o in wr.values()] == ['MESH'] and len(list(wr.values())[0].data.polygons) == 1188,
      [(o.name, o.type, len(o.data.polygons) if o.type == 'MESH' else 0) for o in wr.values()])

# ---------------------------------------------------------------- report
n_pass = sum(1 for r in results if r[0])
overall = n_pass == len(results)
lines = ['ISU-130 S-26 weapon system validation', '%d/%d checks passed' % (n_pass, len(results)), '']
lines += [('PASS ' if r[0] else 'FAIL ') + r[1] + ('  | ' + str(r[2]) if r[2] else '') for r in results]
lines += ['', 'OVERALL ' + ('PASS' if overall else 'FAIL')]
open(os.path.join(OUT, 'ISU130_validation.txt'), 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines[-2:]))
