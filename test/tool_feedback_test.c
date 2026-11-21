#include <stdio.h>
#include <string.h>
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
  CHECK(strcmp(mx_tool_name(PW_QUAKE),"QUAKE")==0 &&
        strcmp(mx_tool_status_label(MX_SHOT_DAZED),"DAZED - AIM LOCKED")==0,
        "clear shared text labels");
  CHECK(strcmp(mx_tool_effect(PW_BEANS),"MID-AIR JET")==0 &&
        strcmp(mx_tool_effect(PW_QUAKE),"LANDING SHOCKWAVE")==0 &&
        strcmp(mx_tool_effect(PW_NONE),"")==0,
        "tooltips explain real powers only when equipped");
  if(bad)return 1;
  puts("TOOL_FEEDBACK_PASS modes=8 pull_threshold=exact gauge=0..100 stock=read-only invalid_ids=guarded");
  return 0;
}
