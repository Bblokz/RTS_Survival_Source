"""Agent A (system Python + numpy): collision vs render surface along the rays of <key>_coll.json.
Per collision variant: rays hitting both, median gap (collision in front of the bags), share within 30 cm,
rays blocked by collision where the render mesh is never hit (holes in the air) and render hits the collision misses."""
import json
import sys

import numpy as np

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Refinery')
from collision_gap import first_hits, load_triangles  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'


def main(keys):
    for key in keys:
        tris, _ = load_triangles(ROOT + 'agentA_' + key + '_tris.bin')
        data = json.load(open(ROOT + 'agentA_' + key + '_coll.json'))
        rays = np.array(data['rays'], dtype=np.float64)
        render, _ = first_hits(tris, rays[:, :3], rays[:, 3:6])
        render[render > rays[:, 6]] = np.inf
        print('==', key, 'rays', len(rays), 'render hits', int(np.isfinite(render).sum()))
        for name, hits in data['meshes'].items():
            coll = np.array([np.inf if h is None else h for h in hits])
            both = np.isfinite(render) & np.isfinite(coll)
            gaps = render[both] - coll[both]
            air = int((np.isfinite(coll) & ~np.isfinite(render)).sum())
            missed = int((np.isfinite(render) & ~np.isfinite(coll)).sum())
            if both.sum() == 0:
                print('  %-8s NO COLLISION HITS (invalid measurement) render_missed=%d' % (name, missed))
                continue
            print('  %-8s both=%4d median gap=%5.1f p90=%6.1f within30=%3.0f%% behind5=%3.0f%% air_blocks=%d render_missed=%d'
                  % (name, both.sum(), np.median(gaps), np.percentile(gaps, 90), 100 * np.mean(gaps < 30),
                     100 * np.mean(gaps < -5), air, missed))


if __name__ == '__main__':
    main(sys.argv[1:] or ['TwinMG42', 'BS3', 'Mortar120', 'B4Stepped', 'ML20Twin'])
