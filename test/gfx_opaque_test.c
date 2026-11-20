/* Bit-identical opaque tile/wall blit reference, plus framebuffer guards. */
#include <stdio.h>
#include <string.h>
#include "nds.h"
#include "gfx.h"
#include "mx.h"
#define LEN (256*192)
static u8 fb[LEN+64];
static unsigned rng=0x63f11871u;
static unsigned step(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){
 gfx_init();
 unsigned checksum=2166136261u;
 for(int iter=0;iter<256;iter++){
   memset(fb,0xA7,sizeof fb);
   View v={fb+32,4,0,0};
   for(int j=0;j<100;j++){
     v.ppc=2+(int)(step()%3);
     int px=(int)(step()%282)-13,py=(int)(step()%216)-12;
     int mat=1+(int)(step()%(MX_NMAT-1));
     int dmg=(int)(step()%3),var=(int)(step()%2);
     gfx_tile(&v,mat,dmg,var,px,py);
     gfx_wall(&v,(int)(step()%4),px+4,py-2);
   }
   for(int k=0;k<32;k++)if(fb[k]!=0xA7||fb[LEN+32+k]!=0xA7){
     fprintf(stderr,"framebuffer canary overwritten at iteration %d\n",iter);return 1;
   }
   for(int k=0;k<LEN;k++)checksum=(checksum^fb[32+k])*16777619u;
 }
 printf("opaque_tiles_checksum=%08x test_frames=256 tile_and_wall_calls=51200\n",checksum);
 return 0;
}
