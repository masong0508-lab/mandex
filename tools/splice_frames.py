#!/usr/bin/env python3
"""
Copies chosen frames from a freshly generated manspr.h into the committed one, leaving every other frame byte-identical.

Why: the HTML prototype's man drawing keeps changing (limbs, belly physics), and regenerating the whole atlas would silently
change every console frame.  This adds only the named frames (new ones are appended to the enum in the order given).

Usage:  python3 tools/splice_frames.py OLD_manspr.h NEW_manspr.h OUT_manspr.h NAME [NAME ...]
"""
import re, sys

def parse(path):
    s = open(path).read()
    arrays = {m.group(1): m.group(0) for m in re.finditer(r"static const unsigned char MS_([A-Z_0-9]+)\[\d+\] = \{.*?\n\};", s, re.S)}
    enum = re.search(r"enum \{(.*?), MSF_COUNT \};", s, re.S).group(1).replace("\n", "").split(", ")
    names = [e.strip()[4:] for e in enum]
    fr = re.search(r"static const MsFrame MS_FRAMES\[MSF_COUNT\] = \{\n(.*?)\n\};", s, re.S).group(1).split("\n")
    hd = re.search(r"static const short MS_HAND\[MSF_COUNT\]\[3\] = \{\n(.*?)\n\};", s, re.S).group(1).split("\n")
    assert len(names) == len(fr) == len(hd), (len(names), len(fr), len(hd))
    return s, names, arrays, dict(zip(names, fr)), dict(zip(names, hd))

old, nm_old, arr_old, fr_old, hd_old = parse(sys.argv[1])
new, nm_new, arr_new, fr_new, hd_new = parse(sys.argv[2])
want = sys.argv[4:]
for n in want: assert n in arr_new, "frame %s is not in the generated header" % n
pal = lambda s: re.search(r"MS_PAL\[MS_NPAL\]\[3\] = \{(.*?)\};", s, re.S).group(1)
assert pal(old) == pal(new), "the palettes differ: the spliced frames would use the wrong colours"
s = old
for n in want:
    a = arr_new[n]
    if n in arr_old:
        s = s.replace(arr_old[n], a)
        fr_old[n] = fr_new[n]; hd_old[n] = hd_new[n]
    else:
        s = s.replace("typedef struct { const unsigned char *px;", a + "\ntypedef struct { const unsigned char *px;", 1)
        s = s.replace(", MSF_COUNT };", ", MSF_%s, MSF_COUNT };" % n, 1)
        nm_old.append(n); fr_old[n] = fr_new[n]; hd_old[n] = hd_new[n]
s = re.sub(r"(static const MsFrame MS_FRAMES\[MSF_COUNT\] = \{\n).*?(\n\};)", lambda m: m.group(1) + "\n".join(fr_old[n] for n in nm_old) + m.group(2), s, count=1, flags=re.S)
s = re.sub(r"(static const short MS_HAND\[MSF_COUNT\]\[3\] = \{\n).*?(\n\};)", lambda m: m.group(1) + "\n".join(hd_old[n] for n in nm_old) + m.group(2), s, count=1, flags=re.S)
open(sys.argv[3], "w").write(s)
print("spliced", ", ".join(want), "->", sys.argv[3])
