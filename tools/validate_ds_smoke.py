#!/usr/bin/env python3
"""Reject blank, diagnostic-only, corrupt or frozen DS boot captures.

Inputs are 256x384, 8-bit RGB PPMs captured from the DeSmuME client window,
not the desktop (whose clock/window title can animate around a frozen game).
This is a boot/title liveness check, not gameplay or physical-device FPS proof.
ImageMagick already used by the smoke job converts its diagnostic PNGs to PPM;
the validator itself needs only Python's standard library.
"""

import argparse
from pathlib import Path
import sys

WIDTH, HEIGHT = 256, 384
SCREEN_PIXELS = WIDTH * HEIGHT // 2
MIN_COLORS = 16
MIN_LIT_PIXELS = SCREEN_PIXELS // 20
MIN_CHANGED_PIXELS = 64


class UnreadyCapture(ValueError):
    """Valid image data, but a DS screen is still blank or diagnostic-only."""


def read_capture(path):
    data = Path(path).read_bytes()
    pos = 0

    def token():
        nonlocal pos
        while pos < len(data):
            if data[pos] in b" \t\r\n":
                pos += 1
            elif data[pos] == ord("#"):
                end = data.find(b"\n", pos)
                if end < 0:
                    raise ValueError(f"{path}: incomplete PPM comment")
                pos = end + 1
            else:
                break
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        return data[start:pos]

    if token() != b"P6":
        raise ValueError(f"{path}: expected binary RGB PPM")
    try:
        width, height, maximum = (int(token()) for _ in range(3))
    except ValueError as exc:
        raise ValueError(f"{path}: invalid PPM header") from exc
    if (width, height, maximum) != (WIDTH, HEIGHT, 255):
        raise ValueError(f"{path}: expected {WIDTH}x{HEIGHT}, 8-bit DS client capture")
    # Consume the one raster separator, not arbitrary whitespace: the first
    # pixel's RGB bytes may themselves be a space, newline or '#'.
    if pos >= len(data) or data[pos] not in b" \t\r\n":
        raise ValueError(f"{path}: missing PPM raster separator")
    pos += 2 if data[pos:pos + 2] == b"\r\n" else 1
    pixels = data[pos:]
    if len(pixels) != WIDTH * HEIGHT * 3:
        raise ValueError(f"{path}: truncated or oversized pixel data")
    return pixels


def inspect_capture(path):
    pixels = read_capture(path)
    screens = []
    for screen in range(2):
        start = screen * SCREEN_PIXELS * 3
        rgb = pixels[start:start + SCREEN_PIXELS * 3]
        colors = set()
        lit = 0
        for i in range(0, len(rgb), 3):
            color = rgb[i:i + 3]
            colors.add(color)
            lit += max(color) > 8
        if len(colors) < MIN_COLORS or lit < MIN_LIT_PIXELS:
            raise UnreadyCapture(f"{path}: DS screen {screen + 1} is blank or diagnostic-only "
                                 f"({len(colors)} colors, {lit} lit pixels)")
        screens.append({"colors": len(colors), "lit_pixels": lit})
    return pixels, screens


def validate(paths):
    if len(paths) < 2:
        raise ValueError("at least two captures are required to check animation")
    previous = None
    results = []
    for path in paths:
        pixels, screens = inspect_capture(path)
        changed = None
        if previous is not None:
            changed = sum(pixels[i:i + 3] != previous[i:i + 3]
                          for i in range(0, len(pixels), 3))
            if changed < MIN_CHANGED_PIXELS:
                raise ValueError(f"{path}: frozen or nearly frozen client image "
                                 f"({changed} changed pixels)")
        results.append({"capture": str(path), "screens": screens,
                        "changed_pixels": changed})
        previous = pixels
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+")
    args = parser.parse_args()
    try:
        for result in validate(args.captures):
            print(result)
    except (OSError, ValueError) as exc:
        print(f"DS_BOOT_SMOKE_FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"DS_BOOT_SMOKE_PASS captures={len(args.captures)} screens=2 title_animation=1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
