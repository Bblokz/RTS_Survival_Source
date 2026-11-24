"""Render one GameMechanicsRecs job headless (16:9 and/or 9:16 take), then post-process each take.

Usage:
    python gmr.py Jobs/<Name>.json [--aspects 16x9,9x16] [--probe] [--development] [--no-post] [--keep-frames]
                  [--seconds N]

Importable:
    run_job(job_path, aspects, probe, development, post, keep_frames, seconds) -> bool

Per take the resolved job (the "Landscape"/"Vertical" override block merged in, resolution and output dir filled) is
written to <GMR root>/_work/<JobName>_<aspect>[_probe]/Job.json and rendered by UnrealEditor-Cmd -game with
-GMRJob=<that file>; the editor-only module RTS_GameMechanicsRecs is loaded by -ExecCmds and drives Movie Render Queue.
Only one render runs at a time: a lock file guards against other gmr.py processes and any other -RenderOffscreen
editor process is waited for. The take is then handed to gmr_post.post_take.

Engine arguments: the fixed render arguments, then the job's python-only "LaunchArgs" list (e.g.
["-SurvivalAutoStart", "-SurvivalStartIndex=0"]), then GMR_EXTRA_ARGS.

Environment:
    GMR_OUTPUT_ROOT   replaces Saved/GameMechanicsRecs (work dirs, outputs, lock, queue files)
    GMR_EXTRA_ARGS    extra engine arguments for the render, e.g. "-LogCmds=LogGMRDirector Verbose"
"""
from __future__ import annotations

import argparse
import copy
import ctypes
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

# gmr_queue and python -I start this script without its folder on sys.path.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gmr_post import (  # noqa: E402
    ASPECT_LANDSCAPE, ASPECT_VERTICAL, DEFAULT_CATEGORY, JOB_FILE_NAME, MANIFEST_FILE_NAME, PROBE_JOB_KEY,
    PROJECT_ROOT, RENDER_LOG_NAME, TOOLS_DIR, format_seconds, gmr_root, post_take, read_json, take_stem,
    work_root, write_json,
)

try:
    import psutil  # optional: faster process queries than PowerShell
except ImportError:
    psutil = None

# ---------------------------------------------------------------------------------------------------------------------
# Editor launch
# ---------------------------------------------------------------------------------------------------------------------
EDITOR_BIN_DIR = Path("D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64")
EDITOR_EXE_DEBUGGAME = "UnrealEditor-Win64-DebugGame-Cmd.exe"
EDITOR_EXE_DEVELOPMENT = "UnrealEditor-Cmd.exe"
UPROJECT = PROJECT_ROOT / "RTS_Survival.uproject"
# The process boots into the empty Entry map; the runner then loads the job's map through Movie Render Queue.
STARTUP_MAP_URL = "/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"
GMR_MODULE_NAME = "RTS_GameMechanicsRecs"
EXEC_COMMANDS = (
    f"Module Load {GMR_MODULE_NAME}",
    # Streamed waves must load their chunks synchronously: the non-realtime renderer never waits for async loads.
    "au.streamcaching.ForceBlockForLoad 1",
    "au.ForceSyncAudioDecodes 1",
)
WINDOW_RESOLUTION = (1280, 720)
DISABLED_PLUGINS = "Fab"
EXTRA_ARGS_ENV = "GMR_EXTRA_ARGS"
# One argument: runs of non-space, non-quote characters and "quoted spans" (which may contain spaces).
EXTRA_ARG_TOKEN_RE = re.compile(r'(?:[^\s"]+|"[^"]*")+')

# ---------------------------------------------------------------------------------------------------------------------
# Job resolution
# ---------------------------------------------------------------------------------------------------------------------
# Insertion order is the render order: 16x9 first.
ASPECT_RESOLUTIONS = {ASPECT_LANDSCAPE: (1920, 1080), ASPECT_VERTICAL: (1080, 1920)}
# Python-only override block merged over the job for that aspect's take only.
ASPECT_OVERRIDE_KEYS = {ASPECT_LANDSCAPE: "Landscape", ASPECT_VERTICAL: "Vertical"}
PROBE_RESOLUTION_DIVISOR = 2
JOB_DEFAULTS = {"Fps": 30, "WarmupSeconds": 6.0, "Category": DEFAULT_CATEGORY}
# Python-only: extra engine arguments of this job (e.g. ["-SurvivalAutoStart"]), placed before GMR_EXTRA_ARGS.
LAUNCH_ARGS_KEY = "LaunchArgs"
JOB_NAME_RE = re.compile(r"^[A-Za-z0-9_\-]+$")

# ---------------------------------------------------------------------------------------------------------------------
# Render monitoring
# ---------------------------------------------------------------------------------------------------------------------
LOG_POLL_SECONDS = 5
# Only the log tail is read: reading a multi-MB log every 5 s raised MemoryError in the Shorts runner.
LOG_TAIL_BYTES = 200_000
DONE_RE = re.compile(r"GMR_JOB_DONE job=(\S+) success=(true|false)")
FAILURE_MARKERS = ("=== Critical error:", "Fatal error")
# A render that ran out of video memory hung with a silent log; kill it after this long without log growth.
STALL_KILL_SECONDS = 900
HARD_TIMEOUT_SECONDS = 3 * 60 * 60
PROGRESS_PRINT_SECONDS = 60
# The runner exits itself right after GMR_JOB_DONE; give it this long before killing it.
EXIT_GRACE_SECONDS = 60
# Another session rebuilding the binaries during startup leaves a modal "Missing target file" window that blocks
# the render forever; once the log has been idle this long, check the window title (and close it) at this interval.
MISSING_TARGET_TITLE = "Missing target file"
MODAL_CHECK_IDLE_SECONDS = 30

# ---------------------------------------------------------------------------------------------------------------------
# One render at a time
# ---------------------------------------------------------------------------------------------------------------------
LOCK_FILE_NAME = "_gmr.lock"
LOCK_POLL_SECONDS = 30
# A lock file without a PID is being written by its owner; treat it as stale only after this many seconds.
LOCK_EMPTY_GRACE_SECONDS = 10
OTHER_RENDER_POLL_SECONDS = 30
WAIT_REPORT_SECONDS = 60
# A failing process query is retried; after this many failures in a row the render goes ahead (the lock still holds).
MAX_PROCESS_QUERY_FAILURES = 3
RENDER_EXE_PREFIX = "unrealeditor"
RENDER_EXE_SUFFIX = "-cmd.exe"
RENDER_FLAG = "-renderoffscreen"
POWERSHELL = "powershell"
POWERSHELL_TIMEOUT_SECONDS = 60
PS_UTF8_PREFIX = "[Console]::OutputEncoding = [Text.Encoding]::UTF8; "
PS_RENDER_QUERY = ("Get-CimInstance Win32_Process -Filter \"Name LIKE 'UnrealEditor%'\" | "
                   "Select-Object ProcessId,Name,CommandLine | ConvertTo-Json -Compress")
PS_CLOSE_MODAL = ("$p = Get-Process -Id {pid} -ErrorAction SilentlyContinue; "
                  "if ($p -and $p.MainWindowTitle -like '*{title}*') {{ [void]$p.CloseMainWindow(); $p.MainWindowTitle }}")

# Win32 process query (PID liveness without psutil).
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
STILL_ACTIVE = 259
ERROR_ACCESS_DENIED = 5


# ---------------------------------------------------------------------------------------------------------------------
# Processes
# ---------------------------------------------------------------------------------------------------------------------
def run_powershell(script: str) -> subprocess.CompletedProcess | None:
    """Run a PowerShell snippet; None when PowerShell itself could not run or timed out."""
    try:
        return subprocess.run([POWERSHELL, "-NoProfile", "-NonInteractive", "-Command", PS_UTF8_PREFIX + script],
                              capture_output=True, text=True, encoding="utf-8", errors="replace",
                              stdin=subprocess.DEVNULL, timeout=POWERSHELL_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"WARNING: PowerShell query failed: {error}")
        return None


def pid_alive(pid: int) -> bool:
    if psutil is not None:
        return psutil.pid_exists(pid)
    if os.name != "nt":
        try:
            os.kill(pid, 0)  # POSIX only: signal 0 probes. On Windows os.kill would terminate the process.
        except OSError:
            return False
        return True
    from ctypes import wintypes
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        # The process exists but belongs to someone we may not query.
        return ctypes.get_last_error() == ERROR_ACCESS_DENIED
    try:
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            return True
        return exit_code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


def is_render_process(name: str, command_line: str) -> bool:
    name = (name or "").lower()
    return (name.startswith(RENDER_EXE_PREFIX) and name.endswith(RENDER_EXE_SUFFIX)
            and RENDER_FLAG in (command_line or "").lower())


def find_other_renders() -> list[tuple[int, str]] | None:
    """(pid, exe name) of every running -RenderOffscreen UnrealEditor*-Cmd.exe; None when the query failed."""
    if psutil is not None:
        renders = []
        for process in psutil.process_iter(["pid", "name", "cmdline"]):
            try:
                command_line = " ".join(process.info.get("cmdline") or [])
            except (psutil.Error, TypeError):
                continue
            if is_render_process(process.info.get("name"), command_line):
                renders.append((process.info["pid"], process.info["name"]))
        return renders
    result = run_powershell(PS_RENDER_QUERY)
    if result is None or result.returncode != 0:
        if result is not None:
            print(f"WARNING: process query failed: {result.stderr.strip()[-500:]}")
        return None
    text = result.stdout.strip()
    if not text:
        return []
    try:
        entries = json.loads(text)
    except json.JSONDecodeError:
        print("WARNING: process query returned unreadable JSON")
        return None
    if isinstance(entries, dict):  # ConvertTo-Json writes a bare object for a single match
        entries = [entries]
    return [(int(entry["ProcessId"]), entry.get("Name") or "") for entry in entries
            if is_render_process(entry.get("Name"), entry.get("CommandLine"))]


def wait_for_other_renders() -> None:
    """Block while any other -RenderOffscreen editor runs (another session's render or a Shorts render)."""
    started = time.monotonic()
    last_report = None
    failures = 0
    while True:
        renders = find_other_renders()
        if renders is None:
            failures += 1
            if failures >= MAX_PROCESS_QUERY_FAILURES:
                print("WARNING: cannot list processes; rendering without the other-render check")
                return
            time.sleep(OTHER_RENDER_POLL_SECONDS)
            continue
        failures = 0
        if not renders:
            if last_report is not None:
                print(f"other render(s) finished after {format_seconds(time.monotonic() - started)}")
            return
        now = time.monotonic()
        if last_report is None or now - last_report >= WAIT_REPORT_SECONDS:
            listing = ", ".join(f"{name} pid {pid}" for pid, name in renders)
            print(f"waiting for other render(s): {listing} ({format_seconds(now - started)})")
            last_report = now
        time.sleep(OTHER_RENDER_POLL_SECONDS)


def close_missing_target_modal(pid: int) -> bool:
    """Close the 'Missing target file' modal of our render process; True when one was closed."""
    result = run_powershell(PS_CLOSE_MODAL.format(pid=pid, title=MISSING_TARGET_TITLE))
    return bool(result and result.returncode == 0 and MISSING_TARGET_TITLE in result.stdout)


class RenderLock:
    """Lock file holding the owner's PID so two gmr.py never render at once; stale when that PID is gone."""

    def __init__(self, path: Path):
        self.path = path
        self.owned = False

    def __enter__(self) -> RenderLock:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        started = time.monotonic()
        last_report = None
        while True:
            try:
                with open(self.path, "x", encoding="utf-8") as stream:
                    stream.write(str(os.getpid()))
                self.owned = True
                return self
            except FileExistsError:
                pass
            owner = self._read_owner()
            if owner == os.getpid():
                return self  # re-entrant: an outer caller of this process holds the lock
            if self._is_stale(owner):
                # Another waiter may have replaced the stale lock meanwhile; only remove the one we judged.
                if self._read_owner() == owner:
                    print(f"removing stale lock {self.path} (pid {owner})")
                    self.path.unlink(missing_ok=True)
                continue
            now = time.monotonic()
            if last_report is None or now - last_report >= WAIT_REPORT_SECONDS:
                print(f"waiting for gmr pid {owner} to finish its render ({format_seconds(now - started)})")
                last_report = now
            time.sleep(LOCK_POLL_SECONDS)

    def __exit__(self, *exc_info) -> None:
        if self.owned and self._read_owner() == os.getpid():
            self.path.unlink(missing_ok=True)
        self.owned = False

    def _read_owner(self) -> int | None:
        try:
            return int(self.path.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            return None

    def _is_stale(self, owner: int | None) -> bool:
        if owner is not None:
            return not pid_alive(owner)
        try:
            age = time.time() - self.path.stat().st_mtime
        except OSError:
            return False  # vanished meanwhile; the next attempt creates it
        return age > LOCK_EMPTY_GRACE_SECONDS


# ---------------------------------------------------------------------------------------------------------------------
# Jobs
# ---------------------------------------------------------------------------------------------------------------------
def deep_merge(target: dict, patch: dict) -> dict:
    """Merge patch into target: dicts merge key by key, everything else (lists included) is replaced by a copy."""
    for key, value in patch.items():
        if isinstance(value, dict) and isinstance(target.get(key), dict):
            deep_merge(target[key], value)
        else:
            target[key] = copy.deepcopy(value)
    return target


def parse_aspects(value) -> list[str]:
    """'9x16,16x9' or a list -> known aspects in render order (16x9 first), duplicates removed."""
    requested = value.split(",") if isinstance(value, str) else list(value)
    requested = [aspect.strip() for aspect in requested if aspect and aspect.strip()]
    unknown = [aspect for aspect in requested if aspect not in ASPECT_RESOLUTIONS]
    if unknown or not requested:
        raise ValueError(f"unknown aspect(s) {unknown or value!r}; use {','.join(ASPECT_RESOLUTIONS)}")
    return [aspect for aspect in ASPECT_RESOLUTIONS if aspect in requested]


def resolve_job_path(job_path) -> Path:
    """A job path as given, or relative to this tools folder (Jobs/<Name>.json works from any cwd)."""
    path = Path(job_path)
    if not path.exists() and not path.is_absolute() and (TOOLS_DIR / path).exists():
        path = TOOLS_DIR / path
    return path.resolve()


def load_job(job_path: Path) -> dict:
    job = read_json(job_path)
    if not isinstance(job, dict):
        raise ValueError(f"{job_path}: a job file holds one JSON object")
    job.setdefault("JobName", job_path.stem)
    return job


def validate_job(job: dict) -> list[str]:
    problems = []
    if not JOB_NAME_RE.match(str(job.get("JobName", ""))):
        problems.append(f"JobName {job.get('JobName')!r} must match {JOB_NAME_RE.pattern}")
    if not job.get("MapPath"):
        problems.append("MapPath is empty")
    if not isinstance(job.get("Shots"), list) or not job["Shots"]:
        problems.append("Shots is empty (the runner refuses a job without shots)")
    for key in ASPECT_OVERRIDE_KEYS.values():
        if key in job and not isinstance(job[key], dict):
            problems.append(f"{key} must be an object")
    for block in [job] + [job[key] for key in ASPECT_OVERRIDE_KEYS.values() if isinstance(job.get(key), dict)]:
        launch_args = block.get(LAUNCH_ARGS_KEY)
        if launch_args is not None and (not isinstance(launch_args, list)
                                        or not all(isinstance(arg, str) for arg in launch_args)):
            problems.append(f"{LAUNCH_ARGS_KEY} must be a list of strings")
    return problems


def resolve_job(job: dict, aspect: str, probe: bool, seconds: float | None) -> dict:
    """The job as the C++ runner sees it for one take (without OutputDir, which the caller sets)."""
    resolved = copy.deepcopy(job)
    overrides = {key: resolved.pop(key, None) for key in ASPECT_OVERRIDE_KEYS.values()}
    deep_merge(resolved, overrides[ASPECT_OVERRIDE_KEYS[aspect]] or {})
    for key, value in JOB_DEFAULTS.items():
        resolved.setdefault(key, value)
    if seconds is not None:
        resolved["DurationSeconds"] = float(seconds)
    width, height = ASPECT_RESOLUTIONS[aspect]
    if probe:
        width, height = width // PROBE_RESOLUTION_DIVISOR, height // PROBE_RESOLUTION_DIVISOR
    resolved.update({"Aspect": aspect, "ResolutionX": width, "ResolutionY": height, PROBE_JOB_KEY: probe})
    return resolved


# ---------------------------------------------------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------------------------------------------------
def editor_exe(development: bool) -> Path:
    return EDITOR_BIN_DIR / (EDITOR_EXE_DEVELOPMENT if development else EDITOR_EXE_DEBUGGAME)


def build_command(job_file: Path, log_file: Path, development: bool, launch_args: list[str] = ()) -> list[str]:
    """Fixed render arguments, then the job's LaunchArgs, then GMR_EXTRA_ARGS."""
    args = [str(editor_exe(development)), UPROJECT.as_posix(), STARTUP_MAP_URL,
            "-game", "-unattended", "-RenderOffscreen", "-windowed",
            f"-ResX={WINDOW_RESOLUTION[0]}", f"-ResY={WINDOW_RESOLUTION[1]}",
            "-NoLoadingScreen", "-FixedSeed", "-deterministicaudio", "-audiomixer",
            "-ExecCmds=" + ", ".join(EXEC_COMMANDS), f"-DisablePlugins={DISABLED_PLUGINS}",
            "-log", f"-ABSLOG={log_file.as_posix()}", f"-GMRJob={job_file.as_posix()}",
            # Map content warnings repeated every frame (Nanite material checks) grew one log to 270 MB.
            "-LogCmds=LogStaticMesh Error"]
    args += list(launch_args)
    args += split_extra_args(os.environ.get(EXTRA_ARGS_ENV, ""))
    return args


def split_extra_args(text: str) -> list[str]:
    """Split GMR_EXTRA_ARGS like a Windows command line: whitespace separates, double quotes group and are removed,
    backslashes stay literal (paths).

    shlex.split(posix=False) cuts -LogCmds="LogGMR Verbose" in two and keeps the quotes, which subprocess then
    escapes; one unquoted list element per argument is what Popen and UE's argv rebuild expect.
    """
    return [token.replace('"', "") for token in EXTRA_ARG_TOKEN_RE.findall(text)]


def read_done(log_file: Path) -> bool | None:
    """True/False once the log tail shows GMR_JOB_DONE (or a crash); None while the render is still going."""
    try:
        with open(log_file, "rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - LOG_TAIL_BYTES))
            tail = stream.read().decode("utf-8", errors="ignore")
    except (OSError, MemoryError):
        return None
    match = DONE_RE.search(tail)
    if match:
        return match.group(2) == "true"
    if any(marker in tail for marker in FAILURE_MARKERS):
        return False
    return None


def log_size(log_file: Path) -> int:
    try:
        return log_file.stat().st_size
    except OSError:
        return 0


def stop_process(process: subprocess.Popen) -> None:
    if process.poll() is None:
        process.kill()
        try:
            process.wait(timeout=EXIT_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            print(f"WARNING: render pid {process.pid} did not exit after kill")


def wait_for_render(process: subprocess.Popen, log_file: Path) -> bool:
    """Poll the log until GMR_JOB_DONE, a crash marker, a stall or the hard timeout; returns the job's success."""
    started = time.monotonic()
    last_size, last_growth = -1, started
    last_progress = last_modal_check = started
    result = None
    while process.poll() is None:
        time.sleep(LOG_POLL_SECONDS)
        result = read_done(log_file)
        if result is not None:
            break
        now = time.monotonic()
        size = log_size(log_file)
        if size != last_size:
            last_size, last_growth = size, now
        idle = now - last_growth
        if idle >= MODAL_CHECK_IDLE_SECONDS and now - last_modal_check >= MODAL_CHECK_IDLE_SECONDS:
            last_modal_check = now
            if close_missing_target_modal(process.pid):
                print(f"closed a '{MISSING_TARGET_TITLE}' modal (another session rebuilt the binaries); waiting on")
                last_growth = now
                continue
        if idle > STALL_KILL_SECONDS:
            print(f"render log silent for {format_seconds(idle)}: killing the hung render")
            stop_process(process)
            result = False
            break
        if now - started > HARD_TIMEOUT_SECONDS:
            print(f"render exceeded {format_seconds(HARD_TIMEOUT_SECONDS)}: killing it")
            stop_process(process)
            result = False
            break
        if now - last_progress >= PROGRESS_PRINT_SECONDS:
            print(f"  rendering... {format_seconds(now - started)} elapsed, log {size / 1e6:.1f} MB")
            last_progress = now
    if process.poll() is None:
        try:
            process.wait(timeout=EXIT_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            print("render did not exit after GMR_JOB_DONE: killing it")
            stop_process(process)
    if result is None:
        result = read_done(log_file)
    success = bool(result)
    print(f"render finished in {format_seconds(time.monotonic() - started)}, exit={process.returncode}, "
          f"success={success}{'' if result is not None else ' (no GMR_JOB_DONE in the log)'}")
    return success


def prepare_work_dir(stem: str) -> Path:
    work = work_root() / stem
    if work.exists():
        shutil.rmtree(work)  # old frames would mix into the new take
    work.mkdir(parents=True)
    return work


def render_take(job: dict, stem: str, development: bool) -> tuple[Path | None, bool]:
    """Render one resolved take; returns (work dir, success). The work dir is reset only once the lock is held, so
    a second gmr.py never deletes the frames of a take that is still rendering."""
    exe = editor_exe(development)
    if not exe.exists():
        print(f"ERROR: editor executable not found: {exe}")
        return None, False
    with RenderLock(gmr_root() / LOCK_FILE_NAME):
        wait_for_other_renders()
        work = prepare_work_dir(stem)
        job_file = work / JOB_FILE_NAME
        log_file = work / RENDER_LOG_NAME
        write_json(job_file, dict(job, OutputDir=work.as_posix()), ascii_only=True)
        command = build_command(job_file, log_file, development, job.get(LAUNCH_ARGS_KEY) or [])
        print("launching: " + subprocess.list2cmdline(command[1:]))
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        try:
            return work, wait_for_render(process, log_file)
        finally:
            # Ctrl+C or an error in the wait loop must not leave an orphaned render holding the GPU.
            stop_process(process)


def run_take(job: dict, aspect: str, probe: bool, development: bool, post: bool, keep_frames: bool,
             seconds: float | None) -> bool:
    resolved = resolve_job(job, aspect, probe, seconds)
    # A Landscape/Vertical block may rename the job; the name becomes a folder that is deleted and recreated.
    if not JOB_NAME_RE.match(str(resolved.get("JobName", ""))):
        print(f"ERROR: {aspect}: JobName {resolved.get('JobName')!r} must match {JOB_NAME_RE.pattern}")
        return False
    stem = take_stem(resolved["JobName"], aspect, probe)
    print(f"=== {stem}: {resolved['ResolutionX']}x{resolved['ResolutionY']} @ {resolved['Fps']} fps, "
          f"{resolved.get('DurationSeconds', '?')} s + {resolved['WarmupSeconds']} s warm-up")
    work, rendered = render_take(resolved, stem, development)
    if work is None:
        return False
    manifest = work / MANIFEST_FILE_NAME
    if not manifest.exists():
        print(f"ERROR: {stem}: no {MANIFEST_FILE_NAME}; see {work / RENDER_LOG_NAME}")
        return False
    if not post:
        print(f"{stem}: post skipped (--no-post); take left in {work}")
        return rendered
    if not rendered:
        print(f"WARNING: {stem}: the render reported failure; posting what it wrote")
    final = post_take(work, keep_frames)
    return final is not None


def run_job(job_path, aspects=tuple(ASPECT_RESOLUTIONS), probe: bool = False, development: bool = False,
            post: bool = True, keep_frames: bool = False, seconds: float | None = None) -> bool:
    """Render (and post) every requested aspect of one job; True only when each aspect produced its final mp4
    (with post=False: when each render succeeded and wrote its manifest)."""
    path = resolve_job_path(job_path)
    try:
        job = load_job(path)
    except (OSError, ValueError) as error:  # JSONDecodeError is a ValueError
        print(f"ERROR: cannot read job {path}: {error}")
        return False
    problems = validate_job(job)
    if problems:
        print(f"ERROR: {path}:\n  " + "\n  ".join(problems))
        return False
    results = {}
    for aspect in parse_aspects(aspects):
        started = time.monotonic()
        results[aspect] = run_take(job, aspect, probe, development, post, keep_frames, seconds)
        print(f"{job['JobName']} {aspect}: {'OK' if results[aspect] else 'FAILED'} "
              f"in {format_seconds(time.monotonic() - started)}")
    return all(results.values())


def main() -> int:
    sys.stdout.reconfigure(errors="replace")
    parser = argparse.ArgumentParser(description="Render one GameMechanicsRecs job headless and post-process it.")
    parser.add_argument("job", help="job JSON, e.g. Jobs/MyJob.json (relative to the cwd or this tools folder)")
    parser.add_argument("--aspects", default=",".join(ASPECT_RESOLUTIONS),
                        help="comma list of 16x9 and/or 9x16 (default both; 16x9 always renders first)")
    parser.add_argument("--probe", action="store_true",
                        help="half resolution, written as <JobName>_<aspect>_probe (never overwrites a final)")
    parser.add_argument("--development", action="store_true",
                        help=f"use {EDITOR_EXE_DEVELOPMENT} instead of {EDITOR_EXE_DEBUGGAME}")
    parser.add_argument("--no-post", action="store_true", help="render only; leave the take in the work dir")
    parser.add_argument("--keep-frames", action="store_true", help="keep the work dir (frames + WAV) after post")
    parser.add_argument("--seconds", type=float, default=None, help="override DurationSeconds (quick tests)")
    args = parser.parse_args()
    try:
        aspects = parse_aspects(args.aspects)
    except ValueError as error:
        parser.error(str(error))
    ok = run_job(args.job, aspects, args.probe, args.development, not args.no_post, args.keep_frames, args.seconds)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
