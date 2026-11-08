#!/bin/sh
# Builds the DS ROM with the bare-metal runtime (ds/bare): no libnds, no calico.
# usage: build_ds_bare.sh <out.nds> [extra -D flags]
set -e
OUT=$1; shift
: "${DEVKITARM:=/opt/devkitpro/devkitARM}"; : "${DEVKITPRO:=/opt/devkitpro}"
export PATH=$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH
CC=arm-none-eabi-gcc
B=ds/bare; W=$(mktemp -d)
COMMON="-std=gnu99 -g -Wall -O2 -ffast-math -fomit-frame-pointer -I$B -Ids/source -Icore -Iart -D__NDS__ -DMX_FIXED -DMX_LIBNDS_MATH -DSUBSTEPS=3 $*"
A9="-march=armv5te -mtune=arm946e-s -mthumb-interwork -DARM9"
for f in ds/source/main.c ds/source/sfx.c; do $CC $COMMON $A9 -mthumb -c $f -o $W/$(basename $f .c).o; done
for f in ds/source/gfx.c core/mx.c core/cloth.c $B/hw9.c; do $CC $COMMON $A9 -marm -c $f -o $W/$(basename $f .c).o; done
$CC $A9 -marm -c $B/crt0_9.S -o $W/crt0_9.o
$CC -nostartfiles -specs=nosys.specs -T $B/arm9.ld -Wl,--gc-sections -Wl,-Map,$W/arm9.map $W/crt0_9.o $(ls $W/*.o | grep -v crt0) -o $W/arm9.elf -lm -lc -lgcc
arm-none-eabi-objcopy -O binary $W/arm9.elf $W/arm9.bin
$CC -march=armv4t -mcpu=arm7tdmi -marm -Os -ffreestanding -c $B/arm7.c -o $W/arm7.o
$CC -march=armv4t -mcpu=arm7tdmi -marm -c $B/crt0_7.S -o $W/crt0_7.o
$CC -nostartfiles -nostdlib -T $B/arm7.ld $W/crt0_7.o $W/arm7.o -o $W/arm7.elf -lgcc
arm-none-eabi-objcopy -O binary $W/arm7.elf $W/arm7.bin
ls -l $W/arm9.bin $W/arm7.bin
ndstool -c "$OUT" -h 0x4000 -9 $W/arm9.bin -7 $W/arm7.bin -r9 0x02000000 -e9 0x02000800 -r7 0x02380000 -e7 0x02380000 \
  -b $DEVKITPRO/calico/share/nds-icon.bmp "MANDEX;a sticky dad-bod game;masong0508"
