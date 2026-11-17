"""Writes Saved/GameMechanicsRecs/INDEX.md: every final recording (both aspects) with duration, map and voice count.

    python gmr_index.py            # write the index
    python gmr_index.py --clean    # also delete work dirs, scouts, review packs and probe files (keep finals only)

Only the latest version of each take exists (renders overwrite their finals), so the index is the deliverable list.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(r"D:/UE5Projects/RTS_Survival/Saved/GameMechanicsRecs")
JOBS = pathlib.Path(__file__).resolve().parent / "Jobs"
SCRATCH_DIRS = ("_work", "_Scout", "_review", "_Probes")
ASPECTS = ("16x9", "9x16")


def duration_of(mp4):
    result = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(mp4)],
                            capture_output=True, text=True, check=False)
    try:
        return float(result.stdout.strip())
    except ValueError:
        return 0.0


def manifest_value(folder, stem, key):
    path = folder / f"{stem}_manifest.json"
    if not path.exists():
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    for name in (key, key[:1].lower() + key[1:]):
        if name in data:
            return data[name]
    return None


def map_of(job_name):
    path = JOBS / f"{job_name}.json"
    if not path.exists():
        return "?"
    return json.loads(path.read_text(encoding="utf-8-sig")).get("MapPath", "?").rsplit("/", 1)[-1]


def write_index():
    rows = []
    for category in sorted(p for p in ROOT.iterdir() if p.is_dir() and not p.name.startswith("_")):
        for job in sorted(p for p in category.iterdir() if p.is_dir()):
            cells = []
            for aspect in ASPECTS:
                mp4 = job / f"{job.name}_{aspect}.mp4"
                if mp4.exists():
                    voices = manifest_value(job, f"{job.name}_{aspect}", "VoiceLinesHeard")
                    cells.append(f"[{aspect}]({mp4.relative_to(ROOT).as_posix()}) {duration_of(mp4):.0f} s"
                                 + (f", {voices} voice lines" if voices else ""))
                else:
                    cells.append(f"{aspect} missing")
            rows.append(f"| {category.name} | {job.name} | {map_of(job.name)} | {' | '.join(cells)} |")
    text = ["# Game mechanics recordings", "",
            "Latest version of every take; 16:9 is 1920x1080, 9:16 is 1080x1920, 60 fps, H.264 + AAC.",
            "Made with Source/Tools/GameMechanicsRecs (see its README).", "",
            "| Category | Recording | Map | 16:9 | 9:16 |", "|---|---|---|---|---|"] + rows
    (ROOT / "INDEX.md").write_text("\n".join(text) + "\n", encoding="utf-8")
    print(f"INDEX.md: {len(rows)} recordings")


def clean():
    for name in SCRATCH_DIRS:
        path = ROOT / name
        if path.exists():
            shutil.rmtree(path)
            print("removed", path)
    for probe in ROOT.glob("*/*/*_probe*"):
        probe.unlink()
        print("removed", probe)
    for stray in ROOT.glob("_queue*"):
        if stray.is_file():
            stray.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()
    if args.clean:
        clean()
    write_index()
    return 0


if __name__ == "__main__":
    sys.exit(main())
