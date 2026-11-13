/* Runs the REAL dreamcast/main.c drawing code on the host; a tiny software rasteriser stands in for the PVR. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "kos.h"
static unsigned char fb[640*480*3];
static pvr_vertex_t q[4]; static int nq;
static pvr_poly_hdr_t curh;
void pvr_init_defaults(void){} void pvr_poly_cxt_col(pvr_poly_cxt_t*c,int l){(void)l;memset(c,0,sizeof *c);}
void pvr_poly_cxt_txr(pvr_poly_cxt_t*c,int l,int fmt,int w,int h,pvr_ptr_t p,int f){(void)l;(void)f;memset(c,0,sizeof *c);c->txr.tex=1;c->txr.fmt=fmt;c->txr.w=w;c->txr.h=h;c->txr.ptr=p;}
void pvr_poly_compile(pvr_poly_hdr_t*h,pvr_poly_cxt_t*c){memset(h,0,sizeof *h);h->tex=c->txr.tex;h->fmt=c->txr.fmt;h->w=c->txr.w;h->h=c->txr.h;h->ptr=c->txr.ptr;}
pvr_ptr_t pvr_mem_malloc(size_t n){return malloc(n);} void pvr_mem_free(pvr_ptr_t p){free(p);} void pvr_txr_load(const void*s,pvr_ptr_t d,uint32 n){memcpy(d,s,n);}
void pvr_wait_ready(void){} void pvr_scene_begin(void){} void pvr_list_begin(int l){(void)l;} void pvr_list_finish(void){} void pvr_scene_finish(void){}
maple_device_t *maple_enum_type(int a,int b){(void)a;(void)b;return 0;} void *maple_dev_status(maple_device_t*d){(void)d;return 0;}
/* texel fetch, bilinear: returns 0..255 argb components */
static void texel(const unsigned short*t,int fmt,int w,int h,int x,int y,float*o){
  if(x<0)x=0; if(y<0)y=0; if(x>=w)x=w-1; if(y>=h)y=h-1; unsigned v=t[y*w+x];
  if(fmt==PVR_TXRFMT_RGB565){ o[0]=255; o[1]=((v>>11)&31)*255/31.f; o[2]=((v>>5)&63)*255/63.f; o[3]=(v&31)*255/31.f; }
  else if(fmt==PVR_TXRFMT_ARGB1555){ o[0]=(v>>15)?255:0; o[1]=((v>>10)&31)*255/31.f; o[2]=((v>>5)&31)*255/31.f; o[3]=(v&31)*255/31.f; }
  else { o[0]=((v>>12)&15)*17; o[1]=((v>>8)&15)*17; o[2]=((v>>4)&15)*17; o[3]=(v&15)*17; } }
static void sample(float u,float v,float*o){
  int fmt=curh.fmt&~PVR_TXRFMT_NONTWIDDLED, w=curh.w, h=curh.h; float x=u*w-0.5f,y=v*h-0.5f; int x0=(int)floorf(x),y0=(int)floorf(y); float fx=x-x0,fy=y-y0,a[4],b[4],c[4],d[4];
  const unsigned short*t=(const unsigned short*)curh.ptr;
  texel(t,fmt,w,h,x0,y0,a); texel(t,fmt,w,h,x0+1,y0,b); texel(t,fmt,w,h,x0,y0+1,c); texel(t,fmt,w,h,x0+1,y0+1,d);
  for(int k=0;k<4;k++) o[k]=(a[k]*(1-fx)+b[k]*fx)*(1-fy)+(c[k]*(1-fx)+d[k]*fx)*fy; }
static void blend(int x,int y,float r,float g,float b,float a){ unsigned char*p2=&fb[(y*640+x)*3]; a/=255.f; p2[0]=(unsigned char)(p2[0]*(1-a)+r*a+.5f); p2[1]=(unsigned char)(p2[1]*(1-a)+g*a+.5f); p2[2]=(unsigned char)(p2[2]*(1-a)+b*a+.5f); }
static void tri(const pvr_vertex_t*A,const pvr_vertex_t*B,const pvr_vertex_t*C){
  float minx=fminf(A->x,fminf(B->x,C->x)),maxx=fmaxf(A->x,fmaxf(B->x,C->x)),miny=fminf(A->y,fminf(B->y,C->y)),maxy=fmaxf(A->y,fmaxf(B->y,C->y));
  float den=(B->y-C->y)*(A->x-C->x)+(C->x-B->x)*(A->y-C->y); if(fabsf(den)<1e-6f) return;
  for(int y=(int)floorf(miny);y<(int)ceilf(maxy);y++)for(int x=(int)floorf(minx);x<(int)ceilf(maxx);x++){
    if(x<0||y<0||x>=640||y>=480)continue; float px=x+0.5f,py=y+0.5f;
    float l1=((B->y-C->y)*(px-C->x)+(C->x-B->x)*(py-C->y))/den, l2=((C->y-A->y)*(px-C->x)+(A->x-C->x)*(py-C->y))/den, l3=1-l1-l2;
    if(l1<-0.001f||l2<-0.001f||l3<-0.001f) continue;
    float cr=0,cg=0,cb=0,ca=0; const pvr_vertex_t*V3[3]={A,B,C}; float L[3]={l1,l2,l3};
    for(int k=0;k<3;k++){ uint32 c=V3[k]->argb; ca+=L[k]*(c>>24); cr+=L[k]*((c>>16)&255); cg+=L[k]*((c>>8)&255); cb+=L[k]*(c&255); }
    if(curh.tex){ float u=l1*A->u+l2*B->u+l3*C->u, v=l1*A->v+l2*B->v+l3*C->v, t[4]; sample(u,v,t); ca=ca*t[0]/255.f; cr=cr*t[1]/255.f; cg=cg*t[2]/255.f; cb=cb*t[3]/255.f; }
    blend(x,y,cr,cg,cb,ca); } }
int pvr_prim(void*p,int n){
  if(n==sizeof(pvr_poly_hdr_t)){ curh=*(pvr_poly_hdr_t*)p; return 0; }
  q[nq++]=*(pvr_vertex_t*)p;
  if(nq==4){ nq=0;                                       /* a strip a,b,c,d = triangles abc and bcd (the PVR does the same) */
    if(!curh.tex && q[0].y==q[1].y && q[2].y==q[3].y && q[0].x==q[2].x && q[1].x==q[3].x && q[0].argb==q[2].argb){   /* flat axis-aligned rectangles: pixel-touch rule so thin rows do not vanish */
      uint32 c=q[0].argb; float a=c>>24; int x0=(int)floorf(q[0].x),x1=(int)ceilf(q[1].x),y0=(int)floorf(q[0].y),y1=(int)ceilf(q[2].y);
      for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){ if(x<0||y<0||x>=640||y>=480)continue; blend(x,y,(c>>16)&255,(c>>8)&255,c&255,a); }
    } else { tri(&q[0],&q[1],&q[2]); tri(&q[1],&q[3],&q[2]); } }
  return 0; }
#define main dc_main_unused
#include "../dreamcast/main.c"
#undef main
static void dump(const char*fn){ FILE*f=fopen(fn,"wb"); fprintf(f,"P6\n640 480\n255\n"); fwrite(fb,1,sizeof fb,f); fclose(f); }
static void frame(const char*fn){ memset(fb,0,sizeof fb); cur_hdr=0; draw_all(); dump(fn); }
static void tick(int calm){ mx_update(&world,DT); handle_events(); fx_step(calm,0); }
int main(void){
  gfx_init();
  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? (float)mx_material_hp(k) : 1.0f;
  memset(fb,0,sizeof fb); title_t=0; cur_hdr=0; draw_title(); dump("/tmp/dc_title.ppm"); title_t=40; cur_hdr=0; draw_title(); dump("/tmp/dc_title_blink.ppm");   /* the title, and the half of the blink with no PRESS START */
  { const int T[3] = { 30, 170, 330 }; for(int k = 0; k < 3; k++){ char fn[48]; snprintf(fn, sizeof fn, "/tmp/dc_tpan%d.ppm", k); memset(fb,0,sizeof fb); title_t = T[k]; cur_hdr=0; draw_title(); dump(fn); } }   /* the pan: logo end, mid glide, the man */
  load_level(4);
  for(int i=0;i<90;i++) tick(0);
  mx_set_pull(&world,-11.5f*cosf(0.45f),11.5f*sinf(0.45f)); frame("/tmp/dc_pull.ppm");
  mx_release(&world);
  int shot_j = 0; float peak = 0;
  for(int f=0;f<75;f++){ tick(0);
    float j = fabsf(world.man.bx) + fabsf(world.man.rx) + fabsf(world.man.by) + fabsf(world.man.ry);
    if(j > peak){ peak = j; if(j > 1.2f && f > 5){ char fn[64]; snprintf(fn, sizeof fn, "/tmp/dc_jig%d.ppm", shot_j++ % 3); frame(fn); } } }
  printf("peak jiggle (cells, summed) %.2f, squash now %.2f\n", peak, world.man.sq);
  frame("/tmp/dc_hit.ppm");
  for(int f=0;f<200;f++) tick(0);
  frame("/tmp/dc_after.ppm"); printf("dc frames rendered; cells=%d\n",mx_cells_left(&world));
  /* idle gags: stand in the sling, photograph every distinct frame of every gag once */
  load_level(0); for(int i=0;i<90;i++) tick(0);
  { unsigned char seen[128] = {0}; int n = 0, lg = -2, ls = -2;
    int forced = 0;                                         /* FORCE_GAG=7 photographs just that gag (7 = the cape taunt, a turn with the towel) */
    for(int i=0;i<2000 && n<(getenv("FORCE_GAG")?40:12);i++){ tick(1);
      if(!forced && getenv("FORCE_GAG") && mi.gag < 0 && mi.total > 0){ forced = 1; mi.gag = atoi(getenv("FORCE_GAG")); mi.step = 0; mi.t = 0; }
      if(mi.gag >= 0 && (mi.gag != lg || mi.step != ls)){ int fr = mi_frame(&mi, i); if(!seen[fr]){ seen[fr]=1; char fn[64]; snprintf(fn,sizeof fn,"/tmp/dc_idle%02d.ppm",n++); frame(fn); printf("  idle shot gag=%d frame=%d\n", mi.gag, fr); } }
      lg = mi.gag; ls = mi.step; } }
  /* chapter 2: Walk-Up Wreckage on foot (22), Collection Day (25) with a walker, Batter Up (26) with the bat and briefcases */
  if(getenv("DC_WALK")){
    load_level(22); for(int i=0;i<30;i++) tick(0);
    mx_set_pull(&world,-11.5f*cosf(0.45f),11.5f*sinf(0.45f)); mx_release(&world);
    for(int i=0;i<900 && world.man.mode != MAN_WALK;i++) tick(0);
    printf("walk mode reached: %d\n", world.man.mode == MAN_WALK);
    for(int i=0;i<20;i++){ mx_walk_input(&world, 1, 0, 0, 0); tick(0); } frame("/tmp/dc_walk0.ppm");
    for(int i=0;i<8;i++){ mx_walk_input(&world, 1, 0, 1, 0); tick(0); } frame("/tmp/dc_walk1.ppm");
    for(int i=0;i<6;i++){ mx_walk_input(&world, 0, 1, 0, 0); tick(0); } frame("/tmp/dc_walk2.ppm");
    load_level(25); for(int i=0;i<60;i++) tick(0); frame("/tmp/dc_l25.ppm");
    load_level(26); for(int i=0;i<60;i++) tick(0); frame("/tmp/dc_l26.ppm");
    for(int i=0;i<400;i++){ tick(0); if(world.cases[0].active || world.cases[1].active){ frame("/tmp/dc_l26b.ppm"); break; } }
  }
  world.state = MX_WIN; frame("/tmp/dc_win.ppm"); world.state = MX_LOSE; frame("/tmp/dc_lose.ppm");
  return 0; }
