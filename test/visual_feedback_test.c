/* Cosmetic continuity and bounded impact levels (float + DS fixed point).
 * No gameplay state/score/save changes are permitted from these helpers. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static int failures;
#define CK(c,msg) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",msg);failures++;}}while(0)
static int near(real a,double b){double d=(double)RTOF(a)-b;return d<.005&&d>-.005;}
int main(void){
  MxMan m;memset(&m,0,sizeof m);
  m.mode=MAN_WALK;m.gr=1;m.vx=RI(9);m.face=1;
  const double expect[]={0.0,.15,.3,.15,0.0,.15,.3,.15,0.0};
  for(int k=0;k<9;k++){
    m.gait=R(k*.5);
    if(k==8)m.gait=0; /* core wraps gait from four cells back to zero */
    CK(near(mx_walk_foot_lift(&m),expect[k]),"walk lift must ease both planted footsteps");
  }
  m.gait=RI(3);
  CK(mx_walk_anim_phase(&m)==3,"existing sprite stride order is unchanged");
  /* Twelve held atlas offsets turn through the BACK, never through
     a face/body mirror pop or new geometry. Reverse input remains instant. */
  { static const int offsets[12]={1,1,2,3,4,5,6,5,4,3,2,1};
    m.face=-1; m.mode=MAN_WALK;
    for(int tick=12;tick>=1;tick--){
      m.turn_ticks=(uint8_t)tick;
      CK(mx_walk_turn_offset(&m)==offsets[12-tick],"turn frames follow atlas rear arc");
      CK(mx_walk_render_face(&m)==(tick>6?1:-1),"render side swaps behind the shoulders");
      CK(mx_walk_foot_lift(&m)==0,"feet remain planted while the body turns");
    }
    m.turn_ticks=11;
    CK(mx_walk_turn_offset(&m)==1, "first visible turn frame advances only 15 degrees");
    m.turn_ticks=0;
    CK(mx_walk_turn_offset(&m)==0 && mx_walk_render_face(&m)==-1,
       "back turn resolves into the requested side view");
  }
  /* Punch silhouette should ease forward then recover on the original
     timer; it must not move the real player body or turn frames. */
  m.mode=MAN_WALK;m.turn_ticks=0;m.pT=R(0.2);
  CK(near(mx_walk_punch_lunge(&m),0), "punch begins without a teleport");
  m.pT=R(0.15);
  CK(near(mx_walk_punch_lunge(&m),0.15), "punch anticipates at quarter-time");
  m.pT=R(0.1);
  CK(near(mx_walk_punch_lunge(&m),0.3), "punch reaches bounded forward lunge");
  m.pT=R(0.05);
  CK(near(mx_walk_punch_lunge(&m),0.15), "punch recovers smoothly");
  m.pT=0;
  CK(near(mx_walk_punch_lunge(&m),0), "punch returns to exact body anchor");
  m.pT=R(0.1);m.turn_ticks=4;
  CK(mx_walk_punch_lunge(&m)==0, "rear turn cannot slide while punching");
  m.turn_ticks=0;m.mode=MAN_FLY;
  CK(mx_walk_punch_lunge(&m)==0, "airborne animation does not inherit grounded lunge");
  m.mode=MAN_WALK;m.pT=0;
  m.vx=0;CK(mx_walk_foot_lift(&m)==0,"stationary foot lift disappears");
  m.vx=RI(9);m.gr=0;CK(mx_walk_foot_lift(&m)==0,"airborne lift stays anchored");
  m.gr=1;m.mode=MAN_FLY;CK(mx_walk_foot_lift(&m)==0,"non-walk modes do not bob");
  m.face=1;
  m.vx=R(-1);mx_flight_face_update(&m);CK(m.face==1,"near-zero reverse motion does not flip face");
  m.vx=RI(-3);mx_flight_face_update(&m);CK(m.face==-1,"meaningful reverse flight turns face");
  m.vx=R(.5);mx_flight_face_update(&m);CK(m.face==-1,"flight direction remains stable through zero");
  m.vx=RI(3);mx_flight_face_update(&m);CK(m.face==1,"meaningful forward flight turns face");
  const int counts[]={0,1,7,8,23,24,47,48,80},tiers[]={0,0,0,1,1,2,2,3,3};
  for(unsigned i=0;i<sizeof counts/sizeof counts[0];i++)
    CK(mx_crumble_tier(RI(counts[i]))==tiers[i],"collapse severity is bounded and deterministic");
  if(failures)return 1;
  puts("VISUAL_FEEDBACK_PASS continuous_grounded_steps=1 flight_face_hysteresis=1 collapse_tiers=4");
  return 0;
}
