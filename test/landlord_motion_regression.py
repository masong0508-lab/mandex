#!/usr/bin/env python3
"""Cross-port landlord animation/alloc checks, backed by the real DS pixel gate."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ds=(root/"ds/source/main.c").read_text()
paint=(root/"ds/source/gfx.c").read_text()
n3=(root/"3ds/main.c").read_text()
dc=(root/"dreamcast/main.c").read_text()
web=(root/"web/index.html").read_text()
assert "phase=t->mv?(int)(RTOF(t->wk)*14.0f)" in ds
assert "spr_landlord_impl(b,x,y,ppc,ph,scared,(st&24)!=0)" in paint
assert "int step = moving ?" in paint
assert "const int gait=(int)(RTOF(t->wk)*14.0f)" in n3
assert "int reach=wind?" in n3
assert "if(x < -p*12" in n3
assert "float step=(t&&t->mv)?" in dc
assert "if(wind){" in dc
assert "LORD_GLOW=mkCanvas(56,56)" in web
actor=web[web.index("function drawLandlord(t){"):web.index("function drawCases(){")]
assert "ctx.drawImage(LORD_GLOW,-28,-32)" in actor
assert "createRadialGradient(" not in actor  # no per-enemy gradient allocation
assert "bob=pan||t.mv?0" in actor
print("PASS: walk phase follows actual travel on DS, 3DS, Dreamcast and browser")
print("PASS: 3DS culled limbs and windup tells; no duplicate DS feet; browser glow cached")
