#include <stdio.h>
#include <string.h>
#include "mx_meta.h" /* Cross-header compile guard: MX_TOOL_* != PW_* */
#include "mx_tool_feedback.h"
static MxWorld w;
static int bad;
#define CHECK(expr,msg) do{if(!(expr)){fprintf(stderr,"FAIL: %s\n",msg);bad++;}}while(0)
int main(void){
  memset(&w,0,sizeof w);
  w.state=MX_PLAY;w.man.mode=MAN_AIM;w.man.x=MX_ANCHOR_X;w.man.y=MX_ANCHOR_Y;
  MxToolFeedback u=mx_tool_feedback(&w);
  CHECK(u.phase==MX_SHOT_IDLE,"resting sling is uncharged");
  CHECK(mx_sling_power_pct(&w)==0,"empty pull has zero launch strength");
  CHECK(u.selected==PW_NONE && u.remaining==0,"none selected");
  w.pw_sel=PW_TURBO;w.pw_left[PW_TURBO]=1;
  u=mx_tool_feedback(&w);
  CHECK(u.remaining==1 && u.selected==PW_TURBO,"stock is from real per-level inventory");
  CHECK(w.pw_left[PW_TURBO]==1,"drawing a tooltip never consumes a power");
  w.man.x=MX_ANCHOR_X-RI(2);
  u=mx_tool_feedback(&w);
  CHECK(u.phase==MX_SHOT_PULL,
        "sling readiness mirrors the exact launch threshold");
  CHECK(mx_sling_power_pct(&w)>0 && mx_sling_power_pct(&w)<100,
        "partial pull has an intermediate true launch-strength reading");
  w.man.x=MX_ANCHOR_X-MX_PULL_MAX;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_PULL,
        "full pull remains marked launch-ready");
  CHECK(mx_sling_power_pct(&w)==100,"full pull fills the shared gauge");
  w.man.stun=RI(1);
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_DAZED,"dazed status takes precedence");
  CHECK(mx_sling_power_pct(&w)==0,"unavailable dazed shot does not display a live meter");
  w.man.stun=0;w.man.seated=1;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_VALVE,"seated valve does not suggest release");
  CHECK(mx_sling_power_pct(&w)==0,"seated valve cannot misreport sling tension");
  w.man.seated=0;w.man.vlv=1;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_VALVE,"valve latch blocks shot prompt");
  w.man.vlv=0;w.man.mode=MAN_FLY;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_FLY,"flying shows midair action");
  w.man.mode=MAN_WALK;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_WALK,"walking shows foot controls");
  w.man.mode=MAN_WAIT;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_RECOVER,"respawn never shows shoot now");
  w.state=MX_WIN;
  CHECK(mx_tool_feedback(&w).phase==MX_SHOT_FINISH,"finished level has no action prompt");
  CHECK(mx_sling_power_pct(&w)==0,"finished level cannot show a launch meter");
  CHECK(mx_tool_stock(&w,-1)==0 && mx_tool_stock(&w,PW_COUNT)==0 &&
        mx_tool_stock(&w,PW_NONE)==0,"invalid stock indices safe");
  w.pw_left[PW_BEANS]=-10;
  CHECK(mx_tool_stock(&w,PW_BEANS)==0,"broken negative stock never displayed");
  CHECK(strcmp(mx_power_name(PW_QUAKE),"QUAKE")==0 &&
        strcmp(mx_power_name(PW_NONE),"NONE")==0 &&
        strcmp(mx_power_name(PW_COUNT),"NONE")==0 &&
        strcmp(mx_tool_status_label(MX_SHOT_DAZED),"DAZED - AIM LOCKED")==0,
        "clear shared text labels");
  CHECK(strcmp(mx_tool_effect(PW_BEANS),"MID-AIR JET")==0 &&
        strcmp(mx_tool_effect(PW_QUAKE),"LANDING SHOCKWAVE")==0 &&
        strcmp(mx_tool_effect(PW_NONE),"")==0,
        "tooltips explain real powers only when equipped");
  /* Test the actual core launch, not only the read-only HUD. A selected
     power is never spent on a short pull; a successful launch spends once. */
  mx_load(&w,0);
  mx_select_power(&w,PW_TURBO);
  CHECK(w.pw_sel==PW_TURBO && w.pw_left[PW_TURBO]==1,
        "fresh level permits one turbo charge");
  mx_set_pull(&w,RI(-1),0);
  mx_release(&w);
  CHECK(w.man.mode==MAN_AIM && w.pw_sel==PW_TURBO &&
        w.pw_left[PW_TURBO]==1,
        "short release retains selected power and stock");
  mx_set_pull(&w,RI(-8),RI(2));
  mx_release(&w);
  CHECK(w.man.mode==MAN_FLY && w.man.pwr==PW_TURBO &&
        w.pw_left[PW_TURBO]==0 && w.pw_sel==PW_NONE,
        "valid launch spends one turbo charge and disarms it");

  /* A stale/depleted selection should launch normally without an ability. */
  mx_load(&w,0);
  w.pw_sel=PW_BEANS;w.pw_left[PW_BEANS]=0;
  mx_set_pull(&w,RI(-8),RI(2));
  mx_release(&w);
  CHECK(w.man.mode==MAN_FLY && w.man.pwr==PW_NONE &&
        w.pw_left[PW_BEANS]==0 && w.pw_sel==PW_NONE &&
        w.st_pw[PW_BEANS]==0,
        "depleted power cannot be launched or decremented below zero");
  for(int n=0;n<2;n++){
    mx_load(&w,0);
    w.pw_sel=n?PW_COUNT+37:-1;
    int before[PW_COUNT];memcpy(before,w.pw_left,sizeof before);
    mx_set_pull(&w,RI(-8),RI(2));
    mx_release(&w);
    CHECK(w.man.mode==MAN_FLY && w.man.pwr==PW_NONE &&
          w.pw_sel==PW_NONE &&
          memcmp(before,w.pw_left,sizeof before)==0,
          "invalid selected power cannot touch the inventory array");
  }
  if(bad)return 1;
  puts("TOOL_FEEDBACK_PASS modes=8 pull_threshold=exact gauge=0..100 stock=read-only invalid_ids=guarded launch_stock=guarded");
  return 0;
}
