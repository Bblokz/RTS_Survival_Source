"""Plots -AircraftFlightValidate CSVs side by side: top view, speed, yaw rate, sideslip and height above ground.

Usage: python plot_flight_qa.py <out.png> <label>=<csv> [<label>=<csv> ...]
Also prints per-step percentiles (p50/p95/max) of the yaw acceleration, so single-frame spikes and steady roughness
can be told apart.
"""
import csv
import sys

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:  # Only the summary without a working matplotlib.
    plt = None


def load(path):
    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))
    for row in rows:
        for key, value in row.items():
            if key != "step":
                row[key] = float(value)
    return rows


def percentile(values, share):
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(share * (len(ordered) - 1)))]


def summarize(label, rows):
    steps = []
    for row in rows:
        if not steps or steps[-1][0] != row["step"]:
            steps.append((row["step"], []))
        steps[-1][1].append(row)
    print(f"== {label}")
    for step, step_rows in steps:
        moving = [r for r in step_rows if r["speed_ratio"] >= 0.3]
        accel = [abs(r["yaw_accel"]) for r in moving]
        slip = [r["slip"] for r in moving]
        speed = [r["speed_ratio"] for r in step_rows[15:]] or [0.0]
        above = [r["z"] - r["ground_z"] for r in step_rows]
        print(f"  {step:15s} frames={len(step_rows):5d} speed min={min(speed):.2f} p50={percentile(speed, .5):.2f} "
              f"slip p95={percentile(slip, .95):5.1f} max={max(slip or [0]):5.1f} "
              f"yaw_accel p50={percentile(accel, .5):6.0f} p95={percentile(accel, .95):6.0f} max={max(accel or [0]):7.0f} "
              f"above_ground min={min(above):6.0f}")


def main():
    out_path = sys.argv[1]
    runs = []
    for argument in sys.argv[2:]:
        label, path = argument.split("=", 1)
        runs.append((label, load(path)))
    for label, rows in runs:
        summarize(label, rows)
    if plt is None:
        print("matplotlib unavailable: no plot written")
        return
    figure, axes = plt.subplots(len(runs), 5, figsize=(26, 5 * len(runs)), squeeze=False)
    for row_index, (label, rows) in enumerate(runs):
        start = rows[0]["t"]
        time = [r["t"] - start for r in rows]
        top, speed_axis, yaw_axis, slip_axis, height_axis = axes[row_index]
        top.scatter([r["x"] for r in rows], [r["y"] for r in rows], c=time, s=2, cmap="viridis")
        top.set_aspect("equal")
        top.set_title(f"{label}: top view (colour = time)")
        speed_axis.plot(time, [r["speed_ratio"] for r in rows])
        speed_axis.set_ylim(0, 1.2)
        speed_axis.set_title("speed / cruise speed")
        yaw_axis.plot(time, [r["yaw_rate"] for r in rows], label="yaw rate deg/s")
        yaw_axis.plot(time, [r["roll"] for r in rows], label="roll deg")
        yaw_axis.legend()
        yaw_axis.set_title("yaw rate and roll")
        slip_axis.plot(time, [r["slip"] for r in rows])
        slip_axis.set_title("nose vs flight direction (deg)")
        height_axis.plot(time, [r["z"] - r["ground_z"] for r in rows])
        height_axis.set_title("height above ground (cm)")
        step_starts = [(time[i], rows[i]["step"]) for i in range(len(rows)) if i == 0 or rows[i]["step"] != rows[i - 1]["step"]]
        for axis in (speed_axis, yaw_axis, slip_axis, height_axis):
            for step_time, step_name in step_starts:
                axis.axvline(step_time, color="grey", lw=0.5)
                axis.text(step_time, axis.get_ylim()[1], step_name, fontsize=7, rotation=90, va="top")
    figure.tight_layout()
    figure.savefig(out_path, dpi=70)
    print("wrote", out_path)


if __name__ == "__main__":
    main()
