"""Ger_Berlin landscape blend pass, offline (numpy). Reads the live export (Saved/Berlin/detail/dump/land_*) and writes
Saved/Berlin/detail/paint/new_<layer>.r8 (+ previews), imported by detail_apply_paint (ImportLandscapeRegion,
heights untouched). Costs nothing at runtime: same material, same layers, only the weights change.

What it does (every change fades in with noise, nothing is a hard edge):
- big flat rubble fields get macro blotches of ash (soot) and mud in the hollows, and old cobble shows through
  on the squares (Koenigsplatz, Reichstag forecourt, Alexanderplatz) and along the streets;
- a ragged soot halo of ash round every ruin footprint and smoulder site;
- carriageway edges fray into rubble grit instead of ending at a line;
- the Tiergarten scrub gets mud and ash patches (shell holes, burnt ground).
Kept as they are: the outer 30 m rim (horizon skirt), the river trench, glass (blast crater, radixite pans), the rail
yard track bed, landmark pads and every vertex where one layer is a deliberate special (glass > 25 %).

    python detail_paint.py [strength]
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/"
DUMP = ROOT + "detail/dump/"
OUT = ROOT + "detail/paint/"
os.makedirs(OUT, exist_ok=True)
STRENGTH = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
CORNER, Q, N = -59850.0, 100.0, 1198
rng = np.random.default_rng(1126)

names = open(DUMP + "land_grid.txt").read().splitlines()[3].split()
W0 = {n: np.fromfile(DUMP + "land_%s.r8" % n, np.uint8).reshape(N, N).astype(np.float32) for n in names}
H = (np.fromfile(DUMP + "land_height.r16", np.uint16).reshape(N, N).astype(np.float32) - 32768) * 100 / 128.0
M = np.load(ROOT + "terrain/Ger_Berlin_masks.npz")
ACT = json.load(open(DUMP + "actors.json"))["actors"]
jj, ii = np.mgrid[0:N, 0:N]
X, Y = CORNER + ii * Q, CORNER + jj * Q


def noise(scale_m, octaves=4, seed=0):
    """fractal value noise in [0, 1] at a base feature size of scale_m metres."""
    r = np.random.default_rng(seed)
    acc = np.zeros((N, N), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        s = max(2, int(scale_m / (2 ** o)))
        g = r.random((N // s + 3, N // s + 3)).astype(np.float32)
        up = ndimage.zoom(g, s, order=3)[:N, :N]
        acc += amp * up
        tot += amp
        amp *= 0.5
    acc /= tot
    return (acc - acc.min()) / (acc.max() - acc.min() + 1e-6)


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


# ----- what must not change
rim = (X < -26000 + 3000) | (X > 26000 - 3000) | (Y < -50000 + 3000) | (Y > 50000 - 3000)
keep = (rim | ndimage.binary_dilation(M["river"], iterations=1) | ndimage.binary_dilation(M["bridge"] | M["rail"], iterations=4) | M["landmark_pad"] |
        (W0["BerlinGlass"] > 200) | M["sinkhole"])
keep_soft = ndimage.gaussian_filter(keep.astype(np.float32), 3.0)
free = np.clip(1.0 - keep_soft * 1.6, 0, 1) * (~keep)          # 0 where kept, fades in over ~5 m

# ----- footprints: ruins and smoulder sites from the live dump (actor positions, radius from the mesh box)
ruin = np.zeros((N, N), bool)
smoulder = np.zeros((N, N), bool)
for a in ACT:
    lab = a["label"]
    if lab.startswith(("RUIN_", "FOOT_", "POI_Reich")):
        b = a.get("box")
        r = 600 if not b else min(2500, max(b[3] - b[0], b[4] - b[1]) * 0.5 * a.get("scale", 1.0))
        i, j, rr = int((a["x"] - CORNER) / Q), int((a["y"] - CORNER) / Q), int(r * 0.8 / Q) + 1
        j0, i0 = max(j - rr, 0), max(i - rr, 0)
        sub = ruin[j0:j + rr + 1, i0:i + rr + 1]
        gy, gx = np.ogrid[j0 - j:j0 - j + sub.shape[0], i0 - i:i0 - i + sub.shape[1]]
        sub |= (gx * gx + gy * gy) <= rr * rr
    if lab.startswith("ATM_Smoulder") or lab.startswith("ATM_Fire"):
        i, j = int((a["x"] - CORNER) / Q), int((a["y"] - CORNER) / Q)
        if 6 <= i < N - 6 and 6 <= j < N - 6:
            gy, gx = np.ogrid[-6:7, -6:7]
            smoulder[j - 6:j + 7, i - 6:i + 7] |= (gx * gx + gy * gy) <= 25
ruin_d = ndimage.distance_transform_edt(~ruin)
smoul_d = ndimage.distance_transform_edt(~smoulder)

n_macro = noise(60, 4, 1)
n_mid = noise(18, 3, 2)
n_fine = noise(6, 2, 3)
hollow = smooth((ndimage.uniform_filter(H, 25) - H - 4.0) / 18.0)       # 0..1 where the ground dips > 4 cm

W = {n: W0[n].copy() for n in names}


def push(layer, amount):
    """move `amount` (0..1 per vertex) of every other layer's weight into `layer`."""
    a = np.clip(amount * STRENGTH, 0, 0.95) * free
    tot_other = sum(W[n] for n in names if n != layer)
    for n in names:
        if n == layer:
            continue
        take = W[n] * a
        W[n] -= take
        W[layer] += take


rubble_pure = smooth((W0["BerlinRubble"] - 150) / 80.0)
street = M["carriageway"] | M["street"]
n_patch = noise(30, 3, 4)                                   # 20-40 m patches (critic round 1: blend was invisible)
# squares where old paving shows: Reichstag forecourt / Koenigsplatz, Pariser Platz east of the Gate,
# Alexanderplatz, Potsdamer Platz; ragged clear-edged patches inside each
sq = np.minimum.reduce([np.hypot(X - 2200, Y + 15500) - 3600, np.hypot(X - 300, Y + 5600) - 2200,
                        np.hypot(X - 7000, Y - 28000) - 2800, np.hypot(X + 14000, Y + 2000) - 2400])
square_cob = smooth(1.0 - (sq + (n_mid - 0.5) * 1800.0) / 900.0)
bank = (ndimage.distance_transform_edt(~M["river"]) > 1.5) & (ndimage.distance_transform_edt(~M["river"]) < 8.0)

# 1 macro soot blotches (two scales) and hollow mud on the big rubble fields
push("BerlinAsh", rubble_pure * smooth((n_macro - 0.52) / 0.2) * (0.45 + 0.3 * n_fine))
push("BerlinAsh", rubble_pure * smooth((n_patch - 0.6) / 0.12) * (0.4 + 0.25 * n_fine))
push("BerlinMud", rubble_pure * hollow * smooth((n_mid - 0.35) / 0.25) * 0.45)
# 2 soot halo round ruins: eroded by noise and feathered (round 2: hard grey rings read like selection decals),
#   plus rubble bleeding outward so the pad edge dissolves
reach = ruin_d - 1.0 + (n_fine - 0.5) * 6.0 + (n_mid - 0.5) * 5.0
halo = smooth(1.0 - reach / (7.0 + 5.0 * n_mid)) * (ruin_d > 0.3)
push("BerlinAsh", halo * smooth((n_mid - 0.3) / 0.35) * smooth((n_fine - 0.25) / 0.5) * 0.32)
push("BerlinRubble", smooth(1.0 - reach / 5.0) * smooth((n_fine - 0.45) / 0.3) * 0.35)
push("BerlinAsh", smooth(1.0 - smoul_d / (6.0 + 6.0 * n_mid)) * 0.5)
# 3 old cobble through the rubble on squares (clear edges) and by the streets
near_street = smooth(1.0 - ndimage.distance_transform_edt(~street) / 5.0) * (~street)
push("BerlinCobble", near_street * smooth((n_mid - 0.45) / 0.2) * 0.5)
push("BerlinCobble", square_cob * smooth((n_fine * 0.5 + n_mid * 0.5 - 0.42) / 0.06) * 0.8)
# 4 frayed carriageway edges: grit (rubble) creeping onto the cobbles
edge_in = street & (ndimage.distance_transform_edt(street) < 3.5)
push("BerlinRubble", edge_in * smooth((n_fine - 0.5) / 0.3) * 0.35)
# 5 Tiergarten: mud in the hollows, burnt (ash) patches, scrub kept patchy
scrub = smooth((ndimage.gaussian_filter(W0["BerlinScrub"], 6.0) - 90) / 110.0)   # soft edge (round 2: hard line)
push("BerlinMud", scrub * smooth((n_patch - 0.5) / 0.15) * 0.55 + scrub * hollow * 0.45)
push("BerlinAsh", scrub * smooth((n_macro - 0.62) / 0.15) * 0.55)
# 6 Spree bank: mud at the waterline
push("BerlinMud", bank * (0.45 + 0.35 * n_fine))
# 7 round 4: big 30-60 m dark patches on the pale plains, darkest round the ruins
n_big = noise(45, 3, 5)
push("BerlinAsh", rubble_pure * smooth((n_big - 0.45) / 0.15) * 0.5)
push("BerlinMud", rubble_pure * smooth((0.35 - n_big) / 0.12) * 0.35)
push("BerlinAsh", smooth(1.0 - ruin_d / 16.0) * smooth((n_mid - 0.35) / 0.3) * 0.35)
# 8 round 4: break the glossy white glass rim ring (glass edge band gets ash and mud in noise)
gl = W0["BerlinGlass"]
band = smooth((gl - 25) / 40.0) * smooth((215 - gl) / 40.0)
push("BerlinAsh", band * smooth((n_fine - 0.35) / 0.25) * 0.55)
push("BerlinMud", band * smooth((n_mid - 0.55) / 0.2) * 0.35)
# 9 round 4: dark ring round the ruin footprint plates (FOOT_ pads), so their pale edge sinks into the ground
foot = np.zeros((N, N), bool)
for a in ACT:
    if a["label"].startswith("FOOT_") and a.get("box"):
        b, sc = a["box"], a.get("scale", 1.0)
        cxl, cyl = (b[0] + b[3]) * 0.5 * sc, (b[1] + b[4]) * 0.5 * sc
        hx, hy = (b[3] - b[0]) * 0.5 * sc, (b[4] - b[1]) * 0.5 * sc
        yw = math.radians(a["yaw"])
        wx_, wy_ = a["x"] + cxl * math.cos(yw) - cyl * math.sin(yw), a["y"] + cxl * math.sin(yw) + cyl * math.cos(yw)
        r = int(math.hypot(hx, hy) / Q) + 2
        i0_, j0_ = int((wx_ - CORNER) / Q), int((wy_ - CORNER) / Q)
        js, is_ = np.mgrid[max(j0_ - r, 0):min(j0_ + r, N), max(i0_ - r, 0):min(i0_ + r, N)]
        dx_, dy_ = CORNER + is_ * Q - wx_, CORNER + js * Q - wy_
        lx_, ly_ = dx_ * math.cos(yw) + dy_ * math.sin(yw), -dx_ * math.sin(yw) + dy_ * math.cos(yw)
        foot[js, is_] |= (np.abs(lx_) <= hx) & (np.abs(ly_) <= hy)
foot_d = ndimage.distance_transform_edt(~foot)
ring = smooth(1.0 - (foot_d - 0.5) / (3.0 + 2.0 * n_fine)) * (foot_d > 0)
# round 5: the ring doubled the plates' own halo (read as a double-exposed stamp), so it is no longer pushed
# 10 round 5: dark 20-30 m mud / ash patches on the Reichstag front apron, more gradient on the far plains
apron = ((X > 800) & (X < 5200) & (Y > -20500) & (Y < -11500)).astype(np.float32)
apron = ndimage.gaussian_filter(apron, 6.0)
n_ap = noise(24, 3, 7)
push("BerlinMud", apron * smooth((n_ap - 0.5) / 0.15) * 0.55)
push("BerlinAsh", apron * smooth((0.42 - n_ap) / 0.12) * 0.5)
far = np.zeros((N, N), np.float32)
for (fx, fy, fr) in ((-22000, -25000, 9000), (-20000, 40000, 9000), (14000, 43000, 7000)):
    far = np.maximum(far, smooth(1.0 - np.hypot(X - fx, Y - fy) / fr))
push("BerlinAsh", far * smooth((n_big - 0.4) / 0.15) * 0.45)
push("BerlinMud", far * smooth((0.4 - n_big) / 0.15) * 0.35)
# 7 ghost streets: the old side-street grid shows as broken setts inside the big empty fields
dens = ndimage.uniform_filter((ruin_d < 3).astype(np.float32), 31)
ghost = M["side_street"] & (dens < 0.02) & (ruin_d > 10)
push("BerlinCobble", ndimage.gaussian_filter(ghost.astype(np.float32), 1.2) * smooth((n_fine - 0.3) / 0.3) * 0.7)

# quantise, renormalise to exactly 255 per vertex
Q8 = {n: np.round(W[n]).astype(np.int32) for n in names}
diff = 255 - sum(Q8.values())
big = np.argmax(np.stack([Q8[n] for n in names]), 0)
for k, n in enumerate(names):
    Q8[n] += np.where(big == k, diff, 0)
S = sum(Q8.values())
assert S.min() == 255 and S.max() == 255
for n in names:
    assert Q8[n].min() >= 0 and Q8[n].max() <= 255
    Q8[n].astype(np.uint8).tofile(OUT + "new_%s.r8" % n)
changed = sum(np.abs(Q8[n] - W0[n]) for n in names) / 2
print("vertices changed > 10/255: %.1f %%, mean change %.1f, kept untouched %.1f %%" % (
    100 * (changed > 10).mean(), changed.mean(), 100 * (changed == 0).mean()))
print("rim unchanged:", bool((changed[rim] == 0).all()), " glass unchanged:", bool((changed[W0["BerlinGlass"] > 64] == 0).all()))
col = {"BerlinRubble": (150, 120, 105), "BerlinAsh": (70, 70, 74), "BerlinCobble": (175, 175, 165),
       "BerlinGlass": (60, 90, 95), "BerlinScrub": (110, 120, 70), "BerlinMud": (85, 65, 45)}
for tag, WW in (("before", W0), ("after", Q8)):
    img = sum(np.asarray(WW[n], np.float32)[..., None] * np.array(col[n], np.float32) for n in names) / 255.0
    j_lo, j_hi = int((-50000 - CORNER) / Q), int((50000 - CORNER) / Q)
    i_lo, i_hi = int((-26000 - CORNER) / Q), int((26000 - CORNER) / Q)
    crop = img[j_lo:j_hi, i_lo:i_hi].transpose(1, 0, 2)[::-1]
    Image.fromarray(np.clip(crop, 0, 255).astype(np.uint8)).save(OUT + "paint_%s.png" % tag)
