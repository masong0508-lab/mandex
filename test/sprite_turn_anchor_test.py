#!/usr/bin/env python3
"""Guard the actual shipped front/side/rear spin art against vertical wobble.

Tests indexed human + Wolfman pixel footprints and gameplay pivots, not
generator metadata alone. The foot plane is intentionally identical across
the complete 12-tick reversal. Allow a small tolerance for future redraws,
but not detached or floating characters caused by a warped sprite anchor.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
data = (ROOT / "art/manspr.h").read_text()
meta = {name: tuple(map(int, (w, h, ax, ay)))
        for name, w, h, ax, ay in re.findall(
            r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),", data)}
arrays = {name: [int(v) for v in re.findall(r"\d+", body)]
          for name, length, body in re.findall(
              r"static const unsigned char MS_(\w+)\[(\d+)\] = \{(.*?)\};",
              data, re.S) if name in meta}

# The spine of the full character must not bob up/down as the camera moves
# from facing front to profile, rear and then back through the opposite side.
names = ([f"SPIN_{i}" for i in range(13)] +
         [f"SPIN_R{i}" for i in range(1, 12)])
for suffix in ("", "_W"):
    upper, lower = [], []
    for label in names:
        name = label + suffix
        w, h, ax, ay = meta[name]
        pixels = arrays[name]
        assert len(pixels) == w * h, name
        occupied_y = [i // w for i, color in enumerate(pixels) if color]
        assert occupied_y, name
        crown = min(occupied_y) - ay
        feet = max(occupied_y) - ay
        assert abs(crown + 28) <= 2, (name, "head/crown jumped", crown)
        assert abs(feet - 23) <= 3, (name, "foot plane jumped", feet)
        upper.append(crown)
        lower.append(feet)
    assert max(upper) - min(upper) <= 3, (suffix, "head flicker", upper)
    assert max(lower) - min(lower) <= 3, (suffix, "floating feet", lower)

print("PASS: all 48 real human/Wolfman turnaround sprites share grounded feet and stable crown height")
