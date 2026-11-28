# Secret City follow-up

- Next-level actions require actual browser victory, including the six-sector qualifier and 18-sector finale. The completed qualifier is skipped during subsequent sequential play; it remains replayable by selection.
- Native D-pad level stepping jumps across the hidden interval directly; unfinished-trial search checks only visible byte ranges and preserves save state. Browser resume uses equivalent contiguous ranges, avoiding repeated hidden-level checks. Tested extracted browser selector against the previous implementation over 8,850 comparisons and native jump algorithm against a reference over 6,660 cases.
- DS/DSi, 3DS and Dreamcast display chapter-relative part numbering and a persistent unlock message on the qualifying victory screen. Browser also displays an unlock notice there.
- No save format, authored geometry, collision, physics or entity budget changes. Full console build/CI verification remains required.
