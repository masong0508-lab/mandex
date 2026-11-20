#!/usr/bin/env python3
"""Convert P6 RGB story contact sheets into PNG without external dependencies."""
from __future__ import annotations
import sys
import struct
import zlib
from pathlib import Path

SIGNATURE = b"\x89PNG\r\n\x1a\n"

def chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

def convert(source: Path) -> Path:
    head = source.read_bytes().split(b"\n", 3)
    if len(head) != 4 or head[0] != b"P6" or head[2] != b"255":
        raise ValueError(f"{source}: expected uncompressed P6 RGB, 255 maxval")
    w, h = map(int, head[1].split())
    rgb = head[3]
    if w <= 0 or h <= 0 or len(rgb) != 3*w*h:
        raise ValueError(f"{source}: invalid pixel dimensions or length")
    rows = b"".join(b"\0" + rgb[y*3*w:(y+1)*3*w] for y in range(h))
    image = (SIGNATURE
         + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
         + chunk(b"IDAT", zlib.compress(rows, level=8))
         + chunk(b"IEND", b""))
    output = source.with_suffix(".png")
    output.write_bytes(image)
    print(f"{source.name} -> {output.name} ({w} x {h})")
    return output

if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: python3 tools/ppm_to_png.py frame.ppm [frame.ppm ...]")
    for p in sys.argv[1:]:
        convert(Path(p))
