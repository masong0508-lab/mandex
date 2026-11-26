/* Exhaustive random playthroughs with a phase watchdog and reproducible
 * regression seeds. A 200-second cutoff from the old short campaign was
 * not a valid total-session deadline for trials with 12 lives and 45s
 * walking per life. Keep a strict phase/life watchdog even when a legal
 * longer session is still making progress. No game timers are changed. */
#ifdef MX_FIXED
#define RS 4096.0
#else
#define RS 1.0
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mx.h"
static MxWorld w;
enum { FPS=60, ORIGINAL_LIMIT=200*FPS, LIFE_LIMIT=240*FPS };
/* These exact float-mode failures from CI run 45857763642 must finish.
 * Level numbers in the original log are one-based. */
static const int regression[][2]={
  {165,72},{170,55},{174,28},{174,58},{178,68},{180,61},
  {210,22},{210,72},{213,4},{214,36},{214,52},{218,69},{220,35}
};
static int phase_limit(int mode){
  switch(mode){
  case MAN_AIM: return 15*FPS; /* harness is always attempting a valid shot */
  case MAN_FLY: return 30*FPS; /* nine simulation seconds, slowdown margin */
  case MAN_STUCK: return 12*FPS;
  case MAN_WALK: return (3*(MX_WALK_T+15)+2)*FPS;
  case MAN_WAIT: return 15*FPS;
  case MAN_NONE: return 45*FPS; /* final rubble and result transition */
  default: return 0;
  }
}
static int runs,wins,hang,maxf,peak,peakrest,extended;
static void play(int L,int seq,int replay){
  memset(&w,0,sizeof w);mx_load(&w,L);
#ifdef SWEEP_COLLAPSE
  mx_set_collapse(&w,1);
#endif
#ifdef SWEEP_NOWOB
  mx_set_wobble(&w,0);
#endif
  const int lives=w.men;
  /* Original stages retain their original global limit. For new trials
   * allow the real life budget, not a blanket relaxation of phase limits. */
  const int budget=L<MX_TRIAL_START?ORIGINAL_LIMIT:
      (lives+1)*(w.walk?LIFE_LIMIT:60*FPS)+45*FPS;
  unsigned r=1234u+seq*7919u+L*104729u;
  int f,shot=0,done=0,phase=w.man.mode,phase_at=0,life_at=0,last_men=w.men,last_district=w.district;
  const char *reason="session budget";
  for(f=0;f<budget;f++){
    if(w.state==MX_PLAY&&w.man.mode==MAN_AIM&&w.men>0&&f>30){
      r=r*1664525u+1013904223u;double a=(12+((r>>16)%40))*3.14159265/180;
      r=r*1664525u+1013904223u;double p=7+((r>>16)%46)/10.0;
      int before=w.men;
      mx_set_pull(&w,(real)(-p*cos(a)*RS),(real)(p*sin(a)*RS));mx_release(&w);
      if(w.men<before)shot++;
    }
    if(w.state==MX_PLAY&&w.man.mode==MAN_WALK){
      r=r*1664525u+1013904223u;
      if(((r>>16)&31)==0){int ax=(int)((r>>8)%3)-1;mx_walk_input(&w,ax,(r>>12)&1,(r>>13)&1,(r>>14)%5==0);}
      if(((r>>10)&511)==0)mx_walk_quit(&w);
    }
    if(w.state==MX_PLAY&&w.man.mode==MAN_FLY){r=r*1664525u+1013904223u;if((r>>20)%90==0)mx_dive(&w);}
    mx_update(&w,R(1.0/60.0));
    int a=0,s=0;
    for(int c=0;c<MX_MAX_CHUNKS;c++){a+=w.chunks[c].active;s+=w.chunks[c].active&&w.chunks[c].rest;}
    if(a>peak)peak=a;if(s>peakrest)peakrest=s;
    if(w.state!=MX_PLAY){done=1;wins+=w.state==MX_WIN;break;}
    if(w.man.mode!=phase){phase=w.man.mode;phase_at=f;}
    if(w.men!=last_men||w.district!=last_district){last_men=w.men;last_district=w.district;life_at=f;}
    int limit=phase_limit(phase);
    if(!limit||f-phase_at>limit){reason="phase watchdog";break;}
    if(f-life_at>LIFE_LIMIT){reason="life watchdog";break;}
  }
  runs++;if(f>maxf)maxf=f;
  if(done&&f>=ORIGINAL_LIMIT)extended++;
  if(replay||!done)
    printf("%s L%d seq%d frames=%d shots=%d men=%d mode=%d targets=%d/%d phase_age=%d life_age=%d reason=%s\n",
      done?"REPLAY_PASS":"HANG",L+1,seq,f+1,shot,w.men,w.man.mode,mx_targets_alive(&w),w.ntargets,f-phase_at,f-life_at,done?(w.state==MX_WIN?"win":"lose"):reason);
  if(!done)hang++;
}
int main(int argc,char **argv){
  if(argc==2&&!strcmp(argv[1],"--replay")){
    for(unsigned i=0;i<sizeof regression/sizeof regression[0];i++)play(regression[i][0]-1,regression[i][1],1);
  }else if(argc==1){
    for(int L=0;L<MX_NUM_LEVELS;L++)for(int seq=0;seq<80;seq++)play(L,seq,0);
  }else if(argc==3){
    int l=atoi(argv[1]),seq=atoi(argv[2]);
    if(l<1||l>MX_NUM_LEVELS||seq<0||seq>=80)return 2;
    play(l-1,seq,1);
  }else {fprintf(stderr,"usage: sweep [--replay | level sequence]\n");return 2;}
  printf("runs=%d wins=%d hangs=%d longest=%d frames extended_sessions=%d (most bodies at once %d of %d, most asleep %d)\n",runs,wins,hang,maxf,extended,peak,MX_MAX_CHUNKS,peakrest);
  return hang!=0;
}
