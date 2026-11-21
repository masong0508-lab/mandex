/* Verify the ACTUAL shared DS/3DS/Dreamcast cutscene mask painter, not a
   reimplementation. This test includes intro.c to inspect its static trans()
   directly while linking the normal software gfx implementation. */
#include <stdio.h>
#include <string.h>
#include "nds.h"
#include "../ds/source/gfx.h"
#include "../ds/source/sfx.h"
#include "../ds/source/intro.c"

void sfx_play(int id,int pan){(void)id;(void)pan;}

static unsigned char canvas[256*192];
static int failures;
#define CHECK(test,msg) do{if(!(test)){fprintf(stderr,"FAIL: %s\n",msg);failures++;}}while(0)
#define AT(x,y) canvas[(y)*256+(x)]

static void fill(void){memset(canvas,C_GOLD,sizeof canvas);}
static unsigned visible(void){
  unsigned n=0;
  for(unsigned k=0;k<sizeof canvas;k++)if(canvas[k]==C_GOLD)n++;
  return n;
}
int main(void){
  mode=0;wolf=0;
  /* The authored types no longer collapse to the same black flash. */
  sc=1;t=3;fill();trans(canvas);
  CHECK(AT(10,90)==C_GOLD && AT(128,90)==C_BLACK,
        "wipes progressively reveal the left before the right");
  sc=1;t=9;fill();trans(canvas);
  CHECK(AT(128,90)==C_GOLD && AT(245,90)==C_BLACK,
        "wipe continues to right edge with easing");

  sc=3;t=5;fill();trans(canvas);
  CHECK(AT(128,96)==C_GOLD && AT(0,0)==C_BLACK && AT(255,191)==C_BLACK,
        "iris reveals central subject while preserving masked corners");
  sc=3;t=11;fill();trans(canvas);
  CHECK(AT(0,0)==C_GOLD && AT(255,191)==C_GOLD,
        "iris opens wide enough to avoid popping corners at the finish");

  /* All reveal masks must uncover progressively more of the actual scene
     rather than flickering black/visible between fixed 60-Hz frames. */
  for(int kind=0;kind<2;kind++){
    sc=kind?3:1;
    unsigned prev=0;
    for(int tick=0;tick<=12;tick++){
      t=tick;fill();trans(canvas);
      unsigned now=visible();
      CHECK(now>=prev,"reveal mask never closes between frames");
      prev=now;
    }
    CHECK(prev==sizeof canvas,"wipe or iris fully uncovers the 256x192 scene");
  }
  sc=0;t=1;fill();trans(canvas);
  CHECK(AT(128,96)==C_BLACK,"fade starts with a true black exposure");
  sc=4;t=1;fill();trans(canvas);
  CHECK(AT(128,96)==C_WHITE,"white flash is distinct from a black fade");
  sc=5;t=1;fill();trans(canvas);
  CHECK(AT(128,96)==C_GOLD,"hard cuts do not draw any transition mask");
  if(failures)return 1;
  puts("CUTSCENE_TRANSITIONS_PASS fade wipe iris white cut monotonic_masks=1");
  return 0;
}
