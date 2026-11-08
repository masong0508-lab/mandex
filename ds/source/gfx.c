/*
 * MANDEX DS - software 2D renderer toolkit.  See gfx.h.
 *
 * Integer-only (the ARM9 has no FPU).  The expensive parts - material tiles and the
 * parallax backdrop - are generated once at start-up, so the per-frame work is just
 * table-driven blits and a handful of small ellipses for the sprites.
 */
#include <string.h>
#include "gfx.h"
#include "manspr.h"

u16 gfx_pal[256];

/* ------------------------------------------------------------------ helpers */
static unsigned isq(unsigned n){
  unsigned r = 0, b = 1u << 30;
  while(b > n) b >>= 2;
  while(b){ if(n >= r + b){ n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return r;
}
static void setc(int i, int r, int g, int b){ gfx_pal[i] = (u16)RGB15(gfx_clamp(r, 0, 255) >> 3, gfx_clamp(g, 0, 255) >> 3, gfx_clamp(b, 0, 255) >> 3); }

/* ---------------------------------------------------------------- primitives */
void gfx_px(u8 *b, int x, int y, u8 c){ if((unsigned)x < 256u && (unsigned)y < 192u) b[y * 256 + x] = c; }

void gfx_rect(u8 *b, int x, int y, int w, int h, u8 c){
  int x1 = x + w, y1 = y + h;
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x1 > 256) x1 = 256;
  if(y1 > 192) y1 = 192;
  if(x1 <= x || y1 <= y) return;
  for(int j = y; j < y1; j++) memset(&b[j * 256 + x], c, (size_t)(x1 - x));
}

static void hspan(u8 *b, int x0, int x1, int y, u8 c){
  if((unsigned)y >= 192u) return;
  if(x0 < 0) x0 = 0;
  if(x1 > 255) x1 = 255;
  if(x1 >= x0) memset(&b[y * 256 + x0], c, (size_t)(x1 - x0 + 1));
}

void gfx_ell(u8 *b, int cx, int cy, int rx, int ry, u8 c){
  if(rx <= 0 || ry <= 0){ if(rx > 0 || ry > 0) gfx_px(b, cx, cy, c); return; }
  int D = ry * ry + ry;
  int den = 8 * ry;
  for(int j = -ry; j <= ry; j++){
    int e = (int)(((unsigned)rx * isq((unsigned)(D - j * j) * 64u)) / (unsigned)den);
    if(e > rx) e = rx;
    hspan(b, cx - e, cx + e, cy + j, c);
  }
}

void gfx_line(u8 *b, int x0, int y0, int x1, int y1, u8 c){
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for(int n = 0; n < 600; n++){
    gfx_px(b, x0, y0, c);
    if(x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if(e2 >= dy){ err += dy; x0 += sx; }
    if(e2 <= dx){ err += dx; y0 += sy; }
  }
}

void gfx_tri(u8 *b, int x0, int y0, int x1, int y1, int x2, int y2, u8 c){
  int t;
  if(y1 < y0){ t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if(y2 < y0){ t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
  if(y2 < y1){ t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if(y2 == y0){                                             /* flat: one row */
    int a = x0 < x1 ? x0 : x1, e = x0 > x1 ? x0 : x1;
    if(x2 < a) a = x2;
    if(x2 > e) e = x2;
    gfx_rect(b, a, y0, e - a + 1, 1, c);
    return;
  }
  for(int y = y0; y <= y2; y++){
    int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0), xb;
    if(y < y1) xb = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    else xb = y2 == y1 ? x1 : x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    if(xa > xb){ t = xa; xa = xb; xb = t; }
    gfx_rect(b, xa, y, xb - xa + 1, 1, c);
  }
}

void gfx_tline(u8 *b, int x0, int y0, int x1, int y1, int th, u8 c){
  if(th <= 1){ gfx_line(b, x0, y0, x1, y1, c); return; }
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, h = th / 2;
  for(int n = 0; n < 600; n++){
    gfx_rect(b, x0 - h, y0 - h, th, th, c);
    if(x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if(e2 >= dy){ err += dy; x0 += sx; }
    if(e2 <= dx){ err += dx; y0 += sy; }
  }
}

void gfx_dither(u8 *b, int x, int y, int w, int h, u8 c){
  for(int j = y; j < y + h; j++) for(int i = x + ((j ^ x) & 1); i < x + w; i += 2) gfx_px(b, i, j, c);
}

/* --------------------------------------------------------------------- font */
#define G(a, b, c, d, e) ((u16)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e)))
static const u16 FONT[] = {
  G(2,5,7,5,5), G(6,5,6,5,6), G(3,4,4,4,3), G(6,5,5,5,6), G(7,4,6,4,7), G(7,4,6,4,4), G(3,4,5,5,3), G(5,5,7,5,5), G(7,2,2,2,7),
  G(1,1,1,5,2), G(5,5,6,5,5), G(4,4,4,4,7), G(5,7,7,5,5), G(6,5,5,5,5), G(2,5,5,5,2), G(6,5,6,4,4), G(2,5,5,6,3), G(6,5,6,5,5),
  G(3,4,2,1,6), G(7,2,2,2,2), G(5,5,5,5,7), G(5,5,5,5,2), G(5,5,7,7,5), G(5,5,2,5,5), G(5,5,2,2,2), G(7,1,2,4,7),   /* A..Z */
  G(7,5,5,5,7), G(2,6,2,2,7), G(6,1,2,4,7), G(6,1,2,1,6), G(5,5,7,1,1), G(7,4,6,1,6), G(3,4,7,5,7), G(7,1,2,2,2), G(7,5,7,5,7), G(7,5,7,1,6), /* 0..9 */
  G(0,0,0,0,2), G(2,2,2,0,2), G(0,0,7,0,0), G(0,2,0,2,0), G(0,2,7,2,0), G(6,1,2,0,2), G(1,1,2,4,4), G(3,6,2,3,6), G(2,2,0,0,0), G(0,0,0,2,4),
  G(5,1,2,4,5), G(1,2,4,2,1), G(4,2,1,2,4)    /* . ! - : + ? / $ ' , % < > */
};
static int glyph_index(int c){
  if(c >= 'a' && c <= 'z') c -= 32;
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= '0' && c <= '9') return 26 + c - '0';
  switch(c){
  case '.': return 36; case '!': return 37; case '-': return 38; case ':': return 39; case '+': return 40;
  case '?': return 41; case '/': return 42; case '$': return 43; case '\'': return 44; case ',': return 45;
  case '%': return 46; case '<': return 47; case '>': return 48;
  }
  return -1;
}
int gfx_text_w(const char *s, int sc){ int n = 0; while(*s++) n++; return n ? n * 4 * sc - sc : 0; }
void gfx_text(u8 *b, int x, int y, const char *s, u8 col, int sc){
  for(; *s; s++, x += 4 * sc){
    int gi = glyph_index(*s);
    if(gi < 0) continue;
    u16 g = FONT[gi];
    for(int r = 0; r < 5; r++) for(int k = 0; k < 3; k++)
      if(g & (1 << (14 - r * 3 - k))) gfx_rect(b, x + k * sc, y + r * sc, sc, sc, col);
  }
}
void gfx_text_o(u8 *b, int x, int y, const char *s, u8 col, u8 outline, int sc){
  gfx_text(b, x - 1, y, s, outline, sc); gfx_text(b, x + 1, y, s, outline, sc);
  gfx_text(b, x, y - 1, s, outline, sc); gfx_text(b, x, y + 1, s, outline, sc);
  gfx_text(b, x + 1, y + 1, s, outline, sc);
  gfx_text(b, x, y, s, col, sc);
}
void gfx_text_c(u8 *b, int cx, int y, const char *s, u8 col, u8 outline, int sc){
  gfx_text_o(b, cx - gfx_text_w(s, sc) / 2, y, s, col, outline, sc);
}
const char *gfx_int(int v){
  static char buf[4][16];
  static int rot;
  char *o = buf[rot = (rot + 1) & 3], tmp[12];
  int n = 0, neg = v < 0, i = 0;
  unsigned u = neg ? (unsigned)(-v) : (unsigned)v;
  do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while(u && n < 11);
  if(neg) o[i++] = '-';
  while(n) o[i++] = tmp[--n];
  o[i] = 0;
  return o;
}

/* -------------------------------------------------------------- palette/tiles */
static u8 tiles[3][MX_NMAT][3][2][16];      /* [ppc-2][material][damage][variant] */
static u8 walltiles[3][4][16];
#define BGW 512
#define BGH 208
static u8 bgs[4][BGW * BGH];                /* four byte-shifted copies -> aligned memcpy for any scroll offset */

static void make_palette(void){
  memset(gfx_pal, 0, sizeof gfx_pal);
  for(int i = 0; i < 16; i++) setc(C_SKY0 + i, 60 + i * 9, 128 + i * 6, 228 + i * 1);   /* deep blue -> pale horizon */
  setc(C_CLOUD, 255, 255, 255);  setc(C_CLOUD2, 205, 222, 245);
  setc(C_SUN, 255, 240, 140);    setc(C_SUNHALO, 235, 240, 235);
  setc(C_HILL1, 120, 170, 190);  setc(C_HILL2, 80, 150, 90);  setc(C_HILL3, 150, 200, 215);
  setc(C_GRASS, 90, 190, 60);    setc(C_GRASS2, 55, 140, 45);
  setc(C_DIRT, 122, 82, 50);     setc(C_DIRT2, 96, 62, 38);
  setc(C_SKIN, 244, 183, 142);   setc(C_SKIN2, 205, 135, 100);  setc(C_SKIN3, 255, 215, 180);
  setc(C_RED, 224, 36, 58);      setc(C_RED2, 160, 20, 40);     setc(C_WHITE, 255, 255, 255);
  setc(C_GOO, 125, 255, 90);     setc(C_GOO2, 70, 190, 50);
  setc(C_HAIR, 74, 45, 26);      setc(C_HAIR2, 40, 24, 14);
  setc(C_SUIT, 74, 85, 104);     setc(C_SUIT2, 48, 56, 72);     setc(C_BLACK, 14, 14, 20);
  setc(C_GOLD, 255, 214, 60);    setc(C_GOLD2, 190, 140, 40);   setc(C_PINK, 255, 126, 176);
  setc(C_FIRE, 255, 170, 40);    setc(C_FIRE2, 255, 90, 20);
  setc(C_SMOKE, 150, 150, 160);  setc(C_SMOKE2, 90, 90, 100);
  setc(C_UIBG, 20, 24, 40);      setc(C_UIHI, 70, 80, 120);     setc(C_UILINE, 130, 150, 200);
  setc(C_STAR, 255, 224, 70);    setc(C_STAR2, 200, 130, 20);
  setc(C_WALL0, 238, 214, 170);  setc(C_WALL1, 200, 225, 205);  setc(C_WALL2, 225, 190, 205);  setc(C_WALL3, 190, 205, 235);
  setc(C_WALLD0, 218, 190, 140); setc(C_WALLD1, 175, 205, 180); setc(C_WALLD2, 205, 165, 185); setc(C_WALLD3, 165, 180, 215);
  setc(C_SHADOW, 40, 60, 40);    setc(C_GRAY, 160, 160, 170);   setc(C_DARK, 30, 30, 40);
  for(int m = 1; m < MX_NMAT; m++){
    const uint8_t *c = mx_material_rgb(m);
    int r = c[0], g = c[1], b = c[2], p = MATPAL(m);
    setc(p + SH_HI,   r * 5 / 4 + 20, g * 5 / 4 + 20, b * 5 / 4 + 20);
    setc(p + SH_BASE, r, g, b);
    setc(p + SH_LO,   r * 7 / 10, g * 7 / 10, b * 7 / 10);
    setc(p + SH_CRACK, r * 3 / 10, g * 3 / 10, b * 3 / 10);
    setc(p + SH_DARK, r * 8 / 10, g * 8 / 10, b * 8 / 10);
  }
  for(int k = 0; k < MS_NPAL; k++) setc(C_MAN0 + k, MS_PAL[k][0], MS_PAL[k][1], MS_PAL[k][2]);   /* the man's own colours */
  setc(MATPAL(MX_GLASS) + SH_CRACK, 235, 250, 255);         /* glass cracks show white */
  setc(MATPAL(MX_TNT) + SH_HI, 250, 235, 180);              /* TNT's cream label stripe */
}

static void make_tiles(void){
  for(int n = 2; n <= 4; n++)
  for(int m = 1; m < MX_NMAT; m++) for(int d = 0; d < 3; d++) for(int var = 0; var < 2; var++){
    u8 *t = tiles[n - 2][m][d][var];
    int p = MATPAL(m);
    for(int y = 0; y < n; y++) for(int x = 0; x < n; x++){
      int c = SH_BASE;
      if(y == 0) c = SH_HI; else if(y == n - 1) c = SH_LO;
      if(n >= 3 && x == n - 1 && y > 0) c = SH_LO;
      switch(m){
      case MX_WOOD:  if(n >= 3 && y > 0 && y < n - 1 && ((x + y * 2 + var) & 3) == 0) c = SH_LO; break;
      case MX_BRICK: if(n >= 3 && y < n - 1 && x == (var ? 1 : n - 2)) c = SH_LO;
                     if(y == n - 1) c = SH_LO;
                     break;
      case MX_STONE: if(((x * 7 + y * 13 + var * 5) % 5) == 0) c = SH_LO;
                     if(x == 0 && y == 1 && n >= 3) c = SH_HI;
                     break;
      case MX_GLASS: c = SH_BASE; if(x == y || (n >= 4 && x == y + 1 && y == 0)) c = SH_HI;
                     if(n >= 3 && (x == n - 1 || y == n - 1)) c = SH_LO;
                     break;
      case MX_TNT:   if(y == n / 2 && n >= 3) c = SH_HI; break;
      case MX_STEEL: if(n >= 3 && x == n - 2 && y == 1) c = SH_HI; if(n >= 3 && x == 1 && y == n - 2) c = SH_HI; break;
      case MX_RUBBER: c = SH_BASE;                                   /* glossy, rounded: bright blob, dark rim */
                      if(n >= 3 && x == 1 && y == 1) c = SH_HI;
                      if(n == 2 && x == 0 && y == 0) c = SH_HI;
                      if(n >= 3 && (x == n - 1 || y == n - 1)) c = SH_LO;
                      break;
      case MX_SAND:  c = SH_BASE;                                    /* speckled grains, no bevel */
                      if(((x * 5 + y * 11 + var * 3) % 4) == 0) c = SH_HI;
                      else if(((x * 3 + y * 7 + var * 5) % 5) == 0) c = SH_LO;
                      break;
      }
      if(d == 1 && ((x + y * 3 + var) % 7) == 2) c = SH_CRACK;
      if(d == 2){
        if(c != SH_CRACK) c = (c == SH_HI ? SH_BASE : SH_DARK);
        if(((x + y * 2 + var) % 3) == 1) c = SH_CRACK;
      }
      t[y * n + x] = (u8)(p + c);
    }
  }
  for(int n = 2; n <= 4; n++) for(int w = 0; w < 4; w++)
    for(int y = 0; y < n; y++) for(int x = 0; x < n; x++)
      walltiles[n - 2][w][y * n + x] = (x == n - 1 && y == n - 1 && n >= 3) || (n == 2 && x == 0 && y == 0) ? (u8)(C_WALLD0 + w) : (u8)(C_WALL0 + w);
}

static int tri(int x, int p){ x %= p; if(x < 0) x += p; return x < p / 2 ? x : p - x; }

static void make_bg(void){
  u8 *bg = bgs[0];
  for(int y = 0; y < BGH; y++){
    int yy = y > 191 ? 191 : y, band = yy / 12, r = yy % 12;
    for(int x = 0; x < BGW; x++){
      int c = C_SKY0 + band;
      if(r >= 9 && band < 15 && ((x + y) & 1) == (r & 1)) c++;          /* dithered band blend */
      bg[y * BGW + x] = (u8)c;
    }
  }
  /* sun with a dithered halo */
  for(int y = 0; y < 100; y++) for(int x = 300; x < 460; x++){
    int dx = x - 380, dy = y - 42, d2 = dx * dx + dy * dy;
    if(d2 < 15 * 15) bg[y * BGW + x] = C_SUN;
    else if(d2 < 26 * 26 && ((x + y) & 1)) bg[y * BGW + x] = C_SUNHALO;
  }
  /* clouds */
  static const short CL[][3] = { {50, 34, 1}, {170, 62, 0}, {290, 24, 1}, {430, 70, 0}, {500, 30, 1}, {110, 96, 0} };
  for(unsigned i = 0; i < sizeof CL / sizeof CL[0]; i++){
    int x = CL[i][0], y = CL[i][1], s = CL[i][2] ? 14 : 10;
    for(int j = -s / 2; j <= s / 2; j++) for(int k = -s * 2; k <= s * 2; k++){
      int a = k * k * 4 / (s * s * 16) + j * j * 4 / (s * s);
      int a2 = (k - s) * (k - s) * 4 / (s * s * 12) + (j + 2) * (j + 2) * 4 / (s * s);
      int a3 = (k + s) * (k + s) * 4 / (s * s * 12) + (j + 2) * (j + 2) * 4 / (s * s);
      if(a <= 1 || a2 <= 1 || a3 <= 1){
        int px = x + k, py = y + j;
        if(px >= 0 && px < BGW && py >= 0 && py < BGH) bg[py * BGW + px] = (u8)(j > s / 4 ? C_CLOUD2 : C_CLOUD);
      }
    }
  }
  /* two hill layers */
  for(int x = 0; x < BGW; x++){
    int h1 = 126 - (tri(x, 150) * 3 / 5 + tri(x + 40, 53) / 3);
    int h2 = 148 - (tri(x + 90, 110) / 2 + tri(x, 37) / 3);
    for(int y = h1; y < BGH; y++) bg[y * BGW + x] = (u8)(y == h1 ? C_HILL3 : C_HILL1);
    for(int y = h2; y < BGH; y++) bg[y * BGW + x] = (u8)(y == h2 ? C_GRASS : ((y + x / 3) % 9 == 0 ? C_GRASS2 : C_HILL2));
  }
  /* shifted copies: bgs[c][i] = bg[i + c] */
  for(int c = 1; c < 4; c++) memcpy(bgs[c], bg + c, BGW * BGH - c);
}

void gfx_init(void){
  make_palette();
  make_tiles();
  make_bg();
}

/* ---------------------------------------------------------------------- scene */
void gfx_bg(const View *v, int parallax){
  int xoff = 0, roff = 0;
  if(parallax){
    xoff = gfx_clamp(v->cx >> 1, 0, BGW - 256 - 4);
    roff = gfx_clamp(v->cy >> 2, 0, BGH - 192);
  }
  const u8 *src = bgs[xoff & 3] + (xoff & ~3);
  for(int y = 0; y < 192; y++) memcpy(&v->buf[y * 256], src + (y + roff) * BGW, 256);
}

void gfx_ground(const View *v){
  int gy = MX_ROWS * v->ppc - v->cy;
  if(gy >= 192) return;
  gfx_rect(v->buf, 0, gy, 256, 2, C_GRASS);
  gfx_rect(v->buf, 0, gy + 2, 256, 2, C_GRASS2);
  for(int y = gy + 4; y < 192; y++){
    memset(&v->buf[y * 256], (y - gy) & 4 ? C_DIRT2 : C_DIRT, 256);
    for(int x = ((y * 37) & 15); x < 256; x += 23) gfx_px(v->buf, x, y, C_DIRT2);   /* pebbles */
  }
}

void gfx_tile(const View *v, int mat, int dmg, int var, int px, int py){
  int n = v->ppc;
  if(px + n <= 0 || py + n <= 0 || px >= 256 || py >= 192) return;
  const u8 *t = tiles[n - 2][mat][dmg][var & 1];
  u8 *d = &v->buf[py * 256 + px];
  if(px >= 0 && py >= 0 && px + n <= 256 && py + n <= 192){
    for(int j = 0; j < n; j++, d += 256, t += n) for(int i = 0; i < n; i++) d[i] = t[i];
  } else {
    for(int j = 0; j < n; j++) for(int i = 0; i < n; i++) gfx_px(v->buf, px + i, py + j, t[j * n + i]);
  }
}

void gfx_wall(const View *v, int pal, int px, int py){
  int n = v->ppc;
  if(px + n <= 0 || py + n <= 0 || px >= 256 || py >= 192) return;
  const u8 *t = walltiles[n - 2][pal & 3];
  for(int j = 0; j < n; j++) for(int i = 0; i < n; i++) gfx_px(v->buf, px + i, py + j, t[j * n + i]);
}

/* -------------------------------------------------------------------- sprites */
#define S(n) (((n) * ppc) / 8)            /* eighths of a cell -> pixels */
#define SM(n) (S(n) < 1 ? 1 : S(n))

/* The man: pixel-art frames generated from the HTML prototype's own drawing (tools/gen_manspr.py -> art/manspr.h).
   Frames are authored at 4 px per cell; other zooms scale them with nearest-neighbour stepping.
   Jiggle: the belly and rear blobs of the frame are pushed around by the core's spring state (inverse-mapped with a
   quadratic falloff), and a hard hit squashes the whole figure along the impact. */
#define JM 9                                    /* margin (px) the jiggle may spill outside the frame */
static void blit_frame(u8 *b, const MsFrame *fr, int x, int y, int ppc, int f, int rows, const ManJig *jg){
  int w = fr->w, h = rows > 0 && rows < fr->h ? rows : fr->h;
  int dw = w * ppc / 4, dh = h * ppc / 4;
  if(dw < 1 || dh < 1) return;
  int ax = (f > 0 ? fr->ax : w - 1 - fr->ax) * ppc / 4, ay = (rows > 0 ? 0 : fr->ay) * ppc / 4;   /* a cropped frame is anchored at its top */
  int x0 = x - ax, y0 = y - ay;
  int live = jg && (jg->bx | jg->by | jg->rx | jg->ry | jg->sx | jg->sy) && rows <= 0;
  int m = live ? JM : 0;
  if(x0 - m >= 256 || y0 - m >= 192 || x0 + dw + m <= 0 || y0 + dh + m <= 0) return;
  if(!live){
    for(int j = 0; j < dh; j++){
      int py = y0 + j;
      if((unsigned)py >= 192u) continue;
      const u8 *row = fr->px + (j * 4 / ppc) * w;
      u8 *d = &b[py * 256];
      for(int i = 0; i < dw; i++){
        int px = x0 + i;
        if((unsigned)px >= 256u) continue;
        int sx = i * 4 / ppc;
        u8 c = row[f > 0 ? sx : w - 1 - sx];
        if(c) d[px] = (u8)(C_MAN0 + c - 1);
      }
    }
    return;
  }
  /* jiggling path: per-column / per-row source maps (with squash about the anchor), then a displacement field */
  static short cm[2 * JM + 160], rm[2 * JM + 160];
  int nw = dw + 2 * m, nh = dh + 2 * m;
  if(nw > (int)(sizeof cm / sizeof *cm) || nh > (int)(sizeof rm / sizeof *rm)) return;
  int ss = jg->sx ? 256 + jg->sx : 256, st = jg->sy ? 256 + jg->sy : 256;
  for(int i = 0; i < nw; i++){
    int u = (i - m - ax) * 256 / ss + ax;                          /* dest column -> unsquashed dest column */
    cm[i] = (short)((u < 0 || u >= dw) ? -1 : (f > 0 ? u * 4 / ppc : w - 1 - u * 4 / ppc));
  }
  for(int j = 0; j < nh; j++){
    int u = (j - m - ay) * 256 / st + ay;
    rm[j] = (short)((u < 0 || u >= dh) ? -1 : u * 4 / ppc);
  }
  int bcx = (f > 0 ? fr->bcx : w - 1 - fr->bcx) * ppc / 4, bcy = fr->bcy * ppc / 4, br = fr->brad * ppc / 4; if(br < 2) br = 2;
  int rcx = (f > 0 ? fr->rcx : w - 1 - fr->rcx) * ppc / 4, rcy = fr->rcy * ppc / 4, rr = fr->rrad * ppc / 4; if(rr < 2) rr = 2;
  int binv = 65536 / (br * br), rinv = 65536 / (rr * rr), b2 = br * br, r2 = rr * rr;
  for(int j = 0; j < nh; j++){
    int py = y0 - m + j;
    if((unsigned)py >= 192u) continue;
    int dyb = j - m - bcy, dyr = j - m - rcy;
    u8 *d = &b[py * 256];
    for(int i = 0; i < nw; i++){
      int px = x0 - m + i;
      if((unsigned)px >= 256u) continue;
      int dxb = i - m - bcx, dxr = i - m - rcx, ox = 0, oy = 0;
      int d2 = dxb * dxb + dyb * dyb;
      if(d2 < b2){ int wgt = 256 - ((d2 * binv) >> 8); ox += (jg->bx * wgt) >> 8; oy += (jg->by * wgt) >> 8; }
      d2 = dxr * dxr + dyr * dyr;
      if(d2 < r2){ int wgt = 256 - ((d2 * rinv) >> 8); ox += (jg->rx * wgt) >> 8; oy += (jg->ry * wgt) >> 8; }
      int si = i - ox, sj = j - oy;
      if(si < 0 || sj < 0 || si >= nw || sj >= nh) continue;
      int sx = cm[si], sy = rm[sj];
      if(sx < 0 || sy < 0) continue;
      u8 c = fr->px[sy * w + sx];
      if(c) d[px] = (u8)(C_MAN0 + c - 1);
    }
  }
}

/* design units (20.12) -> pixels at this zoom: the sprites are drawn at 0.36 px per unit at 4 px per cell, so 0.09 px per unit per ppc */
static int cpx(real v, int ppc){ return (int)((((int64_t)v * (ppc * 369)) + (1LL << 23)) >> 24); }

const short *spr_man_hand(int frame){ return MS_HAND[frame]; }

void gfx_cloth(u8 *b, const Cloth *c, int x, int y, int ppc, int f, int pass){
  ClothQuad q[CL_NQ];
  int n = cloth_quads(c, q);
  const u8 ol = (u8)(C_MAN0 + MS_CL_OUT - 1);
  for(int k = 0; k < n; k++){
    const ClothQuad *o = &q[k];
    if((o->z >= 0) != (pass != 0)) continue;
    int sx[4], sy[4];
    for(int m = 0; m < 4; m++){ int dx = cpx(o->x[m], ppc); sx[m] = x + (f >= 0 ? dx : -dx); sy[m] = y + cpx(o->y[m], ppc); }
    u8 col = (u8)(C_MAN0 + MS_CL_COL[o->face][o->stripe][o->shade] - 1);
    gfx_tri(b, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], col);
    gfx_tri(b, sx[1], sy[1], sx[3], sy[3], sx[2], sy[2], col);
    if(o->edge & 1) gfx_line(b, sx[0], sy[0], sx[2], sy[2], ol);
    if(o->edge & 2) gfx_line(b, sx[1], sy[1], sx[3], sy[3], ol);
    if(o->edge & 4) gfx_line(b, sx[0], sy[0], sx[1], sy[1], ol);
    if(o->edge & 8) gfx_line(b, sx[2], sy[2], sx[3], sy[3], ol);
  }
}

void spr_man_frame(u8 *b, int x, int y, int ppc, int f, int frame, const ManJig *jg){
  blit_frame(b, &MS_FRAMES[frame], x, y, ppc, f, 0, jg);
}

void spr_man(u8 *b, int x, int y, int ppc, int f, int pose, int ph){ spr_man_j(b, x, y, ppc, f, pose, ph, 0); }

void spr_man_j(u8 *b, int x, int y, int ppc, int f, int pose, int ph, const ManJig *jg){
  int sw = (ph >> 2) & 1, slow = (ph >> 4) & 1, fr;
  switch(pose){
  case POSE_FLY:   fr = sw ? MSF_FLY_A : MSF_FLY_B; break;
  case POSE_DIVE:  fr = MSF_DIVE; break;
  case POSE_STUCK: fr = ((ph >> 3) & 1) ? MSF_STUCK_A : MSF_STUCK_B; break;
  case POSE_DRAG:  fr = MSF_DRAG; break;
  case POSE_FLEX:  fr = slow ? MSF_FLEX_A : MSF_FLEX_B; break;
  case POSE_SAD:   fr = MSF_SAD; break;
  default:         fr = slow ? MSF_IDLE_A : MSF_IDLE_B; break;
  }
  if(pose == POSE_STUCK){                                        /* sticky goo coat + drips under him */
    gfx_ell(b, x, y + S(6), S(26), S(20), C_GOO2);
    gfx_ell(b, x, y + S(4), S(23), S(17), C_GOO);
    for(int k = 0; k < 5; k++){
      int len = 5 + ((k * 7 + (ph >> 2)) % 8);
      gfx_rect(b, x + S(-18 + k * 9), y + S(16), SM(3), S(len), C_GOO);
      gfx_px(b, x + S(-18 + k * 9) + (SM(3) > 1 ? 1 : 0), y + S(16 + len), C_GOO2);
    }
  }
  blit_frame(b, &MS_FRAMES[fr], x, y, ppc, f, 0, jg);
}

void spr_man_bust(u8 *b, int x, int y, int ppc){         /* head + shoulders only, for the HUD strip */
  blit_frame(b, &MS_FRAMES[MSF_IDLE_B], x, y, ppc, 1, 20, 0);
}

void spr_landlord(u8 *b, int x, int y, int ppc, int ph, int scared){
  int bob = (ph >> 4) & 1 && ppc >= 4 ? 1 : 0;
  y += bob;
  gfx_rect(b, x - S(8), y + S(10), SM(6), SM(7), C_SUIT2);                  /* legs */
  gfx_rect(b, x + S(2), y + S(10), SM(6), SM(7), C_SUIT2);
  gfx_rect(b, x - S(10), y + S(16), SM(9), SM(2), C_BLACK);                 /* shoes */
  gfx_rect(b, x + S(1), y + S(16), SM(9), SM(2), C_BLACK);
  gfx_ell(b, x, y + S(3), S(14), S(12), C_SUIT2);                           /* plump suit */
  gfx_ell(b, x - S(1), y + S(2), S(12), S(11), C_SUIT);
  gfx_rect(b, x - S(3), y - S(8), SM(6), S(11), C_WHITE);                   /* shirt */
  gfx_rect(b, x - S(1), y - S(6), SM(2), S(11), C_RED);                     /* tie */
  gfx_tline(b, x - S(13), y - S(3), x - S(14), y + S(9), SM(3), C_SUIT);    /* arms */
  gfx_tline(b, x + S(13), y - S(3), x + S(15), y + S(9), SM(3), C_SUIT);
  gfx_rect(b, x + S(12), y + S(9), SM(10), SM(7), C_GOLD2);                 /* briefcase of rent money */
  gfx_rect(b, x + S(12), y + S(9), SM(10), 1, C_GOLD);
  int hy = y - S(16);
  gfx_ell(b, x, hy, S(9), S(9), C_SKIN);
  gfx_ell(b, x - S(5), hy + S(2), SM(2), SM(2), C_SKIN3);                   /* rosy cheeks */
  gfx_ell(b, x + S(5), hy + S(2), SM(2), SM(2), C_SKIN3);
  if(scared){
    gfx_px(b, x - S(3), hy - S(1), C_WHITE); gfx_px(b, x + S(3), hy - S(1), C_WHITE);
    gfx_px(b, x - S(3) + 1, hy - S(1), C_BLACK); gfx_px(b, x + S(3) - 1, hy - S(1), C_BLACK);
    gfx_ell(b, x, hy + S(5), SM(2), SM(2), C_BLACK);
  } else {
    gfx_px(b, x - S(3), hy - S(1), C_BLACK); gfx_px(b, x + S(3), hy - S(1), C_BLACK);
    gfx_rect(b, x - S(2), hy + S(5), S(4), 1, C_BLACK);
  }
  if(ppc >= 3) gfx_ell(b, x + S(3), hy - S(1), SM(3), SM(3), C_GOLD);        /* monocle */
  if(ppc >= 3) gfx_px(b, x + S(3), hy - S(1), scared ? C_WHITE : C_SKIN);
  gfx_rect(b, x - S(6), hy + S(2), S(12), SM(2), C_GRAY);                    /* villain 'stache */
  gfx_px(b, x - S(7), hy + S(1), C_GRAY); gfx_px(b, x + S(6), hy + S(1), C_GRAY);
  gfx_rect(b, x - S(13), hy - S(10), S(26), SM(3), C_BLACK);                 /* top hat */
  gfx_rect(b, x - S(8), hy - S(24), S(16), S(14), C_BLACK);
  gfx_rect(b, x - S(8), hy - S(13), S(16), SM(3), C_GOLD);
  gfx_rect(b, x - S(7), hy - S(23), SM(2), S(8), C_SUIT2);                   /* hat sheen */
}

void spr_hat(u8 *b, int x, int y, int sc, int mode){
  u8 bl = mode == 2 ? C_UIHI : mode == 1 ? C_WHITE : C_BLACK, go = mode == 2 ? C_UILINE : C_GOLD;
  gfx_rect(b, x - 2 * sc, y + 2 * sc, 5 * sc, sc, bl);
  gfx_rect(b, x - sc, y - sc, 3 * sc, 3 * sc, bl);
  gfx_rect(b, x - sc, y + sc, 3 * sc, sc > 1 ? sc / 2 : 1, go);
}

void spr_star(u8 *b, int x, int y, int sc, u8 fill, u8 edge){
  static const u8 ST[7] = { 0x08, 0x1C, 0x7F, 0x3E, 0x1C, 0x36, 0x22 };
  for(int pass = 0; pass < 2; pass++)
    for(int r = 0; r < 7; r++) for(int k = 0; k < 7; k++) if(ST[r] & (0x40 >> k)){
      if(pass == 0){ gfx_rect(b, x + (k - 3) * sc - 1, y + (r - 3) * sc - 1, sc + 2, sc + 2, edge); }
      else gfx_rect(b, x + (k - 3) * sc, y + (r - 3) * sc, sc, sc, fill);
    }
}
