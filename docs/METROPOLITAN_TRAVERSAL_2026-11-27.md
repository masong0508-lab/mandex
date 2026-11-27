# Metropolitan traversal — late-campaign pilot

## Scope
The verified starting point is main at 22b9572: 660 selectable stages, not the proposed 1,110-stage campaign. The final 60 authored layouts (stage IDs 600–659, zero-based) are the three phases of 20 existing circuits. Preserve all earlier layouts, IDs, objectives, save format, phase rules and runtime entity limits. No additional resident world, collision code or art allocation.

## Actual changes
- 135 open ground-floor exterior entrances; reinforced destructible lintels.
- 45 new skybridges and 15 existing bridges opened at their building landings (60 connected layouts).
- 60 additional ground-to-deck lift tubes using existing transfer physics; no new teleport subsystem.
- Stage tips teach the new route. Existing roof pads, targets and material-span checks remain.
- Regenerated editable districts 28–30, native header and standalone browser data from the shared generator. Packed command bytes: 223783 -> 221558 (-2225).

## Verification
Executed the exact generator and browser pack regression in an isolated JavaScript environment using generated files in memory. New production walker regression failed against the old maps (blocked exterior door), then passed 135 entrances, 60 bridges, 60 transfers and 8,267 clearance samples. All 600 trial layouts round-tripped through the embedded browser decoder; first 540 trial layouts unchanged; native FNV digest baseline matched all 600 previous expectations. HTML script parses. Added native actual transfer checks to existing fixed/float test; native build, complete CI and physical playthrough remain required. Clearance and immediate tube arrival do not prove every stage is solvable.

## Remaining campaign / city-to-mountain design
Use the existing fixed-size simulation grid as a bounded active sector, with explicit entry and exit landmarks. The proposed 1,110th level needs additional campaign generation, save-budget and district navigation work and an actual large finale. Do not label any existing circuit as that finale. Connect metropolitan towers through multiple aligned floors, transit shafts and rooftop walkways, then continue the same visual materials and skyline into increasingly terraced foothills and a destructible summit. Preserve recognizable district silhouettes and landmark positions across sector boundaries; keep transitions spatially matched rather than disconnected loading menus. Measure true hardware frame time, memory and complete traversal before increasing active-sector dimensions or pool caps.
