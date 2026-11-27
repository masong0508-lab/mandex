#include <stdio.h>
#include "sweep_shard.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "line %d: %s\n", __LINE__, #v); return 1; } } while (0)
int main(void) {
  const int sizes[] = {1, 7, 60, 660};
  const int partitions[] = {1, 2, 3, 8, 16};
  for (unsigned n=0; n<sizeof sizes/sizeof sizes[0]; ++n) {
    int levels=sizes[n];
    for (unsigned p=0; p<sizeof partitions/sizeof partitions[0]; ++p) {
      int parts=partitions[p];
      if (parts>levels) continue;
      int seen[660]={0}, total=0;
      char a[16], b[16]; snprintf(b,sizeof b,"%d",parts);
      for (int i=0;i<parts;++i) {
        SweepShard s; snprintf(a,sizeof a,"%d",i);
        CHECK(sweep_shard_parse(a,b,levels,&s));
        int count=0;
        for (int l=s.index;l<levels;l+=s.count) {
          CHECK(l>=0&&seen[l]==0); seen[l]++; total++; count++;
        }
        CHECK(count==(levels-1-i)/parts+1);
      }
      CHECK(total==levels);
      for (int l=0;l<levels;++l) CHECK(seen[l]==1);
    }
  }
  SweepShard s;
  CHECK(!sweep_shard_parse("-1","8",660,&s));
  CHECK(!sweep_shard_parse("8","8",660,&s));
  CHECK(!sweep_shard_parse("0","0",660,&s));
  CHECK(!sweep_shard_parse("0","661",660,&s));
  CHECK(!sweep_shard_parse("0x","8",660,&s));
  CHECK(!sweep_shard_parse("0","8junk",660,&s));
  CHECK(!sweep_shard_parse("","8",660,&s));
  CHECK(!sweep_shard_parse("999999999999999999999999","8",660,&s));
  CHECK(sweep_shard_parse("7","8",660,&s)&&s.index==7&&s.count==8);
  puts("SWEEP_SHARD_PASS exact level coverage, no duplicates, invalid arguments rejected");
  return 0;
}
