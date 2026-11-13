/*
 * MANDEX - Nintendo 3DS front-end (devkitARM + libctru, no citro2d/citro3d).
 *
 * Everything is drawn in software straight into the two framebuffers (BGR8, column-major).
 *   TOP    400x240 : the whole level at 3 px per voxel (360x180), HUD above and below it.
 *   BOTTOM 320x240 : the sling, 4 px per voxel.  Touch = drag the man back, lift to fling.  Tap while flying = belly flop.
 * Game logic is the shared core (../core) in float mode (do not define MX_FIXED here).
 *
 *   touch  : aim / fling / belly flop         A or B : belly flop in flight, next level / retry on the end panel
 *   D-pad  : moves the man in the sling, A flings          L/R or left/right : level select on the title
 *   SELECT : restart   START : quit
 */
#include <3ds.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "mx.h"
#include "mx_meta.h"
#include "manspr.h"                                /* the same pixel-art man as the DS and Dreamcast ports (generated from the web game) */
#include "manidle.h"                                /* his idle gags, victory dance */

static MxWorld world;
enum { ST_TITLE, ST_PLAY, ST_PAUSE };
static float g_dim = 1.0f;                      /* 1 = normal; <1 draws the dark back layer that gives blocks thickness in 3D */
static int app = ST_TITLE, sel = 0, frame, dragging, pad_used, cells0 = 1;
static float pad_dx, pad_dy;
static int new_best;                                     /* this win beat the level's old best score */
static uint32_t prev_best;                              /* best[level] just before the win was recorded */
static MxStats stats;                                   /* session achievements / outfits (no save on the 3DS yet) */
static int outfit_sel, toast_id = -1, toast_q[6], toast_nq, toast_t;
static void toast_push(int id){ if(toast_nq < 6) toast_q[toast_nq++] = id; }

/* ---------------------------------------------------------------- pixels */
typedef struct { u8 *fb; int w, h; } Scr;                       /* w x h in screen terms (400x240 or 320x240) */
static Scr top, bot;
static inline void px(Scr *s, int x, int y, u32 rgb){
  if((unsigned)x >= (unsigned)s->w || (unsigned)y >= (unsigned)s->h) return;
  u8 *p = s->fb + ((x * s->h) + (s->h - 1 - y)) * 3;
  p[0] = rgb & 255; p[1] = (rgb >> 8) & 255; p[2] = (rgb >> 16) & 255;   /* the 3DS framebuffer is BGR8: byte 0 = blue, so a colour is 0xRRGGBB and callers use RGB() */
}
#define RGB(r, g, b) ((u32)(r) << 16 | (u32)(g) << 8 | (u32)(b))   /* low byte = blue = framebuffer byte 0 (was r in the low byte, which showed red and blue swapped) */
static void rect(Scr *s, int x, int y, int w, int h, u32 c){
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > s->w ? s->w : x + w, y1 = y + h > s->h ? s->h : y + h;
  for(int i = x0; i < x1; i++){
    u8 *p = s->fb + (i * s->h + (s->h - 1 - y0)) * 3;
    for(int j = y0; j < y1; j++, p -= 3){ p[0] = c & 255; p[1] = (c >> 8) & 255; p[2] = (c >> 16) & 255; }
  }
}
static void disc(Scr *s, int cx, int cy, int r, u32 c){
  for(int y = -r; y <= r; y++) for(int x = -r; x <= r; x++) if(x * x + y * y <= r * r) px(s, cx + x, cy + y, c);
}
static void line(Scr *s, int x0, int y0, int x1, int y1, u32 c){
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for(;;){ px(s, x0, y0, c); if(x0 == x1 && y0 == y1) break; int e2 = 2 * e; if(e2 >= dy){ e += dy; x0 += sx; } if(e2 <= dx){ e += dx; y0 += sy; } }
}

/* 3x5 font: A-Z, 0-9 and a few signs (15 bits, MSB = top-left) */
static const u16 FONT[] = {0x2BED,0x6BAE,0x3923,0x6B6E,0x79A7,0x79A4,0x396B,0x5BED,0x7497,0x126A,0x5BAD,0x4927,0x5FED,0x6B6D,0x2B6A,0x6BA4,0x2B73,0x6BAD,0x388E,0x7492,0x5B6F,0x5B6A,0x5BFD,0x5AAD,0x5A92,0x72A7,0x7B6F,0x2C97,0x62A7,0x628E,0x5BC9,0x798E,0x39EF,0x7292,0x7BEF,0x7BCE,0x12A4,0x01C0,0x2482,0x0410,0x0002,0x1511,0x4454,0x52A5};
static void glyph(Scr *s, int x, int y, int ch, int k, u32 c){
  static const char EXTRA[] = "/-!:.<>%";
  int idx = -1;
  if(ch >= 'A' && ch <= 'Z') idx = ch - 'A';
  else if(ch >= '0' && ch <= '9') idx = 26 + ch - '0';
  else for(int i = 0; EXTRA[i]; i++) if(EXTRA[i] == ch) idx = 36 + i;
  if(idx < 0) return;
  u16 g = FONT[idx];
  for(int r = 0; r < 5; r++) for(int q = 0; q < 3; q++) if(g & (1 << (14 - (r * 3 + q)))) rect(s, x + q * k, y + r * k, k, k, c);
}
static void text(Scr *s, int x, int y, const char *t, int k, u32 c){
  for(; *t; t++, x += 4 * k){ int ch = *t; if(ch >= 'a' && ch <= 'z') ch -= 32; glyph(s, x, y, ch, k, c); }
}
static int tw(const char *t, int k){ return (int)strlen(t) * 4 * k - k; }
static void textc(Scr *s, int cx, int y, const char *t, int k, u32 c){ text(s, cx - tw(t, k) / 2, y, t, k, c); }
static void itoa_(int v, char *o){ sprintf(o, "%d", v); }

/* ---------------------------------------------------------------- tiny effects */
typedef struct { float x, y, vx, vy; int life; u32 c; int kind; } Part;     /* kind 0 = dust/debris, 1 = heart, 2 = sparkle (the last two float: no gravity) */
#define NP 160
static Part parts[NP];
static void spawn_k(float x, float y, float vx, float vy, int life, u32 c, int kind){
  for(int i = 0; i < NP; i++) if(parts[i].life <= 0){ parts[i] = (Part){x, y, vx, vy, life, c, kind}; return; }
}
static void spawn(float x, float y, float vx, float vy, int life, u32 c){ spawn_k(x, y, vx, vy, life, c, 0); }
static void popup(float x, float y, const char *s, u32 c);
static void on_events(void){
  for(int i = 0; i < world.nev; i++){
    const MxEvent *e = &world.ev[i];
    float x = RTOF(e->x), y = RTOF(e->y);
    switch(e->type){
    case EV_CELL: { const uint8_t *c = mx_material_rgb(e->m);
      spawn(x, y, RTOF(e->a) * .3f + (rand() % 9 - 4) * .2f, RTOF(e->b) * .3f - (rand() % 7) * .2f, 30 + rand() % 20, RGB(c[0], c[1], c[2])); } break;
    case EV_BOOM: for(int k = 0; k < 24; k++) spawn(x, y, (rand() % 21 - 10) * .35f, (rand() % 21 - 10) * .35f, 30, RGB(255, 200 - rand() % 100, 40)); break;
    case EV_SPLAT: for(int k = 0; k < 12; k++) spawn(x, y, (rand() % 15 - 7) * .3f, -(rand() % 9) * .3f, 30, RGB(110, 255, 90)); break;
    case EV_TUBE: { static const u32 TC[3] = { RGB(255, 154, 60), RGB(79, 208, 255), RGB(140, 255, 90) };        /* posted through a pneumatic tube */
      for(int k = 0; k < 16; k++){ float an = k * .39f; spawn(x, y, cosf(an) * 3, sinf(an) * 3, 26, TC[e->m % 3]); spawn(RTOF(e->a), RTOF(e->b), cosf(an) * 3, sinf(an) * 3, 26, TC[e->m % 3]); }
      popup(x, y - 5, "FWOOMP!", TC[e->m % 3]); } break;
    case EV_TARGET: for(int k = 0; k < 14; k++) spawn(x, y, (rand() % 15 - 7) * .3f, -(rand() % 11) * .3f, 36, RGB(255, 220, 60)); break;
    default: break;
    }
  }
}
/* floating words ("MWAH!", "BUUURP!"): world position in cells */
typedef struct { float x, y; int life; const char *s; u32 c; } Pop;
static Pop pops[4];
static void popup(float x, float y, const char *s, u32 c){
  for(int i = 0; i < 4; i++) if(pops[i].life <= 0){ pops[i] = (Pop){x, y, 70, s, c}; return; }
}
static void tick_parts(void){
  for(int i = 0; i < NP; i++) if(parts[i].life > 0){ Part *p = &parts[i]; if(!p->kind) p->vy += .06f; p->x += p->vx * .1f; p->y += p->vy * .1f; p->life--; }
  for(int i = 0; i < 4; i++) if(pops[i].life > 0){ pops[i].y -= .05f; pops[i].life--; }
}

/* ---------------------------------------------------------------- the man: indexed sprite frames (art/manspr.h, 4 art pixels per cell), scaled to ppc px per cell */
static inline int fdiv4(int a){ return a >= 0 ? a / 4 : -((-a + 3) / 4); }
static void blit_frame(Scr *s, int fr, int cx, int cy, int ppc, int face){
  const MsFrame *f = &MS_FRAMES[fr];
  for(int sy = 0; sy < f->h; sy++){
    int dy = sy - f->ay, y0 = cy + fdiv4(dy * ppc), y1 = cy + fdiv4((dy + 1) * ppc);
    if(y1 <= y0) y1 = y0 + 1;
    if(y1 < 0 || y0 >= s->h) continue;
    for(int sx = 0; sx < f->w; sx++){
      int c = f->px[sy * f->w + sx];
      if(!c) continue;
      int dx = face >= 0 ? sx - f->ax : f->ax - sx - 1, x0 = cx + fdiv4(dx * ppc), x1 = cx + fdiv4((dx + 1) * ppc);
      if(x1 <= x0) x1 = x0 + 1;
      rect(s, x0, y0, x1 - x0, y1 - y0, RGB(MS_PAL[c - 1][0], MS_PAL[c - 1][1], MS_PAL[c - 1][2]));
    }
  }
}
static MiState mi;                                             /* idle gag state, see art/manidle.h */
static void idle_fx(int fx, float wx, float wy){               /* particles and words for a gag step (no sound on the 3DS port yet) */
  float mxp = wx + 1.3f, myp = wy - 3.6f;
  switch(fx){
  case FX_SLAP: spawn(wx - 2, wy - 4, -.6f, -1.f, 30, RGB(150, 200, 255)); spawn(wx + 2, wy - 4, .6f, -1.f, 30, RGB(150, 200, 255)); break;
  case FX_GRUNT: spawn(wx + 2, wy - 4, .4f, -1.2f, 30, RGB(150, 200, 255)); break;
  case FX_BURP: for(int k = 0; k < 6; k++) spawn(mxp, myp, .8f + (rand() % 10) * .1f, (rand() % 7 - 3) * .1f, 50 + rand() % 20, RGB(125, 255, 90)); popup(mxp, myp - 3, "BUUURP!", RGB(125, 255, 90)); break;
  case FX_FART: for(int k = 0; k < 9; k++) spawn(wx + (rand() % 5 - 2), wy + 1.5f, (rand() % 13 - 6) * .1f, -(rand() % 5) * .1f, 60 + rand() % 30, RGB(125, 255, 90)); popup(wx, wy - 5, "PFFFFT!", RGB(125, 255, 90)); break;
  case FX_FLICK: spawn(mxp, myp, 1.8f, -.9f, 50, RGB(125, 255, 90)); break;
  case FX_TAUNT: popup(wx, wy - 7, "NYAH NYAH!", RGB(255, 126, 176)); break;
  case FX_EEK: spawn(wx - 2, wy - 5, -.6f, -1.f, 30, RGB(150, 200, 255)); spawn(wx + 2, wy - 5, .6f, -1.f, 30, RGB(150, 200, 255)); popup(wx, wy - 7, "EEK!", RGB(255, 90, 90)); break;
  case FX_SNAP: popup(wx, wy - 5, "SNAP!", RGB(255, 210, 63)); break;
  case FX_SNORE: spawn(wx + 3, wy - 5, .2f, -.4f, 60, RGB(235, 240, 255)); break;
  case FX_KISS: for(int k = 0; k < 3; k++) spawn_k(wx + 3 + k, wy - 5, .3f + (rand() % 4) * .1f, -(.3f + (rand() % 3) * .1f), 70 + rand() % 20, RGB(255, 126, 176), 1);
                popup(wx + 2, wy - 7, "MWAH!", RGB(255, 126, 176)); break;
  case FX_SPARKLE: for(int k = 0; k < 5; k++) spawn_k(wx + (rand() % 9 - 4), wy - (rand() % 9), 0, -.1f, 14 + rand() % 8, (k & 1) ? RGB(255, 245, 190) : RGB(255, 210, 63), 2); break;
  case FX_DISCO: spawn_k(wx + ((rand() & 1) ? 1 : -1) * (3 + rand() % 3), wy - (5 + rand() % 3), 0, -.1f, 16, RGB(255, 255, 255), 2); break;
  case FX_POP: if(!(rand() % 3)) popup(wx, wy - 5, "POP!", RGB(255, 210, 63)); break;
  default: break;
  }
}

/* ---------------------------------------------------------------- drawing the world into a view */
typedef struct { Scr *s; int ppc, ox, oy; float cx, cy; } View;   /* screen px = ox + (cell - cx) * ppc */
static inline int VX(const View *v, float c){ return v->ox + (int)floorf((c - v->cx) * v->ppc); }
static inline int VY(const View *v, float c){ return v->oy + (int)floorf((c - v->cy) * v->ppc); }

static float g_pan, g_sl; static int g_sgn;   /* eye sign and slider, so layers inside the sky can have their own disparity */
#define LD(D) ((int)lroundf(g_sgn * (D) * g_sl * 0.5f))
static float g_pan_unused;                              /* camera drift in px: follows the man, each layer scrolls by its own fraction */
static void hills(Scr *s, int base, int ox, int step, int r, int amp, int col0, int col1, int col2){
  for(int k = -2; k < s->w / step + 3; k++){ int x = ox + k * step, h = ((k * 37 + 11) & 7) * amp / 7;
    disc(s, x, base - h + r / 2, r, (k & 1) ? col1 : col0); }
  rect(s, 0, base + r / 2, s->w, s->h, col2);
}
static void draw_sky(const View *v){
  Scr *s = v->s; int gy = VY(v, MX_ROWS);
  for(int y = 0; y < s->h; y += 4){ int t = y * 90 / s->h; rect(s, 0, y, s->w, 4, RGB(90 + t / 2, 170 + t / 2, 240 - t / 3)); }
  disc(s, 330 + LD(10) - (int)(g_pan * .02f), 44, 22, RGB(255, 245, 190));                              /* sun: almost at infinity */
  for(int k = 0; k < 5; k++){ int cx = LD(-2 * (k & 1)) + ((k * 97 + (int)(frame * (.12f + .05f * (k & 1))) - (int)(g_pan * .06f)) % 480) - 40;   /* drifting clouds */
    int cy = 30 + (k * 23) % 60; disc(s, cx, cy, 14, RGB(250, 252, 255)); disc(s, cx + 14, cy + 3, 11, RGB(244, 248, 255)); disc(s, cx - 13, cy + 4, 10, RGB(244, 248, 255)); }
  hills(s, gy - 30, v->ox + LD(-8) - (int)(g_pan * .10f), 120, 70, 28, RGB(128, 170, 205), RGB(138, 180, 210), RGB(128, 170, 205));   /* far hills */
  hills(s, gy - 14, v->ox + LD(-4) - (int)(g_pan * .22f), 90, 52, 18, RGB(95, 150, 150), RGB(105, 160, 150), RGB(95, 150, 150));     /* near hills */
  rect(s, 0, gy, s->w, s->h, RGB(96, 64, 38));
  rect(s, 0, gy - 2, s->w, 3, RGB(70, 170, 70));
}
static void draw_fore(const View *v){               /* grass blades and a fence post in front of everything: the nearest layer */
  Scr *s = v->s; int gy = VY(v, MX_ROWS);
  for(int x = -((int)g_pan % 18) - 18; x < s->w + 18; x += 18){ int h = 6 + ((x * 7 + 3) & 7), sw = (int)(sinf(frame * .05f + x) * 1.5f);
    rect(s, x + sw, gy - h + 6, 2, h, RGB(48, 140, 52)); rect(s, x + 4 + sw, gy - h / 2 + 6, 2, h / 2, RGB(70, 170, 70)); }
}
static void draw_shadow(const View *v, int cx, int cy_ground, int w){ rect(v->s, cx - w / 2, cy_ground, w, 3, RGB(60, 90, 40)); }
static void draw_cells(const View *v){
  int p = v->ppc;
  for(int y = 0; y < MX_ROWS; y++) for(int x = 0; x < MX_COLS; x++){
    int i = y * MX_COLS + x, m = world.grid[i];
    if(!m) continue;
    float fx = x, fy = y;
    if(world.wob_on){ fx += RTOF(world.ox[i]); fy += RTOF(world.oy[i]); }
    int sx = VX(v, fx), sy = VY(v, fy);
    if(sx + p < 0 || sx >= v->s->w || sy + p < 0 || sy >= v->s->h) continue;
    const uint8_t *c = mx_material_rgb(m);
    float f = .55f + .45f * RTOF(world.hp[i]) / (float)mx_material_hp(m);   /* darker as it takes damage */
    if(f > 1) f = 1;
    f *= g_dim;
    u32 col = RGB((int)(c[0] * f), (int)(c[1] * f), (int)(c[2] * f));
    if(world.brk[i]) col = RGB((int)(c[0] * f * .8f), (int)(c[1] * f * .8f), (int)(c[2] * f * .8f));
    rect(v->s, sx, sy, p, p, col);
  }
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    const MxChunk *ch = &world.chunks[c];
    if(!ch->active) continue;
    for(int k = 0; k < ch->count; k++){
      const MxCell *a = &ch->cells[k];
      if(a->dead) continue;
      const uint8_t *col = mx_material_rgb(a->m);
      rect(v->s, VX(v, RTOF(a->wx)) - p / 2, VY(v, RTOF(a->wy)) - p / 2, p, p, RGB((int)(col[0] * g_dim), (int)(col[1] * g_dim), (int)(col[2] * g_dim)));
    }
  }
}
static void draw_landlords(const View *v){
  for(int i = 0; i < world.ntargets; i++){
    const MxTarget *t = &world.targets[i];
    if(!t->alive) continue;
    real ox, oy; mx_target_offset(&world, t, &ox, &oy);
    int x = VX(v, RTOF(t->x + ox)), y = VY(v, RTOF(t->y + oy)), p = v->ppc;
    rect(v->s, x - p, y - p / 2, p * 2, p * 2, RGB(60, 70, 90));          /* suit */
    disc(v->s, x, y - p, p, RGB(240, 200, 160));                           /* head */
    rect(v->s, x - p, y - p * 2, p * 2, p / 2 + 1, RGB(20, 20, 20));       /* hat brim */
    rect(v->s, x - p / 2, y - p * 3, p, p + 1, RGB(20, 20, 20));           /* hat */
  }
}
static void draw_man(const View *v){
  const MxMan *m = &world.man;
  if(m->mode == MAN_NONE || m->mode == MAN_WAIT) return;
  int p = v->ppc, x = VX(v, RTOF(m->x)), y = VY(v, RTOF(m->y)), face = 1, fr;
  if(m->mode == MAN_FLY){ face = m->vx >= 0 ? 1 : -1; fr = m->dive ? MSF_DIVE : (((frame >> 2) & 1) ? MSF_FLY_A : MSF_FLY_B); }
  else if(m->mode == MAN_STUCK){ disc(v->s, x, y, p * 2, RGB(110, 255, 90)); fr = ((frame >> 3) & 1) ? MSF_STUCK_A : MSF_STUCK_B; }
  else if(dragging) fr = MSF_DRAG;
  else fr = mi_frame(&mi, frame);                                            /* idle gags */
  blit_frame(v->s, fr, x, y, p, face);
  if(m->inf > 0 && (m->mode == MAN_AIM || m->mode == MAN_FLY)){             /* only the belly swells */
    float f = RTOF(m->inf); int r = (int)(p * (2 + f * 3.2f)), bx = x + face * (int)(p * f * 1.5f);
    disc(v->s, bx, y + p / 2, r + 1, RGB(205, 135, 100)); disc(v->s, bx, y + p / 2, r, RGB(244, 183, 142));
    disc(v->s, bx - r / 3, y + p / 2 - r / 3, r / 5 + 1, RGB(255, 215, 180));
  }
}
/* chapter 2 pneumatic tubes: a balloon-held brass mouth to fly into, and the nozzle stub on the wall it posts you into */
static void draw_tubes(const View *v){
  static const u32 TC[3] = { RGB(255, 154, 60), RGB(79, 208, 255), RGB(140, 255, 90) };
  Scr *s = v->s; int p = v->ppc;
  for(int i = 0; i < world.ntubes; i++){
    const MxTube *t = &world.tubes[i]; u32 c = TC[i % 3];
    int ax = VX(v, RTOF(t->ax)), ay = VY(v, RTOF(t->ay)), bx = VX(v, RTOF(t->bx)), by = VY(v, RTOF(t->by));
    int balx = ax + (int)(sinf(frame * .02f + i * 2) * 2), baly = ay - p * 31 / 4 + (int)(sinf(frame * .03f + i) * 2);
    line(s, balx - p, baly + p * 3, ax - p * 3 / 2, ay - p * 3, RGB(58, 42, 26)); line(s, balx + p, baly + p * 3, ax + p * 3 / 2, ay - p * 3, RGB(58, 42, 26));
    disc(s, balx, baly, p * 7 / 2 + 1, RGB(0, 0, 0)); disc(s, balx, baly, p * 7 / 2, c); disc(s, balx - p, baly - p * 3 / 2, p * 3 / 4 + 1, RGB(255, 255, 255));
    disc(s, ax, ay, p * 15 / 4, RGB(200, 154, 58)); disc(s, ax, ay, p * 13 / 4, c); disc(s, ax, ay, p * 11 / 4, RGB(42, 33, 24)); disc(s, ax, ay, p, c); disc(s, ax, ay, p / 2 + 1, RGB(42, 33, 24));
    rect(s, bx - p * 19 / 4, by - p * 3 / 2, p * 4, p * 3, RGB(200, 150, 58)); rect(s, bx - p * 9 / 2, by - p * 5 / 4, p * 7 / 2, p * 5 / 8 + 1, RGB(255, 224, 138));
    rect(s, bx - p * 3 / 4, by - p * 13 / 8, p * 3 / 2, p * 13 / 4, c);
  }
}
static void draw_sling(const View *v){
  int sx = VX(v, RTOF(MX_ANCHOR_X)), sy = VY(v, RTOF(MX_ANCHOR_Y)), p = v->ppc;
  rect(v->s, sx - p / 2, sy + p, p, VY(v, MX_ROWS) - sy - p, RGB(122, 82, 48));
  if(world.man.mode == MAN_AIM){
    int mx_ = VX(v, RTOF(world.man.x)), my = VY(v, RTOF(world.man.y));
    line(v->s, sx - p, sy, mx_, my, RGB(42, 22, 10)); line(v->s, sx + p, sy, mx_, my, RGB(42, 22, 10));
  }
}
static void draw_aim(const View *v){
  if(world.man.mode != MAN_AIM || !dragging) return;
  for(int k = 1; k < 40; k++){
    real ox, oy; mx_preview_point(&world, R(0.075) * k, &ox, &oy);
    float fx = RTOF(ox), fy = RTOF(oy);
    int gx = (int)fx, gy = (int)fy;
    if(gx < 0 || gx >= MX_COLS || gy >= MX_ROWS) break;
    if(gy >= 0 && world.grid[gy * MX_COLS + gx]) break;
    rect(v->s, VX(v, fx) - 1, VY(v, fy) - 1, 3, 3, RGB(255, 255, 255));
  }
}
static void draw_parts(const View *v){
  for(int i = 0; i < NP; i++) if(parts[i].life > 0){
    const Part *q = &parts[i]; int x = VX(v, q->x), y = VY(v, q->y), k = v->ppc >= 4 ? 2 : 1;
    if(q->kind == 1){                                                        /* heart: two bumps and a point */
      rect(v->s, x - 2 * k, y - k, 2 * k, 2 * k, q->c); rect(v->s, x + 1, y - k, 2 * k, 2 * k, q->c);
      rect(v->s, x - 2 * k, y, 5 * k - (k - 1), k, q->c); rect(v->s, x - k, y + k, 3 * k, k, q->c); rect(v->s, x, y + 2 * k, k, k, q->c);
    } else if(q->kind == 2){                                                 /* sparkle: a four-point star that pulses */
      int r = (q->life > 8 ? 8 : q->life) * k / 2 + 1;
      rect(v->s, x - r, y, 2 * r + 1, 1, q->c); rect(v->s, x, y - r, 1, 2 * r + 1, q->c); rect(v->s, x - 1, y - 1, 3, 3, q->c);
    } else rect(v->s, x, y, 2, 2, q->c);
  }
  for(int i = 0; i < 4; i++) if(pops[i].life > 0){
    int k = v->ppc >= 4 ? 2 : 1; textc(v->s, VX(v, pops[i].x), VY(v, pops[i].y), pops[i].s, k, pops[i].c);
  }
}
static void draw_valve(const View *v){
  int p = v->ppc, x = VX(v, RTOF(MX_VALVE_X)), y = VY(v, RTOF(MX_VALVE_Y)), gy = VY(v, (float)MX_ROWS);
  const MxMan *m = &world.man; int f = m->mode == MAN_AIM ? (int)(RTOF(m->inf) * 100) : 0;
  rect(v->s, x - p * 2, gy - 2, p * 4, 3, RGB(110, 115, 130));
  rect(v->s, x - p / 2 - 1, y + p, p + 2, gy - y - p, RGB(74, 85, 104));
  rect(v->s, x - p, y, p * 2, p * 2, world.valve_ok ? RGB(255, 160, 40) : RGB(130, 130, 140));                         /* nozzle */
  if(m->mode == MAN_AIM && f > 0){ rect(v->s, x - p * 5, y - p * 9, p * 10, p, RGB(20, 24, 40)); rect(v->s, x - p * 5 + 1, y - p * 9 + 1, f * (p * 10 - 2) / 100, p - 2, f < 50 ? RGB(125, 255, 90) : f < 80 ? RGB(255, 210, 63) : RGB(255, 70, 70)); }
}
static void draw_world(const View *v){
  draw_sky(v); draw_cells(v); draw_landlords(v); draw_tubes(v); draw_sling(v); draw_valve(v); draw_aim(v); draw_man(v); draw_parts(v);
}

/* Stereoscopic top screen.  The slider sets how strong the depth is; each layer gets a horizontal disparity D (px at full slider):
   D > 0 sits behind the screen (hills, sky), 0 is the screen plane (the blocks' front faces), D < 0 floats in front (landlords, man, sling, debris).
   The blocks also get a dark back layer a few px behind them, so every wall has visible thickness. */
static void draw_world_3d(const View *vt, int sgn, float sl){
  View a = *vt; g_sgn = sgn; g_sl = sl;
  #define DX(D) ((int)lroundf(sgn * (D) * sl * 0.5f))
  int stereo = sl > 0.03f;
  const MxMan *m = &world.man; float mxc = RTOF(m->x);
  float tp = (m->mode == MAN_FLY || m->mode == MAN_STUCK) ? (mxc - 20.0f) * 6.0f : 0.0f;   /* camera drift follows the flying man */
  g_pan += (tp - g_pan) * .08f;
  int fly = (m->mode == MAN_FLY) ? 1 : 0, dv = m->dive ? 3 : 0;
  a.ox = vt->ox + DX(14); draw_sky(&a);                                                   /* far plane (hills/clouds inside carry their own parallax) */
  if(stereo){ a.ox = vt->ox + DX(4); g_dim = .45f; draw_cells(&a); g_dim = 1.0f; }       /* back faces */
  a.ox = vt->ox; draw_cells(&a);
  a.ox = vt->ox + DX(-3); draw_landlords(&a); draw_tubes(&a);
  a.ox = vt->ox + DX(-4); draw_sling(&a); draw_valve(&a);
  a.ox = vt->ox + DX(-5); draw_aim(&a);
  if(m->mode != MAN_NONE && m->mode != MAN_WAIT){                                         /* ground shadow stays on the ground plane, man lifts off it */
    int sh = VX(&a, mxc); a.ox = vt->ox; draw_shadow(&a, VX(&a, mxc), VY(&a, (float)MX_ROWS) - 1, 26 - (int)(fabsf(RTOF(m->y) - MX_ROWS) * .3f > 14 ? 14 : fabsf(RTOF(m->y) - MX_ROWS) * .3f)); (void)sh; }
  a.ox = vt->ox + DX(-7 - fly * 2 - dv); draw_man(&a);                                    /* he comes out of the screen when flying, further when diving */
  a.ox = vt->ox + DX(-10); draw_parts(&a);
  a.ox = vt->ox + DX(-16); draw_fore(&a);                                                 /* grass in the face */
  #undef DX
}

/* ---------------------------------------------------------------- HUD / panels */
static int alive(void){ return mx_targets_alive(&world); }
static void draw_hud(void){
  rect(&top, 0, 0, 400, 24, RGB(15, 18, 30));
  char b[48];
  text(&top, 6, 8, "LV", 2, RGB(255, 210, 63)); itoa_(world.level + 1, b); text(&top, 30, 8, b, 2, RGB(255, 210, 63));
  text(&top, 56, 8, mx_level_name(world.level), 2, RGB(255, 255, 255));
  rect(&top, 0, 216, 400, 24, RGB(15, 18, 30));
  text(&top, 6, 224, "MEN", 2, RGB(190, 190, 190)); itoa_(world.men, b); text(&top, 40, 224, b, 2, RGB(255, 255, 255));
  text(&top, 80, 224, "LANDLORDS", 2, RGB(255, 138, 138)); sprintf(b, "%d/%d", alive(), world.ntargets); text(&top, 168, 224, b, 2, RGB(255, 255, 255));
  sprintf(b, "%06d", world.score); text(&top, 394 - tw(b, 2), 224, b, 2, RGB(255, 255, 255));
  if(toast_t > 0 && toast_id >= 0){ rect(&top, 100, 30, 200, 30, RGB(20, 24, 40)); textc(&top, 200, 33, "ACHIEVEMENT", 1, RGB(255, 210, 63)); textc(&top, 200, 45, mx_ach_name(toast_id), 2, RGB(255, 255, 255)); }
}
/* 4-point sparkle star built from rows: half-width (r-|dy|)^2 / r gives the concave points */
static void star(Scr *s, int cx, int cy, int r, u32 c){
  for(int dy = -r; dy <= r; dy++){ int a = r - abs(dy), hw = a * a / r; rect(s, cx - hw, cy + dy, hw * 2 + 1, 1, c); }
  disc(s, cx, cy, r / 2, c);
}
static void draw_end(void){
  if(world.state == MX_PLAY) return;
  int win = world.state == MX_WIN;
  rect(&top, 90, 64, 220, 114, RGB(15, 18, 30));
  textc(&top, 200, 74, win ? (world.level == MX_NUM_LEVELS - 1 ? "THE END!" : "LEVEL CLEAR!") : "OUT OF MEN", 3, win ? RGB(125, 255, 90) : RGB(255, 90, 90));
  char b[32]; sprintf(b, "SCORE %d", world.score); textc(&top, 200, 98, b, 2, RGB(255, 255, 255));
  if(win){
    int n = mx_stars_for(&world);
    for(int i = 0; i < 3; i++) star(&top, 200 + (i - 1) * 30, 128, 10, i < n ? RGB(255, 210, 63) : RGB(60, 64, 78));
    if(new_best && (frame & 16)) textc(&top, 200, 146, "NEW BEST!", 2, RGB(255, 126, 176));
  }
  textc(&top, 200, 162, win ? (world.level == MX_NUM_LEVELS - 1 ? "ALL LANDLORDS EVICTED" : "A = NEXT LEVEL") : "A = TRY AGAIN", 2, RGB(255, 210, 63));
  if(win){                                                                   /* two dancers, one on each side of the panel (the 32-beat loop in art/manidle.h) */
    int bob = (int)(fabsf(sinf(frame * .116f)) * 8);
    blit_frame(&top, mi_dance_frame(frame), 52, 150 - bob, 3, 1);
    blit_frame(&top, mi_dance_frame(frame + 14), 348, 150 - (int)(fabsf(sinf(frame * .116f + 1.3f)) * 8), 3, -1);
  } else blit_frame(&top, MSF_SAD, 52, 150, 3, 1);
}

/* ---------------------------------------------------------------- flow */
static void load_level(int l){
  mx_load(&world, l); mx_meta_level_start(&stats, &world); world.valve_ok = mx_valve_unlocked(&stats);
  memset(parts, 0, sizeof parts); memset(pops, 0, sizeof pops); mi_reset(&mi);
  cells0 = mx_cells_left(&world) ? mx_cells_left(&world) : 1;
  dragging = 0; pad_used = 0; pad_dx = pad_dy = 0; new_best = 0;
}
static int was_state;

int main(void){
  gfxInitDefault();
  gfxSetDoubleBuffering(GFX_TOP, true); gfxSetDoubleBuffering(GFX_BOTTOM, true);
  srand(1234);
  load_level(0);
  View vt = {&top, 3, 20, 28, 0, 0};                 /* whole level: 360x180 centred on the top screen */
  View vb = {&bot, 4, 0, 0, -24, 0};                 /* the sling at 4 px per voxel; touch maps straight back to cells */
  while(aptMainLoop()){
    hidScanInput();
    u32 down = hidKeysDown(), held = hidKeysHeld(), up = hidKeysUp();
    touchPosition t; hidTouchRead(&t);
    frame++;

    if(app == ST_TITLE){
      if(down & KEY_RIGHT){ sel = (sel + 1) % MX_NUM_LEVELS; load_level(sel); }
      if(down & KEY_LEFT){ sel = (sel + MX_NUM_LEVELS - 1) % MX_NUM_LEVELS; load_level(sel); }
      if(down & (KEY_L | KEY_R)){ int dir = (down & KEY_R) ? 1 : MX_NUM_OUTFITS - 1; for(int k = 1; k <= MX_NUM_OUTFITS; k++){ int n = (outfit_sel + dir * k) % MX_NUM_OUTFITS; if(mx_outfit_unlocked(&stats, n)){ outfit_sel = n; break; } } }
      if(down & (KEY_A | KEY_START | KEY_TOUCH)){ app = ST_PLAY; load_level(sel); }
    } else if(app == ST_PAUSE){
      if(down & KEY_START) app = ST_PLAY;
      if(down & KEY_SELECT){ load_level(world.level); app = ST_PLAY; }
      if(down & KEY_B){ app = ST_TITLE; sel = world.level; load_level(sel); }
    } else {
      MxMan *m = &world.man;
      if((down & KEY_START) && world.state == MX_PLAY) app = ST_PAUSE;
      if(down & KEY_SELECT) load_level(world.level);
      if((down & KEY_X) && m->mode == MAN_AIM){ int nx = world.pw_sel; for(int k = 0; k < 4; k++){ nx = (nx + 1) % 4; if(nx == 0 || world.pw_left[nx] > 0) break; }
        if(nx == 0 && world.pw_sel) mx_select_power(&world, world.pw_sel); else if(nx) mx_select_power(&world, nx); }
      if((down & KEY_B) && world.state != MX_PLAY){ app = ST_TITLE; sel = world.level; load_level(sel); }       /* B on the end screen = back to the title */
      if(world.state != MX_PLAY){
        if(down & (KEY_A | KEY_START | KEY_TOUCH)){
          int next = world.state == MX_WIN ? world.level + 1 : world.level;
          if(next >= MX_NUM_LEVELS){ app = ST_TITLE; sel = 0; load_level(0); } else { sel = next; load_level(next); }
        }
      } else if(m->mode == MAN_AIM){
        world.valve_ok = mx_valve_unlocked(&stats);
        if((down & KEY_R) && mx_valve_seat(&world)) pad_used = 0;       /* R: sit on the air valve (beat 3 levels) */
        if(down & KEY_TOUCH) dragging = 1;
        if((held & KEY_TOUCH) && dragging){
          float cx = (float)t.px / 4.0f + vb.cx, cy = (float)t.py / 4.0f + vb.cy;
          mx_set_pull(&world, R(1) * (cx - RTOF(MX_ANCHOR_X)), R(1) * (cy - RTOF(MX_ANCHOR_Y)));
        }
        if((up & KEY_TOUCH) && dragging){ dragging = 0; mx_release(&world); }
        if(!dragging && !pad_used && m->seated){}                      /* seated on the valve: leave him there until he is dragged off */
        if(!dragging && (held & (KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT | KEY_CPAD_UP | KEY_CPAD_DOWN))){   /* same pad aiming as the DS */
          if(!pad_used){ pad_dx = -6.0f; pad_dy = 3.0f; pad_used = 1; }
          if(held & (KEY_DLEFT | KEY_CPAD_LEFT)) pad_dx -= .3f;
          if(held & (KEY_DRIGHT | KEY_CPAD_RIGHT)) pad_dx += .3f;
          if(held & (KEY_DUP | KEY_CPAD_UP)) pad_dy -= .3f;
          if(held & (KEY_DDOWN | KEY_CPAD_DOWN)) pad_dy += .3f;
          if(pad_dx < -12) pad_dx = -12; if(pad_dx > 2) pad_dx = 2; if(pad_dy < -8) pad_dy = -8; if(pad_dy > 12) pad_dy = 12;
          mx_set_pull(&world, R(1) * pad_dx, R(1) * pad_dy);
        }
        if((down & KEY_A) && pad_used){ pad_used = 0; mx_release(&world); }
      } else if(m->mode == MAN_FLY){
        if(down & (KEY_A | KEY_B | KEY_TOUCH)) mx_dive(&world);
      }
      if(app == ST_PLAY){        /* idle gags: only while he waits in the sling and nobody is touching anything */
        if(world.man.mode == MAN_AIM && !dragging && !pad_used && world.banner <= 0 && world.state == MX_PLAY && !(down | held)){
          mi_update(&mi); if(mi.fx) idle_fx(mi.fx, RTOF(world.man.x), RTOF(world.man.y));
        } else mi_reset(&mi);
      }
      if(app == ST_PLAY) mx_update(&world, R(1.0 / 60.0));
      on_events();
      mx_meta_events(&stats, &world);
      for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a);
      if(toast_t > 0){ if(++toast_t > 150){ toast_t = 0; toast_id = -1; } }
      if(toast_t == 0 && toast_nq){ toast_id = toast_q[0]; for(int i = 1; i < toast_nq; i++) toast_q[i-1] = toast_q[i]; toast_nq--; toast_t = 1; }
      tick_parts();
      if(world.state != MX_PLAY && was_state == MX_PLAY && world.state == MX_WIN){ prev_best = stats.best[world.level]; mx_meta_win(&stats, &world); new_best = world.score > 0 && (uint32_t)world.score > prev_best; for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a); }
      was_state = world.state;
    }

    /* ---- draw ---- */
    bot.fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL); bot.w = 320; bot.h = 240;
    float sl = osGet3DSliderState();                   /* the 3D slider: 0 = flat, 1 = full depth */
    gfxSet3D(sl > 0.03f);
    for(int eye = 0; eye < (sl > 0.03f ? 2 : 1); eye++){
    top.fb = gfxGetFramebuffer(GFX_TOP, eye ? GFX_RIGHT : GFX_LEFT, NULL, NULL); top.w = 400; top.h = 240;
    draw_world_3d(&vt, eye ? 1 : -1, sl);
    if(app == ST_TITLE){
      rect(&top, 70, 40, 260, 90, RGB(15, 18, 30));
      textc(&top, 200, 52, "MANDEX", 6, RGB(255, 210, 63));
      textc(&top, 200, 100, "THE LANDLORD SMASHER", 2, RGB(255, 255, 255));
      char b[48]; sprintf(b, "< LEVEL %d  %s >", sel + 1, mx_level_name(sel)); textc(&top, 200, 150, b, 2, RGB(255, 255, 255));
      rect(&top, 140, 161, 120, 9, RGB(15, 18, 30));
      if(stats.best[sel]){ sprintf(b, "BEST %06u", (unsigned)stats.best[sel]); textc(&top, 200, 163, b, 1, RGB(255, 210, 63)); } else textc(&top, 200, 163, "NOT PLAYED YET", 1, RGB(150, 150, 165));
      textc(&top, 200, 176, "A / START = PLAY", 2, RGB(125, 255, 90));
      { char ob[64]; sprintf(ob, "L/R: OUTFIT - %s   ACH %d/%d", mx_outfit(outfit_sel)->name, mx_ach_count(&stats), MX_NUM_ACH); textc(&top, 200, 200, ob, 1, RGB(255, 126, 176)); }
    } else {
      draw_hud(); draw_end();
      if(app == ST_PAUSE){ rect(&top, 110, 80, 180, 70, RGB(15, 18, 30)); textc(&top, 200, 88, "PAUSED", 3, RGB(255, 210, 63)); textc(&top, 200, 116, "START RESUME", 1, RGB(255, 255, 255)); textc(&top, 200, 126, "SELECT RESTART  B MENU", 1, RGB(255, 255, 255)); }
    }
    }
    draw_world(&vb);
    if(app == ST_PLAY){
      if(world.man.mode == MAN_AIM && !dragging && world.state == MX_PLAY) textc(&bot, 200, 12, "DRAG BACK AND LET GO", 2, RGB(255, 255, 255));
      if(world.man.mode == MAN_AIM && world.state == MX_PLAY){ static const char *const PWN[4] = { "NONE", "BEANS", "TOWEL", "SPLAT" }; char pb[48]; sprintf(pb, "X: POWER-UP %s", PWN[world.pw_sel]); textc(&bot, 160, 228, pb, 1, world.pw_sel ? RGB(255, 210, 63) : RGB(200, 200, 200)); }
      if(world.man.mode == MAN_FLY && !world.man.dive && (frame & 32)) textc(&bot, 160, 12, "TAP = BELLY FLOP", 2, RGB(255, 255, 255));
    } else textc(&bot, 160, 12, "TOUCH TO START", 2, RGB(255, 255, 255));
    gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
  }
  gfxExit();
  return 0;
}
