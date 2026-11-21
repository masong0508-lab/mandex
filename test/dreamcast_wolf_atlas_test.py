#!/usr/bin/env python3
"""Ensure the Dreamcast has 71 truly separate HD Wolfman poses.

One 1024x512 texture cannot fit 142 high-resolution characters. The
previous shared atlas silently aliased 32 Wolfman frames to human art, with
incorrect hair and face during turns. The rebuilt generator now produces
two dedicated 16-bit PVR atlases (1 MB each); this test guards that all 71
Wolfman UV rectangles are packed and nonoverlapping within their own page.
No game-frame allocation or extra texture passes are introduced.
"""
from pathlib import Path
import re

s=(Path(__file__).resolve().parents[1]/"art/dcart.h").read_text()
def req(pattern,description):
    m=re.search(pattern,s)
    assert m,description
    return m

for kind in ("MAN","WMAN"):
    w=int(req(r"#define DCT_"+kind+r"_W (\d+)",kind+" width")[1])
    h=int(req(r"#define DCT_"+kind+r"_H (\d+)",kind+" height")[1])
    assert (w,h)==(1024,512),(kind,w,h)
assert int(req(r"#define DCM_NFR (\d+)","frame count")[1])==142
block=req(r"static const short DCM_FR\[142\]\[13\] = \{([\s\S]*?)\n\};","frames")[1]
records=[]
for vals,name in re.findall(r"\{([\d,\- ]+)\}, /\* (\w+) \*/",block):
    v=[int(k) for k in vals.split(",")]
    assert len(v)==13,(name,len(v))
    records.append((name,v))
assert len(records)==142,len(records)
assert len({n for n,_ in records})==142,"duplicate frame names"
human=records[:71]
wolf=records[71:]
assert [n+"_W" for n,_ in human]==[n for n,_ in wolf],"Wolfman indices drifted out of sync with human"
for kind,frames in (("human",human),("Wolfman",wolf)):
    # Both atlases use 1px border and gutter so PVR bilinear doesn't
    # sample neighboring expressions when the character rotates.
    for n,v in frames:
        x,y,w,h,ax,ay=v[:6]
        assert x>=1 and y>=1 and x+w<1024 and y+h<512,(kind,n,v)
        assert 0<=ax<w and 0<=ay<h,(kind,n,"anchor",v)
    for i,(n,a) in enumerate(frames):
        for m,b in frames[i+1:]:
            ax,ay,aw,ah=a[:4]; bx,by,bw,bh=b[:4]
            assert ax+aw<bx or bx+bw<ax or ay+ah<by or by+bh<ay,(
                kind,"aliased or overlapping rectangles",n,m)
print("PASS: 71 human + 71 Wolfman distinct HD atlas rectangles, 1 MB each")
print("PASS: aligned frame indices, safe anchors, no shared texture rectangles")
