/* Outfits: every unlock rule, the Working Man addition (index 11, always owned) and that the Walk-in Closet / Birthday Suit rules cannot deadlock. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
#include "mx_meta.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static void fill_all(MxStats *s){ mx_meta_init(s); s->kills = 100; s->cells = 20000; s->pops = 1; s->oneShots = 5; for(int i = 0; i < MX_NUM_LEVELS; i++) s->stars[i] = 3; s->ach = (1u << 20) - 1u; }
int main(void){
  MxStats S; mx_meta_init(&S);
  CHECK(MX_NUM_OUTFITS == 13 && mx_outfit(12)->name[0] == 'W' && !strcmp(mx_outfit(11)->name, "Working Man") && mx_outfit(11)->work == 1, "Working Man is outfit 11 and flagged work");
  { int w = 0; for(int i = 0; i < MX_NUM_OUTFITS; i++) w += mx_outfit(i)->work; CHECK(w == 1, "only Working Man is a work outfit"); }
  CHECK(mx_outfit(11)->br[0] == 63 && mx_outfit(11)->br[1] == 95 && mx_outfit(11)->br[2] == 143 && mx_outfit(11)->tow[0] == 201, "Working Man colours are the browser's #3f5f8f / #c9ced6");
  CHECK(mx_outfit_unlocked(&S, 0) && mx_outfit_unlocked(&S, 11), "fresh stats: Classic Red and Working Man are owned");
  { int n = 0; for(int i = 0; i < MX_NUM_OUTFITS; i++) n += mx_outfit_unlocked(&S, i); CHECK(n == 2, "fresh stats: exactly those two"); }
  S.kills = 25; CHECK(mx_outfit_unlocked(&S, 2), "25 landlords: Banana Boxers");
  mx_meta_init(&S); S.pops = 1; CHECK(mx_outfit_unlocked(&S, 3), "one air-valve pop: Mint Condition (console rule)");
  mx_meta_init(&S); S.cells = 2000; CHECK(mx_outfit_unlocked(&S, 4), "2000 blocks: Camo Dad");
  mx_meta_init(&S); S.oneShots = 1; CHECK(mx_outfit_unlocked(&S, 6), "a one-man win: Ghost Boxers");
  mx_meta_init(&S); S.ach = (1u << 20) - 1; CHECK(mx_ach_count(&S) == 20 && mx_outfit_unlocked(&S, 9), "20 achievements: Birthday Suit");
  mx_meta_init(&S); for(int i = 0; i < 10; i++) S.stars[i] = 1; CHECK(mx_outfit_unlocked(&S, 7) && !mx_outfit_unlocked(&S, 10), "10 levels beaten: Hot Pink, not Rainbow Rump yet");
  { MxStats A; fill_all(&A); int ok = 1; for(int i = 0; i < MX_NUM_OUTFITS; i++) ok &= mx_outfit_unlocked(&A, i); CHECK(ok, "a finished game owns every outfit"); }
  { MxStats A; fill_all(&A); A.ach = ((1u << MX_NUM_ACH) - 1u) & ~(1u << 20);                      /* every achievement but Walk-in Closet */
    CHECK(mx_meta_next_unlock(&A) == 20, "with everything else done, Walk-in Closet (own all 12) unlocks: no deadlock with Birthday Suit"); }
  { MxStats A; fill_all(&A); uint8_t b[MX_SAVE_MAX]; int n = mx_meta_pack(&A, 11, b, sizeof b), o = 0; MxStats B; mx_meta_init(&B);
    CHECK(n > 0 && mx_meta_unpack(&B, &o, b, n) && o == 11, "outfit 11 survives a save round trip"); }
  printf(fails ? "outfit_test: %d FAILED\n" : "outfit_test: all passed\n", fails);
  return fails != 0;
}
