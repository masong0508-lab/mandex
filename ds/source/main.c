/*
 * MANDEX - Nintendo DS front-end (devkitARM + libnds).
 *
 * Two 256x192 screens, both software-rendered into 8-bit RAM buffers and DMA'd to VRAM:
 *
 *   TOP    : the action camera.  4 px per voxel, follows the man in flight, then lingers on
 *            explosions and evictions.  Parallax sky, HUD, banners, win/lose panel.
 *   BOTTOM : the touch screen.
 *              aiming  -> 4 px per voxel zoom on the sling (precise pulls, trajectory dots)
 *              flying  -> 2 px per voxel overview of the whole level, HUD strip with buttons
 *
 * Controls
 *   stylus   : drag the man back and lift to fling.  Tap during flight = belly flop.
 *   D-pad + A: same, without the stylus (D-pad moves the man in the sling, A flings / flops).
 *   START    : pause / next level / retry      SELECT : restart      Y : music      L : profiler
 *
 * All game logic is the shared fixed-point core in ../../core.  This file never uses float.
 */
#include <nds.h>
#include <string.h>
#include "mx.h"
#include "mx_meta.h"
#include "gfx.h"
#include "sfx.h"
#include "intro.h"
#include "mx_fps_meter.h"
#define MS_ENUM_ONLY
#include "manspr.h"
#include "manidle.h"
#define MX_TITLE_DS
#include "title.h"

#define DS_WOB_MIN 48      /* wobble cells walked per step: the floor and the ceiling of the adaptive cap (a calm lattice is far below both) */
#define DS_WOB_MAX 200
static int wob_budget = DS_WOB_MAX;
#ifndef MX_DS_MAX_SUBSTEPS
#define MX_DS_MAX_SUBSTEPS 3
#endif
#define DT R(1.0 / 60.0)
/* Display two complete software views at 60 Hz if the stylus is active,
   otherwise redraw the touch-screen overview at 30 Hz. Top gameplay always
   redraws at full speed. A/B profiling can disable this with -DMX_DS_BOTTOM30=0. */
#ifndef MX_DS_BOTTOM30
#define MX_DS_BOTTOM30 1
#endif
/* Over budget? Prefer 15 Hz redraw of the noninteractive *secondary* world
   rather than starving the upper gameplay picture. Still use full-speed
   touch/drag/active D-pad frames. Can A/B on emulator with =0. */
#ifndef MX_DS_OVERLOAD_BOTTOM15
#define MX_DS_OVERLOAD_BOTTOM15 0 /* off by default: DeSmuME A/B mixed */
#endif

static MxWorld world;                         /* ~320 KB, lives in main RAM */
static u8 buf_top[256 * 192] __attribute__((aligned(4)));
static u8 buf_bot[256 * 192] __attribute__((aligned(4)));

enum { ST_TITLE, ST_PLAY, ST_PAUSE, ST_INTRO };
static int app = ST_TITLE, frame, sel_level, music_pref = 1, debug_on;
static MxFpsMeter fps_meter;
/* ARM9 hardware timers use 33.513982 MHz ticks. Keep previous frame costs
   for the L profiler, without allocating another framebuffer. */
static int ds_vblank_ticks,ds_dma_ticks,ds_draw_top_ticks,ds_draw_bot_ticks;
#ifdef MX_BARE_NDS_H
static u16 ds_vblank_prev;static int ds_vblank_primed;
static void ds_fps_complete(int sim_steps){
  u16 now=MX_IPC->alive;
  if(ds_vblank_primed){
    unsigned delta=(u16)(now-ds_vblank_prev);
    mx_fps_meter_add(&fps_meter,delta,60,1,(unsigned)sim_steps);
  }else ds_vblank_primed=1;
  ds_vblank_prev=now;
}
#else
static void ds_fps_complete(int sim_steps){ (void)sim_steps; }
#endif
#ifdef MX_DS_PROFILE_HOST
extern void ds_perf_mark(int stage);
#define DS_PERF_MARK(stage) ds_perf_mark(stage)
#else
#define DS_PERF_MARK(stage) ((void)0)
#endif
static int intro_seen, wolf_seen, intro_level, end_seen;   /* wolf_seen: the chapter 3 wolfman cutscene, once per power-on too */
static int intro_seen_unused;               /* the opening cutscene (ds/source/intro.c) plays once per power-on, the first time level 1 is started from the title */
static int disp_score, cells0 = 1, end_t, was_state;      /* best scores live in stats.best[] (core, saved by port 5) */
static MxStats stats;                 /* achievements / outfits / best scores; loaded from the cartridge backup chip at boot, written by save_sync() */
static int ach_open, ach_page;       /* achievements list shown on the title's bottom screen */
static int combo_held, cheat_t;      /* L+R+SELECT on the title = unlock every level for this session: latch (set while the three are held) and the on-screen message timer */
static int valve_equipped;      /* gameplay toolbar gadget: default OFF, not an obstacle when flinging */
static MxOptions opt;                /* port 13: collapsible buildings + start tool.  Saved in the cartridge blob (version 2); defaults until a save loads */
static int opt_open, opt_cur;        /* options screen shown on the title's bottom screen; opt_cur = the highlighted row */
static int was_aim; /* tool arming edge detector; collapse always follows opt.collapse */
static int out_open, out_cur;        /* outfits screen shown on the title's bottom screen; out_cur = the highlighted outfit (outfit_sel = the one worn) */
static int outfit_sel, toast_t = -1, toast_id, toast_q[4], toast_nq;
static void toast_push(int id){ if(toast_nq < 4) toast_q[toast_nq++] = id; }
static unsigned outfit_seen = 1;                                   /* outfits already owned (bit per outfit): a new one pops a "NEW OUTFIT" toast, ids 100+ */
static void outfit_scan(int quiet){ for(int i = 0; i < MX_NUM_OUTFITS; i++){ if(mx_outfit_unlocked(&stats, i) && !((outfit_seen >> i) & 1)){ outfit_seen |= 1u << i; if(!quiet) toast_push(100 + i); } } }
static void toast_tick(void){
  if(toast_t >= 0){ if(++toast_t > 150) toast_t = -1; }
  if(toast_t < 0 && toast_nq){ toast_id = toast_q[0]; for(int i = 1; i < toast_nq; i++) toast_q[i-1] = toast_q[i]; toast_nq--; toast_t = 0; sfx_play(SFX_TARGET, 64); }
}

static int pad_dx, pad_dy, pad_used, dragging;
static int MAXHP[MX_NMAT];                    /* filled from the core's material table */
#ifndef MX_DS_FACADE_CACHE
#define MX_DS_FACADE_CACHE 0 /* measured neutral/slower across 8 host scenarios; retain for A/B */
#endif
#if MX_DS_FACADE_CACHE
/* At most 20 rooms and three windows per room: ~600 bytes of static storage,
   no heap or extra framebuffer. Window positions, palette colors and cell
   indices are unchanged until the next level/district; destruction checks
   still consult the current grid+wallpaper on EVERY rendered frame. */
typedef struct { u16 idx; u8 wx,wy,wc,hc,glass; } FacWin;
typedef struct { u8 first,deskx,desky; } FacRoom;
static FacWin fac_win[MX_MAX_ROOMS*3];
static FacRoom fac_room[MX_MAX_ROOMS];
static int fac_dirty=1;
#endif

static u32 rng = 0x2545F491u;
static int rnd(int n){ rng = rng * 1664525u + 1013904223u; return (int)((rng >> 16) % (u32)n); }

/* ------------------------------------------------------------ particles/popups */
enum { PT_DEBRIS, PT_SMOKE, PT_SPARK, PT_GOO, PT_HAT, PT_DUST, PT_STAR, PT_STINK, PT_FLY, PT_ZZZ, PT_SWEAT, PT_RING, PT_HEART, PT_GLINT };
typedef struct { int x, y, vx, vy; short life, max; u8 type, col; short r; } Part;      /* positions in 1/64 cell; r = a ring's final radius in 1/64 cell */
#define NPART 128
static Part parts[NPART];
#ifndef MX_DS_PARTICLE_HINT
#define MX_DS_PARTICLE_HINT 1
#endif
#if MX_DS_PARTICLE_HINT
/* Earliest possibly vacant slot (not necessarily itself free). Destruction
   bursts skip occupied prefixes and retain the exact old drawing order. */
static int part_free_hint;
#endif

static int spawn(int type, real wx, real wy, int vx, int vy, int col, int life){
#if MX_DS_PARTICLE_HINT
  for(int i = part_free_hint; i < NPART; i++) if(parts[i].life <= 0){
#else
  for(int i = 0; i < NPART; i++) if(parts[i].life <= 0){
#endif
    Part *p = &parts[i];
    p->x = wx >> 6; p->y = wy >> 6; p->vx = vx; p->vy = vy; p->life = p->max = (short)life; p->type = (u8)type; p->col = (u8)col; p->r = 0;
#if MX_DS_PARTICLE_HINT
    part_free_hint = i + 1;
#endif
    return i;
  }
  return -1;            /* pool exhausted */
}
/* the browser's ring(): an outline that swells to rcells and fades */
static void spawn_ring(real wx, real wy, int rcells, int col){
  /* A free-slot hint can point at an OCCUPIED index after scattered
     expirations. Attach the radius to the actual allocated slot, not the
     search start, or later ring sprites may corrupt other particles. */
  int i = spawn(PT_RING, wx, wy, 0, 0, col, 27);
  if(i >= 0) parts[i].r = (short)(rcells * 64);
}
typedef struct { int x, y, life; char txt[12]; u8 col; } Popup;
static Popup pops[4];
static void popup(real wx, real wy, const char *s, int col){
  Popup *p = &pops[rnd(4)];
  for(int i = 0; i < 4; i++) if(pops[i].life <= 0){ p = &pops[i]; break; }
  p->x = wx >> 6; p->y = wy >> 6; p->life = 70; p->col = (u8)col;
  int n = 0; while(s[n] && n < 11){ p->txt[n] = s[n]; n++; } p->txt[n] = 0;
}

/* ------------------------------------------------------------------ idle gags */
static MiState mi;
static ClothActor tow_play, tow_win[2];     /* the towels (core/cloth.c): the man in the sling, the two end-panel dancers */
/* sound + particles for one gag effect.  (wx,wy) = where he stands (cells), sc = 1 in the world, 2 on the title (8 px per cell art) */
static void idle_fx(int fx, real wx, real wy, int face, int sc, int popups){
  real mxp = wx + face * R(1.3) * sc, myp = wy - R(3.6) * sc;                      /* mouth */
  switch(fx){
  case FX_SLAP:  sfx_play(SFX_SLAP, 64); spawn(PT_SWEAT, wx - R(2) * sc, wy - R(4) * sc, -6, -10, 0, 30); spawn(PT_SWEAT, wx + R(2) * sc, wy - R(4) * sc, 6, -10, 0, 30); break;
  case FX_BOING: sfx_play(SFX_BOING, 64); break;
  case FX_GRUNT: sfx_play(SFX_GRUNT, 64); spawn(PT_SWEAT, wx + R(2) * sc, wy - R(4) * sc, 4, -12, 0, 30); break;
  case FX_BURP:
    sfx_play(SFX_BURP, 64);
    for(int k = 0; k < 6; k++) spawn(PT_STINK, mxp, myp, face * (8 + rnd(10)), rnd(7) - 4, 0, 60 + rnd(30));
    if(popups) popup(mxp, myp - RI(3), "BUUURP!", C_GOO);
    break;
  case FX_FART:
    sfx_play(SFX_FART, 64);
    for(int k = 0; k < 9; k++) spawn(PT_STINK, wx + RI(rnd(5) - 2) * sc, wy + R(1.5) * sc, rnd(13) - 6, -rnd(5), 0, 80 + rnd(40));
    for(int k = 0; k < 3; k++) spawn(PT_FLY, wx + RI(rnd(5) - 2) * sc, wy - RI(rnd(3)) * sc, 0, 0, 0, 150);
    if(popups) popup(wx, wy - RI(5), "PFFFFT!", C_GOO);
    break;
  case FX_FLICK: sfx_play(SFX_FLICK, 64); spawn(PT_GOO, mxp, myp, face * (18 + rnd(8)), -(8 + rnd(6)), 0, 110); break;
  case FX_YAWN:  sfx_play(SFX_YAWN, 64); break;
  case FX_SNORE: sfx_play(SFX_SNORE, 64); spawn(PT_ZZZ, wx + face * R(3) * sc, wy - R(5) * sc, 2 * face, -4, 0, 80); break;
  case FX_SNIFF: sfx_play(SFX_STRETCH, 64); break;
  case FX_WHOOSH: sfx_play(SFX_WHOOSH, 64); for(int k = 0; k < 4; k++) spawn(PT_DUST, wx + RI(rnd(9) - 4) * sc, wy - RI(rnd(5)) * sc, rnd(9) - 4, rnd(5) - 2, 0, 22); break;
  case FX_TAUNT: sfx_play(SFX_TAUNT, 64); if(popups) popup(wx, wy - RI(7), "NYAH NYAH!", C_PINK); break;
  case FX_EEK:   sfx_play(SFX_EEK, 64); spawn(PT_SWEAT, wx - R(2) * sc, wy - R(5) * sc, -6, -10, 0, 30); spawn(PT_SWEAT, wx + R(2) * sc, wy - R(5) * sc, 6, -10, 0, 30); if(popups) popup(wx, wy - RI(7), "EEK!", C_RED2); break;
  case FX_SNAP:  sfx_play(SFX_SLAP, 64); sfx_play(SFX_BOING, 64); if(popups) popup(wx, wy - RI(5), "SNAP!", C_GOLD); break;
  case FX_WHEW:  sfx_play(SFX_WHEW, 64); break;
  case FX_KISS:  sfx_play(SFX_POP, 64); for(int k = 0; k < 3; k++) spawn(PT_HEART, wx + face * (R(3) + RI(k)) * sc, wy - R(5) * sc, face * (3 + rnd(4)), -(3 + rnd(3)), 0, 70 + rnd(20));
                 if(popups) popup(wx + face * RI(3), wy - RI(7), "MWAH!", C_PINK); break;
  case FX_SPARKLE: sfx_play(SFX_STRETCH, 64); for(int k = 0; k < 5; k++) spawn(PT_GLINT, wx + RI(rnd(9) - 4) * sc, wy - RI(rnd(9)) * sc, 0, -1, 0, 14 + rnd(8)); break;
  case FX_DISCO: sfx_play(SFX_BOING, 64); spawn(PT_GLINT, wx + (rnd(2) ? 1 : -1) * RI(3 + rnd(3)) * sc, wy - RI(5 + rnd(3)) * sc, 0, -1, 0, 16); break;
  case FX_POP:   sfx_play(SFX_SLAP, 64); if(popups && !rnd(3)) popup(wx, wy - RI(5), "POP!", C_GOLD); break;
  }
}

static void update_parts(void){
  int ground = MX_ROWS * 64;
  for(int i = 0; i < NPART; i++){
    Part *p = &parts[i];
    if(p->life <= 0) continue;
    p->life--;
    p->x += p->vx; p->y += p->vy;
    switch(p->type){
    case PT_DEBRIS: case PT_HAT: case PT_STAR:
      p->vy += 2;
      if(p->y > ground - 2){ p->y = ground - 2; p->vy = -p->vy / 3; p->vx /= 2; if(p->type == PT_DEBRIS && p->life > 20) p->life = 20; }
      break;
    case PT_SPARK: p->vy += 2; break;
    case PT_GOO:
      p->vy += 2;
      if(p->y > ground - 2){ p->y = ground - 2; p->vx = p->vy = 0; }
      break;
    case PT_SMOKE: p->vx = p->vx * 15 / 16; p->vy = p->vy * 15 / 16 - 1; break;
    case PT_STINK: p->vx = p->vx * 29 / 32; p->vy = p->vy * 29 / 32 - (p->life & 1); break;        /* green cloud: spreads, drifts up */
    case PT_FLY:   p->vx = p->vx * 7 / 8; p->vy = p->vy * 7 / 8; break;
    case PT_ZZZ:   p->vx = p->vx * 15 / 16; if(p->life & 3) p->vy = p->vy * 7 / 8 - 0; break;
    case PT_SWEAT: p->vy += 2; if(p->y > ground - 2) p->life = 0; break;
    case PT_HEART: p->vx = p->vx * 31 / 32; p->vy = p->vy * 15 / 16 - (p->life & 1); break;      /* drifts up and slows */
    case PT_GLINT: break;
    case PT_DUST:  p->vx = p->vx * 7 / 8; p->vy = p->vy * 7 / 8; break;
    }
#if MX_DS_PARTICLE_HINT
    if(p->life <= 0 && i < part_free_hint) part_free_hint = i;
#endif
  }
  for(int i = 0; i < 4; i++) if(pops[i].life > 0){ pops[i].life--; if(frame & 1) pops[i].y -= 1; }
}

/* --------------------------------------------------------------------- camera */
static real fx = RI(88), fy = RI(38);            /* smoothed focus (cells) */
static real ev_fx, ev_fy; static int ev_ft;       /* last notable event, lingered on for a moment */

static int pan_of(real x){ return gfx_clamp(((x >> 12) * 128) / MX_COLS, 0, 127); }

static void on_events(void){
  for(int i = 0; i < world.nev; i++){
    const MxEvent *e = &world.ev[i];
    int pan = pan_of(e->x);
    switch(e->type){
    case EV_CELL:
      if(e->m == MX_GLASS){                                  /* glass: glittering shards */
        spawn(PT_SPARK, e->x, e->y, (e->a >> 12) + rnd(9) - 4, (e->b >> 12) - rnd(7), C_WHITE, 12 + rnd(10));
        if(!rnd(3)) sfx_play(SFX_CRACK, pan);
      }
      if(e->m == MX_SAND){ spawn(PT_DUST, e->x, e->y, rnd(9) - 4, -rnd(5), 0, 26 + rnd(12)); }
      spawn(PT_DEBRIS, e->x, e->y, (e->a >> 12) + rnd(9) - 4, (e->b >> 12) - rnd(7), MATPAL(e->m) + (rnd(2) ? SH_BASE : SH_HI), 40 + rnd(25));
      if(!rnd(3)) spawn(PT_DUST, e->x, e->y, rnd(7) - 3, -rnd(4), 0, 22 + rnd(10));
      break;
    case EV_CRUNCH: sfx_play(SFX_CRUNCH, pan); break;
    case EV_CRACK:                                          /* a joint just snapped */
      spawn(PT_DUST, e->x, e->y, rnd(5) - 2, -rnd(3), 0, 16 + rnd(8));
      if(e->m == MX_GLASS) spawn(PT_SPARK, e->x, e->y, rnd(13) - 6, -rnd(8), C_WHITE, 10);
      sfx_play(SFX_CRACK, pan);
      break;
    case EV_BOUNCE:                                         /* the man hit something springy */
      for(int k = 0; k < 5; k++) spawn(PT_STAR, e->x, e->y, rnd(17) - 8, -rnd(12) - 2, 0, 24 + rnd(10));
      sfx_play(SFX_BOUNCE, pan);
      popup(e->x, e->y - RI(3), "BOING!", C_PINK);
      break;
    case EV_TUBE: {                                          /* posted through a pneumatic tube: a ring at each end and FWOOMP */
      static const u8 TC[3] = { C_FIRE, C_UILINE, C_GOO };
      u8 tc = TC[e->m % 3];
      spawn_ring(e->x, e->y, 6, tc); spawn_ring(e->a, e->b, 6, tc);
      popup(e->x, e->y - RI(5), "FWOOMP!", tc); sfx_play(SFX_WHOOSH, pan);
      for(int k = 0; k < 6; k++) spawn(PT_DUST, e->a, e->b, rnd(25) + 4, rnd(9) - 4, 0, 20 + rnd(8));
      break; }
    case EV_WALK:                                           /* he landed on his feet (walk levels) */
      for(int k = 0; k < 10; k++) spawn(PT_GOO, e->x, e->y, rnd(31) - 15, -rnd(20), 0, 40 + rnd(20));
      spawn_ring(e->x, e->y, 6, C_GOO); popup(e->x, e->y - RI(6), "ON FOOT!", C_GOO); sfx_play(SFX_SPLAT, pan);
      break;
    case EV_PUNCH:
      if(e->a > 0){ for(int k = 0; k < 5; k++) spawn(PT_SPARK, e->x, e->y, rnd(31) - 15, -rnd(15), k & 1 ? C_GOLD : C_WHITE, 12 + rnd(8)); popup(e->x, e->y - RI(3), e->m ? "SHOULDER!" : "POW!", C_GOLD); sfx_play(SFX_SLAP, pan); sfx_play(SFX_CRUNCH, pan); }
      else sfx_play(SFX_WHOOSH, pan);
      break;
    case EV_SLAM:
      for(int k = 0; k < 8; k++) spawn(PT_DUST, e->x + RI(rnd(9) - 4), e->y, rnd(31) - 15, -rnd(6), 0, 22 + rnd(10));
      spawn_ring(e->x, e->y, 8, C_WHITE); spawn_ring(e->x, e->y, 5, C_GOLD); popup(e->x, e->y - RI(5), "BELLY SLAM!", C_FIRE); sfx_play(SFX_THUD, pan); sfx_play(SFX_CRUNCH, pan);
      break;
    case EV_PAD:
      for(int k = 0; k < 5; k++) spawn(PT_STAR, e->x, e->y, rnd(17) - 8, -rnd(12) - 2, 0, 24 + rnd(10));
      spawn_ring(e->x, e->y, 5, C_PINK); popup(e->x, e->y - RI(5), "BOING!", C_PINK); sfx_play(SFX_BOING, pan);
      break;
    case EV_HURT:                                           /* a landlord got him: on foot (m=1) or in the sling (m=0, the pull is let go) */
      for(int k = 0; k < 8; k++) spawn(PT_SPARK, e->x, e->y - RI(1), rnd(41) - 20, -rnd(20), k & 1 ? C_GOLD : C_WHITE, 14 + rnd(8));
      spawn_ring(e->x, e->y, 5, C_RED); popup(e->x, e->y - RI(6), "OUCH!", C_RED); sfx_play(SFX_GRUNT, pan); sfx_play(SFX_THUD, pan);
      if(!e->m){ dragging = 0; pad_used = 0; }
      break;
    case EV_BONK:                                           /* a bat landlord swatted a weak shot */
      for(int k = 0; k < 8; k++) spawn(PT_SPARK, e->x, e->y - RI(1), rnd(41) - 20, -rnd(20), k & 1 ? C_GOLD : C_WHITE, 14 + rnd(8));
      spawn_ring(e->x, e->y, 5, C_GOLD); popup(e->x, e->y - RI(6), "BONK!", C_GOLD); sfx_play(SFX_SLAP, pan); sfx_play(SFX_CRUNCH, pan);
      break;
    case EV_OOF:
      for(int k = 0; k < 6; k++) spawn(PT_SPARK, e->x, e->y - RI(1), rnd(41) - 20, -rnd(20), k & 1 ? C_GOLD : C_WHITE, 14 + rnd(8));
      popup(e->x, e->y - RI(6), e->m ? "CRACK!" : "OOF!", C_GOLD); sfx_play(SFX_CRUNCH, pan);
      break;
    case EV_HEAL:
      spawn_ring(e->x, e->y, 4, C_WHITE); popup(e->x, e->y - RI(6), "+HP", C_WHITE); sfx_play(SFX_TARGET, pan);
      break;
    case EV_THROW: case EV_SWING: sfx_play(SFX_WHOOSH, pan); break;
    case EV_THUD:
      if(e->a > R(0.3)) sfx_play(SFX_THUD, pan);
      if(e->a > R(0.6)){                        /* capped two-frame-sized impact dust puff */
        for(int k = 0; k < 3; k++)
          spawn(PT_DUST, e->x + RI(rnd(5) - 2), e->y,
                rnd(11) - 5, -rnd(7) - 2, 0, 18 + rnd(10));
      }
      break;
    case EV_LAND:                                           /* ground touch: a puff of dust, and a hard landing adds a ring */
      for(int k = 0, n = e->m ? 8 : 5; k < n; k++) spawn(PT_DUST, e->x + RI(rnd(5) - 2) / 4, e->y - R(0.5), rnd(15) - 7, -rnd(5) - 1, 0, 22 + rnd(10));
      if(e->m) spawn_ring(e->x, e->y - R(0.5), 4, C_SMOKE);       /* browser: 30 px at 8 px a cell */
      break;
    case EV_BOOM:
      for(int k = 0; k < 9; k++) spawn(PT_SMOKE, e->x, e->y, rnd(25) - 12, rnd(20) - 14, 0, 26 + rnd(14));
      for(int k = 0; k < 12; k++) spawn(PT_SPARK, e->x, e->y, rnd(41) - 20, rnd(31) - 22, k & 1 ? C_GOLD : C_WHITE, 14 + rnd(12));
      sfx_play(SFX_BOOM, pan);
      ev_fx = e->x; ev_fy = e->y; ev_ft = 70;
      popup(e->x, e->y - RI(3), "BOOM!", C_FIRE);
      break;
    case EV_SPLAT:
      for(int k = 0; k < 14; k++) spawn(PT_GOO, e->x, e->y, rnd(25) - 12, -rnd(17), 0, 70 + rnd(40));
      spawn_ring(e->x, e->y, 5, C_GOO);
      sfx_play(SFX_SPLAT, pan);
      break;
    case EV_TARGET: {
      static const char *const WORDS[] = { "EVICTED!", "SO LONG!", "REFUND?", "NO RENT!" };
      for(int k = 0; k < 6; k++) spawn(PT_STAR, e->x, e->y, rnd(17) - 8, -rnd(14) - 3, 0, 36 + rnd(14));
      spawn(PT_HAT, e->x, e->y - RI(3), rnd(9) - 4, -22, 0, 90);
      sfx_play(SFX_TARGET, pan);
      ev_fx = e->x; ev_fy = e->y; ev_ft = 90;
      popup(e->x, e->y - RI(4), e->m > 1 ? mx_combo_taunt(e->m) : WORDS[rnd(4)], e->m > 1 ? C_PINK : C_GOLD);
      break; }
    case EV_LAUNCH: sfx_play(SFX_LAUNCH, pan); dragging = 0; break;
    case EV_DIVE:   sfx_play(SFX_DIVE, pan); break;
    case EV_POP:    for(int k = 0; k < 6; k++) spawn(PT_GOO, e->x, e->y, rnd(17) - 8, -rnd(17), 0, 40); sfx_play(SFX_POP, pan); break;
    case EV_AIR:                                            /* air valve hiss: little white puffs, belly creaks */
      spawn(PT_DUST, MX_VALVE_X + RI(rnd(3) - 1), MX_VALVE_Y + RI(3), rnd(7) - 3, -rnd(6), 0, 14 + rnd(6));
      if(rnd(4) == 0) sfx_play(SFX_STRETCH, pan);
      break;
    case EV_AIRPOP:
      for(int k = 0; k < 18; k++) spawn(PT_GOO, e->x, e->y, rnd(41) - 20, -rnd(25), 0, 70 + rnd(40));
      for(int k = 0; k < 10; k++) spawn(PT_SPARK, e->x, e->y, rnd(41) - 20, rnd(31) - 22, k & 1 ? C_RED : C_WHITE, 16 + rnd(10));
      sfx_play(SFX_BOOM, pan); popup(e->x, e->y - RI(4), "POP!", C_RED); ev_fx = e->x; ev_fy = e->y; ev_ft = 60;
      break;
    case EV_FART:
      for(int k = 0; k < 10; k++) spawn(PT_STINK, e->x, e->y, rnd(17) - 8, rnd(11) - 5, 0, 40 + rnd(20));
      sfx_play(SFX_FART, pan); popup(e->x, e->y - RI(3), "PFFRT!", C_GOO);
      break;
    case EV_SPLOOSH:
      for(int k = 0; k < 24; k++) spawn(PT_GOO, e->x, e->y, rnd(51) - 25, -rnd(30), 0, 80 + rnd(40));
      spawn_ring(e->x, e->y, 11, C_GOO); spawn_ring(e->x, e->y, 8, C_WHITE);
      sfx_play(SFX_SPLAT, pan); popup(e->x, e->y - RI(4), "SPLOOSH!", C_GOO); ev_fx = e->x; ev_fy = e->y; ev_ft = 60;
      break;
    case EV_CRUMBLE: {                                      /* the run is over and the damaged walls give way (core aftershock) */
      for(int k = 0; k < 8; k++) spawn(PT_DUST, e->x + RI(rnd(9) - 4), e->y + RI(rnd(7) - 3), rnd(11) - 5, -rnd(6), 0, 24 + rnd(14));
      sfx_play(SFX_THUD, pan); sfx_play(SFX_CRACK, pan);
      popup(e->x, e->y - RI(4), "CRUMBLE!", C_GOLD);
      ev_fx = e->x; ev_fy = e->y; ev_ft = 60;
      break; }
    case EV_DISTRICT:                                       /* Landlord HQ: the next district is built in the same grid, with fresh men */
      cells0 = mx_cells_left(&world); dragging = 0; pad_used = 0; memset(parts, 0, sizeof parts); memset(pops, 0, sizeof pops); ev_ft = 0;
#if MX_DS_PARTICLE_HINT
      part_free_hint = 0;
#endif
#if MX_DS_FACADE_CACHE
      fac_dirty=1; /* Landlord HQ's next district replaces all room geometry. */
#endif
      sfx_play(SFX_WIN, 64); break;
    case EV_WIN:    sfx_play(SFX_WIN, 64); music_set(0); break;
    case EV_LOSE:   sfx_play(SFX_LOSE, 64); music_set(0); break;
    }
  }
}

/* ------------------------------------------------------------- world drawing */
static void draw_prong_post(const View *v){
  int ppc = v->ppc, ax = vx(v, MX_ANCHOR_X), ay = vy(v, MX_ANCHOR_Y);
  int gy = MX_ROWS * ppc - v->cy, th = ppc * 3 / 4 < 2 ? 2 : ppc * 3 / 4;
  int fork = ay + ppc * 6;
  int tl = ax - ppc * 3, tr = ax + ppc * 3, tip = ay + ppc;
  gfx_rect(v->buf, ax - th / 2, fork, th, gy - fork, MATPAL(MX_WOOD) + SH_LO);
  gfx_rect(v->buf, ax - th / 2, fork, th > 2 ? th / 2 : 1, gy - fork, MATPAL(MX_WOOD) + SH_BASE);
  gfx_tline(v->buf, ax, fork, tl, tip, th, MATPAL(MX_WOOD) + SH_LO);
  gfx_tline(v->buf, ax, fork, tr, tip, th, MATPAL(MX_WOOD) + SH_LO);
  gfx_tline(v->buf, ax, fork - 1, tl, tip - 1, th > 2 ? th - 1 : 1, MATPAL(MX_WOOD) + SH_BASE);
  gfx_tline(v->buf, ax, fork - 1, tr, tip - 1, th > 2 ? th - 1 : 1, MATPAL(MX_WOOD) + SH_BASE);
  gfx_ell(v->buf, tl, tip, th / 2 + 1, th / 2 + 1, MATPAL(MX_WOOD) + SH_HI);
  gfx_ell(v->buf, tr, tip, th / 2 + 1, th / 2 + 1, MATPAL(MX_WOOD) + SH_HI);
}

static void draw_band(const View *v, int front){
  const MxMan *m = &world.man;
  if(m->mode != MAN_AIM) return;
  int ppc = v->ppc, ax = vx(v, MX_ANCHOR_X), ay = vy(v, MX_ANCHOR_Y);
  int tl = ax - ppc * 3, tr = ax + ppc * 3, tip = ay + ppc;
  int mx_ = vx(v, m->x), my = vy(v, m->y), th = ppc > 2 ? 2 : 1;
  if(!front) gfx_tline(v->buf, tl, tip, mx_ - ppc, my + ppc / 2, th + 1, C_HAIR2);
  else       gfx_tline(v->buf, tr, tip, mx_ + ppc, my + ppc / 2, th + 1, C_HAIR);
}

/* The close-up and overview draw the same trajectory in world coordinates.
   Cache up to 16 points per presented frame; both screens still project
   independently, preserving their original sampled pixels. */
#ifndef MX_DS_AIM_CACHE
#define MX_DS_AIM_CACHE 1
#endif
#if MX_DS_AIM_CACHE
static real aimdot_x[16], aimdot_y[16];
static int aimdot_frame = -1, aimdot_count;
#endif
static void draw_aim_dots(const View *v){
  const MxMan *m = &world.man;
  if(m->mode != MAN_AIM) return;
  real dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
  if(rmul(dx, dx) + rmul(dy, dy) < R(3)) return;
#if MX_DS_AIM_CACHE
  if(aimdot_frame != frame){
    aimdot_frame = frame; aimdot_count = 0;
    for(int k = 1; k <= 16; k++){
      real ox, oy;
      mx_preview_point(&world, RI(k) / 9, &ox, &oy);
      int gx = ox >> 12, gy = oy >> 12;
      if(gx < 0 || gx >= MX_COLS || gy >= MX_ROWS) break;
      if(gy >= 0 && world.grid[gy * MX_COLS + gx]) break;
      aimdot_x[aimdot_count] = ox; aimdot_y[aimdot_count] = oy; aimdot_count++;
    }
  }
#endif
  for(int k = 1; k <= 16; k++){
    real ox, oy;
#if MX_DS_AIM_CACHE
    if(k > aimdot_count) break;
    ox = aimdot_x[k - 1]; oy = aimdot_y[k - 1];
#else
    mx_preview_point(&world, RI(k) / 9, &ox, &oy);
    int gx = ox >> 12, gy = oy >> 12;
    if(gx < 0 || gx >= MX_COLS || gy >= MX_ROWS) break;
    if(gy >= 0 && world.grid[gy * MX_COLS + gx]) break;
#endif
    int px = vx(v, ox), py = vy(v, oy);
    if(k & 1){ gfx_rect(v->buf, px - 1, py - 1, 3, 3, C_BLACK); gfx_px(v->buf, px, py, C_WHITE); }
    else     { gfx_rect(v->buf, px - 2, py - 2, 5, 5, C_BLACK); gfx_rect(v->buf, px - 1, py - 1, 3, 3, C_GOLD); }
  }
}

#ifndef MX_DS_WALL_INLINED
#define MX_DS_WALL_INLINED 1
/* The hot DS camera zooms are 4px and 2px per cell. Write the exact opaque
   wallpaper tile directly for entirely visible cells, avoiding hundreds of
   calls to gfx_wall() and its repeated size/clipping dispatch every frame.
   Damaged rooms and edges still obey the SAME world arrays and tile rules. */
#endif
#ifndef MX_DS_WALL_DIRECT_RUNS
#define MX_DS_WALL_DIRECT_RUNS 0 /* A/B: longer spans are still slower on x86; not enabled by default */
/* Long intact wallpaper spans: one clipped row-fill per tile row instead of
   repeating a 4x4 tile blit for every cell. Accent pixels are identical.
   Short/edge-clipped spans use the exact legacy gfx_wall path. Can A/B with
   -DMX_DS_WALL_DIRECT_RUNS=0 (no changes to rooms, physics or assets). */
#endif
#ifndef MX_DS_WALL_RUNS
#define MX_DS_WALL_RUNS 0 /* older gfx_rect-based run prototype was slower */
#endif
#ifndef MX_DS_ROOM_VISIBLE_SCAN
#define MX_DS_ROOM_VISIBLE_SCAN 1
/* Wallpaper only exists inside exported MxRoom rectangles. Limit the scan
   to their intersection with the camera, reading live paper/grid cells so
   explosions and HQ district transitions cannot leave stale scenery. */
#endif
static void draw_rooms(const View *v){
  int ppc = v->ppc;
  int x0 = v->cx > 0 ? v->cx / ppc : 0, x1 = (v->cx + 256) / ppc + 1; if(x1 > MX_COLS) x1 = MX_COLS;
  int y0 = v->cy > 0 ? v->cy / ppc : 0, y1 = (v->cy + 192) / ppc + 1; if(y1 > MX_ROWS) y1 = MX_ROWS;
#if MX_DS_ROOM_VISIBLE_SCAN && MX_DS_WALL_INLINED && !MX_DS_WALL_DIRECT_RUNS && !MX_DS_WALL_RUNS
  /* Skip every off-room tile rather than probing paper/grid for the entire
     camera rectangle. Tile colors always come from the CURRENT world arrays;
     room lists and paper are rebuilt together on district transitions.
     Overlapping rooms are safe: repeated paint has the same final pixels. */
  for(int r=0;r<world.nrooms;r++){
    const MxRoom *rm=&world.rooms[r];
    int xa=rm->x>x0?rm->x:x0, xb=rm->x+rm->w<x1?rm->x+rm->w:x1;
    int ya=rm->y>y0?rm->y:y0, yb=rm->y+rm->h<y1?rm->y+rm->h:y1;
    if(xa>=xb||ya>=yb)continue;
    if(ppc==4){
      for(int y=ya;y<yb;y++){
        int i=y*MX_COLS+xa,sy=y*4-v->cy,sx=xa*4-v->cx;
        for(int x=xa;x<xb;x++,i++,sx+=4){
          int p=world.paper[i];
          if(!p||world.grid[i])continue;
          if((unsigned)sx<=252u&&(unsigned)sy<=188u){
            int pal=(p-1)&3;
            u8 base=(u8)(C_WALL0+pal),accent=(u8)(C_WALLD0+pal);
            u8 *d=v->buf+(size_t)sy*256+sx;
            d[0]=base;d[1]=base;d[2]=base;d[3]=base;
            d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=base;
            d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=base;
            d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=accent;
          }else gfx_wall(v,p-1,sx,sy);
        }
      }
    }else if(ppc==2){
      for(int y=ya;y<yb;y++){
        int i=y*MX_COLS+xa,sy=y*2-v->cy,sx=xa*2-v->cx;
        for(int x=xa;x<xb;x++,i++,sx+=2){
          int p=world.paper[i];
          if(!p||world.grid[i])continue;
          if((unsigned)sx<=254u&&(unsigned)sy<=190u){
            int pal=(p-1)&3;
            u8 base=(u8)(C_WALL0+pal);
            u8 *d=v->buf+(size_t)sy*256+sx;
            d[0]=(u8)(C_WALLD0+pal);d[1]=base;
            d+=256;d[0]=base;d[1]=base;
          }else gfx_wall(v,p-1,sx,sy);
        }
      }
    }else{
      for(int y=ya;y<yb;y++)for(int x=xa;x<xb;x++){
        int i=y*MX_COLS+x;
        if(world.paper[i]&&!world.grid[i])gfx_wall(v,world.paper[i]-1,x*ppc-v->cx,y*ppc-v->cy);
      }
    }
  }
#elif MX_DS_WALL_DIRECT_RUNS
  /* Built from the CURRENT destructible cell arrays each draw: no stale
     cached scenery after an explosion, district swap or collapsed room.
     Only long fully visible spans take this path, so clipping and edge
     pixels retain the original tile blitter. */
  for(int y=y0;y<y1;y++){
    int base=y*MX_COLS,x=x0,sy=y*ppc-v->cy;
    while(x<x1){
      int idx=base+x,paper=world.paper[idx];
      if(!paper || world.grid[idx]){x++;continue;}
      int pal=(paper-1)&3,first=x++;
      while(x<x1){
        int q=base+x,p=world.paper[q];
        if(!p||world.grid[q]||((p-1)&3)!=pal)break;
        x++;
      }
      int n=x-first,sx=first*ppc-v->cx;
      if(n<10||sx<0||sy<0||sx+n*ppc>256||sy+ppc>192){
        for(int k=0;k<n;k++)gfx_wall(v,pal,sx+k*ppc,sy);
        continue;
      }
      u8 col=(u8)(C_WALL0+pal),dot=(u8)(C_WALLD0+pal);
      u8 *row=v->buf+(size_t)sy*256+sx;
      for(int j=0;j<ppc;j++,row+=256)memset(row,col,(size_t)n*ppc);
      int off=ppc==2?0:ppc-1;
      u8 *accent=v->buf+(size_t)(sy+off)*256+sx+off;
      for(int k=0;k<n;k++)accent[k*ppc]=dot;
    }
  }
#elif MX_DS_WALL_RUNS
  /* The wallpaper tile is a solid flat shade with exactly ONE darker
     accent pixel (see make_tiles in gfx.c). Batch contiguous intact cells
     of the same palette into 2-4 horizontal spans rather than calling
     gfx_wall once per cell. Damaged/broken cells split runs at their exact
     boundary, so collisions and destruction still reveal identical art.
     No cache, extra framebuffer, heap allocation or new palette. */
  for(int y=y0;y<y1;y++){
    int base=y*MX_COLS,x=x0,sy=y*ppc-v->cy;
    while(x<x1){
      int idx=base+x,paper=world.paper[idx];
      if(!paper || world.grid[idx]){x++;continue;}
      int pal=(paper-1)&3,first=x++;
      while(x<x1){
        int q=base+x,p=world.paper[q];
        if(!p||world.grid[q]||((p-1)&3)!=pal)break;
        x++;
      }
      int n=x-first,sx=first*ppc-v->cx;
      if(n<3){
        for(int k=0;k<n;k++)gfx_wall(v,pal,sx+k*ppc,sy);
        continue;
      }
      for(int j=0;j<ppc;j++)gfx_rect(v->buf,sx,sy+j,n*ppc,1,(u8)(C_WALL0+pal));
      int dark_y=sy+(ppc==2?0:ppc-1);
      int dark_x=sx+(ppc==2?0:ppc-1);
      for(int k=0;k<n;k++)gfx_px(v->buf,dark_x+k*ppc,dark_y,(u8)(C_WALLD0+pal));
    }
  }
#elif MX_DS_WALL_INLINED
  if(ppc==4){
    for(int y=y0;y<y1;y++){
      int i=y*MX_COLS+x0,sy=y*4-v->cy,sx=x0*4-v->cx;
      for(int x=x0;x<x1;x++,i++,sx+=4){
        int p=world.paper[i];
        if(!p||world.grid[i])continue;
        if((unsigned)sx<=252u&&(unsigned)sy<=188u){
          int pal=(p-1)&3;
          u8 base=(u8)(C_WALL0+pal),accent=(u8)(C_WALLD0+pal);
          u8 *d=v->buf+(size_t)sy*256+sx;
          d[0]=base;d[1]=base;d[2]=base;d[3]=base;
          d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=base;
          d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=base;
          d+=256;d[0]=base;d[1]=base;d[2]=base;d[3]=accent;
        }else gfx_wall(v,p-1,sx,sy);
      }
    }
  }else if(ppc==2){
    for(int y=y0;y<y1;y++){
      int i=y*MX_COLS+x0,sy=y*2-v->cy,sx=x0*2-v->cx;
      for(int x=x0;x<x1;x++,i++,sx+=2){
        int p=world.paper[i];
        if(!p||world.grid[i])continue;
        if((unsigned)sx<=254u&&(unsigned)sy<=190u){
          int pal=(p-1)&3;
          u8 base=(u8)(C_WALL0+pal);
          u8 *d=v->buf+(size_t)sy*256+sx;
          d[0]=(u8)(C_WALLD0+pal);d[1]=base;
          d+=256;d[0]=base;d[1]=base;
        }else gfx_wall(v,p-1,sx,sy);
      }
    }
  }else{
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
      int i=y*MX_COLS+x;
      if(world.paper[i]&&!world.grid[i])
        gfx_wall(v,world.paper[i]-1,x*ppc-v->cx,y*ppc-v->cy);
    }
  }
#else
  for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){
    int i=y*MX_COLS+x;
    if(world.paper[i]&&!world.grid[i])gfx_wall(v,world.paper[i]-1,x*ppc-v->cx,y*ppc-v->cy);
  }
#endif
}

#ifndef MX_DS_RIP_VIEW_CLIP
#define MX_DS_RIP_VIEW_CLIP 1
/* Only visit camera-visible tiles after a room wall is torn. Keep the old
   full-room walk for matched framebuffer/performance A/B regression tests. */
#endif

/* Damage-only paper edge pass, AFTER any wallpaper drawing path.
   Iterates known room rectangles rather than scanning the entire 120x60
   world; the reference and inlined renderers retain identical pixels.
   Four isolated palette pixels per torn tile edge, no extra buffers. */
static void draw_room_rips(const View *v){
  if(!world.paper_torn || v->ppc!=4)return;
  for(int r=0;r<world.nrooms;r++){
    const MxRoom *rm=&world.rooms[r];
    int y0=rm->y,y1=y0+rm->h,x0=rm->x,x1=x0+rm->w;
#if MX_DS_RIP_VIEW_CLIP
    /* A 4px tile is fully drawable only at screen x=0..252, y=0..188.
       The normal camera may shake a few pixels past zero. C's truncation
       toward zero gives ceil(cx/4) for negative cx. */
    int bx0=v->cx>0?(v->cx+3)/4:v->cx/4;
    int by0=v->cy>0?(v->cy+3)/4:v->cy/4;
    int bx1=(v->cx+252)/4+1,by1=(v->cy+188)/4+1;
    if(x0<bx0)x0=bx0;
    if(x1>bx1)x1=bx1;
    if(y0<by0)y0=by0;
    if(y1>by1)y1=by1;
    if(x0>=x1||y0>=y1)continue;
#endif
    for(int y=y0;y<y1;y++){
      int sy=y*4-v->cy;
      if((unsigned)sy>188u)continue;
      for(int x=x0;x<x1;x++){
        int sx=x*4-v->cx;
        if((unsigned)sx>252u)continue;
        int i=y*MX_COLS+x,p=world.paper[i];
        if(!p||world.grid[i])continue;
        u8 *d=v->buf+(size_t)sy*256+sx;
        u8 accent=(u8)(C_WALLD0+((p-1)&3));
        if(x>0&&!world.paper[i-1]){d[256]=accent;d[768]=accent;}
        if(x+1<MX_COLS&&!world.paper[i+1]){d[512+3]=accent;d[768+3]=accent;}
        if(y>0&&!world.paper[i-MX_COLS]){d[1]=accent;d[2]=accent;}
        if(y+1<MX_ROWS&&!world.paper[i+MX_COLS])d[768+1]=accent;
      }
    }
  }
}

/* Façade depth pass, derived from the SAME destructible rooms as the core.
   It is painted behind real blocks, so broken masonry occludes it, and torn-away
   wallpaper removes windows and furniture automatically. No sprites or heap. */
#if MX_DS_FACADE_CACHE
static void facade_prepare(void){
  int nwin=0;
  for(int r=0;r<world.nrooms;r++){
    const MxRoom *rm=&world.rooms[r];
    FacRoom *out=&fac_room[r];
    out->first=(u8)nwin;
    out->deskx=(u8)(rm->x+2);
    out->desky=(u8)(rm->y+rm->h-2);
    if(rm->w<6||rm->h<5)continue;
    int n=rm->w>=18?3:rm->w>=11?2:1;
    for(int k=0;k<n;k++){
      FacWin *w=&fac_win[nwin++];
      w->wx=(u8)(rm->x+rm->w*(2*k+1)/(2*n));
      w->wy=(u8)(rm->y+rm->h/2);
      w->idx=(u16)(w->wy*MX_COLS+w->wx);
      w->wc=(u8)(rm->w>=10?4:3);
      w->hc=(u8)(rm->h>=7?4:3);
      w->glass=(u8)((r+k+world.level)%5==0?C_GOLD2:(r&1)?C_CITY2:C_SKY0+9);
    }
  }
  fac_dirty=0;
}
#endif
static void draw_facades(const View *v){
  int p = v->ppc;
  if(p <= 2) return;  /* overview is only 2 pixels per cell: avoid costly unreadable microdetails */
#if MX_DS_FACADE_CACHE
  if(fac_dirty)facade_prepare();
#endif
  for(int r = 0; r < world.nrooms; r++){
    const MxRoom *rm = &world.rooms[r];
    if(rm->w < 6 || rm->h < 5) continue;
    if((rm->x + rm->w)*p - v->cx < -4 || rm->x*p - v->cx > 260 ||
       (rm->y + rm->h)*p - v->cy < -4 || rm->y*p - v->cy > 196) continue;
    int n = rm->w >= 18 ? 3 : rm->w >= 11 ? 2 : 1;
    for(int k = 0; k < n; k++){
#if MX_DS_FACADE_CACHE
      const FacWin *cached=&fac_win[fac_room[r].first+k];
      int wx=cached->wx,wy=cached->wy,idx=cached->idx;
      if(world.grid[idx] || !world.paper[idx]) continue;
      int ww=cached->wc*p,hh=cached->hc*p;
#else
      int wx = rm->x + rm->w * (2*k + 1) / (2*n);
      int wy = rm->y + rm->h / 2;
      int idx = wy * MX_COLS + wx;
      if(world.grid[idx] || !world.paper[idx]) continue;
      int ww = (rm->w >= 10 ? 4 : 3) * p, hh = (rm->h >= 7 ? 4 : 3) * p;
#endif
      int x = wx*p - v->cx - ww/2, y = wy*p - v->cy - hh/2;
      if(x + ww < -3 || x > 259 || y + hh < -3 || y > 195) continue;
#if MX_DS_FACADE_CACHE
      u8 glass = cached->glass;
#else
      u8 glass = (r + k + world.level) % 5 == 0 ? C_GOLD2 : (r & 1) ? C_CITY2 : C_SKY0 + 9;
#endif
      gfx_rect(v->buf, x - 2, y - 2, ww + 4, hh + 4, C_SHADOW);
      gfx_rect(v->buf, x - 1, y - 1, ww + 2, hh + 2, C_FENCE);
      gfx_rect(v->buf, x, y, ww, hh, glass);
      gfx_rect(v->buf, x + 1, y + 1, ww/3, hh/3, C_CLOUD);
      gfx_rect(v->buf, x + ww/2, y, p > 2 ? 2 : 1, hh, C_WHITE);
      gfx_rect(v->buf, x, y + hh/2, ww, p > 2 ? 2 : 1, C_WHITE);
      gfx_rect(v->buf, x - p/2, y + hh + 1, ww + p, p > 2 ? 2 : 1, C_FENCE2);
      if(p >= 3){ gfx_rect(v->buf, x, y + 2, 1, hh - 3, C_RED2); gfx_rect(v->buf, x + ww - 1, y + 2, 1, hh - 3, C_RED2); }
    }
    /* Occupied interiors: furniture, wall pictures, and a small desk lamp.
       The same wallpaper check makes these vanish when the room is demolished. */
    if(rm->w >= 9 && rm->h >= 6){
#if MX_DS_FACADE_CACHE
      int cx=fac_room[r].deskx,cy=fac_room[r].desky;
#else
      int cx = rm->x + 2, cy = rm->y + rm->h - 2;
#endif
      if(cy < MX_ROWS && !world.grid[cy*MX_COLS+cx] && world.paper[cy*MX_COLS+cx]){
        int x = cx*p-v->cx, y = cy*p-v->cy;
        gfx_rect(v->buf, x, y, 3*p, p, C_SUIT);
        gfx_rect(v->buf, x, y-1, 3*p, 1, C_GOLD2);
        gfx_rect(v->buf, x, y+p, p/2+1, p, C_HAIR2);
        gfx_rect(v->buf, x+2*p, y+p, p/2+1, p, C_HAIR2);
        gfx_rect(v->buf, x+p, y-p*2, p, p, C_SUNHALO);
        gfx_rect(v->buf, x+p+p/2, y-p, 1, p, C_HAIR2);
      }
    }
  }
}
/* Only top-floor rooms get a roof edge: not a floating sticker on every story.
   The bevel requires an actual surviving roof voxel and so disappears on collapse. */
static void draw_roof_trim(const View *v){
  int p = v->ppc;
  if(p <= 2) return;  /* the thin bevel is invisible on the distant overview */
  for(int r = 0; r < world.nrooms; r++){
    const MxRoom *rm = &world.rooms[r];
    int roof = rm->y - 3;
    if(roof < 1 || rm->w < 5) continue;
    int upper = 0;
    for(int j = 0; j < world.nrooms; j++) if(j != r){
      const MxRoom *o = &world.rooms[j];
      if(o->y < rm->y && o->x < rm->x + rm->w && rm->x < o->x + o->w){ upper = 1; break; }
    }
    if(upper) continue;
    for(int x = rm->x - 1; x <= rm->x + rm->w; x++){
      if(x < 0 || x >= MX_COLS) continue;
      int i = roof*MX_COLS + x;
      if(!world.grid[i] || world.grid[i-MX_COLS]) continue;
      int sx=x*p-v->cx, sy=roof*p-v->cy;
      if(sx < -p || sx >= 256 || sy < -p || sy >= 192) continue;
      gfx_rect(v->buf, sx, sy, p, 1, C_GOLD2);
      if(p >= 3) gfx_rect(v->buf, sx+p-1, sy+1, 1, p-1, C_HAIR2);
    }
  }
}
/* A streetside rental notice and lamp, anchored to the street, never the camera. */
static void draw_street_dressing(const View *v){
  int p=v->ppc, x=34*p-v->cx, y=MX_ROWS*p-v->cy;
  if(x < -35 || x > 285 || y < 0 || y > 225) return;
  gfx_rect(v->buf, x-1, y-15*p, 3, 15*p, C_SUIT2);
  gfx_rect(v->buf, x-4, y-15*p-2, 9, 3, C_HAIR2);
  gfx_rect(v->buf, x-6, y-15*p+1, 13, 4, C_SUNHALO);
  gfx_rect(v->buf, x-3, y-15*p+1, 7, 2, C_GOLD2);
  gfx_rect(v->buf, x+5, y-8*p, 18, 10, C_HAIR2);
  gfx_rect(v->buf, x+6, y-8*p+1, 16, 8, C_WHITE);
  gfx_text(v->buf, x+7, y-8*p+2, p >= 3 ? "RENT?" : "NO", C_RED2, 1);
}

static void draw_voxels(const View *v){
  int ppc = v->ppc, wob = world.wob_on;
  int x0 = v->cx > 0 ? v->cx / ppc : 0, x1 = (v->cx + 256) / ppc + 1; if(x1 > MX_COLS) x1 = MX_COLS;
  int y0 = v->cy > 0 ? v->cy / ppc : 0, y1 = (v->cy + 192) / ppc + 1; if(y1 > MX_ROWS) y1 = MX_ROWS;
  for(int y = y0; y < y1; y++){
    const uint8_t *row = &world.grid[y * MX_COLS];
    int py = y * ppc - v->cy;
    for(int x = x0; x < x1; x++){
      int m = row[x];
      if(!m) continue;
      int i = y * MX_COLS + x, hp = world.hp[i] >> 12;
      int d = hp * 3 > MAXHP[m] * 2 ? 0 : hp * 3 > MAXHP[m] ? 1 : 2;
      if(d == 0 && world.brk[i]) d = 1;                       /* a snapped joint shows as a crack */
      int sx = x * ppc - v->cx, sy = py;
      if(wob){ sx += (world.ox[i] * ppc) >> 12; sy += (world.oy[i] * ppc) >> 12; }   /* swaying with the wobble */
      gfx_tile(v, m, d, (x ^ y) & 1, sx, sy);
    }
  }
}

static void draw_chunks(const View *v){
  int h = v->ppc / 2;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    const MxChunk *ch = &world.chunks[c];
    if(!ch->active) continue;
    for(int k = 0; k < ch->count; k++){
      const MxCell *a = &ch->cells[k];
      if(a->dead) continue;
      gfx_tile(v, a->m, 0, k & 1, vx(v, a->wx) - h, vy(v, a->wy) - h);
    }
  }
}

#ifndef MX_DS_CULL_TARGETS
#define MX_DS_CULL_TARGETS 0 /* enabled only if proven fast on the target */
#endif
static void draw_targets(const View *v){
  const MxMan *m = &world.man;
  for(int i=0;i<world.ntargets;i++){
    const MxTarget *t=&world.targets[i];
    if(!t->alive)continue;
    real wx,wy;mx_target_offset(&world,t,&wx,&wy);
    int sx=vx(v,t->x+wx),sy=vy(v,t->y+wy);
#if MX_DS_CULL_TARGETS
    /* All sprites use a common anchor: the silhouette never projects
       beyond +/- 10 cells horizontally or 12 above / 7 below it.
       Account for unusually long multi-hit health bars too.
       Skip both the lengthy landlord painter AND its health bar when
       the entire bounds lie offscreen. Simulation and AI are untouched. */
    int pad_x=v->ppc*10;
    if(t->hpmax>1){
      int bar=(t->hpmax*(v->ppc*7/8>1?v->ppc*7/8:1))/2+3;
      if(bar>pad_x)pad_x=bar;
    }
    if(sx < -pad_x || sx >= 256+pad_x ||
       sy < -v->ppc*12 || sy >= 192+v->ppc*7)continue;
#endif
    int scared=((m->mode==MAN_FLY||m->mode==MAN_STUCK)&&rabs(t->x-m->x)<RI(26))||t->pan>0;
    int st=(t->stun>0?1:0)|(t->wind>0?2:0)|(t->swing>0?4:0)|
           (t->mv==1?8:0)|(t->mv==2?16:0)|(t->armor?32:0);
    if(t->kind||st)spr_landlord_k(v->buf,sx,sy,v->ppc,frame+i*11,scared,t->kind,t->face<0?-1:1,st);
    else spr_landlord(v->buf,sx,sy,v->ppc,frame+i*11,scared);
    if(t->hpmax>1)spr_hpbar(v->buf,sx,sy,v->ppc,t->hp,t->hpmax);
  }
}

/* briefcases in the air (Batter Up) and the bounce pads of walk levels: three chevrons float up from each pink pad */
static void draw_cases(const View *v){
  for(int i = 0; i < MX_MAX_CASES; i++){
    const MxCase *c = &world.cases[i];
    if(c->active) spr_case(v->buf, vx(v, c->x), vy(v, c->y), v->ppc, (int)((c->rot * 2) >> 12) & 3);
  }
}
static void draw_pads(const View *v){
  int p = v->ppc, bob = (frame >> 3) & 3;
  for(int i = 0; i < world.npads; i++){
    const MxPad *pd = &world.pads[i];
    int cx = vx(v, RI(pd->x) + RI(pd->w) / 2), cy = vy(v, RI(pd->y));
    for(int k = 0; k < 3; k++){
      int y = cy - p * (3 + 3 * k) - bob; u8 c = k == 0 ? C_WHITE : k == 1 ? C_PINK : C_RED2;
      gfx_line(v->buf, cx - p * 5 / 4, y + p / 2, cx, y - p / 2, c); gfx_line(v->buf, cx, y - p / 2, cx + p * 5 / 4, y + p / 2, c);
    }
  }
}

/* the core's belly / rear springs, converted to pixels at this zoom (the core only moves them in the DS fixed-point format) */
static ManJig man_jig(const View *v){
  const MxMan *m = &world.man; ManJig j; int p = v->ppc;
  j.bx = (m->bx * p) >> 12; j.by = (m->by * p) >> 12; j.rx = (m->rx * p) >> 12; j.ry = (m->ry * p) >> 12;
  j.sx = j.sy = 0;
  if(m->sq > R(0.04)){
    int q = (m->sq * 30) >> 12;                                             /* up to 12 % squash along the impact */
    if(rabs(m->sqx) > rabs(m->sqy)){ j.sx = -q; j.sy = q / 2; } else { j.sy = -q; j.sx = q / 2; }
  }
  return j;
}

/* a man frame with its towel: the part of the cloth that is behind him, the sprite, then the part in front */
static void man_with_towel(u8 *b, ClothActor *ta, int x, int y, int ppc, int face, int fr, const ManJig *jg){
  cloth_actor_frame(ta, frame, spr_man_hand(fr), face);
  if(ta->live) gfx_cloth(b, &ta->c, x, y, ppc, face, 0);
  spr_man_frame(b, x, y, ppc, face, fr, jg);
  if(ta->live) gfx_cloth(b, &ta->c, x, y, ppc, face, 1);
}

/* chapter 2 pneumatic tubes: a balloon-held brass mouth to fly into, and the nozzle stub on the wall of the room it posts you into */
static void draw_tubes(const View *v){
  static const u8 TC[3] = { C_FIRE, C_UILINE, C_GOO };
  int p = v->ppc;
  for(int i = 0; i < world.ntubes; i++){
    const MxTube *t = &world.tubes[i]; u8 c = TC[i % 3];
    int ax = vx(v, t->ax), ay = vy(v, t->ay), bx = vx(v, t->bx), by = vy(v, t->by);
    int sway = (((frame >> 4) + i * 3) & 3) - 1, bxx = ax + sway, byy = ay - p * 31 / 4 + (((frame >> 5) + i) & 1);
    gfx_line(v->buf, bxx - p, byy + p * 3, ax - p * 3 / 2, ay - p * 3, C_DARK);                        /* ropes */
    gfx_line(v->buf, bxx + p, byy + p * 3, ax + p * 3 / 2, ay - p * 3, C_DARK);
    gfx_ell(v->buf, bxx, byy, p * 7 / 2 + 1, p * 4 + 1, C_BLACK); gfx_ell(v->buf, bxx, byy, p * 7 / 2, p * 4, c);   /* balloon */
    gfx_ell(v->buf, bxx - p, byy - p * 3 / 2, p * 3 / 4 + 1, p * 3 / 2, C_WHITE);
    if(p >= 4) gfx_text_c(v->buf, bxx, byy - 3, "IN", C_WHITE, C_BLACK, 1);
    gfx_ell(v->buf, ax, ay, p * 15 / 4, p * 15 / 4, C_GOLD2);                                           /* brass mouth */
    gfx_ell(v->buf, ax, ay, p * 13 / 4, p * 13 / 4, c);
    gfx_ell(v->buf, ax, ay, p * 11 / 4, p * 11 / 4, C_DARK);
    gfx_ell(v->buf, ax, ay, p, p, c); gfx_ell(v->buf, ax, ay, p / 2 + 1, p / 2 + 1, C_DARK);            /* swirl, still */
    gfx_rect(v->buf, bx - p * 19 / 4, by - p * 3 / 2, p * 4, p * 3, C_GOLD2);                          /* nozzle stub on the room's wall, shooting right */
    gfx_rect(v->buf, bx - p * 9 / 2, by - p * 5 / 4, p * 7 / 2, p * 5 / 8 + 1, C_GOLD);
    gfx_rect(v->buf, bx - p * 3 / 4, by - p * 13 / 8, p * 3 / 2, p * 13 / 4, c);
  }
}

/* the air valve: a pipe with an orange nozzle, a gauge, and a pressure bar while he is on it */
static void draw_valve(const View *v){
  if(dragging && world.valve_ok && world.man.mode == MAN_AIM && !world.man.seated){           /* ghost seat while a pull is in hand: a ring of dots that blinks */
    static const signed char RX[12] = { 100, 87, 50, 0, -50, -87, -100, -87, -50, 0, 50, 87 }, RY[12] = { 0, 50, 87, 100, 87, 50, 0, -50, -87, -100, -87, -50 };
    int gx = vx(v, MX_VALVE_X), gyy = vy(v, MX_VALVE_Y), rr = v->ppc * 5 / 2 + ((frame >> 3) & 1), on = (frame >> 2) & 1;
    for(int k = on; k < 12; k += 2) gfx_rect(v->buf, gx + RX[k] * rr / 100 - 1, gyy + RY[k] * rr / 100 - 1, 2, 2, C_WHITE);
  }
  int ppc = v->ppc, x = vx(v, MX_VALVE_X), y = vy(v, MX_VALVE_Y), gy = MX_ROWS * ppc - v->cy;
  const MxMan *m = &world.man; int f = m->mode == MAN_AIM ? (int)((m->inf * 100) >> 12) : 0;
  int th = ppc * 3 / 4 < 2 ? 2 : ppc * 3 / 4;
  gfx_rect(v->buf, x - ppc * 2, gy - 2, ppc * 4, 3, C_GRAY);
  gfx_rect(v->buf, x - th / 2, y + ppc, th, gy - y - ppc, C_SUIT);
  gfx_rect(v->buf, x - th / 2, y + ppc, th > 2 ? th / 2 : 1, gy - y - ppc, C_GRAY);
  gfx_tri(v->buf, x - ppc * 2, y + ppc * 2, x + ppc * 2, y + ppc * 2, x, y + 1, world.valve_ok ? C_FIRE : C_GRAY);   /* nozzle (grey while locked) */
  int gx = x + ppc * 4, gyy = y + ppc * 8;
  gfx_ell(v->buf, gx, gyy, ppc * 2 + 1, ppc * 2 + 1, C_BLACK); gfx_ell(v->buf, gx, gyy, ppc * 2, ppc * 2, C_WHITE);
  gfx_line(v->buf, gx, gyy, gx + (f - 50) * ppc / 30, gyy - ppc, f > 75 ? C_RED : C_BLACK);               /* needle */
  if(m->mode == MAN_AIM && f > 0){
    gfx_rect(v->buf, x - ppc * 2, y - ppc * 9, ppc * 10, ppc + 2, C_UIBG);
    gfx_rect(v->buf, x - ppc * 2 + 1, y - ppc * 9 + 1, f * (ppc * 10 - 2) / 100, ppc, f < 50 ? C_GOO : f < 80 ? C_GOLD : C_RED);
    if(f > 70 && (frame & 4)) gfx_text_c(v->buf, x + ppc * 3, y - ppc * 12, "DANGER", C_RED, C_BLACK, 1);
  } else if(m->mode == MAN_AIM && !dragging && !m->seated){
    if(!world.valve_ok) gfx_text_c(v->buf, x + ppc * 2, y - ppc * 5, "LOCKED", C_GRAY, C_BLACK, 1);
    else if(frame & 32) gfx_text_c(v->buf, x + ppc * 2, y - ppc * 5, "TAP: AIR", C_WHITE, C_BLACK, 1);
  }
}

static void draw_man(const View *v){
  const MxMan *m = &world.man;
  if(m->mode == MAN_NONE || m->mode == MAN_WAIT) return;
  int x = vx(v, m->x), y = vy(v, m->y);
  if(m->mode == MAN_WALK){                                                                  /* on his own feet: the turning-round frames step him along, three quarters of the sling size */
    if(m->hurtCd > 0 && (frame & 2)) return;                                                  /* blinks while he is protected after a hit */
    int p = v->ppc * 3 / 4, mv = m->gr && (m->vx > R(3) || m->vx < -R(3)), fr = MSF_SPIN_6 + (mv ? ((frame >> 2) & 1 ? 1 : -1) : 0), face = m->face < 0 ? -1 : 1;
    int fy = vy(v, m->y + R(2.375)) - (mv && ((frame >> 2) & 1) ? 1 : 0);
    spr_man_walk(v->buf, x, fy, p < 1 ? 1 : p, face, fr, 0);
    if(m->pT > 0){ int r = v->ppc * 5, ax = x + face * v->ppc * 4; for(int k = -3; k <= 3; k++) gfx_px(v->buf, ax + face * (r - k * k * r / 18), y + k * v->ppc * 3 / 4, C_WHITE); }   /* punch swoosh */
    return;
  }
  int pose = POSE_IDLE, face = 1;
  ManJig jg = man_jig(v);
  if(m->mode == MAN_FLY){ pose = m->dive ? POSE_DIVE : POSE_FLY; face = m->vx >= 0 ? 1 : -1; }
  else if(m->mode == MAN_STUCK) pose = POSE_STUCK;
  else if(m->mode == MAN_AIM && dragging) pose = POSE_DRAG;
  if(m->mode == MAN_AIM) gfx_ell(v->buf, x, y + v->ppc, v->ppc * 2, v->ppc * 3 / 2, C_HAIR2);        /* sling pouch */
  if(m->mode == MAN_AIM && !dragging) man_with_towel(v->buf, &tow_play, x, y, v->ppc, face, mi_frame(&mi, frame), &jg);   /* idle gags */
  else {
    /* body angle, as the browser's ctx.rotate: flight pitches toward the path (core: ang), the belly flop tumbles (spin), a stuck man rocks,
       and a man being dragged leans a quarter of the way toward the sling.  Flying left is the mirror image, so the turn flips with the sprite. */
    real a = 0;
    if(m->mode == MAN_FLY) a = m->dive ? m->spin * face : m->ang * face;
    else if(m->mode == MAN_STUCK) a = rmul(mx_man_wobble(m), R(0.17));
    else if(m->mode == MAN_AIM && dragging) a = rmul(ratan2(MX_ANCHOR_Y - m->y, MX_ANCHOR_X - m->x), R(0.25));
    spr_man_jr(v->buf, x, y, v->ppc, face, pose, frame, &jg, (int)rsin(a), (int)rcos(a));
  }
  if(m->inf > 0 && (m->mode == MAN_AIM || m->mode == MAN_FLY)){                 /* only the belly swells; the face flushes red near the end */
    int f = (int)((m->inf * 100) >> 12), p = v->ppc, rx = p * (2 + f * 5 / 40), ry = p * (2 + f * 4 / 40);
    int bx = x + face * p * f / 60, by = y + p + p * f / 80;
    gfx_ell(v->buf, bx, by, rx + 1, ry + 1, C_SKIN2); gfx_ell(v->buf, bx, by, rx, ry, C_SKIN);
    gfx_ell(v->buf, bx - rx / 3, by - ry / 2, rx / 4 + 1, ry / 5 + 1, C_SKIN3);                       /* shine */
    if(f > 30) for(int k = -1; k <= 1; k++) gfx_line(v->buf, bx + rx / 3, by + k * ry / 3, bx + rx * 4 / 5, by + k * ry / 3, C_SKIN2);   /* stretch marks */
    gfx_ell(v->buf, bx + face * rx * 3 / 4, by + ry / 8, 1 + f / 40, 1 + f / 40, C_SKIN2);            /* belly button */
    if(f > 45) gfx_ell(v->buf, x, y - p * 3, p + 1 + f / 50, p, (f > 85 && (frame & 4)) ? C_RED2 : C_RED);   /* blush */
  }
}

static void draw_parts(const View *v){
  for(int i = 0; i < NPART; i++){
    const Part *p = &parts[i];
    if(p->life <= 0) continue;
    int px = (p->x * v->ppc >> 6) - v->cx, py = (p->y * v->ppc >> 6) - v->cy;
    if(px < -8 || px > 264 || py < -8 || py > 200) continue;
    int age = p->max - p->life, big = v->ppc >= 4;
    switch(p->type){
    case PT_DEBRIS: gfx_rect(v->buf, px, py, big ? 2 : 1, big ? 2 : 1, p->col); break;
    case PT_SPARK:  gfx_px(v->buf, px, py, p->col); if(big) gfx_px(v->buf, px - (p->vx > 0 ? 1 : -1), py, C_FIRE); break;
    case PT_GOO:    gfx_rect(v->buf, px, py, big ? 2 : 1, big ? 2 : 1, age > p->max - 20 ? C_GOO2 : C_GOO); break;
    case PT_HAT:    spr_hat(v->buf, px, py, big ? 2 : 1, 0); break;
    case PT_STAR:   spr_star(v->buf, px, py, 1, C_STAR, C_STAR2); break;
    case PT_DUST:   gfx_dither(v->buf, px - 1, py - 1, 3 + age / 8, 3 + age / 8, C_SMOKE); break;
    case PT_STINK: {                                                       /* green stink cloud: blobs of dither that swell then thin out */
      int r = 2 + age * (big ? 6 : 3) / p->max, fade = age * 5 > p->max * 4;
      gfx_dither(v->buf, px - r, py - r, r * 2, r * 2, fade ? C_GOO2 : C_GOO);
      if(!fade && big) gfx_dither(v->buf, px - r / 2, py - r / 2, r, r, C_GOO2);
      break; }
    case PT_FLY: { int a = age * 5; gfx_px(v->buf, px + ((a * 7) & 7) - 3, py + ((a * 5) & 7) - 3, C_BLACK); if(big) gfx_px(v->buf, px + ((a * 7) & 7) - 2, py + ((a * 5) & 7) - 3, C_GRAY); break; }
    case PT_ZZZ:   gfx_text_o(v->buf, px, py, age > p->max / 2 ? "z" : "Z", C_WHITE, C_BLACK, 1); break;
    case PT_SWEAT: gfx_rect(v->buf, px, py, 1, big ? 2 : 1, C_CLOUD2); break;
    case PT_HEART: {                                                       /* a small pink heart (5x4 at the big scale, 3x2 at the small one) */
      int w = ((age >> 3) & 1) ? 1 : 0; px += w;
      if(big){ gfx_rect(v->buf, px - 2, py - 1, 2, 2, C_PINK); gfx_rect(v->buf, px + 1, py - 1, 2, 2, C_PINK); gfx_rect(v->buf, px - 2, py, 5, 1, C_PINK); gfx_rect(v->buf, px - 1, py + 1, 3, 1, C_PINK); gfx_px(v->buf, px, py + 2, C_PINK); gfx_px(v->buf, px - 1, py - 1, C_WHITE); }
      else { gfx_px(v->buf, px - 1, py, C_PINK); gfx_px(v->buf, px + 1, py, C_PINK); gfx_rect(v->buf, px - 1, py + 1, 3, 1, C_PINK); gfx_px(v->buf, px, py + 2, C_PINK); }
      break; }
    case PT_GLINT: {                                                       /* a four-point sparkle that grows and shrinks */
      int r = age < p->max / 2 ? 1 + age * 2 / p->max * 2 : 2; if(big && age > p->max / 3 && age < p->max * 2 / 3) r = 3;
      gfx_px(v->buf, px, py, C_WHITE); gfx_rect(v->buf, px - r, py, r * 2 + 1, 1, C_GOLD2); gfx_rect(v->buf, px, py - r, 1, r * 2 + 1, C_GOLD2); gfx_px(v->buf, px, py, C_WHITE);
      break; }
    case PT_RING: {                                                        /* swells fast, then slows (ease out), thins out over the last third */
      if(age * 3 > p->max * 2 && (frame & 1)) break;
      int u = 256 - (256 - age * 256 / p->max) * (256 - age * 256 / p->max) / 256;
      int rad = (p->r * v->ppc >> 6) * u >> 8, n = 8 + rad * 2; if(n > 64) n = 64;                /* about a dot every 3 px */
      for(int k = 0; k < n; k++){
        real a = rdiv(RI(k) * 6283 / n, RI(1000));
        gfx_px(v->buf, px + (int)((rcos(a) * rad) >> 12), py + (int)((rsin(a) * rad) >> 12), p->col);
      }
      break; }
    case PT_SMOKE: {
      int r = 2 + age * (big ? 7 : 4) / p->max, q = age * 4 / p->max;
      u8 c = q == 0 ? C_WHITE : q == 1 ? C_FIRE : q == 2 ? C_FIRE2 : C_SMOKE2;
      if(q == 3 && (age & 1)) c = C_SMOKE;
      if(age * 5 > p->max * 4) gfx_dither(v->buf, px - r, py - r, r * 2, r * 2, C_SMOKE2);
      else gfx_ell(v->buf, px, py, r, r, c);
      break; }
    }
  }
}

static void draw_popups(const View *v, int sc){
  for(int i = 0; i < 4; i++){
    const Popup *p = &pops[i];
    if(p->life <= 0) continue;
    int px = (p->x * v->ppc >> 6) - v->cx, py = (p->y * v->ppc >> 6) - v->cy;
    if(p->life < 14 && (frame & 1)) continue;
    gfx_text_c(v->buf, px, py, p->txt, p->col, C_BLACK, sc);
  }
}

#ifdef MX_DS_PROFILE_HOST
extern void ds_perf_layer(int stage);
#define DS_LAYER(v,stage) do{if((v)->buf==buf_top)ds_perf_layer(stage);}while(0)
#else
#define DS_LAYER(v,stage) ((void)0)
#endif
static void draw_world(const View *v, int parallax){
  DS_LAYER(v,0);
  gfx_bg(v, parallax);
  DS_LAYER(v,1);
  gfx_ground(v);
  draw_street_dressing(v);
  draw_prong_post(v);
  draw_band(v, 0);
  DS_LAYER(v,2);
  draw_rooms(v);
  draw_room_rips(v);
  draw_facades(v);
  DS_LAYER(v,3);
  draw_voxels(v);
  DS_LAYER(v,4);
  draw_roof_trim(v);
  DS_LAYER(v,5);
  draw_chunks(v);
  DS_LAYER(v,6);
  draw_targets(v);
  if(world.ntubes) draw_tubes(v);
  if(world.npads) draw_pads(v);
  draw_cases(v);
  if(world.valve_ok) draw_valve(v);
  DS_LAYER(v,7);
  { const MxMan *m = &world.man;                                   /* his shadow on the ground, smaller the higher he is */
    if(m->mode == MAN_AIM || m->mode == MAN_FLY){ int hgt = gfx_clamp((int)((RI(MX_ROWS) - m->y) >> 12), 0, 40), rw = (v->ppc * (5 * (80 - hgt))) / 80;
      gfx_dither(v->buf, vx(v, m->x) - rw, vy(v, RI(MX_ROWS)) + 1, rw * 2, v->ppc > 2 ? 2 : 1, C_BLACK); } }
  draw_man(v);
  draw_band(v, 1);
  draw_aim_dots(v);
  DS_LAYER(v,8);
  draw_parts(v);
  DS_LAYER(v,9);
}

/* ------------------------------------------------------------------- UI bits */
static void panel(u8 *b, int x, int y, int w, int h){
  gfx_dither(b, x + 3, y + 3, w, h, C_BLACK);
  gfx_rect(b, x, y, w, h, C_UIBG);
  gfx_rect(b, x, y, w, 1, C_UILINE); gfx_rect(b, x, y + h - 1, w, 1, C_UILINE);
  gfx_rect(b, x, y, 1, h, C_UILINE); gfx_rect(b, x + w - 1, y, 1, h, C_UILINE);
  gfx_rect(b, x + 1, y + 1, w - 2, 1, C_UIHI);
}
static int hit(const touchPosition *t, int x, int y, int w, int h){ return t->px >= x && t->px < x + w && t->py >= y && t->py < y + h; }
static void button(u8 *b, int x, int y, int w, int h, const char *s){
  gfx_rect(b, x, y, w, h, C_UILINE);
  gfx_rect(b, x + 1, y + 1, w - 2, h - 2, C_UIHI);
  gfx_rect(b, x + 1, y + h - 2, w - 2, 1, C_UIBG);
  gfx_text_c(b, x + w / 2, y + (h - 5) / 2, s, C_WHITE, C_UIBG, 1);
}
/* walk levels: the touch screen shows these buttons while he is on foot (the stylus can only press one; the d-pad and A/B/Y/X play the same moves) */
static const struct { int x, w; const char *s; } WB[6] = { { 4, 22, "<" }, { 28, 22, ">" }, { 58, 44, "JUMP" }, { 104, 50, "PUNCH" }, { 156, 44, "SLAM" }, { 204, 48, "GIVE UP" } };
static int walk_btn_at(const touchPosition *t){ for(int i = 0; i < 6; i++) if(hit(t, WB[i].x, 168, WB[i].w, 20)) return i; return -1; }
static void draw_men_icons(u8 *b, int x, int y, int bust){
  for(int i = 0; i < world.men; i++){
    if(bust) spr_man_bust(b, x + i * 14, y, 2);
    else     spr_man(b, x + i * 14, y, 2, 1, POSE_IDLE, 0);
  }
}
static void draw_hats(u8 *b, int x, int y){
  for(int i = 0; i < world.ntargets; i++) spr_hat(b, x + i * 9, y, 2, world.targets[i].alive ? 1 : 2);
}
static int demolished_pct(void){ return gfx_clamp(100 - mx_cells_left(&world) * 100 / (cells0 ? cells0 : 1), 0, 100); }

static void top_hud(u8 *b){
  const char *nm = mx_level_name(world.level);
  gfx_rect(b, 0, 0, 256, 11, C_UIBG); gfx_rect(b, 0, 11, 256, 1, C_UILINE);
  gfx_text(b, 4, 3, "LV", C_GOLD, 1); gfx_text(b, 14, 3, gfx_int(world.level + 1), C_GOLD, 1);
  gfx_text(b, 24, 3, nm, C_WHITE, 1);
  const char *sc = gfx_int(disp_score);
  gfx_text(b, 252 - gfx_text_w(sc, 1), 3, sc, C_GOLD, 1);
  gfx_text(b, 252 - gfx_text_w(sc, 1) - 6, 3, "$", C_GOLD2, 1);
  /* demolition meter */
  int pct = demolished_pct();
  gfx_rect(b, 4, 15, 62, 6, C_UIBG); gfx_rect(b, 5, 16, 60, 4, C_DARK);
  gfx_rect(b, 5, 16, pct * 60 / 100, 4, pct < 40 ? C_GOO : pct < 75 ? C_GOLD : C_RED);
  gfx_text_o(b, 70, 15, "WRECKED", C_WHITE, C_BLACK, 1);
  gfx_text_o(b, 70 + 30, 15, gfx_int(pct), C_WHITE, C_BLACK, 1);
  gfx_text_o(b, 70 + 30 + gfx_text_w(gfx_int(pct), 1) + 2, 15, "%", C_WHITE, C_BLACK, 1);
  if(world.combo > 1 && world.combo_left > 0){ gfx_rect(b, 140, 14, 110, 14, C_UIBG);
    gfx_text_o(b, 144, 17, mx_combo_taunt(world.combo), C_PINK, C_BLACK, 1);
    gfx_rect(b, 140, 28, (int)(rmul(world.combo_left, RI(110)) / R(2.5)), 2, C_GOLD); }
  gfx_text_o(b, 4, 24, "LANDLORDS", C_WHITE, C_BLACK, 1);
  draw_hats(b, 46, 28);
  if(world.man.mode == MAN_WALK && world.state == MX_PLAY){                      /* walk levels: the clock, and what the buttons do */
    int k = gfx_clamp((int)((world.man.wt * 100 / MX_WALK_T) >> 12), 0, 100);
    gfx_rect(b, 4, 36, 82, 7, C_UIBG); gfx_rect(b, 5, 37, 80, 5, C_DARK); gfx_rect(b, 5, 37, k * 80 / 100, 5, k > 30 ? C_GOO : C_RED);
    gfx_text_o(b, 90, 36, "TIME", C_WHITE, C_BLACK, 1);
    gfx_text_c(b, 128, 183, "PAD MOVE  A JUMP  B PUNCH  DOWN SLAM  L GIVE UP", C_WHITE, C_BLACK, 1);
  }
  toast_tick();
  if(toast_t >= 0){                                                             /* achievement popup: drops in, gold frame, star badge, time bar */
    int y = toast_t < 12 ? toast_t * 4 - 48 : toast_t > 138 ? (150 - toast_t) * 4 - 48 : 0;
    int oc = toast_id >= 100; u8 fr = oc ? C_GRASS : C_GOLD;
    gfx_rect(b, 26, 100 + y - 1, 204, 32, fr); gfx_rect(b, 27, 101 + y - 1, 202, 30, C_UIBG);
    gfx_ell(b, 44, 116 + y, 10, 10, oc ? C_GRASS2 : C_GOLD2); gfx_ell(b, 44, 116 + y, 8, 8, fr); gfx_text_c(b, 44, 112 + y, oc ? "+" : "*", C_BLACK, fr, 1);
    gfx_text(b, 62, 105 + y, oc ? "NEW OUTFIT" : "ACHIEVEMENT", fr, 1);
    gfx_text(b, 62, 116 + y, oc ? mx_outfit(toast_id - 100)->name : mx_ach_name(toast_id), C_WHITE, 1);
    gfx_rect(b, 27, 128 + y, 202 * (150 - toast_t) / 150, 2, fr);
  }
}

static void banner(u8 *b){
  if(world.banner <= 0 || app != ST_PLAY || world.man.mode != MAN_AIM || dragging) return;
  int t = world.banner * 60 >> 12;                       /* frames left */
  int slide = t > 120 ? (t - 120) * 8 : (t < 24 ? (24 - t) * 12 : 0);
  gfx_rect(b, 0, 70, 256, 44, C_UIBG);
  gfx_rect(b, 0, 69, 256, 1, C_UILINE); gfx_rect(b, 0, 114, 256, 1, C_UILINE);
  { static const char *const DN[MX_HQ_DISTRICTS] = { "DISTRICT 1 OF 3", "DISTRICT 2 OF 3", "DISTRICT 3 OF 3" };
    gfx_text_c(b, 128 - slide, 76, world.level == MX_HQ_LEVEL ? DN[world.district % MX_HQ_DISTRICTS] : "LEVEL", C_GOLD, C_BLACK, 1); }
  gfx_text_c(b, 128 + slide, 84, mx_level_name(world.level), C_WHITE, C_BLACK, 2);
  gfx_text_c(b, 128 - slide, 102, "EVICT EVERY LANDLORD", C_PINK, C_BLACK, 1);
}

static void end_panel(u8 *b){
  if(world.state == MX_PLAY) return;
  int win = world.state == MX_WIN;
  panel(b, 24, 34, 208, 124);
  gfx_text_c(b, 128, 42, win ? (world.level == MX_NUM_LEVELS - 1 ? "ALL LANDLORDS EVICTED!" : "EVICTION COMPLETE!") : "THE LANDLORDS LIVE...", win ? C_GOLD : C_RED, C_BLACK, 2);
  if(win){
    man_with_towel(b, &tow_win[0], 52, 98, 3, 1, mi_dance_frame(frame), 0);          /* victory butt-wiggle, one on each side */
    man_with_towel(b, &tow_win[1], 204, 98, 3, -1, mi_dance_frame(frame + 14), 0);
    int stars = mx_stars_for(&world);
    for(int k = 0; k < 3; k++){
      int on = k < stars && end_t > 24 + k * 18;
      spr_star(b, 128 + (k - 1) * 34, 84, on ? 4 : 3, on ? C_STAR : C_UIHI, on ? C_STAR2 : C_UIBG);
    }
  } else {
    spr_man_j(b, 128, 90, 4, 1, POSE_SAD, frame, 0);
  }
  gfx_text_c(b, 128, 120, "DAMAGE", C_UILINE, C_UIBG, 1);
  gfx_text_c(b, 128, 128, gfx_int(disp_score), C_GOLD, C_BLACK, 2);
  if(win && world.men > 0){
    gfx_text_c(b, 128, 144, world.level == MX_NUM_LEVELS - 1 ? "THE END - THANKS FOR PLAYING" : "SPARE MEN BONUS INCLUDED", C_GOO, C_UIBG, 1);
  } else if(!win){
    gfx_text_c(b, 128, 144, "DAMAGE IS NEVER ENOUGH", C_UILINE, C_UIBG, 1);
  }
  if(win && (int)stats.best[world.level] >= world.score && end_t > 60 && (frame & 16)) gfx_text_c(b, 128, 150, "NEW BEST!", C_PINK, C_BLACK, 1);
}

/* the 2 px/cell overview + HUD strip on the touch screen */
static void bottom_overview(View *v){
  draw_world(v, 0);
  draw_popups(v, 1);
  gfx_rect(v->buf, 0, 164, 256, 28, C_UIBG); gfx_rect(v->buf, 0, 164, 256, 1, C_UILINE);
  if(world.man.mode == MAN_WALK){
    for(int i = 0; i < 6; i++) button(v->buf, WB[i].x, 168, WB[i].w, 20, WB[i].s);
  } else {
  draw_men_icons(v->buf, 12, 177, 0);
  draw_hats(v->buf, 100, 178);
  button(v->buf, 170, 170, 46, 16, "RESTART");
  button(v->buf, 220, 170, 32, 16, "MENU");
  }
  if(world.man.mode == MAN_FLY && !world.man.dive && (frame & 32)) gfx_text_c(v->buf, 128, 6, "TAP = BELLY FLOP!", C_WHITE, C_BLACK, 1);
}

/* Six real touch targets fit the 256x192 lower DS screen. The existing
   power gauge keeps its own 82px strip; no sprites or framebuffers added. */
static const struct { int x,y,w,h,pw; const char *name; } AIM_TOOLS[6]={
  {6,150,82,18,0,"AIR VALVE"},
  {94,150,49,18,PW_BEANS,"BEANS"},
  {146,150,49,18,PW_TOWEL,"TOWEL"},
  {198,150,51,18,PW_SPLAT,"SPLAT"},
  {94,172,75,16,PW_TURBO,"TURBO"},
  {174,172,75,16,PW_QUAKE,"QUAKE"}
};
static int aim_tool_at(const touchPosition *t){
  for(int i=0;i<6;i++)if(hit(t,AIM_TOOLS[i].x,AIM_TOOLS[i].y,AIM_TOOLS[i].w,AIM_TOOLS[i].h))return i;
  return -1;
}
/* the 4 px/cell sling zoom */
static void bottom_aim(View *v){
  draw_world(v,1);
  draw_popups(v,1);
  gfx_rect(v->buf,0,0,256,11,C_UIBG);gfx_rect(v->buf,0,11,256,1,C_UILINE);
  gfx_text(v->buf,4,3,"MEN",C_GOLD,1);
  draw_men_icons(v->buf,36,1,1);
  gfx_text(v->buf,256-4-gfx_text_w("SELECT RESTART",1),3,"SELECT RESTART",C_UILINE,1);
  const MxMan *m=&world.man;
  real dx=m->x-MX_ANCHOR_X,dy=m->y-MX_ANCHOR_Y;
  int pct=rsqrt(rmul(dx,dx)+rmul(dy,dy))*100/MX_PULL_MAX;
  gfx_rect(v->buf,0,149,256,43,C_UIBG);
  gfx_rect(v->buf,0,149,256,1,C_UILINE);
  gfx_text(v->buf,6,172,"POWER",C_WHITE,1);
  gfx_rect(v->buf,6,180,82,8,C_UIBG);gfx_rect(v->buf,7,181,80,6,C_DARK);
  gfx_rect(v->buf,7,181,gfx_clamp(pct,0,100)*80/100,6,pct<50?C_GOO:pct<85?C_GOLD:C_RED);
  if(pct<14&&(frame&32))gfx_text_c(v->buf,128,139,"DRAG MAN BACK - RELEASE TO FLING",C_WHITE,C_BLACK,1);
  for(int k=0;k<6;k++){
    int pw=AIM_TOOLS[k].pw;
    int left=k==0?mx_valve_unlocked(&stats):world.pw_left[pw]>0;
    int on=k==0?world.valve_ok:world.pw_sel==pw;
    int x=AIM_TOOLS[k].x,y=AIM_TOOLS[k].y,w=AIM_TOOLS[k].w,h=AIM_TOOLS[k].h;
    gfx_rect(v->buf,x,y,w,h,on?C_GOLD:C_UIBG);
    gfx_rect(v->buf,x,y,w,1,on?C_WHITE:C_UILINE);
    gfx_rect(v->buf,x,y,1,h,C_UILINE);
    gfx_rect(v->buf,x+w-1,y,1,h,C_UILINE);
    gfx_rect(v->buf,x,y+h-1,w,1,C_UILINE);
    gfx_text_c(v->buf,x+w/2,y+(h-5)/2,AIM_TOOLS[k].name,on?C_BLACK:left?C_WHITE:C_GRAY,on?C_GOLD:C_UIBG,1);
  }
}

/* ------------------------------------------------------------------ title */
/* outfit: recolour the man's boxers and towel palette entries (MS_PAL 5,6,7 boxers; 13,14,15 towel) */
/* ---- cartridge save (port 11): the blob is ~175 bytes.  save_sync() packs the stats and writes only when the bytes differ from what the chip holds, so it is
   safe to call at every calm moment: after a level ends, on the way back to the title and when leaving it for a level.  Never during play (a page write takes
   milliseconds and the bare runtime has no interrupts to hide it).  A missing or unwritable chip turns saving off for the session without a message. */
static u8 saved_blob[MX_SAVE_MAX]; static int saved_n, save_off;
static void save_load(void){
  u8 b[MX_SAVE_MAX];
  if(!card_save_read(b, MX_SAVE_MAX)) return;
  int o = 0;
  if(mx_meta_unpack_opt(&stats, &o, &opt, b, MX_SAVE_MAX)){ outfit_sel = o; saved_n = mx_meta_pack_opt(&stats, outfit_sel, &opt, saved_blob, sizeof saved_blob); }
}
static void save_sync(void){
  if(save_off) return;
  u8 b[MX_SAVE_MAX]; int n = mx_meta_pack_opt(&stats, outfit_sel, &opt, b, sizeof b);
  if(n <= 0 || (n == saved_n && memcmp(b, saved_blob, n) == 0)) return;
  if(card_save_write(b, n)){ memcpy(saved_blob, b, n); saved_n = n; } else save_off = 1;
}
static void apply_outfit(int o){
  uint8_t sh[6][3]; mx_outfit_shades(o, sh);
  static const int idx[6] = { 5, 6, 7, 13, 14, 15 };
  for(int k = 0; k < 6; k++){
    u16 c = RGB15(sh[k][0] >> 3, sh[k][1] >> 3, sh[k][2] >> 3);
    gfx_pal[C_MAN0 + idx[k]] = c; BG_PALETTE[C_MAN0 + idx[k]] = c; BG_PALETTE_SUB[C_MAN0 + idx[k]] = c;
  }
  { uint8_t sk[4][3]; extern int gfx_man_fur; gfx_man_fur = mx_outfit_skin(o, sk);                                   /* the Wolfman's skin is grey fur */
    for(int k = 0; k < 4; k++){ u16 c = RGB15(sk[k][0] >> 3, sk[k][1] >> 3, sk[k][2] >> 3); gfx_pal[C_MAN0 + 1 + k] = c; BG_PALETTE[C_MAN0 + 1 + k] = c; BG_PALETTE_SUB[C_MAN0 + 1 + k] = c; } }
}
/* AIR is an explicit gameplay toolbar selection. The title's remembered
   start tool must never silently summon the valve into normal shots. */
static int valve_allowed(void){ return valve_equipped && mx_valve_unlocked(&stats); }
static void apply_options(void){ mx_set_collapse(&world, opt.collapse); world.valve_ok = valve_allowed(); mx_set_fight(&world, !opt.nofight); }
static void load_level(int l){
  mx_load(&world, l); mx_meta_level_start(&stats, &world); valve_equipped=0; apply_options(); was_aim = 0;
#if MX_DS_FACADE_CACHE
  fac_dirty=1;
#endif
  mx_set_substeps(&world, world.substeps > MX_DS_MAX_SUBSTEPS ? MX_DS_MAX_SUBSTEPS : world.substeps);
  mx_set_wobble_budget(&world, wob_budget);
  cells0 = mx_cells_left(&world); disp_score = 0; end_t = 0; was_state = MX_PLAY; dragging = 0; pad_used = 0;
  memset(parts, 0, sizeof parts); memset(pops, 0, sizeof pops);
#if MX_DS_PARTICLE_HINT
  part_free_hint = 0;
#endif
  ev_ft = 0; fx = RI(88); fy = RI(38);
}

/* The title picture (art/title.h, made by tools/gen_title.py from the HTML title): the whole landscape with the man dropping butt-first on a landlord.  It is
   wider than the screen, so the screen pans across it: hold on the logo, glide right to the man, hold, glide back.  It is stored run-length coded and unpacked
   once.  Its colours live in sub-engine palette slots TITLE_DS_BASE.. (the text colours below them are untouched), so the picture is swapped in only while the
   title is up (see present()). */
static u8 title_img[TITLE_DS_W * TITLE_DS_H];
static int title_ready, title_pal_on, title_t;
static void title_unpack(void){
  int n = 0;
  for(int i = 0; i + 1 < TITLE_DS_RLE_LEN && n < TITLE_DS_W * TITLE_DS_H; i += 2)
    for(int r = TITLE_DS_RLE[i]; r > 0 && n < TITLE_DS_W * TITLE_DS_H; r--) title_img[n++] = (u8)(TITLE_DS_RLE[i + 1] + TITLE_DS_BASE);
  title_ready = 1;
}
/* 0..range for tick t: hold (90), glide right (160), hold (150), glide back (160), smoothstep both ways */
static int title_pan(int t, int range){
  t %= 560;
  int k = t < 90 ? 0 : t < 250 ? t - 90 : t < 400 ? 160 : t < 560 ? 160 - (t - 400) : 0;
  int u = (k * 256) / 160;
  return (range * ((u * u * (3 * 256 - 2 * u)) >> 16)) >> 8;
}

static void title_top(View *v){
  if(!title_ready) title_unpack();
  int pan = title_pan(title_t++, TITLE_DS_W - 256);
  for(int y = 0; y < 192; y++) memcpy(v->buf + y * 256, title_img + y * TITLE_DS_W + pan, 256);
  gfx_text(v->buf, 13 - pan, 33, "MANDEX", C_BLACK, 6);                  /* the logo sits in the picture, so it slides away with it */
  gfx_text_o(v->buf, 10 - pan, 30, "MANDEX", C_GOLD, C_RED2, 6);
  gfx_text_o(v->buf, 12, 174, "DRAG + LIFT: FLING   TAP: FLOP", C_WHITE, C_BLACK, 1);
}

#define ACH_PER_PAGE 5
#define ACH_PAGES ((MX_NUM_ACH + ACH_PER_PAGE - 1) / ACH_PER_PAGE)
static void ach_bottom(u8 *b){
  gfx_rect(b, 0, 0, 256, 192, C_BLACK);
  gfx_text_c(b, 128, 5, "ACHIEVEMENTS", C_GOLD, C_BLACK, 1);
  for(int k = 0; k < ACH_PER_PAGE; k++){
    int i = ach_page * ACH_PER_PAGE + k; if(i >= MX_NUM_ACH) break;
    int y = 17 + k * 29, un = mx_ach_unlocked(&stats, i), cur, tg;
    gfx_rect(b, 6, y, 244, 27, C_UIBG); gfx_rect(b, 6, y, 244, 1, C_UILINE);
    gfx_text(b, 12, y + 4, mx_ach_name(i), un ? C_GOLD : C_GRAY, 1);
    gfx_text(b, 12, y + 15, mx_ach_desc(i), un ? C_WHITE : C_GRAY, 1);
    if(mx_ach_progress(&stats, i, &cur, &tg)){
      if(cur > tg) cur = tg;
      const char *ts = gfx_int(tg); int tw = gfx_text_w(ts, 1);
      gfx_text(b, 244 - tw, y + 4, ts, un ? C_GOLD : C_GRAY, 1);
      gfx_text(b, 244 - tw - gfx_text_w("/", 1), y + 4, "/", C_UILINE, 1);
      const char *cs = gfx_int(cur);
      gfx_text(b, 244 - tw - gfx_text_w("/", 1) - gfx_text_w(cs, 1), y + 4, cs, un ? C_GOLD : C_WHITE, 1);
      gfx_rect(b, 190, y + 15, 54, 5, C_DARK);
      gfx_rect(b, 190, y + 15, (int)((long long)54 * cur / tg), 5, un ? C_GOLD : C_UILINE);
    }
  }
  gfx_rect(b, 0, 164, 256, 28, C_UIBG); gfx_rect(b, 0, 164, 256, 1, C_UILINE);
  gfx_text(b, 10, 174, "< BACK (B)", C_WHITE, 1);
  gfx_text(b, 100, 174, "<", C_WHITE, 2); gfx_text(b, 146, 174, ">", C_WHITE, 2);
  gfx_text(b, 116, 177, gfx_int(ach_page + 1), C_GOLD, 1);
  gfx_text(b, 224 - gfx_text_w("00/25", 1) / 2, 174, gfx_int(mx_ach_count(&stats)), C_GOLD, 1);
  gfx_text(b, 224 + 2, 174, "/25", C_UILINE, 1);
}

/* ---- outfits screen: a grid of swatches (boxers colour + towel colour); locked ones show a padlock and, when highlighted, how to unlock them (the hint text comes
   from mx_outfit(i)->hint only, so it can never drift from the unlock rules).  4 columns x 3 rows holds 12 outfits; a 13th needs paging (see the check below). */
#define OUT_COLS 4
#define OUT_ROWS ((MX_NUM_OUTFITS + OUT_COLS - 1) / OUT_COLS)
#define OUT_X0 8
#define OUT_Y0 18
#define OUT_CW 58
#define OUT_CH 43
#define OUT_DX (OUT_CW + 3)
#define OUT_DY (OUT_CH + 4)
#define C_SW0 224                      /* swatch colours: palette slots 224.. (two per outfit), main engine only; the man owns 160-198, the title is sub-engine */
typedef char out_grid_fits[(OUT_ROWS <= 4 && MX_NUM_OUTFITS <= 16) ? 1 : -1];   /* breaks the build if the grid overflows the screen or the palette block */
static void outfit_swatches(void){
  for(int i = 0; i < MX_NUM_OUTFITS; i++){
    const MxOutfit *o = mx_outfit(i);
    const u8 *c2[2] = { o->br, o->tow };
    for(int k = 0; k < 2; k++){
      u16 c = RGB15(c2[k][0] >> 3, c2[k][1] >> 3, c2[k][2] >> 3);
      gfx_pal[C_SW0 + i * 2 + k] = c; BG_PALETTE[C_SW0 + i * 2 + k] = c;
    }
  }
}
static int outfits_owned(void){ int n = 0; for(int i = 0; i < MX_NUM_OUTFITS; i++) n += mx_outfit_unlocked(&stats, i); return n; }
static int out_row0;                                       /* first visible row: three rows fit, a 13th outfit scrolls the grid */
static void out_follow(void){ int r = out_cur / OUT_COLS; if(r < out_row0) out_row0 = r; if(r > out_row0 + 2) out_row0 = r - 2; }
static int out_visible(int i){ int r = i / OUT_COLS; return r >= out_row0 && r < out_row0 + 3; }
static void out_cell_xy(int i, int *x, int *y){ *x = OUT_X0 + (i % OUT_COLS) * OUT_DX; *y = OUT_Y0 + (i / OUT_COLS - out_row0) * OUT_DY; }
static int out_cell_at(const touchPosition *t){           /* the outfit under the stylus, or -1 */
  for(int i = 0; i < MX_NUM_OUTFITS; i++){ int x, y; if(!out_visible(i)) continue; out_cell_xy(i, &x, &y); if(hit(t, x, y, OUT_CW, OUT_CH)) return i; }
  return -1;
}
static int out_move(int c, int dx, int dy){                /* d-pad: left/right walk the list, up/down walk a column; both wrap */
  int n = MX_NUM_OUTFITS;
  if(dx) return (c + dx + n) % n;
  c += dy * OUT_COLS;
  if(c < 0){ c += OUT_ROWS * OUT_COLS; if(c >= n) c -= OUT_COLS; }
  else if(c >= n) c %= OUT_COLS;
  return c;
}
static void out_wear(int i){                               /* wear it if it is unlocked, otherwise just the quiet click */
  if(!mx_outfit_unlocked(&stats, i)){ sfx_play(SFX_CLICK, 24); return; }
  outfit_sel = i; apply_outfit(i); sfx_play(SFX_CLICK, 64);
}
static void draw_lock(u8 *b, int cx, int y){               /* 12 x 15 padlock */
  gfx_rect(b, cx - 4, y, 8, 2, C_GRAY);
  gfx_rect(b, cx - 4, y, 2, 7, C_GRAY); gfx_rect(b, cx + 2, y, 2, 7, C_GRAY);
  gfx_rect(b, cx - 6, y + 6, 12, 9, C_GRAY);
  gfx_rect(b, cx - 1, y + 9, 2, 4, C_UIBG);
}
static void outfits_bottom(u8 *b){
  gfx_rect(b, 0, 0, 256, 192, C_BLACK);
  gfx_text_c(b, 128, 6, "OUTFITS", C_GOLD, C_BLACK, 1);
  out_follow();
  for(int i = 0; i < MX_NUM_OUTFITS; i++){
    int x, y; if(!out_visible(i)) continue; out_cell_xy(i, &x, &y);
    const MxOutfit *o = mx_outfit(i); int un = mx_outfit_unlocked(&stats, i);
    u8 bc = i == out_cur ? C_GOLD : C_UILINE;
    gfx_rect(b, x, y, OUT_CW, OUT_CH, C_UIBG);
    gfx_rect(b, x, y, OUT_CW, 1, bc); gfx_rect(b, x, y + OUT_CH - 1, OUT_CW, 1, bc);
    gfx_rect(b, x, y, 1, OUT_CH, bc); gfx_rect(b, x + OUT_CW - 1, y, 1, OUT_CH, bc);
    if(un){
      gfx_rect(b, x + 7, y + 6, 20, 16, (u8)(C_SW0 + i * 2));
      gfx_rect(b, x + 31, y + 6, 20, 16, (u8)(C_SW0 + i * 2 + 1));
    } else {
      gfx_rect(b, x + 7, y + 6, 44, 16, C_BLACK);
      draw_lock(b, x + OUT_CW / 2, y + 5);
    }
    gfx_text(b, x + (OUT_CW - gfx_text_w(o->name, 1)) / 2, y + 30, o->name, un ? C_WHITE : C_GRAY, 1);
    if(o->work) gfx_text(b, x + 3, y + 37, "WORK", C_GRAY, 1);                              /* intentionally plain */
    if(i == outfit_sel) gfx_text(b, o->work ? x + OUT_CW - 3 - gfx_text_w("WORN", 1) : x + (OUT_CW - gfx_text_w("WORN", 1)) / 2, y + 37, "WORN", C_GOLD, 1);
  }
  gfx_rect(b, 0, 160, 256, 32, C_UIBG); gfx_rect(b, 0, 160, 256, 1, C_UILINE);
  int un = mx_outfit_unlocked(&stats, out_cur);
  if(un) gfx_text(b, 8, 165, "UNLOCKED", C_GOLD, 1);
  else {
    gfx_text(b, 8, 165, "LOCKED:", C_GRAY, 1);
    gfx_text(b, 8 + gfx_text_w("LOCKED: ", 1), 165, mx_outfit(out_cur)->hint, C_WHITE, 1);
  }
  gfx_text(b, 8, 179, "< BACK", C_WHITE, 1); gfx_text(b, 39, 179, "B", C_UILINE, 1);
  gfx_text(b, 217, 179, "A", un ? C_UILINE : C_GRAY, 1); gfx_text(b, 233, 179, "WEAR", un ? C_UILINE : C_GRAY, 1);
}

/* ---- options screen (port 13): three rows on the title's bottom screen.  Tap a row (or d-pad up/down + A / left / right); B or BACK returns and saves. ---- */
enum { OPT_COLLAPSE, OPT_TOOL, OPT_FIGHT, OPT_MUSIC, OPT_ROWS };
#define OPT_RX 8
#define OPT_RW 240
static const int OPT_RY[OPT_ROWS] = { 10, 56, 102, 130 }, OPT_RH[OPT_ROWS] = { 44, 44, 26, 26 };
static void opt_change(int row, int dir){                 /* dir +1 / -1; the two-state rows ignore the sign */
  if(row == OPT_COLLAPSE){ opt.collapse = !opt.collapse; }
  else if(row == OPT_TOOL){ opt.tool = (u8)((opt.tool + dir + MX_NUM_TOOLS) % MX_NUM_TOOLS); }
  else if(row == OPT_FIGHT){ opt.nofight = !opt.nofight; }
  else { music_pref = !music_pref; music_set(music_pref && world.state == MX_PLAY); }
  apply_options(); sfx_play(SFX_CLICK, 64);
}
static int opt_row_at(const touchPosition *t){ for(int i = 0; i < OPT_ROWS; i++) if(hit(t, OPT_RX, OPT_RY[i], OPT_RW, OPT_RH[i])) return i; return -1; }
static void options_bottom(u8 *b){
  gfx_rect(b, 0, 0, 256, 192, C_BLACK);
  gfx_text_c(b, 128, 6, "OPTIONS", C_GOLD, C_BLACK, 1);
  for(int i = 0; i < OPT_ROWS; i++){
    int x = OPT_RX, y = OPT_RY[i], w = OPT_RW, h = OPT_RH[i];
    u8 bc = i == opt_cur ? C_GOLD : C_UILINE;
    gfx_rect(b, x, y, w, h, C_UIBG);
    gfx_rect(b, x, y, w, 1, bc); gfx_rect(b, x, y + h - 1, w, 1, bc);
    gfx_rect(b, x, y, 1, h, bc); gfx_rect(b, x + w - 1, y, 1, h, bc);
    u8 lc = i == opt_cur ? C_GOLD : C_WHITE;
    if(i == OPT_COLLAPSE){
      gfx_text(b, x + 6, y + 5, "COLLAPSIBLE BUILDINGS", lc, 1);
      const char *v = opt.collapse ? "ON" : "OFF";
      gfx_text(b, x + w - 8 - gfx_text_w(v, 2), y + 3, v, opt.collapse ? C_GOO : C_GRAY, 2);
      gfx_text(b, x + 6, y + 17, "ON: FALLEN RUBBLE KEEPS SLUMPING AND TOPPLING", C_WHITE, 1);
      gfx_text(b, x + 6, y + 25, "OFF: IT SETTLES AND LOCKS SOLID AT ONCE", C_GRAY, 1);
      gfx_text(b, x + 6, y + 34, "STAYS ON UNDER LOAD; WOBBLE/SUBSTEPS ADAPT FIRST", C_UILINE, 1);
    } else if(i == OPT_TOOL){
      gfx_text(b, x + 6, y + 5, "START TOOL", lc, 1);
      const char *v = mx_tool_name(opt.tool);
      gfx_text(b, x + w - 8 - gfx_text_w(v, 2), y + 3, v, C_GOLD, 2);
      gfx_text(b, x + 6, y + 17, "<", C_WHITE, 2); gfx_text(b, x + 6 + 12, y + 17, ">", C_WHITE, 2);
      static const char *const D1[MX_NUM_TOOLS] = { "NO GADGET; EQUIP ONE IN THE AIM TOOLBAR", "AIR VALVE: EQUIP IN THE AIM TOOLBAR", "BEANS: PRE-ARM WHEN YOU RELOAD", "TOWEL: PRE-ARM WHEN YOU RELOAD", "SPLAT: PRE-ARM WHEN YOU RELOAD", "TURBO: FASTER LAUNCH, 1 PER LEVEL", "QUAKE: HEAVY IMPACT, 1 PER LEVEL" };
      static const char *const D2[MX_NUM_TOOLS] = { "AIR STAYS HIDDEN UNTIL TAPPED", "AIR IS OFF AT LEVEL START", "ONE USE PER LEVEL; AIR STAYS OFF", "ONE USE PER LEVEL; AIR STAYS OFF", "ONE USE PER LEVEL; AIR STAYS OFF", "NO HEAP / EXTRA ASSETS", "STRONGER, SMALLER SHOCK THAN SPLAT" };
      gfx_text(b, x + 34, y + 21, D1[opt.tool], C_WHITE, 1);
      gfx_text(b, x + 34, y + 29, D2[opt.tool], C_GRAY, 1);
    } else if(i == OPT_FIGHT){
      gfx_text(b, x + 6, y + 5, "LANDLORDS FIGHT BACK", lc, 1);
      const char *v = opt.nofight ? "OFF" : "ON";
      gfx_text(b, x + w - 8 - gfx_text_w(v, 2), y + 3, v, opt.nofight ? C_GRAY : C_GOO, 2);
      gfx_text(b, x + 6, y + 16, "WALKERS, BAT GUYS, BRIEFCASE THROWERS", C_WHITE, 1);
    } else {
      gfx_text(b, x + 6, y + 10, "MUSIC", lc, 1); gfx_text(b, x + 6 + 24, y + 10, "(Y)", C_UILINE, 1);
      const char *v = music_pref ? "ON" : "OFF";
      gfx_text(b, x + w - 8 - gfx_text_w(v, 2), y + 8, v, music_pref ? C_GOO : C_GRAY, 2);
    }
  }
  gfx_rect(b, 0, 160, 256, 32, C_UIBG); gfx_rect(b, 0, 160, 256, 1, C_UILINE);
  gfx_text(b, 8, 170, "< BACK", C_WHITE, 1); gfx_text(b, 39, 170, "B", C_UILINE, 1);
  gfx_text(b, 8, 180, "TAP A ROW OR D-PAD + A", C_UILINE, 1);
}

static void title_bottom(View *v){
  if(opt_open){ options_bottom(v->buf); return; }
  if(ach_open){ ach_bottom(v->buf); return; }
  if(out_open){ outfits_bottom(v->buf); return; }
  draw_world(v, 0);
  panel(v->buf, 8, 6, 240, 38);
  gfx_text_c(v->buf, 128, 11, "SELECT LEVEL", C_UILINE, C_UIBG, 1);
  const char *nm = mx_level_name(sel_level);
  gfx_text_c(v->buf, 128, 21, nm, C_GOLD, C_BLACK, 2);
  gfx_text(v->buf, 14, 20, "<", C_WHITE, 2);
  gfx_text(v->buf, 232, 20, ">", C_WHITE, 2);
  if(!mx_level_unlocked(&stats, sel_level)){ gfx_text_c(v->buf, 128, 36, "LOCKED - BEAT THE LEVEL BEFORE", C_GRAY, C_UIBG, 1); }
  else gfx_text_c(v->buf, 128, 36, stats.best[sel_level] ? "BEST $" : "NOT PLAYED YET", C_UILINE, C_UIBG, 1);
  if(stats.best[sel_level] && mx_level_unlocked(&stats, sel_level)) gfx_text(v->buf, 128 + 12, 36, gfx_int((int)stats.best[sel_level]), C_GOLD, 1);
  gfx_rect(v->buf, 0, 164, 256, 28, C_UIBG); gfx_rect(v->buf, 0, 164, 256, 1, C_UILINE);
  if(frame & 32) gfx_text_c(v->buf, 128, 174, "TOUCH SCREEN OR PRESS A", C_WHITE, C_UIBG, 1);
  else           gfx_text_c(v->buf, 128, 174, "TO START", C_GOLD, C_UIBG, 1);
  gfx_text_c(v->buf, 128, 183, "OPTIONS (X)", C_UILINE, C_UIBG, 1);
  { int ox = 4; const char *own = gfx_int(outfits_owned());                  /* OUTFITS 4/11: the stylus button that opens the outfits screen */
    gfx_text(v->buf, ox, 183, "OUTFITS", C_UILINE, 1); ox += 7 * 4 + 4;
    gfx_text(v->buf, ox, 183, own, C_GOLD, 1);         ox += gfx_text_w(own, 1) + 1;
    gfx_text(v->buf, ox, 183, "/", C_UILINE, 1);       ox += 4;
    gfx_text(v->buf, ox, 183, gfx_int(MX_NUM_OUTFITS), C_UILINE, 1); }
  gfx_text(v->buf, 256 - 4 - gfx_text_w("ACH 00/25", 1), 183, "ACH", C_UILINE, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("00/25", 1), 183, gfx_int(mx_ach_count(&stats)), C_GOLD, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("/25", 1), 183, "/25", C_UILINE, 1);
  gfx_text_c(v->buf, 128, 46, mx_outfit(outfit_sel)->name, C_PINK, C_BLACK, 1);
  if(cheat_t > 0) gfx_text_c(v->buf, 128, 56, stats.unlock_all ? "CHEAT: ALL LEVELS OPEN" : "CHEAT OFF", C_GOLD, C_BLACK, 1);
}

/* ---------------------------------------------------------------------- main */
static void present(int bot_dirty){
  DC_FlushRange(buf_top, sizeof buf_top);
  /* Keep the idle lower-screen saving all the way through presentation:
     do not walk/flush 48 KiB of unchanged cache lines on skipped frames.
     Its DMA is already gated by bot_dirty below. */
  if(bot_dirty) DC_FlushRange(buf_bot, sizeof buf_bot);
  cpuStartTiming(0);
  swiWaitForVBlank();
  ds_vblank_ticks=(int)cpuEndTiming();
  if(title_pal_on != (app == ST_TITLE)){                         /* the top screen's palette: the title picture on the title, the game's otherwise */
    title_pal_on = app == ST_TITLE;
    if(title_pal_on) title_t = 0;                                /* the pan starts from the logo every time the title comes up */
    for(int k = 0; k < TITLE_DS_NCOL; k++)
      BG_PALETTE_SUB[TITLE_DS_BASE + k] = title_pal_on ? RGB15(TITLE_DS_PAL[k * 3] >> 3, TITLE_DS_PAL[k * 3 + 1] >> 3, TITLE_DS_PAL[k * 3 + 2] >> 3) : gfx_pal[TITLE_DS_BASE + k];
  }
  cpuStartTiming(0);
  if(bot_dirty) dmaCopy(buf_bot, BG_GFX, sizeof buf_bot);
  dmaCopy(buf_top, BG_GFX_SUB, sizeof buf_top);
  ds_dma_ticks=(int)cpuEndTiming();
}

static real clampr(real v, real lo, real hi){ return v < lo ? lo : v > hi ? hi : v; }

#ifdef MX_STAGE
/* diagnostic: paint both screens one solid colour and stop, so the colour says how far start-up got */
static void stage_halt(int n, int rgb){
  BG_PALETTE[0] = rgb; BG_PALETTE_SUB[0] = rgb; BG_PALETTE[255] = rgb; BG_PALETTE_SUB[255] = rgb;   /* backdrop colour, in case the bitmap does not show */
  for(int i = 0; i < 256 * 192 / 2; i++){ BG_GFX[i] = 0xFFFF; BG_GFX_SUB[i] = 0xFFFF; }            /* VRAM only takes 16-bit writes */
  if(n == MX_STAGE) for(;;) swiWaitForVBlank();
}
#define STAGE(n, rgb) stage_halt(n, rgb)
#else
#define STAGE(n, rgb) ((void)0)
#endif
int main(void){
  lcdMainOnBottom();                                  /* main engine = touch screen */
  videoSetMode(MODE_5_2D);   videoSetModeSub(MODE_5_2D);
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
  bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
  STAGE(1, RGB15(31,0,0));                             /* red */
  gfx_init();
  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? mx_material_hp(k) : 1;
  for(int i = 0; i < 256; i++){ BG_PALETTE[i] = gfx_pal[i]; BG_PALETTE_SUB[i] = gfx_pal[i]; }
  STAGE(2, RGB15(0,31,0));                             /* green */
  sfx_init();
  music_set(music_pref);
  STAGE(3, RGB15(0,0,31));                             /* blue */

  mx_meta_init(&stats); mx_options_default(&opt); save_load(); outfit_scan(1); apply_outfit(outfit_sel); outfit_swatches();   /* load before the first load_level(), as mx_meta_unpack asks */
  load_level(sel_level);
#ifdef MX_FPS_DEMO_LEVEL
  /* Test-only automatic scenario; normal release builds never define it. */
  if(MX_FPS_DEMO_LEVEL>=0&&MX_FPS_DEMO_LEVEL<MX_NUM_LEVELS){
    sel_level=MX_FPS_DEMO_LEVEL;load_level(sel_level);
    app=ST_PLAY;intro_seen=wolf_seen=1;end_seen=6;debug_on=1;
  }
#endif
  STAGE(4, RGB15(31,31,0));                            /* yellow */
  int slow_frames = 0, fast_frames = 0, phys_ticks = 0, draw_ticks = 0, lite = 0;   /* draw_ticks: last frame's drawing time; lite: the touch screen redraws every other frame while the DS is overloaded */

  for(;;){
    { static int last_app = ST_TITLE; if(app == ST_TITLE && last_app != ST_TITLE) save_sync(); last_app = app; }   /* back on the title: flush anything earned in a lost run */
    frame++;
    DS_PERF_MARK(0);
    scanKeys();
    int down = keysDown(), held = keysHeld(), up = keysUp();
    touchPosition t = { 0, 0 };
    if(held & KEY_TOUCH) touchRead(&t);
#ifdef MX_BARE_NDS_H
    /* Advance the sound clock BEFORE the expensive physics/render work.
       ARM7 consumes a bounded command stream on VBlank independently. */
    { static u16 last_vblank; static int primed;
      u16 now = MX_IPC->alive;
      if(!primed){last_vblank=now;primed=1;}
      unsigned elapsed=(u16)(now-last_vblank);
      last_vblank=now;
      sfx_tick_steps(elapsed ? (int)elapsed : 1);
    }
#else
    sfx_tick();
#endif

    if(down & KEY_Y){ music_pref = !music_pref; music_set(music_pref && app != ST_PAUSE && world.state == MX_PLAY); }
    if(app == ST_TITLE && !ach_open && !out_open && !opt_open && (held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT) && !combo_held){
      combo_held = 1; stats.unlock_all = !stats.unlock_all; cheat_t = 120; sfx_play(SFX_TARGET, 64);       /* never saved (port 5 must leave unlock_all out) */
    }
    if(app == ST_TITLE){ if((up & KEY_L) && !combo_held) debug_on = !debug_on; }      /* on the title L and R act on release, so a held L+R+SELECT never trips the profiler or the outfit cycle first */
    else if(down & KEY_L) debug_on = !debug_on;

    /* ---------------- input ---------------- */
    if(app == ST_TITLE && ach_open){
      if((down & KEY_B) || ((down & KEY_TOUCH) && hit(&t, 0, 168, 70, 24))){ ach_open = 0; sfx_play(SFX_CLICK, 64); }
      if((down & KEY_RIGHT) || ((down & KEY_TOUCH) && hit(&t, 140, 168, 30, 24))){ ach_page = (ach_page + 1) % ACH_PAGES; sfx_play(SFX_CLICK, 64); }
      if((down & KEY_LEFT) || ((down & KEY_TOUCH) && hit(&t, 90, 168, 30, 24))){ ach_page = (ach_page + ACH_PAGES - 1) % ACH_PAGES; sfx_play(SFX_CLICK, 64); }
    } else if(app == ST_TITLE && opt_open){
      int r = (down & KEY_TOUCH) ? opt_row_at(&t) : -1;
      if((down & KEY_B) || ((down & KEY_TOUCH) && hit(&t, 0, 160, 80, 32))){ opt_open = 0; save_sync(); sfx_play(SFX_CLICK, 64); }
      else if(r >= 0){ opt_cur = r; opt_change(r, (r == OPT_TOOL && t.px < OPT_RX + 16) ? -1 : 1); }   /* tap a row; the left end of the tool row steps back */
      else {
        if(down & KEY_DOWN){ opt_cur = (opt_cur + 1) % OPT_ROWS; sfx_play(SFX_CLICK, 40); }
        if(down & KEY_UP){ opt_cur = (opt_cur + OPT_ROWS - 1) % OPT_ROWS; sfx_play(SFX_CLICK, 40); }
        if(down & (KEY_A | KEY_RIGHT)) opt_change(opt_cur, 1);
        else if(down & KEY_LEFT) opt_change(opt_cur, -1);
      }
    } else if(app == ST_TITLE && out_open){
      int c = (down & KEY_TOUCH) ? out_cell_at(&t) : -1;
      if((down & KEY_B) || ((down & KEY_TOUCH) && hit(&t, 0, 160, 80, 32))){ out_open = 0; sfx_play(SFX_CLICK, 64); }
      else if(c >= 0){ out_cur = c; out_wear(c); }                                      /* tap a swatch: highlight it, and wear it if it is yours */
      else if((down & KEY_A) || ((down & KEY_TOUCH) && hit(&t, 176, 160, 80, 32))) out_wear(out_cur);
      else {
        int dx = ((down & KEY_RIGHT) ? 1 : 0) - ((down & KEY_LEFT) ? 1 : 0), dy = ((down & KEY_DOWN) ? 1 : 0) - ((down & KEY_UP) ? 1 : 0);
        if(dx || dy){ out_cur = out_move(out_cur, dx, dy); sfx_play(SFX_CLICK, 40); }
      }
    } else if(app == ST_TITLE){
      if(((down & KEY_TOUCH) && hit(&t, 78, 176, 100, 16)) || (down & KEY_X)){ opt_open = 1; opt_cur = 0; sfx_play(SFX_CLICK, 64); down &= ~(KEY_TOUCH | KEY_A | KEY_START); }
      if((down & KEY_TOUCH) && hit(&t, 190, 176, 66, 16)){ ach_open = 1; ach_page = 0; sfx_play(SFX_CLICK, 64); down &= ~KEY_TOUCH; }
      if((down & KEY_TOUCH) && hit(&t, 0, 176, 64, 16)){ out_open = 1; out_cur = outfit_sel; sfx_play(SFX_CLICK, 64); down &= ~KEY_TOUCH; }
      int ch = 0;
      if(down & KEY_LEFT) ch = -1;
      if(down & KEY_RIGHT) ch = 1;
      if((down & KEY_TOUCH) && hit(&t, 0, 0, 48, 48)) ch = -1;
      if((down & KEY_TOUCH) && hit(&t, 208, 0, 48, 48)) ch = 1;
      if((up & KEY_R) && !combo_held){                           /* next unlocked outfit; locked ones are skipped */
        for(int k = 1; k <= MX_NUM_OUTFITS; k++){ int n = (outfit_sel + k) % MX_NUM_OUTFITS; if(mx_outfit_unlocked(&stats, n)){ outfit_sel = n; break; } }
        apply_outfit(outfit_sel); sfx_play(SFX_CLICK, 64);
      }
      if(ch){ sel_level = (sel_level + ch + MX_NUM_LEVELS) % MX_NUM_LEVELS; load_level(sel_level); sfx_play(SFX_CLICK, 64); }
      else if(((down & (KEY_A | KEY_START)) || ((down & KEY_TOUCH) && t.py > 48)) && !mx_level_unlocked(&stats, sel_level)) sfx_play(SFX_CLICK, 24);
      else if((down & (KEY_A | KEY_START)) || ((down & KEY_TOUCH) && t.py > 48)){
        save_sync();                                               /* an outfit chosen on the title is kept */
        if(sel_level == 0 && !intro_seen){ intro_seen = 1; app = ST_INTRO; intro_level = 0; intro_start(); apply_outfit(0); sfx_play(SFX_CLICK, 64); }   /* the origin story is told in the classic outfit */
        else if(sel_level == MX_CH3_START && !wolf_seen){ wolf_seen = 1; intro_level = sel_level; app = ST_INTRO; intro_start_wolf(); apply_outfit(0); sfx_play(SFX_CLICK, 64); }
        else { app = ST_PLAY; load_level(sel_level); sfx_play(SFX_CLICK, 64); music_set(music_pref); }
      }
    } else if(app == ST_INTRO){
      if(intro_tick(down, t.px, t.py)){
        if(intro_mode() == 3 && intro_level == MX_CH3_START && !wolf_seen){ wolf_seen = 1; intro_start_wolf(); sfx_play(SFX_CLICK, 64); }   /* the closer of chapter 2 hands over to the wolfman cutscene */
        else { app = ST_PLAY; apply_outfit(outfit_sel); load_level(intro_level); music_set(music_pref); sfx_play(SFX_CLICK, 64); } }   /* finished or skipped: straight into level 1 */
    } else if(app == ST_PAUSE){
      if(down & KEY_START) app = ST_PLAY;
      if(down & KEY_SELECT){ load_level(world.level); app = ST_PLAY; music_set(music_pref); }
      if(down & KEY_B){ app = ST_TITLE; sel_level = world.level; load_level(sel_level); }
    } else if(world.state != MX_PLAY){
      if(down & KEY_B){ app = ST_TITLE; sel_level = world.level; load_level(sel_level); }
      else if((down & (KEY_START | KEY_A)) || ((down & KEY_TOUCH) && end_t > 30)){
        int next = world.state == MX_WIN ? world.level + 1 : world.level;
        int ce = (world.state == MX_WIN && next == MX_CH2_START) ? 1 : (world.state == MX_WIN && next == MX_CH3_START) ? 2 : 0;   /* the closing cutscene of the chapter just finished */
        if(ce && !(end_seen & (1 << ce))){ end_seen |= 1 << ce; intro_level = next; app = ST_INTRO; intro_start_end(ce); apply_outfit(0); }
        else if(next >= MX_NUM_LEVELS){ app = ST_TITLE; sel_level = 0; load_level(0); }
        else { load_level(next); music_set(music_pref); }
        sfx_play(SFX_CLICK, 64);
      }
    } else {
      if(down & KEY_START){ app = ST_PAUSE; music_set(0); }
      if(down & KEY_SELECT){ load_level(world.level); sfx_play(SFX_CLICK, 64); }
      MxMan *m = &world.man;
      if(m->mode == MAN_AIM){
        View av = { buf_bot, 4, 0, 56 };
        int toolbar_hit=(down&KEY_TOUCH)?aim_tool_at(&t):-1;
        if(toolbar_hit>=0){
          if(toolbar_hit==0){
            if(mx_valve_unlocked(&stats)){
              valve_equipped=!valve_equipped;
              if(valve_equipped && world.pw_sel)mx_select_power(&world,world.pw_sel);
              if(!valve_equipped && m->seated)mx_set_pull(&world,0,0);
              apply_options();sfx_play(SFX_CLICK,64);
            }else sfx_play(SFX_CLICK,24);
          }else{
            int pw=AIM_TOOLS[toolbar_hit].pw;
            if(world.pw_left[pw]>0){
              if(valve_equipped){
                valve_equipped=0;
                if(m->seated)mx_set_pull(&world,0,0);
                apply_options();
              }
              mx_select_power(&world,pw);sfx_play(SFX_CLICK,64);
            }else sfx_play(SFX_CLICK,24);
          }
        }
        if(down&KEY_X){
          int nx=world.pw_sel;
          for(int tries=0;tries<PW_COUNT;tries++){
            nx=(nx+1)%PW_COUNT;
            if(nx==PW_NONE||world.pw_left[nx]>0)break;
          }
          if(nx==PW_NONE && world.pw_sel)mx_select_power(&world,world.pw_sel);
          else if(nx){
            if(valve_equipped){
              valve_equipped=0;
              if(m->seated)mx_set_pull(&world,0,0);
              apply_options();
            }
            mx_select_power(&world,nx);
          }
          sfx_play(SFX_CLICK,64);
        }
        world.valve_ok=valve_allowed();  /* off by default; only an equipped AIR receives interactions */
        if((down&KEY_R)&&mx_valve_seat(&world))sfx_play(SFX_CLICK,64);
        if((down&KEY_TOUCH)&&toolbar_hit<0){
          int vdx=t.px-vx(&av,MX_VALVE_X),vdy=t.py-vy(&av,MX_VALVE_Y);
          int dx=t.px-vx(&av,m->x),dy=t.py-vy(&av,m->y);
          if(world.valve_ok&&!m->seated&&vdx*vdx+vdy*vdy<20*20&&mx_valve_seat(&world)){
            dragging=0;sfx_play(SFX_CLICK,64);
          }else dragging=dx*dx+dy*dy<34*34;
        }
        if((held & KEY_TOUCH) && dragging){
          real cx = RI(t.px + av.cx) >> 2, cy = RI(t.py + av.cy) >> 2;    /* stylus px -> cells (4 px per cell) */
          int was_seated = m->seated;
          mx_drag(&world, cx - MX_ANCHOR_X, cy - MX_ANCHOR_Y);            /* the sling stretches on toward the valve; close to it he drops on; on it he stays until the stylus leaves */
          if(!was_seated && m->seated) sfx_play(SFX_CLICK, 64);
          else if(!m->seated && frame % 6 == 0) sfx_play(SFX_STRETCH, 40);
        }
        if((up & KEY_TOUCH) && dragging){ dragging = 0; mx_release(&world); }
        if(!dragging){
          int k = held & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN);
          if(k){
            if(!pad_used){ pad_dx = -RI(6); pad_dy = RI(3); pad_used = 1; }
            if(k & KEY_LEFT)  pad_dx -= R(0.3);
            if(k & KEY_RIGHT) pad_dx += R(0.3);
            if(k & KEY_UP)    pad_dy -= R(0.3);
            if(k & KEY_DOWN)  pad_dy += R(0.3);
            pad_dx = clampr(pad_dx, -RI(12), RI(2)); pad_dy = clampr(pad_dy, -RI(8), RI(12));
            mx_set_pull(&world, pad_dx, pad_dy);
            if(frame % 6 == 0) sfx_play(SFX_STRETCH, 40);
          }
          if((down & KEY_A) && pad_used){ pad_used = 0; mx_release(&world); }
        }
      } else if(m->mode == MAN_WALK){                                      /* walk levels: steer him on foot */
        dragging = 0; pad_used = 0;
        int ax = 0, jump = 0, punch = 0, stomp = 0;
        if(held & KEY_LEFT) ax--;
        if(held & KEY_RIGHT) ax++;
        if(held & (KEY_A | KEY_UP)) jump = 1;
        if(held & (KEY_B | KEY_Y)) punch = 1;
        if(held & (KEY_DOWN | KEY_X)) stomp = 1;
        if(held & KEY_TOUCH){ int bn = walk_btn_at(&t); if(bn == 0) ax--; else if(bn == 1) ax++; else if(bn == 2) jump = 1; else if(bn == 3) punch = 1; else if(bn == 4) stomp = 1; }
        mx_walk_input(&world, ax, jump, punch, stomp);
        if((down & KEY_L) || ((down & KEY_TOUCH) && walk_btn_at(&t) == 5)){ mx_walk_quit(&world); sfx_play(SFX_CLICK, 64); }
      } else {
        dragging = 0; pad_used = 0;
        if(down & (KEY_A | KEY_B)) mx_dive(&world);
        if(down & KEY_TOUCH){
          if(hit(&t, 170, 170, 46, 16)){ load_level(world.level); sfx_play(SFX_CLICK, 64); }
          else if(hit(&t, 220, 170, 32, 16)){ app = ST_TITLE; sel_level = world.level; load_level(sel_level); }
          else mx_dive(&world);
        }
      }
    }

    /* ---------------- simulation ---------------- */
    int sim_steps_done=0;
#ifdef MX_FPS_DEMO_LEVEL
    if(frame==180&&app==ST_PLAY&&world.man.mode==MAN_AIM){
      world.banner=0;mx_set_pull(&world,R(-9),R(5));mx_release(&world);
    }
#endif
    if(app == ST_PLAY){
      /* idle gags: only while he waits in the sling and nobody is touching anything */
      if(world.man.mode == MAN_AIM && !dragging && !pad_used && world.banner <= 0 && world.state == MX_PLAY && !(down | held)){
        mi_update(&mi);
        if(mi.fx) idle_fx(mi.fx, world.man.x, world.man.y, 1, 1, 1);
      } else mi_reset(&mi);
      if(dragging && (frame % 7) == 0) spawn(PT_SWEAT, world.man.x + RI(rnd(5) - 2), world.man.y - RI(4), rnd(9) - 4, -rnd(8), 0, 28);       /* straining */
      if(world.man.mode == MAN_FLY && (frame % 10) == 0)                                                                                          /* stink trail */
        spawn(PT_STINK, world.man.x - (world.man.vx > 0 ? RI(2) : -RI(2)), world.man.y + RI(1), rnd(5) - 2, rnd(3) - 1, 0, 40 + rnd(20));
      { int aim = world.man.mode == MAN_AIM && world.state == MX_PLAY;       /* the start tool is armed once each time he is back in the sling; a later tap on a power-up button still wins */
        if(aim && !was_aim){ int pw = mx_tool_power(opt.tool); if(pw && world.pw_sel != pw) mx_select_power(&world, pw); }
        was_aim = aim; }
      /* Target one simulation tick per ARM7 VBlank rather than per completed
         picture. Catch up only 2-3 small fixed steps when the solver has room.
         Events must be consumed after EACH step, or explosions and rewards
         from intermediate updates would be overwritten by the next step. */
      unsigned requested_steps=1;
#ifdef MX_BARE_NDS_H
      {
        static u16 last_sim_vblank;
        static int sim_clock_ready;
        u16 now=MX_IPC->alive;
        if(sim_clock_ready){
          unsigned elapsed=(u16)(now-last_sim_vblank);
          requested_steps=mx_fps_catchup_steps(elapsed,(unsigned)phys_ticks);
        }else sim_clock_ready=1;
        last_sim_vblank=now;
      }
#endif
      DS_PERF_MARK(1);
      cpuStartTiming(0);
      for(unsigned step=0;step<requested_steps;step++){
        /* Keep the original one-step post-win/lose effects every frame;
           stop only OPTIONAL catch-ups after an outcome has been reached. */
        if(step>0 && world.state!=MX_PLAY)break;
        mx_update(&world,DT);
        sim_steps_done++;
        on_events();
        mx_meta_events(&stats,&world);
      }
      phys_ticks=(int)cpuEndTiming();
      DS_PERF_MARK(2);
      /* adaptive quality: fewer substeps when the solver eats too much of the frame, more when there is headroom */
      /* Never auto-disable opted-in building collapse. Cut wobble workload first,
         then chunk substeps; turn off wobble only as the final emergency step. */
      /* a hit wakes the wobble lattice, which is what costs time: the core caps the cells it walks (wob_budget) and this loop moves that cap with the measured cost,
         at once (one slow frame is enough) and back up slowly.  Only when the cap is already at its floor are the other quality switches touched. */
      { int load = phys_ticks + draw_ticks; lite = load > 460000 ? 1 : load < 330000 ? 0 : lite; }      /* a 60 Hz frame is ~560 000 ticks: what counts is physics + drawing, not physics alone */
      if(phys_ticks > 330000){ fast_frames = 0;
        if(wob_budget > DS_WOB_MIN){ wob_budget = wob_budget * 2 / 3; if(wob_budget < DS_WOB_MIN) wob_budget = DS_WOB_MIN; mx_set_wobble_budget(&world, wob_budget); slow_frames = 0; }
        else if(++slow_frames >= 2){ slow_frames = 0;
          if(world.substeps > 2) mx_set_substeps(&world, world.substeps - 1);
          else if(world.wob_enabled) mx_set_wobble(&world, 0); } }
      else if(phys_ticks < 180000){ slow_frames = 0;
        if(wob_budget < DS_WOB_MAX && (frame & 15) == 0){ wob_budget += 24; if(wob_budget > DS_WOB_MAX) wob_budget = DS_WOB_MAX; mx_set_wobble_budget(&world, wob_budget); }
        if(++fast_frames >= 120){ fast_frames = 0;
          if(!world.wob_enabled) mx_set_wobble(&world, 1);
          else if(world.substeps < MX_DS_MAX_SUBSTEPS) mx_set_substeps(&world, world.substeps + 1); } }
      else { slow_frames = 0; fast_frames = 0; }
      for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ){ toast_push(a); } outfit_scan(0);
      update_parts();
      if(world.state != MX_PLAY){
        if(was_state == MX_PLAY){ end_t = 0; if(world.state == MX_WIN){ mx_meta_win(&stats, &world); for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ){ toast_push(a); } outfit_scan(0); } }
        if(was_state == MX_PLAY) save_sync();                      /* win or lose: the one moment a short stall is harmless */
        end_t++;
      }
      was_state = world.state;
      int d = world.score - disp_score;
      if(d) disp_score += d > 0 ? (d + 7) / 8 : -((-d + 7) / 8);
    } else if(app == ST_TITLE){
      update_parts();
    }


    if(!(held & (KEY_L | KEY_R | KEY_SELECT))) combo_held = 0;
    if(cheat_t > 0) cheat_t--;

    /* ---------------- camera ---------------- */
    real tx = RI(88), ty = RI(38), rate = R(0.10);
    const MxMan *m = &world.man;
    if(app == ST_PLAY){
      if(m->mode == MAN_FLY){ tx = m->x + rmul(m->vx, R(0.22)); ty = m->y; rate = R(0.3); }
      else if(m->mode == MAN_STUCK || m->mode == MAN_WALK){ tx = m->x; ty = m->y; rate = R(0.2); }
      else if(ev_ft > 0){ tx = ev_fx; ty = ev_fy; rate = R(0.12); ev_ft--; }
    }
    fx += rmul(tx - fx, rate); fy += rmul(ty - fy, R(0.12));
    int sh = world.shake >> 11; if(sh > 5) sh = 5;
    int sx = sh ? rnd(2 * sh + 1) - sh : 0, sy = sh ? rnd(2 * sh + 1) - sh : 0;

    View top = { buf_top, 4, 0, 0 };
    top.cx = gfx_clamp((fx >> 10) - 128, 0, MX_COLS * 4 - 256);
    top.cy = gfx_clamp((fy >> 10) - 96, 0, MX_ROWS * 4 + 8 - 192);
    if(app == ST_TITLE){ top.cx = (frame >> 1) % 200; top.cy = 48; }
    top.cx += sx; top.cy += sy;

    /* ---------------- render ---------------- */
    DS_PERF_MARK(3);
    cpuStartTiming(0);
    int bot_dirty=1;
    View bot = { buf_bot, 2, -8, -40 };
    if(app == ST_TITLE){
      title_top(&top);
      title_bottom(&bot);
    } else if(app == ST_INTRO){
      intro_draw(buf_top, buf_bot);
    } else {
      draw_world(&top, 1);
      draw_popups(&top, 2);
      top_hud(buf_top);
      banner(buf_top);
      end_panel(buf_top);
      ds_draw_top_ticks=(int)cpuEndTiming();
      DS_PERF_MARK(4);
      cpuStartTiming(0);
      /* Previously the expensive second renderer ran at 60 Hz by default.
         Keep full-rate pointer/drag feedback, but share the CPU frame budget
         with physics when the lower screen is only an overview. */
      int quiet_bottom=app==ST_PLAY && !dragging && !pad_used &&
                        !((held|down|up)&KEY_TOUCH) &&
                        !(down&(KEY_X|KEY_A|KEY_START|KEY_SELECT|KEY_R));
      int bottom_half_rate=MX_DS_BOTTOM30 && quiet_bottom && (frame&1);
      int bottom_overload_rate=MX_DS_OVERLOAD_BOTTOM15 && lite &&
                               quiet_bottom && (frame&3);
      bot_dirty=!(bottom_half_rate || bottom_overload_rate ||
                  (lite && (frame&1) && quiet_bottom));
      if(bot_dirty){       /* keep stylus aiming at 60Hz; throttle only noninteractive bottom redraws */
      if(world.man.mode == MAN_AIM && world.state == MX_PLAY){
        bot = (View){ buf_bot, 4, 0, 56 };
        bot.cx += sx; bot.cy += sy;
        bottom_aim(&bot);
      } else {
        bot.cx += sx; bot.cy += sy;
        bottom_overview(&bot);
        if(world.state != MX_PLAY && end_t > 30 && (frame & 32))
          gfx_text_c(buf_bot, 128, 6, world.state == MX_WIN ? "TOUCH / START = NEXT LEVEL" : "TOUCH / START = TRY AGAIN", C_WHITE, C_BLACK, 1);
      }
      }
      if(app == ST_PAUSE){
        panel(buf_top, 56, 64, 144, 60);
        gfx_text_c(buf_top, 128, 72, "PAUSED", C_GOLD, C_BLACK, 2);
        gfx_text_c(buf_top, 128, 92, "START  RESUME", C_WHITE, C_UIBG, 1);
        gfx_text_c(buf_top, 128, 101, "SELECT RESTART", C_WHITE, C_UIBG, 1);
        gfx_text_c(buf_top, 128, 110, "B      MENU", C_WHITE, C_UIBG, 1);
      }
    }
    if(debug_on){
      gfx_rect(buf_top, 0, 180, 256, 12, C_UIBG);
      gfx_text(buf_top, 2, 183, "PHYS", C_WHITE, 1);
      gfx_text(buf_top, 22, 183, gfx_int(phys_ticks * 10 / 33514), C_GOLD, 1);
      gfx_text(buf_top, 40, 183, "/10MS", C_WHITE, 1);
      gfx_text(buf_top, 66, 183, "SUB", C_WHITE, 1);
      gfx_text(buf_top, 80, 183, gfx_int(world.substeps), C_GOLD, 1);
      gfx_text(buf_top, 90, 183, "CONTACTS", C_WHITE, 1);
      gfx_text(buf_top, 124, 183, gfx_int((int)world.stat_cells), C_GOLD, 1);
      gfx_text(buf_top, 150, 183, "COL", C_WHITE, 1);
      gfx_text(buf_top, 162, 183, world.collapse ? "ON" : "OFF", C_GOLD, 1);
      gfx_rect(buf_top,0,166,256,12,C_UIBG);
#ifdef MX_BARE_NDS_H
      gfx_text(buf_top,2,169,"FPS",C_WHITE,1);
      gfx_text(buf_top,20,169,gfx_int((int)fps_meter.render_x10/10),C_GOLD,1);
      gfx_text(buf_top,40,169,"SIM",C_WHITE,1);
      gfx_text(buf_top,60,169,gfx_int((int)fps_meter.sim_x10/10),C_GOLD,1);
      gfx_text(buf_top,78,169,"MISS",C_WHITE,1);
      gfx_text(buf_top,103,169,gfx_int((int)fps_meter.last_dropped),C_GOLD,1);
#else
      gfx_text(buf_top,2,169,"FPS/SIM: HARDWARE ONLY",C_WHITE,1);
#endif
      gfx_text(buf_top,134,169,"TOP",C_WHITE,1);
      gfx_text(buf_top,152,169,gfx_int(ds_draw_top_ticks/33514),C_GOLD,1);
      gfx_text(buf_top,170,169,"BOT",C_WHITE,1);
      gfx_text(buf_top,188,169,gfx_int(ds_draw_bot_ticks/33514),C_GOLD,1);
      gfx_text(buf_top,206,169,"DMA",C_WHITE,1);
      gfx_text(buf_top,224,169,gfx_int(ds_dma_ticks/33514),C_GOLD,1);
    }
    ds_draw_bot_ticks=(int)cpuEndTiming();
    DS_PERF_MARK(5);
    if(app!=ST_PLAY && app!=ST_PAUSE){ds_draw_top_ticks=ds_draw_bot_ticks;ds_draw_bot_ticks=0;}
    draw_ticks=ds_draw_top_ticks+ds_draw_bot_ticks;
    present(bot_dirty);
    DS_PERF_MARK(6);
    ds_fps_complete(sim_steps_done);
  }
  return 0;
}
