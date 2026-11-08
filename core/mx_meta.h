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
/* the six palette colours an outfit changes: boxers, boxers dark, boxers highlight, towel, towel stripes, towel shade */
void mx_outfit_shades(int i, uint8_t out[6][3]);
#endif
