"""Intro cinematic - the Purple Death in the 1945 trench (shots 5.2 / 5.3) as baked growth stages.

The engine used to reveal the growth with an opacity mask (distance from the pivot, or along the vein). Wherever that
mask edge crossed geometry standing off the ground it cut a hard slab, a spike or a loose speck, and it cut every
vein off blunt. Here each mould mesh is built at six stages of growth (STAGES, fraction of full size), every one a
complete mesh whose edges lie on the ground and whose veins taper to a point; M_Intro_PurpleDeath shows one stage at
a time from the MPC Mould clock (StageLo / StageHi) with a short organic dissolve between them.

- Mats: the trench's own build_mat at stage scale, subdivided, with real vessels and nodules raised out of the surface
  from mould_field (faded out toward the edge, so the rim stays on the ground). UVs = field coordinates.
- Veins: the network of build_veins grown once (same seed), cut at each stage's length; every vessel tapers root to
  tip and a cut vessel tapers to a point at its cut.
- Pustules: the pods placed once; each stage holds the pods within its share of the cluster's radius.

Run:  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P build_mould_stages.py
Then: python bake_mould_relief.py
Writes SM_Intro_Mould_<piece>_S<n>.fbx to Saved/IntroCinematic/import/Sets.
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_VEINS = os.path.join(HERE, 'build_veins.py')
_src = open(_VEINS, encoding='utf-8').read()
_src = _src[:_src.rindex('\nmain()')]          # the veins' (and through them the trench's) functions, nothing built
exec(compile(_src, _VEINS, 'exec'), globals())

import mould_field as MF  # noqa: E402

# 44 small steps (a hard switch per step, so each must be small enough to read as time-lapse growth, not a jump):
# 14 through 5.2 (Mould 0.05 -> 0.22), 30 through 5.3 (0.22 -> 1.0)
STAGES = (0.05, 0.0625, 0.075, 0.0875, 0.1, 0.1125, 0.125, 0.1375, 0.15, 0.1625, 0.175, 0.1875, 0.2, 0.2125, 0.22, 0.2469, 0.2738, 0.3007, 0.3276, 0.3545, 0.3814, 0.4083, 0.4352, 0.4621, 0.489, 0.5159, 0.5428, 0.5697, 0.5966, 0.6234, 0.6503, 0.6772, 0.7041, 0.731, 0.7579, 0.7848, 0.8117, 0.8386, 0.8655, 0.8924, 0.9193, 0.9462, 0.9731, 1)

MAT_ARGS = {
    'SM_Intro_Mould_Mat_A': lambda: dict(x0=wall_foot(-0.4, -1.0, 4.4), dirx=-1.0, y0=4.4, s_c=0.30, Rs=0.95, Rt=1.10,
                                         T=0.080, seed=301,
                                         lobes=[(2.7, 0.35, 0.14), (3.5, 0.45, 0.12), (1.4, 0.22, 0.2), (4.9, 0.25, 0.2)]),
    'SM_Intro_Mould_Mat_B': lambda: dict(x0=1.10, dirx=1.0, y0=1.2, s_c=-0.08, Rs=0.80, Rt=1.15, T=0.075, seed=311,
                                         lobes=[(3.0, 0.40, 0.13), (3.6, 0.3, 0.10), (2.5, 0.35, 0.12), (1.3, 0.2, 0.2),
                                                (4.9, 0.2, 0.2)],
                                         # just over the helmets (16 cm): 30 / 44 cm domes read as smooth balloons
                                         bumps=[(HELMETS[0][0] - 1.10, HELMETS[0][1] - 1.2, 0.20, 0.25),
                                                (HELMETS[1][0] - 1.10, HELMETS[1][1] - 1.2, 0.24, 0.27)]),
    'SM_Intro_Mould_Mat_C': lambda: dict(x0=wall_foot(0.4, 1.0, 7.6), dirx=1.0, y0=7.6, s_c=0.05, Rs=0.5, Rt=2.0,
                                         T=0.085, seed=321,
                                         lobes=[(2.2, 0.5, 0.10), (3.2, 0.6, 0.08), (4.0, 0.5, 0.10), (0.8, 0.4, 0.10),
                                                (5.6, 0.45, 0.10)], corner=0.05),
}


def stage_name(base, index):
    return '%s_S%d' % (base, index)


# ------------------------------------------------------------------------------------------------------------- mats
def raise_relief(ob, offset, levels):
    """Subdivide the draped sheet and raise vessels and nodules (mould_field) along its normals; fade the relief out
    over the last few rows before the boundary so the rim stays on the ground. Shift the UVs by the mat's offset."""
    mod = ob.modifiers.new('sub', 'SUBSURF')
    mod.levels = levels
    mod.render_levels = levels
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    ob.select_set(False)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.verts.ensure_lookup_table()
    uvl = bm.loops.layers.uv.active
    for f in bm.faces:
        for lp in f.loops:
            lp[uvl].uv = (lp[uvl].uv[0] + offset[0], lp[uvl].uv[1] + offset[1])
    # rows from the boundary (breadth first over edges)
    dist = {v.index: 0 for v in bm.verts if v.is_boundary}
    front = list(dist.keys())
    while front:
        nxt = []
        for i in front:
            for e in bm.verts[i].link_edges:
                o = e.other_vert(bm.verts[i]).index
                if o not in dist:
                    dist[o] = dist[i] + 1
                    nxt.append(o)
        front = nxt
    fade_rows = 3.0 * 2 ** levels                  # a spiky fringe where the relief reached the thin rim
    bm.normal_update()
    verts = [v for v in bm.verts if v.link_loops]
    st = np.array([[sum(lp[uvl].uv[k] for lp in v.link_loops) / len(v.link_loops) for k in (0, 1)] for v in verts]) - 0.5
    fade = np.clip(np.array([dist.get(v.index, 99) for v in verts], float) / fade_rows, 0.0, 1.0)
    fade = fade * fade * (3.0 - 2.0 * fade)
    # proud enough to break the silhouette and shade at camera distance (half this read as a smooth blob)
    rise = (0.026 * MF.vessels(st[:, 0], st[:, 1]) + 0.017 * MF.nodules(st[:, 0], st[:, 1])) * fade * C.CM
    moves = [(v, v.normal.copy() * float(r)) for v, r in zip(verts, rise)]
    for v, d in moves:
        v.co += d
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    for p in ob.data.polygons:
        p.use_smooth = True
    ob.data.update()


def build_mat_stages():
    out = []
    for base, args in MAT_ARGS.items():
        for index, g in enumerate(STAGES):
            a = args()
            # the domes over the helmets grow with the stage (unscaled, the first stage was already two domes)
            bumps = [(bs, bt, ba * g, br) for (bs, bt, ba, br) in a.get('bumps', ())]
            ob = build_mat(stage_name(base, index), a['x0'], a['dirx'], a['y0'], a['s_c'], a['Rs'] * g, a['Rt'] * g,
                           a['T'] * (0.55 + 0.45 * g), a['seed'], a['lobes'], bumps, a.get('corner', 0.0),
                           spokes=128, edge_noise=0.07, lumps=0.7)
            raise_relief(ob, MF.OFFSETS[base], 2 if g >= 0.3 else 1)
            out.append(ob)
    return out


# ------------------------------------------------------------------------------------------------------------ veins
def build_vein_stages():
    random.seed(0)
    rng = random.Random(1945)
    SEGMENTS.clear()
    # the same networks as build_veins.main (same seed and order)
    network(rng, ROOT[0], ROOT[1], 6, 0.018, 3.2)
    network(rng, 1.05, 1.2, 5, 0.017, 2.6)
    network(rng, -0.9, 4.4, 5, 0.017, 2.8)
    network(rng, 0.75, 7.6, 5, 0.016, 2.6)
    network(rng, -0.75, 7.1, 4, 0.015, 2.2)
    network(rng, 0.62, 10.9, 4, 0.015, 2.0)
    network(rng, 0.1, -2.2, 4, 0.012, 1.8)
    paths = []
    longest = 0.0
    for pts, rad, start in SEGMENTS:
        Q = np.array(smooth_path(pts, STEP, 2))
        k = np.linspace(0.0, 1.0, len(Q))
        rr = np.interp(k, np.linspace(0.0, 1.0, len(rad)), rad)
        cum = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(Q, axis=0), axis=1))])
        # every vessel ends in a needle point: only its last 22 cm taper (the whole length thinned the capillaries away);
        # a tip left at a tenth of the radius closed with a flat cap that read as a sliced-off end under the glow
        rr = rr * (0.02 + 0.98 * np.clip((cum[-1] - cum) / 0.22, 0.0, 1.0) ** 0.8)
        paths.append((Q, rr, start, cum, max(rad)))
        longest = max(longest, start + cum[-1])
    out = []
    for index, g in enumerate(STAGES):
        reach = g * longest
        acc = C.Acc(stage_name('SM_Intro_Mould_Veins', index), ['Mould'])
        for Q, rr, start, cum, rmax in paths:
            keep = start + cum <= reach
            n = int(np.sum(keep))
            if n < 3:
                continue
            r = rr[:n].copy()
            if n < len(Q):                                     # cut by this stage: taper to a point at the cut
                left = reach - (start + cum[:n])
                tip = np.clip(left / 0.2, 0.0, 1.0) ** 0.8
                r = r * (0.03 + 0.97 * tip)
            tube(acc, [tuple(p) for p in Q[:n]], list(r), 7 if rmax > 0.009 else 5, 'Mould', uscale=1.0, vspan=1.0,
                 u0=start)
        out.append(acc.build(origin=(ROOT[0], ROOT[1], float(Hm(*ROOT))), smooth_angle=80.0))
    print('VEINS longest %.2f m' % longest)
    return out


# --------------------------------------------------------------------------------------------------------- pustules
PODS = []


def _record_pod(acc, c, n, r):
    """Stands in for build_trench.pod while the clusters are laid out: same random draws, recorded not built."""
    q = rng.uniform(0.85, 1.25)
    u0, v0 = rng.random(), rng.random()
    sy = rng.uniform(0.85, 1.15)
    PODS.append((np.array(c, float), np.array(n, float), r, q, u0, v0, sy))


def _build_pod(acc, c, n, r, q, u0, v0, sy):
    n = n / np.linalg.norm(n)
    a = np.cross(n, (0.0, 0.0, 1.0) if abs(n[2]) < 0.9 else (1.0, 0.0, 0.0))
    a /= np.linalg.norm(a)
    b = np.cross(n, a)
    prof = [(0, -0.5 * r), (0.72 * r, -0.3 * r), (r, 0.15 * r), (0.86 * r, 0.7 * r * q), (0.45 * r, 1.1 * r * q), (0, 1.27 * r * q)]
    revolve(acc, frame(tuple(c), tuple(a), tuple(b), tuple(n)), prof, 8, 'Mould', (u0, v0, u0 + 5.0 * r, v0 + 2.6 * r), sy=sy)


def build_pustule_stages():
    global pod, rng
    real_pod, real_build = pod, C.Acc.build
    origins = {}

    def fake_build(self, origin=(0, 0, 0), smooth_angle=45.0, merge=0.01):
        origins[self.name] = origin
        return None
    pod, C.Acc.build = _record_pod, fake_build
    clusters = {}
    try:
        for base, builder, seed in (('SM_Intro_Mould_Pustules_A', build_pustules_a, 5101),
                                    ('SM_Intro_Mould_Pustules_B', build_pustules_b, 5102)):
            rng = random.Random(seed)
            PODS.clear()
            builder()
            clusters[base] = list(PODS)
    finally:
        pod, C.Acc.build = real_pod, real_build
    out = []
    for base, pods in clusters.items():
        o = np.array(origins[base], float)
        dists = [float(np.linalg.norm((p[0] - o)[:2])) for p in pods]
        far = max(dists)
        for index, g in enumerate(STAGES):
            acc = C.Acc(stage_name(base, index), ['Mould'])
            chosen = [p for p, d in zip(pods, dists) if d <= g * far + 1e-6] or [pods[int(np.argmin(dists))]]
            for p in chosen:
                _build_pod(acc, *p)
            out.append(acc.build(origin=tuple(o), smooth_angle=80.0))
        print('PUSTULES %s pods %d' % (base, len(pods)))
    return out


def main():
    C.setup_scene()
    C.make_material('Mould', 'T_Intro_Mould')
    # optional: -- veins | mats | pustules (rebuild one kind only)
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    obs = ((build_mat_stages() if not only or 'mats' in only else []) +
           (build_vein_stages() if not only or 'veins' in only else []) +
           (build_pustule_stages() if not only or 'pustules' in only else []))
    for ob in obs:
        st = C.ob_stats(ob)
        C.export_fbx(ob)
        print('STAGE %-30s tris %7d  pivot_cm %s' % (ob.name, st['tris'], st['pivot_cm']))


main()
