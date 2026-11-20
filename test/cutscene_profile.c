/* Host-only timing for the real shared DS/3DS/Dreamcast cutscene
   renderer. Measures draw CPU only, excludes audio/tick, no hardware FPS claims. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "nds.h"
#include "../ds/source/gfx.h"
#include "../ds/source/intro.h"
#define NPIX (256*192)
#define MAX_SAMPLES 2048
static unsigned char top[NPIX],bottom[NPIX];
static uint64_t samples[MAX_SAMPLES];
static int n_sfx;
void sfx_play(int id,int pan){(void)id;(void)pan;n_sfx++;}
static uint64_t nano(void){
  struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
  return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec;
}
static int u64cmp(const void*a,const void*b){
  uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;
  return x<y?-1:x>y?1:0;
}
int main(void){
  gfx_init();
  unsigned checksum=2166136261u;
  puts("HOST profiling: ns/intro_draw() including two 256x192 screens; NOT console FPS");
  for(int mode=0;mode<4;mode++){
    if(mode==0)intro_start();
    else if(mode==1)intro_start_wolf();
    else intro_start_end(mode==2?1:2);
    int last=-1,n=0,frames=0,seen=0,finished=0;
    uint64_t sum=0,peak=0;
    for(int f=0;f<17000;f++){
      int scene=intro_scene();
      int new_scene=scene!=last;
      if(new_scene){seen++;last=scene;}
      if(new_scene||f%23==0){
        if(n>=MAX_SAMPLES){fputs("too many render samples\n",stderr);return 1;}
        uint64_t beg=nano();
        intro_draw(top,bottom);
        uint64_t dt=nano()-beg;
        samples[n++]=dt;sum+=dt;if(dt>peak)peak=dt;
        for(int k=0;k<NPIX;k+=257)checksum=(checksum^top[k])*16777619u;
        for(int k=0;k<NPIX;k+=521)checksum=(checksum^bottom[k])*16777619u;
      }
      frames++;
      if(intro_tick(0,0,0)){finished=1;break;}
    }
    if(!finished||seen!=intro_count()||n<10){fprintf(stderr,"incomplete profile mode=%d seen=%d\n",mode,seen);return 1;}
    qsort(samples,(size_t)n,sizeof(samples[0]),u64cmp);
    uint64_t p50=samples[n/2],p95=samples[(n*95/100<n)?n*95/100:n-1];
    printf("mode=%d scenes=%d frames=%d samples=%d mean_us=%.1f p50_us=%.1f p95_us=%.1f max_us=%.1f\n",
           mode,seen,frames,n,(double)sum/n/1000.0,(double)p50/1000.0,(double)p95/1000.0,(double)peak/1000.0);
  }
  printf("artwork_checksum=%08x audio_cues=%d\n",checksum,n_sfx);
  return 0;
}
