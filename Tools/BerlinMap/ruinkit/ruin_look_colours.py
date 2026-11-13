"""Mean colours of the look test's top-down picture (ruin_look_test.py) round each heap, skirt and ground patch.
usage: python ruin_look_colours.py <Saved/Berlin/ruinkit_look/<outname>>      -> <folder>/colours.json + a table
The top camera looks straight down from spots.json's top_height with yaw 90: image right = world -X ... the mapping
is checked with the mud patch (darkest ground) when run. Values are display sRGB 0..1; sat = (max - min) / max.
"""
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def main(folder):
    folder = Path(folder)
    spots = json.loads((folder / 'spots.json').read_text())
    img = np.asarray(Image.open(folder / 'top.png').convert('RGB')).astype(np.float32) / 255
    h, w = img.shape[:2]
    half_w = spots['top_height'] * math.tan(math.radians(spots['fov'] / 2))     # world cm at the picture's half width
    scale = (w / 2) / half_w                                                     # px per cm
    cx, cy = spots['centre']

    def mean(x, y, radius, sx, sy):
        px = w / 2 + sx * (x - cx) * scale
        py = h / 2 + sy * (y - cy) * scale
        r = max(2, int(radius * scale))
        x0, x1 = int(px - r), int(px + r)
        y0, y1 = int(py - r), int(py + r)
        if x0 < 0 or y0 < 0 or x1 > w or y1 > h:
            return None
        return img[y0:y1, x0:x1].reshape(-1, 3).mean(0)

    # Find the axis signs: the mud patch must be the darkest of the three ground references.
    best = None
    for sx in (1, -1):
        for sy in (1, -1):
            m = mean(*spots['spots']['ground_mud'], sx, sy)
            a = mean(*spots['spots']['ground_ash'], sx, sy)
            if m is None or a is None:
                continue
            score = float(a.mean() - m.mean())
            if best is None or score > best[0]:
                best = (score, sx, sy)
    _, sx, sy = best
    out = {}
    print('%-28s %-22s %5s %5s' % ('spot', 'sRGB', 'lum', 'sat'))
    for name, (x, y, radius) in sorted(spots['spots'].items()):
        c = mean(x, y, radius, sx, sy)
        if c is None:
            continue
        lum = float(c @ np.array([0.3, 0.59, 0.11]))
        sat = float((c.max() - c.min()) / max(c.max(), 1e-6))
        out[name] = {'rgb': [round(float(v), 3) for v in c], 'lum': round(lum, 3), 'sat': round(sat, 3)}
        print('%-28s %-22s %5.3f %5.3f' % (name, out[name]['rgb'], lum, sat))
    (folder / 'colours.json').write_text(json.dumps({'axis_signs': [sx, sy], 'spots': out}, indent=1))


if __name__ == '__main__':
    main(sys.argv[1])
