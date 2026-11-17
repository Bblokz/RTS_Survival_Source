"""Run many GameMechanicsRecs jobs one after another in this process.

Usage:
    python gmr_queue.py Jobs/A.json Jobs/B.json ... [--aspects 16x9,9x16] [--probe] [--development]
    python gmr_queue.py --list jobs.txt [...same options]

--list reads one job path per line (blank lines and lines starting with # are skipped). Between jobs the queue
honours two control files in the GMR root (Saved/GameMechanicsRecs, or GMR_OUTPUT_ROOT):
    _queue.hold   while it exists, the next job waits
    _queue.stop   the queue stops before the next job and deletes this file
_queue_status.json (current job, finished jobs with result and seconds, remaining jobs) is rewritten before and after
every job.
"""
from __future__ import annotations

import argparse
import sys
import time
import traceback
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gmr  # noqa: E402
from gmr_post import format_seconds, gmr_root, write_json  # noqa: E402

HOLD_FILE_NAME = "_queue.hold"
STOP_FILE_NAME = "_queue.stop"
STATUS_FILE_NAME = "_queue_status.json"
HOLD_POLL_SECONDS = 30
HOLD_REPORT_SECONDS = 60
LIST_COMMENT_PREFIX = "#"
RESULT_OK = "ok"
RESULT_FAILED = "failed"
RESULT_ERROR = "error"


def now_text() -> str:
    return datetime.now().isoformat(timespec="seconds")


def read_job_list(list_file: Path) -> list[str]:
    """Job paths from a text file; relative paths resolve against the list file's folder first."""
    jobs = []
    for line in list_file.read_text(encoding="utf-8-sig").splitlines():
        entry = line.strip()
        if not entry or entry.startswith(LIST_COMMENT_PREFIX):
            continue
        beside_list = list_file.parent / entry
        jobs.append(str(beside_list) if not Path(entry).is_absolute() and beside_list.exists() else entry)
    return jobs


class Queue:
    """Job list, control files and the status file of one queue run."""

    def __init__(self, jobs: list[Path]):
        root = gmr_root()
        root.mkdir(parents=True, exist_ok=True)
        self.hold_file = root / HOLD_FILE_NAME
        self.stop_file = root / STOP_FILE_NAME
        self.status_file = root / STATUS_FILE_NAME
        self.status = {"Started": now_text(), "Updated": now_text(), "State": "running", "Current": None,
                       "Done": [], "Remaining": [str(job) for job in jobs]}

    def write_status(self) -> None:
        self.status["Updated"] = now_text()
        write_json(self.status_file, self.status)

    def stop_requested(self) -> bool:
        if not self.stop_file.exists():
            return False
        print(f"{self.stop_file.name} found: stopping the queue")
        self.stop_file.unlink(missing_ok=True)
        return True

    def wait_while_held(self) -> bool:
        """Wait while the hold file exists; False when a stop was requested meanwhile."""
        started = time.monotonic()
        last_report = None
        while self.hold_file.exists():
            if self.stop_requested():
                return False
            now = time.monotonic()
            if last_report is None or now - last_report >= HOLD_REPORT_SECONDS:
                print(f"queue held by {self.hold_file} ({format_seconds(now - started)})")
                last_report = now
            self.status["State"] = "held"
            self.write_status()
            time.sleep(HOLD_POLL_SECONDS)
        self.status["State"] = "running"
        return True

    def run(self, run_options: dict) -> bool:
        all_ok = True
        try:
            while self.status["Remaining"]:
                if self.stop_requested() or not self.wait_while_held():
                    self.status["State"] = "stopped"
                    return False
                job = self.status["Remaining"][0]
                self.status["Current"] = job
                self.write_status()
                print(f"##### queue: {job} ({len(self.status['Done']) + 1}/"
                      f"{len(self.status['Done']) + len(self.status['Remaining'])})")
                started = time.monotonic()
                entry = {"Job": job}
                try:
                    entry["Result"] = RESULT_OK if gmr.run_job(job, **run_options) else RESULT_FAILED
                except Exception as error:  # one broken job must not end the queue
                    traceback.print_exc()
                    entry["Result"] = RESULT_ERROR
                    entry["Error"] = f"{type(error).__name__}: {error}"
                entry["Seconds"] = round(time.monotonic() - started)
                all_ok = all_ok and entry["Result"] == RESULT_OK
                self.status["Done"].append(entry)
                self.status["Remaining"].pop(0)
                self.status["Current"] = None
                self.write_status()
            self.status["State"] = "finished"
            return all_ok
        except KeyboardInterrupt:
            self.status["State"] = "interrupted"
            raise
        finally:
            self.status["Current"] = None
            self.write_status()


def main() -> int:
    sys.stdout.reconfigure(errors="replace")
    parser = argparse.ArgumentParser(description="Run several GameMechanicsRecs jobs one after another.")
    parser.add_argument("jobs", nargs="*", help="job JSON files (relative to the cwd or the tools folder)")
    parser.add_argument("--list", type=Path, help="text file with one job path per line")
    parser.add_argument("--aspects", default=",".join(gmr.ASPECT_RESOLUTIONS), help="comma list of 16x9 and/or 9x16")
    parser.add_argument("--probe", action="store_true", help="half-resolution _probe takes")
    parser.add_argument("--development", action="store_true", help="use the Development editor executable")
    parser.add_argument("--no-post", action="store_true", help="render only")
    parser.add_argument("--keep-frames", action="store_true", help="keep each take's work dir after post")
    parser.add_argument("--seconds", type=float, default=None, help="override DurationSeconds of every job")
    args = parser.parse_args()

    entries = list(args.jobs)
    if args.list:
        entries += read_job_list(args.list)
    if not entries:
        parser.error("give job files and/or --list")
    jobs = [gmr.resolve_job_path(entry) for entry in entries]
    missing = [str(job) for job in jobs if not job.exists()]
    if missing:
        parser.error("job file(s) not found: " + ", ".join(missing))
    try:
        aspects = gmr.parse_aspects(args.aspects)
    except ValueError as error:
        parser.error(str(error))

    options = {"aspects": aspects, "probe": args.probe, "development": args.development, "post": not args.no_post,
               "keep_frames": args.keep_frames, "seconds": args.seconds}
    queue = Queue(jobs)
    ok = queue.run(options)
    for entry in queue.status["Done"]:
        print(f"  {entry['Result']:7s} {format_seconds(entry['Seconds']):>8s}  {entry['Job']}")
    print(f"queue {queue.status['State']}: status in {queue.status_file}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
