/* Power-loss-safe 3DS save journal: intact slots, truncated or corrupt
   records, generations and MXS payloads.  Runs in float and fixed builds. */
#include <stdio.h>
#include <string.h>
#include "mx_save_journal.h"
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);failures++;}}while(0)
int main(void){
  MxStats s, out; MxOptions opt, restored;
  mx_meta_init(&s); mx_options_default(&opt);
  s.best[0]=123456; s.stars[0]=3; s.wins=7; s.ach=3u;
  opt.nofight=1; opt.tool=MX_TOOL_TOWEL;
  unsigned char good[MX_JOURNAL_MAX], damaged[MX_JOURNAL_MAX];
  int n=mx_journal_pack(&s,11,&opt,42,good,sizeof good);
  CHECK(n>16 && n<=MX_JOURNAL_MAX);
  CHECK(n==MX_SAVE_MAX+12);
  int outfit=-1; uint32_t seq=0;
  mx_meta_init(&out); mx_options_default(&restored);
  CHECK(mx_journal_unpack(&out,&outfit,&restored,&seq,good,n));
  CHECK(seq==42 && out.best[0]==123456 && out.stars[0]==3 && out.wins==7);
  CHECK(outfit==11 && restored.nofight==1 && restored.tool==MX_TOOL_TOWEL);
  CHECK(mx_journal_newer(43,42) && !mx_journal_newer(41,42));
  CHECK(mx_journal_newer(0,0xffffffffu));  /* sequence wraps cleanly */
  CHECK(!mx_journal_newer(0xffffffffu,0));
  memcpy(damaged,good,(size_t)n);
  damaged[0]^=1;
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,damaged,n));
  memcpy(damaged,good,(size_t)n);
  damaged[6]^=4;
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,damaged,n)); /* outer CRC covers generation */
  memcpy(damaged,good,(size_t)n);
  damaged[n-6]^=0x20;
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,damaged,n));
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,good,n-1));
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,good,6));
  CHECK(!mx_journal_unpack(&out,&outfit,&restored,&seq,good,n+1));
  /* No corruption may mutate caller-owned progress or generation. */
  CHECK(out.best[0]==123456 && out.stars[0]==3 && outfit==11 && seq==42);
  printf("journal_test: %s\n",failures?"FAILED":"all passed");
  return failures!=0;
}
