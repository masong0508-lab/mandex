# Mandex: cross-platform improvement waves

Status: November 20, 2026.  All changes below were committed directly to
`main`; no feature branch needs to be merged.  The tested/released status is
different from "source code written"—do not conflate them.

## Wave A: implemented in source (current)

- **Landlords react to structural damage** in the shared C core: explosions,
  breaking joints and strong chunk impacts trigger panic/evasive movement for
  nearby active targets.  The feature uses existing enemy fields, no added
  per-frame world scans, and has a compile-time A/B rollback:
  `MX_LORD_SHOCK_REACT=0`.
- **Browser parity**: blast, snapping and heavy-impact events trigger matching
  `lordShock` responses.  The browser already has mobile controls and a
  creator; those are not being claimed as new features in this wave.
- **3DS progress saving**: two alternating SD files beneath
  `sdmc:/3ds/mandex/`, containing the existing v2 progress payload inside a
  generation-numbered, CRC-checked journal.  Save at completion and menu
  changes, not inside normal gameplay frames.  Previous valid slot survives
  a damaged new write.
- **Dreamcast VMU progress saving (experimental)**: two 512-byte files under
  `/vmu/a1/`, generation/CRC journal as above; no VMU attached means progress
  still works for the current session.  Needs VMU/emulator acceptance checks,
  BIOS save-manager metadata polish and hardware testing.
- **3DS memory / music startup**: synthesize the existing DSP theme directly
  into its linear-allocated buffer, removing an entire PCM-sized duplicate
  allocation and memory copy.
- **Dreamcast AICA RAM startup**: synthesize into aligned WAV data buffers
  directly, removing separate PCM scratch arrays and copies.
- **Audible large rubble impacts**: fixed `EV_THUD` strength selection in both
  shared DSP/AICA event sound routing and DS SFX routing.  Previously the
  sound handler read the unused event `b` value rather than `a`.
- **Scoped destruction dust**: tiny fixed-count smoke/dust bursts for DS and
  3DS severe impacts, cracking and crumbling; existing particle pools still
  cap the cost.
- **Automated safeguards added**: float/fixed point journal corruption
  checks, panic-event tests with old behavior fallback, sound event
  correctness and a VMU mock that simulates torn writes and card absence.

## Current gating

Run GitHub Actions on the latest main SHA, not a stale branch:
- Host tests: `test/run_tests.sh`; includes real DS/3DS/Dreamcast host harnesses
  plus new save/AI/SFX regressions.
- Console ROMs: `.github/workflows/roms.yml` combines NDS, 3DSX, CDI and
  standalone HTML into `mandex-roms`.  Console compilation success is **not**
  hardware proof.
- Pixel parity: `.github/workflows/render-fastpaths.yml`.
- DS emulated FPS / bottlenecks:
  `.github/workflows/ds-emulator-fps.yml` and
  `.github/workflows/ds-fps-audit.yml`.
- Validate SD saves via emulator or physical 3DS; validate VMU file creation,
  reload and behavior if unplugged in Flycast and on original Dreamcast.
- Measure actual rendered FPS and **simulation steps/s** separately under
  worst-case destruction.  Never claim 30 FPS from host microbenchmarks.

## Next production waves (NOT implemented by Wave A)

1. **Destruction presentation**: scalable debris variants, floor-by-floor
   collapse choreography, dynamic environmental dust and readable impact
   timing, without DS framebuffer allocation or high-cost per-frame lighting.
2. **Boss behavior**: deliberate multi-stage landlord encounters, defensible
   counterplay and stage-specific puzzle hazards; keep enemies reachable and
   all 60 original levels completable.
3. **Progress parity**: real-device save recovery, VMU icon/menu polish,
   DS cartridge journal fallback where save chip permits, achievement/outfit
   usability consistency and portable save migration.
4. **Replay and trick shots**: input/event stream recording, deterministic
   reset/replay and slow-motion presentation, strict fixed-size buffers on
   memory-limited consoles.  Do not promise pixel-identical replays until
   both numeric backends are characterized.
5. **Browser creator/sandbox**: improve existing editor stability, sharing,
   mobile usability, structural feedback, reproducible challenges and
   performance/FX quality controls.
6. **Frame rate and audio stability**: performance budgets for the heaviest
   3D rooms, touch zoom, Dreamcast particle drawing and dual-eye 3DS;
   independent audio timing and measured degradation under load.
7. **Platform polish**: richer yet accurate stereo-depth 3DS rendering,
   Dreamcast control feedback, clean DS touch affordances and consistent
   screen-space effects.

No wave is "done" merely because a commit exists.  Build artifacts, host
regressions, visual parity and emulator/hardware tests are distinct gates.
