"""Summaries for the round README from the preview runs' manifests (markdown on stdout).

usage: python poi_summary.py <manifest a> [<manifest b> ...] [--determinism <manifest x> <manifest y>]
"""
import json
import math
import sys
from collections import Counter


def load(path):
    with open(path, encoding='utf-8') as handle:
        return json.load(handle)


def summarise(path):
    manifest = load(path)
    run = manifest['run']
    reference = manifest.get('reward_reference', {}).get('start_ring_wreck_expected_total', 0) or 1
    print(f"### seed {run['seed']} (picked start {run['picked_start']} {run['picked_start_name']})\n")
    types = Counter(p['type'] for p in manifest['pois'])
    print('Types: ' + ', '.join(f'{k} {v}' for k, v in sorted(types.items())) + f"; plan notes: {'; '.join(run['plan_notes']) or 'none'}\n")
    print('| id | type | sector | owner | actors | scavenge | capture | reward EV | x ref | worst gap | worst sink | nav (tank / inf) |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|')
    for poi in manifest['pois']:
        actors = [a for a in poi['actors'] if a.get('alive')]
        scavenge = sum(1 for a in actors if a['role'] == 'scavenge')
        capture = sum(1 for a in actors if a['role'] == 'capture')
        measured = [a for a in actors if a.get('seat_measured') and a['role'] not in ('decal', 'crater')]
        gap = max((a['gap'] for a in measured), default=0)
        sink = max((a['sink'] for a in measured), default=0)
        nav = [a['interaction_nav'] for a in actors if 'interaction_nav' in a]
        tank = sum(1 for n in nav if n['heavytank_reached'])
        infantry = sum(1 for n in nav if n['character_reached'])
        tag = ' (landmark)' if poi['landmark'] else ''
        print(f"| {poi['id']} | {poi['type']}{tag} | {poi['sector']} | {poi['owner_start']} | {len(actors)} | {scavenge} | {capture} | "
              f"{poi['reward_expected_total']:.0f} | {poi['reward_expected_total'] / reference:.1f} | {gap:.0f} | {sink:.0f} | {tank}/{len(nav)} , {infantry}/{len(nav)} |")
    print(f"\nStart-ring wreck reference: {reference:.1f} expected resources ({manifest['reward_reference'].get('how', '')}).\n")
    exclusions = manifest['exclusions']
    print(f"Exclusions: {exclusions['actors_checked']} actors and {exclusions['animal_samples_checked']} animal samples checked, "
          f"{len(exclusions['violations'])} violations{': ' + '; '.join(exclusions['violations'][:10]) if exclusions['violations'] else ''}.\n")
    print('| start | POIs within 30 000 straight / nav | nearest-by-drive scavenge site nav | nearest-by-drive capture site nav | old: nav to the straight-nearest scavenge / capture actor |')
    print('|---|---|---|---|---|')
    scavenge_nav, capture_nav = [], []
    for row in manifest['fairness']['per_start']:
        # Nearest by drive (round 2 on); older manifests only have the nav to the straight-nearest actor.
        scavenge_nav.append(row.get('scavenge_nav_site_heavytank', row.get('scavenge_nav_heavytank', -1)))
        capture_nav.append(row.get('capture_nav_site_heavytank', row.get('capture_nav_heavytank', -1)))
        print(f"| {row['start']} {row['name']} | {row['pois_within_30000_straight']} / {row.get('pois_within_30000_nav', '-')} | "
              f"{scavenge_nav[-1]:.0f} | {capture_nav[-1]:.0f} | "
              f"{row.get('scavenge_nav_heavytank', -1):.0f} / {row.get('capture_nav_heavytank', -1):.0f} |")
    for name, values in (('scavenge', scavenge_nav), ('capture', capture_nav)):
        good = [v for v in values if v > 0]
        spread = max(good) / min(good) if len(good) == len(values) and good else float('nan')
        print(f"\n{name} nav spread max/min: {spread:.2f}" + ('' if len(good) == len(values) else f' ({len(values) - len(good)} starts without a path)'))
    print()
    groups = Counter(w['name'] for w in manifest['wildlife'])
    animals = sum(w['actors_spawned'] for w in manifest['wildlife'] if w['kind'] == 'ground group')
    print('Wildlife: ' + ', '.join(f'{k} {v}' for k, v in groups.items()) + f'; {animals} ground animals.\n')


def signature(manifest):
    pois = [(p['type'], tuple(round(c) for c in p['centre'][:2]), p['yaw'] // 1, len(p['actors'])) for p in manifest['pois']]
    wildlife = [(w['name'], w['count'], tuple(round(c) for c in w['spawn_centre'][:2])) for w in manifest['wildlife']]
    return pois, wildlife


def determinism(path_a, path_b):
    a, b = load(path_a), load(path_b)
    pois_a, wild_a = signature(a)
    pois_b, wild_b = signature(b)
    actors_a = [(x['asset'], tuple(round(c) for c in x['xyz'][:2])) for p in a['pois'] for x in p['actors'] if x.get('alive')]
    actors_b = [(x['asset'], tuple(round(c) for c in x['xyz'][:2])) for p in b['pois'] for x in p['actors'] if x.get('alive')]
    moved = sum(1 for x, y in zip(actors_a, actors_b) if x[0] != y[0] or math.dist(x[1], y[1]) > 50)
    print(f"Determinism ({path_a} vs {path_b}): POIs identical {pois_a == pois_b}, wildlife spawn data identical {wild_a == wild_b}, "
          f"actors {len(actors_a)} vs {len(actors_b)}, differing class or > 50 cm XY: {moved + abs(len(actors_a) - len(actors_b))}.\n")


def main():
    arguments = sys.argv[1:]
    if '--determinism' in arguments:
        index = arguments.index('--determinism')
        pair = arguments[index + 1:index + 3]
        arguments = arguments[:index]
    else:
        pair = None
    for path in arguments:
        summarise(path)
    if pair:
        determinism(*pair)


if __name__ == '__main__':
    main()
