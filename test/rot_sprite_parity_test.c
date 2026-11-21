/* Exact deterministic rotated sprite parity for the DS renderer.
 * Build twice: -DMX_DS_ROT_INCREMENTAL=0 and =1; diff their stdout.
 * Includes the actual renderer so the private blit_frame_rot hot path
 * can be exercised with every original and Wolfman sprite frame. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../ds/source/gfx.c"

static unsigned seed=0xe719f58bu;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static unsigned char framebuffer[256*192+64];
static uint64_t digest=1469598103934665603ull;
static void hash_pixels(void){
  for(size_t i=0;i<sizeof framebuffer;i++){
    digest^=framebuffer[i];
    digest*=1099511628211ull;
  }
}
int main(void){
  int undeformed=0,deformed=0;
  for(int k=0;k<3100;k++){
    int frame=(int)(rnd()%MSF_COUNT);
    int x=(int)(rnd()%560)-155;
    int y=(int)(rnd()%430)-120;
    int ppc=2+(int)(rnd()%3);
    int face=(rnd()&1)?1:-1;
    /* The same rounded trig inputs reach both implementations. */
    float a=(float)((int)(rnd()%629)-314)*0.01f;
    int sn=(int)(sinf(a)*4096.0f),cs=(int)(cosf(a)*4096.0f);
    ManJig jig={0};
    const ManJig *j=NULL;
    switch(k%5){
      case 0: j=NULL; break;
      case 1: j=&jig; break;
      case 2: jig.bx=(int)(rnd()%9)-4;jig.by=(int)(rnd()%9)-4;j=&jig; break;
      case 3: jig.rx=(int)(rnd()%7)-3;jig.ry=(int)(rnd()%7)-3;j=&jig; break;
      default: jig.sx=(int)(rnd()%37)-18;jig.sy=(int)(rnd()%37)-18;j=&jig; break;
    }
    if(!j||(!j->bx&&!j->by&&!j->rx&&!j->ry&&!j->sx&&!j->sy))undeformed++;
    else deformed++;
    memset(framebuffer,0x39,sizeof framebuffer);
    blit_frame_rot(framebuffer+32,&MS_FRAMES[frame],x,y,ppc,face,j,sn,cs);
    hash_pixels();
  }
  printf("rotated_sprite_hash=%016llx frames=3100 undeformed=%d deformed=%d\n",
    (unsigned long long)digest,undeformed,deformed);
  return 0;
}
