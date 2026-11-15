"""Intro cinematic - nuclear crater terrain disc: SM_Intro_CraterTerrain + its 4096 textures and crack mask.

Run:  blender.exe --factory-startup -b -P build_crater.py [-- notex]

Frame (metres here, cm in the FBX): origin at the centre of the crater floor, floor z = 0, surrounding
ground about z = +9, disc radius 80. UV: one planar top-down unwrap, U = (x + 80) / 160, V = (y + 80) / 160.
The floor plates are Voronoi cells; every cell is inset by half the crack width and the gap is a V groove
whose bottom runs along the Voronoi edge. Everything is triangulated in one constrained Delaunay pass.
"""
import os
import sys
import math
import random
import numpy as np
import bpy
from mathutils import Vector
from mathutils.geometry import delaunay_2d_cdt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sets_common as C
from sets_common import sn, sstep, ang_noise, fft_noise, mixc, rgb, F

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
R_DISC, GROUND, FLOOR_R, RIM_R = 80.0, 9.0, 21.0, 30.0
FIELD_R = 17.5                 # crack field: the inner 35 m (diameter) of the floor
TEX = 4096
rng = random.Random(1986)


def crater_h(x, y):
    x = np.asarray(x, float)
    y = np.asarray(y, float)
    r = np.hypot(x, y)
    th = np.arctan2(y, x)
    rim = RIM_R * (1 + 0.045 * ang_noise(th, 1, 2, 7))
    crest = GROUND + 2.5 + 0.45 * np.clip(ang_noise(th, 2, 2, 9), -1.1, 1.1)      # 2..3 m above the ground
    t = np.clip((r - FLOOR_R) / (rim - FLOOR_R), 0, 1)
    zin = crest * (0.5 - 0.5 * np.cos(np.pi * t)) ** 0.85 + 0.3 * np.minimum(r / FLOOR_R, 1) ** 2
    zin = zin + (0.6 * ang_noise(th, 3, 5, 22) + 0.3 * sn(x, y, 8, 0.12, 2)) * np.sin(np.pi * t) ** 2                # slump gullies on the wall
    u = np.maximum(r - rim, 0)
    g = GROUND - 1.5 * sstep(r, 36, 76)
    zout = g + (crest + 0.3 - g) * np.exp(-(u / 7.0) ** 1.4)
    ridge = np.clip(ang_noise(th, 4, 6, 26), 0, 1.6) ** 1.5 * 0.5 * np.exp(-u / 15.0) * sstep(u, 0, 5)
    hum = 0.3 * sn(x, y, 5, 0.045, 2) * sstep(u, 2, 14) + 0.10 * sn(x, y, 6, 0.16, 2) * sstep(u, 0, 6)
    zout = zout + ridge + hum
    z = np.where(r < rim, zin, zout)
    z = z + 0.05 * sn(x, y, 7, 0.3, 2) * sstep(r, FIELD_R + 0.5, FLOOR_R + 2)
    z = z - 6.0 * sstep(r, 73.0, 80.0)                                                 # skirt
    return z


# ------------------------------------------------------------------ crack layout (Voronoi)
def make_layout():
    seeds, interior = [], []
    rings = [(0.0, 1), (2.4, 5), (4.9, 9), (7.5, 12), (10.2, 15), (12.9, 18), (15.6, 21), (19.7, 26)]
    for ri, (r, n) in enumerate(rings):
        ph = rng.uniform(0, 2 * math.pi)
        for k in range(n):
            a = ph + 2 * math.pi * (k + rng.uniform(-0.3, 0.3)) / n
            rr = r + (rng.uniform(-0.5, 0.5) if 0 < ri < len(rings) - 1 else 0.0)
            if ri == 0:
                rr, a = 0.5, rng.uniform(0, 6.28)
            seeds.append((rr * math.cos(a), rr * math.sin(a)))
            interior.append(ri < len(rings) - 1)
    for k in range(40):                                            # far guard ring keeps the guard cells bounded
        a = 2 * math.pi * k / 40
        seeds.append((27.0 * math.cos(a), 27.0 * math.sin(a)))
        interior.append(False)
    S = np.array(seeds)
    _, _, tris, _, _, _ = delaunay_2d_cdt([Vector(p) for p in seeds], [], [], 0, 1e-6)
    cc = []
    for t in tris:
        a, b, c = S[t[0]], S[t[1]], S[t[2]]
        d = 2 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]))
        ux = ((a @ a) * (b[1] - c[1]) + (b @ b) * (c[1] - a[1]) + (c @ c) * (a[1] - b[1])) / d
        uy = ((a @ a) * (c[0] - b[0]) + (b @ b) * (a[0] - c[0]) + (c @ c) * (b[0] - a[0])) / d
        cc.append((ux, uy))
    cc = np.array(cc)
    tri_of = {}
    for ti, t in enumerate(tris):
        for s in t:
            tri_of.setdefault(s, []).append(ti)
    cells = {}
    for s in range(len(seeds)):
        if not interior[s]:
            continue
        tl = sorted(tri_of[s], key=lambda ti: math.atan2(cc[ti][1] - S[s][1], cc[ti][0] - S[s][0]))
        cells[s] = tl                                              # CCW loop of Voronoi vertices (triangle ids)
    # edges: pair of triangles sharing two seeds
    edges = {}
    for s, tl in cells.items():
        for i in range(len(tl)):
            t0, t1 = tl[i], tl[(i + 1) % len(tl)]
            other = (set(tris[t0]) & set(tris[t1])) - {s}
            if len(other) != 1:
                continue
            o = other.pop()
            key = (min(t0, t1), max(t0, t1))
            if key in edges:
                continue
            p, q = cc[t0], cc[t1]
            mid = (p + q) / 2
            rm = float(np.hypot(*mid))
            is_crack = interior[o]
            w = d = 0.0
            if is_crack:
                e = (q - p) / (np.linalg.norm(q - p) + 1e-9)
                rad = abs(float(e @ (mid / (rm + 1e-9)))) if rm > 0.5 else 1.0
                tp = min(max((17.3 - rm) / 4.5, 0.0), 1.0) ** 0.6
                w0 = 0.2 + 0.6 * min(max(0.8 * rad ** 3 + 0.45 * rng.random() ** 2 - 0.05, 0.0), 1.0)
                w = w0 * tp
                d = (0.3 + 0.7 * (w0 - 0.2) / 0.6) * tp
            edges[key] = {'p': p, 'q': q, 'w': w, 'd': d, 'crack': is_crack, 'cells': (s, o)}
    vdepth = {}
    for (t0, t1), e in edges.items():
        for t in (t0, t1):
            vdepth.setdefault(t, []).append(e['d'] if e['crack'] else 0.0)
    for t in list(vdepth):
        boundary = any(not interior[s] for s in tris[t])
        vdepth[t] = 0.0 if boundary else float(np.mean(vdepth[t]))
    return S, np.array(interior), tris, cc, cells, edges, vdepth


def clip_poly(poly, p0, n):
    out = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        da, db = (a - p0) @ n, (b - p0) @ n
        if da >= 0:
            out.append(a)
        if (da >= 0) != (db >= 0):
            out.append(a + (b - a) * (da / (da - db)))
    return out


def build_mesh(layout):
    S, interior, tris, cc, cells, edges, vdepth = layout
    V, Z, E, FACES = [], [], [], []

    def add(p, z):
        V.append(Vector((float(p[0]), float(p[1]))))
        Z.append(float(z))
        return len(V) - 1
    # plates
    nplates = 0
    for s, tl in cells.items():
        poly = [cc[t].copy() for t in tl]
        orig = list(poly)
        for i in range(len(tl)):
            t0, t1 = tl[i], tl[(i + 1) % len(tl)]
            e = edges.get((min(t0, t1), max(t0, t1)))
            if e is None or e['w'] <= 0:
                continue
            a, b = orig[i], orig[(i + 1) % len(orig)]
            n = np.array((-(b - a)[1], (b - a)[0]))
            n /= (np.linalg.norm(n) + 1e-12)
            poly = clip_poly(poly, a + n * e['w'] / 2, n)
            if len(poly) < 3:
                break
        if len(poly) < 3:
            continue
        clean = [poly[0]]
        for p in poly[1:]:
            if np.linalg.norm(p - clean[-1]) > 0.02:
                clean.append(p)
        if np.linalg.norm(clean[0] - clean[-1]) <= 0.02:
            clean.pop()
        if len(clean) < 3:
            continue
        a0, tx, ty = rng.uniform(-0.09, 0.09), rng.uniform(-0.04, 0.04), rng.uniform(-0.04, 0.04)
        ids = []
        for p in clean:
            z = float(crater_h(p[0], p[1])) + a0 + tx * (p[0] - S[s][0]) + ty * (p[1] - S[s][1])
            ids.append(add(p, z))
        FACES.append(ids)
        nplates += 1
    # groove bottoms along the Voronoi edges
    vid = {}
    for (t0, t1), e in edges.items():
        if not e['crack']:
            continue
        for t in (t0, t1):
            if t not in vid:
                vid[t] = add(cc[t], float(crater_h(cc[t][0], cc[t][1])) - vdepth[t])
        E.append((vid[t0], vid[t1]))
    # ejecta blocks (frusta)
    blocks = []
    for i in range(86):
        if i < 10:
            r, R = rng.uniform(21.5, 26.5), rng.uniform(0.4, 0.9)
        else:
            r = 31.0 + 30.0 * rng.random() ** 1.6
            R = rng.uniform(0.45, 1.7) * (1.0 - 0.5 * (r - 31) / 30)
        a = rng.uniform(0, 2 * math.pi)
        c = np.array((r * math.cos(a), r * math.sin(a)))
        if any(np.linalg.norm(c - b[0]) < (R + b[1]) * 1.5 for b in blocks):
            continue
        blocks.append((c, R))
        hgt = R * rng.uniform(0.45, 0.8)
        nside = rng.choice((5, 6, 6, 7))
        ph = rng.uniform(0, 6.28)
        foot = [c + R * rng.uniform(0.75, 1.15) * np.array((math.cos(ph + 6.283 * k / nside), math.sin(ph + 6.283 * k / nside)))
                for k in range(nside)]
        off = np.array((rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15))) * R
        top = [c + off + (p - c) * rng.uniform(0.5, 0.68) for p in foot]
        tilt = (rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12))
        zc = float(crater_h(c[0], c[1]))
        fi = [add(p, float(crater_h(p[0], p[1])) - 0.03) for p in foot]
        ti = [add(p, zc + hgt + tilt[0] * (p[0] - c[0]) + tilt[1] * (p[1] - c[1])) for p in top]
        for k in range(nside):
            E.append((fi[k], fi[(k + 1) % nside]))
            E.append((ti[k], ti[(k + 1) % nside]))
    # terrain points on jittered rings
    BC = np.array([b[0] for b in blocks])
    BR = np.array([b[1] for b in blocks])
    r = 18.4
    pts = []
    while r < R_DISC - 0.5:
        s = 0.74 + 0.40 * sstep(r, 36.0, 62.0)
        n = int(2 * math.pi * r / s)
        ph = rng.uniform(0, 6.28)
        for k in range(n):
            a = ph + 2 * math.pi * (k + rng.uniform(-0.22, 0.22)) / n
            rr = r + rng.uniform(-0.2, 0.2) * s
            pts.append((rr * math.cos(a), rr * math.sin(a)))
        r += s * 0.88
    for k in range(420):
        a = 2 * math.pi * k / 420
        pts.append((R_DISC * math.cos(a), R_DISC * math.sin(a)))
    P = np.array(pts)
    near = np.argmin(((P[:, None, :] - S[None, :, :]) ** 2).sum(-1), 1)
    keep = ~interior[near]
    dB = np.sqrt(((P[:, None, :] - BC[None, :, :]) ** 2).sum(-1))
    keep &= (dB > BR[None, :] * 1.35).all(1)
    P = P[keep]
    PZ = crater_h(P[:, 0], P[:, 1])
    for p, z in zip(P, PZ):
        add(p, z)
    print('CDT input: %d verts, %d plates, %d constraint edges, %d blocks' % (len(V), nplates, len(E), len(blocks)))
    ov, oe, of, orig_v, orig_e, orig_f = delaunay_2d_cdt(V, E, FACES, 0, 1e-5)
    OV = np.array([(v.x, v.y) for v in ov])
    OZ = np.empty(len(ov))
    for i, o in enumerate(orig_v):
        OZ[i] = Z[o[0]] if len(o) else float(crater_h(OV[i, 0], OV[i, 1]))
    Zarr = np.array(Z)
    acc = C.Acc('SM_Intro_CraterTerrain', ['Ground', 'CrackGlow'])
    verts = [(float(OV[i, 0]), float(OV[i, 1]), float(OZ[i])) for i in range(len(ov))]
    cen = np.array([OV[list(f)].mean(0) for f in of])
    near = np.argmin(((cen[:, None, :] - S[None, :, :]) ** 2).sum(-1), 1)
    gf, cf = [], []
    for fi_, f in enumerate(of):
        f = list(f)
        a, b, c = OV[f[0]], OV[f[1]], OV[f[2]]
        area2 = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(area2) < 1e-7:
            continue
        if area2 < 0:
            f = [f[0], f[2], f[1]]
        is_plate = any(o < nplates for o in orig_f[fi_])
        if (not is_plate) and interior[near[fi_]]:
            cf.append(f)
        else:
            gf.append(f)

    def uv(f):
        return [((verts[i][0] + R_DISC) / (2 * R_DISC), (verts[i][1] + R_DISC) / (2 * R_DISC)) for i in f]
    acc.mesh(verts, gf + cf, [uv(f) for f in gf + cf], 'Ground')
    acc.mi[len(gf):] = [1] * len(cf)
    print('CRATER tris: ground %d, crack %d' % (len(gf), len(cf)))
    ob = acc.build(smooth_angle=38.0, merge=0.0)
    return ob


# ------------------------------------------------------------------ textures
def make_textures(layout):
    S_, interior, tris, cc, cells, edges, vdepth = layout
    N = TEX
    ppm = N / (2 * R_DISC)
    ax = ((np.arange(N, dtype=F) + 0.5) / N * 2 * R_DISC - R_DISC)
    X, Y = ax[None, :], ax[:, None]
    r = np.hypot(X, Y)
    th = np.arctan2(Y, X)
    rim = RIM_R * (1 + 0.045 * ang_noise(th, 1, 2, 7)).astype(F)
    u = np.maximum(r - rim, 0)
    # signed distance to the cracks (metres, <= 0 inside), and plate ids, inside the field box only
    i0, i1 = int((R_DISC - 21.0) * ppm), int((R_DISC + 21.0) * ppm)
    sd = np.full((N, N), 50.0, F)
    for e in edges.values():
        if not e['crack'] or e['w'] <= 0:
            continue
        p, q, w = e['p'], e['q'], e['w']
        m = w / 2 + 0.9
        xa, xb = int((min(p[0], q[0]) - m + R_DISC) * ppm), int((max(p[0], q[0]) + m + R_DISC) * ppm) + 1
        ya, yb = int((min(p[1], q[1]) - m + R_DISC) * ppm), int((max(p[1], q[1]) + m + R_DISC) * ppm) + 1
        xx, yy = ax[None, xa:xb], ax[ya:yb, None]
        d = q - p
        t = np.clip(((xx - p[0]) * d[0] + (yy - p[1]) * d[1]) / (d @ d + 1e-12), 0, 1)
        dist = np.hypot(xx - (p[0] + t * d[0]), yy - (p[1] + t * d[1])) - w / 2
        sd[ya:yb, xa:xb] = np.minimum(sd[ya:yb, xa:xb], dist.astype(F))
    mask = (1.0 - sstep(sd, 0.0, 0.16)).astype(F)
    C.save_img('T_Intro_Crater_CrackMask', mask, 'Non-Color')
    pid = np.zeros((N, N), F)
    Sx, Sy = S_[:, 0].astype(F), S_[:, 1].astype(F)
    hv = np.array([random.Random(i).random() for i in range(len(S_))], F)
    for ya in range(i0, i1, 64):
        yb = min(ya + 64, i1)
        dd = (ax[None, i0:i1, None] - Sx[None, None, :]) ** 2 + (ax[ya:yb, None, None] - Sy[None, None, :]) ** 2
        k = np.argmin(dd, -1)
        pid[ya:yb, i0:i1] = np.where(interior[k], hv[k] - 0.5, 0.0)
    del dd
    n1 = fft_noise(N, N, 2.0, 91)
    n2 = fft_noise(N, N, 1.3, 92, cut=0.09)
    n3 = fft_noise(N, N, 0.3, 93)
    rays = np.clip(ang_noise(th, 94, 14, 70), 0, None) ** 1.6
    rays = (rays * np.exp(-u / 22.0) * sstep(u, 0.5, 6.0) * (0.6 + 0.4 * sstep(n1, -1, 1))).astype(F)
    floor = (1 - sstep(r, 19.0, 24.0)).astype(F)
    inside = (1 - sstep(r, rim - 4.0, rim + 1.5)).astype(F)
    scorch = np.exp(-u / 20.0).astype(F)
    alb = np.ones((N, N, 3), F) * rgb(96, 85, 72)[None, None, :]
    alb = mixc(alb, rgb(60, 54, 48), scorch * 0.9)
    alb = mixc(alb, rgb(50, 46, 43), inside)
    alb = mixc(alb, rgb(45, 42, 41), floor)
    alb = mixc(alb, rgb(124, 111, 94), np.clip(rays * 0.75, 0, 1))
    alb = alb * (1 + (0.16 * n1 + 0.10 * n2 + 0.05 * n3 + 0.28 * pid)[..., None])
    rock = sstep(n2 + 0.5 * n3, 1.3, 2.0) * (1 - floor)
    alb = mixc(alb, rgb(118, 112, 104), rock * 0.5)
    glass = (sstep(0.7 * n1 + 0.7 * n2 + 0.8 * pid, 0.35, 0.95) * (1 - sstep(r, 19.0, 28.0))).astype(F)
    alb = mixc(alb, rgb(24, 25, 28), glass * 0.85)
    soot = (1 - sstep(sd, 0.0, 0.7)).astype(F)
    alb = mixc(alb, rgb(22, 19, 17), soot * 0.6)
    alb = mixc(alb, rgb(10, 8, 7), mask)
    hm = (0.030 * n2 + 0.006 * n3) * (1 - 0.85 * glass) + 0.012 * rock - 0.02 * mask
    rough = np.clip(0.9 - 0.2 * floor + 0.04 * n3 - 0.72 * glass, 0.08, 1.0)
    ao = np.clip(1.0 - 0.5 * (1 - sstep(sd, -0.1, 0.3)) + 0.1 * n2 * 0.0, 0.3, 1.0)
    C.save_set('T_Intro_Crater_Ground', np.clip(alb, 0, 1).astype(F), hm.astype(F), ppm, ao.astype(F), rough.astype(F), wrap=False)
    print('MASK coverage %.3f %% of the texture, crack px (mask > 0.99): %d' % (100 * float(mask.mean()), int((mask > 0.99).sum())))


def main():
    layout = make_layout()
    ws = [e['w'] for e in layout[5].values() if e['crack'] and e['w'] > 0]
    ds = [e['d'] for e in layout[5].values() if e['crack'] and e['w'] > 0]
    print('CRACKS: %d edges, width %.2f..%.2f m (median %.2f), depth %.2f..%.2f m' % (
        len(ws), min(ws), max(ws), float(np.median(ws)), min(ds), max(ds)))
    if 'notex' not in ARGS:
        make_textures(layout)
    C.setup_scene()
    C.make_material('Ground', 'T_Intro_Crater_Ground', 'T_Intro_Crater_CrackMask', 0.04, (1.0, 0.16, 0.012))
    C.make_material('CrackGlow', 'T_Intro_Crater_Ground', 'T_Intro_Crater_CrackMask', 0.35, (1.0, 0.16, 0.012))
    ob = build_mesh(layout)
    me = ob.data
    down = sum(1 for p in me.polygons if p.normal.z <= 0)
    print('CHECK crater triangles not facing up: %d of %d' % (down, len(me.polygons)))
    assert down == 0
    st = C.ob_stats(ob)
    assert st['tris'] <= 60000 and st['uv_layers'] == 1
    uvs = np.array([l.uv[:] for l in me.uv_layers[0].data])
    assert uvs.min() >= -1e-5 and uvs.max() <= 1.00001
    co = np.array([v.co[:] for v in me.vertices])
    rr = np.hypot(co[:, 0], co[:, 1]) / 100
    tt = np.linspace(0, 2 * np.pi, 720, endpoint=False)
    rr_ = np.linspace(24, 36, 121)
    hh = crater_h(rr_[None, :] * np.cos(tt[:, None]), rr_[None, :] * np.sin(tt[:, None]))
    st['rim_crest_cm_min_max'] = [round(float(hh.max(1).min() * 100), 1), round(float(hh.max(1).max() * 100), 1)]
    st['rim_radius_m_min_max'] = [round(float(rr_[hh.argmax(1)].min()), 1), round(float(rr_[hh.argmax(1)].max()), 1)]
    st['ground_z_at_45m_cm'] = round(float(co[(rr > 44) & (rr < 46), 2].mean()), 1)
    st['edge_z_cm'] = round(float(co[rr > 79.5, 2].mean()), 1)
    st['z_at_72m_cm'] = round(float(co[(rr > 71) & (rr < 73), 2].mean()), 1)
    st['floor_z_cm'] = round(float(co[rr < 2.0, 2].max()), 1)
    st['crack_tris'] = sum(1 for p in me.polygons if p.material_index == 1)
    C.export_fbx(ob)
    st['fbx_bytes'] = os.path.getsize(os.path.join(C.OUT, ob.name + '.fbx'))
    print('STAT', st)
    C.write_json('crater_stats.json', {ob.name: st})
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(C.WORK, 'IntroCrater.blend'))
    print('CRATER DONE')


main()
