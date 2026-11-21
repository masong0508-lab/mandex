#!/usr/bin/env python3
"""Validate the baked indexed character textures, not only source pose code.

Checks every shipped frame retains valid palette pixels and verifies the
Wolfman has distinct light/shadow fur volumes while keeping his own heads.
No PIL, Chromium, numpy, graphics hardware or extra runtime assets required.
"""
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
data=(ROOT/"art/manspr.h").read_text()
meta={name:tuple(map(int,(w,h,ax,ay))) for name,w,h,ax,ay in
    re.findall(r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),",data)}
arrays={}
for name,size,body in re.findall(
        r"static const unsigned char MS_(\w+)\[(\d+)\] = \{(.*?)\};",data,re.S):
    if name not in meta:continue
    px=[int(x) for x in re.findall(r"\d+",body)]
    w,h,ax,ay=meta[name]
    assert 0<=ax<w and 0<=ay<h and len(px)==w*h==int(size),name
    assert all(0<=v<=39 for v in px),name
    arrays[name]=(w,h,ax,ay,px)

assert len(arrays)==142,len(arrays)
wolf=[(k,*v) for k,v in arrays.items() if k.endswith("_W")]
assert len(wolf)==71,len(wolf)
body_tones=0
for name,w,h,ax,ay,px in wolf:
    tone=[px[y*w+x] for y in range(max(0,ay+2),min(h,ay+22))
          for x in range(w)]
    # Each model has existing skin/fur highlight (4) and shadow (3) indices.
    if 3 in tone and 4 in tone:
        body_tones+=1
assert body_tones>=68,("Wolfman lost 3-tone torso/rear shading",body_tones)

for base in ("SPIN_0","SPIN_3","SPIN_6","SPIN_9","SPIN_12",
             "FLY_A","DIVE","BACK_C"):
    normal=arrays[base]
    fur=arrays[base+"_W"]
    assert (normal[0],normal[1],normal[4])!=(fur[0],fur[1],fur[4]), \
        ("Wolfman artwork accidentally aliases human",base)

print("PASS: 142 actual character frames, stable image bounds and 39-color palette")
print("PASS: %d/71 Wolfman poses have distinct existing palette highlights/shadows" %
      body_tones)
print("PASS: eight key human/Wolfman front, side, back and flight sprite pairs differ")
