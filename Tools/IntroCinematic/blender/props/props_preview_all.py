"""Re-imports every exported FBX, checks it against the brief, renders preview_all.png and writes README.md.

Run after props_build.py:
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P props_preview_all.py
"""
import bpy, math, os, sys, json
import numpy as np
from mathutils import Vector, Matrix, Euler

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import props_lib as L

ORDER = ['OilLamp', 'Canister', 'MapPin', 'StarPin', 'PaperTag', 'PaperTag_Curled', 'BurnRing', 'Banner',
         'PictureFrame', 'MapTable']
LIMITS = {'OilLamp': 4000, 'Canister': 3000, 'MapPin': 200, 'BurnRing': 64, 'MapTable': 3000}
checks = []


def check(prop, what, ok, detail=''):
    checks.append((prop, what, bool(ok), detail))
    print('CHECK %-16s %-46s %s %s' % (prop, what, 'PASS' if ok else 'FAIL', detail))


def import_fbx(key):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(L.OUT, 'SM_Intro_%s.fbx' % key))
    new = [o for o in bpy.data.objects if o not in before]
    assert len(new) == 1, new
    return new[0]


def world_verts(ob):
    bpy.context.view_layer.update()
    n = len(ob.data.vertices)
    a = np.empty(n * 3, np.float64)
    ob.data.vertices.foreach_get('co', a)
    m = np.array(ob.matrix_world)
    return a.reshape(n, 3) @ m[:3, :3].T + m[:3, 3]


def slot_uvs(ob, slot):
    me = ob.data
    idx = [i for i, m in enumerate(me.materials) if m.name.split('.')[0] == slot][0]
    uv = me.uv_layers[0].data
    return np.array([uv[li].uv[:] for p in me.polygons if p.material_index == idx for li in p.loop_indices])


def verify(key, ob, st):
    me = ob.data
    w = world_verts(ob) * 100.0                         # centimetres, Blender axes (Unreal: same X and Z, Y negated)
    lo, hi = w.min(0), w.max(0)
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    st['fbx_tris'] = tris
    st['fbx_size_cm'] = [round(float(v), 2) for v in (hi - lo)]
    st['fbx_min_cm'] = [round(float(v), 2) for v in lo]
    st['fbx_max_cm'] = [round(float(v), 2) for v in hi]
    raw = np.array([v.co[:] for v in me.vertices])
    check(key, 'FBX stores centimetres at node scale 1', abs(ob.scale.x - 0.01) < 1e-6 and
          abs((raw.max(0) - raw.min(0)).max() - (hi - lo).max()) < 0.01, 'import scale %.4f' % ob.scale.x)
    if key in LIMITS:
        check(key, 'triangles <= %d' % LIMITS[key], tris <= LIMITS[key], str(tris))
    check(key, 'triangle count matches the build', tris == st['tris'], str(tris))
    names = [m.name.split('.')[0] for m in me.materials]
    check(key, 'material slots ' + ', '.join(st['slots']), names == st['slots'], str(names))
    check(key, 'one UV channel on every face', len(me.uv_layers) == 1)
    nrm = np.array([p.normal[:] for p in me.polygons])
    check(key, 'no degenerate face normals', bool((np.linalg.norm(nrm, axis=1) > 0.5).all()))
    if key == 'OilLamp':
        check(key, 'pivot at bail top (nothing above but wire radius)', hi[2] <= 0.21 and abs(lo[2] + 33.5) < 0.1,
              'z %.2f .. %.2f' % (lo[2], hi[2]))
    if key in ('MapPin', 'StarPin'):
        tip = w[w[:, 2].argmin()]
        check(key, 'needle tip at the origin', np.abs(tip).max() < 1e-3, str(np.round(tip, 4)))
        check(key, 'height', abs(hi[2] - (4.0 if key == 'MapPin' else 5.0)) < 0.01, '%.2f cm' % hi[2])
    if key == 'Canister':
        s = hi - lo
        check(key, 'long axis along X, about 110 x 35 cm', s[0] > s[1] and s[0] > s[2] and abs(s[0] - 110) < 1,
              str(np.round(s, 1)))
    if key == 'MapTable':
        check(key, 'top exactly at z = 95 cm, nothing above', abs(hi[2] - 95.0) < 1e-3, '%.4f' % hi[2])
        top = w[np.abs(w[:, 2] - 95.0) < 1e-3]
        check(key, 'top spans 360 x 240 cm', abs(np.ptp(top[:, 0]) - 360) < 0.01 and abs(np.ptp(top[:, 1]) - 240) < 0.01,
              '%.2f x %.2f' % (np.ptp(top[:, 0]), np.ptp(top[:, 1])))
        check(key, 'pivot at floor centre', abs(lo[2]) < 1e-3 and abs(lo[0] + hi[0]) < 0.01 and abs(lo[1] + hi[1]) < 0.01)
    if key == 'BurnRing':
        uv = slot_uvs(ob, 'Burn')
        wn = nrm @ np.array(ob.matrix_world)[:3, :3].T * 100
        check(key, 'flat in XY, faces +Z, 6 cm', np.ptp(w[:, 2]) < 1e-4 and (wn[:, 2] > 0.99).all() and
              abs(hi[0] - lo[0] - 6.0) < 0.01)
        check(key, 'UVs span 0-1', abs(uv.min()) < 1e-4 and abs(uv.max() - 1) < 1e-4)
    if key in ('PaperTag', 'PaperTag_Curled'):
        uv = slot_uvs(ob, 'Paper')
        check(key, 'front (Paper) UVs span exactly 0-1', np.abs(uv.min(0)).max() < 1e-4 and np.abs(uv.max(0) - 1).max() < 1e-4,
              '%s .. %s' % (np.round(uv.min(0), 4), np.round(uv.max(0), 4)))
        st['_paper_uv'] = np.round(uv, 5)
    if key == 'Banner':
        uv = slot_uvs(ob, 'Cloth')
        check(key, 'cloth UVs span exactly 0-1', np.abs(uv.min(0)).max() < 1e-4 and np.abs(uv.max(0) - 1).max() < 1e-4)
        idx = [i for i, m in enumerate(me.materials) if m.name.split('.')[0] == 'Cloth'][0]
        zs, vs = [], []
        for p in me.polygons:
            if p.material_index == idx:
                for li, vi in zip(p.loop_indices, p.vertices):
                    zs.append(w[vi][2])
                    vs.append(me.uv_layers[0].data[li].uv[1])
        zs, vs = np.array(zs), np.array(vs)
        check(key, 'V = 1 at the pole, V = 0 at the hem', vs[zs.argmax()] > 0.999 and vs[zs.argmin()] < 0.001)
        cl = np.array([w[vi] for p in me.polygons if p.material_index == idx for vi in p.vertices])
        check(key, 'cloth 220 x 700 cm', abs(np.ptp(cl[:, 0]) - 220) < 0.01 and abs(np.ptp(cl[:, 2]) - 700) < 0.01,
              '%.1f x %.1f' % (np.ptp(cl[:, 0]), np.ptp(cl[:, 2])))
        check(key, 'cloth is 24 x 64 quads', sum(1 for p in me.polygons if p.material_index == idx) == 24 * 64)
        check(key, 'pivot at pole shaft top centre (finials +1.3 cm)', abs(hi[2] - 1.3) < 0.05 and abs(lo[0] + hi[0]) < 0.01,
              'max z %.2f' % hi[2])
    if key == 'PictureFrame':
        s = hi - lo
        check(key, 'outer 120 x 160 cm, pivot top centre', abs(s[0] - 120) < 0.01 and abs(s[2] - 160) < 0.01 and
              abs(hi[2]) < 1e-3 and abs(lo[0] + hi[0]) < 0.01)


def place(ob, loc, rz=0.0):
    ob.matrix_world = Matrix.Translation(Vector(loc)) @ Euler((0, 0, rz)).to_matrix().to_4x4() @ ob.matrix_world


def marker():
    """1.8 m human-height marker: striped post (30 cm bands) with a head ball."""
    b = L.Builder(3)
    for i in range(6):
        b.add(b.cyl((0, 0, 0.3 * i), (0, 0, 0.3 * i + (0.3 if i < 5 else 0.07)), 0.11, 16), 'a' if i % 2 == 0 else 'b')
    b.add(b.lathe([(0.115 * math.sin(math.pi * k / 8), 1.685 - 0.115 * math.cos(math.pi * k / 8)) if 0 < k < 8 else
                   (0, 1.685 - 0.115 * math.cos(math.pi * k / 8)) for k in range(9)], 16), 'a', smooth=180)
    mats = {}
    for n, c in (('a', (0.8, 0.25, 0.02, 1)), ('b', (0.85, 0.85, 0.85, 1))):
        m = bpy.data.materials.new('PV_marker_' + n)
        m.use_nodes = True
        m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = c
        mats[n] = m
    return b.to_object('marker', bpy.context.scene, lambda n: mats[n])


def load_png(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)[::-1, :, :3]


def main():
    sc = L.reset()
    stats, objs = {}, {}
    for key in ORDER:
        st = json.load(open(os.path.join(L.OUT, '_stats_%s.json' % key)))
        ob = import_fbx(key)
        verify(key, ob, st)
        tex = {sl: s['tex'] for s in st['sets'] for sl in s['slots']}
        for i, m in enumerate(ob.data.materials):
            sl = m.name.split('.')[0]
            ob.data.materials[i] = L.pv_material('PV_%s_%s' % (key, sl), tex[sl], st.get('kinds', {}).get(sl, 'opaque'))
        stats[key], objs[key] = st, ob
    a, b = stats['PaperTag'].pop('_paper_uv'), stats['PaperTag_Curled'].pop('_paper_uv')
    check('PaperTag_Curled', 'UVs identical to SM_Intro_PaperTag', a.shape == b.shape and np.abs(a - b).max() < 1e-4)

    # ---------------------------------------------------------------- stage
    place(objs['MapTable'], (0, 0, 0))
    place(objs['OilLamp'], (0.55, -0.80, 0.95 + 0.335))
    place(objs['MapPin'], (0.72, -1.02, 0.95))
    place(objs['StarPin'], (0.80, -0.98, 0.95))
    place(objs['PaperTag'], (0.98, -1.00, 0.9512), 0.4)
    place(objs['PaperTag_Curled'], (1.16, -0.92, 0.9512), 0.2)
    place(objs['BurnRing'], (0.86, -1.08, 0.9503))
    place(objs['Banner'], (-3.3, 2.2, 7.07))
    place(objs['PictureFrame'], (3.1, 0.9, 1.6), math.pi - 0.35)
    place(objs['Canister'], (2.9, -1.3, 0.192), 0.5)
    mk = marker()
    mk.location = (-2.5, -1.9, 0)
    allobjs = list(objs.values()) + [mk]
    L.setup_cycles(sc, samples=64)
    lo, hi = L.bbox(allobjs)
    tmp = L.add_stage(sc, lo, hi, (0.42, -1.0, 0.30), ground_z=0.0)
    cam = sc.camera
    cam.data.lens = 76
    sc.render.filepath = os.path.join(L.OUT, 'preview_all.png')
    bpy.ops.render.render(write_still=True)
    # inset: the small props on the table
    cam.data.lens = 60
    small = [objs[k] for k in ('OilLamp', 'MapPin', 'StarPin', 'PaperTag', 'PaperTag_Curled', 'BurnRing')]
    lo, hi = L.bbox(small)
    c = (lo + hi) / 2
    d = Vector((0.25, -1.0, 0.55)).normalized()
    dist = (hi - lo).length / 2 / math.sin(math.atan(18 * 0.75 / 60)) * 0.82
    cam.location = c + d * dist
    cam.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
    cam.data.clip_start = 0.05
    sc.render.resolution_x, sc.render.resolution_y = 520, 390
    tmp_path = os.path.join(bpy.app.tempdir, 'inset.png')
    sc.render.filepath = tmp_path
    bpy.ops.render.render(write_still=True)
    big = load_png(os.path.join(L.OUT, 'preview_all.png')).copy()
    ins = load_png(tmp_path)
    h, w = ins.shape[:2]
    y0, x0 = 16, big.shape[1] - w - 16
    big[y0 - 3:y0 + h + 3, x0 - 3:x0 + w + 3] = 0.93
    big[y0:y0 + h, x0:x0 + w] = ins
    L.write_png(os.path.join(L.OUT, 'preview_all.png'), L.to8(big))
    write_readme(stats)
    bad = [c for c in checks if not c[2]]
    print('CHECKS %d passed, %d failed' % (len(checks) - len(bad), len(bad)))


def write_readme(stats):
    out = L.OUT
    T = []
    T.append('# Intro cinematic props (Blender side)\n')
    T.append('Ten static meshes for the bunker-briefing intro film, modelled and textured procedurally with Blender 4.0 '
             'Python scripts (no downloads, no image sources, no AI images). Nothing here has been imported into Unreal.\n')
    T.append('## Import notes\n')
    T.append('- Units: every FBX stores **centimetre** vertex positions with node scale 1 and `UnitScaleFactor` 1, so it '
             'should import at 1 uu = 1 cm with scale (1,1,1) and nothing to convert. Verified by re-importing in Blender, '
             '**not tested in Unreal**; it differs from the usual project recipe (metre scene + FBX_SCALE_NONE, '
             'which stores node scale 100).\n'
             '- Axes: Z up. The file is Y-up FBX (Blender default); Unreal converts it so Blender X -> Unreal X, '
             'Blender Y -> Unreal -Y, Z -> Z. "Faces -Y in Blender" below therefore means +Y in Unreal. '
             'This mapping is the standard one but was not checked in Unreal.\n'
             '- Normals are exported (smoothing by sharp edges); import with "Import Normals". No collision, no LODs, '
             'no lightmap UV channel.\n'
             '- Materials: the FBX carries only empty material slots with the names below; build the Unreal materials '
             'from the PNGs. Normal maps are **DirectX** (green down) - import as normal map, do not flip green. '
             'ORM is linear (untick sRGB): R = AO, G = roughness, B = metallic.\n'
             '- UV V: authored in the FBX/Blender convention (V = 1 is the top of the texture). Unreal flips V on '
             'import, so in a material `TexCoord.V` is 0 at the top (banner pole edge) and 1 at the hem.\n')
    T.append('## Meshes\n')
    T.append('| FBX | Size X x Y x Z (cm) | Triangles | Pivot | Material slots (in order) | Texture sets |')
    T.append('|---|---|---|---|---|---|')
    for k in ORDER:
        s = stats[k]
        lim = ' (limit %d)' % LIMITS[k] if k in LIMITS else ''
        T.append('| %s.fbx | %s | %d%s | %s | %s | %s |' % (
            s['name'], ' x '.join('%.1f' % v for v in s['fbx_size_cm']), s['fbx_tris'], lim, s['pivot'],
            ', '.join(s['slots']), '; '.join('%s -> T_Intro_%s (%d)' % ('+'.join(x['slots']), x['tex'], x['size'])
                                             for x in s['sets'])))
    T.append('')
    T.append('Bounding boxes relative to the pivot (cm, Blender axes; negate Y for Unreal):\n')
    for k in ORDER:
        s = stats[k]
        T.append('- %s: min (%s), max (%s)' % (s['name'], ', '.join('%.2f' % v for v in s['fbx_min_cm']),
                                               ', '.join('%.2f' % v for v in s['fbx_max_cm'])))
    T.append('')
    T.append(NOTES)
    T.append('## Checks run on the re-imported FBX files\n')
    T.append('Normals: every part is recalculated outward after `normal_update()` and each closed shell is asserted to '
             'have positive signed volume; open sheets are asserted against an explicit direction '
             '(closed / open shells per prop: %s).\n' % ', '.join(
                 '%s %d/%d' % (k, stats[k]['shells_closed'], stats[k]['shells_open']) for k in ORDER))
    T.append('| Prop | Check | Result | Value |')
    T.append('|---|---|---|---|')
    for p, w, ok, d in checks:
        T.append('| %s | %s | %s | %s |' % (p, w, 'pass' if ok else '**FAIL**', d))
    T.append('')
    T.append('## Files\n')
    T.append('| File | Size |')
    T.append('|---|---|')
    for f in sorted(os.listdir(out)):
        if f.startswith('_stats_') or f == 'README.md':
            continue
        n = os.path.getsize(os.path.join(out, f))
        T.append('| %s | %s |' % (f, '%.1f KB' % (n / 1024) if n < 1024 * 1024 else '%.2f MB' % (n / 1024 / 1024)))
    T.append('')
    T.append(RERUN)
    with open(os.path.join(out, 'README.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(T))


NOTES = """## Per-prop notes

- **SM_Intro_OilLamp** - hurricane lantern: tank, brass burner and wick wheel, double-walled glass globe, wire guard,
  chimney cap, two side tubes, bail modelled upright (hanging pose). Lantern body is 26.5 cm tall; the pivot is the
  bail wire centreline apex 33.5 cm above the base, so rotating the actor swings the whole lamp from the hook.
  `T_Intro_OilLamp_BC` has an **alpha channel** = glass opacity (soot); use it for the Glass slot (translucent,
  two-sided off: the globe has an inner wall) and use the BC as emissive colour on the Flame slot. All three slots
  share one UV atlas / texture set.
- **SM_Intro_Canister** - no text, numbers, symbols or decals anywhere; wear comes only from noise. Long axis X,
  valve cap at +X, four fins at -X (45 degrees off the axes), lifting lug on +Z. Pivot = volume centroid.
  Shell is 35.0 cm diameter (36.2 over the two bands) and 110.0 cm long; fins and lug take the box to 37.9 x 41.4.
- **SM_Intro_MapPin** - head is worn ivory enamel so it can be tinted per pin in the material. Needle is 1 mm.
- **SM_Intro_StarPin** - the faceted star lies horizontal (faces +Z, one point towards +Y in Blender / -Y in Unreal)
  so it reads as a star from a camera looking down at the map. Red enamel over brass.
- **SM_Intro_PaperTag / _Curled** - lies in XY, front faces +Z, 0.3 mm thick closed solid. U runs along the 9 cm
  length (U = 0 plain end, U = 1 at the hole end, hole centre at U 0.889, V 0.5), V across the 4.5 cm. Slots: Paper =
  front (full 0-1 square), PaperBack = back and edges (same UVs, so the same textures fit), String = twine loop
  (UVs tile along the length: U repeats every 2 cm, set the String textures to wrap). The curled mesh has identical
  topology and UVs and the same string; `T_Intro_PaperTag_Charred` is a scorched version of the paper textures for it.
- **SM_Intro_BurnRing** - 32-triangle disc, UV 0-1 planar. `T_Intro_BurnRing_BC` alpha = scorch-ring mask (a starting
  point for the animated burn material).
- **SM_Intro_Banner** - cloth is one sheet of 24 x 64 even quads, 220 x 700 cm, facing -Y in Blender (+Y in Unreal),
  single-sided (use a two-sided material). Folds are static and fade to zero at the pole. The pole is 250 cm with
  turned finials; a cloth sleeve around it belongs to the Pole slot and the Banner_Pole texture set. The cloth weave in
  `T_Intro_Banner_*` is deliberately coarse (about 1.4 cm thread pairs) because 7 m over 2048 px cannot resolve real
  threads; `T_Intro_Banner_WeaveDetail_*` is a tileable fine weave to multiply in for close shots (tile about 22 x 70).
  No emblem, no text.
- **SM_Intro_PictureFrame** - hangs in the XZ plane; the back faces +Y in Blender (-Y in Unreal). Pivot: top centre on
  the rear plane of the moulding. Back has a mitred stretcher with cross bars and corner keys, backing board, eight
  Z-clips with screws, two screw eyes and a slack hanging wire. The front is a plain moulding and one flat dark panel
  (slot Front).
- **SM_Intro_MapTable** - twelve chamfered planks (7 cm thick) inside a steel edge strip with dome-head bolts, on four
  bolted angle-iron trestles with X braces and long diagonal braces. Plank faces and the strip top are all exactly at
  z = 95 cm; only the 4 mm chamfer grooves dip below it. The dome bolt heads make the overall footprint 361.1 x 241.1 cm.
"""

RERUN = """## Re-running

Scripts: `Source/Tools/IntroCinematic/blender/props/` (`props_lib.py` helpers, `props_mats.py` procedural materials,
`props_build.py` models + bake + export + per-prop preview, `props_preview_all.py` checks + preview_all + this README).

```
cd D:/UE5Projects/RTS_Survival/Source/Tools/IntroCinematic/blender/props
D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P props_build.py
D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P props_preview_all.py
```

Options for `props_build.py` (after `--`): `--only OilLamp,MapTable` builds a subset; `--no-bake` skips texture baking
and renders the preview with the procedural materials (fast shape iteration). A full run takes roughly ten minutes on
CPU. Output folder: `Saved/IntroCinematic/import/Props/` (the `_stats_*.json` files are build records read by
`props_preview_all.py`).
"""

if __name__ == '__main__':
    main()
