"""Build a review pack for a camera critic who reads images, not video.

Usage:
    python gmr_review.py <mp4 or folder> [--out DIR] [--every 0.5]

For every mp4 (a folder is searched recursively; _work and _review folders are skipped) frames are taken every
--every seconds, sampled in the middle of each interval (the first tile is not the black fade-in frame), labelled with
their time in the mp4 and tiled into 4 x 3 strip images (<out>/<JobName>/<take>_strip_NNN.jpg). review.md lists per
job: the mp4 paths and durations, the manifest's shot list (Name, Rig, start/end seconds relative to the cut,
OcclusionPitchBoost, FirstFocal), the manifest warnings and the strip images in order.
Default output: <GMR root>/_review/<timestamp>/.
"""
from __future__ import annotations

import argparse
import math
import re
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gmr_post import (  # noqa: E402
    ASPECT_LANDSCAPE, ASPECT_VERTICAL, FFMPEG, JPEG_QUALITY, PROBE_SUFFIX, WORK_FOLDER_NAME, GMRToolError,
    aspect_of_size, gmr_root, label_filter, normalise_keys, probe_duration, probe_video_stream, read_json, run_tool,
    VIDEO_FADE_IN_SECONDS, VIDEO_FADE_OUT_SECONDS, safe_folder_name, sample_filter,
)

REVIEW_FOLDER_NAME = "_review"
REVIEW_FILE_NAME = "review.md"
TIMESTAMP_FORMAT = "%Y%m%d_%H%M%S"
DEFAULT_EVERY_SECONDS = 0.5
STRIP_COLUMNS = 4
STRIP_ROWS = 3
STRIP_TILE_WIDTH = {ASPECT_LANDSCAPE: 640, ASPECT_VERTICAL: 360}
STRIP_INDEX_DIGITS = 3
ASPECT_ORDER = (ASPECT_LANDSCAPE, ASPECT_VERTICAL)
SKIPPED_FOLDERS = {WORK_FOLDER_NAME, REVIEW_FOLDER_NAME}
TEMP_VIDEO_SUFFIX = ".tmp.mp4"
TAKE_FILE_RE = re.compile(rf"^(?P<job>.+)_(?P<aspect>{ASPECT_LANDSCAPE}|{ASPECT_VERTICAL})(?P<probe>{PROBE_SUFFIX})?$")
MANIFEST_SUFFIX = "_manifest.json"
NO_VALUE = "-"


def tile_time_label(offset: float) -> str:
    """drawtext text for 't=12.25 s': the tile's time in the mp4 (sampled t plus the grid offset), to 1/100 s.

    Built from two integer expansions because drawtext has no float formatting; no commas, colons are escaped.
    """
    whole = f"trunc(t+{offset:.4f}+0.005)"
    hundredths = f"trunc((t+{offset:.4f})*100+0.5)-100*{whole}"
    return rf"t=%{{eif\:{whole}\:d}}.%{{eif\:{hundredths}\:d\:2}} s"


def find_videos(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    return sorted(path for path in target.rglob("*.mp4")
                  if not SKIPPED_FOLDERS.intersection(path.relative_to(target).parts[:-1])
                  and not path.name.endswith(TEMP_VIDEO_SUFFIX))


def describe_take(video: Path) -> dict:
    """Job name, aspect and probe flag from <JobName>_<aspect>[_probe].mp4 (aspect from the frame size otherwise)."""
    stream = probe_video_stream(video)
    width, height = int(stream.get("width") or 0), int(stream.get("height") or 0)
    match = TAKE_FILE_RE.match(video.stem)
    return {
        "video": video,
        "job": match.group("job") if match else video.stem,
        "aspect": match.group("aspect") if match else aspect_of_size(width, height),
        "probe": bool(match and match.group("probe")),
        "width": width,
        "height": height,
        "duration": probe_duration(video),
        "manifest_path": video.with_name(video.stem + MANIFEST_SUFFIX),
    }


def extract_strips(take: dict, out_dir: Path, every: float) -> list[tuple[Path, float, float]]:
    """Write <stem>_strip_NNN.jpg; returns (path, first tile time, last tile time) per strip."""
    tile_width = STRIP_TILE_WIDTH.get(take["aspect"], STRIP_TILE_WIDTH[ASPECT_LANDSCAPE])
    offset = every / 2.0
    stem = take["video"].stem
    for old in out_dir.glob(f"{stem}_strip_*.jpg"):
        old.unlink()
    pattern = out_dir / f"{stem}_strip_%0{STRIP_INDEX_DIGITS}d.jpg"
    tiles_total = max(1, math.floor(max(0.0, take["duration"] - offset) / every) + 1)
    video_filter = (f"{sample_filter(every, tiles_total, offset)},scale={tile_width}:-2,"
                    f"{label_filter(tile_time_label(offset), tile_width)},tile={STRIP_COLUMNS}x{STRIP_ROWS}")
    run_tool([FFMPEG, "-hide_banner", "-nostdin", "-y", "-i", str(take["video"]), "-an",
              "-vf", video_filter, "-fps_mode", "passthrough", "-q:v", str(JPEG_QUALITY), "-start_number", "1",
              str(pattern)])
    strips = sorted(out_dir.glob(f"{stem}_strip_*.jpg"))
    per_strip = STRIP_COLUMNS * STRIP_ROWS
    ranges = []
    for index, strip in enumerate(strips):
        first_tile = index * per_strip
        last_tile = min(tiles_total, first_tile + per_strip) - 1
        ranges.append((strip, offset + first_tile * every, offset + max(first_tile, last_tile) * every))
    return ranges


def shot_rows(manifest: dict) -> list[str]:
    fps = int(manifest.get("Fps") or 0)
    start = int(manifest.get("StartFrame", 0))
    rows = ["| # | Name | Rig | Start s | End s | OcclusionPitchBoost | FirstFocal |",
            "|---|------|-----|---------|-------|---------------------|------------|"]
    for index, shot in enumerate(manifest.get("Shots") or [], 1):
        def seconds(key: str) -> str:
            return f"{(int(shot.get(key, 0)) - start) / fps:.2f}" if fps else NO_VALUE
        rows.append(f"| {index} | {shot.get('Name', '')} | {shot.get('Rig', '')} | {seconds('StartFrame')} | "
                    f"{seconds('EndFrame')} | {float(shot.get('OcclusionPitchBoost', 0)):.1f} | "
                    f"{float(shot.get('FirstFocal', 0)):.1f} |")
    return rows


def take_section(take: dict, strips: list[tuple[Path, float, float]]) -> list[str]:
    lines = [f"### {take['aspect']}{' (probe)' if take['probe'] else ''}", "",
             f"- Video: `{take['video'].as_posix()}`",
             f"- Duration: {take['duration']:.2f} s ({take['width']}x{take['height']})"]
    manifest_path = take["manifest_path"]
    if manifest_path.exists():
        manifest = normalise_keys(read_json(manifest_path))
        lines += [f"- Manifest: `{manifest_path.as_posix()}` (complete: {bool(manifest.get('bComplete'))}, "
                  f"voice lines heard: {manifest.get('VoiceLinesHeard', 0)})", "", "Shots (seconds from the cut):", ""]
        lines += shot_rows(manifest)
        warnings = manifest.get("Warnings") or []
        lines += ["", "Warnings:" if warnings else "Warnings: none"]
        lines += [f"- {warning}" for warning in warnings]
    else:
        lines += ["- Manifest: none found next to the video"]
    lines += ["", "Strips (4 x 3 tiles, left to right, top to bottom):", ""]
    lines += [f"{index}. `{path.as_posix()}` ({first:.2f}-{last:.2f} s)"
              for index, (path, first, last) in enumerate(strips, 1)]
    return lines + [""]


def build_review(target: Path, out_dir: Path, every: float) -> Path:
    videos = find_videos(target)
    if not videos:
        raise GMRToolError(f"no mp4 found at {target}")
    out_dir.mkdir(parents=True, exist_ok=True)
    jobs: dict[str, list[dict]] = {}
    for video in videos:
        take = describe_take(video)
        jobs.setdefault(take["job"], []).append(take)
    lines = [f"# GMR review pack ({datetime.now():%Y-%m-%d %H:%M})", "",
             f"Source: `{target.as_posix()}`", "",
             f"One tile every {every:g} s, taken mid-interval (first tile at {every / 2:g} s). Each tile shows its "
             f"time in the mp4; the mp4 starts at the cut (manifest StartFrame) with a {VIDEO_FADE_IN_SECONDS:g} s "
             f"fade from black and ends with a {VIDEO_FADE_OUT_SECONDS:g} s fade to black.", ""]
    for job_name in sorted(jobs):
        takes = sorted(jobs[job_name], key=lambda take: (ASPECT_ORDER.index(take["aspect"]), take["probe"]))
        lines += [f"## {job_name}", ""]
        job_dir = out_dir / safe_folder_name(job_name, "Job")
        job_dir.mkdir(exist_ok=True)
        for take in takes:
            print(f"strips for {take['video'].name}")
            strips = extract_strips(take, job_dir, every)
            lines += take_section(take, strips)
    review = out_dir / REVIEW_FILE_NAME
    review.write_text("\n".join(lines), encoding="utf-8")
    return review


def main() -> int:
    sys.stdout.reconfigure(errors="replace")
    parser = argparse.ArgumentParser(description="Build image strips + review.md from GMR mp4s for a camera critic.")
    parser.add_argument("target", type=Path, help="an mp4, or a folder searched recursively for mp4s")
    parser.add_argument("--out", type=Path, default=None,
                        help=f"output folder (default <GMR root>/{REVIEW_FOLDER_NAME}/<timestamp>)")
    parser.add_argument("--every", type=float, default=DEFAULT_EVERY_SECONDS, help="seconds between tiles")
    args = parser.parse_args()
    if args.every <= 0:
        parser.error("--every must be positive")
    if not args.target.exists():
        parser.error(f"not found: {args.target}")
    out_dir = args.out or gmr_root() / REVIEW_FOLDER_NAME / datetime.now().strftime(TIMESTAMP_FORMAT)
    try:
        review = build_review(args.target.resolve(), out_dir.resolve(), args.every)
    except GMRToolError as error:
        print(f"ERROR: {error}")
        return 1
    print(f"review pack: {review}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
