/* Real DS software raster regression for the distance-locked landlord gait.
   No emulator, heap allocation, or renderer replacement: gfx.c as shipped. */
#include <stdio.h>
#include <string.h>
#include "../ds/source/gfx.h"
#define SZ (256*192)
static unsigned char a[SZ+64],b[SZ+64];
static int failed;
static void check(int ok,const char *what){
  if(!ok){fprintf(stderr,"FAIL: %s\n",what);failed++;}
}
static void erase(void){
  memset(a,0xA6,sizeof a);memset(b,0xA6,sizeof b);
}
static int safe(void){
  for(int i=0;i<32;i++)
    if(a[i]!=0xA6||b[i]!=0xA6||a[SZ+32+i]!=0xA6||b[SZ+32+i]!=0xA6)return 0;
  return 1;
}
int main(void){
  for(int p=2;p<=8;p+=2){
    erase();
    spr_landlord_k(a+32,126,110,p,0,0,1,1,8);  /* walking, left shoe planted */
    spr_landlord_k(b+32,126,110,p,4,0,1,1,8);  /* distance traveled, right shoe planted */
    check(memcmp(a+32,b+32,SZ)!=0,"different walk phase must move visible feet");
    check(safe(),"walking sprite must honor framebuffer canaries");
    erase();
    spr_landlord(a+32,126,110,p,0,0);
    spr_landlord(b+32,126,110,p,4,0);
    check(memcmp(a+32,b+32,SZ)==0,"idle without travel must not walk");
    check(safe(),"idle sprite must honor framebuffer canaries");
    erase();
    spr_landlord_k(a+32,126,110,p,0,0,1,1,0);
    spr_landlord_k(b+32,126,110,p,0,0,1,1,2);
    check(memcmp(a+32,b+32,SZ)!=0,"attack windup must have a visible warning");
    check(safe(),"windup indicator must honor framebuffer canaries");
  }
  for(int q=0;q<4;q++){
    erase();
    int x=q&1?255:0,y=q&2?191:0;
    spr_landlord_k(a+32,x,y,8,12,0,3,-1,2|8|32);
    check(safe(),"clipped enemy, club, shield and warning must not overdraw");
  }
  if(failed)return 1;
  puts("PASS: actual DS enemy pixels differ by travel/windup, idle stays still, frame edges remain safe");
  return 0;
}
