"""Rev2 measure of the bush pictures, the reviewer's method (system python, Pillow + numpy). Re-runnable.

python -B bush_rev2_measure.py  -> Saved/OreMountains/flora/bushes/rev2/measure.txt / .json + masks/*.png
For rts34 / rts45 (each with a bg_* shot taken with the bushes hidden): a pixel counts as plant when it differs from bg
(max channel diff > 24); cast shadows (darker than bg and not more saturated) are dropped. Each pixel goes to the nearest
projected plant inside that plant's projected 1.4 m disc (plant positions logged by the test). "Lit" = mean of the brightest 30 % of
that piece's pixels with saturation > 0.3; "ground" = the bg pixels under the same mask; dE = CIE76.
Targets (review on Ger_2ndOreMountains): red lit ~ (160, 85, 56) H17 S0.65 V0.63; ochre lit ~ (185, 135, 72) H33 S0.61 V0.73.
"""
import colorsys
import json
import os

import numpy as np
from PIL import Image

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/rev2/'


def project(eye, look, fov, p, w=1600, h=900):
    e, t, q = np.array(eye, float), np.array(look, float), np.array(p, float)
    f = t - e
    f /= np.linalg.norm(f)
    up_ref = np.array([0, 0, 1.0]) if abs(f[2]) < 0.99 else np.array([1.0, 0, 0])
    right = np.cross(up_ref, f)
    right /= np.linalg.norm(right)
    up = np.cross(f, right)
    d = q - e
    z = d @ f
    if z <= 1:
        return None
    foc = (w / 2) / np.tan(np.radians(fov) / 2)
    return w / 2 + foc * (d @ right) / z, h / 2 - foc * (d @ up) / z


def lab(rgb):
    c = np.asarray(rgb, float) / 255.0
    c = np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)
    m = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = m @ c / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.array([116 * f[1] - 16, 500 * (f[0] - f[1]), 200 * (f[1] - f[2])])


def hsv(rgb):
    h, s, v = colorsys.rgb_to_hsv(*(np.asarray(rgb, float) / 255.0))
    return round(h * 360), round(s, 2), round(v, 2)


def measure(shot):
    pr = json.load(open(ROOT + 'test/result.json'))['result']['probes'][shot]
    img = np.asarray(Image.open(ROOT + shot + '.png').convert('RGB'), np.float32)
    bg = np.asarray(Image.open(ROOT + 'bg_' + shot + '.png').convert('RGB'), np.float32)
    diff = np.abs(img - bg).max(2) > 24
    v_i, v_b = img.max(2), bg.max(2)
    s_i = (img.max(2) - img.min(2)) / np.maximum(img.max(2), 1)
    s_b = (bg.max(2) - bg.min(2)) / np.maximum(bg.max(2), 1)
    shadow = (v_i < v_b - 4) & (s_i < s_b + 0.05)
    mask = diff & ~shadow
    h, w = mask.shape
    # only pixels inside each plant's own projected disc count (moving cloud shadows on the test map make the bg
    # difference noisy away from the plants); the disc radius is the plant's 1.2 m x scale spread projected at its height
    yy, xx = np.mgrid[0:h, 0:w]
    near_d = np.full((h, w), 1e9)
    own = np.full((h, w), -1)
    keys = list(pr['pieces'])
    for ki, key in enumerate(keys):
        d = pr['pieces'][key]
        pts = d['plants'] or [d['centre'] + [1.0]]
        for p in pts:
            sc = p[2] if len(p) > 3 else 1.0
            c = project(pr['eye'], pr['look'], pr['fov'], [p[0], p[1], d['centre'][2] + 60])
            e = project(pr['eye'], pr['look'], pr['fov'], [p[0] + 120 * 1.2, p[1], d['centre'][2] + 60])
            if c is None or e is None:
                continue
            rad = max(6.0, abs(e[0] - c[0]))
            dd = np.sqrt((xx - c[0]) ** 2 + (yy - c[1]) ** 2)
            hit = (dd < rad * 1.15) & (dd < near_d)
            near_d[hit] = dd[hit]
            own[hit] = ki
    mask = mask & (own >= 0)
    ys, xs = np.nonzero(mask)
    owner_px = own[ys, xs]
    rows = {}
    lab_img = None
    for key in pr['pieces']:
        sel = owner_px == keys.index(key)
        px = img[ys[sel], xs[sel]]
        gpx = bg[ys[sel], xs[sel]]
        if len(px) < 10:
            rows[key] = dict(px=int(len(px)))
            continue
        sat = (px.max(1) - px.min(1)) / np.maximum(px.max(1), 1)
        fol = px[sat > 0.3] if (sat > 0.3).sum() > 10 else px
        lum = fol.mean(1)
        lit = fol[lum >= np.percentile(lum, 70)].mean(0)
        ground = gpx.mean(0)
        bx = (int(xs[sel].min()), int(ys[sel].min()), int(xs[sel].max()), int(ys[sel].max()))
        rows[key] = dict(px=int(len(px)), bbox_wh=[bx[2] - bx[0], bx[3] - bx[1]], lit=[int(v) for v in lit.round()],
                         lit_hsv=hsv(lit), ground=[int(v) for v in ground.round()], ground_hsv=hsv(ground),
                         dE76=round(float(np.linalg.norm(lab(lit) - lab(ground))), 1))
    os.makedirs(ROOT + 'masks', exist_ok=True)
    vis = (img * 0.45).astype(np.uint8)
    vis[mask] = [0, 255, 255]
    Image.fromarray(vis).save(ROOT + 'masks/mask_' + shot + '.png')
    return rows


def main():
    out = {}
    lines = ['targets: red lit (160, 85, 56) H17 S0.65 V0.63 | ochre lit (185, 135, 72) H33 S0.61 V0.73',
             'rows: @0 map ground (orange test sand), @1 IronOxide-tinted swatch, @2 Ochre-tinted swatch']
    for shot in ('rts34', 'rts45'):
        if not os.path.exists(ROOT + 'bg_' + shot + '.png'):
            continue
        rows = measure(shot)
        out[shot] = rows
        lines.append('== ' + shot)
        for k in sorted(rows, key=lambda k: (k.split('@')[1], k)):
            v = rows[k]
            if 'lit' not in v:
                lines.append('  %-26s px=%5d  not visible' % (k, v['px']))
                continue
            lines.append('  %-26s px=%5d bbox %4dx%-4d lit %-15s H%3d S%.2f V%.2f | ground %-15s H%3d | dE %5.1f' % (
                k, v['px'], v['bbox_wh'][0], v['bbox_wh'][1], tuple(v['lit']), *v['lit_hsv'], tuple(v['ground']),
                v['ground_hsv'][0], v['dE76']))
    json.dump(out, open(ROOT + 'measure.json', 'w'), indent=1)
    open(ROOT + 'measure.txt', 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
