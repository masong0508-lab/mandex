/* Robustness sweep: 560 pseudo-random shot sequences (7 levels x 80) with random power/angle/belly-flops.
   Fails (exit 1) if any level can never end - the soft-lock class we hit with creeping chunks. */
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
int main(void){
  int hang=0, runs=0, wins=0, maxf=0;
  for(int L=0; L<MX_NUM_LEVELS; L++) for(int seq=0; seq<80; seq++){
    memset(&w,0,sizeof w); mx_load(&w,L);
#ifdef SWEEP_COLLAPSE
    mx_set_collapse(&w,1);
#endif
#ifdef SWEEP_NOWOB
    mx_set_wobble(&w,0);
#endif
    unsigned r=1234u+seq*7919u+L*104729u; int f, shot=0, done=0;
    for(f=0; f<12000; f++){
      if(w.state==MX_PLAY && w.man.mode==MAN_AIM && w.men>0 && f>30){
        r=r*1664525u+1013904223u; double a=(12+((r>>16)%40))*3.14159265/180; r=r*1664525u+1013904223u; double p=7+((r>>16)%46)/10.0;
        mx_set_pull(&w,(real)(-p*cos(a)*RS),(real)(p*sin(a)*RS)); mx_release(&w); shot++;
      }
      if(w.state==MX_PLAY && w.man.mode==MAN_FLY){ r=r*1664525u+1013904223u; if((r>>20)%90==0) mx_dive(&w); }
      mx_update(&w,R(1.0/60.0));
      if(w.state!=MX_PLAY){ done=1; wins+=w.state==MX_WIN; break; }
    }
    runs++; if(f>maxf) maxf=f;
    if(!done){ hang++; int nch=0; for(int c=0;c<MX_MAX_CHUNKS;c++) nch+=w.chunks[c].active; printf("HANG L%d seq%d men=%d mode=%d chunks=%d state=%d targets=%d/%d loseT=%.2f winT=%.2f waitT=%.2f freeze=%d slow=%.2f time=%.0f\n",L+1,seq,w.men,w.man.mode,nch,w.state,mx_targets_alive(&w),w.ntargets,RTOF(w.loseT),RTOF(w.winT),RTOF(w.waitT),w.freeze,RTOF(w.slow),RTOF(w.time)); }
  }
  printf("runs=%d wins=%d hangs=%d longest=%d frames\n",runs,wins,hang,maxf);
  return hang != 0;
}
