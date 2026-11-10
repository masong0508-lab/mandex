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
void mx_meta_win(MxStats *s, const MxWorld *w);        /* when state becomes MX_WIN (once) */
int  mx_meta_next_unlock(MxStats *s);                  /* newly met achievement index or -1 */

#define MX_NUM_ACH 25
const char *mx_ach_name(int i);
const char *mx_ach_desc(int i);

#define MX_NUM_OUTFITS 11
typedef struct { const char *name, *hint; uint8_t br[3], brd[3], tow[3], tows[3]; uint8_t rainbow; } MxOutfit;
const MxOutfit *mx_outfit(int i);
int mx_outfit_unlocked(const MxStats *s, int i);
int mx_ach_count(const MxStats *s);
/* the air valve is an unlockable: beat MX_VALVE_UNLOCK_LEVELS levels.  Front-ends copy this into world.valve_ok. */
#define MX_VALVE_UNLOCK_LEVELS 3
int mx_valve_unlocked(const MxStats *s);
/* the six palette colours an outfit changes: boxers, boxers dark, boxers highlight, towel, towel stripes, towel shade */
void mx_outfit_shades(int i, uint8_t out[6][3]);
#endif
