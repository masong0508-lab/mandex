#!/usr/bin/env python3
"""Exercise real false-positive boot captures without an emulator or PIL."""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

VALIDATOR = Path(__file__).resolve().parents[1] / "tools/validate_ds_smoke.py"
spec = importlib.util.spec_from_file_location("ds_smoke", VALIDATOR)
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


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


if __name__ == "__main__":
    unittest.main()
