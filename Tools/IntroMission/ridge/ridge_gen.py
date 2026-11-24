"""Base ridge rework of Ger_IntroMission (plain python + numpy + matplotlib, no Unreal). 2026-11-18.

User request: make the ridge's rear (east side, toward the road hub) bigger so the player has room for trenches and
defences, and put cliffs all around the ridge like on the rear ridges, but not at the ramps ("All ramps!").

    python ridge_gen.py preview   -> Saved/RidgeRework/ridge_rim_preview.png + rim.json (classification only)
    python ridge_gen.py build     -> terrain/ridge_full_height.r16 + ridge layer files for the changed rectangle,
                                     cliffs/Ridge_cliffs.json (cliff kit instances), ridge_masks.npz, plan png

Rim stations: the plateau outline (contour at PLATEAU_Z - 60) around the HQ spot, every metre, with outward normals.
Per station the outward profile gives crest, foot and flank slope:
    gentle  < GENTLE_DEG        a ramp: left alone
    steep   GENTLE_DEG..44 deg  climbable today: becomes a rock face (unless the user says otherwise)
    cliff   > 44 deg            already impassable: rock face
Stations next to gameplay actors (walls, gates, buildings, pits, slabs, resources), roads, the truck lane and the
counterattack route points are never changed.
"""
import json
import math
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
REAR = os.path.join(os.path.dirname(HERE), 'rear')
sys.path.insert(0, REAR)
import rear_terrain_gen as rtg  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/'
T = ROOT + 'IntroRear/terrain/'
OUT = ROOT + 'RidgeRework/'
GRID = (105, 75, 825, 585)
ORIGIN = (-51600.0, -32701.0)
SY, SX = GRID[3] - GRID[1] + 1, GRID[2] - GRID[0] + 1
LAYERS = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']

HQ = (1450.0, 300.0)
PLATEAU_Z = 531.0
RIM_LEVEL = PLATEAU_Z - 60.0
BOUNDS = (-9200.0, 9200.0, -6400.0, 7600.0)          # the base ridge only (the SW hill chain and far ground stay out)
GENTLE_DEG = 25.0
MIN_H = 200.0
FULL_H = 280.0
PROFILE = (-1000.0, 2600.0)                          # inward .. outward reach from the rim station (cm)

# Rear extension (user picked area A of ridge_proposal.png). The south edge stays well north of road D at the hub.
EXTENSION = [(4350.0, 600.0), (7400.0, 800.0), (7800.0, 2300.0), (7600.0, 4300.0), (6800.0, 5900.0), (4350.0, 5950.0)]
EXT_FALLOFF = 700.0                                  # raw flank width outside the polygon before steepening
TRUCK_LANE = [(4640.0, -5400.0), (4640.0, -1500.0)]  # the fuel truck's straight run to the crane (secure-line mission)
KEEP_POINTS = [(8400.0, -1500.0), (4137.0, -1752.0), (4950.0, -1250.0), (6500.0, -700.0)]   # route / apron / hub
GAMEPLAY_KEYS = ('Wall', 'Gate', 'NavBl', 'Rus_', 'BP_Airport_Ground', 'Resource_', 'Crane', 'Pit', 'Bunker',
                 'Tower', 'FenceWire', 'Ground1', 'Electricity')


def to_z(h):
    return (h.astype(np.float64) - 32768.0) * 100.0 / 128.0 + 100.0


def to_h(z):
    return np.clip(np.round((z - 100.0) * 1.28 + 32768.0), 0, 65535).astype(np.uint16)


def grid_xy():
    I, J = np.meshgrid(np.arange(SX) + GRID[0], np.arange(SY) + GRID[1])
    return ORIGIN[0] + I * 100.0, ORIGIN[1] + J * 100.0


def bilinear(Z, x, y):
    fi = np.clip((np.asarray(x) - ORIGIN[0]) / 100.0 - GRID[0], 0, SX - 1.001)
    fj = np.clip((np.asarray(y) - ORIGIN[1]) / 100.0 - GRID[1], 0, SY - 1.001)
    i0, j0 = np.floor(fi).astype(int), np.floor(fj).astype(int)
    tx, ty = fi - i0, fj - j0
    return ((Z[j0, i0] * (1 - tx) + Z[j0, i0 + 1] * tx) * (1 - ty) + (Z[j0 + 1, i0] * (1 - tx) + Z[j0 + 1, i0 + 1] * tx) * ty)


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def point_in_poly(X, Y, poly):
    from matplotlib.path import Path
    path = Path(poly)
    pts = np.column_stack([X.ravel(), Y.ravel()])
    return path.contains_points(pts).reshape(X.shape)


def dist_to_poly_edge(X, Y, poly):
    p = np.array(poly + [poly[0]])
    d = np.full(X.shape, 1e9)
    for a, b in zip(p[:-1], p[1:]):
        ab = b - a
        t = np.clip(((X - a[0]) * ab[0] + (Y - a[1]) * ab[1]) / (ab @ ab), 0, 1)
        d = np.minimum(d, np.hypot(X - (a[0] + t * ab[0]), Y - (a[1] + t * ab[1])))
    return d


def load_terrain():
    Z = to_z(np.fromfile(T + 'now_full_height.r16', np.uint16).reshape(SY, SX))
    W = {n: np.fromfile(T + 'now_full_%s.r8' % n, np.uint8).reshape(SY, SX) for n in LAYERS}
    return Z, W


def gameplay_boxes():
    """Ground that must not change: footprints of walls, gates, buildings, pits, slabs, resources. Bounds of fences,
    power lines and decal-carrying resources span far beyond what stands on the ground, so those get a small fixed
    footprint around their location (they are re-seated with the terrain, not protected)."""
    S = json.load(open(OUT + 'survey.json'))
    boxes = []
    for r in S:
        if not any(k in r['cls'] or k in r['label'] for k in GAMEPLAY_KEYS):
            continue
        if r['be'][0] > 6000 or r['be'][1] > 6000:      # volumes / managers
            continue
        if 'FenceWire' in r['cls'] or 'Electricity' in r['cls']:
            continue
        big = any(k in r['cls'] for k in ('Rus_', 'Wall', 'Gate', 'Ground1', 'Crane', 'NavBl'))
        cap = 1300.0 if big else 350.0
        ox, oy = (r['bo'][0], r['bo'][1]) if big else (r['loc'][0], r['loc'][1])
        ex, ey = min(r['be'][0], cap), min(r['be'][1], cap)
        boxes.append((ox - ex, ox + ex, oy - ey, oy + ey, r['cls']))
    return boxes


def keep_weight(X, Y, boxes, margin=150.0, ramp=250.0):
    """1 = may change, 0 = must stay (gameplay actors, roads, truck lane, route points)."""
    w = np.ones_like(X)
    for (x0, x1, y0, y1, _) in boxes:
        dx = np.maximum(np.maximum(x0 - X, X - x1), 0)
        dy = np.maximum(np.maximum(y0 - Y, Y - y1), 0)
        w = np.minimum(w, smooth((np.hypot(dx, dy) - margin) / ramp))
    for pts in rtg.ROADS.values():
        d, _ = rtg.dist_to_poly(X, Y, rtg.catmull(pts, 100.0))
        w = np.minimum(w, smooth((d - rtg.ROAD_HALF - 250.0) / 450.0))
    a, b = np.array(TRUCK_LANE[0]), np.array(TRUCK_LANE[1])
    ab = b - a
    t = np.clip(((X - a[0]) * ab[0] + (Y - a[1]) * ab[1]) / (ab @ ab), 0, 1)
    d = np.hypot(X - (a[0] + t * ab[0]), Y - (a[1] + t * ab[1]))
    w = np.minimum(w, smooth((d - 450.0) / 400.0))
    for (px, py) in KEEP_POINTS:
        w = np.minimum(w, smooth((np.hypot(X - px, Y - py) - 900.0) / 600.0))
    return w


def extend(Z, X, Y):
    """Raises area A to plateau height with a raw flank falling off over EXT_FALLOFF outside the polygon."""
    inside = point_in_poly(X, Y, EXTENSION)
    d = dist_to_poly_edge(X, Y, EXTENSION)
    target = np.where(inside, 1.0, 1.0 - smooth(d / EXT_FALLOFF))
    ext = target > 0.0
    Zn = Z.copy()
    zt = Z + (PLATEAU_Z - Z) * target
    Zn[ext] = np.maximum(Z[ext], zt[ext])
    return Zn, inside


def rim_stations(Z, X, Y):
    """Plateau outline around the HQ (closed contour at RIM_LEVEL), every metre, normals pointing outward."""
    import contourpy
    from matplotlib.path import Path
    xs, ys = X[0, :], Y[:, 0]
    m = (xs >= BOUNDS[0]) & (xs <= BOUNDS[1])
    n = (ys >= BOUNDS[2]) & (ys <= BOUNDS[3])
    sub = Z[np.ix_(n, m)].copy()
    sub[0, :] = sub[-1, :] = sub[:, 0] = sub[:, -1] = 0.0      # close every contour at the bounds
    lines = contourpy.contour_generator(xs[m], ys[n], sub, line_type='Separate').lines(RIM_LEVEL)
    best = None
    for seg in lines:
        if len(seg) > 50 and Path(seg).contains_point(HQ):
            if best is None or len(seg) > len(best):
                best = seg
    seg = np.asarray(best)
    d = np.r_[0, np.cumsum(np.hypot(np.diff(seg[:, 0]), np.diff(seg[:, 1])))]
    s = np.arange(0.0, d[-1], 100.0)
    sp = np.column_stack([np.interp(s, d, seg[:, 0]), np.interp(s, d, seg[:, 1])])
    tan = np.gradient(sp, axis=0)
    # smooth the tangent over 5 m so a corner does not throw the normal around
    k = np.ones(11) / 11
    tan = np.column_stack([np.convolve(np.pad(tan[:, c], 5, mode='wrap'), k, mode='valid') for c in (0, 1)])
    tan /= np.linalg.norm(tan, axis=1)[:, None] + 1e-9
    nrm = np.column_stack([tan[:, 1], -tan[:, 0]])
    # outward = downhill
    test = bilinear(Z, sp[:, 0] + nrm[:, 0] * 500, sp[:, 1] + nrm[:, 1] * 500) - \
        bilinear(Z, sp[:, 0] - nrm[:, 0] * 500, sp[:, 1] - nrm[:, 1] * 500)
    if np.median(test) > 0:
        nrm = -nrm
    return sp, nrm


def classify(Z, sp, nrm):
    ds = np.arange(PROFILE[0], PROFILE[1] + 1, 50.0)
    px = sp[:, 0, None] + nrm[:, 0, None] * ds[None, :]
    py = sp[:, 1, None] + nrm[:, 1, None] * ds[None, :]
    prof = bilinear(Z, px, py)
    inner = ds <= 300
    outer = ds >= 0
    crest = prof[:, inner].max(axis=1)
    foot = prof[:, outer].min(axis=1)
    H = crest - foot
    deg = np.zeros(len(sp))
    for k in range(len(sp)):
        if H[k] < MIN_H:
            continue
        hi, lo = crest[k] - 0.2 * H[k], foot[k] + 0.2 * H[k]
        r = prof[k]
        a = np.argmax(r < hi)                 # first sample below the crest band (going outward)
        b = a + np.argmax(r[a:] < lo)         # first sample below the foot band after it
        w = max(ds[b] - ds[a], 50.0)
        deg[k] = math.degrees(math.atan(0.6 * H[k] / w))
    # 9 m median along the rim so one notch does not flip a stretch
    r = 4
    pad = np.pad(deg, r, mode='wrap')
    deg_s = np.array([np.median(pad[i:i + 2 * r + 1]) for i in range(len(deg))])
    band = np.where(H < MIN_H, 'low', np.where(deg_s < GENTLE_DEG, 'gentle', np.where(deg_s < 44.0, 'steep', 'cliff')))
    # where the ground beyond the rim climbs again (the SW hill chain, the SE mound) the edge is a saddle, not a flank
    rising = prof[:, outer].max(axis=1) > crest + 150.0
    band = np.where(rising, 'low', band)
    # stations on the analysis bounds are where the contour was cut through the SW hill chain: not a real rim
    x0, x1, y0, y1 = BOUNDS
    edge = (np.minimum.reduce([sp[:, 0] - x0, x1 - sp[:, 0], sp[:, 1] - y0, y1 - sp[:, 1]]) < 700.0)
    band = np.where(edge, 'low', band)
    # the SW corner runs into the hill chain: a face there would carve a notch into the hill flank
    sw_corner = (sp[:, 0] > -6400.0) & (sp[:, 0] < -3300.0) & (sp[:, 1] < -3800.0)
    band = np.where(sw_corner, 'low', band)
    return dict(crest=crest, foot=foot, H=H, deg=deg_s, band=band, prof=prof, ds=ds)


def preview():
    os.makedirs(OUT, exist_ok=True)
    Z0, W = load_terrain()
    X, Y = grid_xy()
    Z1, inside = extend(Z0, X, Y)
    sp, nrm = rim_stations(Z1, X, Y)
    c = classify(Z1, sp, nrm)
    boxes = gameplay_boxes()
    # protection on the flank itself (5 m out from the rim station), where the rock face goes
    kw = keep_weight(X, Y, boxes)
    keep = np.minimum(bilinear(kw, sp[:, 0] + nrm[:, 0] * 300, sp[:, 1] + nrm[:, 1] * 300),
                      bilinear(kw, sp[:, 0] + nrm[:, 0] * 700, sp[:, 1] + nrm[:, 1] * 700))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x0, x1, y0, y1 = BOUNDS
    xs, ys = X[0, :], Y[:, 0]
    m = (xs >= x0) & (xs <= x1)
    n = (ys >= y0) & (ys <= y1)
    fig, ax = plt.subplots(figsize=(14, 11.5))
    ax.imshow(Z1[np.ix_(n, m)], origin='lower', extent=(xs[m][0], xs[m][-1], ys[n][0], ys[n][-1]), cmap='gist_earth',
              vmin=100, vmax=900)
    col = {'low': '#888888', 'gentle': '#00ff00', 'steep': '#ffaa00', 'cliff': '#ff2020'}
    for b in ('low', 'gentle', 'steep', 'cliff'):
        sel = c['band'] == b
        ax.scatter(sp[sel, 0], sp[sel, 1], s=9, c=col[b], label={'low': 'low edge (< 3 m): unchanged',
                                                                 'gentle': 'ramp (< %d deg): kept open' % GENTLE_DEG,
                                                                 'steep': 'steep %d-44 deg, climbable today: becomes cliff' % GENTLE_DEG,
                                                                 'cliff': 'already > 44 deg: becomes cliff'}[b])
    kept = keep < 0.5
    ax.scatter(sp[kept, 0], sp[kept, 1], s=30, facecolors='none', edgecolors='cyan', label='kept: wall/building/road/route nearby')
    ax.plot(*np.array(EXTENSION + [EXTENSION[0]]).T, 'y--', lw=2, label='rear extension A')
    for p in KEEP_POINTS:
        ax.plot(*p, 'w*', ms=10)
    ax.legend(loc='lower left', fontsize=9)
    ax.set_title('Base ridge rim after extension A: what becomes cliff (orange/red) and what stays (green ramps, grey, cyan)')
    ax.grid(alpha=.2)
    plt.tight_layout()
    plt.savefig(OUT + 'ridge_rim_preview.png', dpi=70)
    runs = []
    for b in ('gentle', 'steep', 'cliff', 'low'):
        runs.append('%s %.0f m' % (b, (c['band'] == b).sum()))
    print('rim %.0f m:' % len(sp), ', '.join(runs), '| kept by actors/roads %d m' % kept.sum())
    json.dump(dict(sp=sp.round(1).tolist(), band=c['band'].tolist(), deg=c['deg'].round(1).tolist(),
                   H=c['H'].round(0).tolist(), keep=keep.round(2).tolist()), open(OUT + 'rim.json', 'w'))


# ---- build: terrain, faces, cliff kit, props -------------------------------------------------------------------------
TARGET_DEG = 64.0          # face angle the steepening aims for (nav agents stop at 44)


def rim_series(c):
    """Crest / foot smoothed along the rim (median 15 m, box 9 m), as the rear cliffs do."""
    def med(a, r):
        pad = np.pad(a, r, mode='wrap')
        return np.array([np.median(pad[i:i + 2 * r + 1]) for i in range(len(a))])

    def box(a, r):
        pad = np.pad(a, r, mode='wrap')
        return np.convolve(pad, np.ones(2 * r + 1) / (2 * r + 1), mode='valid')
    return box(med(c['crest'], 7), 4), box(med(c['foot'], 7), 4)


def station_weight(c, keep):
    """1 where the rim becomes a rock face (steep or cliff band, high enough, not kept), eased over 4 m along the rim
    so a face never ends in a step next to a ramp."""
    w = np.isin(c['band'], ['steep', 'cliff']).astype(float)
    w *= smooth((c['H'] - MIN_H) / (FULL_H - MIN_H))
    w *= smooth((keep - 0.3) / 0.5)
    r = 4
    pad = np.pad(w, r, mode='wrap')
    return np.minimum(w, np.convolve(pad, np.ones(2 * r + 1) / (2 * r + 1), mode='valid'))


def steepen(Z1, X, Y, sp, nrm, c, sw, kw):
    crest, foot = rim_series(c)
    H = np.maximum(crest - foot, 1.0)
    kk = np.clip(np.tan(np.radians(TARGET_DEG)) / np.tan(np.radians(np.maximum(c['deg'], 12.0))), 1.6, 6.0)
    x0, x1, y0, y1 = BOUNDS
    band = (X >= x0 - 2600) & (X <= x1 + 2600) & (Y >= y0 - 2600) & (Y <= y1 + 2600)
    cj, ci = np.nonzero(band)
    cx, cy = X[cj, ci], Y[cj, ci]
    near = np.empty(len(cx), int)
    dist = np.empty(len(cx))
    for a0 in range(0, len(cx), 3000):
        dx = cx[a0:a0 + 3000, None] - sp[None, :, 0]
        dy = cy[a0:a0 + 3000, None] - sp[None, :, 1]
        d2 = dx * dx + dy * dy
        k = np.argmin(d2, axis=1)
        near[a0:a0 + 3000] = k
        dist[a0:a0 + 3000] = np.sqrt(d2[np.arange(len(k)), k])
    t = (cx - sp[near, 0]) * nrm[near, 0] + (cy - sp[near, 1]) * nrm[near, 1]
    m = (t >= PROFILE[0]) & (t <= PROFILE[1]) & (dist <= max(-PROFILE[0], PROFILE[1]) + 100)
    j, i, nk = cj[m], ci[m], near[m]
    z = Z1[j, i]
    q = np.clip((z - foot[nk]) / H[nk], 0.0, 1.0)
    qn = 0.9 * np.clip((q - 0.5) * kk[nk] + 0.5, 0.0, 1.0) + 0.1 * q
    znew = foot[nk] + qn * H[nk]
    znew = np.where(q > 0.85, np.maximum(znew, z), znew)          # never cut the plateau edge
    wgt = sw[nk] * kw[j, i]
    Z = Z1.copy()
    Z[j, i] = z + wgt * (znew - z)
    weight = np.zeros_like(Z1)
    weight[j, i] = wgt
    return Z, weight


def faces(Zr, sp, nrm, weight):
    import rear_cliffs_gen as rcg
    ock_map = rcg.ock_map
    ds = np.arange(PROFILE[0], PROFILE[1] + 1, 100.0)
    st = []
    for k in range(len(sp)):
        px = sp[k, 0] + nrm[k, 0] * ds
        py = sp[k, 1] + nrm[k, 1] * ds
        zs = bilinear(Zr, px, py) / 100.0
        ok = bilinear(weight, px, py) > 0.6
        run = ock_map.Map._run(None, zs, ok, 1.0)
        if run is None or zs[run[0]] - zs[run[1]] < 2.5:
            st.append(None)
            continue
        a, b = run
        st.append(dict(lip=(px[a] / 100.0, py[a] / 100.0, float(zs[a])), foot=(px[b] / 100.0, py[b] / 100.0, float(zs[b]))))
    segs = ock_map.Map._segments(None, st, closed=True)
    for seg in segs:
        seg['open0'] = seg['open1'] = True
    return {'rim': segs}


def layout(Zr, fs, boxes):
    import rear_cliffs_gen as rcg
    m = rcg.IntroMap(Zr)
    lay = rcg.IntroLayout(m, seed=2026)
    ends = [(seg, e, seg['lip'][0 if e == 0 else -1, :2]) for segs in fs.values() for seg in segs for e in (0, 1)]
    for seg, e, p in ends:
        seg['join%d' % e] = any(o is not seg and np.linalg.norm(p - q) < 12.0 for o, _, q in ends)
    for face, segs in fs.items():
        for seg in segs:
            lay.crag_face(face, seg)
    lay.inst = rcg.drop_on_roads(lay.inst)
    keep, dropped = [], 0
    for it in lay.inst:
        it['fp'] = rcg.plan_points(it)
        if any(b[0] - 150 <= x <= b[1] + 150 and b[2] - 150 <= y <= b[3] + 150 for (x, y) in it['fp'] for b in boxes):
            dropped += 1
            continue
        keep.append(it)
    print('cliff pieces dropped near gameplay actors', dropped)
    return keep


def footprint(fs):
    fp = np.zeros((SY, SX), bool)
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


REMOVE_UNDER_FACE = ('REAR_Detail_Stone', 'REAR_CraterStone', 'REAR_Decal_', 'REAR_Detail_Shrub', 'REAR_Detail_Rock',
                     'IDT_Decal', 'IDT_Shrub', 'IDT_Cactus')
NEVER_MOVE = ('REAR_Cliff_', 'REAR_Road', 'REAR_Rail', 'REAR_Edge', 'REAR_Skirt', 'REAR_Water', 'Landscape', 'LAND_',
              'NavMesh', 'RecastNavMesh', 'BP_MissionManager', 'BP_FogOfWar', 'BP_Deformation', 'ExponentialHeightFog',
              'DirectionalLight', 'SkyLight', 'PostProcess', 'WorldSettings', 'BP_GroundPathSpline', 'BP_RoadSpline')


def prop_actions(Zr, Z0, fp):
    S = json.load(open(OUT + 'survey.json'))
    dz_all = Zr - Z0
    acts, moved, removed, warn = [], 0, 0, []
    for r in S:
        label, cls = r['label'], r['cls']
        if label.startswith(NEVER_MOVE) or cls.startswith(NEVER_MOVE) or cls in ('NavMeshBoundsVolume', 'BlockingVolume'):
            continue
        i = int(round((r['loc'][0] - ORIGIN[0]) / 100.0 - GRID[0]))
        j = int(round((r['loc'][1] - ORIGIN[1]) / 100.0 - GRID[1]))
        if not (0 <= i < SX and 0 <= j < SY):
            continue
        d = float(dz_all[j, i])
        under = bool(fp[j, i])
        if under and label.startswith(REMOVE_UNDER_FACE):
            acts.append(dict(label=label, action='remove'))
            removed += 1
            continue
        if abs(d) > 3.0:
            if any(k in cls for k in ('Wall', 'Gate', 'Rus_', 'Ground1', 'Resource_')):
                warn.append((label, cls, round(d)))
            acts.append(dict(label=label, action='move', dz=round(d, 1)))
            moved += 1
    json.dump(acts, open(OUT + 'prop_actions.json', 'w'), indent=1)
    print('props: move %d, remove %d; gameplay actors that would move: %s' % (moved, removed, warn))


def build():
    Z0, W = load_terrain()
    X, Y = grid_xy()
    Z1, inside = extend(Z0, X, Y)
    sp, nrm = rim_stations(Z1, X, Y)
    c = classify(Z1, sp, nrm)
    boxes = gameplay_boxes()
    kw = keep_weight(X, Y, boxes)
    keep = np.minimum(bilinear(kw, sp[:, 0] + nrm[:, 0] * 300, sp[:, 1] + nrm[:, 1] * 300),
                      bilinear(kw, sp[:, 0] + nrm[:, 0] * 700, sp[:, 1] + nrm[:, 1] * 700))
    sw = station_weight(c, keep)
    Z, weight = steepen(Z1, X, Y, sp, nrm, c, sw, kw)
    hn = to_h(Z)
    Zr = to_z(hn)
    dz = Zr - Z0
    changed = np.abs(dz) > 1.0
    jj, ii = np.nonzero(changed)
    rect = (int(ii.min() + GRID[0] - 2), int(jj.min() + GRID[1] - 2), int(ii.max() + GRID[0] + 2), int(jj.max() + GRID[1] + 2))
    sub = hn[rect[1] - GRID[1]:rect[3] - GRID[1] + 1, rect[0] - GRID[0]:rect[2] - GRID[0] + 1]
    sub.tofile(T + 'ridge_height.r16')
    hn.tofile(T + 'ridge_full_height.r16')
    gy, gx = np.gradient(Zr, 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    fs = faces(Zr, sp, nrm, weight)
    inst = layout(Zr, fs, boxes)
    fp = footprint(fs)
    prop_actions(Zr, Z0, fp)
    counts = {}
    for it in inst:
        counts[it['piece']] = counts.get(it['piece'], 0) + 1
    doc = dict(map='Ger_IntroMission', kit='OreCliffKit', units='cm and degrees, Unreal world', rect=rect,
               mesh_path='/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Meshes/SM_OreCliff_<piece>',
               counts=counts, total_instances=len(inst), instances=inst)
    json.dump(doc, open(OUT + 'Ridge_cliffs.json', 'w'), separators=(',', ':'))
    np.savez_compressed(OUT + 'ridge_masks.npz', weight=weight.astype(np.float32), dz=dz.astype(np.float32), footprint=fp)
    print('rect', rect, 'changed vertices', int(changed.sum()), 'max raise %.0f, max cut %.0f' % (dz.max(), -dz.min()))
    print('segments', [(round(s['length']), round(float(np.median(s['lip'][:, 2] - s['foot'][:, 2])), 1)) for s in fs['rim']])
    print('instances', len(inst), dict(sorted(counts.items())))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x0, x1, y0, y1 = BOUNDS
    xs, ys = X[0, :], Y[:, 0]
    mm = (xs >= x0) & (xs <= x1)
    nn = (ys >= y0) & (ys <= y1)
    fig, ax = plt.subplots(1, 2, figsize=(22, 9.5))
    ext = (xs[mm][0], xs[mm][-1], ys[nn][0], ys[nn][-1])
    im = ax[0].imshow(slope[np.ix_(nn, mm)], origin='lower', extent=ext, cmap='magma', vmin=0, vmax=70)
    plt.colorbar(im, ax=ax[0])
    ax[0].scatter([it['x'] for it in inst], [it['y'] for it in inst], s=3, c='yellow')
    ax[0].set_title('slope after (deg); yellow = cliff kit pieces')
    im = ax[1].imshow(dz[np.ix_(nn, mm)], origin='lower', extent=ext, cmap='RdBu_r', vmin=-400, vmax=400)
    plt.colorbar(im, ax=ax[1])
    ax[1].set_title('height change (cm)')
    plt.tight_layout()
    plt.savefig(OUT + 'ridge_plan.png', dpi=60)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'preview':
        preview()
    elif len(sys.argv) > 1 and sys.argv[1] == 'build':
        build()
