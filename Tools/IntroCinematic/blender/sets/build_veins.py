"""Intro cinematic - the Purple Death's veins in the 1945 trench (shots 5.2 / 5.3): SM_Intro_Mould_Veins, rebuilt.

Branching networks grow from the places the mould takes hold (the mould mats, the pustule clusters, the crates)
across the floor and up both walls: trunks about 1.8 cm thick that fork again and again down to 3 mm capillaries,
swollen nodules along them, lying on the earth. U of the mesh is the distance from the network's root in
metres, so the engine material reveals the growth outward over time (MPC Mould) without scaling the mesh.

Run (reuses the trench's own surface functions, so the veins sit on the exported trench):
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P build_veins.py
Writes Saved/IntroCinematic/import/Sets/SM_Intro_Mould_Veins.fbx and veins_stats.json (longest reach for the material).
"""
import math
import os
import random
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_TRENCH = os.path.join(HERE, 'build_trench.py')
_src = open(_TRENCH, encoding='utf-8').read()
_src = _src[:_src.rindex('\nmain()')]          # the trench's functions and surface, without building it
exec(compile(_src, _TRENCH, 'exec'), globals())

STEP = 0.022
ROOT = (-0.6, 3.62)                             # pivot of the old veins mesh: TRENCH_MOULD keeps its offset


def s_of(x, y):
    """Signed arc length across the trench section at y, from the centre line (x = 0) to x."""
    xs = np.linspace(0.0, x, 240)
    zs = Hs(xs, np.full_like(xs, y))
    return math.copysign(float(np.sum(np.hypot(np.diff(xs), np.diff(zs)))), x)


def at(s, y):
    return np.array(sec_point(0.0, 1.0, s, y))


SEGMENTS = []                                   # (points, radii, distance at start)


def grow(rng, s, y, ang, r, dist, depth, budget):
    pts, rad = [], []
    start = dist
    nodules = [rng.uniform(0.0, budget) for _ in range(int(budget / 0.35) + 1)]
    travelled = 0.0
    while travelled < budget:
        P = at(s, y)
        n = surf_n(P[0], P[1], 0.05)
        bump = 1.0 + sum(0.85 * math.exp(-((travelled - c) / 0.03) ** 2) for c in nodules)
        bump *= 1.0 + 0.12 * math.sin(travelled * 47.0 + depth)
        # on the surface, not in it: the exported trench is a triangulated grid that rises above the smooth
        # height function in places, and half-sunk veins then break up into floating dashes
        pts.append(P + n * (r * 0.85 + 0.006))
        rad.append(r * bump)
        if depth < 4 and travelled > 0.1 and r > 0.0045 and rng.random() < 0.04:
            side = rng.choice((-1.0, 1.0))
            grow(rng, s, y, ang + side * rng.uniform(0.4, 1.05), r * rng.uniform(0.52, 0.72), dist, depth + 1,
                 (budget - travelled) * rng.uniform(0.45, 0.85) + 0.15)
        ang += rng.gauss(0.0, 0.17)
        if abs(s) > 0.95:                       # on a wall: it creeps along rather than straight up
            along = math.pi / 2 if math.sin(ang) >= 0.0 else -math.pi / 2
            ang += 0.16 * math.atan2(math.sin(along - ang), math.cos(along - ang))
        s += math.cos(ang) * STEP
        y += math.sin(ang) * STEP
        travelled += STEP
        dist += STEP
        r *= 0.994
        if r < 0.0026 or abs(s) > 1.85 or y < -4.6 or y > 11.25:
            break
    if len(pts) >= 3:
        SEGMENTS.append((pts, rad, start))


def network(rng, x, y, trunks, r0, reach):
    s = s_of(x, y)
    for k in range(trunks):
        ang = 2 * math.pi * k / trunks + rng.uniform(-0.4, 0.4)
        grow(rng, s, y, ang, r0 * rng.uniform(0.8, 1.15), 0.0, 0, reach * rng.uniform(0.7, 1.15))


def main():
    rng = random.Random(1945)
    C.setup_scene()
    C.make_material('Mould', 'T_Intro_Mould')
    # Where it took hold (Blender trench coordinates): the crates, the three mats, the two pustule clusters.
    network(rng, ROOT[0], ROOT[1], 6, 0.018, 3.2)
    network(rng, 1.05, 1.2, 5, 0.017, 2.6)        # east wall at the helmets (mat B)
    network(rng, -0.9, 4.4, 5, 0.017, 2.8)        # west wall foot (mat A)
    network(rng, 0.75, 7.6, 5, 0.016, 2.6)        # east wall (mat C)
    network(rng, -0.75, 7.1, 4, 0.015, 2.2)        # revetment post (pustules B)
    network(rng, 0.62, 10.9, 4, 0.015, 2.0)       # north corner (pustules A)
    network(rng, 0.1, -2.2, 4, 0.012, 1.8)        # first runners toward the camera end
    acc = C.Acc('SM_Intro_Mould_Veins', ['Mould'])
    longest = 0.0
    for pts, rad, start in SEGMENTS:
        Q = smooth_path(pts, STEP, 2)
        # radii resampled onto the smoothed path by index fraction
        k = np.linspace(0.0, 1.0, len(Q))
        rr = np.interp(k, np.linspace(0.0, 1.0, len(rad)), rad)
        sides = 7 if max(rad) > 0.009 else 5
        tube(acc, Q, list(rr), sides, 'Mould', uscale=1.0, vspan=1.0, u0=start)
        length = float(np.sum(np.linalg.norm(np.diff(np.array(Q), axis=0), axis=1)))
        longest = max(longest, start + length)
    ob = acc.build(origin=(ROOT[0], ROOT[1], float(Hm(*ROOT))), smooth_angle=80.0)
    st = C.ob_stats(ob)
    C.export_fbx(ob)
    st['segments'] = len(SEGMENTS)
    st['longest_m'] = round(longest, 3)
    C.write_json('veins_stats.json', st)
    print('VEINS segments %d tris %d longest %.2f m' % (len(SEGMENTS), st['tris'], longest))


main()
