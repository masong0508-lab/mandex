/* mx_ach_progress must agree with the real unlock rules: for every counter achievement, cur >= target exactly when it is unlocked. */
#include <stdio.h>
#include <limits.h>
#include "mx.h"
#include "mx_meta.h"
static int fails, checked;
int main(void){
  static const int V[] = {0,1,2,3,4,5,9,10,11,19,20,24,25,26,99,100,101,1999,2000,2001,5000,19999,20000,20001,59999,60000,60001};
  for(unsigned vi = 0; vi < sizeof V / sizeof *V; vi++) for(int n = 0; n <= MX_NUM_LEVELS; n++) for(int stars = 1; stars <= 3; stars += 2){
    MxStats S; mx_meta_init(&S);
    int v = V[vi];
    S.kills = S.cells = S.booms = S.maxChain = S.flops = S.oneShots = S.bestScore = S.shots = v;
    for(int l = 0; l < n; l++) S.stars[l] = (uint8_t)stars;
    while(mx_meta_next_unlock(&S) >= 0) {}
    for(int i = 0; i < MX_NUM_ACH; i++){
      int cur, tg;
      if(!mx_ach_progress(&S, i, &cur, &tg)) continue;
      checked++;
      if((cur >= tg) != mx_ach_unlocked(&S, i)){ if(fails < 10) printf("FAIL: ach %d (%s) cur=%d target=%d unlocked=%d (v=%d n=%d)\n", i, mx_ach_name(i), cur, tg, mx_ach_unlocked(&S, i), v, n); fails++; }
    }
  }
  { MxStats S; mx_meta_init(&S); int cur, tg; int counters = 0;
    for(int i = 0; i < MX_NUM_ACH; i++) counters += mx_ach_progress(&S, i, &cur, &tg);
    if(counters != 18){ printf("FAIL: expected 18 counter achievements, got %d\n", counters); fails++; } }
  /* Large imported progress must not wrap negative or lose unlocks. */
  {
    static MxWorld W;
    MxStats S; mx_meta_init(&S);
    S.kills = S.cells = S.booms = S.flops = S.shots = INT_MAX - 1;
    S.wins = S.oneShots = S.inflated = S.pops = INT_MAX - 1;
    S.pw[1] = INT_MAX - 1;
    S.lv_booms = S.lv_shots = INT_MAX - 1;
    W.nev = 8;
    for(int k = 0; k < 2; k++){
      W.ev[4*k+0].type = EV_CELL;
      W.ev[4*k+1].type = EV_BOOM;
      W.ev[4*k+2].type = EV_LAUNCH;
      W.ev[4*k+3].type = EV_TARGET;
    }
    W.man.mode = MAN_FLY; W.man.dive = 1;
    W.st_inflated = W.st_pops = W.st_pw[1] = 5;
    mx_meta_events(&S, &W);
    if(S.kills != INT_MAX || S.cells != INT_MAX || S.booms != INT_MAX ||
       S.flops != INT_MAX || S.shots != INT_MAX || S.inflated != INT_MAX ||
       S.pops != INT_MAX || S.pw[1] != INT_MAX || S.lv_shots != INT_MAX ||
       S.lv_booms != INT_MAX || S.maxChain != INT_MAX){
      puts("FAIL: saturating lifetime and level stats"); fails++;
    }
    W.st_inflated = W.st_pops = W.st_pw[1] = 0; W.nev = 0;
    mx_meta_events(&S, &W);
    if(S.inflated != INT_MAX || S.pops != INT_MAX || S.pw[1] != INT_MAX){
      puts("FAIL: world counter resets reduced lifetime totals"); fails++;
    }
    W.st_inflated = W.st_pops = W.st_pw[1] = 2;
    mx_meta_events(&S, &W);
    W.level = -1; W.men = 2; W.score = 100;
    mx_meta_win(&S, &W);
    if(S.wins != INT_MAX || S.oneShots != INT_MAX){
      puts("FAIL: win totals wrapped"); fails++;
    }
    /* Save round-trip remains compatible with the existing v2 format. */
    uint8_t save[MX_SAVE_MAX]; MxStats loaded; int outfit = -1;
    mx_meta_init(&loaded);
    int len = mx_meta_pack(&S, 0, save, sizeof save);
    if(!len || !mx_meta_unpack(&loaded, &outfit, save, len) ||
       loaded.kills != INT_MAX || loaded.wins != INT_MAX || loaded.pw[1] != INT_MAX){
      puts("FAIL: saturated stats save round-trip"); fails++;
    }
  }
  printf(fails ? "ach_test: %d FAILED\n" : "ach_test: all passed\n", fails);
  return fails != 0;
}
