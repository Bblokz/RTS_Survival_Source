"""Intro cinematic sets - re-imports every exported FBX to verify it, then writes README.md.

Run:  blender.exe --factory-startup -b -P write_readme.py      (after the build scripts and the renders)
"""
import os
import sys
import json
import numpy as np
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sets_common as C

TRENCH_FAMILY = ['SM_Intro_Trench', 'SM_Intro_TrenchProps', 'SM_Intro_Mould_Mat_A', 'SM_Intro_Mould_Mat_B',
                 'SM_Intro_Mould_Mat_C', 'SM_Intro_Mould_Veins', 'SM_Intro_Mould_Pustules_A', 'SM_Intro_Mould_Pustules_B']
GROWS = {
    'SM_Intro_Mould_Mat_A': 'west wall 30 cm above the floor, beside the crates (blanket over floor + up the wall)',
    'SM_Intro_Mould_Mat_B': 'middle of the fire-step tread, between the two helmets',
    'SM_Intro_Mould_Mat_C': 'east wall-to-floor corner (north of the fire step), middle of its 4 m run',
    'SM_Intro_Mould_Veins': 'floor just north of the open crate (root of the network)',
    'SM_Intro_Mould_Pustules_A': 'floor in the north-east inside corner of the trench',
    'SM_Intro_Mould_Pustules_B': 'foot of the west revetment post at Y = 710',
}
SLOT_TEX = {'Earth': 'T_Intro_Earth', 'Wood': 'T_Intro_Wood', 'Sandbag': 'T_Intro_Sandbag', 'Steel': 'T_Intro_Steel',
            'Mould': 'T_Intro_Mould', 'Ground': 'T_Intro_Crater_Ground', 'CrackGlow': 'T_Intro_Crater_Ground'}


def reimport(name):
    """Returns measured (tris, min cm, max cm, slots, uv layers, signed-volume-free facts) of the FBX."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(C.OUT, name + '.fbx'))
    obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    assert len(obs) == 1, name
    ob = obs[0]
    me = ob.data
    me.calc_loop_triangles()
    co = np.array([ob.matrix_world @ v.co for v in me.vertices]) * 100.0        # importer -> metres; x100 = Unreal cm
    return {'tris': len(me.loop_triangles), 'min': co.min(0), 'max': co.max(0),
            'slots': [m.name.split('.')[0] for m in me.materials], 'uv': len(me.uv_layers),
            'origin': np.array(ob.matrix_world.translation) * 100.0}


def fmt(v):
    return '%.0f, %.0f, %.0f' % (v[0], v[1], v[2])


def size(path):
    b = os.path.getsize(path)
    return '%.2f MB' % (b / 1e6) if b > 1e6 else '%.0f KB' % (b / 1e3)


def main():
    stats = json.load(open(os.path.join(C.WORK, 'trench_stats.json')))
    stats.update(json.load(open(os.path.join(C.WORK, 'crater_stats.json'))))
    meas = {n: reimport(n) for n in TRENCH_FAMILY + ['SM_Intro_CraterTerrain']}
    for n, m in meas.items():
        assert m['tris'] == stats[n]['tris'] and m['uv'] == 1, n
        assert np.abs(m['origin']).max() < 0.01, n                               # pivot at the FBX origin
        assert np.allclose(m['min'], stats[n]['min_cm'], atol=0.6) and np.allclose(m['max'], stats[n]['max_cm'], atol=0.6), n
        print('VERIFIED %-28s tris %6d  size cm %s' % (n, m['tris'], fmt(m['max'] - m['min'])))
    cr = stats['SM_Intro_CraterTerrain']
    L = []
    w = L.append
    w('# Intro cinematic set pieces: 1945 trench (+ props + mould) and nuclear crater terrain')
    w('')
    w('Built offline in Blender 4.0 (headless) by the scripts in `Source/Tools/IntroCinematic/blender/sets/`.')
    w('Nothing was imported into Unreal and Unreal was not launched. All geometry and textures are procedural.')
    w('Numbers below are measured by re-importing the exported FBX files (`write_readme.py`).')
    w('')
    w('FBX conventions: 1 unit = 1 cm in Unreal, scale applied (no object scale), Z up, normals exported, one UV channel,')
    w('triangulated, no collision, no LODs, no sockets. Same exporter settings as the Ural Shield kit.')
    w('Unreal mirrors Y when importing FBX. Every coordinate below is given in Blender/FBX axes; the mould offset table')
    w('also gives the value expected in Unreal (Y negated). That flip is the usual importer behaviour, not tested here.')
    w('')
    w('## Meshes')
    w('')
    w('| File | Size | Triangles | Bounds min (cm) | Bounds max (cm) | Material slots (in order) |')
    w('|---|---|---|---|---|---|')
    for n in TRENCH_FAMILY + ['SM_Intro_CraterTerrain']:
        m = meas[n]
        w('| %s.fbx | %s | %d | %s | %s | %s |' % (n, size(os.path.join(C.OUT, n + '.fbx')), m['tris'], fmt(m['min']), fmt(m['max']),
                                                   ', '.join(m['slots'])))
    w('')
    w('Bounds are relative to each mesh\'s own pivot (the FBX origin).')
    w('')
    w('## Textures')
    w('')
    w('`*_BaseColor` sRGB, `*_Normal` DirectX (green down) linear, `*_ORM` linear with R = AO, G = roughness, B = metallic.')
    w('')
    w('| Slot | Texture set | Size | Notes |')
    w('|---|---|---|---|')
    w('| Earth | T_Intro_Earth_BaseColor / _Normal / _ORM | 2048 | dark wet brown soil, soil band mean sRGB 60,45,32. Atlas: rows 520-2040 = soil that tiles in U (4 m) and in V over its own band (3 m); rows 0-511 = the muddy puddle strip (8 m x 2 m of floor). Must NOT be set to a different tiling in the material: the UVs already repeat. |')
    w('| Wood | T_Intro_Wood_* | 2048 | weathered grey-brown boards, 4 m x 4 m, grain along U, 0.2 m board rows. Shared by the trench and the props. |')
    w('| Sandbag | T_Intro_Sandbag_* | 2048 | dirty hessian weave, 1 m x 1 m, tiles both ways |')
    w('| Steel | T_Intro_Steel_* | 1024 | U 0-0.5 painted helmet steel; U 0.5-1 / V 0.5-1 chipped enamel (mug); U 0.5-1 / V 0-0.5 rusty wire. No markings. |')
    w('| Mould | T_Intro_Mould_* | 2048 | deep purple, mean sRGB 70,25,110, creases about 35,12,60, violet bulges; roughness about 0.19 on bulges, about 0.59 in creases; pores, wrinkles, dark veining. 1 m x 1 m, tiles both ways. Used by all six mould meshes. |')
    w('| Ground | T_Intro_Crater_Ground_* | 4096 | scorched grey-brown fused earth, glassy low-roughness patches on the floor, pale ejecta rays. Unique (not tiling), 3.9 cm per pixel. |')
    w('| CrackGlow | same T_Intro_Crater_Ground_* + T_Intro_Crater_CrackMask | 4096 | the faces inside the grooves. Mask: single greyscale, same UV layout as the whole mesh, white inside the cracks, smooth falloff 16 cm onto the plate edges, black elsewhere. Sample it in BOTH materials if the glow should spill onto the plate edges. |')
    w('')
    for f in sorted(os.listdir(C.OUT)):
        if f.lower().endswith('.png') and f.startswith('T_'):
            pass
    w('Texture files: ' + ', '.join('%s (%s)' % (f, size(os.path.join(C.OUT, f))) for f in sorted(os.listdir(C.OUT))
                                    if f.startswith('T_') and f.endswith('.png')))
    w('')
    w('## Piece 1: trench family')
    w('')
    w('**Transform convention: place every trench-family FBX with the SAME rotation and scale as SM_Intro_Trench.**')
    w('- `SM_Intro_Trench` and `SM_Intro_TrenchProps`: same location as well (they share one pivot and one coordinate frame).')
    w('- The six mould meshes: their pivot is the point they grow from, so each one goes at the trench transform PLUS the')
    w('  offset in the table below (offset in the trench\'s local space). Then scale each about its own pivot from 0 to 1.')
    w('  A static mesh has only one pivot, so "pivot at the growth point" and "drops in at the trench transform with no offset"')
    w('  cannot both hold; I chose the pivot. Easiest in Unreal: make the mould meshes children of the trench actor/component')
    w('  and type the offset as relative location.')
    w('')
    w('Frame: the trench runs along Y, about 24 m long (inner ends at Y = +-1135). **Pivot: on the trench floor centreline at mid')
    w('length, at floor level. Floor z = 0, surrounding ground z = +190** (the brief also said "z = 0 at ground, floor at -190";')
    w('the explicit pivot instruction won, so put the actor 190 cm below your ground level).')
    w('')
    w('| Mould mesh | Offset from the trench pivot, Blender/FBX axes (cm) | Expected relative location in Unreal (cm) | Grows from |')
    w('|---|---|---|---|')
    for n in TRENCH_FAMILY[2:]:
        p = stats[n]['pivot_cm']
        w('| %s | %.1f, %.1f, %.1f | %.1f, %.1f, %.1f | %s |' % (n, p[0], p[1], p[2], p[0], -p[1], p[2], GROWS[n]))
    w('')
    w('What is in SM_Intro_Trench (one mesh, %d triangles of the 40k budget):' % meas['SM_Intro_Trench']['tris'])
    w('- Earth (slot Earth): one height-field surface 28 m x 40 m. Trench cut 1.9 m deep, floor about 1.7 m wide (the walls')
    w('  undulate by about +-10 cm so it varies), walls battered about 30 cm over their height with a varying belly.')
    w('  Fire step on the +X (east) wall, 45 cm high, 50 cm deep, from Y = -800 to Y = +500.')
    w('  Both ends are closed traverses: the north end turns west, the south end turns east, each into a 3 m stub that ends in an earth wall.')
    w('  Apron (model edge at X = +-1400, Y = +-2000): 12.85 m of ground beside the west wall, 12.35 m beside the fire-step wall, about 9.8 m')
    w('  beside the closed end of each 3 m stub (the one place under the asked 12 m), and 8.65 m beyond each end, with')
    w('  spoil heaps behind the parapet, six shell-scrape dips with raised lips, and the outer 3 m raised by up to 0.5 m.')
    w('  The puddle patch is on the floor around Y = -320 (floor dip + wet mud / standing water in the texture).')
    w('- Wood: revetment on the west wall Y -900..-150 and Y 560..860, and on the east wall behind the fire step Y -650..-50:')
    w('  10 cm posts every 1.5 m, 15 cm planks behind them, some missing, some snapped and hanging. Duckboard walkway')
    w('  (runners + slats) from Y -900 to Y +900 with missing and snapped slats; one section has slid into the puddle.')
    w('- Sandbag: 241 bags, parapet 2 to 3 bags high all round both lips and the stubs, with a gap on the west lip where the ladder goes over.')
    w('')
    w('SM_Intro_TrenchProps: closed crate (60 x 40 x 35) and open crate with its lid tipped off, on the floor against the west wall')
    w('(Y 190..330); two steel helmets on the fire step (Y 90 and 150; one upright, one on its side); enamel mug on the closed crate;')
    w('coil of wire standing on the fire step at Y 360; ladder against the west wall at Y -105, top over the lip. No text or markings.')
    w('')
    w('Mould meshes (slot Mould, T_Intro_Mould):')
    w('- Mat_A / _B / _C: single-sided blankets, relaxed so they sag over creases, rim rolled down to the surface, drooping lobes.')
    w('  Measured against the trench mesh: outer skin typically 9 to 11 cm off the surface (median), up to 14 / 18 cm on A / C, rim at about 0.5 cm;')
    w('  6 of 2115 vertices dip up to 1.6 cm below the surface. Open underneath (never visible). Mat_B swells to 43 cm where it buries the two helmets.')
    w('- Veins: 6-sided tubes, about 4 to 8 cm diameter, half sunk into the surface they run on, 7 runners over floor / duckboard / up both walls,')
    w('  plus three rings and one strap wrapped tightly round the two crates.')
    w('- Pustules_A (53 pods, corner) and _B (25 pods, round a post and its foot): closed pods 8 to 20 cm.')
    w('')
    w('## Piece 2: SM_Intro_CraterTerrain')
    w('')
    w('- One mesh, %d triangles (budget 60k), disc 160 m in diameter. **Pivot: centre of the crater floor at floor level (floor z = 0).**' % meas['SM_Intro_CraterTerrain']['tris'])
    w('- Surrounding ground about z = +%.0f cm at 45 m from the centre, falling to about +%.0f cm at 72 m, then the skirt drops 6 m to about +%.0f cm at the 80 m edge (a 45 degree slope, not a vertical wall).' % (
        cr['ground_z_at_45m_cm'], cr['z_at_72m_cm'], cr['edge_z_cm']))
    w('- Rim crest between z = +%.0f and +%.0f cm (2.2 to 3.4 m above the 9 m ground), at 27 to 33 m from the centre (about 60 m rim to rim, irregular). Floor is flat to within a few cm out to 21 m.' % tuple(cr['rim_crest_cm_min_max']))
    w('- Cracks: 81 polygonal plates (Voronoi), each slightly tilted and stepped; 218 grooves modelled as real V grooves, 0.16 to 0.80 m wide')
    w('  (median 0.40) and 0.24 to 1.0 m deep, widest on the radial lines, tapering out toward the edge of the field.')
    w('  "The inner 35 m" was read as a 35 m DIAMETER field (17.5 m radius) in the middle of the 42 m floor.')
    w('- 74 ejecta blocks (0.5 to 3 m, part of the same mesh), low ejecta ridges outside the rim, slump gullies on the inner wall.')
    w('- Slots: Ground (everything), CrackGlow (the %d triangles inside the grooves).' % cr['crack_tris'])
    w('- UVs: one non-overlapping planar top-down unwrap of the whole disc. In Blender/FBX axes (cm):')
    w('  **U = (X + 8000) / 16000, V = (Y + 8000) / 16000**, V measured upward from the bottom row of the texture.')
    w('  After the Unreal import (Y mirrored, V flipped) that should read u = (X + 8000) / 16000, v = (Y + 8000) / 16000 in Unreal')
    w('  world-aligned terms with v measured from the top of the texture; not tested in Unreal.')
    w('')
    w('## Previews (1600 x 900, Cycles)')
    w('')
    w('- preview_trench_inside.png: camera in the trench 1.5 m above the floor looking north along it, low warm sun.')
    w('- preview_trench_above.png: three-quarter view from above.')
    w('- preview_trench_mould.png: inside view with the props and all six mould meshes at full size.')
    w('- preview_crater_low.png: camera 1.6 m above the floor, 20 m from the centre, looking across the cracks to the far rim; faint moon, cracks emissive through the mask.')
    w('- preview_crater_above.png: the whole disc from above, same night lighting.')
    w('- `work/`: the .blend files, extra check views and the back-face debug renders (red = back face; all 14 views count 0 red pixels).')
    w('- The previews are tone-mapped (Filmic) under a strong sun / emissive light, so colours read brighter and more saturated than the texture values.')
    w('')
    w('## Checks that were run')
    w('')
    w('- Winding: every closed shell (bags, planks, posts, crates, helmets, pods, tubes) is oriented by a signed-volume test per connected island.')
    w('  Open surfaces: all earth and crater triangles are asserted to face up; the mould blankets are oriented away from the surface they lie on.')
    w('- Back-face render (emission only, no tone mapping) of every preview and check view: 0 back-face pixels. Positive control:')
    w('  with one mesh deliberately flipped the same counter reports 79k (trench) and 915k (crater) red pixels.')
    w('- Texture means read back from the saved PNGs (Earth soil band 60,45,32; Mould 70,25,109).')
    w('- Triangle budgets, single UV channel, crater UVs inside 0..1, FBX re-import (pivot at origin, bounds, slot order) are asserted.')
    w('')
    w('## How to re-run')
    w('')
    w('```')
    w('cd Source/Tools/IntroCinematic/blender/sets')
    w('B=D:/Blender/BlenderSoftware/4.0/blender.exe')
    w('$B --factory-startup -b -P make_set_textures.py        # Earth, Wood, Sandbag, Steel, Mould  (-- only=Earth,Mould)')
    w('$B --factory-startup -b -P build_trench.py             # trench, props, 6 mould meshes -> FBX + work/IntroTrench.blend')
    w('$B --factory-startup -b -P build_crater.py             # crater mesh + its 4 textures -> FBX + work/IntroCrater.blend  (-- notex)')
    w('$B --factory-startup -b -P render_set_previews.py      # previews  (-- checks | backface | only=preview_trench_inside)')
    w('$B --factory-startup -b -P write_readme.py             # re-import checks + this file')
    w('```')
    w('Everything is seeded, so a re-run reproduces the same meshes. Shared helpers are in sets_common.py.')
    w('')
    with open(os.path.join(C.OUT, 'README.md'), 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(L))
    print('README written')


main()
