"""Draws crew montage poses from a -TWQualityCrewAnims run as stick figures, on the CPU (no GPU, no renders).

Usage: python plot_crew_poses.py <run folder, e.g. Saved/TWCrewAnims/at> [max frames per strip]
For every subject and every distinct crew montage it writes <folder>/plots/<subject>__<montage>.png: a strip of
evenly spaced frames over the first play, each with a top view (X forward/downrange, Y right) and a side view
(X forward, Z up) in the gun's local space, the gun's bones drawn as grey dots and every operator as a stick figure.
"""
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.patches
import matplotlib.pyplot as plt
import pandas as pd

BONES = ["pelvis", "spine_03", "head", "upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r",
         "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"]
LINKS = [("pelvis", "spine_03"), ("spine_03", "head"), ("spine_03", "upperarm_l"), ("upperarm_l", "lowerarm_l"),
         ("lowerarm_l", "hand_l"), ("spine_03", "upperarm_r"), ("upperarm_r", "lowerarm_r"),
         ("lowerarm_r", "hand_r"), ("pelvis", "thigh_l"), ("thigh_l", "calf_l"), ("calf_l", "foot_l"),
         ("pelvis", "thigh_r"), ("thigh_r", "calf_r"), ("calf_r", "foot_r")]
OPERATOR_COLOURS = ["tab:red", "tab:blue", "tab:green", "tab:orange", "tab:purple"]


def load_weapon_frames(folder, subject):
    path = os.path.join(folder, f"weapon_{subject}.csv")
    if not os.path.exists(path):
        return None
    return pd.read_csv(path)


def load_geometry(folder, subject):
    path = os.path.join(folder, f"geom_{subject}.csv")
    return pd.read_csv(path) if os.path.exists(path) else None


def draw_geometry(axis, geometry, horizontal, vertical):
    """Physics bodies as grey boxes, sockets as small labelled crosses (deployed gun, its local space)."""
    if geometry is None:
        return
    for _, part in geometry.iterrows():
        low_h, high_h = part[f"min_{horizontal}"], part[f"max_{horizontal}"]
        low_v, high_v = part[f"min_{vertical}"], part[f"max_{vertical}"]
        if part["kind"] != "socket":
            continue  # body boxes are world AABBs of rotated bodies: too inflated to draw
        if part["kind"] == "socket":
            axis.plot(low_h, low_v, "x", color="black", markersize=3)
            axis.text(low_h, low_v, str(part["name"]), fontsize=3.5, color="black")
            continue
        axis.add_patch(matplotlib.patches.Rectangle((low_h, low_v), high_h - low_h, high_v - low_v, fill=True,
                                                    facecolor="lightgrey", edgecolor="grey", alpha=0.45,
                                                    linewidth=0.5))


def weapon_points_at(weapon_frames, time):
    if weapon_frames is None or weapon_frames.empty:
        return None
    row = weapon_frames.iloc[(weapon_frames["t"] - time).abs().argsort().iloc[0]]
    xs = [row[c] for c in weapon_frames.columns if c.endswith("_x")]
    ys = [row[c] for c in weapon_frames.columns if c.endswith("_y")]
    zs = [row[c] for c in weapon_frames.columns if c.endswith("_z")]
    return xs, ys, zs


def draw_figure(axis, row, colour, horizontal, vertical):
    for start, end in LINKS:
        axis.plot([row[f"{start}_{horizontal}"], row[f"{end}_{horizontal}"]],
                  [row[f"{start}_{vertical}"], row[f"{end}_{vertical}"]], color=colour, linewidth=1.4)
    for hand in ("hand_l", "hand_r"):
        axis.plot(row[f"{hand}_{horizontal}"], row[f"{hand}_{vertical}"], "o", color=colour, markersize=3)


def plot_montage(folder, subject, montage, crew, weapon_frames, geometry, max_frames):
    plays = crew[(crew["subject"] == subject) & (crew["montage"] == montage)]
    first_start = plays["t"].min()
    # First continuous play only: samples are one frame apart, a gap starts a later play.
    times = sorted(plays["t"].unique())
    first_play = [times[0]]
    for time in times[1:]:
        if time - first_play[-1] > 0.2:
            break
        first_play.append(time)
    picks = [first_play[round(i * (len(first_play) - 1) / max(1, max_frames - 1))] for i in range(max_frames)]
    picks = sorted(set(picks))
    figure, axes = plt.subplots(2, len(picks), figsize=(2.6 * len(picks), 5.4), squeeze=False)
    kind = plays["kind"].iloc[0]
    figure.suptitle(f"{subject}  {montage}  ({kind})  first play from t={first_start:.2f}s", fontsize=9)
    for column, time in enumerate(picks):
        frame = crew[(crew["subject"] == subject) & (abs(crew["t"] - time) < 1e-3)]
        points = weapon_points_at(weapon_frames, time)
        for row_index, (horizontal, vertical, title) in enumerate((("x", "y", "top"), ("x", "z", "side"))):
            axis = axes[row_index][column]
            draw_geometry(axis, geometry, horizontal, vertical)
            if points is not None:
                coordinate = {"x": points[0], "y": points[1], "z": points[2]}
                axis.scatter(coordinate[horizontal], coordinate[vertical], s=4, color="grey")
            for _, row in frame.iterrows():
                colour = OPERATOR_COLOURS[int(row["op"]) % len(OPERATOR_COLOURS)]
                draw_figure(axis, row, colour, horizontal, vertical)
                if row["montage"] == montage and row_index == 0:
                    axis.text(row["pelvis_x"], row["pelvis_y"], f"{row['pos']:.2f}", fontsize=6, color=colour)
            axis.set_aspect("equal")
            axis.tick_params(labelsize=5)
            axis.set_title(f"{title} t={time - first_start:.2f}", fontsize=7)
            if horizontal == "x" and vertical == "y":
                axis.invert_yaxis()
    out_dir = os.path.join(folder, "plots")
    os.makedirs(out_dir, exist_ok=True)
    figure.tight_layout()
    figure.savefig(os.path.join(out_dir, f"{subject}__{montage}.png"), dpi=110)
    plt.close(figure)


def main():
    folder = sys.argv[1]
    max_frames = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    crew = pd.read_csv(os.path.join(folder, "crew_pose.csv"))
    for subject in crew["subject"].unique():
        weapon_frames = load_weapon_frames(folder, subject)
        geometry = load_geometry(folder, subject)
        for montage in crew[crew["subject"] == subject]["montage"].unique():
            plot_montage(folder, subject, montage, crew, weapon_frames, geometry, max_frames)
            print(f"plotted {subject} {montage}")


if __name__ == "__main__":
    main()
