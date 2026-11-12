# blender -b <HQDeploy_VAT.blend> --factory-startup -P vat_events.py -- <pieces_vat.json> <out.json>
# Deploy-effect event timeline from the keyframed VAT scene (re-run after hand edits to the .blend, it re-times itself).
# Per piece: progress of FIRST CONTACT = first frame after it appears where it is back at (or within a few cm / degrees
# of) its rest pose; thrown pieces touch down before their final settle, hinged planks slap down, the mast plants.
# Contacts are clustered in space and time so production can play a few dozen pooled bursts instead of one per piece.
# Output (Unreal cm, mesh space): {"frames", "pieces": [...], "events": [{t, cat, centre, extent, count, size}], ...}
import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
table, dst = argv[0], argv[1]
data = json.load(open(table))
scene = bpy.context.scene
f0, f1 = scene['deploy_frame_start'], scene['deploy_frame_end']
objs = sorted((o for o in scene.objects if 'piece_id' in o), key=lambda o: o['piece_id'])
CONTACT_CM = 12.0
CONTACT_DEG = 6.0

first_visible = {}
contact = {}
for f in range(f0, f1 + 1):
    scene.frame_set(f)
    p = (f - f0) / (f1 - f0)
    for ob in objs:
        pid = ob['piece_id']
        if pid in contact or ob['category'] == 'truck':
            continue
        loc, rot, sca = ob.matrix_world.decompose()
        if sca.x < 0.5:
            continue
        if pid not in first_visible:
            first_visible[pid] = p
            # pieces that appear already in place (none expected) would contact at once; require some motion first
            continue
        off = (loc - common.from_ue(Vector(ob['rest_pivot_ue']))).length * common.TO_CM
        angle = math.degrees(rot.angle) if rot.angle <= math.pi else math.degrees(2 * math.pi - rot.angle)
        if off < CONTACT_CM and angle < CONTACT_DEG:
            contact[pid] = p

pieces = []
for r in data['pieces']:
    if r['cat'] == 'truck' or r['id'] not in contact:
        continue
    lo, hi = Vector(r['lo']), Vector(r['hi'])
    ext = hi - lo
    pieces.append({'id': r['id'], 'cat': r['cat'], 't': contact[r['id']], 'appear': first_visible.get(r['id'], 0.0),
                   'centre': [(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z], 'extent': [ext.x, ext.y, ext.z],
                   'size': max(ext.x, ext.y, ext.z)})

# cluster: same category, contact within WINDOW of the cluster's first contact, centre within RADIUS of its seed
WINDOW = {'deck': 0.03, 'sandbag': 0.035, 'prop': 0.03, 'structure': 0.03, 'mast': 0.0, 'antenna': 0.02}
RADIUS = {'deck': 300.0, 'sandbag': 260.0, 'prop': 220.0, 'structure': 350.0, 'mast': 0.0, 'antenna': 300.0}
events = []
for pc in sorted(pieces, key=lambda x: x['t']):
    target = None
    for ev in events:
        if ev['cat'] != pc['cat'] or pc['t'] - ev['t'] > WINDOW[pc['cat']]:
            continue
        d = math.hypot(pc['centre'][0] - ev['seed'][0], pc['centre'][1] - ev['seed'][1])
        if d <= RADIUS[pc['cat']]:
            target = ev
            break
    if target is None:
        events.append({'t': pc['t'], 'cat': pc['cat'], 'seed': pc['centre'][:2], 'members': [pc]})
    else:
        target['members'].append(pc)
out_events = []
for ev in events:
    m = ev['members']
    lo = [min(x['centre'][a] - x['extent'][a] / 2 for x in m) for a in (0, 1)]
    hi = [max(x['centre'][a] + x['extent'][a] / 2 for x in m) for a in (0, 1)]
    z = min(x['centre'][2] for x in m)
    out_events.append({'t': round(min(x['t'] for x in m), 4), 'cat': ev['cat'], 'count': len(m),
                       'centre': [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, z],
                       'extent': [hi[0] - lo[0], hi[1] - lo[1], max(x['extent'][2] for x in m)],
                       'size': max(x['size'] for x in m), 'ids': [x['id'] for x in m]})
out_events.sort(key=lambda e: e['t'])
counts = {}
for e in out_events:
    counts[e['cat']] = counts.get(e['cat'], 0) + 1
json.dump({'frames': f1 - f0 + 1, 'contact_cm': CONTACT_CM, 'pieces': pieces, 'events': out_events}, open(dst, 'w'), indent=1)
print('pieces with contact', len(pieces), 'of', len(objs) - 1, 'events', len(out_events), counts)
print('first/last contact', round(min(p['t'] for p in pieces), 3), round(max(p['t'] for p in pieces), 3))
for e in out_events:
    if e['cat'] in ('structure', 'mast'):
        print('  ', e['cat'], e['t'], [round(c) for c in e['centre']], 'size', round(e['size']))
