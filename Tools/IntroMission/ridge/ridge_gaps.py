"""Second cliff pass on Ger_IntroMission's base ridge (plain python + numpy, no Unreal). 2026-11-19.

User (with a screenshot of a steep terrace edge that had rocks only on part of it): "make sure that on the cliff of the
picture we add cliff rocks all around it". This pass adds Ore kit rocks to every steep face around the ridge that has
none yet, without changing the terrain: the rim stretches the first pass kept for nearby props, the lower terraces,
the mound flanks.

How: contours of the current terrain (terrain/ridge_full_height.r16, what ridge_apply put on the map) every metre of
height; every metre along a contour where the ground is steeper than FACE_DEG becomes a station; its downhill profile
gives the lip and foot (ock _run), and the station only counts where the contour lies near the face's mid-height (so each
face is found once). Stations already within COVERED_CM of a rock of the first pass are skipped. Runs of stations become
face segments for the same IntroLayout as the rear ridges. Pieces never reach into gameplay actors, roads, the truck
lane or the counterattack climb.

    python ridge_gaps.py   -> Saved/RidgeRework/Ridge_cliffs.json gains the new pieces ('pass': 2), plan png
"""
import json
import math
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'rear'))
import ridge_gen as g  # noqa: E402
import rear_cliffs_gen as rcg  # noqa: E402

AREA = (-9200.0, 9400.0, -9800.0, 7800.0)      # the ridge, its terraces and the mound (x0, x1, y0, y1)
FACE_DEG = 30.0
MIN_FACE_M = 1.5
COVERED_CM = 650.0
MID_TOL_M = 0.6
KEEP_CLEAR = [((5600.0, -1300.0), 1500.0)]      # the east climb from the hub (road D / counterattack): no rocks


class GapLayout(rcg.IntroLayout):
    """IntroLayout whose face row also covers the low terrace faces (1.5-2.5 m): MassLow pieces scaled to the face."""
    def face_row(self, face, seg):
        rng, L, P = self.rng, seg['length'], rcg.ock_place.PROUD
        s, last = (0.5 if seg.get('join0') else rng.uniform(2.0, 4.0)), None
        while s < L - (1.0 if seg.get('join1') else 4.0):
            h = min(rcg.ock_map.at(seg, min(L, s + t))['h'] for t in (0.0, 2.0, 4.0, 6.0))
            if h >= 5.5:
                names, dout, dz, sc = ['Mass10', 'Mass12', 'Mass8'], P, 0.0, 1.0
            elif h >= 1.4:
                names, dout, dz = ['MassLow10', 'MassLow7'], P - 0.2, 0.3
                sc = float(np.clip(h / 2.8, 0.6, 1.0))
            else:
                s += 2.0
                continue
            pick = [n for n in names if n != last] or names
            name = rng.choice(pick)
            w = rcg.ock_place.PIECES[name]['spacing_cm'] / 100.0 * sc
            self.add(name, face, rcg.ock_map.at(seg, min(L, s + w / 2)), dout=dout * sc + rng.uniform(-0.2, 0.2),
                     dz=dz * sc, yaw=rng.uniform(-4, 4), scale=sc * rng.uniform(0.97, 1.05))
            last = name
            s += w * 0.93


def footprint_boxes():
    """Gameplay boxes for this pass: bunkers / buildings shrink to their core (camo nets and sandbags may sit under
    rocks), no extra margin."""
    out = []
    for (x0, x1, y0, y1, cls) in g.gameplay_boxes():
        if cls.startswith('Rus_BXP') or cls.startswith('Rus_Building'):
            cx, cy, ex, ey = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2 * 0.6, (y1 - y0) / 2 * 0.6
            out.append((cx - ex, cx + ex, cy - ey, cy + ey, cls))
        else:
            out.append((x0, x1, y0, y1, cls))
    return out


def first_pass():
    doc = json.load(open(g.OUT + 'Ridge_cliffs.json'))
    old = [it for it in doc['instances'] if it.get('pass', 1) == 1]
    return doc, old


def bridge(st, max_gap=3):
    """Fills gaps of up to max_gap stations between two found ones (a short dip in slope must not split a face)."""
    out = list(st)
    k = 0
    while k < len(out):
        if out[k] is not None:
            k += 1
            continue
        j = k
        while j < len(out) and out[j] is None:
            j += 1
        if 0 < k and j < len(out) and j - k <= max_gap:
            a, b = out[k - 1], out[j]
            for t in range(k, j):
                f = (t - k + 1) / (j - k + 1)
                out[t] = {key: tuple(a[key][c] * (1 - f) + b[key][c] * f for c in range(3)) for key in ('lip', 'foot')}
        k = j
    return out


def stations(Z, old):
    import contourpy
    X, Y = g.grid_xy()
    xs, ys = X[0, :], Y[:, 0]
    m = (xs >= AREA[0]) & (xs <= AREA[1])
    n = (ys >= AREA[2]) & (ys <= AREA[3])
    sub = Z[np.ix_(n, m)]
    gy, gx = np.gradient(Z, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    gen = contourpy.contour_generator(xs[m], ys[n], sub, line_type='Separate')
    old_xy = np.array([[it['x'], it['y']] for it in old]) if old else np.zeros((0, 2))
    ds = np.arange(-1200.0, 1201.0, 100.0)
    lines = []
    for level in np.arange(180.0, 1300.0, 100.0):
        for seg in gen.lines(level):
            if len(seg) < 8:
                continue
            d = np.r_[0, np.cumsum(np.hypot(np.diff(seg[:, 0]), np.diff(seg[:, 1])))]
            s = np.arange(0.0, d[-1], 100.0)
            pts = np.column_stack([np.interp(s, d, seg[:, 0]), np.interp(s, d, seg[:, 1])])
            st = []
            for p in pts:
                ok = g.bilinear(slope, p[0], p[1]) > FACE_DEG
                if ok and len(old_xy):
                    ok = np.hypot(old_xy[:, 0] - p[0], old_xy[:, 1] - p[1]).min() > COVERED_CM
                if ok:
                    ok = all(math.hypot(p[0] - c[0], p[1] - c[1]) > r for c, r in KEEP_CLEAR)
                if not ok:
                    st.append(None)
                    continue
                gxv, gyv = g.bilinear(gx, p[0], p[1]), g.bilinear(gy, p[0], p[1])
                nrm = -np.array([gxv, gyv]) / (math.hypot(gxv, gyv) + 1e-9)       # downhill
                px, py = p[0] + nrm[0] * ds, p[1] + nrm[1] * ds
                zs = g.bilinear(Z, px, py) / 100.0
                run = rcg.ock_map.Map._run(None, zs, np.ones(len(zs), bool), 0.5)
                if run is None:
                    st.append(None)
                    continue
                a, b = run
                h = zs[a] - zs[b]
                if h < MIN_FACE_M or abs(level / 100.0 - (zs[a] + zs[b]) / 2.0) > MID_TOL_M:
                    st.append(None)
                    continue
                st.append(dict(lip=(px[a] / 100.0, py[a] / 100.0, float(zs[a])), foot=(px[b] / 100.0, py[b] / 100.0, float(zs[b]))))
            st = bridge(st)
            closed = np.hypot(*(seg[0] - seg[-1])) < 150.0
            lines.extend(rcg.ock_map.Map._segments(None, st, closed=closed))
    for seg in lines:
        seg['open0'] = seg['open1'] = True
    # one face can be found by two neighbouring levels: drop a segment whose lips mostly lie on an earlier one
    keep = []
    for seg in sorted(lines, key=lambda s: -s['length']):
        lip = seg['lip'][:, :2]
        dup = False
        for k in keep:
            dd = np.hypot(lip[:, None, 0] - k['lip'][None, :, 0], lip[:, None, 1] - k['lip'][None, :, 1]).min(axis=1)
            if (dd < 3.0).mean() > 0.5:
                dup = True
                break
        if not dup:
            keep.append(seg)
    return keep


def main():
    full = np.fromfile(g.T + 'ridge_full_height.r16', np.uint16).reshape(g.SY, g.SX)
    Z = g.to_z(full)
    doc, old = first_pass()
    segs = stations(Z, old)
    print('new face segments', len(segs), 'total %.0f m' % sum(s['length'] for s in segs))
    boxes = footprint_boxes()
    lay = GapLayout(rcg.IntroMap(Z), seed=1119)
    for seg in segs:
        lay.crag_face('gap', seg)
    lay.inst = rcg.drop_on_roads(lay.inst)
    a, b = np.array([4640.0, -5400.0]), np.array([4640.0, -1500.0])
    new, dropped = [], 0
    for it in lay.inst:
        it['fp'] = rcg.plan_points(it)
        it['pass'] = 2
        bad = any(bx[0] <= x <= bx[1] and bx[2] <= y <= bx[3] for (x, y) in it['fp'] for bx in boxes)
        if not bad:
            for (x, y) in it['fp']:
                t = np.clip(((x - a[0]) * (b - a)[0] + (y - a[1]) * (b - a)[1]) / ((b - a) @ (b - a)), 0, 1)
                if math.hypot(x - (a[0] + t * (b - a)[0]), y - (a[1] + t * (b - a)[1])) < 500.0:
                    bad = True
                    break
        if bad:
            dropped += 1
            continue
        new.append(it)
    counts = {}
    for it in new:
        counts[it['piece']] = counts.get(it['piece'], 0) + 1
    print('new pieces', len(new), dict(sorted(counts.items())), 'dropped near actors/lane', dropped)
    doc['instances'] = old + new
    doc['total_instances'] = len(doc['instances'])
    allc = {}
    for it in doc['instances']:
        allc[it['piece']] = allc.get(it['piece'], 0) + 1
    doc['counts'] = allc
    json.dump(doc, open(g.OUT + 'Ridge_cliffs.json', 'w'), separators=(',', ':'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    X, Y = g.grid_xy()
    gy, gx = np.gradient(Z, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    xs, ys = X[0, :], Y[:, 0]
    mm = (xs >= AREA[0]) & (xs <= AREA[1])
    nn = (ys >= AREA[2]) & (ys <= AREA[3])
    fig, ax = plt.subplots(figsize=(14, 13))
    ax.imshow(slope[np.ix_(nn, mm)], origin='lower', extent=(xs[mm][0], xs[mm][-1], ys[nn][0], ys[nn][-1]),
              cmap='gray', vmin=0, vmax=60)
    ax.scatter([it['x'] for it in old], [it['y'] for it in old], s=6, c='yellow', label='first pass')
    ax.scatter([it['x'] for it in new], [it['y'] for it in new], s=6, c='cyan', label='this pass')
    ax.legend()
    ax.set_title('Base ridge rocks: slope (white = steep), yellow = first pass, cyan = added now')
    plt.tight_layout()
    plt.savefig(g.OUT + 'ridge_gaps_plan.png', dpi=60)


if __name__ == '__main__':
    main()
