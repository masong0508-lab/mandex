#include <string.h>
#include "mx_meta.h"

static int story_done(const MxStats *s){ int n = 0; for(int i = 0; i < MX_STORY_LEVELS; i++) n += s->stars[i] > 0; return n; }
static int three_stars(const MxStats *s){ int n = 0; for(int i = 0; i < MX_STORY_LEVELS; i++) n += s->stars[i] >= 3; return n; }
static int total_stars(const MxStats *s){ int n = 0; for(int i = 0; i < MX_NUM_LEVELS; i++) n += s->stars[i]; return n; }
int mx_valve_unlocked(const MxStats *s){ return story_done(s) >= MX_VALVE_UNLOCK_LEVELS; }
int mx_ach_count(const MxStats *s){ int n = 0; for(int i = 0; i < MX_NUM_ACH; i++) n += (s->ach >> i) & 1; return n; }

typedef struct { const char *n, *d; } AchDef;
static const AchDef ACH[MX_NUM_ACH] = {
  {"First Eviction", "Evict your first landlord"}, {"Landlord Slayer", "Evict 25 landlords"}, {"Landlord Menace", "Evict 100 landlords"},
  {"Demolition Crew", "Smash 2,000 blocks"}, {"Wrecking Ball", "Smash 20,000 blocks"}, {"Boom Shaka", "Blow up 10 TNT blocks"},
  {"Chain Reaction", "5 TNT in one level"}, {"Belly Flopper", "Evict mid belly-flop"}, {"Flop Star", "Belly-flop 10 landlords"},
  {"Sniper", "Win with one man only"}, {"Sniper Elite", "Win 5 levels with one man"}, {"Star Pupil", "Three stars on a level"},
  {"Star Collector", "Three stars on 8 levels"}, {"Constellation", "Three stars everywhere"}, {"Getting Started", "Beat 5 levels"},
  {"Halfway Homeless", "Beat 10 levels"}, {"Evicted Everyone", "Beat every level"}, {"High Roller", "Score 60,000 in a level"},
  {"Persistent", "Launch 100 men"}, {"Fashionista", "Unlock 3 outfits"}, {"Walk-in Closet", "Unlock every outfit"},
  {"Hot Air", "Launch over 60% inflated"}, {"Pop Goes the Dad", "Explode on the air valve"}, {"Gadget Guy", "Use beans, towel and splat"},
  {"Trophy Hunter", "Unlock 10 achievements"}};
const char *mx_ach_name(int i){ return ACH[(unsigned)i % MX_NUM_ACH].n; }
const char *mx_ach_desc(int i){ return ACH[(unsigned)i % MX_NUM_ACH].d; }

static const MxOutfit OUT[MX_NUM_OUTFITS] = {
  {"Classic Red",    "always yours",           {224,36,58},   {168,22,42},   {255,126,176}, {255,79,143},  0},
  {"Navy Stripes",   "earn 3 stars in total",  {47,79,168},   {31,52,122},   {242,242,242}, {201,201,214}, 0},
  {"Banana Boxers",  "evict 25 landlords",     {245,208,47},  {201,165,23},  {124,211,106}, {79,174,66},   0},
  {"Mint Condition", "pop on the air valve",   {79,207,166},  {47,155,120},  {255,232,168}, {242,201,106}, 0},
  {"Camo Dad",       "smash 2,000 blocks",     {95,122,58},   {63,84,36},    {138,111,67},  {95,74,42},    0},
  {"Midnight",       "earn 20 stars in total", {43,43,58},    {20,20,31},    {122,92,214},  {90,63,176},   0},
  {"Ghost Boxers",   "win with one man",       {244,244,255}, {185,189,214}, {199,215,255}, {143,165,230}, 0},
  {"Hot Pink",       "beat 10 levels",         {255,79,163},  {201,42,120},  {90,214,255},  {43,176,224},  0},
  {"Golden Boy",     "three stars on 8 levels",{240,185,42},  {184,133,26},  {255,240,168}, {230,195,74},  0},
  {"Birthday Suit",  "unlock 20 achievements", {244,183,142}, {226,160,119}, {255,255,255}, {216,216,224}, 0},
  {"Rainbow Rump",   "beat every level",       {224,36,58},   {168,22,42},   {255,126,176}, {255,79,143},  1, 0},
  {"Working Man",    "always yours",           {63,95,143},   {43,68,104},   {201,206,214}, {143,152,166}, 0, 1}};
const MxOutfit *mx_outfit(int i){ return &OUT[(unsigned)i % MX_NUM_OUTFITS]; }
int mx_outfit_unlocked(const MxStats *s, int i){
  switch(i){
  case 0: return 1;
  case 1: return total_stars(s) >= 3;
  case 2: return s->kills >= 25;
  case 3: return s->pops >= 1;
  case 4: return s->cells >= 2000;
  case 5: return total_stars(s) >= 20;
  case 6: return s->oneShots >= 1;
  case 7: return story_done(s) >= 10;
  case 8: return three_stars(s) >= 8;
  case 9: return mx_ach_count(s) >= 20;
  case 10: return story_done(s) >= MX_STORY_LEVELS;
  case 11: return 1;                 /* Working Man */
  }
  return 0;
}
static int owned(const MxStats *s){ int n = 0; for(int i = 0; i < MX_NUM_OUTFITS; i++) n += mx_outfit_unlocked(s, i); return n; }

static int met(const MxStats *s, int i){
  switch(i){
  case 0: return s->kills >= 1;      case 1: return s->kills >= 25;     case 2: return s->kills >= 100;
  case 3: return s->cells >= 2000;   case 4: return s->cells >= 20000;  case 5: return s->booms >= 10;
  case 6: return s->maxChain >= 5;   case 7: return s->flops >= 1;      case 8: return s->flops >= 10;
  case 9: return s->oneShots >= 1;   case 10: return s->oneShots >= 5;  case 11: return three_stars(s) >= 1;
  case 12: return three_stars(s) >= 8; case 13: return three_stars(s) >= MX_STORY_LEVELS;
  case 14: return story_done(s) >= 5; case 15: return story_done(s) >= 10; case 16: return story_done(s) >= MX_STORY_LEVELS;
  case 17: return s->bestScore >= 60000; case 18: return s->shots >= 100;
  case 19: return owned(s) >= 3;     case 20: return owned(s) >= MX_NUM_OUTFITS;
  case 21: return s->inflated >= 1;  case 22: return s->pops >= 1;
  case 23: return s->pw[PW_BEANS] >= 1 && s->pw[PW_TOWEL] >= 1 && s->pw[PW_SPLAT] >= 1;
  case 24: return mx_ach_count(s) >= 10;
  }
  return 0;
}
int mx_ach_progress(const MxStats *s, int i, int *cur, int *target){
  int c, t;
  switch(i){
  case 1:  c = s->kills;  t = 25;    break;   case 2:  c = s->kills;  t = 100;   break;
  case 3:  c = s->cells;  t = 2000;  break;   case 4:  c = s->cells;  t = 20000; break;
  case 5:  c = s->booms;  t = 10;    break;   case 6:  c = s->maxChain; t = 5;   break;
  case 8:  c = s->flops;  t = 10;    break;   case 10: c = s->oneShots; t = 5;   break;
  case 12: c = three_stars(s); t = 8; break;  case 13: c = three_stars(s); t = MX_STORY_LEVELS; break;
  case 14: c = story_done(s); t = 5;  break;  case 15: c = story_done(s); t = 10; break;
  case 16: c = story_done(s); t = MX_STORY_LEVELS; break;
  case 17: c = s->bestScore; t = 60000; break; case 18: c = s->shots; t = 100;   break;
  case 19: c = owned(s);  t = 3;     break;   case 20: c = owned(s);  t = MX_NUM_OUTFITS; break;
  case 24: c = mx_ach_count(s); t = 10; break;
  default: return 0;
  }
  *cur = c; *target = t; return 1;
}
int mx_meta_next_unlock(MxStats *s){
  for(int i = 0; i < MX_NUM_ACH; i++) if(!((s->ach >> i) & 1) && met(s, i)){ s->ach |= 1u << i; return i; }
  return -1;
}
void mx_meta_init(MxStats *s){ memset(s, 0, sizeof *s); }
void mx_meta_level_start(MxStats *s, const MxWorld *w){
  s->lv_booms = s->lv_shots = 0;
  s->seen_st_inflated = w->st_inflated; s->seen_st_pops = w->st_pops;
  for(int i = 0; i < 4; i++) s->seen_pw[i] = w->st_pw[i];
}
void mx_meta_events(MxStats *s, const MxWorld *w){
  for(int i = 0; i < w->nev; i++){
    const MxEvent *e = &w->ev[i];
    switch(e->type){
    case EV_CELL: s->cells++; break;
    case EV_BOOM: s->booms++; s->lv_booms++; if(s->lv_booms > s->maxChain) s->maxChain = s->lv_booms; break;
    case EV_LAUNCH: s->shots++; s->lv_shots++; break;
    case EV_TARGET: s->kills++; if(w->man.dive && w->man.mode == MAN_FLY) s->flops++; break;
    }
  }
  s->inflated += w->st_inflated - s->seen_st_inflated; s->seen_st_inflated = w->st_inflated;
  s->pops += w->st_pops - s->seen_st_pops;             s->seen_st_pops = w->st_pops;
  for(int i = 1; i < 4; i++){ s->pw[i] += w->st_pw[i] - s->seen_pw[i]; s->seen_pw[i] = w->st_pw[i]; }
}
int mx_level_unlocked(const MxStats *s, int level){
#ifdef MX_LEVEL_LOCKS
  return s->unlock_all || level < 3 || (level < MX_NUM_LEVELS && s->stars[level - 1] > 0);
#else
  (void)s; (void)level; return 1;
#endif
}
int mx_stars_for(const MxWorld *w){ return w->men >= 2 ? 3 : w->men >= 1 ? 2 : 1; }
void mx_meta_win(MxStats *s, const MxWorld *w){
  int st = mx_stars_for(w);
  if(w->level >= 0 && w->level < MX_NUM_LEVELS && st > s->stars[w->level]) s->stars[w->level] = (uint8_t)st;
  s->wins++; if(s->lv_shots == 1) s->oneShots++;
  if(w->score > s->bestScore) s->bestScore = w->score;
  if(w->level >= 0 && w->level < MX_NUM_LEVELS && w->score > 0 && (uint32_t)w->score > s->best[w->level]) s->best[w->level] = (uint32_t)w->score;
}

static uint8_t lift(int v, int to, int pct){ int r = v + (to - v) * pct / 100; return (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r); }
void mx_outfit_shades(int i, uint8_t out[6][3]){
  const MxOutfit *o = mx_outfit(i);
  for(int k = 0; k < 3; k++){
    out[0][k] = o->br[k]; out[1][k] = o->brd[k]; out[2][k] = lift(o->br[k], 255, 30);
    out[3][k] = o->tow[k]; out[4][k] = o->tows[k]; out[5][k] = lift(o->tows[k], 0, 20);
  }
}

/* ------------------------------------------------------------------ save blob (layout: see mx_meta.h) */
uint32_t mx_crc32(const uint8_t *p, int n){
  uint32_t c = 0xFFFFFFFFu;
  for(int i = 0; i < n; i++){ c ^= p[i]; for(int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  return ~c;
}
static void put32(uint8_t *p, uint32_t v){ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *p){ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
#define SAVE_NCOUNT 11
#define SAVE_HDR 6
#define SAVE_PW (SAVE_HDR + 4 * SAVE_NCOUNT)       /* 50 */
#define SAVE_STARS (SAVE_PW + 16)                  /* 66 */
static int save_size(int nlev, int ver){ return SAVE_STARS + nlev + 4 + 1 + 4 * nlev + (ver >= 2 ? 1 : 0) + 4; }

void mx_options_default(MxOptions *o){ o->collapse = 0; o->tool = MX_TOOL_VALVE; }
const char *mx_tool_name(int t){
  static const char *const N[MX_NUM_TOOLS] = { "NONE", "AIR VALVE", "BEANS", "TOWEL", "SPLAT" };
  return t >= 0 && t < MX_NUM_TOOLS ? N[t] : "?";
}
int mx_tool_power(int t){ return t == MX_TOOL_BEANS ? PW_BEANS : t == MX_TOOL_TOWEL ? PW_TOWEL : t == MX_TOOL_SPLAT ? PW_SPLAT : PW_NONE; }

int mx_meta_pack(const MxStats *s, int outfit, uint8_t *out, int cap){ return mx_meta_pack_opt(s, outfit, 0, out, cap); }
int mx_meta_pack_opt(const MxStats *s, int outfit, const MxOptions *opt, uint8_t *out, int cap){
  const int n = save_size(MX_NUM_LEVELS, MX_SAVE_VERSION);
  if(!s || !out || cap < n) return 0;
  const int *cnt[SAVE_NCOUNT] = { &s->kills, &s->cells, &s->booms, &s->maxChain, &s->shots, &s->flops, &s->wins, &s->oneShots, &s->bestScore, &s->inflated, &s->pops };
  memcpy(out, "MXS1", 4); out[4] = MX_SAVE_VERSION; out[5] = (uint8_t)MX_NUM_LEVELS;
  for(int i = 0; i < SAVE_NCOUNT; i++) put32(out + SAVE_HDR + 4 * i, *cnt[i] < 0 ? 0u : (uint32_t)*cnt[i]);
  for(int i = 0; i < 4; i++) put32(out + SAVE_PW + 4 * i, s->pw[i] < 0 ? 0u : (uint32_t)s->pw[i]);
  uint8_t *p = out + SAVE_STARS;
  for(int i = 0; i < MX_NUM_LEVELS; i++) *p++ = s->stars[i];
  put32(p, s->ach); p += 4;
  *p++ = (uint8_t)(outfit >= 0 && outfit < MX_NUM_OUTFITS ? outfit : 0);
  for(int i = 0; i < MX_NUM_LEVELS; i++){ put32(p, s->best[i]); p += 4; }
  { MxOptions o; mx_options_default(&o); if(opt) o = *opt;
    *p++ = (uint8_t)((o.collapse ? 1 : 0) | ((o.tool < MX_NUM_TOOLS ? o.tool : MX_TOOL_VALVE) << 1)); }
  put32(p, mx_crc32(out, (int)(p - out)));
  return n;
}

int mx_meta_unpack(MxStats *s, int *outfit, const uint8_t *in, int len){ return mx_meta_unpack_opt(s, outfit, 0, in, len); }
int mx_meta_unpack_opt(MxStats *s, int *outfit, MxOptions *opt, const uint8_t *in, int len){
  if(!s || !outfit || !in || len < SAVE_HDR) return 0;
  if(memcmp(in, "MXS1", 4) != 0 || in[4] < 1 || in[4] > MX_SAVE_VERSION) return 0;
  const int ver = in[4], nl = in[5], n = save_size(nl, ver);
  if(len < n) return 0;
  if(mx_crc32(in, n - 4) != get32(in + n - 4)) return 0;
  /* everything below is validated: nothing can fail from here, so *s is only ever written whole */
  int *cnt[SAVE_NCOUNT] = { &s->kills, &s->cells, &s->booms, &s->maxChain, &s->shots, &s->flops, &s->wins, &s->oneShots, &s->bestScore, &s->inflated, &s->pops };
  for(int i = 0; i < SAVE_NCOUNT; i++){ uint32_t v = get32(in + SAVE_HDR + 4 * i); *cnt[i] = (int)(v > 0x7FFFFFFFu ? 0x7FFFFFFFu : v); }
  for(int i = 0; i < 4; i++){ uint32_t v = get32(in + SAVE_PW + 4 * i); s->pw[i] = (int)(v > 0x7FFFFFFFu ? 0x7FFFFFFFu : v); }
  const uint8_t *st = in + SAVE_STARS, *p = st + nl;
  for(int i = 0; i < MX_NUM_LEVELS; i++) s->stars[i] = i < nl ? (uint8_t)(st[i] > 3 ? 3 : st[i]) : 0;
  s->ach = get32(p) & ((1u << MX_NUM_ACH) - 1u); p += 4;
  *outfit = *p < MX_NUM_OUTFITS ? *p : 0; p++;
  for(int i = 0; i < MX_NUM_LEVELS; i++){ uint32_t v = i < nl ? get32(p + 4 * i) : 0; s->best[i] = v > 0x7FFFFFFFu ? 0x7FFFFFFFu : v; }
  p += 4 * nl;
  if(opt){
    mx_options_default(opt);
    if(ver >= 2){ int b = *p; opt->collapse = (uint8_t)(b & 1); int t = (b >> 1) & 7; opt->tool = (uint8_t)(t < MX_NUM_TOOLS ? t : MX_TOOL_VALVE); }
  }
  s->lv_booms = s->lv_shots = 0; s->seen_st_inflated = s->seen_st_pops = 0;
  for(int i = 0; i < 4; i++) s->seen_pw[i] = 0;
  return 1;
}
