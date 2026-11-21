#!/usr/bin/env python3
"""Finish Wolfman upper-silhouette fur lighting in the baked indexed atlas.

The torso and lower body already get palette lighting in gen_manspr.py,
but the upper shoulders, raised arms and head still look like large uniform
flat-grey patches. This purely OFFLINE pass shades the inside of broad
existing fur runs above the head/body pivot with original SKH/SKD entries.
It never edits the pixel mask, source palette, eye/ear/muzzle ink, pivot or
frame dimensions. Idempotent, safe to run on every canonical sprite bake.
"""
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
HEADER=ROOT/"art"/"manspr.h"
META=re.compile(r"\{\s*MS_(\w+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),")
ARRAY=re.compile(r"(static const unsigned char MS_(\w+)\[(\d+)\] = \{\n)([\s\S]*?)(\};)",re.M)
SK,SKD,SKH,SKM=2,3,4,5

def finish_grid(rows,anchor_y,name):
    if not name.endswith("_W"):
        return 0
    height,width=len(rows),len(rows[0])
    changed=0
    # Use the source surface when choosing edges: an already-coloured pixel
    # must not cause the highlight to spread after a second bake.
    for y in range(max(0,min(height,anchor_y-7))):
        x=0
        while x<width:
            if int(rows[y][x]) not in (SK,SKD,SKH,SKM):
                x+=1
                continue
            begin=x
            while x<width and int(rows[y][x]) in (SK,SKD,SKH,SKM):
                x+=1
            if x-begin<7:
                continue
            if int(rows[y][begin+1])==SK:
                rows[y][begin+1]=SKH
                changed+=1
            if int(rows[y][x-2])==SK:
                rows[y][x-2]=SKD
                changed+=1
    return changed

def patch_string(data):
    meta={m[1]:tuple(map(int,m.groups()[1:])) for m in META.finditer(data)}
    affected=[]
    def patch(m):
        name=m[2]
        if not name.endswith("_W"):
            return m[0]
        width,height,ax,ay=meta[name]
        px=[int(v) for v in re.findall(r"\d+",m[4])]
        assert len(px)==width*height==int(m[3]),name
        grid=[px[y*width:(y+1)*width] for y in range(height)]
        count=finish_grid(grid,ay,name)
        affected.append((name,count))
        if not count:
            return m[0]
        return m[1]+"".join("  "+",".join(map(str,row))+",\n" for row in grid)+m[5]
    out=ARRAY.sub(patch,data)
    return out,sum(n for _,n in affected),affected

def main():
    original=HEADER.read_text()
    updated,count,frames=patch_string(original)
    assert len(frames)==71,("Wolfman frame count changed",len(frames))
    if "--check" in sys.argv:
        assert updated==original and count==0,("Upper fur lighting drift",frames)
        print("PASS: 71 Wolfman upper-body/face-side fur volumes are consistently baked")
        return
    assert count>=500,("Missing large upper-fur regions",count)
    again,repeat,_=patch_string(updated)
    assert repeat==0 and again==updated,"Upper fur lighting should be idempotent"
    HEADER.write_text(updated)
    print("Finished %d upper fur pixels in %d Wolfman frames" %
          (count,len([1 for _,n in frames if n])))
if __name__=="__main__":
    main()
