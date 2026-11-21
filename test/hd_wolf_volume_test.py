#!/usr/bin/env python3
"""Synthetic unit checks for offline Dreamcast HD fur contour finishing.

This test runs with stdlib only (no Pillow, numpy, Playwright or KOS).
The full high-res image itself is produced by the canonical rebake workflow.
"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from hd_wolf_finish import finish_hd_wolf_volume

width,height,pivot=22,16,4
img=[[[0,0,0,0] for x in range(width)] for y in range(height)]
for y in range(7,15):
    for x in range(3,19):
        img[y][x]=[30,25,19,255] if x in (3,18) else [145,120,95,255]
img[2][10]=[220,190,150,255]       # canonical facial detail above pivot
img[7][11]=[175,145,125,96]        # translucent blend fringe outside opaque run
before_alpha=[[v[3] for v in row] for row in img]
changes=finish_hd_wolf_volume(img,pivot)
assert changes>25,changes
assert img[2][10]==[220,190,150,255],"head/eyes must never be repainted"
assert img[7][11]==[175,145,125,96],"translucent silhouette fringe must stay unchanged"
assert img[8][5][0]>145,"left edge should catch key light"
assert img[8][16][0]<145,"opposite edge should gain soft fill shadow"
assert img[8][10][:3]==[145,120,95],"body centre should retain authored HD materials"
assert img[8][3][:3]==[30,25,19],"dark contour must stay dark"
assert [[v[3] for v in row] for row in img]==before_alpha,"ARGB4444 opacity/UV silhouette changed"
assert finish_hd_wolf_volume(img,pivot)>0,"the rebake finish is a deliberate RGB grading operation"
print("PASS: Dreamcast HD wolf key/fill contour, preserved head and 100% alpha invariants")
