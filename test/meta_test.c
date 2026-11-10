/* Level unlock order. Built twice by run_tests.sh: with -DMX_LEVEL_LOCKS (rules on) and without (default: everything open). */
#include <stdio.h>
#include "mx.h"
#include "mx_meta.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
int main(void){
  MxStats S; mx_meta_init(&S);
#ifdef MX_LEVEL_LOCKS
  CHECK(mx_level_unlocked(&S, 0) && mx_level_unlocked(&S, 1) && mx_level_unlocked(&S, 2), "fresh stats: levels 1-3 open");
  CHECK(!mx_level_unlocked(&S, 3), "fresh stats: level 4 locked");
  S.stars[2] = 1; CHECK(mx_level_unlocked(&S, 3), "beating level 3 opens level 4");
  CHECK(!mx_level_unlocked(&S, 4), "level 5 still locked");
  { MxStats U; mx_meta_init(&U); U.bestScore = 99999; CHECK(!mx_level_unlocked(&U, 3), "bestScore alone does not unlock"); }
  { MxStats U; mx_meta_init(&U); U.unlock_all = 1; int ok = 1; for(int i = 0; i < MX_NUM_LEVELS; i++) ok &= mx_level_unlocked(&U, i); CHECK(ok, "unlock_all opens every level"); }
#else
  { int ok = 1; for(int i = 0; i < MX_NUM_LEVELS; i++) ok &= mx_level_unlocked(&S, i); CHECK(ok, "default build: every level open"); }
#endif
  printf(fails ? "meta_test: %d FAILED\n" : "meta_test: all passed\n", fails);
  return fails != 0;
}
