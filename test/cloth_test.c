/* Towel cloth: stays finite, stays roughly the right length, does not explode under a hard whip, behaves the same in float and fixed. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "cloth.h"

static real RAD(float v){ return R(v); }

/* a hand path like the cape gag: turns 0 -> 180 degrees in 40 ticks, waggles, turns back, then a hard whip (60 units in 12 ticks = 5 units per tick, faster than any frame change in the gag) */
static void hand(int t, float *x, float *y, float *z){
  float th = t < 40 ? 3.14159f * t / 40 : t < 200 ? 3.14159f : t < 240 ? 3.14159f * (240 - t) / 40 : 0;
  *x = 30 * cosf(th); *y = -69; *z = -30 * sinf(th);
  if(t >= 40 && t < 200){ *x += 2.5f * sinf(t * 0.7f); }
  if(t >= 300 && t < 312) *x += 60 * (t - 299) / 12.0f;
  if(t >= 312 && t < 420) *x += 60;
  if(t >= 420 && t < 432) *x += 60 * (432 - t) / 12.0f;
}

static float stretch(const Cloth *c){                     /* worst relative error of the structural links */
  float worst = 0;
  for(int i = 0; i < CL_NX; i++) for(int j = 0; j < CL_NY; j++){
    int k = i * CL_NY + j;
    if(i + 1 < CL_NX){ int m = k + CL_NY; float d = 0; for(int a = 0; a < 3; a++){ float e = RTOF(c->x[m][a]) - RTOF(c->x[k][a]); d += e * e; } d = sqrtf(d) / 7.0f - 1; if(fabsf(d) > worst) worst = fabsf(d); }
    if(j + 1 < CL_NY){ int m = k + 1; float d = 0; for(int a = 0; a < 3; a++){ float e = RTOF(c->x[m][a]) - RTOF(c->x[k][a]); d += e * e; } d = sqrtf(d) / 9.0f - 1; if(fabsf(d) > worst) worst = fabsf(d); }
  }
  return worst;
}

int main(void){
  Cloth c; float hx, hy, hz; hand(0, &hx, &hy, &hz);
  cloth_init(&c, RAD(hx), RAD(hy), RAD(hz), 1);
  float maxd = 0, maxs = 0, maxw = 0, minz = 1e9f, maxz = -1e9f; int bad = 0;
  for(int t = 0; t < 3000; t++){
    hand(t % 600, &hx, &hy, &hz);
    cloth_step(&c, RAD(hx), RAD(hy), RAD(hz));
    for(int k = 0; k < CL_N; k++) for(int a = 0; a < 3; a++){
      float v = RTOF(c.x[k][a]);
      if(v != v || fabsf(v) > 400) bad++;
      float h = a == 0 ? hx : a == 1 ? hy : hz, dd = fabsf(v - h);
      if(dd > maxd) maxd = dd;
      if(a == 2){ if(v < minz) minz = v; if(v > maxz) maxz = v; }
    }
    float s = stretch(&c); if(t % 600 > 30 && t % 600 < 300 && s > maxs) maxs = s; if(t > 30 && s > maxw) maxw = s;
  }
  ClothQuad q[CL_NQ]; int n = cloth_quads(&c, q); int sorted = 1;
  for(int i = 1; i < n; i++) if(q[i - 1].z > q[i].z) sorted = 0;
  printf("cloth: steps=3000 bad=%d farthest-from-hand=%.1f units  worst-link-stretch: gag motion %.0f%%, hard whip %.0f%%  z-range=%.1f..%.1f  quads=%d sorted=%d\n", bad, maxd, maxs * 100, maxw * 100, minz, maxz, n, sorted);
  int ok = bad == 0 && maxd < 75 && maxs < 0.25f && maxw < 0.8f && sorted && n == CL_NQ;
  /* it must also actually move (a frozen cloth would pass everything above) */
  hand(2999 % 600, &hx, &hy, &hz);
  Cloth d = c; for(int t = 0; t < 30; t++) cloth_step(&d, RAD(hx), RAD(hy), RAD(hz));
  float mv = 0; for(int k = 0; k < CL_N; k++) mv += fabsf(RTOF(d.x[k][0]) - RTOF(c.x[k][0])) + fabsf(RTOF(d.x[k][2]) - RTOF(c.x[k][2]));
  printf("cloth: movement over 30 ticks = %.1f units (must be > 5)\n", mv);
  ok = ok && mv > 5;
  /* final shape dump so float and fixed runs can be compared by eye / script */
  for(int k = 0; k < CL_N; k += 4) printf("  p%02d % .2f % .2f % .2f\n", k, RTOF(c.x[k][0]), RTOF(c.x[k][1]), RTOF(c.x[k][2]));
  /* the back-view towel (mode 2: a horizontal bar held in the fist) must hang from the hand and stay finite */
  { Cloth e; cloth_init(&e, RAD(38), RAD(-58), 0, 2); int bad2 = 0; float far2 = 0, lo = 1e9f;
    for(int t = 0; t < 900; t++){
      float wv = 6.2832f * 2.3f * t / 60;
      cloth_step(&e, RAD(38 + 5 * sinf(wv)), RAD(-58 + 2.5f * cosf(wv)), RAD(3 * cosf(wv + 1.3f)));
      for(int k = 0; k < CL_N; k++){ float x = RTOF(e.x[k][0]), y = RTOF(e.x[k][1]), z = RTOF(e.x[k][2]);
        if(x != x || y != y || z != z || fabsf(x) > 200 || fabsf(y) > 200 || fabsf(z) > 200) bad2++;
        float dd = sqrtf((x - 38) * (x - 38) + (y + 58) * (y + 58) + z * z); if(dd > far2) far2 = dd; if(t > 120 && y < lo) lo = y; }
    }
    printf("cloth: back-view towel bad=%d farthest-from-hand=%.1f highest-point-y=%.1f (hand at -58)\n", bad2, far2, lo);
    ok = ok && bad2 == 0 && far2 < 70 && lo > -75;
  }
  puts(ok ? "cloth test OK" : "cloth test FAILED");
  return ok ? 0 : 1;
}
