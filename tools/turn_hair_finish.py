#!/usr/bin/env python3
"""Idempotent crown-light finish for Mandex's shipped human turnaround.

Reuses HAIRL (palette index 23) on a narrow strip of existing HAIR pixels.
Never changes transparency, atlas size, pivot, palette, wolf silhouette, or
frame ordering. The same function is called by tools/gen_manspr.py during
a real Chromium sprite rebuild and can polish the current compiled header.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "art" / "manspr.h"
HAIR, DARK, LIGHT = 11, 12, 23
META = re.compile(r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),")
ARRAY = re.compile(
    r"(static const unsigned char MS_(\w+)\[(\d+)\] = \{\n)"
    r"([\s\S]*?)"
    r"(\};)", re.M,
)


def is_turn(name):
    if name.endswith("_W"):  # wolf fur receives different highlights
        return False
    if name in ("FLY_A", "FLY_B", "DIVE"):
        return True
    return re.fullmatch(r"SPIN_(?:R)?(?:[0-9]|1[0-2])", name) is not None


def finish_grid(rows, anchor_y, name):
    """Apply a narrow, angle-independent hair glint; works on numpy or lists."""
    if not is_turn(name):
        return 0
    top = None
    changed = 0
    for y in range(min(len(rows), max(0, anchor_y + 2))):
        hair = [x for x, color in enumerate(rows[y])
                if int(color) in (HAIR, DARK, LIGHT)]
        if len(hair) < 3:
            continue
        if top is None:
            top = y
        if y < top + 2 or y > top + 7 or len(hair) < 4:
            continue
        # Keep the same normalized place on the crown regardless of the
        # current head width/angle, without searching for a new pixel on
        # rerun (which would cause the highlight to spread each time).
        x = hair[len(hair) * 35 // 100]
        if int(rows[y][x]) == HAIR:
            rows[y][x] = LIGHT
            changed += 1
    return changed


def patch_string(data):
    meta = {m[1]: tuple(map(int, m.groups()[1:])) for m in META.finditer(data)}
    affected = []

    def modify(m):
        name = m[2]
        if not is_turn(name):
            return m[0]
        w, h, ax, ay = meta[name]
        pix = [int(v) for v in re.findall(r"\d+", m[4])]
        assert len(pix) == int(m[3]) == w * h, (name, len(pix), w, h)
        rows = [pix[k*w:(k+1)*w] for k in range(h)]
        count = finish_grid(rows, ay, name)
        affected.append((name, count))
        if not count:
            return m[0]
        body = "".join("  " + ",".join(str(v) for v in row) + ",\n"
                       for row in rows)
        return m[1] + body + m[5]

    updated = ARRAY.sub(modify, data)
    return updated, sum(n for _, n in affected), affected


def main():
    before = HEADER.read_text()
    after, changes, frames = patch_string(before)
    assert len(frames) == 27, ("Missing full turn or flight artwork", frames)
    assert all(n == 0 for name, n in frames) if "--check" in sys.argv else True
    if "--check" in sys.argv:
        assert after == before, "Re-running crown finish changes the shipped atlas"
        print("PASS: 27 actual compiled human turn/flight frames retain stable crown lighting")
        return
    assert changes > 50, ("No meaningful improvement to the existing art", changes)
    again, repeats, _ = patch_string(after)
    assert repeats == 0 and again == after, "Crown finish must be idempotent"
    HEADER.write_text(after)
    print("Finished %d existing hair pixels across %d actual atlas frames" %
          (changes, len(frames)))


if __name__ == "__main__":
    main()
