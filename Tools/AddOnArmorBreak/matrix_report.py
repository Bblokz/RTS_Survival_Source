"""Summarises a perf matrix written by run_matrix.sh into Markdown tables.
Usage: python matrix_report.py <Saved/AddOnArmorBreak/matrix_<stamp>.txt>

Per technique, scale and phase: medians over the repeats of frame, game thread, render thread, GPU and process CPU,
then the cost over the None baseline (vehicles without skirts) of the same scale and phase, and the per-run change
from the idle to the hitting phase (the same run, so other load on the machine cancels out best there)."""
import re
import statistics
import sys
from collections import defaultdict

FIELDS = ['frame_avg', 'frame_p95', 'game_avg', 'render_avg', 'gpu_avg', 'cpu_cores', 'max_debris', 'max_particles']
PAIR = re.compile(r'(\w+)=(\S+)')


def parse(path):
    runs = []
    for line in open(path, encoding='utf-8'):
        values = dict(PAIR.findall(line))
        if 'phase' in values:
            runs.append(values)
    return runs


def median(values):
    return statistics.median(values) if values else float('nan')


def main():
    runs = parse(sys.argv[1])
    grouped = defaultdict(lambda: defaultdict(list))
    deltas = defaultdict(lambda: defaultdict(list))
    by_tag = defaultdict(dict)
    for run in runs:
        key = (run['technique'], int(run['vehicles']), float(run['hits_per_s']), run['phase'])
        for field in FIELDS:
            grouped[key][field].append(float(run[field]))
        by_tag[run['tag']][run['phase']] = run
    for tag, phases in by_tag.items():
        if 'idle' not in phases or 'hitting' not in phases:
            continue
        idle = phases['idle']
        hitting = phases['hitting']
        key = (idle['technique'], int(idle['vehicles']), float(idle['hits_per_s']))
        for field in ('frame_avg', 'frame_p95', 'game_avg', 'gpu_avg', 'cpu_cores'):
            deltas[key][field].append(float(hitting[field]) - float(idle[field]))

    print('## Medians per phase\n')
    print('| technique | vehicles | hits/s | phase | runs | frame avg | frame p95 | game | render | GPU | CPU cores | debris | particles |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|---|')
    for key in sorted(grouped, key=lambda k: (k[1], k[2], k[3], k[0])):
        technique, vehicles, hits, phase = key
        values = grouped[key]
        row = [median(values[f]) for f in FIELDS]
        print(f'| {technique} | {vehicles} | {hits:.0f} | {phase} | {len(values["frame_avg"])} | '
              + ' | '.join(f'{v:.2f}' for v in row[:6]) + f' | {row[6]:.0f} | {row[7]:.0f} |')

    print('\n## Cost over the None baseline (same scale and phase, medians, ms / cores)\n')
    print('| technique | vehicles | hits/s | phase | frame avg | frame p95 | game | GPU | CPU cores |')
    print('|---|---|---|---|---|---|---|---|---|')
    for key in sorted(grouped, key=lambda k: (k[1], k[2], k[3], k[0])):
        technique, vehicles, hits, phase = key
        baseline = grouped.get(('None', vehicles, hits, phase))
        if technique == 'None' or not baseline:
            continue
        values = grouped[key]
        cells = [median(values[f]) - median(baseline[f]) for f in ('frame_avg', 'frame_p95', 'game_avg', 'gpu_avg', 'cpu_cores')]
        print(f'| {technique} | {vehicles} | {hits:.0f} | {phase} | ' + ' | '.join(f'{c:+.2f}' for c in cells) + ' |')

    print('\n## Hitting minus idle within each run (median over repeats)\n')
    print('| technique | vehicles | hits/s | frame avg | frame p95 | game | GPU | CPU cores |')
    print('|---|---|---|---|---|---|---|---|')
    for key in sorted(deltas, key=lambda k: (k[1], k[2], k[0])):
        technique, vehicles, hits = key
        values = deltas[key]
        cells = [median(values[f]) for f in ('frame_avg', 'frame_p95', 'game_avg', 'gpu_avg', 'cpu_cores')]
        print(f'| {technique} | {vehicles} | {hits:.0f} | ' + ' | '.join(f'{c:+.2f}' for c in cells) + ' |')


main()
