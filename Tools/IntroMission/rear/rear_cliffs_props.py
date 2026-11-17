"""Which actors to remove or re-seat for the rear-ridge rock faces (plain python + numpy).

Reads Saved/IntroRear/cliffs/actors_before.json (rear_cliffs_editor.dump_actors) and cliffs/cliff_masks.npz
(rear_cliffs_gen.py) and writes cliffs/prop_actions.json for rear_cliffs_editor.apply_props:
- on the steepened flanks (weight > 0.05) or under a face: the rear-area stones and decals go (user: "remove the small
  stones and decals here"), and so do shrubs, rocks and dead trees that would stand inside the rock;
- everything else there that sits on the ground follows the new height (cacti on the ridge tops stay).
"""
import json
import os
import sys

import numpy as np

D = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/cliffs/'
REMOVE_ON_FLANK = ('REAR_Detail_Stone', 'REAR_CraterStone', 'REAR_Decal_')
REMOVE_UNDER_FACE = ('REAR_Detail_Shrub', 'REAR_Detail_Rock', 'REAR_Detail_Tree')
NEVER = ('REAR_Cliff_', 'REAR_Road', 'REAR_Rail', 'REAR_Edge', 'REAR_Skirt', 'REAR_Water', 'Landscape', 'LAND_', 'REAR_Depot_')
SKIP_CLASSES = ('Landscape', 'LandscapeStreamingProxy', 'BlockingVolume', 'RecastNavMesh', 'NavMeshBoundsVolume',
                'RuntimeVirtualTextureVolume', 'WorldPartitionHLOD')


def main():
    m = np.load(D + 'cliff_masks.npz')
    weight, dz, fp = m['weight'], m['dz'], m['footprint']
    grid, origin = m['grid'], m['origin']
    # second pass (station east flank + nose, 2026-11-17): the map already carries the first cliff terrain, so actors
    # move by the difference to that terrain, not to the pre-cliff one
    prev = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/cliffs_v3_full_height.r16'   # the terrain on the map now
    cur = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/cliffs_full_height.r16'
    if os.path.exists(prev) and '--from-precliff' not in sys.argv:
        to_z = lambda h: (h.astype(np.float64) - 32768.0) * 100.0 / 128.0
        dz = to_z(np.fromfile(cur, np.uint16).reshape(weight.shape)) - to_z(np.fromfile(prev, np.uint16).reshape(weight.shape))
        print('re-seating against', prev)
    sy, sx = weight.shape
    actors = json.load(open(D + 'actors_before.json'))
    acts, report = [], {'remove': {}, 'move': {}, 'kept_under_face': []}
    for a in actors:
        if a['label'].startswith(NEVER) or a['cls'] in SKIP_CLASSES:
            continue
        i = int(round((a['x'] - origin[0]) / 100.0 - grid[0]))
        j = int(round((a['y'] - origin[1]) / 100.0 - grid[1]))
        if not (0 <= i < sx and 0 <= j < sy):
            continue
        w, d, under = float(weight[j, i]), float(dz[j, i]), bool(fp[j, i])
        on_flank = w > 0.05 or under
        if not on_flank:
            continue
        key = a['label'].rstrip('0123456789_')
        if a['label'].startswith(REMOVE_ON_FLANK) or (under and a['label'].startswith(REMOVE_UNDER_FACE)):
            acts.append(dict(path=a['path'], label=a['label'], action='remove'))
            report['remove'][key] = report['remove'].get(key, 0) + 1
            continue
        if under and not a['label'].startswith('REAR_Detail_Cactus'):
            report['kept_under_face'].append((a['label'], a['cls'], round(a['x']), round(a['y'])))
        if abs(d) > 3.0:
            acts.append(dict(path=a['path'], label=a['label'], action='move', dz=round(d, 1)))
            report['move'][key] = report['move'].get(key, 0) + 1
    json.dump(acts, open(D + 'prop_actions.json', 'w'), indent=1)
    print(json.dumps(report, indent=1))


if __name__ == '__main__':
    main()
