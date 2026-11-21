# Mandex memory-quality pass — 2026-11-21

The goal is to free **resident** memory before raising destruction/character presentation budgets, without reducing sprite resolution, music duration, particle caps, world size or existing animation content.

## Implemented in this patch

| Port | Previous lifetime | New lifetime | Expected persistent RAM removed |
|---|---|---|---:|
| Dreamcast, human outfit source atlas | 1024 × 512 × 2-byte decoded `man_px` kept for the entire session, plus another temporary atlas when changing outfits | One temporary RLE decode/recolor/upload per outfit change, immediately freed | 1,048,576 bytes (1 MiB) |
| Dreamcast, 12.8-second music WAV staging | `static mwav[44 + MX_MUS_SAMPLES*2]` persisted for the whole session | Temporary heap buffer freed after KOS copies it to AICA RAM | 564,524 bytes (~551 KiB) |
| Nintendo 3DS, sound-effect rendering | `static short tmp[MX_SND_MAX]` persisted after SFX were copied to their DSP buffers | Direct rendering into individually right-sized DSP-linear samples | 48,510 bytes (~47 KiB) |

Dreamcast expected SH-4 *persistent storage* removed: **1,613,100 bytes (~1.54 MiB)**. This is not a claim of lower peak boot RAM: staging is transient and the Dreamcast still needs memory for the atlas upload and music synthesis. AICA/PVR usage stays the same and must be measured separately.

### Reliability and visual/audio parity

- Dreamcast outfit changes still start with the original compressed pixels. The same color transformation is applied in-place, then uploaded to the existing atlas; memory allocation failure preserves the previous valid texture.
- Dreamcast music is optional if the staging allocation fails; sound effects and gameplay remain available.
- 3DS sound effect generation uses exact sample-count preflight. If any allocation fails, already-created DSP buffers are released, then gameplay continues without broken PCM playback.
- `test/snd_test.c`: exact size and byte-for-byte direct-vs-reference SFX synthesis for float and fixed core builds.
- `test/dc_render.c` / `DC_OUTFIT_RECOLOR_PROBE=1`: alternate outfits and return to Classic Red; the atlas pointer must remain the same and the restored pixel hash must be unchanged.
- Existing DS host/emulator regression paths and browser code are not changed by this patch.

## Still to validate

1. All host tests, Nintendo DS and 3DS builds, Dreamcast KallistiOS build, and browser smoke results on the exact merged SHA.
2. 3DS DSP sound playback with real firmware, including repeated SFX and optional music.
3. Dreamcast Flycast/redream and physical AICA music playback after temporary WAV memory is freed, including returning to title and repeated outfit changes.
4. Dreamcast SH-4 heap free space before/after atlas recolors and music initialization; PVR/AICA memory headroom is separate from SH-4 system RAM.
5. Rendered FPS, simulation steps per second, image hash, crash count, and heap high-water during the worst destruction scenes. Never describe expected reclaimed bytes as measured hardware performance.

## Recommended next work (not implemented here)

- Cull off-screen fragment work before issuing Dreamcast PVR vertices; preserve image parity with the reference path.
- Reduce high-frequency transient CPU work on the DS under collapse without removing room detail, and gate it on emulator A/B frame hashes.
- Improve character turnaround and motion continuity using the same atlas dimensions, instead of increasing asset count without evidence of better silhouettes.
- Add low-memory fallbacks for nonessential effects only after confirming actual memory headroom, keeping controls, saved progress, and all sixty levels intact.
