"""Offline (numpy): paint the BerlinGlass layer into the start crater patch exported by start_crater_fix.export_region.
Glass = 255 inside ~9 m of the crater centre, fading out to 0 by ~17 m with a ragged edge (the fallout glazed the bowl
and splashed over the rim). The other layers keep their mix and share what is left, so every vertex sums to 255.
Writes new_<layer>.r8 next to the orig_ files and prints a small coverage report."""
import math
import os

import numpy as np

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/start_resources/land/"
CENTRE = (17300.0, 47300.0)
R_FULL, R_ZERO = 900.0, 1700.0

lines = open(os.path.join(OUT, "grid.txt")).read().splitlines()
i0, j0, i1, j1 = map(int, lines[0].split())
lx, ly = map(float, lines[1].split())
sx, sy = map(float, lines[2].split())
names = lines[3].split()
nx, ny = i1 - i0 + 1, j1 - j0 + 1
W = {n: np.fromfile(os.path.join(OUT, "orig_%s.r8" % n), dtype=np.uint8).reshape(ny, nx).astype(np.float64)
     for n in names}
total = sum(W.values())
print("orig sum min/max", total.min(), total.max())

jj, ii = np.mgrid[0:ny, 0:nx]
wx = lx + (ii + i0) * sx
wy = ly + (jj + j0) * sy
dx, dy = wx - CENTRE[0], wy - CENTRE[1]
r = np.hypot(dx, dy)
ang = np.arctan2(dy, dx)
# ragged rim: a few angular harmonics, +-180 cm
edge = 180.0 * (0.55 * np.sin(3 * ang + 0.7) + 0.3 * np.sin(7 * ang + 2.1) + 0.15 * np.sin(13 * ang + 4.0))
t = np.clip((r + edge - R_FULL) / (R_ZERO - R_FULL), 0.0, 1.0)
target = 255.0 * (1.0 - (3 * t ** 2 - 2 * t ** 3))           # smoothstep fade

glass = np.maximum(W["BerlinGlass"], target)
others = [n for n in names if n != "BerlinGlass"]
rest = sum(W[n] for n in others)
left = 255.0 - glass
out = {"BerlinGlass": glass}
for n in others:
    share = np.where(rest > 0, W[n] / np.maximum(rest, 1e-6), 0.0)
    out[n] = share * left
# vertices that had only glass before but now need fill: give the remainder to ash
fill = 255.0 - sum(out.values())
out["BerlinAsh"] = out["BerlinAsh"] + np.clip(fill, 0, None)
q = {n: np.round(out[n]).astype(np.int32) for n in names}
diff = 255 - sum(q.values())                                   # rounding: fix on the largest layer
largest = np.argmax(np.stack([q[n] for n in names]), axis=0)
for k, n in enumerate(names):
    q[n] = q[n] + np.where(largest == k, diff, 0)
s = sum(q.values())
assert s.min() == 255 and s.max() == 255, (s.min(), s.max())
for n in names:
    assert q[n].min() >= 0 and q[n].max() <= 255
    q[n].astype(np.uint8).tofile(os.path.join(OUT, "new_%s.r8" % n))
print("glass >= 200 on %d of %d vertices (was %d); full-glass radius ~%.0f m" % (
    int((q["BerlinGlass"] >= 200).sum()), nx * ny, int((W["BerlinGlass"] >= 200).sum()), R_FULL / 100))
print("edge vertices unchanged:", bool(all((q[n][[0, -1], :] == W[n][[0, -1], :]).all() and
                                           (q[n][:, [0, -1]] == W[n][:, [0, -1]]).all() for n in names)))
