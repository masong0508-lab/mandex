/* Man pose (core): ratan2 accuracy, flight pitch eases toward the path, belly-flop spin, stuck wobble decays.  float and fixed. */
#include <stdio.h>
#include <math.h>
#include "mx.h"
#ifdef MX_FIXED
#define F(x) RTOF(x)
#else
#define F(x) (x)
#endif
static MxWorld w;
static int fails;
#define CHECK(c, ...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)
int main(void){
  double worst = 0;
  for(int i = 0; i < 360; i++){
    double a = (i - 180) * 3.14159265358979 / 180.0 + 0.0123;
    double got = (double)F(ratan2(R(sin(a) * 7), R(cos(a) * 7)));
    double d = fabs(got - a); if(d > 3.14159265358979) d = fabs(d - 2 * 3.14159265358979);
    if(d > worst) worst = d;
  }
  printf("ratan2 worst error %.4f rad\n", worst);
  CHECK(worst < 0.012, "ratan2 error %.4f", worst);
  CHECK(F(ratan2(0, 0)) == 0, "atan2(0,0)");

  mx_load(&w, 0); w.state = MX_PLAY;
  w.man.mode = MAN_AIM; w.man.x = MX_ANCHOR_X - RI(8); w.man.y = MX_ANCHOR_Y + RI(1);
  mx_release(&w);
  CHECK(w.man.mode == MAN_FLY, "launch");
  double a0 = (double)F(w.man.ang);
  CHECK(a0 < 0, "launched upward: ang %.3f should be negative (nose up)", a0);
  double want = 0.6 * atan2((double)F(w.man.vy), fabs((double)F(w.man.vx)));
  CHECK(fabs(a0 - want) < 0.02, "launch ang %.3f vs %.3f", a0, want);
  double peak = 0; int rose = 0;
  for(int f = 0; f < 50 && w.man.mode == MAN_FLY; f++){
    mx_update(&w, R(1.0 / 60.0));
    double a = (double)F(w.man.ang); if(a > peak) peak = a;
    if(a > a0 + 0.05) rose = 1;
  }
  CHECK(rose, "ang should swing toward nose-down as the arc falls (start %.3f, peak %.3f)", a0, peak);
  if(w.man.mode == MAN_FLY){
    mx_dive(&w);
    double s0 = (double)F(w.man.spin);
    for(int f = 0; f < 30 && w.man.mode == MAN_FLY; f++) mx_update(&w, R(1.0 / 60.0));
    double s1 = (double)F(w.man.spin);
    printf("dive spin %.2f -> %.2f rad in 0.5 s\n", s0, s1);
    CHECK(s1 - s0 > 5.5 && s1 - s0 < 7.5, "spin 14 rad/s: got %.2f", s1 - s0);
  }
  /* stuck wobble: compare with the browser's formula */
  MxMan m = w.man; m.mode = MAN_STUCK; double worstw = 0;
  for(int i = 0; i < 150; i++){
    double t = i / 60.0; m.t = R(t);
    double ref = sin(t * 26) * exp(-t * 3.2), got = (double)F(mx_man_wobble(&m));
    if(fabs(ref - got) > worstw) worstw = fabs(ref - got);
  }
  printf("wobble worst error vs browser %.3f\n", worstw);
  CHECK(worstw < 0.05, "wobble error %.3f", worstw);
  m.mode = MAN_FLY; CHECK(F(mx_man_wobble(&m)) == 0, "no wobble in flight");
  /* landing: every ground touch emits EV_LAND (m = 1 for a skip); a skip also shakes the screen; the last one is the sticking landing */
  mx_load(&w, 0); w.state = MX_PLAY;
  for(int shot = 0; shot < 3; shot++){
    mx_load(&w, 0); w.state = MX_PLAY; w.men = 3;
    w.man.mode = MAN_AIM; w.man.x = MX_ANCHOR_X - RI(10) + RI(shot); w.man.y = MX_ANCHOR_Y + RI(7 - shot * 3);
    mx_release(&w);
    int lands = 0, skips = 0, last_m = -1; double shake_at_skip = 0;
    for(int f = 0; f < 60 * 8 && w.man.mode == MAN_FLY; f++){
      mx_update(&w, R(1.0 / 60.0));
      for(int i = 0; i < w.nev; i++) if(w.ev[i].type == EV_LAND){ lands++; last_m = w.ev[i].m; if(w.ev[i].m){ skips++; shake_at_skip = (double)F(w.shake); } }
    }
    printf("landing test %d: %d ground touches, %d skips, man mode %d\n", shot, lands, skips, w.man.mode);
    CHECK(w.man.mode != MAN_FLY, "shot %d never came down", shot);
    if(lands == 0) continue;                                    /* it hit a building and stuck without touching the ground: fine */
    CHECK(lands >= skips && lands <= skips + 1, "shot %d: %d touches but %d skips (only the last touch may be a non-skip)", shot, lands, skips);
    if(lands == skips + 1) CHECK(last_m == 0, "shot %d: the final touch must be the sticking one", shot);
    if(skips) CHECK(shake_at_skip >= 3.0, "shot %d: skip shake %.2f, want >= 4", shot, shake_at_skip);
  }
  printf(fails ? "pose_test: %d FAILED\n" : "pose_test: all ok\n", fails);
  return fails != 0;
}
