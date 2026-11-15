/* Host harness for the 3DS cutscenes (ds/source/intro.c shared with the DS): runs the real 3ds/main.c loop, presses A on level 1 (origin story) and plays it out with
   no input, then the same for level 41 (wolfman), photographing both screens as it goes.  No hardware, no timing, no sound (see PORTING.md).
     gcc ... n3ds_intro.c gfx.o intro.o (-DMX_FIXED -D__3DS__ units) ../core/*.c ;  ./n3ds_intro [outprefix] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "3ds.h"
static u8 fb_top[2][400 * 240 * 3], fb_bot[320 * 240 * 3];
static int g_keys, g_prev, g_dn, g_up; static touchPosition g_touch;
void osSetSpeedupEnable(bool b){ (void)b; } void gfxInitDefault(void){} void gfxSetDoubleBuffering(gfxScreen_t s, bool b){ (void)s; (void)b; } void gfxSet3D(bool b){ (void)b; }
void gfxFlushBuffers(void){} void gfxSwapBuffers(void){} void gspWaitForVBlank(void){} void gfxExit(void){}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t e, u16 *w, u16 *h){ (void)w; (void)h; return s == GFX_BOTTOM ? fb_bot : fb_top[e == GFX_RIGHT]; }
float osGet3DSliderState(void){ return 0; }
void hidScanInput(void){ g_dn = g_keys & ~g_prev; g_up = g_prev & ~g_keys; g_prev = g_keys; }
u32 hidKeysDown(void){ return g_dn; } u32 hidKeysHeld(void){ return g_keys; } u32 hidKeysUp(void){ return g_up; }
void hidTouchRead(touchPosition *t){ *t = g_touch; }
int stub_snd_plays;
#define main n3ds_main
#include "../3ds/main.c"
#undef main
static const char *OUT = "/tmp/n3i"; static int fails;
#define CHECK(c, msg) do{ if(c) printf("ok: %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } }while(0)
static void dump(const u8 *fb, int w, int h, const char *fn){
  FILE *f = fopen(fn, "wb"); if(!f) return; fprintf(f, "P6\n%d %d\n255\n", w, h);
  for(int y = 0; y < h; y++) for(int x = 0; x < w; x++){ const u8 *p = fb + (x * h + (h - 1 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static int nonblack(const u8 *fb, int n){ int c = 0; for(int i = 0; i < n; i++) c += fb[i * 3] | fb[i * 3 + 1] | fb[i * 3 + 2] ? 1 : 0; return c; }
bool aptMainLoop(void){
  static int ph, t, sc_seen = -1;
  t++; g_keys = 0;
  if(ph == 0){ if(t == 3){ CHECK(app == ST_TITLE && !intro_seen, "fresh title, the origin story not seen yet"); g_keys = KEY_A; } if(t == 4){ CHECK(app == ST_INTRO, "A on level 1 starts the origin story"); ph = 1; t = 0; } }
  else if(ph == 1 || ph == 3){
    if(app == ST_INTRO && intro_scene() != sc_seen){ sc_seen = intro_scene(); char p[200]; snprintf(p, sizeof p, "%s_p%d_s%d_top.ppm", OUT, ph, sc_seen); dump(fb_top[0], 400, 240, p); snprintf(p, sizeof p, "%s_p%d_s%d_bot.ppm", OUT, ph, sc_seen); dump(fb_bot, 320, 240, p);
      if(t > 40){ char q[200]; snprintf(q, sizeof q, "%s_p%d_s%d_mid_top.ppm", OUT, ph, sc_seen); } }
    if(app == ST_INTRO && t == 200){ char p[200]; snprintf(p, sizeof p, "%s_p%d_t200_top.ppm", OUT, ph); dump(fb_top[0], 400, 240, p); snprintf(p, sizeof p, "%s_p%d_t200_bot.ppm", OUT, ph); dump(fb_bot, 320, 240, p); CHECK(nonblack(fb_top[0], 400 * 240) > 20000 && nonblack(fb_bot, 320 * 240) > 3000, "both screens carry the cutscene picture and narration"); }
    if(app == ST_PLAY){ CHECK(world.level == (ph == 1 ? 0 : MX_CH3_START), ph == 1 ? "the origin story ends in level 1" : "the wolfman scene ends in level 41"); if(ph == 1){ ph = 2; t = 0; sc_seen = -1; app = ST_TITLE; sel = MX_CH3_START; load_level(sel); } else { CHECK(stub_snd_plays > 8, "the cutscenes play their sound effects on the DSP"); printf(fails ? "3ds intro: %d FAILED\n" : "3ds intro: all passed\n", fails); exit(fails != 0); } }
  }
  else if(ph == 2){ if(t == 3){ CHECK(!wolf_seen, "the wolfman scene is not seen yet"); g_keys = KEY_A; } if(t == 4){ CHECK(app == ST_INTRO, "A on level 41 starts the wolfman cutscene"); ph = 3; t = 0; } }
  if(t > 60000){ printf("FAIL: timeout\n"); exit(1); }
  return 1;
}
int main(void){ return n3ds_main(); }
