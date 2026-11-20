/* Shared cutscene safety: every story mode, every scene, and dynamic shots.
 * Guard both 256x192 8-bit framebuffers to catch unclipped comic effects,
 * and verify automatic progression and animated scene content. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "nds.h"
#include "../ds/source/intro.h"
#include "../ds/source/gfx.h"
#include "../ds/source/sfx.h"
#define LEN (256*192)
#define PAD 64
static unsigned char top[LEN+2*PAD],bot[LEN+2*PAD];
static int played,by_id[SFX_COUNT];
void sfx_play(int id,int pan){(void)pan;played++;if(id>=0&&id<SFX_COUNT)by_id[id]++;}
static int protected_buffers(void){
  for(int i=0;i<PAD;i++){
    if(top[i]!=0xa5||top[PAD+LEN+i]!=0x5a)return 0;
    if(bot[i]!=0xa5||bot[PAD+LEN+i]!=0x5a)return 0;
  }
  return 1;
}
static void prepare(void){
  memset(top,0xa5,PAD);memset(bot,0xa5,PAD);
  memset(top+PAD,0,LEN);memset(bot+PAD,0,LEN);
  memset(top+PAD+LEN,0x5a,PAD);memset(bot+PAD+LEN,0x5a,PAD);
}
static unsigned distinct(const unsigned char *p){
  unsigned char seen[256]={0};unsigned n=0;
  for(int i=0;i<LEN;i++){if(!seen[p[i]]){seen[p[i]]=1;n++;}}
  return n;
}
int main(void){
  gfx_init();
  for(int mode=0;mode<4;mode++){
    prepare();
    if(mode==0)intro_start();
    else if(mode==1)intro_start_wolf();
    else intro_start_end(mode==2?1:2);
    int count=intro_count(),last=-1,finished=0,frames=0,samples=0;
    unsigned seen=0,colors=0;
    if(count!=(mode<2?8:4)||intro_mode()!=mode){
      fprintf(stderr,"wrong mode/count: %d/%d\n",mode,count);return 1;
    }
    for(int frame=0;frame<18000;frame++){
      int scene=intro_scene();
      if(scene!=last||frame%19==0){
        intro_draw(top+PAD,bot+PAD);
        if(!protected_buffers()){
          fprintf(stderr,"cutscene overwrite, mode=%d scene=%d frame=%d\n",mode,scene,frame);return 1;
        }
        unsigned now=distinct(top+PAD);
        if(now<2){
          fprintf(stderr,"empty top frame mode=%d scene=%d\n",mode,scene);return 1;
        }
        if(now>colors)colors=now;
        samples++;
      }
      if(scene!=last){if(scene<0||scene>=count){fprintf(stderr,"invalid scene\n");return 1;}seen|=1u<<scene;last=scene;}
      frames++;
      if(intro_tick(0,0,0)){finished=1;break;}
    }
    if(!finished||seen!=((1u<<count)-1u)||colors<4||samples<50){
      fprintf(stderr,"cutscene failed mode=%d done=%d mask=0x%x colors=%u samples=%d\n",
              mode,finished,seen,colors,samples);return 1;
    }
    printf("ok: cutscene mode %d, %d scenes, %d frames, %d guarded render samples\n",
           mode,count,frames,samples);
  }
  if(played<25||by_id[SFX_BOOM]<3||by_id[SFX_POP]<3){
    fprintf(stderr,"too few cinematic audio cues total=%d boom=%d pop=%d\n",played,by_id[SFX_BOOM],by_id[SFX_POP]);return 1;
  }
  puts("ok: all shared cutscenes preserve framebuffer guards and produce visible art");
  return 0;
}
