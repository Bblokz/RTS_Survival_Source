"""Round 5 card fix (review r4 S5): rust mats on the cracked-mud paint (Weedy layer, the same mud as the pit floor)
read as orange straw. From plan_r4.json: on mud drop the rust mats, keep half the tussocks as the grey '#pit' variant.
Patches untouched (they were verified and topped up in the editor). Writes plan_r5.json."""
import json

import numpy as np

import fol_plan as fp

MUD = 0.35
p = json.load(open(fp.ROOT + 'plan_r4.json'))
w = np.load(fp.SURVEY + 'weights.npz')['Weedy']
rng = np.random.default_rng(55)
out = {}
moved = dropped = 0
for key, items in p['detail'].items():
    for it in items:
        i, j = fp.to_ij(it[0], it[1])
        i, j = int(round(i)), int(round(j))
        mud = 0 <= i < fp.NI and 0 <= j < fp.NJ and w[j, i] > MUD
        if mud and '#pit' not in key:
            if 'DryStar' not in key or rng.random() < 0.5:
                dropped += 1
                continue
            out.setdefault(key + '#pit', []).append(it)
            moved += 1
            continue
        out.setdefault(key, []).append(it)
p['detail'] = out
p['round'] = 5
p['counts'] = {k.split('/')[-1]: len(v) for k, v in out.items()}
json.dump(p, open(fp.ROOT + 'plan_r5.json', 'w'))
print('mud cards dropped', dropped, 'greyed', moved, p['counts'])
