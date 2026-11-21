#!/usr/bin/env python3
"""One-time in-place profile ink migration for the shipped console sprite sheet.

The canonical model and future rebuild are in manhead.py/gen_manspr.py. This
migration puts the new shared hair/ear/eye/moustache details in the checked-in
art/manspr.h *now*, without changing its width, height, palette, anchors,
alpha mask, RAM footprint or frame order. After a full browser sprite rebake,
the canonical source replaces the migration; do not use it to resize heads.
"""
from pathlib import Path
import re
import sys
import manhead

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "art/manspr.h"
INDEX = {k: n+1 for n, k in enumerate(
    "RIM SK SKD SKH SKM BR BRD BRH WH GR HAIR HAIR2 BLK "
    "TOW TOW2 TOWD GOLD GOLD2 SHOE SHOED MOUTH TONG HAIRL "
    "BLUSH IRIS LIP TOOTH".split())}
INK = {ch: INDEX[name] for ch, name in manhead.KEY.items()}
FRAME = re.compile(r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),")
ARRAY = re.compile(
    r"(static const unsigned char MS_(\w+)\[(\d+)\] = \{\n)"
    r"([\s\S]*?)"
    r"(\};)", re.M)
DIGIT = re.compile(r"\d+")

def is_profile(name):
    if name.endswith("_W"):
        return False  # the Wolfman's muzzle is a different canonical design
    if name in ("FLY_A", "FLY_B", "DIVE"):
        return True
    if name.startswith("SPIN_"):
        rest = name[5:]
        returning = rest.startswith("R")
        try:
            k = int(rest[1:] if returning else rest)
        except ValueError:
            return False
        a = k * 15
        m = min(1.0, a / 20.0, (180-a) / 20.0)
        a += (-12 if returning else 12) * m
        return 56 <= a < 125
    return False

def patch_string(data):
    meta = {m[1]:tuple(map(int,m.groups()[1:])) for m in FRAME.finditer(data)}
    total, affected = 0, []
    def replacement(m):
        nonlocal total
        name = m[2]
        if not is_profile(name):
            return m[0]
        w, h, ax, ay = meta[name]
        px = list(map(int, DIGIT.findall(m[4-1])))
        assert len(px)==w*h==(int(m[3])), (name,len(px),w,h)
        side = manhead.side_head("dive" if name == "DIVE" or name.startswith("SPIN_") else "fly")
        rows = manhead.shrink(side,14,11)
        if name in ("FLY_A","FLY_B","DIVE"):
            x0 = ax + round(31 * .40) - 7
            y0 = ay + round(-17 * .40) - 4
        else:
            x0 = ax - 7
            y0 = ay - 19
        changed=0
        for j,row in enumerate(rows):
            for i,ch in enumerate(row):
                x,y=x0+i,y0+j
                if ch=='.' or not (0<=x<w and 0<=y<h):
                    continue
                pos=y*w+x
                if px[pos]==0:
                    continue  # pixel-exact opaque silhouette is invariant
                color=INK[ch]
                if px[pos] != color:
                    px[pos]=color
                    changed+=1
        affected.append((name,changed))
        total+=changed
        body="".join("  "+",".join(map(str,px[y*w:(y+1)*w]))+",\n" for y in range(h))
        return m[1]+body+m[4]
    output=ARRAY.sub(replacement,data)
    assert len(affected)>=10 and total>=60,(affected,total)
    return output,total,affected

def main():
    original=HEADER.read_text()
    updated,total,affected=patch_string(original)
    if "--check" in sys.argv:
        # A second run must leave the committed sheet unchanged.
        assert updated==original, ("Prebaked profile mismatch",total)
        print("PASS: pre-baked human profile ink matches canonical face/ear/hair landmarks")
    else:
        HEADER.write_text(updated)
        print("Retouched %d profile pixels across %d frames" % (total,len(affected)))
        print(affected)
if __name__=="__main__":
    main()
