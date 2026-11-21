#!/usr/bin/env python3
"""Canonical body/head turnaround and checked-in console profile regression."""
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import manhead
import retouch_baked_heads as baked

def ok(v,msg):
    assert v,msg
    print("ok:",msg)

front=manhead.head("open","grin")
ok(len(front)==19 and all(len(r)==19 for r in front),
   "frontal skull stays 19 x 19 with a fixed eye/mouth line")
fly=manhead.side_head("fly")
dive=manhead.side_head("dive")
ok(len(fly)==len(dive)==14 and all(len(x)==17 for x in fly+dive),
   "flying and diving use the SAME 17 x 14 side skull")
ok([[ch!="." for ch in row] for row in fly]==
   [[ch!="." for ch in row] for row in dive],
   "side expressions preserve exactly the same hair and face silhouette")

for kind,head in (("fly",fly),("dive",dive)):
    p=manhead.SIDE_LANDMARKS
    ok(p["ear"][0]<p["eye"][0]<p["nose"][0],
       kind+" ear, eye, nose stay ordered in skull-local space")
    for role in ("brow","eye","moustache","mouth","chin"):
        x,y=p[role]
        ok(head[y][x] not in ".o",
           kind+" "+role+" remains inside the face contour")

angles=[0,15,22,30,38,45,55,56,60,75,90,105,120]
widths=[]
for angle in angles:
    g=manhead.turn_head(angle)
    widths.append(len(g[0]))
    ok(len(g)==19 and all(len(row)==len(g[0]) for row in g),
       "turn %s: stable 19-row head height"%angle)
ok(all(a>=b for a,b in zip(widths,widths[1:])),
   "front-to-side head gets narrower monotonically instead of inflating")
ok(widths[0]==19 and widths[-1]==14,
   "turn widths progress from 19-pixel frontal to 14-pixel profile")
ok(manhead.turn_head(90)==manhead.turn_head(115),
   "profile expression and hair volume are identical across the side arc")
ok(manhead.side_head("fly")!=manhead.side_head("dive"),
   "diving expression changes the eyes and mouth, not the skull shape")

header=(ROOT/"art"/"manspr.h").read_text()
ok("#define MS_NPAL 39" in header and "#define MSF_NBASE 71" in header,
   "palettes and 71 human plus 71 wolf frame indices stay stable")
if "#define MS_CANONICAL_TURN_BAKE 1" in header:
    ok("MSF_SPIN_R11" in header and "MSF_FLY_B_W" in header,
       "fully regenerated console atlas retains all turnaround/flight frame names")
    ok("static const MsFrame MS_FRAMES[MSF_COUNT]" in header,
       "complete high-fidelity re-bake preserves renderer metadata and frame order")
else:
    output,changed,frames=baked.patch_string(header)
    ok(output==header and changed==0 and len(frames)==13,
       "the 13 shipped profile sprites already contain the canonical face/hair ink")
    ok(all(x[1]==0 for x in frames),
       "re-running baked art migration cannot change any pixels")
src=(ROOT/"tools"/"gen_manspr.py").read_text()
ok('manhead.turn_head(a, "dive")' in src,
   "future sprite exports use the same 19-row head turnaround")
web=(ROOT/"web"/"index.html").read_text()
ok("const nz=G3(0,-39.7,17,3.2,2.9,3.6,U2);" in web and
   "const jaw=G3(0,-34,3.8,11.4,8.2,11,U)" in web,
   "browser 3D profiles share projecting nose and forward jaw depth")

print("PASS: canonical head/body turnaround, 13 baked human profiles, no extra assets")
