"""Ger_IntroMission: blend more ground layers into the flat sand around the Soviet ridge base (2026-11-23).

User: "the sand area around the intro base looks too flat; use more blending of different landscape materials around it."

Offline planner (numpy, no Unreal). Input: the region export the live editor wrote with
URTSShortsLandscapeLibrary.ExportLandscapeRegion (read only):
    Saved/IntroRear/terrain/basepaint_now_<layer>.r8 + basepaint_now_height.r16, vertices (436,227)-(616,437).
Output: Saved/IntroPaint/base/strokes.json (one paint stroke per 1 m scan-line run) + preview_before/after.png.

Paint goes in through the editor's own stroke path (ALandscapeProxy::EditorApplySpline, see basepaint_apply.py):
raw weightmap imports broke this landscape on 2026-11-19 (memory intro-landscape-paint-import). A stroke paints its
layer at full weight with a soft falloff at its sides, so the masks are binary organic noise shapes:
    DesertGravel    subtle red gravel   broad patches over the open sand
    DesertRockDark  pale pebbles        slopes, feet of the cliffs, a few small scattered patches
    Weedy           dark cracked mud    hollows
Only cells that are pure Sand today are painted (the user's own patches and roads stay), never inside the base walls,
and only within a ring around the base whose outer edge is noise-shaped so no rectangle shows.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paint_gen import ALBEDO, box_mean, dilate, fbm, smooth  # noqa: E402

T = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/IntroPaint/base/'
LAYERS = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']
RECT = (436, 227, 616, 437)                  # vertex indices of the export
ORIGIN = (-51600.0, -32701.0)
QUAD = 100.0
WALLS = (-2610.0, 4152.0, -4256.0, 5265.0)   # base wall centres x0, x1, y0, y1 (cm)
WALL_MARGIN = 450.0                          # keep paint this far outside the wall line (slab edge, sandbags)
RING = 4200.0                                # mean reach of the new paint beyond the walls (cm)
RING_WOBBLE = 1200.0                         # noise on that reach, so the outer edge is organic
PAINT_ORDER = ['DesertGravel', 'DesertRockDark', 'Weedy']   # later strokes win where masks overlap


def load():
    w, h = RECT[2] - RECT[0] + 1, RECT[3] - RECT[1] + 1
    weights = {n: np.fromfile(T + 'basepaint_now_%s.r8' % n, np.uint8).reshape(h, w) for n in LAYERS}
    z = (np.fromfile(T + 'basepaint_now_height.r16', '<u2').reshape(h, w).astype(float) - 32768.0) * 100.0 / 128.0
    return weights, z


def world_grids(shape):
    xs = ORIGIN[0] + (RECT[0] + np.arange(shape[1])) * QUAD
    ys = ORIGIN[1] + (RECT[1] + np.arange(shape[0])) * QUAD
    return np.meshgrid(xs, ys)


def distance_outside_walls(wx, wy):
    dx = np.maximum(np.maximum(WALLS[0] - wx, wx - WALLS[1]), 0.0)
    dy = np.maximum(np.maximum(WALLS[2] - wy, wy - WALLS[3]), 0.0)
    inside = (wx > WALLS[0] - WALL_MARGIN) & (wx < WALLS[1] + WALL_MARGIN) & \
             (wy > WALLS[2] - WALL_MARGIN) & (wy < WALLS[3] + WALL_MARGIN)
    return np.hypot(dx, dy), inside


def opening(mask, radius):
    eroded = ~dilate(~mask, radius)
    return dilate(eroded, radius) & mask


def masks(weights, z):
    shape = z.shape
    pure = weights['Sand'] >= 230
    for n in LAYERS[1:]:
        pure &= weights[n] <= 25
    wx, wy = world_grids(shape)
    dist, inside = distance_outside_walls(wx, wy)
    reach = RING + (fbm(shape, [40, 18, 8], 71, warp=10) - 0.5) * 2.0 * RING_WOBBLE
    area = pure & ~inside & (dist < reach)

    gy, gx = np.gradient(z, QUAD)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    hollow = box_mean(z, 12) - z                        # cm below the 25 m surroundings

    n_gravel = fbm(shape, [34, 16, 7], 111, warp=14)
    n_gravel2 = fbm(shape, [12, 5, 2], 112)
    n_pebble = fbm(shape, [20, 9, 4], 121, warp=6)
    n_mud = fbm(shape, [26, 11, 5], 131, warp=8)

    # Coverage aim: about 40 % of the ring painted, so the sand still reads as the ground and the patches as variety.
    gravel = (n_gravel > 0.66) & (n_gravel2 > 0.32)
    steep = slope > 38.0
    cliff_foot = dilate(steep, 3) & ~steep
    near_cliff = dilate(steep, 8)
    pebble = ((slope > 18.0) & (n_pebble > 0.58)) | (cliff_foot & (n_pebble > 0.50)) | (n_pebble > 0.94)
    # Mud only in real hollows away from the cliff feet (there it drew dark bands along every face).
    mud = (((hollow > 22.0) & (n_mud > 0.52)) | ((n_mud > 0.95) & (slope < 8.0))) & ~near_cliff
    # Opening drops shapes thinner than about 4 m: thin runs along slopes read as stripes or cracks.
    out = {'DesertGravel': opening(gravel & area, 1), 'DesertRockDark': opening(pebble & area, 2),
           'Weedy': opening(mud & area, 2)}
    return out, area, pure


def runs(mask):
    """(row, col0, col1) for every horizontal run of True cells."""
    out = []
    for r in range(mask.shape[0]):
        row = mask[r].astype(np.int8)
        d = np.diff(np.concatenate([[0], row, [0]]))
        for c0, c1 in zip(np.where(d == 1)[0], np.where(d == -1)[0] - 1):
            out.append((r, int(c0), int(c1)))
    return out


def strokes_for(mask_by_layer):
    strokes = []
    for name in PAINT_ORDER:
        for r, c0, c1 in runs(mask_by_layer[name]):
            y = ORIGIN[1] + (RECT[1] + r) * QUAD
            x0 = ORIGIN[0] + (RECT[0] + c0) * QUAD - 40.0
            x1 = ORIGIN[0] + (RECT[0] + c1) * QUAD + 40.0
            strokes.append([name, round(y, 1), round(x0, 1), round(x1, 1)])
    return strokes


def simulate(weights, mask_by_layer):
    after = {n: weights[n].astype(float).copy() for n in LAYERS}
    for name in PAINT_ORDER:
        m = mask_by_layer[name]
        for n in LAYERS:
            after[n][m] = 255.0 if n == name else 0.0
    return after


def preview(weights, path):
    from PIL import Image
    shape = weights['Sand'].shape
    img = np.zeros(shape + (3,))
    for n in LAYERS:
        img += weights[n][..., None].astype(float) / 255.0 * np.array(ALBEDO[n])[None, None, :]
    Image.fromarray(np.clip(img[::-1], 0, 255).astype(np.uint8)).resize((shape[1] * 4, shape[0] * 4),
                                                                         Image.NEAREST).save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    weights, z = load()
    mask_by_layer, area, pure = masks(weights, z)
    strokes = strokes_for(mask_by_layer)
    json.dump({'rect': RECT, 'strokes': strokes}, open(OUT + 'strokes.json', 'w'))
    preview(weights, OUT + 'preview_before.png')
    preview(simulate(weights, mask_by_layer), OUT + 'preview_after.png')
    painted = np.zeros(area.shape, bool)
    for m in mask_by_layer.values():
        painted |= m
    print('cells %d, pure sand %.0f%%, paint area %.0f%%, painted %.0f%% of the area' % (
        area.size, 100 * pure.mean(), 100 * area.mean(), 100 * painted.sum() / max(1, area.sum())))
    for name in PAINT_ORDER:
        print('  %-15s %5d cells, %4d strokes' % (name, mask_by_layer[name].sum(),
                                                   sum(1 for s in strokes if s[0] == name)))


if __name__ == '__main__':
    main()
