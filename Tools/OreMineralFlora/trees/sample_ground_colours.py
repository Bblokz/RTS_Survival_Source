"""Samples the IronOxide (red) and Ochre (yellow) landscape layers from saved game-camera views (system python, numpy + Pillow).

python -B sample_ground_colours.py [extra.png x0 y0 x1 y1 label ...]
Prints sRGB mean / median and hue-saturation-value per patch. Patches were picked by eye on the min3 critic views.
Writes Saved/OreMountains/flora/trees/ground_colours.json.
"""
import colorsys
import json
import os
import sys

import numpy as np
from PIL import Image

MIN3 = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/critic/min3/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/ground_colours.json'
# file, box (x0, y0, x1, y1), label
PATCHES = [
    ('53_d_iron0.png', (170, 400, 250, 520), 'iron_core_a'),
    ('53_d_iron0.png', (380, 340, 480, 420), 'iron_core_b'),
    ('53_d_iron0.png', (40, 470, 130, 560), 'ochre_beside_iron'),
    ('53_d_iron0.png', (900, 450, 1100, 600), 'pale_ground'),
    ('55_e_ochre0.png', (650, 250, 850, 400), 'ochre_core_a'),
    ('55_e_ochre0.png', (900, 450, 1150, 600), 'ochre_core_b'),
    ('55_e_ochre0.png', (180, 120, 330, 220), 'ochre_red_streak'),
    ('55_e_ochre0.png', (320, 540, 370, 600), 'existing_rust_spruce_fan'),
    ('55_e_ochre0.png', (530, 590, 590, 645), 'existing_rust_spruce_fan_b'),
]


def stats(path, box):
    im = np.asarray(Image.open(path).convert('RGB'), np.float32)
    x0, y0, x1, y1 = box
    px = im[y0:y1, x0:x1].reshape(-1, 3)
    mean = px.mean(0)
    med = np.median(px, 0)
    # saturation-weighted: the most coloured third, which is what the eye picks up as "the layer colour"
    hsv = np.array([colorsys.rgb_to_hsv(*(p / 255.0)) for p in px[:: max(1, len(px) // 4000)]])
    sat_thr = np.quantile(hsv[:, 1], 0.66)
    vivid = px[:: max(1, len(px) // 4000)][hsv[:, 1] >= sat_thr].mean(0)
    h, s, v = (float(c) for c in colorsys.rgb_to_hsv(*(mean / 255.0)))
    return dict(mean=[int(round(float(c))) for c in mean], median=[int(round(float(c))) for c in med], vivid_third=[int(round(float(c))) for c in vivid],
                hue_deg=round(h * 360, 1), sat=round(s, 3), val=round(v, 3), r_over_g=round(float(mean[0] / max(1, mean[1])), 2))


def main():
    out = {}
    patches = list(PATCHES)
    a = sys.argv[1:]
    while len(a) >= 6:
        patches.append((a[0], tuple(int(v) for v in a[1:5]), a[5]))
        a = a[6:]
    for f, box, label in patches:
        path = f if os.path.isabs(f) else MIN3 + f
        out[label] = dict(file=path, box=box, **stats(path, box))
        print(label, json.dumps(out[label]))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as fh:
        json.dump(out, fh, indent=1)


if __name__ == '__main__':
    main()
