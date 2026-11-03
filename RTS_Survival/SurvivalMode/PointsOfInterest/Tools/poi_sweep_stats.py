"""Statistics of a POI seed sweep (sweep.csv from -SurvivalPOISweep=N) for the round README.

usage: python poi_sweep_stats.py <sweep.csv>
"""
import csv
import statistics
import sys
from collections import Counter


def main():
    with open(sys.argv[1], encoding='utf-8') as handle:
        rows = list(csv.DictReader(handle))
    print(f'seeds: {len(rows)}')
    types = ['battlefield', 'hangar', 'crashsite', 'convoy', 'graveyard', 'pumpstation', 'relaystation']
    for name in ['sites'] + [t for t in types if t in rows[0]] + ['objects', 'wildlife_groups', 'violations']:
        values = [int(r[name]) for r in rows]
        print(f'{name}: min {min(values)} mean {statistics.mean(values):.2f} max {max(values)}')
    starts = sorted({key.split('_')[0] for key in rows[0] if key.startswith('s') and key.endswith('_scavenge')})
    for kind in ('scavenge', 'capture'):
        spreads = []
        missing = 0
        for row in rows:
            values = [float(row[f'{s}_{kind}']) for s in starts]
            if min(values) < 0:
                missing += 1
                continue
            spreads.append(max(values) / min(values))
        if spreads:
            spreads.sort()
            print(f'{kind} straight-distance spread max/min per seed: median {statistics.median(spreads):.2f}, '
                  f'p90 {spreads[int(0.9 * (len(spreads) - 1))]:.2f}, worst {spreads[-1]:.2f}; seeds with a start lacking one: {missing}')
    within = Counter()
    for row in rows:
        values = [int(row[f'{s}_within30k']) for s in starts]
        within[max(values) - min(values)] += 1
    print('POIs within 30 000 straight, max-min per seed: ' + ', '.join(f'{k}: {v} seeds' for k, v in sorted(within.items())))
    repeats = 0
    total = 0
    for row in rows:
        signatures = row['signatures'].split()
        total += len(signatures)
        repeats += len(signatures) - len(set(signatures))
    print(f'layout signatures: {total}, repeated inside one seed: {repeats}')


if __name__ == '__main__':
    main()
