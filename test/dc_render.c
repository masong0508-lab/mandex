/* Runs the REAL dreamcast/main.c drawing code on the host; a tiny software rasteriser stands in for the PVR. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "kos.h"
static unsigned char fb[640*480*3];
static pvr_vertex_t q[4]; static int nq;
void pvr_init_defaults(void){} void pvr_poly_cxt_col(pvr_poly_cxt_t*c,int l){(void)c;(void)l;} void pvr_poly_compile(pvr_poly_hdr_t*h,pvr_poly_cxt_t*c){(void)h;(void)c;}
void pvr_wait_ready(void){} void pvr_scene_begin(void){} void pvr_list_begin(int l){(void)l;} void pvr_list_finish(void){} void pvr_scene_finish(void){}
maple_device_t *maple_enum_type(int a,int b){(void)a;(void)b;return 0;} void *maple_dev_status(maple_device_t*d){(void)d;return 0;}
int pvr_prim(void*p,int n){
  if(n==sizeof(pvr_poly_hdr_t)) return 0;
  q[nq++]=*(pvr_vertex_t*)p;
  if(nq==4){ nq=0;
    float x0=q[0].x,y0=q[0].y,x1=q[3].x,y1=q[3].y; uint32 c=q[0].argb; int a=c>>24;
    for(int y=(int)floorf(y0);y<(int)ceilf(y1);y++)for(int x=(int)floorf(x0);x<(int)ceilf(x1);x++){
      if(x<0||y<0||x>=640||y>=480)continue; unsigned char*p2=&fb[(y*640+x)*3];
      p2[0]=(p2[0]*(255-a)+((c>>16)&255)*a)/255; p2[1]=(p2[1]*(255-a)+((c>>8)&255)*a)/255; p2[2]=(p2[2]*(255-a)+(c&255)*a)/255; } }
  return 0; }
#define main dc_main_unused
#include "../dreamcast/main.c"
#undef main
static void dump(const char*fn){ FILE*f=fopen(fn,"wb"); fprintf(f,"P6\n640 480\n255\n"); fwrite(fb,1,sizeof fb,f); fclose(f); }
static void frame(const char*fn){ memset(fb,0,sizeof fb); draw_all(); dump(fn); }
static void tick(int calm){ mx_update(&world,DT); handle_events(); fx_step(calm,0); }
int main(void){
  pvr_poly_hdr_t h; (void)h;
  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? (float)mx_material_hp(k) : 1.0f;
  mx_load(&world,4);
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
  mx_load(&world,0); for(int i=0;i<90;i++) tick(0);
  { unsigned char seen[64] = {0}; int n = 0, lg = -2, ls = -2;
    for(int i=0;i<2000 && n<12;i++){ tick(1);
      if(mi.gag >= 0 && (mi.gag != lg || mi.step != ls)){ int fr = mi_frame(&mi, i); if(!seen[fr]){ seen[fr]=1; char fn[64]; snprintf(fn,sizeof fn,"/tmp/dc_idle%02d.ppm",n++); frame(fn); printf("  idle shot gag=%d frame=%d\n", mi.gag, fr); } }
      lg = mi.gag; ls = mi.step; } }
  world.state = MX_WIN; frame("/tmp/dc_win.ppm"); world.state = MX_LOSE; frame("/tmp/dc_lose.ppm");
  return 0; }
