#!/usr/bin/env python3
"""Wait for a bounded DS startup, then require three live animated captures.

Run inside xvfb-run with DeSmuME, xwininfo and ImageMagick installed. A valid
emulator window can precede the first game frame: preserve those startup
captures and wait up to 30 seconds for both screens to become ready. After
that point every capture must pass the unchanged boot/animation thresholds.
This checks a release ROM's boot/title, not gameplay or hardware frame rate.
"""

import argparse
import json
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

from validate_ds_smoke import UnreadyCapture, inspect_capture, validate

STARTUP_TIMEOUT = 30
STARTUP_POLL = 1
CAPTURE_DELAYS = (2, 3, 5)
WINDOW_NAME = re.compile(r'^\s*(0x[0-9a-fA-F]+)\s+"[Dd]e[Ss]mu[Mm][Ee][^"]*"', re.M)


def run_sequence(capture, ensure_alive, report, *, clock=time.monotonic,
                 sleep=time.sleep, startup_timeout=STARTUP_TIMEOUT):
    """Only pre-ready blank/diagnostic frames or missing windows may retry."""
    started = clock()
    deadline = started + startup_timeout
    report.update(stage="startup", startup_probes=[], captures=[])
    attempt = 0
    while True:
        ensure_alive()
        if clock() >= deadline:
            raise RuntimeError(f"both DS screens did not become ready within {startup_timeout}s")
        attempt += 1
        path = capture(f"startup-{attempt:02d}", allow_missing=True)
        ensure_alive()
        if clock() >= deadline:
            raise RuntimeError(f"both DS screens did not become ready within {startup_timeout}s")
        probe = {"capture": str(path) if path else None,
                 "elapsed_seconds": round(clock() - started, 3), "ready": False}
        report["startup_probes"].append(probe)
        if path is not None:
            try:
                _, screens = inspect_capture(path)
            except UnreadyCapture as error:
                probe["reason"] = str(error)
            else:
                probe.update(ready=True, screens=screens)
                report["ready_after_seconds"] = probe["elapsed_seconds"]
                break
        else:
            probe["reason"] = "emulator window has not appeared"
        sleep(min(STARTUP_POLL, max(0, deadline - clock())))

    paths = []
    for index, delay in enumerate(CAPTURE_DELAYS, 1):
        report["stage"] = f"title-{index:02d}"
        sleep(delay)
        ensure_alive()
        path = capture(report["stage"], allow_missing=False)
        ensure_alive()
        if path is None:
            raise RuntimeError("emulator window disappeared after startup")
        # Do not treat a later blank, corrupt or missing frame as another
        # startup attempt. It is a failed running ROM/capture immediately.
        _, screens = inspect_capture(path)
        paths.append(path)
        report["captures"].append({"capture": str(path), "screens": screens})
    report["captures"] = validate(paths)
    ensure_alive()
    report["stage"] = "complete"
    return report["captures"]


class EmulatorCapture:
    def __init__(self, process, out_dir):
        self.process = process
        self.out_dir = out_dir

    def ensure_alive(self):
        code = self.process.poll()
        if code is not None:
            raise RuntimeError(f"emulator exited with code {code}")

    def __call__(self, label, *, allow_missing):
        self.ensure_alive()
        windows = subprocess.run(["xwininfo", "-root", "-tree"],
                                 capture_output=True, text=True, timeout=10)
        (self.out_dir / f"windows-{label}.txt").write_text(windows.stdout + windows.stderr)
        windows.check_returncode()
        match = WINDOW_NAME.search(windows.stdout)
        if not match:
            if allow_missing:
                return None
            raise RuntimeError("no emulator client window after startup")
        png = self.out_dir / f"{label}.png"
        ppm = self.out_dir / f"{label}.ppm"
        subprocess.run(["import", "-window", match[1], str(png)], check=True, timeout=10)
        subprocess.run(["convert", str(png), "-depth", "8", str(ppm)], check=True, timeout=10)
        self.ensure_alive()
        return ppm


def interrupted(signum, _frame):
    raise RuntimeError(f"smoke interrupted by signal {signum}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report = {"status": "running", "stage": "launch", "rom": str(args.rom),
              "startup_deadline_seconds": STARTUP_TIMEOUT,
              "post_ready_capture_delays_seconds": CAPTURE_DELAYS}
    process = None
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, interrupted)
    try:
        if not args.rom.is_file():
            raise FileNotFoundError(f"missing release ROM: {args.rom}")
        with (args.out_dir / "desmume.log").open("w") as log:
            process = subprocess.Popen(["desmume-cli", "--disable-sound", str(args.rom)],
                                       stdout=log, stderr=subprocess.STDOUT)
            capture = EmulatorCapture(process, args.out_dir)
            run_sequence(capture, capture.ensure_alive, report)
        report["status"] = "pass"
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        report.update(status="fail", error=str(error))
        print(f"DS_BOOT_SMOKE_FAIL: {error}", file=sys.stderr, flush=True)
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        (args.out_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if report["status"] == "pass":
        print("DS_BOOT_SMOKE_PASS captures=3 screens=2 title_animation=1 "
              f"startup_probes={len(report['startup_probes'])} "
              f"ready_after_seconds={report['ready_after_seconds']}", flush=True)
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
