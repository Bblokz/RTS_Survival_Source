"""Agent B: top-down + radial profile plots of the B-4 / ML-20 pit meshes from agentB_dump.py tris.bin (system Python)."""
import struct, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'

def load(key):
    raw = open(ROOT + 'agentB_' + key + '_tris.bin', 'rb').read()
    rec = struct.calcsize('<h9f'); n = len(raw) // rec
    a = np.array([struct.unpack_from('<h9f', raw, i * rec) for i in range(n)])
    return a[:, 0].astype(int), a[:, 1:].reshape(n, 3, 3)

for pair, out in ((('B4Pit_Sandbags', 'B4Pit_Emplacement'), 'B4Pit'), (('B4ConcretePit_Sandbags', 'B4ConcretePit'), 'B4ConcretePit')):
    fig, ax = plt.subplots(1, 3, figsize=(24, 8))
    for key, colour in zip(pair, ('tab:brown', 'tab:gray')):
        slot, tris = load(key)
        c = tris.mean(axis=1)
        if key.endswith('Sandbags'):
            ax[0].scatter(c[:, 0], c[:, 1], s=0.3, c=c[:, 2], cmap='viridis')
        else:
            for s in np.unique(slot):
                m = slot == s
                ax[1].scatter(c[m, 0], c[m, 1], s=1, label='%s slot %d' % (key, s))
        r = np.hypot(c[:, 0], c[:, 1])
        ax[2].scatter(r, c[:, 2], s=0.3, c=colour, label=key)
        print(key, 'tris', len(tris), 'z %.0f..%.0f' % (tris[..., 2].min(), tris[..., 2].max()),
              'r pct 5/50/95 %.0f %.0f %.0f' % tuple(np.percentile(r, (5, 50, 95))))
    ax[0].set_title(pair[0] + ' (colour z)'); ax[0].set_aspect('equal')
    ax[1].set_title(pair[1]); ax[1].legend(); ax[1].set_aspect('equal')
    ax[2].set_title('radius vs z'); ax[2].legend()
    plt.savefig(ROOT + 'agentB_' + out + '_layout.png', dpi=60)
