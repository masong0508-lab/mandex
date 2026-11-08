#!/usr/bin/env python3
"""
Turns the DSi-enhanced header that calico / ndstool writes (unit code 0x02, DSi9 / DSi7 regions, because the stock ARM7 binary has a TWL part)
into a plain Nintendo DS (NTR) header: unit code 0, DSi fields cleared, header CRC recomputed.  Some emulators (melonDS in its DS mode, DraStic)
do not boot a DSi-enhanced ROM; a plain DS ROM runs everywhere, including a DSi in DS mode.

Usage: ntr_patch.py in.nds out.nds
"""
import struct, sys


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


rom = bytearray(open(sys.argv[1], "rb").read())
stored = struct.unpack_from("<H", rom, 0x15E)[0]
calc = crc16(rom[0:0x15E])
if stored != calc:
    sys.exit("header CRC check failed (stored %04X, computed %04X): not patching" % (stored, calc))
print("unit code was %02X, header CRC %04X ok" % (rom[0x12], stored))
rom[0x12] = 0
a9 = struct.unpack_from("<I", rom, 0x20)[0]
ent, ram = struct.unpack_from("<II", rom, 0x24)
print("ARM9 starts with", rom[a9:a9 + 16].hex(), "entry-ram = %#x" % (ent - ram))
# DraStic (and real carts) call a ROM "encrypted" unless the first 0x800 bytes of the ARM9 image are the standard decrypted
# secure-area fill (E7FFDEFF repeated) or zeros.  When the code starts 0x800 after the image base that area is free, so fill it.
if ent - ram == 0x800:
    rom[a9:a9 + 0x800] = struct.pack("<I", 0xE7FFDEFF) * 0x200
    struct.pack_into("<H", rom, 0x6C, crc16(rom[a9:a9 + 0x4000]))      # secure area CRC (ARM9 0x4000..0x7FFF)
    print("secure area filled, CRC %04X" % struct.unpack_from("<H", rom, 0x6C)[0])
else:
    print("WARNING: code starts at the image base, no secure area to fill")
for i in range(0x1C0, 0x1E0):            # DSi9 / DSi7 ROM offsets, RAM addresses, sizes
    rom[i] = 0
struct.pack_into("<H", rom, 0x15E, crc16(rom[0:0x15E]))
open(sys.argv[2], "wb").write(rom)
print("wrote", sys.argv[2], "unit code now", rom[0x12])
