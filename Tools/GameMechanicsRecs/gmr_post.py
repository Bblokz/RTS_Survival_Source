"""Post-process one GameMechanicsRecs take into the final mp4, contact sheet, manifest copy and log excerpt.

Usage:
    python gmr_post.py --work <work dir> [--keep-frames]

Importable:
    post_take(work_dir, keep_frames=False) -> Path | None   (the final mp4, or None when the take failed)

A work dir (written by gmr.py and the C++ director) holds Job.json, Manifest.json, render.log, the JPEG sequence
LS_GMR_<JobName>.<NNNNN>.jpeg and <JobName>_capture.wav (time 0 of the WAV == manifest StartFrame). The cut is
[StartFrame, EndFrame): 0.4 s fade from black, 0.6 s fade to black, H.264 CRF 16. Audio is gained to -14 LUFS from a
loudnorm measurement and peak-limited 4x oversampled at -1 dBTP, then padded with silence to the video length (the
video is never shortened). Output lands in <GMR root>/<Category>/<JobName>/ and replaces the previous version.

This module is also the shared base of the GMR tools (root folders, JSON I/O, ffmpeg helpers): gmr.py, gmr_queue.py
and gmr_review.py import from it, so it must not import any of them.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------------------------------------------------
# Folders
# ---------------------------------------------------------------------------------------------------------------------
TOOLS_DIR = Path(__file__).resolve().parent
# Source/Tools/GameMechanicsRecs -> Source/Tools -> Source -> project root.
PROJECT_ROOT = TOOLS_DIR.parents[2]
DEFAULT_GMR_ROOT = PROJECT_ROOT / "Saved" / "GameMechanicsRecs"
# Overrides the GMR root (work dirs, final outputs, lock and queue files), e.g. for a self-test in a temp folder.
OUTPUT_ROOT_ENV = "GMR_OUTPUT_ROOT"
WORK_FOLDER_NAME = "_work"
DEFAULT_CATEGORY = "Misc"

# ---------------------------------------------------------------------------------------------------------------------
# Work dir contents
# ---------------------------------------------------------------------------------------------------------------------
JOB_FILE_NAME = "Job.json"
MANIFEST_FILE_NAME = "Manifest.json"
RENDER_LOG_NAME = "render.log"
SEQUENCE_PREFIX = "LS_GMR_"
CAPTURE_WAV_SUFFIX = "_capture.wav"
FRAME_NUMBER_DIGITS = 5
PROBE_SUFFIX = "_probe"
# Python-only Job.json flag written by gmr.py: the take is a half-resolution probe.
PROBE_JOB_KEY = "Probe"
ASPECT_LANDSCAPE = "16x9"
ASPECT_VERTICAL = "9x16"
# A few lost JPEG writes are held with the previous frame; more than this many missing frames fails the take.
MAX_HELD_FRAMES = 3

# ---------------------------------------------------------------------------------------------------------------------
# Tools and fonts
# ---------------------------------------------------------------------------------------------------------------------
FFMPEG = "ffmpeg"
FFPROBE = "ffprobe"
FONT_FILE = "C:/Windows/Fonts/segoeui.ttf"
STDERR_TAIL_CHARS = 4000

# ---------------------------------------------------------------------------------------------------------------------
# Video encode
# ---------------------------------------------------------------------------------------------------------------------
VIDEO_CODEC = "libx264"
VIDEO_PROFILE = "high"
VIDEO_CRF = 16
VIDEO_PRESET = "slow"
VIDEO_PIXEL_FORMAT = "yuv420p"
# MRQ JPEGs are full-range BT.601 (JFIF); the mp4 is limited-range BT.709 and tagged so, which is what players
# assume for HD video when tags are ignored.
VIDEO_COLOR_RANGE = "tv"
VIDEO_COLOR_STANDARD = "bt709"
KEYFRAME_INTERVAL_SECONDS = 2
VIDEO_FADE_IN_SECONDS = 0.4
VIDEO_FADE_OUT_SECONDS = 0.6

# ---------------------------------------------------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------------------------------------------------
TARGET_LUFS = -14.0
TARGET_TRUE_PEAK_DB = -1.0
TARGET_LRA = 11.0
# Safety cap: a near-silent take must not have its noise floor pushed up by 40 dB.
MAX_LOUDNESS_GAIN_DB = 30.0
AUDIO_SAMPLE_RATE = 48000
# The limiter runs at 4x the output rate so its sample peaks approximate true peaks.
LIMITER_OVERSAMPLING = 4
LIMITER_ATTACK_MS = 2
LIMITER_RELEASE_MS = 50
AUDIO_CODEC = "aac"
AUDIO_BITRATE = "256k"
AUDIO_CHANNEL_LAYOUT = "stereo"
AUDIO_CHANNELS = 2
AUDIO_FADE_IN_SECONDS = 0.3
AUDIO_FADE_OUT_SECONDS = 0.6
# volumedetect mean below this counts as a silent capture (the render lost its audio).
SILENT_MEAN_VOLUME_DB = -70.0

# ---------------------------------------------------------------------------------------------------------------------
# Contact sheet and tile labels (gmr_review.py uses the label helpers too)
# ---------------------------------------------------------------------------------------------------------------------
SHEET_TILE_WIDTH = {ASPECT_LANDSCAPE: 480, ASPECT_VERTICAL: 270}
SHEET_COLUMNS = {ASPECT_LANDSCAPE: 6, ASPECT_VERTICAL: 8}
SHEET_TILES_PER_SECOND = 1
JPEG_QUALITY = 3  # ffmpeg -q:v (2 best .. 31 worst)
LABEL_FONT_DIVISOR = 12  # label font size = tile width / this
LABEL_STYLE = "fontcolor=white:box=1:boxcolor=black@0.6:boxborderw=6:x=10:y=10"
# drawtext expansion: the tile's whole second ("3 s"); colons inside %{...} are escaped for the filter parser.
SHEET_SECOND_LABEL = r"%{eif\:t\:d} s"

# ---------------------------------------------------------------------------------------------------------------------
# Log excerpt
# ---------------------------------------------------------------------------------------------------------------------
LOG_EXCERPT_MAX_LINES = 2000
LOG_EXCERPT_RE = re.compile(r"GMR_|LogGMR\w*:.*(?:Error|Warning: )|=== Critical error|Fatal error")

FRAME_FILE_RE_TEMPLATE = r"^{prefix}\.(\d+)\.(jpe?g)$"
LOUDNORM_JSON_RE = re.compile(r"\{[^{}]*\"input_i\"[^{}]*\}", re.S)
MEAN_VOLUME_RE = re.compile(r"mean_volume:\s*(-?inf|-?[\d.]+) dB")
BOOL_KEY_RE = re.compile(r"^b[A-Z]")


class GMRToolError(RuntimeError):
    """An external tool (ffmpeg, ffprobe) failed or a take's inputs are unusable."""


# ---------------------------------------------------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------------------------------------------------
def gmr_root() -> Path:
    """Root of all GMR output: Saved/GameMechanicsRecs, or the folder in GMR_OUTPUT_ROOT."""
    override = os.environ.get(OUTPUT_ROOT_ENV, "").strip()
    return Path(override).resolve() if override else DEFAULT_GMR_ROOT


def work_root() -> Path:
    return gmr_root() / WORK_FOLDER_NAME


def read_json(path: Path):
    """Read JSON written by hand or by PowerShell: a UTF-8 BOM is tolerated."""
    with open(path, "r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def write_json(path: Path, data, ascii_only: bool = False) -> None:
    """Write UTF-8 JSON WITHOUT a BOM (the C++ job reader rejects a BOM). Replaces the file atomically."""
    path = Path(path)
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(json.dumps(data, indent=2, ensure_ascii=ascii_only) + "\n", encoding="utf-8")
    os.replace(temp, path)


def normalise_keys(value):
    """FJsonObjectConverter writes camelCase keys (startFrame, voiceLinesHeard); the tools read the UPROPERTY names.

    Bool UPROPERTYs keep their lower-case b prefix (bComplete), so a key like "bComplete" is left as it is.
    """
    if isinstance(value, dict):
        normalised = {}
        for key, item in value.items():
            if key and not BOOL_KEY_RE.match(key):
                key = key[:1].upper() + key[1:]
            normalised[key] = normalise_keys(item)
        return normalised
    if isinstance(value, list):
        return [normalise_keys(item) for item in value]
    return value


def safe_folder_name(name: str, fallback: str) -> str:
    """A category or job name usable as a single folder name."""
    cleaned = re.sub(r"[^A-Za-z0-9_\- ]", "_", str(name or "")).strip(" .")
    return cleaned or fallback


def take_stem(job_name: str, aspect: str, probe: bool) -> str:
    """<JobName>_<aspect>[_probe]: the work dir name and the base name of every output file of the take."""
    return f"{job_name}_{aspect}{PROBE_SUFFIX if probe else ''}"


def output_dir_for(category: str, job_name: str) -> Path:
    return gmr_root() / safe_folder_name(category, DEFAULT_CATEGORY) / safe_folder_name(job_name, "Job")


def aspect_of_size(width: int, height: int) -> str:
    return ASPECT_VERTICAL if height > width else ASPECT_LANDSCAPE


def drawtext_path(path: str) -> str:
    """drawtext needs the drive colon escaped: C\\:/Windows/Fonts/segoeui.ttf."""
    return path.replace("\\", "/").replace(":", "\\:")


def label_filter(text_expression: str, tile_width: int) -> str:
    """drawtext filter for a tile label; text_expression may use drawtext expansions such as %{eif\\:t\\:d}."""
    font_size = max(1, tile_width // LABEL_FONT_DIVISOR)
    return (f"drawtext=fontfile='{drawtext_path(FONT_FILE)}':text='{text_expression}':"
            f"fontsize={font_size}:{LABEL_STYLE}")


def format_seconds(seconds: float) -> str:
    minutes, rest = divmod(int(round(seconds)), 60)
    return f"{minutes}m{rest:02d}s" if minutes else f"{rest}s"


def run_tool(cmd: list[str], check: bool = True, echo: bool = True) -> subprocess.CompletedProcess:
    """Run ffmpeg/ffprobe with captured output; raises GMRToolError on failure when check is set."""
    if echo:
        print("  $ " + subprocess.list2cmdline(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                            stdin=subprocess.DEVNULL)
    if check and result.returncode != 0:
        raise GMRToolError(f"{Path(cmd[0]).name} failed (exit {result.returncode}):\n"
                           f"{result.stderr[-STDERR_TAIL_CHARS:]}")
    return result


def probe_duration(path: Path) -> float:
    result = run_tool([FFPROBE, "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                      check=False, echo=False)
    try:
        return float(result.stdout.strip())
    except ValueError:
        return 0.0


def probe_video_stream(path: Path) -> dict:
    """width, height, nb_frames, r_frame_rate of the first video stream ({} when there is none)."""
    result = run_tool([FFPROBE, "-v", "error", "-select_streams", "v:0", "-show_entries",
                       "stream=width,height,nb_frames,r_frame_rate", "-of", "json", str(path)],
                      check=False, echo=False)
    try:
        streams = json.loads(result.stdout or "{}").get("streams", [])
    except json.JSONDecodeError:
        return {}
    return streams[0] if streams else {}


def measure_loudness(input_args: list[str], pre_filter: str = "") -> dict | None:
    """First loudnorm pass: integrated loudness (input_i), true peak (input_tp) and LRA of the given input."""
    loudnorm = (f"loudnorm=I={TARGET_LUFS}:TP={TARGET_TRUE_PEAK_DB}:LRA={TARGET_LRA}:print_format=json")
    result = run_tool([FFMPEG, "-hide_banner", "-nostdin", "-nostats"] + input_args
                      + ["-af", pre_filter + loudnorm, "-f", "null", "-"], check=False)
    match = LOUDNORM_JSON_RE.search(result.stderr)
    if not match:
        return None
    try:
        return json.loads(match.group(0))
    except json.JSONDecodeError:
        return None


def finite_number(value) -> float | None:
    """loudnorm reports "-inf" for digital silence; return None for anything that is not a finite number."""
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def mean_volume_db(wav: Path, duration: float) -> float | None:
    result = run_tool([FFMPEG, "-hide_banner", "-nostdin", "-nostats", "-i", str(wav),
                       "-af", f"atrim=end={duration:.4f},volumedetect", "-f", "null", "-"], check=False)
    match = MEAN_VOLUME_RE.search(result.stderr)
    return finite_number(match.group(1)) if match else None


# ---------------------------------------------------------------------------------------------------------------------
# Take inputs
# ---------------------------------------------------------------------------------------------------------------------
def find_frames(work: Path, job_name: str) -> dict[int, Path]:
    """Sequence frame number -> file. MRQ may name the files .jpeg or .jpg."""
    pattern = re.compile(FRAME_FILE_RE_TEMPLATE.format(prefix=re.escape(SEQUENCE_PREFIX + job_name)), re.I)
    frames = {}
    for path in work.iterdir():
        match = pattern.match(path.name)
        if match:
            frames[int(match.group(1))] = path
    return frames


def frame_path(work: Path, job_name: str, number: int, extension: str) -> Path:
    return work / f"{SEQUENCE_PREFIX}{job_name}.{number:0{FRAME_NUMBER_DIGITS}d}{extension}"


def prepare_frame_range(work: Path, job_name: str, frames: dict[int, Path], start: int, end: int) -> tuple[str, int]:
    """Make [start, end) one contiguous image2 sequence with one extension; returns (extension, held frame count).

    image2 stops at the first missing number, which would silently shorten the video, so a missing frame is filled
    with the nearest earlier one (or the next one at the very start). More than MAX_HELD_FRAMES gaps fails the take.
    """
    in_range = [frames[number] for number in range(start, end) if number in frames]
    if not in_range:
        raise GMRToolError(f"no frames in [{start}, {end})")
    extensions = [path.suffix.lower() for path in in_range]
    extension = max(set(extensions), key=extensions.count)
    missing = [number for number in range(start, end) if number not in frames]
    if len(missing) > MAX_HELD_FRAMES:
        raise GMRToolError(f"{len(missing)} frames missing in [{start}, {end}) (first {missing[0]}); "
                           f"at most {MAX_HELD_FRAMES} are held")
    for number in range(start, end):
        target = frame_path(work, job_name, number, extension)
        if number in frames:
            if frames[number] != target:
                os.replace(frames[number], target)
            frames[number] = target
            continue
        earlier = [n for n in range(number - 1, start - 1, -1) if n in frames]
        source = frames[earlier[0]] if earlier else frames[min(n for n in frames if n > number)]
        shutil.copyfile(source, target)
        frames[number] = target
    if missing:
        print(f"WARNING: {len(missing)} missing frame(s) held with a neighbour: {missing}")
    return extension, len(missing)


def hide_cut_frames(work: Path, job_name: str, manifest: dict, start: int, end: int, extension: str) -> int:
    """Replace the first frame of every shot after a cut with the frame after it.

    Movie Render Queue keeps temporal anti-aliasing history across a game camera cut (it only resets at its own
    shot boundaries), so the cut frame is a smeared blend of both shots. One duplicated frame at 60 fps is invisible.
    """
    replaced = 0
    for shot in manifest.get("Shots", [])[1:]:
        cut = int(shot.get("StartFrame", -1))
        if not (start < cut < end - 1):
            continue
        source = frame_path(work, job_name, cut + 1, extension)
        target = frame_path(work, job_name, cut, extension)
        if source.exists():
            shutil.copyfile(source, target)
            replaced += 1
    if replaced:
        print(f"cut frames replaced by their next frame: {replaced}")
    return replaced


def write_log_excerpt(render_log: Path, destination: Path) -> int:
    """Copy the GMR_ markers and LogGMR errors/warnings (plus crash lines) of render.log; returns the line count."""
    if not render_log.exists():
        destination.write_text("(no render.log in the work dir)\n", encoding="utf-8")
        return 0
    kept = []
    with open(render_log, "r", encoding="utf-8", errors="ignore") as stream:
        for line in stream:
            if LOG_EXCERPT_RE.search(line):
                kept.append(line.rstrip("\r\n"))
    if len(kept) > LOG_EXCERPT_MAX_LINES:
        half = LOG_EXCERPT_MAX_LINES // 2
        dropped = len(kept) - 2 * half
        kept = kept[:half] + [f"... {dropped} matching lines dropped ..."] + kept[-half:]
    destination.write_text("\n".join(kept) + "\n", encoding="utf-8")
    return len(kept)


# ---------------------------------------------------------------------------------------------------------------------
# Encode
# ---------------------------------------------------------------------------------------------------------------------
def build_audio_filter(duration: float, gain_db: float, has_audio: bool) -> str:
    """Slice -> gain -> 4x oversampled limiter at the TP target -> fades -> pad with silence to the video length."""
    steps = [f"atrim=end={duration:.4f}", "asetpts=PTS-STARTPTS"]
    if has_audio:
        ceiling = 10 ** (TARGET_TRUE_PEAK_DB / 20.0)
        steps += [f"volume={gain_db:.2f}dB",
                  f"aresample={AUDIO_SAMPLE_RATE * LIMITER_OVERSAMPLING}",
                  f"alimiter=limit={ceiling:.4f}:attack={LIMITER_ATTACK_MS}:release={LIMITER_RELEASE_MS}:level=false",
                  f"aresample={AUDIO_SAMPLE_RATE}",
                  f"afade=t=in:st=0:d={AUDIO_FADE_IN_SECONDS}",
                  f"afade=t=out:st={max(0.0, duration - AUDIO_FADE_OUT_SECONDS):.4f}:d={AUDIO_FADE_OUT_SECONDS}"]
    steps += [f"apad=whole_dur={duration:.4f}", f"atrim=end={duration:.4f}"]
    return ",".join(steps)


def build_video_filter(duration: float) -> str:
    """Fades, then full-range BT.601 JPEG colour to limited-range BT.709 yuv420p."""
    fade_out_start = max(0.0, duration - VIDEO_FADE_OUT_SECONDS)
    return (f"fade=t=in:st=0:d={VIDEO_FADE_IN_SECONDS},"
            f"fade=t=out:st={fade_out_start:.4f}:d={VIDEO_FADE_OUT_SECONDS},"
            f"scale=out_color_matrix={VIDEO_COLOR_STANDARD}:out_range={VIDEO_COLOR_RANGE},"
            f"format={VIDEO_PIXEL_FORMAT},"
            # The JPEGs carry no primaries/transfer; the encoder takes the frame tags, so set them on the frames.
            f"setparams=colorspace={VIDEO_COLOR_STANDARD}:color_primaries={VIDEO_COLOR_STANDARD}:"
            f"color_trc={VIDEO_COLOR_STANDARD}:range={VIDEO_COLOR_RANGE}")


def sample_filter(every_seconds: float, tiles: int, offset_seconds: float = 0.0) -> str:
    """Pick exactly `tiles` frames, the k-th at offset + k * every (the latest frame at or before that time).

    fps with round=up puts in slot k the last frame whose time is <= k * every; the default rounding would show the
    frame half an interval later than its label. setpts shifts the sampling grid by the offset; t of a picked frame
    is then k * every, so a label must add the offset back.
    """
    shift = f"setpts=PTS-{offset_seconds:.4f}/TB," if offset_seconds else ""
    return f"{shift}fps=fps=1/{every_seconds:g}:round=up,trim=end_frame={tiles}"


def image_sequence_input(pattern: Path, start: int, fps: int) -> list[str]:
    return ["-start_number", str(start), "-framerate", str(fps), "-i", str(pattern)]


def encode_take(pattern: Path, start: int, frame_count: int, fps: int, wav: Path | None, gain_db: float,
                destination: Path) -> None:
    """Encode the cut to destination; the frame count of the result is checked before it is accepted."""
    duration = frame_count / fps
    cmd = [FFMPEG, "-hide_banner", "-nostdin", "-y"] + image_sequence_input(pattern, start, fps)
    if wav is not None:
        cmd += ["-i", str(wav)]
    else:
        cmd += ["-f", "lavfi", "-i", f"anullsrc=channel_layout={AUDIO_CHANNEL_LAYOUT}:sample_rate={AUDIO_SAMPLE_RATE}"]
    cmd += ["-map", "0:v:0", "-map", "1:a:0", "-frames:v", str(frame_count),
            "-vf", build_video_filter(duration), "-af", build_audio_filter(duration, gain_db, wav is not None),
            "-c:v", VIDEO_CODEC, "-profile:v", VIDEO_PROFILE, "-crf", str(VIDEO_CRF), "-preset", VIDEO_PRESET,
            "-pix_fmt", VIDEO_PIXEL_FORMAT, "-g", str(KEYFRAME_INTERVAL_SECONDS * fps),
            "-c:a", AUDIO_CODEC, "-b:a", AUDIO_BITRATE, "-ar", str(AUDIO_SAMPLE_RATE), "-ac", str(AUDIO_CHANNELS),
            "-movflags", "+faststart", str(destination)]
    run_tool(cmd)
    encoded = probe_video_stream(destination).get("nb_frames")
    if str(encoded) != str(frame_count):
        raise GMRToolError(f"encoded {encoded} frames, expected {frame_count}")


def write_contact_sheet(pattern: Path, start: int, frame_count: int, fps: int, aspect: str, destination: Path) -> None:
    """One tile per second of the cut (from the source frames, so no fade), second number drawn on each tile."""
    duration = frame_count / fps
    tile_width = SHEET_TILE_WIDTH.get(aspect, SHEET_TILE_WIDTH[ASPECT_LANDSCAPE])
    columns = SHEET_COLUMNS.get(aspect, SHEET_COLUMNS[ASPECT_LANDSCAPE])
    tiles = max(1, math.ceil(duration * SHEET_TILES_PER_SECOND))
    rows = math.ceil(tiles / columns)
    video_filter = (f"{sample_filter(1.0 / SHEET_TILES_PER_SECOND, tiles)},scale={tile_width}:-2,"
                    f"{label_filter(SHEET_SECOND_LABEL, tile_width)},tile={columns}x{rows}")
    run_tool([FFMPEG, "-hide_banner", "-nostdin", "-y", "-t", f"{duration:.4f}"]
             + image_sequence_input(pattern, start, fps)
             + ["-vf", video_filter, "-frames:v", "1", "-q:v", str(JPEG_QUALITY), str(destination)])


# ---------------------------------------------------------------------------------------------------------------------
# Take
# ---------------------------------------------------------------------------------------------------------------------
def delete_work_dir(work: Path, frames: dict[int, Path], wav: Path | None) -> None:
    """Delete a take's work dir; outside <root>/_work only the big sources (frames, WAV) are removed."""
    if work.parent.name == WORK_FOLDER_NAME and (work / JOB_FILE_NAME).exists():
        shutil.rmtree(work, ignore_errors=True)
        print(f"deleted work dir {work}")
        return
    for path in list(frames.values()) + ([wav] if wav else []):
        path.unlink(missing_ok=True)
    print(f"deleted frames and WAV; kept {work} (not a {WORK_FOLDER_NAME} take folder)")


def print_report(stem: str, frame_count: int, fps: int, before: dict | None, gain_db: float, after: dict | None,
                 manifest: dict, final: Path) -> None:
    def loudness(measure: dict | None) -> str:
        if not measure:
            return "n/a"
        return f"{measure.get('input_i')} LUFS (TP {measure.get('input_tp')} dBTP)"

    print(f"--- {stem}")
    print(f"  duration:   {frame_count / fps:.2f} s ({frame_count} frames @ {fps} fps)")
    print(f"  loudness:   {loudness(before)} -> {loudness(after)}, gain {gain_db:+.2f} dB")
    print(f"  voice lines heard: {manifest.get('VoiceLinesHeard', 0)}")
    warnings = manifest.get("Warnings") or []
    print(f"  manifest warnings: {len(warnings) or 'none'}")
    for warning in warnings:
        print(f"    - {warning}")
    print(f"  final: {final}")


def post_take(work_dir, keep_frames: bool = False) -> Path | None:
    """Post one take; returns the final mp4 path, or None when the take could not be encoded (nothing replaced)."""
    work = Path(work_dir).resolve()
    try:
        return _post_take(work, keep_frames)
    except (GMRToolError, OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: post failed for {work}: {error}")
        return None


def _post_take(work: Path, keep_frames: bool) -> Path | None:
    job = read_json(work / JOB_FILE_NAME)
    manifest = normalise_keys(read_json(work / MANIFEST_FILE_NAME))
    job_name = manifest.get("JobName") or job["JobName"]
    aspect = manifest.get("Aspect") or job.get("Aspect") or ASPECT_LANDSCAPE
    probe = bool(job.get(PROBE_JOB_KEY, work.name.endswith(PROBE_SUFFIX)))
    fps = int(manifest.get("Fps") or job.get("Fps") or 0)
    if fps <= 0:
        raise GMRToolError("manifest has no Fps")
    stem = take_stem(job_name, aspect, probe)

    if not manifest.get("bComplete", False):
        # A take that ended early must never replace a good final (keep only the latest COMPLETE version).
        raise GMRToolError("the manifest is not complete (bComplete false): the render ended early; final kept")
    start = int(manifest.get("StartFrame", -1))
    if start < 0:
        raise GMRToolError(f"manifest StartFrame {start}: the director never started recording")
    frames = find_frames(work, job_name)
    if not frames:
        raise GMRToolError(f"no {SEQUENCE_PREFIX}{job_name}.*.jpeg frames in {work}")
    end = int(manifest.get("EndFrame", -1))
    if end < 0:
        end = max(frames) + 1
        print(f"manifest EndFrame unset: cutting to the last frame present ({end - 1})")
    frame_count = end - start
    if frame_count <= 0:
        raise GMRToolError(f"empty cut [{start}, {end})")
    extension, _ = prepare_frame_range(work, job_name, frames, start, end)
    hide_cut_frames(work, job_name, manifest, start, end, extension)
    pattern = work / f"{SEQUENCE_PREFIX}{job_name}.%0{FRAME_NUMBER_DIGITS}d{extension}"
    duration = frame_count / fps

    wav = work / f"{job_name}{CAPTURE_WAV_SUFFIX}"
    before, gain_db, audio_source = None, 0.0, None
    if not wav.exists():
        print(f"WARNING: no {wav.name}: encoding a silent audio track")
    else:
        mean = mean_volume_db(wav, duration)
        if mean is None or mean < SILENT_MEAN_VOLUME_DB:
            print(f"WARNING: {wav.name} is silent (mean volume {mean} dB): encoding a silent audio track")
        else:
            audio_source = wav
            before = measure_loudness(["-i", str(wav)], f"atrim=end={duration:.4f},")
            measured = finite_number(before.get("input_i")) if before else None
            if measured is not None:
                gain_db = max(-MAX_LOUDNESS_GAIN_DB, min(MAX_LOUDNESS_GAIN_DB, TARGET_LUFS - measured))

    out_dir = output_dir_for(job.get("Category", DEFAULT_CATEGORY), job_name)
    out_dir.mkdir(parents=True, exist_ok=True)
    final = out_dir / f"{stem}.mp4"
    temp = out_dir / f"{stem}.tmp.mp4"
    try:
        encode_take(pattern, start, frame_count, fps, audio_source, gain_db, temp)
        # Only the latest version is kept: the new take replaces the old one once it encoded cleanly.
        os.replace(temp, final)
    finally:
        temp.unlink(missing_ok=True)
    after = measure_loudness(["-i", str(final), "-map", "0:a:0"]) if audio_source else None

    # The mp4 is in place: a failing sidecar is reported but does not fail the take.
    sidecars = (
        (lambda: write_contact_sheet(pattern, start, frame_count, fps, aspect, out_dir / f"{stem}_sheet.jpg")),
        (lambda: shutil.copyfile(work / MANIFEST_FILE_NAME, out_dir / f"{stem}_manifest.json")),
        (lambda: write_log_excerpt(work / RENDER_LOG_NAME, out_dir / f"{stem}.log")),
    )
    for write_sidecar in sidecars:
        try:
            write_sidecar()
        except (GMRToolError, OSError) as error:
            print(f"WARNING: sidecar file failed: {error}")
    print_report(stem, frame_count, fps, before, gain_db, after, manifest, final)

    if not keep_frames:
        delete_work_dir(work, frames, wav if wav.exists() else None)
    return final


def main() -> int:
    sys.stdout.reconfigure(errors="replace")
    parser = argparse.ArgumentParser(description="Post one GameMechanicsRecs take: cut, fade, loudness-normalise, "
                                                 "encode, contact sheet, manifest copy and log excerpt.")
    parser.add_argument("--work", required=True, help="take work dir (Job.json, Manifest.json, frames, WAV)")
    parser.add_argument("--keep-frames", action="store_true", help="keep the work dir (frames + WAV) afterwards")
    args = parser.parse_args()
    return 0 if post_take(args.work, args.keep_frames) else 1


if __name__ == "__main__":
    sys.exit(main())
