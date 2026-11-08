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

static MxWorld world;
enum { ST_TITLE, ST_PLAY };
static int app = ST_TITLE, sel = 0, frame, dragging, pad_used, cells0 = 1;
static float pad_dx, pad_dy;
static int best[MX_NUM_LEVELS];
static MxStats stats;                                   /* session achievements / outfits (no save on the 3DS yet) */
static int outfit_sel, toast_id = -1, toast_q[6], toast_nq, toast_t;
static void toast_push(int id){ if(toast_nq < 6) toast_q[toast_nq++] = id; }

/* ---------------------------------------------------------------- pixels */
typedef struct { u8 *fb; int w, h; } Scr;                       /* w x h in screen terms (400x240 or 320x240) */
static Scr top, bot;
static inline void px(Scr *s, int x, int y, u32 rgb){
  if((unsigned)x >= (unsigned)s->w || (unsigned)y >= (unsigned)s->h) return;
  u8 *p = s->fb + ((x * s->h) + (s->h - 1 - y)) * 3;
  p[0] = rgb & 255; p[1] = (rgb >> 8) & 255; p[2] = (rgb >> 16) & 255;   /* BGR: rgb is 0xBBGGRR so callers use RGB() */
}
#define RGB(r, g, b) ((u32)(b) << 16 | (u32)(g) << 8 | (u32)(r))
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
typedef struct { float x, y, vx, vy; int life; u32 c; } Part;
#define NP 160
static Part parts[NP];
static void spawn(float x, float y, float vx, float vy, int life, u32 c){
  for(int i = 0; i < NP; i++) if(parts[i].life <= 0){ parts[i] = (Part){x, y, vx, vy, life, c}; return; }
}
static void on_events(void){
  for(int i = 0; i < world.nev; i++){
    const MxEvent *e = &world.ev[i];
    float x = RTOF(e->x), y = RTOF(e->y);
    switch(e->type){
    case EV_CELL: { const uint8_t *c = mx_material_rgb(e->m);
      spawn(x, y, RTOF(e->a) * .3f + (rand() % 9 - 4) * .2f, RTOF(e->b) * .3f - (rand() % 7) * .2f, 30 + rand() % 20, RGB(c[0], c[1], c[2])); } break;
    case EV_BOOM: for(int k = 0; k < 24; k++) spawn(x, y, (rand() % 21 - 10) * .35f, (rand() % 21 - 10) * .35f, 30, RGB(255, 200 - rand() % 100, 40)); break;
    case EV_SPLAT: for(int k = 0; k < 12; k++) spawn(x, y, (rand() % 15 - 7) * .3f, -(rand() % 9) * .3f, 30, RGB(110, 255, 90)); break;
    case EV_TARGET: for(int k = 0; k < 14; k++) spawn(x, y, (rand() % 15 - 7) * .3f, -(rand() % 11) * .3f, 36, RGB(255, 220, 60)); break;
    default: break;
    }
  }
}
static void tick_parts(void){
  for(int i = 0; i < NP; i++) if(parts[i].life > 0){ Part *p = &parts[i]; p->vy += .06f; p->x += p->vx * .1f; p->y += p->vy * .1f; p->life--; }
}

/* ---------------------------------------------------------------- drawing the world into a view */
typedef struct { Scr *s; int ppc, ox, oy; float cx, cy; } View;   /* screen px = ox + (cell - cx) * ppc */
static inline int VX(const View *v, float c){ return v->ox + (int)floorf((c - v->cx) * v->ppc); }
static inline int VY(const View *v, float c){ return v->oy + (int)floorf((c - v->cy) * v->ppc); }

static void draw_sky(const View *v){
  Scr *s = v->s;
  for(int y = 0; y < s->h; y += 4){ int t = y * 90 / s->h; rect(s, 0, y, s->w, 4, RGB(90 + t / 2, 170 + t / 2, 240 - t / 3)); }
  rect(s, 0, VY(v, MX_ROWS), s->w, s->h, RGB(96, 64, 38));
  rect(s, 0, VY(v, MX_ROWS) - 2, s->w, 3, RGB(70, 170, 70));
}
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
      rect(v->s, VX(v, RTOF(a->wx)) - p / 2, VY(v, RTOF(a->wy)) - p / 2, p, p, RGB(col[0], col[1], col[2]));
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
  int p = v->ppc, x = VX(v, RTOF(m->x)), y = VY(v, RTOF(m->y));
  if(m->mode == MAN_STUCK){ disc(v->s, x, y, p * 2, RGB(110, 255, 90)); }
  const MxOutfit *of = mx_outfit(outfit_sel);
  disc(v->s, x, y, p * 2 - 1, RGB(244, 183, 142));                          /* body */
  if(m->inf > 0 && (m->mode == MAN_AIM || m->mode == MAN_FLY)){             /* only the belly swells */
    float f = RTOF(m->inf); int r = (int)(p * (2 + f * 3.2f)), bx = x + (int)(p * f * 1.5f);
    disc(v->s, bx, y + p / 2, r + 1, RGB(205, 135, 100)); disc(v->s, bx, y + p / 2, r, RGB(244, 183, 142));
    disc(v->s, bx - r / 3, y + p / 2 - r / 3, r / 5 + 1, RGB(255, 215, 180));
    if(f > .45f) disc(v->s, x, y - p * 2, p + 1, RGB(235, 40 + (int)(40 * (1 - f)), 50));   /* red in the face */
  }
  rect(v->s, x - p * 2 + 2, y + p / 2, p * 4 - 4, p, RGB(of->br[0], of->br[1], of->br[2]));     /* boxers */
  disc(v->s, x + (m->vx >= 0 ? p / 2 : -p / 2), y - p * 2, p, (m->inf > .45f) ? RGB(235, 90, 90) : RGB(244, 183, 142));
  rect(v->s, x - p * 2, y - p, p, p * 2, RGB(of->tow[0], of->tow[1], of->tow[2]));               /* towel */
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
  for(int i = 0; i < NP; i++) if(parts[i].life > 0) rect(v->s, VX(v, parts[i].x), VY(v, parts[i].y), 2, 2, parts[i].c);
}
static void draw_valve(const View *v){
  int p = v->ppc, x = VX(v, RTOF(MX_VALVE_X)), y = VY(v, RTOF(MX_VALVE_Y)), gy = VY(v, (float)MX_ROWS);
  const MxMan *m = &world.man; int f = m->mode == MAN_AIM ? (int)(RTOF(m->inf) * 100) : 0;
  rect(v->s, x - p * 2, gy - 2, p * 4, 3, RGB(110, 115, 130));
  rect(v->s, x - p / 2 - 1, y + p, p + 2, gy - y - p, RGB(74, 85, 104));
  rect(v->s, x - p, y, p * 2, p * 2, RGB(255, 160, 40));                         /* nozzle */
  if(m->mode == MAN_AIM && f > 0){ rect(v->s, x - p * 5, y - p * 9, p * 10, p, RGB(20, 24, 40)); rect(v->s, x - p * 5 + 1, y - p * 9 + 1, f * (p * 10 - 2) / 100, p - 2, f < 50 ? RGB(125, 255, 90) : f < 80 ? RGB(255, 210, 63) : RGB(255, 70, 70)); }
}
static void draw_world(const View *v){
  draw_sky(v); draw_cells(v); draw_landlords(v); draw_sling(v); draw_valve(v); draw_aim(v); draw_man(v); draw_parts(v);
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
static void draw_end(void){
  if(world.state == MX_PLAY) return;
  int win = world.state == MX_WIN;
  rect(&top, 90, 70, 220, 100, RGB(15, 18, 30));
  textc(&top, 200, 82, win ? "LEVEL CLEAR!" : "OUT OF MEN", 3, win ? RGB(125, 255, 90) : RGB(255, 90, 90));
  char b[32]; sprintf(b, "SCORE %d", world.score); textc(&top, 200, 118, b, 2, RGB(255, 255, 255));
  textc(&top, 200, 146, win ? "A = NEXT LEVEL" : "A = TRY AGAIN", 2, RGB(255, 210, 63));
}

/* ---------------------------------------------------------------- flow */
static void load_level(int l){
  mx_load(&world, l); mx_meta_level_start(&stats, &world);
  memset(parts, 0, sizeof parts);
  cells0 = mx_cells_left(&world) ? mx_cells_left(&world) : 1;
  dragging = 0; pad_used = 0; pad_dx = pad_dy = 0;
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
    if(down & KEY_START) break;
    touchPosition t; hidTouchRead(&t);
    frame++;

    if(app == ST_TITLE){
      if(down & (KEY_RIGHT | KEY_R)){ sel = (sel + 1) % MX_NUM_LEVELS; load_level(sel); }
      if(down & (KEY_LEFT | KEY_L)){ sel = (sel + MX_NUM_LEVELS - 1) % MX_NUM_LEVELS; load_level(sel); }
      if(down & KEY_Y){ for(int k = 1; k <= MX_NUM_OUTFITS; k++){ int n = (outfit_sel + k) % MX_NUM_OUTFITS; if(mx_outfit_unlocked(&stats, n)){ outfit_sel = n; break; } } }
      if(down & (KEY_A | KEY_TOUCH)){ app = ST_PLAY; load_level(sel); }
    } else {
      MxMan *m = &world.man;
      if(down & KEY_SELECT) load_level(world.level);
      if((down & KEY_L) && m->mode == MAN_AIM){ int nx = world.pw_sel; for(int k = 0; k < 4; k++){ nx = (nx + 1) % 4; if(nx == 0 || world.pw_left[nx] > 0) break; }
        if(nx == 0 && world.pw_sel) mx_select_power(&world, world.pw_sel); else if(nx) mx_select_power(&world, nx); }
      if(down & KEY_X){ app = ST_TITLE; sel = world.level; load_level(sel); }
      if(world.state != MX_PLAY){
        if(down & (KEY_A | KEY_TOUCH)){
          int next = world.state == MX_WIN ? world.level + 1 : world.level;
          if(next >= MX_NUM_LEVELS){ app = ST_TITLE; sel = 0; load_level(0); } else { sel = next; load_level(next); }
        }
      } else if(m->mode == MAN_AIM){
        if(down & KEY_TOUCH) dragging = 1;
        if((held & KEY_TOUCH) && dragging){
          float cx = (float)t.px / 4.0f + vb.cx, cy = (float)t.py / 4.0f + vb.cy;
          mx_set_pull(&world, R(1) * (cx - RTOF(MX_ANCHOR_X)), R(1) * (cy - RTOF(MX_ANCHOR_Y)));
        }
        if((up & KEY_TOUCH) && dragging){ dragging = 0; mx_release(&world); }
        if(!dragging && !pad_used && m->seated){}                      /* seated on the valve: leave him there until he is dragged off */
        if(held & (KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT | KEY_CPAD_UP | KEY_CPAD_DOWN)){
          if(held & (KEY_DLEFT | KEY_CPAD_LEFT)) pad_dx -= .25f;
          if(held & (KEY_DRIGHT | KEY_CPAD_RIGHT)) pad_dx += .25f;
          if(held & (KEY_DUP | KEY_CPAD_UP)) pad_dy -= .25f;
          if(held & (KEY_DDOWN | KEY_CPAD_DOWN)) pad_dy += .25f;
          pad_used = 1; mx_set_pull(&world, R(1) * pad_dx, R(1) * pad_dy);
        }
        if((down & KEY_A) && pad_used){ pad_used = 0; pad_dx = pad_dy = 0; mx_release(&world); }
      } else if(m->mode == MAN_FLY){
        if(down & (KEY_A | KEY_B | KEY_TOUCH)) mx_dive(&world);
      }
      mx_update(&world, R(1.0 / 60.0));
      on_events();
      mx_meta_events(&stats, &world);
      for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a);
      if(toast_t > 0){ if(++toast_t > 150){ toast_t = 0; toast_id = -1; } }
      if(toast_t == 0 && toast_nq){ toast_id = toast_q[0]; for(int i = 1; i < toast_nq; i++) toast_q[i-1] = toast_q[i]; toast_nq--; toast_t = 1; }
      tick_parts();
      if(world.state != MX_PLAY && was_state == MX_PLAY && world.state == MX_WIN){ mx_meta_win(&stats, &world); for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ) toast_push(a); if(world.score > best[world.level]) best[world.level] = world.score; }
      was_state = world.state;
    }

    /* ---- draw ---- */
    top.fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL); top.w = 400; top.h = 240;
    bot.fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL); bot.w = 320; bot.h = 240;
    draw_world(&vt);
    if(app == ST_TITLE){
      rect(&top, 70, 40, 260, 90, RGB(15, 18, 30));
      textc(&top, 200, 52, "MANDEX", 6, RGB(255, 210, 63));
      textc(&top, 200, 100, "THE LANDLORD SMASHER", 2, RGB(255, 255, 255));
      char b[48]; sprintf(b, "< LEVEL %d  %s >", sel + 1, mx_level_name(sel)); textc(&top, 200, 150, b, 2, RGB(255, 255, 255));
      textc(&top, 200, 176, "A / TOUCH = PLAY", 2, RGB(125, 255, 90));
      { char ob[64]; sprintf(ob, "Y: OUTFIT - %s   ACH %d/%d", mx_outfit(outfit_sel)->name, mx_ach_count(&stats), MX_NUM_ACH); textc(&top, 200, 200, ob, 1, RGB(255, 126, 176)); }
    } else {
      draw_hud(); draw_end();
    }
    draw_world(&vb);
    if(app == ST_PLAY){
      if(world.man.mode == MAN_AIM && !dragging && world.state == MX_PLAY) textc(&bot, 200, 12, "DRAG BACK AND LET GO", 2, RGB(255, 255, 255));
      if(world.man.mode == MAN_AIM && world.state == MX_PLAY){ static const char *const PWN[4] = { "NONE", "BEANS", "TOWEL", "SPLAT" }; char pb[48]; sprintf(pb, "L: POWER-UP %s", PWN[world.pw_sel]); textc(&bot, 160, 228, pb, 1, world.pw_sel ? RGB(255, 210, 63) : RGB(200, 200, 200)); }
      if(world.man.mode == MAN_FLY && !world.man.dive && (frame & 32)) textc(&bot, 160, 12, "TAP = BELLY FLOP", 2, RGB(255, 255, 255));
    } else textc(&bot, 160, 12, "TOUCH TO START", 2, RGB(255, 255, 255));
    gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
  }
  gfxExit();
  return 0;
}
