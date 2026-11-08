#!/bin/sh
# Builds the shared core in both math modes and runs the regression test.
set -e
cd "$(dirname "$0")"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c host_test.c -lm -o test_float
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c host_test.c -lm -o test_fixed
./test_float > float.txt && tail -n 1 float.txt
./test_fixed > fixed.txt && tail -n 1 fixed.txt
echo "float vs fixed per-shot comparison:"
paste -d'|' float.txt fixed.txt | grep " L[0-9]" | sed 's/score=[0-9]*//g' | awk -F'|' '{print $1 "  ||  fixed: " substr($2, index($2,"destroyed"))}'
echo "--- libnds hardware-math hooks vs software fixed point (must be bit-identical) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_LIBNDS_MATH -Istubs -I../core ../core/mx.c host_test.c -lm -o test_fixed_hw && ./test_fixed_hw > fixed_hw.txt && cmp fixed.txt fixed_hw.txt && echo "identical"
echo "--- wobble / block-kind physics (fixed point): sway, snaps, topple, sand, rubber ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DKICKV=10 -I../core wobble_test.c -lm -o wobble_test && ./wobble_test
echo "--- robustness sweep (280 random shot sequences per math mode; must never hang) ---"
gcc -std=gnu99 -O2 -I../core ../core/mx.c sweep.c -lm -o sweep_float && ./sweep_float
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c sweep.c -lm -o sweep_fixed && ./sweep_fixed
echo "--- same sweep with the wobble lattice switched off (the DS falls back to this under load) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DSWEEP_NOWOB -I../core ../core/mx.c sweep.c -lm -o sweep_nowob && ./sweep_nowob
echo "--- solver work with / without interior-voxel contact culling (fixed point) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c profile_contacts.c -lm -o prof_edge && ./prof_edge
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_NO_EDGE -I../core ../core/mx.c profile_contacts.c -lm -o prof_noedge && ./prof_noedge
echo "--- DS front-end: real main loop on stub libnds, scripted stylus play on levels 1, 4, 6, 7 ---"
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -Istubs -I../core -I../art -I../ds/source ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../core/mx.c -lm -o ds_render
for L in 0 3 5 6; do ./ds_render /tmp/dsL$L 12000 $L | grep -v "^  chunk"; done
echo "--- idle gags: every step valid, every gag ends and plays, he falls asleep, input wakes him ---"
gcc -std=c99 -O2 -Wall -Wno-unused-function -I../art idle_test.c -o idle_test && ./idle_test
echo "--- Dreamcast front-end drawing on the host (software rasteriser instead of the PVR): jiggle, gags, win/lose ---"
gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -Istubs -I../core -I../art ../core/mx.c dc_render.c -lm -o dc_render && ./dc_render
