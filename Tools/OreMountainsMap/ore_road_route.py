"""Least-cost route for the Base B road: SW corner -> crossing of the valley road -> knoll west ramp -> Base B."""
import json, heapq, math, sys
import numpy as np
from PIL import Image, ImageDraw
L = json.load(open("Saved/OreMountains/terrain/Ore_layout.json"))
V = 757
H = (np.fromfile(L["files"]["height"], dtype=np.uint16).reshape(V, V).astype(float) - 32768) * 100 / 128   # [iy, ix]
M = np.load(L["files"]["masks"])
masks = {k: M[k] for k in M.files}
print("masks", sorted(masks)[:60], masks["cliff"].shape)
def ij(x, y):
    return int(round((y + 37800) / 100)), int(round((x + 37800) / 100))
print("orientation check road mask at (3070,-20500):", masks["road"][ij(3070, -20500)], "transposed:", masks["road"][ij(3070, -20500)[::-1]])
gy, gx = np.gradient(H)
slope = np.degrees(np.arctan(np.hypot(gx, gy) / 100.0))
step = 3
S = slope[::step, ::step]
sub = lambda k: masks[k][::step, ::step]
def grow(a, n):
    out = a.copy()
    for _ in range(n):
        out[1:, :] |= a[:-1, :]; out[:-1, :] |= a[1:, :]; out[:, 1:] |= a[:, :-1]; out[:, :-1] |= a[:, 1:]
        a = out.copy()
    return out
blocked = grow(sub("cliff") | sub("peat_bog") | sub("bog_near") | sub("pads") | sub("crater") | sub("spoil_heap") | sub("viaduct_rubble"), 3) | (S > 22.0)
ramps = sub("ramps")
cost = 1.0 + (S / 4.0) ** 2
cost = cost * np.where(sub("woods_dense"), 6.0, 1.0) * np.where(sub("woods_thin"), 1.5, 1.0) * np.where(grow(sub("gully"), 2), 8.0, 1.0)
cost = cost * np.where(sub("wash"), 3.0, 1.0)
n = cost.shape[0]
Hs = H[::step, ::step]
cost = 1.0 + (S / 14.0) ** 2
cost = cost * np.where(sub("woods_dense"), 6.0, 1.0) * np.where(sub("woods_thin"), 1.5, 1.0) * np.where(grow(sub("gully"), 2), 8.0, 1.0)
cost = cost * np.where(sub("wash"), 3.0, 1.0) * np.where(grow(grow(sub("road"), 3), 3), 5.0, 1.0)
yy = (np.arange(n) * step * 100.0 - 37800.0)[:, None] * np.ones((1, n))
xx = np.ones((n, 1)) * (np.arange(n) * step * 100.0 - 37800.0)[None, :]
blocked |= (yy < -27900.0) & (xx > -14500.0)          # stay inside the play area once off the shoulder
def node(x, y):
    i, j = ij(x, y)
    return (min(n - 1, i // step), min(n - 1, j // step))
def world(i, j):
    return [j * step * 100.0 - 37800.0, i * step * 100.0 - 37800.0]
def route(a, b, allow=None):
    a, b = node(*a), node(*b)
    blk = blocked.copy()
    for p in (a, b):
        blk[max(0, p[0] - 3):p[0] + 4, max(0, p[1] - 3):p[1] + 4] = False
    if allow is not None:
        blk &= ~allow
    dist = np.full(cost.shape, np.inf); prev = {}; dist[a] = 0.0; heap = [(0.0, a)]
    while heap:
        d, cur = heapq.heappop(heap)
        if cur == b: break
        if d > dist[cur]: continue
        r, c = cur
        for dr, dc, w in ((1,0,1.0),(-1,0,1.0),(0,1,1.0),(0,-1,1.0),(1,1,1.414),(1,-1,1.414),(-1,1,1.414),(-1,-1,1.414)):
            rr, cc = r + dr, c + dc
            if rr < 0 or cc < 0 or rr >= n or cc >= n or blk[rr, cc]: continue
            g = abs(Hs[rr, cc] - Hs[r, c]) / (w * step * 100.0)
            nd = d + w * 0.5 * (cost[r, c] + cost[rr, cc]) * (1.0 + (g / 0.045) ** 2)
            if nd < dist[rr, cc]:
                dist[rr, cc] = nd; prev[(rr, cc)] = cur; heapq.heappush(heap, (nd, (rr, cc)))
    if b not in prev: return None
    nodes = [b]
    while nodes[-1] != a: nodes.append(prev[nodes[-1]])
    return [world(*p) for p in nodes[::-1]]
def smooth(pts, keep=6, rounds=3):
    pts = np.array(pts[::keep] + pts[-1:], float)
    for _ in range(rounds):
        q = 0.75 * pts[:-1] + 0.25 * pts[1:]; s = 0.25 * pts[:-1] + 0.75 * pts[1:]
        pts = np.vstack([pts[:1], np.column_stack([q, s]).reshape(-1, 2), pts[-1:]])
    return pts
START = (-17500.0, -32500.0)
CROSS_S, CROSS_N = (1500.0, -20500.0), (4700.0, -20500.0)
RAMP = [p for p in [f for f in (L["features"] if isinstance(L["features"], list) else L["features"].values()) if f["id"] == "knoll_ramp_w"][0]["centreline"]][::-1]   # foot -> top
END = (13300.0, -6700.0)
south_ramps = ramps & (xx < 0.0)
blocked |= grow(south_ramps, 4)                        # not down the player's own ramps
leg_s = route(START, CROSS_S)
leg_n = route(CROSS_N, RAMP[0], allow=ramps)
print("legs", leg_s is not None, leg_n is not None)
if leg_s is None or leg_n is None: sys.exit(1)
pts = np.vstack([smooth(leg_s), np.array([CROSS_S, CROSS_N]), smooth(leg_n)[1:], np.array(RAMP[1:]), np.array([END])])
# resample at 8 m
seg = np.hypot(*np.diff(pts, axis=0).T); cum = np.concatenate([[0], np.cumsum(seg)])
t = np.arange(0, cum[-1], 800.0)
R = np.column_stack([np.interp(t, cum, pts[:, 0]), np.interp(t, cum, pts[:, 1])])
R = np.vstack([R, pts[-1:]])
z = np.array([H[ij(x, y)] for x, y in R])
grade = np.abs(np.diff(z)) / np.hypot(*np.diff(R, axis=0).T) * 100
straight = math.hypot(END[0] - START[0], END[1] - START[1])
print("length %.0f m (straight %.0f m), max grade %.1f %%, mean %.1f %%, points %d" % (cum[-1] / 100, straight / 100, grade.max(), grade.mean(), len(R)))
bad = [(round(R[k][0]), round(R[k][1]), round(grade[k], 1)) for k in range(len(grade)) if grade[k] > 12]
print("grade > 12 %:", bad[:20])
json.dump({"points": R.tolist(), "cross_s": CROSS_S, "cross_n": CROSS_N}, open("Saved/OreMountains/road2_route.json", "w"))
im = Image.open("Saved/OreMountains/road2_plan0.jpg"); d = ImageDraw.Draw(im)
px = lambda x, y: ((y + 37800) / 100 * 2, (37800 - x) / 100 * 2)
d.line([px(*p) for p in R], fill=(255, 128, 0), width=4)
im.crop((0, 500, 900, 1514)).save("Saved/OreMountains/road2_plan1.jpg", quality=85)
