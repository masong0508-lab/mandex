# MANDEX - Nintendo DS build ("hardware" build: own start-up code and ARM7, no libnds / calico).
# Needs only devkitARM + ndstool (export DEVKITARM=/opt/devkitpro/devkitARM).
#   make            -> mandex.nds
all:
	cd .. && sh tools/build_ds_bare.sh ds/mandex.nds $(EXTRA_DEFS) && python3 tools/ntr_patch.py ds/mandex.nds ds/mandex.nds.tmp && mv ds/mandex.nds.tmp ds/mandex.nds
clean:
	rm -f mandex.nds mandex.nds.tmp
.PHONY: all clean
