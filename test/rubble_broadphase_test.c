/* Geometry regression for the real landlord/rubble fixed and float hit
 * calculation. No collision may disappear with the rectangle broadphase.
 * The full 60-level A/B harness separately runs the production update. */
#include <stdint.h>
#include <stdio.h>
#include "../core/mx.h"
static uint32_t seed=0x1d10f7acu;
static uint32_t next(void){seed=seed*1664525u+1013904223u;return seed;}
#ifdef MX_FIXED
static real from4096(int32_t x){return x;}
#else
static real from4096(int32_t x){return x/4096.0f;}
#endif
int main(void){
  const real lim=R(0.875)+R(0.5),lim2=rmul(lim,lim);
  int hit=0,miss=0,edge=0;
  for(int i=0;i<200000;i++){
    /* Include exact contact edges, subpixel contacts and very far cells. */
    int32_t xx=(int32_t)(next()%81921)-40960;
    int32_t yy=(int32_t)(next()%81921)-40960;
    if((i&63)==0){xx=5632+(i%3)-1;yy=0;edge++;}
    if((i&63)==1){xx=-5632+(i%3)-1;yy=0;edge++;}
    if((i&63)==2){xx=0;yy=5632+(i%3)-1;edge++;}
    if((i&63)==3){xx=0;yy=-5632+(i%3)-1;edge++;}
    real dx=from4096(xx),dy=from4096(yy);
    int reference=rmul(dx,dx)+rmul(dy,dy)<lim2;
    int broadphase=rabs(dx)<lim && rabs(dy)<lim &&
                   rmul(dx,dx)+rmul(dy,dy)<lim2;
    if(reference!=broadphase){
      fprintf(stderr,"rubble collision disagreement i=%d x=%ld y=%ld old=%d new=%d\n",i,(long)xx,(long)yy,reference,broadphase);
      return 1;
    }
    hit+=reference;miss+=!reference;
  }
  if(!hit||!miss||!edge){fputs("missing collision cases\n",stderr);return 1;}
  printf("rubble AABB parity: %d randomized positions, %d contact edges, %d real hits, %d misses PASSED\n",hit+miss,edge,hit,miss);
  return 0;
}
