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
echo "--- air valve + power-up rules (float and fixed) ---"
gcc -std=c99 -O2 -I../core ../core/mx.c ../core/mx_meta.c valve_test.c -lm -o valve_float && ./valve_float | tail -n 1
gcc -std=c99 -O2 -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c valve_test.c -lm -o valve_fixed && ./valve_fixed | tail -n 1
echo "--- star rating helper (shared by all front-ends) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c stars_test.c -lm -o stars_float && ./stars_float | tail -n 1
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c stars_test.c -lm -o stars_fixed && ./stars_fixed | tail -n 1
echo "--- level unlock order (rules on / default off) ---"
gcc -std=c99 -O2 -Wall -DMX_LEVEL_LOCKS -I../core ../core/mx.c ../core/mx_meta.c meta_test.c -lm -o meta_locks && ./meta_locks | tail -n 1
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c meta_test.c -lm -o meta_open && ./meta_open | tail -n 1
echo "--- save blob: layout, round trip, corruption, truncation, other level counts (float and fixed) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c save_test.c -lm -o save_float && ./save_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c save_test.c -lm -o save_fixed && ./save_fixed | grep -v "^ok:"
echo "--- achievement progress bars agree with the unlock rules ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c ach_test.c -lm -o ach_float && ./ach_float | tail -n 3
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c ach_test.c -lm -o ach_fixed && ./ach_fixed | tail -n 3
echo "--- wobble / block-kind physics (fixed point): sway, snaps, topple, sand, rubber ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DKICKV=10 -I../core wobble_test.c -lm -o wobble_test && ./wobble_test
echo "--- robustness sweep (560 random shot sequences per math mode; must never hang) ---"
gcc -std=gnu99 -O2 -I../core ../core/mx.c sweep.c -lm -o sweep_float && ./sweep_float
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c sweep.c -lm -o sweep_fixed && ./sweep_fixed
echo "--- same sweep with the wobble lattice switched off (the DS falls back to this under load) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DSWEEP_NOWOB -I../core ../core/mx.c sweep.c -lm -o sweep_nowob && ./sweep_nowob
echo "--- solver work with / without interior-voxel contact culling (fixed point) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c profile_contacts.c -lm -o prof_edge && ./prof_edge
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_NO_EDGE -I../core ../core/mx.c profile_contacts.c -lm -o prof_noedge && ./prof_noedge
echo "--- towel cloth (core/cloth.c): stays finite and the right length, moves, same in float and fixed point ---"
gcc -std=gnu99 -O2 -Wall -I../core ../core/cloth.c ../core/mx.c cloth_test.c -lm -o cloth_float && ./cloth_float | grep -v "^  p"
gcc -std=gnu99 -O2 -Wall -DMX_FIXED -I../core ../core/cloth.c ../core/mx.c cloth_test.c -lm -o cloth_fixed && ./cloth_fixed | grep -v "^  p"
echo "--- DS front-end: real main loop on stub libnds, scripted stylus play on levels 1, 4, 6, 7 ---"
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -Istubs -I../core -I../art -I../ds/source ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_render
echo "--- DS cartridge save: a win writes the stub chip, a second boot loads it, garbage means a clean start ---"
rm -f /tmp/mx_save.sav; MX_SAVE_PATH=/tmp/mx_save.sav ./ds_render /tmp/dssave1 12000 0 | grep "^save:"
MX_SAVE_PATH=/tmp/mx_save.sav ./ds_render /tmp/dssave2 400 0 | grep "^save:" | grep "file_valid=1" > /dev/null && echo "ok: second boot reads the first boot's progress"
head -c 100 /dev/urandom > /tmp/mx_bad.sav; MX_SAVE_PATH=/tmp/mx_bad.sav ./ds_render /tmp/dssave3 400 0 | grep -c "^final" > /dev/null && echo "ok: a garbage chip starts fresh"
for L in 0 3 5 6; do ./ds_render /tmp/dsL$L 12000 $L | grep -v "^  chunk"; done
echo "--- DS outfits screen: open, walk, wear, locked swatches, back (scripted stylus + d-pad) ---"
OUTFIT_SHOTS=1 ./ds_render /tmp/dsout 400 0 | grep -v "^ok:"
echo "--- DS title cheat: L+R+SELECT unlocks every level for the session, in any key order ---"
CHEAT_TEST=1 ./ds_render /tmp/dscheat 400 0 | grep -v "^ok:"
echo "--- idle gags: every step valid, every gag ends and plays, he falls asleep, input wakes him ---"
gcc -std=c99 -O2 -Wall -Wno-unused-function -I../art idle_test.c -o idle_test && ./idle_test
echo "--- Dreamcast front-end drawing on the host (software rasteriser instead of the PVR): jiggle, gags, win/lose ---"
gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -Istubs -I../core -I../art ../core/mx.c ../core/mx_meta.c ../core/cloth.c dc_render.c -lm -o dc_render && ./dc_render
