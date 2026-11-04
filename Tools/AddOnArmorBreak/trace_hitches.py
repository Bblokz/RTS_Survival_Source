"""Explains long frames in an add-on armour trace run (run_test.sh with TRACE=1): which timing scopes hold them.

Exports every timing event of the traced threads with a headless UnrealInsights, then for each event longer than
HITCH_SECONDS lists the scopes nested in it that take more than SHARE of its time, deepest first. That chain names
what the stalled thread was doing. Events on other threads that overlap the stall are listed too, so a render thread
waiting on a worker or the RHI shows the work it waited for.

Usage: python trace_hitches.py <run .utrace> [<thread filter, default GameThread,RenderThread*,RHIThread>]
"""
import csv
import os
import subprocess
import sys

INSIGHTS = 'D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealInsights.exe'
HITCH_SECONDS = 0.08
SHARE = 0.25
# Long-lived scopes that wrap every frame or wait; they say nothing about the cause.
WRAPPER_TIMERS = {'FEngineLoop::Tick', 'FRenderingThread::Run', 'RenderingThreadMain', 'FRHIThread::Run'}


def export_events(trace_path, threads):
    csv_path = os.path.splitext(trace_path)[0] + '_events.csv'
    if os.path.exists(csv_path):
        return csv_path
    log_path = os.path.splitext(trace_path)[0] + '_insights.log'
    # No quotes inside: the command line arrives as one quoted switch, and nested quotes stop it from running.
    command = (f'TimingInsights.ExportTimingEvents {csv_path} -threads={threads} '
               f'-columns=ThreadName,TimerName,StartTime,EndTime,Duration,Depth')
    subprocess.run([INSIGHTS, f'-OpenTraceFile={trace_path}', f'-ABSLOG={log_path}', '-AutoQuit', '-NoUI',
                    f'-ExecOnAnalysisCompleteCmd={command}', '-log'], check=True)
    return csv_path


def load_events(csv_path):
    events = []
    with open(csv_path, newline='', encoding='utf-8', errors='replace') as file:
        for row in csv.DictReader(file):
            events.append((row['ThreadName'], row['TimerName'], float(row['StartTime']), float(row['EndTime']),
                           float(row['Duration']), int(row['Depth'])))
    return events


def explain_hitch(hitch, events):
    thread, timer, start, end, duration, depth = hitch
    print(f'\n{thread} {timer} {duration * 1000:.0f} ms at {start:.3f}s')
    inside = [e for e in events if e[0] == thread and e[5] > depth and e[2] >= start and e[3] <= end
              and e[4] >= SHARE * duration]
    for event in sorted(inside, key=lambda e: (e[5], e[2])):
        print(f'  {"  " * (event[5] - depth)}{event[1]} {event[4] * 1000:.0f} ms')
    others = [e for e in events if e[0] != thread and e[2] < end and e[3] > start and e[4] >= SHARE * duration
              and e[1] not in WRAPPER_TIMERS]
    for event in sorted(others, key=lambda e: -e[4])[:8]:
        print(f'  overlapping {event[0]} d{event[5]} {event[1]} {event[4] * 1000:.0f} ms')


def main():
    trace_path = sys.argv[1]
    threads = sys.argv[2] if len(sys.argv) > 2 else 'GameThread,RenderThread*,RHIThread'
    events = load_events(export_events(trace_path, threads))
    hitches = [e for e in events if e[4] >= HITCH_SECONDS and e[1] not in WRAPPER_TIMERS and e[5] <= 1]
    print(f'{os.path.basename(trace_path)}: {len(events)} events, {len(hitches)} top-level scopes over '
          f'{HITCH_SECONDS * 1000:.0f} ms')
    for hitch in sorted(hitches, key=lambda e: e[2]):
        explain_hitch(hitch, events)


if __name__ == '__main__':
    main()
