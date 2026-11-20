/* Combo score and timeout parity in the float and Nintendo DS fixed-point core.
   Include the core once to exercise the same internal eviction path as all ports. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
#include "../core/mx.c"
static MxWorld w;
static int fail;
#define OK(expr) do { if(!(expr)){ fprintf(stderr,"FAIL %s (line %d)\n",#expr,__LINE__); fail++; } } while(0)
static void evict(void){
  MxTarget *t = &w.targets[0]; memset(t, 0, sizeof *t);
  t->alive = 1; t->hp = 1; t->x = RI(50); t->y = RI(42);
  kill_target_ex(&w, t, 0);
}
int main(void){
  memset(&w,0,sizeof w); w.man.mode=MAN_AIM; w.state=MX_PLAY;
  evict(); OK(w.score == 5000 && w.combo == 1 && w.combo_peak == 1);
  OK(w.nev == 1 && w.ev[0].type == EV_TARGET && w.ev[0].m == 1 && w.ev[0].a == 0);
  w.nev=0; evict(); OK(w.score == 11000 && w.combo == 2 && w.combo_peak == 2);
  OK(w.nev == 1 && w.ev[0].m == 2 && w.ev[0].a == RI(1000));
  for(int i=0;i<6;i++){w.nev=0;evict();}
  OK(w.combo == 5 && w.combo_peak == 5 && w.score == 11000 + 7000 + 8000 + 9000 + 9000 + 9000 + 9000);
  OK(strcmp(mx_combo_taunt(3),"THREESOME OF DOOM!")==0);
  combo_tick(&w, R(2.6)); OK(w.combo == 0 && w.combo_left == 0);
  int prev=w.score; evict(); OK(w.score == prev + 5000 && w.combo == 1 && w.combo_peak == 5);
  /* Failed armor hits never extend the timer or award bonus. */
  { MxTarget *t=&w.targets[0]; t->alive=1; t->hp=2; t->inv=0;
    int old=w.score; real left=w.combo_left; kill_target_ex(&w,t,0);
    OK(t->hp==1 && t->alive==1 && old==w.score && w.combo_left==left); }
  printf(fail ? "combo_test: %d failed\n" : "combo_test: all passed\n",fail);
  return fail!=0;
}
