"""Offline planner for the Ger_IntroMission foliage pass (plain python + numpy + cv2; no editor).

Inputs (Saved/IntroFoliage/survey, written by fol_survey.py / fol_nav.py in the throwaway editor):
  z.npy, slope.npy       full landscape height (cm) / slope (deg), 1 m grid, shape (511 j=y, 721 i=x)
  weights.npz            paint layers (Sand, DesertRockDark, Weedy, DesertGravel, ScorchSandyStone, ScorchConcrete)
  actors.tsv             every actor: label, class, folder, location, AABB centre + half extent
  splines.json           roads, two-tracks, rails, trench (sampled every 5 m)
  ../nav_baseline.json   the mission routes for all four nav agents

Outputs (Saved/IntroFoliage/):
  plan_r<N>.json         small detail instances per mesh + crushable scrub patches (outline polygons)
  plan_r<N>.png          review picture (keep-outs, attraction, instances, patches) on the base-colour top view
  masks_r<N>.npz         keep-out distances used (for the adversarial review to recompute)

    python fol_plan.py --round 1
Everything is seeded: the same round gives the same plan.
"""
import argparse
import csv
import json
import math
import os

import cv2
import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/'
SURVEY = ROOT + 'survey/'
ORIGIN = (-51600.0, -32701.0)
I0, J0 = 105, 75                       # first exported vertex
NI, NJ = 721, 511
PLAY = (-32700.0, 24600.0, -24200.0, 24950.0)   # x0, x1, y0, y1 inside the map blockers

GF = '/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/'
DETAIL_MESHES = {
    # mesh: (share, min scale, max scale, align to slope, sink cm). Round 3 (user: the yellow straw/tuft cards do not fit
    # the sand shader): concept A "Aschgras" = the radial star cards in an ash-tan tint, concept C "Rostquecke" = the
    # flat litter cards in a rust-umber tint (a darker, fibrous tone of the sand). Materials: DETAIL_MATERIALS.
    GF + 'MeshesTop/SM_GFT_StrawLitter': (0.30, 2.2, 3.6, True, 1.0),
    GF + 'Meshes/SM_GF_DryLitter': (0.22, 2.0, 3.2, True, 1.0),
    GF + 'MeshesTop/SM_GFT_DryStar_B': (0.26, 1.8, 3.0, True, 1.0),
    GF + 'MeshesTop/SM_GFT_DryStar_A': (0.22, 1.8, 3.0, True, 1.0),
}
DS = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/DryScrub/'
DETAIL_MATERIALS = {
    GF + 'MeshesTop/SM_GFT_StrawLitter': DS + 'MI_DryScrub_RustMatTop',
    GF + 'Meshes/SM_GF_DryLitter': DS + 'MI_DryScrub_RustMat',
    GF + 'MeshesTop/SM_GFT_DryStar_B': DS + 'MI_DryScrub_AshGrassTop',
    GF + 'MeshesTop/SM_GFT_DryStar_A': DS + 'MI_DryScrub_AshGrassTop',
    # review r3: cards in the dried pit read golden on the grey mud; only greyed tussocks there ('#pit' = own HISMs)
    GF + 'MeshesTop/SM_GFT_DryStar_B#pit': DS + 'MI_DryScrub_AshGrassPit',
    GF + 'MeshesTop/SM_GFT_DryStar_A#pit': DS + 'MI_DryScrub_AshGrassPit',
}
PIT_BOX = (-13000, -1000, 7000, 20000)     # east quarry lake bowl (world x0, x1, y0, y1)
PIT_Z = -650.0                              # below the rim (rim -600..-30, floor -1900)


def in_pit(f, i, j, x, y):
    return PIT_BOX[0] < x < PIT_BOX[1] and PIT_BOX[2] < y < PIT_BOX[3] and f.z[j, i] < PIT_Z


def patch_layout(bp, area_m2, rng):
    """Review r3: fixed 3.3 m spacing left small patches with 1-2 plants (one empty). Spacing per patch for 6-10
    plants; a third of the brush patches get smaller shrubs (scale 1.4-2.6) for size variety."""
    if bp == 'Thistle':
        n, miss, lo, hi = rng.integers(7, 12), 0.3, 1.3, 2.1
        smin = float(rng.uniform(1.25, 1.45))
        return {'spacing': round(100 * float(np.clip(math.sqrt(area_m2 * 0.7 * (1 - miss) / n), lo, hi))),
                'smin': round(smin, 2), 'smax': round(smin + 0.5, 2)}
    n, miss, lo, hi = rng.integers(6, 11), 0.35, 1.7, 3.3
    small = rng.random() < 0.35
    smin = float(rng.uniform(1.4, 1.8) if small else rng.uniform(2.2, 2.6))
    return {'spacing': round(100 * float(np.clip(math.sqrt(area_m2 * 0.7 * (1 - miss) / n), lo, hi))),
            'smin': round(smin, 2), 'smax': round(smin + (0.8 if small else 1.2), 2)}

# keep-out half widths (m) around each road class, measured from the spline centre line
ROAD_HALF = {'BP_RoadSpline_AirportStopWay1_C': 4.5, 'BP_GroundPathSpline_TwoTrack_C': 2.2, 'BP_RoadSpline_C': 3.5,
             'BP_SovFieldTrenchSpline_C': 3.0}

# mission spots that must stay clear (x, y, radius m)
MISSION_SPOTS = [(-30984, 3366, 16), (-30807, -2496, 18), (-15697, -1275, 22), (-4500, 300, 28), (1450, 300, 32),
                 (8400, -1500, 22), (-5780, -23100, 18), (-646, -266, 25), (-10382, -2416, 14), (-10382, -5375, 14),
                 (-13016, -7736, 14), (4640, -3500, 12), (-21200, 3500, 15), (-14000, -1300, 12)]
BASE_BOX = (-2750.0, 4250.0, -4400.0, 5400.0)
# combat / work lanes kept free of crushable patches (x0, x1, y0, y1): the west ramp approach and the fuel truck lane
LANE_BOXES = [(-9500.0, -4000.0, -2800.0, 2800.0), (3500.0, 5700.0, -6800.0, -600.0)]

# actors that plants like to grow against (rocks, trees, cacti, logs, wrecks, fences)
ATTRACT_KEYS = ('Rock', 'Rocky', 'Stone', 'Cactus', 'DeadTree', 'DeadWillow', 'Shrub', 'Log', 'Hedgehog', 'Fence',
                'Wreck', 'Pole', 'Barrier')
IGNORE_CLASSES = ('DecalActor', 'RecastNavMesh', 'NavMeshBoundsVolume', 'BlockingVolume', 'PostProcessVolume', 'SkyLight',
                  'ExponentialHeightFog', 'DirectionalLight', 'SkyAtmosphere', 'VolumetricCloud', 'SpotLight',
                  'PointLight', 'RuntimeVirtualTextureVolume', 'PlayerStart', 'WorldDataLayers', 'WorldSettings',
                  'Landscape', 'LandscapeStreamingProxy', 'LevelBounds', 'BrushShape', 'Note', 'TextRenderActor',
                  'CameraActor', 'TriggerBox', 'TriggerVolume', 'AbstractNavData')


# ---------------------------------------------------------------------------------------------------------------------
def to_ij(x, y):
    return (x - ORIGIN[0]) / 100.0 - I0, (y - ORIGIN[1]) / 100.0 - J0


def to_xy(i, j):
    return ORIGIN[0] + (i + I0) * 100.0, ORIGIN[1] + (j + J0) * 100.0


def dist_to(mask):
    """Metres from every cell to the nearest True cell of mask."""
    inv = np.where(mask, 0, 255).astype(np.uint8)
    return cv2.distanceTransform(inv, cv2.DIST_L2, 5)


def raster_polyline(pts_xy, thick=1):
    m = np.zeros((NJ, NI), np.uint8)
    p = np.array([to_ij(x, y) for x, y in pts_xy], np.float32)
    cv2.polylines(m, [np.round(p).astype(np.int32)], False, 1, thick)
    return m.astype(bool)


def fractal_noise(seed, scales, weights):
    rng = np.random.default_rng(seed)
    acc = np.zeros((NJ, NI), np.float32)
    for s, w in zip(scales, weights):
        small = rng.random((max(2, NJ // s + 2), max(2, NI // s + 2))).astype(np.float32)
        big = cv2.resize(small, (NI + 2 * s, NJ + 2 * s), interpolation=cv2.INTER_CUBIC)[s:s + NJ, s:s + NI]
        acc += w * big
    acc -= acc.min()
    return acc / max(acc.max(), 1e-6)


def load_actors():
    rows = []
    with open(SURVEY + 'actors.tsv', encoding='utf-8') as h:
        for r in csv.DictReader(h, delimiter='\t'):
            try:
                for k in ('x', 'y', 'z', 'bx', 'by', 'ex', 'ey', 'ez'):
                    r[k] = float(r[k])
            except (TypeError, ValueError):
                continue
            rows.append(r)
    return rows


def load_footprints():
    rows = []
    with open(SURVEY + 'footprints.tsv', encoding='utf-8') as h:
        for r in csv.DictReader(h, delimiter='	'):
            try:
                for k in ('cx', 'cy', 'ex', 'ey', 'top'):
                    r[k] = float(r[k])
            except (TypeError, ValueError):
                continue
            rows.append(r)
    return rows


def actor_is_footprint(r):
    if r['class'] in IGNORE_CLASSES or r['class'].startswith('RTS_BP_NavBlo') or 'NavBlocker' in r['class']:
        return False
    if r['label'].startswith(('REAR_Skirt', 'REAR_Edge', 'FOL_', 'POIW_', 'POIE_')):
        return False
    if r['ex'] > 8000 or r['ey'] > 8000:
        return False
    return True


# ---------------------------------------------------------------------------------------------------------------------
class Field:
    def __init__(self, skip_prefixes=('FOL_',)):
        self.skip_prefixes = tuple(skip_prefixes)
        self.z = np.load(SURVEY + 'z.npy')
        self.slope = np.load(SURVEY + 'slope.npy')
        w = np.load(SURVEY + 'weights.npz')
        self.w = {k: w[k] for k in w.files}
        self.actors = load_actors()
        with open(SURVEY + 'splines.json') as h:
            self.splines = json.load(h)
        with open(ROOT + 'nav_baseline.json') as h:
            self.nav = json.load(h)
        jj, ii = np.mgrid[0:NJ, 0:NI]
        self.X = ORIGIN[0] + (ii + I0) * 100.0
        self.Y = ORIGIN[1] + (jj + J0) * 100.0
        self.build_masks()

    # -- keep-outs ------------------------------------------------------------------------------------------------------
    def build_masks(self):
        X, Y = self.X, self.Y
        play = (X > PLAY[0]) & (X < PLAY[1]) & (Y > PLAY[2]) & (Y < PLAY[3]) & (self.z > -5000)
        play &= dist_to(self.z <= -5000) > 3.0           # landscape holes (review r1: cards on a hole edge)
        # ground hidden under collision-less meshes (desert mountains, cliff kits); review r2 found 114 crushables and
        # ~820 cards planted under S_Desert_Mountain. survey/cover.png = show-only capture of those meshes.
        cov = cv2.imread(SURVEY + 'cover.png')
        if cov is not None:
            Wc = cov.shape[0]
            px = (((Y - 300.0) / 72000.0 + 0.5) * Wc).astype(np.int32).clip(0, Wc - 1)
            py = ((0.5 - (X + 5100.0) / 72000.0) * Wc).astype(np.int32).clip(0, Wc - 1)
            covered = cov.sum(-1)[py, px] > 30
            self.covered = covered
            play &= dist_to(covered) > 1.5
        else:
            self.covered = np.zeros_like(play)
        self.play = play
        # roads: distance from the road EDGE (centre distance minus half width)
        road_edge = np.full((NJ, NI), 999.0, np.float32)
        for s in self.splines:
            half = ROAD_HALF.get(s['class'])
            if half is None or s['length'] < 300:
                continue
            d = dist_to(raster_polyline([(p[0], p[1]) for p in s['pts']])) - half
            road_edge = np.minimum(road_edge, d)
        self.road_edge = road_edge
        # mission routes, all agents
        route = np.zeros((NJ, NI), bool)
        for agent, routes in self.nav.items():
            for name, r in routes.items():
                if len(r['pts']) >= 2:
                    route |= raster_polyline(r['pts'])
        self.route_d = dist_to(route)
        # painted track stripes / gravel (the long light bands of DesertGravel + DesertRockDark)
        g = self.w['DesertGravel'] + self.w['DesertRockDark']
        self.gravel = g
        self.stripe_d = dist_to(g > 0.35)
        # concrete / charcoal paint
        hard = (self.w['ScorchConcrete'] > 0.25) | (self.w['ScorchSandyStone'] > 0.35)
        self.hard_d = dist_to(hard)
        # water: the lake plane, the pit plane and the two rear ponds (ground under water level + 0.4 m)
        wet = np.zeros((NJ, NI), bool)
        for r in self.actors:
            lab = r['label']
            if lab in ('Plane', 'Plane2') or lab.startswith('REAR_Water'):
                box = (X > r['bx'] - r['ex'] - 300) & (X < r['bx'] + r['ex'] + 300) & \
                      (Y > r['by'] - r['ey'] - 300) & (Y < r['by'] + r['ey'] + 300)
                wet |= box & (self.z < r['z'] + 40.0)
        self.wet = wet
        self.water_d = dist_to(wet)
        # actor footprints from visible components (footprints.tsv; actor bounds are polluted by child actors / GCs)
        foot = np.zeros((NJ, NI), np.uint8)
        attract = np.zeros((NJ, NI), np.uint8)
        fence = np.zeros((NJ, NI), np.uint8)
        for r in load_footprints():
            if r['comp'] == 'SplineMeshComponent' and ('Road' in r['class'] or 'GroundPath' in r['class']):
                continue        # roads: road_edge handles them
            if r['label'] in ('Plane', 'Plane2') or r['label'].startswith(('REAR_Water',) + self.skip_prefixes):
                continue        # water: handled by the water test; our own foliage never blocks a re-plan (POIs do)
            i0, j0 = to_ij(r['cx'] - r['ex'], r['cy'] - r['ey'])
            i1, j1 = to_ij(r['cx'] + r['ex'], r['cy'] + r['ey'])
            pt0 = (int(math.floor(i0)), int(math.floor(j0)))
            pt1 = (int(math.ceil(i1)), int(math.ceil(j1)))
            cv2.rectangle(foot, pt0, pt1, 1, -1)
            if any(k in r['label'] or k in r['class'] for k in ATTRACT_KEYS):
                cv2.rectangle(attract, pt0, pt1, 1, -1)
            if 'Fence' in r['class'] or 'Fence' in r['label']:
                cv2.rectangle(fence, pt0, pt1, 1, -1)
        self.foot_d = dist_to(foot.astype(bool))
        self.attract_d = dist_to(attract.astype(bool))
        self.fence_d = dist_to(fence.astype(bool))
        spots = np.zeros((NJ, NI), bool)
        for (sx, sy, rad) in MISSION_SPOTS:
            spots |= np.hypot(X - sx, Y - sy) < rad * 100.0
        self.spots = spots
        self.base = (X > BASE_BOX[0]) & (X < BASE_BOX[1]) & (Y > BASE_BOX[2]) & (Y < BASE_BOX[3])
        lanes = np.zeros((NJ, NI), bool)
        for (x0, x1, y0, y1) in LANE_BOXES:
            lanes |= (X > x0) & (X < x1) & (Y > y0) & (Y < y1)
        self.lanes = lanes
        # radixite crystals and metal resources: blight (dead ground, nothing grows) + harvester access.
        # Distance from the resource's FOOTPRINT edge (large crystals are ~8 m across), pivot as fallback.
        res = np.zeros((NJ, NI), np.uint8)
        for r in load_footprints():
            if r['label'].startswith('Resource_') or r['class'].startswith('Resource_'):
                i0, j0 = to_ij(r['cx'] - r['ex'], r['cy'] - r['ey'])
                i1, j1 = to_ij(r['cx'] + r['ex'], r['cy'] + r['ey'])
                cv2.rectangle(res, (int(i0), int(j0)), (int(math.ceil(i1)), int(math.ceil(j1))), 1, -1)
        for r in self.actors:
            if r['label'].startswith('Resource_') or r['class'].startswith('Resource_'):
                i, j = to_ij(r['x'], r['y'])
                if 0 <= int(i) < NI and 0 <= int(j) < NJ:
                    res[int(j), int(i)] = 1
        res = res.astype(bool)
        self.resource_d = dist_to(res)
        # local hollows (moisture collects): ground below its 12 m smoothed neighbourhood
        zb = cv2.GaussianBlur(self.z.astype(np.float32), (0, 0), 12)
        self.hollow = np.clip((zb - self.z) / 60.0, 0, 1)
        # foot of a slope: gentle cell within 10 m of a steep one
        self.steep_d = dist_to(self.slope > 24)
        self.weedy = self.w['Weedy']

    # -- placement field ------------------------------------------------------------------------------------------------
    def allowed(self, kind, cfg):
        """Boolean mask where `kind` ('detail' | 'patch') may go."""
        c = cfg[kind]
        ok = self.play & ~self.spots & ~self.base & ~self.wet
        ok &= self.road_edge > c['road']
        ok &= self.route_d > c['route']
        ok &= self.stripe_d > c['stripe']
        ok &= self.hard_d > c['hard']
        ok &= self.water_d > c['water']
        ok &= self.foot_d > c['foot']
        ok &= self.slope < c['slope']
        ok &= self.resource_d > c['resource']
        if c.get('lanes'):
            ok &= ~self.lanes
        for (x0, x1, y0, y1) in cfg.get('reserved', []):
            ok &= ~((self.X > x0) & (self.X < x1) & (self.Y > y0) & (self.Y < y1))
        return ok

    def attraction(self, cfg):
        a = 0.10 * np.ones((NJ, NI), np.float32)
        a += 0.9 * np.exp(-np.maximum(self.attract_d - 0.5, 0) / 3.5) * (self.attract_d > 0.6)   # hug rocks/trees
        a += 0.7 * self.hollow                                                                   # hollows, gullies
        a += 0.5 * np.exp(-np.abs(self.steep_d - 4.0) / 3.0) * (self.slope < 22)                 # slope feet
        a += 0.6 * np.exp(-np.abs(self.water_d - 7.0) / 4.0) * ~self.wet                        # former shorelines
        a += 0.6 * np.clip(self.weedy * 2.5, 0, 1) * (self.weedy < 0.7)                          # cracked-mud margins
        a += 0.35 * np.exp(-np.abs(self.road_edge - cfg['verge']) / 2.0)                         # road verges
        a += 0.25 * np.exp(-np.abs(self.stripe_d - cfg['verge']) / 2.0)                          # track verges
        a += 0.5 * np.exp(-np.maximum(self.fence_d - 0.4, 0) / 1.2) * (self.fence_d > 0.4)       # wind-blown at fences
        return a


def plan(round_no, cfg):
    f = Field()
    rng = np.random.default_rng(1000 + round_no)
    attract = f.attraction(cfg)
    clump = fractal_noise(11 + round_no, (40, 14, 5), (0.5, 0.35, 0.15))
    # ---- small detail: clumps of flat dry-grass / straw cards -----------------------------------------------------------
    # Sparse single cards are invisible from the RTS camera (about 24 px per metre); a clump of 5-16 overlapping cards
    # within 1-4 m reads as a patch of dry grass. Clump centres come from attraction x noise; every card is re-checked.
    ok_d = f.allowed('detail', cfg)
    cl = np.clip((clump - cfg['clump_cut']) / (1 - cfg['clump_cut']), 0, 1)
    prob = cfg['clump_density'] * (0.12 + attract) * (0.1 + cl)
    prob = np.where(ok_d, np.minimum(prob, 0.5), 0.0)
    hits = rng.random((NJ, NI)) < prob
    jj, ii = np.nonzero(hits)
    shares = np.array([v[0] for v in DETAIL_MESHES.values()])
    shares = shares / shares.sum()
    meshes = list(DETAIL_MESHES.keys())
    detail = {m: [] for m in meshes}
    gy, gx = np.gradient(f.z, 100.0)
    amax = max(float(attract.max()), 1.0)
    for j, i in zip(jj, ii):
        richness = attract[j, i] / amax
        n_cards = int(rng.integers(cfg['clump_cards'][0], cfg['clump_cards'][1] + 1) * (0.7 + 0.6 * richness))
        radius = rng.uniform(*cfg['clump_radius'])
        # one clump leans on one or two meshes, so clumps differ from each other
        main = rng.choice(len(meshes), p=shares)
        for _ in range(n_cards):
            r = radius * math.sqrt(rng.random()) ** 0.8
            t = rng.uniform(0, 2 * math.pi)
            fi, fj = i + 0.5 + r * math.cos(t), j + 0.5 + r * math.sin(t)
            ci, cj = int(fi), int(fj)
            if not (0 <= ci < NI and 0 <= cj < NJ) or not ok_d[cj, ci]:
                continue
            k = main if rng.random() < 0.6 else rng.choice(len(meshes), p=shares)
            m = meshes[k]
            _, s0, s1, align, sink = DETAIL_MESHES[m]
            pit_key = None
            if in_pit(f, ci, cj, *to_xy(fi, fj)):
                if 'DryStar' not in m or rng.random() < 0.5:
                    continue                    # review r3: no rust mats in the pit, half the tussocks
                pit_key = m + '#pit'
            # cards shrink towards the clump edge
            s = float(rng.uniform(s0, s1) * (1.05 - 0.35 * r / max(radius, 0.1)))
            x, y = to_xy(fi, fj)
            z = bilinear(f.z, fi, fj) - sink
            pitch = roll = 0.0
            if align:
                pitch = math.degrees(math.atan(gx[cj, ci]))
                roll = -math.degrees(math.atan(gy[cj, ci]))
            detail.setdefault(pit_key or m, []).append([round(x, 1), round(y, 1), round(z, 1),
                                                        round(float(rng.uniform(0, 360)), 1), round(s, 3),
                                                        round(pitch, 1), round(roll, 1)])
    # ---- crushable scrub patches -------------------------------------------------------------------------------------
    ok_p = f.allowed('patch', cfg)
    score = attract * (0.6 + 0.8 * clump)
    score = np.where(ok_p, score, 0).astype(np.float32)
    patches = []
    taken = np.zeros((NJ, NI), bool)
    cand = cv2.GaussianBlur(score, (0, 0), 3)
    forced = []
    for (fx, fy) in cfg.get('forced_sites', []):
        fi, fj = to_ij(fx, fy)
        forced.append(int(round(fj)) * NI + int(round(fi)))
    flat = forced + [int(k) for k in np.argsort(cand.ravel())[::-1]]
    for n_idx, idx in enumerate(flat):
        if len(patches) >= cfg['max_patches'] + len(forced):
            break
        j, i = divmod(int(idx), NI)
        is_forced = n_idx < len(forced)
        if not is_forced and cand[j, i] < cfg['patch_min_score']:
            break
        if is_forced and not ok_p[j, i]:
            print('forced site not allowed', to_xy(i, j))
            continue
        if taken[max(0, j - 2):j + 3, max(0, i - 2):i + 3].any():
            continue
        poly = patch_polygon(f, ok_p, cand, i, j, rng, cfg)
        if poly is None:
            continue
        pm = np.zeros((NJ, NI), np.uint8)
        cv2.fillPoly(pm, [np.round(np.array([to_ij(x, y) for x, y in poly])).astype(np.int32)], 1)
        pm = pm.astype(bool)
        if (pm & ~ok_p).sum() > 0.02 * pm.sum():
            continue
        area = int(pm.sum())
        cx, cy = to_xy(i, j)
        bp = cfg['patch_bp_pick'](rng, f, i, j)
        if area < cfg['brush_min_area']:
            continue                 # review r1: small brush patches came out as one lonely shrub
        if area < cfg['patch_min_area']:
            continue
        patches.append({'bp': bp, 'centre': [round(cx), round(cy)], 'area_m2': area,
                        'poly': [[round(x), round(y)] for x, y in poly], 'seed': int(rng.integers(1, 99999)),
                        'layout': patch_layout(bp, area, rng)})
        grow = cv2.dilate(pm.astype(np.uint8), np.ones((2 * cfg['patch_gap'] + 1,) * 2, np.uint8)).astype(bool)
        taken |= grow
    # ---- concept D: dead thistle accents (user pick 2026-11-20) -------------------------------------------------------
    # small crushable thistle patches where thistles grow in West Germany: just past the road verges, along fences, the
    # farm and in rocky / hollow ground; same keep-outs as the brush, never inside a brush patch's gap ring.
    tcfg = dict(cfg, patch_r=cfg['thistle_r'])
    t_score = (attract + 0.9 * np.exp(-np.abs(f.road_edge - (cfg['patch']['road'] + 2.0)) / 2.5)
               + 0.8 * np.exp(-np.maximum(f.fence_d - 1.5, 0) / 2.0)) * (0.5 + clump)
    t_score = np.where(ok_p & ~taken, t_score, 0).astype(np.float32)
    t_cand = cv2.GaussianBlur(t_score, (0, 0), 2)
    n_thistle = 0
    for idx in np.argsort(t_cand.ravel())[::-1]:
        if n_thistle >= cfg['max_thistle']:
            break
        j, i = divmod(int(idx), NI)
        if t_cand[j, i] < cfg['thistle_min_score']:
            break
        if taken[max(0, j - 2):j + 3, max(0, i - 2):i + 3].any():
            continue
        poly = patch_polygon(f, ok_p, t_cand, i, j, rng, tcfg)
        if poly is None:
            continue
        pm = np.zeros((NJ, NI), np.uint8)
        cv2.fillPoly(pm, [np.round(np.array([to_ij(x, y) for x, y in poly])).astype(np.int32)], 1)
        pm = pm.astype(bool)
        if (pm & ~ok_p).sum() > 0.02 * pm.sum() or (pm & taken).any():
            continue
        area = int(pm.sum())
        if area < cfg['thistle_min_area']:
            continue
        cx, cy = to_xy(i, j)
        patches.append({'bp': 'Thistle', 'centre': [round(cx), round(cy)], 'area_m2': area,
                        'poly': [[round(x), round(y)] for x, y in poly], 'seed': int(rng.integers(1, 99999)),
                        'layout': patch_layout('Thistle', area, rng)})
        taken |= cv2.dilate(pm.astype(np.uint8), np.ones((2 * cfg['thistle_gap'] + 1,) * 2, np.uint8)).astype(bool)
        n_thistle += 1
    seen = set()                  # review r1/r2: near-duplicate cards, also across meshes
    for m in detail:
        kept = []
        for it in detail[m]:
            key = (int(it[0] // 25), int(it[1] // 25))
            if key in seen:
                continue
            seen.add(key)
            kept.append(it)
        detail[m] = kept
    out = {'round': round_no, 'cfg': {k: v for k, v in cfg.items() if not callable(v)},
           'detail': detail, 'patches': patches, 'materials': DETAIL_MATERIALS,
           'counts': {m.split('/')[-1]: len(v) for m, v in detail.items()}}
    with open(ROOT + 'plan_r%d.json' % round_no, 'w') as h:
        json.dump(out, h)
    np.savez_compressed(ROOT + 'masks_r%d.npz' % round_no, road_edge=f.road_edge, route_d=f.route_d,
                        stripe_d=f.stripe_d, water_d=f.water_d, foot_d=f.foot_d, slope=f.slope, resource_d=f.resource_d, ok_detail=ok_d,
                        ok_patch=ok_p, attract=attract)
    picture(f, out, ok_d, ok_p, round_no)
    print('detail', out['counts'], 'total', sum(out['counts'].values()))
    print('patches', len(patches), 'area', sum(p['area_m2'] for p in patches))
    return out


def bilinear(a, fi, fj):
    i, j = int(fi), int(fj)
    i = min(max(i, 0), NI - 2)
    j = min(max(j, 0), NJ - 2)
    tx, ty = fi - i, fj - j
    return float(a[j, i] * (1 - tx) * (1 - ty) + a[j, i + 1] * tx * (1 - ty) + a[j + 1, i] * (1 - tx) * ty +
                 a[j + 1, i + 1] * tx * ty)


def patch_polygon(f, ok, cand, i, j, rng, cfg):
    """Organic outline around (i, j): radial noise blob clipped to the allowed mask, as world points."""
    r0 = rng.uniform(cfg['patch_r'][0], cfg['patch_r'][1])
    n = 14
    ph = rng.uniform(0, 2 * math.pi, 3)
    stretch = rng.uniform(1.0, 1.8)
    rot = rng.uniform(0, math.pi)
    pts = []
    for k in range(n):
        t = 2 * math.pi * k / n
        r = r0 * (1 + 0.22 * math.sin(2 * t + ph[0]) + 0.15 * math.sin(3 * t + ph[1]) + 0.1 * math.sin(5 * t + ph[2]))
        dx, dy = r * math.cos(t) * stretch, r * math.sin(t) / stretch
        ex = dx * math.cos(rot) - dy * math.sin(rot)
        ey = dx * math.sin(rot) + dy * math.cos(rot)
        # pull the point in until it is on allowed ground (patches never reach a keep-out)
        for shrink in (1.0, 0.8, 0.6, 0.45, 0.3):
            pi_, pj_ = i + ex * shrink, j + ey * shrink
            if 0 <= int(pi_) < NI and 0 <= int(pj_) < NJ and ok[int(pj_), int(pi_)]:
                break
        else:
            return None
        pts.append(to_xy(pi_, pj_))
    return pts


def picture(f, out, ok_d, ok_p, round_no):
    top = cv2.imread(SURVEY + 'top_full.png')
    W = top.shape[0]
    cx, cy, w = -5100, 300, 72000

    def px(x, y):
        return int(((y - cy) / w + 0.5) * W), int((0.5 - (x - cx) / w) * W)
    img = (top * 0.75).astype(np.uint8)
    # keep-outs tinted red (detail keep-out) where in play
    blocked = f.play & ~ok_d
    tint = np.zeros((NJ, NI, 3), np.uint8)
    tint[blocked] = (40, 40, 160)
    tint[f.play & ok_d & ~ok_p] = (120, 90, 30)
    big = cv2.resize(tint.transpose(1, 0, 2)[::-1], None, fx=1, fy=1)
    # map grid to image pixels
    x0, y0 = to_xy(0, 0)
    x1, y1 = to_xy(NI - 1, NJ - 1)
    p0 = px(x1, y0)
    p1 = px(x0, y1)
    big = cv2.resize(big, (p1[0] - p0[0], p1[1] - p0[1]), interpolation=cv2.INTER_NEAREST)
    sub = img[p0[1]:p1[1], p0[0]:p1[0]]
    mask = big.sum(-1) > 0
    sub[mask] = (sub[mask] * 0.45 + big[mask] * 0.55).astype(np.uint8)
    colours = [(80, 220, 255), (60, 180, 255), (120, 255, 160), (90, 255, 90), (200, 230, 255), (255, 200, 120)]
    for (m, items), col in zip(out['detail'].items(), colours):
        for it in items:
            cv2.circle(img, px(it[0], it[1]), 1, col, -1)
    for p in out['patches']:
        poly = np.array([px(x, y) for x, y in p['poly']], np.int32)
        cv2.polylines(img, [poly], True, (0, 255, 255) if 'Grass' in p['bp'] else (0, 140, 255), 3)
    for s in f.splines:
        cv2.polylines(img, [np.array([px(q[0], q[1]) for q in s['pts']], np.int32)], False, (255, 255, 0), 2)
    for agent, routes in f.nav.items():
        if agent != 'RecastNavMesh-HeavyTank':
            continue
        for r in routes.values():
            if len(r['pts']) > 1:
                cv2.polylines(img, [np.array([px(*q) for q in r['pts']], np.int32)], False, (255, 0, 255), 2)
    for gx in range(-40000, 31000, 5000):
        cv2.putText(img, 'x%d' % gx, px(gx, -25000), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 255, 255), 2)
        cv2.line(img, px(gx, -25200), px(gx, 25800), (200, 200, 200), 1)
    for gy in range(-25000, 26000, 5000):
        cv2.putText(img, 'y%d' % gy, px(-40500, gy), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 255, 255), 2)
        cv2.line(img, px(-41000, gy), px(30800, gy), (200, 200, 200), 1)
    crop = img[:, int(W * 0.12):int(W * 0.88)]
    cv2.imwrite(ROOT + 'plan_r%d_full.jpg' % round_no, crop, [cv2.IMWRITE_JPEG_QUALITY, 88])
    small = cv2.resize(crop, (crop.shape[1] // 2, crop.shape[0] // 2), interpolation=cv2.INTER_AREA)
    cv2.imwrite(ROOT + 'plan_r%d.jpg' % round_no, small, [cv2.IMWRITE_JPEG_QUALITY, 85])


def pick_patch_bp(rng, f, i, j):
    # round 3: the feral-wheat DryGrass read as golden pom-poms (reviews r1+r2, user: yellow does not fit); every
    # crushable patch is dead brush now
    return 'DeadBrush'


CFG = {
    'detail': {'road': 1.5, 'route': 2.5, 'stripe': 1.2, 'hard': 1.5, 'water': 2.5, 'foot': 0.6, 'slope': 28,
               'resource': 7.0, 'lanes': True},
    'patch': {'road': 6.0, 'route': 8.5, 'stripe': 4.5, 'hard': 4.0, 'water': 3.0, 'foot': 1.5, 'slope': 22,
              'resource': 9.0, 'lanes': True},
    'verge': 3.0,
    'clump_density': 0.07,
    'clump_cards': (5, 14),
    'clump_radius': (1.2, 3.8),
    'clump_cut': 0.35,
    'max_patches': 60,
    'patch_min_area': 45,
    'brush_min_area': 80,
    # review r1: the open -Y quadrant the T-26s cross had no scrub; sites validated by the reviewer (route/stripe/road)
    'forced_sites': [(-18000, -9500), (-16000, -12500), (-21000, -12500), (-24000, -14500), (-13000, -15500),
                     (-12000, -18500), (-25000, -20500), (-14000, -22500), (-24000, -23500)],
    'patch_min_score': 0.45,
    'patch_r': (4.5, 9.0),
    'patch_gap': 12,
    'patch_bp_pick': pick_patch_bp,
    'max_thistle': 40,
    'thistle_r': (2.6, 5.0),
    'thistle_min_area': 18,
    'thistle_min_score': 0.5,
    'thistle_gap': 9,
    'reserved': [],
}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--round', type=int, default=1)
    args = ap.parse_args()
    plan(args.round, CFG)
