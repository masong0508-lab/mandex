#!/usr/bin/env python3
"""
Makes ds/mandex9.ld + ds/mandex9.specs from calico's legacy ARM9 linker script, with the layout classic homebrew ROMs have:
ARM9 image at 0x02000000, a 0x800-byte (blank) secure area first, code entry at 0x02000800.
Usage: mk_secure_ld.py <calico dir> <out dir>
"""
import sys, os
cal, out = sys.argv[1], sys.argv[2]
ld = open(os.path.join(cal, "lib", "ds9-legacy.ld")).read()
mark = "\t/* Initialization code section (crt0) */"
assert mark in ld, "marker not found in ds9-legacy.ld"
ld = ld.replace(mark, "\t/* classic secure area: 0x800 blank bytes, code starts after it */\n\t.secure : {\n\t\tBYTE(0);\n\t\t. = 0x800;\n\t} >lma9 :main = 0x00\n\n" + mark, 1)
open(os.path.join(out, "mandex9.ld"), "w").write(ld)
sp = open(os.path.join(cal, "share", "ds9-legacy.specs")).read().replace("ds9-legacy.ld", "mandex9.ld")
open(os.path.join(out, "mandex9.specs"), "w").write(sp)
print("wrote mandex9.ld / mandex9.specs")
