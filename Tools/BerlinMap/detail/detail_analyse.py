"""Offline analysis of the Ger_Berlin dump (detail_dump.py + detail_dump_land.py): where is the map empty, and how
blended is the landscape paint. Plain python (numpy, scipy, PIL). Writes Saved/Berlin/detail/analysis/:
  layers.png       dominant paint layer per metre (play area, north up, east right)
  density.png      props (red) and decals (blue) over the layers, empty ground (> EMPTY_M from anything) in yellow
  reichstag.png    the same at 4 px/m round the Reichstag
  empty.json       empty-ground clusters (centre, area) sorted by size
  stats.json       counts and layer purity numbers
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

DUMP = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/dump/"
OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/analysis/"
os.makedirs(OUT, exist_ok=True)
PLAY_X, PLAY_Y = (-26000, 26000), (-50000, 50000)
EMPTY_M = 12.0
LAYER_COL = {"BerlinRubble": (150, 120, 105), "BerlinAsh": (95, 95, 98), "BerlinCobble": (170, 170, 160),
             "BerlinGlass": (60, 90, 95), "BerlinScrub": (110, 120, 70), "BerlinMud": (85, 70, 55)}

g = open(DUMP + "land_grid.txt").read().splitlines()
x0, y0, x1, y1 = map(int, g[0].split())
lx, ly, lz = map(float, g[1].split())
sx, sy, sz = map(float, g[2].split())
names = g[3].split()
nx, ny = x1 - x0 + 1, y1 - y0 + 1
W = np.stack([np.fromfile(DUMP + "land_%s.r8" % n, np.uint8).reshape(ny, nx) for n in names]).astype(np.float32)
H = (np.fromfile(DUMP + "land_height.r16", np.uint16).reshape(ny, nx).astype(np.float32) - 32768) * sz / 128.0


def to_idx(x, y):
    """world cm -> landscape vertex (i along X, j along Y)."""
    return (x - lx) / sx, (y - ly) / sy


# play-area crop, image rows = north (high X) at the top, columns = Y west->east
i_lo, _ = to_idx(PLAY_X[0], 0)
i_hi, _ = to_idx(PLAY_X[1], 0)
_, j_lo = to_idx(0, PLAY_Y[0])
_, j_hi = to_idx(0, PLAY_Y[1])
i_lo, i_hi, j_lo, j_hi = int(i_lo), int(i_hi), int(j_lo), int(j_hi)
# arrays are [j (Y)][i (X)] in the file (row = Y); crop and transpose to [X][Y]
crop = W[:, j_lo:j_hi + 1, i_lo:i_hi + 1].transpose(0, 2, 1)[:, ::-1, :]     # rows north->south
Hc = H[j_lo:j_hi + 1, i_lo:i_hi + 1].T[::-1, :]
rows, cols = crop.shape[1], crop.shape[2]


def px(x, y):
    """world cm -> image (col, row) in the play crop (1 px = 1 m)."""
    i, j = to_idx(x, y)
    return j - j_lo, (i_hi - i)


dom = crop.argmax(0)
purity = crop.max(0) / 255.0
img = np.zeros((rows, cols, 3), np.uint8)
for k, n in enumerate(names):
    img[dom == k] = LAYER_COL[n]
shade = np.clip(1.0 + (Hc - ndimage.uniform_filter(Hc, 9)) / 120.0, 0.6, 1.4)[..., None]
img = np.clip(img * shade, 0, 255).astype(np.uint8)
Image.fromarray(img).save(OUT + "layers.png")

A = json.load(open(DUMP + "actors.json"))["actors"]
prop = np.zeros((rows, cols), np.float32)
dec = np.zeros((rows, cols), np.float32)
skip = ("Landscape", "RecastNavMesh", "WorldSettings", "PostProcess", "SkyLight", "DirectionalLight", "Boundary")
for a in A:
    if any(s in a["class"] for s in skip) or a["label"].startswith(("SKIRT", "DZ_", "RVT", "Boundary")):
        continue
    pts = [(a["x"], a["y"])] + [(r[1], r[2]) for r in a.get("instances", [])]
    target = dec if a["class"] == "DecalActor" else prop
    for (x, y) in pts:
        c, r = px(x, y)
        c, r = int(round(c)), int(round(r))
        if 0 <= c < cols and 0 <= r < rows:
            if a["class"] == "DecalActor":
                rad = max(1, int(max(a["size"][1], a["size"][2]) * a.get("scale", 1) / 100))
            else:
                bx = a.get("box")
                rad = max(1, int(max(bx[3] - bx[0], bx[4] - bx[1]) * 0.5 / 100)) if bx else 2
                rad = min(rad, 40)
            target[max(0, r - rad):r + rad + 1, max(0, c - rad):c + rad + 1] += 1

water = crop[names.index("BerlinMud")] > 250   # rough; the Spree trench is pure mud
anything = (prop > 0) | (dec > 0)
dist = ndimage.distance_transform_edt(~anything)
empty = (dist > EMPTY_M) & ~water
lab, n = ndimage.label(empty)
clusters = []
for k in range(1, n + 1):
    rr, cc = np.nonzero(lab == k)
    if len(rr) < 60:
        continue
    # back to world
    i = i_hi - rr.mean()
    j = cc.mean() + j_lo
    clusters.append({"x": round(lx + i * sx), "y": round(ly + j * sy), "area_m2": int(len(rr)),
                     "px": [int(cc.mean()), int(rr.mean())]})
clusters.sort(key=lambda c: -c["area_m2"])
json.dump(clusters, open(OUT + "empty.json", "w"), indent=1)

ov = img.astype(np.float32) * 0.75
ov[..., 0] += np.clip(prop * 60, 0, 120)
ov[..., 2] += np.clip(dec * 40, 0, 100)
ov[empty] = ov[empty] * 0.4 + np.array([255, 220, 0]) * 0.6
ov = Image.fromarray(np.clip(ov, 0, 255).astype(np.uint8))
dr = ImageDraw.Draw(ov)
for k, c in enumerate(clusters[:25]):
    dr.text((c["px"][0] - 4, c["px"][1] - 6), str(k + 1), fill=(0, 0, 0))
c, r = px(8100, -15500)
dr.rectangle([c - 70, r - 60, c + 70, r + 60], outline=(255, 0, 255))
ov.save(OUT + "density.png")
ov.crop((int(c - 160), int(r - 130), int(c + 160), int(r + 130))).resize((1280, 1040), Image.NEAREST).save(OUT + "reichstag.png")

blend = 1.0 - purity
stats = {"actors": len(A), "decals": int(sum(1 for a in A if a["class"] == "DecalActor")),
         "instances": int(sum(len(a.get("instances", [])) for a in A)),
         "layer_share": {n: round(float((dom == k).mean()), 3) for k, n in enumerate(names)},
         "pure_over_0.95": round(float((purity > 0.95).mean()), 3),
         "mean_blend": round(float(blend.mean()), 3),
         "empty_share": round(float(empty.mean()), 3), "empty_clusters": len(clusters)}
json.dump(stats, open(OUT + "stats.json", "w"), indent=1)
print(json.dumps(stats, indent=1))
print("top empty clusters:", clusters[:12])
