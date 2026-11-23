"""Side-by-side comparison of two economy sim runs (for example today's converter against a what-if override).

    python compare_runs.py <baseline run dir> <variant run dir>

Prints the opening (vehicle parts banked at fixed minutes), the build order's step times, the best 5-minute window
after the build order and the 30-minute totals. Reads only the CSVs the RTS_EconomySim director writes.
"""
import csv
import os
import sys

WINDOW_SECONDS = 300.0
METAL_WEIGHT = 2.0


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def num(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError, TypeError):
        return 0.0


def at_minute(timeline, minute, key):
    """Value of a column at the first sample on or after the minute."""
    for row in timeline:
        if num(row, "t") >= minute * 60.0:
            return num(row, key)
    return num(timeline[-1], key) if timeline else 0.0


def slope(timeline, key, t0, t1):
    inside = [r for r in timeline if t0 <= num(r, "t") <= t1]
    if len(inside) < 2:
        return 0.0
    dt = num(inside[-1], "t") - num(inside[0], "t")
    return (num(inside[-1], key) - num(inside[0], key)) * 60.0 / dt if dt > 0 else 0.0


def best_window(timeline, t_start):
    end_t = num(timeline[-1], "t")
    best, best_score = t_start, -1.0
    for row in timeline:
        t0 = num(row, "t")
        if t0 < t_start or t0 + WINDOW_SECONDS > end_t:
            continue
        score = slope(timeline, "harvested_R", t0, t0 + WINDOW_SECONDS) + METAL_WEIGHT * slope(
            timeline, "harvested_M", t0, t0 + WINDOW_SECONDS)
        if score > best_score:
            best, best_score = t0, score
    return best, best + WINDOW_SECONDS


def summarise(run_dir):
    timeline = read_csv(os.path.join(run_dir, "timeline.csv"))
    events = read_csv(os.path.join(run_dir, "events.csv"))
    enemy = read_csv(os.path.join(run_dir, "enemy_units.csv"))
    done = [num(e, "t") for e in events if e["kind"] == "build_order_done"]
    t_start = done[0] if done else 0.0
    t0, t1 = best_window(timeline, t_start)
    steps = [(num(e, "t"), e["detail"]) for e in events if e["kind"] == "step_done"]
    data = {
        "build order done (min)": t_start / 60.0,
        "converter on (min)": next((t / 60.0 for t, d in steps if "BTX_RefConverter" in d and "conversion" in d), 0.0),
        "VP banked at 5 min": at_minute(timeline, 5, "bank_VP"),
        "VP banked at 7 min": at_minute(timeline, 7, "bank_VP"),
        "VP banked at 10 min": at_minute(timeline, 10, "bank_VP"),
        "VP banked at 15 min": at_minute(timeline, 15, "bank_VP"),
        "VP banked at 30 min": at_minute(timeline, 30, "bank_VP"),
        "R banked at 30 min": at_minute(timeline, 30, "bank_R"),
        "R harvested at 30 min": at_minute(timeline, 30, "harvested_R"),
        "window (min)": "%.1f-%.1f" % (t0 / 60.0, t1 / 60.0),
        "window: R harvested /min": slope(timeline, "harvested_R", t0, t1),
        "window: M harvested /min": slope(timeline, "harvested_M", t0, t1),
        "window: VP made /min": slope(timeline, "bank_VP", t0, t1),
        "window: R left after conversion /min": slope(timeline, "bank_R", t0, t1) + slope(timeline, "R", t0, t1),
        "window: M left after conversion /min": slope(timeline, "bank_M", t0, t1) + slope(timeline, "M", t0, t1),
        "enemy trained units": sum(1 for e in enemy if e["origin"] in ("trained", "spawned")),
    }
    return data


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return
    base, variant = summarise(sys.argv[1]), summarise(sys.argv[2])
    print("| metric | %s | %s |" % (os.path.basename(sys.argv[1].rstrip("/\\")), os.path.basename(sys.argv[2].rstrip("/\\"))))
    print("|---|---|---|")
    for key in base:
        a, b = base[key], variant[key]
        fmt = (lambda v: v) if isinstance(a, str) else (lambda v: "%.1f" % v)
        print("| %s | %s | %s |" % (key, fmt(a), fmt(b)))


if __name__ == "__main__":
    main()
