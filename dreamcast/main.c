/*
 * MANDEX - Sega Dreamcast front-end (KallistiOS + PVR).
 *
 * 640x480, 5 px per voxel, every voxel drawn as a flat-shaded PVR quad
 * (a few thousand quads is nothing for the PVR).  The game logic is the
 * shared core in ../core in its float build - the SH-4 has an FPU.
 *
 * Controls (port A controller)
 *   analog stick : pull the man back in the sling (push the direction you
 *                  want to pull him: left/down fires up and to the right)
 *   A            : release the sling / belly flop in mid-air
 *   START        : start from the title screen; next level after a win, retry after a loss
 *   Y            : restart the level
 *
 * Not done yet (all slated for the next pass): text/score font, sound via
 * AICA, and the real-3D voxel upgrade described in MANDEX_DESIGN.md.
 */
#include <kos.h>
#include "manspr.h"
#include "manidle.h"
#define MX_TITLE_DC
#include "title.h"
#include <math.h>
#include <stdlib.h>
#include "mx.h"
#include "cloth.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PXC 5.0f
#define OX  20.0f
#define OY  150.0f
#define DT  (1.0f / 60.0f)

static MxWorld world;
static pvr_poly_hdr_t hdr;

enum { Z_BG = 1, Z_SLING, Z_VOX, Z_CHUNK, Z_TARGET, Z_MAN, Z_FX, Z_HUD };
static float MAXHP[MX_NMAT];                 /* filled from the core's material table in main() */

static uint32 argb(int r, int g, int b, int a){ return ((uint32)a << 24) | ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b; }

static void quad(float x, float y, float w, float h, float z, uint32 c){
  pvr_vertex_t v;
  v.flags = PVR_CMD_VERTEX; v.z = z; v.u = v.v = 0; v.argb = c; v.oargb = 0;
  v.x = x;     v.y = y;     pvr_prim(&v, sizeof v);
  v.x = x + w;              pvr_prim(&v, sizeof v);
  v.x = x;     v.y = y + h; pvr_prim(&v, sizeof v);
  v.flags = PVR_CMD_VERTEX_EOL; v.x = x + w; pvr_prim(&v, sizeof v);
}
static void disc(float cx, float cy, float r, float z, uint32 c){
  for(float dy = -r; dy <= r; dy += 2.0f){ float hw = sqrtf(r * r - dy * dy); quad(cx - hw, cy + dy, hw * 2, 2.0f, z, c); }
}
/* a quad with four arbitrary corners (a b c d = a triangle strip), for the towel */
static void quad4(const float *x, const float *y, float z, uint32 c){
  pvr_vertex_t v;
  v.flags = PVR_CMD_VERTEX; v.z = z; v.u = v.v = 0; v.argb = c; v.oargb = 0;
  for(int k = 0; k < 3; k++){ v.x = x[k]; v.y = y[k]; pvr_prim(&v, sizeof v); }
  v.flags = PVR_CMD_VERTEX_EOL; v.x = x[3]; v.y = y[3]; pvr_prim(&v, sizeof v);
}
static void seg(float x0, float y0, float x1, float y1, float w, float z, uint32 c){      /* a line w px wide */
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  if(l < 0.01f) return;
  float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
  float X4[4] = { x0 + nx, x1 + nx, x0 - nx, x1 - nx }, Y4[4] = { y0 + ny, y1 + ny, y0 - ny, y1 - ny };
  quad4(X4, Y4, z, c);
}
static float X(float cell){ return OX + cell * PXC; }
static float Y(float cell){ return OY + cell * PXC; }

/* ---- tiny particle pool driven by core events and the idle gags ---- */
enum { PK_BIT, PK_STINK, PK_FLY, PK_ZZZ, PK_SWEAT };
typedef struct { float x, y, vx, vy, life, max; uint32 c; int kind; } Part;
static Part parts[256];
static float frand(void){ return (float)rand() / (float)RAND_MAX; }
static void spawn_k(int kind, float x, float y, float vx, float vy, float life, uint32 c){      /* x,y in cells, vx,vy in px/s */
  for(int i = 0; i < 256; i++) if(parts[i].life <= 0){
    parts[i].x = X(x); parts[i].y = Y(y); parts[i].vx = vx; parts[i].vy = vy; parts[i].life = parts[i].max = life; parts[i].c = c; parts[i].kind = kind; return;
  }
}
static void spawn(float x, float y, uint32 c, float spd){
  spawn_k(PK_BIT, x, y, (frand() - 0.5f) * spd, -frand() * spd, 0.7f + frand() * 0.8f, c);
}

static void handle_events(void){
  for(int i = 0; i < world.nev; i++){
    const MxEvent *e = &world.ev[i];
    switch(e->type){
    case EV_CELL: { const uint8_t *m = mx_material_rgb(e->m);
      spawn(e->x, e->y, argb(m[0], m[1], m[2], 255), 260); spawn(e->x, e->y, argb(m[0] / 2, m[1] / 2, m[2] / 2, 255), 220); break; }
    case EV_BOOM:   for(int k = 0; k < 30; k++) spawn(e->x, e->y, k & 1 ? argb(255, 160, 40, 255) : argb(255, 220, 90, 255), 600); break;
    case EV_SPLAT:  for(int k = 0; k < 20; k++) spawn(e->x, e->y, argb(125, 255, 90, 255), 380); break;
    case EV_CRACK:  for(int k = 0; k < 3; k++) spawn(e->x, e->y, e->m == MX_GLASS ? argb(235, 250, 255, 255) : argb(200, 190, 170, 255), 120);
                    break;                                          /* TODO: creak sound on the AICA */
    case EV_BOUNCE: for(int k = 0; k < 10; k++) spawn(e->x, e->y, k & 1 ? argb(255, 126, 176, 255) : argb(255, 255, 255, 255), 420); break;
    case EV_TARGET: for(int k = 0; k < 14; k++) spawn(e->x, e->y, argb(255, 255, 255, 255), 360); break;
    default: break;   /* TODO: AICA sound for EV_CRUNCH / EV_LAUNCH / EV_THUD / ... */
    }
  }
}


/* belly / rear jiggle, in sprite pixels (the core keeps them in cells; the art is 4 px per cell), plus squash factors */
typedef struct { float bx, by, rx, ry, sx, sy; } Jig;
static Jig man_jig(void){
  const MxMan *m = &world.man; Jig j = { m->bx * 4, m->by * 4, m->rx * 4, m->ry * 4, 1, 1 };
  if(m->sq > 0.04f){
    float q = m->sq * 0.12f;                                             /* up to 12 % squash along the impact direction */
    if(fabsf(m->sqx) > fabsf(m->sqy)){ j.sx = 1 - q; j.sy = 1 + q * 0.5f; } else { j.sy = 1 - q; j.sx = 1 + q * 0.5f; }
  }
  return j;
}
static float blob(float dx, float dy, float rad){ float d2 = dx * dx + dy * dy, r2 = rad * rad; if(rad < 2) r2 = 4; return d2 < r2 ? 1.0f - d2 / r2 : 0.0f; }

/* one frame of the man: pixel size is PXC/4 because the art is authored at 4 px per cell.  One quad per horizontal run;
   with a Jig, runs are cut short and each piece is moved by the belly / rear springs, rows are squashed about the anchor. */
static void draw_frame(const MsFrame *f, float cx, float cy, int face, float z, const Jig *jg){
  float s = PXC / 4.0f;
  int live = jg && (jg->bx != 0 || jg->by != 0 || jg->rx != 0 || jg->ry != 0 || jg->sx != 1 || jg->sy != 1);
  float sx = live ? jg->sx : 1, sy = live ? jg->sy : 1;
  int axd = face > 0 ? f->ax : f->w - 1 - f->ax;
  int bcx = face > 0 ? f->bcx : f->w - 1 - f->bcx, rcx = face > 0 ? f->rcx : f->w - 1 - f->rcx;
  int maxrun = live ? 5 : 1000;
  for(int j = 0; j < f->h; j++){
    int i = 0;
    while(i < f->w){
      int srcx = face > 0 ? i : f->w - 1 - i, c = f->px[j * f->w + srcx];
      int k = i + 1;
      while(k < f->w && k - i < maxrun && f->px[j * f->w + (face > 0 ? k : f->w - 1 - k)] == c) k++;
      if(c){
        float ox = 0, oy = 0;
        if(live){
          float mx = (i + k) * 0.5f, wb = blob(mx - bcx, j - f->bcy, (float)f->brad), wr = blob(mx - rcx, j - f->rcy, (float)f->rrad);
          ox = (jg->bx * wb + jg->rx * wr) * s; oy = (jg->by * wb + jg->ry * wr) * s;
        }
        float qx = cx + ((i - axd) * sx) * s + ox, qy = cy + ((j - f->ay) * sy) * s + oy;
        quad(qx, qy, (k - i) * s * sx + 0.4f, s * sy + 0.4f, z, argb(MS_PAL[c - 1][0], MS_PAL[c - 1][1], MS_PAL[c - 1][2], 255));
      }
      i = k;
    }
  }
}

/* the towel of the man whose centre is at cx,cy (core/cloth.c): pass 0 = the part behind him, 1 = the part in front.  The sprites are drawn at
   0.36 px per design unit at 4 px per cell, so a design unit is 0.36 * PXC / 4 screen pixels. */
static void draw_cloth(const Cloth *cl, float cx, float cy, int face, float z, int pass){
  ClothQuad q[CL_NQ];
  int n = cloth_quads(cl, q);
  float s = 0.36f * PXC / 4.0f;
  uint32 ol = argb(MS_PAL[MS_CL_OUT - 1][0], MS_PAL[MS_CL_OUT - 1][1], MS_PAL[MS_CL_OUT - 1][2], 255);
  for(int k = 0; k < n; k++){
    const ClothQuad *o = &q[k];
    if((o->z >= 0) != (pass != 0)) continue;
    float qx[4], qy[4];
    for(int m = 0; m < 4; m++){ qx[m] = cx + face * o->x[m] * s; qy[m] = cy + o->y[m] * s; }
    int v = MS_CL_COL[o->face][o->stripe][o->shade] - 1;
    quad4(qx, qy, z, argb(MS_PAL[v][0], MS_PAL[v][1], MS_PAL[v][2], 255));
    float w = s * 1.1f + 0.6f, zo = z + 0.05f;
    if(o->edge & 1) seg(qx[0], qy[0], qx[2], qy[2], w, zo, ol);
    if(o->edge & 2) seg(qx[1], qy[1], qx[3], qy[3], w, zo, ol);
    if(o->edge & 4) seg(qx[0], qy[0], qx[1], qy[1], w, zo, ol);
    if(o->edge & 8) seg(qx[2], qy[2], qx[3], qy[3], w, zo, ol);
  }
}

/* ---- idle gags (art/manidle.h): sound is not implemented on the Dreamcast port yet, so the gags are visual only ---- */
static MiState mi;
static int fcount;
static ClothActor tow_play, tow_win[2];          /* towels: the man in the sling, the two victory dancers */
/* a man frame with his towel: the cloth behind him, the sprite, the cloth in front */
static void man_with_towel(ClothActor *ta, int fr, float x, float y, int face, float z, const Jig *jg){
  cloth_actor_frame(ta, fcount, MS_HAND[fr], face);
  if(ta->live) draw_cloth(&ta->c, x, y, face, z - 0.5f, 0);
  draw_frame(&MS_FRAMES[fr], x, y, face, z, jg);
  if(ta->live) draw_cloth(&ta->c, x, y, face, z + 0.5f, 1);
}
static void idle_fx(int fx, float wx, float wy, int face){
  float mxp = wx + face * 1.3f, myp = wy - 3.6f;
  switch(fx){
  case FX_SLAP:  spawn_k(PK_SWEAT, wx - 2, wy - 4, -40, -80, 0.5f, argb(190, 230, 255, 255)); spawn_k(PK_SWEAT, wx + 2, wy - 4, 40, -80, 0.5f, argb(190, 230, 255, 255)); break;
  case FX_GRUNT: spawn_k(PK_SWEAT, wx + 2, wy - 4, 30, -90, 0.5f, argb(190, 230, 255, 255)); break;
  case FX_BURP:  for(int k = 0; k < 6; k++) spawn_k(PK_STINK, mxp, myp, face * (50 + frand() * 70), (frand() - 0.5f) * 40, 1.1f + frand() * 0.5f, argb(140, 225, 80, 255)); break;
  case FX_FART:
    for(int k = 0; k < 9; k++) spawn_k(PK_STINK, wx + (frand() - 0.5f) * 3, wy + 1.5f, (frand() - 0.5f) * 110, -frand() * 50, 1.4f + frand() * 0.7f, argb(130, 215, 70, 255));
    for(int k = 0; k < 3; k++) spawn_k(PK_FLY, wx + (frand() - 0.5f) * 3, wy - frand() * 3, 0, 0, 2.5f, argb(20, 20, 20, 255));
    break;
  case FX_FLICK: spawn_k(PK_BIT, mxp, myp, face * (140 + frand() * 60), -(60 + frand() * 50), 1.2f, argb(125, 255, 90, 255)); break;
  case FX_WHOOSH: for(int k = 0; k < 6; k++) spawn_k(PK_BIT, wx + (frand() - 0.5f) * 8, wy - frand() * 5, (frand() - 0.5f) * 160, (frand() - 0.5f) * 80, 0.5f, argb(255, 160, 205, 255)); break;
  case FX_TAUNT: for(int k = 0; k < 4; k++) spawn_k(PK_BIT, wx + (frand() - 0.5f) * 5, wy + 2, (frand() - 0.5f) * 90, -frand() * 120, 0.6f, argb(255, 255, 255, 255)); break;
  case FX_EEK:   for(int k = 0; k < 4; k++) spawn_k(PK_SWEAT, wx + (k & 1 ? 2 : -2), wy - 5, (k & 1 ? 1 : -1) * (30 + 15 * k), -90, 0.6f, argb(190, 230, 255, 255)); break;
  case FX_SNAP:  for(int k = 0; k < 4; k++) spawn_k(PK_BIT, wx + (frand() - 0.5f) * 5, wy - 3, (frand() - 0.5f) * 120, -frand() * 90, 0.5f, argb(255, 255, 210, 60)); break;
  case FX_WHEW:  spawn_k(PK_SWEAT, wx + 3, wy - 6, 30, -60, 0.8f, argb(200, 190, 232, 255)); break;
  case FX_SNORE: spawn_k(PK_ZZZ, wx + face * 3, wy - 5, 20 * face, -45, 1.4f, argb(255, 255, 255, 255)); break;
  default: break;
  }
}

/* one 60 Hz tick of everything cosmetic: idle gags, trails, particles */
static void fx_step(int calm, int strain){
  fcount++;
  if(calm){ mi_update(&mi); if(mi.fx) idle_fx(mi.fx, world.man.x, world.man.y, 1); } else mi_reset(&mi);     /* gags run only while he waits, hands off */
  if(world.state == MX_PLAY && world.man.mode == MAN_FLY && fcount % 10 == 0)                                  /* stink trail */
    spawn_k(PK_STINK, world.man.x - (world.man.vx > 0 ? 2 : -2), world.man.y + 1, (frand() - 0.5f) * 30, (frand() - 0.5f) * 20, 0.7f + frand() * 0.4f, argb(135, 220, 75, 255));
  if(strain && fcount % 7 == 0)                                                                                 /* straining in the sling */
    spawn_k(PK_SWEAT, world.man.x + (frand() - 0.5f) * 3, world.man.y - 4, (frand() - 0.5f) * 60, -frand() * 70, 0.5f, argb(190, 230, 255, 255));
  for(int i = 0; i < 256; i++) if(parts[i].life > 0){
    Part *p = &parts[i]; p->life -= DT; p->x += p->vx * DT; p->y += p->vy * DT;
    switch(p->kind){
    case PK_STINK: p->vx *= 0.97f; p->vy = p->vy * 0.97f - 18.0f * DT; break;      /* drifts up, never falls */
    case PK_FLY:   p->vx *= 0.9f;  p->vy *= 0.9f; break;
    case PK_ZZZ:   p->vx *= 0.98f; break;
    case PK_SWEAT: p->vy += 700.0f * DT; if(p->y > 452) p->life = 0; break;
    default:       p->vy += 900.0f * DT; if(p->y > 452){ p->y = 452; p->vy *= -0.3f; p->vx *= 0.6f; }
    }
  }
}

/* The title: the whole landscape with the man dropping butt-first on a landlord (art/title.h, made by tools/gen_title.py from the HTML title, logo included).
   Flat quads at 2x like everything else here.  The picture is wider than the screen, so the screen pans across it: hold on the logo, glide right to the man,
   hold, glide back.  PRESS START is a second small set of quads that blinks. */
static int on_title = 1, title_t;
static int title_pan(int t, int range){                 /* 0..range for tick t: hold (90), glide right (160), hold (150), glide back (160), smoothstep */
  t %= 560;
  int k = t < 90 ? 0 : t < 250 ? t - 90 : t < 400 ? 160 : t < 560 ? 160 - (t - 400) : 0;
  int u = (k * 256) / 160;
  return (range * ((u * u * (3 * 256 - 2 * u)) >> 16)) >> 8;
}
static void draw_title(void){
  int pan = title_pan(title_t, TITLE_DC_W * 2 - 640);   /* whole pixels, so neighbouring quads never leave a seam */
  for(int i = 0; i < TITLE_DC_NRECT; i++){
    const unsigned short *r = TITLE_DC_RECT[i]; const unsigned char *c = &TITLE_DC_PAL[r[4] * 3];
    int x = r[0] * 2 - pan, w = r[2] * 2;
    if(x + w <= 0 || x >= 640) continue;
    quad((float)x, r[1] * 2.0f, (float)w, r[3] * 2.0f, Z_BG, argb(c[0], c[1], c[2], 255));
  }
  if((title_t >> 5) & 1) return;
  for(int i = 0; i < TITLE_START_NRECT; i++){
    const unsigned short *r = TITLE_START_RECT[i];
    quad(TITLE_START_X + (float)r[0], TITLE_START_Y + (float)r[1], r[2], r[3], Z_HUD, r[4] == 1 ? argb(0, 0, 0, 255) : argb(255, 255, 255, 255));
  }
}

static void draw_all(void){
  /* sky + ground */
  quad(0, 0, 640, 140, Z_BG, argb(70, 140, 225, 255));
  quad(0, 140, 640, 310, Z_BG, argb(150, 205, 245, 255));
  quad(0, 450, 640, 8, Z_BG, argb(90, 180, 70, 255));
  quad(0, 458, 640, 22, Z_BG, argb(110, 76, 48, 255));
  /* sling */
  quad(X(15.8f), Y(MX_ANCHOR_Y + 5), 8, 450 - Y(MX_ANCHOR_Y + 5), Z_SLING, argb(122, 82, 48, 255));
  quad(X(14.6f), Y(MX_ANCHOR_Y + 5), 30, 6, Z_SLING, argb(122, 82, 48, 255));
  /* voxels: each drawn where the wobble lattice has pushed it; a snapped joint shows as a dark crack */
  for(int y = 0; y < MX_ROWS; y++) for(int x = 0; x < MX_COLS; x++){
    int i = y * MX_COLS + x, m = world.grid[i];
    if(!m) continue;
    const uint8_t *c = mx_material_rgb(m);
    float r = world.hp[i] / MAXHP[m], k = r > 0.66f ? 1.0f : r > 0.33f ? 0.75f : 0.5f;
    float px = OX + x * PXC, py = OY + y * PXC;
    if(world.wob_on){ px += world.ox[i] * PXC; py += world.oy[i] * PXC; }
    quad(px, py, PXC, PXC, Z_VOX, argb((int)(c[0] * k), (int)(c[1] * k), (int)(c[2] * k), 255));
    if(world.brk[i]) quad(px + PXC * 0.2f, py + PXC * 0.45f, PXC * 0.6f, 1.5f, Z_VOX + 0.1f, argb((int)(c[0] * 0.3f), (int)(c[1] * 0.3f), (int)(c[2] * 0.3f), 255));
  }
  /* rigid chunks: each voxel at its simulated world position */
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    const MxChunk *ch = &world.chunks[c];
    if(!ch->active) continue;
    for(int k = 0; k < ch->count; k++){
      const MxCell *a = &ch->cells[k];
      if(a->dead) continue;
      const uint8_t *col = mx_material_rgb(a->m);
      quad(X(a->wx) - PXC * 0.5f, Y(a->wy) - PXC * 0.5f, PXC, PXC, Z_CHUNK, argb(col[0], col[1], col[2], 255));
    }
  }
  /* landlords */
  for(int i = 0; i < world.ntargets; i++){
    const MxTarget *t = &world.targets[i];
    if(!t->alive) continue;
    float wx, wy; mx_target_offset(&world, t, &wx, &wy);
    float x = X(t->x + wx), y = Y(t->y + wy);
    quad(x - 8, y - 4, 16, 18, Z_TARGET, argb(74, 85, 104, 255));        /* suit */
    disc(x, y - 10, 7, Z_TARGET, argb(242, 199, 161, 255));              /* head */
    quad(x - 8, y - 20, 16, 3, Z_TARGET, argb(17, 17, 17, 255));         /* hat brim */
    quad(x - 5, y - 30, 10, 10, Z_TARGET, argb(17, 17, 17, 255));        /* hat */
    quad(x - 1, y - 2, 3, 10, Z_TARGET, argb(210, 40, 40, 255));         /* tie */
  }
  /* the man: the shared pixel-art frames (art/manspr.h, built from the HTML prototype), one quad per horizontal run */
  const MxMan *m = &world.man;
  if(m->mode != MAN_NONE && m->mode != MAN_WAIT){
    float x = X(m->x), y = Y(m->y);
    int ph = (int)(world.time * 60.0f), face = 1, fr = (ph >> 4) & 1 ? MSF_IDLE_A : MSF_IDLE_B;
    Jig jg = man_jig();
    if(m->mode == MAN_AIM){
      float dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
      if(dx * dx + dy * dy > 4.0f) fr = MSF_DRAG;                         /* pulled back: yelling */
      else fr = mi_frame(&mi, ph);                                        /* waiting: idle gags */
      quad(X(MX_ANCHOR_X) + 6, Y(MX_ANCHOR_Y) + 36, x - (X(MX_ANCHOR_X) + 6) + 4, 3, Z_SLING, argb(40, 22, 10, 255));
    }
    else if(m->mode == MAN_STUCK){ disc(x, y + 6, 26, Z_MAN, argb(111, 227, 74, 255)); fr = ((ph >> 3) & 1) ? MSF_STUCK_A : MSF_STUCK_B; }
    else if(m->mode == MAN_FLY){ face = m->vx >= 0 ? 1 : -1; fr = m->dive ? MSF_DIVE : ((ph >> 2) & 1 ? MSF_FLY_A : MSF_FLY_B); }
    man_with_towel(&tow_play, fr, x, y, face, Z_MAN, &jg);
  }
  for(int i = 0; i < 256; i++) if(parts[i].life > 0){
    const Part *p = &parts[i]; float age = 1.0f - p->life / p->max;
    switch(p->kind){
    case PK_STINK: {                                                      /* green cloud: swells, then thins out */
      float r = 6 + age * 20; int a = (int)(150 * (1.0f - age * age));
      disc(p->x, p->y, r, Z_FX, argb((p->c >> 16) & 255, (p->c >> 8) & 255, p->c & 255, a > 20 ? a : 20));
      break; }
    case PK_FLY: { float t = world.time * 18.0f + i; quad(p->x + sinf(t * 1.3f) * 9, p->y + cosf(t) * 7, 3, 3, Z_FX, p->c); break; }
    case PK_ZZZ: { float z = 9; uint32 w = p->c;                           /* a little Z from three bars */
      quad(p->x, p->y, z, 2, Z_FX, w); quad(p->x + z - 2, p->y + 2, 2, z - 4, Z_FX, w); quad(p->x, p->y + z - 2, z, 2, Z_FX, w); break; }
    case PK_SWEAT: quad(p->x, p->y, 3, 5, Z_FX, p->c); break;
    default: quad(p->x, p->y, 4, 4, Z_FX, p->c);
    }
  }
  /* HUD: men left as little red-brief icons, landlords left as dark squares */
  quad(0, 0, 640, 26, Z_HUD, argb(15, 18, 30, 200));
  for(int i = 0; i < world.men; i++){ quad(14 + i * 26, 6, 18, 14, Z_HUD + 1, argb(244, 183, 142, 255)); quad(14 + i * 26, 14, 18, 6, Z_HUD + 1, argb(224, 36, 58, 255)); }
  for(int i = 0; i < world.ntargets; i++) quad(620 - i * 24, 6, 16, 16, Z_HUD + 1, world.targets[i].alive ? argb(255, 90, 90, 255) : argb(60, 70, 60, 255));
  if(world.state == MX_WIN){                                              /* victory: two men doing the butt-wiggle dance */
    quad(120, 200, 400, 80, Z_HUD + 1, argb(60, 220, 80, 230));
    man_with_towel(&tow_win[0], mi_dance_frame(fcount), 180, 252, 1, Z_HUD + 2, 0);
    man_with_towel(&tow_win[1], mi_dance_frame(fcount + 14), 460, 252, -1, Z_HUD + 2, 0);
  }
  if(world.state == MX_LOSE){
    quad(120, 200, 400, 80, Z_HUD + 1, argb(220, 60, 60, 230));
    draw_frame(&MS_FRAMES[MSF_SAD], 320, 252, 1, Z_HUD + 2, 0);
  }
  if(world.flash > 0) quad(0, 0, 640, 480, Z_HUD + 2, argb(255, 240, 200, (int)(world.flash * 400)));
}

int main(int argc, char **argv){
  (void)argc; (void)argv;
  pvr_init_defaults();
  pvr_poly_cxt_t cxt;
  pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);          /* translucent list so HUD/flash alpha works */
  cxt.gen.culling = PVR_CULLING_NONE;
  pvr_poly_compile(&hdr, &cxt);

  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? (float)mx_material_hp(k) : 1.0f;
  mx_load(&world, 0);
  uint32 prev = 0;
  while(1){
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    if(st){
      uint32 pressed = st->buttons & ~prev; prev = st->buttons;
      if(on_title){
        if(pressed & (CONT_START | CONT_A)) on_title = 0;
      } else if(world.state != MX_PLAY){
        if(pressed & CONT_START) mx_load(&world, world.state == MX_WIN ? world.level + 1 : world.level);
      } else {
        if(pressed & CONT_Y) mx_load(&world, world.level);
        if(world.man.mode == MAN_AIM){
          float jx = st->joyx / 127.0f, jy = st->joyy / 127.0f;
          if(fabsf(jx) < 0.12f) jx = 0;
          if(fabsf(jy) < 0.12f) jy = 0;
          mx_set_pull(&world, jx * MX_PULL_MAX, jy * MX_PULL_MAX);
          if(pressed & CONT_A) mx_release(&world);
        } else if(pressed & CONT_A) mx_dive(&world);
      }
    }
    if(on_title){ title_t++; }
    else {
    mx_update(&world, DT);
    handle_events();
    {
      int calm = st && world.state == MX_PLAY && world.man.mode == MAN_AIM && world.banner <= 0 && !st->buttons && abs(st->joyx) < 30 && abs(st->joyy) < 30;
      int strain = st && world.man.mode == MAN_AIM && (abs(st->joyx) > 90 || abs(st->joyy) > 90);
      fx_step(calm, strain);
    }
    }
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_prim(&hdr, sizeof hdr);
    if(on_title) draw_title(); else draw_all();
    pvr_list_finish();
    pvr_scene_finish();
  }
  return 0;
}
