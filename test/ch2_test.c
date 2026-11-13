/* Chapter 2 mechanics (core): walk mode with bounce pads and doors (Walk-Up Wreckage), and landlords that fight back
   (walkers on Collection Day, bats + briefcase throwers on Batter Up).  Built twice by run_tests.sh (float and fixed). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mx.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static MxWorld W;
#define DT R(1.0/60.0)
static double D(real v){ return RTOF(v); }
static void set_cell_test(MxWorld *w, int x, int y, int m){ int i = y * MX_COLS + x; w->grid[i] = (uint8_t)m; w->hp[i] = RI(10); }
static int saw[32];
static void step(int n){ for(int f = 0; f < n; f++){ mx_update(&W, DT); for(int e = 0; e < W.nev; e++) if(W.ev[e].type < 32) saw[W.ev[e].type]++; } }
static void clear_saw(void){ memset(saw, 0, sizeof saw); }
static void place_walk(double x, double y){        /* put him on his feet at (x,y) without the flight */
  W.man.mode = MAN_WALK; W.man.walked = 1; W.man.x = R(x); W.man.y = R(y); W.man.vx = W.man.vy = 0; W.man.wt = RI(MX_WALK_T); W.man.face = 1; W.man.gr = 0;
  mx_walk_input(&W, 0, 0, 0, 0);
}
#define FOOT_Y (59 - 2.375)       /* standing on the ground floor of the walk-up block */
int main(void){
  /* ---- the level ---- */
  mx_load(&W, 22);
  CHECK(W.walk == 1 && W.npads == 3 && W.ntargets == 11 && W.men == 4, "Walk-Up Wreckage: walk level, 3 pads, 11 landlords, 4 men");
  CHECK(strcmp(mx_level_name(22), "Walk-Up Wreckage") == 0 && strcmp(mx_level_name(25), "Collection Day") == 0 && strcmp(mx_level_name(26), "Batter Up") == 0, "level names");
  { mx_load(&W, 0); CHECK(W.walk == 0 && W.npads == 0, "an ordinary level is not a walk level"); }
  /* the three levels that fill the chapter 2 screen (36-38), then the finale at 39 */
  mx_load(&W, 36); CHECK(W.ntubes == 2 && W.ntargets == 6 && W.targets[0].kind == LK_THROWER && W.targets[3].kind == LK_THROWER && W.walk == 0, "Mail Room Mayhem: 2 tubes, a thrower per floor");
  mx_load(&W, 37); CHECK(W.walk == 1 && W.npads == 2 && W.ntargets == 8 && W.men == 4, "Rent Strike: walk level, 2 pads, 8 landlords, 4 men");
  { int walkers = 0; for(int i = 0; i < W.ntargets; i++) walkers += W.targets[i].kind == LK_WALKER; CHECK(walkers == 3, "Rent Strike: three walkers"); }
  mx_load(&W, 38); CHECK(W.ntargets == 6 && W.targets[0].kind == LK_BAT && W.targets[1].kind == LK_BAT && W.men == 7, "Sand Standoff: two bat guards, 6 landlords");
  CHECK(MX_NUM_LEVELS == 60 && MX_CH3_START == 40 && strcmp(mx_level_name(39), "Landlord HQ") == 0 && strcmp(mx_level_name(36), "Mail Room Mayhem") == 0 && strcmp(mx_level_name(40), "Full Moon Rising") == 0 && strcmp(mx_level_name(59), "Alpha Landlord") == 0, "60 levels: chapter 2 ends with Landlord HQ, chapter 3 is 40-59");

  /* ---- a flight onto a walk level ends on his feet ---- */
  mx_load(&W, 22);
  for(int i = 0; i < 60; i++) mx_update(&W, DT);
  W.man.mode = MAN_FLY; W.man.x = R(64); W.man.y = R(54); W.man.vx = R(3); W.man.vy = R(10); W.man.gt = 0; W.men--;
  clear_saw(); step(120);
  CHECK(W.man.mode == MAN_WALK && W.man.walked && saw[EV_WALK] == 1, "he lands on his feet (EV_WALK), not stuck");
  CHECK(W.man.gr, "...and stands on the floor");
  CHECK(D(W.man.wt) > 40 && D(W.man.wt) < 45, "the 45 s walking clock is running");

  /* ---- walking, jumping ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  place_walk(62, FOOT_Y); step(10);
  double x0 = D(W.man.x);
  mx_walk_input(&W, 1, 0, 0, 0); step(60);
  CHECK(D(W.man.x) - x0 > 8 && W.man.gr && W.man.face == 1, "right walks him along the floor at about 17 cells/s");
  mx_walk_input(&W, -1, 0, 0, 0); step(30);
  CHECK(W.man.face == -1 && D(W.man.x) < x0 + 14, "left turns him round");
  { double y0 = D(W.man.y), ymin = y0; mx_walk_input(&W, 0, 1, 0, 0);
    for(int i = 0; i < 30; i++){ step(1); if(D(W.man.y) < ymin) ymin = D(W.man.y); } mx_walk_input(&W, 0, 0, 0, 0); step(60);
    CHECK(y0 - ymin > 3 && fabs(D(W.man.y) - y0) < 0.8 && W.man.gr, "jump rises about 4 cells and lands back on the floor"); }
  { double wall_x; mx_walk_input(&W, -1, 0, 0, 0); step(180); wall_x = D(W.man.x);
    CHECK(wall_x < 57 && W.man.gr, "the front door is open: he can walk straight out of the block"); }

  /* ---- a wooden door needs a punch ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  CHECK(W.grid[55 * MX_COLS + 83] == MX_WOOD, "there is a wooden door at x 83 on the ground floor");
  place_walk(80.5, FOOT_Y); step(10);
  mx_walk_input(&W, 1, 0, 0, 0); step(40);
  CHECK(D(W.man.x) < 82.5, "he cannot walk through the door");
  clear_saw(); mx_walk_input(&W, 1, 0, 1, 0); step(150);
  CHECK(saw[EV_PUNCH] >= 2 && W.grid[55 * MX_COLS + 83] == MX_EMPTY, "punches break it");
  mx_walk_input(&W, 1, 0, 0, 0); step(60);
  CHECK(D(W.man.x) > 84, "and he walks through");

  /* ---- bounce pad: boings him up its shaft ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  CHECK(W.grid[58 * MX_COLS + 101] == MX_RUBBER, "the first pad is rubber at the far right of the ground floor");
  place_walk(96.5, FOOT_Y); step(10);
  clear_saw(); { double ymin = 99; mx_walk_input(&W, 1, 0, 0, 0);
    for(int i = 0; i < 120; i++){ step(1); if(D(W.man.y) < ymin) ymin = D(W.man.y); }
    CHECK(saw[EV_PAD] >= 1, "stepping onto the pad boings (EV_PAD)");
    CHECK(ymin < 50, "...up the shaft to the floor above"); }
  { mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
    real before = W.hp[58 * MX_COLS + 101]; place_walk(98.5, FOOT_Y); mx_walk_input(&W, 0, 0, 1, 0); step(40);
    CHECK(W.grid[58 * MX_COLS + 101] == MX_RUBBER && W.hp[58 * MX_COLS + 101] == before, "pads shrug off fists"); }

  /* ---- bumping a landlord ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  place_walk(D(W.targets[0].x), D(W.targets[0].y) - 0.5); W.man.wt = RI(20);
  clear_saw(); step(10);
  CHECK(!W.targets[0].alive && saw[EV_TARGET] == 1 && D(W.man.wt) > 21, "walking into a landlord evicts him and adds 3 s");

  /* ---- the zig-zag works on foot: a dumb bot (walk toward each floor's pad, punch all the time, hop now and then) clears all 11 landlords, TNT removed ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  for(int i = 0; i < MX_ROWS * MX_COLS; i++) if(W.grid[i] == MX_TNT) W.grid[i] = 0;
  W.man.mode = MAN_FLY; W.man.x = R(64); W.man.y = R(54); W.man.vx = R(3); W.man.vy = R(10); W.man.gt = 0; W.men--;
  { int f, done = 0;
    for(f = 0; f < 60 * 45 && !done && W.man.mode != MAN_WAIT; f++){
      if(W.man.mode == MAN_WALK){
        double x = D(W.man.x), y = D(W.man.y); int fl = (int)floor((59 - (y + 2.375)) / 10.0 + 0.5), ax = (fl % 2 == 0) ? 1 : -1;
        if(fl >= 3) ax = D(W.targets[8].x) > x ? 1 : -1;
        mx_walk_input(&W, ax, (f / 20) % 7 == 0, 1, 0);
      }
      mx_update(&W, DT);
      done = mx_targets_alive(&W) == 0;
    }
    char b[96]; sprintf(b, "a simple bot clears all 11 landlords on foot in %.0f s (floor pads, doors, stairs-by-pad)", f / 60.0);
    CHECK(done && f < 60 * 30, b); }

  /* ---- time up / give up ---- */
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  place_walk(62, FOOT_Y); W.man.wt = R(0.5); clear_saw(); step(60);
  CHECK(W.man.mode == MAN_WAIT || W.man.mode == MAN_AIM, "time up ends the walk");
  mx_load(&W, 22); for(int i = 0; i < 60; i++) mx_update(&W, DT);
  place_walk(62, FOOT_Y); mx_walk_quit(&W); CHECK(W.man.mode == MAN_WAIT, "giving up ends the walk");

  /* ---- Collection Day: walkers patrol, and only when fighting back is on ---- */
  for(int fight = 1; fight >= 0; fight--){
    mx_load(&W, 25); mx_set_fight(&W, fight); mx_load(&W, 25);
    CHECK(W.ntargets == 6 && W.targets[0].kind == LK_WALKER && W.targets[5].kind == LK_NONE, "Collection Day: five walkers and one plain landlord");
    double x0s[6]; for(int i = 0; i < 6; i++) x0s[i] = D(W.targets[i].x);
    double far = 0, back = 0; step(30 * 60 / 2);
    for(int i = 0; i < 5; i++){ double d = fabs(D(W.targets[i].x) - x0s[i]); if(d > far) far = d; if(d > 8.5) back = 1; }
    if(fight) CHECK(far > 3 && !back, "walkers patrol around their post, no farther than 7 cells");
    else CHECK(far < 0.01, "with 'landlords fight back' off they stand still");
    int alive = 0; for(int i = 0; i < 6; i++) alive += W.targets[i].alive;
    CHECK(alive == 6, fight ? "patrolling does not get them killed" : "...and nobody falls over");
  }
  /* a walker on foot-duty chases the man and swings; he loses time */
  mx_load(&W, 25); mx_set_fight(&W, 1); mx_load(&W, 25);
  W.targets[0].cd = 0;
  place_walk(D(W.targets[0].x) + 12, D(W.targets[0].y) - 0.1); W.man.wt = RI(30);
  clear_saw(); step(60 * 5);
  CHECK(saw[EV_SWING] >= 1, "a walker comes for a man on foot and swings");
  CHECK(saw[EV_HURT] >= 1 || !W.targets[0].alive, "...he lands a hit (-3 s) unless the man got him first");

  /* ---- Batter Up: bats swat weak shots, hard ones plough through ---- */
  for(int mode = 0; mode < 3; mode++){      /* 0: slow shot, fight on   1: fast shot   2: slow shot, fight off */
    mx_load(&W, 26); mx_set_fight(&W, mode != 2); mx_load(&W, 26);
    CHECK(W.targets[0].kind == LK_BAT && W.targets[1].kind == LK_BAT && W.targets[3].kind == LK_THROWER && W.targets[4].kind == LK_THROWER, "Batter Up: 2 bats, 2 throwers");
    W.man.mode = MAN_FLY; W.man.x = R(57); W.man.y = R(55.5); W.man.vx = R(mode == 1 ? 95 : 45); W.man.vy = 0; W.man.gt = 0;
    clear_saw(); step(25);
    if(mode == 0) CHECK(saw[EV_BONK] >= 1 && W.targets[0].alive && D(W.man.vx) < 40 && W.targets[0].stun > 0, "a weak shot is swatted back: BONK, the bat survives and is dazed");
    if(mode == 1) CHECK(saw[EV_BONK] == 0 && !W.targets[0].alive, "a hard shot ploughs through the bat");
    if(mode == 2) CHECK(saw[EV_BONK] == 0 && !W.targets[0].alive, "with fighting back off a weak shot just evicts him");
  }
  /* throwers: lob briefcases at the man waiting in the sling */
  mx_load(&W, 26); mx_set_fight(&W, 1); mx_load(&W, 26); W.score = 10000;
  clear_saw(); step(60 * 12);
  CHECK(saw[EV_THROW] >= 1, "a thrower lobs a briefcase at the man in the sling");
  CHECK(saw[EV_HURT] >= 1 && W.score <= 8000, "...and it hurts: dazed, -2000 points");
  { real px = W.man.x; mx_set_pull(&W, R(-8), R(5)); CHECK(W.man.stun > 0 ? W.man.x == px : 1, "a dazed man cannot be pulled back"); }
  mx_load(&W, 26); mx_set_fight(&W, 0); mx_load(&W, 26); clear_saw(); step(60 * 12);
  CHECK(saw[EV_THROW] == 0 && saw[EV_HURT] == 0, "with fighting back off nobody throws anything");
  mx_set_fight(&W, 1);   /* leave the default */


  /* ---- chapter 3: health, armor, new kinds, smarter movement, collapse margins ---- */
  { int ok = 1, kinds_seen[8] = {0};
    for(int l = 40; l < 60; l++){ mx_load(&W, l); if(W.ntargets < 4 || W.men < 5) ok = 0; for(int i = 0; i < W.ntargets; i++) kinds_seen[W.targets[i].kind]++; for(int f = 0; f < 120; f++) mx_update(&W, DT); if(mx_targets_alive(&W) != W.ntargets) ok = 0; }
    CHECK(ok, "chapter 3: all 20 levels load with 4+ landlords and nobody dies while the buildings settle");
    CHECK(kinds_seen[LK_SHIELD] && kinds_seen[LK_SPRINTER] && kinds_seen[LK_BOSS] && kinds_seen[LK_HEALER] && kinds_seen[LK_THROWER] && kinds_seen[LK_BAT], "chapter 3 uses every landlord kind"); }
  { mx_load(&W, 40);                                                            /* a boss: three hits, immunity between them */
    MxTarget *t = &W.targets[0]; t->kind = LK_BOSS; t->hp = t->hpmax = 3; t->armor = 0; t->inv = 0; t->y = RI(20); t->vy = 0;
    W.state = MX_PLAY; clear_saw();
    W.man.mode = MAN_FLY; W.man.x = t->x; W.man.y = t->y; W.man.vx = RI(60); W.man.vy = 0; W.man.rad = R(1.75);
    step(1);
    CHECK(t->alive && t->hp == 2 && t->stun > 0 && saw[EV_OOF] > 0, "a boss takes a hit and survives with 2 hp");
    t->inv = 0; t->stun = 0; W.man.x = t->x; W.man.y = t->y; W.man.mode = MAN_FLY; W.man.vx = RI(60); step(1);
    t->inv = 0; t->stun = 0; W.man.x = t->x; W.man.y = t->y; W.man.mode = MAN_FLY; W.man.vx = RI(60); step(1);
    CHECK(!t->alive, "...and the third hit evicts him"); }
  { mx_load(&W, 40);                                                            /* a shield landlord facing the man blocks a slow shot; a fast one smashes the shield */
    MxTarget *t = &W.targets[0]; t->kind = LK_SHIELD; t->hp = t->hpmax = 2; t->armor = 1; t->face = 1; t->stun = 0; t->inv = 0; t->y = RI(20); t->vy = 0; W.state = MX_PLAY; W.nofight = 0;
    W.man.mode = MAN_FLY; W.man.x = t->x + RI(1); W.man.y = t->y; W.man.vx = -RI(25); W.man.vy = 0; W.man.rad = R(1.75);
    step(1);
    CHECK(t->alive && t->hp == 2 && W.man.vx > 0, "a riot shield bounces a slow shot (from the front)");
    t->stun = 0; t->inv = 0; t->armor = 1; W.man.mode = MAN_FLY; W.man.x = t->x + RI(1); W.man.y = t->y; W.man.vx = -RI(90); W.man.vy = 0; step(1);
    CHECK((t->armor == 0 && t->hp == 1) || !t->alive, "a hard shot smashes the shield and hurts him"); }
  { mx_load(&W, 40);                                                            /* a healer patches a hurt landlord up */
    MxTarget *a = &W.targets[0], *h = &W.targets[1]; h->kind = LK_HEALER; h->x = a->x + RI(3); h->y = a->y; h->heal = R(0.05);
    a->kind = LK_BOSS; a->hp = 1; a->hpmax = 3; clear_saw(); step(10);
    CHECK(a->hp == 2 && saw[EV_HEAL] > 0, "a healer heals a hurt landlord +1 hp"); }
  { mx_load(&W, 40);                                                            /* a walker dodges a falling chunk even with fighting back off */
    mx_set_fight(&W, 0);
    MxTarget *t = &W.targets[0]; t->kind = LK_WALKER; t->range = RI(2); real x0 = t->x;
    for(int c = 0; c < MX_MAX_CHUNKS; c++) if(!W.chunks[c].active){ MxChunk *ch = &W.chunks[c]; memset(ch, 0, sizeof *ch); ch->active = 1; ch->x = t->x; ch->y = t->y - RI(6); ch->vy = RI(25); ch->rad = RI(1); ch->count = 1; ch->m = RI(1); ch->I = RI(1); ch->cells[0].wx = ch->x; ch->cells[0].wy = ch->y; ch->cells[0].m = MX_WOOD; break; }
    mx_update(&W, DT);
    CHECK(t->dodge > 0 || rabs(t->x - x0) > R(0.1), "a walker starts running from falling debris");
    for(int c = 0; c < MX_MAX_CHUNKS; c++) W.chunks[c].active = 0; }
  { mx_load(&W, 40);                                                            /* buried by light rubble: dig out instead of dying; a hard fall only stuns */
    MxTarget *t = &W.targets[0]; real y0 = t->y;
    for(int dy = 0; dy < 2; dy++) set_cell_test(&W, (int)D(t->x), (int)D(t->y) - dy, MX_WOOD);
    step(2);
    CHECK(t->alive && t->y < y0, "a landlord buried by a thin layer of rubble digs out");
    mx_load(&W, 40); t = &W.targets[0]; t->vy = RI(42); t->y -= RI(1); step(40);
    CHECK(t->alive, "a hard landing (not a huge fall) only stuns"); }
  printf(fails ? "CH2 TEST FAILED (%d)\n" : "CH2 TEST OK\n", fails);
  return fails != 0;
}
