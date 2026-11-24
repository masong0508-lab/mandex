#!/usr/bin/env python3
"""Verify prelit handheld art and the render-budget rules (no external deps)."""
import re
from pathlib import Path

root=Path(__file__).resolve().parents[1]
data=(root/'art/manspr.h').read_text()
script=(root/'tools/gen_manspr.py').read_text()
web=(root/'web/index.html').read_text()
ds=(root/'ds/source/main.c').read_text()
dc=(root/'dreamcast/main.c').read_text()
palette=re.search(r'#define MS_NPAL (\d+)',data)
nbase=re.search(r'#define MSF_NBASE (\d+)',data)
assert palette and nbase
ncolors=int(palette[1]);bases=int(nbase[1])
assert ncolors==39 and bases==71,(ncolors,bases)
frames={}
for m in re.finditer(r'\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),',data):
    name,w,h,ax,ay=m.groups()
    frames[name]=(int(w),int(h),int(ax),int(ay))
arrays={}
for m in re.finditer(r'static const unsigned char MS_(\w+)\[(\d+)\] = \{(.*?)\};',data,re.S):
    name,total,body=m.groups()
    nums=[int(t) for t in re.findall(r'\d+',body)]
    assert len(nums)==int(total),(name,len(nums),total)
    assert name in frames,name
    w,h,ax,ay=frames[name]
    assert len(nums)==w*h and 0<=min(nums)<=max(nums)<=ncolors,name
    assert sum(v!=0 for v in nums)>60,name
    arrays[name]=(w,h,ax,ay,nums)
assert len(arrays)==bases*2,(len(arrays),bases)
assert all(name+'_W' in arrays for name in list(arrays) if not name.endswith('_W'))
assert 'finish_cel_shading(idx, PI, int(ay), name)' in script
assert 'if name.endswith("_W"):' in script
assert "source = idx.copy()" in script
# The silhouette remains binary, never made of semi-transparent pixels;
# highlights and shadows occupy the body below the hand-painted face.
for name in ('IDLE_A','IDLE_B','FLEX_A','DRAG','STUCK_A'):
    w,h,ax,ay,p=arrays[name]
    torso=[v for y in range(ay+2,min(h,ay+21)) for v in p[y*w:(y+1)*w]]
    assert 3 in torso and 4 in torso,(name,'lost face-safe body shading')
# The browser now follows the handheld's collision-resolved gait instead of
# tying footsteps to the absolute map X coordinate (which slid at walls).
assert "function walkStrideStep(m){" in web
assert "Math.sin((m.gait||0)*Math.PI/(2*V))" in web
assert "if(m.gr)m.gait=((m.gait||0)+Math.abs(m.x-strideStartX))%(4*V)" in web
assert "m.x/15" not in web
# Reversals now blend through walkTurnAngle()/walkTurnFace() rather than
# snapping directly back to the old side-on walking transform.
assert "const th=turning?walkTurnAngle(m.turn):Math.PI/2+.055*step;" in web
assert "const visualFace=turning?walkTurnFace(m.face,m.turn):m.face;" in web
assert "lit?'rgba(255,224,164,.52)'" in web
assert "x.fillStyle='rgba(215,248,166,.20)'" in web
# The intact-glazing ledge must be rendered with the same palette ink,
# whether the clipped reference painter or the optional in-bounds painter
# handles the stroke. The A/B renderer test checks pixel-exact output.
assert ('gfx_rect(v->buf, x + 1, y + hh - 2, ww - 2, 1, C_CITY2);' in ds or
        'FAC_RECT(x + 1, y + hh - 2, ww - 2, 1, C_CITY2);' in ds)
assert 'else gfx_rect(v->buf,(X),(Y),(W),(H),(C));' in ds
assert 'for(float k = 0; k < w; k += 20)' in dc
assert 'light[j+1][i+1]' in dc
assert 'static uint32 light[' not in dc  # small stack-only vertex gradient
print(f'PASS: {len(arrays)} sprites, {ncolors} palette colors, stable byte-count/opacity, baked shading')
print('PASS: distance-locked walk cycle, pre-baked scenery, console glazing and Dreamcast single-pass lighting')
