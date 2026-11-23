/* All story and chapter-two stages must have useful, appropriately
   sized tactical instructions, on both float and fixed C cores. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static int faults;
static void ok(int condition,int index,const char *why){
  if(!condition){fprintf(stderr,"FAIL tactic L%d: %s\n",index+1,why);faults++;}
}
int main(void){
  for(int i=0;i<MX_NUM_LEVELS;i++){
    const char *s=mx_level_tactic(i);
    ok(s && s[0],i,"missing tactic");
    if(!s)continue;
    size_t n=strlen(s);
    ok(n<=50,i,"tactic too wide for Nintendo DS introduction");
    if(i<MX_CH3_START)
      ok(strcmp(s,"EVICT EVERY LANDLORD")!=0,i,"original stage still has a generic tip");
  }
  ok(strstr(mx_level_tactic(20),"TUBE")!=0,20,"tube mechanic is explained");
  ok(strstr(mx_level_tactic(22),"WALK")!=0,22,"on-foot level controls are explained");
  ok(strstr(mx_level_tactic(26),"BAT")!=0,26,"bat opponent counterplay is explained");
  ok(strstr(mx_level_tactic(37),"PARRY")!=0,37,"briefcase parry is explained");
  ok(strstr(mx_level_tactic(39),"DISTRICT")!=0,39,"finale multi-district objective is clear");
  ok(strstr(mx_level_tactic(50),"HEAL")!=0,50,"later Wolfman healer introduction survives");
  ok(strcmp(mx_level_tactic(-1),"EVICT EVERY LANDLORD")==0,0,
     "out-of-range callers receive a safe fallback");
  if(faults)return 1;
  puts("PASS: all 60 native level tactics, early-chapter mechanisms and DS text bounds");
  return 0;
}
