"""Rock faces on the two rear ridges of Ger_IntroMission (plain python + numpy, no Unreal).

User request 2026-11-17: remove the small stones and decals on the station ridge and depot ridge flanks and "add proper
cliffs instead like we also have on the Ore mission", on both ridges, all flanks. The cacti on the ridge tops stay.
Later the same day: the station ridge's east flank and its south nose by the buffer stop as well.

What it does:
1. Reads the current landscape (Saved/IntroRear/terrain/cliffpre_full_height.r16, exported by rear_lib.export_region).
2. Steepens each flank into a rock face. Per spine station (one per metre, per side) it reads the crest and foot
   heights of the flank, smooths them along the ridge, and remaps every flank vertex by its height fraction q:
   q' = 0.9 * clip((q - 0.5) * k + 0.5) + 0.1 * q. The face stays where the flank's mid-height contour was, the crest
   and foot get a gentle lip and talus toe, and the middle goes above 60 degrees (nav agents stop at 44).
   Not touched: road corridors and the depot saddle, ponds, the depot pad, the station ridge's east flank, the north
   embankment and anything under 3 m high (ridge ends).
3. Finds the new faces (lip / foot lines) and lays the Ore cliff kit (kit C crag pieces, OreCliffKit) along them
   with the kit's own Layout class (ock_place.py in the Blender project).
Writes:
   terrain/cliffs_full_height.r16                     full grid, for rear_lib.import_heights('cliffs_full', GRID)
   cliffs/Intro_cliffs.json                           cliff instances (cm, Unreal rotator) for rear_build.step_cliffs
   cliffs/cliff_masks.npz                             weight, height change and face footprint (for clearing props)
   cliffs/cliffs_plan.png                             preview
"""
import json
import math
import os
import random
import sys

import numpy as np

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rear_terrain_gen as rtg  # noqa: E402

KIT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCliffKit/Model'
sys.path.insert(0, KIT)
import ock_map  # noqa: E402
import ock_place  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/'
T = ROOT + 'terrain/'
OUT = ROOT + 'cliffs/'
GRID = rtg.GRID
ORIGIN = rtg.ORIGIN
SY, SX = GRID[3] - GRID[1] + 1, GRID[2] - GRID[0] + 1

RIDGES = {
    # k = how much steeper the middle of the flank gets; reach = how far out from the spine the flank can go (cm)
    # station: west flank (base side) and, since 2026-11-17 later, the east flank down towards the map edge (a gentle
    # 25 degree slope over 26 m, so it needs a much larger k); start=13 leaves the cells south of y ~4800 to the nose
    'station': dict(spine=rtg.RIDGE_S['spine'], k={-1: 3.6, 1: 5.5}, reach={-1: 4200.0, 1: 5000.0}, sides=(-1, 1), start=6,
                    label={-1: 'w', 1: 'e'}),
    # the station ridge's south nose by the buffer stop (user 2026-11-17: "add the cliffs at the end here"): a straight
    # line across the ridge top whose +1 side points south
    'nose': dict(spine=[(18400.0, 4900.0), (20000.0, 4900.0), (21500.0, 4900.0), (23600.0, 4900.0)], k=3.6, reach=3400.0,
                 sides=(1,), label={1: 's'}),
    'depot': dict(spine=rtg.RIDGE_D['spine'], k=2.2, reach=3200.0, sides=(-1, 1), label={-1: 'w', 1: 'e'}),
}


def per_side(value, side):
    return value[side] if isinstance(value, dict) else value
MIN_H = 300.0          # flanks lower than this (cm) stay as they are
FULL_H = 500.0         # full effect from this height
STATION_NORTH_Y = 22600.0   # the rails come down the embankment north of here: keep it open


def smooth(t):
    return rtg.smooth(t)


def resample(points, step):
    p = rtg.catmull(points, 100.0)
    seg = np.r_[0, np.cumsum(np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1])))]
    s = np.arange(0.0, seg[-1], step)
    return np.column_stack([np.interp(s, seg, p[:, 0]), np.interp(s, seg, p[:, 1])]), s


def bilinear(Z, x, y):
    fi = (np.asarray(x) - ORIGIN[0]) / 100.0 - GRID[0]
    fj = (np.asarray(y) - ORIGIN[1]) / 100.0 - GRID[1]
    fi = np.clip(fi, 0, SX - 1.001)
    fj = np.clip(fj, 0, SY - 1.001)
    i0, j0 = np.floor(fi).astype(int), np.floor(fj).astype(int)
    tx, ty = fi - i0, fj - j0
    return ((Z[j0, i0] * (1 - tx) + Z[j0, i0 + 1] * tx) * (1 - ty) + (Z[j0 + 1, i0] * (1 - tx) + Z[j0 + 1, i0 + 1] * tx) * ty)


def median_filter_1d(a, r):
    pad = np.pad(a, r, mode='edge')
    return np.array([np.median(pad[i:i + 2 * r + 1]) for i in range(len(a))])


def box_1d(a, r):
    pad = np.pad(a, r, mode='edge')
    return np.convolve(pad, np.ones(2 * r + 1) / (2 * r + 1), mode='valid')


def station_profiles(Z, sp, normals, side, reach):
    """Crest and foot height per spine station on one side (cm)."""
    ds = np.arange(0.0, reach + 1, 50.0)
    px = sp[:, 0, None] + side * normals[:, 0, None] * ds[None, :]
    py = sp[:, 1, None] + side * normals[:, 1, None] * ds[None, :]
    prof = bilinear(Z, px, py)
    # 5 m box along the profile so a crater or gully does not set the foot
    k = 5
    kern = np.ones(2 * k + 1) / (2 * k + 1)
    prof_s = np.array([np.convolve(np.pad(r, k, mode='edge'), kern, mode='valid') for r in prof])
    crest = prof_s.max(axis=1)
    foot = prof_s.min(axis=1)
    crest = median_filter_1d(crest, 7)
    foot = median_filter_1d(foot, 7)
    return box_1d(crest, 4), box_1d(foot, 4)


def exclusion_weight(X, Y):
    """1 = may become cliff, 0 = keep as it is (roads, saddle, ponds, depot pad, keep-outs)."""
    w = np.ones_like(X)
    for name, pts in rtg.ROADS.items():
        poly = rtg.catmull(pts, 100.0)
        d, _ = rtg.dist_to_poly(X, Y, poly)
        w = np.minimum(w, smooth((d - rtg.ROAD_HALF - 250.0) / 450.0))
    sad = np.hypot(X - rtg.SADDLE_D[0], Y - rtg.SADDLE_D[1])
    w = np.minimum(w, smooth((sad - 2600.0) / 1200.0))
    for pd in rtg.PONDS.values():
        c = pd['centre']
        w = np.minimum(w, smooth((np.hypot(X - c[0], Y - c[1]) - (pd['rx'] + 700.0)) / 800.0))
    dc = rtg.DEPOT['centre']
    a = math.radians(rtg.DEPOT['yaw'])
    u = (X - dc[0]) * math.cos(a) + (Y - dc[1]) * math.sin(a)
    v = -(X - dc[0]) * math.sin(a) + (Y - dc[1]) * math.cos(a)
    hu = rtg.DEPOT['tiles'][0] * rtg.DEPOT['tile'] / 2 + 600.0
    hv = rtg.DEPOT['tiles'][1] * rtg.DEPOT['tile'] / 2 + 600.0
    pad_d = np.hypot(np.maximum(np.abs(u) - hu, 0), np.maximum(np.abs(v) - hv, 0))
    w = np.minimum(w, smooth((pad_d - 1500.0) / 800.0))
    for (x0, x1, y0, y1) in rtg.KEEP_OUT:
        dx = np.maximum(np.maximum(x0 - X, X - x1), 0)
        dy = np.maximum(np.maximum(y0 - Y, Y - y1), 0)
        w = np.minimum(w, smooth((np.hypot(dx, dy) - 300.0) / 800.0))
    w = np.minimum(w, smooth((X - 5000.0) / 800.0))
    w = np.minimum(w, smooth((26500.0 - X) / 600.0))    # the station's east face reaches the playable edge (x 24775)
    return w


def steepen(Z0):
    I, J = np.meshgrid(np.arange(SX) + GRID[0], np.arange(SY) + GRID[1])
    X = ORIGIN[0] + I * 100.0
    Y = ORIGIN[1] + J * 100.0
    Z = Z0.copy()
    excl = exclusion_weight(X, Y)
    weight = np.zeros_like(Z)
    info = {}
    for name, R in RIDGES.items():
        sp, s = resample(R['spine'], 100.0)
        tan = np.gradient(sp, axis=0)
        tan /= np.linalg.norm(tan, axis=1)[:, None]
        nrm = np.column_stack([tan[:, 1], -tan[:, 0]])          # points to +X for a spine running +Y
        # cells near this ridge: nearest station by brute force in chunks
        band = np.zeros_like(Z, bool)
        rmax = max(per_side(R['reach'], sd) for sd in R['sides'])
        x0, x1 = sp[:, 0].min() - rmax - 200, sp[:, 0].max() + rmax + 200
        y0, y1 = sp[:, 1].min() - rmax - 200, sp[:, 1].max() + rmax + 200
        band = (X >= x0) & (X <= x1) & (Y >= y0) & (Y <= y1)
        cj, ci = np.nonzero(band)
        cx, cy = X[cj, ci], Y[cj, ci]
        near = np.empty(len(cx), int)
        dist = np.empty(len(cx))
        for a0 in range(0, len(cx), 4000):
            dx = cx[a0:a0 + 4000, None] - sp[None, :, 0]
            dy = cy[a0:a0 + 4000, None] - sp[None, :, 1]
            d2 = dx * dx + dy * dy
            k = np.argmin(d2, axis=1)
            near[a0:a0 + 4000] = k
            dist[a0:a0 + 4000] = np.sqrt(d2[np.arange(len(k)), k])
        side_of = np.sign((cx - sp[near, 0]) * nrm[near, 0] + (cy - sp[near, 1]) * nrm[near, 1])
        prof = {sd: station_profiles(Z0, sp, nrm, sd, per_side(R['reach'], sd)) for sd in R['sides']}
        info[name] = dict(sp=sp, s=s, nrm=nrm, prof=prof)
        for sd in R['sides']:
            crest, foot = prof[sd]
            m = (side_of == sd) & (dist <= per_side(R['reach'], sd))
            # not at the ends of the spine (the ridge noses run out past them; the station nose has its own line)
            m &= (near > 3) & (near < len(sp) - 4) if name == 'depot' else (near >= R.get('start', 0))
            j, i = cj[m], ci[m]
            c, f = crest[near[m]], foot[near[m]]
            H = c - f
            q = np.clip((Z0[j, i] - f) / np.maximum(H, 1.0), 0.0, 1.0)
            qn = 0.9 * np.clip((q - 0.5) * per_side(R['k'], sd) + 0.5, 0.0, 1.0) + 0.1 * q
            znew = f + qn * H
            # the top stays where it is: never cut the upper flank or the ridge top (rail bed, buffer stop)
            znew = np.where(q > 0.85, np.maximum(znew, Z0[j, i]), znew)
            wgt = smooth((H - MIN_H) / (FULL_H - MIN_H)) * excl[j, i]
            if name == 'station':
                wgt = wgt * smooth((STATION_NORTH_Y - Y[j, i]) / 1200.0)
            # where two flanks overlap (the nose corners) the stronger one wins
            win = wgt >= weight[j, i]
            j, i, wgt, znew = j[win], i[win], wgt[win], znew[win]
            Z[j, i] = Z0[j, i] + wgt * (znew - Z0[j, i])
            weight[j, i] = wgt
    return X, Y, Z, weight, info


# ---- face finding on the new terrain and the kit layout ------------------------------------------------------------
class IntroMap:
    """Duck-typed ock_map.Map for the Layout class: metres, Unreal world axes."""
    def __init__(self, Z):
        self.Zcm = Z

    def z(self, x, y):
        return float(bilinear(self.Zcm, x * 100.0, y * 100.0)) / 100.0

    def feature(self, fid):
        return None


def faces(Znew, info, weight):
    out = {}
    for name, R in RIDGES.items():
        sp, nrm = info[name]['sp'], info[name]['nrm']
        for sd in R['sides']:
            ds = np.arange(0.0, per_side(R['reach'], sd) + 1, 100.0)
            st = []
            for k in range(len(sp)):
                px = sp[k, 0] + sd * nrm[k, 0] * ds
                py = sp[k, 1] + sd * nrm[k, 1] * ds
                zs = bilinear(Znew, px, py) / 100.0
                wv = bilinear(weight, px, py)
                ok = wv > 0.6
                run = ock_map.Map._run(None, zs, ok, 1.0)
                if run is None or zs[run[0]] - zs[run[1]] < 2.5:
                    st.append(None)
                    continue
                a, b = run
                st.append(dict(lip=(px[a] / 100.0, py[a] / 100.0, float(zs[a])), foot=(px[b] / 100.0, py[b] / 100.0, float(zs[b]))))
            segs = ock_map.Map._segments(None, st, closed=False)
            for seg in segs:
                seg['open0'] = seg['open1'] = True
            out['%s_%s' % (name, R['label'][sd])] = segs
    return out


class IntroLayout(ock_place.Layout):
    """Kit C along a face, sized to these ridges: one tier for 6-10.5 m, MassLow below, two tiers above."""
    def face_row(self, face, seg):
        """Face pieces side by side, each sized to the local face height: Mass (10 m) from 5.5 m, MassLow (6.8 m) from
        2.5 m, nothing lower (the ends of a face run out as scree and chunks, no flat end slabs)."""
        rng, L, P = self.rng, seg['length'], ock_place.PROUD
        # the last metres of a face bend and run out: chunks and scree only, unless the face joins another one there
        s, last = (0.5 if seg.get('join0') else rng.uniform(3.0, 5.0)), None
        while s < L - (1.0 if seg.get('join1') else 7.0):
            # the lowest point under the whole piece decides, so a tall piece never lies on a face that runs out
            h = min(ock_map.at(seg, min(L, s + t))['h'] for t in (0.0, 3.0, 6.0, 9.0))
            if h >= 5.5:
                names, dout, dz = ['Mass10', 'Mass12', 'Mass8'], P, 0.0
            elif h >= 2.5:
                names, dout, dz = ['MassLow10', 'MassLow7'], P - 0.2, 0.3
            else:
                s += 2.0
                continue
            pick = [n for n in names if n != last] or names
            name = rng.choice(pick)
            w = ock_place.PIECES[name]['spacing_cm'] / 100.0
            self.add(name, face, ock_map.at(seg, min(L, s + w / 2)), dout=dout + rng.uniform(-0.25, 0.25), dz=dz,
                     yaw=rng.uniform(-4, 4), scale=rng.uniform(0.97, 1.05))
            last = name
            s += w * 0.93

    def crag_face(self, face, seg, skip=()):
        rng, L = self.rng, seg['length']
        P = ock_place.PROUD
        hh = seg['lip'][:, 2] - seg['foot'][:, 2]
        med = float(np.median(hh))
        tall = med > 6.0
        self.face_row(face, seg)
        # crest: a few broken lip blocks sunk into the top, irregular, never a row of teeth
        s = rng.uniform(2.0, 6.0)
        while s < L - 2.0:
            st = ock_map.at(seg, s)
            if st['h'] >= 3.0 and rng.random() < 0.55:
                self.add(rng.choice(['Lip6', 'Lip10', 'Lip6']), face, st, dout=P + rng.uniform(-0.2, 0.7), dz=-rng.uniform(0.8, 1.5),
                         yaw=rng.uniform(-25, 25), roll=rng.uniform(-5, 5), scale=rng.uniform(0.75, 1.0))
            s += rng.uniform(7.0, 14.0)
        low = []
        foot_med = float(np.median(seg['foot'][:, 2]))
        for k in range(len(seg['s'])):
            if hh[k] < 3.5 or seg['foot'][k, 2] > foot_med + 2.0:      # no apron lying up a slope where a face ends
                low.append((seg['s'][k] - 1.0, seg['s'][k] + 1.0))
        self.row(face, seg, ['Scree9', 'Scree5'] if tall else ['Scree5'], 0.0, L, low, anchor='foot', dout=P * 0.5 + 0.6,
                 dz=-0.5, jit=0.5)
        s = rng.uniform(5, 12)
        while s < L - 4:
            st = ock_map.at(seg, s)
            if rng.random() < 0.5 or not tall:
                self.add(rng.choice(['Chunk3', 'Chunk3', 'Chunk4']), face, st, dout=P + rng.uniform(-1.2, 0.4), dz=rng.uniform(-0.3, 0.3),
                         yaw=rng.uniform(-180, 180), pitch=rng.uniform(-6, 6), roll=rng.uniform(-6, 6), scale=rng.uniform(0.9, 1.15))
            else:
                dzz = -rng.uniform(2.5, max(3.0, med - 2.0))
                self.add(rng.choice(['Chunk4', 'Chunk6']), face, st, dout=P + ock_place.SLOPE * (-dzz) + 0.9, dz=dzz,
                         yaw=rng.uniform(-180, 180), pitch=rng.uniform(-15, 15), roll=rng.uniform(-15, 15), scale=rng.uniform(0.85, 1.1))
            if rng.random() < 0.6:
                self.add('Block2', face, ock_map.at(seg, s + rng.uniform(-4, 4)), anchor='foot', dout=rng.uniform(2.5, 5.5), dz=0.35,
                         yaw=rng.uniform(-180, 180), pitch=rng.uniform(-20, 20), roll=rng.uniform(-20, 20), scale=rng.uniform(0.8, 1.3),
                         ground=True)
            s += rng.uniform(13, 22) if tall else rng.uniform(16, 26)


def plan_points(it, nx=4, ny=5):
    """Plan-view sample points (cm) over a piece's bounds, for overlap tests here and in the editor."""
    b = ock_place.PIECES[it['piece']]['bounds_cm']
    a = math.radians(it['yaw'])
    ca, sa = math.cos(a), math.sin(a)
    sc = it['scale']
    pts = []
    for lx in np.linspace(b['min'][0], b['max'][0], nx):
        for ly in np.linspace(b['min'][1], b['max'][1], ny):
            pts.append((round(it['x'] + (lx * ca - ly * sa) * sc), round(it['y'] + (lx * sa + ly * ca) * sc)))
    return pts


def drop_on_roads(inst):
    """Remove pieces whose plan footprint reaches within 1.5 m of a road's driven width."""
    roads = [rtg.catmull(pts, 100.0) for pts in rtg.ROADS.values()]
    keep = []
    for it in inst:
        b = ock_place.PIECES[it['piece']]['bounds_cm']
        a = math.radians(it['yaw'])
        ca, sa = math.cos(a), math.sin(a)
        sc = it['scale']
        pts = []
        for lx in np.linspace(b['min'][0], b['max'][0], 4):
            for ly in np.linspace(b['min'][1], b['max'][1], 5):
                pts.append((it['x'] + (lx * ca - ly * sa) * sc, it['y'] + (lx * sa + ly * ca) * sc))
        pts = np.array(pts)
        bad = False
        for poly in roads:
            d, _ = rtg.dist_to_poly(pts[:, 0], pts[:, 1], poly)
            if d.min() < rtg.ROAD_HALF + 150.0:
                bad = True
                break
        if not bad:
            keep.append(it)
    return keep


def footprint(Znew, fs, X, Y):
    """Cells covered by a face (lip 3 m inward to foot 6 m outward): props there are cleared."""
    fp = np.zeros_like(Znew, bool)
    pts = []
    for segs in fs.values():
        for seg in segs:
            for k in range(len(seg['s'])):
                lip, foot, out = seg['lip'][k], seg['foot'][k], seg['out'][k]
                for t in np.arange(-3.0, np.linalg.norm(foot[:2] - lip[:2]) + 6.01, 0.5):
                    pts.append((lip[0] + out[0] * t, lip[1] + out[1] * t))
    pts = np.array(pts) * 100.0
    fi = np.round((pts[:, 0] - ORIGIN[0]) / 100.0 - GRID[0]).astype(int)
    fj = np.round((pts[:, 1] - ORIGIN[1]) / 100.0 - GRID[1]).astype(int)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            fp[np.clip(fj + dj, 0, SY - 1), np.clip(fi + di, 0, SX - 1)] = True
    return fp


def main():
    os.makedirs(OUT, exist_ok=True)
    h = np.fromfile(T + 'cliffpre_full_height.r16', np.uint16).reshape(SY, SX)
    Z0 = rtg.to_z(h)
    X, Y, Z, weight, info = steepen(Z0)
    hn = rtg.to_h(Z)
    hn.tofile(T + 'cliffs_full_height.r16')
    Zr = rtg.to_z(hn)
    dz = Zr - Z0
    gy, gx = np.gradient(Zr, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    print('changed vertices', int((np.abs(dz) > 1).sum()), 'max raise %.0f cm, max cut %.0f cm' % (dz.max(), -dz.min()))
    print('cells > 44 deg in the rear: before', int((np.degrees(np.arctan(np.hypot(*np.gradient(Z0, 100.0)))) > 44)[X > 4000].sum()),
          'after', int((slope > 44)[X > 4000].sum()))

    m = IntroMap(Zr)
    fs = faces(Zr, info, weight)
    lay = IntroLayout(m, seed=1976)
    ends = [(seg, e, seg['lip'][0 if e == 0 else -1, :2]) for segs in fs.values() for seg in segs for e in (0, 1)]
    for seg, e, p in ends:
        seg['join%d' % e] = any(o is not seg and np.linalg.norm(p - q) < 12.0 for o, _, q in ends)
    for face, segs in fs.items():
        for seg in segs:
            lay.crag_face(face, seg)
    # where two faces meet (the nose corners) the bend between their last pieces is bare: fill it with a corner mass
    done = set()
    for seg, e, p in ends:
        for o, e2, q in ends:
            if o is seg or np.linalg.norm(p - q) >= 12.0 or (id(o), e2) in done:
                continue
            done.add((id(seg), e))
            s1 = ock_map.at(seg, 0.0 if e == 0 else seg['length'])
            s2 = ock_map.at(o, 0.0 if e2 == 0 else o['length'])
            outv = s1['out'] + s2['out']
            outv = outv / (np.linalg.norm(outv) + 1e-9)
            lip = (s1['lip'] + s2['lip']) / 2.0
            corner = dict(lip=lip, foot=(s1['foot'] + s2['foot']) / 2.0, out=outv, tan=np.array([-outv[1], outv[0]]),
                          h=float(lip[2] - (s1['foot'][2] + s2['foot'][2]) / 2.0), run=0.0)
            face = [k for k, v in fs.items() if any(x is seg for x in v)][0]
            piece = 'Mass8' if corner['h'] >= 5.5 else 'MassLow7'
            lay.add(piece, face, corner, dout=ock_place.PROUD, dz=0.0 if piece == 'Mass8' else 0.3, scale=1.05)
            lay.add('Chunk6', face, corner, dout=ock_place.PROUD + 1.5, dz=-2.0, yaw=lay.rng.uniform(-180, 180), scale=1.1)
    n_before = len(lay.inst)
    lay.inst = drop_on_roads(lay.inst)
    for it in lay.inst:
        it['fp'] = plan_points(it)          # rear_cliffs_editor.step_cliffs drops pieces that reach into station props
    print('pieces dropped near roads', n_before - len(lay.inst))
    for face, segs in fs.items():
        print('face %-10s %d segments, %.0f m, height median %s, face angle median %s' % (
            face, len(segs), sum(s['length'] for s in segs),
            [round(float(np.median(s['lip'][:, 2] - s['foot'][:, 2])), 1) for s in segs],
            [round(float(np.degrees(np.arctan(np.median((s['lip'][:, 2] - s['foot'][:, 2]) /
                np.maximum(np.linalg.norm(s['lip'][:, :2] - s['foot'][:, :2], axis=1), 0.1))))), 0) for s in segs]))
        for s_ in segs:
            print('    from (%.0f,%.0f) to (%.0f,%.0f)' % (s_['lip'][0, 0] * 100, s_['lip'][0, 1] * 100, s_['lip'][-1, 0] * 100, s_['lip'][-1, 1] * 100))
    counts = {}
    for it in lay.inst:
        counts[it['piece']] = counts.get(it['piece'], 0) + 1
    fp = footprint(Zr, fs, X, Y)
    doc = dict(map='Ger_IntroMission', kit='OreCliffKit', units='cm and degrees, Unreal world',
               mesh_path='/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Meshes/SM_OreCliff_<piece>',
               counts=counts, total_instances=len(lay.inst),
               faces={k: [dict(lip=[[round(float(v), 1) for v in p] for p in s['lip'][::4]],
                               foot=[[round(float(v), 1) for v in p] for p in s['foot'][::4]]) for s in segs] for k, segs in fs.items()},
               instances=lay.inst)
    json.dump(doc, open(OUT + 'Intro_cliffs.json', 'w'), separators=(',', ':'))
    np.savez_compressed(OUT + 'cliff_masks.npz', weight=weight.astype(np.float32), dz=dz.astype(np.float32), footprint=fp,
                        grid=np.array(GRID), origin=np.array(ORIGIN))
    print('instances', len(lay.inst), dict(sorted(counts.items())))

    from PIL import Image
    x0 = int((4000 - ORIGIN[0]) / 100 - GRID[0])
    img = np.zeros((SY, SX - x0, 3), np.uint8)
    shade = np.clip(128 + (gx[:, x0:] - gy[:, x0:]) * 120, 0, 255)
    img[..., 0] = img[..., 1] = img[..., 2] = shade.astype(np.uint8)
    img[..., 0] = np.where(slope[:, x0:] > 44, 230, img[..., 0])
    img[..., 2] = np.where(fp[:, x0:], 230, img[..., 2])
    for it in lay.inst:
        i = int(round((it['x'] - ORIGIN[0]) / 100 - GRID[0])) - x0
        j = int(round((it['y'] - ORIGIN[1]) / 100 - GRID[1]))
        if 0 <= i < img.shape[1] and 0 <= j < img.shape[0]:
            img[j, i] = (255, 255, 0)
    Image.fromarray(img[::-1]).resize((img.shape[1] * 3, img.shape[0] * 3), Image.NEAREST).save(OUT + 'cliffs_plan.png')


if __name__ == '__main__':
    main()
