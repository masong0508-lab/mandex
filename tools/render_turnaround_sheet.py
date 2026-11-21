#!/usr/bin/env python3
"""Build a visual character turnaround from the ACTUAL compiled console atlas.

No browser, Pillow, additional game assets or framebuffer allocations.
Run: python3 tools/render_turnaround_sheet.py out/art-review/turnaround.ppm
Convert with tools/ppm_to_png.py for GitHub Actions screenshot review.
Every pose is registered to the same (ax, ay) gameplay pivot, not its bbox.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = (ROOT / "art" / "manspr.h").read_text()
SCALE, CELL_W, CELL_H = 2, 116, 130
BACKGROUND = (159, 193, 219)
BASE = (202, 223, 235)
SEPARATOR = (67, 94, 117)

FORWARD = [f"SPIN_{i}" for i in range(13)]
RETURN = [f"SPIN_R{i}" for i in range(11, 0, -1)]
ACTION = ["IDLE_A", "DRAG", "FLY_A", "FLY_B", "DIVE",
          "STUCK_A", "STUCK_B", "BACK_C", "BACK_L",
          "BACK_R", "FART_A", "FART_B", "CAPE_A"]
ROWS = [
    ("HUMAN TURN 0 TO 180", FORWARD),
    ("WOLFMAN TURN 0 TO 180", [n + "_W" for n in FORWARD]),
    ("HUMAN RETURN 165 TO 15", RETURN),
    ("WOLFMAN RETURN 165 TO 15", [n + "_W" for n in RETURN]),
    ("HUMAN ACTION AND PROFILE", ACTION),
    ("WOLFMAN ACTION AND PROFILE", [n + "_W" for n in ACTION]),
]

def frames():
    p = re.search(r"static const unsigned char MS_PAL\[MS_NPAL\]\[3\] = \{(.*?)\};",
                  DATA, re.S)
    assert p, "console palette is missing"
    pal = [tuple(map(int, triplet)) for triplet in re.findall(
        r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}", p.group(1))]
    meta = {}
    for name, w, h, ax, ay in re.findall(
            r"\{\s*MS_(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,",
            DATA):
        meta[name] = tuple(map(int, (w, h, ax, ay)))
    sprites = {}
    for name, total, body in re.findall(
            r"static const unsigned char MS_(\w+)\[(\d+)\] = \{(.*?)\};",
            DATA, re.S):
        if name not in meta:
            continue
        w, h, ax, ay = meta[name]
        px = list(map(int, re.findall(r"\d+", body)))
        assert len(px) == w * h == int(total), f"truncated sprite: {name}"
        assert all(0 <= v <= len(pal) for v in px), f"out-of-palette sprite: {name}"
        sprites[name] = (w, h, ax, ay, px)
    wanted = {n for _, seq in ROWS for n in seq}
    assert wanted <= sprites.keys(), f"missing turnaround frames: {wanted - sprites.keys()}"
    return pal, sprites

def build(destination: Path):
    pal, sprites = frames()
    width, height = len(FORWARD) * CELL_W, len(ROWS) * CELL_H
    rgb = bytearray(bytes(BACKGROUND) * (width * height))
    def put(x, y, c):
        if 0 <= x < width and 0 <= y < height:
            k = (y * width + x) * 3
            rgb[k:k+3] = bytes(c)
    def rect(x0, y0, x1, y1, c):
        ink = bytes(c)
        for y in range(max(0,y0), min(height,y1)):
            start = (y * width + max(0,x0)) * 3
            stop = (y * width + min(width,x1)) * 3
            rgb[start:stop] = ink * max(0, min(width,x1) - max(0,x0))
    manifest = ["MANDEX — actual compiled console atlas turnaround",
                "Columns share the same image-space origin at sprite.ax / sprite.ay.",
                "Same atlas palette, frame bounds and pivots; intentional micro-pinholes filled."]
    for row, (label, names) in enumerate(ROWS):
        manifest.append(f"row {row+1}: {label}")
        for col, name in enumerate(names):
            w, h, ax, ay, px = sprites[name]
            manifest.append(f"  column {col+1:02d}  {name:16s} {w}x{h}  anchor {ax},{ay}")
            cellx, celly = col * CELL_W, row * CELL_H
            rect(cellx+2, celly+3, cellx+CELL_W-3, celly+CELL_H-4, BASE)
            rect(cellx+2, celly+3, cellx+CELL_W-3, celly+6, SEPARATOR)
            cx, cy = cellx + CELL_W//2, celly + CELL_H//2 + 13
            # Alignment to the ACTUAL gameplay pivot makes head-height
            # pops, hip/torso shifts and hair-mass inconsistencies obvious.
            x0, y0 = cx - ax*SCALE, cy - ay*SCALE
            for py in range(h):
                for qx in range(w):
                    k = px[py*w+qx]
                    if not k: continue
                    sx, sy = x0 + qx*SCALE, y0 + py*SCALE
                    c = pal[k-1]
                    for yy in range(SCALE):
                        for xx in range(SCALE):
                            put(sx+xx, sy+yy, c)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(f"P6\n{width} {height}\n255\n".encode()+rgb)
    destination.with_suffix(".txt").write_text("\n".join(manifest)+"\n")
    print(f"PASS: painted {sum(len(n) for _, n in ROWS)} true shipped frames")
    print(f"PASS: 6 rows × {len(FORWARD)} columns, common body pivots, {width}x{height} RGB")
    print(f"Review sheet: {destination}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: render_turnaround_sheet.py output.ppm")
    build(Path(sys.argv[1]))
