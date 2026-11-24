#!/bin/sh
# Builds the shared core in both math modes and runs the regression test.
set -e
cd "$(dirname "$0")"
echo "--- Canonical face, hair and side-profile bake consistency (web and all console atlases) ---"
python3 character_turnaround_test.py
echo "--- checked-in 142-frame atlas keeps consistent front/profile/rear hair lighting ---"
python3 ../tools/turn_hair_finish.py --check
python3 ../tools/sprite_pinhole_cleanup.py --check
python3 ../tools/wolf_upper_volume.py --check
python3 sprite_volume_continuity_test.py
python3 hd_wolf_volume_test.py
python3 turnaround_preview_palette_test.py
python3 sprite_turn_anchor_test.py
python3 head_turnaround_test.py
echo "--- Dreamcast: all 71 Wolfman profiles use independent HD UV rectangles ---"
python3 dreamcast_wolf_atlas_test.py
echo "--- DS inflation native-resolution detail, silhouette and clipping ---"
gcc -std=gnu99 -O2 -Wall -DMX_FIXED -Istubs -I../core -I../art ds_inflation_detail_test.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_inflation_detail_test && ./ds_inflation_detail_test
echo "--- DS emulator smoke validator: reject frozen, blank and incomplete boot captures ---"
python3 ds_smoke_test.py
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c host_test.c -lm -o test_float
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c host_test.c -lm -o test_fixed
./test_float > float.txt && tail -n 1 float.txt
./test_fixed > fixed.txt && tail -n 1 fixed.txt
echo "float vs fixed per-shot comparison:"
paste -d'|' float.txt fixed.txt | grep " L[0-9]" | sed 's/score=[0-9]*//g' | awk -F'|' '{print $1 "  ||  fixed: " substr($2, index($2,"destroyed"))}'
echo "--- libnds hardware-math hooks vs software fixed point (must be bit-identical) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_LIBNDS_MATH -Istubs -I../core ../core/mx.c host_test.c -lm -o test_fixed_hw && ./test_fixed_hw > fixed_hw.txt && cmp fixed.txt fixed_hw.txt && echo "identical"
echo "--- back-wall impacts and floating room collapse (shared core fixed/float, web) ---"
gcc -std=gnu99 -O2 -Wno-unused-function -I../core room_backdrop_test.c -lm -o room_backdrop_float && ./room_backdrop_float
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_FIXED -I../core room_backdrop_test.c -lm -o room_backdrop_fixed && ./room_backdrop_fixed
if command -v node >/dev/null 2>&1;then node room_backdrop_web_test.js;fi
echo "--- actual victory objective vs wreck score, last landlord direction, HQ progression (float/fixed/browser) ---"
for mode in float fixed; do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=c99 -O2 -Wall $flags -I../core objective_tracker_test.c -o objective_tracker_$mode
  ./objective_tracker_$mode
done
if command -v node >/dev/null 2>&1;then node browser_objective_test.js;fi
echo "--- cross-port character pose continuity and bounded collapse feedback (float/fixed) ---"
for mode in float fixed; do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=c99 -O2 -Wall $flags -I../core ../core/mx.c visual_feedback_test.c -lm -o visual_feedback_$mode
  ./visual_feedback_$mode
done
echo "--- walking countersteer: interrupted rear turns remain continuous (float/fixed) ---"
for mode in float fixed; do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=c99 -O2 -Wall $flags -I../core ../core/mx.c walk_turn_interrupt_test.c -lm -o walk_turn_interrupt_$mode
  ./walk_turn_interrupt_$mode
done
echo "--- gameplay recovery and stomp-pad ascent (float/fixed/browser) ---"
for mode in float fixed; do
  flags=""
  if test "$mode" = fixed; then flags="-DMX_FIXED"; fi
  for probe in combat_recovery walk_pad_stomp walk_jump_forgiveness case_parry; do
    gcc -std=gnu99 -O2 -Wno-unused-function $flags -I../core "${probe}_test.c" -lm -o "${probe}_$mode"
    "./${probe}_$mode"
  done
done
if command -v node >/dev/null 2>&1;then
  node browser_combat_recovery_test.js
  node browser_pad_stomp_test.js
  node browser_walk_jump_forgiveness_test.js
  node browser_case_parry_test.js
fi
echo "--- real-time frame meter: rendered FPS vs simulation steps/s ---"
gcc -std=gnu99 -O2 -I../core fps_meter_test.c -o fps_meter_test && ./fps_meter_test
echo "--- Browser refresh-rate-independent fixed-step simulation ---"
if command -v node >/dev/null 2>&1;then node browser_fixed_step_test.js;fi
echo "--- Browser walk physics parity, reusable touch controls and character-detail GC ---"
if command -v node >/dev/null 2>&1;then node browser_walk_memory_test.js;fi
echo "--- Browser: cached structural-support buffers, 120/360 world widths ---"
if command -v node >/dev/null 2>&1;then node browser_support_cache_test.js;fi
echo "--- Browser: bounded collision hashing, repeated contacts and no stale rubble refs ---"
if command -v node >/dev/null 2>&1;then node browser_collision_hash_test.js;fi
echo "--- Browser walking camera: portrait focus, zoom bounds and world edges ---"
if command -v node >/dev/null 2>&1;then node browser_walk_camera_test.js;fi
echo "--- Browser portrait/landscape layouts, touch hitboxes and editor panning ---"
if command -v node >/dev/null 2>&1;then node browser_orientation_test.js;fi
echo "--- Browser custom levels: safe 360-column finale transition ---"
if command -v node >/dev/null 2>&1;then node browser_custom_level_width_test.js;fi
echo "--- Browser editor: real pointer events and touch cancellation ---"
if command -v node >/dev/null 2>&1;then node browser_editor_pointer_events_test.js;fi
echo "--- Browser editor: isolate painting to owning pointer ---"
if command -v node >/dev/null 2>&1;then node browser_editor_touch_isolation_test.js;fi
echo "--- Browser creator: starter structures, undo/redo, mobile actions ---"
if command -v node >/dev/null 2>&1;then node browser_creator_starters_test.js;fi
echo "--- Browser creator: large-brush allocations, rectangle cancellation and preflight ---"
if command -v node >/dev/null 2>&1;then node browser_creator_paint_test.js;fi
echo "--- Browser creator: storage quota and recovery saves ---"
if command -v node >/dev/null 2>&1;then node browser_creator_storage_test.js;fi
echo "--- Browser creator: bounded MX1 imports, Unicode share names and legacy codes ---"
if command -v node >/dev/null 2>&1;then node browser_creator_codec_test.js;fi
echo "--- Web: touch capture, landscape/portrait two-thumb controls, safe camera pan ---"
if command -v node >/dev/null 2>&1;then node browser_multitouch_test.js;fi
echo "--- Portrait: captured pull, viewport safety, neutral thumb, camera and touch actions ---"
if command -v node >/dev/null 2>&1;then node browser_portrait_play_test.js;fi
echo "--- Stage guidance: all 60 console levels and 40 browser early-stage tactics ---"
for mode in float fixed;do
  flags=''
  if test "$mode" = fixed;then flags="-DMX_FIXED";fi
  gcc -std=c99 -O2 -Wall $flags -I../core ../core/mx.c level_tactic_test.c -lm -o "level_tactic_$mode"
  "./level_tactic_$mode"
done
if command -v node >/dev/null 2>&1;then node browser_early_tactics_test.js;fi
echo "--- Chapter 3 street encounters: landmark geometry, defenders and 3-second stability (float/fixed) ---"
for mode in float fixed; do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=c99 -O2 -Wall $flags -I../core ../core/mx.c wolf_annex_test.c -lm -o "wolf_annex_$mode"
  "./wolf_annex_$mode"
done
echo "--- Browser late Wolfman alleys: all 12 added structures and truthful tactical tips ---"
if command -v node >/dev/null 2>&1;then node browser_wolf_alley_test.js;fi
echo "--- Responsive collapse: 512-cell split, damaged joints, impact waking (float/fixed/browser) ---"
for mode in float fixed;do
  flags=''
  if test "$mode" = fixed;then flags='-DMX_FIXED';fi
  gcc -std=gnu99 -O2 -Wall -Wno-unused-function $flags -I../core collapse_response_test.c -lm -o "collapse_response_$mode"
  "./collapse_response_$mode"
done
if command -v node >/dev/null 2>&1;then node browser_collapse_response_test.js;fi
echo "--- Moving TNT: padded fuse state, impact/safe outcomes, precise blast hitboxes (float/fixed/browser) ---"
for mode in float fixed;do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=gnu99 -O2 -Wall -Wno-unused-function $flags -I../core mobile_tnt_test.c -lm -o "mobile_tnt_$mode"
  "./mobile_tnt_$mode"
done
if command -v node >/dev/null 2>&1;then node browser_mobile_tnt_test.js;fi
echo "--- read-only shot readiness and actual power stock (float/fixed/browser) ---"
for mode in float fixed;do
  flags=""
  test "$mode" = fixed && flags="-DMX_FIXED"
  gcc -std=gnu99 -O2 -Wno-unused-function $flags -I../core tool_feedback_test.c ../core/mx.c ../core/cloth.c -lm -o tool_feedback_$mode
  ./tool_feedback_$mode
done
if command -v node >/dev/null 2>&1;then node browser_tool_feedback_test.js;fi
echo "--- Browser side -> rear -> side pose coherence at 60Hz ---"
if command -v node >/dev/null 2>&1;then node browser_turn_pose_test.js;fi
echo "--- Browser walking gait: actual contacts, 4-cell stride, and reusable pose ---"
if command -v node >/dev/null 2>&1;then node browser_walk_gait_test.js;fi
echo "--- Browser left/right flight, dive and pneumatic tube animation parity ---"
if command -v node >/dev/null 2>&1;then node browser_flight_pose_test.js;fi
echo "--- Browser cutscene JavaScript syntax and required rear-gag effects ---"
if command -v node >/dev/null 2>&1; then
  node -e 'const fs=require("fs");const h=fs.readFileSync("../web/index.html","utf8");const a=h.indexOf("<script>"),b=h.indexOf("</script>",a);if(a<0||b<a)throw Error("game script missing");const s=h.slice(a+8,b);new Function(s);for(const sym of ["function cgRearPanel","function cgHotTake","function cgMoonlitKiss"])if(!s.includes(sym))throw Error(sym+" missing");console.log("ok: browser script parses and cutscene effects are present")'
fi
echo "--- rare collapsed-room survivor: HP, frame support, trapped pocket, one-time roll (float/fixed) ---"
for mode in float fixed; do
  defs=""
  test "$mode" = fixed && defs="-DMX_FIXED"
  gcc -std=gnu99 -O2 -Wno-unused-function $defs -I../core collapse_survivor_test.c -lm -o collapse_survivor_$mode
  ./collapse_survivor_$mode
done
if command -v node >/dev/null 2>&1; then
  node collapse_survivor_web_test.js
  node landlord_ai_web_test.js
  node browser_rubble_draw_test.js
  node browser_flight_collision_fastpath_test.js
fi
echo "--- moving rubble vs landlords: conservative contact AABB parity (fixed/float/browser) ---"
for mode in float fixed; do
  defs=""
  test "$mode" = fixed && defs="-DMX_FIXED"
  gcc -std=gnu99 -O2 $defs -I../core rubble_broadphase_test.c -lm -o rubble_broadphase_$mode
  ./rubble_broadphase_$mode
done
if command -v node >/dev/null 2>&1;then node rubble_broadphase_web_test.js;fi
echo "--- shattered rubble stops contacting during the same solver step (float/fixed/browser) ---"
for mode in float fixed; do
  defs=""
  test "$mode" = fixed && defs="-DMX_FIXED"
  gcc -std=gnu99 -O2 -Wno-unused-function $defs -I../core chunk_contact_lifetime_test.c -lm -o chunk_contact_lifetime_$mode
  ./chunk_contact_lifetime_$mode
done
if command -v node >/dev/null 2>&1;then node browser_dead_rubble_test.js;fi
echo "--- BANK SHOT trick bonus: true rubber rebound, direct eviction only, 2s expiry (fixed/float) ---"
for mode in float fixed; do
  defs=""
  test "$mode" = fixed && defs="-DMX_FIXED"
  gcc -std=gnu99 -O2 -Wno-unused-function $defs -I../core bank_shot_test.c -lm -o bank_shot_$mode
  ./bank_shot_$mode
done
echo "--- eviction streak: capped score, fixed/float parity, timer expiry and armored targets ---"
gcc -std=gnu99 -O2 -Wno-unused-function -I../core combo_test.c -lm -o combo_float && ./combo_float
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_FIXED -I../core combo_test.c -lm -o combo_fixed && ./combo_fixed
echo "--- air valve + power-up rules (float and fixed) ---"
gcc -std=c99 -O2 -I../core ../core/mx.c ../core/mx_meta.c valve_test.c -lm -o valve_float && ./valve_float | tail -n 1
gcc -std=c99 -O2 -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c valve_test.c -lm -o valve_fixed && ./valve_fixed | tail -n 1
echo "--- star rating helper (shared by all front-ends) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c stars_test.c -lm -o stars_float && ./stars_float | tail -n 1
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c stars_test.c -lm -o stars_fixed && ./stars_fixed | tail -n 1
echo "--- level unlock order (rules on / default off) ---"
gcc -std=c99 -O2 -Wall -DMX_LEVEL_LOCKS -I../core ../core/mx.c ../core/mx_meta.c meta_test.c -lm -o meta_locks && ./meta_locks | tail -n 1
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c meta_test.c -lm -o meta_open && ./meta_open | tail -n 1
echo "--- shared event queue: preserve win, district, surviving landlords and controls after 96 cosmetic events ---"
for mode in float fixed; do
  defs=""
  if test "$mode" = fixed; then defs="-DMX_FIXED"; fi
  gcc -std=gnu99 -O2 -Wno-unused-function $defs -I../core event_overflow_test.c -lm -o event_overflow_$mode
  ./event_overflow_$mode
done
echo "--- event-driven landlord structural-shock reactions (float, fixed, fallback-off) ---"
gcc -std=gnu99 -O2 -Wno-unused-function -I../core landlord_shock_test.c -lm -o landlord_shock_float && ./landlord_shock_float
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_FIXED -I../core landlord_shock_test.c -lm -o landlord_shock_fixed && ./landlord_shock_fixed
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_LORD_SHOCK_REACT=0 -I../core landlord_shock_test.c -lm -o landlord_shock_old && ./landlord_shock_old
echo "--- Dreamcast VMU: two-sector mock for valid saves, torn writes and missing cards ---"
gcc -std=gnu99 -O2 -Wall -Wno-unused-function -I../core ../core/mx.c ../core/mx_meta.c vmu_save_test.c -lm -o vmu_save_float && ./vmu_save_float
gcc -std=gnu99 -O2 -Wall -Wno-unused-function -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c vmu_save_test.c -lm -o vmu_save_fixed && ./vmu_save_fixed
echo "--- dual-slot progress journal: CRC, truncation, generation wrap (float and fixed) ---"
gcc -std=gnu99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c journal_test.c -lm -o journal_float && ./journal_float
gcc -std=gnu99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c journal_test.c -lm -o journal_fixed && ./journal_fixed
echo "--- save blob: layout, round trip, corruption, truncation, other level counts (float and fixed) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c save_test.c -lm -o save_float && ./save_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c save_test.c -lm -o save_fixed && ./save_fixed | grep -v "^ok:"
echo "--- outfits: unlock rules, Working Man (index 11), no Walk-in Closet / Birthday Suit deadlock ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c outfit_test.c -lm -o outfit_float && ./outfit_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c outfit_test.c -lm -o outfit_fixed && ./outfit_fixed | grep -v "^ok:"
echo "--- achievement progress bars agree with the unlock rules ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c ach_test.c -lm -o ach_float && ./ach_float | tail -n 3
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c ach_test.c -lm -o ach_fixed && ./ach_fixed | tail -n 3
echo "--- post-run crumble (aftershock): rules, cap of 80, sand untouched, blasted wood tower comes apart (float and fixed) ---"
gcc -std=c99 -O2 -Wall -Wno-unused-function -I../core crumble_test.c -lm -o crumble_float && ./crumble_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -Wno-unused-function -DMX_FIXED -I../core crumble_test.c -lm -o crumble_fixed && ./crumble_fixed | grep -v "^ok:"
echo "--- man pose (core): atan2 accuracy, flight pitch follows the path, belly-flop spin, stuck wobble matches the browser (float and fixed) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c pose_test.c -lm -o pose_float && ./pose_float | tail -n 1
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c pose_test.c -lm -o pose_fixed && ./pose_fixed | tail -n 1
echo "--- wobble / block-kind physics (fixed point): sway, snaps, topple, sand, rubber ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DKICKV=10 -I../core wobble_test.c -lm -o wobble_test && ./wobble_test
echo "--- robustness sweep (560 random shot sequences per math mode; must never hang) ---"
gcc -std=gnu99 -O2 -I../core ../core/mx.c sweep.c -lm -o sweep_float && ./sweep_float
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c sweep.c -lm -o sweep_fixed && ./sweep_fixed
echo "--- same sweep with the wobble lattice switched off (the DS falls back to this under load) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DSWEEP_NOWOB -I../core ../core/mx.c sweep.c -lm -o sweep_nowob && ./sweep_nowob
echo "--- same sweep with collapsible buildings ON (rubble sleeps as solids until the level ends; peak bodies must stay within MX_MAX_CHUNKS): DS pool of 16 in fixed point, 3DS/Dreamcast pool of 48 in float ---"
gcc -std=gnu99 -O2 -DSWEEP_COLLAPSE -I../core ../core/mx.c sweep.c -lm -o sweep_collapse_float && ./sweep_collapse_float
gcc -std=gnu99 -O2 -DMX_FIXED -DSWEEP_COLLAPSE -I../core ../core/mx.c sweep.c -lm -o sweep_collapse_fixed && ./sweep_collapse_fixed
gcc -std=gnu99 -O2 -DMX_MAX_CHUNKS=48 -DSWEEP_COLLAPSE -I../core ../core/mx.c sweep.c -lm -o sweep_collapse_48 && ./sweep_collapse_48
echo "--- collapsible buildings: off = nothing sleeps, on = rubble sleeps, set_collapse(0) wakes it, level end welds everything back (float, fixed, 48-body pool) ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c collapse_test.c -lm -o collapse_float && ./collapse_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c collapse_test.c -lm -o collapse_fixed && ./collapse_fixed | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_MAX_CHUNKS=48 -I../core ../core/mx.c collapse_test.c -lm -o collapse_48 && ./collapse_48 | grep -v "^ok:"
echo "--- solver work with / without interior-voxel contact culling (fixed point) ---"
gcc -std=gnu99 -O2 -DMX_FIXED -I../core ../core/mx.c profile_contacts.c -lm -o prof_edge && ./prof_edge
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_NO_EDGE -I../core ../core/mx.c profile_contacts.c -lm -o prof_noedge && ./prof_noedge
echo "--- towel cloth (core/cloth.c): stays finite and the right length, moves, same in float and fixed point ---"
gcc -std=gnu99 -O2 -Wall -I../core ../core/cloth.c ../core/mx.c cloth_test.c -lm -o cloth_float && ./cloth_float | grep -v "^  p"
gcc -std=gnu99 -O2 -Wall -DMX_FIXED -I../core ../core/cloth.c ../core/mx.c cloth_test.c -lm -o cloth_fixed && ./cloth_fixed | grep -v "^  p"
echo "--- DS front-end: real main loop on stub libnds, scripted stylus play on levels 1, 4, 6, 7 ---"
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -Istubs -I../core -I../art -I../ds/source ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_render
echo "--- DS actual 256x192 objective HUD and last-target locator pixels ---"
DS_OBJECTIVE_PROBE=1 ./ds_render
echo "--- DS particle edges: 8,568 frames, 14 effects, six ages, three scales, unculled pixel reference ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PARTICLE_VIEW_CULL=$mode \
    -Istubs -I../core -I../art -I../ds/source \
    ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
    ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_particle_edge_$mode
  DS_PARTICLE_EDGE_PROBE=1 ./ds_particle_edge_$mode > ds_particle_edge_$mode.txt
done
cmp ds_particle_edge_0.txt ds_particle_edge_1.txt
cat ds_particle_edge_1.txt
echo "--- DS landlords: 9,792 extreme pose/edge screenshots, 8 level frame hashes + paired CPU timings ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PROFILE_HOST -DMX_DS_CULL_TARGETS=$mode \
    -Istubs -I../core -I../art -I../ds/source \
    ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
    ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_target_cull_$mode
  DS_TARGET_CULL_PROBE=1 ./ds_target_cull_$mode > ds_target_cull_$mode.txt
done
cmp ds_target_cull_0.txt ds_target_cull_1.txt
cat ds_target_cull_1.txt
for L in 0 9 19 20 29 39 40 59; do
  DS_PERF_BENCH=1 ./ds_target_cull_0 /tmp/ds_target_ref 360 "$L" > ds_target_ref_run.txt
  DS_PERF_BENCH=1 ./ds_target_cull_1 /tmp/ds_target_opt 360 "$L" > ds_target_opt_run.txt
  grep '^DS_FRAME_HASH ' ds_target_ref_run.txt > ds_target_ref.txt
  grep '^DS_FRAME_HASH ' ds_target_opt_run.txt > ds_target_opt.txt
  cmp ds_target_ref.txt ds_target_opt.txt
  ref_us=$(sed -n 's/.*cpu_frame_mean_us=\([0-9.]*\).*/\1/p' ds_target_ref_run.txt | head -n 1)
  opt_us=$(sed -n 's/.*cpu_frame_mean_us=\([0-9.]*\).*/\1/p' ds_target_opt_run.txt | head -n 1)
  test -n "$ref_us" && test -n "$opt_us"
  awk -v lev="$((L+1))" -v ref="$ref_us" -v opt="$opt_us" 'BEGIN{
    gain=ref>0?(ref-opt)*100/ref:0
    printf "DS_TARGET_CULL_AB level=%d ref_host_us=%.2f new_host_us=%.2f gain_pct=%+.2f pixel_identical=1\n",lev,ref,opt,gain
  }'
done
echo "--- DS furniture offscreen culling: 3 HQ districts plus 8 gameplay frame-hash A/B checks ---"
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PROFILE_HOST -DMX_DS_FURNITURE_VIEW_CULL=0 \
  -Istubs -I../core -I../art -I../ds/source \
  ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
  ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_furn_reference
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PROFILE_HOST -DMX_DS_FURNITURE_VIEW_CULL=1 \
  -Istubs -I../core -I../art -I../ds/source \
  ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
  ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_furn_optimized
DS_FACADE_DISTRICT_PROBE=1 ./ds_furn_reference > ds_furn_district_reference.txt
DS_FACADE_DISTRICT_PROBE=1 ./ds_furn_optimized > ds_furn_district_optimized.txt
cmp ds_furn_district_reference.txt ds_furn_district_optimized.txt
for L in 0 9 20 21 29 39 50 59; do
  DS_PERF_BENCH=1 ./ds_furn_reference /tmp/ds_furn_ref 340 "$L" | grep '^DS_FRAME_HASH ' > ds_furn_ref.txt
  DS_PERF_BENCH=1 ./ds_furn_optimized /tmp/ds_furn_opt 340 "$L" | grep '^DS_FRAME_HASH ' > ds_furn_opt.txt
  cmp ds_furn_ref.txt ds_furn_opt.txt
  echo "DS_FURNITURE_CULL_PASS level=$((L+1)) pixel_identical=1"
done
echo "--- DS landlord walking phase: exhaustively match 1,966,080 fixed-point values to original float phase ---"
DS_WALK_PHASE_PROBE=1 ./ds_render
echo "--- DS walking phase fastpath: sampled complete gameplay frame hashes vs original for 5 levels ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PROFILE_HOST -DMX_DS_INTEGER_WALK_PHASE=$mode \
    -Istubs -I../core -I../art -I../ds/source \
    ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
    ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_walk_phase_$mode
done
for L in 0 20 29 39 59; do
  DS_PERF_BENCH=1 ./ds_walk_phase_0 /tmp/mx_walk_ref 380 "$L" | grep '^DS_FRAME_HASH ' > ds_walk_phase_ref.txt
  DS_PERF_BENCH=1 ./ds_walk_phase_1 /tmp/mx_walk_new 380 "$L" | grep '^DS_FRAME_HASH ' > ds_walk_phase_new.txt
  cmp ds_walk_phase_ref.txt ds_walk_phase_new.txt
  echo "DS_WALK_RENDER_PASS level=$((L+1)) pixel_identical=1"
done
echo "--- DS particle pool: 420-frame exhaustive 256x192 dual-view pixel parity vs full 128-slot reference ---"
DS_PARTICLE_PROBE=1 ./ds_render > ds_particle_bounded.txt
gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_PARTICLE_BOUNDS=0 -Istubs -I../core -I../art -I../ds/source ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_render_particle_reference
DS_PARTICLE_PROBE=1 ./ds_render_particle_reference > ds_particle_reference.txt
cmp ds_particle_reference.txt ds_particle_bounded.txt
cat ds_particle_bounded.txt
echo "--- DS cartridge save: a win writes the stub chip, a second boot loads it, garbage means a clean start ---"
rm -f /tmp/mx_save.sav; MX_SAVE_PATH=/tmp/mx_save.sav ./ds_render /tmp/dssave1 12000 0 | grep "^save:"
MX_SAVE_PATH=/tmp/mx_save.sav ./ds_render /tmp/dssave2 400 0 | grep "^save:" | grep "file_valid=1" > /dev/null && echo "ok: second boot reads the first boot's progress"
head -c 100 /dev/urandom > /tmp/mx_bad.sav; MX_SAVE_PATH=/tmp/mx_bad.sav ./ds_render /tmp/dssave3 400 0 | grep -c "^final" > /dev/null && echo "ok: a garbage chip starts fresh"
for L in 0 3 5 6; do ./ds_render /tmp/dsL$L 12000 $L | grep -v "^  chunk"; done
echo "--- rotated DS character parity: 3100 frames, every pose, angle, edge and jiggle condition ---"
for rot in before after; do
  flag=1
  test "$rot" = before && flag=0
  gcc -std=gnu99 -O2 -DMX_FIXED -DMX_DS_ROT_INCREMENTAL=$flag \
    -Istubs -I../core -I../art -I../ds/source \
    rot_sprite_parity_test.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o rot_sprite_$rot
  ./rot_sprite_$rot > rot_sprite_$rot.txt
done
cmp rot_sprite_before.txt rot_sprite_after.txt
cat rot_sprite_after.txt
echo "--- DS sprite contour silhouette parity, original/Wolfman, 2D/rotated ---"
for mode in 0 1; do
  gcc -std=gnu99 -O2 -DMX_FIXED -DMX_DS_MAN_VOLUME=$mode -Istubs -I../core -I../art -I../ds/source ds_sprite_volume_test.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_volume_$mode
  ./ds_volume_$mode > ds_volume_$mode.txt
  grep '^DS_MAN_OCCUPANCY_HASH ' ds_volume_$mode.txt > ds_volume_mask_$mode.txt
done
cmp ds_volume_mask_0.txt ds_volume_mask_1.txt
grep -q '^DS_MAN_CONTOUR_COUNT 0$' ds_volume_0.txt
grep -Eq '^DS_MAN_CONTOUR_COUNT [1-9][0-9]*$' ds_volume_1.txt
cat ds_volume_1.txt
echo "--- DS hardware sound ring: atomic start, no dropped kills, reserved stop headroom ---"
gcc -std=gnu99 -O2 -Wno-pointer-to-int-cast -I../ds/bare ds_sound_queue_test.c -o ds_sound_queue_test && ./ds_sound_queue_test
echo "--- DS music clock: catch up missed ARM7 VBlank without accelerating gameplay physics ---"
gcc -std=gnu99 -O2 -Istubs -I../ds/source music_clock_test.c -o music_clock_test && ./music_clock_test
echo "--- DS effects: no wasted segment pitches after missed VBlank, correct scaled glides ---"
gcc -std=gnu99 -O2 -Istubs -I../ds/source sfx_catchup_test.c -o sfx_catchup_test && ./sfx_catchup_test
echo "--- DS music: one soft PCM note at a time, every note fades, no leaked channel (a looping note that is never killed would drone forever) ---"
for L in 3 6; do ./ds_render /tmp/dsM$L 12000 $L | grep -q "music wave channels peak=[2-8] (fades=[0-9][0-9]*, bad=0)" && echo "ok: level index $L: music (melody + bass) and tone effects share the 8 PCM channels, volumes and frequencies in range" || { echo "FAIL: music channel leak or bad wave parameters (level index $L)"; exit 1; }; done
echo "--- DS outfits screen: open, walk, wear, locked swatches, back (scripted stylus + d-pad) ---"
OUTFIT_SHOTS=1 ./ds_render /tmp/dsout 400 0 | grep -v "^ok:"
echo "--- DS options screen (port 13): open, tap + d-pad, start tool pre-armed, valve hidden, collapse stays ON while overload cuts wobble work and substeps, saved and reloaded ---"
rm -f /tmp/mx_opt.sav; MX_SAVE_PATH=/tmp/mx_opt.sav OPTIONS_TEST=1 ./ds_render /tmp/dsopt 3000 0 | grep -v "^ok:"
MX_SAVE_PATH=/tmp/mx_opt.sav OPTIONS_LOAD=1 ./ds_render /tmp/dsopt2 400 0
echo "--- DS pause touchscreen: RESUME/RESTART/MENU are working, disjoint, thumb-sized touch targets ---"
PAUSE_TOUCH_TEST=1 ./ds_render /tmp/ds_pause_touch 50 0 | grep -v "^ok:"
echo "--- DS pause safety: START cancels a live stylus pull without firing, music resumes, new grab works ---"
PAUSE_DRAG_TEST=1 ./ds_render /tmp/ds_pause_drag 70 0 | grep -v "^ok:"
echo "--- DS six-slot touchscreen toolbar: valve opt-in, normal aiming, TURBO/QUAKE selection ---"
TOOLBAR_TEST=1 ./ds_render /tmp/dstool 60 0 | grep -v "^ok:"
echo "--- DS title cheat: L+R+SELECT unlocks every level for the session, in any key order ---"
CHEAT_TEST=1 ./ds_render /tmp/dscheat 400 0 | grep -v "^ok:"
echo "--- packed cutscene artwork: 15 unique readable silhouettes, no allocations ---"
gcc -std=c99 -O2 -Wall -I../ds/source cut_art_test.c -o cut_art_test && ./cut_art_test
echo "--- all 24 story/cutscene chapters: framebuffer guards, colored shots, automatic progression ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_SHARED_CUTSCENE -Dgfx_init=ds_gfx_init -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source cutscene_guard_test.c ../ds/source/gfx.c ../ds/source/intro.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o cutscene_guard_test && ./cutscene_guard_test
echo "--- cinematic transitions: real shared software renderer with monotonic wipe/iris masks ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_SHARED_CUTSCENE -Dgfx_init=ds_gfx_init -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source cutscene_transition_test.c ../ds/source/gfx.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o cutscene_transition_test && ./cutscene_transition_test
echo "--- DS opening cutscene: plays once per power-on before level 1; all 8 scenes run and hand over to level 1; A finishes the typing then goes on; SKIP and START jump to level 1; the worn outfit comes back ---"
INTRO_TEST=1 ./ds_render /tmp/dsintro 9000 0 | grep -v "^ok:"
INTRO_SKIP=1 ./ds_render /tmp/dsintro2 600 0 | grep -v "^ok:"
END_TEST=1 ./ds_render /tmp/dsend1 9000 0 | grep -v "^ok:"
END_TEST=2 ./ds_render /tmp/dsend2 9000 0 | grep -v "^ok:"
echo "--- idle gags: every step valid, every gag ends and plays, he falls asleep, input wakes him ---"
gcc -std=c99 -O2 -Wall -Wno-unused-function -I../art idle_test.c -o idle_test && ./idle_test
echo "--- DS moving rubble screen-cull parity (2/3/4 px zoom, all edges) ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -DMX_FIXED -DMX_DS_CHUNK_VIEW_CULL=$mode -Istubs -I../core -I../art -I../ds/source \
    ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../ds/source/intro.c \
    ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o ds_chunk_view_$mode
  DS_CHUNK_VIS_PROBE=1 ./ds_chunk_view_$mode > ds_chunk_view_$mode.txt
  if [ "$mode" -eq 1 ]; then
    DS_RUBBLE_DAMAGE_PROBE=1 ./ds_chunk_view_$mode
  fi
done
cmp ds_chunk_view_0.txt ds_chunk_view_1.txt
cat ds_chunk_view_1.txt
echo "--- Dreamcast front-end drawing on the host (software rasteriser instead of the PVR): jiggle, gags, win/lose ---"
for f in gfx intro; do gcc -std=gnu99 -O1 -Wall -Wno-unused-function -DMX_FIXED -DMX_SHARED_CUTSCENE -Dgfx_init=ds_gfx_init -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source -c ../ds/source/$f.c -o dc_$f.o; done   # the shared cutscene code, built exactly as dreamcast/Makefile does
gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source dc_gfx.o dc_intro.o ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c dc_render.c -lm -o dc_render && DC_OUTFIT_RECOLOR_PROBE=1 ./dc_render && DC_PRESSURE_PROBE=1 ./dc_render && DC_MENU_FLOW_TEST=1 ./dc_render && ./dc_render
echo "--- Dreamcast untouched-wall damage fastpath vs exact division reference ---"
gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -DMX_DC_FAST_PRISTINE_DAMAGE=0 \
  -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source \
  dc_gfx.o dc_intro.o ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c \
  dc_render.c -lm -o dc_damage_reference
DC_DAMAGE_FASTPATH_PROBE=1 ./dc_damage_reference > dc_damage_reference.txt
DC_DAMAGE_FASTPATH_PROBE=1 ./dc_render > dc_damage_new.txt
cmp dc_damage_reference.txt dc_damage_new.txt
cat dc_damage_new.txt
echo "--- Dreamcast particle painter: exact 640x480 pixel parity and fewer PVR submissions ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -DMX_DC_PARTICLE_BENCH=1 -DMX_DC_PARTICLE_VIEW_CULL=$mode \
    -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source \
    dc_gfx.o dc_intro.o ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c \
    dc_render.c -lm -o dc_particle_$mode
  DC_PARTICLE_VIEW_PROBE=1 ./dc_particle_$mode > dc_particle_$mode.txt
  grep '^DC_PARTICLE_VIEW_HASH ' dc_particle_$mode.txt > dc_particle_hash_$mode.txt
done
cmp dc_particle_hash_0.txt dc_particle_hash_1.txt
dc_ref=$(awk '/^DC_PARTICLE_VIEW_RASTERIZED /{print $2}' dc_particle_0.txt)
dc_new=$(awk '/^DC_PARTICLE_VIEW_RASTERIZED /{print $2}' dc_particle_1.txt)
test "$dc_new" -lt "$dc_ref"
echo "DC_PARTICLE_CULL_PASS rasterized=$dc_new reference=$dc_ref pixel_identical=1"
echo "--- Dreamcast particle active-range: tail expiry/reuse, exact pixels, 6 of 256 scans ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused-function -Wno-misleading-indentation -DMX_DC_PARTICLE_BENCH=1 -DMX_DC_PARTICLE_BOUNDS=$mode \
    -I../dreamcast/shim -Istubs -I../core -I../art -I../ds/source \
    dc_gfx.o dc_intro.o ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c \
    dc_render.c -lm -o dc_particle_bounds_$mode
  DC_PARTICLE_BOUNDS_PROBE=1 ./dc_particle_bounds_$mode > dc_particle_bounds_$mode.txt
  grep '^DC_PARTICLE_BOUNDS_HASH ' dc_particle_bounds_$mode.txt > dc_particle_bounds_hash_$mode.txt
done
cmp dc_particle_bounds_hash_0.txt dc_particle_bounds_hash_1.txt
dc_ref=$(awk '/^DC_PARTICLE_BOUNDS_SCANNED /{print $2}' dc_particle_bounds_0.txt)
dc_new=$(awk '/^DC_PARTICLE_BOUNDS_SCANNED /{print $2}' dc_particle_bounds_1.txt)
test "$dc_new" -eq 6 && test "$dc_ref" -eq 256
echo "DC_PARTICLE_BOUNDS_PASS scanned=$dc_new reference=$dc_ref pixel_identical=1"
echo "--- 3DS front-end drawing on the host (real 3ds/main.c loop, stub libctru): win-panel stars, NEW BEST!, best score on the title; slider flat and full 3D ---"
N3D_UNITS="n3d_gfx.o n3d_intro.o"
for f in gfx intro; do gcc -std=gnu99 -O1 -Wall -Wno-unused-function -DMX_FIXED -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source -c ../ds/source/$f.c -o n3d_$f.o; done   # the shared DS cutscene code, as its own fixed-point unit, exactly as 3ds/Makefile builds it
gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3ds_render
N3DS_RUBBLE_DAMAGE_PROBE=1 ./n3ds_render
N3DS_TILE_CLIP_PROBE=1 ./n3ds_render
N3DS_PRESSURE_PROBE=1 ./n3ds_render
echo "--- 3DS silhouette, outfit colors and antialiased rotated edges ---"
for config in base lit smooth; do
  case "$config" in
    base) defs="-DMX_N3DS_SPRITE_VOLUME=0 -DMX_N3DS_ROT_COVERAGE=0";;
    lit) defs="-DMX_N3DS_SPRITE_VOLUME=1 -DMX_N3DS_ROT_COVERAGE=0";;
    smooth) defs="-DMX_N3DS_SPRITE_VOLUME=1 -DMX_N3DS_ROT_COVERAGE=1";;
  esac
  gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ $defs -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3d_sprite_$config
  N3DS_SPRITE_VOLUME_PROBE=1 ./n3d_sprite_$config > n3d_sprite_$config.txt
  grep '^N3_SPRITE_OCCUPANCY ' n3d_sprite_$config.txt > n3d_sprite_coverage_$config.txt
done
cmp n3d_sprite_coverage_base.txt n3d_sprite_coverage_lit.txt
cmp n3d_sprite_coverage_lit.txt n3d_sprite_coverage_smooth.txt
grep -q 'shaded=0 ' n3d_sprite_base.txt
grep -Eq 'shaded=[1-9][0-9]* ' n3d_sprite_lit.txt
grep -Eq 'blended_rotated=[1-9][0-9]*' n3d_sprite_smooth.txt
cat n3d_sprite_smooth.txt
echo "--- 3DS tight rotation bounds: bit-identical antialiased pixels, fewer candidates ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ \
    -DMX_N3DS_ROT_WORK_COUNT=1 -DMX_N3DS_ROT_TIGHT_BOUNDS=$mode \
    -I../3ds/shim -Istubs -I../core -I../art -I../ds/source \
    n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c \
    -lm -o n3d_rot_bounds_$mode
  N3DS_ROT_TIGHT_PROBE=1 ./n3d_rot_bounds_$mode > n3d_rot_bounds_$mode.txt
  grep '^N3_ROT_TIGHT_HASH ' n3d_rot_bounds_$mode.txt > n3d_rot_hash_$mode.txt
done
cmp n3d_rot_hash_0.txt n3d_rot_hash_1.txt
rot_ref=$(awk '/^N3_ROT_TIGHT_WORK /{split($2,a,"=");print a[2]}' n3d_rot_bounds_0.txt)
rot_new=$(awk '/^N3_ROT_TIGHT_WORK /{split($2,a,"=");print a[2]}' n3d_rot_bounds_1.txt)
test "$rot_new" -lt "$rot_ref"
echo "N3_ROT_TIGHT_PASS exact_pixels=1 candidates=$rot_new reference=$rot_ref"
echo "--- 3DS four-tap sampler: bit-identical fast positive coordinates and opaque RGB ---"
for config in reference fast opaque; do
  case "$config" in
    reference) defs="-DMX_N3DS_ROT_FAST_SAMPLE=0 -DMX_N3DS_ROT_OPAQUE_SHORTCUT=0";;
    fast) defs="-DMX_N3DS_ROT_FAST_SAMPLE=1 -DMX_N3DS_ROT_OPAQUE_SHORTCUT=0";;
    opaque) defs="-DMX_N3DS_ROT_FAST_SAMPLE=1 -DMX_N3DS_ROT_OPAQUE_SHORTCUT=1";;
  esac
  gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ \
    -DMX_N3DS_ROT_WORK_COUNT=1 -DMX_N3DS_ROT_TIGHT_BOUNDS=1 $defs \
    -I../3ds/shim -Istubs -I../core -I../art -I../ds/source \
    n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3d_rot_sampler_$config
  N3DS_ROT_TIGHT_PROBE=1 ./n3d_rot_sampler_$config > n3d_rot_sampler_$config.txt
  grep '^N3_ROT_TIGHT_HASH ' n3d_rot_sampler_$config.txt > n3d_sampler_hash_$config.txt
done
cmp n3d_sampler_hash_reference.txt n3d_sampler_hash_fast.txt
cmp n3d_sampler_hash_reference.txt n3d_sampler_hash_opaque.txt
grep -Eq '^N3_ROT_SAMPLER_WORK floors=[1-9][0-9]* opaque=[1-9][0-9]*$' n3d_rot_sampler_reference.txt
grep -Eq '^N3_ROT_SAMPLER_WORK floors=0 opaque=[1-9][0-9]*$' n3d_rot_sampler_fast.txt
grep -Eq '^N3_ROT_SAMPLER_WORK floors=0 opaque=[1-9][0-9]*$' n3d_rot_sampler_opaque.txt
cat n3d_rot_sampler_opaque.txt
echo "N3_ROT_SAMPLER_PASS exact_pixels=1 no_floor_calls=1 no_bg_reads_for_opaque=1"
echo "--- 3DS stereo/bottom particle culling: pixel parity and measured raster work ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ -DMX_N3DS_PARTICLE_BENCH=1 -DMX_N3DS_PARTICLE_VIEW_CULL=$mode \
    -I../3ds/shim -Istubs -I../core -I../art -I../ds/source \
    n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3d_particle_$mode
  N3DS_PARTICLE_VIEW_PROBE=1 ./n3d_particle_$mode > n3d_particle_$mode.txt
  grep '^N3_PARTICLE_VIEW_HASH ' n3d_particle_$mode.txt > n3d_particle_hash_$mode.txt
done
cmp n3d_particle_hash_0.txt n3d_particle_hash_1.txt
n3_ref=$(awk '/^N3_PARTICLE_VIEW_RASTERIZED /{print $2}' n3d_particle_0.txt)
n3_new=$(awk '/^N3_PARTICLE_VIEW_RASTERIZED /{print $2}' n3d_particle_1.txt)
test "$n3_new" -lt "$n3_ref"
echo "N3_PARTICLE_CULL_PASS rasterized=$n3_new reference=$n3_ref pixel_identical=1"
echo "--- 3DS particle active-range: tail expiry/reuse, exact pixels, 6 of 160 scans ---"
for mode in 0 1; do
  gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ -DMX_N3DS_PARTICLE_BENCH=1 -DMX_N3DS_PARTICLE_BOUNDS=$mode \
    -I../3ds/shim -Istubs -I../core -I../art -I../ds/source \
    n3ds_render.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3d_particle_bounds_$mode
  N3DS_PARTICLE_BOUNDS_PROBE=1 ./n3d_particle_bounds_$mode > n3d_particle_bounds_$mode.txt
  grep '^N3_PARTICLE_BOUNDS_HASH ' n3d_particle_bounds_$mode.txt > n3d_particle_bounds_hash_$mode.txt
done
cmp n3d_particle_bounds_hash_0.txt n3d_particle_bounds_hash_1.txt
n3_ref=$(awk '/^N3_PARTICLE_BOUNDS_SCANNED /{print $2}' n3d_particle_bounds_0.txt)
n3_new=$(awk '/^N3_PARTICLE_BOUNDS_SCANNED /{print $2}' n3d_particle_bounds_1.txt)
test "$n3_new" -eq 6 && test "$n3_ref" -eq 160
echo "N3_PARTICLE_BOUNDS_PASS scanned=$n3_new reference=$n3_ref pixel_identical=1"
./n3ds_render /tmp/n3r0 0 | grep -v "^ok:"
./n3ds_render /tmp/n3r1 1.0 | grep -v "^ok:"
echo "--- pneumatic tubes and the hourglass (core, both math modes): a man flying into a mouth is posted out of the nozzle; the sand stands until the plug goes ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c tube_test.c -lm -o tube_float && ./tube_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c tube_test.c -lm -o tube_fixed && ./tube_fixed | grep -v "^ok:"
echo "--- 3DS man: pixel-art sprites, idle gags, hearts and sparkles, tubes (real 3ds/main.c against the stub libctru) ---"
gcc -std=gnu99 -O1 -Wall -Wno-unused -Wno-misleading-indentation -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_gags.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3ds_gags && ./n3ds_gags | grep -v "^ok:"
echo "--- chapter 2 (walk mode, bounce pads, walker / thrower / bat landlords, the fight-back switch): core in both math modes, DS and 3DS front-ends ---"
gcc -std=c99 -O2 -Wall -I../core ../core/mx.c ../core/mx_meta.c ch2_test.c -lm -o ch2_float && ./ch2_float | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core ../core/mx.c ../core/mx_meta.c ch2_test.c -lm -o ch2_fixed && ./ch2_fixed | grep -v "^ok:"
WALK_TEST=1 ./ds_render /tmp/dswalk 1400 22 | grep -v "^ok:"
WALK_TEST=3 ./ds_render /tmp/dswalk3 1400 25 | grep -v "^ok:"
WALK_TEST=2 ./ds_render /tmp/dswalk2 1400 26 | grep -v "^ok:"
gcc -std=gnu99 -O1 -Wall -Wno-unused -Wno-misleading-indentation -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_walk.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3ds_walk && ./n3ds_walk | grep -v "^ok:"
echo "--- 3DS cutscenes (shared with the DS) ---"
gcc -std=gnu99 -O1 -Wall -Wno-unused -Wno-misleading-indentation -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_intro.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3ds_intro && ./n3ds_intro | grep -v "^ok:"
echo "--- Landlord HQ: three districts in turn (float and fixed core) ---"
for m in "" "-DMX_FIXED"; do gcc -std=c99 -O2 -Wall $m -I../core ../core/mx.c hq_test.c -lm -o hq_test && ./hq_test | grep -v "^ok:"; done
echo "--- triangle span reference: original rectangle path must retain identical pixels ---"
gcc -std=gnu99 -O2 -DMX_FIXED -DMX_DS_TRI_SPAN=0 -Istubs -I../core -I../ds/source -I../art gfx_clip_test.c ../ds/source/gfx.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o gfx_tri_reference && ./gfx_tri_reference
echo "--- offscreen art culling: ellipses / triangles / dither match reference pixels ---"
gcc -std=gnu99 -O2 -DMX_FIXED -Istubs -I../core -I../ds/source -I../art gfx_clip_test.c ../ds/source/gfx.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o gfx_clip_test && ./gfx_clip_test
echo "--- text drawing is pixel-identical to the reference ---"
gcc -std=gnu99 -O2 -DMX_FIXED -Istubs -I../core -I../ds/source -I../art text_hash.c ../ds/source/gfx.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o text_hash && [ "$(./text_hash)" = "dbd2d34c803f756e" ] && echo "ok: gfx_text / gfx_text_o / gfx_text_c unchanged"
echo "--- sound effect synth (3DS + Dreamcast) ---"
gcc -std=c99 -O2 -Wall -I../core -I../ds/source snd_test.c ../core/mx_snd.c ../core/mx.c -lm -o snd_test && ./snd_test | grep -v "^ok:"
gcc -std=c99 -O2 -Wall -DMX_FIXED -I../core -I../ds/source snd_test.c ../core/mx_snd.c ../core/mx.c -lm -o snd_test_fx && ./snd_test_fx | tail -1
gcc -std=gnu99 -O1 -Wno-unused -Wno-misleading-indentation -D__3DS__ -I../3ds/shim -Istubs -I../core -I../art -I../ds/source n3ds_fur.c $N3D_UNITS ../core/mx.c ../core/mx_meta.c ../core/cloth.c ../core/mx_snd.c -lm -o n3ds_fur && ./n3ds_fur | grep -v "^ok:"
echo "--- Dreamcast chapter 2 drawing (walking man, pads, briefcases, landlord kinds) ---"
DC_WALK=1 ./dc_render | grep "walk mode"
DC_INTRO=1 ./dc_render | { grep -v " finished after " || true; } ; DC_INTRO=1 ./dc_render | grep -c " finished after " | grep -qx 2 && echo "dc cutscenes: both finish"
# Optional, slow:  SOLVE=1 bash run_tests.sh   greedy search for a winning shot sequence on every level (see level_solve.c; a "NOT solved" is not a proof)
if [ -n "$SOLVE" ]; then
  echo "--- greedy solver: which levels have a winning shot sequence in the core ---"
  gcc -std=c99 -O2 -I../core ../core/mx.c level_solve.c -lm -o level_solve && ./level_solve
fi
# Optional (needs node + playwright):  PARITY=1 bash run_tests.sh   every console level against the web game's level, cell by cell (tools/level_parity.js)
if [ -n "$PARITY" ]; then
  echo "--- level parity with the web game ---"
  gcc -std=c99 -O2 -I../core ../core/mx.c level_dump.c -lm -o level_dump && ./level_dump > /tmp/mx_c_levels.txt
  node ../tools/level_parity.js /tmp/mx_c_levels.txt 0 38
  node ../tools/level_parity.js /tmp/mx_c_levels.txt 40 59
fi
