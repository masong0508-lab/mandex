/*
 * MANDEX DS - 2D software renderer toolkit (palette, tiles, font, sprites).
 *
 * Everything draws into a 256x192 8-bit RAM buffer (one per screen).  Nothing in
 * here touches DS hardware registers, so it is also what the host test harness
 * uses to dump PNGs of both screens.
 */
#ifndef MX_GFX_H
#define MX_GFX_H

#include <nds.h>
#include "mx.h"
#include "cloth.h"

#ifndef MX_FIXED
#error "the DS front-end only supports the fixed-point core (build with -DMX_FIXED)"
#endif

/* ---- palette layout (shared by both screens) ---- */
enum {
  C_CLEAR = 0,
  C_SKY0 = 1,                   /* 1..16 : sky gradient */
  C_CLOUD = 17, C_CLOUD2, C_SUN, C_SUNHALO, C_HILL1, C_HILL2, C_HILL3,
  C_GRASS, C_GRASS2, C_DIRT, C_DIRT2,
  C_SKIN, C_SKIN2, C_SKIN3, C_RED, C_RED2, C_WHITE, C_GOO, C_GOO2,
  C_HAIR, C_HAIR2, C_SUIT, C_SUIT2, C_BLACK, C_GOLD, C_GOLD2, C_PINK,
  C_FIRE, C_FIRE2, C_SMOKE, C_SMOKE2, C_UIBG, C_UIHI, C_UILINE,
  C_STAR, C_STAR2,
  C_WALL0, C_WALL1 = C_WALL0 + 1, C_WALL2, C_WALL3,          /* wallpaper bases   */
  C_WALLD0, C_WALLD1, C_WALLD2, C_WALLD3,                    /* wallpaper pattern */
  C_SHADOW, C_GRAY, C_DARK,
  C_MAT = 64,                   /* 64 + mat*5 + {HI,BASE,LO,CRACK,DARK} */
  C_MAN0 = 160                  /* 160.. : the man's own palette (art/manspr.h) */
};
enum { SH_HI, SH_BASE, SH_LO, SH_CRACK, SH_DARK };
#define MATPAL(m) (C_MAT + (m) * 5)

extern u16 gfx_pal[256];        /* RGB15 palette, copied to both engines by main.c */

/* A view = one screen + how the world maps onto it.
   pixel = (cell * ppc) - cx   (cx, cy are whole pixels so the camera scrolls smoothly) */
typedef struct { u8 *buf; int ppc, cx, cy; } View;
static inline int vx(const View *v, real c){ return ((c * v->ppc) >> 12) - v->cx; }
static inline int vy(const View *v, real c){ return ((c * v->ppc) >> 12) - v->cy; }

void gfx_init(void);                                  /* palette, tiles, backgrounds */

/* primitives (all clip) */
void gfx_px(u8 *b, int x, int y, u8 c);
void gfx_rect(u8 *b, int x, int y, int w, int h, u8 c);
void gfx_ell(u8 *b, int cx, int cy, int rx, int ry, u8 c);
void gfx_line(u8 *b, int x0, int y0, int x1, int y1, u8 c);
void gfx_tri(u8 *b, int x0, int y0, int x1, int y1, int x2, int y2, u8 c);   /* flat filled triangle */
void gfx_tline(u8 *b, int x0, int y0, int x1, int y1, int th, u8 c);
void gfx_dither(u8 *b, int x, int y, int w, int h, u8 c);   /* checkerboard fill (translucency) */

/* text: 3x5 font, `sc` = integer scale */
int  gfx_text_w(const char *s, int sc);
void gfx_text(u8 *b, int x, int y, const char *s, u8 col, int sc);
void gfx_text_o(u8 *b, int x, int y, const char *s, u8 col, u8 outline, int sc);
void gfx_text_c(u8 *b, int cx, int y, const char *s, u8 col, u8 outline, int sc);
const char *gfx_int(int v);                           /* static buffer, rotating */

/* scene */
void gfx_bg(const View *v, int parallax);             /* sky + hills, scrolls at half speed */
void gfx_ground(const View *v);
void gfx_tile(const View *v, int mat, int dmg, int var, int px, int py);
void gfx_wall(const View *v, int pal, int px, int py);

/* sprites.  (x,y) = centre; ppc = pixels per cell the sprite is drawn at */
enum { POSE_IDLE, POSE_FLY, POSE_DIVE, POSE_STUCK, POSE_DRAG, POSE_FLEX, POSE_SAD };
/* the man's jiggle at the drawn zoom: belly (bx,by) and rear (rx,ry) offsets in pixels, squash sx/sy in 1/256 */
typedef struct { int bx, by, rx, ry, sx, sy; } ManJig;
void spr_man_frame(u8 *b, int x, int y, int ppc, int face, int frame, const ManJig *jg);   /* frame = MSF_* */
/* the towel (core/cloth.c) of the man whose centre is at x,y: pass 0 = the part behind him (draw before his sprite), 1 = the part in front */
const short *spr_man_hand(int frame);          /* where the raised hand is in this frame (quarter design units x,y,z; x == CL_NONE: no towel) */
void gfx_cloth(u8 *b, const Cloth *c, int x, int y, int ppc, int face, int pass);
void spr_man(u8 *b, int x, int y, int ppc, int face, int pose, int ph);
void spr_man_j(u8 *b, int x, int y, int ppc, int face, int pose, int ph, const ManJig *jg);
void spr_man_bust(u8 *b, int x, int y, int ppc);   /* x = centre, y = top */
void spr_landlord(u8 *b, int x, int y, int ppc, int ph, int scared);
void spr_hat(u8 *b, int x, int y, int sc, int mode); /* 0 = real black hat, 1 = HUD icon alive, 2 = HUD icon evicted */
void spr_star(u8 *b, int x, int y, int sc, u8 fill, u8 edge);

/* helpers */
static inline int gfx_clamp(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }

#endif
