/*
 * MANDEX DS - opening cutscene.  Eight short scenes, same story and order as the browser version (web/index.html, "OPENING CUTSCENE"):
 *   1 the suburb   2 the divorce   3 Dennis   4 the couch   5 the landlord   6 the snap   7 MANDEX is born   8 the mission
 * Everything is drawn live with the gfx.c primitives (no art data), so it costs ROM only for the code and the text.
 * Controls: A / touch = next (first finishes the typing), START or the SKIP button = straight to level 1.
 */
#include <nds.h>
#include <string.h>
#include "gfx.h"
#include "sfx.h"
#include "intro.h"

static const char *TXT[INTRO_SCENES][3] = {
  { "GERALD FITZGERALD.", "Regional Assistant Manager of Paper Clip Compliance. One good tie.", "A house. A wife named Brenda. A fence so white it hurt." },
  { "A Tuesday. Papers.", "Brenda got the house, the dog, the boat, and the good towels.", "Gerald got the couch. And one pink towel." },
  { "His lawyer, Dennis: $400 an hour.", "Dennis's entire strategy: 'Ooh. That's rough.'", "Gerald paid. Gerald paid it all." },
  { "Then Gerald stopped.", "Gym: cancelled. Dinner: over the sink. Light source: the fridge.", "By day nine the pants were gone. He never asked where." },
  { "Then the landlord. 'Rent's due, Gerald.'", "Then all of them. Every hike. Every 'no refunds.' Every 'quiet hours.'", "Something in Gerald snapped. Also his waistband." },
  { "Gerald snapped.", "The sky snapped. Part of the roof snapped.", "And something old, pink and terry-cloth answered." },
  { "GERALD FITZGERALD IS DEAD.", "In his place: a sticky, shirtless, furious man in red underwear and a pink towel.", "Behold: MANDEX." },
  { "The mission is simple.", "EVICT. EVERY. LANDLORD.", "LEVEL 1: SUBURBAN EVICTION" },
};
static const signed char HL[INTRO_SCENES] = { 0, 2, -1, 0, 2, 2, 2, 1 };                  /* the line shown big and gold */
static const short HOLD[INTRO_SCENES] = { 192, 300, 216, 216, 204, 180, 270, 192 };      /* frames to linger once the text is typed (3.2 5 3.6 3.6 3.4 3 4.5 3.2 s) */
static const short CUTS[INTRO_SCENES][4] = {                                              /* frame at which each shot starts */
  { 0, 138, 246, 9999 }, { 0, 72, 336, 9999 }, { 0, 90, 186, 276 }, { 0, 114, 252, 9999 },
  { 0, 96, 240, 9999 }, { 0, 96, 192, 9999 }, { 0, 108, 204, 9999 }, { 0, 96, 186, 9999 } };
static const signed char TRANS[INTRO_SCENES] = { 0, 1, 1, 2, 3, 4, 4, 2 };              /* 0 fade, 1 wipe, 2 iris, 3 white, 4 cut */
static const signed char SHAKE[INTRO_SCENES][3] = { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,5,8}, {6,5,3}, {0,3,5} };

#define CPS 58
static int sc, t, fxdone;

static int total_chars(int s){ int n = 0; for(int i = 0; i < 3; i++) n += (int)strlen(TXT[s][i]); return n; }
static int type_end(int s){ return total_chars(s) * 60 / CPS + 24; }          /* frame at which the last letter is down */
static int shot_of(void){ int k = 0; while(k < 3 && t >= CUTS[sc][k + 1]) k++; return k; }
static int shot_t(int k){ return t - CUTS[sc][k]; }

void intro_start(void){ sc = 0; t = 0; fxdone = 0; sfx_play(SFX_WHOOSH, 64); }
int  intro_scene(void){ return sc; }

/* sound cues, frame exact */
static void cue(void){
  switch(sc){
  case 0: if(t < 130 && t % 14 == 6) sfx_play(SFX_CLICK, 40); if(t == 138) sfx_play(SFX_TARGET, 64); break;
  case 1: if(t == 0) sfx_play(SFX_THUD, 64); for(int i = 0; i < 6; i++) if(t == 96 + i * 27) sfx_play(SFX_POP, 64); if(t == 336) sfx_play(SFX_THUD, 64); break;
  case 2: if(t >= 90 && t < 186 && t % 12 == 0) sfx_play(SFX_CLICK, 64); if(t == 206) sfx_play(SFX_BOING, 64); if(t == 276) sfx_play(SFX_YAWN, 64); break;
  case 3: if(t == 0) sfx_play(SFX_SNORE, 64); if(t == 114) sfx_play(SFX_WHOOSH, 64); if(t == 252) sfx_play(SFX_BURP, 64); break;
  case 4: if(t == 10 || t == 26 || t == 42) sfx_play(SFX_THUD, 64); if(t == 96) sfx_play(SFX_WHOOSH, 64); if(t == 240){ sfx_play(SFX_CRACK, 64); sfx_play(SFX_SLAP, 64); } break;
  case 5: if(t == 0) sfx_play(SFX_CRACK, 64); if(t == 96) sfx_play(SFX_THUD, 64); if(t == 192) sfx_play(SFX_WHOOSH, 64); break;
  case 6: if(t == 0) sfx_play(SFX_BOOM, 64); if(t == 108) sfx_play(SFX_CRUNCH, 64); if(t == 204) sfx_play(SFX_WIN, 64); break;
  case 7: if(t == 0) sfx_play(SFX_TARGET, 64); if(t == 96) sfx_play(SFX_EEK, 64); if(t == 186) sfx_play(SFX_TAUNT, 64); break;
  }
}

static int next_scene(void){ sc++; t = 0; fxdone = 0; if(sc >= INTRO_SCENES) return 1; sfx_play(SFX_CLICK, 64); return 0; }

int intro_tick(int down, int tx, int ty){
  if(down & KEY_START) return 1;
  if((down & KEY_TOUCH) && tx >= 184 && ty >= 170) return 1;                    /* SKIP >> */
  if(down & (KEY_A | KEY_B | KEY_TOUCH)){
    if(t < type_end(sc)) t = type_end(sc);                                      /* first press finishes the typing... */
    else return next_scene();                                                   /* ...the next goes on */
  }
  t++;
  cue();
  if(t > type_end(sc) + HOLD[sc]) return next_scene();
  return 0;
}

/* ------------------------------------------------------------------ drawing helpers */
static int wv(int tt, int per, int amp){ int p = tt % per; int h = per / 2; int v = p < h ? p : per - p; return v * 2 * amp / h - amp; }   /* triangle wave -amp..amp */
static int iabs(int v){ return v < 0 ? -v : v; }
static u32 hs(u32 n){ n = n * 2654435761u; n ^= n >> 15; n *= 2246822519u; return n ^ (n >> 13); }
static int isq(int n){ int r = 0; while((r + 1) * (r + 1) <= n) r++; return r; }

static void sky(u8 *b, int y0, int y1, int dusk){
  static const u8 DUSK[8] = { C_RED2, C_RED, C_PINK, C_GOLD2, C_GOLD, C_SKIN, C_SKIN3, C_SKIN3 };
  for(int y = y0; y < y1; y++){
    int u = (y - y0) * 16 / (y1 - y0 > 0 ? y1 - y0 : 1); if(u > 15) u = 15;
    gfx_rect(b, 0, y, 256, 1, dusk ? DUSK[u >> 1] : (u8)(C_SKY0 + u));
  }
}
static void hills(u8 *b, int gy, int sx){
  gfx_ell(b, 60 - sx, gy + 6, 110, 34, C_HILL1); gfx_ell(b, 200 - sx, gy + 10, 120, 30, C_HILL2); gfx_ell(b, 128 - sx, gy + 16, 160, 22, C_HILL3);
}
static void ground(u8 *b, int gy){ gfx_rect(b, 0, gy, 256, 192 - gy, C_GRASS); gfx_rect(b, 0, gy, 256, 2, C_GRASS2); for(int x = 0; x < 256; x += 9) gfx_rect(b, x + (gy & 3), gy + 10 + (x * 7) % 20, 3, 1, C_GRASS2); }
static void sun(u8 *b, int x, int y, int r){ gfx_ell(b, x, y, r + 6, r + 6, C_SUNHALO); gfx_ell(b, x, y, r, r, C_SUN); }
static void cloud(u8 *b, int x, int y, int s){ gfx_ell(b, x, y, 10 * s, 4 * s, C_CLOUD); gfx_ell(b, x - 6 * s, y + 2 * s, 7 * s, 3 * s, C_CLOUD2); gfx_ell(b, x + 7 * s, y + 1, 7 * s, 3 * s, C_CLOUD); }
static void fence(u8 *b, int x0, int x1, int gy, int s){
  gfx_rect(b, x0, gy - 7 * s, x1 - x0, s, C_WHITE); gfx_rect(b, x0, gy - 3 * s, x1 - x0, s, C_WHITE);
  for(int x = x0; x < x1; x += 4 * s){ gfx_rect(b, x, gy - 10 * s, 3 * s, 11 * s, C_WHITE); gfx_rect(b, x + 2 * s, gy - 10 * s, s > 1 ? s : 1, 11 * s, C_CLOUD2); gfx_tri(b, x, gy - 10 * s, x + 3 * s, gy - 10 * s, x + 3 * s / 2, gy - 12 * s, C_WHITE); }
}
static void house(u8 *b, int x, int gy, int s, int roof){      /* x = left edge, 40 units wide */
  gfx_rect(b, x, gy - 22 * s, 40 * s, 22 * s, C_WALL0);
  if(roof){ gfx_rect(b, x + 30 * s, gy - 38 * s, 4 * s, 10 * s, C_DIRT2); gfx_tri(b, x - 3 * s, gy - 22 * s, x + 43 * s, gy - 22 * s, x + 20 * s, gy - 38 * s, C_RED2); }
  gfx_rect(b, x + 16 * s, gy - 14 * s, 8 * s, 14 * s, C_RED2); gfx_rect(b, x + 22 * s, gy - 7 * s, s, s, C_GOLD);
  for(int k = 0; k < 2; k++){ int wx = x + (k ? 28 : 4) * s; gfx_rect(b, wx, gy - 18 * s, 8 * s, 8 * s, C_WHITE); gfx_rect(b, wx + s, gy - 17 * s, 6 * s, 6 * s, C_SKY0 + 5); gfx_rect(b, wx + 3 * s, gy - 17 * s, s, 6 * s, C_WHITE); }
}
static void bubble(u8 *b, int x, int y, const char *s, int tail){      /* speech bubble, text centred at x */
  int w = gfx_text_w(s, 1) + 10;
  gfx_rect(b, x - w / 2 - 1, y - 1, w + 2, 15, C_BLACK); gfx_rect(b, x - w / 2, y, w, 13, C_WHITE);
  gfx_tri(b, x - 3 + tail, y + 12, x + 5 + tail, y + 12, x + tail * 2, y + 22, C_BLACK); gfx_tri(b, x - 2 + tail, y + 12, x + 4 + tail, y + 12, x + tail * 2, y + 19, C_WHITE);
  gfx_text(b, x - gfx_text_w(s, 1) / 2, y + 4, s, C_BLACK, 1);
}
enum { GF_TIE = 1, GF_BOXERS = 2, GF_SAD = 4, GF_WAVE = 8, GF_COOL = 16, GF_ANGRY = 32 };
static void face(u8 *b, int x, int y, int s, int fl){       /* head centred x,y, radius ~5 units */
  gfx_ell(b, x, y, 5 * s, 5 * s, C_SKIN); gfx_ell(b, x, y + s, 5 * s, 4 * s, C_SKIN);
  gfx_ell(b, x + 5 * s, y, s, 2 * s, C_SKIN2); gfx_ell(b, x - 5 * s, y, s, 2 * s, C_SKIN2);                      /* ears */
  gfx_ell(b, x, y - 5 * s, 5 * s, 2 * s, C_HAIR); gfx_rect(b, x - 5 * s, y - 5 * s, s, 3 * s, C_HAIR); gfx_rect(b, x + 4 * s, y - 5 * s, s, 3 * s, C_HAIR);
  int ey = y - s / 2, ew = s / 3 + 1;
  if(fl & GF_ANGRY){ gfx_tline(b, x - 4 * s, y - 3 * s, x - s, y - s, ew, C_BLACK); gfx_tline(b, x + 4 * s, y - 3 * s, x + s, y - s, ew, C_BLACK); ey = y; }
  else if(fl & GF_SAD){ gfx_tline(b, x - 4 * s, y - 2 * s, x - s, y - 3 * s, ew, C_BLACK); gfx_tline(b, x + 4 * s, y - 2 * s, x + s, y - 3 * s, ew, C_BLACK); }
  gfx_rect(b, x - 3 * s, ey, 2 * s > 2 ? 2 * s - 1 : 2, 2 * s > 2 ? 2 * s - 1 : 2, C_BLACK); gfx_rect(b, x + s, ey, 2 * s > 2 ? 2 * s - 1 : 2, 2 * s > 2 ? 2 * s - 1 : 2, C_BLACK);
  if(fl & GF_SAD){ gfx_tline(b, x - 2 * s, y + 3 * s, x + 2 * s, y + 3 * s, ew, C_BLACK); gfx_tline(b, x - 2 * s, y + 3 * s, x - 3 * s, y + 4 * s, ew, C_BLACK); gfx_tline(b, x + 2 * s, y + 3 * s, x + 3 * s, y + 4 * s, ew, C_BLACK); }
  else if(fl & GF_ANGRY){ gfx_rect(b, x - 2 * s, y + 2 * s, 4 * s, 2 * s, C_BLACK); gfx_rect(b, x - 2 * s + 1, y + 2 * s + 1, 4 * s - 2, s, C_WHITE); }
  else { gfx_tline(b, x - 2 * s, y + 3 * s, x, y + 3 * s + s, ew, C_BLACK); gfx_tline(b, x, y + 3 * s + s, x + 3 * s, y + 2 * s, ew, C_BLACK); }
  gfx_ell(b, x, y + s, s, s, C_SKIN2);                                                                              /* nose */
}
/* Gerald: centre x, feet at fy, s pixels per unit (about 30 units tall) */
static void gerald(u8 *b, int x, int fy, int s, int fl){
  int boxers = fl & GF_BOXERS;
  gfx_rect(b, x - 4 * s, fy - 8 * s, 3 * s, 8 * s, C_SKIN); gfx_rect(b, x + s, fy - 8 * s, 3 * s, 8 * s, C_SKIN);           /* legs */
  gfx_rect(b, x - 5 * s, fy - s, 4 * s, s, C_BLACK); gfx_rect(b, x + s, fy - s, 4 * s, s, C_BLACK);                         /* shoes */
  gfx_rect(b, x - 5 * s, fy - 13 * s, 10 * s, 6 * s, boxers ? C_RED : C_GOLD2);                                             /* khaki shorts / red boxers */
  if(boxers) gfx_rect(b, x - 5 * s, fy - 13 * s, 10 * s, s, C_RED2);
  if(boxers){ gfx_rect(b, x - 5 * s, fy - 23 * s, 10 * s, 10 * s, C_SKIN); gfx_ell(b, x, fy - 14 * s, 6 * s, 3 * s, C_SKIN2); }   /* shirtless, belly */
  else { gfx_rect(b, x - 5 * s, fy - 23 * s, 10 * s, 10 * s, C_WHITE); gfx_rect(b, x + 3 * s, fy - 23 * s, 2 * s, 10 * s, C_CLOUD2); }
  if(fl & GF_TIE){ gfx_tri(b, x - s, fy - 23 * s, x + s, fy - 23 * s, x, fy - 21 * s, C_BLACK); gfx_tri(b, x - s, fy - 22 * s, x + s, fy - 22 * s, x + s, fy - 14 * s, C_SKY0 + 4); gfx_tri(b, x - s, fy - 22 * s, x + s, fy - 14 * s, x - s, fy - 14 * s, C_SKY0 + 6); }
  gfx_rect(b, x - 7 * s, fy - 23 * s, 2 * s, 9 * s, boxers ? C_SKIN : C_WHITE);                                            /* left arm */
  if(fl & GF_WAVE){ gfx_tline(b, x + 5 * s, fy - 22 * s, x + 9 * s, fy - 31 * s, 2 * s, boxers ? C_SKIN : C_WHITE); gfx_ell(b, x + 9 * s, fy - 32 * s, s + 1, s + 1, C_SKIN); }
  else gfx_rect(b, x + 5 * s, fy - 23 * s, 2 * s, 9 * s, boxers ? C_SKIN : C_WHITE);
  gfx_rect(b, x - 7 * s, fy - 14 * s, 2 * s, 2 * s, C_SKIN); if(!(fl & GF_WAVE)) gfx_rect(b, x + 5 * s, fy - 14 * s, 2 * s, 2 * s, C_SKIN);
  face(b, x, fy - 28 * s, s, fl);
}
static void towel(u8 *b, int x, int y, int w, int h, int ph, int th){         /* the pink towel, rippling, with two terry stripes */
  (void)th;
  for(int i = 0; i < w; i++){ int dy = wv(ph + i * 4, 40, 3); gfx_rect(b, x + i, y + dy, 1, h, C_PINK); gfx_rect(b, x + i, y + dy + h / 4, 1, 2, C_RED2); gfx_rect(b, x + i, y + dy + h * 3 / 4, 1, 2, C_RED2); gfx_rect(b, x + i, y + dy, 1, 1, C_WHITE); }
}
static void rain(u8 *b, int tt, int n){
  for(int i = 0; i < n; i++){ int x = (int)(hs(i) % 270), y = (int)((hs(i + 99) + tt * (7 + (i & 3))) % 200); gfx_line(b, x, y - 8, x - 2, y, C_UILINE); }
}
static void stars_night(u8 *b){ gfx_rect(b, 0, 0, 256, 192, C_UIBG); for(int i = 0; i < 30; i++) gfx_px(b, (int)(hs(i + 5) % 256), (int)(hs(i + 7) % 100), C_UILINE); }
static void lamp(u8 *b, int x, int gy){ gfx_rect(b, x, gy - 80, 3, 80, C_DARK); gfx_rect(b, x - 8, gy - 84, 14, 5, C_DARK); gfx_ell(b, x - 1, gy - 78, 5, 3, C_GOLD); gfx_tri(b, x - 22, gy, x + 20, gy, x - 1, gy - 76, C_UIHI); gfx_dither(b, x - 18, gy - 40, 38, 40, C_UIBG); }
static void couch_back(u8 *b, int x, int gy){ gfx_rect(b, x, gy - 40, 110, 40, C_RED2); gfx_rect(b, x + 3, gy - 38, 104, 6, C_RED); gfx_rect(b, x - 8, gy - 30, 12, 30, C_RED2); gfx_rect(b, x + 106, gy - 30, 12, 30, C_RED2); }
static void couch_front(u8 *b, int x, int gy){ gfx_rect(b, x - 8, gy - 20, 126, 22, C_RED); gfx_rect(b, x - 8, gy - 4, 126, 6, C_RED2); gfx_rect(b, x + 54, gy - 20, 1, 18, C_RED2); }
static void pizza(u8 *b, int x, int gy, int n){ for(int i = 0; i < n; i++){ gfx_rect(b, x + (i & 1) * 2, gy - 4 - i * 5, 26, 5, C_WHITE); gfx_rect(b, x + (i & 1) * 2, gy - 4 - i * 5, 26, 1, C_FIRE); gfx_rect(b, x + 9 + (i & 1) * 2, gy - 3 - i * 5, 8, 3, C_RED); } }
static void can(u8 *b, int x, int gy){ gfx_rect(b, x, gy - 7, 4, 7, C_GRAY); gfx_rect(b, x, gy - 7, 4, 1, C_WHITE); }
static void landlord(u8 *b, int x, int y, int ppc, int tt, int scared){ spr_landlord(b, x, y, ppc, tt, scared); }
static void dennis(u8 *b, int x, int by, int s, int tt){         /* behind a desk: x centre, by = desk top */
  gfx_ell(b, x, by - 6 * s, 9 * s, 7 * s, C_SUIT2); gfx_ell(b, x, by - 7 * s, 8 * s, 6 * s, C_SUIT);
  gfx_tri(b, x - s, by - 11 * s, x + s, by - 11 * s, x, by - 4 * s, C_GOLD);
  gfx_ell(b, x, by - 15 * s, 4 * s, 4 * s, C_SKIN); gfx_ell(b, x, by - 18 * s, 4 * s, 2 * s, C_BLACK);
  gfx_rect(b, x - 2 * s, by - 16 * s, s, s, C_BLACK); gfx_rect(b, x + s, by - 16 * s, s, s, C_BLACK);
  int m = 1 + (wv(tt, 30, 1) > 0); gfx_rect(b, x - 2 * s, by - 12 * s, 4 * s, m * s, C_WHITE); gfx_rect(b, x - 2 * s, by - 12 * s, 4 * s, 1, C_BLACK);
}
static void bpt(int p, int *x, int *y){                 /* a point on the rectangle (-40,-40)-(296,232) */
  const int w = 336, h = 272; p %= 2 * (w + h);
  if(p < w){ *x = -40 + p; *y = -40; } else if((p -= w) < h){ *x = 296; *y = -40 + p; } else if((p -= h) < w){ *x = 296 - p; *y = 232; } else { p -= w; *x = -40; *y = 232 - p; }
}
static void rays(u8 *b, int cx, int cy, int tt, int n, u8 c){       /* n slowly turning wedges */
  const int per = 2 * (336 + 272);
  for(int i = 0; i < n; i++){ int p0 = (i * per / n + tt * 4) % per, p1 = p0 + per / n / 2, x0, y0, x1, y1; bpt(p0, &x0, &y0); bpt(p1, &x1, &y1); gfx_tri(b, cx, cy, x0, y0, x1, y1, c); }
}
static void sparkle(u8 *b, int x, int y, int tt, int ph){ int u = (tt + ph) % 40; if(u < 14){ int r = u < 7 ? u : 14 - u; gfx_line(b, x - r, y, x + r, y, C_WHITE); gfx_line(b, x, y - r, x, y + r, C_WHITE); } }

/* ------------------------------------------------------------------ the scenes (top screen) */
static void sc_suburb(u8 *b, int k, int tt){
  sky(b, 0, 150, 0); sun(b, 200, 40, 14); cloud(b, 60 + tt / 6, 36, 2); cloud(b, 170 - tt / 9, 62, 1); hills(b, 140, 0); ground(b, 150);
  if(k == 0){
    fence(b, 0, 110, 152, 2); house(b, 150, 152, 2, 1);
    int gx = -20 + tt * 2; if(gx > 100) gx = 100;
    gerald(b, gx, 176 + (gx < 100 ? -iabs(wv(tt, 12, 3)) : 0), 2, GF_TIE | GF_COOL | (gx >= 100 ? GF_WAVE : 0));
  } else if(k == 1){
    gfx_rect(b, 0, 150, 256, 42, C_GRASS); house(b, 120, 170, 3, 1);
    gerald(b, 60, 190, 3, GF_TIE | GF_COOL | GF_WAVE); fence(b, 0, 30, 172, 2);
    if(tt > 12) bubble(b, 70, 24, "HONEY, I'M HOME!", 0);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_SKY0 + 9); gfx_rect(b, 0, 120, 256, 72, C_GRASS); fence(b, -4, 262, 150, 5);
    for(int i = 0; i < 6; i++) sparkle(b, 20 + i * 44, 100 + (i & 1) * 12, tt, i * 7);
    gfx_text_c(b, 128, 30, "SO WHITE IT HURT", C_BLACK, C_WHITE, 2);
  }
}
static void sc_divorce(u8 *b, int k, int tt){
  if(k == 0){
    stars_night(b); gfx_rect(b, 0, 150, 256, 42, C_DARK); gfx_rect(b, 0, 150, 256, 2, C_UIHI);
    for(int i = 0; i < 3; i++) gfx_ell(b, 130 + i * 60, 170 + i * 8, 28 - i * 6, 3, C_UIHI);
    lamp(b, 40, 150); gerald(b, 118, 160, 2, GF_TIE | GF_SAD); rain(b, tt, 50);
  } else if(k == 1){
    stars_night(b); rain(b, tt, 24);
    gfx_rect(b, 20, 10, 216, 172, C_WHITE); gfx_rect(b, 20, 10, 216, 3, C_UILINE);
    gfx_text_c(b, 128, 20, "DECREE OF DIVORCE", C_BLACK, C_WHITE, 1); gfx_rect(b, 32, 30, 192, 1, C_BLACK);
    static const char *ROW[6][2] = { {"HOUSE","BRENDA"}, {"DOG","BRENDA"}, {"BOAT","BRENDA"}, {"THE GOOD TOWELS","BRENDA"}, {"COUCH","GERALD"}, {"ONE PINK TOWEL","GERALD"} };
    for(int i = 0; i < 6; i++){
      int y = 38 + i * 22; if(i & 1) gfx_rect(b, 24, y - 3, 208, 18, C_CLOUD2);
      if(tt > 24 + i * 27){ gfx_text(b, 32, y + 2, ROW[i][0], C_BLACK, 1); gfx_text(b, 224 - gfx_text_w(ROW[i][1], 1), y + 2, ROW[i][1], i < 4 ? C_RED2 : C_GOO2, 1); }
    }
    if(tt > 24 + 6 * 27 + 20){ gfx_rect(b, 150, 150, 70, 18, C_RED); gfx_text_c(b, 185, 154, "FINAL", C_WHITE, C_RED2, 2); }
  } else {
    stars_night(b); rain(b, tt, 40);
    for(int i = 0; i < 8; i++) gfx_ell(b, (int)(hs(i + 40) % 256), (int)(hs(i + 60) % 100) + 20, 6 + (int)(hs(i) % 8), 6 + (int)(hs(i) % 8), i & 1 ? C_GOLD2 : C_UIHI);
    face(b, 128, 100, 12, GF_SAD);
    int ty = 112 + (tt * 2) % 70; if(ty < 170) gfx_ell(b, 128 - 12 * 3, ty, 3, 4, C_SKY0 + 7);
  }
}
static void sc_dennis(u8 *b, int k, int tt){
  gfx_rect(b, 0, 0, 256, 192, C_WALL3); gfx_rect(b, 0, 150, 256, 42, C_DIRT);
  if(k == 0 || k == 2){
    gfx_rect(b, 190, 20, 40, 40, C_WHITE); gfx_ell(b, 210, 40, 17, 17, C_CLOUD2); gfx_line(b, 210, 40, 210, 28, C_BLACK); gfx_line(b, 210, 40, 218 + wv(tt, 60, 3), 40, C_BLACK);   /* wall clock */
  }
  if(k == 0){
    dennis(b, 190, 120, 3, tt); gfx_rect(b, 110, 120, 140, 40, C_DIRT2); gfx_rect(b, 110, 120, 140, 3, C_DIRT); gerald(b, 50, 170, 2, GF_TIE | GF_SAD);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG);
    int n = 400 * (1 + tt / 8); char buf[12]; buf[0] = '$'; const char *d = gfx_int(n); int i = 0; while(d[i] && i < 9){ buf[i + 1] = d[i]; i++; } buf[i + 1] = 0;
    gfx_text_c(b, 128, 60, buf, C_GOLD, C_BLACK, 6); gfx_text_c(b, 128, 110, "AND COUNTING", C_WHITE, C_BLACK, 2);
    for(int j = 0; j < 5; j++){ int y = (tt * 3 + j * 40) % 200 - 8; gfx_text(b, 20 + j * 48, y, "$", C_GOO, 2); }
  } else if(k == 2){
    dennis(b, 170, 130, 5, tt); gfx_rect(b, 0, 130, 256, 62, C_DIRT2); gfx_rect(b, 0, 130, 256, 3, C_DIRT); gerald(b, 52, 200, 3, GF_TIE | GF_SAD);
    if(tt > 20) bubble(b, 150, 20, "OOH. THAT'S ROUGH.", -10);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_UIBG); gfx_rect(b, 60, 60, 136, 76, C_DIRT2); gfx_rect(b, 60, 60, 136, 4, C_DIRT);
    gfx_rect(b, 70, 74, 116, 52, C_WALL0); gfx_text_c(b, 128, 90, "$0.00", C_RED2, C_WALL0, 3);
    int mx = 90 + (tt * 2) % 80; gfx_ell(b, mx, 40 + wv(tt, 24, 6), 4, 2, C_GRAY);
  }
}
static void sc_couch(u8 *b, int k, int tt){
  if(k == 0){
    gfx_rect(b, 0, 0, 256, 192, C_DARK); gfx_rect(b, 0, 150, 256, 42, C_UIBG);
    int fl = 3 + ((tt * 7) & 3) + (hs(tt / 3) & 3); gfx_rect(b, 150, 30, 70, 50, C_BLACK); gfx_rect(b, 153, 33, 64, 44, C_SKY0 + 6 + (fl & 3));   /* TV */
    gfx_dither(b, 130, 14, 110, 80, C_SKY0 + 8);
    couch_back(b, 30, 160); gerald(b, 85, 160, 2, GF_BOXERS | GF_SAD); couch_front(b, 30, 160);
    pizza(b, 170, 170, 1 + tt / 28 > 6 ? 6 : 1 + tt / 28); can(b, 150, 176); can(b, 18, 180);
    gfx_rect(b, 8, 20, 28, 34, C_WHITE); gfx_text_c(b, 22, 24, "DAY", C_BLACK, C_WHITE, 1); gfx_text_c(b, 22, 34, gfx_int(1 + tt / 12 > 9 ? 9 : 1 + tt / 12), C_RED2, C_WHITE, 2);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_BLACK);
    gfx_tri(b, 84, 20, 84, 180, 230, 170, C_UIHI); gfx_dither(b, 84, 20, 150, 160, C_BLACK);                          /* the fridge light */
    gfx_rect(b, 20, 14, 64, 170, C_WHITE); gfx_rect(b, 28, 20, 50, 158, C_CLOUD2);
    for(int i = 0; i < 3; i++){ gfx_rect(b, 28, 56 + i * 40, 50, 2, C_GRAY); gfx_rect(b, 40 + i * 12, 40 + i * 40, 8, 14, i == 1 ? C_RED : C_GOO2); }
    gerald(b, 170, 192, 3, GF_BOXERS | GF_SAD);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_WALL0); gfx_rect(b, 40, 20, 176, 140, C_WHITE); gfx_rect(b, 40, 20, 176, 18, C_RED);
    gfx_text_c(b, 128, 25, "DAY 9", C_WHITE, C_RED2, 1);
    for(int i = 0; i < 9; i++){ int x = 52 + (i % 3) * 56, y = 46 + (i / 3) * 24; if(tt > 12 + i * 8){ gfx_tline(b, x, y, x + 16, y + 16, 2, C_RED2); gfx_tline(b, x + 16, y, x, y + 16, 2, C_RED2); } }
    gfx_text_c(b, 128, 134, "PANTS: ???", C_BLACK, C_WHITE, 2);
  }
}
static void sc_landlord(u8 *b, int k, int tt){
  if(k == 0){
    gfx_rect(b, 0, 0, 256, 192, C_WALL1); gfx_rect(b, 0, 150, 256, 42, C_DIRT2);
    gfx_rect(b, 150, 40, 60, 112, C_DIRT); gfx_rect(b, 154, 44, 52, 104, C_RED2); gfx_ell(b, 198, 100, 3, 3, C_GOLD);
    landlord(b, 90, 128, 10, tt, 0); gerald(b, 218, 190, 2, GF_BOXERS | GF_SAD);
    if(tt > 30) bubble(b, 90, 14, "RENT'S DUE, GERALD.", 0);
    if(tt % 16 < 3 && tt < 48) gfx_text_c(b, 140, 70, "KNOCK", C_GOLD, C_BLACK, 1);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG);
    static const char *SIGN[4] = { "PRICE HIKE +40%", "NO REFUNDS", "QUIET HOURS", "SORRY, NO PETS" };
    for(int i = 0; i < 4; i++){ int u = tt - i * 28; if(u < 0) continue; int y = 14 + i * 40; int x = u < 10 ? -200 + u * 30 : 128 + wv(tt, 24, 2) * (i & 1 ? 1 : -1) / 2;
      gfx_rect(b, x - 90 + 2, y + 2, 180, 30, C_BLACK); gfx_rect(b, x - 90, y, 180, 30, i & 1 ? C_RED : C_GOLD); gfx_rect(b, x - 88, y + 2, 176, 26, i & 1 ? C_RED2 : C_GOLD2);
      gfx_text_c(b, x, y + 8, SIGN[i], C_WHITE, C_BLACK, 2); }
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_RED2);
    rays(b, 128, 96, tt, 14, C_RED);
    face(b, 128, 90, 12, GF_ANGRY);
    { int sp = tt > 40 ? (tt - 40) * 3 : 0; if(sp > 60) sp = 60;                                                  /* the waistband, parting */
      gfx_rect(b, 0, 164, 128 - sp, 14, C_RED); gfx_rect(b, 128 + sp, 164, 128 - sp, 14, C_RED); gfx_rect(b, 0, 164, 128 - sp, 2, C_WHITE); gfx_rect(b, 128 + sp, 164, 128 - sp, 2, C_WHITE); }
    if(tt > 60){ gfx_text_c(b, 128, 20, "SNAP!", C_GOLD, C_BLACK, 4); }
  }
}
static void bolt(u8 *b, int x, int y0, int y1, int sd){ int px = x; for(int y = y0; y < y1; y += 12){ int nx = x + (int)(hs(y + sd) % 24) - 12; gfx_tline(b, px, y, nx, y + 12, 2, C_GOLD); gfx_line(b, px, y, nx, y + 12, C_WHITE); px = nx; } }
static void sc_snap(u8 *b, int k, int tt){
  if(k == 0){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG); for(int i = 0; i < 7; i++) gfx_ell(b, 20 + i * 38, 14 + (i & 1) * 12, 30, 12, C_UIHI); ground(b, 150);
    bolt(b, 60, 0, 150, 1); bolt(b, 140, 0, 150, 9); bolt(b, 210, 0, 120, 5);
    if(tt % 8 < 2) gfx_dither(b, 0, 0, 256, 150, C_WHITE);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG); for(int i = 0; i < 7; i++) gfx_ell(b, 20 + i * 38, 14 + (i & 1) * 12, 30, 12, C_UIHI); ground(b, 150); house(b, 100, 152, 3, 0);
    int u = tt; gfx_tri(b, 100 - 9 + u, 152 - 66 - u * 2 + u * u / 10, 100 + 129 + u, 152 - 66 - u * 2 + u * u / 10, 100 + 60 + u, 152 - 114 - u * 2 + u * u / 10, C_RED2);
    for(int i = 0; i < 10; i++) gfx_rect(b, (int)(hs(i) % 256), (tt * (3 + (int)(hs(i + 3) % 4)) + i * 30) % 190, 3, 3, C_DIRT);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_UIBG);
    rays(b, 128, 96, tt, 18, C_RED2);
    gfx_dither(b, 0, 0, 256, 192, C_UIBG);
    if(tt > 30){ gfx_ell(b, 128, 96, 40, 40, C_WHITE); gfx_dither(b, 80, 48, 96, 96, C_PINK); }
    { int x = -50 + tt * 3; if(x > 128) x = 128; towel(b, x - 22, 80 + wv(tt, 30, 6), 44, 30, tt * 3, 0); }
  }
}
static void sc_born(u8 *b, int k, int tt){
  if(k == 0){
    gfx_rect(b, 0, 0, 256, 192, C_WHITE); if(tt > 12) gfx_dither(b, 0, 0, 256, 192, C_PINK);
    gfx_rect(b, 108, 20, 40, 80, C_GRAY); gfx_ell(b, 128, 20, 20, 12, C_GRAY); gfx_rect(b, 112, 40, 32, 3, C_DARK);   /* headstone */
    gfx_text_c(b, 128, 28, "R.I.P.", C_DARK, C_GRAY, 1); gfx_text_c(b, 128, 52, "GERALD", C_DARK, C_GRAY, 1); gfx_text_c(b, 128, 62, "FITZGERALD", C_DARK, C_GRAY, 1);
    gfx_rect(b, 0, 100, 256, 92, C_GRASS2); int ty = 30 + tt; if(ty < 140) gfx_tri(b, 150, ty, 156, ty, 153, ty + 18, C_SKY0 + 4);                      /* the tie, falling */
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG);
    rays(b, 128, 96, tt, 12, C_UIHI);
    int u = tt > 60 ? 60 : tt; int by = 192 - u * 2; spr_man(b, 128, by + 20, 6, 1, POSE_FLEX, tt >> 3);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_RED2);
    rays(b, 128, 120, tt, 14, C_RED);
    spr_man(b, 128, 118, 8, 1, POSE_FLEX, tt >> 3);
    gfx_text_c(b, 128, 18, "MANDEX", C_GOLD, C_BLACK, 4);
  }
}
static void sc_mission(u8 *b, int k, int tt){
  if(k == 0){
    sky(b, 0, 150, 1); sun(b, 70, 120, 24); hills(b, 140, 0); ground(b, 150); fence(b, 0, 80, 152, 2); house(b, 156, 152, 2, 1);
    spr_man(b, 66, 128, 6, 1, POSE_FLEX, tt >> 3); landlord(b, 200, 130, 10, tt, 1);
  } else if(k == 1){
    sky(b, 0, 192, 1); gfx_tri(b, 0, 192, 0, 120, 100, 192, C_RED2); gfx_tri(b, 256, 192, 256, 120, 156, 192, C_DIRT2);
    landlord(b, 128, 118, 16, tt, 1); bubble(b, 128, 16, "OH NO.", 0);
  } else {
    sky(b, 0, 192, 1);
    rays(b, 128, 150, tt, 14, C_GOLD);
    gfx_rect(b, 0, 150, 256, 42, C_GRASS2);
    spr_man(b, 128, 120, 8, 1, POSE_FLEX, tt >> 3);
  }
}

/* transitions (the first 30 frames of a scene) */
static void trans(u8 *b){
  int ty = TRANS[sc]; if(t >= 30 || ty == 4) return;
  int u = t * 256 / 30;
  if(ty == 1){ int x = u; gfx_rect(b, x, 0, 256 - x, 192, C_BLACK); gfx_rect(b, x - 3, 0, 3, 192, C_GOLD); }
  else if(ty == 2){ int r = (u * u >> 8) * 180 / 256 + 1; for(int y = 0; y < 192; y++){ int dy = y - 96, d = r * r - dy * dy; int dx = d > 0 ? isq(d) : 0; if(dx < 128){ gfx_rect(b, 0, y, 128 - dx, 1, C_BLACK); gfx_rect(b, 128 + dx, y, 128 - dx, 1, C_BLACK); } } }
  else { u8 c = ty == 3 ? C_WHITE : C_BLACK; if(t < 8) gfx_rect(b, 0, 0, 256, 192, c); else if(t < 20) gfx_dither(b, 0, 0, 256, 192, c); else if(t & 1) gfx_dither(b, 0, 0, 256, 192, c); }
}
static void shake(u8 *b, int amp){
  if(amp <= 0) return;
  int dx = (int)(hs(t * 3 + 1) % (u32)(2 * amp + 1)) - amp, dy = (int)(hs(t * 5 + 2) % (u32)(2 * amp + 1)) - amp;
  if(dy > 0){ memmove(b + dy * 256, b, (size_t)(192 - dy) * 256); memset(b, C_BLACK, (size_t)dy * 256); }
  else if(dy < 0){ memmove(b, b - dy * 256, (size_t)(192 + dy) * 256); memset(b + (192 + dy) * 256, C_BLACK, (size_t)(-dy) * 256); }
  for(int y = 0; y < 192 && dx; y++){ u8 *r = b + y * 256; if(dx > 0){ memmove(r + dx, r, (size_t)(256 - dx)); memset(r, C_BLACK, (size_t)dx); } else { memmove(r, r - dx, (size_t)(256 + dx)); memset(r + 256 + dx, C_BLACK, (size_t)(-dx)); } }
}

/* ------------------------------------------------------------------ the narration (bottom screen) */
#define ROW_CH 30
#define MAX_ROWS 10
/* wrap one line of text into rows of at most ROW_CH characters; returns the number of rows.  rows[] point into the string, len[] = their lengths */
static int wrap(const char *s, const char **rows, int *len, int n0){
  int n = n0; const char *p = s;
  while(*p && n < MAX_ROWS){
    int l = (int)strlen(p); if(l <= ROW_CH){ rows[n] = p; len[n++] = l; break; }
    int cut = ROW_CH; while(cut > 0 && p[cut] != ' ') cut--;
    if(cut == 0) cut = ROW_CH;
    rows[n] = p; len[n++] = cut; p += cut; while(*p == ' ') p++;
  }
  return n;
}
int intro_text_rows(int s){ const char *r[MAX_ROWS + 4]; int l[MAX_ROWS + 4]; int n = 0; for(int i = 0; i < 3; i++) n = wrap(TXT[s][i], r, l, n); return n; }

static void narration(u8 *b){
  gfx_rect(b, 0, 0, 256, 192, C_UIBG);
  for(int i = 0; i < INTRO_SCENES; i++) gfx_rect(b, 8 + i * 12, 8, 8, 4, i == sc ? C_GOLD : i < sc ? C_UILINE : C_UIHI);
  const char *rows[MAX_ROWS + 4]; int len[MAX_ROWS + 4]; int line_of[MAX_ROWS + 4]; int n = 0;
  for(int i = 0; i < 3; i++){ int n1 = wrap(TXT[sc][i], rows, len, n); for(int j = n; j < n1; j++) line_of[j] = i; n = n1; }
  int shown = t * CPS / 60; if(shown > total_chars(sc)) shown = total_chars(sc);
  int y = 24 + (n < 8 ? (8 - n) * 3 : 0);
  for(int j = 0; j < n; j++){
    int cnt = shown < len[j] ? shown : len[j]; shown -= cnt;
    if(cnt > 0){ char tmp[ROW_CH + 1]; memcpy(tmp, rows[j], (size_t)cnt); tmp[cnt] = 0;
      int hl = line_of[j] == HL[sc]; gfx_text(b, 8, y, tmp, hl ? C_GOLD : C_WHITE, 2); }
    y += (j + 1 < n && line_of[j + 1] != line_of[j]) ? 17 : 12;
    if(cnt < len[j]) break;
  }
  gfx_rect(b, 184, 172, 66, 16, C_UIHI); gfx_rect(b, 184, 172, 66, 1, C_UILINE); gfx_rect(b, 184, 187, 66, 1, C_UILINE); gfx_rect(b, 184, 172, 1, 16, C_UILINE); gfx_rect(b, 249, 172, 1, 16, C_UILINE);
  gfx_text_c(b, 217, 177, "SKIP >>", C_WHITE, C_UIHI, 1);
  if(t >= type_end(sc) && (t & 32)) gfx_text(b, 8, 177, "A / TOUCH: NEXT", C_UILINE, 1);
}

void intro_draw(u8 *top, u8 *bot){
  int k = shot_of(), tt = shot_t(k);
  switch(sc){
  case 0: sc_suburb(top, k, tt); break;   case 1: sc_divorce(top, k, tt); break;  case 2: sc_dennis(top, k, tt); break;  case 3: sc_couch(top, k, tt); break;
  case 4: sc_landlord(top, k, tt); break; case 5: sc_snap(top, k, tt); break;     case 6: sc_born(top, k, tt); break;    default: sc_mission(top, k, tt); break;
  }
  trans(top); shake(top, SHAKE[sc][k > 2 ? 2 : k]);
  /* letterbox bars sliding in, like the browser version */
  int bar = t * 3 > 12 ? 12 : t * 3; gfx_rect(top, 0, 0, 256, bar, C_BLACK); gfx_rect(top, 0, 192 - bar, 256, bar, C_BLACK);
  narration(bot);
}
