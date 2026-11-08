"""Recomputes add-on armour perf statistics from the per-frame CSVs, leaving out hitch frames.

ProfileGPU (-AddOnArmorBreakProfileGpu) stalls the frame it profiles and the one after it for 0.5-1.4 s, which
dominates a phase average. Frames above HITCH_MS are dropped and counted per run; a run that drops more than the
expected profiling frames is listed so a real hitch is not hidden.
Outside GPU load slows a whole run alike: a run's phase is left out when its median GPU time exceeds
GPU_OUTLIER_FACTOR times the median over all runs of the view (listed). Outside CPU load has no such control, so
the best (lowest) repeat is printed next to the median.

Usage: python frames_report.py <csv prefix, e.g. Perf_ or PerfClose_> [--no-profile] [<dir>]
  --no-profile: the runs had no -AddOnArmorBreakProfileGpu, so every dropped frame is a real hitch.
"""
import csv
import glob
import os
import re
import statistics
import sys

HITCH_MS = 250.0
GPU_OUTLIER_FACTOR = 1.5
# ProfileGPU frames per phase: the profiled frame and the frame after it.
EXPECTED_DROPS = {'idle': 2, 'hitting': 2, 'settle': 0}
TAG = re.compile(r'(\w*?)_(None|Niagara|GeometryCollection|BakedOpaque|BakedMask|BakedWpo)_v(\d+)_h(\d+)_r(\d+)_frames\.csv$')
ORDER = ['None', 'Niagara', 'GeometryCollection', 'BakedOpaque', 'BakedMask', 'BakedWpo']
PHASES = ['idle', 'hitting', 'settle']


def run_stats(path):
    """@return {phase: dict of per-run statistics} for one run's CSV."""
    rows = {}
    with open(path, newline='') as file:
        for row in csv.DictReader(file):
            rows.setdefault(row['phase'], []).append(row)
    result = {}
    for phase, phase_rows in rows.items():
        kept = [r for r in phase_rows if float(r['frame_ms']) <= HITCH_MS]
        frames = sorted(float(r['frame_ms']) for r in kept)
        result[phase] = {
            'dropped': len(phase_rows) - len(kept),
            'frame_mean': statistics.mean(frames),
            'frame_median': statistics.median(frames),
            'frame_p95': frames[int(0.95 * (len(frames) - 1))],
            'game_mean': statistics.mean(float(r['game_thread_ms']) for r in kept),
            'render_mean': statistics.mean(float(r['render_thread_ms']) for r in kept),
            'gpu_mean': statistics.mean(float(r['gpu_ms']) for r in kept),
            'gpu_median': statistics.median(float(r['gpu_ms']) for r in kept),
        }
    return result


def main():
    args = sys.argv[1:]
    expected_drops = EXPECTED_DROPS
    if '--no-profile' in args:
        args.remove('--no-profile')
        expected_drops = {phase: 0 for phase in PHASES}
    prefix = args[0]
    directory = args[1] if len(args) > 1 else 'D:/UE5Projects/RTS_Survival/Saved/AddOnArmorBreak'
    cells = {}
    warnings = []
    for path in glob.glob(os.path.join(directory, f'{prefix}*_frames.csv')):
        tag = TAG.search(os.path.basename(path))
        if not tag or tag.group(1) != prefix.rstrip('_'):
            continue
        technique, vehicles = tag.group(2), int(tag.group(3))
        for phase, stats in run_stats(path).items():
            stats['run'] = f'{technique} v{vehicles} r{tag.group(5)} {phase}'
            cells.setdefault((vehicles, phase, technique), []).append(stats)
            if stats['dropped'] > expected_drops.get(phase, 0):
                warnings.append(f'{os.path.basename(path)} {phase}: {stats["dropped"]} frames over {HITCH_MS:.0f} ms')
    view_gpu = statistics.median(run['gpu_median'] for runs in cells.values() for run in runs)
    for cell_key, runs in cells.items():
        slow = [run for run in runs if run['gpu_median'] > GPU_OUTLIER_FACTOR * view_gpu]
        for run in slow:
            warnings.append(f'EXCLUDED {run["run"]}: median GPU {run["gpu_median"]:.1f} ms (view median {view_gpu:.1f})')
        cells[cell_key] = [run for run in runs if run not in slow]
    keys = ['frame_mean', 'frame_median', 'frame_p95', 'game_mean', 'render_mean', 'gpu_mean']
    print(f'| technique | vehicles | phase | runs | dropped | {" | ".join(keys)} | best frame | best game | frame vs None | GPU vs None |')
    print('|' + '---|' * (len(keys) + 9))
    for vehicles, phase, technique in sorted(cells, key=lambda k: (k[0], PHASES.index(k[1]), ORDER.index(k[2]))):
        runs = cells[(vehicles, phase, technique)]
        if not runs:
            continue
        medians = {key: statistics.median(run[key] for run in runs) for key in keys}
        baseline = cells.get((vehicles, phase, 'None'))
        delta_frame = delta_gpu = ''
        if baseline:
            delta_frame = f'{medians["frame_mean"] - statistics.median(r["frame_mean"] for r in baseline):+.2f}'
            delta_gpu = f'{medians["gpu_mean"] - statistics.median(r["gpu_mean"] for r in baseline):+.2f}'
        dropped = '/'.join(str(run['dropped']) for run in runs)
        values = ' | '.join(f'{medians[key]:.2f}' for key in keys)
        best = f'{min(r["frame_mean"] for r in runs):.2f} | {min(r["game_mean"] for r in runs):.2f}'
        print(f'| {technique} | {vehicles} | {phase} | {len(runs)} | {dropped} | {values} | {best} | {delta_frame} | {delta_gpu} |')
    for warning in warnings:
        print(warning if warning.startswith('EXCLUDED') else 'WARNING ' + warning)


if __name__ == '__main__':
    main()
