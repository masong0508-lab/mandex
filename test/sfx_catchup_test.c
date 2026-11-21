/* Real DS effect sequencer: catch-up frames must not enqueue expired
   pitches.  The starting pitch used by the glide must match the scaled
   hardware note (especially the noise channel). */
#include <stdio.h>
#include <string.h>
#include "nds.h"
static int initial_noise, initial_wave, last_pitch, pitch_calls;
void soundEnable(void){}
int soundPlayPSG(int d,u16 f,u8 v,u8 p){(void)d;(void)f;(void)v;(void)p;return -1;}
int soundPlayNoise(u16 f,u8 v,u8 p){
  (void)v;(void)p; initial_noise=f; return 100;
}
int soundPlayWave(const signed char *cycle,u16 f,u8 v,u8 p){
  (void)cycle;(void)v;(void)p; initial_wave=f; return 200;
}
void soundSetFreq(int id,int hz){(void)id; last_pitch=hz; pitch_calls++;}
void soundSetVol(int id,u8 v){(void)id;(void)v;}
void soundKill(int id){(void)id;}
#include "../ds/source/sfx.c"

#define ASSERT(c,msg) do{if(!(c)){fprintf(stderr,"DS effect catch-up: %s (line %d)\n",msg,__LINE__);return 1;}}while(0)
static void reset_effects(void){
  memset(vo,0,sizeof vo);
  memset(cool,0,sizeof cool);
  initial_noise=initial_wave=last_pitch=pitch_calls=0;
}
int main(void){
  reset_effects();
  sfx_play(SFX_CRUNCH,64); /* 1900 source Hz -> 1140 Hz noise */
  ASSERT(initial_noise==1140 && vo[0].freq==1140,"noise glide starts from the wrong hardware pitch");
  sfx_tick_steps(1);
  ASSERT(last_pitch==870 && pitch_calls==1,"noise glide jumped from unscaled source pitch");
  reset_effects();
  sfx_play(SFX_TARGET,64); /* 659, 880, 1047, 1319 Hz, 3 ticks each */
  ASSERT(initial_wave==659 && vo[0].freq==659,"wrong initial target sound pitch");
  sfx_tick_steps(7);
  ASSERT(vo[0].seg==2 && vo[0].freq==1047,"elapsed steps lost effect timing");
  ASSERT(pitch_calls==1 && last_pitch==1047,"expired segment pitch was unnecessarily queued");
  reset_effects();
  sfx_play(SFX_TARGET,64);
  for(int i=0;i<7;i++)sfx_tick_steps(1);
  ASSERT(pitch_calls==2 && last_pitch==1047,"normal-rate effect pitches changed");
  ASSERT(vo[0].pitch_pending==0,"pending pitch left after final commit");
  puts("DS effect catch-up: start pitch, skipped segments and normal 60 Hz playback passed");
  return 0;
}
