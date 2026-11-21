# Mandex: Power-up inventory and shot-readiness UI

## What changed (production code)

The original touch toolbars showed the five one-use abilities as lit or greyed out, without exposing each one's actual remaining stock. Dreamcast described the selection but used a generic "x1" hint, and browser status could render an undefined/negative stock string. Moreover, the sling kept saying "drag back" while the shot was charged, the character was dazed, or the air valve was engaged.

### New player-visible behavior

- **Nintendo DS:** every one-use ability button shows its live remaining uses (including 0 after it is spent). The existing POWER bar now uses the shared exact sling-strength calculation and goes to zero while air-valve-seated or stunned. The lower screen shows a stable `DRAG BACK TO AIM` or `RELEASE TO LAUNCH` caption, or the appropriate `AIR FILLING - DRAG OFF` / `DAZED - AIM LOCKED` state. When a power is selected, a second short line describes its effect.
- **Nintendo 3DS:** the six-option lower touch toolbar retains the exact same input targets; the five consumable options now show their remaining counts. A new **bottom-screen sling-strength bar** uses the same 0–100% power reading as DS, without affecting stereo top-screen render calls. Above the toolbar, the selected power's effect is named. The aim caption changes based on the real sling status.
- **Dreamcast:** the existing bottom HUD now shows `X: POWER <NAME> x<N>` using the real per-level stock, with a new **0–100% launch-strength meter**, a separate action-ready status, and a power effect line. When no power is selected, it indicates `X: POWER NONE (CYCLE)` rather than implying every power is always available.
- **Web (portrait and landscape):** a narrow shot-readiness strip sits above the existing tool selector, using the browser's real pull threshold, stun, and valve flags. The existing selected power effect tooltip remains. The selected stock badge now clamps undefined or negative values to zero.

### Shared semantics

`core/mx_tool_feedback.h` is a header-only, stateless projection of the existing game state. Selecting powers does **not** spend a charge; `mx_release()` spends the selected charge only on a successful launch. Readiness uses the identical squared displacement threshold (`R(3)`) from `mx_release()`, not a guessed charge percentage. No `sqrt` runs in the common status snapshot: only ports drawing an actual meter call `mx_sling_power_pct()` once when the aiming HUD is visible (DS's existing gauge is reused rather than doubled).

Short power descriptions refer to actual mechanics: BEANS → mid-air jet, TOWEL → glide, SPLAT → goo on impact, TURBO → faster launch, QUAKE → landing shockwave. The air valve is a reusable unlockable interaction, not a consumable, and deliberately displays no count.

### Engineering and validation

- All changes are draw-only. No new textures, sprite atlas frames, framebuffer, dynamic allocations, inventory fields, save-format changes, simulation branches, collision/movement changes, or independent RNG calls.
- `test/tool_feedback_test.c` exercises normal/charged sling, actual partial/full power 0–100% calculation, stock, valve, stun, fly, walk, recovery, finished state, and guard behavior in both floating-point and fixed-point modes.
- `test/browser_tool_feedback_test.js` executes the real browser readiness helper in isolation, checks playable/nonplayable modes and shot threshold, and verifies the complete browser script still parses.
- Both are part of `test/run_tests.sh`. The DS CI build must also compile successfully. The 3DS/Dreamcast HUD placement still requires screenshot and actual hardware/emulator checks; no console FPS improvement is claimed.

### Cross-port build correction

The `PW_*` inventory identifiers and `MX_TOOL_*` starting-option identifiers do not have identical numeric values: `MX_TOOL_VALVE` occupies an option index but is not consumable inventory. The original `mx_tool_name()` is declared externally by `mx_meta.h` for options. This UI now uses a distinct, header-only `mx_power_name()` for live `PW_*` inventory labels, so the DS/Dreamcast/3DS translation units can include both headers without linkage conflicts or mistakenly labeling a selected power. The host feedback test includes both public headers to catch future collisions. This correction is UI-only; it does not change any save indices, charges or simulation behavior.
