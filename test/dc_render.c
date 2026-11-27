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
int stub_snd_plays;
#define main dc_main_unused
#include "../dreamcast/main.c"
#undef main
static void dump(const char*fn){ FILE*f=fopen(fn,"wb"); fprintf(f,"P6\n640 480\n255\n"); fwrite(fb,1,sizeof fb,f); fclose(f); }
static void frame(const char*fn){ memset(fb,0,sizeof fb); cur_hdr=0; draw_all(); dump(fn); }
static void tick(int calm){ mx_update(&world,DT); handle_events(); fx_step(calm,0); }
/* The real ic_upload() function, with exact RGB565 texture bytes, is run
   in two separately compiled variants by render-fastpaths CI. */
static unsigned dc_hash(const unsigned char *p,size_t n){
  unsigned h=2166136261u;
  for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}
  return h;
}
/* Test the production on-demand RLE outfit path, including recovery
   from a different tint. The source pixels must never accumulate rounding
   errors and the PVR atlas allocation must stay stable. */
static int dc_outfit_memory_probe(void){
  if(!tx_man){ puts("FAIL: Dreamcast man atlas missing"); return 1; }
  pvr_ptr_t atlas=tx_man;
  const size_t bytes=(size_t)DCT_MAN_W*DCT_MAN_H*sizeof(unsigned short);
  recolor_man(0);
  unsigned classic=dc_hash((const unsigned char *)tx_man,bytes);
#if MX_CEL_DC_AVAILABLE
  if(!tx_wman)return 1;
  pvr_ptr_t wolf_atlas=tx_wman;
  unsigned wolf_classic=dc_hash((const unsigned char*)tx_wman,bytes);
#endif
  recolor_man(11); /* Working Man blue briefs */
  unsigned work=dc_hash((const unsigned char *)tx_man,bytes);
  recolor_man(4);  /* Camo Dad */
  unsigned camo=dc_hash((const unsigned char *)tx_man,bytes);
  recolor_man(0);
  unsigned restored=dc_hash((const unsigned char *)tx_man,bytes);
  if(atlas!=tx_man || classic!=restored || classic==work || classic==camo || work==camo){
    printf("FAIL: Dreamcast outfit tint parity / VRAM stability %08x %08x %08x %08x\n",
           classic,work,camo,restored);
    return 1;
  }
  mx_meta_init(&stats);mx_options_default(&console_opt);
  console_opt.art_style=1;recolor_man(0);
  if(cel_active||dc_hash((const unsigned char *)tx_man,bytes)!=classic){puts("FAIL locked Dreamcast art changed atlas");return 1;}
  for(int i=0;i<MX_STORY_LEVELS;i++)stats.stars[i]=1;
  recolor_man(0);unsigned painted=dc_hash((const unsigned char *)tx_man,bytes);
  if(!cel_active||painted==classic||tx_man!=atlas){puts("FAIL Dreamcast cel atlas not changed in place");return 1;}
#if MX_CEL_DC_AVAILABLE
  if(!dc_atlas_cel||mx_dc_frame(MSF_IDLE_A,dc_atlas_cel)!=MC_DCM_FR[MSF_IDLE_A]||
     tx_wman!=wolf_atlas||dc_hash((const unsigned char*)tx_wman,bytes)==wolf_classic){puts("FAIL Dreamcast alternate Wolfman/geometry");return 1;}
#endif
  console_opt.art_style=0;recolor_man(0);
#if MX_CEL_DC_AVAILABLE
  if(dc_atlas_cel||mx_dc_frame(MSF_IDLE_A,dc_atlas_cel)!=DCM_FR[MSF_IDLE_A]||
     tx_wman!=wolf_atlas||dc_hash((const unsigned char*)tx_wman,bytes)!=wolf_classic){puts("FAIL Dreamcast classic Wolfman restoration");return 1;}
  puts("DC_LIVE_CEL_ATLAS_PASS");
#endif
  if(cel_active||dc_hash((const unsigned char *)tx_man,bytes)!=classic||tx_man!=atlas){puts("FAIL Dreamcast classic atlas restoration");return 1;}
  puts("DC_GOLDEN_CELS_PASS locked, atlas, restoration");
  printf("ok: Dreamcast outfits round-trip identically in-place, permanent 1 MiB atlas not needed (%08x)\n",classic);
  return 0;
}
static int dc_fastpath_probe(void){
  static unsigned char idx[256*192];
  ds_gfx_init();
  tx_ic_top=pvr_mem_malloc(256*256*2);
  if(!tx_ic_top)return 1;
  for(int i=0;i<256*192;i++)idx[i]=(unsigned char)((i*23u+(i>>6)*11u)&255);
  ic_upload(tx_ic_top,idx);
  printf("DC_FAST_PALETTE=%08x\n",dc_hash((const unsigned char*)tx_ic_top,256*256*2));
  gfx_pal[29]^=0x1f;
  ic_upload(tx_ic_top,idx);
  printf("DC_FAST_PALETTE_ANIM=%08x\n",dc_hash((const unsigned char*)tx_ic_top,256*256*2));
  pvr_mem_free(tx_ic_top);
  /* The first-free hint must NOT change which of the 256 particle slots
     is occupied after repeated bursts and expiry/reuse. */
  memset(parts,0,sizeof parts);
  for(int wave=0;wave<7;wave++){
    for(int k=0;k<112;k++)
      spawn_k(PK_BIT,(float)(k%60), (float)(k%33),3.0f,-8.0f,
              0.2f+(k%5)*0.03f,argb(k&255,wave&255,77,255));
    for(int i=0;i<256;i++)
      if(parts[i].life>0 && (i+wave)%9==0) parts[i].life=0.01f;
    fx_step(0,0);
  }
  { unsigned h=2166136261u;
    for(int i=0;i<256;i++){
      h^=(unsigned)((int)(parts[i].life*100)+i*7+parts[i].kind);
      h*=16777619u;
      h^=(unsigned)(int)(parts[i].x*10);
      h*=16777619u;
    }
    printf("DC_PARTICLE_POOL=%08x\n",h);
  }
  /* The same helper that submits actual Dreamcast rubble polygons, in
     reference and culled modes. Render real textured rotated quads into
     the software PVR for a range of rotations and all screen edges.
     Identical framebuffer hashes prove invisible polygons are the ONLY
     ones omitted, not just equivalent vertex-count guesses. */
  {
    static unsigned short fake_atlas[512*512];
    memset(fake_atlas,0xff,sizeof fake_atlas);
    make_hdr(&hdr_atl,PVR_TXRFMT_ARGB1555,512,512,fake_atlas);
    memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
    for(int k=0;k<260;k++){
      float cx=(float)((k*131)%680)-20.f;
      float cy=(float)((k*97)%520)-20.f;
      float angle=(float)(k%33)*0.187f;
      tile_rot_visible(cx,cy,cosf(angle),sinf(angle),
                       MX_WOOD,k&3,k%3,Z_CHUNK);
    }
    for(int k=0;k<7;k++){
      const float xedge[]={-4.f,-3.9f,-3.2f,0.f,639.5f,643.8f,644.f};
      tile_rot_visible(xedge[k],240.f,cosf(k*.4f),sinf(k*.4f),
                       MX_STONE,k&3,0,Z_CHUNK);
      tile_rot_visible(320.f,k<4?xedge[k]:480.f+(xedge[k]-639.5f),
                       cosf(k*.4f),sinf(k*.4f),MX_WOOD,k&3,0,Z_CHUNK);
    }
    printf("DC_FAST_RUBBLE=%08x\n",dc_hash(fb,sizeof fb));
    /* Compare the actual renderer's whole-chunk painter under clipping.
       One body in view, one beyond view and one crossing the right edge. */
    memset(world.chunks,0,sizeof world.chunks);
    for(int m=1;m<MX_NMAT;m++)MAXHP[m]=(float)mx_material_hp(m);
    MxChunk *near=&world.chunks[0];
    near->active=1;near->count=18;near->x=53;near->y=22;near->rad=26;near->ang=0.41f;
    for(int k=0;k<18;k++){
      MxCell *c=&near->cells[k];c->m=(k&1)?MX_STONE:MX_WOOD;
      c->h=mx_material_hp(c->m);c->wx=41+(k*7)%24;c->wy=12+(k*11)%19;
      c->lx=c->wx-near->x;c->ly=c->wy-near->y;
      if(k%5==0){c->m=MX_TNT;c->h=mx_material_hp(MX_TNT);c->fuse=(uint8_t)(k&1?8:24);}
    }
    MxChunk *off=&world.chunks[1];
    off->active=1;off->count=2;off->x=500;off->y=300;off->rad=5;
    for(int k=0;k<2;k++){
      off->cells[k].m=MX_STONE;off->cells[k].h=mx_material_hp(MX_STONE);
      off->cells[k].wx=500+k;off->cells[k].wy=300+k;
    }
    MxChunk *edge=&world.chunks[2];
    edge->active=1;edge->count=2;edge->x=124;edge->y=30;edge->rad=8;edge->ang=1.3f;
    for(int k=0;k<2;k++){
      edge->cells[k].m=MX_WOOD;edge->cells[k].h=mx_material_hp(MX_WOOD);
      edge->cells[k].wx=118+k*6;edge->cells[k].wy=29+k;
      edge->cells[k].lx=edge->cells[k].wx-edge->x;
      edge->cells[k].ly=edge->cells[k].wy-edge->y;
    }
    memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
    draw_rubble_chunks();
    printf("DC_FAST_RUBBLE_CHUNKS=%08x\n",dc_hash(fb,sizeof fb));
  }
  return 0;
}
/* Title bitmap is freed for RAM during gameplay. Validate its complete
   VRAM lifecycle and the final-level transition against the REAL front-end,
   not a mock image/header. Can run alone as DC_MENU_FLOW_TEST=1. */
static int dc_menu_flow_test(void){
  int fail=0;
#define MENU_CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);fail++;}else printf("ok: %s\n",m);}while(0)
  MENU_CHECK(tx_ttl!=0,"title is initially allocated on the Dreamcast PVR");
  MENU_CHECK(tx_wman!=0 && hdr_wman.ptr==tx_wman,
             "Dreamcast allocates real dedicated 1024x512 Wolfman PVR texture");
  MENU_CHECK(DCT_WMAN_W==DCT_MAN_W && DCT_WMAN_H==DCT_MAN_H,
             "human and Wolfman atlas UV dimensions match without extra view transforms");
  /* Render the exact same flying pose from the two different atlases, as
     the real PVR will do. The former atlas alias rendered human features
     in place of Wolfman fur and muzzle for 32 of the 71 poses. */
  on_title=0;man_zoom=1;man_rot=0;man_fur=0;cur_hdr=0;
  memset(fb,0,sizeof fb);
  draw_frame(MSF_FLY_A,320,240,1,Z_MAN,NULL);
  unsigned human_face=dc_hash(fb,sizeof fb);
  man_fur=1;cur_hdr=0;memset(fb,0,sizeof fb);
  draw_frame(MSF_FLY_A,320,240,1,Z_MAN,NULL);
  unsigned wolf_face=dc_hash(fb,sizeof fb);
  MENU_CHECK(human_face!=wolf_face && human_face!=0 && wolf_face!=0,
             "Dreamcast wolf side profile is visibly distinct from the human face");
  man_fur=0;
  /* A walk used to draw its real side-profile sprite, then run the regular
     towel path with frame=-1. That read MS_HAND[-1] / DCM_FR[-1]. */
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  unsigned empty=dc_hash(fb,sizeof fb);
  man_with_towel(&tow_play,-1,320,240,1,Z_MAN,NULL);
  MENU_CHECK(dc_hash(fb,sizeof fb)==empty && nq==0,
             "negative walk sentinel never accesses/draws a nonexistent atlas frame");
  /* Both grounded stride extremes use valid unique sprite silhouettes.
     draw_all() must paint only the walking figure, with no -1 fallback. */
  for(int m=1;m<MX_NMAT;m++)MAXHP[m]=(float)mx_material_hp(m);
  load_level(22);on_title=0;world.state=MX_PLAY;
  world.man.mode=MAN_WALK;world.man.walked=1;world.man.gr=1;
  world.man.x=64.0f;world.man.y=56.625f;world.man.face=1;world.man.vx=9.0f;
  world.man.gait=0;memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  draw_all();
  unsigned left_foot=dc_hash(fb,sizeof fb);
  world.man.gait=2.0f;memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  draw_all();
  unsigned right_foot=dc_hash(fb,sizeof fb);
  MENU_CHECK(left_foot!=empty && right_foot!=empty && left_foot!=right_foot,
             "Dreamcast renders both grounded stride profiles without an invalid fallback");
  /* Exact production draw: with velocity 0 the atlas frame and foot lift are
     identical whether he is grounded or airborne. A pixel difference must
     therefore come from the ground-contact shadow, not a changed pose. */
  world.man.vx=0.0f;world.man.gait=0.0f;world.man.pT=0.0f;
  world.man.gr=1;memset(fb,0,sizeof fb);cur_hdr=0;nq=0;draw_all();
  unsigned contact=dc_hash(fb,sizeof fb);
  world.man.gr=0;memset(fb,0,sizeof fb);cur_hdr=0;nq=0;draw_all();
  unsigned airborne=dc_hash(fb,sizeof fb);
  MENU_CHECK(contact!=airborne && contact!=empty,
             "Dreamcast grounded walking contact renders independently of airborne pose");
  world.man.gr=1;world.man.pT=0.1f;
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;draw_all();
  unsigned punch=dc_hash(fb,sizeof fb);
  MENU_CHECK(punch!=contact && punch!=empty,
             "Dreamcast punch anticipation changes real rendered pixels");
  world.man.pT=0;
  /* Run the real collapse event handler at all four damage tiers. A wide
     facade should be legible, not fourteen particles stacked at one point.
     No new particle capacity is allocated; every event remains bounded. */
  for(int tier=0;tier<4;tier++){
    memset(parts,0,sizeof parts);part_high_water=0;
#if MX_DC_PARTICLE_HINT
    part_free_hint=0;
#endif
    world.nev=1;
    world.ev[0]=(MxEvent){EV_CRUMBLE,0,RI(50),RI(25),
                           RI(tier==0?1:tier==1?8:tier==2?24:48),0};
    handle_events();
    int n=8+2*tier;
    MENU_CHECK(part_high_water==n,
               "Dreamcast collapse keeps bounded particle counts at each severity");
    float left=parts[0].x,right=parts[0].x,top_y=parts[0].y,low_y=parts[0].y;
    for(int k=1;k<n;k++){
      if(parts[k].x<left)left=parts[k].x;
      if(parts[k].x>right)right=parts[k].x;
      if(parts[k].y<top_y)top_y=parts[k].y;
      if(parts[k].y>low_y)low_y=parts[k].y;
    }
    MENU_CHECK(right-left>0.1f && low_y-top_y>0.1f,
               "Dreamcast collapse debris covers both facade width and height");
  }
  world.nev=0;memset(parts,0,sizeof parts);part_high_water=0;
#if MX_DC_PARTICLE_HINT
  part_free_hint=0;
#endif
  on_title=1;title_t=0;title_lv=0;nofight=0;cur_hdr=0;
  memset(fb,0,sizeof fb);draw_title();
  unsigned before=dc_hash(fb,sizeof fb);
  pvr_mem_free(tx_ttl);tx_ttl=0;
  /* The old code would draw a stale PVR header pointing into freed VRAM. */
  memset(fb,0,sizeof fb);cur_hdr=0;draw_title();
  MENU_CHECK(dc_hash(fb,sizeof fb)!=before,
             "missing title allocation displays safe fallback instead of stale texture");
  dc_return_to_title(0);
  MENU_CHECK(on_title && !paused && tx_ttl!=0,
             "returning to title reacquires the freed PVR texture");
  MENU_CHECK(tx_ttl==hdr_ttl.ptr,
             "restored PVR header points to the new valid texture address");
  memset(fb,0,sizeof fb);cur_hdr=0;draw_title();
  MENU_CHECK(dc_hash(fb,sizeof fb)==before,
             "restored title image is byte-identical to its initial drawing");
  pvr_mem_free(tx_ttl);tx_ttl=0;
  on_title=0;world.level=MX_NUM_LEVELS-1;world.state=MX_WIN;
  dc_end_input(CONT_START);
  MENU_CHECK(on_title && !paused && title_lv==0 && tx_ttl!=0,
             "winning final level returns to playable title instead of level out of bounds");
  on_title=0;load_level(7);world.state=MX_LOSE;
  dc_end_input(CONT_START);
  MENU_CHECK(!on_title && world.level==7 && world.state==MX_PLAY,
             "losing a level retries that same valid level");
  on_title=0;load_level(11);world.state=MX_LOSE;
  dc_end_input(CONT_B);
  MENU_CHECK(on_title && title_lv==11 && tx_ttl!=0,
             "B on a loss screen returns to title instead of forcing a retry");
  on_title=0;load_level(13);world.state=MX_WIN;
  dc_end_input(CONT_B|CONT_START);
  MENU_CHECK(on_title && title_lv==13 && tx_ttl!=0,
             "B on a win screen takes priority over START and returns to title");
  dc_return_to_title(7);
  MENU_CHECK(on_title && title_lv==7 && tx_ttl!=0,
             "pause-menu exit to title preserves the current level selection");
  /* Exercise the exact predicate used by the production Dreamcast main
     loop. Unlike the cinematic timeline, gameplay must stop completely
     during intro/end movies, title screens, and paused frames. */
  load_level(4); on_title=0;paused=0;in_intro=0;
  float t0=world.time;
  MENU_CHECK(dc_world_tick_allowed(),"normal Dreamcast gameplay can simulate");
  if(dc_world_tick_allowed())tick(0);
  MENU_CHECK(world.time>t0,"active Dreamcast gameplay advances physics");
  t0=world.time;in_intro=1;
  for(int i=0;i<900;i++)if(dc_world_tick_allowed())tick(0);
  MENU_CHECK(world.time==t0 && world.state==MX_PLAY,
             "900 cutscene frames freeze physics and cannot decide a level");
  in_intro=0;paused=1;
  for(int i=0;i<90;i++)if(dc_world_tick_allowed())tick(0);
  MENU_CHECK(world.time==t0,"pausing freezes the Dreamcast world");
  paused=0;on_title=1;
  for(int i=0;i<90;i++)if(dc_world_tick_allowed())tick(0);
  MENU_CHECK(world.time==t0,"title UI frames cannot advance hidden gameplay");
  on_title=0;
  if(dc_world_tick_allowed())tick(0);
  MENU_CHECK(world.time>t0,"world simulation resumes after the intro/pause");
  printf(fail?"Dreamcast menu flow: %d FAILED\n":"Dreamcast menu flow: all passed\n",fail);
  return fail!=0;
}
/* Hash the precise old-vs-new material damage classifier over a dense
   spread of fractional HP values around both thresholds and full health.
   The fast branch may ONLY skip division for truly pristine material. */
static int dc_damage_fastpath_probe(void){
  unsigned hash=2166136261u;
  unsigned bins[3]={0,0,0};
  for(int m=1;m<MX_NMAT;m++)MAXHP[m]=(float)mx_material_hp(m);
  for(int m=1;m<MX_NMAT;m++){
    for(int j=-300;j<=12000;j++){
      float hp=(float)j*MAXHP[m]/10000.0f;
      int d=dmg(hp,m);
      if(d<0||d>2){puts("FAIL: invalid Dreamcast damage stage");return 1;}
      bins[d]++;
      hash=(hash^(unsigned)(m*17+j*3+d*113))*16777619u;
    }
    if(dmg(MAXHP[m],m)!=0){puts("FAIL: pristine material must remain pristine");return 1;}
  }
  if(!bins[0]||!bins[1]||!bins[2]){puts("FAIL: damage probe lacks all three art stages");return 1;}
  printf("DC_DAMAGE_CLASS_HASH cases=%u intact=%u chipped=%u cracked=%u checksum=%08x\n",
         bins[0]+bins[1]+bins[2],bins[0],bins[1],bins[2],hash);
  return 0;
}
#ifdef MX_DC_PARTICLE_BENCH
static int dc_particle_bounds_probe(void){
  memset(parts,0,sizeof parts);part_high_water=0;
#if MX_DC_PARTICLE_HINT
  part_free_hint=0;
#endif
  for(int k=0;k<8;k++) spawn_k(PK_BIT,20.f+k,15.f,1.f,0.f,.8f,argb(210,176,130,255));
  if(part_high_water!=8){fputs("FAIL: DC particle highwater allocation\n",stderr);return 1;}
  for(int k=5;k<8;k++)parts[k].life=.001f;
  fx_step(0,0);
  if(part_high_water!=5){fputs("FAIL: DC expired high-water tail\n",stderr);return 1;}
  spawn_k(PK_BIT,27.f,16.f,1.f,0.f,.8f,argb(210,176,130,255));
  if(part_high_water!=6){fputs("FAIL: DC particle tail reuse\n",stderr);return 1;}
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  draw_particles();
  printf("DC_PARTICLE_BOUNDS_HASH %08x\n",dc_hash(fb,sizeof fb));
  printf("DC_PARTICLE_BOUNDS_SCANNED %d\n",MX_DC_PARTICLE_BOUNDS?part_high_water:256);
  return 0;
}
/* Render identical sprite scenes through reference and culled production
 * painters. The PVR emulator compares all 640x480 pixels byte-for-byte. */
static int dc_particle_view_probe(void){
  memset(parts,0,sizeof parts);
  part_high_water=256; /* synthetic fixture bypasses spawn_k() */
  for(int i=0;i<256;i++){
    Part *p=&parts[i];
    p->x=(float)((i*131)%1000)-175.0f;
    p->y=(float)((i*97)%700)-125.0f;
    p->life=.7f;p->max=1.0f;p->kind=i%7;
    p->c=argb(150+(i&31),110+(i&63),155+(i&47),230);
  }
  world.time=2.5f;
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  dc_particles_rasterized=0;
  draw_particles();
  printf("DC_PARTICLE_VIEW_HASH %08x\n",dc_hash(fb,sizeof fb));
  printf("DC_PARTICLE_VIEW_RASTERIZED %u\n",dc_particles_rasterized);
  return 0;
}
#endif
/* Real ARGB4444 PVR atlas/rasterizer pressure check, including
   monochrome fur recolour and a conservative body-size envelope. */
static int dc_pressure_probe(void){
  if(!tx_atl||!tx_man){puts("FAIL: Dreamcast pressure atlas missing");return 1;}
  recolor_man(0);
  const uint32 skin=belly_tone[0];
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  soft_oval(320,240,27,23,Z_MAN+.4f,skin);
  int ink=0,minx=640,maxx=-1,miny=480,maxy=-1;
  for(int y=0;y<480;y++)for(int x=0;x<640;x++){
    const unsigned char *p=&fb[(y*640+x)*3];
    if(!(p[0]|p[1]|p[2]))continue;
    ink++;if(x<minx)minx=x;if(x>maxx)maxx=x;
    if(y<miny)miny=y;if(y>maxy)maxy=y;
  }
  if(ink<150||maxx-minx>56||maxy-miny>48){
    fprintf(stderr,"FAIL: Dreamcast pressure oval bounds ink=%d w=%d h=%d\n",
            ink,maxx-minx+1,maxy-miny+1);
    return 1;
  }
  recolor_man(12);
  if(!man_fur || belly_tone[0]==skin){
    fputs("FAIL: Dreamcast pressure is not recoloured for Wolfman\n",stderr);
    return 1;
  }
  memset(fb,0,sizeof fb);cur_hdr=0;nq=0;
  soft_oval(320,240,27,23,Z_MAN+.4f,belly_tone[0]);
  int grey=0;
  for(int y=220;y<=260;y++)for(int x=298;x<=342;x++){
    const unsigned char *p=&fb[(y*640+x)*3];
    if((int)p[0]+(int)p[1]+(int)p[2]>30 &&
       abs((int)p[0]-(int)p[1])<18 &&
       abs((int)p[1]-(int)p[2])<34)grey++;
  }
  if(grey<80){fprintf(stderr,"FAIL: Dreamcast grey fur detail count=%d\n",grey);return 1;}
  printf("DC_PRESSURE_PASS ink=%d span=%dx%d wolf_gray=%d\n",
         ink,maxx-minx+1,maxy-miny+1,grey);
  return 0;
}
int main(void){
  if(getenv("DC_DAMAGE_FASTPATH_PROBE"))return dc_damage_fastpath_probe();
  if(getenv("DC_FASTPATH_PROBE"))return dc_fastpath_probe();
  gfx_init();
#ifdef MX_DC_PARTICLE_BENCH
  if(getenv("DC_PARTICLE_BOUNDS_PROBE"))return dc_particle_bounds_probe();
  if(getenv("DC_PARTICLE_VIEW_PROBE"))return dc_particle_view_probe();
#endif
  if(getenv("DC_OUTFIT_RECOLOR_PROBE"))return dc_outfit_memory_probe();
  if(getenv("DC_PRESSURE_PROBE"))return dc_pressure_probe();
  sound_init(); if(!snd_ok){ puts("FAIL: dc sound did not initialise"); return 1; }
  if(getenv("DC_MENU_FLOW_TEST"))return dc_menu_flow_test();
  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? (float)mx_material_hp(k) : 1.0f;
  memset(fb,0,sizeof fb); title_t=0; cur_hdr=0; draw_title(); dump("/tmp/dc_title.ppm"); title_t=40; cur_hdr=0; draw_title(); dump("/tmp/dc_title_blink.ppm");   /* the title, and the half of the blink with no PRESS START */
  { const int T[3] = { 30, 170, 330 }; for(int k = 0; k < 3; k++){ char fn[48]; snprintf(fn, sizeof fn, "/tmp/dc_tpan%d.ppm", k); memset(fb,0,sizeof fb); title_t = T[k]; cur_hdr=0; draw_title(); dump(fn); } }   /* the pan: logo end, mid glide, the man */
  if(getenv("DC_INTRO")){                                                  /* the shared cutscene player through the real draw_intro() */
    ds_gfx_init(); tx_ic_top = pvr_mem_malloc(256 * 256 * 2); tx_ic_bot = pvr_mem_malloc(256 * 256 * 2); make_hdr(&hdr_ic_top, PVR_TXRFMT_RGB565, 256, 256, tx_ic_top); make_hdr(&hdr_ic_bot, PVR_TXRFMT_RGB565, 256, 256, tx_ic_bot);
    for(int w = 0; w < 2; w++){ if(w) intro_start_wolf(); else intro_start(); int lastsc = -1, done = 0, f;
      for(f = 0; f < 6000 && !done; f++){ int dn = (f % 400 == 399) ? 1 : 0; done = intro_tick(dn, 0, 0);
        if(!done && (intro_scene() != lastsc || f % 150 == 149)){ lastsc = intro_scene(); char fn[64]; snprintf(fn, sizeof fn, "/tmp/dc_intro%d_%d_%d.ppm", w, lastsc, f); memset(fb, 0, sizeof fb); cur_hdr = 0; draw_intro(); dump(fn); } }
      if(!stub_snd_plays){ puts("FAIL: dc cutscene played no sound"); return 1; }
      printf("dc intro %s: %s after %d frames, %d scenes\n", w ? "wolf" : "origin", done ? "finished" : "NOT finished", f, intro_count()); }
    return 0; }
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
  printf("%s dc sound effects played during the run: %d\n", stub_snd_plays > 3 ? "ok:" : "FAIL:", stub_snd_plays); return stub_snd_plays > 3 ? 0 : 1; }
