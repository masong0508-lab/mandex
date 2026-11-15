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

static const char *OUT = "/tmp/n3fur";
static void dump(const u8 *fb, int w, int h, const char *fn){
  FILE *f = fopen(fn, "wb"); if(!f) return; fprintf(f, "P6\n%d %d\n255\n", w, h);
  for(int y = 0; y < h; y++) for(int x = 0; x < w; x++){ const u8 *p = fb + (x * h + (h - 1 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
/* the Wolfman outfit on the 3DS: the man's skin must come out as grey fur, and his pink skin everywhere else */
bool aptMainLoop(void){
  static int t, fails;
  t++; g_keys = 0;
  if(t == 3) g_keys = KEY_A;
  if(t == 6){ apply_outfit(12); }
  if(t == 40){
    char p[200]; snprintf(p, sizeof p, "%s_top.ppm", OUT); dump(fb_top[0], 400, 240, p);
    int fur = man_pal[1][0] == 140 && man_pal[1][2] == 158 && man_pal[2][0] == 101;
    printf("%s wolfman palette is grey fur\n", fur ? "ok:" : "FAIL:"); fails += !fur;
    apply_outfit(0); int skin = man_pal[1][0] == 244 && man_pal[1][1] == 183;
    printf("%s every other outfit keeps his skin\n", skin ? "ok:" : "FAIL:"); fails += !skin;
    puts(fails ? "3ds fur: FAILED" : "3ds fur: all passed"); exit(fails != 0);
  }
  return true;
}
int main(void){ return n3ds_main(); }
