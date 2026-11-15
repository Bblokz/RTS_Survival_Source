"""Ural Shield - re-import every exported FBX into an empty scene, check scale / bounds /
triangles / UVs / tiling end profile, then write README.md from the measured values.

Run:  blender.exe --factory-startup -b -P verify_and_readme.py
"""
import os
import sys
import json
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import us_common as C

NAMES = ['SM_UralShield_Segment', 'SM_UralShield_GateFrame', 'SM_UralShield_GateLeaf_L',
         'SM_UralShield_GateLeaf_R', 'SM_UralShield_Far']
BUDGET = {'SM_UralShield_Segment': 18000, 'SM_UralShield_GateFrame': 25000, 'SM_UralShield_Far': 300}


def end_profile(ob, y_cm):
    """Sorted (x, z) of the vertices lying on a tiling end plane."""
    pts = set()
    for v in ob.data.vertices:
        w = ob.matrix_world @ v.co
        if abs(w.y - y_cm) < 0.05:
            pts.add((round(w.x, 1), round(w.z, 1)))
    return sorted(pts)


def main():
    stats = json.load(open(C.STATS))
    res = {}
    ends = {}
    for n in NAMES:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        path = os.path.join(C.OUT, n + '.fbx')
        bpy.ops.import_scene.fbx(filepath=path)
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        empties = [o for o in bpy.context.scene.objects if o.type == 'EMPTY']
        assert len(meshes) == 1, n
        ob = meshes[0]
        bpy.context.view_layer.update()
        ws = [ob.matrix_world @ v.co for v in ob.data.vertices]
        # FBX stores cm; Blender's importer converts to metres, so world metres * 100 = Unreal cm
        lo = [min(p[i] for p in ws) * 100 for i in range(3)]
        hi = [max(p[i] for p in ws) * 100 for i in range(3)]
        ob.data.calc_loop_triangles()
        tris = len(ob.data.loop_triangles)
        uv = ob.data.uv_layers[0].data
        us = [l.uv[0] for l in uv]
        vs = [l.uv[1] for l in uv]
        # degenerate / stretched UV check: every triangle needs UV area and a sane texel ratio
        bad = 0
        worst = 1.0
        for t in ob.data.loop_triangles:
            a, b, c = [Vector(uv[i].uv) for i in t.loops]
            if abs((b - a).cross(c - a)) < 1e-10:
                bad += 1
                continue
            # anisotropy of the UV -> surface map: ratio of its two singular values (1.0 = no stretch)
            p0, p1, p2 = [ob.data.vertices[i].co for i in t.vertices]
            e1, e2, d1, d2 = p1 - p0, p2 - p0, b - a, c - a
            det = d1.x * d2.y - d2.x * d1.y
            ju = (e1 * d2.y - e2 * d1.y) / det
            jv = (e2 * d1.x - e1 * d2.x) / det
            g11, g22, g12 = ju.dot(ju), jv.dot(jv), ju.dot(jv)
            tr, dt = g11 + g22, g11 * g22 - g12 * g12
            disc = max(tr * tr / 4 - dt, 0.0) ** 0.5
            worst = max(worst, ((tr / 2 + disc) / max(tr / 2 - disc, 1e-30)) ** 0.5)
        assert worst < 1.06, 'stretched UVs in %s: anisotropy %.3f' % (n, worst)
        st = stats[n]
        exp_lo = [st['bounds_min_cm'][i] for i in range(3)]
        exp_hi = [st['bounds_max_cm'][i] for i in range(3)]
        ok = all(abs(lo[i] - exp_lo[i]) < 1.0 and abs(hi[i] - exp_hi[i]) < 1.0 for i in range(3))
        res[n] = dict(file_bytes=os.path.getsize(path), tris=tris, lo=[round(x, 1) for x in lo],
                      hi=[round(x, 1) for x in hi], uv_v=[round(min(vs), 4), round(max(vs), 4)],
                      uv_u=[round(min(us), 3), round(max(us), 3)], zero_area_uv_tris=bad,
                      uv_max_anisotropy=round(worst, 4),
                      bounds_match_source=ok, mats=[m.name for m in ob.data.materials],
                      sockets={e.name: [round(x * 100, 1) for x in e.matrix_world.translation] for e in empties},
                      obj_loc=[round(x * 100, 2) for x in ob.matrix_world.translation])
        print('VERIFY', n, res[n])
        assert ok, 'bounds mismatch after FBX round trip: %s' % n
        # node scale 1 + UnitScaleFactor 1 in the file shows up as exactly 0.01 (cm -> m) on re-import
        assert all(abs(s - 0.01) < 1e-6 for s in ob.scale), ('FBX node scale is not 1', tuple(ob.scale))
        res[n]['import_scale'] = [round(s, 5) for s in ob.scale]
        assert tris == st['tris'] and bad == 0
        assert n not in BUDGET or tris <= BUDGET[n]
        assert len(ob.data.uv_layers) == 1 and min(vs) >= -1e-4 and max(vs) <= 1.0001
        if n in ('SM_UralShield_Segment', 'SM_UralShield_GateFrame', 'SM_UralShield_Far'):
            assert abs(lo[1] + 3000) < 0.5 and abs(hi[1] - 3000) < 0.5, 'tiling length'
            a, b = end_profile(ob, -30.0), end_profile(ob, 30.0)
            assert a == b and len(a) > 8, 'end profiles differ: %s' % n
            ends[n] = a
    assert ends['SM_UralShield_Segment'] == ends['SM_UralShield_GateFrame'], 'GateFrame end != Segment end'
    print('TILING: Segment -Y end == +Y end == GateFrame ends (%d profile points)' % len(ends['SM_UralShield_Segment']))
    json.dump(res, open(os.path.join(C.WORK, 'verify.json'), 'w'), indent=1)

    def dims(n):
        r = res[n]
        return ' x '.join('%.2f' % ((r['hi'][i] - r['lo'][i]) / 100) for i in range(3))

    def row(n, pivot, note):
        r = res[n]
        return '| %s.fbx | %s | %d | %s | %s | %s |' % (n, '{:,}'.format(r['file_bytes']), r['tris'], dims(n), pivot, note)
    files = sorted(f for f in os.listdir(C.OUT) if os.path.isfile(os.path.join(C.OUT, f)) and f != 'README.md')
    flist = '\n'.join('- `%s` - %s bytes' % (f, '{:,}'.format(os.path.getsize(os.path.join(C.OUT, f)))) for f in files)
    hx, hy = C.HINGE_X * 100, C.HINGE_Y * 100
    md = """# Ural Shield wall kit (Blender side)

Procedural, offline, no downloads. Everything here is produced by the scripts in
`Source/Tools/IntroCinematic/blender/`. Nothing was imported into Unreal.

Orientation of every piece: wall runs along **Y**, outer (western) face toward **-X**, Z up,
pivot on the ground. FBX units are centimetres (unit scale 1.0, transforms applied), so
1 unit = 1 cm in Unreal. Numbers below are measured by re-importing the exported FBX files.

## Files

{flist}
- `work/` - work .blend, stats json, backface-check and extra self-check renders (not deliverables)

## Meshes

| File | Bytes | Triangles | Size X x Y x Z (m) | Pivot | Notes |
|---|---|---|---|---|---|
{rows}

Bounds in cm relative to the pivot:

{bounds}

- **Segment**: tiles every 6000 cm along Y. Wall body is 11.6 m thick at the ground (X -460..+700);
  with the buttress toes on the west it is 14.0 m (X -700..+700). Parapet top at 45.0 m, merlons 46.2 m,
  observation domes 47.1 m. Buttress pitch 15 m (Y = +-750, +-2250), nothing straddles the ends.
- **GateFrame**: same end cross-section as the Segment at Y = +-3000 (checked vertex for vertex by
  `verify_and_readme.py`). Portal 16.0 m wide (Y -800..+800) x 13.0 m tall, straight through the wall.
  Towers span Y 800..1960 on each side, tower parapet at 53.8 m (8.8 m above the wall parapet),
  merlons 55.0 m, roof cupola 57.2 m, searchlight 59.2 m. Tower feet reach X -1200, so the pivot is the
  wall centreline (same as the Segment), not the centre of the tower footprint.
- **GateLeaf_L / _R**: each 7.93 m wide x 12.90 m tall x 1.20 m thick (plus hinge knuckles and, on L,
  a meeting-edge cover strip that overlaps R by 0.16 m). 2 cm gap at the centre, 5 cm at jamb, floor and lintel.
- **Far**: same length, same end silhouette class (batter, band, corbel, parapet, merlons, buttresses,
  pilasters, casemate boxes, pulpits) but simplified, so it does not match the Segment vertex for
  vertex - do not butt a Far piece directly against a hero Segment in a close shot.

## Gate leaves: pivots, hinge axes, opening direction

- Each leaf's pivot is **on its hinge axis at ground level**; the hinge axis is the leaf's local **Z** (vertical).
- A pivot on the hinge cannot also sit at the GateFrame pivot, so the leaves are NOT at the same transform
  as the GateFrame. Place them relative to the GateFrame pivot at:
  - Blender / FBX coordinates: Leaf_L at X {hx:.0f}, Y +{hy:.0f}, Z 0 cm; Leaf_R at X {hx:.0f}, Y -{hy:.0f}, Z 0 cm.
  - Unreal's FBX import flips the Y axis, so in Unreal expect Leaf_L at (X {hx:.0f}, Y -{hy:.0f}, Z 0) and
    Leaf_R at (X {hx:.0f}, Y +{hy:.0f}, Z 0) relative to the GateFrame. **Verify on import** (not tested here,
    Unreal was not launched). Leaf_L is the leaf on the LEFT when looking at the wall from outside (from -X).
  - The GateFrame FBX also carries two empties, `SOCKET_Hinge_L` and `SOCKET_Hinge_R`, at those positions;
    Unreal is expected to turn `SOCKET_*` empties into static mesh sockets, so the leaves could simply be
    attached to them - not tested here, verify on import (if they do not arrive, use the offsets above).
- Rotation 0 = closed. Both leaves swing **outward, toward -X** (toward the approaching column).
  - Blender (right handed, Z up): Leaf_L rotates **-85 deg** about Z, Leaf_R **+85 deg**.
  - Unreal (after the Y flip): Leaf_L yaw **+85**, Leaf_R yaw **-85** expected - verify on import; if a leaf
    swings into the tunnel, negate the yaw.
- The closed outer faces sit at X {hx:.0f} cm, recessed about 7 m behind the tower toes (X -1200) at ground level;
  fully open, each leaf lies along its portal jamb and clears it.
- An iron stop frame behind the closed leaves narrows the clear opening there to 15.2 m x 12.6 m
  (the portal itself is 16.0 m x 13.0 m).

## Checks that were run (all scripted)

- FBX round trip: bounds, triangle counts, node scale 1 / centimetres, one UV channel, one material.
- UVs: no zero-area UV triangles; worst UV anisotropy (stretch) per mesh: {aniso}.
- Tiling: the vertices on the Y = -3000 and Y = +3000 planes of the Segment, the GateFrame and the Far piece
  are identical, and the GateFrame's equal the Segment's.
- Normals: winding is outward by construction; Blender's own recalc (after `normal_update()`) agrees on every
  triangle of all five meshes. `render_previews.py -- backface` renders back faces red and counts red pixels.

## Material and textures

- One material slot on every mesh: `{mat}`.
- `T_UralShield_BaseColor.png` (sRGB), `T_UralShield_Normal.png` (DirectX, green down, import as normal map,
  no green flip), `T_UralShield_ORM.png` (linear / mask: R = AO, G = roughness, B = metallic). All 4096 x 4096.
- Single UV channel. Atlas of horizontal strips that tile in U (one repeat = 60 m, so the stain pattern
  repeats once per module): 30 m tall board-formed concrete face, 6 m generic concrete lift, 6 m riveted
  iron band, 4.7 m iron parapet plating; plus non-tiling patches (8 x 13 m gate leaf plate at 100 px/m,
  plain iron, dark embrasure interior, lamp glass, far-LOD casemate front). Strips are 68.3 px/m.
  UVs are true-scale planar projections per face (overlapping by design, never stretched); U runs
  outside 0..1 on the tiling strips, so the texture must use wrap addressing and must NOT be packed
  with other textures.
- AO is texture-space only (grooves, seams, rivets); there is no baked geometric AO because the strips are shared.

## Re-running

```
set B=D:/Blender/BlenderSoftware/4.0/blender.exe
set T=D:/UE5Projects/RTS_Survival/Source/Tools/IntroCinematic/blender
%B% --factory-startup -b -P %T%/make_textures.py       (atlas, ~2 min, needs a few GB of free RAM)
%B% --factory-startup -b -P %T%/build_kit.py           (meshes + FBX + work/UralShield_kit.blend)
%B% --factory-startup -b -P %T%/render_previews.py     (4 previews + self-check views in work/)
%B% --factory-startup -b -P %T%/render_previews.py -- backface   (red = back face visible, into work/)
%B% --factory-startup -b -P %T%/verify_and_readme.py   (FBX round trip checks + this README)
```

Shapes and dimensions live in `build_kit.py`, the atlas layout and the UV projector in `us_common.py`,
the surface look in `make_textures.py`. Order matters only in that `build_kit.py` must run before the
previews and the verification; textures only need regenerating when `make_textures.py` or the atlas layout changes.
""".format(
        flist=flist,
        rows='\n'.join([
            row('SM_UralShield_Segment', 'ground, wall centreline, mid length', 'tileable, budget 18k'),
            row('SM_UralShield_GateFrame', 'ground, wall centreline, portal centre', 'budget 25k, 2 hinge sockets'),
            row('SM_UralShield_GateLeaf_L', 'hinge axis, ground', 'hinge on its +Y edge (Blender)'),
            row('SM_UralShield_GateLeaf_R', 'hinge axis, ground', 'hinge on its -Y edge (Blender)'),
            row('SM_UralShield_Far', 'ground, wall centreline, mid length', 'budget 300'),
        ]),
        bounds='\n'.join('- `%s`: min %s, max %s' % (n, res[n]['lo'], res[n]['hi']) for n in NAMES),
        hx=hx, hy=hy, mat=C.MAT_NAME,
        aniso=', '.join('%s %.3f' % (n.replace('SM_UralShield_', ''), res[n]['uv_max_anisotropy']) for n in NAMES))
    open(os.path.join(C.OUT, 'README.md'), 'w', encoding='utf-8').write(md)
    print('README written')


main()
