/* Host-side regression test: runs scripted shots through the shared core.
 * Build twice (float = Dreamcast math, -DMX_FIXED = DS math) and compare. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "mx.h"

static MxWorld W;
#define DT R(1.0/60.0)

static int fire(int level, int deg, int dive_after, int verbose){
  mx_load(&W, level);
  for(int i = 0; i < 120; i++) mx_update(&W, DT);           /* settle banner etc. */
  double a = deg * 3.14159265 / 180.0;
  mx_set_pull(&W, R(-11.5 * cos(a)), R(11.5 * sin(a)));
  mx_release(&W);
  int maxch = 0, c0 = mx_cells_left(&W), f;
  for(f = 0; f < 60 * 12; f++){
    if(dive_after && f == dive_after) mx_dive(&W);
    mx_update(&W, DT);
    int n = 0; for(int c = 0; c < MX_MAX_CHUNKS; c++) n += W.chunks[c].active;
    if(n > maxch) maxch = n;
  }
  if(verbose)
    printf("  L%d %2ddeg%s: destroyed=%3d maxChunks=%2d landlords %d/%d  score=%d\n", level + 1, deg,
           dive_after ? "+dive" : "     ", c0 - mx_cells_left(&W), maxch,
           W.ntargets - mx_targets_alive(&W), W.ntargets, W.score);
  return c0 - mx_cells_left(&W);
}

int main(void){
  int fails = 0;
  printf("%s build, sizeof(MxWorld)=%zu bytes\n",
#ifdef MX_FIXED
         "FIXED 20.12",
#else
         "FLOAT",
#endif
         sizeof(MxWorld));
  for(int l = 0; l < MX_NUM_LEVELS; l++){
    mx_load(&W, l);
    int ch = 0; for(int c = 0; c < MX_MAX_CHUNKS; c++) ch += W.chunks[c].active;
    for(int i = 0; i < 180; i++) mx_update(&W, DT);          /* level must stand on its own */
    int cells_after = mx_cells_left(&W);
    int ch2 = 0; for(int c = 0; c < MX_MAX_CHUNKS; c++) ch2 += W.chunks[c].active;
    mx_load(&W, l);
    int cells0 = mx_cells_left(&W);
    printf("level %d '%s': cells=%d targets=%d chunks_at_load=%d, stable after 3s: %s\n", l + 1, mx_level_name(l),
           cells0, W.ntargets, ch, (cells_after == cells0 && ch2 == 0) ? "yes" : "NO");
    if(ch || cells_after != cells0 || ch2) fails++;
  }
  static const int degs[] = {18, 26, 34, 44};
  for(int l = 0; l < MX_NUM_LEVELS; l++){
    for(int d = 0; d < 4; d++) fire(l, degs[d], 0, 1);
    fire(l, 40, 35, 1);
  }
  if(fails){ printf("FAILED %d\n", fails); return 1; }
  printf("OK\n");
  return 0;
}
