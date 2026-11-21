/*
 * MANDEX - Sega Dreamcast front-end (KallistiOS + PVR).
 *
 * 640x480, 5 px per voxel.  Everything is drawn as textured PVR quads from art baked out of the HTML prototype (tools/gen_dc_art.py -> art/dcart.h):
 * the sky and the hills, a tile for every building block (material x variant x damage), smooth high-resolution man and landlord sprites, a real font
 * for the HUD and the menus, and the title picture.  The game logic is the shared core in ../core in its float build - the SH-4 has an FPU.
 *
 * Controls (port A controller)
 *   analog stick : pull the man back in the sling (push the direction you
 *                  want to pull him: left/down fires up and to the right)
 *   A or B       : release the sling / belly flop in mid-air
 *   X            : cycle the power-up
 *   START        : start from the title; pause / resume; next level after a win, retry after a loss
 *   Y            : restart the level (the DS and 3DS use SELECT)
 *   title        : D-pad left/right = level, triggers = outfit
 *
 * Not done yet: sound via AICA, and the real-3D voxel upgrade described in MANDEX_DESIGN.md.
 * Texture memory: ~3.8 MB (the title picture, 1 MB of it, is freed when the game starts).
 */
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include "manspr.h"
#include "manidle.h"
#define MX_DCART
#include "dcart.h"
#include <math.h>
#include <stdlib.h>
#include "mx.h"
#include "mx_fps_meter.h"
#include "mx_meta.h"
#include "cloth.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PXC 5.0f
#define OX  20.0f
#define OY  150.0f
#define DT  (1.0f / 60.0f)

static MxWorld world;
static int dc_valve_equipped; /* selected explicitly; hidden during ordinary shots */

/* layers: bigger z is nearer */
#define Z_BG     1.00f
#define Z_CLOUD  1.10f
#define Z_BGB    1.20f
#define Z_ROOM   1.30f
#define Z_SLING  1.40f
#define Z_VOX    2.00f
#define Z_CHUNK  2.10f
#define Z_TARGET 2.20f
#define Z_MAN    3.00f
#define Z_FX     4.00f
#define Z_HUD    5.00f
#define Z_END    6.00f
static int hint_t;                            /* ticks left of the 'pull back' hint */
static int bank_toast_t;                      /* rubber rebound trick shot HUD message */
static int survivor_toast_t;                  /* rare catastrophic collapse escape */
static float MAXHP[MX_NMAT];                 /* filled from the core's material table in main() */

static uint32 argb(int r, int g, int b, int a){ return ((uint32)a << 24) | ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b; }
#define WHITE argb(255, 255, 255, 255)

/* ---- the cutscenes (ds/source/intro.c, shared with the DS and 3DS): drawn into two 256x192 palette buffers, expanded to RGB565 and shown as two textured quads ---- */
#include "intro.h"
#include "sfx.h"
#include "mx_snd.h"
extern unsigned short gfx_pal[256]; void ds_gfx_init(void);       /* ds/source/gfx.c (built with -DMX_FIXED -Dgfx_init=ds_gfx_init as its own unit: only its palette is used here) */
/* Sound: each effect is synthesised once at start (core/mx_snd.c, the DS's own effect table), wrapped in a WAV header in memory and handed to KOS's
   sound-effect player, which copies it into the AICA's RAM.  If the sound system will not start the game is simply silent. */
static sfxhnd_t snd_h[SFX_COUNT], mus_h; static int snd_ok, snd_cool[SFX_COUNT], mus_ch = -1, mus_pref = 1; static uint64 mus_t0;
static void le32(unsigned char *p, unsigned v){ p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void sound_init(void){
  if(snd_stream_init() < 0) return;
  /* Dreamcast SH-4 is little-endian: synthesize PCM directly into the WAV data
     area (44 bytes from a 4-byte aligned base).  No duplicate PCM BSS. */
  static unsigned char wav[44 + MX_SND_MAX * 2] __attribute__((aligned(4)));
  for(int i = 0; i < SFX_COUNT; i++){
    int n = mx_snd_render(i, (short *)(void *)(wav + 44), MX_SND_MAX);
    memcpy(wav, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0\1\0\0\0\0\0\0\0\0\0\2\0\x10\0data\0\0\0\0", 44);
    le32(wav + 4, 36 + n * 2); le32(wav + 24, MX_SND_RATE); le32(wav + 28, MX_SND_RATE * 2); le32(wav + 40, n * 2);
    snd_h[i] = snd_sfx_load_buf((char *)wav);
    if(snd_h[i] == SFXHND_INVALID){ snd_ok = 0; return; }
  }
  snd_ok = 1;
  { static unsigned char mwav[44 + MX_MUS_SAMPLES * 2] __attribute__((aligned(4)));          /* the theme: one 12.8 s loop */
    int n = mx_music_render((short *)(void *)(mwav + 44), MX_MUS_SAMPLES);
    if(n){ memcpy(mwav, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0\1\0\0\0\0\0\0\0\0\0\2\0\x10\0data\0\0\0\0", 44);
      le32(mwav + 4, 36 + n * 2); le32(mwav + 24, MX_SND_RATE); le32(mwav + 28, MX_SND_RATE * 2); le32(mwav + 40, n * 2);
      mus_h = snd_sfx_load_buf((char *)mwav); } }                                                   /* if it does not fit in sound RAM there is simply no music */
}
static void music_update(int want){                       /* plays the loop while a level runs; restarted every 12.8 s (the AICA effect player does not loop) */
  uint64 now = timer_ms_gettime64();
  if(!snd_ok || mus_h == SFXHND_INVALID || mus_h == 0){ return; }
  if(want && mus_ch >= 0 && now - mus_t0 >= 12800 - 20){ mus_ch = -1; }
  if(want && mus_ch < 0){ mus_ch = snd_sfx_play(mus_h, 120, 128); mus_t0 = now; }
  else if(!want && mus_ch >= 0){ snd_sfx_stop(mus_ch); mus_ch = -1; }
}
static void sound_tick(void){ for(int i = 0; i < SFX_COUNT; i++) if(snd_cool[i] > 0) snd_cool[i]--; }
void sfx_play(int id, int pan){
  if(!snd_ok || id < 0 || id >= SFX_COUNT || snd_cool[id] > 0) return;
  snd_cool[id] = mx_snd_cooldown(id);
  snd_sfx_play(snd_h[id], 200, pan * 2);                  /* volume 0..255, pan 0..255 (128 = centre) */
}
static unsigned char ic_top[256 * 192], ic_bot[256 * 192];
static unsigned short ic_rgb[256 * 256];
static pvr_ptr_t tx_ic_top, tx_ic_bot;
static pvr_poly_hdr_t hdr_ic_top, hdr_ic_bot;
static int in_intro, intro_seen, wolf_seen, intro_level, end_seen;
#ifndef MX_DC_FAST_PALETTE
#define MX_DC_FAST_PALETTE 1
#endif
static void ic_upload(pvr_ptr_t tx, const unsigned char *src){                 /* palette index -> RGB565, 256 texels wide, 192 of 256 rows used */
#if MX_DC_FAST_PALETTE
  /* Palette conversion stays exact, including animation, but move the
     RGB15 unpacking out of the 49,152-texel inner loop on the SH-4. */
  unsigned short lut[256];
  for(int i = 0; i < 256; i++){
    unsigned c = gfx_pal[i], r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    lut[i] = (unsigned short)((r << 11) | (g << 6 << 0) | b);
  }
  for(int i = 0; i < 256 * 192; i++) ic_rgb[i] = lut[src[i]];
#else
  for(int y = 0; y < 192; y++) for(int x = 0; x < 256; x++){ unsigned c = gfx_pal[src[y * 256 + x]]; unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; ic_rgb[y * 256 + x] = (unsigned short)((r << 11) | (g << 6 << 0) | b); }
#endif
  pvr_txr_load(ic_rgb, tx, 256 * 256 * 2);
}

/* ---- textures ---- */
static pvr_ptr_t tx_bga, tx_bgb, tx_atl, tx_man, tx_ttl;
static pvr_poly_hdr_t hdr_col, hdr_bga, hdr_bgb, hdr_atl, hdr_man, hdr_ttl;
static const pvr_poly_hdr_t *cur_hdr;
static void use(const pvr_poly_hdr_t *h){ if(cur_hdr != h){ pvr_prim((void *)h, sizeof *h); cur_hdr = h; } }

static pvr_ptr_t load_tex(const unsigned short *rle, int rle_len, int w, int h){
  unsigned short *tmp = (unsigned short *)malloc((size_t)w * h * 2);
  size_t n = 0, tot = (size_t)w * h;
  for(int i = 0; i + 1 < rle_len && n < tot; i += 2){ unsigned cnt = rle[i]; unsigned short v = rle[i + 1]; while(cnt-- && n < tot) tmp[n++] = v; }
  pvr_ptr_t p = pvr_mem_malloc((size_t)w * h * 2);
  if(p) pvr_txr_load(tmp, p, (uint32)((size_t)w * h * 2));
  free(tmp);
  return p;
}
static void make_hdr(pvr_poly_hdr_t *h, int fmt, int w, int hh, pvr_ptr_t p){
  pvr_poly_cxt_t c;
  pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, fmt | PVR_TXRFMT_NONTWIDDLED, w, hh, p, PVR_FILTER_BILINEAR);
  c.gen.culling = PVR_CULLING_NONE;
  c.txr.uv_clamp = PVR_UVCLAMP_UV;
  pvr_poly_compile(h, &c);
}

/* ---- primitives ---- */
static void vtx(float x, float y, float z, float u, float v, uint32 c, int eol){
  pvr_vertex_t p;
  p.flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; p.x = x; p.y = y; p.z = z; p.u = u; p.v = v; p.argb = c; p.oargb = 0;
  pvr_prim(&p, sizeof p);
}
static void quad(float x, float y, float w, float h, float z, uint32 c){
  use(&hdr_col);
  vtx(x, y, z, 0, 0, c, 0); vtx(x + w, y, z, 0, 0, c, 0); vtx(x, y + h, z, 0, 0, c, 0); vtx(x + w, y + h, z, 0, 0, c, 1);
}
static void gquad(float x, float y, float w, float h, float z, uint32 ct, uint32 cb){      /* vertical gradient */
  use(&hdr_col);
  vtx(x, y, z, 0, 0, ct, 0); vtx(x + w, y, z, 0, 0, ct, 0); vtx(x, y + h, z, 0, 0, cb, 0); vtx(x + w, y + h, z, 0, 0, cb, 1);
}
/* a textured quad: texel rectangle (sx, sy, sw, sh) of a tw x th texture, drawn at x, y, w, h */
static void tquad(const pvr_poly_hdr_t *hd, int tw, int th, float x, float y, float w, float h, float sx, float sy, float sw, float sh, float z, uint32 c){
  use(hd);
  float u0 = sx / tw, v0 = sy / th, u1 = (sx + sw) / tw, v1 = (sy + sh) / th;
  vtx(x, y, z, u0, v0, c, 0); vtx(x + w, y, z, u1, v0, c, 0); vtx(x, y + h, z, u0, v1, c, 0); vtx(x + w, y + h, z, u1, v1, c, 1);
}
static void atl(float x, float y, float w, float h, const short *r, float z, uint32 c){ tquad(&hdr_atl, 512, 512, x, y, w, h, r[0], r[1], r[2], r[3], z, c); }
/* a quad with four arbitrary corners (a b c d = a triangle strip), for the towel */
static void quad4(const float *x, const float *y, float z, uint32 c){
  use(&hdr_col);
  for(int k = 0; k < 3; k++) vtx(x[k], y[k], z, 0, 0, c, 0);
  vtx(x[3], y[3], z, 0, 0, c, 1);
}
static void seg(float x0, float y0, float x1, float y1, float w, float z, uint32 c){      /* a line w px wide */
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  if(l < 0.01f) return;
  float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
  float X4[4] = { x0 + nx, x1 + nx, x0 - nx, x1 - nx }, Y4[4] = { y0 + ny, y1 + ny, y0 - ny, y1 - ny };
  quad4(X4, Y4, z, c);
}
static void puff(float cx, float cy, float r, float z, uint32 c){ atl(cx - r, cy - r, r * 2, r * 2, DCA_PUFF, z, c); }     /* soft round blob */

static float man_zoom = 1.0f;                           /* the end panels draw the man bigger */
static float shx, shy;                                  /* screen shake */
static float X(float cell){ return OX + cell * PXC + shx; }
static float Y(float cell){ return OY + cell * PXC + shy; }
static float XB(float cell){ return OX + cell * PXC; }
static float YB(float cell){ return OY + cell * PXC; }

/* ---- the font (art/dcart.h: ASCII 32..95, upper case; y is the baseline; align 0 left, 1 centre, 2 right) ---- */
static int glyph(int c){ if(c >= 'a' && c <= 'z') c -= 32; if(c < DCA_FONT_FIRST || c >= DCA_FONT_FIRST + DCA_FONT_N) c = '?'; return c - DCA_FONT_FIRST; }
static float text_w(const char *s, float size){ float w = 0; for(; *s; s++) w += DCA_FONT[glyph((unsigned char)*s)][2]; return w * size / DCA_FONT_SIZE; }
static void text(const char *s, float x, float y, float size, uint32 col, int align, float z){
  float sc = size / DCA_FONT_SIZE, w = text_w(s, size);
  if(align == 1) x -= w * 0.5f; else if(align == 2) x -= w;
  for(; *s; s++){
    const short *g = DCA_FONT[glyph((unsigned char)*s)];
    if(*s != ' ') tquad(&hdr_atl, 512, 512, x + (g[2] * 0.5f - 22) * sc, y - 36 * sc, 44 * sc, 48 * sc, g[0], g[1], 44, 48, z, col);
    x += g[2] * sc;
  }
}

/* ---- tiny particle pool driven by core events and the idle gags ---- */
enum { PK_BIT, PK_STINK, PK_FLY, PK_ZZZ, PK_SWEAT, PK_HEART, PK_GLINT };
typedef struct { float x, y, vx, vy, life, max; uint32 c; int kind; } Part;
static Part parts[256];
#ifndef MX_DC_PARTICLE_HINT
#define MX_DC_PARTICLE_HINT 1
#endif
#if MX_DC_PARTICLE_HINT
/* The first vacant slot. Updated on expiry so rendered particle order is
   unchanged, but an explosion no longer rescans the 256-slot pool for
   each flying spark or collapse chip. */
static int part_free_hint;
#endif
static float frand(void){ return (float)rand() / (float)RAND_MAX; }
static void spawn_k(int kind, float x, float y, float vx, float vy, float life, uint32 c){      /* x,y in cells, vx,vy in px/s */
#if MX_DC_PARTICLE_HINT
  for(int i = part_free_hint; i < 256; i++) if(parts[i].life <= 0){
    parts[i].x = XB(x); parts[i].y = YB(y); parts[i].vx = vx; parts[i].vy = vy; parts[i].life = parts[i].max = life; parts[i].c = c; parts[i].kind = kind;
    part_free_hint = i + 1; return;
  }
#else
  for(int i = 0; i < 256; i++) if(parts[i].life <= 0){
    parts[i].x = XB(x); parts[i].y = YB(y); parts[i].vx = vx; parts[i].vy = vy; parts[i].life = parts[i].max = life; parts[i].c = c; parts[i].kind = kind; return;
  }
#endif
}
static void spawn(float x, float y, uint32 c, float spd){
  spawn_k(PK_BIT, x, y, (frand() - 0.5f) * spd, -frand() * spd, 0.7f + frand() * 0.8f, c);
}

/* ---- achievements / outfits (persisted to VMU when available) ---- */
static MxStats stats;
static int outfit_sel, toast_id = -1, toast_q[6], toast_nq; static float toast_t;
static void toast_push(int id){ if(toast_nq < 6) toast_q[toast_nq++] = id; }
static unsigned outfit_seen = 1;                                   /* outfits already owned (bit per outfit): a new one pops a "NEW OUTFIT" toast, ids 100+ */
static void outfit_scan(int quiet){ for(int i = 0; i < MX_NUM_OUTFITS; i++){ if(mx_outfit_unlocked(&stats, i) && !((outfit_seen >> i) & 1)){ outfit_seen |= 1u << i; if(!quiet) toast_push(100 + i); } } }
static void toast_tick(void){
  if(toast_id >= 0){ toast_t += DT; if(toast_t > 2.8f) toast_id = -1; }
  if(toast_id < 0 && toast_nq){ toast_id = toast_q[0]; for(int i = 1; i < toast_nq; i++) toast_q[i - 1] = toast_q[i]; toast_nq--; toast_t = 0; }
}
/* the man's colours: boxers (red family) and towel (pink family) are re-tinted in the decoded atlas by hue, keeping each texel's brightness */
static int man_fur;                                                          /* the Wolfman outfit: flying frames get the wolf head */
static unsigned short *man_px;
static void recolor_man(int o){
  if(!man_px || !tx_man) return;
  uint8_t sh[6][3], sk[4][3]; mx_outfit_shades(o, sh); int fur = mx_outfit_skin(o, sk); man_fur = fur;
  size_t tot = (size_t)DCT_MAN_W * DCT_MAN_H;
  unsigned short *tmp = (unsigned short *)malloc(tot * 2);
  if(!tmp) return;
  for(size_t i = 0; i < tot; i++){
    unsigned short v = man_px[i]; unsigned a = (v >> 12) & 15, r = ((v >> 8) & 15) * 17, g = ((v >> 4) & 15) * 17, b = (v & 15) * 17;
    int mx_ = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx_ - mn;
    if(fur && a && r >= 190 && g * 100 >= 62 * (int)r && g * 100 <= 88 * (int)r && b * 100 >= 45 * (int)r && b * 100 <= 74 * (int)r && (int)(g - b) * 100 >= 11 * (int)r){   /* skin: his pink becomes grey fur, same brightness steps */
      int nr = sk[0][0] * (int)r / 244, ng = sk[0][1] * (int)r / 244, nb = sk[0][2] * (int)r / 244;
      nr = nr > 255 ? 255 : nr; ng = ng > 255 ? 255 : ng; nb = nb > 255 ? 255 : nb;
      v = (unsigned short)((a << 12) | ((nr >> 4) << 8) | ((ng >> 4) << 4) | (nb >> 4));
    } else if(a && d * 100 > 40 * mx_ && r == (unsigned)mx_ && r > 120 && b > g){      /* saturated, red-dominant and leaning to blue (not skin or hair) */
      int pink = b * 100 > 45 * (int)r;                                         /* the towel is the pinker one */
      const uint8_t *tc = pink ? sh[3] : sh[0];
      int lum = (int)r * 100 / (pink ? 255 : 224);
      int nr = tc[0] * lum / 100, ng = tc[1] * lum / 100, nb = tc[2] * lum / 100;
      nr = nr > 255 ? 255 : nr; ng = ng > 255 ? 255 : ng; nb = nb > 255 ? 255 : nb;
      v = (unsigned short)((a << 12) | ((nr >> 4) << 8) | ((ng >> 4) << 4) | (nb >> 4));
    }
    tmp[i] = v;
  }
  pvr_txr_load(tmp, tx_man, (uint32)(tot * 2));
  free(tmp);
}
static void handle_events(void){
  for(int i = 0; i < world.nev; i++){
    const MxEvent *e = &world.ev[i];
    { int sn[2], nn = mx_event_sfx(e, sn); for(int k = 0; k < nn; k++) sfx_play(sn[k], 64); }
    switch(e->type){
    case EV_CELL: { const uint8_t *m = mx_material_rgb(e->m);
      spawn(e->x, e->y, argb(m[0], m[1], m[2], 255), 260); spawn(e->x, e->y, argb(m[0] / 2, m[1] / 2, m[2] / 2, 255), 220); break; }
    case EV_BOOM:   for(int k = 0; k < 30; k++) spawn(e->x, e->y, k & 1 ? argb(255, 160, 40, 255) : argb(255, 220, 90, 255), 600); break;
    case EV_SPLAT:  for(int k = 0; k < 20; k++) spawn(e->x, e->y, argb(125, 255, 90, 255), 380); break;
    case EV_CRACK:  for(int k = 0; k < 3; k++) spawn(e->x, e->y, e->m == MX_GLASS ? argb(235, 250, 255, 255) : argb(200, 190, 170, 255), 120);
                    break;
    case EV_THUD: if(e->a > R(0.6)) for(int k = 0; k < 4; k++)
      spawn(e->x, e->y, argb(175, 165, 145, 235), 190); break;
    case EV_CRUMBLE: for(int k = 0; k < 10; k++)
      spawn(e->x, e->y, k & 1 ? argb(191, 181, 162, 240) : argb(140, 134, 127, 240), 280); break;
    case EV_BOUNCE: for(int k = 0; k < 10; k++) spawn(e->x, e->y, k & 1 ? argb(255, 126, 176, 255) : argb(255, 255, 255, 255), 420); break;
    case EV_TARGET: for(int k = 0; k < 14; k++) spawn(e->x, e->y, argb(255, 255, 255, 255), 360);
      if(e->b > 0) bank_toast_t = 105; break;
    case EV_AIR:     spawn_k(PK_SWEAT, MX_VALVE_X + (frand() - 0.5f) * 2, MX_VALVE_Y + 3, (frand() - 0.5f) * 40, -40 - frand() * 40, 0.5f, argb(220, 235, 255, 255)); break;
    case EV_AIRPOP:  for(int k = 0; k < 30; k++) spawn(e->x, e->y, k & 1 ? argb(255, 60, 60, 255) : k & 2 ? argb(255, 240, 220, 255) : argb(125, 255, 90, 255), 700); break;
    case EV_FART:    for(int k = 0; k < 10; k++) spawn_k(PK_STINK, e->x, e->y, (frand() - 0.5f) * 140, (frand() - 0.5f) * 90, 1.2f, argb(180, 225, 60, 255)); break;
    case EV_SPLOOSH: for(int k = 0; k < 40; k++) spawn(e->x, e->y, argb(125, 255, 90, 255), 760); break;
    case EV_TUBE: { const uint32 TC[3] = { argb(255, 154, 60, 255), argb(79, 208, 255, 255), argb(140, 255, 90, 255) };   /* posted through a pneumatic tube */
      for(int k = 0; k < 14; k++){ spawn(e->x, e->y, TC[e->m % 3], 300); spawn(e->a, e->b, TC[e->m % 3], 420); } break; }
    case EV_WALK:  for(int k = 0; k < 8; k++) spawn(e->x, e->y + 2.3f, argb(255, 255, 255, 255), 200); break;
    case EV_PUNCH: for(int k = 0; k < (e->a > 0 ? 12 : 3); k++) spawn(e->x, e->y, k & 1 ? argb(255, 255, 255, 255) : argb(255, 210, 63, 255), e->m ? 420 : 300); break;
    case EV_SLAM:  for(int k = 0; k < 14; k++) spawn(e->x, e->y + 2.3f, k & 1 ? argb(255, 255, 255, 255) : argb(200, 190, 170, 255), 380); break;
    case EV_PAD:   for(int k = 0; k < (e->m ? 16 : 8); k++) spawn(e->x, e->y, k & 1 ? argb(255, 126, 176, 255) : argb(255, 255, 255, 255), 460); break;
    case EV_HURT:  for(int k = 0; k < 12; k++) spawn(e->x, e->y, k & 1 ? argb(255, 74, 74, 255) : argb(255, 255, 255, 255), 420); break;
    case EV_OOF:   for(int k = 0; k < 10; k++) spawn(e->x, e->y, k & 1 ? argb(255, 210, 63, 255) : argb(255, 255, 255, 255), 520); break;
    case EV_SURVIVOR: survivor_toast_t=120;
      for(int k=0;k<6;k++)spawn(e->x,e->y,k&1?argb(255,210,63,255):argb(190,180,160,255),230);
      break;
    case EV_DISTRICT: break;                               /* Landlord HQ: next district; the banner (below) announces it */
    case EV_HEAL:  for(int k = 0; k < 8; k++) spawn(e->x, e->y, argb(255, 255, 255, 255), 520); break;
    case EV_BONK:  for(int k = 0; k < 16; k++) spawn(e->x, e->y, k & 1 ? argb(255, 210, 63, 255) : argb(255, 255, 255, 255), 520); break;
    case EV_THROW: case EV_SWING: for(int k = 0; k < 4; k++) spawn(e->x, e->y, argb(255, 255, 255, 255), 160); break;
    default: break;   /* AICA sounds are routed through mx_event_sfx above. */
    }
  }
}



/* belly / rear jiggle, in sprite pixels of the old 4-px-per-cell art (the core keeps them in cells), plus squash factors */
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

/* one frame of the man (art/dcart.h, MAN texture): a smooth sprite 1.6x the size it is drawn at.  Without a Jig it is one quad; with one it is a small
   mesh whose vertices are pushed by the belly / rear springs (the same weights as the pixel version had) and squashed about the anchor. */
#define MG_X 8
#define MG_Y 10
static float man_rot;                                                       /* radians the next draw_frame turns the sprite by (flight pitch, tumble, a stuck man's rock) */
static void draw_frame(int fr, float cx, float cy, int face, float z, const Jig *jg){
  if(man_fur && fr >= 0 && fr < MSF_NBASE) fr += MSF_NBASE;                  /* the Wolfman's frames follow the human ones */
  const short *f = DCM_FR[fr];
  float sc = DCM_SCALE * man_zoom, s = PXC / 4.0f;
  int live = jg && (jg->bx != 0 || jg->by != 0 || jg->rx != 0 || jg->ry != 0 || jg->sx != 1 || jg->sy != 1);
  float sx = live ? jg->sx : 1, sy = live ? jg->sy : 1;
  int gx = live ? MG_X : 1, gy = live ? MG_Y : 1;
  float px[MG_Y + 1][MG_X + 1], py[MG_Y + 1][MG_X + 1];
  for(int j = 0; j <= gy; j++) for(int i = 0; i <= gx; i++){
    float u = f[2] * (float)i / gx, v = f[3] * (float)j / gy;                  /* sprite pixel (0..w, 0..h) */
    float dx = 0, dy = 0;
    if(live){
      float wb = blob(u - f[6], v - f[7], (float)f[8]), wr = blob(u - f[9], v - f[10], (float)f[11]);
      dx = (jg->bx * wb + jg->rx * wr) * s; dy = (jg->by * wb + jg->ry * wr) * s;
    }
    float ex = (u - f[4]) * sc * sx, ey = (v - f[5]) * sc * sy;
    if(man_rot != 0){ float c = cosf(man_rot), sn = sinf(man_rot), rx = ex * c - ey * sn; ey = ex * sn + ey * c; ex = rx; }
    px[j][i] = cx + (face > 0 ? ex : -ex) + dx;
    py[j][i] = cy + ey + dy;
  }
  /* One cohesive warm top-left lighting gradient across the *whole* sprite,
     including the jiggle mesh. Reusing vertex colour adds no texture reads,
     extra geometry, or additional heap/VRAM; no per-cell checkerboard. */
  uint32 light[MG_Y + 1][MG_X + 1];
  for(int j=0;j<=gy;j++)for(int i=0;i<=gx;i++){
    int fall=3*i/gx+10*j/gy,bright=255-fall;
    light[j][i]=argb(bright,bright-1,bright-3,255);
  }
  use(&hdr_man);
  for(int j = 0; j < gy; j++) for(int i = 0; i < gx; i++){
    float u0 = (f[0] + f[2] * (float)i / gx) / DCT_MAN_W, u1 = (f[0] + f[2] * (float)(i + 1) / gx) / DCT_MAN_W;
    float v0 = (f[1] + f[3] * (float)j / gy) / DCT_MAN_H, v1 = (f[1] + f[3] * (float)(j + 1) / gy) / DCT_MAN_H;
    vtx(px[j][i], py[j][i], z, u0, v0, light[j][i], 0);
    vtx(px[j][i + 1], py[j][i + 1], z, u1, v0, light[j][i+1], 0);
    vtx(px[j + 1][i], py[j + 1][i], z, u0, v1, light[j+1][i], 0);
    vtx(px[j + 1][i + 1], py[j + 1][i + 1], z, u1, v1, light[j+1][i+1], 1);
  }
}

/* the towel of the man whose centre is at cx,cy (core/cloth.c): pass 0 = the part behind him, 1 = the part in front.  The sprites are drawn at
   0.36 px per design unit at 4 px per cell, so a design unit is 0.36 * PXC / 4 screen pixels. */
static void draw_cloth(const Cloth *cl, float cx, float cy, int face, float z, int pass){
  ClothQuad q[CL_NQ];
  int n = cloth_quads(cl, q);
  float s = 0.36f * PXC / 4.0f * man_zoom;
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
  draw_frame(fr, x, y, face, z, jg);
  if(ta->live) draw_cloth(&ta->c, x, y, face, z + 0.5f, 1);
}
static void idle_fx(int fx, float wx, float wy, int face){
  switch(fx){ case FX_SLAP: sfx_play(SFX_SLAP, 64); break; case FX_GRUNT: sfx_play(SFX_GRUNT, 64); break; case FX_BURP: sfx_play(SFX_BURP, 64); break; case FX_FART: sfx_play(SFX_FART, 64); break;
    case FX_FLICK: sfx_play(SFX_FLICK, 64); break; case FX_TAUNT: sfx_play(SFX_TAUNT, 64); break; case FX_EEK: sfx_play(SFX_EEK, 64); break; case FX_SNAP: sfx_play(SFX_SLAP, 64); sfx_play(SFX_BOING, 64); break;
    case FX_SNORE: sfx_play(SFX_SNORE, 64); break; case FX_KISS: case FX_POP: sfx_play(SFX_POP, 64); break; case FX_SPARKLE: sfx_play(SFX_STRETCH, 64); break; case FX_DISCO: case FX_BOING: sfx_play(SFX_BOING, 64); break;
    case FX_YAWN: sfx_play(SFX_YAWN, 64); break; case FX_WHEW: sfx_play(SFX_WHEW, 64); break; case FX_WHOOSH: sfx_play(SFX_WHOOSH, 64); break; default: break; }
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
  case FX_KISS:  for(int k = 0; k < 3; k++) spawn_k(PK_HEART, wx + face * (3 + k * 0.6f), wy - 5 - k * 0.5f, face * (40 + frand() * 40), -(40 + frand() * 30), 1.5f + frand() * 0.4f, argb(255, 95, 147, 255)); break;
  case FX_SPARKLE: for(int k = 0; k < 5; k++) spawn_k(PK_GLINT, wx + (frand() - 0.5f) * 9, wy - frand() * 9, 0, -8, 0.5f + frand() * 0.4f, argb(255, 247, 194, 255)); break;
  case FX_DISCO: spawn_k(PK_GLINT, wx + (frand() < 0.5f ? 1 : -1) * (3 + frand() * 3), wy - 5 - frand() * 3, 0, -8, 0.5f, argb(255, 255, 120, 255)); break;
  case FX_POP:   for(int k = 0; k < 2; k++) spawn_k(PK_BIT, wx + (k ? 2.5f : -2.5f), wy - 3, (k ? 1 : -1) * (40 + frand() * 40), -50 - frand() * 40, 0.4f, argb(255, 255, 255, 255)); break;
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
#if MX_DC_PARTICLE_HINT
    if(p->life <= 0 && i < part_free_hint) part_free_hint = i;
#endif
  }
}

/* The title: the whole landscape with the man dropping butt-first on a landlord (a 852x480 picture, art/dcart.h, from the HTML title, logo included).
   The picture is wider than the screen, so the screen pans across it: hold on the logo, glide right to the man, hold, glide back. */
static int on_title = 1, title_t, paused, title_lv;
static MxFpsMeter dc_fps;
static unsigned long long dc_prev_ms;
static void dc_frame_complete(int simulated){
  unsigned long long now=timer_ms_gettime64();
  if(dc_prev_ms&&now>=dc_prev_ms)
    mx_fps_meter_add(&dc_fps,(unsigned)(now-dc_prev_ms),1000u,1u,(unsigned)simulated);
  dc_prev_ms=now;
}
static int nofight;                                       /* 1 = landlords don't fight back (title: Y; pause: X) */
#include "vmu_save.h"
static int title_pan(int t, int range){                 /* 0..range for tick t: hold (90), glide right (160), hold (150), glide back (160), smoothstep */
  t %= 560;
  int k = t < 90 ? 0 : t < 250 ? t - 90 : t < 400 ? 160 : t < 560 ? 160 - (t - 400) : 0;
  int u = (k * 256) / 160;
  return (range * ((u * u * (3 * 256 - 2 * u)) >> 16)) >> 8;
}
static void draw_title(void){
  int pan = title_pan(title_t, 852 - 640);
  tquad(&hdr_ttl, DCT_TTL_W, DCT_TTL_H, (float)-pan, 0, 852, 480, 0, 0, 852, 480, Z_BG, WHITE);
  if(!((title_t >> 5) & 1)) text("PRESS START", 320, 438, 40, WHITE, 1, Z_HUD);
  { char tb[80]; snprintf(tb, sizeof tb, "< LEVEL %d  %s >   L/R TRIGGER: OUTFIT - %s", title_lv + 1, mx_level_name(title_lv), mx_outfit(outfit_sel)->name); text(tb, 320, 466, 16, argb(255, 126, 176, 255), 1, Z_HUD);
    snprintf(tb, sizeof tb, "Y: LANDLORDS FIGHT BACK - %s", nofight ? "OFF" : "ON"); text(tb, 320, 418, 16, argb(255, 255, 255, 220), 1, Z_HUD); }
}

/* ---- the world ---- */
static int variant(int x, int y){ return (x * 5 + y * 3 + (x >> 2)) & 3; }
static void tile(float px, float py, int m, int v, int d, float z){
  int t = ((m - 1) * 4 + v) * 3 + d;
  float tx = DCA_TILE_X + (t & 15) * DCA_TILE_S + 1, ty = DCA_TILE_Y + (t >> 4) * DCA_TILE_S + 1;
  tquad(&hdr_atl, 512, 512, px, py, PXC + 0.35f, PXC + 0.35f, tx, ty, 16, 16, z, WHITE);
}
static void tile_rot(float cx, float cy, float ca, float sa, int m, int v, int d, float z){      /* a block of a flying chunk, turned */
  int t = ((m - 1) * 4 + v) * 3 + d;
  float tx = DCA_TILE_X + (t & 15) * DCA_TILE_S + 1, ty = DCA_TILE_Y + (t >> 4) * DCA_TILE_S + 1;
  float u0 = tx / 512, v0 = ty / 512, u1 = (tx + 16) / 512, v1 = (ty + 16) / 512, h = PXC * 0.5f + 0.2f;
  float ex[4] = { -h, h, -h, h }, ey[4] = { -h, -h, h, h }, uu[4] = { u0, u1, u0, u1 }, vv[4] = { v0, v0, v1, v1 };
  use(&hdr_atl);
  for(int k = 0; k < 4; k++) vtx(cx + ex[k] * ca - ey[k] * sa, cy + ex[k] * sa + ey[k] * ca, z, uu[k], vv[k], WHITE, k == 3);
}
/* A rotated voxel has half-side (PXC/2 + .2) = 2.7 pixels.
   Even at 45 degrees its axis extent stays below 3.82 px. Reject cells
   whose entire conservative 4px envelope misses the 640x480 screen.
   Saves PVR vertices and texture state without any asset/framebuffer cost. */
#ifndef MX_DC_RUBBLE_CULL
#define MX_DC_RUBBLE_CULL 1
#endif
static void tile_rot_visible(float cx,float cy,float ca,float sa,int m,int v,int d,float z){
#if MX_DC_RUBBLE_CULL
  if(cx<=-4.0f || cx>=644.0f || cy<=-4.0f || cy>=484.0f)return;
#endif
  tile_rot(cx,cy,ca,sa,m,v,d,z);
}
static int dmg(float hp, int m){ float r = hp / MAXHP[m]; return r > 0.66f ? 0 : r > 0.33f ? 1 : 2; }

/* chapter 2 kinds: the same sprite, tinted (bat green, thrower blue), mirrored to face his way, plus the club / briefcase / papers / dazed stars */
static void draw_landlord(float x, float y, float z, const MxTarget *t){
  const short *r = DCA_LANDLORD;
  int k = t ? t->kind : LK_NONE, fc = t && t->face < 0 ? -1 : 1, stun = t && t->stun > 0, wind = t && t->wind > 0, swing = t && t->swing > 0;
  uint32 c = k == LK_BAT ? argb(200, 255, 200, 255) : k == LK_THROWER ? argb(205, 220, 255, 255) : k == LK_BOSS ? argb(120, 120, 140, 255) : k == LK_SPRINTER ? argb(255, 225, 160, 255) : k == LK_HEALER ? argb(235, 255, 235, 255) : k == LK_SHIELD ? argb(170, 190, 190, 255) : WHITE;   /* tints: bat green, thrower blue, boss dark, sprinter tan, healer white-green, shield grey */
  float w = r[2] * 0.5f, h = r[3] * 0.5f, lx = x - r[4] * 0.5f;
  /* The atlas remains one textured quad. Separate shoe layers are tied to
     real distance traveled, which avoids speed-dependent walk jitter. */
  float step=(t&&t->mv)?(((int)(t->wk*14.0f)>>2)&1?2.5f:-2.5f):0.0f;
  if(t&&t->mv){
    quad(x-10,y+12,20,2,z-.02f,argb(28,42,36,95));
    quad(x-9+step,y+8,8,4,z+.02f,argb(28,29,38,255));
    quad(x+1-step,y+8,8,4,z+.02f,argb(26,27,35,255));
    quad(x-8+step,y+8,4,1,z+.025f,argb(126,132,146,255));
    quad(x+2-step,y+8,4,1,z+.025f,argb(126,132,146,255));
  }
  if(k == LK_NONE) tquad(&hdr_atl, 512, 512, lx, y - r[5] * 0.5f, w, h, r[0], r[1], r[2], r[3], z, c);
  else if(fc > 0) tquad(&hdr_atl, 512, 512, lx, y - r[5] * 0.5f, w, h, r[0], r[1], r[2], r[3], z, c);
  else tquad(&hdr_atl, 512, 512, x + r[4] * 0.5f - w, y - r[5] * 0.5f, w, h, r[0] + r[2], r[1], -r[2], r[3], z, c);
  if(!t || k == LK_NONE) return;
  float zz = z + 0.05f, top = y - r[5] * 0.5f;
  if(stun){
    for(int i = 0; i < 3; i++){ float a = world.time * 6 + i * 2.1f; puff(x + cosf(a) * 7, top - 3 + sinf(a) * 2, 2.2f, zz, argb(255, 235, 90, 255)); }
  }
  if(k == LK_BAT){                                                      /* the club: up, or swung out flat */
    float hx = x + fc * 5, hy = y + 2, ex = x + fc * (swing ? 26 : 16), ey = swing ? y + 4 : y - 18;
    seg(hx, hy, ex, ey, 3.2f, zz, argb(90, 52, 20, 255)); seg(hx, hy, ex, ey, 1.6f, zz + 0.01f, argb(185, 122, 60, 255));
    quad(x - 4, top + 2, 8, 2.5f, zz, argb(42, 138, 42, 255));
  } else if(k == LK_THROWER || k == LK_BOSS){                           /* briefcase, raised over the head to throw */
    float bx = wind ? x + fc * 5 : swing ? x + fc * 16 : x + fc * 11, by = wind ? y - 24 : swing ? y : y + 3;
    quad(bx - 7, by - 4.5f, 14, 9, zz, argb(58, 34, 12, 255)); quad(bx - 6, by - 3.5f, 12, 7, zz + 0.01f, argb(122, 74, 34, 255)); quad(bx - 1, by - 1, 2, 2, zz + 0.02f, argb(255, 210, 63, 255));
    quad(x - 4, top + 2, 8, 2.5f, zz, k == LK_BOSS ? argb(255, 210, 63, 255) : argb(58, 106, 209, 255));
  } else if(k == LK_WALKER || k == LK_SHIELD || k == LK_SPRINTER){      /* papers in his hand */
    float bx = swing ? x + fc * 16 : x + fc * 11; quad(bx - 3, y, 6, 7, zz, WHITE); quad(bx - 3, y + 1, 6, 1.2f, zz + 0.01f, argb(42, 138, 42, 255));
    quad(x - 4, top + 2, 8, 2.5f, zz, k == LK_SPRINTER ? argb(255, 210, 63, 255) : argb(138, 143, 153, 255));
  } else if(k == LK_HEALER){                                            /* green cross and cap band */
    quad(x - 1, y - 4, 2, 9, zz, argb(47, 191, 74, 255)); quad(x - 4.5f, y - 0.5f, 9, 2, zz, argb(47, 191, 74, 255)); quad(x - 4, top + 2, 8, 2.5f, zz, argb(47, 191, 74, 255));
  }
  /* Every enemy class shares a short-lived, gameplay-accurate windup
     tell. It is only drawn while the AI really is about to attack. */
  if(wind){
    quad(x-8,top-13,16,2,zz,argb(255,210,63,255));
    text("!",x,top-15,14,argb(255,210,63,255),1,Z_HUD);
  }
  if(t->armor){ float sx = x + fc * 14; quad(sx - 3, y - 12, 6, 24, zz, argb(20, 20, 20, 255)); quad(sx - 2, y - 11, 4, 22, zz + 0.01f, argb(154, 164, 178, 255)); quad(sx - 0.8f, y - 4, 1.6f, 6, zz + 0.02f, argb(255, 210, 63, 255)); }   /* riot shield */
  if(t->hpmax > 1){ float bw = t->hpmax * 7.0f, bx0 = x - bw * 0.5f; quad(bx0 - 1, top - 9, bw + 2, 6, zz, argb(0, 0, 0, 255)); for(int i = 0; i < t->hpmax; i++) quad(bx0 + i * 7, top - 8, 6, 4, zz + 0.01f, i < t->hp ? argb(232, 64, 64, 255) : argb(85, 85, 34, 255)); }   /* health bar */
}

static void draw_rooms(void){
  static const unsigned char PAP[4][3] = { { 238, 214, 170 }, { 200, 225, 205 }, { 225, 190, 205 }, { 190, 205, 235 } };
  for(int y = 0; y < MX_ROWS; y++){
    int x = 0;
    while(x < MX_COLS){
      int i = y * MX_COLS + x, n = world.paper[i];
      if(!n || world.grid[i]){ x++; continue; }
      int x2 = x; while(x2 < MX_COLS && world.paper[y * MX_COLS + x2] == n && !world.grid[y * MX_COLS + x2]) x2++;     /* one quad per run of wallpaper */
      float qx = X(x), qy = Y(y), w = (x2 - x) * PXC, h = PXC;
      const unsigned char *p = PAP[(n - 1) & 3];
      gquad(qx, qy, w, h, Z_ROOM, argb(p[0], p[1], p[2], 255), argb(p[0] * 9 / 10, p[1] * 9 / 10, p[2] * 9 / 10, 255));
      /* Fine pinstripes made the rooms look like placeholder graph paper.
         One narrow architectural panel seam per 20px reduces fill/draws
         versus the original 10px stripes and reads as actual wall cladding. */
      for(float k = 0; k < w; k += 20) quad(qx + k, qy, 2, h, Z_ROOM + 0.01f, argb(36, 29, 26, 24));
      if(y + 1 >= MX_ROWS || !world.paper[(y + 1) * MX_COLS + x]) quad(qx, qy + h - 3, w, 3, Z_ROOM + 0.02f, argb(0, 0, 0, 46));
      if(world.paper_torn){
        /* One narrow exposed plaster lip per edge, not per pixel. */
        if(x>0&&!world.paper[i-1])quad(qx,qy,1.5f,h,Z_ROOM+0.025f,argb(72,45,40,175));
        if(x2<MX_COLS&&!world.paper[y*MX_COLS+x2])quad(qx+w-1.5f,qy,1.5f,h,Z_ROOM+0.025f,argb(72,45,40,175));
      }
      x = x2;
    }
  }
}

/* Actual room geometry, rather than a texture of generic windows: glass and
   furniture share the room's surviving wallpaper cell. Frustum culling and
   at most three windows per room bound the translucent polygon budget. */
static void draw_facades(void){
  for(int r=0;r<world.nrooms;r++){
    const MxRoom *rm=&world.rooms[r];
    if(rm->w<6||rm->h<5)continue;
    float roomx=X(rm->x),roomy=Y(rm->y);
    if(roomx>644||roomx+rm->w*PXC<-4||roomy>484||roomy+rm->h*PXC<-4)continue;
    int n=rm->w>=18?3:rm->w>=11?2:1;
    for(int k=0;k<n;k++){
      int wx=rm->x+rm->w*(2*k+1)/(2*n),wy=rm->y+rm->h/2;
      int i=wy*MX_COLS+wx;
      if(world.grid[i]||!world.paper[i])continue;
      float ww=(rm->w>=10?4:3)*PXC,hh=(rm->h>=7?4:3)*PXC;
      float sx=X(wx)-ww*.5f,sy=Y(wy)-hh*.5f;
      if(sx>643||sx+ww<0||sy>483||sy+hh<0)continue;
      int lit=(r+k+world.level)%5==0;
      uint32 glass=lit?argb(247,200,123,255):(r&1)?argb(114,165,188,255):argb(102,139,178,255);
      quad(sx-2,sy-2,ww+4,hh+4,Z_ROOM+.04f,argb(29,30,45,255));
      quad(sx-1,sy-1,ww+2,hh+2,Z_ROOM+.05f,argb(223,216,198,255));
      gquad(sx,sy,ww,hh,Z_ROOM+.06f,lit?argb(255,224,163,255):argb(174,203,226,255),glass);
      quad(sx+2,sy+2,ww*.3f,hh*.24f,Z_ROOM+.07f,argb(229,240,251,100));
      quad(sx+ww*.5f-1,sy,2,hh,Z_ROOM+.08f,argb(234,225,205,255));
      quad(sx,sy+hh*.5f-1,ww,2,Z_ROOM+.08f,argb(234,225,205,255));
      quad(sx-PXC*.5f,sy+hh+1,ww+PXC,2,Z_ROOM+.09f,argb(101,78,71,255));
    }
    if(rm->w>=9&&rm->h>=6){
      int cx=rm->x+2,cy=rm->y+rm->h-2,i=cy*MX_COLS+cx;
      if(cy>=MX_ROWS||world.grid[i]||!world.paper[i])continue;
      float sx=X(cx),sy=Y(cy);
      quad(sx,sy,3*PXC,PXC,Z_ROOM+.04f,argb(102,53,81,255));
      quad(sx,sy,3*PXC,1,Z_ROOM+.05f,argb(231,151,132,255));
      quad(sx,PXC+sy,2,PXC,Z_ROOM+.05f,argb(57,42,51,255));
      quad(sx+3*PXC-2,PXC+sy,2,PXC,Z_ROOM+.05f,argb(57,42,51,255));
      quad(sx+PXC,sy-2*PXC,PXC,PXC,Z_ROOM+.06f,argb(255,225,138,255));
    }
  }
}
static float cam_pan;                        /* camera drift (px): follows the man in flight; only the clouds and the near grass react */
typedef struct { float x, y, s, v; } Cloud;
static void draw_clouds(void){
  static const Cloud C[4] = { { 100, 90, 1.0f, 6 }, { 430, 60, 0.8f, 4 }, { 660, 130, 1.1f, 8 }, { -120, 170, 0.9f, 5 } };
  const short *r = DCA_CLOUD;
  for(int i = 0; i < 4; i++){
    float x = fmodf(C[i].x + world.time * C[i].v + 400.0f - cam_pan * 0.25f * C[i].s + 1000.0f, 1000.0f) - 250.0f;     /* clouds also slide against the camera drift */
    atl(x, C[i].y, r[2] * C[i].s, r[3] * C[i].s, r, Z_CLOUD + i * 0.001f, argb(255, 255, 255, 235));
  }
}

/* A rigid chunk may carry hundreds of tiles entirely beyond the display.
   Conservative radial culling happens before sin/cos and the PVR submits.
   Partially visible bodies still draw every original visible fragment. */
#ifndef MX_DC_CHUNK_VIS_CULL
#define MX_DC_CHUNK_VIS_CULL 1
#endif
static void draw_rubble_chunks(void){
  for(int c=0;c<MX_MAX_CHUNKS;c++){
    const MxChunk *ch=&world.chunks[c];
    if(!ch->active)continue;
#if MX_DC_CHUNK_VIS_CULL
    const float cx=X(ch->x),cy=Y(ch->y),reach=ch->rad*PXC+10.0f;
    if(cx+reach<0 || cx-reach>640 || cy+reach<0 || cy-reach>480)
      continue;
#endif
    const float ca=cosf(ch->ang),sa=sinf(ch->ang);
    for(int k=0;k<ch->count;k++){
      const MxCell *a=&ch->cells[k];
      if(a->dead)continue;
      tile_rot_visible(X(a->wx),Y(a->wy),ca,sa,a->m,
                       variant((int)(a->lx*7),(int)(a->ly*3)),dmg(a->h,a->m),Z_CHUNK);
    }
  }
}
static void draw_all(void){
  shx = shy = 0;
  if(world.shake > 0.05f){ shx = (frand() - 0.5f) * world.shake * 0.6f; shy = (frand() - 0.5f) * world.shake * 0.6f; }
  /* backdrop: sky, drifting clouds, hills / city / ground + the sling post */
  tquad(&hdr_bga, DCT_BGA_W, DCT_BGA_H, 0, 0, 640, 480, 0, 0, 512, 256, Z_BG, WHITE);
  draw_clouds();
  tquad(&hdr_bgb, DCT_BGB_W, DCT_BGB_H, 0, 0, 1024, 512, 0, 0, 1024, 512, Z_BGB, WHITE);
  draw_rooms();
  draw_facades();
  const MxMan *m = &world.man;
  /* sling band behind the man */
  if(m->mode == MAN_AIM){
    seg(XB(MX_ANCHOR_X) + 8.0f, YB(MX_ANCHOR_Y) + 15.0f, X(m->x) + 2.5f, Y(m->y) + 3.8f, 2.8f, Z_SLING, argb(42, 22, 10, 255));
  }
  /* voxels: each tile drawn where the wobble lattice has pushed it; a snapped joint shows as a dark crack */
  for(int y = 0; y < MX_ROWS; y++) for(int x = 0; x < MX_COLS; x++){
    int i = y * MX_COLS + x, mt = world.grid[i];
    if(!mt) continue;
    float px = X(x), py = Y(y);
    if(world.wob_on){ px += world.ox[i] * PXC; py += world.oy[i] * PXC; }
    int d = dmg(world.hp[i], mt);
    if(d == 0 && world.brk[i]) d = 1;
    tile(px, py, mt, variant(x, y), d, Z_VOX);
  }
  /* Cull whole offscreen rubble chunks before their per-cell draw. */
  draw_rubble_chunks();
  /* landlords */
  for(int i = 0; i < world.ntargets; i++){
    const MxTarget *t = &world.targets[i];
    if(!t->alive) continue;
    float wx, wy; mx_target_offset(&world, t, &wx, &wy);
    float tx=X(t->x+wx),ty=Y(t->y+wy)+(t->mv?0.0f:sinf(world.time*5+t->t)*0.5f);
    draw_landlord(tx,ty,Z_TARGET,t);
    if(t->collapse_rolled==2) /* survivor remains an objective after smoke fades */
      text("!",tx,ty-44,18,argb(255,210,63,255),1,Z_TARGET+0.75f);
  }
  /* briefcases in the air (Batter Up) and the pink bounce pads of the walk levels */
  for(int i = 0; i < MX_MAX_CASES; i++){
    const MxCase *c = &world.cases[i]; if(!c->active) continue;
    float cx = X(c->x), cy = Y(c->y), a = c->rot * 2, ca = cosf(a), sa = sinf(a), hw = 8, hh = 6;
    float ex[4] = { -hw, hw, -hw, hw }, ey[4] = { -hh, -hh, hh, hh }, qx[4], qy[4], ix[4], iy[4];
    for(int k = 0; k < 4; k++){ qx[k] = cx + ex[k] * ca - ey[k] * sa; qy[k] = cy + ex[k] * sa + ey[k] * ca; float fx = ex[k] * 0.8f, fy = ey[k] * 0.75f; ix[k] = cx + fx * ca - fy * sa; iy[k] = cy + fx * sa + fy * ca; }
    quad4(qx, qy, Z_TARGET + 0.2f, argb(30, 18, 6, 255)); quad4(ix, iy, Z_TARGET + 0.25f, argb(122, 74, 34, 255)); puff(cx, cy, 1.8f, Z_TARGET + 0.3f, argb(255, 210, 63, 255));
  }
  for(int i = 0; i < world.npads; i++){
    const MxPad *pd = &world.pads[i];
    float cx = X(pd->x + pd->w * 0.5f), cy = Y((float)pd->y); float bob = (float)((int)(world.time * 8) & 3);
    for(int k = 0; k < 3; k++){ float yy = cy - 15 - 15 * k - bob * 1.5f; uint32 c = k == 0 ? WHITE : k == 1 ? argb(255, 158, 200, 255) : argb(255, 110, 150, 255);
      seg(cx - 10, yy + 4, cx, yy - 4, 2.4f, Z_FX - 0.2f, c); seg(cx, yy - 4, cx + 10, yy + 4, 2.4f, Z_FX - 0.2f, c); }
  }
  /* chapter 2 pneumatic tubes: balloon, brass mouth, nozzle stub */
  for(int i = 0; i < world.ntubes; i++){
    const uint32 TC[3] = { argb(255, 154, 60, 255), argb(79, 208, 255, 255), argb(140, 255, 90, 255) };
    const MxTube *t = &world.tubes[i]; uint32 c = TC[i % 3];
    float ax = X(t->ax), ay = Y(t->ay), bx = X(t->bx), by = Y(t->by);
    float sw = sinf(world.time * 1.3f + i * 2) * 2, balx = ax + sw, baly = ay - 39 + sinf(world.time * 1.9f + i) * 2;
    seg(balx - 6, baly + 16, ax - 8, ay - 15, 1.4f, Z_TARGET + 0.1f, argb(58, 42, 26, 255)); seg(balx + 6, baly + 16, ax + 8, ay - 15, 1.4f, Z_TARGET + 0.1f, argb(58, 42, 26, 255));
    puff(balx, baly, 19.5f, Z_TARGET + 0.2f, argb(0, 0, 0, 255)); puff(balx, baly, 17.5f, Z_TARGET + 0.25f, c);
    puff(balx - 6, baly - 7, 5.5f, Z_TARGET + 0.3f, argb(255, 255, 255, 110));
    text("IN", balx, baly + 3, 10, WHITE, 1, Z_TARGET + 0.4f);
    puff(ax, ay, 19, Z_TARGET + 0.2f, argb(200, 154, 58, 255)); puff(ax, ay, 16.5f, Z_TARGET + 0.25f, c); puff(ax, ay, 14.5f, Z_TARGET + 0.3f, argb(42, 33, 24, 255));
    { float a = world.time * 5; for(int k = 0; k < 24; k++){ float aa = k * 0.3f + a, r = k * 0.5f; puff(ax + cosf(aa) * r, ay + sinf(aa) * r, 1.4f, Z_TARGET + 0.35f, c); } }
    quad(bx - 24, by - 8, 20, 16, Z_TARGET + 0.2f, argb(200, 150, 58, 255)); quad(bx - 22, by - 6, 17, 3, Z_TARGET + 0.25f, argb(255, 224, 138, 255)); quad(bx - 4, by - 9, 7, 18, Z_TARGET + 0.3f, c);
  }
  /* the air valve has no presence until selected with DOWN. */
  if(world.valve_ok){ float vx = X(MX_VALVE_X), vy = Y(MX_VALVE_Y), gy = Y((float)MX_ROWS); int f = m->mode == MAN_AIM ? (int)(m->inf * 100) : 0;
    quad(vx - 14, gy - 4, 28, 5, Z_MAN - 0.6f, argb(110, 115, 130, 255));
    quad(vx - 4, vy + 6, 8, gy - vy - 6, Z_MAN - 0.5f, argb(74, 85, 104, 255));
    quad(vx - 4, vy + 6, 3, gy - vy - 6, Z_MAN - 0.45f, argb(150, 160, 175, 255));
    quad(vx - 7, vy + 2, 14, 8, Z_MAN - 0.4f, world.valve_ok ? argb(255, 160, 40, 255) : argb(130, 130, 140, 255));                                            /* nozzle */
    puff(vx + 20, vy + 40, 10, Z_MAN - 0.4f, argb(20, 20, 20, 255)); puff(vx + 20, vy + 40, 8.5f, Z_MAN - 0.38f, WHITE);
    { float a = 3.14159f * (0.85f + 1.3f * f / 100.0f); seg(vx + 20, vy + 40, vx + 20 + cosf(a) * 7, vy + 40 + sinf(a) * 7, 1.6f, Z_MAN - 0.3f, f > 75 ? argb(220, 30, 30, 255) : argb(20, 20, 20, 255)); }
    if(m->mode == MAN_AIM && f > 0){ quad(vx - 12, vy - 52, 50, 8, Z_HUD, argb(20, 24, 40, 255)); quad(vx - 11, vy - 51, 48.0f * f / 100.0f, 6, Z_HUD + 0.1f, f < 50 ? argb(125, 255, 90, 255) : f < 80 ? argb(255, 210, 63, 255) : argb(255, 70, 70, 255));
      if(f > 70 && ((int)(world.time * 8) & 1)) text("DANGER", vx, vy - 58, 12, argb(255, 70, 70, 255), 1, Z_HUD); }
    else if(m->mode == MAN_AIM && ((int)(world.time * 2) & 1)) text(world.valve_ok ? "D-PAD DOWN: AIR" : "AIR: LOCKED", vx + 30, vy - 20, 10, WHITE, 1, Z_HUD); }
  /* the aiming dots */
  if(m->mode == MAN_AIM){
    float dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
    if(dx * dx + dy * dy > 3.0f) for(int k = 1; k <= 16; k++){
      float ox, oy; mx_preview_point(&world, (float)k / 9.0f, &ox, &oy);
      int gx = (int)ox, gy = (int)oy;
      if(gx < 0 || gx >= MX_COLS || gy >= MX_ROWS) break;
      if(gy >= 0 && world.grid[gy * MX_COLS + gx]) break;
      puff(X(ox), Y(oy), 4.2f - k * 0.12f, Z_FX, argb(255, 255, 255, 235 - k * 11));
    }
  }
  /* the man: smooth sprites from art/dcart.h */
  if(m->mode != MAN_NONE && m->mode != MAN_WAIT){
    float x = X(m->x), y = Y(m->y);
    int ph = (int)(world.time * 60.0f), face = 1, fr = (ph >> 4) & 1 ? MSF_IDLE_A : MSF_IDLE_B;
    Jig jg = man_jig();
    if(m->mode == MAN_AIM){
      float dx = m->x - MX_ANCHOR_X, dy = m->y - MX_ANCHOR_Y;
      if(dx * dx + dy * dy > 4.0f) fr = MSF_DRAG;                         /* pulled back: yelling */
      else fr = mi_frame(&mi, ph);                                        /* waiting: idle gags */
    }
    else if(m->mode == MAN_WALK){ fr = -1; }
    else if(m->mode == MAN_STUCK){ puff(x, y + 6, 30, Z_MAN - 0.2f, argb(111, 227, 74, 200)); fr = ((ph >> 3) & 1) ? MSF_STUCK_A : MSF_STUCK_B; }
    else if(m->mode == MAN_FLY){ face = m->vx >= 0 ? 1 : -1; fr = m->dive ? MSF_DIVE : ((ph >> 2) & 1 ? MSF_FLY_A : MSF_FLY_B); }
    if(m->mode == MAN_AIM || m->mode == MAN_FLY){                         /* his shadow on the ground: smaller and fainter the higher he is */
      float hgt = (MX_ROWS - m->y) / 40.0f; if(hgt < 0) hgt = 0; if(hgt > 1) hgt = 1;
      float rw = 30.0f * (1.0f - 0.55f * hgt); quad(XB(m->x) - rw, YB(MX_ROWS) + 1, rw * 2, 4, Z_SLING + 0.05f, argb(0, 0, 0, (int)(90 - 60 * hgt)));
    }
    if(m->mode == MAN_WALK){                                              /* on his own feet: the turning frame steps him along, three quarters of the sling size */
      if(!(m->hurtCd > 0 && (fcount & 2))){
        int mv = m->gr && (m->vx > 3 || m->vx < -3), wf = MSF_SPIN_6 + (mv ? ((fcount >> 2) & 1 ? 1 : -1) : 0), fc = m->face < 0 ? -1 : 1;
        const short *f = DCM_FR[wf + (man_fur ? MSF_NBASE : 0)]; float zm = man_zoom; man_zoom = zm * 0.75f;
        float foot = Y(m->y + 2.375f) - (mv && ((fcount >> 2) & 1) ? 1.0f : 0.0f);
        draw_frame(wf, x, foot - (f[3] - f[5]) * DCM_SCALE * man_zoom, fc, Z_MAN, NULL);
        man_zoom = zm;
        if(m->pT > 0) for(int k = -3; k <= 3; k++) puff(x + fc * (24 - k * k * 1.4f) + fc * 18, y + k * 3.4f, 1.8f, Z_MAN + 0.3f, WHITE);
      }
    } else
    if(m->mode == MAN_FLY) man_rot = (m->dive ? m->spin : m->ang) * face; else if(m->mode == MAN_STUCK) man_rot = mx_man_wobble(m) * 0.17f;
    man_with_towel(&tow_play, fr, x, y, face, Z_MAN, &jg);
    man_rot = 0;
    if(m->inf > 0 && (m->mode == MAN_AIM || m->mode == MAN_FLY)){               /* only the belly swells; the face flushes as the pressure builds */
      float f = m->inf, rx = 11 + f * 28, ry = 10 + f * 24, bx = x + face * f * 10, by = y + 6 + f * 4;
      puff(bx, by, (rx > ry ? rx : ry) + 2, Z_MAN + 0.3f, argb(205, 135, 100, 255)); puff(bx, by, (rx > ry ? rx : ry), Z_MAN + 0.4f, argb(244, 183, 142, 255));
      puff(bx - rx * 0.3f, by - ry * 0.45f, rx * 0.22f, Z_MAN + 0.5f, argb(255, 215, 180, 180));
      puff(bx + face * rx * 0.8f, by + ry * 0.1f, 2 + f * 3, Z_MAN + 0.5f, argb(205, 135, 100, 255));
      if(f > 0.45f){ float al = (f - 0.45f) * 1.6f; if(al > 0.75f) al = 0.75f; puff(x, y - 16, 9 + f * 3, Z_MAN + 0.45f, argb(235, 40, 50, (int)(255 * al))); }
    }
  }
  if(m->mode == MAN_AIM)
    seg(XB(MX_ANCHOR_X) - 6.0f, YB(MX_ANCHOR_Y) + 13.8f, X(m->x) - 1.0f, Y(m->y) + 3.8f, 2.8f, Z_MAN + 1.0f, argb(42, 22, 10, 255));
  for(int i = 0; i < 256; i++) if(parts[i].life > 0){
    const Part *p = &parts[i]; float age = 1.0f - p->life / p->max;
    uint32 c = p->c & 0x00ffffff;
    switch(p->kind){
    case PK_STINK: { float r = 7 + age * 20; int a = (int)(150 * (1.0f - age * age)); puff(p->x, p->y, r, Z_FX, c | ((uint32)(a > 20 ? a : 20) << 24)); break; }   /* green cloud: swells, then thins out */
    case PK_FLY: { float t = world.time * 18.0f + i; quad(p->x + sinf(t * 1.3f) * 9, p->y + cosf(t) * 7, 3, 3, Z_FX, p->c); break; }
    case PK_ZZZ: { text("Z", p->x, p->y, 16 + age * 8, argb(255, 255, 255, (int)(255 * (1.0f - age))), 1, Z_FX); break; }
    case PK_SWEAT: puff(p->x, p->y, 3.0f, Z_FX, p->c); break;
    case PK_HEART: { float r = 7 + 2 * sinf(world.time * 9 + i), hx = p->x + sinf(world.time * 4 + i) * 5;      /* two blobs and a point: a heart */
      puff(hx - r * 0.55f, p->y - r * 0.3f, r * 0.7f, Z_FX, p->c); puff(hx + r * 0.55f, p->y - r * 0.3f, r * 0.7f, Z_FX, p->c);
      { float xs[4] = { hx - r * 1.1f, hx, hx, hx + r * 1.1f }, ys[4] = { p->y, p->y - 0.1f, p->y + r * 1.3f, p->y }; quad4(xs, ys, Z_FX, p->c); } break; }
    case PK_GLINT: { float r = 10 * sinf(age * 3.14159f); if(r > 0){ quad(p->x - r, p->y - 1, r * 2, 2, Z_FX, p->c); quad(p->x - 1, p->y - r, 2, r * 2, Z_FX, p->c); } break; }
    default: { float a = 1.0f - age; puff(p->x, p->y, 3.4f + a * 1.6f, Z_FX, c | ((uint32)((int)(255 * (a > 0.3f ? 1.0f : a / 0.3f))) << 24)); }
    }
  }
  if(world.flash > 0){ int a = (int)(world.flash * 400); if(a > 255) a = 255; quad(0, 0, 640, 480, Z_HUD - 0.5f, argb(255, 240, 200, a)); }

  /* HUD */
  gquad(0, 0, 640, 40, Z_HUD, argb(15, 18, 30, 200), argb(15, 18, 30, 150));
  char buf[64], nm[40]; int n = 0;
  for(const char *q = mx_level_name(world.level); *q && n < 38; q++) nm[n++] = (*q >= 'a' && *q <= 'z') ? *q - 32 : *q;
  nm[n] = 0;
  snprintf(buf, sizeof buf, "LV %d  %s", world.level + 1, nm);
  text(buf, 12, 26, 16, argb(255, 210, 63, 255), 0, Z_HUD + 0.1f);
  int alive = 0; for(int i = 0; i < world.ntargets; i++) alive += world.targets[i].alive;
  snprintf(buf, sizeof buf, "LANDLORDS %d/%d", alive, world.ntargets);
  text(buf, 440, 26, 15, alive ? argb(255, 138, 138, 255) : argb(125, 255, 90, 255), 0, Z_HUD + 0.1f);
  { static float shown; shown += (world.score - shown) * 0.12f; if(fabsf(world.score - shown) < 0.6f) shown = (float)world.score;
    snprintf(buf, sizeof buf, "%06d", (int)(shown + 0.5f)); text(buf, 634, 27, 14, WHITE, 2, Z_HUD + 0.1f); }
  text("MEN", 292, 25, 11, argb(190, 190, 190, 255), 0, Z_HUD + 0.1f);
  for(int i = 0; i < world.men; i++) atl(324 + i * 15, 11, 15, 16, DCA_ICON, Z_HUD + 0.1f, WHITE);
  if(world.combo > 1 && world.combo_left > 0){
    text(mx_combo_taunt(world.combo), 18, 69, 18, argb(255, 126, 176, 255), 0, Z_HUD + 0.1f);
    quad(18, 74, 174.0f * world.combo_left / 2.5f, 3, Z_HUD + 0.1f, argb(255, 210, 63, 255)); }
  if(bank_toast_t > 0 && world.state == MX_PLAY)
    text("BANK SHOT! +1500", 18, world.combo > 1 ? 99 : 69, 20, argb(255, 126, 176, 255), 0, Z_HUD + 0.1f);
  if(survivor_toast_t > 0 && world.state == MX_PLAY)
    text("SOMEHOW ALIVE!", 18, world.combo > 1 || bank_toast_t > 0 ? 124 : 69, 20, argb(255, 210, 63, 255), 0, Z_HUD + 0.1f);
  if(hint_t > 0 && world.state == MX_PLAY && m->mode == MAN_AIM)
    text("PULL BACK WITH THE STICK, RELEASE WITH A!", 340, 330, 18, argb(255, 255, 255, 160 + (int)(95 * sinf(world.time * 5))), 1, Z_HUD);
  if(m->mode == MAN_WALK && world.state == MX_PLAY){                       /* walk levels: the clock and the controls */
    int k = (int)(m->wt * 100.0f / MX_WALK_T); if(k < 0) k = 0; if(k > 100) k = 100;
    quad(246, 40, 148, 12, Z_HUD, WHITE); quad(247, 41, 146, 10, Z_HUD + 0.05f, argb(40, 40, 50, 255)); quad(247, 41, 146.0f * k / 100.0f, 10, Z_HUD + 0.1f, k > 30 ? argb(125, 255, 90, 255) : argb(255, 74, 74, 255));
    text("TIME", 238, 51, 12, WHITE, 2, Z_HUD + 0.1f);
    text("A JUMP   B PUNCH   X SLAM   L GIVE UP", 320, 466, 14, argb(255, 255, 255, 205), 1, Z_HUD);
  }
  if(m->mode == MAN_FLY && !m->dive) text("PRESS A = BELLY FLOP", 320, 466, 20, argb(255, 255, 255, 205), 1, Z_HUD);
  if(world.state == MX_PLAY && m->mode == MAN_AIM){
    static const char *const PWN[PW_COUNT] = { "NONE", "BEANS", "TOWEL", "SPLAT", "TURBO", "QUAKE" };
    snprintf(buf, sizeof buf, "X: POWER-UP  %s%s", PWN[world.pw_sel], world.pw_sel ? "" : " (B/T/S/T/Q x1)");
    text(buf, 12, 466, 14, world.pw_sel ? argb(255, 210, 63, 255) : argb(255, 255, 255, 200), 0, Z_HUD);
    snprintf(buf,sizeof buf,"DOWN: AIR %s   UP: SEAT (3 LEVELS)",world.valve_ok?"ON":"OFF");
    if(m->inf<=0)text(buf,628,444,12,world.valve_ok?argb(255,210,63,255):argb(255,255,255,155),2,Z_HUD);
  }
  toast_tick();
  if(toast_id >= 0){                                                          /* achievement popup: drops in, gold frame, star badge, time bar */
    float y = toast_t < 0.25f ? -70 + toast_t * 4 * 110 : toast_t > 2.55f ? (2.8f - toast_t) * 4 * 110 - 70 : 40, x = 170, w = 300, h = 56, k = 1.0f - toast_t / 2.8f;
    int oc = toast_id >= 100; uint32 fc = oc ? argb(125, 255, 90, 255) : argb(255, 210, 63, 255);
    quad(x - 3, y - 3, w + 6, h + 6, Z_HUD + 1, fc); quad(x, y, w, h, Z_HUD + 1.02f, argb(22, 26, 48, 245)); quad(x, y, w, h / 2, Z_HUD + 1.03f, argb(40, 46, 80, 120));
    puff(x + 30, y + 28, 19, Z_HUD + 1.05f, argb(255, 236, 140, 255)); puff(x + 30, y + 28, 15, Z_HUD + 1.06f, fc);
    text(oc ? "+" : "*", x + 30, y + 38, 26, argb(60, 40, 10, 255), 1, Z_HUD + 1.1f);
    text(oc ? "NEW OUTFIT" : "ACHIEVEMENT", x + 62, y + 20, 12, fc, 0, Z_HUD + 1.1f); text(oc ? mx_outfit(toast_id - 100)->name : mx_ach_name(toast_id), x + 62, y + 43, 18, WHITE, 0, Z_HUD + 1.1f);
    quad(x, y + h - 4, w * k, 4, Z_HUD + 1.05f, fc); }
  if(world.banner > 0 && world.state == MX_PLAY){
    int a = (int)(255 * (world.banner > 1 ? 1.0f : world.banner));
    if(world.level == MX_HQ_LEVEL){ char dn[32]; snprintf(dn, sizeof dn, "DISTRICT %d OF %d", world.district + 1, MX_HQ_DISTRICTS); text(dn, 320, 150, 22, argb(255, 126, 176, a), 1, Z_HUD); }
    text(nm, 320, 180, 40, argb(255, 255, 255, a), 1, Z_HUD);
    text("EVICT EVERY LANDLORD!", 320, 214, 20, argb(255, 210, 63, a), 1, Z_HUD);
  }

  { float tp = (m->mode == MAN_FLY || m->mode == MAN_STUCK) ? (m->x - MX_ANCHOR_X) * 3.0f : 0; cam_pan += (tp - cam_pan) * 0.06f;
    for(int i = -1; i < 46; i++){                                            /* near grass in front of everything, sliding the most */
      float gx = i * 15.0f - fmodf(cam_pan * 0.5f, 15.0f), h = 10 + ((i * 7 + 3) & 7) * 2, sw = sinf(world.time * 3.0f + i) * 2.0f;
      float xs[4] = { gx, gx + sw + 2, gx + sw + 4, gx + 6 }, ys[4] = { 480, 480 - h, 480 - h, 480 };
      quad4(xs, ys, Z_FX + 0.5f, (i & 1) ? argb(47, 122, 42, 255) : argb(68, 152, 58, 255));
    } }
  if(world.state != MX_PLAY){
    quad(0, 0, 640, 480, Z_END, argb(8, 10, 20, 184));
    int win = world.state == MX_WIN;
    man_zoom = 1.9f;
    if(win){
      man_with_towel(&tow_win[0], mi_dance_frame(fcount), 110, 330 - fabsf(sinf(fcount * 0.116f)) * 14, 1, Z_END + 1, 0);
      man_with_towel(&tow_win[1], mi_dance_frame(fcount + 14), 530, 330 - fabsf(sinf(fcount * 0.116f + 1.3f)) * 14, -1, Z_END + 1, 0);
    } else {
      draw_frame(MSF_SAD, 110, 330, 1, Z_END + 1, 0);
      for(int sd = -1; sd <= 1; sd += 2) for(int k = 0; k < 2; k++){
        float f = fmodf(fcount / 66.0f + k * 0.5f + (sd > 0 ? 0.17f : 0), 1.0f);
        puff(110 + sd * 22.0f, 284 + f * 60, 4.5f, Z_END + 2, argb(127, 212, 255, (int)(230 * (1 - f))));
      }
    }
    text(win ? (world.level == MX_NUM_LEVELS - 1 ? "EVERY LANDLORD EVICTED!" : "EVICTION COMPLETE!") : "THE LANDLORDS LIVE...", 320, 190, 46, win ? argb(125, 255, 90, 255) : argb(255, 106, 106, 255), 1, Z_END + 2);
    if(win){ int st = world.men >= 2 ? 3 : world.men >= 1 ? 2 : 1;
      for(int i = 0; i < 3; i++) atl(320 - 105 + i * 70 - 31, 215, 62, 62, &DCA_STARS[i < st ? 0 : 4], Z_END + 2, WHITE); }
    man_zoom = 1.0f;
    snprintf(buf, sizeof buf, "SCORE  %d", world.score);
    text(buf, 320, 330, 34, WHITE, 1, Z_END + 2);
    if(((fcount >> 5) & 1) == 0 || fcount > 0) text(win ? (world.level == MX_NUM_LEVELS - 1 ? "THE END - THANKS FOR PLAYING!" : "PRESS START: NEXT LEVEL") : "PRESS START: TRY AGAIN", 320, 390, 24, argb(255, 210, 63, 120 + (int)(120 * (0.5f + 0.5f * sinf(fcount * 0.1f)))), 1, Z_END + 2);
  }
}

static void load_level(int n){ bank_toast_t = survivor_toast_t = 0; mx_load(&world, n); mx_set_fight(&world, !nofight); mx_meta_level_start(&stats, &world); dc_valve_equipped=0; world.valve_ok=0; hint_t = 420; }

static void draw_intro(void){
  intro_draw(ic_top, ic_bot); ic_upload(tx_ic_top, ic_top); ic_upload(tx_ic_bot, ic_bot);
  quad(0, 0, 640, 480, Z_BG, argb(255, 0, 0, 0));
  tquad(&hdr_ic_top, 256, 256, 128, 2, 384, 288, 0, 0, 256, 192, Z_HUD, WHITE);                   /* the picture, 1.5x */
  tquad(&hdr_ic_bot, 256, 256, 128, 292, 384, 171, 0, 18, 256, 114, Z_HUD, WHITE);                /* the typed narration under it (rows 18..132 of the text screen) */
}
static void gfx_init(void){
  pvr_init_defaults();
  { pvr_poly_cxt_t cxt;
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);          /* translucent list so alpha works everywhere */
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr_col, &cxt); }
  tx_bga = load_tex(DCT_BGA_RLE, DCT_BGA_RLE_LEN, DCT_BGA_W, DCT_BGA_H);
  tx_bgb = load_tex(DCT_BGB_RLE, DCT_BGB_RLE_LEN, DCT_BGB_W, DCT_BGB_H);
  tx_atl = load_tex(DCT_ATL_RLE, DCT_ATL_RLE_LEN, DCT_ATL_W, DCT_ATL_H);
  tx_man = load_tex(DCT_MAN_RLE, DCT_MAN_RLE_LEN, DCT_MAN_W, DCT_MAN_H);
  { size_t tot = (size_t)DCT_MAN_W * DCT_MAN_H; man_px = (unsigned short *)malloc(tot * 2);       /* a copy of the atlas so outfits can re-tint it */
    if(man_px){ size_t n = 0; for(int i = 0; i + 1 < DCT_MAN_RLE_LEN && n < tot; i += 2){ unsigned cnt = DCT_MAN_RLE[i]; unsigned short v = DCT_MAN_RLE[i + 1]; while(cnt-- && n < tot) man_px[n++] = v; } } }
  tx_ttl = load_tex(DCT_TTL_RLE, DCT_TTL_RLE_LEN, DCT_TTL_W, DCT_TTL_H);
  make_hdr(&hdr_bga, PVR_TXRFMT_RGB565, DCT_BGA_W, DCT_BGA_H, tx_bga);
  make_hdr(&hdr_bgb, PVR_TXRFMT_ARGB1555, DCT_BGB_W, DCT_BGB_H, tx_bgb);
  make_hdr(&hdr_atl, PVR_TXRFMT_ARGB4444, DCT_ATL_W, DCT_ATL_H, tx_atl);
  make_hdr(&hdr_man, PVR_TXRFMT_ARGB4444, DCT_MAN_W, DCT_MAN_H, tx_man);
  make_hdr(&hdr_ttl, PVR_TXRFMT_RGB565, DCT_TTL_W, DCT_TTL_H, tx_ttl);
  ds_gfx_init();
  tx_ic_top = pvr_mem_malloc(256 * 256 * 2); tx_ic_bot = pvr_mem_malloc(256 * 256 * 2);
  if(tx_ic_top) make_hdr(&hdr_ic_top, PVR_TXRFMT_RGB565, 256, 256, tx_ic_top);
  if(tx_ic_bot) make_hdr(&hdr_ic_bot, PVR_TXRFMT_RGB565, 256, 256, tx_ic_bot);
}

int main(int argc, char **argv){
  (void)argc; (void)argv;
  gfx_init();
  sound_init();
  mx_meta_init(&stats);
  dc_save_load();                         /* before level-start rebases counters */
  recolor_man(outfit_sel);

  for(int k = 0; k < MX_NMAT; k++) MAXHP[k] = k ? (float)mx_material_hp(k) : 1.0f;
  load_level(0);
  uint32 prev = 0; int prev_trig = 0;
  while(1){
    sound_tick();
    music_update(mus_pref && !on_title && !in_intro && !paused && world.state == MX_PLAY);
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    if(st){
      uint32 pressed = st->buttons & ~prev; prev = st->buttons;
      if(in_intro){                                       /* a cutscene: A / B / START moves on (START skips), then the level that started it */
        int dn = ((pressed & CONT_A) ? 1 : 0) | ((pressed & CONT_B) ? 2 : 0) | ((pressed & CONT_START) ? 8 : 0);
        if(intro_tick(dn, 0, 0)){
          if(intro_mode() == 3 && intro_level == MX_CH3_START && !wolf_seen){ wolf_seen = 1; intro_start_wolf(); }       /* the chapter 2 closer hands over to the wolfman cutscene */
          else { in_intro = 0; load_level(intro_level); } }
      } else if(on_title){
        if(pressed & (CONT_DPAD_RIGHT | CONT_DPAD_LEFT)) title_lv = (title_lv + ((pressed & CONT_DPAD_RIGHT) ? 1 : MX_NUM_LEVELS - 1)) % MX_NUM_LEVELS;
        { int tl = (st->ltrig > 128 && !(prev_trig & 1)), tr = (st->rtrig > 128 && !(prev_trig & 2)); prev_trig = (st->ltrig > 128) | ((st->rtrig > 128) << 1);
        if(tl || tr){
          for(int k = 1; k <= MX_NUM_OUTFITS; k++){ int n = (outfit_sel + (tr ? k : MX_NUM_OUTFITS - k)) % MX_NUM_OUTFITS; if(mx_outfit_unlocked(&stats, n)){ outfit_sel = n; break; } }
          recolor_man(outfit_sel);
          dc_save_sync();
        } }
        if(pressed & CONT_Y){ nofight = !nofight; dc_save_sync(); }
        if(pressed & (CONT_START | CONT_A)){ on_title = 0; if(tx_ttl){ pvr_mem_free(tx_ttl); tx_ttl = 0; }
          if(title_lv == 0 && !intro_seen && tx_ic_top && tx_ic_bot){ intro_seen = 1; intro_level = 0; in_intro = 1; intro_start(); }
          else if(title_lv == MX_CH3_START && !wolf_seen && tx_ic_top && tx_ic_bot){ wolf_seen = 1; intro_level = title_lv; in_intro = 1; intro_start_wolf(); }
          else load_level(title_lv); }
      } else if(world.state != MX_PLAY){
        if(pressed & CONT_START){
          int next = world.state == MX_WIN ? world.level + 1 : world.level;
          int ce = (world.state == MX_WIN && next == MX_CH2_START) ? 1 : (world.state == MX_WIN && next == MX_CH3_START) ? 2 : 0;   /* the closing cutscene of the chapter just finished */
          if(ce && !(end_seen & (1 << ce)) && tx_ic_top && tx_ic_bot){ end_seen |= 1 << ce; intro_level = next; in_intro = 1; intro_start_end(ce); }
          else load_level(next); }
      } else if(paused){
        if(pressed & CONT_START) paused = 0;
        if(pressed & CONT_Y){ load_level(world.level); paused = 0; }
        if(pressed & CONT_X){ nofight = !nofight; mx_set_fight(&world, !nofight); dc_save_sync(); }
        if(pressed & CONT_B) mus_pref = !mus_pref;                                      /* B in the pause menu: music on / off */
      } else {
        if(pressed & CONT_START) paused = 1;
        if(pressed & CONT_Y) load_level(world.level);
        if(world.man.mode == MAN_AIM){
          world.valve_ok = dc_valve_equipped && mx_valve_unlocked(&stats);
          float jx = st->joyx / 127.0f, jy = st->joyy / 127.0f;
          if(fabsf(jx) < 0.12f) jx = 0;
          if(fabsf(jy) < 0.12f) jy = 0;
          if(pressed & CONT_X){
            int nx=world.pw_sel;
            for(int k=0;k<PW_COUNT;k++){nx=(nx+1)%PW_COUNT;if(nx==PW_NONE||world.pw_left[nx]>0)break;}
            if(nx==PW_NONE&&world.pw_sel)mx_select_power(&world,world.pw_sel);
            else if(nx){
              dc_valve_equipped=0;world.valve_ok=0;
              if(world.man.seated)mx_set_pull(&world,0,0);
              mx_select_power(&world,nx);
            }
          }
          if(!(world.man.seated && jx == 0 && jy == 0)) mx_set_pull(&world, jx * MX_PULL_MAX, jy * MX_PULL_MAX);   /* seated on the valve: a centred stick leaves him there */
          /* DOWN equips/unequips AIR; UP seats only after equipping. */
          if((pressed&CONT_DPAD_DOWN)&&mx_valve_unlocked(&stats)){
            dc_valve_equipped=!dc_valve_equipped;
            world.valve_ok=dc_valve_equipped;
            if(dc_valve_equipped&&world.pw_sel)mx_select_power(&world,world.pw_sel);
            if(!dc_valve_equipped&&world.man.seated)mx_set_pull(&world,0,0);
          }
          if((pressed&CONT_DPAD_UP)&&world.valve_ok)mx_valve_seat(&world);
          if(pressed & (CONT_A | CONT_B)){ mx_release(&world); hint_t = 0; }
        } else if(world.man.mode == MAN_WALK){                                  /* walk levels: steer him on foot */
          int ax = 0;
          if((st->buttons & CONT_DPAD_LEFT) || st->joyx < -40) ax--;
          if((st->buttons & CONT_DPAD_RIGHT) || st->joyx > 40) ax++;
          mx_walk_input(&world, ax, (st->buttons & (CONT_A | CONT_DPAD_UP)) != 0 || st->joyy < -90, (st->buttons & CONT_B) != 0, (st->buttons & (CONT_X | CONT_DPAD_DOWN)) != 0 || st->joyy > 90);
          { static int lwas; int l = st->ltrig > 128; if(l && !lwas) mx_walk_quit(&world); lwas = l; }
        } else if(pressed & (CONT_A | CONT_B)) mx_dive(&world);
      }
    }
    if(on_title){ title_t++; }
    else if(!paused){
      mx_update(&world, DT);
      handle_events();
      mx_meta_events(&stats, &world);
      for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ){ toast_push(a); } outfit_scan(0);
      { static int was = MX_PLAY; if(world.state != MX_PLAY && was == MX_PLAY){
          if(world.state == MX_WIN){ mx_meta_win(&stats, &world); for(int a; (a = mx_meta_next_unlock(&stats)) >= 0; ){ toast_push(a); } outfit_scan(0); }
          dc_save_sync();
        } was = world.state; }
      if(hint_t > 0) hint_t--;
      if(bank_toast_t > 0) bank_toast_t--;
      if(survivor_toast_t > 0) survivor_toast_t--;
      {
        int calm = st && world.state == MX_PLAY && world.man.mode == MAN_AIM && world.banner <= 0 && !st->buttons && abs(st->joyx) < 30 && abs(st->joyy) < 30;
        int strain = st && world.man.mode == MAN_AIM && (abs(st->joyx) > 90 || abs(st->joyy) > 90);
        fx_step(calm, strain);
      }
    }
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    cur_hdr = 0;
    if(in_intro){ draw_intro();
    } else if(on_title) draw_title(); else { draw_all(); if(paused){ text("PAUSED", 320, 200, 64, WHITE, 1, Z_HUD); text("START RESUME   Y RESTART   B MUSIC", 320, 270, 24, WHITE, 1, Z_HUD); { char pb[48]; snprintf(pb, sizeof pb, "X: LANDLORDS FIGHT BACK - %s", nofight ? "OFF" : "ON"); text(pb, 320, 310, 20, WHITE, 1, Z_HUD); } } }
    if(st&&st->ltrig>128&&st->rtrig>128){
      char stats_line[60];
      snprintf(stats_line,sizeof stats_line,"FPS %d.%d SIM %d.%d",
               (int)dc_fps.render_x10/10,(int)dc_fps.render_x10%10,
               (int)dc_fps.sim_x10/10,(int)dc_fps.sim_x10%10);
      quad(8,8,224,24,Z_HUD+4,argb(12,16,31,225));
      text(stats_line,120,12,15,argb(255,218,116,255),1,Z_HUD+5);
    }
    pvr_list_finish();
    pvr_scene_finish();
    dc_frame_complete(!on_title&&!paused&&!in_intro);
  }
  return 0;
}
