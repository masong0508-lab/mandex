/* Landlord HQ is three 120-cell districts played in turn: loading, clearing one builds the next with +4 men and +3000, the third ends the level.
   gcc -std=c99 -O2 -I../core ../core/mx.c hq_test.c -lm -o hq_test   (also with -DMX_FIXED) */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static MxWorld w;
static int fails;
#define CHECK(c, msg) do{ if(c) printf("ok: %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } }while(0)
static int run_until(int type, int frames){ for(int f = 0; f < frames; f++){ mx_update(&w, R(1.0/60)); for(int i = 0; i < w.nev; i++) if(w.ev[i].type == type) return f; } return -1; }
int main(void){
  static const int NT[3] = { 12, 12, 17 };
  mx_load(&w, MX_HQ_LEVEL);
  for(int d = 0; d < MX_HQ_DISTRICTS; d++){
    char m[96];
    snprintf(m, sizeof m, "district %d has %d landlords (%d)", d + 1, NT[d], w.ntargets); CHECK(w.district == d && w.ntargets == NT[d], m);
    int c0 = mx_cells_left(&w);
    for(int f = 0; f < 120; f++) mx_update(&w, R(1.0/60));
    snprintf(m, sizeof m, "district %d stands by itself (%d cells, %d after 2 s)", d + 1, c0, mx_cells_left(&w)); CHECK(mx_cells_left(&w) >= c0 * 97 / 100 && mx_targets_alive(&w) == NT[d], m);
    int right = 0; for(int y = 0; y < MX_ROWS; y++) for(int x = MX_COLS - 1; x >= 0; x--) if(w.grid[y * MX_COLS + x]){ if(x > right) right = x; break; }
    snprintf(m, sizeof m, "district %d fits the grid (rightmost cell %d of %d)", d + 1, right, MX_COLS - 1); CHECK(right <= MX_COLS - 2, m);
    int men = w.men, score = w.score;
    for(int i = 0; i < w.ntargets; i++) w.targets[i].alive = 0;
    if(d < MX_HQ_DISTRICTS - 1){
      int f = run_until(EV_DISTRICT, 400);
      snprintf(m, sizeof m, "clearing district %d builds the next one (after %d frames)", d + 1, f); CHECK(f >= 0 && w.district == d + 1 && w.state == MX_PLAY, m);
      snprintf(m, sizeof m, "...with +%d men (%d -> %d) and +3000 (%d -> %d)", MX_HQ_BONUS_MEN, men, w.men, score, w.score); CHECK(w.men == men + MX_HQ_BONUS_MEN && w.score == score + 3000, m);
      CHECK(w.man.mode != MAN_NONE || w.men > 0, "...and the man is ready at the sling");
    } else {
      int f = run_until(EV_WIN, 400); CHECK(f >= 0 && w.state == MX_WIN, "clearing the last district wins the level");
    }
  }
  mx_load(&w, 0); CHECK(w.district == 0, "other levels are district 0");
  puts(fails ? "hq_test: FAILED" : "hq_test: all passed"); return fails != 0;
}
