"""Summarises the ProfileGPU dumps of add-on armour perf runs (-AddOnArmorBreakProfileGpu) per technique and phase.

Each run dumps one GPU frame halfway through the idle and the hitting phase. The pass times are taken from the
PerfView scene capture (the measured game view), so the main offscreen viewport does not dilute them.
A single frame is noisy: compare medians over the repeats, and the draw / primitive counts, which are exact.
Outside GPU load slows every pass of a run alike. A dump is left out when a pass the skirt material cannot affect
(CONTROL_PASSES) takes over CONTROL_FACTOR times its median over all dumps of the view; left-out dumps are listed.

Usage: python profilegpu_report.py [--view Close] [<log dir>]   (default D:/UE5Projects/RTS_Survival/Saved/Logs)
"""
import glob
import os
import re
import statistics
import sys

PASSES = ['PrePass', 'BasePass', 'ShadowDepths', 'RenderDeferredLighting', 'Translucency', 'PostProcessing',
          'Nanite::VisBuffer']
CONTROL_PASSES = ['PostProcessing', 'Nanite::VisBuffer', 'RenderDeferredLighting']
CONTROL_FACTOR = 1.5
BEGIN = re.compile(r'RTS_ADDON_ARMOR_PROFILEGPU begin tag=(\S+) phase=(\S+)')
ROW = re.compile(r'LogRHI:(\s*)([\d.]+)% ([\d.]+)ms\s+(.*)$')
COUNTS = re.compile(r'(\d+) draws? (\d+) prims')
TAG = re.compile(r'Perf([A-Za-z]*)_(None|Niagara|GeometryCollection|BakedOpaque|BakedMask|BakedWpo)_v(\d+)_h(\d+)_r(\d+)')


def parse_dump(lines, start):
    """@return {pass: (ms, draws, prims)} for the scene capture plus 'Capture' (its total) and 'Frame'."""
    result = {}
    capture_indent = None
    for line in lines[start:]:
        if 'LogRHI:' not in line:
            if result:
                break
            continue
        row = ROW.search(line)
        if not row:
            continue
        indent, milliseconds, name = len(row.group(1)), float(row.group(3)), row.group(4)
        counts = COUNTS.search(name)
        draws, prims = (int(counts.group(1)), int(counts.group(2))) if counts else (0, 0)
        if name.startswith('Frame '):
            result['Frame'] = (milliseconds, draws, prims)
            continue
        if name.startswith('SceneCapture '):
            capture_indent = indent
            result['Capture'] = (milliseconds, draws, prims)
            continue
        if capture_indent is None:
            continue
        if indent <= capture_indent:
            break
        for pass_name in PASSES:
            if name.startswith(pass_name) and pass_name not in result:
                result[pass_name] = (milliseconds, draws, prims)
    return result


def collect(log_dir, view):
    samples = {}
    for path in glob.glob(os.path.join(log_dir, 'AddOnArmorBreak_Perf*.log')):
        tag = TAG.search(os.path.basename(path))
        if not tag or '-backup-' in path or (tag.group(1) or '') != view:
            continue
        with open(path, encoding='utf-8', errors='replace') as log:
            lines = log.readlines()
        for index, line in enumerate(lines):
            begin = BEGIN.search(line)
            if begin:
                key = (tag.group(2), int(tag.group(3)), begin.group(2))
                dump = parse_dump(lines, index + 1)
                dump['run'] = int(tag.group(5))
                samples.setdefault(key, []).append(dump)
    return samples


def main():
    args = sys.argv[1:]
    view = ''
    if '--view' in args:
        view = args[args.index('--view') + 1]
        del args[args.index('--view'):args.index('--view') + 2]
    log_dir = args[0] if args else 'D:/UE5Projects/RTS_Survival/Saved/Logs'
    samples = collect(log_dir, view)
    all_dumps = [dump for dumps in samples.values() for dump in dumps]
    control_medians = {name: statistics.median(d[name][0] for d in all_dumps if name in d) for name in CONTROL_PASSES}
    excluded = []
    for key, dumps in samples.items():
        kept = []
        for dump in dumps:
            slow = [n for n in CONTROL_PASSES if n in dump and dump[n][0] > CONTROL_FACTOR * control_medians[n]]
            if slow:
                excluded.append(f'{key[0]} r{dump["run"]} {key[2]}: ' + ', '.join(f'{n} {dump[n][0]:.2f} ms' for n in slow))
            else:
                kept.append(dump)
        samples[key] = kept
    columns = ['Frame', 'Capture'] + PASSES
    print(f'view={view or "Game"}  median ms over repeats (n), capture draws / kprims in brackets')
    print('technique            vehicles phase    runs  ' + ''.join(f'{c:>24}' for c in columns))
    order = ['None', 'Niagara', 'GeometryCollection', 'BakedOpaque', 'BakedMask', 'BakedWpo']
    for key in sorted(samples, key=lambda k: (k[1], k[2], order.index(k[0]))):
        dumps = samples[key]
        if not dumps:
            continue
        cells = []
        for column in columns:
            values = [d[column] for d in dumps if column in d]
            if not values:
                cells.append(f'{"-":>24}')
                continue
            milliseconds = statistics.median(v[0] for v in values)
            draws = statistics.median(v[1] for v in values)
            prims = statistics.median(v[2] for v in values) / 1000.0
            cells.append(f'{milliseconds:>8.2f} ({draws:>4.0f}/{prims:>6.0f})')
        runs = ','.join(str(d['run']) for d in dumps)
        print(f'{key[0]:<20} {key[1]:>8} {key[2]:<8} {runs:<5} ' + ''.join(cells))
    print('control medians: ' + ', '.join(f'{n} {m:.2f} ms' for n, m in control_medians.items()))
    for line in excluded:
        print('EXCLUDED', line)


if __name__ == '__main__':
    main()
