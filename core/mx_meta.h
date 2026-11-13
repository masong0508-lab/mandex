/* Mandex meta layer shared by the DS, 3DS and Dreamcast front-ends: session stats, achievements, outfits.
   (The HTML game keeps these in localStorage; the consoles have no save, so progress lasts for the session.)
   Level creator is HTML only. */
#ifndef MX_META_H
#define MX_META_H
#include <stdint.h>
#include "mx.h"

typedef struct {
  int kills, cells, booms, maxChain, shots, flops, wins, oneShots, bestScore, inflated, pops, pw[4];
  uint8_t stars[MX_NUM_LEVELS];     /* best stars per level, 0 = not beaten */
  int unlock_all;                   /* cheat, session only: every level open (never saved) */
  uint32_t ach;                     /* bit i = achievement i unlocked */
  uint32_t best[MX_NUM_LEVELS];     /* best score per level, 0 = not played; updated by mx_meta_win (the front-ends no longer keep their own) */
  /* per-level, reset by mx_meta_level_start */
  int lv_booms, lv_shots;
  int seen_st_inflated, seen_st_pops, seen_pw[4];   /* last values read from the world */
} MxStats;

void mx_meta_init(MxStats *s);
void mx_meta_level_start(MxStats *s, const MxWorld *w);
/* call once per frame after mx_update: reads the world's events/counters.  Returns the index of a newly unlocked achievement, or -1;
   call again until -1 to drain several. */
void mx_meta_events(MxStats *s, const MxWorld *w);
int  mx_level_unlocked(const MxStats *s, int level);   /* levels 1-3 open, then each opens when the one before is beaten; always 1 unless built with -DMX_LEVEL_LOCKS */
int  mx_stars_for(const MxWorld *w);                   /* 3 stars with 2+ men left, 2 with one, else 1: the one formula every front-end uses */
void mx_meta_win(MxStats *s, const MxWorld *w);        /* when state becomes MX_WIN (once): stars, wins, one-shots, bestScore and best[level].  Read the old best[level] BEFORE calling if you want a NEW BEST! test. */
int  mx_meta_next_unlock(MxStats *s);                  /* newly met achievement index or -1 */

#define MX_NUM_ACH 25
const char *mx_ach_name(int i);
const char *mx_ach_desc(int i);

#define MX_NUM_OUTFITS 13     /* appended, never inserted: the save blob stores the outfit as an index */
typedef struct { const char *name, *hint; uint8_t br[3], brd[3], tow[3], tows[3]; uint8_t rainbow, work; } MxOutfit;   /* work: the plain Working Man look (blue briefs; the browser also drops the gold logo, the console sprites cannot yet) */
const MxOutfit *mx_outfit(int i);
int mx_outfit_unlocked(const MxStats *s, int i);
int mx_ach_count(const MxStats *s);
/* 1 if achievement i is a counter (fills *cur / *target for a progress bar such as 12/25), 0 if it is a one-off.  Mirrors met() in mx_meta.c. */
int mx_ach_progress(const MxStats *s, int i, int *cur, int *target);
static inline int mx_ach_unlocked(const MxStats *s, int i){ return (int)((s->ach >> i) & 1); }
/* the air valve is an unlockable: beat MX_VALVE_UNLOCK_LEVELS levels.  Front-ends copy this into world.valve_ok. */
#define MX_VALVE_UNLOCK_LEVELS 3
int mx_valve_unlocked(const MxStats *s);
/* the six palette colours an outfit changes: boxers, boxers dark, boxers highlight, towel, towel stripes, towel shade */
void mx_outfit_shades(int i, uint8_t out[6][3]);

/* ---- options (port 13): kept by each front-end, saved in the blob (version 2).  Default = off / air valve, the browser's defaults. ---- */
enum { MX_TOOL_NONE, MX_TOOL_VALVE, MX_TOOL_BEANS, MX_TOOL_TOWEL, MX_TOOL_SPLAT, MX_NUM_TOOLS };   /* appended, never inserted (stored as an index); Burpbuster is slot 5 when port 14 lands */
typedef struct { uint8_t collapse, tool, nofight; } MxOptions;   /* collapse: collapsible buildings; tool: the start tool (MX_TOOL_*); nofight: 1 = landlords just stand there (default 0: they fight back) */
void mx_options_default(MxOptions *o);
const char *mx_tool_name(int tool);                      /* "NONE", "AIR VALVE", "BEANS", "TOWEL", "SPLAT" */
int mx_tool_power(int tool);                             /* the PW_* a start tool pre-arms before every launch, or PW_NONE (none / air valve) */

/* ---- save blob (port 5): identical bytes from the float and fixed builds, no struct memcpy, no malloc ----
   all fields little-endian:
     0   "MXS1"                       magic
     4   version (2; version 1 blobs are still read: they have no options byte and load the default options)
     5   nlev                         number of levels the writer had (stars[] and best[] have this many entries)
     6   11 x u32  kills cells booms maxChain shots flops wins oneShots bestScore inflated pops
     50  4 x u32   pw[0..3]
     66  nlev x u8 stars[]
         u32       ach
         u8        outfit index
         nlev x u32 best[]
         u8        options (version 2 only): bit 0 collapse, bits 1-3 start tool (MX_TOOL_*), bit 4 landlords-fight-back OFF (0 = on, so older saves load the default), bits 5-7 reserved (written 0, ignored)
         u32       CRC32 (IEEE) of every byte before it
   20 levels = 176 bytes (175 for a version 1 blob).  Not saved: unlock_all and the per-level transients (lv_*, seen_*).  A save with fewer or more levels than this build
   still loads (missing levels read as 0, extra ones are dropped); any other layout change must bump the version byte. */
#define MX_SAVE_VERSION 2
#define MX_SAVE_MAX (66 + MX_NUM_LEVELS + 4 + 1 + 4 * MX_NUM_LEVELS + 1 + 4)
uint32_t mx_crc32(const uint8_t *p, int n);
/* returns the number of bytes written, or 0 if cap is too small */
int mx_meta_pack(const MxStats *s, int outfit, uint8_t *out, int cap);                                  /* = mx_meta_pack_opt with the default options */
int mx_meta_pack_opt(const MxStats *s, int outfit, const MxOptions *opt, uint8_t *out, int cap);       /* opt NULL = defaults; a tool index out of range is written as the air valve */
/* returns 1 and fills *s and *outfit, or returns 0 and leaves both untouched (wrong magic, unknown version, short buffer, CRC mismatch).  len may be longer than the
   blob (a chip read of a fixed size); bytes after it are ignored.  Counters are clamped to INT_MAX, stars to 3, achievement bits to the ones that exist and an
   outfit index this build does not have becomes 0.  The transients are cleared and unlock_all is left alone.  Call it BEFORE the first load_level(): that call
   (mx_meta_level_start) is what re-bases the seen_* deltas, so a loaded game cannot count old inflations or pops twice. */
int mx_meta_unpack(MxStats *s, int *outfit, const uint8_t *in, int len);                                /* = mx_meta_unpack_opt, options ignored */
/* opt may be NULL.  On success *opt is the saved options (version 2) or the defaults (version 1); on failure it is untouched like the rest. */
int mx_meta_unpack_opt(MxStats *s, int *outfit, MxOptions *opt, const uint8_t *in, int len);
#endif
