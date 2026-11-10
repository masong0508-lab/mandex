/* mx_ach_progress must agree with the real unlock rules: for every counter achievement, cur >= target exactly when it is unlocked. */
#include <stdio.h>
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
  printf(fails ? "ach_test: %d FAILED\n" : "ach_test: all passed\n", fails);
  return fails != 0;
}
