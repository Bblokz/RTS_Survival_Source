"""Measures the rendered foliage colour of the mineral-flora trees in the engine pictures (system python, numpy + Pillow).

python -B measure_colours.py [picture folder]
For every close_<piece>.png with a matching close_<piece>_bg.png (same camera, the piece hidden), the piece's pixels are
the ones whose chromaticity differs from the background (a tree shadow only darkens the ground, it keeps its
chromaticity). Those pixels are split into foliage (saturation >= 0.35) and wood. Without a _bg picture, the centre
40 % of the frame is used with a saturation mask (rough: the orange test ground is saturated too).
Writes engine_colours.json next to the pictures; compares with the min3 ground samples (ground_colours.json).
"""
import colorsys
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

DIR = sys.argv[1] if len(sys.argv) > 1 else 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/'
GROUND = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/ground_colours.json'


def hsv(rgb):
    h, s, v = colorsys.rgb_to_hsv(*(np.asarray(rgb, float) / 255.0))
    return round(h * 360, 1), round(s, 3), round(v, 3)


def summary(px):
    if len(px) == 0:
        return None
    m = px.mean(0)
    s = (px.max(1) - px.min(1)) / np.maximum(px.max(1), 1)
    viv = px[s >= np.quantile(s, 0.66)].mean(0)
    return dict(n=int(len(px)), mean=[int(round(c)) for c in m], vivid_third=[int(round(c)) for c in viv], mean_hsv=hsv(m), vivid_hsv=hsv(viv))


def measure(path):
    im = np.asarray(Image.open(path).convert('RGB'), np.float32)
    bgp = path[:-4] + '_bg.png'
    H, W = im.shape[:2]
    if os.path.exists(bgp):
        bg = np.asarray(Image.open(bgp).convert('RGB'), np.float32)
        cn = im / np.maximum(im.sum(2, keepdims=True), 1)
        cb = bg / np.maximum(bg.sum(2, keepdims=True), 1)
        dchroma = np.abs(cn - cb).sum(2)
        dval = np.abs(im - bg).sum(2)
        # a tree's shadow only darkens the ground: drop pixels that are a darker copy of the background
        shadow = im.sum(2) < 0.8 * bg.sum(2)
        mask = (dchroma > 0.045) & (dval > 30) & ~shadow
        mode = 'diff'
    else:
        mask = np.zeros((H, W), bool)
        mask[int(H * 0.3):int(H * 0.7), int(W * 0.3):int(W * 0.7)] = True
        mode = 'centre'
    px = im[mask]
    s = (px.max(1) - px.min(1)) / np.maximum(px.max(1), 1)
    fol = px[s >= 0.35]
    wood = px[s < 0.35]
    v = px.max(1)
    lit = px[v >= np.quantile(v, 0.7)] if len(px) else px     # the sunlit top 30 % by value: what the eye reads as the colour
    return dict(mode=mode, piece_pixels=int(mask.sum()), foliage=summary(fol), wood=summary(wood), lit=summary(lit),
                foliage_share=round(float(len(fol)) / max(1, len(px)), 3))


def main():
    out = {}
    for p in sorted(glob.glob(os.path.join(DIR, 'close_*.png'))):
        if p.endswith('_bg.png'):
            continue
        k = os.path.basename(p)[6:-4]
        out[k] = measure(p)
        f = out[k]['foliage']
        L = out[k]['lit']
        print(k, out[k]['mode'], 'all', f and f['mean'], f and f['mean_hsv'], '| lit', L and L['mean'], L and L['mean_hsv'])
    if os.path.exists(GROUND):
        g = json.load(open(GROUND))
        out['_ground_min3'] = {k: dict(mean=v['mean'], hue=v['hue_deg'], sat=v['sat'], val=v['val']) for k, v in g.items()}
    with open(os.path.join(DIR, 'engine_colours.json'), 'w') as fh:
        json.dump(out, fh, indent=1)


if __name__ == '__main__':
    main()
