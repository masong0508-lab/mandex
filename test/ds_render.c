/*
 * Host harness: runs the REAL ds/source/main.c game loop (title -> play -> flings -> win/lose)
 * against the stub libnds, feeding it scripted stylus/button input, and dumps both screens
 * as PPM files at chosen frames.  Also counts sound-channel use.
 *
 *   gcc -DMX_FIXED -Istubs -I../core -I../ds/source ds_render.c ../ds/source/gfx.c ../ds/source/sfx.c ../core/mx.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"

u16 BG_PALETTE[256], BG_PALETTE_SUB[256]; u8 BG_GFX[65536], BG_GFX_SUB[65536];

/* ---- stub hardware ---- */
static int g_down, g_held, g_up, g_prev, g_keys; static touchPosition g_touch;
void lcdMainOnBottom(void){} void videoSetMode(int m){(void)m;} void videoSetModeSub(int m){(void)m;}
void vramSetBankA(int m){(void)m;} void vramSetBankC(int m){(void)m;}
int bgInit(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
int bgInitSub(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
void soundEnable(void){}
void scanKeys(void){ g_down = g_keys & ~g_prev; g_up = g_prev & ~g_keys; g_held = g_keys; g_prev = g_keys; }
int keysDown(void){return g_down;} int keysHeld(void){return g_held;} int keysUp(void){return g_up;}
void touchRead(touchPosition*t){ *t = g_touch; }
static int psg_used[16], noise_used[16], nextid = 1, sound_calls, sound_dropped, psg_peak, noise_peak;
static int count(int *a){ int n = 0; for(int i = 0; i < 16; i++) n += a[i] != 0; return n; }
int soundPlayPSG(int d,u16 f,u8 v,u8 p){
  (void)d;(void)f;(void)v;(void)p; sound_calls++;
  if(count(psg_used) >= 6){ sound_dropped++; return -1; }
  for(int i = 0; i < 16; i++){
    if(psg_used[i]) continue;
    psg_used[i] = nextid++;
    if(count(psg_used) > psg_peak) psg_peak = count(psg_used);
    return i;
  }
  return -1;
}
int soundPlayNoise(u16 f,u8 v,u8 p){
  (void)f;(void)v;(void)p; sound_calls++;
  if(count(noise_used) >= 2){ sound_dropped++; return -1; }
  for(int i = 0; i < 16; i++){
    if(noise_used[i]) continue;
    noise_used[i] = nextid++;
    if(count(noise_used) > noise_peak) noise_peak = count(noise_used);
    return 100 + i;
  }
  return -1;
}
void soundKill(int id){ if(id >= 100) noise_used[id-100] = 0; else if(id >= 0) psg_used[id] = 0; }
void soundSetFreq(int id,int f){(void)id;(void)f;}
void dmaCopy(const void*a,void*b,u32 n){memcpy(b,a,n);} void DC_FlushRange(const void*a,u32 n){(void)a;(void)n;}
void cpuStartTiming(int t){(void)t;} u32 cpuEndTiming(void){ return 100000; }

/* ---- the real game ---- */
#define main ds_main
#include "../ds/source/main.c"
#undef main

static void dump(const u8 *buf, const u16 *pal, const char *fn){
  FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n256 192\n255\n");
  for(int i = 0; i < 256 * 192; i++){ u16 c = pal[buf[i]]; unsigned char rgb[3] = { (c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3 }; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static const char *OUT = "/tmp/dsr";
static void shot(const char *name){
  char p[256]; snprintf(p, sizeof p, "%s_%s_top.ppm", OUT, name); dump(BG_GFX_SUB, BG_PALETTE_SUB, p);
  snprintf(p, sizeof p, "%s_%s_bot.ppm", OUT, name); dump(BG_GFX, BG_PALETTE, p);
}

/* ---- scripted player ---- */
static int vf;                       /* vblank counter */
static int stat_max_nev, stat_max_parts, stat_frames_play;
static int shots_taken, launches, max_frames = 9000;
static int aim_phase = -1, aim_t;    /* stylus drag state machine */
#define POL(a) { -11.5 * cos((a) * 3.14159265 / 180), 11.5 * sin((a) * 3.14159265 / 180) }
static const struct { double dx, dy; } SHOTS[] = { POL(18), POL(26), POL(34), POL(22), POL(44), POL(30) };
static int start_clicks;

static void summary(void){
  printf("play frames=%d  max events/frame=%d  max live particles=%d/%d\n", stat_frames_play, stat_max_nev, stat_max_parts, NPART);
  printf("final: level=%d state=%d men=%d manmode=%d launches=%d aim_phase=%d targets=%d/%d\n", world.level + 1, world.state, world.men, world.man.mode, launches, aim_phase, mx_targets_alive(&world), world.ntargets);
  for(int c = 0; c < MX_MAX_CHUNKS; c++) if(world.chunks[c].active){ const MxChunk *k = &world.chunks[c]; printf("  chunk %d: cells=%d x=%.1f y=%.1f vx=%.3f vy=%.3f w=%.3f sleep=%.2f age=%.1f ang=%.2f\n", c, k->count, RTOF(k->x), RTOF(k->y), RTOF(k->vx), RTOF(k->vy), RTOF(k->w), RTOF(k->sleep), RTOF(k->age), RTOF(k->ang)); }
  printf("sound calls=%d dropped(no free channel)=%d  peak PSG=%d/6  peak noise=%d/2  substeps=%d\n", sound_calls, sound_dropped, psg_peak, noise_peak, world.substeps);
}
void swiWaitForVBlank(void){
  vf++;
  g_keys = 0; /* default: nothing held; script sets below */
  int mode = world.man.mode;
  if(app == ST_TITLE){
    if(vf == 6) shot("title");
    for(int k = 0; k < start_clicks; k++) if(vf == 8 + k * 2) g_keys |= KEY_RIGHT;
    if(vf == 8 + start_clicks * 2 + 4) shot("title2");
    if(vf == 8 + start_clicks * 2 + 8) g_keys |= KEY_A;
  } else if(app == ST_PLAY){
    stat_frames_play++;
    if(world.nev > stat_max_nev) stat_max_nev = world.nev;
    int np = 0; for(int i = 0; i < NPART; i++) np += parts[i].life > 0; if(np > stat_max_parts) stat_max_parts = np;
    if(world.state != MX_PLAY){
      static int endshot;
      if(end_t == 100 && !endshot){ endshot = 1; shot(world.state == MX_WIN ? "win" : "lose"); }
      if(end_t > 140){ printf("level %d ended: %s after %d frames, score=%d\n", world.level + 1, world.state == MX_WIN ? "WIN" : "LOSE", vf, world.score); shot("end2"); summary(); exit(0); }
    } else if(mode == MAN_AIM){
      if(getenv("IDLE_SHOTS")){                                   /* just stand there: photograph every frame of every gag, once */
        static unsigned char seen[256]; static int lg = -2, ls = -2, cnt, forced;
        if(!forced && getenv("FORCE_GAG") && world.banner <= 0 && mi.gag < 0 && mi.total > 0){ forced = 1; mi.gag = atoi(getenv("FORCE_GAG")); mi.step = 0; mi.t = 0; }
        if(mi.gag >= 0 && (mi.gag != lg || mi.step != ls)){
          int fr = mi_frame(&mi, vf);
          if(!seen[fr]){ seen[fr] = 1; char nm[48]; snprintf(nm, sizeof nm, "idle%02d_f%02d", cnt++, fr); shot(nm); printf("  idle shot gag=%d step=%d frame=%d\n", mi.gag, mi.step, fr); }
        }
        lg = mi.gag; ls = mi.step;
        if(mi.fx) printf("  fx %d at vf=%d (live particles so far max %d)\n", mi.fx, vf, stat_max_parts);
      } else
      if(aim_phase < 0 && world.time > RI(1) && launches < 12){ aim_phase = 0; aim_t = 0; }
      if(aim_phase >= 0){
        View av = { buf_bot, 4, 0, 56 };
        double dx = SHOTS[launches % 6].dx, dy = SHOTS[launches % 6].dy;
        int tx = (int)((16 + dx) * 4 - av.cx), ty = (int)((45.25 + dy) * 4 - av.cy);
        int mx0 = vx(&av, MX_ANCHOR_X), my0 = vy(&av, MX_ANCHOR_Y);
        aim_t++;
        if(aim_t < 3){ g_keys |= KEY_TOUCH; g_touch.px = mx0; g_touch.py = my0; }
        else if(aim_t < 20){ g_keys |= KEY_TOUCH; g_touch.px = mx0 + (tx - mx0) * (aim_t - 3) / 17; g_touch.py = my0 + (ty - my0) * (aim_t - 3) / 17; }
        else if(aim_t < 24){ g_keys |= KEY_TOUCH; g_touch.px = tx; g_touch.py = ty; if(aim_t == 22 && launches == 0) shot("aim"); }
        else { aim_phase = -1; launches++; shots_taken = 0; /* lift = no KEY_TOUCH */ }
      }
    } else if(mode == MAN_FLY){
      shots_taken++;
      if(launches == 1){ if(shots_taken == 20) shot("fly1"); if(shots_taken == 45) shot("fly2"); }
      if(launches == 2 && shots_taken == 18) g_keys |= KEY_TOUCH, g_touch.px = 128, g_touch.py = 80;   /* belly flop tap */
      if(launches == 2 && shots_taken == 40) shot("dive");
    } else if(mode == MAN_STUCK){
      static int ss; if(++ss == 25) shot("stuck");
    }
    static int dd[8]; (void)dd;
    /* grab interesting moments */
    static int boom_shot, tgt_shot;
    for(int i = 0; i < world.nev; i++){
      if(world.ev[i].type == EV_BOOM && !boom_shot){ boom_shot = 1; shot("boom"); }
      if(world.ev[i].type == EV_TARGET && !tgt_shot){ tgt_shot = 1; shot("target"); }
    }
    if(world.banner > R(1.9) && world.banner < R(2.0)) shot("banner");
    { static int wob_shot, crack_shot, bounce_shot;
      if(world.wob_on && !wob_shot && launches >= 1){
        real mo = 0; for(int i = 0; i < MX_ROWS * MX_COLS; i++) if(world.grid[i] && rabs(world.ox[i]) > mo) mo = rabs(world.ox[i]);
        if(mo > R(0.6)){ wob_shot = 1; shot("wobble"); printf("  wobble shot: max sway %.2f cells\n", (double)mo / 4096.0); }
      }
      for(int i = 0; i < world.nev; i++){
        if(world.ev[i].type == EV_CRACK && !crack_shot){ crack_shot = 1; shot("crack"); }
        if(world.ev[i].type == EV_BOUNCE && !bounce_shot){ bounce_shot = 1; shot("bounce"); }
      }
    }
  }
  if(vf > max_frames){ printf("max frames reached\n"); shot("final"); summary(); exit(0); }
}

int main(int argc, char **argv){
  if(argc > 1) OUT = argv[1];
  if(argc > 2) max_frames = atoi(argv[2]);
  if(argc > 3) start_clicks = atoi(argv[3]);
  ds_main();
  return 0;
}
void ds_report(void){}
