/*
 * Host harness for the 3DS front-end: runs the REAL 3ds/main.c loop against test/stubs/3ds.h with scripted pad input, plays level 1 to a win, and dumps the
 * framebuffers as PPM.  Checks the end-panel stars (3, 2 and 1 men left), NEW BEST! and the best score on the title.
 *
 *   gcc -std=gnu99 -O1 -Istubs -I../core -I../art n3ds_render.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o n3ds_render
 *   ./n3ds_render [outprefix] [slider 0..1]        (the slider 0 and 1 runs both draw; 1 draws both eyes)
 * It stands in for no GPU and no real libctru: it says nothing about timing, and the 3DS build has still never run on hardware or an emulator.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "3ds.h"

static u8 fb_top[2][400 * 240 * 3], fb_bot[320 * 240 * 3];
static float g_slider; static int g_keys, g_prev, g_dn, g_up;
static touchPosition g_touch;
void osSetSpeedupEnable(bool b){ (void)b; } void gfxInitDefault(void){} void gfxSetDoubleBuffering(gfxScreen_t s, bool b){ (void)s; (void)b; } void gfxSet3D(bool b){ (void)b; }
void gfxFlushBuffers(void){} void gfxSwapBuffers(void){} void gspWaitForVBlank(void){} void gfxExit(void){}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t e, u16 *w, u16 *h){ (void)w; (void)h; return s == GFX_BOTTOM ? fb_bot : fb_top[e == GFX_RIGHT]; }
float osGet3DSliderState(void){ return g_slider; }
void hidScanInput(void){ g_dn = g_keys & ~g_prev; g_up = g_prev & ~g_keys; g_prev = g_keys; }
u32 hidKeysDown(void){ return g_dn; } u32 hidKeysHeld(void){ return g_keys; } u32 hidKeysUp(void){ return g_up; }
void hidTouchRead(touchPosition *t){ *t = g_touch; }

#define MX_TEST_NO_CUTSCENES 1
int stub_snd_plays;
#define main n3ds_main
#include "../3ds/main.c"
#undef main

static const char *OUT = "/tmp/n3r";
static int fails;
#define CHECK(c, msg) do{ if(c) printf("ok: %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } }while(0)
static void dump(const u8 *fb, int w, int h, const char *fn){
  FILE *f = fopen(fn, "wb"); if(!f) return; fprintf(f, "P6\n%d %d\n255\n", w, h);
  for(int y = 0; y < h; y++) for(int x = 0; x < w; x++){ const u8 *p = fb + (x * h + (h - 1 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static void shots(const char *name){
  char p[256];
  snprintf(p, sizeof p, "%s_%s_top.ppm", OUT, name); dump(fb_top[0], 400, 240, p);
  if(g_slider > .03f){ snprintf(p, sizeof p, "%s_%s_topR.ppm", OUT, name); dump(fb_top[1], 400, 240, p); }
  snprintf(p, sizeof p, "%s_%s_bot.ppm", OUT, name); dump(fb_bot, 320, 240, p);
}
static u32 topix(int eye, int x, int y){ const u8 *p = fb_top[eye] + (x * 240 + (239 - y)) * 3; return (u32)p[2] << 16 | (u32)p[1] << 8 | p[0]; }   /* 0xBBGGRR as RGB() writes it */
static int gold(u32 c){ return c == RGB(255, 210, 63); }
/* how many of the three star slots hold gold (a 3x3 patch around each star centre, so one stray pixel cannot fool it) */
static int stars_lit(int eye){ int n = 0; for(int i = 0; i < 3; i++){ int g = 0; for(int dy = -1; dy <= 1; dy++) for(int dx = -1; dx <= 1; dx++) g += gold(topix(eye, 200 + (i - 1) * 30 + dx, 128 + dy)); n += g == 9; } return n; }

/* the scripted player lives in aptMainLoop(): the real loop calls it once per frame, after drawing, so the framebuffers hold the frame just drawn */
bool aptMainLoop(void){
  static int ph, t, hold, shot; static const int HOLD[] = { 17, 19, 15, 21, 13, 23, 18, 20 };
  int eyes = g_slider > .03f ? 2 : 1;
  t++; g_keys = 0;
  switch(ph){
  case 0:                                                        /* fresh title: nothing played yet */
    if(t == 4){ CHECK(stats.best[0] == 0, "fresh title: level 1 has no best score"); shots("title0"); }
    if(t == 6) g_keys = KEY_A;
    if(t == 7){ ph = 1; t = 0; }
    break;
  case 1:                                                        /* wait for the sling, or for the end */
    if(world.state != MX_PLAY){ ph = 3; t = 0; break; }
    if(world.man.mode == MAN_AIM && t > 10 && t < 13 && (shot == 0 || shot == 2)){ shots(shot ? "aim2" : "aim0"); }
    if(world.man.mode == MAN_AIM && t > 10){ hold = HOLD[shot % 8]; ph = 2; t = 0; }
    if(shot > 24){ CHECK(0, "level 1 reached an end panel within 24 shots"); return false; }
    break;
  case 2:                                                        /* pull back with the pad, then let go */
    if(t <= hold) g_keys = KEY_DLEFT; else if(t == hold + 1) g_keys = KEY_A; else { shot++; ph = 1; t = 0; }
    break;
  case 3:                                                        /* end panel: wait for the blink to be on, then look at it */
    if(t < 20 || !(frame & 16)) break;
    CHECK(world.state == MX_WIN, "level 1 was won");
    CHECK(new_best == 1, "first win is a NEW BEST");
    { int pink = 0; for(int y = 146; y < 156; y++) for(int x = 150; x < 250; x++) pink += topix(0, x, y) == RGB(255, 126, 176); CHECK(pink > 20, "NEW BEST! text is on the panel (left eye)"); }
    printf("  men left %d -> %d stars\n", world.men, mx_stars_for(&world));
    CHECK(stars_lit(0) == mx_stars_for(&world), "lit stars on the panel equal mx_stars_for (left eye)");
    if(eyes == 2) CHECK(stars_lit(1) == mx_stars_for(&world), "lit stars on the panel equal mx_stars_for (right eye)");
    CHECK(stats.best[0] > 0 && stats.stars[0] == mx_stars_for(&world), "stats hold the best score and stars");
    shots("win");
    world.men = 2; ph = 4; t = 0; break;                         /* redraw the same panel with other men counts: only the star helper and draw_end read it */
  case 4: CHECK(stars_lit(0) == 3 && (eyes == 1 || stars_lit(1) == 3), "2 men left: 3 stars lit"); shots("win_m2"); world.men = 1; ph = 5; t = 0; break;
  case 5: CHECK(stars_lit(0) == 2 && (eyes == 1 || stars_lit(1) == 2), "1 man left: 2 stars lit"); shots("win_m1"); world.men = 0; ph = 6; t = 0; break;
  case 6: CHECK(stars_lit(0) == 1 && (eyes == 1 || stars_lit(1) == 1), "0 men left: 1 star lit"); shots("win_m0"); ph = 7; t = 0; break;
  case 7: g_keys = KEY_B; ph = 8; t = 0; break;                  /* back to the title */
  case 8:
    if(t < 4) break;
    { int gold_px = 0; for(int y = 163; y < 169; y++) for(int x = 140; x < 260; x++) gold_px += gold(topix(0, x, y)); CHECK(app == ST_TITLE && gold_px > 20, "title shows the best score line after a win"); }
    shots("title1");
    printf("%s\n", fails ? "FAILED" : "all 3DS checks passed"); return false;
  }
  return true;
}

/* Frame/palette checksum used by CI to prove both optimized 3DS paths
   produce the exact same pixels as the compile-time reference variants. */
static unsigned n3_hash(const u8 *p, size_t n){
  unsigned h=2166136261u;
  for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}
  return h;
}
static int n3_fastpath_probe(void){
  static u8 indices[256*192];
  gfx_init();
  for(int i=0;i<256*192;i++)indices[i]=(u8)((i*29u+(i>>4)*7u)&255);
  top.fb=fb_top[0];top.w=400;top.h=240;
  bot.fb=fb_bot;bot.w=320;bot.h=240;
  blit_pal(&top,indices);blit_pal(&bot,indices);
  printf("N3_FAST_PALETTE top=%08x bottom=%08x\n",
         n3_hash(fb_top[0],sizeof fb_top[0]),n3_hash(fb_bot,sizeof fb_bot));
  /* Explicitly exercise an animated palette change, not only init colors. */
  gfx_pal[29]^=0x1f;
  blit_pal(&top,indices);
  printf("N3_FAST_PALETTE_ANIM=%08x\n",n3_hash(fb_top[0],sizeof fb_top[0]));
  /* Disc/ring primitive: radius 0 through 45, coordinates inside and
     deliberately beyond all four edges. Reference/optimized code must
     generate the identical BGR8 column-major framebuffer. */
  memset(fb_top,0,sizeof fb_top);
  top.fb=fb_top[0]; top.w=400; top.h=240;
  for(int r=0;r<=45;r++){
    disc(&top, (r*37)%450-25, (r*41)%292-26, r,
         RGB((r*11)&255,(r*17)&255,(r*23)&255));
    disc(&top, 396-r*3, 237-r, r/2,
         RGB((r*29)&255,(r*7)&255,(r*13)&255));
  }
  printf("N3_FAST_CIRCLES=%08x\n",n3_hash(fb_top[0],sizeof fb_top[0]));
  /* Spawn bursts interleaved with expiry must allocate exactly the same
     lowest free indices with and without the cursor optimization. */
  memset(parts,0,sizeof parts);
  for(int wave=0;wave<7;wave++){
    for(int k=0;k<83;k++)
      spawn_k((float)(k+wave), (float)(k%33), 0.2f,-0.3f,
              2+(k%5), RGB(k&255,wave&255,77), k%3);
    for(int j=0;j<NP;j++)
      if(parts[j].life>0 && ((j+wave)%7)==0)parts[j].life=1;
    tick_parts();
  }
  { unsigned h=2166136261u;
    for(int j=0;j<NP;j++){
      h^=(unsigned)((parts[j].life+2)*23u+j*7u+parts[j].kind);
      h*=16777619u;
      h^=(unsigned)(parts[j].x*10);
      h*=16777619u;
    }
    printf("N3_PARTICLE_POOL=%08x\n",h);
  }
  load_level(0);
  mx_set_pull(&world,R(-9),R(5));
  dragging=1;
  View vt={&top,3,20,28,0,0},vb={&bot,4,0,0,-24,0};
  for(int f=0;f<3;f++){
    frame=200+f;
    if(f==2)mx_set_pull(&world,R(-7),R(6));
    memset(fb_top,0,sizeof fb_top);
    memset(fb_bot,0,sizeof fb_bot);
    draw_aim(&vt);
    /* Right-eye and lower-screen projections reuse world points. */
    top.fb=fb_top[1];draw_aim(&vt);top.fb=fb_top[0];
    draw_aim(&vb);
    printf("N3_FAST_AIM frame=%d left=%08x right=%08x bottom=%08x\n",
           f,n3_hash(fb_top[0],sizeof fb_top[0]),
           n3_hash(fb_top[1],sizeof fb_top[1]),n3_hash(fb_bot,sizeof fb_bot));
  }
  /* Exercise the ACTUAL standing-cell and moving-rubble drawing with
     artificial chunks that straddle all four screen edges. Both render
     variants must hash to precisely identical stereo+bottom bytes. */
  {
    memset(world.chunks,0,sizeof world.chunks);
    MxChunk *ch=&world.chunks[0];
    ch->active=1;ch->count=24;
    ch->x=R(77);ch->y=R(40);ch->rad=R(95); /* bound the synthetic cells */
    for(int k=0;k<24;k++){
      MxCell *a=&ch->cells[k];a->dead=0;a->m=(k&1)?MX_STONE:MX_WOOD;
      a->wx=R((k*13)%157);a->wy=R((k*11)%79);
    }
    /* Completely remote rubble plus a chunk crossing the screen edge. */
    MxChunk *far=&world.chunks[1];
    far->active=1;far->count=3;far->x=R(400);far->y=R(300);far->rad=R(5);
    for(int k=0;k<3;k++){
      far->cells[k].m=MX_STONE;far->cells[k].wx=R(398+k);far->cells[k].wy=R(300+k);
    }
    MxChunk *edge=&world.chunks[2];
    edge->active=1;edge->count=3;edge->x=R(15);edge->y=R(18);edge->rad=R(18);
    for(int k=0;k<3;k++){
      edge->cells[k].m=MX_WOOD;edge->cells[k].wx=R(3+k*12);edge->cells[k].wy=R(18);
    }
    View vc={&top,4,120,50,45,18};
    View vr={&top,4,122,50,45,18};
    View vb={&bot,4,70,35,45,18};
    for(int sample=0;sample<3;sample++){
      g_dim=sample==0?1.0f:sample==1?0.7f:0.93f;
      ch->cells[sample].wx=R(45);ch->cells[sample].wy=R(18);
      memset(fb_top,0,sizeof fb_top);memset(fb_bot,0,sizeof fb_bot);
      top.fb=fb_top[0];draw_cells(&vc);
      top.fb=fb_top[1];draw_cells(&vr);
      top.fb=fb_top[0];draw_cells(&vb);
      printf("N3_FAST_RUBBLE sample=%d left=%08x right=%08x bottom=%08x\n",
             sample,n3_hash(fb_top[0],sizeof fb_top[0]),
             n3_hash(fb_top[1],sizeof fb_top[1]),
             n3_hash(fb_bot,sizeof fb_bot));
    }
  }
  return 0;
}
/* Verify a detached 3DS wall fragment retains its existing damage
   texture rather than snapping back to a pristine masonry sprite. */
static int n3_rubble_damage_probe(void){
  gfx_init();g_dim=1.0f;
  top.fb=fb_top[0];top.w=400;top.h=240;
  memset(world.grid,0,sizeof world.grid);
  memset(world.chunks,0,sizeof world.chunks);
  MxChunk *ch=&world.chunks[0];
  ch->active=1;ch->count=1;ch->x=R(40);ch->y=R(25);ch->rad=R(2);
  MxCell *a=&ch->cells[0];
  a->m=MX_WOOD;a->wx=ch->x;a->wy=ch->y;
  View view={&top,4,20,5,0,0};
  memset(fb_top[0],0,sizeof fb_top[0]);
  a->h=R(mx_material_hp(MX_WOOD));
  draw_cells(&view);
  unsigned intact=n3_hash(fb_top[0],sizeof fb_top[0]);
  memset(fb_top[0],0,sizeof fb_top[0]);
  a->h=R(1);
  draw_cells(&view);
  unsigned cracked=n3_hash(fb_top[0],sizeof fb_top[0]);
  if(intact==cracked){fputs("FAIL: 3DS damaged rubble reuses pristine texture\n",stderr);return 1;}
  printf("N3_RUBBLE_DAMAGE_PASS intact=%08x damaged=%08x\n",intact,cracked);
  return 0;
}
#ifdef MX_N3DS_PARTICLE_BENCH
static int n3_particle_bounds_probe(void){
  gfx_init();top.fb=fb_top[0];top.w=400;top.h=240;
  memset(parts,0,sizeof parts);memset(pops,0,sizeof pops);part_high_water=0;
#if MX_N3DS_PARTICLE_HINT
  part_free_hint=0;
#endif
  for(int k=0;k<8;k++)spawn_k(20.f+k,15.f,.1f,0.f,14,RGB(206,165,120),0);
  if(part_high_water!=8){fputs("FAIL: 3DS highwater allocation\n",stderr);return 1;}
  for(int k=5;k<8;k++)parts[k].life=1;
  tick_parts();
  if(part_high_water!=5){fputs("FAIL: 3DS highwater tail expiry\n",stderr);return 1;}
  spawn_k(27.f,16.f,.1f,0.f,14,RGB(206,165,120),0);
  if(part_high_water!=6){fputs("FAIL: 3DS highwater reuse\n",stderr);return 1;}
  View v={&top,3,50,22,30,10};
  memset(fb_top[0],0,sizeof fb_top[0]);
  draw_parts(&v);
  printf("N3_PARTICLE_BOUNDS_HASH %08x\n",n3_hash(fb_top[0],sizeof fb_top[0]));
  printf("N3_PARTICLE_BOUNDS_SCANNED %d\n",MX_N3DS_PARTICLE_BOUNDS?part_high_water:NP);
  return 0;
}
/* Exercise the real sprite painter on both stereoscopic top-eye projections
 * and on the bottom screen, including deliberately offscreen effects. */
static int n3_particle_view_probe(void){
  gfx_init();top.fb=fb_top[0];top.w=400;top.h=240;
  bot.fb=fb_bot;bot.w=320;bot.h=240;
  memset(parts,0,sizeof parts);
  part_high_water=NP; /* generated positions are written without spawn_k() */
  for(int i=0;i<NP;i++){
    Part *p=&parts[i];
    p->x=(float)((i*37)%210)-42.0f;
    p->y=(float)((i*47)%130)-29.0f;
    p->life=15+(i%8);p->kind=i%3;p->c=RGB(150+(i&31),100+(i&63),135+(i&47));
  }
  View vt={&top,3,120,60,45,18},vr={&top,3,124,60,45,18},vb={&bot,4,70,35,45,18};
  memset(fb_top,0,sizeof fb_top);memset(fb_bot,0,sizeof fb_bot);
  n3_particles_rasterized=0;
  draw_parts(&vt);top.fb=fb_top[1];draw_parts(&vr);draw_parts(&vb);
  printf("N3_PARTICLE_VIEW_HASH %08x %08x %08x\n",
         n3_hash(fb_top[0],sizeof fb_top[0]),n3_hash(fb_top[1],sizeof fb_top[1]),
         n3_hash(fb_bot,sizeof fb_bot));
  printf("N3_PARTICLE_VIEW_RASTERIZED %u\n",n3_particles_rasterized);
  return 0;
}
#endif
/* Art-quality check on the REAL renderer, not pre-rendered mockups.
   Verify all 142 human/Wolfman frames retain their silhouette as the
   torso turns, and that a 4-tap rotation actually blends fine outlines
   against the scene (rather than chopping hair/fingers from the cutout). */
static int n3_sprite_volume_probe(void){
  const u32 bg=RGB(7,34,61);
  top.fb=fb_top[0];top.w=400;top.h=240;
  apply_outfit(0);
  /* Exercise the production walking renderer, not only standalone sprites.
     With vx=0 the grounded and airborne frames are identical except for the
     deliberately anchored foot shadow. Test each stereo eye projection. */
  MxMan prior=world.man;
  world.man.mode=MAN_WALK;world.man.walked=1;world.man.face=1;
  world.man.x=R(60);world.man.y=R(40);world.man.vx=0;
  world.man.gait=0;world.man.pT=0;world.man.hurtCd=0;
  for(int eye=0;eye<2;eye++){
    View walkview={&top,4,20+eye*2,5,0,0};
    world.man.gr=1;
    rect(&top,0,0,400,240,bg);draw_man(&walkview);
    unsigned contact=n3_hash(top.fb,(size_t)top.w*top.h*3);
    world.man.gr=0;
    rect(&top,0,0,400,240,bg);draw_man(&walkview);
    unsigned airborne=n3_hash(top.fb,(size_t)top.w*top.h*3);
    if(contact==airborne){
      fprintf(stderr,"FAIL: 3DS walking contact shadow invisible in eye %d\\n",eye);
      return 1;
    }
    world.man.gr=1;world.man.pT=R(0.1);
    rect(&top,0,0,400,240,bg);draw_man(&walkview);
    unsigned punch=n3_hash(top.fb,(size_t)top.w*top.h*3);
    if(punch==contact){
      fprintf(stderr,"FAIL: 3DS punch animation invisible in eye %d\\n",eye);
      return 1;
    }
    printf("N3_WALK_IMPACT eye=%d ground=%08x air=%08x punch=%08x\\n",
           eye,contact,airborne,punch);
    world.man.pT=0;
  }
  world.man=prior;
  u32 original=man_tone[0][5],shadow=man_tone[2][5];
  if(original==shadow){puts("FAIL: 3DS contour palette lacks shaded side");return 1;}
  apply_outfit(3);
  if(original==man_tone[0][5]){
    puts("FAIL: 3DS contour colors do not follow the selected outfit");return 1;
  }
  apply_outfit(0);
  unsigned long long occupancy=1469598103934665603ull;
  unsigned filled=0,soft=0,lit=0;
  unsigned long long rotated_hash=1469598103934665603ull;
  for(int fr=0;fr<MSF_COUNT;fr++)for(int face=-1;face<=1;face+=2){
    rect(&top,0,0,400,240,bg);
    blit_frame(&top,fr,200,120,4,face);
    for(int y=30;y<211;y++)for(int x=120;x<281;x++){
      u8 *p=top.fb+(x*top.h+(top.h-1-y))*3;
      u32 pixel=RGB(p[2],p[1],p[0]);
      unsigned active=pixel!=bg;filled+=active;
      occupancy^=active;occupancy*=1099511628211ull;
      if(active){
        int matchesBase=0;
        for(int i=0;i<MS_NPAL;i++)if(pixel==man_tone[0][i]){matchesBase=1;break;}
        if(!matchesBase)lit++;
      }
    }
  }
  if(!filled){puts("FAIL: 3DS original and Wolfman poses lost their bodies");return 1;}
#if MX_N3DS_SPRITE_VOLUME
  if(!lit){puts("FAIL: 3DS volume normals never shade sprite contours");return 1;}
#else
  if(lit){puts("FAIL: 3DS reference sprite used extra contour colors");return 1;}
#endif
  for(int face=-1;face<=1;face+=2)
  for(int k=0;k<8;k++){
    rect(&top,0,0,400,240,bg);
    blit_frame_rot(&top,MSF_SPIN_6,200,120,3,face,(k+1)*.27f);
    for(int y=44;y<197;y++)for(int x=144;x<257;x++){
      u8 *p=top.fb+(x*top.h+(top.h-1-y))*3;
      u32 pixel=RGB(p[2],p[1],p[0]);
      rotated_hash^=pixel;rotated_hash*=1099511628211ull;
      if(pixel==bg)continue;
      int full=0;
      for(int s=0;s<3;s++)for(int i=0;i<MS_NPAL;i++)
        if(pixel==man_tone[s][i])full=1;
      if(!full)soft++;
    }
  }
#if MX_N3DS_ROT_COVERAGE
  if(!soft){puts("FAIL: 3DS rotated outline lacks blended subpixel edges");return 1;}
#endif
  printf("N3_SPRITE_OCCUPANCY %016llx pixels=%u\n",occupancy,filled);
  printf("N3_SPRITE_CONTOURS shaded=%u blended_rotated=%u\n",lit,soft);
  printf("N3_SPRITE_ROT_HASH %016llx\n",rotated_hash);
#ifdef MX_N3DS_ROT_WORK_COUNT
  printf("N3_ROT_WORK candidate=%u sampled=%u\n",man_rot_candidate_pixels,man_rot_sampled_pixels);
#endif
  return 0;
}
/* The same real four-tap renderer with its tight bounds disabled/enabled.
   Screens cover both 3DS software framebuffers, left/right facing, human
   and Wolfman, thin/wide silhouettes, rotations and screen-edge clipping.
   The EXACT same background and sprite colors must survive the shortcut. */
#ifdef MX_N3DS_ROT_WORK_COUNT
static int n3_rot_tight_bounds_probe(void){
  static const int poses[]={MSF_IDLE_A,MSF_FLY_A,MSF_DIVE,
                            MSF_SPIN_0,MSF_SPIN_6,MSF_SPIN_12,MSF_HOWL};
  static const float angles[]={-.83f,0.f,.27f,.69f,1.5707963f,
                                2.21f,3.1415927f,5.35f};
  unsigned long long digest=1469598103934665603ull;
  unsigned cases=0;
  man_rot_candidate_pixels=man_rot_sampled_pixels=0;
  man_rot_floor_calls=man_rot_opaque_pixels=0;
  for(int viewport=0;viewport<2;viewport++){
    Scr *screen=viewport?&bot:&top;
    screen->fb=viewport?fb_bot:fb_top[0];
    screen->w=viewport?320:400;screen->h=240;
    for(int fur=0;fur<2;fur++){
      apply_outfit(fur?12:0);
      for(unsigned fi=0;fi<sizeof poses/sizeof poses[0];fi++)
      for(int facing=-1;facing<=1;facing+=2)
      for(unsigned ai=0;ai<sizeof angles/sizeof angles[0];ai++)
      for(int pos=0;pos<3;pos++){
        int cx=pos==0?screen->w/2:pos==1?7:screen->w-8;
        int cy=pos==0?screen->h/2:pos==1?12:screen->h-12;
        memset(screen->fb,41,(size_t)screen->w*screen->h*3);
        blit_frame_rot(screen,poses[fi],cx,cy,viewport?4:3,facing,angles[ai]);
        unsigned h=n3_hash(screen->fb,(size_t)screen->w*screen->h*3);
        digest^=h;digest*=1099511628211ull;
        cases++;
      }
    }
  }
  if(!man_rot_sampled_pixels || man_rot_candidate_pixels<man_rot_sampled_pixels){
    fputs("FAIL: 3DS rotated sprite coverage work counters invalid\n",stderr);
    return 1;
  }
  printf("N3_ROT_TIGHT_HASH %016llx cases=%u\n",digest,cases);
  printf("N3_ROT_TIGHT_WORK candidate=%u sampled=%u\n",
         man_rot_candidate_pixels,man_rot_sampled_pixels);
  printf("N3_ROT_SAMPLER_WORK floors=%u opaque=%u\n",
         man_rot_floor_calls,man_rot_opaque_pixels);
  return 0;
}
#endif
int main(int argc, char **argv){
#ifdef MX_N3DS_ROT_WORK_COUNT
  if(getenv("N3DS_ROT_TIGHT_PROBE"))return n3_rot_tight_bounds_probe();
#endif
  if(getenv("N3DS_SPRITE_VOLUME_PROBE"))return n3_sprite_volume_probe();
#ifdef MX_N3DS_PARTICLE_BENCH
  if(getenv("N3DS_PARTICLE_BOUNDS_PROBE"))return n3_particle_bounds_probe();
  if(getenv("N3DS_PARTICLE_VIEW_PROBE"))return n3_particle_view_probe();
#endif
  if(getenv("N3DS_RUBBLE_DAMAGE_PROBE"))return n3_rubble_damage_probe();
  if(getenv("N3DS_FASTPATH_PROBE"))return n3_fastpath_probe();
  if(argc > 1) OUT = argv[1];
  g_slider = argc > 2 ? (float)atof(argv[2]) : 0.f;
  n3ds_main();
  return fails ? 1 : 0;
}
