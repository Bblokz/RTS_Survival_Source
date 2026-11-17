"""Ger_Berlin floating-gap scan and sculpt plan, offline (numpy). Input: gaps_dump.py's files in Saved/Berlin/gaps.

    python gaps_scan.py scan            report only (scan.json, gap_map.png)
    python gaps_scan.py plan            scan, then plan: drops for small loose pieces, a raised landscape for the rest
                                        -> plan.json, new_height.r16 (+ rescan against the planned ground)

Detector. Every mesh is sampled as a point cloud on its triangles (about every 20 cm); every instance is transformed to
the world and its lowest geometry per 50 cm cell is kept. The lowest geometry of ANYTHING in a cell (a global map) is
compared with the landscape there: a cell floats when that underside is 3..CAP cm above the ground and belongs to the
base of its piece (within BASE cm of the piece's own lowest point, so balconies, lintels and roofs do not count). A
piece standing on another piece is not floating: the lower piece owns the cell.
"""
import json
import math
import sys

import numpy as np
from PIL import Image

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/gaps/'
ORG, QUAD, NV = -59850.0, 100.0, 1198
CELL = 50.0
NC = int((NV - 1) * QUAD / CELL)                 # 2394 cells a side
FLOAT_MIN, CAP, BASE = 3.0, 260.0, 45.0
SPACING = 20.0
SKIP_FOLDERS = ('Berlin/Ambient', 'Berlin/Atmosphere', 'MapSkirt', 'Berlin/Terrain')
SKIP_MESH = ('Plane.Plane',)
# pieces that are simply lowered when all of their base floats (loose things; ruins and landmarks get ground instead)
DROP_FOLDERS = ('Berlin/Furniture', 'Berlin/Cover', 'Berlin/Streets', 'Berlin/Tiergarten')
DROP_MAX_CELLS = 260                              # 65 m2


def is_loose(item, mesh):
    """loose things follow the ground (and are lowered when all of their base floats); the rest is fixed and the
    ground comes to it: ruins, footprints, landmarks, the power station, the rail yard"""
    if item['folder'].startswith('Berlin/Cover/Heaps'):
        return False                               # big cover heaps: the ground comes up to their edge
    if item['folder'].startswith(('Berlin/PowerStation', 'Berlin/RailYard')):
        return any(k in mesh for k in ('_Car_', 'VehicleParts', 'container', 'Barrel', 'Crate'))
    return item['folder'].startswith(DROP_FOLDERS) or item['folder'] == 'Berlin/Ruins' or 'BerlinFurn' in mesh


def load_heights(name='full_height.r16'):
    h = np.fromfile(OUT + name, dtype=np.uint16).reshape(NV, NV).astype(np.float64)
    return (h - 32768.0) / 128.0 * 100.0          # [j (y), i (x)]


def ground_at(Z, x, y):
    fi, fj = (x - ORG) / QUAD, (y - ORG) / QUAD
    i0 = np.clip(np.floor(fi).astype(int), 0, NV - 2)
    j0 = np.clip(np.floor(fj).astype(int), 0, NV - 2)
    u, v = np.clip(fi - i0, 0.0, 1.0), np.clip(fj - j0, 0.0, 1.0)
    return (Z[j0, i0] * (1 - u) * (1 - v) + Z[j0, i0 + 1] * u * (1 - v) +
            Z[j0 + 1, i0] * (1 - u) * v + Z[j0 + 1, i0 + 1] * u * v)


def mesh_points(index):
    with open(OUT + 'mesh/%d.bin' % index, 'rb') as f:
        nv, nt = np.fromfile(f, dtype=np.int32, count=2)
        v = np.fromfile(f, dtype=np.float32, count=nv * 3).reshape(nv, 3).astype(np.float64)
        t = np.fromfile(f, dtype=np.int32, count=nt * 3).reshape(nt, 3)
    if nt == 0:
        return v
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    longest = np.sqrt(np.maximum(np.maximum(((b - a) ** 2).sum(1), ((c - a) ** 2).sum(1)), ((c - b) ** 2).sum(1)))
    spacing = SPACING
    while True:
        n = np.clip(np.ceil(longest / spacing).astype(int), 1, 400)
        total = int(((n + 1) * (n + 2) // 2).sum())
        if total <= 1500000:
            break
        spacing *= 1.5
    parts = [v]
    for k in np.unique(n):
        if k == 1:
            continue
        sel = n == k
        ii, jj = np.meshgrid(np.arange(k + 1), np.arange(k + 1), indexing='ij')
        keep = ii + jj <= k
        wu, wv = ii[keep] / float(k), jj[keep] / float(k)
        pa, pb, pc = a[sel], b[sel], c[sel]
        pts = pa[:, None, :] * (1 - wu - wv)[None, :, None] + pb[:, None, :] * wu[None, :, None] + pc[:, None, :] * wv[None, :, None]
        parts.append(pts.reshape(-1, 3))
    return np.concatenate(parts)


def rot_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


class Scan:
    def __init__(self):
        self.data = json.load(open(OUT + 'components.json'))
        self.points = {}
        self.items = []                           # one per component instance
        for row in self.data['rows']:
            mesh = self.data['meshes'][row['m']]
            if row['folder'].startswith(SKIP_FOLDERS) or mesh.endswith(SKIP_MESH):
                continue
            xfs = row['inst'] if 'inst' in row else [row['xf']]
            for k, xf in enumerate(xfs):
                self.items.append({'a': row['a'], 'c': row['c'], 'folder': row['folder'], 'cls': row['cls'], 'm': row['m'],
                                   'k': k if 'inst' in row else -1, 'xf': list(xf), 'drop': 0.0})
                self.items[-1]['loose'] = is_loose(self.items[-1], mesh)

    def pts(self, m):
        if m not in self.points:
            self.points[m] = mesh_points(m)
        return self.points[m]

    def cells(self, item):
        """lowest geometry per cell of one instance -> (ci, cj, zmin) arrays"""
        xf = item['xf']
        p = self.pts(item['m']) * np.array(xf[7:10])
        p = p @ rot_matrix(xf[3:7]).T + np.array(xf[0:3])
        p[:, 2] -= item['drop']
        ci = np.floor((p[:, 0] - ORG) / CELL).astype(np.int64)
        cj = np.floor((p[:, 1] - ORG) / CELL).astype(np.int64)
        ok = (ci >= 0) & (ci < NC) & (cj >= 0) & (cj < NC)
        if not ok.any():
            return None
        key = cj[ok] * NC + ci[ok]
        order = np.argsort(key, kind='stable')
        key, z = key[order], p[ok, 2][order]
        first = np.concatenate(([True], key[1:] != key[:-1]))
        starts = np.flatnonzero(first)
        zmin = np.minimum.reduceat(z, starts)
        uk = key[starts]
        return uk % NC, uk // NC, zmin

    def evaluate(self, Z):
        """-> global lowest map, owner map, per-item base cells"""
        low = np.full((NC, NC), np.inf)
        owner = np.full((NC, NC), -1, dtype=np.int32)
        per = []
        for n, item in enumerate(self.items):
            got = self.cells(item)
            per.append(got)
            if got is None:
                continue
            ci, cj, zmin = got
            better = zmin < low[cj, ci]
            low[cj[better], ci[better]] = zmin[better]
            owner[cj[better], ci[better]] = n
        cx = ORG + (np.arange(NC) + 0.5) * CELL
        gx, gy = np.meshgrid(cx, cx)
        ground = ground_at(Z, gx, gy)
        gap = low - ground
        flagged = np.zeros((NC, NC), dtype=bool)
        # A gap shows only at the outline of what covers the ground: a floating cell counts when one of its eight
        # neighbours has nothing near the ground (the inside of a floor sheet or a solid block is never seen).
        covered = np.isfinite(low) & (gap <= CAP)
        pad = np.pad(covered, 1, constant_values=False)
        open_side = np.zeros((NC, NC), dtype=bool)
        for dj in (0, 1, 2):
            for di in (0, 1, 2):
                open_side |= ~pad[dj:dj + NC, di:di + NC]
        self.loose_flag = np.zeros((NC, NC), dtype=bool)
        self.fixed_low = np.full((NC, NC), np.inf)       # lowest geometry of fixed pieces that is not under ground
        stats = []
        for n, got in enumerate(per):
            if got is None:
                stats.append(None)
                continue
            ci, cj, zmin = got
            mine = owner[cj, ci] == n
            base = mine & (zmin <= zmin.min() + BASE)
            g = gap[cj, ci]
            all_float = base & (g > FLOAT_MIN) & (g <= CAP)
            fl = all_float & open_side[cj, ci]
            if self.items[n]['loose']:
                self.loose_flag[cj[fl], ci[fl]] = True
            else:
                flagged[cj[fl], ci[fl]] = True
            # the cap protects floors and flat things; walls and heaps may stand a little deeper in the ground
            if not self.items[n]['loose'] and not self.items[n]['folder'].startswith(
                    ('Berlin/Ruins/B', 'Berlin/Ruins/E', 'Berlin/Cover')):
                up = g > -5.0
                # a footprint sheet's rubble skirt dips 15-35 cm below its floor: the ground may come up into the skirt
                allow = 20.0 if self.items[n]['folder'].endswith('Footprints') else (
                    12.0 if self.items[n]['folder'].endswith('Heaps') else 3.0)
                lim = zmin[up] + allow
                if self.items[n]['folder'].endswith('Footprints'):
                    # ... and anywhere up to just under its floor (local z about 6-8 cm), so a ragged rim can be filled
                    lim = np.maximum(lim, self.items[n]['xf'][2] + 3.0 * self.items[n]['xf'][9])
                self.fixed_low[cj[up], ci[up]] = np.minimum(self.fixed_low[cj[up], ci[up]], lim)
            nb = int(base.sum())
            stats.append({'base': nb, 'float': int(fl.sum()), 'max': float(g[fl].max()) if fl.any() else 0.0,
                          'min_base_gap': float(g[base].min()) if nb else 0.0,
                          'whole': bool(nb and all_float.sum() == nb),
                          'own': float(mine.sum()) / len(mine),
                          'share': float(all_float.sum()) / nb if nb else 0.0,
                          'mid': float(np.percentile(g[all_float], 60)) if all_float.any() else 0.0,
                          'max_base_gap': float(g[base].max()) if nb else 0.0,
                          'bury': float(-g[base].min()) if nb else 0.0})
        return low, owner, gap, flagged, stats

    def report(self, flagged, gap, stats, tag):
        by = {}
        worst = []
        for item, s in zip(self.items, stats):
            if not s or not s['float']:
                continue
            f = item['folder'] or 'None'
            e = by.setdefault(f, [0, 0, 0.0])
            e[0] += 1
            e[1] += s['float']
            e[2] = max(e[2], s['max'])
            worst.append((s['float'] * min(s['max'], 100.0), item['a'], item['c'], s['float'], round(s['max'], 1),
                          round(item['xf'][0]), round(item['xf'][1]), self.data['meshes'][item['m']].split('.')[-1]))
        worst.sort(reverse=True)
        g = gap[flagged]
        print('[%s] floating cells %d (%.0f m2), pieces %d; gap median %.1f, p90 %.1f, max %.1f cm' % (
            tag, int(flagged.sum()), flagged.sum() * CELL * CELL / 1e4, len(worst),
            float(np.median(g)) if g.size else 0, float(np.percentile(g, 90)) if g.size else 0, float(g.max()) if g.size else 0))
        for f in sorted(by, key=lambda k: -by[k][1]):
            print('   %-28s pieces %4d  cells %6d  max %.0f cm' % (f, by[f][0], by[f][1], by[f][2]))
        return worst

    def picture(self, gap, flagged, name):
        img = np.zeros((NC, NC, 3), dtype=np.uint8) + 30
        g = np.clip(gap, 0, 60) / 60.0
        img[flagged, 0] = (120 + 135 * g[flagged]).astype(np.uint8)
        img[flagged, 1] = (200 * (1 - g[flagged])).astype(np.uint8)
        # north (+X) up, east (+Y) right
        Image.fromarray(np.rot90(img.transpose(1, 0, 2), 1)[:, :, :]).save(OUT + name)


def dilate_falloff(delta, reach_cells, block):
    """spreads the raise outwards: each vertex takes the largest neighbour value scaled by a smooth falloff"""
    out = delta.copy()
    r = reach_cells
    src = delta
    js, is_ = np.nonzero(src > 0)
    if not len(js):
        return out
    for dj in range(-r, r + 1):
        for di in range(-r, r + 1):
            d = math.hypot(di, dj)
            if d == 0 or d > r:
                continue
            t = 1.0 - d / (r + 0.5)
            w = t * t * (3 - 2 * t)
            jj, ii = js + dj, is_ + di
            ok = (jj >= 0) & (jj < NV) & (ii >= 0) & (ii < NV)
            np.maximum.at(out, (jj[ok], ii[ok]), src[js[ok], is_[ok]] * w)
    out[block & (delta <= 0)] = 0.0
    return out


def main(mode):
    Z = load_heights(sys.argv[2] if len(sys.argv) > 2 else 'full_height.r16')
    scan = Scan()
    print('instances scanned', len(scan.items))
    low, owner, gap, flagged, stats = scan.evaluate(Z)
    worst = scan.report(flagged, gap, stats, 'before')
    print('loose floating-edge cells before: %d; pieces with base > 60 cm under ground before: %d' % (
        int(scan.loose_flag.sum()), sum(1 for s in stats if s and s['bury'] > 60.0)))
    if mode != 'plan':
        scan.picture(gap, flagged | scan.loose_flag, 'gap_map_%s.png' % (sys.argv[2][:-11] if len(sys.argv) > 2 else 'before'))
    json.dump({'worst': worst[:400]}, open(OUT + 'scan.json', 'w'), indent=0)
    for row in worst[:25]:
        print('  ', row[1:])
    if mode != 'plan':
        return
    # 1. loose pieces whose whole base floats are lowered onto the ground
    drops = 0
    for item, s in zip(scan.items, stats):
        if not (s and item['loose'] and s['float'] and s['base'] <= DROP_MAX_CELLS):
            continue
        if s['own'] < 0.85:
            continue                                        # it rests on another piece (a slab on a wall, a turret)
        if s['min_base_gap'] > FLOAT_MIN and s['min_base_gap'] <= CAP:
            item['drop'] = s['min_base_gap'] + 2.0          # wholly in the air: down until it touches
            if s['max_base_gap'] - s['min_base_gap'] > 6.0:  # on a slope: a little further, into the ground
                item['drop'] += min(0.6 * (s['max_base_gap'] - s['min_base_gap']), 30.0)
            if item['drop'] > 60.0:
                print('   not lowered (more than 60 cm in the air, to be looked at): %s #%d at %d, %d: %.0f cm' % (
                    item['a'], item['k'], item['xf'][0], item['xf'][1], item['drop']))
                item['drop'] = 0.0
                continue
            drops += 1
        elif 'nosink' not in sys.argv:
            item['drop'] = min(s['mid'], 30.0)              # one side in the air: sink it in
            drops += 1
    # an actor moves as one: all of its components take the move of the one with the largest base
    groups = {}
    for item, s in zip(scan.items, stats):
        if item['k'] < 0 and s:
            groups.setdefault(item['a'], []).append((s['base'], item))
    for members in groups.values():
        if len(members) > 1:
            lead = max(members, key=lambda e: e[0])[1]
            for _b, item in members:
                item['drop'], item['lead'] = lead['drop'], lead
    for item in scan.items:
        item['drop0'] = item['drop']
    low, owner, gap, flagged, stats = scan.evaluate(Z)
    print('lowered pieces', drops)
    scan.report(flagged, gap, stats, 'after drops')
    # 2. the ground comes up to what still floats. It never rises above the underside of a fixed piece that is not
    #    already under ground (floor sheets, slabs): cap per vertex = lowest such geometry in the 16 cells around it.
    cobble = np.fromfile(OUT + 'full_BerlinCobble.r8', dtype=np.uint8).reshape(NV, NV)
    idx = np.arange(NV)
    edge = np.minimum(idx, NV - 1 - idx)
    rim = np.minimum(edge[:, None], edge[None, :]) < 32
    water = Z < -150.0
    wet = np.zeros_like(water)
    wet |= water        # only the pit and river floors themselves: a rim under an overhanging plate may come up
    hard_block = rim | wet

    def vertex_cap():
        fl = np.pad(scan.fixed_low, 2, constant_values=np.inf)
        cap = np.full((NV, NV), np.inf)
        for dj in range(4):
            for di in range(4):
                # vertex (j, i) touches cells 2j-2..2j+1
                part = fl[dj:dj + 2 * NV:2, di:di + 2 * NV:2][:NV, :NV]
                if part.shape != (NV, NV):
                    part = np.pad(part, ((0, NV - part.shape[0]), (0, NV - part.shape[1])), constant_values=np.inf)
                cap = np.minimum(cap, part)
        return cap

    newZ = Z.copy()
    for rnd in range(5):
        cap = np.maximum(vertex_cap() + 1.0, Z)
        want = np.full((NV, NV), -np.inf)
        cj, ci = np.nonzero(flagged)
        target = low[cj, ci] + 1.0
        x, y = (ci + 0.5) * CELL / QUAD, (cj + 0.5) * CELL / QUAD
        for oi in (0, 1):
            for oj in (0, 1):
                vi = np.clip(np.floor(x).astype(int) + oi, 0, NV - 1)
                vj = np.clip(np.floor(y).astype(int) + oj, 0, NV - 1)
                np.maximum.at(want, (vj, vi), target)
        direct = np.where(np.isfinite(want), np.maximum(np.minimum(want, cap) - newZ, 0.0), 0.0)
        direct[hard_block] = 0.0
        blocked_cells = int((np.isfinite(want) & hard_block).sum())
        soft_block = hard_block | (cobble > 110)
        delta = dilate_falloff(direct, 3, soft_block)
        newZ = np.minimum(newZ + delta, np.maximum(cap, newZ))
        # loose things ride on the ground
        total = newZ - Z
        for item in scan.items:
            if item['loose']:
                d = float(ground_at(total, np.array(item['xf'][0]), np.array(item['xf'][1])))
                item['lift'] = d
        for item in scan.items:
            if item['loose'] and 'lead' in item:
                item['lift'] = item['lead']['lift']
        for item in scan.items:
            if item['loose']:
                item['drop'] = item['drop0'] - item['lift']
        low, owner, gap, flagged, stats = scan.evaluate(newZ)
        scan.report(flagged, gap, stats, 'round %d' % (rnd + 1))
        if not flagged.any():
            break
    total = newZ - Z
    changed = total > 0.5
    print('vertices raised %d (%.2f %% of the map), max raise %.0f cm, mean raise where raised %.1f cm' % (
        int(changed.sum()), 100.0 * changed.mean(), float(total.max()), float(total[changed].mean()) if changed.any() else 0))
    print('raise on cobble (lane) vertices: %d, max %.0f cm' % (int((changed & (cobble > 110)).sum()),
                                                               float(total[cobble > 110].max())))
    # newly buried: base cells that the new ground now covers by more than 12 cm and did not before
    scan.picture(gap, flagged, 'gap_map_after.png')
    np.save(OUT + 'delta.npy', total)
    h16 = np.clip(np.round(newZ / 100.0 * 128.0 + 32768.0), 0, 65535).astype(np.uint16)
    h16.tofile(OUT + 'new_height.r16')
    js, is_ = np.nonzero(changed)
    plan = {'drops': [{'a': it['a'], 'c': it['c'], 'k': it['k'], 'drop': round(it['drop'], 1), 'xy': it['xf'][:2]}
                      for it in scan.items if abs(it['drop']) > 0.5],
            'bbox': [int(is_.min()), int(js.min()), int(is_.max()), int(js.max())] if len(js) else None}
    if plan['bbox']:
        i0, j0, i1, j1 = plan['bbox']
        h16[j0:j1 + 1, i0:i1 + 1].tofile(OUT + 'sub_height.r16')
        for name in open(OUT + 'grid.txt').read().splitlines()[3].split():
            np.fromfile(OUT + 'full_%s.r8' % name, dtype=np.uint8).reshape(NV, NV)[j0:j1 + 1, i0:i1 + 1].tofile(
                OUT + 'sub_%s.r8' % name)
    json.dump(plan, open(OUT + 'plan.json', 'w'))
    json.dump([e for e in worst[:400]], open(OUT + 'worst_before.json', 'w'))
    print('loose pieces still with a floating edge: cells %d' % int(scan.loose_flag.sum()))
    print('flagged vertices inside the rim band or at the water (not raised): %d' % blocked_cells)
    left = [(it['a'], it['c'], s['float'], round(s['max']), round(it['xf'][0]), round(it['xf'][1]),
             scan.data['meshes'][it['m']].split('.')[-1]) for it, s in zip(scan.items, stats)
            if s and s['float'] and not it['loose']]
    left.sort(key=lambda e: -e[2])
    buried = [(it['a'], it['c'], round(s['bury'])) for it, s in zip(scan.items, stats) if s and s['bury'] > 60.0]
    print('pieces whose base is more than 60 cm under ground now: %d' % len(buried))
    print('still floating after the plan: %d pieces' % len(left))
    for e in left[:60]:
        print('  ', e)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'scan')
