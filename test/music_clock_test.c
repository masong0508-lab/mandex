/* ARM7 VBlank catch-up: music should keep its beat after a slow visual frame.
   Exercises the real DS sound sequencer independently of rendering. */
#include <stdio.h>
#include <stdlib.h>
#include "nds.h"
static int next_id=200, live[8], faults, fade_calls;
void soundEnable(void){}
int soundPlayPSG(int d,u16 f,u8 v,u8 p){(void)d;(void)f;(void)v;(void)p;return -1;}
int soundPlayNoise(u16 f,u8 v,u8 p){(void)f;(void)v;(void)p;return -1;}
int soundPlayWave(const signed char *cycle,u16 f,u8 v,u8 p){
  (void)cycle;(void)f;(void)v;(void)p;
  for(int i=0;i<8;i++)if(!live[i]){live[i]=1;return 200+i;}
  faults++;return -1;
}
void soundSetFreq(int id,int hz){(void)id;(void)hz;}
void soundSetVol(int id,u8 v){(void)v; if(id<200||id>=208||!live[id-200])faults++; fade_calls++;}
void soundKill(int id){if(id>=200&&id<208)live[id-200]=0;}
#include "../ds/source/sfx.c"
int main(void){
  sfx_init();music_set(1);
  for(int i=0;i<MUS_STEP_FRAMES*3;i+=3)sfx_tick_steps((MUS_STEP_FRAMES*3-i)<3 ? (MUS_STEP_FRAMES*3-i) : 3);
  if(mus_step!=3 || mus_t!=0){fprintf(stderr,"song tempo lost: step=%d t=%d\n",mus_step,mus_t);return 1;}
  music_set(1);int prev_fades=fade_calls;sfx_tick_steps(50);
  if(mus_t!=8){fprintf(stderr,"catch-up overran IPC budget: %d\n",mus_t);return 1;}
  if(fade_calls-prev_fades>2){fprintf(stderr,"catch-up flooded IPC: %d repeated envelopes\n",fade_calls-prev_fades);return 1;}
  music_set(0);if(mus_on||faults||!fade_calls){fprintf(stderr,"audio channels fault=%d fade=%d\n",faults,fade_calls);return 1;}
  puts("music clock: passed - 3 bars caught up, capped backlog, no channel leaks");return 0;
}
