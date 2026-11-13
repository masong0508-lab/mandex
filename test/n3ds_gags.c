/*
 * Host check for the 3DS man (sprite blit, idle gags, effects): includes the real 3ds/main.c against test/stubs/3ds.h, then
 *   - blits every one of the 70 man frames at both scales and both facings and checks something is drawn inside the frame's box,
 *   - plays every idle gag through mi_update() / idle_fx() / draw_parts() and checks hearts, sparkles and words appear for the cheeky ones,
 *   - writes a contact sheet of the new frames (PPM) for a look.
 *   gcc -std=gnu99 -O1 -Istubs -I../core -I../art n3ds_gags.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o n3ds_gags && ./n3ds_gags [out.ppm]
 * Like n3ds_render this says nothing about the real console: it has never run on hardware or an emulator.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "3ds.h"
static u8 fb[400 * 240 * 3];
void gfxInitDefault(void){} void gfxSetDoubleBuffering(gfxScreen_t s, bool b){ (void)s; (void)b; } void gfxSet3D(bool b){ (void)b; }
void gfxFlushBuffers(void){} void gfxSwapBuffers(void){} void gspWaitForVBlank(void){} void gfxExit(void){}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t e, u16 *w, u16 *h){ (void)s; (void)e; (void)w; (void)h; return fb; }
float osGet3DSliderState(void){ return 0; }
void hidScanInput(void){} u32 hidKeysDown(void){ return 0; } u32 hidKeysHeld(void){ return 0; } u32 hidKeysUp(void){ return 0; }
void hidTouchRead(touchPosition *t){ (void)t; }
bool aptMainLoop(void){ return false; }
#define main n3ds_main
#include "../3ds/main.c"
#undef main
static int fails;
#define CHECK(c, msg) do{ if(c) printf("ok: %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } }while(0)
static int lit(Scr *s, int x0, int y0, int x1, int y1, u32 bg){        /* pixels in the box that differ from the background */
  int n = 0;
  for(int y = y0; y < y1; y++) for(int x = x0; x < x1; x++){
    if((unsigned)x >= (unsigned)s->w || (unsigned)y >= (unsigned)s->h) continue;
    const u8 *p = s->fb + (x * s->h + (s->h - 1 - y)) * 3; u32 c = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16;
    if(c != bg) n++;
  }
  return n;
}
int main(int argc, char **argv){
  Scr s = { fb, 400, 240 };
  const u32 BG = RGB(1, 2, 3);
  int bad = 0, drawn = 0;
  for(int fr = 0; fr < MSF_COUNT; fr++) for(int ppc = 3; ppc <= 4; ppc++) for(int face = -1; face <= 1; face += 2){
    rect(&s, 0, 0, 400, 240, BG);
    blit_frame(&s, fr, 200, 120, ppc, face);
    const MsFrame *f = &MS_FRAMES[fr];
    int n = lit(&s, 200 - (f->w + 2) * ppc / 4 - 2, 120 - (f->h + 2) * ppc / 4 - 2, 200 + (f->w + 2) * ppc / 4 + 2, 120 + (f->h + 2) * ppc / 4 + 2, BG);
    int all = lit(&s, 0, 0, 400, 240, BG);
    drawn++;
    if(n < 50 || n != all) bad++;      /* something drawn, and nothing outside the frame's own box */
  }
  CHECK(bad == 0, "all frames draw inside their own box at ppc 3 and 4, both facings");
  printf("   %d blits\n", drawn);

  /* every gag: run it through mi_update / idle_fx, count what the effects produce */
  int hearts = 0, glints = 0, words = 0, ends_ok = 0;
  for(int g = 0; g < GAG_COUNT + 1; g++){
    mi_reset(&mi); memset(parts, 0, sizeof parts); memset(pops, 0, sizeof pops);
    mi.gag = g == GAG_COUNT ? GAG_COUNT : g; mi.step = 0; mi.t = 0; mi.fx = MI_GAGS[g < GAG_COUNT ? g : 0][0].fx;
    int ticks = 0;
    do {
      if(mi.fx) idle_fx(mi.fx, 20, 30);
      for(int i = 0; i < NP; i++) if(parts[i].life > 0){ if(parts[i].kind == 1) hearts++; if(parts[i].kind == 2) glints++; }
      for(int i = 0; i < 4; i++) if(pops[i].life > 0) words++;
      tick_parts();
      rect(&s, 0, 0, 400, 240, BG);
      View v = { &s, 4, 0, 0, 0, 0 };
      draw_parts(&v);
      mi_update(&mi);
      ticks++;
    } while(mi.gag >= 0 && mi.gag != GAG_COUNT && ticks < 4000);
    if(g < GAG_COUNT && mi.gag < 0) ends_ok++;
  }
  CHECK(ends_ok == GAG_COUNT, "every gag plays through and ends");
  CHECK(hearts > 0, "the kiss gags float hearts");
  CHECK(glints > 0, "the pin-up and disco gags throw sparkles");
  CHECK(words > 0, "gags pop up words");

  /* hearts and sparkles really draw something */
  memset(parts, 0, sizeof parts);
  spawn_k(20, 30, 0, 0, 40, RGB(255, 126, 176), 1); spawn_k(30, 30, 0, 0, 40, RGB(255, 210, 63), 2);
  rect(&s, 0, 0, 400, 240, BG);
  View v = { &s, 4, 0, 0, 0, 0 };
  draw_parts(&v);
  CHECK(lit(&s, 70, 110, 90, 130, BG) > 10, "a heart is drawn");
  CHECK(lit(&s, 110, 110, 130, 130, BG) > 10, "a sparkle is drawn");

  /* Special Delivery: the three tubes are drawn (balloon + mouth + nozzle in their colours) on the top screen; level 1 has none */
  { View vt = { &s, 3, 20, 28, 0, 0 }; g_sgn = -1; g_sl = 0;
    mx_load(&world, 20); rect(&s, 0, 0, 400, 240, BG); draw_tubes(&vt);
    int orange = 0, blue = 0, green = 0;
    for(int y = 0; y < 240; y++) for(int x = 0; x < 400; x++){ const u8 *p = fb + (x * 240 + (239 - y)) * 3; u32 c = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16;
      if(c == RGB(255, 154, 60)) orange++; if(c == RGB(79, 208, 255)) blue++; if(c == RGB(140, 255, 90)) green++; }
    CHECK(orange > 40 && blue > 40 && green > 40, "level 21 (Special Delivery) draws its three tubes in orange, blue and green");
    mx_load(&world, 0); rect(&s, 0, 0, 400, 240, BG); draw_tubes(&vt);
    CHECK(lit(&s, 0, 0, 400, 240, BG) == 0, "an ordinary level draws no tubes");
    mx_load(&world, 20); rect(&s, 0, 0, 400, 240, RGB(120, 190, 240)); draw_world_3d(&vt, -1, 0);
    if(argc > 2){ FILE *f = fopen(argv[2], "wb"); if(f){ fprintf(f, "P6\n400 240\n255\n");
        for(int y = 0; y < 240; y++) for(int x = 0; x < 400; x++){ const u8 *p = fb + (x * 240 + (239 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
        fclose(f); } }
  }
  /* contact sheet of the new cheeky frames + the dance, for a look */
  rect(&s, 0, 0, 400, 240, RGB(120, 190, 240));
  static const int show[] = { MSF_BK_A, MSF_BK_B, MSF_POSE_A, MSF_POSE_B, MSF_DANCE_A, MSF_DANCE_B, MSF_PEC_A, MSF_PEC_B };
  for(int i = 0; i < 8; i++) blit_frame(&s, show[i], 25 + i * 48, 70, 4, 1);
  for(int i = 0; i < 8; i++) blit_frame(&s, mi_dance_frame(i * 28), 25 + i * 48, 170, 3, i & 1 ? -1 : 1);
  if(argc > 1){ FILE *f = fopen(argv[1], "wb"); if(f){ fprintf(f, "P6\n400 240\n255\n");
      for(int y = 0; y < 240; y++) for(int x = 0; x < 400; x++){ const u8 *p = fb + (x * 240 + (239 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
      fclose(f); } }
  if(fails){ printf("FAILED %d\n", fails); return 1; }
  printf("all 3DS gag checks passed\n");
  return 0;
}
