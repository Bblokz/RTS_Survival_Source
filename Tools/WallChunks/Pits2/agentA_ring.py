"""Agent A (system Python): top-view height maps of the pit meshes from the *_tris.bin dumps, plus per-direction
ring numbers (where the bags stand tallest along 8 directions) for the rim aim points."""
import math
import struct
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'


def load(key):
    raw = open(ROOT + 'agentA_' + key + '_tris.bin', 'rb').read()
    rec = struct.calcsize('<h9f')
    arr = np.frombuffer(raw, dtype=np.dtype([('s', '<i2'), ('v', '<f4', (9,))]), count=len(raw) // rec)
    return arr['s'], arr['v'].reshape(-1, 3, 3).astype(np.float64)


def heightmap(tris, cell=10.0):
    pts = tris.reshape(-1, 3)
    # sample triangle interiors too
    a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
    samples = [pts]
    for u, v in ((1 / 3, 1 / 3), (0.6, 0.2), (0.2, 0.6), (0.2, 0.2), (0.5, 0.5), (0.5, 0.0), (0.0, 0.5)):
        samples.append(a + (b - a) * u + (c - a) * v)
    pts = np.concatenate(samples)
    mn = pts[:, :2].min(0)
    idx = ((pts[:, :2] - mn) / cell).astype(int)
    shape = idx.max(0) + 1
    hm = np.full(shape, np.nan)
    flat = idx[:, 0] * shape[1] + idx[:, 1]
    order = np.argsort(pts[:, 2])
    hmf = hm.reshape(-1)
    hmf[flat[order]] = pts[order, 2]
    return hm, mn, cell


def main(keys):
    for key in keys:
        slots, tris = load(key)
        hm, mn, cell = heightmap(tris)
        fig, ax = plt.subplots(figsize=(8, 8))
        ext = [mn[0], mn[0] + hm.shape[0] * cell, mn[1], mn[1] + hm.shape[1] * cell]
        im = ax.imshow(hm.T, origin='lower', extent=ext, cmap='viridis')
        fig.colorbar(im, ax=ax, label='z')
        ax.set_title(key + ' top view (x right, y up), max z per 10 cm')
        ax.grid(True, alpha=0.3)
        fig.savefig(ROOT + 'agentA_' + key + '_top.png', dpi=80)
        plt.close(fig)
        print('==', key, 'tris', len(tris), 'z range %.0f..%.0f' % (tris[..., 2].min(), tris[..., 2].max()))
        # per direction: the cell with max z along the ray from the centre, and the outermost geometry radius
        xs = mn[0] + (np.arange(hm.shape[0]) + 0.5) * cell
        ys = mn[1] + (np.arange(hm.shape[1]) + 0.5) * cell
        X, Y = np.meshgrid(xs, ys, indexing='ij')
        R = np.hypot(X, Y)
        A = np.degrees(np.arctan2(Y, X)) % 360
        for ang in range(0, 360, 30):
            d = np.abs(((A - ang + 180) % 360) - 180) < 6
            m = d & np.isfinite(hm)
            if not m.any():
                print('  %3d: none' % ang)
                continue
            zs = np.where(m, hm, -1e9)
            i = np.unravel_index(np.argmax(zs), zs.shape)
            tall = m & (hm > 30)
            rin = R[tall].min() if tall.any() else float('nan')
            rout = R[tall].max() if tall.any() else float('nan')
            print('  %3d: max z %.0f at r %.0f (%.0f,%.0f); z>30 from r %.0f to %.0f' % (ang, hm[i], R[i], X[i], Y[i], rin, rout))


if __name__ == '__main__':
    main(sys.argv[1:] or ['TwinMG42', 'BS3', 'Mortar120', 'B4Stepped', 'ML20Twin'])
