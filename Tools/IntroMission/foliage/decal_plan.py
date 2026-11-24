"""Cliff stone decals for Ger_IntroMission (user 2026-11-20: small stones of the cliff rock type as decals, above and
below the cliffs, never on the cliff stones). Offline: numpy + OpenCV.

python decal_plan.py --round 4  -> Saved/IntroFoliage/decals/decals_r<N>.json (+ decals_r<N>.jpg)

Where: a band just outside the cliff kit (REAR_Cliff_* / RIDGE_Cliff_* instances). "Below" = ground lower than the
nearby cliff mass (the foot: scree tongues, strips, fallen blocks, chips), "above" = higher (the lip: cracked-off plates,
gravel). Each decal's whole rotated rectangle, rasterised on survey/cover.png (17.6 cm per pixel, show-only capture of
every cliff / mountain mesh from above) and dilated 40 cm, must miss every covered pixel: DBuffer decals paint whatever
mesh is inside the box. Same keep-outs as the crushable patches (roads, mission routes, track stripes, water, resource
fields, lanes, POIs) plus the crushable patch outlines (their runtime ISMs would receive the stones).
"""
import argparse
import json
import math
import os

import cv2
import numpy as np

import fol_plan as fp

ROOT = fp.ROOT
SPEC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCliffKit/Decals/CliffStones/decals.json'
OUT = ROOT + 'decals/'
CLIFF_PREFIX = ('REAR_Cliff_', 'RIDGE_Cliff_')
CW = 4096                                        # cover.png: 72000 cm square, centre (-5100, 300)


def cover_px(x, y):
    return ((np.asarray(y) - 300.0) / 72000.0 + 0.5) * CW, (0.5 - (np.asarray(x) + 5100.0) / 72000.0) * CW


# per decal: weight below the cliff (foot), weight above (lip), orient image top to the cliff
# review r4: the single-stone types read, chips / gravel are specks -> weighted to the blocks; fans only below,
# plates only above
USE = {'FallenBlock': (1.4, 0.2, False), 'BlockPair': (1.2, 0.1, False), 'ChipScatter': (0.7, 0.8, False),
       'ScreeFan': (0.9, 0.0, True), 'RubbleStrip': (1.0, 0.0, True), 'HalfBuriedSlab': (0.8, 0.8, False),
       'FineGravel': (0.2, 0.4, False), 'LipFragments': (0.0, 1.4, False)}


def load_spec():
    doc = json.load(open(SPEC))
    out = {}
    for d in doc['decals']:
        b, a, o = USE.get(d['name'], (0.5, 0.5, False))
        out[d['name']] = dict(d, below=b, above=a, orient='cliff' if o else None)
    return out


class Site:
    def __init__(self, cfg):
        self.f = fp.Field()
        f = self.f
        cov = cv2.imread(fp.SURVEY + 'cover.png')
        self.cover = (cov.sum(-1) > 30).astype(np.uint8)
        k = int(round(cfg['stone_gap_m'] * CW / 720.0))
        self.cover_d = cv2.dilate(self.cover, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * k + 1, 2 * k + 1)))
        # cliff kit on the 1 m grid, and every other footprint (POIs, buildings, props; not the cliffs, not our foliage)
        cliff = np.zeros((fp.NJ, fp.NI), np.uint8)
        other = np.zeros((fp.NJ, fp.NI), np.uint8)
        for r in fp.load_footprints():
            if r['label'].startswith(('FOL_', 'Plane', 'REAR_Water')) or not fp.actor_is_footprint(r) and \
                    not r['label'].startswith(CLIFF_PREFIX):
                continue
            if r['comp'] == 'SplineMeshComponent' and ('Road' in r['class'] or 'GroundPath' in r['class']):
                continue
            i0, j0 = fp.to_ij(r['cx'] - r['ex'], r['cy'] - r['ey'])
            i1, j1 = fp.to_ij(r['cx'] + r['ex'], r['cy'] + r['ey'])
            tgt = cliff if r['label'].startswith(CLIFF_PREFIX) else other
            cv2.rectangle(tgt, (int(math.floor(i0)), int(math.floor(j0))), (int(math.ceil(i1)), int(math.ceil(j1))), 1, -1)
        self.cliff = cliff.astype(bool)
        self.cliff_d = fp.dist_to(self.cliff)
        self.other_d = fp.dist_to(other.astype(bool))
        # crushable patch outlines + POI fields (their runtime plants would receive the decal)
        pm = np.zeros((fp.NJ, fp.NI), np.uint8)
        plan = json.load(open(ROOT + 'plan_r%d.json' % cfg['round']))
        polys = [p['poly'] for p in plan['patches']]
        poi = json.load(open(ROOT + 'poi/poi_r%d.json' % cfg['round']))
        for it in poi['items']:
            if it['kind'] == 'field':
                polys.append(it['outline'])
        for poly in polys:
            cv2.fillPoly(pm, [np.round(np.array([fp.to_ij(x, y) for x, y in poly])).astype(np.int32)], 1)
        self.patch_d = fp.dist_to(pm.astype(bool))
        self.poi = [it for it in poi['items'] if it['kind'] not in ('cards', 'field', 'fence') and it['half'][0]]
        # local cliff height: median ground z over the cliff kit cells within 7 m
        zc = np.where(self.cliff, f.z, np.nan).astype(np.float32)
        self.cliff_z = self._nanmedian_filter(zc, 7)

    @staticmethod
    def _nanmedian_filter(a, r):
        """Mean of the non-nan cells in a (2r+1) box (a median is too slow on the full grid; the mean is fine here)."""
        valid = (~np.isnan(a)).astype(np.float32)
        s = cv2.boxFilter(np.nan_to_num(a), -1, (2 * r + 1, 2 * r + 1), normalize=False)
        n = cv2.boxFilter(valid, -1, (2 * r + 1, 2 * r + 1), normalize=False)
        return np.where(n > 0, s / np.maximum(n, 1e-6), np.nan)

    def rect_clear(self, x, y, yaw, w_m, h_m):
        """True when the decal rectangle (w across, h along +V) misses every dilated cover pixel."""
        a = math.radians(yaw)
        ax = np.array([math.cos(a), math.sin(a)])      # decal U axis in world (x, y)
        ay = np.array([-math.sin(a), math.cos(a)])     # decal V axis
        corners = [np.array([x, y]) + ax * su * w_m * 50 + ay * sv * h_m * 50
                   for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        pts = np.array([cover_px(cx, cy) for cx, cy in corners], np.float32)
        x0, y0 = np.floor(pts.min(0)).astype(int) - 1
        x1, y1 = np.ceil(pts.max(0)).astype(int) + 2
        if x0 < 0 or y0 < 0 or x1 >= CW or y1 >= CW:
            return False
        m = np.zeros((y1 - y0, x1 - x0), np.uint8)
        cv2.fillPoly(m, [np.round(pts - [x0, y0]).astype(np.int32)], 1)
        return not (m.astype(bool) & self.cover_d[y0:y1, x0:x1].astype(bool)).any()

    def far_side_z(self, i, j, vx, vy, max_m=30):
        """Ground z just past the cliff along (vx, vy) (towards and through the cliff kit), or None."""
        n = math.hypot(vx, vy)
        vx, vy = vx / n, vy / n
        entered = False
        for s in range(1, max_m * 2):
            fi, fj = i + vx * s * 0.5, j + vy * s * 0.5
            ci, cj = int(round(fi)), int(round(fj))
            if not (0 <= ci < fp.NI and 0 <= cj < fp.NJ):
                return None
            if self.cliff[cj, ci]:
                entered = True
            elif entered:
                fi2, fj2 = fi + vx * 1.5, fj + vy * 1.5
                if not (0 <= int(fi2) < fp.NI - 1 and 0 <= int(fj2) < fp.NJ - 1):
                    return None
                return fp.bilinear(self.f.z, fi2, fj2)
        return None

    @staticmethod
    def rect_points(x, y, yaw, w_m, h_m):
        a = math.radians(yaw)
        ax, ay = (math.cos(a), math.sin(a)), (-math.sin(a), math.cos(a))
        pts = []
        for su in (-1, -0.5, 0, 0.5, 1):
            for sv in (-1, -0.5, 0, 0.5, 1):
                pts.append((x + (ax[0] * su * w_m + ay[0] * sv * h_m) * 50,
                            y + (ax[1] * su * w_m + ay[1] * sv * h_m) * 50))
        return pts

    def rect_ground_ok(self, x, y, yaw, w_m, h_m, cfg):
        """Review r4 S3/S4: the ground under the whole rectangle stays inside the +-box (z0 = ground at the centre),
        and every sample is outside lanes / base / mission spots, off steep ground, clear of routes / roads / stripes.
        Returns None when fine, else the reason."""
        f = self.f
        i0, j0 = fp.to_ij(x, y)
        z0 = fp.bilinear(f.z, i0, j0)
        for px, py in self.rect_points(x, y, yaw, w_m, h_m):
            fi, fj = fp.to_ij(px, py)
            ci, cj = int(round(fi)), int(round(fj))
            if not (0 <= ci < fp.NI - 1 and 0 <= cj < fp.NJ - 1):
                return 'edge'
            if abs(fp.bilinear(f.z, fi, fj) - z0) > cfg['box_tolerance_cm']:
                return 'box'
            if f.slope[cj, ci] > cfg['slope'] or f.lanes[cj, ci] or f.base[cj, ci] or f.spots[cj, ci]:
                return 'zone'
            if f.route_d[cj, ci] < cfg['route'] or f.road_edge[cj, ci] < cfg['road'] or f.stripe_d[cj, ci] < cfg['stripe']:
                return 'path'
        return None

    def ok_cell(self, j, i, r_m, cfg):
        f = self.f
        if not f.play[j, i] or f.spots[j, i] or f.base[j, i] or f.lanes[j, i]:
            return False
        if f.road_edge[j, i] < r_m + cfg['road'] or f.route_d[j, i] < r_m + cfg['route']:
            return False
        if f.stripe_d[j, i] < r_m + cfg['stripe'] or f.water_d[j, i] < r_m + cfg['water']:
            return False
        if f.resource_d[j, i] < r_m + cfg['resource'] or self.other_d[j, i] < r_m + cfg['foot']:
            return False
        if self.patch_d[j, i] < r_m + cfg['patch'] or f.slope[j, i] > cfg['slope']:
            return False
        if f.hard_d[j, i] < r_m + cfg['hard']:
            return False
        x, y = fp.to_xy(i, j)
        for it in self.poi:
            if math.hypot(x - it['x'], y - it['y']) < max(it['half']) * it.get('scale', 1.0) + (r_m + 1.0) * 100:
                return False
        return True


def plan(cfg):
    spec = load_spec()
    site = Site(cfg)
    f = site.f
    rng = np.random.default_rng(4040 + cfg['round'])
    names = list(spec)
    below_w = np.array([spec[n].get('below', 1.0) for n in names], float)
    above_w = np.array([spec[n].get('above', 0.3) for n in names], float)
    grad_j, grad_i = np.gradient(site.cliff_d)
    band = (site.cliff_d > 0.5) & (site.cliff_d < cfg['band_m'])
    jj, ii = np.nonzero(band)
    order = rng.permutation(len(jj))
    placed = []
    rejects = {}
    for k in order:
        j, i = int(jj[k]), int(ii[k])
        if len(placed) >= cfg['max_decals']:
            break
        if rng.random() > cfg['density']:
            continue
        x, y = fp.to_xy(i + rng.uniform(0, 1) - 0.5, j + rng.uniform(0, 1) - 0.5)
        if any(math.hypot(x - p['x'], y - p['y']) < cfg['spacing_m'] * 100 for p in placed):
            continue
        vx, vy = -float(grad_i[j, i]), -float(grad_j[j, i])      # towards the nearest cliff cell
        if abs(vx) + abs(vy) < 1e-6:
            continue
        zf = site.far_side_z(i, j, vx, vy)
        if zf is None:
            continue
        # review r4 S2: compare with the ground on the far side of the rock (the mean under the rock sat halfway down
        # the drop); cliffs on flat ground (both sides level) = 'beside'
        dz = float(f.z[j, i] - zf)
        side = 'below' if dz < -cfg['dz_m'] * 100 else ('above' if dz > cfg['dz_m'] * 100 else 'beside')
        w8 = above_w if side == 'above' else below_w
        name = names[rng.choice(len(names), p=w8 / w8.sum())]
        d = spec[name]
        sc = float(rng.uniform(*cfg['scale']))
        w_m, h_m = d['size_m'][0] * sc, d['size_m'][1] * sc
        r_m = 0.5 * math.hypot(w_m, h_m)
        if not site.ok_cell(j, i, r_m * 0.6, cfg):
            rejects['keepout'] = rejects.get('keepout', 0) + 1
            continue
        # oriented decals (fan, strip): image top (+V) towards the cliff; the rest any yaw
        if d.get('orient') == 'cliff':
            yaw = math.degrees(math.atan2(vy, vx)) - 90.0 + rng.uniform(-20, 20)
        else:
            yaw = rng.uniform(0, 360)
        if not site.rect_clear(x, y, yaw, w_m, h_m):
            rejects['on_stones'] = rejects.get('on_stones', 0) + 1
            continue
        # review r4 S6: the stones lay a median 2.6 m off the rock; slide towards the cliff while the rectangle stays
        # clear of the (dilated) stones, so the scree reaches the foot
        n = math.hypot(vx, vy)
        ux, uy = vx / n * 100.0 * cfg['slide_step_m'], vy / n * 100.0 * cfg['slide_step_m']
        for _ in range(int(cfg['slide_max_m'] / cfg['slide_step_m'])):
            if not site.rect_clear(x + ux, y + uy, yaw, w_m, h_m):
                break
            x, y = x + ux, y + uy
        if any(math.hypot(x - p['x'], y - p['y']) < cfg['spacing_m'] * 100 for p in placed):
            continue
        why = site.rect_ground_ok(x, y, yaw, w_m, h_m, cfg)
        if why:
            rejects[why] = rejects.get(why, 0) + 1
            continue
        i, j = (int(round(v)) for v in fp.to_ij(x, y))
        placed.append({'name': name, 'x': round(x), 'y': round(y), 'z': round(float(fp.bilinear(f.z, i, j))),
                       'yaw': round(yaw % 360, 1), 'w_m': round(w_m, 2), 'h_m': round(h_m, 2), 'side': side,
                       'slope': round(float(f.slope[j, i]), 1)})
    os.makedirs(OUT, exist_ok=True)
    out = {'round': cfg['round'], 'cfg': cfg, 'decals': placed, 'rejects': rejects,
           'counts': {n: sum(p['name'] == n for p in placed) for n in names},
           'sides': {s: sum(p['side'] == s for p in placed) for s in ('above', 'below', 'beside')}}
    json.dump(out, open(OUT + 'decals_r%d.json' % cfg['round'], 'w'), indent=1)
    picture(site, placed, cfg['round'])
    print('decals', len(placed), out['sides'], out['counts'], 'rejects', rejects)
    return out


def picture(site, placed, rnd):
    top = cv2.imread(fp.SURVEY + 'top_full.png')
    H, W = top.shape[:2]
    img = top.copy()
    cov = cv2.resize(site.cover * 255, (W, H), interpolation=cv2.INTER_NEAREST)
    img[cov > 0] = (img[cov > 0] * 0.6 + np.array([90, 90, 200]) * 0.4).astype(np.uint8)
    for p in placed:
        # top_full.png: same framing as cover.png
        px, py = cover_px(p['x'], p['y'])
        px, py = px * W / CW, py * H / CW
        rr = max(2, int(0.5 * max(p['w_m'], p['h_m']) * W / 720.0))
        col = {'above': (255, 255, 0), 'below': (0, 165, 255), 'beside': (255, 0, 255)}[p['side']]   # BGR
        cv2.circle(img, (int(px), int(py)), rr, col, 2)
    cv2.imwrite(OUT + 'decals_r%d.jpg' % rnd, cv2.resize(img, (W // 2, H // 2)), [cv2.IMWRITE_JPEG_QUALITY, 85])


CFG = {
    'round': 4,
    'band_m': 9.0,             # decal centres within this of the cliff kit
    'stone_gap_m': 0.4,        # dilation of the cover mask the decal rectangle must miss
    'dz_m': 1.5,               # ground this far below / above the far side of the rock = foot / lip, else beside
    'slide_step_m': 0.25,
    'slide_max_m': 6.0,
    'box_tolerance_cm': 45.0,  # decal_apply DEPTH_HALF 60
    'density': 0.26,
    'spacing_m': 6.0,
    'max_decals': 260,
    'scale': (1.4, 2.0),            # PIE: at 1.0 the stones were 2-3 px from the RTS camera
    'road': 1.5, 'route': 4.0, 'stripe': 1.0, 'water': 1.0, 'resource': 4.0, 'foot': 0.5, 'patch': 0.5, 'slope': 30,
    'hard': 0.5,
}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--round', type=int, default=4)
    a = ap.parse_args()
    CFG['round'] = a.round
    plan(CFG)
