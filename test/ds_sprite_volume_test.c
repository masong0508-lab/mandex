/* DS palette-based sprite volume check, on/off builds render identical
 * silhouettes. All original & Wolfman frames, sides and rotated poses.
 * No pixel buffer beyond the normal 256x192 screen. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "../ds/source/gfx.c"
static unsigned char surface[256*192+64];
static uint64_t digest=1469598103934665603ull;
static unsigned total,shaded;
static int bad;
static void sample(void){
  for(unsigned i=0;i<256u*192u;i++){
    u8 c=surface[32+i];
    unsigned occupied=c>=C_MAN0 && c<C_MAN_RIM0+C_MAN_RIM_COUNT;
    if(occupied){total++;if(c>=C_MAN_RIM0)shaded++;}
    digest^=occupied;digest*=1099511628211ull;
  }
  for(int i=0;i<32;i++)if(surface[i]!=0x57 || surface[32+256*192+i]!=0x57)bad++;
}
int main(void){
  gfx_init();
  if(MS_NPAL!=39 || C_MAN_RIM0!=199 || C_MAN_RIM0+C_MAN_RIM_COUNT!=224){
    puts("FAIL: DS contour ink overlaps another hardware palette range");return 1;
  }
  const u16 first=gfx_pal[C_MAN_RIM0+5];
  gfx_pal[C_MAN0+5]=(u16)RGB15(28,5,8);
  gfx_man_rim_palette();
  if(gfx_pal[C_MAN_RIM0+5]==first){
    puts("FAIL: DS shadow ink does not follow outfit recolors");return 1;
  }
  gfx_init();
  for(int fr=0;fr<MSF_COUNT;fr+=3)for(int face=-1;face<=1;face+=2)
  for(int ppc=2;ppc<=4;ppc++){
    memset(surface,0x57,sizeof surface);
    memset(surface+32,0,256*192);
    spr_man_frame(surface+32,128,90,ppc,face,fr,0);
    sample();
    memset(surface,0x57,sizeof surface);
    memset(surface+32,0,256*192);
    float a=(float)((fr%9)-4)*.23f;
    blit_frame_rot(surface+32,&MS_FRAMES[fr],128,90,ppc,face,0,
                   (int)(sinf(a)*4096),(int)(cosf(a)*4096));
    sample();
  }
  if(!total || bad){printf("FAIL: DS sprite invalid pixels=%u guard=%d\n",total,bad);return 1;}
#if MX_DS_MAN_VOLUME
  if(!shaded){puts("FAIL: DS shaded palette never appears");return 1;}
#else
  if(shaded){puts("FAIL: DS reference sprite unexpectedly uses contour slots");return 1;}
#endif
  printf("DS_MAN_OCCUPANCY_HASH %016llx total=%u\n",(unsigned long long)digest,total);
  printf("DS_MAN_CONTOUR_COUNT %u\n",shaded);
  return 0;
}
