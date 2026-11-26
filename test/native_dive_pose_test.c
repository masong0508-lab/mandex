/* Shared native animation regression: use the actual core in both math
 * modes, including its real dive input and pose step. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../core/mx.c"
static void pose_check(int ok,const char *why){
  if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}
}
int main(void){
  static MxWorld w; MxMan *m=&w.man;
  m->mode=MAN_FLY;m->ang=R(-.35);m->vx=RI(-50);m->vy=RI(-15);
  mx_dive(&w);
  pose_check(m->spin==m->ang,"dive starts at existing flight pitch");
  pose_check(fabsf(RTOF(m->vx)+20)<.02f && fabsf(RTOF(m->vy)-97.5f)<.02f,"unchanged dive impulse");
  const real start=m->spin;man_pose(m,R(.01));
  pose_check(m->spin>start&&m->spin-start<R(.04),"initial angular acceleration");
  for(int i=1;i<20;i++)man_pose(m,R(.01));
  const real fine=m->spin;
  memset(&w,0,sizeof w);m=&w.man;m->mode=MAN_FLY;m->ang=R(-.35);m->vx=RI(-50);m->vy=RI(-15);mx_dive(&w);
  man_pose(m,20*R(.01));
  pose_check(fabsf(RTOF(m->spin-fine))<.025f,"partition-independent ramp");
  mx_dive(&w);pose_check(fabsf(RTOF(m->spin-fine))<.025f,"repeated dive input cannot restart animation");
  real p=m->spin;man_pose(m,R(.03));
  pose_check(fabsf(RTOF(m->spin-p-rmul(R(.03),RI(14))))<.003f,"settled tumble rate unchanged");
  m->mode=MAN_STUCK;man_pose(m,R(.03));pose_check(m->spin==0&&m->ang==0,"landing clears airborne pose");
  puts("NATIVE_DIVE_POSE_PASS");return 0;
}
