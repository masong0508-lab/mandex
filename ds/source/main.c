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
#define MS_ENUM_ONLY
#include "manspr.h"
#include "manidle.h"
#define MX_TITLE_DS
#include "title.h"

#ifndef MX_DS_MAX_SUBSTEPS
#define MX_DS_MAX_SUBSTEPS 3
#endif
#define DT R(1.0 / 60.0)

static MxWorld world;                         /* ~320 KB, lives in main RAM */
static u8 buf_top[256 * 192], buf_bot[256 * 192];

enum { ST_TITLE, ST_PLAY, ST_PAUSE };
static int app = ST_TITLE, frame, sel_level, music_pref = 1, debug_on;
static int disp_score, cells0 = 1, end_t, was_state, best[MX_NUM_LEVELS];
static MxStats stats;                 /* achievements / outfits for this session (the DS has no save) */
static int outfit_sel, toast_t = -1, toast_id, toast_q[4], toast_nq;
static void toast_push(int id){ if(toast_nq < 4) toast_q[toast_nq++] = id; }
static void toast_tick(void){
  if(toast_t >= 0){ if(++toast_t > 150) toast_t = -1; }
  if(toast_t < 0 && toast_nq){ toast_id = toast_q[0]; for(int i = 1; i < toast_nq; i++) toast_q[i-1] = toast_q[i]; toast_nq--; toast_t = 0; sfx_play(SFX_TARGET, 64); }
}

static int pad_dx, pad_dy, pad_used, dragging;
static int MAXHP[MX_NMAT];                    /* filled from the core's material table */

static u32 rng = 0x2545F491u;
static int rnd(int n){ rng = rng * 1664525u + 1013904223u; return (int)((rng >> 16) % (u32)n); }

/* ------------------------------------------------------------ particles/popups */
enum { PT_DEBRIS, PT_SMOKE, PT_SPARK, PT_GOO, PT_HAT, PT_DUST, PT_STAR, PT_STINK, PT_FLY, PT_ZZZ, PT_SWEAT };
typedef struct { int x, y, vx, vy; short life, max; u8 type, col; } Part;      /* positions in 1/64 cell */
#define NPART 128
static Part parts[NPART];

static void spawn(int type, real wx, real wy, int vx, int vy, int col, int life){
  for(int i = 0; i < NPART; i++) if(parts[i].life <= 0){
    Part *p = &parts[i];
    p->x = wx >> 6; p->y = wy >> 6; p->vx = vx; p->vy = vy; p->life = p->max = (short)life; p->type = (u8)type; p->col = (u8)col;
    return;
  }
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
    case PT_DUST:  p->vx = p->vx * 7 / 8; p->vy = p->vy * 7 / 8; break;
    }
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
    case EV_THUD:   if(e->b > R(0.3)) sfx_play(SFX_THUD, pan); break;
    case EV_BOOM:
      for(int k = 0; k < 9; k++) spawn(PT_SMOKE, e->x, e->y, rnd(25) - 12, rnd(20) - 14, 0, 26 + rnd(14));
      for(int k = 0; k < 12; k++) spawn(PT_SPARK, e->x, e->y, rnd(41) - 20, rnd(31) - 22, k & 1 ? C_GOLD : C_WHITE, 14 + rnd(12));
      sfx_play(SFX_BOOM, pan);
      ev_fx = e->x; ev_fy = e->y; ev_ft = 70;
      popup(e->x, e->y - RI(3), "BOOM!", C_FIRE);
      break;
    case EV_SPLAT:
      for(int k = 0; k < 14; k++) spawn(PT_GOO, e->x, e->y, rnd(25) - 12, -rnd(17), 0, 70 + rnd(40));
      sfx_play(SFX_SPLAT, pan);
      break;
    case EV_TARGET: {
      static const char *const WORDS[] = { "EVICTED!", "SO LONG!", "REFUND?", "NO RENT!" };
      for(int k = 0; k < 6; k++) spawn(PT_STAR, e->x, e->y, rnd(17) - 8, -rnd(14) - 3, 0, 36 + rnd(14));
      spawn(PT_HAT, e->x, e->y - RI(3), rnd(9) - 4, -22, 0, 90);
      sfx_play(SFX_TARGET, pan);
      ev_fx = e->x; ev_fy = e->y; ev_ft = 90;
      popup(e->x, e->y - RI(4), WORDS[rnd(4)], C_GOLD);
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
      sfx_play(SFX_SPLAT, pan); popup(e->x, e->y - RI(4), "SPLOOSH!", C_GOO); ev_fx = e->x; ev_fy = e->y; ev_ft = 60;
      break;
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

static void draw_aim_dots(const View *v){
  const MxMan *m = &world.man;
  if(m->mode != MAN_AIM) return;
  real dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
  if(rmul(dx, dx) + rmul(dy, dy) < R(3)) return;
  for(int k = 1; k <= 16; k++){
    real ox, oy;
    mx_preview_point(&world, RI(k) / 9, &ox, &oy);
    int gx = ox >> 12, gy = oy >> 12;
    if(gx < 0 || gx >= MX_COLS || gy >= MX_ROWS) break;
    if(gy >= 0 && world.grid[gy * MX_COLS + gx]) break;                  /* stop where it would hit */
    int px = vx(v, ox), py = vy(v, oy);
    if(k & 1){ gfx_rect(v->buf, px - 1, py - 1, 3, 3, C_BLACK); gfx_px(v->buf, px, py, C_WHITE); }
    else     { gfx_rect(v->buf, px - 2, py - 2, 5, 5, C_BLACK); gfx_rect(v->buf, px - 1, py - 1, 3, 3, C_GOLD); }
  }
}

static void draw_rooms(const View *v){
  int ppc = v->ppc;
  for(int r = 0; r < world.nrooms; r++){
    const MxRoom *rm = &world.rooms[r];
    int px0 = rm->x * ppc - v->cx, py0 = rm->y * ppc - v->cy;
    if(px0 + rm->w * ppc < 0 || px0 >= 256 || py0 + rm->h * ppc < 0 || py0 >= 192) continue;
    int fl = rm->y + rm->h, cover = 0;
    if(fl < MX_ROWS) for(int x = rm->x; x < rm->x + rm->w; x++) cover += world.grid[fl * MX_COLS + x] != 0;
    if(cover * 2 < rm->w) continue;                                      /* floor gone: no wallpaper left hanging */
    for(int y = rm->y; y < rm->y + rm->h; y++) for(int x = rm->x; x < rm->x + rm->w; x++)
      if(!world.grid[y * MX_COLS + x]) gfx_wall(v, rm->pal, x * ppc - v->cx, y * ppc - v->cy);
  }
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

static void draw_targets(const View *v){
  const MxMan *m = &world.man;
  for(int i = 0; i < world.ntargets; i++){
    const MxTarget *t = &world.targets[i];
    if(!t->alive) continue;
    int scared = (m->mode == MAN_FLY || m->mode == MAN_STUCK) && rabs(t->x - m->x) < RI(26);
    real wx, wy; mx_target_offset(&world, t, &wx, &wy);
    spr_landlord(v->buf, vx(v, t->x + wx), vy(v, t->y + wy), v->ppc, frame + i * 11, scared);
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

/* the air valve: a pipe with an orange nozzle, a gauge, and a pressure bar while he is on it */
static void draw_valve(const View *v){
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
  int pose = POSE_IDLE, face = 1;
  ManJig jg = man_jig(v);
  if(m->mode == MAN_FLY){ pose = m->dive ? POSE_DIVE : POSE_FLY; face = m->vx >= 0 ? 1 : -1; }
  else if(m->mode == MAN_STUCK) pose = POSE_STUCK;
  else if(m->mode == MAN_AIM && dragging) pose = POSE_DRAG;
  if(m->mode == MAN_AIM) gfx_ell(v->buf, x, y + v->ppc, v->ppc * 2, v->ppc * 3 / 2, C_HAIR2);        /* sling pouch */
  if(m->mode == MAN_AIM && !dragging) man_with_towel(v->buf, &tow_play, x, y, v->ppc, face, mi_frame(&mi, frame), &jg);   /* idle gags */
  else spr_man_j(v->buf, x, y, v->ppc, face, pose, frame, &jg);
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

static void draw_world(const View *v, int parallax){
  gfx_bg(v, parallax);
  gfx_ground(v);
  draw_prong_post(v);
  draw_band(v, 0);
  draw_rooms(v);
  draw_voxels(v);
  draw_chunks(v);
  draw_targets(v);
  draw_valve(v);
  draw_man(v);
  draw_band(v, 1);
  draw_aim_dots(v);
  draw_parts(v);
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
  gfx_text_o(b, 4, 24, "LANDLORDS", C_WHITE, C_BLACK, 1);
  draw_hats(b, 46, 28);
  toast_tick();
  if(toast_t >= 0){
    int y = toast_t < 12 ? toast_t * 3 - 36 : toast_t > 138 ? (150 - toast_t) * 3 - 36 : 0;      /* slides down, waits, slides up */
    gfx_rect(b, 28, 100 + y - 34 + 34, 200, 26, C_UIBG); gfx_rect(b, 28, 100 + y, 200, 1, C_GOLD); gfx_rect(b, 28, 125 + y, 200, 1, C_GOLD);
    gfx_text_c(b, 128, 104 + y, "ACHIEVEMENT", C_GOLD, C_UIBG, 1);
    gfx_text_c(b, 128, 114 + y, mx_ach_name(toast_id), C_WHITE, C_UIBG, 1);
  }
}

static void banner(u8 *b){
  if(world.banner <= 0 || app != ST_PLAY || world.man.mode != MAN_AIM || dragging) return;
  int t = world.banner * 60 >> 12;                       /* frames left */
  int slide = t > 120 ? (t - 120) * 8 : (t < 24 ? (24 - t) * 12 : 0);
  gfx_rect(b, 0, 70, 256, 44, C_UIBG);
  gfx_rect(b, 0, 69, 256, 1, C_UILINE); gfx_rect(b, 0, 114, 256, 1, C_UILINE);
  gfx_text_c(b, 128 - slide, 76, "LEVEL", C_GOLD, C_BLACK, 1);
  gfx_text_c(b, 128 + slide, 84, mx_level_name(world.level), C_WHITE, C_BLACK, 2);
  gfx_text_c(b, 128 - slide, 102, "EVICT EVERY LANDLORD", C_PINK, C_BLACK, 1);
}

static void end_panel(u8 *b){
  if(world.state == MX_PLAY) return;
  int win = world.state == MX_WIN;
  panel(b, 24, 34, 208, 124);
  gfx_text_c(b, 128, 42, win ? "EVICTION COMPLETE!" : "THE LANDLORDS LIVE...", win ? C_GOLD : C_RED, C_BLACK, 2);
  if(win){
    man_with_towel(b, &tow_win[0], 52, 98, 3, 1, mi_dance_frame(frame), 0);          /* victory butt-wiggle, one on each side */
    man_with_towel(b, &tow_win[1], 204, 98, 3, -1, mi_dance_frame(frame + 14), 0);
    int stars = 1 + (world.men >= 1) + (world.men >= 2);
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
    gfx_text_c(b, 128, 144, "SPARE MEN BONUS INCLUDED", C_GOO, C_UIBG, 1);
  } else if(!win){
    gfx_text_c(b, 128, 144, "DAMAGE IS NEVER ENOUGH", C_UILINE, C_UIBG, 1);
  }
  if(win && best[world.level] >= world.score && end_t > 60 && (frame & 16)) gfx_text_c(b, 128, 150, "NEW BEST!", C_PINK, C_BLACK, 1);
}

/* the 2 px/cell overview + HUD strip on the touch screen */
static void bottom_overview(View *v){
  draw_world(v, 0);
  draw_popups(v, 1);
  gfx_rect(v->buf, 0, 164, 256, 28, C_UIBG); gfx_rect(v->buf, 0, 164, 256, 1, C_UILINE);
  draw_men_icons(v->buf, 12, 177, 0);
  draw_hats(v->buf, 100, 178);
  button(v->buf, 170, 170, 46, 16, "RESTART");
  button(v->buf, 220, 170, 32, 16, "MENU");
  if(world.man.mode == MAN_FLY && !world.man.dive && (frame & 32)) gfx_text_c(v->buf, 128, 6, "TAP = BELLY FLOP!", C_WHITE, C_BLACK, 1);
}

/* the 4 px/cell sling zoom */
static void bottom_aim(View *v){
  draw_world(v, 1);
  draw_popups(v, 1);
  gfx_rect(v->buf, 0, 0, 256, 11, C_UIBG); gfx_rect(v->buf, 0, 11, 256, 1, C_UILINE);
  gfx_text(v->buf, 4, 3, "MEN", C_GOLD, 1);
  draw_men_icons(v->buf, 36, 1, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("SELECT RESTART", 1), 3, "SELECT RESTART", C_UILINE, 1);
  const MxMan *m = &world.man;
  real dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
  int pct = rsqrt(rmul(dx, dx) + rmul(dy, dy)) * 100 / MX_PULL_MAX;
  gfx_text_o(v->buf, 6, 172, "POWER", C_WHITE, C_BLACK, 1);
  gfx_rect(v->buf, 6, 180, 82, 8, C_UIBG); gfx_rect(v->buf, 7, 181, 80, 6, C_DARK);
  gfx_rect(v->buf, 7, 181, gfx_clamp(pct, 0, 100) * 80 / 100, 6, pct < 50 ? C_GOO : pct < 85 ? C_GOLD : C_RED);
  if(pct < 14 && (frame & 32)) gfx_text_c(v->buf, 150, 40, "DRAG THE MAN BACK, LET GO TO FLING", C_WHITE, C_BLACK, 1);
  static const char *const PWN[3] = { "BEANS", "TOWEL", "SPLAT" };      /* one use each per level; touch to arm, then fling */
  for(int k = 0; k < 3; k++){
    int left = world.pw_left[k + 1], on = world.pw_sel == k + 1;
    gfx_rect(v->buf, 96 + k * 52, 172, 50, 16, on ? C_GOLD : C_UIBG);
    gfx_rect(v->buf, 96 + k * 52, 172, 50, 1, C_UILINE);
    gfx_text_c(v->buf, 96 + k * 52 + 25, 176, PWN[k], on ? C_BLACK : left ? C_WHITE : C_GRAY, on ? C_GOLD : C_UIBG, 1);
  }
  if(world.pw_sel) gfx_text_c(v->buf, 128, 160, world.pw_sel == PW_BEANS ? "TAP IN THE AIR = FART JET" : world.pw_sel == PW_TOWEL ? "TOWEL: GLIDE FURTHER" : "SPLAT: BIG GOO BLAST", C_GOLD, C_BLACK, 1);
}

/* ------------------------------------------------------------------ title */
/* outfit: recolour the man's boxers and towel palette entries (MS_PAL 5,6,7 boxers; 13,14,15 towel) */
static void apply_outfit(int o){
  uint8_t sh[6][3]; mx_outfit_shades(o, sh);
  static const int idx[6] = { 5, 6, 7, 13, 14, 15 };
  for(int k = 0; k < 6; k++){
    u16 c = RGB15(sh[k][0] >> 3, sh[k][1] >> 3, sh[k][2] >> 3);
    gfx_pal[C_MAN0 + idx[k]] = c; BG_PALETTE[C_MAN0 + idx[k]] = c; BG_PALETTE_SUB[C_MAN0 + idx[k]] = c;
  }
}
static void load_level(int l){
  mx_load(&world, l); mx_meta_level_start(&stats, &world); world.valve_ok = mx_valve_unlocked(&stats);
  mx_set_substeps(&world, world.substeps > MX_DS_MAX_SUBSTEPS ? MX_DS_MAX_SUBSTEPS : world.substeps);
  cells0 = mx_cells_left(&world); disp_score = 0; end_t = 0; was_state = MX_PLAY; dragging = 0; pad_used = 0;
  memset(parts, 0, sizeof parts); memset(pops, 0, sizeof pops);
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

static void title_bottom(View *v){
  draw_world(v, 0);
  panel(v->buf, 8, 6, 240, 38);
  gfx_text_c(v->buf, 128, 11, "SELECT LEVEL", C_UILINE, C_UIBG, 1);
  const char *nm = mx_level_name(sel_level);
  gfx_text_c(v->buf, 128, 21, nm, C_GOLD, C_BLACK, 2);
  gfx_text(v->buf, 14, 20, "<", C_WHITE, 2);
  gfx_text(v->buf, 232, 20, ">", C_WHITE, 2);
  gfx_text_c(v->buf, 128, 36, best[sel_level] ? "BEST $" : "NOT PLAYED YET", C_UILINE, C_UIBG, 1);
  if(best[sel_level]) gfx_text(v->buf, 128 + 12, 36, gfx_int(best[sel_level]), C_GOLD, 1);
  gfx_rect(v->buf, 0, 164, 256, 28, C_UIBG); gfx_rect(v->buf, 0, 164, 256, 1, C_UILINE);
  if(frame & 32) gfx_text_c(v->buf, 128, 174, "TOUCH SCREEN OR PRESS A", C_WHITE, C_UIBG, 1);
  else           gfx_text_c(v->buf, 128, 174, "TO START", C_GOLD, C_UIBG, 1);
  gfx_text_c(v->buf, 128, 183, "Y = MUSIC", C_UILINE, C_UIBG, 1);
  gfx_text(v->buf, 4, 183, "R: OUTFIT", C_UILINE, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("ACH 00/25", 1), 183, "ACH", C_UILINE, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("00/25", 1), 183, gfx_int(mx_ach_count(&stats)), C_GOLD, 1);
  gfx_text(v->buf, 256 - 4 - gfx_text_w("/25", 1), 183, "/25", C_UILINE, 1);
  gfx_text_c(v->buf, 128, 46, mx_outfit(outfit_sel)->name, C_PINK, C_BLACK, 1);
}

/* ---------------------------------------------------------------------- main */
static void present(void){
  DC_FlushRange(buf_top, sizeof buf_top);
  DC_FlushRange(buf_bot, sizeof buf_bot);
  swiWaitForVBlank();
  if(title_pal_on != (app == ST_TITLE)){                         /* the top screen's palette: the title picture on the title, the game's otherwise */
    title_pal_on = app == ST_TITLE;
    if(title_pal_on) title_t = 0;                                /* the pan starts from the logo every time the title comes up */
    for(int k = 0; k < TITLE_DS_NCOL; k++)
      BG_PALETTE_SUB[TITLE_DS_BASE + k] = title_pal_on ? RGB15(TITLE_DS_PAL[k * 3] >> 3, TITLE_DS_PAL[k * 3 + 1] >> 3, TITLE_DS_PAL[k * 3 + 2] >> 3) : gfx_pal[TITLE_DS_BASE + k];
  }
  dmaCopy(buf_bot, BG_GFX, sizeof buf_bot);
  dmaCopy(buf_top, BG_GFX_SUB, sizeof buf_top);
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

  mx_meta_init(&stats); apply_outfit(0);
  load_level(sel_level);
  STAGE(4, RGB15(31,31,0));                            /* yellow */
  int slow_frames = 0, fast_frames = 0, phys_ticks = 0;

  for(;;){
    frame++;
    scanKeys();
    int down = keysDown(), held = keysHeld(), up = keysUp();
    touchPosition t = { 0, 0 };
    if(held & KEY_TOUCH) touchRead(&t);
    if(down & KEY_Y){ music_pref = !music_pref; music_set(music_pref && app != ST_PAUSE && world.state == MX_PLAY); }
    if(down & KEY_L) debug_on = !debug_on;

    /* ---------------- input ---------------- */
    if(app == ST_TITLE){
      int ch = 0;
      if(down & KEY_LEFT) ch = -1;
      if(down & KEY_RIGHT) ch = 1;
      if((down & KEY_TOUCH) && hit(&t, 0, 0, 48, 48)) ch = -1;
      if((down & KEY_TOUCH) && hit(&t, 208, 0, 48, 48)) ch = 1;
      if(down & KEY_R){                                          /* next unlocked outfit; locked ones are skipped */
        for(int k = 1; k <= MX_NUM_OUTFITS; k++){ int n = (outfit_sel + k) % MX_NUM_OUTFITS; if(mx_outfit_unlocked(&stats, n)){ outfit_sel = n; break; } }
        apply_outfit(outfit_sel); sfx_play(SFX_CLICK, 64);
      }
      if(ch){ sel_level = (sel_level + ch + MX_NUM_LEVELS) % MX_NUM_LEVELS; load_level(sel_level); sfx_play(SFX_CLICK, 64); }
      else if((down & (KEY_A | KEY_START)) || ((down & KEY_TOUCH) && t.py > 48)){
        app = ST_PLAY; load_level(sel_level); sfx_play(SFX_CLICK, 64); music_set(music_pref);
      }
    } else if(app == ST_PAUSE){
      if(down & KEY_START) app = ST_PLAY;
      if(down & KEY_SELECT){ load_level(world.level); app = ST_PLAY; music_set(music_pref); }
      if(down & KEY_B){ app = ST_TITLE; sel_level = world.level; load_level(sel_level); }
    } else if(world.state != MX_PLAY){
      if(down & KEY_B){ app = ST_TITLE; sel_level = world.level; load_level(sel_level); }
      else if((down & (KEY_START | KEY_A)) || ((down & KEY_TOUCH) && end_t > 30)){
        int next = world.state == MX_WIN ? world.level + 1 : world.level;
        if(next >= MX_NUM_LEVELS){ app = ST_TITLE; sel_level = 0; load_level(0); }
        else { load_level(next); music_set(music_pref); }
        sfx_play(SFX_CLICK, 64);
      }
    } else {
      if(down & KEY_START){ app = ST_PAUSE; music_set(0); }
      if(down & KEY_SELECT){ load_level(world.level); sfx_play(SFX_CLICK, 64); }
      MxMan *m = &world.man;
      if(m->mode == MAN_AIM){
        View av = { buf_bot, 4, 0, 56 };
        if(down & KEY_TOUCH){
          for(int k = 0; k < 3; k++) if(hit(&t, 96 + k * 52, 172, 50, 16)){ mx_select_power(&world, k + 1); sfx_play(SFX_CLICK, 64); }
        }
        if(down & KEY_X){ int nx = world.pw_sel; for(int tries = 0; tries < 4; tries++){ nx = (nx + 1) % 4; if(nx == 0 || world.pw_left[nx] > 0) break; } if(nx == 0 && world.pw_sel) mx_select_power(&world, world.pw_sel); else if(nx) mx_select_power(&world, nx); sfx_play(SFX_CLICK, 64); }
        world.valve_ok = mx_valve_unlocked(&stats);                       /* beat 3 levels to unlock the air valve */
        if((down & KEY_R) && mx_valve_seat(&world)) sfx_play(SFX_CLICK, 64);
        if(down & KEY_TOUCH){
          int vdx = t.px - vx(&av, MX_VALVE_X), vdy = t.py - vy(&av, MX_VALVE_Y);
          int dx = t.px - vx(&av, m->x), dy = t.py - vy(&av, m->y);
          if(!m->seated && vdx * vdx + vdy * vdy < 20 * 20 && mx_valve_seat(&world)){ dragging = 0; sfx_play(SFX_CLICK, 64); }   /* tap the valve */
          else dragging = dx * dx + dy * dy < 34 * 34;                   /* grab the man */
        }
        if((held & KEY_TOUCH) && dragging){
          int vdx = t.px - vx(&av, MX_VALVE_X), vdy = t.py - vy(&av, MX_VALVE_Y);
          if(!(m->seated && vdx * vdx + vdy * vdy < 20 * 20)){            /* seated: the stylus must leave the valve to pull him off */
          real cx = RI(t.px + av.cx) >> 2, cy = RI(t.py + av.cy) >> 2;    /* stylus px -> cells (4 px per cell) */
          mx_set_pull(&world, cx - MX_ANCHOR_X, cy - MX_ANCHOR_Y);
          if(frame % 6 == 0) sfx_play(SFX_STRETCH, 40);
          }
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
    if(app == ST_PLAY){
      /* idle gags: only while he waits in the sling and nobody is touching anything */
      if(world.man.mode == MAN_AIM && !dragging && !pad_used && world.banner <= 0 && world.state == MX_PLAY && !(down | held)){
        mi_update(&mi);
        if(mi.fx) idle_fx(mi.fx, world.man.x, world.man.y, 1, 1, 1);
      } else mi_reset(&mi);
      if(dragging && (frame % 7) == 0) spawn(PT_SWEAT, world.man.x + RI(rnd(5) - 2), world.man.y - RI(4), rnd(9) - 4, -rnd(8), 0, 28);       /* straining */
      if(world.man.mode == MAN_FLY && (frame % 10) == 0)                                                                                          /* stink trail */
        spawn(PT_STINK, world.man.x - (world.man.vx > 0 ? RI(2) : -RI(2)), world.man.y + RI(1), rnd(5) - 2, rnd(3) - 1, 0, 40 + rnd(20));
      cpuStartTiming(0);
      mx_update(&world, DT);
      phys_ticks = (int)cpuEndTiming();
      /* adaptive quality: fewer substeps when the solver eats too much of the frame, more when there is headroom */
      /* order of sacrifice when the solver is slow: fewer chunk substeps first, then the wobble lattice; the reverse when there is headroom */
      if(phys_ticks > 300000){ fast_frames = 0;
        if(++slow_frames >= 3){ slow_frames = 0;
          if(world.substeps > 2) mx_set_substeps(&world, world.substeps - 1);
          else if(world.wob_enabled) mx_set_wobble(&world, 0); } }
      else if(phys_ticks < 150000){ slow_frames = 0;
        if(++fast_frames >= 120){ fast_frames = 0;
          if(!world.wob_enabled) mx_set_wobble(&world, 1);
          else if(world.substeps < MX_DS_MAX_SUBSTEPS) mx_set_substeps(&world, world.substeps + 1); } }
      else { slow_frames = 0; fast_frames = 0; }
      on_events();
      mx_meta_events(&stats, &world);
      for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a);
      update_parts();
      if(world.state != MX_PLAY){
        if(was_state == MX_PLAY){ end_t = 0; if(world.state == MX_WIN){ if(world.score > best[world.level]) best[world.level] = world.score; mx_meta_win(&stats, &world); for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a); } }
        end_t++;
      }
      was_state = world.state;
      int d = world.score - disp_score;
      if(d) disp_score += d > 0 ? (d + 7) / 8 : -((-d + 7) / 8);
    } else if(app == ST_TITLE){
      update_parts();
    }
    sfx_tick();

    /* ---------------- camera ---------------- */
    real tx = RI(88), ty = RI(38), rate = R(0.10);
    const MxMan *m = &world.man;
    if(app == ST_PLAY){
      if(m->mode == MAN_FLY){ tx = m->x + rmul(m->vx, R(0.22)); ty = m->y; rate = R(0.3); }
      else if(m->mode == MAN_STUCK){ tx = m->x; ty = m->y; rate = R(0.2); }
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
    View bot = { buf_bot, 2, -8, -40 };
    if(app == ST_TITLE){
      title_top(&top);
      title_bottom(&bot);
    } else {
      draw_world(&top, 1);
      draw_popups(&top, 2);
      top_hud(buf_top);
      banner(buf_top);
      end_panel(buf_top);
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
    }
    present();
  }
  return 0;
}
