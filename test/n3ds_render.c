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
void gfxInitDefault(void){} void gfxSetDoubleBuffering(gfxScreen_t s, bool b){ (void)s; (void)b; } void gfxSet3D(bool b){ (void)b; }
void gfxFlushBuffers(void){} void gfxSwapBuffers(void){} void gspWaitForVBlank(void){} void gfxExit(void){}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t e, u16 *w, u16 *h){ (void)w; (void)h; return s == GFX_BOTTOM ? fb_bot : fb_top[e == GFX_RIGHT]; }
float osGet3DSliderState(void){ return g_slider; }
void hidScanInput(void){ g_dn = g_keys & ~g_prev; g_up = g_prev & ~g_keys; g_prev = g_keys; }
u32 hidKeysDown(void){ return g_dn; } u32 hidKeysHeld(void){ return g_keys; } u32 hidKeysUp(void){ return g_up; }
void hidTouchRead(touchPosition *t){ *t = g_touch; }

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

int main(int argc, char **argv){
  if(argc > 1) OUT = argv[1];
  g_slider = argc > 2 ? (float)atof(argv[2]) : 0.f;
  n3ds_main();
  return fails ? 1 : 0;
}
