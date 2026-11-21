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
