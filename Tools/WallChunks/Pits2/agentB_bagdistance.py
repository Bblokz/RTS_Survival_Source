"""Agent B (system Python + numpy): how far each real-fire impact of agentB_pit_test.py lies from the sandbag surface.

A hole is a sphere at the hit point (clamped into the Sandbags bounds), so it only shows where that sphere reaches
bag triangles. Usage: python agentB_bagdistance.py <key> <sandbag tris key> [<editor log>]
"""
import json
import re
import struct
import sys

import numpy as np

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'


def bag_points(tris_key):
    raw = open(ROOT + 'agentB_' + tris_key + '_tris.bin', 'rb').read()
    record = struct.calcsize('<h9f')
    data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, record)
    tris = np.ascontiguousarray(data[:, 2:]).view(np.float32).reshape(-1, 3, 3).astype(np.float64)
    # Vertices plus triangle centres: dense enough (bags are ~25 cm) for a nearest-surface estimate.
    return np.concatenate([tris.reshape(-1, 3), tris.mean(axis=1)]), tris


def main():
    key, tris_key = sys.argv[1], sys.argv[2]
    points, tris = bag_points(tris_key)
    lo, hi = tris.reshape(-1, 3).min(axis=0), tris.reshape(-1, 3).max(axis=0)
    result = json.load(open(ROOT + key + '/headless_result.json'))
    impacts = result['impacts']
    rows = {}
    for impact in impacts:
        if impact['own_gun']:
            rows.setdefault('own_gun_' + str(impact['comp']), []).append(impact)
            continue
        rows.setdefault(str(impact['comp']), []).append(impact)
    print('impacts near the enemy pit:', len(impacts))
    for comp_name, items in sorted(rows.items()):
        line = f'  {comp_name}: {len(items)}'
        hits = [i['hit_local'] for i in items if 'hit_local' in i]
        if hits:
            hit = np.array(hits, dtype=np.float64)
            clamped = np.clip(hit, lo, hi)
            distance = np.array([np.min(np.linalg.norm(points - c, axis=1)) for c in clamped])
            line += (f'  dist to bags median {np.median(distance):.0f} cm, <=30: {int((distance <= 30).sum())}, '
                     f'<=60: {int((distance <= 60).sum())}, z median {np.median(hit[:, 2]):.0f}')
            for h, d in zip(hits, distance):
                line += f'\n      hit {h} -> {d:.0f} cm'
        print(line)
    if len(sys.argv) > 3:
        radii = re.findall(r'RTS_WALL_HOLES hit (\S+) radius=(\d+) direct=(\d) removed=([\d.]+) m2 holes=(\d+)',
                           open(sys.argv[3], encoding='utf-8', errors='ignore').read())
        print('LogWallHoleDamage hit lines:', len(radii))
        for row in radii:
            print('   ', row)


main()
