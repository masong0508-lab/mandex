# MANDEX - Nintendo DS build.  Needs devkitPro with devkitARM + libnds:
#   https://devkitpro.org/wiki/Getting_Started   (pacman -S nds-dev)
#
#   export DEVKITPRO=/opt/devkitpro
#   export DEVKITARM=$DEVKITPRO/devkitARM
#   make            -> mandex.nds   (runs in melonDS / DeSmuME / flashcart)
#
# NOTE: this Makefile has NOT been run - no devkitPro toolchain was available when it was written.
# If a flag is off, the devkitPro "nds/hello_world" example Makefile is the reference.
#
# Build knobs (all optional, add to DEFS):
#   -DMX_DS_MAX_SUBSTEPS=3   upper bound for the adaptive physics substeps (2..4)
#   -DMX_SUPPORT_INTERVAL=2  run the structural-support flood fill at most every N frames
#                            (cuts CPU in big collapses; slightly delays things falling down)
#   -DMX_NO_EDGE             disable the interior-voxel contact culling (for A/B profiling)

ifeq ($(strip $(DEVKITARM)),)
$(error "Set DEVKITARM, e.g. export DEVKITARM=/opt/devkitpro/devkitARM")
endif
include $(DEVKITARM)/ds_rules

TARGET  := mandex
CORE    := ../core
# libnds 2.x sits on top of calico (its own headers and library); older libnds has no calico directory and this still works
CALICO  := $(DEVKITPRO)/calico
INCLUDE := -I$(DEVKITPRO)/libnds/include -I$(CALICO)/include -I$(CORE) -I../art -Isource

# MX_FIXED         20.12 fixed point (the ARM9 has no FPU)
# MX_LIBNDS_MATH   core uses libnds mulf32/divf32/sqrtf32 (hardware divide + sqrt units)
# MX_HOT_ITCM      the solver's hot functions go into the 32 KB instruction TCM
# SUBSTEPS         starting physics substeps per frame (the game adapts at run time)
DEFS    := -D__NDS__ -DARM9 -DMX_FIXED -DMX_LIBNDS_MATH -DMX_HOT_ITCM -DSUBSTEPS=3

CFLAGS  := -std=gnu99 -g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer \
           -ffast-math -mthumb-interwork $(INCLUDE) $(DEFS)
# libnds 2.x (calico) has its own specs file (crt0, linker script, calico runtime); devkitARM's ds_arm9.specs is the one for the old libnds 1.x
ifneq ($(wildcard $(CALICO)/share/ds9.specs),)
SPECS   := -specs=$(CALICO)/share/ds9.specs
else
SPECS   := -specs=ds_arm9.specs
endif
LDFLAGS := $(SPECS) -g -mthumb-interwork -Wl,-Map,$(TARGET).map
LIBS    := -L$(DEVKITPRO)/libnds/lib -L$(CALICO)/lib -lnds9 -lm

# ARM mode for the hot code (solver + pixel blitting), Thumb for the rest.
ARM_OBJS   := source/gfx.o $(CORE)/mx.o $(CORE)/cloth.o
THUMB_OBJS := source/main.o source/sfx.o
OBJS       := $(THUMB_OBJS) $(ARM_OBJS)

all: $(TARGET).nds

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(ARM_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -marm -c $< -o $@

$(THUMB_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -mthumb -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).nds $(TARGET).map $(TARGET).arm9

.PHONY: all clean
