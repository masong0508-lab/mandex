#!/usr/bin/env python3
"""Rendered-pixel check: human and Wolfman previews use real runtime palettes.

The source indexed atlas stores neutral skin colors; DS and 3DS recolor the
Wolfman via mx_outfit_skin() while drawing. The screenshot review must apply
that same four-entry fur palette, without editing the actual indexed atlas.
"""
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import render_turnaround_sheet as preview

base, frames = preview.frames()
fur = preview.wolfman_palette(base)
assert base[1:5] == [
    (244, 183, 142), (205, 135, 100), (255, 215, 180), (226, 160, 118)
], "production base skin colors changed"
assert fur[1:5] == [
    (140, 144, 158), (101, 105, 121), (185, 189, 201), (120, 124, 139)
], "runtime Wolfman palette did not match mx_outfit_skin"
assert fur[:1] == base[:1] and fur[5:] == base[5:]
assert fur != base, "the Wolfman palette cannot look like human skin"

with tempfile.TemporaryDirectory() as tmp:
    output = Path(tmp) / "actual-turns.ppm"
    preview.build(output)
    data = output.read_bytes()
    header, dims, maxval, image = data.split(b"\n", 3)
    width, height = map(int, dims.split())
    assert header == b"P6" and maxval == b"255"
    assert len(image) == width * height * 3
    for row, name, palette in [
            (0, "SPIN_6", base),
            (1, "SPIN_6_W", fur)]:
        w, h, ax, ay, pixels = frames[name]
        samples = [(x, y, pixels[y * w + x])
                   for y in range(h) for x in range(w)
                   if pixels[y * w + x] in (2, 3, 4, 5)]
        assert samples, name
        x, y, code = samples[len(samples) // 2]
        cx = 6 * preview.CELL_W + preview.CELL_W // 2
        cy = row * preview.CELL_H + preview.CELL_H // 2 + 13
        xx = cx + (x - ax) * preview.SCALE
        yy = cy + (y - ay) * preview.SCALE
        shown = tuple(image[(yy * width + xx) * 3:(yy * width + xx) * 3 + 3])
        assert shown == palette[code - 1], (name, shown, palette[code - 1])

print("PASS: actual turnaround PPM uses runtime human and grey Wolfman fur colors")
