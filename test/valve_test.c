/* Air valve + power-up rules: hold on the valve -> inflates, bursts at ~3.6 s and costs a man; let go on the valve -> stays seated;
   drag off -> fires slower with a bigger radius; beans / towel / splat are one use per level. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mx.h"
#include "mx_meta.h"
static MxWorld W;
#define DT R(0.0166667)
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static void run(int n){ for(int i = 0; i < n; i++) mx_update(&W, DT); }
int main(void){
  mx_load(&W, 0);
  int men0 = W.men;
  /* 0: the valve is an unlockable and never grabs a pull-back */
  { MxStats S; mx_meta_init(&S);
    CHECK(!mx_valve_unlocked(&S), "valve locked on a fresh save");
    S.stars[0] = 1; S.stars[1] = 1; CHECK(!mx_valve_unlocked(&S), "still locked after 2 levels");
    S.stars[2] = 1; CHECK(mx_valve_unlocked(&S), "unlocked after beating 3 levels"); }
  CHECK(!mx_valve_seat(&W) && !W.man.vlv && !W.man.seated, "locked: seating is refused");
  { int grabbed = 0; double best = 1e9;
    for(int a = 0; a < 360; a += 3) for(int r = 1; r <= 4; r++){
      double k = RTOF(MX_PULL_MAX) * r / 4, c = cos(a * 3.14159265 / 180), sn = sin(a * 3.14159265 / 180);
      mx_set_pull(&W, R(k * c), R(k * sn)); grabbed |= W.man.vlv | W.man.seated; }
    CHECK(!grabbed, "no pull-back anywhere in the sling's reach grabs the valve");
    for(int a = 0; a < 360; a++){ double px = RTOF(MX_ANCHOR_X) + RTOF(MX_PULL_MAX) * cos(a * 3.14159265 / 180) - RTOF(MX_VALVE_X),
                                         py = RTOF(MX_ANCHOR_Y) + RTOF(MX_PULL_MAX) * sin(a * 3.14159265 / 180) - RTOF(MX_VALVE_Y);
      double d = sqrt(px * px + py * py); if(d < best) best = d; }
    CHECK(best > 1.5, "a full pull-back stays clear of the valve nozzle"); }
  mx_set_pull(&W, 0, 0);
  W.valve_ok = 1;
  CHECK(mx_valve_seat(&W) && W.man.vlv == 1 && W.man.seated == 1, "unlocked: seating puts him on the valve");
  mx_load(&W, 0);
  CHECK(W.valve_ok == 1, "the unlock survives a level reload");
  /* 1: hold on the valve until it bursts */
  CHECK(mx_valve_seat(&W) && W.man.vlv == 1, "seated on the valve");
  run(60 * 2);
  CHECK(W.man.inf > R(0.4) && W.man.inf < R(0.7), "about 55% inflated after 2 s");
  run(60 * 3);
  CHECK(W.st_pops == 1 && W.men == men0 - 1, "held too long: burst, one man lost");
  run(60 * 2);
  CHECK(W.man.mode == MAN_AIM && W.man.inf == 0, "next man arrives deflated");
  /* 2: let go on the valve: stays seated and keeps filling */
  mx_valve_seat(&W);
  mx_release(&W);
  CHECK(W.man.seated == 1 && W.man.mode == MAN_AIM, "release on the valve = seated, not fired");
  run(60);
  CHECK(W.man.inf > R(0.2), "seated man keeps inflating");
  /* 3: drag him off and fire inflated */
  mx_set_pull(&W, -R(3), R(5));
  CHECK(W.man.vlv == 0 && W.man.seated == 0, "dragging off the valve unseats him");
  mx_release(&W);
  CHECK(W.man.mode == MAN_FLY && W.man.rad > R(1.9), "fires with a bigger radius");
  /* 4: powers are one use per level */
  mx_load(&W, 1);
  mx_select_power(&W, PW_TOWEL);
  CHECK(W.pw_sel == PW_TOWEL, "arm towel");
  mx_set_pull(&W, -R(3), R(5)); mx_release(&W);
  CHECK(W.man.pwr == PW_TOWEL && W.pw_left[PW_TOWEL] == 0 && W.pw_sel == 0, "towel consumed on launch");
  for(int i = 0; i < 60 * 8 && W.man.mode != MAN_AIM; i++) mx_update(&W, DT);
  mx_select_power(&W, PW_TOWEL);
  CHECK(W.pw_sel == 0, "towel can't be armed twice");
  mx_select_power(&W, PW_BEANS);
  mx_set_pull(&W, -R(3), R(5)); mx_release(&W);
  run(20); real vx0 = W.man.vx; mx_dive(&W);
  CHECK(W.man.beaned && W.man.vx > vx0 && !W.man.dive, "beans jet speeds him up instead of diving");
  printf(fails ? "VALVE TEST FAILED\n" : "valve test passed\n");
  return fails != 0;
}
