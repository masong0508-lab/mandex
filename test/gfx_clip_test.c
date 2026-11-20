/* Pixel-for-pixel regression for invisible-work culling in shared DS/3DS/DC art.
 * The reference routines below are the pre-optimization versions. */
#include <stdio.h>
#include <string.h>
#include "../ds/source/gfx.h"
#define W 256
#define H 192
#define N (W*H)
static unsigned char want[N+64],have[N+64];
static unsigned seed=0x57bada55u;
static unsigned rand32(void){seed=seed*1664525u+1013904223u;return seed;}
static int pick(int lo,int hi){return lo+(int)(rand32()%(unsigned)(hi-lo+1));}
static unsigned ref_isq(unsigned n){
  unsigned r=0,b=1u<<30;
  while(b>n)b>>=2;
  while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}
  return r;
}
static void ref_hspan(u8 *b,int x0,int x1,int y,u8 c){
  if((unsigned)y>=192u)return;
  if(x0<0)x0=0;if(x1>255)x1=255;
  if(x1>=x0)memset(&b[y*256+x0],c,(size_t)(x1-x0+1));
}
static void ref_ell(u8 *b,int cx,int cy,int rx,int ry,u8 c){
  if(rx<=0||ry<=0){if(rx>0||ry>0)gfx_px(b,cx,cy,c);return;}
  int D=ry*ry+ry,den=8*ry;
  for(int j=-ry;j<=ry;j++){
    int e=(int)(((unsigned)rx*ref_isq((unsigned)(D-j*j)*64u))/(unsigned)den);
    if(e>rx)e=rx;
    ref_hspan(b,cx-e,cx+e,cy+j,c);
  }
}
static void ref_tri(u8 *b,int x0,int y0,int x1,int y1,int x2,int y2,u8 c){
  int t;
  if(y1<y0){t=y0;y0=y1;y1=t;t=x0;x0=x1;x1=t;}
  if(y2<y0){t=y0;y0=y2;y2=t;t=x0;x0=x2;x2=t;}
  if(y2<y1){t=y1;y1=y2;y2=t;t=x1;x1=x2;x2=t;}
  if(y2==y0){
    int a=x0<x1?x0:x1,e=x0>x1?x0:x1;
    if(x2<a)a=x2;if(x2>e)e=x2;
    gfx_rect(b,a,y0,e-a+1,1,c);return;
  }
  for(int y=y0;y<=y2;y++){
    int xa=x0+(x2-x0)*(y-y0)/(y2-y0),xb;
    if(y<y1)xb=x0+(x1-x0)*(y-y0)/(y1-y0);
    else xb=y2==y1?x1:x1+(x2-x1)*(y-y1)/(y2-y1);
    if(xa>xb){t=xa;xa=xb;xb=t;}
    gfx_rect(b,xa,y,xb-xa+1,1,c);
  }
}
static void ref_dither(u8 *b,int x,int y,int w,int h,u8 c){
  for(int j=y;j<y+h;j++)for(int i=x+((j^x)&1);i<x+w;i+=2)gfx_px(b,i,j,c);
}
static void reset(void){memset(want,0x5a,sizeof want);memset(have,0x5a,sizeof have);}
static int check(const char *name,int case_no){
  if(memcmp(want,have,sizeof want)){
    for(int i=0;i<(int)sizeof want;i++)if(want[i]!=have[i]){
      fprintf(stderr,"%s case %d mismatch at byte %d: expected %u got %u\n",
              name,case_no,i,(unsigned)want[i],(unsigned)have[i]);break;
    }
    return 1;
  }
  return 0;
}
int main(void){
  for(int k=0;k<480;k++){
    int x=pick(-320,550),y=pick(-320,490);
    int rx=pick(0,210),ry=pick(0,210);
    if((k&15)==0){x=pick(-5,260);y=pick(-5,200);rx=pick(50,180);ry=pick(50,170);}
    reset();ref_ell(want+32,x,y,rx,ry,77);gfx_ell(have+32,x,y,rx,ry,77);
    if(check("ellipse",k))return 1;
  }
  for(int k=0;k<480;k++){
    int x0=pick(-330,550),x1=pick(-330,550),x2=pick(-330,550);
    int y0=pick(-300,480),y1=pick(-300,480),y2=pick(-300,480);
    if(k%13==0)y1=y0=y2;
    reset();ref_tri(want+32,x0,y0,x1,y1,x2,y2,31);
    gfx_tri(have+32,x0,y0,x1,y1,x2,y2,31);
    if(check("triangle",k))return 1;
  }
  for(int k=0;k<480;k++){
    int x=pick(-350,510),y=pick(-350,420);
    int w=pick(-10,550),h=pick(-10,500);
    if((k&7)==0){x=0;y=0;w=256;h=192;}
    reset();ref_dither(want+32,x,y,w,h,121);
    gfx_dither(have+32,x,y,w,h,121);
    if(check("dither",k))return 1;
  }
  puts("ok: clipped ellipses, triangles and dither are pixel-identical on 1,440 random/offscreen cases");
  return 0;
}
