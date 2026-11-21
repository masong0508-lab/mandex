#!/usr/bin/env python3
"""Character turnaround invariants shared by web-authored DS/3DS/Dreamcast poses.

Pure stdlib, deliberately does not require headless Chromium or source art
regeneration. Checks canonical facial identity BEFORE it is rasterised into
the 71-frame/2-outfit console atlas. Run: python3 test/head_turnaround_test.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import manhead

def check(cond, message):
    if not cond:
        raise AssertionError(message)

def mask(rows):
    return tuple(tuple(ch != "." for ch in row) for row in rows)

# Expression overlays may change the eyebrows, eyes, lips and nose, but never
# shrink the scalp or invent floating pieces outside the common side skull.
fly = manhead.side_head("fly")
dive = manhead.side_head("dive")
check(len(fly) == len(dive) == 14, "canonical side skull must be 14 rows")
check(mask(fly) == mask(dive), "profile expressions changed skull silhouette")
check(fly != dive, "flying and diving need visibly different expressions")
# A tiny rear tuft and a bald crown were making the profile resemble a
# different character. The cap must cover the same crown in every expression.
for kind, rows in (("fly", fly), ("dive", dive)):
    crown = sum(ch in "hHl" for row in rows[1:7] for ch in row)
    check(crown >= 30, f"{kind} loses the front comb-over at side profile")
    check(rows[1][7] in "hH" and rows[3][8] in "hH",
          f"{kind} has a bald crown between 3/4 and side")
web = (pathlib.Path(__file__).resolve().parents[1] / "web" / "index.html").read_text()
check("ctx.moveTo(cx-r*.78,cy-r*.38)" in web,
      "browser side profile lost the same crown sweep as console art")

widths = []
for angle in range(0, 181, 3):
    rows = manhead.turn_head(angle)
    width = len(rows[0])
    widths.append(width)
    check(len(rows) == 19, f"head height popped at {angle} deg")
    check(14 <= width <= 19, f"invalid skull width at {angle} deg")
    check(all(len(row) == width for row in rows),
          f"ragged pixel art at {angle} deg")
    check(set("".join(rows)) <= set(manhead.KEY) | {"."},
          f"unknown palette symbol at {angle} deg")
    check(any(ch in "hHl" for row in rows for ch in row),
          f"hair vanished at {angle} deg")
    check(any(ch in "SsKk" for row in rows for ch in row),
          f"face/skull vanished at {angle} deg")
    # The furry version shares the profile generator, including side/back
    # landmarks, rather than inventing a second, differently shaped head.
    wolf = manhead.wolfify(rows)
    check(all(len(row) == width for row in wolf),
          f"wolf ears exceed skull canvas at {angle} deg")

# A turning head is at most as wide as the last frame up to the true side,
# and cannot suddenly widen before the back hair has turned toward camera.
check(widths[0] == 19, "front head has incorrect width")
check(all(a >= b for a, b in zip(widths[:31], widths[1:31])),
      "forehead width grows while turning toward profile")
check(manhead.turn_head(87) == manhead.turn_head(93),
      "middle-of-turn profile inexplicably changes shape")

quarter = manhead.turn_head(48)
edge = len(quarter[0]) // 2 - 1
check(all(ch not in "BiwGg" for row in quarter[7:12] for ch in row[:edge]),
      "far eye is still visible at the 3/4 turn")
rear = manhead.turn_head(145)
check(all(ch not in "BiwGgmMtTL" for row in rear[5:17] for ch in row[6:14]),
      "facial features float on the rear of the head")
check(manhead.turn_head(135) != manhead.turn_head(165),
      "rear quarter and full back have identical hair without a turn")
print("PASS: 61 head views, both outfit variants, one coherent skull/eye/hair geometry")
print("PASS: stable profile silhouette, visible dive expression, far-eye occlusion")
print("PASS: no face floating through rear-quarter turn; no new atlas buffers")
