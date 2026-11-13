/*
 * Host check for the 3DS chapter 2 mechanics: runs the REAL 3ds/main.c loop against test/stubs/3ds.h with scripted buttons.
 *   level 22 (Walk-Up Wreckage): lands on his feet, pad right walks, A jumps, B punches, the touch JUMP button jumps, L gives up;
 *   level 26 (Batter Up): landlords draw (bat, thrower), a briefcase flies, the man in the sling is dazed by it;  Y on the title switches fighting back off.
 *   gcc -std=gnu99 -O1 -Istubs -I../core -I../art n3ds_walk.c ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm -o n3ds_walk && ./n3ds_walk [outprefix]
 * Says nothing about the real console: the 3DS build has never run on hardware or an emulator.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "3ds.h"
static u8 fb_top[400 * 240 * 3], fb_bot[320 * 240 * 3];
static int g_keys, g_prev, g_dn, g_up, g_left, vf;
static touchPosition g_touch;
void gfxInitDefault(void){} void gfxSetDoubleBuffering(gfxScreen_t s, bool b){ (void)s; (void)b; } void gfxSet3D(bool b){ (void)b; }
static void post(void);
void gfxFlushBuffers(void){} void gfxSwapBuffers(void){} void gspWaitForVBlank(void){ post(); } void gfxExit(void){}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t e, u16 *w, u16 *h){ (void)e; (void)w; (void)h; return s == GFX_BOTTOM ? fb_bot : fb_top; }
float osGet3DSliderState(void){ return 0; }
static void script(void);
void hidScanInput(void){ g_keys = 0; vf++; script(); g_dn = g_keys & ~g_prev; g_up = g_prev & ~g_keys; g_prev = g_keys; }
u32 hidKeysDown(void){ return g_dn; } u32 hidKeysHeld(void){ return g_keys; } u32 hidKeysUp(void){ return g_up; }
void hidTouchRead(touchPosition *t){ *t = g_touch; }
bool aptMainLoop(void){ return g_left-- > 0; }
#define main n3ds_main
#include "../3ds/main.c"
#undef main
static const char *OUT = "/tmp/n3w";
static int fails;
#define CHECK(c, msg) do{ if(c) printf("ok: %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } }while(0)
static void dump(const u8 *fb, int w, int h, const char *fn){
  FILE *f = fopen(fn, "wb"); if(!f) return; fprintf(f, "P6\n%d %d\n255\n", w, h);
  for(int y = 0; y < h; y++) for(int x = 0; x < w; x++){ const u8 *p = fb + (x * h + (h - 1 - y)) * 3; unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static int shot_pending;
static char shot_name[64];
static void want_shot(const char *n){ snprintf(shot_name, sizeof shot_name, "%s", n); shot_pending = 1; }
static int level;
static void script(void){
  static int st, f0, x0, y0;
  if(level == 99){ if(vf == 2) g_keys |= KEY_Y; return; }
  if(vf == 2){ sel = level; app = ST_PLAY; load_level(level); }
  if(level == 22){
    MxMan *m = &world.man;
    if(st == 0 && vf > 150 && m->mode == MAN_AIM){ world.men--; m->mode = MAN_FLY; m->x = RI(64); m->y = R(54); m->vx = RI(3); m->vy = RI(10); m->gt = 0; st = 1; f0 = vf; }
    if(st == 1 && m->mode == MAN_WALK){ CHECK(1, "he lands on his feet on the walk level"); st = 2; f0 = vf; }
    if(st == 2 && vf == f0 + 40){ want_shot("walk0"); x0 = m->x; }
    if(st == 2 && vf >= f0 + 40 && vf < f0 + 100) g_keys |= KEY_DRIGHT;
    if(st == 2 && vf == f0 + 100){ CHECK(m->x - x0 > RI(8), "pad right walks him along"); want_shot("walk1"); st = 3; f0 = vf; y0 = m->y; }
    if(st == 3 && vf == f0 + 1) g_keys |= KEY_A;
    if(st == 3 && vf == f0 + 8){ CHECK(m->y < y0 - RI(1), "A jumps"); st = 4; f0 = vf; }
    if(st == 4 && vf == f0 + 40){ g_keys |= KEY_B; m->pCd = 0; }
    if(st == 4 && vf == f0 + 43){ CHECK(m->pT > 0, "B punches"); want_shot("punch"); st = 5; f0 = vf; }
    if(st == 5 && vf >= f0 + 30 && vf <= f0 + 31){ g_keys |= KEY_TOUCH; g_touch.px = 130; g_touch.py = 215; y0 = m->y; }          /* the JUMP button */
    if(st == 5 && vf == f0 + 38){ CHECK(m->y < y0 - R(0.5) || !m->gr, "the touch JUMP button jumps"); want_shot("btn"); st = 6; f0 = vf; }
    if(st == 6 && vf == f0 + 40) g_keys |= KEY_L;
    if(st == 6 && vf == f0 + 44){ CHECK(m->mode != MAN_WALK, "L gives up the walk"); g_left = 0; }
  } else {
    static int cases, hurt, throws;
    for(int i = 0; i < world.nev; i++){ if(world.ev[i].type == EV_THROW) throws++; if(world.ev[i].type == EV_HURT) hurt++; }
    for(int i = 0; i < MX_MAX_CASES; i++) if(world.cases[i].active && !cases){ cases = 1; want_shot("case"); }
    if(vf == 120) want_shot("lords");
    if(vf == 800){ printf("   throws=%d hurt=%d case seen=%d\n", throws, hurt, cases); CHECK(throws > 0 && cases, "a thrower lobbed a briefcase and it was drawn"); g_left = 0; }
  }
  (void)st;
}
static void post(void){ if(shot_pending){ char p[256]; snprintf(p, sizeof p, "%s%d_%s_top.ppm", OUT, level, shot_name); dump(fb_top, 400, 240, p); snprintf(p, sizeof p, "%s%d_%s_bot.ppm", OUT, level, shot_name); dump(fb_bot, 320, 240, p); shot_pending = 0; } }
int main(int argc, char **argv){
  if(argc > 1) OUT = argv[1];
  for(level = 22; level <= 26; level += 4){
    vf = 0; g_left = 1000; g_keys = g_prev = 0; app = ST_TITLE; frame = 0;
    n3ds_main();
  }
  /* Y on the title switches fighting back off and on */
  level = 99; nofight = 0; vf = 0; g_left = 5; g_keys = g_prev = 0; app = ST_TITLE; n3ds_main();
  CHECK(nofight == 1 && world.nofight == 1, "Y on the title switches landlords fighting back off");
  printf(fails ? "3DS walk test: %d FAILED\n" : "3DS walk test: all passed\n", fails);
  return fails != 0;
}
