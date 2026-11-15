"""Rebuilds the user's own dark painted patches that the detail pass damaged (offline part).

Source of truth: Saved/IntroDetail/top_basecolor.png, the base-colour capture taken before any edit
(centre -25250,-1250, 33000 uu wide, 2048x1344). Dark pixels inside each zone become scan-line strokes of the layer
the user had there; everything else inside the zone goes back to Sand. Writes Saved/IntroDetail/repair.json, applied
in the editor with intro_detail_apply.repair().
"""
import json
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "Saved", "IntroDetail"))
img = np.asarray(Image.open(os.path.join(OUT, "top_basecolor.png")).convert("RGB")).astype(float).sum(2)
PX = 33000.0 / img.shape[1]
WX0, WY0 = -25250 - 16500, -1250 - img.shape[0] / 2 * PX
MED = np.median(img[img > 20])
scene = json.load(open(os.path.join(OUT, "scene.json")))
ROAD = np.array(scene["splines"]["BP_RoadSpline_AirportStopWay1"], dtype=float)[:, :2]

# zone box (x0, x1, y0, y1), the layer the user painted there, darkness threshold
ZONES = [dict(name="hut", box=(-24300, -19300, 4450, 7000), layer="ScorchConcrete", thr=0.74),
         dict(name="west_lake", box=(-29600, -26800, -4700, -1900), layer="ScorchSandyStone", thr=0.70),
         dict(name="small", box=(-28000, -26700, -1400, -150), layer="ScorchSandyStone", thr=0.70)]


def lum(x, y):
    c, r = int((x - WX0) / PX), int((y - WY0) / PX)
    return img[r, c] if 0 <= r < img.shape[0] and 0 <= c < img.shape[1] else MED


strokes = []
report = {}
for z in ZONES:
    x0, x1, y0, y1 = z["box"]
    xs, ys = np.arange(x0, x1 + 1, 100), np.arange(y0, y1 + 1, 100)
    dark = np.zeros((len(ys), len(xs)), bool)
    for r, y in enumerate(ys):
        for c, x in enumerate(xs):
            v = lum(x, y)
            near_road = np.hypot(ROAD[:, 0] - x, ROAD[:, 1] - y).min() < 620
            dark[r, c] = (3 < v < MED * z["thr"]) and not near_road
    # knock out single cells, keep blobs
    f = dark.astype(float)
    acc = sum(np.roll(np.roll(f, dr, 0), dc, 1) for dr in (-1, 0, 1) for dc in (-1, 0, 1))
    dark = acc >= 5
    report[z["name"]] = int(dark.sum())
    zone_strokes = []
    # 1) the whole zone back to sand (only the hut zone was wiped/over-painted wholesale; the others get sand only
    #    over the first-pass stone dabs, which the applier does from plan["paint_v1"])
    if z["name"] == "hut":
        for r, y in enumerate(ys):
            zone_strokes.append(dict(pts=[[int(x0), int(y)], [int(x1), int(y)]], w=50, f=110, layer="Sand"))
    # 2) the dark shape
    for r, y in enumerate(ys):
        c = 0
        while c < len(xs):
            if dark[r, c]:
                c1 = c
                while c1 + 1 < len(xs) and dark[r, c1 + 1]:
                    c1 += 1
                zone_strokes.append(dict(pts=[[int(xs[c]) - 50, int(y)], [int(xs[c1]) + 50, int(y)]], w=50, f=170,
                                         layer=z["layer"]))
                c = c1 + 1
            else:
                c += 1
    strokes.append(dict(name=z["name"], box=z["box"], strokes=zone_strokes))
json.dump(dict(zones=strokes), open(os.path.join(OUT, "repair.json"), "w"))
print(report, [len(s["strokes"]) for s in strokes])
