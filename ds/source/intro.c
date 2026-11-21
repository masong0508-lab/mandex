/*
 * MANDEX DS - opening cutscene.  Eight short scenes, same story and order as the browser version (web/index.html, "OPENING CUTSCENE"):
 *   1 the suburb   2 the divorce   3 Dennis   4 the couch   5 the landlord   6 the snap   7 MANDEX is born   8 the mission
 * Everything is drawn live with the gfx.c primitives (no art data), so it costs ROM only for the code and the text.
 * Controls: A / touch = next (first finishes the typing), START or the SKIP button = straight to level 1.
 */
#include <nds.h>
#if defined(__3DS__) || defined(MX_SHARED_CUTSCENE)                      /* shared with the 3DS build: the caller passes libctru key bits (A = bit 0, B = bit 1, START = bit 3, TOUCH = bit 20) */
#define IK_A (1 << 0)
#define IK_B (1 << 1)
#define IK_START (1 << 3)
#define IK_TOUCH (1 << 20)
#else
#define IK_A KEY_A
#define IK_B KEY_B
#define IK_START KEY_START
#define IK_TOUCH KEY_TOUCH
#endif
#include <string.h>
#include "gfx.h"
#include "sfx.h"
#include "intro.h"
#include "cut_art.h"

static const char *TXT8[INTRO_SCENES][3] = {
  { "GERALD FITZGERALD.", "Regional Assistant Manager of Paper Clip Compliance. One good tie.", "A house. A wife named Brenda. A fence so white it hurt." },
  { "A Tuesday. Papers.", "Brenda got the house, the dog, the boat, and the good towels.", "Gerald got the couch. And one pink towel." },
  { "His lawyer, Dennis: $400 an hour.", "Dennis's entire strategy: 'Ooh. That's rough.'", "Gerald paid. Gerald paid it all." },
  { "Then Gerald stopped.", "Gym: cancelled. Dinner: over the sink. Light source: the fridge.", "By day nine the pants were gone. He never asked where." },
  { "Then the landlord. 'Rent's due, Gerald.'", "Then all of them. Every hike. Every 'no refunds.' Every 'quiet hours.'", "Something in Gerald snapped. Also his waistband." },
  { "Gerald snapped.", "The sky snapped. Part of the roof snapped.", "And something old, pink and terry-cloth answered." },
  { "GERALD FITZGERALD IS DEAD.", "In his place: a sticky, shirtless, furious man in red underwear and a pink towel.", "Behold: MANDEX." },
  { "The mission is simple.", "EVICT. EVERY. LANDLORD.", "LEVEL 1: SUBURBAN EVICTION" },
};
static const signed char HL8[INTRO_SCENES] = { 0, 2, -1, 0, 2, 2, 2, 1 };                  /* the line shown big and gold */
static const short HOLD8[INTRO_SCENES] = { 192, 300, 216, 216, 204, 180, 270, 192 };      /* frames to linger once the text is typed (3.2 5 3.6 3.6 3.4 3 4.5 3.2 s) */
static const short CUTS8[INTRO_SCENES][4] = {                                              /* frame at which each shot starts */
  { 0, 138, 246, 9999 }, { 0, 72, 336, 9999 }, { 0, 90, 186, 276 }, { 0, 114, 252, 9999 },
  { 0, 96, 240, 9999 }, { 0, 96, 192, 9999 }, { 0, 108, 204, 9999 }, { 0, 96, 186, 9999 } };
static const signed char TRANS8[INTRO_SCENES] = { 0, 1, 1, 2, 3, 4, 4, 2 };              /* 0 fade, 1 wipe, 2 iris, 3 white, 4 cut */
static const signed char SHAKE8[INTRO_SCENES][3] = { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,5,8}, {6,5,3}, {0,3,5} };

/* chapter 3: THE WOLFMAN (same seven scenes, lines and order as the browser's CUT3), played once when chapter 3 is first started */
static const char *TXTW[WOLF_SCENES][3] = {
  { "One week after Landlord HQ. Mandex took the night off.", "Gerald took a walk. In a shirt. Brenda would have wept.", "Above him, the moon was doing something unreasonable." },
  { "Home: a garage, a couch, a fridge.", "In the fridge: one wedge of MOON CHEESE.", "\"BEST BEFORE: THE FULL MOON.\" Gerald read this as a deadline, not a warning." },
  { "An itch. Then a sniff. Then a physics problem.", "The moon leaned in at the window. Gerald leaned back.", "Gerald inhaled most of the room." },
  { "Ears. Boing.", "A snout. Eleven shirt buttons resigning, one at a time.", "His mustache did not move. It is load-bearing." },
  { "The shorts held out for one more second.", "Then: a tail. And the discovery that pants were always optional.", "Gerald checked the rear view. Round. Furry. Free." },
  { "Naturally, he howled.", "At a moon he had called \"a lot\" two minutes earlier.", "Three blocks away, a landlord's wine glass cracked." },
  { "A knock. Brenda, back for \"the good towels\".", "She winked at the wolf. His tail wagged WITHOUT PERMISSION.", "The door closed. This is a family camera. Allegedly." },
  { "CHAPTER 3: THE WOLFMAN", "Landlords now have health bars, riot shields and a healer. Nobody asked the healer.", "EVICT. EVERY. LANDLORD. AWOOOO." },
};
static const signed char HLW[WOLF_SCENES] = { 2, 2, 2, 1, 2, 2, 2, 2 };
static const short HOLDW[WOLF_SCENES] = { 216, 204, 192, 216, 216, 216, 228, 240 };
static const short CUTSW[WOLF_SCENES][4] = { { 0, 168, 9999, 9999 }, { 0, 90, 198, 9999 }, { 0, 78, 156, 9999 }, { 0, 66, 144, 228 }, { 0, 90, 192, 9999 }, { 0, 108, 210, 9999 }, { 0, 144, 300, 9999 }, { 0, 9999, 9999, 9999 } };
static const signed char TRANSW[WOLF_SCENES] = { 0, 1, 2, 3, 2, 1, 1, 2 };
static const signed char SHAKEW[WOLF_SCENES][3] = { {0,3,0}, {0,0,5}, {0,2,12}, {10,6,8}, {0,6,0}, {6,10,0}, {0,4,0}, {8,0,0} };
/* closing cutscenes: the end of chapter 1 (Mandex Manor) and of chapter 2 (Landlord HQ), four scenes each, same story as the browser's CUT_E1 / CUT_E2 */
static const char *TXTE1[END_SCENES][3] = {
  { "The manor came down in eleven seconds.", "Gerald stood on top of it for the other four hours. For the view.", "Somewhere under the bricks, a top hat gave up." },
  { "Word got round the street faster than a rent increase.", "Brenda came with the neighbours. And a dangerously flirty smile.", "Gerald brought the belly. The crowd demanded an encore." },
  { "Thump. The mailbox coughed up a metal tube.", "Inside: one letter, on very expensive paper.", "\"We noticed. Come to the office. Bring the belly.\"" },
  { "CHAPTER 1 COMPLETE", "Every landlord on Maple Street has been evicted.", "Next: the paperwork. So much paperwork." },
};
static const char *TXTE2[END_SCENES][3] = {
  { "Landlord HQ had forty floors, three districts and a plant called Kevin.", "It had a lobby fountain, a gift shop, and no insurance.", "It leaned. It thought about it. It came down." },
  { "The Head Landlord crawled out of the lobby, carrying his mug.", "\"WORLD'S OKAYEST LANDLORD\", it said. It was still full.", "He took a sip. It was cold. It had always been cold." },
  { "The neighbourhood threw a party. Dennis from accounts cried quietly.", "Gerald ate one slice of pizza. Then another. Then Dennis's.", "Above the rubble, something large and pale was rising." },
  { "CHAPTER 2 COMPLETE", "Landlord HQ is now a very large rubble pile with a gift shop.", "Next: the moon. It has opinions." },
};
static const signed char HLE1[END_SCENES] = { 1, 2, 2, 0 }, HLE2[END_SCENES] = { 2, 2, 2, 0 };
static const short HOLDE[END_SCENES] = { 204, 204, 228, 240 };
static const short CUTSE1[END_SCENES][4] = { { 0, 156, 9999, 9999 }, { 0, 132, 252, 9999 }, { 0, 96, 204, 9999 }, { 0, 9999, 9999, 9999 } };
static const short CUTSE2[END_SCENES][4] = { { 0, 144, 252, 9999 }, { 0, 144, 9999, 9999 }, { 0, 144, 276, 9999 }, { 0, 9999, 9999, 9999 } };
static const signed char TRANSE1[END_SCENES] = { 0, 1, 2, 3 }, TRANSE2[END_SCENES] = { 0, 1, 2, 3 };
static const signed char SHAKEE1[END_SCENES][3] = { {0,4,0}, {0,3,0}, {0,6,0}, {6,0,0} }, SHAKEE2[END_SCENES][3] = { {0,10,18}, {0,0,0}, {0,0,5}, {6,0,0} };
static const char *(*const TXTP[4])[3] = { TXT8, TXTW, TXTE1, TXTE2 };
static const signed char *const HLP[4] = { HL8, HLW, HLE1, HLE2 };
static const short *const HOLDP[4] = { HOLD8, HOLDW, HOLDE, HOLDE };
static const short (*const CUTSP[4])[4] = { CUTS8, CUTSW, CUTSE1, CUTSE2 };
static const signed char *const TRANSP[4] = { TRANS8, TRANSW, TRANSE1, TRANSE2 };
static const signed char (*const SHAKEP[4])[3] = { SHAKE8, SHAKEW, SHAKEE1, SHAKEE2 };
static const unsigned char NSCP[4] = { INTRO_SCENES, WOLF_SCENES, END_SCENES, END_SCENES };
static int wolf, mode;                                                   /* mode: 0 opening, 1 wolfman, 2 end of chapter 1, 3 end of chapter 2 */
#define NSC() ((int)NSCP[mode])
#define TXT(s) (TXTP[mode][s])
#define HL(s) (HLP[mode][s])
#define HOLD(s) (HOLDP[mode][s])
#define CUTS(s, k) (CUTSP[mode][s][k])
#define TRANS(s) (TRANSP[mode][s])
#define SHAKE(s, k) (SHAKEP[mode][s][k])

#define CPS 58
static int sc, t, fxdone;

static int total_chars(int s){ int n = 0; for(int i = 0; i < 3; i++) n += (int)strlen(TXT(s)[i]); return n; }
static int type_end(int s){ return total_chars(s) * 60 / CPS + 24; }          /* frame at which the last letter is down */
static int shot_of(void){ int k = 0; while(k < 3 && t >= CUTS(sc, k + 1)) k++; return k; }
static int shot_t(int k){ return t - CUTS(sc, k); }

void intro_start(void){ wolf = 0; mode = 0; sc = 0; t = 0; fxdone = 0; sfx_play(SFX_WHOOSH, 64); }
void intro_start_end(int chapter){ wolf = 0; mode = chapter == 1 ? 2 : 3; sc = 0; t = 0; fxdone = 0; sfx_play(SFX_WHOOSH, 64); }
void intro_start_wolf(void){ wolf = 1; mode = 1; sc = 0; t = 0; fxdone = 0; sfx_play(SFX_WHOOSH, 64); }
int  intro_mode(void){ return mode; }
int  intro_count(void){ return NSC(); }
int  intro_scene(void){ return sc; }

/* sound cues, frame exact */
static void cue(void){
  if(mode == 2){ switch(sc){ case 0: if(t == 1) sfx_play(SFX_BOOM, 64); if(t == 156) sfx_play(SFX_EEK, 64); break; case 1: if(t == 1) sfx_play(SFX_WIN, 64); if(t == 132) sfx_play(SFX_POP, 64); if(t == 260) sfx_play(SFX_BOING, 64); if(t == 290) sfx_play(SFX_TAUNT, 64); break; case 2: if(t == 8) sfx_play(SFX_THUD, 64); if(t == 96) sfx_play(SFX_CLICK, 64); break; default: if(t == 1) sfx_play(SFX_WIN, 64); break; } return; }
  if(mode == 3){ switch(sc){ case 0: if(t == 144) sfx_play(SFX_THUD, 64); if(t == 252){ sfx_play(SFX_BOOM, 64); sfx_play(SFX_CRUNCH, 64); } break; case 1: if(t == 144) sfx_play(SFX_EEK, 64); break; case 2: if(t == 1) sfx_play(SFX_WIN, 64); if(t == 276) sfx_play(SFX_TARGET, 64); break; default: if(t == 1) sfx_play(SFX_WIN, 64); break; } return; }
  if(wolf){ switch(sc){ case 0: if(t == 168) sfx_play(SFX_TAUNT, 64); break; case 1: if(t == 90) sfx_play(SFX_POP, 64); break; case 2: if(t == 156) sfx_play(SFX_FART, 64); break; case 3: if(t == 20) sfx_play(SFX_BOING, 64); if(t == 144) sfx_play(SFX_CRACK, 64); if(t == 228) sfx_play(SFX_BOOM, 64); break; case 4: if(t == 1) sfx_play(SFX_CRACK, 64); if(t == 90) sfx_play(SFX_WHOOSH, 64); if(t == 198) sfx_play(SFX_BOING, 64); break; case 5: if(t == 1) sfx_play(SFX_YAWN, 64); if(t == 230) sfx_play(SFX_CRACK, 64); break; case 6: if(t == 1) sfx_play(SFX_POP, 64); if(t == 144) sfx_play(SFX_BOING, 64); if(t == 210) sfx_play(SFX_POP, 64); if(t == 300) sfx_play(SFX_THUD, 64); break; case 7: if(t == 1) sfx_play(SFX_WIN, 64); break; } return; }
  switch(sc){
  case 0: if(t < 130 && t % 14 == 6) sfx_play(SFX_CLICK, 40); if(t == 138) sfx_play(SFX_TARGET, 64); break;
  case 1: if(t == 1) sfx_play(SFX_THUD, 64); for(int i = 0; i < 6; i++) if(t == 96 + i * 27) sfx_play(SFX_POP, 64); if(t == 336) sfx_play(SFX_THUD, 64); break;
  case 2: if(t >= 90 && t < 186 && t % 12 == 0) sfx_play(SFX_CLICK, 64); if(t == 206) sfx_play(SFX_BOING, 64); if(t == 276) sfx_play(SFX_YAWN, 64); break;
  case 3: if(t == 1) sfx_play(SFX_SNORE, 64); if(t == 114) sfx_play(SFX_WHOOSH, 64); if(t == 252) sfx_play(SFX_BURP, 64); break;
  case 4: if(t == 10 || t == 26 || t == 42) sfx_play(SFX_THUD, 64); if(t == 96) sfx_play(SFX_WHOOSH, 64); if(t == 240){ sfx_play(SFX_CRACK, 64); sfx_play(SFX_SLAP, 64); } break;
  case 5: if(t == 1) sfx_play(SFX_CRACK, 64); if(t == 96) sfx_play(SFX_THUD, 64); if(t == 192) sfx_play(SFX_WHOOSH, 64); break;
  case 6: if(t == 1) sfx_play(SFX_BOOM, 64); if(t == 108) sfx_play(SFX_CRUNCH, 64); if(t == 204) sfx_play(SFX_WIN, 64); break;
  case 7: if(t == 1) sfx_play(SFX_TARGET, 64); if(t == 96) sfx_play(SFX_EEK, 64); if(t == 186) sfx_play(SFX_TAUNT, 64); break;
  }
}

static int next_scene(void){ sc++; t = 0; fxdone = 0; if(sc >= NSC()) return 1; sfx_play(SFX_CLICK, 64); return 0; }

int intro_tick(int down, int tx, int ty){
  if(down & IK_START) return 1;
  if((down & IK_TOUCH) && tx >= 184 && ty >= 170) return 1;                    /* SKIP >> */
  if(down & (IK_A | IK_B | IK_TOUCH)){
    if(t < type_end(sc)) t = type_end(sc);                                      /* first press finishes the typing... */
    else return next_scene();                                                   /* ...the next goes on */
  }
  t++;
  cue();
  if(t > type_end(sc) + HOLD(sc)) return next_scene();
  return 0;
}

/* ------------------------------------------------------------------ drawing helpers */
static int wv(int tt, int per, int amp){ int p = tt % per; int h = per / 2; int v = p < h ? p : per - p; return v * 2 * amp / h - amp; }   /* triangle wave -amp..amp */
static int iabs(int v){ return v < 0 ? -v : v; }
static u32 hs(u32 n){ n = n * 2654435761u; n ^= n >> 15; n *= 2246822519u; return n ^ (n >> 13); }
static int isq(int n){ int r = 0; while((r + 1) * (r + 1) <= n) r++; return r; }

static void sky(u8 *b, int y0, int y1, int dusk){
  static const u8 DUSK[8] = { C_RED2, C_RED, C_PINK, C_GOLD2, C_GOLD, C_SKIN, C_SKIN3, C_SKIN3 };
  int span = y1 > y0 ? y1-y0 : 1;
  for(int y = y0; y < y1; y++){
    int u = (y-y0)*16/span; if(u>15) u=15;
    gfx_rect(b,0,y,256,1,dusk?DUSK[u>>1]:(u8)(C_SKY0+u));
  }
  /* Painted cloud-bank strata and refracted horizon light: thin strokes, not a texture upload. */
  for(int i=0;i<5;i++){
    int y=y0+span*(8+i)/16;
    if(y>=y1) break;
    gfx_rect(b, (i*47+13)%110, y, 100+(i&1)*35,1,dusk?C_GOLD2:C_CLOUD2);
    gfx_rect(b, (i*31+152)%236, y+3, 42,1,dusk?C_PINK:C_CLOUD);
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
static void house(u8 *b, int x, int gy, int s, int roof){
  /* Layered suburban house illustration: chimney, brick foundation, shingled gable,
     siding, real window mullions, door surround, front path and flowerboxes. */
  gfx_rect(b,x+2*s,gy-23*s,38*s,23*s,C_HAIR);
  gfx_rect(b,x+3*s,gy-22*s,36*s,20*s,C_WALL0);
  gfx_rect(b,x+3*s,gy-3*s,36*s,3*s,C_DIRT2);
  for(int i=0;i<5;i++) gfx_rect(b,x+4*s,gy-(6+i*3)*s,34*s,s>1?s/2:1,C_WALL1);
  if(roof){
    gfx_rect(b,x+30*s,gy-40*s,5*s,15*s,C_HAIR);
    gfx_rect(b,x+31*s,gy-39*s,3*s,12*s,C_DIRT2);
    gfx_rect(b,x+31*s,gy-39*s,3*s,s,C_RED2);
    gfx_tri(b,x-4*s,gy-21*s,x+44*s,gy-21*s,x+20*s,gy-40*s,C_HAIR);
    gfx_tri(b,x-2*s,gy-23*s,x+42*s,gy-23*s,x+20*s,gy-38*s,C_RED2);
    gfx_tline(b,x-2*s,gy-22*s,x+42*s,gy-22*s,s>1?s:1,C_RED);
    for(int i=0;i<3;i++)gfx_tline(b,x+(9-i*5)*s,gy-(29+i*3)*s,x+(31+i*5)*s,gy-(29+i*3)*s,1,C_RED);
    gfx_ell(b,x+20*s,gy-28*s,3*s,3*s,C_WHITE);gfx_ell(b,x+20*s,gy-28*s,2*s,2*s,C_SKY0+5);
  }
  gfx_rect(b,x+15*s,gy-16*s,10*s,16*s,C_WHITE);
  gfx_rect(b,x+16*s,gy-15*s,8*s,15*s,C_RED2);
  gfx_rect(b,x+16*s,gy-15*s,8*s,2*s,C_RED);
  gfx_rect(b,x+22*s,gy-7*s,s,s,C_GOLD);
  for(int k=0;k<2;k++){
    int wx=x+(k?28:4)*s;
    gfx_rect(b,wx,gy-19*s,9*s,10*s,C_HAIR);
    gfx_rect(b,wx+s,gy-18*s,7*s,8*s,C_WHITE);
    gfx_rect(b,wx+2*s,gy-17*s,5*s,6*s,C_SKY0+5);
    gfx_rect(b,wx+4*s,gy-17*s,s,6*s,C_WHITE);
    gfx_rect(b,wx+2*s,gy-15*s,5*s,s,C_WHITE);
    gfx_rect(b,wx-s,gy-10*s,11*s,2*s,C_DIRT2);
    for(int j=0;j<4;j++){ gfx_rect(b,wx+j*2*s,gy-12*s-(j&1)*s,s,2*s,(j&1)?C_PINK:C_GOO2); }
  }
  gfx_rect(b,x+13*s,gy-s,14*s,2*s,C_GRAY);
  gfx_tri(b,x+15*s,gy+s,x+25*s,gy+s,x+27*s,gy+10*s,C_CLOUD2);
}
/* Perspective-aware interior stage shared by office, apartment and porch.
   Layered wall panels, low-cost floor vanishing lines and a consistent baseboard
   keep the room readable while the character and props change pose. */
static void room_stage(u8 *b,int gy,u8 wall,u8 floor,u8 trim){
  gfx_rect(b,0,0,256,gy,wall);
  gfx_rect(b,0,gy,256,192-gy,floor);
  for(int x=16;x<256;x+=56){
    gfx_rect(b,x,4,2,gy-9,trim);
    gfx_rect(b,x+4,7,1,gy-16,C_CLOUD2);
    gfx_rect(b,x+12,gy-30,38,1,trim);
  }
  /* Stage cornice and floor junction give every camera angle a consistent
     architecture. The baseboard casts a small grounded shadow. */
  gfx_rect(b,0,2,256,3,trim);
  gfx_rect(b,0,5,256,1,C_CLOUD2);
  gfx_rect(b,0,gy-8,256,2,C_DARK);
  gfx_rect(b,0,gy-6,256,6,trim);
  gfx_rect(b,0,gy-5,256,1,C_CLOUD2);
  gfx_rect(b,0,gy+1,256,2,C_DARK);
  for(int x=-64;x<=320;x+=64)gfx_tline(b,128,gy,x,191,1,trim);
  for(int y=gy+14;y<192;y+=18)gfx_rect(b,0,y,256,1,trim);
}
static void bubble(u8 *b, int x, int y, const char *s, int tail){      /* speech bubble, text centred at x */
  int w = gfx_text_w(s, 1) + 10;
  gfx_rect(b, x - w / 2 - 1, y - 1, w + 2, 15, C_BLACK); gfx_rect(b, x - w / 2, y, w, 13, C_WHITE);
  gfx_tri(b, x - 3 + tail, y + 12, x + 5 + tail, y + 12, x + tail * 2, y + 22, C_BLACK); gfx_tri(b, x - 2 + tail, y + 12, x + 4 + tail, y + 12, x + tail * 2, y + 19, C_WHITE);
  gfx_text(b, x - gfx_text_w(s, 1) / 2, y + 4, s, C_BLACK, 1);
}
enum { GF_TIE = 1, GF_BOXERS = 2, GF_SAD = 4, GF_WAVE = 8, GF_COOL = 16, GF_ANGRY = 32, GF_FUR = 64, GF_EARS = 128, GF_SNOUT = 256, GF_TAIL = 512, GF_HOWL = 1024, GF_WALK = 2048, GF_SHIVER = 4096, GF_FLIRT = 8192, GF_NOD = 16384, GF_RECOIL = 32768 };
static int g_t;                                                   /* the frame counter of the shot being drawn: blinks, breathing, sway */
static void face(u8 *b, int x, int y, int s, int fl){       /* head centred x,y, radius ~5 units */
  const u8 hc = (fl & GF_FUR) ? C_GRAY : C_SKIN, hs2 = (fl & GF_FUR) ? C_DARK : C_SKIN2;
  const int big = s >= 3, blink = (g_t % 110) < 4, wink = (fl & GF_FLIRT) && (g_t % 100) > 60 && (g_t % 100) < 88;
  if(fl & GF_EARS){ gfx_tri(b, x - 5 * s, y - 3 * s, x - s, y - 5 * s, x - 5 * s, y - 11 * s, C_GRAY); gfx_tri(b, x + 5 * s, y - 3 * s, x + s, y - 5 * s, x + 5 * s, y - 11 * s, C_GRAY); gfx_tri(b, x - 4 * s, y - 4 * s, x - 2 * s, y - 5 * s, x - 4 * s, y - 8 * s, C_PINK); gfx_tri(b, x + 4 * s, y - 4 * s, x + 2 * s, y - 5 * s, x + 4 * s, y - 8 * s, C_PINK); }
  gfx_ell(b, x, y + (big ? 1 : 0), 5 * s + 1, 5 * s + 1, C_HAIR);                                                          /* outline */
  gfx_ell(b, x, y, 5 * s, 5 * s, hc); gfx_ell(b, x, y + s, 5 * s, 4 * s, hc);
  gfx_ell(b, x + 2 * s, y + 2 * s, 3 * s, 3 * s, hs2);  gfx_ell(b, x, y + s, 4 * s, 3 * s, hc);                           /* soft shadow down the right side, face front */
  gfx_ell(b, x + 5 * s, y, s, 2 * s, hs2); gfx_ell(b, x - 5 * s, y, s, 2 * s, hs2);                                         /* ears */
  gfx_ell(b, x, y - 5 * s, 5 * s, 2 * s, C_HAIR); gfx_rect(b, x - 5 * s, y - 5 * s, s, 3 * s, C_HAIR); gfx_rect(b, x + 4 * s, y - 5 * s, s, 3 * s, C_HAIR);
  if(big){ gfx_tline(b, x - 3 * s, y - 5 * s, x - 2 * s, y - 4 * s, 1, C_HAIR2); gfx_tline(b, x + s, y - 5 * s, x + 2 * s, y - 4 * s, 1, C_HAIR2); }  /* thinning hair: a few strands */
  int ey = y - s / 2, ew = s / 3 + 1;
  if(fl & GF_ANGRY){ gfx_tline(b, x - 4 * s, y - 3 * s, x - s, y - s, ew, C_BLACK); gfx_tline(b, x + 4 * s, y - 3 * s, x + s, y - s, ew, C_BLACK); ey = y; }
  else if(fl & GF_SAD){ gfx_tline(b, x - 4 * s, y - 2 * s, x - s, y - 3 * s, ew, C_BLACK); gfx_tline(b, x + 4 * s, y - 2 * s, x + s, y - 3 * s, ew, C_BLACK); }
  else if(big){ gfx_tline(b, x - 4 * s, y - 2 * s, x - s, y - 2 * s - s / 2, ew, C_HAIR); gfx_tline(b, x + s, y - 2 * s - s / 2, x + 4 * s, y - 2 * s, ew, C_HAIR); }   /* eyebrows */
  const int ew2 = 2 * s > 2 ? 2 * s - 1 : 2;
  if(blink){ gfx_tline(b, x - 3 * s, ey + ew2 / 2, x - 3 * s + ew2, ey + ew2 / 2, 1, C_BLACK); gfx_tline(b, x + s, ey + ew2 / 2, x + s + ew2, ey + ew2 / 2, 1, C_BLACK); }
  else if(big){                                                                                                                   /* eyes: whites with a pupil that drifts */
    int dx = wv(g_t, 150, 1);
    gfx_rect(b, x - 3 * s, ey - 1, ew2 + 1, ew2 + 2, C_WHITE); gfx_rect(b, x + s, ey - 1, ew2 + 1, ew2 + 2, C_WHITE);
    gfx_rect(b, x - 3 * s + (ew2 + 1) / 2 - 1 + dx, ey + 1, s - 1 + (s > 3), ew2 - 1, C_BLACK); gfx_rect(b, x + s + (ew2 + 1) / 2 - 1 + dx, ey + 1, s - 1 + (s > 3), ew2 - 1, C_BLACK);
  } else { gfx_rect(b, x - 3 * s, ey, ew2, ew2, C_BLACK); gfx_rect(b, x + s, ey, ew2, ew2, C_BLACK); }
  if(wink){
    gfx_rect(b,x-3*s,ey-1,ew2+1,ew2+2,hc);
    gfx_tline(b,x-3*s,ey+ew2/2,x-3*s+ew2,ey+ew2/2,2,C_HAIR);
    gfx_tline(b,x+s,y-2*s,x+4*s,y-3*s,2,C_HAIR2);
  }
  if(big && !(fl & GF_FUR)){ gfx_ell(b, x - 4 * s, y + 2 * s, s, s / 2 + 1, C_PINK); gfx_ell(b, x + 4 * s, y + 2 * s, s, s / 2 + 1, C_PINK); }   /* blush */
  if(fl & GF_SAD){ gfx_tline(b, x - 2 * s, y + 3 * s, x + 2 * s, y + 3 * s, ew, C_BLACK); gfx_tline(b, x - 2 * s, y + 3 * s, x - 3 * s, y + 4 * s, ew, C_BLACK); gfx_tline(b, x + 2 * s, y + 3 * s, x + 3 * s, y + 4 * s, ew, C_BLACK); }
  else if(fl & GF_ANGRY){ gfx_rect(b, x - 2 * s, y + 2 * s, 4 * s, 2 * s, C_BLACK); gfx_rect(b, x - 2 * s + 1, y + 2 * s + 1, 4 * s - 2, s, C_WHITE); }
  else { gfx_tline(b, x - 2 * s, y + 3 * s, x, y + 3 * s + s, ew, C_BLACK); gfx_tline(b, x, y + 3 * s + s, x + 3 * s, y + 2 * s, ew, C_BLACK); }
  if(fl & GF_SNOUT){ gfx_ell(b, x, y + 2 * s, 3 * s, 2 * s, C_SKIN3); gfx_ell(b, x, y + s, (s + 1) / 2 + 1, (s + 1) / 2 + 1, C_BLACK); }
  else { gfx_ell(b, x, y + s, s, s, hs2); if(big && !(fl & GF_FUR)){ gfx_ell(b, x - s, y + 2 * s - 1, 2 * s, s / 2 + 1, C_HAIR); gfx_ell(b, x + s, y + 2 * s - 1, 2 * s, s / 2 + 1, C_HAIR); } }   /* nose, and the moustache */
  if(fl & GF_HOWL){ gfx_rect(b, x - 2 * s, y + 2 * s, 4 * s, 3 * s, C_BLACK); gfx_rect(b, x - s, y + 4 * s, 2 * s, s, C_RED); }
}
/* Gerald: centre x, feet at fy, s pixels per unit (about 30 units tall) */
/* Gerald's cutscene puppet: rounded, overlapping body masses and linked
   shoulder/elbow/knee joints. Each shot is still rendered in the same palette
   framebuffer without any sprite atlas, heap data or scaling buffer. */
static void gerald(u8 *b, int x, int fy, int s, int fl){
  const int boxers=(fl&GF_BOXERS)!=0;
  const u8 skin=(fl&GF_FUR)?C_GRAY:C_SKIN, shade=(fl&GF_FUR)?C_DARK:C_SKIN2;
  const u8 shirt=boxers?skin:C_WHITE, shirtshade=boxers?shade:C_CLOUD2;
  const int breathe=s>=2?wv(g_t,100,1):0;
  const int stride=(fl&GF_WALK)?wv(g_t,24,3*s):0;
  const int liftL=stride>0?stride/3:0,liftR=stride<0?-stride/3:0;
  if(fl&GF_SHIVER)x+=wv(g_t,16,s>2?2:1);
  if(fl&GF_RECOIL)x+=wv(g_t,22,s);
  const int left=x-4*s+stride/2,right=x+4*s-stride/2;
  const int hip=fy-12*s,elbowY=fy-18*s+breathe,shoulderY=fy-23*s+breathe;
  if(fy>=140&&fy<192)gfx_ell(b,x,fy+1,8*s,2*s,C_DARK);  /* planted feet */
  if(fl&GF_TAIL){
    int tip=x+11*s+wv(g_t,60,2*s);
    gfx_tline(b,x+4*s,fy-10*s,tip,fy-16*s,3*s,C_HAIR);
    gfx_tline(b,x+4*s,fy-10*s,tip,fy-16*s,2*s,C_GRAY);
    gfx_ell(b,tip,fy-17*s,2*s,2*s,C_CLOUD2);
  }
  /* Two articulated legs: knees and feet move with the stride, but each leg
     remains connected to its hip in every walk and recoil pose. */
  gfx_tline(b,left,hip,left-stride/4,fy-2*s-liftL,3*s+2,C_HAIR);
  gfx_tline(b,right,hip,right+stride/4,fy-2*s-liftR,3*s+2,C_HAIR);
  gfx_tline(b,left,hip,left-stride/4,fy-2*s-liftL,3*s,skin);
  gfx_tline(b,right,hip,right+stride/4,fy-2*s-liftR,3*s,shade);
  gfx_ell(b,left-stride/8,fy-6*s-liftL/2,s+1,s+1,skin);
  gfx_ell(b,right+stride/8,fy-6*s-liftR/2,s+1,s+1,skin);
  gfx_ell(b,left-stride/3,fy-s-liftL,3*s,2*s,C_HAIR);
  gfx_ell(b,right+stride/3,fy-s-liftR,3*s,2*s,C_HAIR);
  gfx_ell(b,left-stride/3-s/2,fy-s-liftL-s/3,2*s,s,C_DIRT2);
  gfx_ell(b,right+stride/3-s/2,fy-s-liftR-s/3,2*s,s,C_DIRT2);
  /* Pelvis tapers into the thighs, not a disconnected red rectangular slab. */
  gfx_ell(b,x,fy-11*s,6*s+1,4*s+1,C_HAIR);
  gfx_ell(b,x,fy-11*s,6*s,4*s,boxers?C_RED:C_GOLD2);
  gfx_ell(b,x+2*s,fy-10*s,3*s,3*s,boxers?C_RED2:C_DIRT2);
  gfx_tline(b,x-5*s,fy-13*s,x+5*s,fy-13*s,s>2?2:1,boxers?C_PINK:C_GOLD);
  gfx_tline(b,x,fy-9*s,x,fy-7*s,s>2?2:1,boxers?C_RED2:C_DIRT);
  /* Layer the arms behind the torso. Both elbows and hands are attached.
     The raised hand swings in an arc rather than teleporting above the head. */
  int lHandX=x-8*s-stride/4,lHandY=fy-13*s+breathe;
  if(fl&GF_WALK){lHandX=x-8*s-stride/3;lHandY=fy-14*s+stride/4;}
  gfx_tline(b,x-6*s,shoulderY,x-9*s,elbowY,3*s+1,C_HAIR);
  gfx_tline(b,x-9*s,elbowY,lHandX,lHandY,3*s+1,C_HAIR);
  gfx_tline(b,x-6*s,shoulderY,x-9*s,elbowY,2*s+1,shirt);
  gfx_tline(b,x-9*s,elbowY,lHandX,lHandY,2*s+1,skin);
  gfx_ell(b,lHandX,lHandY,s+1,s+1,skin);
  int rElbowX=x+9*s,rElbowY=elbowY,rHandX=x+8*s+stride/3,rHandY=fy-13*s+breathe;
  if(fl&(GF_WAVE|GF_RECOIL)){
    int w=wv(g_t,(fl&GF_RECOIL)?20:36,(fl&GF_RECOIL)?2*s:s);
    rElbowX=x+9*s;rElbowY=fy-25*s+breathe;
    rHandX=x+11*s+w;rHandY=fy-31*s+breathe;
  }else if(fl&GF_WALK){rHandX=x+7*s+stride/3;rHandY=fy-14*s-stride/4;}
  gfx_tline(b,x+6*s,shoulderY,rElbowX,rElbowY,3*s+1,C_HAIR);
  gfx_tline(b,rElbowX,rElbowY,rHandX,rHandY,3*s+1,C_HAIR);
  gfx_tline(b,x+6*s,shoulderY,rElbowX,rElbowY,2*s+1,shirtshade);
  gfx_tline(b,rElbowX,rElbowY,rHandX,rHandY,2*s+1,skin);
  gfx_ell(b,rHandX,rHandY,s+1,s+1,skin);
  /* Rounded chest and a single continuous abdomen. Side shadow establishes
     volume on a low-resolution screen without a texture or framebuffer. */
  gfx_ell(b,x,fy-18*s+breathe,7*s+1,7*s+1,C_HAIR);
  gfx_ell(b,x,fy-18*s+breathe,7*s,7*s,boxers?shade:shirtshade);
  gfx_ell(b,x-s,fy-19*s+breathe,6*s,6*s,shirt);
  if(boxers){
    gfx_ell(b,x,fy-14*s,6*s,4*s,shade);
    gfx_ell(b,x-s,fy-15*s,5*s,3*s,skin);
    gfx_ell(b,x-2*s,fy-20*s+breathe,2*s,s,skin);
    if(s>=2){gfx_px(b,x,fy-14*s,C_HAIR);gfx_tline(b,x-5*s,fy-22*s,x-2*s,fy-22*s,1,shade);}
  }else{
    gfx_ell(b,x,fy-14*s,6*s,3*s,C_WHITE);
    gfx_tline(b,x+4*s,fy-22*s+breathe,x+4*s,fy-15*s,1,C_CLOUD2);
    if(s>=2)for(int i=0;i<3;i++)gfx_px(b,x-s,fy-22*s+i*3*s+breathe,C_GRAY);
  }
  if(fl&GF_TIE){
    gfx_tri(b,x-2*s,fy-24*s,x+2*s,fy-24*s,x,fy-21*s,C_BLACK);
    gfx_tri(b,x-s,fy-22*s,x+s,fy-22*s,x+s,fy-15*s,C_SKY0+4);
    gfx_tri(b,x-s,fy-22*s,x+s,fy-15*s,x-s,fy-15*s,C_SKY0+3);
    gfx_tline(b,x,fy-21*s,x,fy-17*s,1,C_WHITE);
  }
  /* The neck and head inherit the torso's breathing offset. */
  gfx_ell(b,x,fy-24*s+breathe,2*s,3*s,skin);
  int nod=(fl&GF_NOD)?wv(g_t,48,s):0;
  if(fl&GF_RECOIL)nod-=iabs(wv(g_t,22,s));
  face(b,x+((fl&GF_WALK)?stride/8:0),fy-28*s+breathe+nod,s,fl);
}
static void towel(u8 *b, int x, int y, int w, int h, int ph, int th){         /* the pink towel, rippling, with two terry stripes */
  (void)th;
  for(int i = 0; i < w; i++){ int dy = wv(ph + i * 4, 40, 3); gfx_rect(b, x + i, y + dy, 1, h, C_PINK); gfx_rect(b, x + i, y + dy + h / 4, 1, 2, C_RED2); gfx_rect(b, x + i, y + dy + h * 3 / 4, 1, 2, C_RED2); gfx_rect(b, x + i, y + dy, 1, 1, C_WHITE); }
}
static void rain(u8 *b, int tt, int n){
  for(int i = 0; i < n; i++){ int x = (int)(hs(i) % 270), y = (int)((hs(i + 99) + tt * (7 + (i & 3))) % 200); gfx_line(b, x, y - 8, x - 2, y, C_UILINE); }
}
static void stars_night(u8 *b){
  gfx_rect(b,0,0,256,192,C_UIBG);
  for(int i=0;i<42;i++){int x=(int)(hs(i+5)%256),y=(int)(hs(i+7)%112);
    gfx_px(b,x,y,(i%6==0)?C_CLOUD2:C_UILINE);
    if(i%11==0){gfx_rect(b,x-2,y,5,1,C_UILINE);gfx_rect(b,x,y-2,1,5,C_UILINE);}
  }
  /* Rooftops in two ink layers; lit windows tell us the neighbourhood is awake. */
  for(int i=0;i<13;i++){
    int x=i*23-10,h=13+(int)(hs(i+19)%21);
    gfx_rect(b,x,145-h,24,h+48,C_DARK);
    gfx_rect(b,x+2,145-h,20,1,C_UIHI);
    for(int k=0;k<3;k++){int wx=x+4+(k%2)*8,wy=151-h+k*8;
      if((hs(i*11+k)&3)==0)gfx_rect(b,wx,wy,3,4,C_GOLD2);
    }
  }
}
static void lamp(u8 *b, int x, int gy){ gfx_rect(b, x, gy - 80, 3, 80, C_DARK); gfx_rect(b, x - 8, gy - 84, 14, 5, C_DARK); gfx_ell(b, x - 1, gy - 78, 5, 3, C_GOLD); gfx_tri(b, x - 22, gy, x + 20, gy, x - 1, gy - 76, C_UIHI); gfx_dither(b, x - 18, gy - 40, 38, 40, C_UIBG); }
static void couch_back(u8 *b,int x,int gy){
  gfx_rect(b,x,gy-40,110,40,C_RED2);gfx_rect(b,x+3,gy-38,104,6,C_RED);
  gfx_rect(b,x-8,gy-30,12,30,C_RED2);gfx_rect(b,x+106,gy-30,12,30,C_RED2);
  for(int i=0;i<3;i++){gfx_rect(b,x+8+i*34,gy-30,29,23,C_RED);gfx_rect(b,x+9+i*34,gy-29,27,2,C_PINK);gfx_ell(b,x+22+i*34,gy-19,2,2,C_RED2);}
  gfx_rect(b,x-6,gy-30,8,4,C_RED);gfx_rect(b,x+108,gy-30,8,4,C_RED);
}
static void couch_front(u8 *b,int x,int gy){
  gfx_rect(b,x-8,gy-20,126,22,C_RED);gfx_rect(b,x-8,gy-4,126,6,C_RED2);
  gfx_rect(b,x+54,gy-20,2,18,C_RED2);gfx_rect(b,x-8,gy-20,126,2,C_PINK);
  gfx_rect(b,x-4,gy+2,9,5,C_HAIR);gfx_rect(b,x+106,gy+2,9,5,C_HAIR);
}
static void pizza(u8 *b, int x, int gy, int n){ for(int i = 0; i < n; i++){ gfx_rect(b, x + (i & 1) * 2, gy - 4 - i * 5, 26, 5, C_WHITE); gfx_rect(b, x + (i & 1) * 2, gy - 4 - i * 5, 26, 1, C_FIRE); gfx_rect(b, x + 9 + (i & 1) * 2, gy - 3 - i * 5, 8, 3, C_RED); } }
static void can(u8 *b, int x, int gy){ gfx_rect(b, x, gy - 7, 4, 7, C_GRAY); gfx_rect(b, x, gy - 7, 4, 1, C_WHITE); }
static void landlord(u8 *b, int x, int y, int ppc, int tt, int scared){ spr_landlord(b, x, y, ppc, tt, scared); }
static void dennis(u8 *b, int x, int by, int s, int tt){         /* behind a desk: x centre, by = desk top.  The landlord: slicked hair, jowls, a cigar, a tie like a bar chart */
  int bob = wv(tt, 50, 1) > 0, talk = wv(tt, 30, 1) > 0;
  gfx_ell(b, x, by - 5 * s, 10 * s, 7 * s, C_SUIT2); gfx_ell(b, x, by - 6 * s, 9 * s, 6 * s, C_SUIT);                       /* shoulders, lit from above */
  gfx_tri(b, x - 3 * s, by - 11 * s, x + 3 * s, by - 11 * s, x, by - 3 * s, C_WHITE);                                      /* shirt V */
  gfx_tri(b, x - 4 * s, by - 11 * s, x - s, by - 11 * s, x - 3 * s, by - 3 * s, C_SUIT2); gfx_tri(b, x + 4 * s, by - 11 * s, x + s, by - 11 * s, x + 3 * s, by - 3 * s, C_SUIT2);   /* lapels */
  gfx_tri(b, x - s, by - 11 * s, x + s, by - 11 * s, x, by - 4 * s, C_GOLD); gfx_rect(b, x - s / 2, by - 8 * s, s, s, C_GOLD2); gfx_rect(b, x - s / 2, by - 6 * s, s, s, C_GOLD2);   /* tie, striped */
  gfx_rect(b, x - 6 * s, by - 7 * s, s, s, C_GOLD);                                                                         /* pocket square */
  int hy = by - 15 * s - bob;
  gfx_ell(b, x, hy + 2 * s, 5 * s, 4 * s, C_SKIN2);                                                                        /* jowls and a second chin, in shade */
  gfx_ell(b, x, hy, 4 * s + s / 2, 4 * s, C_SKIN);                                                                          /* head */
  gfx_ell(b, x - 4 * s, hy + s, s, s, C_SKIN); gfx_ell(b, x + 4 * s, hy + s, s, s, C_SKIN);                                  /* ears */
  gfx_ell(b, x, hy - 3 * s, 4 * s + s / 2, 2 * s, C_BLACK); gfx_rect(b, x - 3 * s, hy - 4 * s, 3 * s, s / 2 + 1, C_DARK);      /* slicked hair with a shine */
  gfx_rect(b, x - 3 * s, hy - 2 * s, 2 * s + 1, s / 2 + 1, C_BLACK); gfx_rect(b, x + s, hy - 2 * s, 2 * s + 1, s / 2 + 1, C_BLACK);   /* heavy brows */
  gfx_line(b, x - 3 * s, hy - 2 * s, x - s, hy - s, C_BLACK); gfx_line(b, x + 3 * s, hy - 2 * s, x + s, hy - s, C_BLACK);    /* angry slant */
  gfx_rect(b, x - 3 * s, hy - s / 2, 2 * s, s, C_WHITE); gfx_rect(b, x + s, hy - s / 2, 2 * s, s, C_WHITE);
  gfx_rect(b, x - 2 * s, hy - s / 2, s, s, C_BLACK); gfx_rect(b, x + s, hy - s / 2, s, s, C_BLACK);                           /* beady eyes */
  gfx_rect(b, x - s / 2, hy + s / 2, s, 2 * s, C_SKIN2);                                                                     /* nose */
  gfx_rect(b, x - 3 * s, hy + s, 6 * s, s, C_HAIR);                                                                           /* moustache */
  int m = talk ? 2 : 1; gfx_rect(b, x - 2 * s, hy + 2 * s, 4 * s, m * s, C_RED2); gfx_rect(b, x - 2 * s, hy + 2 * s, 4 * s, 1, C_WHITE);   /* mouth, flapping */
  gfx_rect(b, x + 2 * s, hy + 2 * s, 4 * s, s, C_DIRT2); gfx_rect(b, x + 6 * s, hy + 2 * s, s, s, C_FIRE);                 /* cigar and its glow */
  { int u = tt % 60; gfx_ell(b, x + 7 * s + u / 6, hy - u / 5, 1 + u / 14, 1 + u / 14, C_SMOKE); }                          /* a curl of smoke */
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

static void cut_icon(u8 *b,int x,int y,int which,u8 ink); /* scene props use the same packed artwork as the title card */
/* ------------------------------------------------------------------ the scenes (top screen) */
static void sc_suburb(u8 *b, int k, int tt){
  sky(b, 0, 150, 0); sun(b, 200, 40, 14); cloud(b, 60 + tt / 6, 36, 2); cloud(b, 170 - tt / 9, 62, 1); hills(b, 140, 0); ground(b, 150);
  if(k == 0){
    fence(b, 0, 110, 152, 2); house(b, 150, 152, 2, 1);
    int gx = -20 + tt * 2; if(gx > 100) gx = 100;
    gerald(b, gx, 176 + (gx < 100 ? -iabs(wv(tt, 12, 3)) : 0), 2, GF_TIE | GF_COOL | (gx >= 100 ? GF_WAVE : GF_WALK));
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
    lamp(b, 40, 150); gerald(b, 118, 160, 2, GF_TIE | GF_SAD | GF_SHIVER); rain(b, tt, 50);
  } else if(k == 1){
    stars_night(b); rain(b, tt, 24);
    gfx_rect(b, 20, 10, 216, 172, C_WHITE); gfx_rect(b, 20, 10, 216, 3, C_UILINE);
    gfx_text_c(b, 128, 20, "DECREE OF DIVORCE", C_BLACK, C_WHITE, 1); gfx_rect(b, 32, 30, 192, 1, C_BLACK);
    cut_icon(b,208,14,MX_CUT_PAPERS,C_RED2);
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
  room_stage(b,150,C_WALL3,C_DIRT,C_DIRT2);
  if(k==0){ /* a fixed shelf, frames and window stay in the office wide shot */
    gfx_rect(b,10,15,63,82,C_HAIR);gfx_rect(b,13,18,57,76,C_UIBG);
    for(int i=0;i<3;i++){gfx_rect(b,17,28+i*21,48,2,C_DIRT2);gfx_rect(b,20+i*10,19+i*21,9,19,C_GOLD2);}
    gfx_rect(b,82,30,50,52,C_HAIR);gfx_rect(b,85,33,44,46,C_GOLD2);
    gfx_rect(b,87,35,40,42,C_WALL0);gfx_tri(b,92,66,124,44,126,68,C_HILL3);
  }
  if(k == 0 || k == 2){
    gfx_rect(b, 190, 20, 40, 40, C_WHITE); gfx_ell(b, 210, 40, 17, 17, C_CLOUD2); gfx_line(b, 210, 40, 210, 28, C_BLACK); gfx_line(b, 210, 40, 218 + wv(tt, 60, 3), 40, C_BLACK);   /* wall clock */
  }
  if(k == 0){
    dennis(b, 190, 120, 3, tt); gfx_rect(b, 110, 120, 140, 40, C_DIRT2); gfx_rect(b, 110, 120, 140, 3, C_DIRT); gerald(b, 50, 170, 2, GF_TIE | GF_SAD | GF_SHIVER);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG);
    int n = 400 * (1 + tt / 8); char buf[12]; buf[0] = '$'; const char *d = gfx_int(n); int i = 0; while(d[i] && i < 9){ buf[i + 1] = d[i]; i++; } buf[i + 1] = 0;
    gfx_text_c(b, 128, 60, buf, C_GOLD, C_BLACK, 6); gfx_text_c(b, 128, 110, "AND COUNTING", C_WHITE, C_BLACK, 2);
    cut_icon(b,22,84,MX_CUT_CASH,C_GOLD2);
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
/* Cartoon rear-view inserts, defined below and shared by all console builds. */
static void gag_rear(u8 *b,int cx,int cy,int z,int tt,int wolf);
static void gag_plunger(u8 *b,int x,int y,int z,int tt);
static void sc_couch(u8 *b, int k, int tt){
  if(k == 0){
    room_stage(b,150,C_DARK,C_UIBG,C_HAIR);
    gfx_rect(b,8,32,70,79,C_HAIR);gfx_rect(b,11,35,64,73,C_UIHI);
    gfx_rect(b,14,38,58,67,C_SKY0+3);gfx_rect(b,42,38,3,67,C_WHITE);
    gfx_rect(b,14,68,58,3,C_WHITE);gfx_ell(b,50,53,10,10,C_CLOUD2);
    gfx_rect(b,244,50,6,100,C_DIRT2);gfx_rect(b,234,48,18,4,C_GOLD2);
    gfx_rect(b,230,115,20,35,C_DIRT2);gfx_rect(b,232,116,16,3,C_GOLD2);
    int fl = 3 + ((tt * 7) & 3) + (hs(tt / 3) & 3); gfx_rect(b, 150, 30, 70, 50, C_BLACK); gfx_rect(b, 153, 33, 64, 44, C_SKY0 + 6 + (fl & 3));   /* TV */
    gfx_dither(b, 130, 14, 110, 80, C_SKY0 + 8);
    couch_back(b, 30, 160); gerald(b, 85, 160, 2, GF_BOXERS | GF_SAD | GF_SHIVER); couch_front(b, 30, 160);
    pizza(b, 170, 170, 1 + tt / 28 > 6 ? 6 : 1 + tt / 28); can(b, 150, 176); can(b, 18, 180);
    gfx_rect(b, 8, 20, 28, 34, C_WHITE); gfx_text_c(b, 22, 24, "DAY", C_BLACK, C_WHITE, 1); gfx_text_c(b, 22, 34, gfx_int(1 + tt / 12 > 9 ? 9 : 1 + tt / 12), C_RED2, C_WHITE, 2);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_BLACK);
    gfx_tri(b, 84, 20, 84, 180, 230, 170, C_UIHI); gfx_dither(b, 84, 20, 150, 160, C_BLACK);                          /* the fridge light */
    gfx_rect(b, 20, 14, 64, 170, C_WHITE); gfx_rect(b, 28, 20, 50, 158, C_CLOUD2);
    for(int i = 0; i < 3; i++){ gfx_rect(b, 28, 56 + i * 40, 50, 2, C_GRAY); gfx_rect(b, 40 + i * 12, 40 + i * 40, 8, 14, i == 1 ? C_RED : C_GOO2); }
    gerald(b,170,192,3,GF_BOXERS|GF_SAD);
    if(tt>38){gag_rear(b,216,65,1,tt,0);gfx_text_c(b,205,106,"PANTS: AWOL",C_GOLD,C_BLACK,1);}
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_WALL0); gfx_rect(b, 40, 20, 176, 140, C_WHITE); gfx_rect(b, 40, 20, 176, 18, C_RED);
    gfx_text_c(b, 128, 25, "DAY 9", C_WHITE, C_RED2, 1);
    for(int i = 0; i < 9; i++){ int x = 52 + (i % 3) * 56, y = 46 + (i / 3) * 24; if(tt > 12 + i * 8){ gfx_tline(b, x, y, x + 16, y + 16, 2, C_RED2); gfx_tline(b, x + 16, y, x, y + 16, 2, C_RED2); } }
    gfx_text_c(b, 128, 134, "PANTS: ???", C_BLACK, C_WHITE, 2);
  }
}
static void sc_landlord(u8 *b, int k, int tt){
  if(k == 0){
    room_stage(b,150,C_WALL1,C_DIRT2,C_DIRT);
    gfx_rect(b,144,33,73,120,C_HAIR);gfx_rect(b,149,37,63,115,C_DIRT);
    gfx_rect(b,154,44,52,104,C_RED2);gfx_rect(b,154,44,52,4,C_RED);
    gfx_rect(b,154,44,4,104,C_RED);gfx_rect(b,202,44,4,104,C_DARK);
    gfx_rect(b,157,147,46,3,C_HAIR);gfx_ell(b,198,100,3,3,C_GOLD);
    gfx_rect(b,150,28,60,3,C_DIRT2);gfx_rect(b,174,18,14,10,C_GOLD2);
    gfx_ell(b,181,22,7,3,C_GOLD);
    landlord(b,tt<28?90-(28-tt)*3:90,128-(tt<28?iabs(wv(tt,10,3)):0),10,tt,0);
    gerald(b,218,190,2,GF_BOXERS|GF_SAD|GF_SHIVER);
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
    /* The roof remains attached until the snap. It tilts and falls onto the
       wall instead of accelerating up and right off-screen. */
    int impact=tt>24?tt-24:0,lean=impact/4;
    if(lean>22)lean=22;
    int fall=impact*impact/120;if(fall>64)fall=64;
    int by=152-66+fall;
    gfx_tri(b,100-9+lean,by,100+129+lean/2,by+lean/3,100+60+lean,152-114+fall,C_RED2);
    gfx_tline(b,100-8+lean,by,100+129+lean/2,by+lean/3,2,C_RED);
    for(int i=0;i<8;i++){
      int born=24+i*5,age=tt-born;
      if(age<0||age>66)continue;
      int x=110+(int)(hs((u32)(i+6))%112)+age*((i&1)?1:-1)/3;
      int y=88+(i%3)*8+age*age/38;if(y>183)y=183;
      gfx_rect(b,x,y,3,3,(i&1)?C_DIRT:C_DIRT2);
    }
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
    cut_icon(b,26,72,MX_CUT_CAPE,C_GOLD);cut_icon(b,214,72,MX_CUT_TARGET,C_GOLD);
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

/* ---- chapter 3 scenes ---- */
static void moon(u8 *b,int x,int y,int r){
  gfx_ell(b,x,y,r+9,r+9,C_SUNHALO);gfx_ell(b,x,y,r+4,r+4,C_UIHI);
  gfx_ell(b,x,y,r,r,C_CLOUD);gfx_ell(b,x-r/3,y-r/4,r/4+1,r/5+1,C_CLOUD2);
  gfx_ell(b,x+r/3,y+r/3,r/5+1,r/6+1,C_CLOUD2);
  gfx_ell(b,x+r/4,y-r/3,r/8+1,r/8+1,C_WHITE);
  gfx_ell(b,x-r/4,y+r/3,r/10+1,r/12+1,C_CLOUD2);
  /* Glossy crescent edge rather than a featureless disk. */
  gfx_line(b,x-r/2,y-r/2,x-r/3,y-r*3/4,C_WHITE);
}
static void night_hills(u8 *b, int gy){ gfx_ell(b, 60, gy + 12, 120, 30, C_UIHI); gfx_ell(b, 210, gy + 16, 130, 28, C_UIBG); gfx_rect(b, 0, gy + 8, 256, 192 - gy, C_DARK); }

/* Console story skyline: low-cost integer-only urban parallax.
   Shared by DS, 3DS and Dreamcast; no allocations or texture uploads. */
static void night_town(u8 *b,int tt){
  for(int j=0;j<4;j++){
    int bx=8+j*67-(tt/11%17),ht=28+(j*13)%26;
    gfx_rect(b,bx,147-ht,49,ht+3,C_BLACK);
    gfx_rect(b,bx+2,149-ht,45,ht,C_WALL3);
    gfx_tline(b,bx-3,149-ht,bx+51,149-ht,2,C_HAIR);
    for(int i=0;i<4;i++){
      int wx=bx+8+(i&1)*23,wy=151-ht+(i>>1)*14;
      gfx_rect(b,wx,wy,9,8,C_HAIR);
      gfx_rect(b,wx+1,wy+1,7,6,(j+i+tt/120)%5?C_GOLD2:C_SKY0+4);
      gfx_rect(b,wx+4,wy,1,8,C_HAIR);
    }
  }
  for(int j=0;j<2;j++){
    int lx=45+j*164;
    gfx_rect(b,lx,101,3,48,C_GRAY);
    gfx_rect(b,lx-11,100,14,3,C_GRAY);
    gfx_ell(b,lx-11,104,7,4,C_GOLD2);
    gfx_rect(b,lx-15,105,8,1,C_WHITE);
    for(int k=0;k<3;k++)gfx_rect(b,lx-19+k*6,143+k,7,1,C_GOLD2);
  }
  gfx_rect(b,0,150,256,42,C_DARK);
  gfx_rect(b,0,151,256,2,C_GRAY);
  for(int i=0;i<7;i++)gfx_rect(b,(i*39+12)%256,178,17,1,C_CLOUD2);
}
static void w_walk(u8 *b, int k, int tt){
  stars_night(b);
  if(k == 0){
    moon(b,200,40,20+(tt>180?3:tt/60));night_hills(b,140);night_town(b,tt);
    int gx=-20+tt*2; gerald(b, gx, 176 - iabs(wv(tt, 12, 3)), 2, GF_COOL | GF_WALK);
    if(tt > 80 && gx < 220) bubble(b, gx + 40 > 200 ? 200 : gx + 40, 20, "NICE NIGHT.", 0);
  } else {
    moon(b,128,60,54+(tt>90?2:tt/45));night_hills(b,150);night_town(b,tt/2);
    gfx_ell(b,214,146,12,6,C_BLACK); gfx_ell(b, 207, 138, 4, 6, C_BLACK);
    gerald(b,70,190,3,GF_COOL|GF_RECOIL);bubble(b,120,120,"THAT'S... A LOT OF MOON.",0);
  }
}
static void w_fridge(u8 *b, int k, int tt){
  if(k == 0){
    gfx_rect(b,0,0,256,192,C_WALL3);
    gfx_rect(b,0,0,256,8,C_HAIR);
    for(int i=0;i<4;i++)gfx_rect(b,15+i*78,8,4,142,C_DIRT2);
    gfx_rect(b,0,144,256,6,C_HAIR);
    gfx_rect(b,0,150,256,42,C_DIRT);
    for(int i=0;i<7;i++)gfx_rect(b,0,153+i*7,256,1,C_DIRT2);
    for(int i=0;i<8;i++)gfx_rect(b,(i*53)%256,154+(i%3)*12,24,1,C_HAIR);
    gfx_rect(b,12,41,73,4,C_HAIR);gfx_rect(b,12,83,73,4,C_HAIR);
    for(int i=0;i<6;i++){
      int bx=15+(i%3)*22,by=46+(i/3)*42;
      gfx_rect(b,bx,by,16,31,(i%3)?C_RED2:C_DIRT2);
      gfx_rect(b,bx+3,by+3,10,2,C_GOLD2);
    }
    gfx_rect(b,100,24,48,41,C_WHITE);
    gfx_rect(b,103,27,42,35,C_SKY0+5);
    gfx_rect(b,122,27,3,35,C_WHITE);
    gfx_rect(b,103,43,42,3,C_WHITE);
    gfx_ell(b,135,36,7,7,C_CLOUD2);
    couch_back(b,20,160);couch_front(b,20,160);
    gfx_rect(b, 180, 30, 50, 122, C_WHITE); gfx_rect(b, 184, 34, 42, 114, C_CLOUD2); gfx_rect(b, 180, 80, 50, 2, C_GRAY);
    gerald(b,120,176,2,GF_COOL|GF_NOD);
  } else if(k == 1){
    gfx_rect(b, 0, 0, 256, 192, C_UIBG); gfx_tri(b, 40, 10, 40, 182, 220, 170, C_UIHI); gfx_dither(b, 40, 10, 180, 170, C_BLACK);
    gfx_rect(b, 20, 20, 216, 140, C_WHITE); gfx_rect(b, 24, 24, 208, 132, C_CLOUD2); gfx_rect(b, 24, 96, 208, 3, C_GRAY);
    gfx_tri(b, 80, 120, 170, 120, 125, 70, C_GOLD); gfx_tri(b, 125, 70, 170, 120, 170, 95, C_GOLD2); for(int i = 0; i < 5; i++) gfx_ell(b, 100 + i * 14, 108 + (i & 1) * 6, 3, 3, C_GOLD2);
    gfx_text_c(b, 128, 134, "MOON CHEESE", C_BLACK, C_CLOUD2, 1);
    cut_icon(b,199,31,MX_CUT_CHEESE,C_GOLD2);
  } else {
    gfx_rect(b, 0, 0, 256, 192, C_UIBG); gfx_rect(b, 24, 40, 208, 110, C_WHITE); gfx_rect(b, 24, 40, 208, 14, C_RED);
    gfx_text_c(b, 128, 44, "BEST BEFORE", C_WHITE, C_RED2, 1); gfx_text_c(b, 128, 80, "THE FULL", C_BLACK, C_WHITE, 3); gfx_text_c(b, 128, 108, "MOON", C_BLACK, C_WHITE, 3);
    moon(b, 214, 30, 12 + wv(tt, 40, 2));
  }
}
static void w_itch(u8 *b, int k, int tt){
  gfx_rect(b, 0, 0, 256, 192, C_UIBG);
  if(k == 0){ gfx_rect(b, 0, 150, 256, 42, C_DARK); face(b, 128, 90, 12, GF_COOL); gfx_tline(b, 98, 130, 112, 116 + wv(tt, 10, 3), 3, C_SKIN2); for(int i = 0; i < 3; i++) gfx_text(b, 170 + i * 14, 40 + wv(tt + i * 7, 20, 4), "~", C_GOLD, 2); }
  else if(k == 1){ gfx_rect(b, 60, 20, 136, 120, C_BLACK); gfx_rect(b, 64, 24, 128, 112, C_UIBG); moon(b, 128 + wv(tt, 80, 4), 80, 40 + (tt>110?2:tt/55)); gfx_rect(b, 126, 24, 4, 112, C_DARK); gfx_rect(b, 64, 78, 128, 4, C_DARK); gerald(b,214,190,2,GF_COOL|GF_RECOIL); }
  else { gfx_rect(b, 0, 150, 256, 42, C_DARK); moon(b, 128, 60, 70); face(b, 128 + wv(tt,96,1), 100, 14, GF_COOL | GF_SNOUT); for(int i = 0; i < 6; i++) gfx_text(b, 20 + i * 40, 20 + ((tt * 3 + i * 30) % 130), "<", C_WHITE, 1); }
}
static void w_ears(u8 *b, int k, int tt){
  if(k == 0){ gfx_rect(b, 0, 0, 256, 192, C_UIBG); rays(b, 128, 96, tt, 12, C_UIHI); int u = tt < 20 ? tt : 20; face(b, 128, 100, 10 + u / 6, (tt > 10 ? GF_EARS : 0) | GF_FUR | GF_COOL); gfx_text_c(b, 128, 20, "BOING", C_GOLD, C_BLACK, 3); }
  else if(k == 1){ gfx_rect(b, 0, 0, 256, 192, C_DARK); face(b, 128, 96, 14, GF_EARS | GF_FUR | GF_SNOUT); }
  else if(k == 2){ gfx_rect(b, 0, 0, 256, 192, C_UIBG); gerald(b, 128, 180, 4, GF_EARS | GF_FUR | GF_SNOUT | GF_COOL); for(int i = 0; i < 8; i++){ int y = 70 + (tt * 3 + i * 20) % 110; gfx_ell(b, 90 + (int)(hs(i) % 80), y, 3, 3, C_WHITE); } }
  else { gfx_rect(b, 0, 0, 256, 192, C_RED2); face(b, 128, 96, 16, GF_EARS | GF_FUR | GF_SNOUT); gfx_rect(b, 104, 112, 48, 6, C_BLACK); gfx_text_c(b, 128, 150, "LOAD-BEARING", C_GOLD, C_BLACK, 2); }
}

/* Squash-and-stretch close-up: all costume/fur and stylized comic linework.
   Draws only into the existing 256x192 8-bit buffer, no image assets. */
static void gag_rear(u8 *b,int cx,int cy,int z,int tt,int wolf){
  int wig=wv(tt,28,3*z),bob=iabs(wv(tt,28,2*z));
  u8 cloth=wolf?C_GRAY:C_RED,shade=wolf?C_DARK:C_RED2;
  gfx_ell(b,cx,cy,35*z,29*z,C_BLACK);
  gfx_ell(b,cx,cy,33*z,27*z,C_UIHI);
  gfx_ell(b,cx,cy+2*z,31*z,25*z,C_DARK);
  gfx_ell(b,cx-13*z-wig,cy+5*z+bob,16*z,19*z,C_BLACK);
  gfx_ell(b,cx+13*z+wig,cy+5*z+bob,16*z,19*z,C_BLACK);
  gfx_ell(b,cx-13*z-wig,cy+4*z+bob,15*z,17*z,cloth);
  gfx_ell(b,cx+13*z+wig,cy+4*z+bob,15*z,17*z,shade);
  gfx_ell(b,cx-19*z-wig,cy-2*z+bob,6*z,5*z,wolf?C_CLOUD2:C_RED);
  gfx_ell(b,cx+7*z+wig,cy-2*z+bob,6*z,5*z,wolf?C_GRAY:C_PINK);
  gfx_rect(b,cx-30*z,cy-13*z+bob,60*z,5*z,C_BLACK);
  gfx_rect(b,cx-28*z,cy-13*z+bob,56*z,3*z,wolf?C_CLOUD2:C_WHITE);
  gfx_rect(b,cx-28*z,cy-10*z+bob,56*z,z,wolf?C_DARK:C_RED2);
  gfx_tline(b,cx,cy-6*z+bob,cx+wig/2,cy+20*z+bob,z,C_BLACK);
  gfx_tline(b,cx-25*z,cy+13*z+bob,cx-10*z,cy+20*z+bob,1,C_WHITE);
  gfx_tline(b,cx+11*z,cy+19*z+bob,cx+26*z,cy+12*z+bob,1,C_PINK);
  if(wolf){
    int tip=cx+29*z+wv(tt,18,9*z);
    gfx_tline(b,cx+20*z,cy-11*z,tip,cy-23*z+bob,4*z,C_BLACK);
    gfx_tline(b,cx+20*z,cy-12*z,tip,cy-24*z+bob,2*z,C_GRAY);
    gfx_ell(b,tip,cy-24*z+bob,3*z,3*z,C_CLOUD2);
  }
  for(int i=0;i<4;i++){
    int a=tt<14?tt*3:36,xx=cx+(i&1?1:-1)*(30*z+a);
    gfx_line(b,xx,cy-12*z+i*8*z,xx+(i&1?7:-7)*z,cy-10*z+i*8*z,C_GOLD);
  }
}
/* Plunger comedy: the rubber cup touches clothing, then springs off it.
   The prop never enters clothing, the body, or anything else. */
static void gag_plunger(u8 *b,int x,int y,int z,int tt){
  int contact=tt<22?tt:22,retreat=tt>28?(tt-28)*3:0;
  if(retreat>90)retreat=90;
  int cup=x+28*z+18*z-contact*z/2+retreat;
  int end=cup+24*z+wv(tt,28,2*z);
  gfx_tline(b,cup,y+7*z,end,y-9*z,3*z,C_DIRT2);
  gfx_tline(b,cup,y+7*z,end,y-9*z,1,C_GOLD2);
  gfx_ell(b,cup,y+7*z,7*z,5*z,C_BLACK);
  gfx_ell(b,cup-2*z,y+6*z,5*z,3*z,C_PINK);
  if(tt>28&&tt<51){
    gfx_text(b,cup-7*z,y-23*z,"BOING!",C_GOLD,1);
    gfx_line(b,cup,y-5*z,cup+8*z,y-15*z,C_WHITE);
  }
}
static void w_tail(u8 *b, int k, int tt){
  stars_night(b); gfx_rect(b, 0, 150, 256, 42, C_DARK);
  if(k == 0){ gerald(b, 128, 176, 4, GF_EARS | GF_FUR | GF_SNOUT | GF_COOL); gfx_text_c(b, 128, 20, "RRRIP.", C_GOLD, C_BLACK, 2); }
  else if(k == 1){ gerald(b, 120, 176, 4, GF_EARS | GF_FUR | GF_SNOUT | GF_TAIL | GF_COOL); gfx_text_c(b, 128, 20, "PANTS: OPTIONAL", C_WHITE, C_BLACK, 2); }
  else {
    gfx_rect(b,0,0,256,192,C_RED2);
    rays(b,128,100,tt/2,12,C_RED);
    gag_rear(b,115,104,2,tt,1);
    if(tt<80)gag_plunger(b,115,104,1,tt);
    gfx_rect(b,5,19,246,18,C_BLACK);
    gfx_text_c(b,128,23,"EXTREME REAR VIEW",C_GOLD,C_BLACK,2);
    gfx_text_c(b,128,172,tt<90?"WARNING: WIGGLE ZONE":"TAIL: UNLICENSED",C_WHITE,C_BLACK,1);
  }
}
static void w_howl(u8 *b, int k, int tt){
  stars_night(b);
  if(k == 0){ moon(b, 128, 70, 62); night_hills(b, 150); gerald(b, 128, 182, 3, GF_EARS | GF_FUR | GF_SNOUT | GF_TAIL | GF_HOWL); gfx_text_c(b, 128, 14, "AWOOOO", C_WHITE, C_BLACK, 3); }
  else if(k == 1){ moon(b, 128, 80, 30); gfx_rect(b, 0, 130, 256, 62, C_DARK); face(b, 128, 120, 16, GF_EARS | GF_FUR | GF_SNOUT | GF_HOWL); bubble(b, 128, 12, "A LOT. HE SAID A LOT.", 0); }
  else { gfx_rect(b, 0, 0, 256, 192, C_WALL1); gfx_rect(b, 0, 150, 256, 42, C_DIRT2); landlord(b, 128, 130, 10, tt, 1); int cr = tt > 20; gfx_rect(b, 150, 70, 10, 14, cr ? C_CLOUD2 : C_RED); gfx_rect(b, 154, 84, 2, 10, C_WHITE); if(cr) gfx_text_c(b, 160, 56, "CRACK!", C_GOLD, C_BLACK, 2); }
}
static void brenda(u8 *b,int x,int fy,int s,int t,int flirt){ /* adult character, deliberately cheeky glamour pose */
  if(flirt)x+=wv(t,60,2); /* hips sway; the face and hem follow */
  int bob=wv(t,40,1)+(flirt?wv(t,36,1):0),hy=fy-27*s+bob;
  int skirt_sway=flirt?wv(t+9,40,s):0;
  gfx_ell(b, x, hy + s, 8 * s, 9 * s, C_GOLD2);                                                                              /* the hair behind, in shade */
  gfx_ell(b, x - 7 * s, hy + 7 * s, 3 * s, 4 * s, C_GOLD2); gfx_ell(b, x + 7 * s, hy + 7 * s, 3 * s, 4 * s, C_GOLD2);        /* curls on the shoulders */
  gfx_rect(b, x - 4 * s, fy - 10 * s, 2 * s, 10 * s, C_SKIN); gfx_rect(b, x + 2 * s, fy - 10 * s, 2 * s, 10 * s, C_SKIN);     /* legs */
  gfx_rect(b, x - 4 * s, fy - s, 3 * s, s, C_RED); gfx_rect(b, x + s, fy - s, 3 * s, s, C_RED);                              /* heels */
  gfx_rect(b, x - 3 * s / 2, hy + 4 * s, 3 * s, 5 * s, C_SKIN); gfx_rect(b, x + s / 2, hy + 4 * s, s, 5 * s, C_SKIN2);                  /* neck, shaded on one side */
  gfx_tri(b,x-8*s+skirt_sway,fy-8*s+bob,x+8*s+skirt_sway,fy-8*s+bob,x,fy-22*s+bob,C_PINK);                         /* skirt */
  gfx_rect(b, x - 6 * s, fy - 12 * s + bob, 12 * s, 3 * s, C_PINK); gfx_rect(b, x - 6 * s, fy - 13 * s + bob, 12 * s, s, C_RED);   /* hem, belt */
  gfx_rect(b,x-5*s,fy-21*s+bob,10*s,8*s,C_PINK);
  gfx_tri(b,x-2*s,fy-21*s+bob,x+2*s,fy-21*s+bob,x,fy-18*s+bob,C_SKIN);
  if(flirt){ /* sweetheart neckline and satin side-slit, no nudity */
    gfx_tline(b,x-5*s,fy-20*s+bob,x,fy-18*s+bob,1,C_RED2);
    gfx_tline(b,x,fy-18*s+bob,x+5*s,fy-20*s+bob,1,C_RED2);
    gfx_line(b,x+4*s,fy-11*s+bob,x+7*s,fy-8*s+bob,C_RED);
    gfx_line(b,x+5*s,fy-13*s+bob,x+7*s,fy-10*s+bob,C_WHITE);
    gfx_ell(b,x-4*s,fy-15*s+bob,s,s,C_WHITE);
  }
  for(int i = 0; i < 4; i++) gfx_rect(b, x - 7 * s + i * 4 * s + (i & 1) * s, fy - 11 * s + bob + (i & 1) * s, s, s, C_WHITE);        /* polka dots on the hem */
  int wave = wv(t, 24, 1);
  gfx_rect(b, x - 7 * s, fy - 20 * s + bob, 2 * s, 7 * s, C_SKIN); gfx_rect(b, x + 5 * s, fy - 20 * s + bob - (wave > 0 ? s : 0), 2 * s, 7 * s, C_SKIN);   /* arms, one waving */
  gfx_ell(b, x - 6 * s, fy - 20 * s + bob, s + s / 2, s + s / 2, C_SKIN); gfx_ell(b, x + 6 * s, fy - 20 * s + bob - (wave > 0 ? s : 0), s + s / 2, s + s / 2, C_SKIN);   /* round shoulders */
  gfx_ell(b, x - 6 * s, fy - 12 * s + bob, s + s / 2, s, C_SKIN); gfx_ell(b, x + 6 * s, fy - 12 * s + bob - (wave > 0 ? s : 0), s + s / 2, s, C_SKIN);                                     /* round hands */
  gfx_rect(b, x + 8 * s, fy - 14 * s + bob, 5 * s, 4 * s, C_DIRT); gfx_rect(b, x + 8 * s, fy - 14 * s + bob, 5 * s, s, C_DIRT2); gfx_rect(b, x + 10 * s, fy - 16 * s + bob, s, 2 * s, C_DIRT2);   /* the handbag, its strap */
  gfx_ell(b, x, hy + s, 5 * s + s / 2, 6 * s, C_SKIN);                                                                       /* face (no chin shadow: it read as a beard) */
  gfx_ell(b, x, hy - 4 * s, 7 * s, 3 * s, C_GOLD); gfx_tri(b, x - 6 * s, hy - 3 * s, x, hy - 3 * s, x - 5 * s, hy + 2 * s, C_GOLD); gfx_tri(b, x + 6 * s, hy - 3 * s, x, hy - 3 * s, x + 5 * s, hy + 2 * s, C_GOLD);   /* fringe */
  gfx_rect(b, x - 3 * s, hy - 5 * s, s, s, C_WHITE);                                                                         /* a shine on the hair */
  int blink=(t%90)>86,wink=flirt&&(t%108)>64&&(t%108)<84;
  gfx_rect(b,x-3*s,hy-s,2*s,(blink||wink)?1:s+1,(blink||wink)?C_BLACK:C_WHITE); gfx_rect(b, x + s, hy - s, 2 * s, blink ? 1 : s + 1, blink ? C_BLACK : C_WHITE);
  if(!blink){if(!wink)gfx_rect(b,x-2*s,hy-s,s,s,C_BLACK);gfx_rect(b,x+s,hy-s,s,s,C_BLACK);}
  gfx_line(b, x - 3 * s, hy - s, x - 4 * s, hy - 2 * s, C_BLACK); gfx_line(b, x + 3 * s, hy - s, x + 4 * s, hy - 2 * s, C_BLACK);    /* lashes */
  gfx_rect(b, x - 3 * s, hy - 2 * s, 2 * s, 1, C_HAIR); gfx_rect(b, x + s, hy - 2 * s, 2 * s, 1, C_HAIR);                      /* brows */
  gfx_ell(b, x - 4 * s, hy + 2 * s, s, s / 2 + 1, C_PINK); gfx_ell(b, x + 4 * s, hy + 2 * s, s, s / 2 + 1, C_PINK);            /* blush */
  gfx_rect(b, x - 2 * s, hy + 3 * s, 4 * s, s, C_RED); gfx_rect(b, x - s, hy + 3 * s, s, 1, C_WHITE);                         /* lipstick, with a highlight */
  gfx_rect(b,x-3*s,hy+3*s-1,s,1,C_RED);gfx_rect(b,x+2*s,hy+3*s-1,s,1,C_RED);
  if(flirt){gfx_ell(b,x-5*s,hy+2*s,s+1,s,C_PINK);gfx_ell(b,x+5*s,hy+2*s,s+1,s,C_PINK);
    if(t%90<45)gfx_text(b,x+7*s,hy-8*s+wv(t,36,2),"<3",C_PINK,1);
  }
}
static void w_visit(u8 *b, int k, int tt){
  if(k==0){
    stars_night(b);moon(b,234,38,13);night_hills(b,150);
    /* The doorstep is now a proper miniature stage: architectural silhouette,
       layered shingles, brickwork, glazed mullions and warm porch lighting. */
    gfx_rect(b,0,155,256,37,C_DIRT2);
    gfx_rect(b,0,157,256,2,C_CLOUD2);
    gfx_rect(b,148,68,72,88,C_HAIR);
    gfx_rect(b,151,70,66,85,C_WALL3);
    for(int i=0;i<11;i++){
      int yy=76+i*7;
      gfx_rect(b,154,yy,60,1,C_WALL1);
      gfx_rect(b,154+(i&1)*12,yy+3,10,1,C_DIRT2);
      gfx_rect(b,183+(i&1)*12,yy+3,10,1,C_DIRT2);
    }
    gfx_rect(b,203,48,7,26,C_HAIR);
    gfx_rect(b,204,49,5,24,C_RED2);
    gfx_tri(b,142,72,225,72,182,49,C_HAIR);
    gfx_tri(b,146,70,221,70,182,53,C_RED2);
    gfx_tline(b,146,70,221,70,2,C_RED);
    for(int i=0;i<4;i++)gfx_line(b,155+i*14,67,167+i*14,61,C_RED);
    gfx_rect(b,158,96,22,32,C_WHITE);
    gfx_rect(b,160,98,18,28,C_SKY0+5);
    gfx_rect(b,168,98,2,28,C_WHITE);
    gfx_rect(b,160,109,18,2,C_WHITE);
    gfx_rect(b,155,128,27,3,C_RED2);
    gfx_rect(b,166,128,4,9,C_DIRT);
    gfx_rect(b,183,93,29,63,C_HAIR);
    gfx_rect(b,186,96,23,60,C_DIRT2);
    gfx_rect(b,188,99,19,54,C_RED2);
    gfx_rect(b,189,101,17,3,C_RED);
    gfx_rect(b,191,109,13,14,C_DIRT);
    gfx_rect(b,192,110,11,12,C_GOLD2);
    gfx_rect(b,197,110,2,12,C_WHITE);
    gfx_ell(b,204,127,2,2,C_GOLD);
    gfx_rect(b,181,155,33,4,C_CLOUD2);
    gfx_rect(b,178,160,39,3,C_GRAY);
    gfx_rect(b,174,165,47,3,C_CLOUD);
    /* Sconces and two little plant pots make the home feel inhabited. */
    gfx_rect(b,180,94,3,7,C_HAIR);
    gfx_ell(b,181,99,5,4,C_GOLD2);
    gfx_ell(b,181,99,2,2,C_WHITE);
    for(int i=0;i<2;i++){
      int px=150+i*68;
      gfx_rect(b,px-3,150,7,8,C_DIRT);
      gfx_rect(b,px-4,149,9,2,C_RED2);
      gfx_tline(b,px,148,px+(i?5:-5),139,2,C_GRASS2);
      gfx_tline(b,px,148,px+(i?-6:6),142,2,C_GRASS2);
      gfx_ell(b,px+(i?5:-5),138,2,2,C_PINK);
    }
    brenda(b,90,176,3,tt,0);
    gerald(b,196,176,2,GF_FUR|GF_EARS|GF_SNOUT|GF_TAIL|GF_COOL|GF_FLIRT|GF_NOD);
    bubble(b,70,40,"HI, GERALD.",0);
  }
  else if(k==1){
    gfx_rect(b,0,0,256,192,C_RED2);
    /* Stained-glass romance stage: radiating lines, fabric drapes and
       a bright inset, all simple integer strokes behind the actors. */
    for(int i=0;i<7;i++){
      int px=12+i*37;
      gfx_tline(b,128,94,px,0,1,(i&1)?C_GOLD2:C_RED);
      gfx_tline(b,128,94,px,191,1,(i&1)?C_PINK:C_RED);
    }
    gfx_ell(b,126,100,118,88,C_RED);
    gfx_ell(b,130,94,80,68,C_PINK);
    for(int j=0;j<5;j++){
      gfx_line(b,12+j*56,0,12+j*56,18,C_GOLD2);
      gfx_ell(b,12+j*56,19+wv(tt+j*9,48,3),3,3,C_GOLD);
    }
    gfx_rect(b,0,175,256,17,C_RED);
    gfx_rect(b,0,175,256,2,C_GOLD2);
    for(int i=0;i<9;i++){
      int xx=24+i*26+wv(tt+i*9,38,4),yy=110-((tt*2+i*24)%125);
      gfx_text(b,xx,yy,"<3",i&1?C_WHITE:C_RED2,1+(i%3==0));
    }
    brenda(b,60,186,4,tt,1);
    gerald(b,190,186,3,GF_FUR|GF_EARS|GF_SNOUT|GF_TAIL|GF_COOL|GF_FLIRT|GF_NOD);
    bubble(b,62,20,"THAT TAIL'S TROUBLE.",0);
    if(tt>90)bubble(b,196,60,"WANNA FIND OUT?",0);
    if(tt>130)gfx_text_c(b,128,154,"TOO HOT FOR THIS CAMERA",C_GOLD,C_RED2,1);
  }
  else {
    gfx_rect(b,0,0,256,192,C_UIBG);
    moon(b,128,90,52);
    /* Brief silhouettes behind frosted glass; a playful embrace and a kiss,
       then curtains politely end the shot. Never graphic or naked. */
    if(tt<54){
      int lean=tt<28?tt/4:7, kiss=tt>21?3:0;
      /* Lighting on the jaw, hair, wolf ears and hands makes these clearly
         two *people* in profile, rather than ambiguous circular shapes. */
      gfx_ell(b,103+lean,124,28,24,C_DARK); /* Brenda's shoulders */
      gfx_ell(b,153-lean,124,29,25,C_DARK); /* Gerald's shoulders */
      gfx_rect(b,75+lean,122,55,25,C_DARK);
      gfx_rect(b,132-lean,122,57,25,C_DARK);
      gfx_ell(b,102+lean,82,14,18,C_DARK);
      gfx_ell(b,149-lean,81,16,19,C_DARK);
      /* Crown of hair, fringe and bangs flowing away from the kiss */
      gfx_ell(b,96+lean,70,17,8,C_DARK);
      gfx_tri(b,83+lean,76,87+lean,93,95+lean,83,C_DARK);
      gfx_tri(b,86+lean,69,84+lean,58,98+lean,68,C_DARK);
      gfx_tri(b,144-lean,67,140-lean,48,153-lean,62,C_DARK);
      gfx_tri(b,160-lean,68,164-lean,49,154-lean,61,C_DARK);
      /* Two asymmetric nose-and-chin profiles approach at the center */
      gfx_tri(b,113+lean,81,122+lean+kiss,86,113+lean,91,C_DARK);
      gfx_tri(b,138-lean,82,129-lean-kiss,87,138-lean,92,C_DARK);
      gfx_tline(b,114+lean,95,116+lean,99,1,C_CLOUD2);
      gfx_tline(b,137-lean,96,135-lean,99,1,C_CLOUD2);
      gfx_line(b,95+lean,82,95+lean,89,C_CLOUD2);
      gfx_line(b,158-lean,81,158-lean,89,C_CLOUD2);
      /* Hand on shoulder and the other arm around the waist */
      gfx_tline(b,94+lean,126,142-lean,120,9,C_DARK);
      gfx_ell(b,142-lean,120,5,5,C_DARK);
      gfx_tline(b,160-lean,121,120+lean,133,7,C_DARK);
      gfx_ell(b,120+lean,133,4,4,C_DARK);
      if(tt>19){
        gfx_text_c(b,128,45,"SMOOCH!",C_PINK,C_UIBG,1);
        gfx_text(b,171,85+wv(tt,34,3),"<3",C_PINK,1);
      }
    }
    int curtain=tt<40?tt:40;
    gfx_rect(b,0,0,curtain*128/40,192,C_DARK);
    gfx_rect(b,256-curtain*128/40,0,curtain*128/40,192,C_DARK);
    if(tt>80)gfx_text_c(b,128,168,"THE CAMERA PREFERS THE MOON.",C_GOLD,C_BLACK,1);
  }
}
static void w_title(u8 *b, int k, int tt){
  (void)k; stars_night(b); moon(b, 128, 80, 50 + wv(tt, 120, 3)); night_hills(b, 150);
  gerald(b, 128, 182, 3, GF_EARS | GF_FUR | GF_SNOUT | GF_TAIL | GF_HOWL);
  gfx_text_c(b, 128, 12, "CHAPTER 3", C_GOLD, C_BLACK, 2); gfx_text_c(b, 128, 34, "THE WOLFMAN", C_WHITE, C_BLACK, 3);
}


/* ---- closing scenes ---- */
static void rubble(u8 *b, int x, int gy, int s, int seed){
  for(int i = 0; i < 9; i++){ int bx = x + (int)(hs(seed + i) % 120) * s / 2 - 30 * s, bw = (10 + (int)(hs(seed + i + 30) % 14)) * s / 2, bh = (6 + (int)(hs(seed + i + 60) % 12)) * s / 2, by = gy - bh - (i % 3) * 3 * s;
    gfx_tri(b, bx, by + bh, bx + bw / 2, by, bx + bw, by + bh, (i % 3) ? C_DIRT : C_RED2); gfx_rect(b, bx, by + bh, bw, 3 * s, (i % 3) ? C_DIRT2 : C_RED2); }
}
static void smoke(u8 *b, int x, int gy, int tt, int n){ for(int i = 0; i < n; i++){ int ph = (tt + i * 240 / n) % 240; gfx_dither(b, x + wv(ph + i * 13, 90, 12) + (i - n / 2) * 5, gy - ph * 3 / 4, 8 + ph / 12, 6 + ph / 14, C_CLOUD2); } }
static void confetti(u8 *b, int tt, int n){ static const u8 col[5] = { C_GOLD, C_PINK, C_GRASS2, C_SKY0 + 6, C_WHITE }; for(int i = 0; i < n; i++){ int x = (int)(hs(i) % 256) + wv(tt + i * 7, 40, 6), y = (int)(((u32)tt * (2 + (hs(i + 9) % 3)) + hs(i + 3)) % 200) - 4; gfx_rect(b, x, y, 3, 2, col[i % 5]); } }
static void folk(u8 *b, int x, int gy, int tt, u8 c, int sign){ int j = iabs(wv(tt * 3 + x, 24, 2)); gfx_rect(b, x - 3, gy - 11 - j, 6, 11, c); gfx_rect(b, x - 3, gy - 11 - j, 2, 11, C_DARK); gfx_ell(b, x, gy - 14 - j, 3, 3, C_SKIN); gfx_rect(b, x - 3, gy - 18 - j, 6, 2, (u8)((x / 9) & 1 ? C_HAIR : C_GOLD2)); gfx_px(b, x - 1, gy - 14 - j, C_BLACK); gfx_px(b, x + 1, gy - 14 - j, C_BLACK);
  gfx_tline(b, x - 3, gy - 9 - j, x - 6, gy - 17 - j + wv(tt + x, 20, 2), 2, C_SKIN); gfx_tline(b, x + 3, gy - 9 - j, x + 6, gy - 17 - j - wv(tt + x, 20, 2), 2, C_SKIN);
  if(sign){ gfx_rect(b, x + 6, gy - 30 - j, 14, 9, C_WHITE); gfx_rect(b, x + 7, gy - 28 - j, 12, 2, C_RED); gfx_rect(b, x + 12, gy - 21 - j, 2, 12, C_DIRT2); } }
static void tower(u8 *b, int x, int gy, int lean, int fall){      /* HQ: lean in 1/64 of a unit per row, fall 0..64 shrinks it */
  int th = 120 - fall * 55 / 64, w = 46;
  for(int y = 0; y < th; y++){ int dx = lean * y / 64; gfx_rect(b, x - w / 2 + dx, gy - y, w, 1, y % 14 < 2 ? C_UIHI : C_UILINE); }
  for(int r = 0; r < 6; r++) for(int c = 0; c < 4; c++){ int y = 12 + r * 17; if(y + 8 >= th) continue; int dx = lean * y / 64; gfx_rect(b, x - w / 2 + 5 + c * 10 + dx, gy - y - 8, 6, 8, hs(r * 4 + c) % 3 ? C_GOLD : C_DARK); }
  int dx = lean * th / 64; gfx_rect(b, x - w / 2 - 3 + dx, gy - th - 4, w + 6, 5, C_DARK);
}
static void e1_rubble(u8 *b, int k, int tt){
  if(k == 0){ sky(b, 0, 150, 1); sun(b, 190, 100, 20); hills(b, 140, 0); ground(b, 150); smoke(b, 70, 150, tt, 4); smoke(b, 170, 150, tt + 40, 4); rubble(b, 128, 170, 3, 3);
    spr_man(b, 118, 112, 6, 1, POSE_FLEX, tt >> 3); if(tt > 80) bubble(b, 190, 24, "...IS THAT ALL OF IT?", 0); }
  else { sky(b, 0, 150, 1); sun(b, 190, 100, 20); hills(b, 140, 0); ground(b, 150); smoke(b, 190, 150, tt, 4); rubble(b, 70, 172, 3, 9);
    spr_man(b, 70, 120, 5, 1, POSE_IDLE, tt >> 3); landlord(b, 180, 148, 8, tt, 1);
    { int w = tt < 30 ? tt : 30; gfx_rect(b, 210, 40, 2, 60, C_DIRT2); gfx_rect(b, 212, 40, w * 22 / 30, 16, C_WHITE); }
    if(tt > 50) bubble(b, 120, 14, "I SURRENDER. JUST A MAN IN A HAT.", 0); }
}
static void e1_street(u8 *b, int k, int tt){
  static const u8 col[5] = { C_SKY0 + 4, C_RED, C_GRASS2, C_DIRT, C_PINK };
  sky(b, 0, 150, 1); sun(b, 40, 80, 18); hills(b, 140, 0); ground(b, 150); gfx_rect(b, 0, 164, 256, 20, C_SKIN3);
  for(int i = 0; i < 7; i++) folk(b, 18 + i * 36, 172 + (i & 1) * 8, tt + i * 7, col[i % 5], i & 1);
  confetti(b, tt, 30);
  if(k == 0){ brenda(b, 100, 176, 3, tt, 0); spr_man(b, 190, 130, 6, 1, POSE_FLEX, tt >> 3); bubble(b, 90, 16, "YOU DID IT, GERALD!", 0); }
  else if(k == 1){ brenda(b, 60, 186, 4, tt, 1); spr_man(b, 170, 140, 8, 1, POSE_IDLE, tt >> 3); for(int i = 0; i < 4; i++) gfx_text(b, 100 + i * 20 + wv(tt + i * 9, 50, 6), 70 - ((tt + i * 26) % 60), "<3", C_PINK, 2);
    bubble(b,60,14,"NICE PACKAGE, GERALD.",0);if(tt>80)bubble(b,190,56,"THE BELLY, I MEAN.",0);}
  else {
    spr_man(b,128+wv(tt,32,3),136-iabs(wv(tt,28,3)),8,1,(tt/20)&1?POSE_IDLE:POSE_FLEX,tt>>3);
    gag_rear(b,213,112,1,tt,0);
    if(tt<72)gag_plunger(b,213,112,1,tt);
    bubble(b,128,12,"RENT IS CANCELLED!",0);
    gfx_text_c(b,213,159,"ENCORE!",C_GOLD,C_BLACK,1);
  }
}
static void e1_letter(u8 *b, int k, int tt){
  stars_night(b); night_hills(b, 150);
  if(k == 0){ gfx_rect(b, 60, 118, 4, 40, C_DIRT2); gfx_rect(b, 48, 104, 28, 18, C_SKY0 + 3); { int pop = tt < 20 ? tt : 20; gfx_rect(b, 52, 110 - pop, 20, 8, C_GRAY); }
    spr_man(b, 190, 150, 6, 1, POSE_IDLE, tt >> 3); bubble(b, 120, 20, "...THAT'S NOT MAIL.", 0); }
  else if(k == 1){ gfx_rect(b, 44, 18, 168, 144, C_WHITE); gfx_rect(b, 44, 18, 168, 2, C_BLACK); gfx_rect(b, 44, 160, 168, 2, C_BLACK); gfx_rect(b, 44, 18, 2, 144, C_BLACK); gfx_rect(b, 210, 18, 2, 144, C_BLACK);
    gfx_text_c(b, 128, 26, "LANDLORD HQ", C_RED2, C_WHITE, 2); gfx_rect(b, 56, 46, 144, 2, C_RED);
    gfx_text_c(b, 128, 62, "DEAR MR. FITZGERALD,", C_BLACK, C_WHITE, 1); gfx_text_c(b, 128, 78, "YOUR WORK HAS BEEN NOTICED.", C_BLACK, C_WHITE, 1); gfx_text_c(b, 128, 94, "YOUR PAPERWORK HAS NOT", C_BLACK, C_WHITE, 1); gfx_text_c(b, 128, 104, "BEEN FILED.", C_BLACK, C_WHITE, 1); gfx_text_c(b, 128, 124, "PLEASE REPORT TO HQ.", C_BLACK, C_WHITE, 1);
    if(tt > 54) gfx_text_c(b, 150, 142, "URGENT", C_RED, C_WHITE, 2); }
  else { spr_man(b, 128, 150, 8, 1, POSE_SAD, tt >> 3); bubble(b, 128, 16, "DO THEY VALIDATE PARKING?", 0); }
}
static void e_title(u8 *b, int k, int tt, int ch){
  (void)k;
  if(ch == 1){ sky(b, 0, 192, 1); rays(b, 128, 150, tt, 14, C_GOLD); confetti(b, tt, 36); gfx_text_c(b, 128, 12, "CHAPTER 1", C_GOLD, C_RED2, 3); gfx_text_c(b, 128, 38, "COMPLETE", C_WHITE, C_RED2, 2); gfx_rect(b, 0, 160, 256, 32, C_GRASS2); }
  else { stars_night(b); moon(b, 128, 90, 44 + wv(tt, 120, 3)); night_hills(b, 150); gfx_text_c(b, 128, 12, "CHAPTER 2", C_GOLD, C_DARK, 3); gfx_text_c(b, 128, 38, "COMPLETE", C_WHITE, C_DARK, 2); }
  spr_man(b, 128, ch == 1 ? 130 : 138, 7, 1, POSE_FLEX, tt >> 3);
  cut_icon(b,22,70,MX_CUT_STAR,C_GOLD);
  cut_icon(b,216,70,MX_CUT_STAR,C_GOLD);
}
static void e2_tower(u8 *b, int k, int tt){
  sky(b, 0, 150, 1); sun(b, 50, 110, 20); hills(b, 140, 0); gfx_rect(b, 0, 150, 256, 42, C_DARK);
  int lean = k == 0 ? 0 : k == 1 ? 2 + tt / 12 : 14 + tt * 2, fall = k < 2 ? 0 : (tt > 60 ? 64 : tt * 64 / 60);
  tower(b, 170, 150, lean > 90 ? 90 : lean, fall);
  if(k == 2){ for(int i = 0; i < 12; i++){ int ph = (tt * 3 + i * 17) % 100; gfx_rect(b, 170 + (int)(hs(i) % 120) - 60 + ph / 2 * ((i & 1) ? 1 : -1), 90 + ph - (int)(hs(i + 5) % 60), 4, 3, C_DIRT); } smoke(b, 170, 150, tt, 5); }
  spr_man(b, 56, 118, 5, 1, k == 1 ? POSE_IDLE : POSE_FLEX, tt >> 3);
  bubble(b, 86, 18, k == 0 ? "..." : k == 1 ? "TIMBER?" : "...TIMBER.", 0);
}
static void e2_boss(u8 *b, int k, int tt){
  sky(b, 0, 150, 1); hills(b, 140, 0); ground(b, 150); smoke(b, 60, 150, tt, 4); smoke(b, 200, 150, tt + 30, 4); rubble(b, 40, 172, 3, 21); rubble(b, 210, 176, 3, 5);
  if(k == 0){ landlord(b, 130, 150, 9, tt, 0); gfx_rect(b, 168, 108, 16, 14, C_WHITE); gfx_rect(b, 184, 112, 5, 8, C_WHITE); gfx_rect(b, 170, 112, 12, 2, C_GOLD); bubble(b, 90, 14, "MY... PAPERWORK.", 0); }
  else { landlord(b, 128, 150, 14, tt, 1); if(tt > 40) bubble(b, 80, 10, "IS GERALD STILL OUT THERE?", 0); if(tt > 100) bubble(b, 180, 40, "I CAN HEAR THE BELLY.", 0); }
}
static void e2_dusk(u8 *b, int k, int tt){
  static const u8 col[6] = { C_SKY0 + 4, C_RED, C_GRASS2, C_DIRT, C_PINK, C_SKIN2 };
  stars_night(b); int mr = k < 2 ? 22 : 22 + (tt > 90 ? 90 : tt) / 3; moon(b, k < 2 ? 210 : 150, k < 2 ? 70 : 76, mr); night_hills(b, 150);
  gfx_rect(b, 0, 164, 256, 28, C_UIBG); confetti(b, tt, 16);
  for(int i = 0; i < 6; i++) folk(b, 16 + i * 44, 180 + (i & 1) * 6, tt + i * 9, col[i], i % 3 == 0);
  if(k == 0){ pizza(b, 150, 176, 3); spr_man(b, 100, 140, 6, 1, POSE_IDLE, tt >> 3); bubble(b, 150, 16, "BEST PIZZA OF MY LIFE.", 0); }
  else if(k==1){
    spr_man(b,110+wv(tt,30,3),142-iabs(wv(tt,26,3)),7,1,(tt/18)&1?POSE_FLEX:POSE_IDLE,tt>>3);
    gag_rear(b,211,119,1,tt+13,0);
    bubble(b,160,16,"EVICTION COMPLETE.",0);
    gfx_text_c(b,210,159,"NO CAMERAS!",C_GOLD,C_BLACK,1);
  }
  else { spr_man(b, 90, 146, 7, 1, POSE_SAD, tt >> 3); bubble(b, 90, 12, "...HUH. WEIRD MOON.", 0); }
}

/* transitions (the first 30 frames of a scene) */
/* The console cut holds its composition: a short palette fade is cheaper and
   more legible than a rapidly expanding iris or a full-width wipe. */
static void trans(u8 *b){
  int type=TRANS(sc);
  if(type==4||t>=12)return;
  u8 ink=type==3?C_WHITE:C_BLACK;
  if(t<4)gfx_rect(b,0,0,256,192,ink);
  else if(t<9)gfx_dither(b,0,0,256,192,ink);
}
/* Only a local exposure rim after a strong shot; random glints in interior
   scenes previously looked like unsupported floating objects. */
static void film_fx(u8 *b,int k,int tt){
  if(k>0&&tt<8&&(SHAKE(sc,k>2?2:k)>=5)){
    int w=(8-tt)*2;
    gfx_rect(b,0,15,w,120,C_GOLD2);
    gfx_rect(b,256-w,15,w,120,C_GOLD2);
  }
}
static void shake(u8 *b, int amp){
  if(amp <= 0) return;
  int dx = (int)(hs(t * 3 + 1) % (u32)(2 * amp + 1)) - amp, dy = (int)(hs(t * 5 + 2) % (u32)(2 * amp + 1)) - amp;
  if(dy > 0){ memmove(b + dy * 256, b, (size_t)(192 - dy) * 256); memset(b, C_BLACK, (size_t)dy * 256); }
  else if(dy < 0){ memmove(b, b - dy * 256, (size_t)(192 + dy) * 256); memset(b + (192 + dy) * 256, C_BLACK, (size_t)(-dy) * 256); }
  for(int y = 0; y < 192 && dx; y++){ u8 *r = b + y * 256; if(dx > 0){ memmove(r + dx, r, (size_t)(256 - dx)); memset(r, C_BLACK, (size_t)dx); } else { memmove(r, r - dx, (size_t)(256 + dx)); memset(r + 256 + dx, C_BLACK, (size_t)(-dx)); } }
}


/* Per-shot art direction: movement belongs to props and layers, never a
   full-frame software zoom. All motifs are 16x16 ROM sprites. */
static const char *const CUT_LABEL[4][8] = {
  { "THE PERFECT LIFE", "THE PAPERS", "LEGAL ADVICE", "DAY NINE",
    "THE RENT", "THE SNAP", "THE REBIRTH", "THE MISSION" },
  { "A STRANGE MOON", "MOON CHEESE", "SOMETHING WRONG", "THE CHANGE",
    "FREE AT LAST", "AWOOOO", "BRENDA RETURNS", "THE WOLFMAN" },
  { "THE MANOR FALLS", "MAPLE STREET", "THE LETTER", "CHAPTER ONE" },
  { "HEAD OFFICE", "THE BOSS", "THE AFTER PARTY", "CHAPTER TWO" }
};
static const unsigned char CUT_SYMBOL[4][8] = {
  { MX_CUT_HOME, MX_CUT_PAPERS, MX_CUT_CASH, MX_CUT_COUCH,
    MX_CUT_KEY, MX_CUT_BOLT, MX_CUT_CAPE, MX_CUT_TARGET },
  { MX_CUT_MOON, MX_CUT_CHEESE, MX_CUT_BOLT, MX_CUT_WOLF,
    MX_CUT_CAPE, MX_CUT_MOON, MX_CUT_HEART, MX_CUT_WOLF },
  { MX_CUT_RUBBLE, MX_CUT_HEART, MX_CUT_PAPERS, MX_CUT_STAR },
  { MX_CUT_TOWER, MX_CUT_CASH, MX_CUT_MOON, MX_CUT_STAR }
};
static void cut_icon(u8 *b,int x,int y,int which,u8 ink){
  if(which<0||which>=MX_CUT_ICON_COUNT) return;
  for(int r=0;r<16;r++){
    unsigned short bits=mx_cut_icons[which][r];
    for(int c=0;c<16;c++) if(bits & (0x8000u>>c)) gfx_px(b,x+c,y+r,ink);
  }
}
/* Contextual effects only: leaves outdoors, impact sparks, snowfall/moon
   motes at night and confetti at victories. No omnipresent floating noise. */
static void cut_motion(u8 *b,int k,int tt){
  int mood=-1;
  if(mode==0){
    if(sc==0&&k==0)mood=0;                 /* yard leaves */
    else if(sc==5||sc==6&&k>=1)mood=2;    /* storm / transformation sparks */
  }else if(mode==1){
    if(sc==0||sc==5)mood=4;               /* starlight */
    else if(sc==3)mood=2;                /* transformation */
  }else if(mode==2){
    if(sc==0)mood=2;else if(sc==1)mood=3;/* rubble and parade */
  }else if(sc==0||sc==2)mood=2;
  if(mood<0)return;
  for(int i=0;i<7;i++){
    u32 seed=hs((u32)(sc*401+mode*187+i*53+k*7));
    int x=(int)(seed&255u),y=(int)((seed>>10)%160u)+14;
    if(mood==0){
      x=(x+tt/(8+(i&3)))&255;y=140+(int)((seed>>18)%28u);
      gfx_rect(b,x,y,2,1,(i&1)?C_GRASS2:C_GOLD2);
    }else if(mood==2){
      int age=(tt+i*13)%55,px=128+(int)(seed%60u)-30+(i&1?age:-age)/3;
      int py=105+(int)((seed>>12)%30u)-age/2;
      if(age<44)gfx_rect(b,px,py,age<9?2:1,1,(i&1)?C_WHITE:C_GOLD2);
    }else if(mood==3){
      x=(x+wv(tt+i*19,42,5))&255;y=(y+tt*(1+(i&1)))%180;
      gfx_rect(b,x,y,2,2,(i&1)?C_PINK:C_GOLD);
    }else{
      int phase=(tt+i*19)%100;
      gfx_px(b,x+wv(tt+i*9,90,3),y,phase<14?C_WHITE:C_GOLD2);
    }
  }
}
static void cut_title_bug(u8 *b){
  /* Static shot label, away from actors and beneath the matte.
     No sliding panels, oversized type or decorative swoosh. */
  if(t>=76)return;
  gfx_rect(b,8,16,120,15,C_BLACK);
  gfx_rect(b,8,16,3,15,mode==1?C_PINK:C_GOLD);
  gfx_text(b,15,20,CUT_LABEL[mode][sc],C_WHITE,1);
  gfx_text(b,114,20,gfx_int(sc+1),C_GOLD2,1);
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
int intro_text_rows(int s){  const char *r[MAX_ROWS + 4]; int l[MAX_ROWS + 4]; int n = 0; for(int i = 0; i < 3; i++) n = wrap(TXT(s)[i], r, l, n); return n; }

static void narration(u8 *b){
  gfx_rect(b,0,0,256,192,C_UIBG);
  /* Inked screenplay card: polished header, tracked scene number, emblem. */
  gfx_rect(b,0,0,256,19,C_DARK);
  gfx_rect(b,0,19,256,2,mode==1?C_PINK:C_GOLD2);
  gfx_text(b,8,4,mode==0?"MANDEX: ORIGIN":mode==1?"THE WOLFMAN":mode==2?"MANDEX: ACT I":"MANDEX: ACT II",C_GOLD,1);
  gfx_text(b,8,11,CUT_LABEL[mode][sc],C_WHITE,1);
  cut_icon(b,230,2,CUT_SYMBOL[mode][sc],mode==1?C_PINK:C_GOLD);
  gfx_rect(b,8,22,240,1,C_UILINE);
  for(int i=0;i<NSC();i++){
    int w=NSC()>4?25:52;
    gfx_rect(b,8+i*(w+4),158,w,3,i==sc?C_GOLD:i<sc?C_UIHI:C_DARK);
  }
  gfx_rect(b,3,23,2,132,C_UILINE);
  gfx_rect(b,251,23,2,132,C_UILINE);
  const char *rows[MAX_ROWS + 4]; int len[MAX_ROWS + 4]; int line_of[MAX_ROWS + 4]; int n = 0;
  for(int i = 0; i < 3; i++){ int n1 = wrap(TXT(sc)[i], rows, len, n); for(int j = n; j < n1; j++) line_of[j] = i; n = n1; }
  int shown = t * CPS / 60; if(shown > total_chars(sc)) shown = total_chars(sc);
  int y = 27 + (n < 8 ? (8 - n) * 2 : 0);
  for(int j = 0; j < n; j++){
    int cnt = shown < len[j] ? shown : len[j]; shown -= cnt;
    if(cnt > 0){ char tmp[ROW_CH + 1]; memcpy(tmp, rows[j], (size_t)cnt); tmp[cnt] = 0;
      int hl = line_of[j] == HL(sc); gfx_text(b, 8, y, tmp, hl ? C_GOLD : C_WHITE, 2); }
    y += (j + 1 < n && line_of[j + 1] != line_of[j]) ? 17 : 12;
    if(cnt < len[j]) break;
  }
  gfx_rect(b, 184, 172, 66, 16, C_UIHI); gfx_rect(b, 184, 172, 66, 1, C_UILINE); gfx_rect(b, 184, 187, 66, 1, C_UILINE); gfx_rect(b, 184, 172, 1, 16, C_UILINE); gfx_rect(b, 249, 172, 1, 16, C_UILINE);
  gfx_text_c(b, 217, 177, "SKIP >>", C_WHITE, C_UIHI, 1);
  /* Animated chapter/story progress rail and a pulsing dialogue pointer. */
  { int end = type_end(sc); int progress = end > 0 ? (t < end ? t * 240 / end : 240) : 240;
    gfx_rect(b, 8, 165, 240, 2, C_DARK); gfx_rect(b, 8, 165, progress, 2, C_GOLD);
    if(t < end) gfx_rect(b, 10 + (t * 5 % 225), 160, 2, 3, C_GOLD2); }
  if(t >= type_end(sc) && (t & 32)) gfx_text(b, 8, 177, "A / TOUCH: NEXT", C_UILINE, 1);
}

void intro_draw(u8 *top, u8 *bot){
  int k = shot_of(), tt = shot_t(k); g_t = tt;
  if(mode == 2){ switch(sc){ case 0: e1_rubble(top, k, tt); break; case 1: e1_street(top, k, tt); break; case 2: e1_letter(top, k, tt); break; default: e_title(top, k, tt, 1); break; } }
  else if(mode == 3){ switch(sc){ case 0: e2_tower(top, k, tt); break; case 1: e2_boss(top, k, tt); break; case 2: e2_dusk(top, k, tt); break; default: e_title(top, k, tt, 2); break; } }
  else if(wolf){
    switch(sc){ case 0: w_walk(top, k, tt); break; case 1: w_fridge(top, k, tt); break; case 2: w_itch(top, k, tt); break; case 3: w_ears(top, k, tt); break;
                case 4: w_tail(top, k, tt); break; case 5: w_howl(top, k, tt); break; case 6: w_visit(top, k, tt); break; default: w_title(top, k, tt); break; }
  } else
  switch(sc){
  case 0: sc_suburb(top, k, tt); break;   case 1: sc_divorce(top, k, tt); break;  case 2: sc_dennis(top, k, tt); break;  case 3: sc_couch(top, k, tt); break;
  case 4: sc_landlord(top, k, tt); break; case 5: sc_snap(top, k, tt); break;     case 6: sc_born(top, k, tt); break;    default: sc_mission(top, k, tt); break;
  }
  cut_motion(top,k,tt);
  trans(top);
  /* Impact shake now falls off on each shot rather than jittering through every line of dialogue. */
  { int amp = SHAKE(sc, k > 2 ? 2 : k); if(tt >= 24) amp = 0; else amp = amp * (24 - tt) / 24; shake(top, amp); }
  film_fx(top, k, tt);
  cut_title_bug(top);
  /* letterbox bars sliding in, like the browser version */
  int bar = t * 3 > 12 ? 12 : t * 3; gfx_rect(top, 0, 0, 256, bar, C_BLACK); gfx_rect(top, 0, 192 - bar, 256, bar, C_BLACK);
  narration(bot);
}
