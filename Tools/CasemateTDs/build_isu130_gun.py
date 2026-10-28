"""Build the ISU-130's S-26 130mm weapon system on the ISU-152 rig.

Usage: Blender5.0.exe -b --factory-startup --python build_isu130_gun.py

The ISU-130 uses the ISU-152 hull and gun mount unchanged, so only the gun is new. The script opens
ISU_152_Split.blend (read only, made by build_td.py), keeps its Armature (B_Turret yaw, B_Mantlet pitch, B_Gun recoil)
and the mantlet sleeve island, and replaces the ML-20S barrel with the long S-26: a 130mm tube with a mid collar and
a windowed multi-baffle muzzle brake. Keeping the bones identical lets the new mesh share
SK_ISU152_WeaponSystem_Skeleton, so AnimBP_ISU152, AO_ISU152, the anims and the fire montage carry over.

Coordinates are centimetres with the gun pivot at the origin and the bore along +X (the build_td.py convention); the
FBX export uses build_td.py's settings so Unreal receives the same bone transforms as the ISU-152.

Outputs (Saved/ClaudeExports/ISU130): SK_ISU130_WeaponSystem.fbx, SM_ISU130_Gun_Destroyed.fbx, rig_info.json;
the source scene is saved to (KADO-3D)/(RUSSIAN)/ISU_130/SK_ISU130_WeaponSystem.blend.
"""
import bpy, bmesh, json, math, os
from mathutils import Matrix, Vector

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_152/ISU_152_Split.blend'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/ISU130'
BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(RUSSIAN)/ISU_130/SK_ISU130_WeaponSystem.blend'
NAME = 'ISU130'
os.makedirs(OUT, exist_ok=True)
os.makedirs(os.path.dirname(BLEND), exist_ok=True)

# S-26: 130mm, L/54.7. The real gun reaches 4.8 m past the nose (11.42 m overall); the KADO hull is fitted at x1.1,
# so the muzzle sits ~595 cm ahead of the pivot (hull nose at x 362, pivot at x 293.6).
MUZZLE_X = 600.0
SEG_BARREL = 16
SEG_BRAKE = 20
SHARP_DEG = 40.0
# Atlas regions on T_ISU152_Camo_D (u0, v0, u1, v1), inset from the source islands.
BARREL_STRIP = (0.885, 0.080, 0.960, 0.320)   # the clean camo part of the ML-20S barrel strip (its brake is below)
BRAKE_METAL = (0.270, 0.795, 0.332, 0.858)     # the ML-20S muzzle brake's dark slotted steel
BORE_DARK = (0.300, 0.830, 0.304, 0.834)       # a dark texel inside the brake cell

bpy.ops.wm.open_mainfile(filepath=SRC)
arm = bpy.data.objects['Armature']
src = bpy.data.objects['SK_ISU152_WeaponSystem']
for o in list(bpy.data.objects):
    if o not in (arm, src):
        bpy.data.objects.remove(o)
for m in list(bpy.data.materials):
    if m.name != 'M_ISU152':
        bpy.data.materials.remove(m)
weapon = src
weapon.name = 'SK_%s_WeaponSystem' % NAME
weapon.data = weapon.data.copy()
weapon.data.name = weapon.name
assert [m.name for m in weapon.data.materials] == ['M_ISU152']
UV_NAME = weapon.data.uv_layers[0].name
print('SOURCE attributes', [(a.name, a.domain, a.data_type) for a in weapon.data.attributes])

# ---------------------------------------------------------------- drop the ML-20S barrel, keep the mantlet sleeve
bm = bmesh.new()
bm.from_mesh(weapon.data)
dl = bm.verts.layers.deform.verify()
gi = {g.name: g.index for g in weapon.vertex_groups}
barrel_verts = [v for v in bm.verts if gi['B_Gun'] in v[dl]]
bmesh.ops.delete(bm, geom=barrel_verts, context='VERTS')
bm.to_mesh(weapon.data)
bm.free()
n_keep = len(weapon.data.vertices)
print('KEPT mantlet sleeve verts', n_keep, 'faces', len(weapon.data.polygons))


# ---------------------------------------------------------------- lathe helpers
def _uv(box, a, b):
    return (box[0] + (box[2] - box[0]) * a, box[1] + (box[3] - box[1]) * b)


def lathe(bm, uvl, prof, seg, box, a0=0.0, a1=2 * math.pi, closed=False, caps=False, cap_box=None):
    """Revolve a (x, r) profile around +X. Traversal rule: the solid lies to the left of the profile direction
    (walking +x at constant r gives an outward face). r == 0 points become poles. A partial sweep (a0..a1) can
    close its two ends with planar caps. UV: around -> u, cumulative profile length -> v."""
    full = abs((a1 - a0) - 2 * math.pi) < 1e-6
    nv = seg if full else seg + 1
    pts = list(prof) + ([prof[0]] if closed else [])
    s = [0.0]
    for i in range(1, len(pts)):
        s.append(s[-1] + math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]))
    total = s[-1] or 1.0
    rings = []
    for i, (x, r) in enumerate(pts):
        if closed and i == len(pts) - 1:
            rings.append(rings[0])
            continue
        if r < 1e-6:
            rings.append([bm.verts.new((x, 0.0, 0.0))] * nv)
            continue
        ring = []
        for j in range(nv):
            t = a0 + (a1 - a0) * j / seg
            ring.append(bm.verts.new((x, r * math.cos(t), r * math.sin(t))))
        rings.append(ring)
    faces = []
    for i in range(len(pts) - 1):
        for j in range(seg):
            j1 = (j + 1) % nv if full else j + 1
            q = [rings[i][j], rings[i][j1], rings[i + 1][j1], rings[i + 1][j]]
            uvs = [_uv(box, j / seg, s[i] / total), _uv(box, (j + 1) / seg, s[i] / total),
                   _uv(box, (j + 1) / seg, s[i + 1] / total), _uv(box, j / seg, s[i + 1] / total)]
            keep = [k for k in range(4) if q[k] not in q[:k]]
            if len(keep) < 3:
                continue
            f = bm.faces.new([q[k] for k in keep])
            for lp, k in zip(f.loops, keep):
                lp[uvl].uv = uvs[k]
            faces.append(f)
    if caps and not full:
        cb = cap_box or box
        xs = [p[0] for p in prof]
        rs = [p[1] for p in prof]
        for j, t, sign in ((0, a0, -1.0), (seg, a1, 1.0)):
            vs = [rings[i][j] for i in range(len(prof))]
            f = bm.faces.new(vs)
            f.normal_update()
            want = Vector((0.0, -math.sin(t), math.cos(t))) * sign
            if f.normal.dot(want) < 0:
                f.normal_flip()
            for lp in f.loops:
                i = vs.index(lp.vert)
                lp[uvl].uv = _uv(cb, (xs[i] - min(xs)) / (max(xs) - min(xs) or 1), (rs[i] - min(rs)) / (max(rs) - min(rs) or 1))
            faces.append(f)
    return faces


# ---------------------------------------------------------------- S-26 barrel and muzzle brake (all on B_Gun)
gm = bpy.data.meshes.new('S26')
bm = bmesh.new()
uvl = bm.loops.layers.uv.new(UV_NAME)
# Rear tube: starts 34 cm inside the sleeve so the 45 cm recoil stroke never shows the end; r 13 stays inside the
# sleeve's 13.4 cm exit. The mid collar marks the hoop joint; the long chase tapers to the muzzle swell.
REAR = [(60.0, 13.0), (255.0, 13.0), (255.0, 13.9)]
FRONT = [(255.0, 13.9), (266.0, 13.9), (266.0, 12.2), (528.0, 10.0), (528.0, 11.0), (536.0, 11.0),
         # brake rear collar (threaded onto the muzzle) whose front face closes the bore
         (536.0, 15.0), (552.0, 15.0), (552.0, 0.0)]
lathe(bm, uvl, REAR, SEG_BARREL, BARREL_STRIP)
lathe(bm, uvl, FRONT[:6], SEG_BARREL, BARREL_STRIP)
# The brake starts at the swell with the brake's own segment count; this lathe begins at r 11 like the swell end.
lathe(bm, uvl, FRONT[5:], SEG_BRAKE, BRAKE_METAL)
# Baffles: annular plates (bore r 7) with three side windows between them and the front plate.
for x0, x1 in ((560.0, 565.0), (572.0, 577.0), (584.0, 589.0), (594.0, MUZZLE_X)):
    lathe(bm, uvl, [(x0, 7.0), (x0, 16.0), (x1, 16.0), (x1, 7.0)], SEG_BRAKE, BRAKE_METAL, closed=True)
# Top and bottom straps tie the baffles together and leave the sides open (the windows).
STRAP = [(552.0, 13.0), (552.0, 16.4), (MUZZLE_X, 16.4), (MUZZLE_X, 13.0)]
half = math.radians(34.0)
for centre in (math.pi / 2, -math.pi / 2):
    lathe(bm, uvl, STRAP, 6, BRAKE_METAL, a0=centre - half, a1=centre + half, closed=True, caps=True)
# Dark bore face on the collar's front (the inside of the barrel seen through the brake).
for f in bm.faces:
    f.normal_update()
    if abs(f.calc_center_median().x - 552.0) < 0.01 and f.normal.x > 0.9:
        for lp in f.loops:
            lp[uvl].uv = _uv(BORE_DARK, 0.5, 0.5)

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.001)
bmesh.ops.triangulate(bm, faces=bm.faces)
for f in bm.faces:
    f.smooth = True
for e in bm.edges:
    if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(SHARP_DEG):
        e.smooth = False
    elif len(e.link_faces) != 2:
        e.smooth = False
bm.to_mesh(gm)
bm.free()
gun = bpy.data.objects.new('S26', gm)
bpy.context.scene.collection.objects.link(gun)
gun.data.materials.append(bpy.data.materials['M_ISU152'])
vg = gun.vertex_groups.new(name='B_Gun')
vg.add(list(range(len(gm.vertices))), 1.0, 'REPLACE')
print('S26 verts', len(gm.vertices), 'tris', len(gm.polygons))

# ---------------------------------------------------------------- join (the kept sleeve stays the vertex prefix)
for o in bpy.context.selected_objects:
    o.select_set(False)
gun.select_set(True)
weapon.select_set(True)
bpy.context.view_layer.objects.active = weapon
bpy.ops.object.join()
assert [m.name for m in weapon.data.materials] == ['M_ISU152'], [m.name for m in weapon.data.materials]
assert [u.name for u in weapon.data.uv_layers] == [UV_NAME]
for p in weapon.data.polygons:
    p.material_index = 0

# Triangulate the kept sleeve too (its vertices and UVs are untouched), so Blender and Unreal use the same triangles.
bm = bmesh.new()
bm.from_mesh(weapon.data)
quads = [f for f in bm.faces if len(f.verts) > 3]
if quads:
    bmesh.ops.triangulate(bm, faces=quads)
bm.to_mesh(weapon.data)
bm.free()
weapon.data.update()
print('WEAPON verts', len(weapon.data.vertices), 'tris', len(weapon.data.polygons))

# ---------------------------------------------------------------- wreck gun (static, drooped and turned at the pivot)
wreck = weapon.copy()
wreck.data = weapon.data.copy()
bpy.context.scene.collection.objects.link(wreck)
wreck.modifiers.clear()
wreck.parent = None
wreck.matrix_world = Matrix.Identity(4)
wreck.vertex_groups.clear()
wreck.data.materials.clear()
wreck.data.materials.append(bpy.data.materials.new('Destroyed'))
# Same pose as the ISU-152 wreck gun (wreck_isu152.json): droop 6, turn -4, sink 5 cm.
wreck.data.transform(Matrix.Rotation(math.radians(-4), 4, 'Z') @ Matrix.Rotation(math.radians(6), 4, 'Y')
                     @ Matrix.Translation(Vector((0, 0, -5))))
wreck.name = wreck.data.name = 'SM_%s_Gun_Destroyed' % NAME


# ---------------------------------------------------------------- export (build_td.py settings)
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


export_fbx([arm, weapon], os.path.join(OUT, 'SK_%s_WeaponSystem.fbx' % NAME), object_types={'ARMATURE', 'MESH'})
export_fbx([wreck], os.path.join(OUT, 'SM_%s_Gun_Destroyed.fbx' % NAME), object_types={'MESH'}, use_armature_deform_only=False)

json.dump({'pivot_cm': [293.5799865722656, 35.20000076293945, 192.10000610351562],
           'muzzle_rel_cm': [MUZZLE_X, 0.0, 0.0],
           'isu152_muzzle_rel_cm': [317.4600524902344, 0.0, 0.0],
           'kept_sleeve_verts': n_keep,
           'weapon_tris': len(weapon.data.polygons)},
          open(os.path.join(OUT, 'rig_info.json'), 'w'), indent=1)
bpy.data.objects.remove(wreck)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print('DONE')
