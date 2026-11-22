#!/usr/bin/env python3
"""Exercise real false-positive boot captures without an emulator or PIL."""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

VALIDATOR = Path(__file__).resolve().parents[1] / "tools/validate_ds_smoke.py"
spec = importlib.util.spec_from_file_location("ds_smoke", VALIDATOR)
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)
sys.modules["validate_ds_smoke"] = smoke
RUNNER = VALIDATOR.with_name("run_ds_smoke.py")
spec = importlib.util.spec_from_file_location("ds_smoke_runner", RUNNER)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class BootCaptureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.frame = bytes((24 + i % 192 for i in range(256 * 384 * 3)))
        self.next_frame = bytes((32 + i % 192 for i in range(256 * 192 * 3))) + self.frame[256 * 192 * 3:]

    def capture(self, name, pixels=None, header=b"P6\n256 384\n255\n"):
        path = self.directory / name
        path.write_bytes(header + (self.frame if pixels is None else pixels))
        return path

    def test_one_screen_animation_is_enough(self):
        a = self.capture("title1.ppm")
        b = self.capture("title2.ppm", self.next_frame)
        self.assertEqual(smoke.validate([a, b])[1]["changed_pixels"], 256 * 192)

    def test_frozen_game_is_rejected_even_with_many_colors(self):
        a = self.capture("title1.ppm")
        b = self.capture("title2.ppm")
        with self.assertRaisesRegex(ValueError, "frozen"):
            smoke.validate([a, b])

    def test_black_and_boot_diagnostic_roms_are_rejected(self):
        for color in (b"\0\0\0", b"\xff\0\0", b"\0\xff\0", b"\xff\xff\0"):
            with self.subTest(color=color):
                a = self.capture("diagnostic.ppm", color * 256 * 384)
                with self.assertRaisesRegex(ValueError, "blank or diagnostic"):
                    smoke.validate([a, a])

    def test_one_black_screen_is_rejected(self):
        a = self.capture("one-screen.ppm", self.frame[:256 * 192 * 3] + b"\0" * (256 * 192 * 3))
        with self.assertRaisesRegex(ValueError, "screen 2"):
            smoke.validate([a, a])

    def test_desktop_capture_and_corrupt_image_are_rejected(self):
        for header, pixels in ((b"P6\n1024 768\n255\n", self.frame),
                               (b"not an image\n", b""),
                               (b"P6\n256 384\n255\n", self.frame[:-1])):
            with self.subTest(header=header, size=len(pixels)):
                a = self.capture("bad.ppm", pixels, header)
                with self.assertRaises(ValueError):
                    smoke.read_capture(a)

    def test_binary_pixel_whitespace_is_preserved(self):
        pixels = b"\n #" + self.frame[3:]
        a = self.capture("binary.ppm", pixels, b"P6\n# ImageMagick comment\n256 384\n255\n")
        self.assertEqual(smoke.read_capture(a), pixels)

    def test_missing_capture_and_single_frame_cannot_pass_cli(self):
        a = self.capture("title.ppm")
        for paths in ([a], [a, self.directory / "missing.ppm"]):
            with self.subTest(paths=paths):
                p = subprocess.run([sys.executable, str(VALIDATOR), *map(str, paths)],
                                   capture_output=True, text=True)
                self.assertNotEqual(p.returncode, 0)
                self.assertIn("DS_BOOT_SMOKE_FAIL", p.stderr)

    def sequence(self, startup, titles=None, alive=None, timeout=4):
        """Drive the real sequence with fixture frames and a deterministic clock."""
        report, calls = {}, []
        now = [0.0]
        startup = list(startup)
        titles = list(titles if titles is not None else startup[-1:] * 3)

        def sleep(seconds):
            now[0] += seconds

        def capture(label, *, allow_missing):
            calls.append((label, allow_missing))
            frames = startup if allow_missing else titles
            value = frames.pop(0) if len(frames) > 1 else frames[0]
            if isinstance(value, Exception):
                raise value
            return value

        self.report, self.calls, self.clock = report, calls, now
        return runner.run_sequence(capture, alive or (lambda: None), report,
                                   clock=lambda: now[0], sleep=sleep,
                                   startup_timeout=timeout)

    def test_delayed_first_game_frame_then_three_animated_captures(self):
        # Main c5735b6: the four-second capture was all black; the later
        # captures were a healthy animated title. A blank warmup frame is
        # not an accepted title sample and must remain in the probe report.
        black = self.capture("startup-black.ppm", b"\0" * 256 * 384 * 3)
        a = self.capture("title-a.ppm")
        b = self.capture("title-b.ppm", self.next_frame)
        with self.assertRaisesRegex(ValueError, "blank or diagnostic"):
            smoke.validate([black, a, b])
        result = self.sequence([black, black, a], [a, b, a])
        self.assertEqual(len(result), 3)
        self.assertEqual([probe["ready"] for probe in self.report["startup_probes"]],
                         [False, False, True])
        self.assertEqual(self.report["ready_after_seconds"], 2)
        self.assertEqual(self.report["stage"], "complete")
        self.assertEqual([allow for _, allow in self.calls],
                         [True, True, True, False, False, False])

    def test_permanently_blank_diagnostic_or_one_screen_boot_times_out(self):
        for pixels in (b"\0" * 256 * 384 * 3,
                       b"\xff\0\0" * 256 * 384,
                       self.frame[:256 * 192 * 3] + b"\0" * 256 * 192 * 3):
            with self.subTest(pixels=pixels[:6]):
                frame = self.capture("not-ready.ppm", pixels)
                with self.assertRaisesRegex(RuntimeError, "within 4s"):
                    self.sequence([frame])
                self.assertEqual(self.clock[0], 4)
                self.assertEqual(len(self.calls), 4)
                self.assertEqual(self.report["captures"], [])

    def test_frozen_title_after_readiness_still_fails(self):
        a = self.capture("title.ppm")
        with self.assertRaisesRegex(ValueError, "frozen"):
            self.sequence([a], [a, a, a])
        self.assertEqual(len(self.calls), 4)

    def test_blank_capture_after_readiness_is_not_retried(self):
        a = self.capture("title.ppm")
        black = self.capture("late-black.ppm", b"\0" * 256 * 384 * 3)
        with self.assertRaisesRegex(ValueError, "blank or diagnostic"):
            self.sequence([a], [a, black, a])
        self.assertEqual(len(self.calls), 3)

    def test_corrupt_startup_capture_is_fatal_without_retry(self):
        bad = self.capture("broken.ppm", b"", b"not an image")
        with self.assertRaisesRegex(ValueError, "expected binary RGB PPM"):
            self.sequence([bad])
        self.assertEqual(len(self.calls), 1)
        self.assertEqual(self.clock[0], 0)

    def test_missing_startup_window_is_bounded_but_later_loss_is_fatal(self):
        a = self.capture("title-a.ppm")
        b = self.capture("title-b.ppm", self.next_frame)
        self.sequence([None, a], [a, b, a])
        self.assertEqual(self.report["ready_after_seconds"], 1)
        with self.assertRaisesRegex(RuntimeError, "within 4s"):
            self.sequence([None])
        self.assertEqual(self.clock[0], 4)
        with self.assertRaisesRegex(RuntimeError, "window disappeared"):
            self.sequence([a], [None])
        self.assertEqual(len(self.calls), 2)

    def test_capture_command_failure_is_fatal_without_retry(self):
        error = subprocess.CalledProcessError(1, ["import", "-window", "0x200007"])
        with self.assertRaises(subprocess.CalledProcessError):
            self.sequence([error])
        self.assertEqual(len(self.calls), 1)
        self.assertEqual(self.clock[0], 0)

    def test_process_exit_before_or_during_capture_is_fatal(self):
        a = self.capture("title.ppm")
        for checks_before_death in (0, 1):
            with self.subTest(checks_before_death=checks_before_death):
                checks = [checks_before_death]

                def alive():
                    if not checks[0]:
                        raise RuntimeError("emulator exited with code 7")
                    checks[0] -= 1

                with self.assertRaisesRegex(RuntimeError, "emulator exited"):
                    self.sequence([a], alive=alive)
                self.assertEqual(len(self.calls), checks_before_death)

    def test_real_capture_wrapper_preserves_command_failures_and_missing_window(self):
        process = mock.Mock()
        process.poll.return_value = None
        capture = runner.EmulatorCapture(process, self.directory)
        missing = subprocess.CompletedProcess(["xwininfo"], 0, "no children\n", "")
        with mock.patch.object(runner.subprocess, "run", return_value=missing):
            self.assertIsNone(capture("initial", allow_missing=True))
            with self.assertRaisesRegex(RuntimeError, "no emulator client window"):
                capture("later", allow_missing=False)
        windows = subprocess.CompletedProcess(["xwininfo"], 0,
            '  0x200007 "Desmume SDL": ("desmume-cli" "desmume-cli") 256x384\n', "")
        failed = subprocess.CalledProcessError(1, ["import"])
        with mock.patch.object(runner.subprocess, "run", side_effect=[windows, failed]) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                capture("failed", allow_missing=True)
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args_list[-1].kwargs["check"], True)
        self.assertTrue((self.directory / "windows-failed.txt").is_file())


if __name__ == "__main__":
    unittest.main()
