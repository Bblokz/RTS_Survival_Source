"""Location scouting: one top-down render per map with a still of every job's anchor and coloured markers on every
planned spawn (red = player 1, blue = player 2) and deploy / move target (yellow), so overlaps with buildings, cliffs
and water are caught before any final render.

    python gmr_scout.py                # write Jobs/_Scout_<Map>.json for every map the jobs use
    python gmr_scout.py --run          # also render them (16x9, keeps nothing but the sheet + a frame per job)

Image orientation of each still: up = the job's anchor +X, right = anchor +Y (the job's own frame).
"""
import argparse
import json
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gmr_shots import MAPS, base_job, ev, key, shot  # noqa: E402

SECONDS_PER_STILL = 0.6
SCOUT_DISTANCE = 17000.0
SCOUT_PITCH = 84.0


def rotate(x, y, yaw_deg):
    a = math.radians(yaw_deg)
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def job_markers(job):
    """World-space markers for one job (assumes AnchorActor is empty)."""
    ax, ay, yaw = job["Anchor"]["X"], job["Anchor"]["Y"], job.get("AnchorYaw", 0.0)
    markers = []
    for e in job["Events"]:
        if e["Type"] == "Spawn":
            o = e.get("Offset", {"X": 0, "Y": 0})
            colour = "red" if e.get("Player", 1) == 1 else "blue"
            count, columns, spacing = e.get("Count", 1), e.get("Columns", 0) or e.get("Count", 1), e.get("Spacing", 1200)
            for i in range(count):
                row, col = divmod(i, columns)
                in_row = min(columns, count - row * columns)
                lx, ly = rotate(-row * spacing, (col - (in_row - 1) / 2) * spacing, e.get("Yaw", 0.0))
                markers.append((o["X"] + lx, o["Y"] + ly, colour, 250))
        elif e["Type"] in ("Move", "AttackMove") and "TargetOffset" in e:
            t = e["TargetOffset"]
            markers.append((t["X"], t["Y"], "yellow", 180))
        elif e["Type"] == "Deploy":
            spawn = next((s for s in job["Events"] if s["Type"] == "Spawn" and s.get("Tag") == e.get("Tag")), None)
            if spawn is not None:
                dx, dy = rotate(e.get("Value", 0.0), 0.0, spawn.get("Yaw", 0.0))
                markers.append((spawn["Offset"]["X"] + dx, spawn["Offset"]["Y"] + dy, "green", 900))
        elif e["Type"] in ("Ability",):
            for name in ("Offset", "TargetOffset"):
                if name in e:
                    markers.append((e[name]["X"], e[name]["Y"], "magenta", 300))
    world = []
    for x, y, colour, radius in markers:
        wx, wy = rotate(x, y, yaw)
        world.append((ax + wx, ay + wy, colour, radius))
    return world


def actor_anchored_scout(job, map_key):
    """A job anchored on a level actor gets its own scout with the same anchor; markers stay in its frame."""
    scout = base_job(f"Scout_{job['JobName']}", "_Scout", map_key, (0, 0, 0), job.get("AnchorYaw", 0.0),
                     SECONDS_PER_STILL, ClearUnitsRadius=0.0, WarmupSeconds=5.0, AnchorActor=job["AnchorActor"])
    local = dict(job, Anchor={"X": 0.0, "Y": 0.0, "Z": 0.0}, AnchorYaw=0.0)
    for x, y, colour, radius in job_markers(local):
        scout["Events"].append(ev(-1, "Marker", Offset=(x, y, 0), Text=colour, Value=float(radius),
                                  Settings={"Height": 900.0}))
    scout["Shots"].append(shot(job["JobName"], 0, SECONDS_PER_STILL, "Orbit",
                               [key(0, "", 180, SCOUT_PITCH, SCOUT_DISTANCE, 24, look=(0, 0, 0))], occlusion=False))
    return scout


def scout_jobs(only=None):
    jobs_by_map = {}
    actor_scouts = []
    for path in sorted((HERE / "Jobs").glob("*.json")):
        if path.name.startswith("_"):
            continue
        if only and not any(token in path.stem for token in only):
            continue
        job = json.loads(path.read_text(encoding="utf-8-sig"))
        map_key = next((k for k, v in MAPS.items() if v["MapPath"] == job["MapPath"]), None)
        if map_key is None:
            continue
        if job.get("AnchorActor"):
            actor_scouts.append(actor_anchored_scout(job, map_key))
            continue
        jobs_by_map.setdefault(map_key, []).append(job)
    scouts = list(actor_scouts)
    for map_key, jobs in jobs_by_map.items():
        scout = base_job(f"Scout_{map_key}", "_Scout", map_key, (0, 0, 0), 0, SECONDS_PER_STILL * len(jobs),
                         ClearUnitsRadius=0.0, WarmupSeconds=5.0)
        if "LaunchArgs" in jobs[0]:
            scout["LaunchArgs"] = jobs[0]["LaunchArgs"]
        for index, job in enumerate(jobs):
            for wx, wy, colour, radius in job_markers(job):
                scout["Events"].append(ev(-1, "Marker", Offset=(wx, wy, 0), Text=colour, Value=float(radius),
                                          Settings={"Height": 900.0}))
            a = job["Anchor"]
            scout["Events"].append(ev(-1, "Marker", Offset=(a["X"], a["Y"], 0), Text="white", Value=150.0,
                                      Settings={"Height": 2500.0}))
            start = index * SECONDS_PER_STILL
            scout["Shots"].append(shot(job["JobName"], start, start + SECONDS_PER_STILL, "Orbit",
                                       [key(0, "", 180 + job.get("AnchorYaw", 0.0), SCOUT_PITCH, SCOUT_DISTANCE, 24,
                                            off=(a["X"], a["Y"], 0), look=(0, 0, 0))], occlusion=False))
        scouts.append(scout)
    return scouts


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--only", nargs="*", help="only jobs whose name contains one of these tokens")
    args = parser.parse_args()
    paths = []
    for scout in scout_jobs(args.only):
        path = HERE / "Jobs" / f"_{scout['JobName']}.json"
        path.write_text(json.dumps(scout, indent=1), encoding="utf-8")
        paths.append(path)
        print("wrote", path.name, len(scout["Shots"]), "stills")
    if args.run:
        import gmr_queue  # noqa: F401  (imported for its run loop)
        import subprocess
        subprocess.call([sys.executable, "-I", str(HERE / "gmr_queue.py")] + [str(p) for p in paths] +
                        ["--aspects", "16x9", "--keep-frames"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
