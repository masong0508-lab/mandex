#!/usr/bin/env python3
"""Repair tiny enclosed transparent sprite pinholes in baked character art.

Do not globally close holes: arm gaps, the space between knees and elbow
loops must remain transparent. Only 1-2 pixel, four-connected, *enclosed*
voids sufficiently above the body pivot with a contour-ink boundary qualify.
All operations happen offline, not on the handheld CPU or Dreamcast PVR.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "art/manspr.h"
META = re.compile(r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),")
ARRAY = re.compile(r"(static const unsigned char MS_(\w+)\[(\d+)\] = \{\n)([\s\S]*?)(\};)", re.M)


def close_pinholes(rows, anchor_y, name=None):
    """Mutable 2D list or numpy array; returns pixels made opaque."""
    h = len(rows)
    w = len(rows[0])
    seen = bytearray(w * h)
    holes = []
    for start in range(w * h):
        if seen[start] or int(rows[start // w][start % w]):
            continue
        area = [start]
        seen[start] = 1
        touches_edge = False
        for pos in area:
            x, y = pos % w, pos // w
            if x in (0, w - 1) or y in (0, h - 1):
                touches_edge = True
            near = (pos - 1 if x else -1,
                    pos + 1 if x < w - 1 else -1,
                    pos - w if y else -1,
                    pos + w if y < h - 1 else -1)
            for other in near:
                if other < 0 or seen[other]:
                    continue
                if not int(rows[other // w][other % w]):
                    seen[other] = 1
                    area.append(other)
        if touches_edge or len(area) > 2 or max(p // w for p in area) >= anchor_y - 6:
            continue
        touching = []
        for pos in area:
            x, y = pos % w, pos // w
            for other in (pos - 1 if x else -1,
                          pos + 1 if x < w - 1 else -1,
                          pos - w if y else -1,
                          pos + w if y < h - 1 else -1):
                if other >= 0:
                    color = int(rows[other // w][other % w])
                    if color:
                        touching.append(color)
        if touching.count(1) < 2:
            continue
        skin = [v for v in touching if 2 <= v <= 5]
        hair = [v for v in touching if v in (11, 12, 23)]
        # A little skin/fur inside a closed brown outline, rather than
        # another thick dark-ink blob. Adjacent hair can be preserved.
        color = max(set(skin), key=lambda v: (skin.count(v), -v)) if skin else (
            max(set(hair), key=lambda v: (hair.count(v), -v)) if hair else 2)
        holes.append((area, color))
    for area, color in holes:
        for pos in area:
            rows[pos // w][pos % w] = color
    return sum(len(area) for area, color in holes)


def patch_string(data):
    meta = {m[1]: tuple(map(int, m.groups()[1:])) for m in META.finditer(data)}
    changed = []
    def fix(m):
        name = m[2]
        if name not in meta:
            return m[0]
        w, h, ax, ay = meta[name]
        values = [int(v) for v in re.findall(r"\d+", m[4])]
        assert len(values) == int(m[3]) == w * h, (name, w, h)
        grid = [values[i * w:(i + 1) * w] for i in range(h)]
        amount = close_pinholes(grid, ay, name)
        if not amount:
            return m[0]
        changed.append((name, amount))
        body = "".join("  " + ",".join(str(v) for v in row) + ",\n" for row in grid)
        return m[1] + body + m[5]
    out = ARRAY.sub(fix, data)
    return out, sum(n for _, n in changed), changed


def main():
    original = HEADER.read_text()
    new, count, frames = patch_string(original)
    if "--check" in sys.argv:
        assert new == original and count == 0, ("Regression: transparent pinholes", frames)
        print("PASS: compiled console silhouettes have no enclosed upper-body micro-pinholes")
        return
    assert count >= 25, (count, frames)
    next_pass, next_count, _ = patch_string(new)
    assert next_count == 0 and next_pass == new, "Sprite pinhole fix is not idempotent"
    HEADER.write_text(new)
    print("Closed %d enclosed single/double-pixel gaps across %d atlas frames" %
          (count, len(frames)))


if __name__ == "__main__":
    main()
