"""Offline (numpy): Ger_Berlin start crater, owner's pick 2026-11-19 = concept v02 option 2 look B "Mauerreste"
(Docs/Concepts/BerlinStartCrater/v02; geometry O2R in the concept tools' bsc_common.py).

Reads the region crater_probe.py exported (Saved/BerlinCrater/land/orig_*), adds the concept height delta evaluated at
the REAL landscape vertex positions to the REAL heights, repaints the layers (look B: BerlinGlass on the pan floor and
under the crystals, ash on the inner wall, rubble under the rim) and writes new_height.r16 + new_<layer>.r8 for
start_crater.import_region(), plus ground.npz (new ground in world metres) for the Blender rim kit.

    python crater_heights.py
"""
import json
import math
import sys

import numpy as np

TOOLS = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinStartCrater"
sys.path.insert(0, TOOLS)
import bsc_common as K  # noqa: E402

LAND = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/land/"
KEY = "2b"


def sramp(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def blur(a, px):
    """Separable box blur x3 (about gaussian), edge-clamped."""
    out = a.astype(np.float64)
    r = max(1, int(round(px)))
    for _ in range(3):
        for ax in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[ax] = (r, r)
            p = np.pad(out, pad, mode="edge")
            c = np.cumsum(p, axis=ax)
            c = np.concatenate([np.zeros_like(np.take(c, [0], axis=ax)), c], axis=ax)
            n = out.shape[ax]
            out = (np.take(c, np.arange(2 * r + 1, n + 2 * r + 1), axis=ax) - np.take(c, np.arange(0, n), axis=ax)) / (2 * r + 1)
    return out


def ellipse(U, V, u, v, rx, ry, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    du, dv = U - u, V - v
    lx, ly = du * c + dv * s, -du * s + dv * c
    return (lx / rx) ** 2 + (ly / ry) ** 2 <= 1.0


def main():
    g = json.load(open(LAND + "grid.json"))
    nx, ny = g["i1"] - g["i0"] + 1, g["j1"] - g["j0"] + 1
    h16 = np.fromfile(LAND + "orig_height.r16", dtype=np.uint16).reshape(ny, nx)
    jj, ii = np.mgrid[0:ny, 0:nx]
    X = g["lx"] + (ii + g["i0"]) * g["sx"]
    Y = g["ly"] + (jj + g["j0"]) * g["sy"]
    U, V = (X - K.C[0]) / 100.0, (Y - K.C[1]) / 100.0
    z0 = g["lz"] + (h16.astype(np.float64) - 32768.0) * g["sz"] / 128.0          # world cm
    print("real ground in region: z %.0f .. %.0f cm (mean %.0f); inside 34 m of C: %.0f .. %.0f" % (
        z0.min(), z0.max(), z0.mean(), z0[np.hypot(U, V) < 34].min(), z0[np.hypot(U, V) < 34].max()))
    d = K.HEIGHT[KEY](U, V)                                                       # m, + up
    d = np.where(V > K.MARGIN_V, 0.0, d)
    border = np.concatenate([d[0], d[-1], d[:, 0], d[:, -1]])
    print("delta: min %.3f max %.3f m; border |max| %.4f; past Y 49100 |max| %.4f; at Y 48900-49100 |max| %.3f" % (
        d.min(), d.max(), np.abs(border).max(), np.abs(d[Y > 49100]).max(), np.abs(d[(Y > 48900) & (Y <= 49100)]).max()))
    assert np.abs(border).max() < 1e-3, "delta reaches the region border"
    assert np.abs(d[Y > 49100]).max() < 1e-6
    # step at the Y 49100 clamp: the old ground resumes there, so the last row before it must already be ~0
    edge_step = np.abs(d[(Y > 48950) & (Y <= 49100)]).max()
    print("largest delta in the last 1.5 m before Y 49100: %.3f m" % edge_step)
    z1 = z0 + d * 100.0
    h1 = np.round(32768.0 + (z1 - g["lz"]) * 128.0 / g["sz"])
    assert h1.min() >= 0 and h1.max() <= 65535
    h1.astype(np.uint16).tofile(LAND + "new_height.r16")

    # slopes on the real result (1 m grid)
    gy, gx = np.gradient(z1 / 100.0)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    inner = np.hypot(U - 1, V + 5) < 26
    print("slope inside 26 m of the bowl centre: max %.1f deg; > 12 deg on %d m2; > 20 deg on %d m2" % (
        slope[inner].max(), int((slope[inner] > 12).sum()), int((slope[inner] > 20).sum())))

    # ---- paint (look B = 2b rules of bsc_prep.edit_weights, on the real weights)
    names = g["layers"]
    W = {n: np.fromfile(LAND + "orig_%s.r8" % n, dtype=np.uint8).reshape(ny, nx).astype(np.float64) / 255.0 for n in names}
    r = np.hypot(U, V)
    glass = np.clip((-d - 0.5) / 0.25, 0, 1) * (r < 30)
    glass = np.maximum(glass, (1 - sramp((r - 7.0) / 6.0)) * 0.9)
    glass = blur(glass > 0.5, 1.0) * 0.5 + glass * 0.5
    rim = np.zeros(U.shape, bool)
    for (u, v, rx, ry, hh, yaw) in K.heaps_for(KEY):
        rim |= ellipse(U, V, u, v, rx * 1.35, ry * 1.35, yaw)
    for (u0, v0, u1, v1, t, top, base) in K.walls_for(KEY):
        for s in np.linspace(0, 1, 9):
            rim |= np.hypot(U - (u0 + (u1 - u0) * s), V - (v0 + (v1 - v0) * s)) < 2.0
    rim = blur(rim, 1.5)
    gdy, gdx = np.gradient(d)
    slope_ash = np.clip((np.hypot(gdx, gdy) - 0.06) / 0.1, 0, 1) * (1 - glass)
    edit = (np.abs(d) > 0.005) | (glass > 0.001) | (rim > 0.001)
    out = {}
    for n in names:
        out[n] = W[n] * (1 - glass) * (1 - rim * 0.8) * (1 - slope_ash * 0.6)
    out["BerlinGlass"] = out["BerlinGlass"] + glass
    out["BerlinRubble"] = out["BerlinRubble"] + rim * 0.8 * (1 - glass)
    out["BerlinAsh"] = out["BerlinAsh"] + slope_ash * 0.6
    tot = sum(out.values()) + 1e-9
    q = {}
    for n in names:
        q[n] = np.where(edit, np.round(255.0 * out[n] / tot), np.round(W[n] * 255.0)).astype(np.int32)
    diff = 255 - sum(q.values())
    largest = np.argmax(np.stack([q[n] for n in names]), axis=0)
    for k, n in enumerate(names):
        q[n] = q[n] + np.where(largest == k, diff, 0)
    s = sum(q.values())
    orig_s = sum(np.round(W[n] * 255.0) for n in names)
    print("weight sums: new %d..%d (orig %d..%d)" % (s.min(), s.max(), orig_s.min(), orig_s.max()))
    for n in names:
        assert q[n].min() >= 0 and q[n].max() <= 255, n
        q[n].astype(np.uint8).tofile(LAND + "new_%s.r8" % n)
    unchanged = all((q[n][~edit] == np.round(W[n] * 255.0)[~edit]).all() for n in names)
    print("glass >= 200 on %d m2 (was %d); rubble >= 128 on %d m2; vertices outside the edit unchanged: %s" % (
        int((q["BerlinGlass"] >= 200).sum()), int((W["BerlinGlass"] * 255 >= 200).sum()),
        int((q["BerlinRubble"] >= 128).sum()), unchanged))
    np.savez(LAND + "ground.npz", U=U, V=V, X=X, Y=Y, z_old_m=z0 / 100.0, z_new_m=z1 / 100.0, delta_m=d,
             glass=q["BerlinGlass"] / 255.0, slope=slope)
    print("wrote new_height.r16, new_<layer>.r8, ground.npz", (nx, ny))


if __name__ == "__main__":
    main()
