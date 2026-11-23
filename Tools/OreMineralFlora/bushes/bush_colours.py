"""Samples the engine pictures of the bush test (system python, Pillow + numpy). Re-runnable.

python -B bush_colours.py   -> Saved/OreMountains/flora/bushes/colours.json + a printed table
Reads test/result.json (camera pose + world probe points per picture, projected here) and the PNGs. For each probe: mean sRGB of the 40% brightest pixels in a
small window (lit foliage, not the shadowed core) and the mean of the whole window, with r/g and b/g ratios, so the red
and yellow foliage can be compared with the ground layers measured on Ger_2ndOreMountains:
  IronOxide ~ (126, 77, 58) r/g 1.65 b/g 0.75 | Ochre ~ (125, 101, 67) r/g 1.24 b/g 0.66 | pale ~ (195, 172, 141) r/g 1.14 b/g 0.82
"""
import json
import os

import numpy as np
from PIL import Image

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/'
TARGETS = {'IronOxide(map)': (126, 77, 58), 'Ochre(map)': (125, 101, 67), 'pale(map)': (195, 172, 141)}


def ratios(c):
    return round(float(c[0]) / max(float(c[1]), 1.0), 2), round(float(c[2]) / max(float(c[1]), 1.0), 2)


def window(img, x, y, r):
    h, w = img.shape[:2]
    return img[max(0, y - r):min(h, y + r), max(0, x - r):min(w, x + r)].reshape(-1, 3)


def project(eye, look, fov, p, w=1600, h=900):
    """Pinhole projection matching an Unreal CameraActor aimed with find_look_at_rotation (no roll, horizontal FOV)."""
    e, t, q = np.array(eye, float), np.array(look, float), np.array(p, float)
    f = t - e
    f /= np.linalg.norm(f)
    right = np.cross([0, 0, 1.0], f)          # Unreal is left-handed: screen right = up x forward
    right /= np.linalg.norm(right)
    up = np.cross(f, right)
    d = q - e
    z = d @ f
    if z <= 1:
        return None
    foc = (w / 2) / np.tan(np.radians(fov) / 2)
    return int(round(w / 2 + foc * (d @ right) / z)), int(round(h / 2 - foc * (d @ up) / z))


def main():
    res = json.load(open(ROOT + 'test/result.json'))['result']
    out = {}
    for shot, pr in res.get('probes', {}).items():
        path = ROOT + shot + '.png'
        if not os.path.exists(path) or 'points' not in pr:
            continue
        img = np.asarray(Image.open(path).convert('RGB'), np.float32)
        r = 5 if 'game_view' in shot else 12
        rows = {}
        for key, p in pr['points'].items():
            xy = project(pr['eye'], pr['look'], pr['fov'], p)
            if xy is None:
                continue
            x, y = xy
            if not (r <= x < img.shape[1] - r and r <= y < img.shape[0] - r):
                continue
            px = window(img, x, y, r)
            lum = px.mean(1)
            top = px[lum >= np.percentile(lum, 60)]
            m_all, m_top = px.mean(0), top.mean(0)
            rows[key] = dict(xy=[x, y], lit=[int(v) for v in m_top.round()], lit_ratios=ratios(m_top),
                             mean=[int(v) for v in m_all.round()], mean_ratios=ratios(m_all))
        out[shot] = rows
        # marked copy so the sample spots can be checked by eye
        im = Image.open(path).convert('RGB')
        from PIL import ImageDraw
        d = ImageDraw.Draw(im)
        for key, v in rows.items():
            x, y = v['xy']
            d.rectangle([x - r, y - r, x + r, y + r], outline=(0, 255, 255))
            d.text((x + r + 2, y - r), key, fill=(0, 255, 255))
        os.makedirs(ROOT + 'probes', exist_ok=True)
        im.save(ROOT + 'probes/' + shot + '_probes.jpg', quality=85)
    json.dump(out, open(ROOT + 'colours.json', 'w'), indent=1)
    for t, c in TARGETS.items():
        print('%-28s %s r/g %.2f b/g %.2f' % (t, c, *ratios(c)))
    for shot, rows in out.items():
        print('==', shot)
        for k, v in rows.items():
            print('  %-24s lit %-16s r/g %.2f b/g %.2f | mean %-16s r/g %.2f b/g %.2f' % (
                k, v['lit'], *v['lit_ratios'], v['mean'], *v['mean_ratios']))


if __name__ == '__main__':
    main()
