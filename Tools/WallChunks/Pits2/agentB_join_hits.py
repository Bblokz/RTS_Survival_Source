"""Agent B: join LogWallHoleDamage hit lines on the enemy pit to agentB_pit_test impact records by time.
Usage: python agentB_join_hits.py <key> <tris key> <editor log> <pit actor name>"""
import json, re, struct, sys
from datetime import datetime
import numpy as np
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
key, tris_key, log_path, pit = sys.argv[1:5]
raw = open(ROOT + 'agentB_' + tris_key + '_tris.bin', 'rb').read()
rec = struct.calcsize('<h9f')
tris = np.ascontiguousarray(np.frombuffer(raw, dtype=np.uint8).reshape(-1, rec)[:, 2:]).view(np.float32).reshape(-1, 3, 3).astype(float)
points = np.concatenate([tris.reshape(-1, 3), tris.mean(axis=1)])
lo, hi = tris.reshape(-1, 3).min(axis=0), tris.reshape(-1, 3).max(axis=0)
stamp = re.compile(r'^\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d):(\d{3})\]')
def secs(line):
    m = stamp.match(line)
    if not m:
        return None
    return datetime.strptime(m.group(1), '%Y.%m.%d-%H.%M.%S').timestamp() + int(m.group(2)) / 1000.0
# Join by engine frame: log lines carry GFrameCounter % 1000, impact records the full frame count; the wall
# clock (offset from the 't=' lines) picks the right thousand.
frame_re = re.compile(r'^\[[^\]]+\]\[\s*(\d+)\]')
hits, offsets = [], []
for line in open(log_path, encoding='utf-8', errors='ignore'):
    f = frame_re.match(line)
    if not f:
        continue
    m = re.search(r'AGENTB_TEST t=([\d.]+)', line)
    if m:
        offsets.append(secs(line) - float(m.group(1)))
    m = re.search(r'RTS_WALL_HOLES hit (\S+) radius=(\d+) direct=(\d) removed=([\d.]+)', line)
    if m and m.group(1) == pit:
        hits.append((int(f.group(1)), secs(line), int(m.group(2)), float(m.group(4))))
offset = float(np.median(offsets))
result_path = ROOT + key if key.endswith('.json') else ROOT + key + '/headless_result.json'
impacts = json.load(open(result_path))['impacts']
if len(sys.argv) > 5:
    impacts = [i for i in impacts if i.get('pit') == sys.argv[5]]
def where(i):
    if i['comp'] is None:
        e = i['end_local']
        return ('inside_other' if np.hypot(e[0], e[1]) < 900 else 'outside_other'), e
    return i['comp'], i['hit_local']
cats = {}
for frame, wall, radius, removed in hits:
    t = wall - offset
    near = [i for i in impacts if 'frame' in i and (i['frame'] - frame) % 1000 in (0, 1, 999) and abs(i['t'] - t) < 4]
    if not near:
        cat, dist = 'unmatched', None
    else:
        best = min(near, key=lambda i: abs(i['t'] - t))
        cat, p = where(best)
        dist = float(np.min(np.linalg.norm(points - np.clip(np.array(p, float), lo, hi), axis=1)))
    if cat != 'unmatched':
        sub = cat + (' near' if dist is not None and dist < max(radius, 1) else ' far')
    else:
        sub = cat
    visible = removed > 0 and dist is not None and dist < radius
    cats.setdefault(sub, [0, 0, 0])
    cats[sub][0] += 1
    cats[sub][1] += removed > 0
    cats[sub][2] += visible
print('log hit lines', len(hits), 'clock offset spread', round(np.ptp(offsets), 2))
print('category: lines / removed>0 / visible (removed>0 and bag surface within the hole radius)')
for k, v in sorted(cats.items()):
    print('  %-28s %3d %3d %3d' % (k, *v))
print('total visible', sum(v[2] for v in cats.values()), 'of', len(hits))
