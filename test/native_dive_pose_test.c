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
  /* The rear-view accent must be bounded, time-based and never alter velocity,
     body position, pressure, spin, hitboxes, score or save state. */
  m->mode=MAN_FLY;m->inf=0;m->rear_t=MX_REAR_ROLL_T;int peak=0,shown=0,last=0;
  for(int k=0;k<30;k++){
    int o=mx_rear_roll_offset(m);pose_check(o>=0&&o<=6,"valid atlas offset");
    if(o>peak)peak=o;if(o)shown++;
    pose_check(abs(o-last)<=2,"adjacent yaw samples");last=o;
    man_pose(m,R(1.0/60.0));
  }
  pose_check(peak==6&&shown>=18&&m->rear_t==0,"full rear hold and expiry");
  m->rear_t=R(.2);m->inf=R(.5);pose_check(!mx_rear_roll_offset(m),"pressure silhouette takes priority");
  m->inf=0;m->mode=MAN_STUCK;man_pose(m,R(.02));pose_check(!m->rear_t&&!mx_rear_roll_offset(m),"no rear roll while stuck");
  puts("NATIVE_DIVE_POSE_PASS");return 0;
}
