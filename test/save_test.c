/* Save blob (port 5, core part): round trip, the documented byte layout, every single-byte flip and every truncation rejected with the stats untouched,
   version / magic / erased-chip / padding cases, clamping, saves from builds with fewer or more levels, and the best-score update in mx_meta_win.
   Built twice by run_tests.sh (float and fixed): the blob must not depend on the math mode. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
#include "mx_meta.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static MxWorld W;

static void fill(MxStats *s){
  mx_meta_init(s);
  s->kills = 101; s->cells = 2222; s->booms = 33; s->maxChain = 6; s->shots = 144; s->flops = 5; s->wins = 17; s->oneShots = 2; s->bestScore = 61234; s->inflated = 8; s->pops = 3;
  for(int i = 0; i < 4; i++) s->pw[i] = 10 + i;
  for(int i = 0; i < MX_NUM_LEVELS; i++){ s->stars[i] = (uint8_t)(i % 4); s->best[i] = (uint32_t)(i * 1000 + 7); }
  s->ach = 0x1A5A5A5u & ((1u << MX_NUM_ACH) - 1u);
  s->unlock_all = 1; s->lv_booms = 7; s->lv_shots = 8; s->seen_st_inflated = 9; s->seen_st_pops = 10; for(int i = 0; i < 4; i++) s->seen_pw[i] = 11 + i;
}
static int same_saved(const MxStats *a, const MxStats *b){
  if(a->kills != b->kills || a->cells != b->cells || a->booms != b->booms || a->maxChain != b->maxChain || a->shots != b->shots || a->flops != b->flops) return 0;
  if(a->wins != b->wins || a->oneShots != b->oneShots || a->bestScore != b->bestScore || a->inflated != b->inflated || a->pops != b->pops || a->ach != b->ach) return 0;
  for(int i = 0; i < 4; i++) if(a->pw[i] != b->pw[i]) return 0;
  for(int i = 0; i < MX_NUM_LEVELS; i++) if(a->stars[i] != b->stars[i] || a->best[i] != b->best[i]) return 0;
  return 1;
}
static void w32(uint8_t *p, uint32_t v){ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void reseal(uint8_t *b, int n){ w32(b + n - 4, mx_crc32(b, n - 4)); }
/* an independent writer of the documented layout, for a build that had nlev levels: stars[i] = 1 + i % 3, best[i] = 500 + i */
static int craft(uint8_t *o, int nlev){
  int n = 0; memcpy(o, "MXS1", 4); o[4] = 1; o[5] = (uint8_t)nlev; n = 6;
  for(int i = 0; i < 11; i++){ w32(o + n, 1000u + i); n += 4; }
  for(int i = 0; i < 4; i++){ w32(o + n, 20u + i); n += 4; }
  for(int i = 0; i < nlev; i++) o[n++] = (uint8_t)(1 + i % 3);
  w32(o + n, 0x15u); n += 4; o[n++] = 2;
  for(int i = 0; i < nlev; i++){ w32(o + n, 500u + i); n += 4; }
  w32(o + n, mx_crc32(o, n)); return n + 4;
}

int main(void){
  static const uint8_t v123[9] = { '1','2','3','4','5','6','7','8','9' };
  CHECK(mx_crc32(v123, 9) == 0xCBF43926u, "CRC32 matches the standard check value");

  MxStats S, T; uint8_t buf[MX_SAVE_MAX + 64]; int of = -1;
  fill(&S);
  CHECK(mx_meta_pack(&S, 5, buf, MX_SAVE_MAX - 1) == 0, "pack into a too-small buffer returns 0");
  int n = mx_meta_pack(&S, 5, buf, (int)sizeof buf);
  CHECK(n == MX_SAVE_MAX && n == 176, "20 levels pack to 176 bytes (version 2: one options byte)");

  /* the documented layout, byte by byte */
  CHECK(memcmp(buf, "MXS1", 4) == 0 && buf[4] == 2 && buf[5] == MX_NUM_LEVELS, "header: magic, version 2, level count");
  CHECK(buf[6] == (101 & 255) && buf[7] == 0 && buf[8] == 0 && buf[9] == 0, "first counter (kills) is little-endian at offset 6");
  CHECK(buf[6 + 4 * 8] == (61234 & 255) && buf[7 + 4 * 8] == ((61234 >> 8) & 255) && buf[8 + 4 * 8] == 0, "bestScore is the 9th counter");
  CHECK(buf[50] == 10 && buf[66] == S.stars[0] && buf[66 + 3] == S.stars[3], "pw[] at 50, stars[] at 66");
  CHECK(buf[66 + 20 + 4] == 5, "outfit index byte follows stars and ach");
  CHECK(buf[n - 5] == (0 | (MX_TOOL_VALVE << 1)), "options byte (default: collapse off, air valve) sits just before the CRC");
  { uint32_t crc = mx_crc32(buf, n - 4); CHECK(buf[n - 4] == (uint8_t)crc && buf[n - 1] == (uint8_t)(crc >> 24), "CRC is the last four bytes, little-endian"); }

  /* round trip */
  mx_meta_init(&T); of = -1;
  CHECK(mx_meta_unpack(&T, &of, buf, n) == 1, "unpack accepts what pack wrote");
  CHECK(same_saved(&S, &T) && of == 5, "round trip equals the original, outfit included");
  CHECK(T.unlock_all == 0, "unlock_all is not saved");
  CHECK(T.lv_booms == 0 && T.lv_shots == 0 && T.seen_st_inflated == 0 && T.seen_st_pops == 0 && T.seen_pw[0] == 0 && T.seen_pw[3] == 0, "transients come back cleared");
  { uint8_t b2[MX_SAVE_MAX + 64]; int n2 = mx_meta_pack(&T, of, b2, (int)sizeof b2); CHECK(n2 == n && memcmp(buf, b2, n) == 0, "pack(unpack(pack(x))) is byte-identical"); }
  { MxStats U = S; U.unlock_all = 1; int o2 = 0; mx_meta_unpack(&U, &o2, buf, n); CHECK(U.unlock_all == 1, "unpack leaves a session unlock_all cheat alone"); }

  /* a chip read is longer than the blob: padding is ignored */
  { uint8_t big[256]; memset(big, 0xFF, sizeof big); memcpy(big, buf, n); mx_meta_init(&T);
    CHECK(mx_meta_unpack(&T, &of, big, (int)sizeof big) == 1 && same_saved(&S, &T), "trailing 0xFF padding after the blob is ignored"); }

  /* failures leave everything untouched */
  { MxStats sentinel; fill(&sentinel); sentinel.kills = 4242; int osent = 9, bad = 0, badlen = 0, flips = 0;
    for(int i = 0; i < n; i++) for(int bit = 0; bit < 8; bit += 7){            /* low and high bit of every byte */
      uint8_t b2[MX_SAVE_MAX]; memcpy(b2, buf, n); b2[i] ^= (uint8_t)(1 << bit); MxStats X = sentinel; int ox = osent; flips++;
      if(mx_meta_unpack(&X, &ox, b2, n) != 0 || memcmp(&X, &sentinel, sizeof X) != 0 || ox != osent) bad++;
    }
    CHECK(bad == 0, "flipping any single bit-0 / bit-7 of any byte fails and leaves stats and outfit untouched");
    printf("    (%d flips tried)\n", flips);
    for(int l = 0; l < n; l++){ MxStats X = sentinel; int ox = osent; if(mx_meta_unpack(&X, &ox, buf, l) != 0 || memcmp(&X, &sentinel, sizeof X) != 0 || ox != osent) badlen++; }
    CHECK(badlen == 0, "every truncation (0 .. n-1 bytes) fails and leaves stats untouched");
    { uint8_t b2[MX_SAVE_MAX]; memcpy(b2, buf, n); b2[4] = 3; reseal(b2, n); MxStats X = sentinel; int ox = osent;
      CHECK(mx_meta_unpack(&X, &ox, b2, n) == 0 && memcmp(&X, &sentinel, sizeof X) == 0, "version 3 (CRC valid) is refused"); }
    { uint8_t b2[MX_SAVE_MAX]; memcpy(b2, buf, n); b2[4] = 0; reseal(b2, n); MxStats X = sentinel; int ox = osent;
      CHECK(mx_meta_unpack(&X, &ox, b2, n) == 0 && memcmp(&X, &sentinel, sizeof X) == 0, "version 0 (CRC valid) is refused"); }
    { uint8_t b2[MX_SAVE_MAX]; memcpy(b2, buf, n); b2[3] = '2'; reseal(b2, n); MxStats X = sentinel; int ox = osent;
      CHECK(mx_meta_unpack(&X, &ox, b2, n) == 0 && memcmp(&X, &sentinel, sizeof X) == 0, "wrong magic (CRC valid) is refused"); }
    { uint8_t erased[256]; memset(erased, 0xFF, sizeof erased); MxStats X = sentinel; int ox = osent;
      CHECK(mx_meta_unpack(&X, &ox, erased, (int)sizeof erased) == 0 && memcmp(&X, &sentinel, sizeof X) == 0, "an erased chip (all 0xFF) means fresh start"); }
    { uint8_t zero[256]; memset(zero, 0, sizeof zero); MxStats X = sentinel; int ox = osent;
      CHECK(mx_meta_unpack(&X, &ox, zero, (int)sizeof zero) == 0, "an all-zero buffer means fresh start"); }
    { MxStats X = sentinel; int ox = osent; CHECK(mx_meta_unpack(&X, &ox, buf, 0) == 0 && mx_meta_unpack(&X, &ox, NULL, 0) == 0, "an empty file / null buffer means fresh start"); }
  }

  /* values that pass the CRC but make no sense are clamped, not trusted */
  { uint8_t b2[MX_SAVE_MAX]; memcpy(b2, buf, n); b2[66 + 20 + 4] = 200; b2[66] = 9; w32(b2 + 66 + 20, 0xFFFFFFFFu); w32(b2 + 6, 0xFFFFFFF0u); reseal(b2, n);
    mx_meta_init(&T); int ok = mx_meta_unpack(&T, &of, b2, n);
    CHECK(ok && of == 0, "an outfit index this build does not have becomes 0");
    CHECK(T.stars[0] == 3, "stars above 3 clamp to 3");
    CHECK(T.ach == ((1u << MX_NUM_ACH) - 1u), "achievement bits that do not exist are dropped");
    CHECK(T.kills == 0x7FFFFFFF, "a counter above INT_MAX clamps to INT_MAX"); }
  { MxStats X; fill(&X); X.kills = -5; X.pw[1] = -1; uint8_t b2[MX_SAVE_MAX]; mx_meta_pack(&X, -3, b2, (int)sizeof b2); mx_meta_init(&T);
    CHECK(mx_meta_unpack(&T, &of, b2, MX_SAVE_MAX) == 1 && T.kills == 0 && T.pw[1] == 0 && of == 0, "negative counters and a negative outfit pack as 0"); }

  /* saves written by a build with a different number of levels */
  { uint8_t c[400]; int cn = craft(c, MX_NUM_LEVELS); mx_meta_init(&T);
    CHECK(cn == MX_SAVE_MAX - 1 && mx_meta_unpack(&T, &of, c, cn) == 1, "the independent writer's 20-level version 1 blob is accepted (layout agrees with the documentation)");
    CHECK(T.kills == 1000 && T.pops == 1010 && T.pw[3] == 23 && T.ach == 0x15 && of == 2 && T.stars[0] == 1 && T.stars[1] == 2 && T.stars[2] == 3 && T.best[19] == 519, "...and every field lands where the layout says"); }
  { uint8_t c[400]; int cn = craft(c, 5); mx_meta_init(&T); for(int i = 0; i < MX_NUM_LEVELS; i++){ T.stars[i] = 3; T.best[i] = 99; }
    CHECK(mx_meta_unpack(&T, &of, c, cn) == 1 && T.stars[4] == 2 && T.best[4] == 504 && T.stars[5] == 0 && T.best[5] == 0 && T.best[19] == 0, "a save from a 5-level build loads: the new levels read as not played"); }
  { uint8_t c[600]; int cn = craft(c, MX_NUM_LEVELS + 5); mx_meta_init(&T);
    CHECK(cn <= (int)sizeof c && mx_meta_unpack(&T, &of, c, cn) == 1 && T.best[MX_NUM_LEVELS - 1] == 500u + MX_NUM_LEVELS - 1 && of == 2, "a save from a 25-level build loads: the extra levels are dropped"); }

  /* best score now lives in the core */
  { MxStats B; mx_meta_init(&B); memset(&W, 0, sizeof W); W.level = 3; W.men = 2; W.score = 500; mx_meta_win(&B, &W);
    CHECK(B.best[3] == 500 && B.best[2] == 0 && B.best[4] == 0, "a win records the score as that level's best");
    W.score = 300; mx_meta_win(&B, &W); CHECK(B.best[3] == 500, "a lower score does not replace it");
    W.score = 900; mx_meta_win(&B, &W); CHECK(B.best[3] == 900 && B.bestScore == 900, "a higher one does");
    W.level = MX_NUM_LEVELS; W.score = 5; mx_meta_win(&B, &W); CHECK(B.best[3] == 900, "an out-of-range level is ignored, not written"); }

  /* ---- options (port 13): saved in version 2, version 1 blobs still load with the defaults ---- */
  { MxOptions d; mx_options_default(&d); CHECK(d.collapse == 0 && d.tool == MX_TOOL_VALVE, "default options: collapse off, air valve (the browser's defaults)"); }
  { MxStats A; fill(&A); MxOptions o = { 1, MX_TOOL_SPLAT }, r = { 0, 0 }; uint8_t b[MX_SAVE_MAX]; int of2 = -1; MxStats B; mx_meta_init(&B);
    int n1 = mx_meta_pack_opt(&A, 7, &o, b, (int)sizeof b);
    CHECK(n1 == MX_SAVE_MAX && b[n1 - 5] == (1 | (MX_TOOL_SPLAT << 1)), "collapse on + splat packs into the options byte");
    CHECK(mx_meta_unpack_opt(&B, &of2, &r, b, n1) == 1 && r.collapse == 1 && r.tool == MX_TOOL_SPLAT && of2 == 7 && same_saved(&A, &B), "options round trip, stats and outfit unharmed");
    { uint8_t b2[MX_SAVE_MAX]; int n2 = mx_meta_pack_opt(&B, of2, &r, b2, (int)sizeof b2); CHECK(n2 == n1 && memcmp(b, b2, n1) == 0, "repack is byte-identical with options too"); }
    for(int t = 0; t < MX_NUM_TOOLS; t++) for(int c = 0; c < 2; c++){ MxOptions i2 = { (uint8_t)c, (uint8_t)t }, o2 = { 9, 9 }; uint8_t bb[MX_SAVE_MAX]; int nn = mx_meta_pack_opt(&A, 0, &i2, bb, (int)sizeof bb);
      if(!mx_meta_unpack_opt(&B, &of2, &o2, bb, nn) || o2.collapse != c || o2.tool != t){ CHECK(0, "every collapse x tool combination round trips"); t = 99; break; } }
    CHECK(mx_tool_power(MX_TOOL_NONE) == PW_NONE && mx_tool_power(MX_TOOL_VALVE) == PW_NONE && mx_tool_power(MX_TOOL_BEANS) == PW_BEANS && mx_tool_power(MX_TOOL_TOWEL) == PW_TOWEL && mx_tool_power(MX_TOOL_SPLAT) == PW_SPLAT, "start tools map to the right power-ups (valve / none arm nothing)");
    CHECK(strcmp(mx_tool_name(MX_TOOL_VALVE), "AIR VALVE") == 0 && strcmp(mx_tool_name(MX_TOOL_NONE), "NONE") == 0 && strcmp(mx_tool_name(99), "?") == 0, "tool names");
    { MxOptions bad = { 5, 200 }; uint8_t bb[MX_SAVE_MAX]; MxOptions r2 = { 0, 0 }; int nn = mx_meta_pack_opt(&A, 0, &bad, bb, (int)sizeof bb);
      CHECK(mx_meta_unpack_opt(&B, &of2, &r2, bb, nn) == 1 && r2.collapse == 1 && r2.tool == MX_TOOL_VALVE, "an out-of-range tool packs as the air valve, any non-zero collapse as on"); }
    { uint8_t bb[MX_SAVE_MAX]; memcpy(bb, b, n1); bb[n1 - 5] = 0xFF; reseal(bb, n1); MxOptions r2 = { 0, 0 };
      CHECK(mx_meta_unpack_opt(&B, &of2, &r2, bb, n1) == 1 && r2.collapse == 1 && r2.tool == MX_TOOL_VALVE, "an options byte with a tool index this build lacks (and reserved bits set) loads as air valve, never trusted"); }
    { MxOptions sent = { 1, MX_TOOL_BEANS }; uint8_t bb[MX_SAVE_MAX]; memcpy(bb, b, n1); bb[0] ^= 1; MxStats X; fill(&X);
      CHECK(mx_meta_unpack_opt(&X, &of2, &sent, bb, n1) == 0 && sent.collapse == 1 && sent.tool == MX_TOOL_BEANS, "a failed unpack leaves the options untouched"); }
    CHECK(mx_meta_unpack_opt(&B, &of2, 0, b, n1) == 1, "options pointer may be NULL"); }
  { uint8_t c[MX_SAVE_MAX + 8]; int cn = craft(c, MX_NUM_LEVELS); MxOptions r = { 1, MX_TOOL_SPLAT }; MxStats B; mx_meta_init(&B); int of2 = -1;
    CHECK(cn == 175 && c[4] == 1, "(the independent writer still produces a 175-byte version 1 blob)");
    CHECK(mx_meta_unpack_opt(&B, &of2, &r, c, cn) == 1 && r.collapse == 0 && r.tool == MX_TOOL_VALVE && B.best[4] == 504 && of2 == 2, "a version 1 save still loads: progress kept, options = defaults");
    CHECK(mx_meta_unpack(&B, &of2, c, cn) == 1, "...also through the old entry point");
    CHECK(mx_meta_unpack_opt(&B, &of2, &r, c, cn - 1) == 0, "a version 1 blob one byte short is refused (the size check follows the version)"); }

  printf(fails ? "save_test: %d FAILED\n" : "save_test: all passed\n", fails);
  return fails != 0;
}
