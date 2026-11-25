# Demolition contracts — cross-port expansion

72 contracts: 18 districts with Open, Bank, Chain and Guard variants. New districts include Pier Crossing (destructible bridge pier), Furnace Row (steel barrier with rubber cap), Sand Quarry, Signal Station, Skyline Court and Breakwater Works.

Web: Play > Contracts. Nintendo DS/DSi, 3DS and Dreamcast: level selector, D-pad up/down jumps between chapters and four-contract district groups; left/right selects a stage. All contracts are open separately from campaign completion. IDs 0–59 stay unchanged; contracts use 60–131 natively and their own ID namespace in browser saves. Ending the campaign still returns to the title instead of silently starting a contract.

Native level records use the existing variable-length version-2 payload (132 levels, 736 bytes; 748-byte journal). Legacy 60-level saves continue to load; new fields default to zero. Dreamcast uses two alternating two-block files rather than one-block files and detects legacy record lengths from the validated header. DS EEPROM and 3DS SD use the expanded payload. Native contract stars and best scores persist; contract stars are excluded from campaign star totals and contract wins are excluded from campaign win counts.

The shared native and browser blast loops skip empty space while retaining damage order. The new layouts reuse existing material, room, TNT and enemy simulation. Automated validation checks browser generation and raw native grid/target fingerprints, and tests old-save migration and VMU recovery.

This is not yet the full 3 MB expansion. Physical handheld testing, balancing, richer assets and integration with concurrent portrait/performance work remain outstanding.
