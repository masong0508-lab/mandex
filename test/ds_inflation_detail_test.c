/* DS native-resolution inflation quality and pixel-safe clipping.
   Exercise the production artwork, no simulated ellipse substitute. */
#include <stdio.h>
#include <string.h>
#include "../ds/source/gfx.c"
static u8 screen[256*192+64];
static int fails;
static void ok(int yes,const char *msg){if(!yes){fprintf(stderr,"FAIL DS inflation: %s\n",msg);fails++;}}
static unsigned draw(int ppc,int pressure,int face,int x,int y,int phase,int *w,int *h){
  memset(screen,0xa5,sizeof screen);memset(screen+32,0,256*192);
  spr_man_inflation(screen+32,x,y,ppc,face,pressure,phase);
  int minx=256,maxx=-1,miny=192,maxy=-1;unsigned n=0;int badcol=0;
  for(int yy=0;yy<192;yy++)for(int xx=0;xx<256;xx++){
    u8 c=screen[32+yy*256+xx];if(!c)continue;
    n++;if(xx<minx)minx=xx;if(xx>maxx)maxx=xx;
    if(yy<miny)miny=yy;if(yy>maxy)maxy=yy;
    if(c!=C_RED2 && (c<C_MAN0 || c>=C_MAN_RIM0+C_MAN_RIM_COUNT))badcol++;
  }
  for(int i=0;i<32;i++)if(screen[i]!=0xa5 || screen[32+256*192+i]!=0xa5)fails++;
  ok(!badcol,"no stray/unrecoloured skin palette colours");
  *w=n?maxx-minx+1:0;*h=n?maxy-miny+1:0;
  if(x>=60&&x<=196&&y>=60&&y<=130&&n){
    ok(miny>=y-4*ppc,"belly cannot cover face");
    ok(maxy<=y+10*ppc,"inflation stays on lower torso");
    ok(*w<=17*ppc,"inflation cannot grow as large as the old circle");
  }
  return n;
}
int main(void){
  gfx_init();int w,h;unsigned samples=0;
  for(int ppc=2;ppc<=4;ppc++)for(int face=-1;face<=1;face+=2){
    unsigned prev=0;const int pressure[]={0,1,10,20,40,60,80,100};
    for(unsigned k=0;k<sizeof pressure/sizeof pressure[0];k++){
      unsigned n=draw(ppc,pressure[k],face,128,100,0,&w,&h);
      if(k){ok(n>0,"positive pressure draws inflation");
            ok(n>=prev,"silhouette expands smoothly across pressure stages");}
      else ok(!n,"empty valve leaves the original sprite untouched");
      prev=n;samples++;
      if(pressure[k]==100)ok(w>=10*ppc && w<=17*ppc,"bounded full pressure");
    }
    for(int f=1;f<=100;f+=13){
      (void)draw(ppc,f,face,0,0,8,&w,&h);
      (void)draw(ppc,f,face,255,191,8,&w,&h);
      samples+=2;
    }
  }
  memset(screen,0,sizeof screen);spr_man_inflation(screen+32,128,95,4,1,100,8);
  int shade=0,skin=0,light=0,rim=0,cloth=0;
  for(int i=0;i<256*192;i++){
    u8 c=screen[32+i];
    shade+=(c==C_MAN0+2);skin+=(c==C_MAN0+1);light+=(c==C_MAN0+3);
    rim+=(c==C_MAN_RIM0+1);cloth+=(c==C_RED2);
  }
  ok(shade>0&&skin>0&&light>0&&rim>0&&cloth>0,
     "full inflation has shadow, midtone, highlight, contour and clothing linework");
  gfx_man_fur=1;(void)draw(4,100,-1,128,95,8,&w,&h);
  if(fails)return 1;
  printf("DS inflation: %u render cases, bounded torso/skin detail/edge canaries passed\n",samples);
  return 0;
}
